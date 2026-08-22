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
    var reviewEl = document.getElementById('ai-review');
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
    var MAX_STEPS = 1200;

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
    function addStep(label, detail, opts) {
        opts = opts || {};
        var step = document.createElement('div');
        step.className = 'ai-step' + (debugEl.checked || opts.open ? ' open' : '') +
                         (opts.kind ? ' ' + opts.kind : '');

        var head = document.createElement('div');
        head.className = 'ai-step-head';
        head.appendChild(svgIcon('#i-chev', 'icon ai-chev'));
        var lab = document.createElement('span');
        lab.className = 'ai-step-label';
        lab.textContent = label;
        head.appendChild(lab);

        var body = document.createElement('div');
        body.className = 'ai-step-body' + (opts.markdown ? ' md' : '');
        if (opts.markdown) body.innerHTML = renderMarkdown(detail || '');
        else body.textContent = detail || '';

        head.onclick = function () { step.classList.toggle('open'); };
        step.appendChild(head);
        step.appendChild(body);
        chatEl.appendChild(step);
        scrollDown();
        return {
            append: function (t) {
                if (opts.markdown) body.innerHTML += renderMarkdown(t);
                else body.textContent += (body.textContent ? '\n' : '') + t;
            },
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
        'You are an autonomous AI agent embedded into the web (WebAssembly) build of the o2 game editor, working on the',
        'PetStory game project. The editor UI is rendered into a canvas; the hosting page adds an assets browser, a',
        'changes dialog and you. You act on the project through tools and you can also drive the editor UI itself.',
        '',
        'TWO WORLDS, do not confuse them:',
        '- The project assets (Assets/) are a private server-side copy for this browser session. Read and write them',
        '  freely with list_files / search_files / read_file / edit_file / write_file / file_op; paths are relative to',
        "  Assets ('' is its root). Nobody else sees this copy.",
        '- The o2 engine checkout is shared and READ-ONLY reference material: its documentation and C++ sources, reachable',
        '  with search_engine / read_engine_file / list_engine_dir (roots: o2/Docs, o2/Framework/Sources, o2/Editor/Sources,',
        '  o2/AssetsBuildTool, Sources). You cannot change it and cannot compile C++ - deliver through assets, scripts and',
        '  the editor UI.',
        '',
        'KNOW BEFORE YOU ACT. read_guide holds briefings distilled from that documentation. Topics: project (what PetStory',
        'is, how C++ and JS split the work), assets (the JSON shape of scenes, prototypes and .meta, and the concrete ways',
        'to corrupt them), scripting (the JavaScript model, the real lifecycle hooks, the actor API), editor (windows, modes,',
        'hotkeys), particles (the emitter, its effects and how to configure one), workflows (recipes for the usual jobs).',
        'Rule: if you are about to write or change a .js script, a .scn or',
        '.proto, or to drive the editor UI, and you have not read the matching guide in this conversation, read it first.',
        'The guides are authoritative for what they state: field names, defaults, the JSON shape, which calls are',
        'scriptable. Do not re-verify them against the C++ headers - that is the single biggest source of wasted steps.',
        'Go to search_engine / read_engine_file only for what a guide does not cover, and then read one targeted place',
        'rather than browsing.',
        '',
        'FACTS THAT BREAK THINGS SILENTLY IF YOU GET THEM WRONG (the guides explain them):',
        '- A script file must define a class whose name equals the file name, assigned to the global without let/const:',
        '  Name = class Name extends o2.Component { ... }. The only lifecycle hooks that exist are OnStart(), OnEnabled(),',
        '  OnDisabled() and Update(dt) - a method called OnUpdate or OnDestroy is dead code. Log with print(), not',
        '  console.log, and never Dump() or enumerate the JS global.',
        '- A window script reports to C++ through the property C++ injects, which is named exactly `action`:',
        "  this.action('close'). Inventing another callback name compiles fine and is simply never called.",
        '- Never change an asset uid, an actor Id or a PrototypeLink number, and always move a .meta together with its file:',
        '  every reference elsewhere is by uid and breaks quietly, leaving a prototype instance as a near-empty actor.',
        '',
        'HOW TO WORK - in three phases, not in a hundred small moves:',
        '1. Gather. Collect what you need in a handful of calls: read the guide that matches the job (cheaper than',
        '   guessing an API, and far cheaper than the errors that follow), search for the files involved, read the parts',
        '   you will actually change. Do not start editing while you are still unsure what to write.',
        '2. Plan. State the plan in one short paragraph: which files change, what each change is, how you will verify.',
        '   If the job needs an experiment, design one experiment that answers several questions at once.',
        '3. Act in batches. Apply the whole set of edits, then rebuild once, then verify once. Do not rebuild, screenshot',
        '   or re-read between every small step - a rebuild costs 10-20 seconds and every call costs a round trip.',
        'When something fails, go back to gathering: read the guide or the engine source, then fix it in one go. Trying',
        'variant after variant of an API you have not looked up is the most expensive thing you can do.',
        '',
        'run_script talks to the running engine and nothing else. It is not a text processor: never use it to assemble',
        'JSON or strings for a file - write the content directly with write_file or edit_file. It has no Node, no require',
        'and no filesystem, and long results come back truncated.',
        '',
        'WORKING RULES:',
        '- Locate things with search_files (project) or search_engine (engine) rather than reading files to look for them.',
        '- After creating or changing a scene, open exactly that scene before entering play mode. scene_tree and view_info',
        '  report openScene, so check it instead of assuming: hunting your object in the wrong scene wastes a whole run.',
        '- Change an existing file with edit_file, not by rewriting it with write_file.',
        '- Asset content is loaded from the built copy, so file changes need rebuild_assets to take effect. It freezes the',
        '  editor for 10-20 seconds: make all your edits first, then rebuild once.',
        '- Verify with read_log, which carries the engine\'s own output including build errors and script exceptions.',
        '  Take a screenshot only when the thing you changed is visual.',
        '- NEVER try to operate the editor with the cursor. Menus, the Assets window, the Tree and Properties ignore',
        '  synthetic clicks entirely, and aiming clicks from a screenshot wastes steps and gets you nowhere.',
        '  click/type_text/press_key exist for one purpose only: playing the running game, inside the Game window,',
        '  while play mode is on. Everything else you do through the file tools, run_script and the editor tools',
        '  (open_scene, save_scene, rebuild_assets, play_mode).',
        '- To see the scene, read it: scene_tree gives names, paths, transforms, widget layouts and components, and',
        '  view_info gives the canvas size, the Game window rectangle, the camera and the formula to convert a world',
        '  position into a canvas pixel. Aim clicks from those numbers, never from a picture.',
        '- run_script executes JavaScript inside the engine right away, with sceneRoots and findActor(path) available and',
        '  the whole o2 namespace bound. Use it to inspect or change the scene, and to do editor work no tool covers -',
        '  it needs neither a script asset on the scene nor play mode.',
        '- To watch runtime behaviour (animation, particles, gameplay), call play_mode({on:true}), wait a moment, take a',
        '  screenshot, then play_mode({on:false}). screenshot() returns a half-size image; click coordinates are full-size',
        '  canvas pixels, so double what you measure on the picture.',
        '- Context discipline: everything a tool returns is re-sent to you on every later step, so keep results small.',
        '  Read the window of lines you need (read_file takes offset/limit), and pull a parked result back with read_output',
        '  only as far as you need it.',
        '',
        'Work autonomously: plan, use the tools, verify the result. Say plainly when something did not work or when you are',
        'unsure rather than reporting success. When done, reply with a short summary of what you changed and what you checked.',
    ].join('\n');

    // ---------- tool declarations ----------
    var TOOLS = [{
        functionDeclarations: [
            { name: 'list_files', description: 'List one directory under Assets. Returns subfolder names and file names with sizes.',
              parameters: { type: 'OBJECT', properties: { dir: { type: 'STRING', description: "Directory relative to Assets, '' for the root" } }, required: [] } },
            { name: 'search_files', description: 'Search the text of all assets (grep). Returns path, line number and the matching line. Use this to locate a symbol or a string instead of paging through files.',
              parameters: { type: 'OBJECT', properties: { query: { type: 'STRING' }, glob: { type: 'STRING', description: "limit to matching file names, e.g. '*.js'" }, regex: { type: 'BOOLEAN', description: 'treat query as a POSIX regular expression' } }, required: ['query'] } },
            { name: 'read_file', description: 'Read a text file under Assets. Returns a window of lines (400 by default, 20 kB max) plus the total line count - request the next window with offset instead of pulling whole files.',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' }, offset: { type: 'NUMBER', description: 'first line to return, 1-based' }, limit: { type: 'NUMBER', description: 'how many lines' } }, required: ['path'] } },
            { name: 'edit_file', description: 'Replace an exact fragment of a text file under Assets. Preferred over write_file for changing an existing file: only the fragment travels. old_text must match the file exactly (whitespace included) and be unique, unless replaceAll is set.',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' }, old: { type: 'STRING', description: 'exact text to replace' }, new: { type: 'STRING', description: 'replacement text' }, replaceAll: { type: 'BOOLEAN' } }, required: ['path', 'old', 'new'] } },
            { name: 'write_file', description: 'Create a file or replace one wholesale under Assets. Use edit_file to change part of an existing file. Also updates the running editor\'s in-memory copy (call rebuild_assets when the batch of edits is done).',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' }, content: { type: 'STRING' } }, required: ['path', 'content'] } },
            { name: 'read_output', description: 'Read a result that was too large to inline, in windows. Takes the outputId from that result.',
              parameters: { type: 'OBJECT', properties: { outputId: { type: 'STRING' }, offset: { type: 'NUMBER', description: 'byte offset, 0-based' } }, required: ['outputId'] } },
            { name: 'read_guide', description: 'Read the built-in briefing on this project and engine. Call it before engine-specific work rather than rediscovering things: topics are listed by calling it with no arguments.',
              parameters: { type: 'OBJECT', properties: { topic: { type: 'STRING' } }, required: [] } },
            { name: 'search_engine', description: 'Search the o2 engine checkout on the server (its documentation, framework, editor and game C++ sources). Read-only reference material - use it to look up how the engine or the editor works.',
              parameters: { type: 'OBJECT', properties: { query: { type: 'STRING' }, glob: { type: 'STRING', description: "limit to matching file names, e.g. '*.md' for documentation or '*.h' for headers" }, regex: { type: 'BOOLEAN' } }, required: ['query'] } },
            { name: 'read_engine_file', description: 'Read a file from the engine checkout, in windows like read_file. Paths are repo-relative, e.g. o2/Docs/en/main.md or o2/Framework/Sources/o2/Scene/Actor.h. Readable subtrees: o2/Docs, o2/Framework/Sources, o2/Editor/Sources, o2/AssetsBuildTool, Sources.',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' }, offset: { type: 'NUMBER' }, limit: { type: 'NUMBER' } }, required: ['path'] } },
            { name: 'list_engine_dir', description: 'List a directory of the engine checkout (documentation and sources).',
              parameters: { type: 'OBJECT', properties: { dir: { type: 'STRING', description: "repo-relative, '' lists the readable roots" } }, required: [] } },
            { name: 'file_op', description: 'File management under Assets: mkdir, delete, move or copy.',
              parameters: { type: 'OBJECT', properties: { op: { type: 'STRING', description: 'mkdir | delete | move | copy' }, path: { type: 'STRING' }, path2: { type: 'STRING', description: 'destination for move/copy' } }, required: ['op', 'path'] } },
            { name: 'rebuild_assets', description: 'Rebuild the project assets inside the running editor so that file changes take effect. Takes 10-20 seconds and freezes the editor while running; call once per batch of edits. Pass force to rebuild everything from scratch - do that if an asset or a scene fails to load, since the incremental build trusts its own index and cannot heal a missing built file.',
              parameters: { type: 'OBJECT', properties: { force: { type: 'BOOLEAN' } } } },
            { name: 'screenshot', description: 'Take a screenshot of the editor canvas. The image is attached to the tool reply; the result carries its width and height.',
              parameters: { type: 'OBJECT', properties: {} } },
            { name: 'scene_tree', description: 'Read the hierarchy of the open scene: names, paths, types, enabled state, transforms (position, size, scale, angle, world rectangle), widget layout (anchors and offsets), component types and children. This is how you see the scene - never guess object positions from a screenshot.',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING', description: 'start at this actor path instead of the scene roots' }, depth: { type: 'NUMBER', description: 'how many levels of children, default 3' } }, required: [] } },
            { name: 'view_info', description: 'Canvas size, play state, the Game window rectangle and the game camera, plus the formula for turning a world position into a canvas pixel. Use it together with scene_tree to aim a click.',
              parameters: { type: 'OBJECT', properties: {} } },
            { name: 'run_script', description: 'Run JavaScript inside the running engine and return its result as text. The scene is reachable through the globals sceneRoots (array of root actors) and findActor(path); the o2 namespace holds the bound engine types. This is the way to do anything the other tools do not cover - inspect or change actors, components and assets - without adding a script to the scene or entering play mode.',
              parameters: { type: 'OBJECT', properties: { code: { type: 'STRING', description: 'JavaScript; the value of the last expression is returned as a string' } }, required: ['code'] } },
            { name: 'open_scene', description: 'Open a scene in the editor by its path under Assets, e.g. Boot.scn. This is the only way to open a scene - the editor windows do not take synthetic clicks.',
              parameters: { type: 'OBJECT', properties: { path: { type: 'STRING' } }, required: ['path'] } },
            { name: 'save_scene', description: 'Save the scene currently open in the editor back to its file.',
              parameters: { type: 'OBJECT', properties: {} } },
            { name: 'play_mode', description: 'Start or stop the editor play mode. While playing, the scene runs in the Game window and the input tools work; stopping restores the scene. Use this to actually see runtime behaviour such as animations or particles.',
              parameters: { type: 'OBJECT', properties: { on: { type: 'BOOLEAN', description: 'true starts play, false stops it' } }, required: [] } },
            { name: 'click', description: 'Mouse click inside the Game window, in full-size canvas pixels. Only works in play mode: the editor chrome (Assets, Tree, Properties, menus) cannot be driven - change the project with the file tools instead.',
              parameters: { type: 'OBJECT', properties: { x: { type: 'NUMBER' }, y: { type: 'NUMBER' }, button: { type: 'STRING', description: 'left (default) | right' }, double: { type: 'BOOLEAN', description: 'true for a double-click' } }, required: ['x', 'y'] } },
            { name: 'type_text', description: 'Type text into the running game (play mode only). \\n presses Enter.',
              parameters: { type: 'OBJECT', properties: { text: { type: 'STRING' } }, required: ['text'] } },
            { name: 'press_key', description: 'Press a key in the running game (play mode only): Enter, Escape, Backspace, Delete, Tab, ArrowLeft/Right/Up/Down, Home, End, or combos like Ctrl+Z.',
              parameters: { type: 'OBJECT', properties: { key: { type: 'STRING' } }, required: ['key'] } },
            { name: 'read_log', description: 'Read what the engine itself printed (the editor Log window contents, including asset build errors and script exceptions). Check this after rebuild_assets or after changing a script instead of reading the Log window off a screenshot.',
              parameters: { type: 'OBJECT', properties: { lines: { type: 'NUMBER', description: 'how many last lines, default 60' }, filter: { type: 'STRING', description: 'only lines containing this text' } }, required: [] } },
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
    // Everything a tool returns stays in the conversation forever and is re-sent
    // on every later step, so reads are bounded by default and anything bulky is
    // parked in `outputs` and fetched back in pieces with read_output
    // The default read window is deliberately smaller than INLINE_LIMIT: a normal
    // read then comes back inline, and only unusually bulky results get parked
    var READ_LINES = 300;         // lines returned when the model asks for no range
    var READ_MAX_BYTES = 6000;    // hard cap on one file read
    var INLINE_LIMIT = 8000;      // above this a result is parked instead of inlined
    var CHUNK_BYTES = 6000;       // read_output window

    var outputs = new Map();      // id -> full text of a parked result
    var outputSeq = 0;

    function park(text, note) {
        var id = 'out' + (++outputSeq);
        outputs.set(id, text);
        return {
            outputId: id,
            totalBytes: text.length,
            preview: text.slice(0, 2000),
            note: (note ? note + ' ' : '') +
                  'Result too large to inline; read the rest with read_output(outputId, offset).',
        };
    }

    function fetchText(path) {
        return fetch(fileUrl(path)).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status + ' (file not found?)');
            return r.text();
        }).then(function (text) {
            if (text.indexOf('\u0000') >= 0) throw new Error('binary file, ' + text.length + ' bytes');
            return text;
        });
    }

    function toolSearchFiles(a) {
        var q = '/api/assets/search?q=' + encodeURIComponent(a.query || '');
        if (a.glob) q += '&glob=' + encodeURIComponent(a.glob);
        if (a.regex) q += '&regex=1';
        return fetch(q).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return r.json();
        });
    }

    // shared by the session and the engine reader: a bounded window plus the
    // pointers the model needs to continue
    function windowText(text, a, extra) {
        var lines = text.split('\n');
        var from = Math.max(1, Number(a.offset) || 1);
        var count = Math.max(1, Number(a.limit) || READ_LINES);
        var slice = lines.slice(from - 1, from - 1 + count);
        var content = slice.join('\n');
        var cut = false;
        if (content.length > READ_MAX_BYTES) {
            content = content.slice(0, READ_MAX_BYTES);
            slice = content.split('\n');
            cut = true;
        }
        if (!slice.length)
            return Object.assign({ path: a.path, totalLines: lines.length, from: from, to: from - 1, content: '',
                                   more: 'offset is past the end of the file (' + lines.length + ' lines)' }, extra);

        var to = from + slice.length - 1;
        var res = Object.assign({ path: a.path, totalLines: lines.length, from: from, to: to, content: content }, extra);
        if (to < lines.length || cut)
            res.more = 'showing lines ' + from + '-' + to + ' of ' + lines.length +
                       '; continue with offset ' + (to + 1);
        return res;
    }

    function toolReadFile(a) {
        return fetchText(a.path).then(function (text) { return windowText(text, a); });
    }

    function toolReadGuide(a) {
        var topics = Object.keys(AI_GUIDES);
        if (!a.topic)
            return Promise.resolve({ topics: topics, note: 'call read_guide with one of these topics' });
        var text = AI_GUIDES[a.topic];
        if (!text)
            return Promise.reject(new Error('no such topic; available: ' + topics.join(', ')));
        return Promise.resolve({ topic: a.topic, text: text });
    }

    // ---- the engine checkout: read-only, shared, never the user's session ----
    function toolSearchEngine(a) {
        var q = '/api/engine/search?q=' + encodeURIComponent(a.query || '');
        if (a.glob) q += '&glob=' + encodeURIComponent(a.glob);
        if (a.regex) q += '&regex=1';
        return fetch(q).then(function (r) { return r.json(); }).then(function (d) {
            if (d.error) throw new Error(d.error);
            return d;
        });
    }

    function toolReadEngineFile(a) {
        return fetch('/api/engine/file?path=' + encodeURIComponent(a.path)).then(function (r) {
            return r.json();
        }).then(function (d) {
            if (d.error) throw new Error(d.error);
            return windowText(d.text, a, { source: 'engine (read-only)' });
        });
    }

    function toolListEngineDir(a) {
        return fetch('/api/engine/list?dir=' + encodeURIComponent(a.dir || '')).then(function (r) {
            return r.json();
        }).then(function (d) {
            if (d.error) throw new Error(d.error);
            return d;
        });
    }

    function saveFile(path, content) {
        return fetch(fileUrl(path), { method: 'PUT', body: content }).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            // the running editor works off its own MEMFS copy, keep it in step
            try {
                if (Module.FS) {
                    var full = '/project/Assets/' + path;
                    Module.FS.mkdirTree(full.substring(0, full.lastIndexOf('/')));
                    Module.FS.writeFile(full, content);
                }
            } catch (e) { console.warn('[ai] engine FS sync failed', e); }
        });
    }

    function toolWriteFile(a) {
        return saveFile(a.path, a.content).then(function () {
            return { ok: true, bytes: a.content.length };
        });
    }

    function toolEditFile(a) {
        return fetchText(a.path).then(function (text) {
            var hits = text.split(a.old).length - 1;
            if (!hits)
                throw new Error('old_text not found in ' + a.path +
                                ' - read the file again and copy the exact text, whitespace included');
            if (hits > 1 && !a.replaceAll)
                throw new Error('old_text matches ' + hits + ' times in ' + a.path +
                                ' - include more surrounding text, or pass replaceAll');
            var updated = a.replaceAll ? text.split(a.old).join(a.new) : text.replace(a.old, a.new);
            return saveFile(a.path, updated).then(function () {
                return { ok: true, replaced: a.replaceAll ? hits : 1, bytes: updated.length };
            });
        });
    }

    function toolReadOutput(a) {
        var text = outputs.get(a.outputId);
        if (text === undefined)
            return Promise.reject(new Error('no such outputId (parked results are dropped when the chat is cleared)'));
        var from = Math.max(0, Number(a.offset) || 0);
        var chunk = text.slice(from, from + CHUNK_BYTES);
        var res = { outputId: a.outputId, from: from, to: from + chunk.length,
                    totalBytes: text.length, content: chunk };
        if (from + chunk.length < text.length)
            res.more = 'continue with offset ' + (from + chunk.length);
        return Promise.resolve(res);
    }

    function toolFileOp(a) {
        return fetch('/api/assets/op', {
            method: 'POST', headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ op: a.op, path: a.path, path2: a.path2 }),
        }).then(function (r) {
            if (!r.ok) return r.json().then(function (j) { throw new Error(j.error || ('HTTP ' + r.status)); });
            // The server copy and the editor's in-memory copy must not drift apart:
            // a file deleted only on the server comes back on the next rebuild
            try {
                var FS = Module.FS;
                if (FS) {
                    var full = '/project/Assets/' + a.path;
                    var dest = a.path2 ? '/project/Assets/' + a.path2 : null;
                    if (a.op === 'mkdir') FS.mkdirTree(full);
                    else if (a.op === 'delete') {
                        var st = FS.analyzePath(full);
                        if (st.exists) {
                            if (FS.isDir(st.object.mode)) rmTree(FS, full);
                            else FS.unlink(full);
                        }
                    } else if (a.op === 'move' && dest) {
                        FS.mkdirTree(dest.substring(0, dest.lastIndexOf('/')));
                        FS.rename(full, dest);
                    } else if (a.op === 'copy' && dest) {
                        FS.mkdirTree(dest.substring(0, dest.lastIndexOf('/')));
                        FS.writeFile(dest, FS.readFile(full));
                    }
                }
            } catch (e) { console.warn('[ai] engine FS sync failed for ' + a.op, e); }
            return { ok: true };
        });
    }

    function rmTree(FS, dir) {
        FS.readdir(dir).forEach(function (name) {
            if (name === '.' || name === '..') return;
            var child = dir + '/' + name;
            var st = FS.analyzePath(child);
            if (st.exists && FS.isDir(st.object.mode)) rmTree(FS, child);
            else if (st.exists) FS.unlink(child);
        });
        FS.rmdir(dir);
    }
    function toolRebuild(a) {
        var forced = a && a.force;
        var fn = forced ? '_o2_web_rebuild_assets_forced' : '_o2_web_rebuild_assets';
        if (!Module.calledRun || typeof Module[fn] !== 'function')
            return Promise.reject(new Error('editor engine is not running'));
        return new Promise(function (resolve) {
            setTimeout(function () {
                Module[fn]();
                resolve();
            }, 50);
        }).then(function () {
            // The built files are mirrored to the server through an async queue.
            // Returning before it drains risks a reload cutting the tail off, which
            // leaves the server missing files the build index still claims exist.
            return drainMirror();
        }).then(function () {
            return { ok: true, forced: !!forced };
        });
    }

    // Waits until the BuiltAssets mirror queue has settled (nothing new for a moment)
    function drainMirror() {
        var rounds = 0;
        function settle() {
            var q = window.__o2MirrorQueue || Promise.resolve();
            return q.then(function () {
                return sleep(400);
            }).then(function () {
                var again = window.__o2MirrorQueue || Promise.resolve();
                if (again !== q && rounds++ < 60) return settle();
            });
        }
        return settle();
    }
    window.__o2DrainMirror = drainMirror;
    // Half size: the model reads it just as well, and the payload (and so the
    // round-trip) is a quarter. Coordinates below are still full-size CSS pixels.
    function toolScreenshot() {
        var w = canvas.clientWidth, h = canvas.clientHeight;
        var sw = Math.round(w / 2), sh = Math.round(h / 2);
        var t = document.createElement('canvas');
        t.width = sw; t.height = sh;
        t.getContext('2d').drawImage(canvas, 0, 0, sw, sh);
        var b64 = t.toDataURL('image/jpeg', 0.85).split(',')[1];
        return Promise.resolve({
            result: { imageWidth: sw, imageHeight: sh, canvasWidth: w, canvasHeight: h,
                      note: 'the image is half size; click coordinates are full-size canvas pixels, so double what you measure on it' },
            image: { mime: 'image/jpeg', b64: b64 },
        });
    }

    function isPlaying() {
        try { return !!(Module._o2_web_is_playing && Module._o2_web_is_playing()); }
        catch (e) { return false; }
    }

    function callJson(fn, args, types) {
        if (typeof Module.ccall !== 'function')
            return Promise.reject(new Error('the editor is not running yet'));
        var ptr = Module.ccall(fn, 'number', types || [], args || []);
        if (!ptr) return Promise.reject(new Error(fn + ' returned nothing'));
        var text = Module.UTF8ToString(ptr);
        try { Module._free(ptr); } catch (e) {}
        var parsed;
        try { parsed = JSON.parse(text); }
        catch (e) { throw new Error('bad reply from ' + fn + ': ' + text.slice(0, 200)); }
        if (parsed.error) throw new Error(parsed.error);
        return Promise.resolve(parsed);
    }

    function toolSceneTree(a) {
        var depth = a.depth === undefined ? 3 : Math.max(0, Math.min(Number(a.depth), 12));
        return callJson('o2_web_scene_dump', [a.path || '', depth], ['string', 'number']);
    }

    function toolViewInfo() {
        return callJson('o2_web_view_info', [], []).then(function (info) {
            // everything the model needs to aim a click, spelled out
            info.howToClick = 'A world point maps to a canvas pixel as: ' +
                'canvasX = canvas.x/2 + worldX, canvasY = canvas.y/2 - worldY for screen-space widgets. ' +
                'In play mode the game is drawn inside gameView, so first map the world point through the ' +
                'camera: u = (worldX - camera.position.x) / camera.size.x + 0.5, ' +
                'v = 0.5 - (worldY - camera.position.y) / camera.size.y, then ' +
                'canvasX = canvas.x/2 + gameView.left + u * (gameView.right - gameView.left), ' +
                'canvasY = canvas.y/2 - (gameView.top - v * (gameView.top - gameView.bottom)).';
            return info;
        });
    }

    // The engine's browser backend evaluates scripts in the page's global scope, so
    // bare let/const would collide between calls - the body is wrapped in a function,
    // and a small prelude adds the helpers the model is told about
    var SCRIPT_PRELUDE =
        'function findActor(path) {' +
        '  var parts = String(path).split("/").filter(function (p) { return p; });' +
        '  var list = sceneRoots, cur = null;' +
        '  for (var i = 0; i < parts.length; i++) {' +
        '    cur = null;' +
        '    for (var j = 0; j < list.length; j++) {' +
        '      if (list[j] && list[j].GetName() === parts[i]) { cur = list[j]; break; }' +
        '    }' +
        '    if (!cur) return null;' +
        '    list = cur.GetChildren ? cur.GetChildren() : [];' +
        '  }' +
        '  return cur;' +
        '}' +
        'function eachActor(fn, list, path) {' +
        '  list = list || sceneRoots; path = path || "";' +
        '  for (var i = 0; i < list.length; i++) {' +
        '    var a = list[i]; if (!a) continue;' +
        '    var p = path ? path + "/" + a.GetName() : a.GetName();' +
        '    fn(a, p);' +
        '    if (a.GetChildren) eachActor(fn, a.GetChildren(), p);' +
        '  }' +
        '}';

    function toolRunScript(a) {
        if (!a.code) return Promise.reject(new Error('code is required'));
        // Console semantics: the value of the last expression comes back, so the model
        // does not have to remember a return (and a stray one is forgiven)
        var code = String(a.code).replace(/^\s*return\s+/, '');
        var asExpression = '(function () {' + SCRIPT_PRELUDE + '\nreturn eval(' + JSON.stringify(code) + ');\n})()';
        var asBody = '(function () {' + SCRIPT_PRELUDE + '\n' + code + '\n})()';

        function run(wrapped) {
            return callJson('o2_web_run_script', [wrapped], ['string']).then(function (r) {
                if (typeof r.result === 'string' && /^(TypeError|SyntaxError|ReferenceError|RangeError|Error)\b/.test(r.result))
                    throw new Error(r.result);
                return r;
            });
        }

        // eval gives console semantics (the last expression is the result), but code
        // written with a return statement needs a real function body instead
        return run(asExpression).catch(function (e) {
            if (/Illegal return statement/i.test(e.message)) return run(asBody);
            throw e;
        });
    }

    function toolOpenScene(a) {
        if (typeof Module.ccall !== 'function')
            return Promise.reject(new Error('the editor is not running yet'));
        if (!a.path) return Promise.reject(new Error('path is required, e.g. Boot.scn'));
        // The engine silently does nothing for a missing scene, which reads as success
        var slash = a.path.lastIndexOf('/');
        var dir = slash < 0 ? '' : a.path.slice(0, slash);
        var name = slash < 0 ? a.path : a.path.slice(slash + 1);
        return toolListFiles({ dir: dir }).then(function (listing) {
            var exists = (listing.files || []).some(function (f) { return f.name === name; });
            if (!exists) throw new Error('no such scene: ' + a.path + ' (list_files to see what exists)');
            var before = (window.engineLogLines || []).length;
            Module.ccall('o2_web_open_scene', null, ['string'], [a.path]);
            return sleep(1800).then(function () {
                var failed = (window.engineLogLines || []).slice(before)
                    .some(function (l) { return l.indexOf('Failed to load scene') >= 0; });
                if (!failed || a._healed) return failed;
                // The built copy can go missing while the build index still lists it,
                // and only a full rebuild puts it back
                return toolRebuild({ force: true })
                    .then(function () { return sleep(30000); })
                    .then(function () { return toolOpenScene({ path: a.path, _healed: true }); })
                    .then(function () { return 'healed'; });
            });
        }).then(function (state) {
            return callJson('o2_web_scene_dump', ['', 0], ['string', 'number']).then(function (dump) {
                var count = (dump.actors || []).length;
                var result = { ok: true, opened: a.path, rootActors: count };
                if (state === 'healed')
                    result.note = 'the built scene was missing and has been rebuilt from source';
                else if (state === true)
                    result.note = 'the engine could not load the built scene; run rebuild_assets({force:true})';
                else if (!count)
                    result.note = 'the scene opened but has no root actors - check the file';
                return result;
            });
        });
    }

    function toolSaveScene() {
        if (typeof Module._o2_web_save_scene !== 'function')
            return Promise.reject(new Error('the editor is not running yet'));
        Module._o2_web_save_scene();
        return sleep(800).then(function () { return { ok: true }; });
    }

    function toolPlayMode(a) {
        if (typeof Module._o2_web_set_play !== 'function')
            return Promise.reject(new Error('the editor is not running yet'));
        var want = a.on === undefined ? true : !!a.on;
        Module._o2_web_set_play(want ? 1 : 0);
        return sleep(600).then(function () {
            var now = isPlaying();
            return { playing: now,
                     note: now
                        ? 'the scene is running in the Game window; input tools now work, and they only make sense inside that window'
                        : 'stopped; the scene is back to its saved state' };
        });
    }

    // Driving the editor chrome by synthetic clicks proved unreliable and
    // expensive, so input is allowed only while the game runs
    function requirePlay(what) {
        if (isPlaying()) return null;
        return Promise.reject(new Error(
            what + ' is only available in play mode, and only inside the Game window. ' +
            'Turn it on with play_mode({on:true}) - and note that editor windows (Assets, Tree, Properties, menus) ' +
            'cannot be driven at all: change the project through the file tools instead.'));
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
        var blocked = requirePlay('clicking');
        if (blocked) return blocked;
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
        var blocked = requirePlay('typing');
        if (blocked) return blocked;
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
        var blocked = requirePlay('pressing keys');
        if (blocked) return blocked;
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
    function toolReadLog(a) {
        var all = (window.engineLogLines || []);
        var filtered = a.filter
            ? all.filter(function (l) { return l.indexOf(a.filter) >= 0; })
            : all;
        var n = Math.min(Math.max(Number(a.lines) || 60, 1), 300);
        var tail = filtered.slice(-n);
        return Promise.resolve({
            lines: tail, shown: tail.length, totalKept: all.length,
            note: all.length ? undefined : 'the engine has printed nothing yet',
        });
    }

    function toolWait(a) {
        var ms = Math.min(Math.max(Number(a.ms) || 0, 0), 5000);
        return sleep(ms).then(function () { return { ok: true, waited: ms }; });
    }

    var EXEC = {
        list_files: toolListFiles, search_files: toolSearchFiles,
        read_file: toolReadFile, edit_file: toolEditFile,
        write_file: toolWriteFile, read_output: toolReadOutput,
        read_guide: toolReadGuide,
        search_engine: toolSearchEngine, read_engine_file: toolReadEngineFile,
        list_engine_dir: toolListEngineDir,
        file_op: toolFileOp, rebuild_assets: toolRebuild, screenshot: toolScreenshot,
        click: toolClick, type_text: toolTypeText, press_key: toolPressKey,
        read_log: toolReadLog, wait: toolWait, play_mode: toolPlayMode,
        open_scene: toolOpenScene, save_scene: toolSaveScene,
        scene_tree: toolSceneTree, view_info: toolViewInfo, run_script: toolRunScript,
    };
    window.__o2aiExec = EXEC;       // debug/testing handles
    window.__o2aiPark = park;
    window.__o2aiOutputs = outputs;

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
    var rawLog = [];

    // Mirror of what the agent does, shipped to the server so its behaviour can be
    // debugged from outside the browser (o2editor/workdir/agent-log.jsonl)
    var runSeq = 0;
    var lastReview = null;
    function trace(kind, data) {
        try {
            fetch('/api/agent/log', {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(Object.assign({ t: new Date().toISOString(), run: runSeq, kind: kind }, data)),
            }).catch(function () {});
        } catch (e) {}
    }
   // full request/response pairs, for the Copy log button

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

        runSeq++;
        var triedModels = [model];
        trace('user', { text: userText, model: model });
        history.push({ role: 'user', parts: [{ text: userText }] });
        addMsg('user', userText);

        running = true; aborted = false;
        document.getElementById('btn-ai').classList.add('busy');
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
                    // An overloaded model must not kill a long autonomous run:
                    // switch to the next one and retry this step
                    var overloaded = e.status === 503 || e.status === 429;
                    var next = overloaded && FALLBACK_MODELS.filter(function (m) {
                        return triedModels.indexOf(m) < 0;
                    })[0];
                    if (next) {
                        triedModels.push(next);
                        addStep('⇄ ' + model + ' overloaded, switching to ' + next, String(e.message));
                        trace('model-switch', { step: step, from: model, to: next, reason: e.message });
                        model = next;
                        modelEl.value = next;
                        step--;
                        continue;
                    }
                    addMsg('error', 'API error: ' + e.message +
                        (overloaded ? '\nEvery model tried is overloaded; try again later.' : ''));
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
                trace('step', { step: step, finish: cand.finishReason, usage: u, parts: parts.map(partKind) });
                parts.forEach(function (p) {
                    if (p.text) trace(p.thought ? 'thought' : 'answer', { step: step, text: p.text.slice(0, 4000) });
                    if (p.text && p.thought)
                        addStep('🧠 thinking · ' + ((p.text.replace(/[#*`]/g, '').split('\n')
                                    .filter(function (l) { return l.trim(); })[0] || 'reasoning').slice(0, 90)),
                                p.text, { markdown: true });
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
                    var took = Date.now() - toolStarted;

                    // park anything bulky: whatever lands in `response` here is
                    // re-sent with every later step for the rest of the chat
                    var flat = JSON.stringify(response);
                    if (flat && flat.length > INLINE_LIMIT && !response.error) {
                        var full = typeof response.content === 'string'
                            ? response.content : JSON.stringify(response, null, 2);
                        response = park(full, fc.name === 'read_file'
                            ? 'From read_file; for source files prefer paging with read_file(offset, limit).'
                            : 'From ' + fc.name + '.');
                        uiStep.append('parked as ' + response.outputId + ' (' +
                                      response.totalBytes + ' bytes; ' + flat.length + ' would have been inlined)');
                    }

                    trace('tool', { step: step, tool: fc.name, args: fc.args, ms: took,
                                   ok: !(response && response.error), error: response && response.error,
                                   result: JSON.stringify(response).slice(0, 1200) });
                    var dump = JSON.stringify(response, null, 2);
                    uiStep.append('result (' + took + 'ms) ' +
                                  (dump.length > 8000 ? dump.slice(0, 8000) + '\n… truncated' : dump));
                    uiStep.setLabel(uiStep.label() + '  ' + took + 'ms');
                    console.log('[ai] tool ' + fc.name, { args: fc.args, response: response, ms: took });
                    respParts.push({ functionResponse: { name: fc.name, response: response } });
                }
                history.push({ role: 'user', parts: respParts.concat(imageParts) });
            }
            trace('end', { steps: step, aborted: aborted });
            if (!aborted && step > 3 && reviewEl.checked)
                await selfReview(key, model, step);
            if (aborted) addMsg('error', 'Stopped.');
            else if (step > MAX_STEPS) addMsg('error', 'Step limit reached (' + MAX_STEPS + ').');
        } finally {
            running = false;
            document.getElementById('btn-ai').classList.remove('busy');
            setStatus('');
            sendBtn.style.display = '';
            stopBtn.style.display = 'none';
            inputEl.disabled = false;
        }
    }

    // ---------- self-review ----------
    // One extra call at the end of a run: the model critiques its own work, which is
    // where the useful signal for improving the agent lives (wrong turns, wasted steps,
    // missing knowledge). Kept as a separate debug block, never as the answer.
    var REVIEW_PROMPT = [
        'The task is over. Step out of it and review your own run as an engineer would review a colleague.',
        'Answer in markdown, in the language of my first message, under these headings and nothing else:',
        '',
        '## Mistakes',
        'What you actually got wrong: wrong assumptions, calls that failed, things you had to redo.',
        'Name the step numbers. Write "none" if there were none.',
        '',
        '## Waste',
        'Where the run was inefficient: repeated lookups, reading what you already knew, exploring instead of acting,',
        'tools you should have used earlier or not at all. Be specific about how many steps went nowhere.',
        '',
        '## What would have helped',
        'Knowledge or a tool that was missing and would have shortened the run - phrased so it can be added to the',
        'agent briefing or built as a new tool.',
        '',
        '## Advice for next time',
        'Two or three concrete rules you would follow on a similar task.',
        '',
        'Be blunt and concrete. Do not praise yourself, do not restate what the task was, do not repeat the summary.',
    ].join('\n');

    async function selfReview(key, model, steps) {
        setStatus('reviewing the run…');
        history.push({ role: 'user', parts: [{ text: REVIEW_PROMPT }] });
        try {
            var resp = await callModel(key, model);
            var cand = resp.candidates && resp.candidates[0];
            var text = ((cand && cand.content && cand.content.parts) || [])
                .filter(function (p) { return p.text && !p.thought; })
                .map(function (p) { return p.text; }).join('\n').trim();
            if (!text) throw new Error('empty review');

            history.push({ role: 'model', parts: [{ text: text }] });
            addStep('🔍 self-review · what went wrong and what to improve (' + steps + ' steps)',
                    text, { markdown: true, kind: 'review', open: true });
            trace('review', { steps: steps, text: text.slice(0, 6000) });
            lastReview = text;
        } catch (e) {
            addStep('🔍 self-review failed', String(e.message), { kind: 'review' });
        }
        setStatus('');
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
        var dump = JSON.stringify({ model: modelEl.value.trim(), review: lastReview,
                                    history: history, exchanges: rawLog }, null, 2);
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
