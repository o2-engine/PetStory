#!/usr/bin/env python3
"""
PetStory Project Generator (ImGui edition)
Native UI to configure, generate, and open the project in an IDE.
Reads presets from CMakePresets.json. Requires `imgui-bundle`.
"""

import json
import os
import platform
import shutil
import subprocess
import sys
import threading
from pathlib import Path

try:
    from imgui_bundle import imgui, hello_imgui, immapp
except ImportError:
    sys.stderr.write(
        "imgui-bundle is not installed.\n"
        "Install it with:  python -m pip install --upgrade imgui-bundle\n"
    )
    sys.exit(1)


SCRIPT_DIR = Path(__file__).resolve().parent
PRESETS_FILE = SCRIPT_DIR / "CMakePresets.json"
HOST = platform.system()


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

CACHE_KEYS = ("CMAKE_GENERATOR", "CMAKE_BUILD_TYPE",
              "O2_EDITOR", "O2_TESTS", "O2_ASAN", "O2_TRACY")


# --- Presets ----------------------------------------------------------------

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


def read_cache(build_dir):
    cache_file = Path(build_dir) / "CMakeCache.txt"
    out = {}
    if not cache_file.exists():
        return out
    try:
        for line in cache_file.read_text().splitlines():
            for k in CACHE_KEYS:
                if line.startswith(k + ":"):
                    out[k] = line.split("=", 1)[1]
                    break
    except Exception:
        pass
    return out


# --- IDE opener -------------------------------------------------------------

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
    if ide == "Visual Studio":
        if project:
            os.startfile(project)
            return "Opening Visual Studio..."
        return "Error: No .sln found. Generate with Visual Studio generator first."
    if ide == "VS Code":
        if HOST == "Darwin":
            subprocess.Popen(["open", "-a", "Visual Studio Code", str(SCRIPT_DIR)])
        else:
            subprocess.Popen(["code", str(SCRIPT_DIR)], shell=(HOST == "Windows"))
        return "Opening VS Code..."
    if ide == "CLion":
        if HOST == "Darwin":
            subprocess.Popen(["open", "-a", "CLion", str(SCRIPT_DIR)])
        else:
            clion = shutil.which("clion")
            if not clion:
                return "Error: CLion not found in PATH."
            subprocess.Popen([clion, str(SCRIPT_DIR)])
        return "Opening CLion..."
    return f"Unknown IDE: {ide}"


# --- Process runner ---------------------------------------------------------

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

    def snapshot(self):
        with self.lock:
            return ("".join(self.lines), len(self.lines), self.running, self.done_msg)


# --- Style ------------------------------------------------------------------

def setup_style():
    style = imgui.get_style()
    style.window_rounding = 0.0
    style.frame_rounding = 3.0
    style.grab_rounding = 3.0
    style.scrollbar_rounding = 3.0
    style.popup_rounding = 3.0
    style.tab_rounding = 3.0
    style.frame_border_size = 1.0
    style.window_border_size = 0.0
    style.child_border_size = 1.0
    style.frame_padding = imgui.ImVec2(6, 4)
    style.item_spacing = imgui.ImVec2(8, 6)
    style.item_inner_spacing = imgui.ImVec2(6, 4)
    style.scrollbar_size = 14.0
    style.grab_min_size = 10.0
    style.indent_spacing = 14.0

    C = imgui.Col_

    def s(idx, r, g, b, a=1.0):
        col = imgui.ImVec4(r, g, b, a)
        try:
            style.set_color_(idx, col)
        except AttributeError:
            style.colors[int(idx)] = col

    s(C.text,                    0.13, 0.16, 0.15)
    s(C.text_disabled,           0.50, 0.55, 0.53)
    s(C.window_bg,               0.91, 0.94, 0.93)
    s(C.child_bg,                0.91, 0.94, 0.93)
    s(C.popup_bg,                0.97, 0.98, 0.97)
    s(C.border,                  0.60, 0.70, 0.67, 0.50)
    s(C.border_shadow,           0.0,  0.0,  0.0,  0.0)
    s(C.frame_bg,                1.00, 1.00, 1.00)
    s(C.frame_bg_hovered,        0.96, 0.98, 0.97)
    s(C.frame_bg_active,         0.93, 0.97, 0.95)
    s(C.title_bg,                0.62, 0.80, 0.77)
    s(C.title_bg_active,         0.62, 0.80, 0.77)
    s(C.title_bg_collapsed,      0.62, 0.80, 0.77, 0.85)
    s(C.menu_bar_bg,             0.78, 0.88, 0.85)
    s(C.scrollbar_bg,            0.85, 0.90, 0.88)
    s(C.scrollbar_grab,          0.62, 0.75, 0.71)
    s(C.scrollbar_grab_hovered,  0.55, 0.68, 0.64)
    s(C.scrollbar_grab_active,   0.48, 0.60, 0.57)
    s(C.check_mark,              0.20, 0.55, 0.45)
    s(C.slider_grab,             0.55, 0.74, 0.71)
    s(C.slider_grab_active,      0.48, 0.66, 0.63)
    s(C.button,                  0.93, 0.96, 0.95)
    s(C.button_hovered,          0.85, 0.92, 0.90)
    s(C.button_active,           0.78, 0.86, 0.83)
    s(C.header,                  0.62, 0.80, 0.77)
    s(C.header_hovered,          0.68, 0.84, 0.81)
    s(C.header_active,           0.55, 0.74, 0.71)
    s(C.separator,               0.55, 0.65, 0.62, 0.50)
    s(C.separator_hovered,       0.55, 0.65, 0.62)
    s(C.separator_active,        0.48, 0.58, 0.55)
    s(C.resize_grip,             0.55, 0.65, 0.62, 0.60)
    s(C.resize_grip_hovered,     0.62, 0.72, 0.69, 0.80)
    s(C.resize_grip_active,      0.48, 0.58, 0.55)


# --- App --------------------------------------------------------------------

LABEL_W = 110.0


class App:
    def __init__(self, presets):
        self.presets = presets
        self.preset_names = [p.get("displayName", p["name"]) for p in presets]

        self.host_generators = GENERATORS.get(HOST, [])
        self.host_ides = IDES.get(HOST, ["VS Code"])

        self.preset_index = 0
        self.gen_index = 0
        self.bt_index = 0
        self.ide_index = 0
        self.opt_states = {k: d for k, _, d in O2_OPTIONS}
        self.build_dir = ""

        self.runner = ProcessRunner()
        self.log_text = ""
        self.last_count = 0
        self.scroll_log = False
        self.status = "Ready"
        self.was_running = False
        self.auto_open_ide = False

        self.apply_preset()

    def apply_preset(self):
        if not self.presets:
            return
        p = self.presets[self.preset_index]
        self.build_dir = resolve_binary_dir(p)

        gen = p.get("generator", "")
        if gen and gen in self.host_generators:
            self.gen_index = self.host_generators.index(gen)

        bt = p.get("cacheVariables", {}).get("CMAKE_BUILD_TYPE", "Debug")
        if bt in BUILD_TYPES:
            self.bt_index = BUILD_TYPES.index(bt)

        if "Xcode" in gen and "Xcode" in self.host_ides:
            self.ide_index = self.host_ides.index("Xcode")
        elif "Visual Studio" in gen and "Visual Studio" in self.host_ides:
            self.ide_index = self.host_ides.index("Visual Studio")

        for key, _, default in O2_OPTIONS:
            v = p.get("cacheVariables", {}).get(key)
            self.opt_states[key] = (v.upper() == "ON") if v else default

        cache = read_cache(self.build_dir)
        if cache:
            cg = cache.get("CMAKE_GENERATOR")
            if cg and cg in self.host_generators:
                self.gen_index = self.host_generators.index(cg)
            cb = cache.get("CMAKE_BUILD_TYPE")
            if cb and cb in BUILD_TYPES:
                self.bt_index = BUILD_TYPES.index(cb)
            for key, _, _ in O2_OPTIONS:
                if key in cache:
                    self.opt_states[key] = cache[key].upper() == "ON"

    def labeled(self, label, content_fn):
        imgui.align_text_to_frame_padding()
        imgui.text(label)
        imgui.same_line(LABEL_W)
        imgui.set_next_item_width(-1)
        return content_fn()

    def gui(self):
        if imgui.collapsing_header("Configuration", flags=imgui.TreeNodeFlags_.default_open):
            imgui.dummy(imgui.ImVec2(0, 2))

            def _preset():
                ch, idx = imgui.combo("##preset", self.preset_index, self.preset_names)
                if ch:
                    self.preset_index = idx
                    self.apply_preset()
            self.labeled("Preset", _preset)

            def _gen():
                ch, idx = imgui.combo("##gen", self.gen_index, self.host_generators)
                if ch:
                    self.gen_index = idx
            self.labeled("Generator", _gen)

            def _bt():
                ch, idx = imgui.combo("##bt", self.bt_index, BUILD_TYPES)
                if ch:
                    self.bt_index = idx
            self.labeled("Build type", _bt)

            def _ide():
                ch, idx = imgui.combo("##ide", self.ide_index, self.host_ides)
                if ch:
                    self.ide_index = idx
            self.labeled("IDE", _ide)

            def _builddir():
                imgui.input_text("##builddir", self.build_dir,
                                 imgui.InputTextFlags_.read_only)
            self.labeled("Build dir", _builddir)

            imgui.dummy(imgui.ImVec2(0, 4))

        if imgui.collapsing_header("Options", flags=imgui.TreeNodeFlags_.default_open):
            imgui.dummy(imgui.ImVec2(0, 2))
            imgui.align_text_to_frame_padding()
            imgui.text("Flags")
            imgui.same_line(LABEL_W)
            for i, (key, label, _) in enumerate(O2_OPTIONS):
                if i > 0:
                    imgui.same_line(0, 16)
                ch, val = imgui.checkbox(label + "##" + key, self.opt_states[key])
                if ch:
                    self.opt_states[key] = val
            imgui.dummy(imgui.ImVec2(0, 4))

        imgui.spacing()
        imgui.separator()
        imgui.spacing()

        running = self.runner.running
        avail_w = imgui.get_content_region_avail().x
        btn_w = (avail_w - 16) / 3.0

        if running:
            imgui.begin_disabled(True)

        if imgui.button("Generate Project", imgui.ImVec2(btn_w, 32)):
            self.do_generate()

        imgui.same_line()
        imgui.push_style_color(imgui.Col_.button,         imgui.ImVec4(0.32, 0.62, 0.54, 1.0))
        imgui.push_style_color(imgui.Col_.button_hovered, imgui.ImVec4(0.40, 0.70, 0.61, 1.0))
        imgui.push_style_color(imgui.Col_.button_active,  imgui.ImVec4(0.28, 0.56, 0.48, 1.0))
        imgui.push_style_color(imgui.Col_.text,           imgui.ImVec4(1.00, 1.00, 1.00, 1.0))
        if imgui.button("Generate & Open IDE", imgui.ImVec2(btn_w, 32)):
            self.do_generate(open_after=True)
        imgui.pop_style_color(4)

        imgui.same_line()
        if imgui.button("Open IDE", imgui.ImVec2(btn_w, 32)):
            self.do_open_ide()

        if running:
            imgui.end_disabled()

        imgui.spacing()

        text, count, run_state, done_msg = self.runner.snapshot()
        if count != self.last_count:
            self.log_text = text
            self.last_count = count
            self.scroll_log = True
        if run_state:
            self.status = "Running..."
        elif done_msg:
            self.status = done_msg

        if self.was_running and not run_state:
            if self.auto_open_ide and done_msg == "Done":
                self.do_open_ide()
            self.auto_open_ide = False
        self.was_running = run_state

        if imgui.collapsing_header("Output", flags=imgui.TreeNodeFlags_.default_open):
            imgui.dummy(imgui.ImVec2(0, 2))

            status_h = imgui.get_text_line_height_with_spacing()
            log_h = imgui.get_content_region_avail().y - status_h - 6
            if log_h < 60:
                log_h = 60

            imgui.push_style_color(imgui.Col_.child_bg, imgui.ImVec4(0.15, 0.17, 0.16, 1.0))
            imgui.push_style_color(imgui.Col_.text,     imgui.ImVec4(0.85, 0.88, 0.86, 1.0))
            imgui.begin_child("##log", imgui.ImVec2(0, log_h), imgui.ChildFlags_.border)
            imgui.text_unformatted(self.log_text or "")
            if self.scroll_log:
                imgui.set_scroll_here_y(1.0)
                self.scroll_log = False
            imgui.end_child()
            imgui.pop_style_color(2)

            imgui.text_disabled(self.status)

    def do_generate(self, open_after=False):
        if not self.presets:
            return
        self.auto_open_ide = open_after
        p = self.presets[self.preset_index]
        gen = self.host_generators[self.gen_index] if self.host_generators else ""
        bt = BUILD_TYPES[self.bt_index]

        cache_file = Path(self.build_dir) / "CMakeCache.txt"
        if cache_file.exists() and gen:
            try:
                for line in cache_file.read_text().splitlines():
                    if line.startswith("CMAKE_GENERATOR:"):
                        old_gen = line.split("=", 1)[1]
                        if old_gen != gen:
                            shutil.rmtree(self.build_dir)
                        break
            except Exception:
                pass

        cmd = ["cmake", "-B", self.build_dir, "-S", str(SCRIPT_DIR)]
        if gen:
            cmd += ["-G", gen]
        cmd.append(f"-DCMAKE_BUILD_TYPE={bt}")

        for k in ("CMAKE_SYSTEM_NAME", "CMAKE_OSX_SYSROOT", "CMAKE_OSX_DEPLOYMENT_TARGET"):
            v = p.get("cacheVariables", {}).get(k)
            if v:
                cmd.append(f"-D{k}={v}")

        for key, _, _ in O2_OPTIONS:
            cmd.append(f"-D{key}={'ON' if self.opt_states[key] else 'OFF'}")

        self.last_count = 0
        self.log_text = ""
        if self.runner.start(cmd):
            self.status = "Running..."
        else:
            self.status = "Already running"

    def do_open_ide(self):
        ide = self.host_ides[self.ide_index] if self.host_ides else ""
        gen = self.host_generators[self.gen_index] if self.host_generators else ""
        self.status = open_ide(ide, self.build_dir, gen)


# --- Entry point ------------------------------------------------------------

def main():
    if not PRESETS_FILE.exists():
        sys.stderr.write(f"Error: CMakePresets.json not found in {SCRIPT_DIR}\n")
        sys.exit(1)

    presets = load_presets()
    app = App(presets)

    runner_params = hello_imgui.RunnerParams()
    runner_params.app_window_params.window_title = "PetStory  -  Project Generator"
    runner_params.app_window_params.window_geometry.size = [720, 720]
    runner_params.imgui_window_params.show_menu_bar = False
    runner_params.imgui_window_params.show_status_bar = False
    runner_params.fps_idling.enable_idling = True
    runner_params.fps_idling.fps_idle = 10.0
    runner_params.callbacks.show_gui = app.gui
    runner_params.callbacks.setup_imgui_style = setup_style

    immapp.run(runner_params)


if __name__ == "__main__":
    main()
