// AI agent: Gemini chat driving the project files and the editor UI.

// ---- AI agent ------------------------------------------------------
(function () {
    var dlg = document.getElementById('ai');
    var back = document.getElementById('ai-back');
    var chatEl = document.getElementById('ai-chat');
    var statusEl = document.getElementById('ai-status');
    var inputEl = document.getElementById('ai-input');
    var sendBtn = document.getElementById('ai-send');
    var stopBtn = document.getElementById('ai-stop');
    var keyEl = document.getElementById('ai-key');
    var modelEl = document.getElementById('ai-model');
    var debugEl = document.getElementById('ai-debug');
    var rawEl = document.getElementById('ai-raw');
    var canvas = document.getElementById('canvas');

    // "steps" expands/collapses every debug step, new ones follow it
    debugEl.onchange = function () {
        chatEl.querySelectorAll('.ai-step').forEach(function (s) {
            s.classList.toggle('open', debugEl.checked);
        });
    };
    // file names in the answer open the file in the assets browser
    chatEl.addEventListener('click', function (e) {
        var el = e.target.closest('.ai-file');
        if (!el || !window.__o2RevealAsset) return;
        window.__o2RevealAsset(el.dataset.path);
    });

    var DEFAULT_MODEL = 'gemini-3.7-flash';
    // shown until the real list is fetched with the key
    var FALLBACK_MODELS = ['gemini-3.7-flash', 'gemini-3.6-flash', 'gemini-3.5-flash',
                           'gemini-3.1-pro-preview', 'gemini-3.5-flash-lite',
                           'gemini-pro-latest', 'gemini-flash-latest', 'gemini-2.5-pro'];

    // settings live in cookies so they survive across tabs and sessions
    // (localStorage is kept as a fallback for values saved earlier)
    function setSetting(name, value) {
        document.cookie = name + '=' + encodeURIComponent(value) +
            ';path=/;max-age=' + (365 * 24 * 3600) + ';SameSite=Lax';
        try { localStorage.setItem(name, value); } catch (e) {}
    }
    function getSetting(name) {
        var m = document.cookie.match(new RegExp('(?:^|; )' + name + '=([^;]*)'));
        if (m) return decodeURIComponent(m[1]);
        try { return localStorage.getItem(name) || ''; } catch (e) { return ''; }
    }

    keyEl.value = getSetting('o2ai_key');
    modelEl.value = getSetting('o2ai_model') || DEFAULT_MODEL;
    keyEl.onchange = function () {
        setSetting('o2ai_key', keyEl.value.trim());
        loadModels();
    };
    modelEl.onchange = function () { setSetting('o2ai_model', modelEl.value.trim()); };

    // ---------- model list ----------
    // non-chat models the agent can't drive a conversation with
    var MODEL_SKIP = /image|nano-banana|tts|lyria|embedding|aqa|gemma|robotics|deep-research|computer-use|antigravity|veo|imagen/i;
    function fillModels(names) {
        var dl = document.getElementById('ai-models');
        dl.innerHTML = '';
        names.forEach(function (n) {
            var o = document.createElement('option');
            o.value = n;
            dl.appendChild(o);
        });
    }
    function modelRank(n) {
        var v = parseFloat((n.match(/gemini-(\d+(\.\d+)?)/) || [0, 0])[1]) || 0;
        if (/-latest$/.test(n)) v = 90;                      // aliases stay near the top
        var tier = /pro/.test(n) ? 2 : /lite/.test(n) ? 0 : 1;
        return v * 10 + tier - (/preview/.test(n) ? 0.5 : 0);
    }
    var modelsLoaded = false;
    function loadModels() {
        var key = keyEl.value.trim();
        if (!key) { fillModels(FALLBACK_MODELS); return Promise.resolve(); }
        return fetch('https://generativelanguage.googleapis.com/v1beta/models?pageSize=200&key=' +
                     encodeURIComponent(key))
            .then(function (r) { return r.json(); })
            .then(function (d) {
                if (!d.models) throw new Error((d.error && d.error.message) || 'no models');
                var names = d.models
                    .filter(function (m) { return (m.supportedGenerationMethods || []).indexOf('generateContent') >= 0; })
                    .map(function (m) { return m.name.replace('models/', ''); })
                    .filter(function (n) { return !MODEL_SKIP.test(n); })
                    .sort(function (a, b) { return modelRank(b) - modelRank(a) || a.localeCompare(b); });
                if (!names.length) throw new Error('no chat models available');
                fillModels(names);
                modelsLoaded = true;
                if (names.indexOf(modelEl.value.trim()) < 0) {
                    modelEl.value = names[0];
                    setSetting('o2ai_model', names[0]);
                }
            })
            .catch(function (e) {
                fillModels(FALLBACK_MODELS);
                console.warn('[ai] model list failed:', e.message);
            });
    }
    fillModels(FALLBACK_MODELS);

    var history = [];          // Gemini `contents`
    var running = false, aborted = false, aborter = null;
    var MAX_STEPS = 60;

    function sleep(ms) { return new Promise(function (r) { setTimeout(r, ms); }); }
    function scrollDown() { chatEl.scrollTop = chatEl.scrollHeight; }

    // ---------- markdown ----------
    function esc(s) {
        return s.replace(/[&<>"]/g, function (c) {
            return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
        });
    }
    var ASSET_EXT = /\.(scn|proto|js|json|png|jpe?g|gif|webp|atlas|anim|ttf|otf|fntstyle|meta|mat|frag|vert|metal|glsl|txt|xml|md|ogg|wav|mp3)$/i;
    function looksLikeAsset(s) {
        s = s.trim();
        return !/\s{2,}|^https?:|^\//.test(s) && ASSET_EXT.test(s);
    }
    function fileLink(path) {
        return '<span class="ai-file" data-path="' + esc(path.trim()) + '">' + esc(path.trim()) + '</span>';
    }
    // inline markdown: code spans, links, bold, italic; asset paths become
    // clickable links into the assets browser
    function mdInline(src) {
        var out = '', rest = src, m;
        var re = /(`[^`]+`)|(\[([^\]]+)\]\(([^)\s]+)\))|(\*\*[^*]+\*\*)|(\*[^*\n]+\*)|(__[^_]+__)/;
        while ((m = re.exec(rest))) {
            out += esc(rest.slice(0, m.index));
            var tok = m[0];
            if (m[1]) {
                var code = tok.slice(1, -1);
                out += looksLikeAsset(code) ? fileLink(code) : '<code>' + esc(code) + '</code>';
            } else if (m[2]) {
                var label = m[3], href = m[4];
                out += /^https?:/.test(href)
                    ? '<a href="' + esc(href) + '" target="_blank" rel="noopener">' + esc(label) + '</a>'
                    : '<span class="ai-file" data-path="' + esc(href) + '">' + esc(label) + '</span>';
            } else if (m[5]) out += '<b>' + esc(tok.slice(2, -2)) + '</b>';
            else if (m[6]) out += '<i>' + esc(tok.slice(1, -1)) + '</i>';
            else out += '<b>' + esc(tok.slice(2, -2)) + '</b>';
            rest = rest.slice(m.index + tok.length);
        }
        return out + esc(rest);
    }
    function renderMarkdown(src) {
        var html = '', lines = String(src).split('\n'), list = null;
        function closeList() { if (list) { html += '</' + list + '>'; list = null; } }
        for (var i = 0; i < lines.length; i++) {
            var line = lines[i];
            var fence = line.match(/^\s*```(\w*)/);
            if (fence) {                                   // fenced code block
                closeList();
                var buf = [];
                for (i++; i < lines.length && !/^\s*```/.test(lines[i]); i++) buf.push(lines[i]);
                html += '<pre><code>' + esc(buf.join('\n')) + '</code></pre>';
                continue;
            }
            var h = line.match(/^(#{1,6})\s+(.*)$/);
            if (h) { closeList(); html += '<h' + Math.min(h[1].length, 3) + '>' + mdInline(h[2]) + '</h' + Math.min(h[1].length, 3) + '>'; continue; }
            if (/^\s*([-*_])\1{2,}\s*$/.test(line)) { closeList(); html += '<hr>'; continue; }
            var ul = line.match(/^\s*[-*+]\s+(.*)$/);
            var ol = line.match(/^\s*\d+[.)]\s+(.*)$/);
            if (ul || ol) {
                var want = ul ? 'ul' : 'ol';
                if (list !== want) { closeList(); html += '<' + want + '>'; list = want; }
                html += '<li>' + mdInline((ul || ol)[1]) + '</li>';
                continue;
            }
            closeList();
            if (!line.trim()) continue;
            html += '<p>' + mdInline(line) + '</p>';
        }
        closeList();
        return html;
    }

    function addMsg(cls, text) {
        var d = document.createElement('div');
        d.className = 'ai-m ' + cls;
        if (cls === 'model') d.innerHTML = renderMarkdown(text);
        else d.textContent = text;
        chatEl.appendChild(d);
        scrollDown();
        return d;
    }

    // a collapsible debug step: header with a summary, body with the details
    function addStep(label, detail) {
        var step = document.createElement('div');
        step.className = 'ai-step' + (debugEl.checked ? ' open' : '');

        var head = document.createElement('div');
        head.className = 'ai-step-head';
        head.appendChild(svgIcon('#i-chev', 'icon ai-chev'));
        var lab = document.createElement('span');
        lab.className = 'ai-step-label';
        lab.textContent = label;
        head.appendChild(lab);

        var body = document.createElement('div');
        body.className = 'ai-step-body';
        body.textContent = detail || '';

        head.onclick = function () { step.classList.toggle('open'); };
        step.appendChild(head);
        step.appendChild(body);
        chatEl.appendChild(step);
        scrollDown();
        return {
            append: function (t) { body.textContent += (body.textContent ? '\n' : '') + t; },
            setLabel: function (t) { lab.textContent = t; },
            label: function () { return lab.textContent; },
            body: body,
        };
    }
    function addShot(b64, mime) {
        var img = document.createElement('img');
        img.className = 'ai-shot';
        img.src = 'data:' + mime + ';base64,' + b64;
        img.onclick = function () { img.classList.toggle('big'); };
        chatEl.appendChild(img);
        scrollDown();
    }
    function setStatus(t) { statusEl.textContent = t || ''; }

    // ---------- environment description for the model ----------
    var SYSTEM = [
        'You are an autonomous AI agent embedded into the web (WebAssembly) build of the o2 game editor, working on the PetStory game project. The editor UI is rendered into a canvas; the hosting page adds an assets browser, a changes dialog and you.',
        '',
        'Environment:',
        "- Project asset files live under Assets/; all file-tool paths are relative to Assets ('' is its root). Asset kinds: .scn scenes (JSON), .proto actor prototypes (JSON), .js game scripts (JavaScript), images (png/jpg), .atlas atlases, .anim animations, .ttf fonts. Every asset and folder has a sibling .meta JSON holding its UID.",
        '- You work on the user\'s private server-side session copy of the project. The running editor keeps its own in-memory copy; write_file updates both. After finishing a batch of file changes call rebuild_assets once so the running editor applies them (takes ~10-20 s).',
        '- Editor layout: top menu bar (File, Edit, View, Run, Help, Debug) with playback controls; left "Tree" panel — scene objects hierarchy; center — Scene view; right — Properties panel (with a Game tab); bottom dock — Log / Animation / Assets windows. The Assets window has a folders tree on the left and an asset icons grid.',
        '- Double-click on an asset icon in the editor\'s Assets window: scene — opens it; prototype (.proto) — instantiates it into the current scene; .js — opens a text editor; others — show in Properties. Double-click on the asset NAME label starts renaming. Reliability note: the first click on a not-yet-selected asset can trigger a slow Properties rebuild that makes the editor miss the double-click — click once to select, wait ~1 s, then double-click.',
        '- screenshot() captures the editor canvas. click/type coordinates are CSS pixels in exactly the screenshot\'s coordinate space, origin at the top-left of the canvas.',
        '- Prefer inspecting and modifying files directly with the file tools; use click/type/screenshot when you need to drive or verify the editor UI itself.',
        '',
        'Work autonomously: plan, call tools, verify the result (screenshot or read back files). When done, reply with a short summary of what you changed.',
    ].join('\n');

    // ---------- tool declarations ----------
    var TOOLS = [{
        functionDeclarations: [
            { name: 'list_files', description: 'List one directory under Assets. Returns subfolder names and file names with sizes.',
              parameters: { type: 'OBJECT', properties: { dir: { type: 'STRING', description: "Directory relative to Assets, '' for the root" } }, required: [] } },
            { name: 'read_file', description: 'Read a text file under Assets. Returns its content (truncated to 100 kB).',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' } }, required: ['path'] } },
            { name: 'write_file', description: 'Create or overwrite a text file under Assets. Also updates the running editor\'s in-memory copy (call rebuild_assets when the batch of edits is done).',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' }, content: { type: 'STRING' } }, required: ['path', 'content'] } },
            { name: 'file_op', description: 'File management under Assets: mkdir, delete, move or copy.',
              parameters: { type: 'OBJECT', properties: { op: { type: 'STRING', description: 'mkdir | delete | move | copy' }, path: { type: 'STRING' }, path2: { type: 'STRING', description: 'destination for move/copy' } }, required: ['op', 'path'] } },
            { name: 'rebuild_assets', description: 'Rebuild the project assets inside the running editor so that file changes take effect. Takes 10-20 seconds and freezes the editor while running; call once per batch of edits.',
              parameters: { type: 'OBJECT', properties: {} } },
            { name: 'screenshot', description: 'Take a screenshot of the editor canvas. The image is attached to the tool reply; the result carries its width and height.',
              parameters: { type: 'OBJECT', properties: {} } },
            { name: 'click', description: 'Mouse click on the editor canvas at CSS-pixel coordinates matching the last screenshot.',
              parameters: { type: 'OBJECT', properties: { x: { type: 'NUMBER' }, y: { type: 'NUMBER' }, button: { type: 'STRING', description: 'left (default) | right' }, double: { type: 'BOOLEAN', description: 'true for a double-click' } }, required: ['x', 'y'] } },
            { name: 'type_text', description: 'Type text into the editor\'s focused control (use after clicking an edit field). \\n presses Enter.',
              parameters: { type: 'OBJECT', properties: { text: { type: 'STRING' } }, required: ['text'] } },
            { name: 'press_key', description: 'Press a key or shortcut in the editor: Enter, Escape, Backspace, Delete, Tab, ArrowLeft/Right/Up/Down, Home, End, or combos like Ctrl+Z, Ctrl+Y, Ctrl+S, Ctrl+C, Ctrl+V, Ctrl+A.',
              parameters: { type: 'OBJECT', properties: { key: { type: 'STRING' } }, required: ['key'] } },
            { name: 'wait', description: 'Wait for the editor to settle (e.g. before a screenshot). Max 5000 ms.',
              parameters: { type: 'OBJECT', properties: { ms: { type: 'NUMBER' } }, required: ['ms'] } },
        ],
    }];

    // ---------- tool implementations ----------
    function fileUrl(path) { return '/api/assets/file?path=' + encodeURIComponent(path); }

    function toolListFiles(a) {
        return fetch('/api/fs/list?dir=' + encodeURIComponent(a.dir || '')).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return r.json();
        }).then(function (d) {
            return {
                dirs: (d.dirs || []).map(function (f) { return f.name; }),
                files: (d.files || []).map(function (f) { return { name: f.name, size: f.size, kind: f.kind }; }),
            };
        });
    }
    function toolReadFile(a) {
        return fetch(fileUrl(a.path)).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status + ' (file not found?)');
            return r.text();
        }).then(function (text) {
            if (text.indexOf('\u0000') >= 0) throw new Error('binary file, ' + text.length + ' bytes');
            var truncated = text.length > 100000;
            return { content: truncated ? text.slice(0, 100000) : text, truncated: truncated };
        });
    }
    function toolWriteFile(a) {
        return fetch(fileUrl(a.path), { method: 'PUT', body: a.content }).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            try {
                if (Module.FS) {
                    var full = '/project/Assets/' + a.path;
                    var dir = full.substring(0, full.lastIndexOf('/'));
                    Module.FS.mkdirTree(dir);
                    Module.FS.writeFile(full, a.content);
                }
            } catch (e) { console.warn('[ai] engine FS sync failed', e); }
            return { ok: true, bytes: a.content.length };
        });
    }
    function toolFileOp(a) {
        return fetch('/api/assets/op', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ op: a.op, path: a.path, path2: a.path2 }),
        }).then(function (r) {
            if (!r.ok) return r.json().then(function (j) { throw new Error(j.error || ('HTTP ' + r.status)); });
            return { ok: true };
        });
    }
    function toolRebuild() {
        if (!Module.calledRun || typeof Module._o2_web_rebuild_assets !== 'function')
            return Promise.reject(new Error('editor engine is not running'));
        return new Promise(function (resolve) {
            setTimeout(function () {
                Module._o2_web_rebuild_assets();
                resolve({ ok: true });
            }, 50);
        });
    }
    function toolScreenshot() {
        var w = canvas.clientWidth, h = canvas.clientHeight;
        var t = document.createElement('canvas');
        t.width = w; t.height = h;
        t.getContext('2d').drawImage(canvas, 0, 0, w, h);
        var b64 = t.toDataURL('image/jpeg', 0.85).split(',')[1];
        return Promise.resolve({ result: { width: w, height: h, note: 'image attached in the next part' },
                                 image: { mime: 'image/jpeg', b64: b64 } });
    }

    function focusCanvas() {
        if (document.activeElement && document.activeElement !== canvas && document.activeElement.blur)
            document.activeElement.blur();
        canvas.focus();
    }
    function fireMouse(type, cx, cy, extra) {
        var isPointer = type.indexOf('pointer') === 0;
        var Ctor = isPointer && window.PointerEvent ? PointerEvent : MouseEvent;
        canvas.dispatchEvent(new Ctor(type, Object.assign({
            bubbles: true, cancelable: true, view: window,
            clientX: cx, clientY: cy,
            pointerId: 1, pointerType: 'mouse', isPrimary: true,
        }, extra)));
    }
    function toolClick(a) {
        return (async function () {
            var r = canvas.getBoundingClientRect();
            var cx = r.left + Number(a.x), cy = r.top + Number(a.y);
            focusCanvas();
            fireMouse('pointermove', cx, cy, { buttons: 0 });
            fireMouse('mousemove', cx, cy, { buttons: 0 });
            await sleep(300); // the engine dispatches presses to last-frame under-cursor listeners
            var btn = a.button === 'right' ? 2 : 0;
            var buttons = btn === 2 ? 2 : 1;
            var n = a.double ? 2 : 1;
            for (var i = 1; i <= n; i++) {
                fireMouse('pointerdown', cx, cy, { button: btn, buttons: buttons, detail: i });
                fireMouse('mousedown', cx, cy, { button: btn, buttons: buttons, detail: i });
                await sleep(40);
                fireMouse('pointerup', cx, cy, { button: btn, buttons: 0, detail: i });
                fireMouse('mouseup', cx, cy, { button: btn, buttons: 0, detail: i });
                if (i < n) await sleep(110);
            }
            await sleep(200);
            return { ok: true };
        })();
    }
    var CHAR_CODES = { ' ': 'Space', '.': 'Period', ',': 'Comma', '/': 'Slash', ';': 'Semicolon', "'": 'Quote',
        '[': 'BracketLeft', ']': 'BracketRight', '-': 'Minus', '=': 'Equal', '`': 'Backquote', '\\': 'Backslash',
        '!': 'Digit1', '@': 'Digit2', '#': 'Digit3', '$': 'Digit4', '%': 'Digit5', '^': 'Digit6', '&': 'Digit7',
        '*': 'Digit8', '(': 'Digit9', ')': 'Digit0', '_': 'Minus', '+': 'Equal', '{': 'BracketLeft',
        '}': 'BracketRight', ':': 'Semicolon', '"': 'Quote', '<': 'Comma', '>': 'Period', '?': 'Slash',
        '~': 'Backquote', '|': 'Backslash' };
    function codeForChar(ch) {
        if (/[a-zA-Z]/.test(ch)) return 'Key' + ch.toUpperCase();
        if (/[0-9]/.test(ch)) return 'Digit' + ch;
        return CHAR_CODES[ch] || '';
    }
    function fireKey(type, key, code, mods) {
        canvas.dispatchEvent(new KeyboardEvent(type, Object.assign({
            bubbles: true, cancelable: true, key: key, code: code,
        }, mods || {})));
    }
    function toolTypeText(a) {
        return (async function () {
            focusCanvas();
            await sleep(100);
            var text = String(a.text);
            for (var i = 0; i < text.length; i++) {
                var ch = text[i];
                if (ch === '\n') { fireKey('keydown', 'Enter', 'Enter'); fireKey('keyup', 'Enter', 'Enter'); }
                else {
                    var code = codeForChar(ch);
                    fireKey('keydown', ch, code, { shiftKey: /[A-Z~!@#$%^&*()_+{}:"<>?|]/.test(ch) });
                    fireKey('keyup', ch, code);
                }
                await sleep(45);
            }
            return { ok: true };
        })();
    }
    var NAMED_KEYS = { Enter: 'Enter', Escape: 'Escape', Backspace: 'Backspace', Delete: 'Delete', Tab: 'Tab',
        ArrowLeft: 'ArrowLeft', ArrowRight: 'ArrowRight', ArrowUp: 'ArrowUp', ArrowDown: 'ArrowDown',
        Home: 'Home', End: 'End', PageUp: 'PageUp', PageDown: 'PageDown', Space: 'Space' };
    function toolPressKey(a) {
        return (async function () {
            focusCanvas();
            await sleep(100);
            var parts = String(a.key).split('+');
            var main = parts.pop();
            var mods = { ctrlKey: false, shiftKey: false, altKey: false, metaKey: false };
            parts.forEach(function (m) {
                m = m.toLowerCase();
                if (m === 'ctrl' || m === 'control') mods.ctrlKey = true;
                else if (m === 'shift') mods.shiftKey = true;
                else if (m === 'alt') mods.altKey = true;
                else if (m === 'meta' || m === 'cmd') mods.metaKey = true;
            });
            var key, code;
            if (NAMED_KEYS[main]) { key = main === 'Space' ? ' ' : main; code = NAMED_KEYS[main]; }
            else if (main.length === 1) { key = mods.ctrlKey || mods.altKey ? main.toLowerCase() : main; code = codeForChar(main); }
            else { key = main; code = main; }
            if (mods.ctrlKey) fireKey('keydown', 'Control', 'ControlLeft', { ctrlKey: true });
            if (mods.shiftKey) fireKey('keydown', 'Shift', 'ShiftLeft', { shiftKey: true });
            if (mods.altKey) fireKey('keydown', 'Alt', 'AltLeft', { altKey: true });
            await sleep(40);
            fireKey('keydown', key, code, mods);
            await sleep(40);
            fireKey('keyup', key, code, mods);
            if (mods.altKey) fireKey('keyup', 'Alt', 'AltLeft');
            if (mods.shiftKey) fireKey('keyup', 'Shift', 'ShiftLeft');
            if (mods.ctrlKey) fireKey('keyup', 'Control', 'ControlLeft');
            await sleep(100);
            return { ok: true };
        })();
    }
    function toolWait(a) {
        var ms = Math.min(Math.max(Number(a.ms) || 0, 0), 5000);
        return sleep(ms).then(function () { return { ok: true, waited: ms }; });
    }

    var EXEC = {
        list_files: toolListFiles, read_file: toolReadFile, write_file: toolWriteFile,
        file_op: toolFileOp, rebuild_assets: toolRebuild, screenshot: toolScreenshot,
        click: toolClick, type_text: toolTypeText, press_key: toolPressKey, wait: toolWait,
    };
    window.__o2aiExec = EXEC; // debug/testing handle

    function partKind(p) {
        return p.thought ? 'thought' : p.text ? 'text'
             : p.functionCall ? 'functionCall:' + p.functionCall.name
             : p.functionResponse ? 'functionResponse' : p.inlineData ? 'inlineData' : 'other';
    }

    function toolLabel(name, args) {
        var brief = Object.keys(args || {}).map(function (k) {
            var v = String(args[k]);
            return k + ': ' + (v.length > 60 ? v.slice(0, 60) + '…' : v);
        }).join(', ');
        return '▸ ' + name + '(' + brief + ')';
    }

    // ---------- Gemini call + agent loop ----------
    var rawLog = [];   // full request/response pairs, for the Copy log button

    function callModelOnce(key, model) {
        aborter = new AbortController();
        var body = {
            systemInstruction: { parts: [{ text: SYSTEM }] },
            contents: history,
            tools: TOOLS,
            // without this the model never sends its reasoning back
            generationConfig: { thinkingConfig: { includeThoughts: true } },
        };
        var started = Date.now();
        return fetch('https://generativelanguage.googleapis.com/v1beta/models/' +
                     encodeURIComponent(model) + ':generateContent?key=' + encodeURIComponent(key), {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            signal: aborter.signal,
            body: JSON.stringify(body),
        }).then(function (r) {
            return r.json().then(function (j) {
                var entry = { model: model, ms: Date.now() - started, request: body, response: j, status: r.status };
                rawLog.push(entry);
                console.log('[ai] ' + model + ' ' + r.status + ' in ' + entry.ms + 'ms', entry);
                if (rawEl.checked) {
                    addStep('◇ request → ' + model + '  (' + history.length + ' turns)',
                            JSON.stringify(body, null, 2));
                    addStep('◇ response ← ' + r.status + '  ' + entry.ms + 'ms',
                            JSON.stringify(j, null, 2));
                }
                if (!r.ok) {
                    var err = new Error((j.error && j.error.message) || ('HTTP ' + r.status));
                    err.status = r.status;
                    throw err;
                }
                return j;
            });
        });
    }
    // overloaded/rate-limited models answer 429/503 for a while; back off instead of giving up
    async function callModel(key, model) {
        var delays = [4000, 10000, 20000, 40000];
        for (var attempt = 0; ; attempt++) {
            try { return await callModelOnce(key, model); }
            catch (e) {
                var retryable = e.status === 429 || e.status === 503 || e.status === 500;
                if (aborted || !retryable || attempt >= delays.length) throw e;
                setStatus('model busy (' + e.status + '), retry in ' + (delays[attempt] / 1000) + ' s…');
                await sleep(delays[attempt]);
                if (aborted) throw e;
            }
        }
    }

    async function runAgent(userText) {
        var key = keyEl.value.trim();
        var model = modelEl.value.trim() || DEFAULT_MODEL;
        if (!key) { addMsg('error', 'Set the Gemini API key in the settings field above.'); return; }
        setSetting('o2ai_key', key);
        setSetting('o2ai_model', model);

        history.push({ role: 'user', parts: [{ text: userText }] });
        addMsg('user', userText);

        running = true; aborted = false;
        sendBtn.style.display = 'none';
        stopBtn.style.display = '';
        inputEl.disabled = true;

        try {
            for (var step = 1; step <= MAX_STEPS && !aborted; step++) {
                setStatus('step ' + step + ' · ' + model + '…');
                var resp;
                try { resp = await callModel(key, model); }
                catch (e) {
                    if (aborted) break;
                    addMsg('error', 'API error: ' + e.message +
                        (e.status === 503 || e.status === 429
                            ? '\nThe model stays overloaded — pick another one in the Model field and send again.'
                            : ''));
                    break;
                }
                var cand = resp.candidates && resp.candidates[0];
                if (!cand || !cand.content) {
                    addMsg('error', 'Empty model response' +
                        (resp.promptFeedback ? ' (' + JSON.stringify(resp.promptFeedback) + ')' : ''));
                    break;
                }
                var parts = cand.content.parts || [];
                history.push({ role: 'model', parts: parts });

                var u = resp.usageMetadata || {};
                addStep('· step ' + step + ' · ' + (cand.finishReason || 'OK') +
                        ' · in ' + (u.promptTokenCount || 0) +
                        ' / think ' + (u.thoughtsTokenCount || 0) +
                        ' / out ' + (u.candidatesTokenCount || 0) + ' tok',
                        JSON.stringify({ finishReason: cand.finishReason, usage: u,
                                         safetyRatings: cand.safetyRatings,
                                         partKinds: parts.map(partKind) }, null, 2));

                // render every part the model sent, whatever its kind
                parts.forEach(function (p) {
                    if (p.text && p.thought)
                        addStep('🧠 thinking · ' + ((p.text.replace(/[#*`]/g, '').split('\n')
                                    .filter(function (l) { return l.trim(); })[0] || 'reasoning').slice(0, 90)),
                                p.text);
                    else if (p.text) addMsg('model', p.text);
                    else if (p.functionCall) { /* rendered below, with its result */ }
                    else if (p.inlineData) addShot(p.inlineData.data, p.inlineData.mimeType);
                    else addStep('? unknown part', JSON.stringify(p, null, 2));
                });

                var calls = parts.filter(function (p) { return p.functionCall; });
                if (!calls.length) break;

                var respParts = [], imageParts = [];
                for (var i = 0; i < calls.length && !aborted; i++) {
                    var fc = calls[i].functionCall;
                    var uiStep = addStep(toolLabel(fc.name, fc.args),
                                         'args ' + JSON.stringify(fc.args || {}, null, 2));
                    setStatus('step ' + step + ' · ' + fc.name + '…');
                    var toolStarted = Date.now();
                    var response;
                    try {
                        var impl = EXEC[fc.name];
                        if (!impl) throw new Error('unknown tool');
                        var out = await impl(fc.args || {});
                        if (out && out.image) {
                            imageParts.push({ inlineData: { mimeType: out.image.mime, data: out.image.b64 } });
                            addShot(out.image.b64, out.image.mime);
                            response = out.result;
                        } else
                            response = out;
                    } catch (e) {
                        response = { error: String(e && e.message || e) };
                        uiStep.setLabel('✗ ' + fc.name + ': ' + response.error);
                    }
                    var dump = JSON.stringify(response, null, 2);
                    var took = Date.now() - toolStarted;
                    uiStep.append('result (' + took + 'ms) ' +
                                  (dump.length > 8000 ? dump.slice(0, 8000) + '\n… truncated' : dump));
                    uiStep.setLabel(uiStep.label() + '  ' + took + 'ms');
                    console.log('[ai] tool ' + fc.name, { args: fc.args, response: response, ms: took });
                    respParts.push({ functionResponse: { name: fc.name, response: response } });
                }
                history.push({ role: 'user', parts: respParts.concat(imageParts) });
            }
            if (aborted) addMsg('error', 'Stopped.');
            else if (step > MAX_STEPS) addMsg('error', 'Step limit reached (' + MAX_STEPS + ').');
        } finally {
            running = false;
            setStatus('');
            sendBtn.style.display = '';
            stopBtn.style.display = 'none';
            inputEl.disabled = false;
        }
    }

    // ---------- UI wiring ----------
    function openDlg() {
        if (window.__o2CloseBrowser) window.__o2CloseBrowser(); // it renders above the chat
        dlg.classList.add('open');
        back.classList.add('open');
        inputEl.focus();
        if (!modelsLoaded) loadModels();
    }
    function closeDlg() { dlg.classList.remove('open'); back.classList.remove('open'); }
    document.getElementById('btn-ai').onclick = function () {
        if (dlg.classList.contains('open')) closeDlg(); else openDlg();
    };
    document.getElementById('ai-close').onclick = closeDlg;
    back.onclick = closeDlg; // the agent keeps running in the background

    document.getElementById('ai-clear').onclick = function () {
        if (running) return;
        history = [];
        rawLog = [];
        chatEl.innerHTML = '';
    };
    // whole session as JSON: conversation plus every raw API exchange
    document.getElementById('ai-copy').onclick = function () {
        var dump = JSON.stringify({ model: modelEl.value.trim(), history: history, exchanges: rawLog }, null, 2);
        console.log('[ai] session log', { history: history, exchanges: rawLog });
        var btn = document.getElementById('ai-copy');
        navigator.clipboard.writeText(dump).then(function () {
            btn.textContent = 'Copied';
            setTimeout(function () { btn.textContent = 'Log'; }, 1200);
        }, function () {
            // clipboard blocked (no focus / insecure origin): fall back to a download
            var a = document.createElement('a');
            a.href = URL.createObjectURL(new Blob([dump], { type: 'application/json' }));
            a.download = 'ai-session.json';
            a.click();
            URL.revokeObjectURL(a.href);
        });
    };
    stopBtn.onclick = function () {
        aborted = true;
        if (aborter) aborter.abort();
    };
    function send() {
        if (running) return;
        var t = inputEl.value.trim();
        if (!t) return;
        inputEl.value = '';
        runAgent(t);
    }
    sendBtn.onclick = send;
    inputEl.addEventListener('keydown', function (e) {
        if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); send(); }
        e.stopPropagation(); // don't leak typing into the engine
    });
    inputEl.addEventListener('keyup', function (e) { e.stopPropagation(); });
})();
