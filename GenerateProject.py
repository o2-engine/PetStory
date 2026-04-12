#!/usr/bin/env python3
"""
PetStory Project Generator
Local web UI to configure, generate, and open the project in an IDE.
Reads presets from CMakePresets.json. Zero external dependencies.
"""

import http.server
import json
import os
import platform
import shutil
import subprocess
import sys
import threading
import webbrowser
from pathlib import Path
from urllib.parse import parse_qs

SCRIPT_DIR = Path(__file__).resolve().parent
PRESETS_FILE = SCRIPT_DIR / "CMakePresets.json"
HOST = platform.system()
PORT = 0  # auto-pick free port

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------

GENERATORS = {
    "Darwin":  ["Xcode", "Unix Makefiles", "Ninja", "Ninja Multi-Config"],
    "Windows": ["Visual Studio 17 2022", "Visual Studio 16 2019", "Ninja", "Ninja Multi-Config"],
    "Linux":   ["Unix Makefiles", "Ninja", "Ninja Multi-Config"],
}

IDES = {
    "Darwin":  ["Xcode", "VS Code", "CLion"],
    "Windows": ["Visual Studio", "VS Code", "CLion"],
    "Linux":   ["VS Code", "CLion"],
}

BUILD_TYPES = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]

O2_OPTIONS = [
    ("O2_EDITOR",  "Editor",          True),
    ("O2_TESTS",   "Tests",           True),
    ("O2_ASAN",    "ASAN",            False),
    ("O2_TRACY",   "Tracy profiling", False),
]

# ---------------------------------------------------------------------------
# Presets
# ---------------------------------------------------------------------------

def load_presets():
    with open(PRESETS_FILE, "r") as f:
        data = json.load(f)
    presets = []
    for p in data.get("configurePresets", []):
        cond = p.get("condition")
        if cond and cond.get("type") == "equals":
            lhs = cond["lhs"].replace("${hostSystemName}", HOST)
            if lhs != cond["rhs"]:
                continue
        presets.append(p)
    return presets

def resolve_binary_dir(preset):
    bd = preset.get("binaryDir", "build")
    return bd.replace("${sourceDir}", str(SCRIPT_DIR))

# ---------------------------------------------------------------------------
# IDE opener
# ---------------------------------------------------------------------------

def find_project_file(build_dir, generator):
    bd = Path(build_dir)
    if not bd.exists():
        return None
    if "Xcode" in generator:
        for f in bd.glob("*.xcodeproj"):
            return str(f)
    elif "Visual Studio" in generator:
        for f in bd.glob("*.sln"):
            return str(f)
    return None

def open_ide(ide, build_dir, generator):
    project = find_project_file(build_dir, generator)
    if ide == "Xcode":
        if project:
            subprocess.Popen(["open", project])
            return "Opening Xcode..."
        return "Error: No .xcodeproj found. Generate with Xcode generator first."
    elif ide == "Visual Studio":
        if project:
            os.startfile(project)
            return "Opening Visual Studio..."
        return "Error: No .sln found. Generate with Visual Studio generator first."
    elif ide == "VS Code":
        if HOST == "Darwin":
            subprocess.Popen(["open", "-a", "Visual Studio Code", str(SCRIPT_DIR)])
        else:
            subprocess.Popen(["code", str(SCRIPT_DIR)])
        return "Opening VS Code..."
    elif ide == "CLion":
        if HOST == "Darwin":
            subprocess.Popen(["open", "-a", "CLion", str(SCRIPT_DIR)])
        else:
            clion = shutil.which("clion")
            if clion:
                subprocess.Popen([clion, str(SCRIPT_DIR)])
            else:
                return "Error: CLion not found in PATH."
        return "Opening CLion..."
    return f"Unknown IDE: {ide}"

# ---------------------------------------------------------------------------
# Process runner (streams output via SSE)
# ---------------------------------------------------------------------------

class ProcessRunner:
    def __init__(self):
        self.lock = threading.Lock()
        self.lines = []
        self.running = False
        self.done_msg = ""

    def start(self, cmd):
        with self.lock:
            if self.running:
                return False
            self.lines = []
            self.running = True
            self.done_msg = ""

        def worker():
            try:
                proc = subprocess.Popen(
                    cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True, bufsize=1, cwd=str(SCRIPT_DIR))
                for line in proc.stdout:
                    with self.lock:
                        self.lines.append(line)
                proc.wait()
                rc = proc.returncode
                msg = "Done" if rc == 0 else f"Failed (exit code {rc})"
                with self.lock:
                    self.lines.append(f"\n--- {msg} ---\n")
                    self.done_msg = msg
            except Exception as e:
                with self.lock:
                    self.lines.append(f"\nError: {e}\n")
                    self.done_msg = "Error"
            finally:
                with self.lock:
                    self.running = False

        threading.Thread(target=worker, daemon=True).start()
        return True

    def poll(self, since=0):
        with self.lock:
            new_lines = self.lines[since:]
            return {
                "lines": new_lines,
                "total": len(self.lines),
                "running": self.running,
                "done": self.done_msg,
            }

runner = ProcessRunner()

# ---------------------------------------------------------------------------
# HTML
# ---------------------------------------------------------------------------

def build_html(presets):
    presets_json = json.dumps([{
        "name": p["name"],
        "display": p.get("displayName", p["name"]),
        "generator": p.get("generator", ""),
        "buildDir": resolve_binary_dir(p),
        "vars": p.get("cacheVariables", {}),
    } for p in presets])

    generators_json = json.dumps(GENERATORS.get(HOST, []))
    ides_json = json.dumps(IDES.get(HOST, ["VS Code"]))
    build_types_json = json.dumps(BUILD_TYPES)
    options_json = json.dumps(O2_OPTIONS)

    return f"""<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>PetStory — Project Generator</title>
<style>
* {{ box-sizing: border-box; margin: 0; padding: 0; }}
body {{
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    background: #1e1e1e; color: #ccc; padding: 24px;
    max-width: 600px; margin: 0 auto;
    -webkit-font-smoothing: antialiased;
}}
h1 {{ font-size: 20px; color: #fff; margin-bottom: 20px; font-weight: 600; }}
.row {{ display: flex; align-items: center; margin-bottom: 10px; }}
.row label {{ width: 100px; font-size: 13px; color: #999; flex-shrink: 0; }}
.row select, .row input[type=text] {{
    flex: 1; padding: 6px 10px; border-radius: 6px; border: 1px solid #444;
    background: #2a2a2a; color: #eee; font-size: 13px; outline: none;
}}
.row select:focus, .row input:focus {{ border-color: #5a9; }}
.row input[readonly] {{ color: #888; }}
.opts {{ display: flex; flex-wrap: wrap; gap: 4px 18px; flex: 1; }}
.opts label {{
    width: auto; font-size: 13px; color: #ccc; cursor: pointer;
    display: flex; align-items: center; gap: 5px;
}}
.opts input[type=checkbox] {{ accent-color: #5a9; }}
hr {{ border: none; border-top: 1px solid #333; margin: 16px 0; }}
.buttons {{ display: flex; gap: 10px; margin-bottom: 16px; }}
.buttons button {{
    flex: 1; padding: 10px; border-radius: 8px; border: none;
    font-size: 14px; font-weight: 500; cursor: pointer; transition: background .15s;
}}
.btn-primary {{ background: #4a8; color: #fff; }}
.btn-primary:hover {{ background: #5b9; }}
.btn-primary:disabled {{ background: #3a5; opacity: .5; cursor: default; }}
.btn-secondary {{ background: #444; color: #eee; }}
.btn-secondary:hover {{ background: #555; }}
.btn-secondary:disabled {{ opacity: .5; cursor: default; }}
#log {{
    background: #181818; border: 1px solid #333; border-radius: 8px;
    padding: 10px; font-family: "SF Mono", Menlo, Consolas, monospace;
    font-size: 11px; line-height: 1.5; color: #aaa;
    height: 220px; overflow-y: auto; white-space: pre-wrap; word-break: break-all;
}}
#status {{ font-size: 12px; color: #666; margin-top: 8px; }}
</style>
</head>
<body>

<h1>PetStory — Project Generator</h1>

<div class="row">
    <label>Preset</label>
    <select id="preset" onchange="onPresetChanged()"></select>
</div>
<div class="row">
    <label>Generator</label>
    <select id="generator"></select>
</div>
<div class="row">
    <label>Build type</label>
    <select id="buildtype"></select>
</div>
<div class="row">
    <label>IDE</label>
    <select id="ide"></select>
</div>
<div class="row">
    <label>Build dir</label>
    <input type="text" id="builddir" readonly>
</div>

<hr>

<div class="row">
    <label>Options</label>
    <div class="opts" id="options"></div>
</div>

<hr>

<div class="buttons">
    <button class="btn-primary" id="btnGenerate" onclick="doGenerate()">Generate Project</button>
    <button class="btn-secondary" id="btnIDE" onclick="doOpenIDE()">Open IDE</button>
</div>

<div id="log"></div>
<div id="status">Ready</div>

<script>
const presets = {presets_json};
const generators = {generators_json};
const ides = {ides_json};
const buildTypes = {build_types_json};
const o2Options = {options_json};

let pollTimer = null;
let pollOffset = 0;

function init() {{
    const ps = document.getElementById('preset');
    presets.forEach((p,i) => {{
        const o = document.createElement('option');
        o.value = i; o.textContent = p.display;
        ps.appendChild(o);
    }});

    fillSelect('generator', generators);
    fillSelect('ide', ides);
    fillSelect('buildtype', buildTypes);

    const opts = document.getElementById('options');
    o2Options.forEach(([key, label, def]) => {{
        const lbl = document.createElement('label');
        const cb = document.createElement('input');
        cb.type = 'checkbox'; cb.id = 'opt_' + key; cb.checked = def;
        lbl.appendChild(cb);
        lbl.appendChild(document.createTextNode(label));
        opts.appendChild(lbl);
    }});

    onPresetChanged();
}}

function fillSelect(id, items) {{
    const s = document.getElementById(id);
    s.innerHTML = '';
    items.forEach(v => {{
        const o = document.createElement('option');
        o.value = v; o.textContent = v;
        s.appendChild(o);
    }});
}}

function onPresetChanged() {{
    const p = presets[document.getElementById('preset').value];
    if (!p) return;

    document.getElementById('builddir').value = p.buildDir;

    // Set defaults from preset
    if (p.generator) document.getElementById('generator').value = p.generator;
    const bt = p.vars.CMAKE_BUILD_TYPE;
    if (bt) document.getElementById('buildtype').value = bt;

    if (p.generator === 'Xcode') document.getElementById('ide').value = 'Xcode';
    else if (p.generator && p.generator.includes('Visual Studio'))
        document.getElementById('ide').value = 'Visual Studio';

    o2Options.forEach(([key, , def]) => {{
        const el = document.getElementById('opt_' + key);
        const v = p.vars[key];
        el.checked = v ? v.toUpperCase() === 'ON' : def;
    }});

    // Override with existing build cache if present
    fetch('/cache?buildDir=' + encodeURIComponent(p.buildDir))
        .then(r => r.json())
        .then(c => {{
            if (!c.found) return;
            if (c.generator) document.getElementById('generator').value = c.generator;
            if (c.buildType) document.getElementById('buildtype').value = c.buildType;
            o2Options.forEach(([key]) => {{
                if (c[key] !== undefined) {{
                    document.getElementById('opt_' + key).checked = c[key].toUpperCase() === 'ON';
                }}
            }});
        }})
        .catch(() => {{}});
}}

function setRunning(v) {{
    document.getElementById('btnGenerate').disabled = v;
    document.getElementById('btnIDE').disabled = v;
}}

function doGenerate() {{
    const p = presets[document.getElementById('preset').value];
    const gen = document.getElementById('generator').value;
    const bt = document.getElementById('buildtype').value;
    const buildDir = document.getElementById('builddir').value;

    const params = new URLSearchParams();
    params.set('buildDir', buildDir);
    params.set('generator', gen);
    params.set('buildType', bt);

    // iOS vars from preset
    ['CMAKE_SYSTEM_NAME','CMAKE_OSX_SYSROOT','CMAKE_OSX_DEPLOYMENT_TARGET'].forEach(k => {{
        if (p.vars[k]) params.set(k, p.vars[k]);
    }});

    o2Options.forEach(([key]) => {{
        params.set(key, document.getElementById('opt_' + key).checked ? 'ON' : 'OFF');
    }});

    document.getElementById('log').textContent = '';
    document.getElementById('status').textContent = 'Running...';
    setRunning(true);
    pollOffset = 0;

    fetch('/generate?' + params.toString())
        .then(r => r.json())
        .then(d => {{
            if (d.ok) startPolling();
            else {{ document.getElementById('status').textContent = d.error; setRunning(false); }}
        }});
}}

function doOpenIDE() {{
    const ide = document.getElementById('ide').value;
    const gen = document.getElementById('generator').value;
    const buildDir = document.getElementById('builddir').value;
    const params = new URLSearchParams({{ ide, generator: gen, buildDir }});

    fetch('/open_ide?' + params.toString())
        .then(r => r.json())
        .then(d => {{ document.getElementById('status').textContent = d.message; }});
}}

function startPolling() {{
    if (pollTimer) clearInterval(pollTimer);
    pollTimer = setInterval(pollLog, 100);
}}

function pollLog() {{
    fetch('/poll?since=' + pollOffset)
        .then(r => r.json())
        .then(d => {{
            if (d.lines.length > 0) {{
                const log = document.getElementById('log');
                log.textContent += d.lines.join('');
                log.scrollTop = log.scrollHeight;
            }}
            pollOffset = d.total;
            if (!d.running) {{
                clearInterval(pollTimer);
                pollTimer = null;
                document.getElementById('status').textContent = d.done || 'Done';
                setRunning(false);
            }}
        }});
}}

init();
</script>
</body>
</html>"""

# ---------------------------------------------------------------------------
# HTTP handler
# ---------------------------------------------------------------------------

class Handler(http.server.BaseHTTPRequestHandler):
    presets = []

    def log_message(self, *args):
        pass  # silence request logging

    def _json(self, data, code=200):
        body = json.dumps(data).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = self.path.split("?")[0]
        qs = parse_qs(self.path.split("?", 1)[1]) if "?" in self.path else {}
        # flatten qs values
        params = {k: v[0] for k, v in qs.items()}

        if path == "/":
            html = build_html(self.presets).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(html)))
            self.end_headers()
            self.wfile.write(html)

        elif path == "/generate":
            build_dir = params.get("buildDir", "build")
            gen = params.get("generator", "")
            bt = params.get("buildType", "Debug")

            # If generator changed, wipe the build dir to avoid CMake error
            cache_file = Path(build_dir) / "CMakeCache.txt"
            if cache_file.exists() and gen:
                try:
                    for line in cache_file.read_text().splitlines():
                        if line.startswith("CMAKE_GENERATOR:"):
                            old_gen = line.split("=", 1)[1]
                            if old_gen != gen:
                                shutil.rmtree(build_dir)
                            break
                except Exception:
                    pass

            cmd = ["cmake", "-B", build_dir, "-S", str(SCRIPT_DIR)]
            if gen:
                cmd += ["-G", gen]
            cmd.append(f"-DCMAKE_BUILD_TYPE={bt}")

            for key in ("CMAKE_SYSTEM_NAME", "CMAKE_OSX_SYSROOT", "CMAKE_OSX_DEPLOYMENT_TARGET"):
                if key in params:
                    cmd.append(f"-D{key}={params[key]}")

            for cmake_name, _, _ in O2_OPTIONS:
                val = params.get(cmake_name, "OFF")
                cmd.append(f"-D{cmake_name}={val}")

            ok = runner.start(cmd)
            if ok:
                self._json({"ok": True, "cmd": " ".join(cmd)})
            else:
                self._json({"ok": False, "error": "A command is already running."})

        elif path == "/poll":
            since = int(params.get("since", "0"))
            self._json(runner.poll(since))

        elif path == "/cache":
            build_dir = params.get("buildDir", "build")
            cache_file = Path(build_dir) / "CMakeCache.txt"
            result = {"found": False}
            if cache_file.exists():
                try:
                    text = cache_file.read_text()
                    result["found"] = True
                    for line in text.splitlines():
                        if line.startswith("CMAKE_GENERATOR:"):
                            result["generator"] = line.split("=", 1)[1]
                        elif line.startswith("CMAKE_BUILD_TYPE:"):
                            result["buildType"] = line.split("=", 1)[1]
                        elif line.startswith("O2_EDITOR:"):
                            result["O2_EDITOR"] = line.split("=", 1)[1]
                        elif line.startswith("O2_TESTS:"):
                            result["O2_TESTS"] = line.split("=", 1)[1]
                        elif line.startswith("O2_ASAN:"):
                            result["O2_ASAN"] = line.split("=", 1)[1]
                        elif line.startswith("O2_TRACY:"):
                            result["O2_TRACY"] = line.split("=", 1)[1]
                except Exception:
                    pass
            self._json(result)

        elif path == "/open_ide":
            ide = params.get("ide", "")
            build_dir = params.get("buildDir", "build")
            gen = params.get("generator", "")
            msg = open_ide(ide, build_dir, gen)
            self._json({"message": msg})

        else:
            self.send_error(404)

# ---------------------------------------------------------------------------

def main():
    if not PRESETS_FILE.exists():
        print(f"Error: CMakePresets.json not found in {SCRIPT_DIR}", file=sys.stderr)
        sys.exit(1)

    presets = load_presets()
    Handler.presets = presets

    server = http.server.HTTPServer(("127.0.0.1", PORT), Handler)
    actual_port = server.server_address[1]
    url = f"http://127.0.0.1:{actual_port}"

    print(f"Project Generator running at {url}")
    print("Press Ctrl+C to stop.\n")

    webbrowser.open(url)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
        server.server_close()


if __name__ == "__main__":
    main()
