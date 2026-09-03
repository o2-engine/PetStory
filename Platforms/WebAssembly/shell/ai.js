// AI agent: Claude Code runs on the server, scoped to this session's project;
// this file is its face in the page and its hands in the editor.
//
// The server streams the conversation over SSE (/api/agent/stream). File work
// happens there with Claude's own tools; anything that needs the running
// editor (screenshot, scene tree, run_script, play mode, asset rebuild...)
// arrives as a tool_request event, is executed here and posted back.
//
// UI shape: the title bar carries identity and live status; settings and
// developer output are folded away until asked for, so an ordinary turn is
// just the conversation, a progress line and the list of files that changed.

// ---- AI agent ------------------------------------------------------
(function () {
    var ICONS_PLUS = '<svg viewBox="0 0 16 16"><path d="M8 3.5v9M3.5 8h9"/></svg>';
    var ICONS_GEAR = '<svg viewBox="0 0 16 16"><path d="M8 10a2 2 0 1 0 0-4 2 2 0 0 0 0 4z"/><path d="M13 8a5 5 0 0 0-.1-1l1.3-1-1.5-2.6-1.6.6a5 5 0 0 0-1.7-1L9.2 1H6.8l-.2 1.7a5 5 0 0 0-1.7 1l-1.6-.6L1.8 6l1.3 1a5 5 0 0 0 0 2l-1.3 1 1.5 2.6 1.6-.6a5 5 0 0 0 1.7 1l.2 1.7h2.4l.2-1.7a5 5 0 0 0 1.7-1l1.6.6 1.5-2.6-1.3-1A5 5 0 0 0 13 8z"/></svg>';
    var ICONS_CODE = '<svg viewBox="0 0 16 16"><path d="M5.5 4 2 8l3.5 4M10.5 4 14 8l-3.5 4"/></svg>';
    var ICONS_CLOSE = '<svg viewBox="0 0 16 16"><path d="M4 4l8 8M12 4l-8 8"/></svg>';
    var ICONS_STOP = '<svg viewBox="0 0 16 16"><rect x="4.5" y="4.5" width="7" height="7" rx="1.5"/></svg>';
    var dlg = document.getElementById('ai');
    var back = document.getElementById('ai-back');
    var canvas = document.getElementById('canvas');

    // The window is built here rather than in editor.html: the shell page is
    // baked into the wasm binary at link time, this file is not.
    dlg.innerHTML =
        '<div id="ai-title">' +
          '<svg class="icon" viewBox="0 0 16 16"><use href="#i-ai"/></svg>' +
          '<span class="name">Agent</span>' +
          '<span id="ai-chip">ready</span>' +
          '<span class="grow"></span>' +
          '<span id="ai-history-slot"></span>' +
          '<button class="titlebtn" id="ai-new" title="Start a new conversation">' + ICONS_PLUS + ' New</button>' +
          '<button class="titlebtn icon-only" id="ai-gear" title="Key, model, self-review">' + ICONS_GEAR + '</button>' +
          '<button class="titlebtn icon-only" id="ai-dev" title="Debug: steps, thinking, raw events">' + ICONS_CODE + '</button>' +
          '<button class="titlebtn icon-only" id="ai-close" title="Close — the agent keeps working">' + ICONS_CLOSE + '</button>' +
        '</div>' +
        '<div id="ai-chat"></div>' +
        '<div id="ai-files"><div class="fhead"><span id="ai-files-title"></span></div><div class="flist"></div></div>' +
        '<div id="ai-bar">' +
          '<span class="spin"></span><span id="ai-act">working…</span>' +
          '<span id="ai-meta"></span>' +
          '<button class="ai-btn danger" id="ai-stop">' + ICONS_STOP + ' Stop</button>' +
        '</div>' +
        '<div id="ai-inputrow">' +
          '<div id="ai-composer">' +
            '<textarea id="ai-input" rows="1" placeholder="What should the agent do? For example: add a title label to the scene…"></textarea>' +
            '<div id="ai-controls">' +
              '<span id="ai-model-slot"></span>' +
              '<span id="ai-effort-slot"></span>' +
              '<span id="ai-mode-slot"></span>' +
              '<span class="grow"></span>' +
              '<button id="ai-send" title="Send (Enter)">' +
                '<svg viewBox="0 0 16 16"><path d="M1.7 14.3 15 8 1.7 1.7l2 5.1 6.6 1.2-6.6 1.2z"/></svg>' +
              '</button>' +
            '</div>' +
          '</div>' +
        '</div>' +
        '<div id="ai-modal">' +
          '<div id="ai-settings">' +
            '<div class="sheet-head"><span>Agent settings</span><span class="grow"></span>' +
              '<button class="ai-btn" id="ai-settings-close">Done</button></div>' +
            '<div class="row"><span class="lbl">Sign in with</span>' +
              '<span class="ai-seg" id="ai-auth"><button data-auth="key">API key</button>' +
              '<button data-auth="sub">Claude subscription</button></span>' +
              '<span class="grow"></span><span class="keystate" id="ai-keystate"></span></div>' +
            '<div class="row" id="row-key"><span class="lbl">Anthropic key</span>' +
              '<input id="ai-key" class="ai-input" type="password" placeholder="sk-ant-…" spellcheck="false" autocomplete="off"></div>' +
            '<div class="row" id="row-workspace"><span class="lbl">Workspace</span>' +
              '<input id="ai-workspace" class="ai-input" type="text" placeholder="wrkspc_… — for keys scoped to all workspaces" spellcheck="false" autocomplete="off"></div>' +
            '<div class="row" id="row-sub"><span class="lbl">Subscription token</span>' +
              '<input id="ai-oauth" class="ai-input" type="password" placeholder="sk-ant-oat…" spellcheck="false" autocomplete="off"></div>' +
            '<div class="row"><span class="lbl">Self-review</span>' +
              '<span id="ai-review-slot"></span><span class="grow"></span>' +
              '<button class="ai-btn" id="ai-log" title="The whole conversation and its events as JSON">Copy log</button></div>' +
            '<p class="hint" id="hint-key">The key is sent to this server and handed to the Claude Code process; usage is billed to it. ' +
               'Create one at <a href="https://console.anthropic.com/settings/keys" target="_blank" rel="noopener">console.anthropic.com</a>.</p>' +
            '<p class="hint" id="hint-sub">Run <code>claude setup-token</code> in a terminal where you are logged in to Claude Code, ' +
               'then paste the token it prints. Work then runs on your Claude subscription instead of API billing. ' +
               'The token is yours: this page never opens a claude.ai login.</p>' +
          '</div>' +
        '</div>';

    // Inline icons: the page sprite carries file-browser glyphs only, and the
    // window is built here so it can ship its own without a wasm relink.
    var ICON = {
        gear: '<path d="M8 10a2 2 0 1 0 0-4 2 2 0 0 0 0 4z"/><path d="M13 8a5 5 0 0 0-.1-1l1.3-1-1.5-2.6-1.6.6a5 5 0 0 0-1.7-1L9.2 1H6.8l-.2 1.7a5 5 0 0 0-1.7 1l-1.6-.6L1.8 6l1.3 1a5 5 0 0 0 0 2l-1.3 1 1.5 2.6 1.6-.6a5 5 0 0 0 1.7 1l.2 1.7h2.4l.2-1.7a5 5 0 0 0 1.7-1l1.6.6 1.5-2.6-1.3-1A5 5 0 0 0 13 8z"/>',
        code: '<path d="M5.5 4 2 8l3.5 4M10.5 4 14 8l-3.5 4"/>',
        plus: '<path d="M8 3.5v9M3.5 8h9"/>',
        close: '<path d="M4 4l8 8M12 4l-8 8"/>',
        stop: '<rect x="4.5" y="4.5" width="7" height="7" rx="1.5"/>',
        file: '<path d="M9 2H4.5A1.5 1.5 0 0 0 3 3.5v9A1.5 1.5 0 0 0 4.5 14h7a1.5 1.5 0 0 0 1.5-1.5V6z"/><path d="M9 2v4h4"/>',
        spark: '<path d="M8 2.5 9.3 6l3.5 1.3L9.3 8.6 8 12.1 6.7 8.6 3.2 7.3 6.7 6z"/>',
        gauge: '<path d="M13 11a5.5 5.5 0 1 0-10 0"/><path d="M8 11 10.5 7"/>',
        shield: '<path d="M8 2 3.5 4v4c0 2.6 1.9 4.8 4.5 5.5 2.6-.7 4.5-2.9 4.5-5.5V4z"/>',
        cpu: '<rect x="4.5" y="4.5" width="7" height="7" rx="1.5"/><path d="M6.5 1.5v2M9.5 1.5v2M6.5 12.5v2M9.5 12.5v2M1.5 6.5h2M1.5 9.5h2M12.5 6.5h2M12.5 9.5h2"/>',
        history: '<path d="M3 8a5 5 0 1 0 1.6-3.7"/><path d="M3 3v2.5h2.5"/><path d="M8 5.5V8l2 1.2"/>',
    };
    function icon(name, cls) {
        var svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        svg.setAttribute('viewBox', '0 0 16 16');
        if (cls) svg.setAttribute('class', cls);
        svg.innerHTML = ICON[name] || '';
        return svg;
    }
    function iconHtml(name) {
        return '<svg viewBox="0 0 16 16">' + (ICON[name] || '') + '</svg>';
    }

    var chatEl = document.getElementById('ai-chat');
    var inputEl = document.getElementById('ai-input');
    var sendBtn = document.getElementById('ai-send');
    var stopBtn = document.getElementById('ai-stop');
    var keyEl = document.getElementById('ai-key');
    var keyStateEl = document.getElementById('ai-keystate');
    var workspaceEl = document.getElementById('ai-workspace');
    var oauthEl = document.getElementById('ai-oauth');
    var authSeg = document.getElementById('ai-auth');
    var modalEl = document.getElementById('ai-modal');
    var devBtn = document.getElementById('ai-dev');
    var gearBtn = document.getElementById('ai-gear');
    var chipEl = document.getElementById('ai-chip');
    var barEl = document.getElementById('ai-bar');
    var actEl = document.getElementById('ai-act');
    var metaEl = document.getElementById('ai-meta');
    var filesEl = document.getElementById('ai-files');
    var filesTitle = document.getElementById('ai-files-title');
    var filesList = filesEl.querySelector('.flist');

    // ---------- a dropdown of our own: no native select anywhere ----------
    var openDd = null;
    document.addEventListener('mousedown', function (e) {
        if (openDd && !openDd.root.contains(e.target)) openDd.close();
    });
    document.addEventListener('keydown', function (e) {
        if (e.key !== 'Escape' || !dlg.classList.contains('open')) return;
        // innermost thing first: a menu, then the settings sheet, then the window
        if (openDd) { openDd.close(); e.stopPropagation(); return; }
        if (modalEl.classList.contains('open')) { closeSettings(); e.stopPropagation(); return; }
        closeDlg();
    }, true);

    function dropdown(opts) {
        var root = document.createElement('div');
        root.className = 'ai-dd ' + (opts.cls || '') + ' ' + (opts.up ? 'up' : 'down') + (opts.right ? ' right' : '');
        var val = document.createElement('div');
        val.className = 'val';
        val.title = opts.title || '';
        if (opts.icon) val.appendChild(icon(opts.icon));
        var cur = document.createElement('span');
        cur.className = 'cur';
        val.appendChild(cur);
        var caret = document.createElement('span');
        caret.className = 'caret';
        val.appendChild(caret);
        var menu = document.createElement('div');
        menu.className = 'menu';
        root.appendChild(val);
        root.appendChild(menu);

        var items = [], value = null, api;
        function paint() {
            var it = items.filter(function (i) { return i.value === value; })[0];
            var text = it ? (it.short || it.label) : (opts.empty || '—');
            if (opts.short && value) text = opts.short(value);
            cur.textContent = (opts.prefix || '') + text;
            val.title = it ? (it.title || it.label) : (opts.title || '');
            menu.querySelectorAll('.opt').forEach(function (el) {
                el.classList.toggle('sel', el.dataset.value === value);
            });
        }
        function build() {
            menu.innerHTML = '';
            items.forEach(function (it) {
                if (it.separator) { var sp = document.createElement('div'); sp.className = 'sep'; menu.appendChild(sp); return; }
                var o = document.createElement('div');
                o.className = 'opt';
                o.dataset.value = it.value;
                var label = document.createElement('span');
                label.textContent = it.label;
                o.appendChild(label);
                if (it.sub) {
                    var sub = document.createElement('span');
                    sub.className = 'sub';
                    sub.textContent = it.sub;
                    o.appendChild(sub);
                }
                o.onclick = function () {
                    api.close();
                    if (it.action) { it.action(api); return; }
                    api.set(it.value);
                    if (opts.onChange) opts.onChange(it.value, it);
                };
                menu.appendChild(o);
            });
            paint();
        }
        val.onclick = function () {
            if (root.classList.contains('open')) { api.close(); return; }
            if (openDd) openDd.close();
            root.classList.add('open');
            openDd = api;
        };
        api = {
            root: root,
            close: function () { root.classList.remove('open'); if (openDd === api) openDd = null; },
            set: function (v) { value = v; paint(); },
            get: function () { return value; },
            options: function (list) { items = list; build(); },
            add: function (it) { items.push(it); build(); },
            has: function (v) { return items.some(function (i) { return i.value === v; }); },
            el: root,
        };
        api.options(opts.items || []);
        if (opts.value !== undefined) api.set(opts.value);
        return api;
    }

    function toggle(labelText, checked, onChange) {
        var root = document.createElement('div');
        root.className = 'ai-check' + (checked ? ' on' : '');
        var box = document.createElement('span');
        box.className = 'box';
        root.appendChild(box);
        if (labelText) root.appendChild(document.createTextNode(labelText));
        root.onclick = function () {
            root.classList.toggle('on');
            onChange(root.classList.contains('on'));
        };
        return { el: root, get: function () { return root.classList.contains('on'); },
                 set: function (v) { root.classList.toggle('on', !!v); } };
    }

    // file names in the answer open the file in the assets browser
    chatEl.addEventListener('click', function (e) {
        var el = e.target.closest('.ai-file');
        if (!el || !window.__o2RevealAsset) return;
        window.__o2RevealAsset(el.dataset.path);
    });

    var DEFAULT_MODEL = 'claude-opus-5';
    var FALLBACK_MODELS = ['claude-opus-5', 'claude-sonnet-5', 'claude-opus-4-8', 'claude-opus-4-7', 'claude-haiku-4-5'];

    // settings live in cookies so they survive across tabs and sessions
    // (localStorage is kept as a fallback for values saved earlier)
    function setSetting(name, value) {
        document.cookie = name + '=' + encodeURIComponent(value) +
            ';path=' + (window.o2Base || '/') + ';max-age=' + (365 * 24 * 3600) + ';SameSite=Lax';
        try { localStorage.setItem(name, value); } catch (e) {}
    }
    function getSetting(name) {
        var m = document.cookie.match(new RegExp('(?:^|; )' + name + '=([^;]*)'));
        if (m) return decodeURIComponent(m[1]);
        try { return localStorage.getItem(name) || ''; } catch (e) { return ''; }
    }

    keyEl.value = getSetting('o2ai_claude_key');
    workspaceEl.value = getSetting('o2ai_workspace');
    oauthEl.value = getSetting('o2ai_oauth');
    var authMode = getSetting('o2ai_auth') || (oauthEl.value ? 'sub' : 'key');
    var devMode = getSetting('o2ai_dev') === '1';

    // effort and permissions sit next to Send, where they are decided
    // model, effort and permissions sit together next to Send, where a turn
    // is actually decided; the model list is a list, never a filter
    var modelDd = dropdown({
        cls: 'pill', up: true, icon: 'cpu', title: 'Claude model', empty: DEFAULT_MODEL,
        short: function (v) { return String(v).replace(/^claude-/, ''); },
        value: getSetting('o2ai_claude_model') || DEFAULT_MODEL,
        onChange: function (v) { setSetting('o2ai_claude_model', v); },
    });
    document.getElementById('ai-model-slot').appendChild(modelDd.el);

    var effortDd = dropdown({
        cls: 'pill', up: true, icon: 'gauge', title: 'Reasoning effort',
        items: ['low', 'medium', 'high', 'xhigh', 'max'].map(function (e) { return { value: e, label: e }; }),
        value: getSetting('o2ai_effort') || 'high',
        onChange: function (v) { setSetting('o2ai_effort', v); },
    });
    document.getElementById('ai-effort-slot').appendChild(effortDd.el);

    var MODES = [
        { value: 'acceptEdits', label: 'edit files and drive the editor', short: 'edits' },
        { value: 'default', label: 'ask before everything', short: 'ask' },
        { value: 'plan', label: 'plan only, no changes', short: 'plan' },
        { value: 'bypassPermissions', label: 'never ask', short: 'no prompts' },
    ];
    var modeDd = dropdown({
        cls: 'pill', up: true, icon: 'shield',
        title: 'What the agent may do without asking (reading the scene is never asked)',
        items: MODES, value: getSetting('o2ai_mode') || 'acceptEdits',
        onChange: function (v) { setSetting('o2ai_mode', v); },
    });
    document.getElementById('ai-mode-slot').appendChild(modeDd.el);

    var reviewToggle = toggle('the agent reviews its own run afterwards', getSetting('o2ai_review') === '1',
                              function (v) { setSetting('o2ai_review', v ? '1' : '0'); });
    document.getElementById('ai-review-slot').appendChild(reviewToggle.el);

    var histDd = dropdown({
        cls: 'ghost', right: true, icon: 'history', title: 'Continue an earlier conversation of this tab',
        empty: 'conversations',
        onChange: function (v) { resumeConversation(v); },
    });
    document.getElementById('ai-history-slot').appendChild(histDd.el);
    histDd.el.style.display = 'none';

    // `claude setup-token` prints the token wrapped across terminal lines, so a
    // copy of it arrives with newlines inside — which the API rejects as invalid
    function cleanSecret(v) {
        return String(v == null ? '' : v).replace(/\s+/g, '').replace(/^["']+|["']+$/g, '');
    }
    function tidy(el) {
        var v = cleanSecret(el.value);
        if (el.value !== v) el.value = v;
        return v;
    }
    function setKeyState(kind, text) {
        keyStateEl.className = 'keystate ' + kind;
        keyStateEl.textContent = text;
    }
    function credential() {
        return cleanSecret(authMode === 'sub' ? oauthEl.value : keyEl.value);
    }
    function paintKeyState() {
        var local = /localhost|127\.0\.0\.1/.test(location.hostname);
        if (credential()) setKeyState('ok', 'saved');
        else setKeyState(local ? 'ok' : 'missing', local ? 'local: host credentials' : 'credential required');
    }
    function paintAuthMode() {
        authSeg.querySelectorAll('button').forEach(function (b) {
            b.classList.toggle('on', b.dataset.auth === authMode);
        });
        document.getElementById('row-key').classList.toggle('hidden', authMode !== 'key');
        document.getElementById('row-workspace').classList.toggle('hidden', authMode !== 'key');
        document.getElementById('row-sub').classList.toggle('hidden', authMode !== 'sub');
        document.getElementById('hint-key').classList.toggle('hidden', authMode !== 'key');
        document.getElementById('hint-sub').classList.toggle('hidden', authMode !== 'sub');
        paintKeyState();
    }
    authSeg.querySelectorAll('button').forEach(function (b) {
        b.onclick = function () {
            authMode = b.dataset.auth;
            setSetting('o2ai_auth', authMode);
            paintAuthMode();
            loadModels();
        };
    });
    paintAuthMode();

    keyEl.onchange = function () { setSetting('o2ai_claude_key', tidy(keyEl)); paintKeyState(); loadModels(); };
    workspaceEl.onchange = function () { setSetting('o2ai_workspace', tidy(workspaceEl)); loadModels(); };
    oauthEl.onchange = function () { setSetting('o2ai_oauth', tidy(oauthEl)); paintKeyState(); loadModels(); };

    function openSettings() {
        modalEl.classList.add('open');
        gearBtn.classList.add('on');
        if (!modelsLoaded) loadModels();
    }
    function closeSettings() {
        modalEl.classList.remove('open');
        gearBtn.classList.remove('on');
    }
    gearBtn.onclick = function () {
        if (modalEl.classList.contains('open')) closeSettings(); else openSettings();
    };
    document.getElementById('ai-settings-close').onclick = closeSettings;
    modalEl.onclick = function (e) { if (e.target === modalEl) closeSettings(); };
    // a missing key is the one thing worth opening the sheet for on its own
    if (!credential() && !/localhost|127\.0\.0\.1/.test(location.hostname)) openSettings();

    function applyDev() {
        devBtn.classList.toggle('on', devMode);
        chatEl.classList.toggle('dev', devMode);
        chatEl.querySelectorAll('.ai-step').forEach(function (s) {
            if (s.dataset.dev === '1') s.style.display = devMode ? '' : 'none';
        });
    }
    devBtn.onclick = function () {
        devMode = !devMode;
        setSetting('o2ai_dev', devMode ? '1' : '0');
        applyDev();
    };

    // ---------- model list ----------
    var modelsLoaded = false;
    function fillModels(names) {
        var items = names.map(function (n) { return { value: n, label: n }; });
        // a model typed by hand stays in the list instead of vanishing
        var cur = modelDd.get();
        if (cur && names.indexOf(cur) < 0) items.unshift({ value: cur, label: cur, sub: 'custom' });
        items.push({ separator: true });
        items.push({ value: '__custom__', label: 'Enter manually…', action: function () {
            var v = window.prompt('Model id', modelDd.get() || DEFAULT_MODEL);
            if (!v) return;
            v = v.trim();
            modelDd.add({ value: v, label: v, sub: 'custom' });
            modelDd.set(v);
            setSetting('o2ai_claude_model', v);
        } });
        modelDd.options(items);
        modelDd.set(cur || DEFAULT_MODEL);
    }
    function loadModels() {
        var headers = {};
        if (authMode === 'sub') {
            if (cleanSecret(oauthEl.value)) headers['X-Anthropic-Oauth'] = cleanSecret(oauthEl.value);
        } else {
            if (cleanSecret(keyEl.value)) headers['X-Anthropic-Key'] = cleanSecret(keyEl.value);
            if (cleanSecret(workspaceEl.value)) headers['X-Anthropic-Workspace'] = cleanSecret(workspaceEl.value);
        }
        return fetch(o2Base + '/api/agent/models', { headers: headers })
            .then(function (r) { return r.json(); })
            .then(function (j) {
                fillModels(j.models || FALLBACK_MODELS);
                modelsLoaded = j.source === 'api';
                // a background fetch never reopens a sheet the user closed:
                // it only marks the field it is complaining about
                if (j.error && /workspace/i.test(j.error)) {
                    workspaceEl.style.borderColor = '#C86A6A';
                    workspaceEl.title = j.error;
                    setKeyState('missing', 'workspace id required');
                } else {
                    workspaceEl.style.borderColor = '';
                    workspaceEl.title = '';
                    paintKeyState();
                }
            })
            .catch(function () { fillModels(FALLBACK_MODELS); });
    }
    fillModels(FALLBACK_MODELS);

    var running = false;

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
        return !/\s{2,}|^https?:/.test(s) && ASSET_EXT.test(s);
    }
    // the server-side paths Claude prints are session-absolute; the browser
    // knows them relative to Assets
    function assetRel(p) {
        var m = String(p).match(/(?:^|\/)Assets\/(.+)$/);
        return m ? m[1] : p;
    }
    function fileLink(path) {
        var rel = assetRel(path.trim());
        return '<span class="ai-file" data-path="' + esc(rel) + '">' + esc(assetRel(path.trim())) + '</span>';
    }
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
                    : '<span class="ai-file" data-path="' + esc(assetRel(href)) + '">' + esc(label) + '</span>';
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
            if (fence) {
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

    // ---------- chat pieces ----------
    var EXAMPLES = [
        'Describe this project and what is on the scene right now.',
        'Add a title label to the scene and place it sensibly.',
        'Find every script under Assets and say what each one does.',
        'Enter play mode, take a screenshot and tell me what you see.',
    ];
    function showEmptyState() {
        if (chatEl.querySelector('#ai-empty') || chatEl.children.length) return;
        var d = document.createElement('div');
        d.id = 'ai-empty';
        d.innerHTML =
            '<div class="badge">' + iconHtml('spark') + '</div>' +
            '<h2>The agent works on a copy of this project</h2>' +
            '<p>Claude Code on the server: it reads and edits files under <b>Assets/</b> in your session, ' +
            'sees the scene and drives the editor. The rest of the repository is read-only.</p>' +
            '<div class="examples"></div>';
        var ex = d.querySelector('.examples');
        EXAMPLES.forEach(function (t) {
            var b = document.createElement('div');
            b.className = 'ex';
            var dot = document.createElement('span');
            dot.className = 'dot';
            b.appendChild(icon('spark'));
            b.appendChild(document.createTextNode(t));
            b.onclick = function () {
                inputEl.value = t;
                inputEl.dispatchEvent(new Event('input'));
                inputEl.focus();
            };
            ex.appendChild(b);
        });
        chatEl.appendChild(d);
    }
    function clearEmptyState() {
        var e = chatEl.querySelector('#ai-empty');
        if (e) e.remove();
    }

    function addMsg(cls, text) {
        clearEmptyState();
        var d = document.createElement('div');
        d.className = 'ai-m ' + cls;
        if (cls === 'model') d.innerHTML = renderMarkdown(text);
        else d.textContent = text;
        chatEl.appendChild(d);
        scrollDown();
        return d;
    }

    // An error the user can act on: plain words, and a button when a retry
    // is the obvious next move
    function addError(message, opts) {
        opts = opts || {};
        clearEmptyState();
        var d = document.createElement('div');
        d.className = 'ai-m error';
        var head = document.createElement('div');
        head.className = 'errhead';
        head.textContent = opts.head || 'Something went wrong';
        d.appendChild(head);
        var body = document.createElement('div');
        body.textContent = message;
        d.appendChild(body);
        if (opts.retry) {
            var row = document.createElement('div');
            row.className = 'errrow';
            var b = document.createElement('button');
            b.className = 'tbtn';
            b.textContent = 'Retry';
            b.onclick = function () { row.remove(); opts.retry(); };
            row.appendChild(b);
            d.appendChild(row);
        }
        chatEl.appendChild(d);
        scrollDown();
        return d;
    }

    // a collapsible line: tool call, thinking, or a debug dump
    function addStep(label, detail, opts) {
        opts = opts || {};
        clearEmptyState();
        var step = document.createElement('div');
        step.className = 'ai-step' + (opts.open ? ' open' : '') + (opts.kind ? ' ' + opts.kind : '');
        if (opts.dev) {
            step.dataset.dev = '1';
            if (!devMode) step.style.display = 'none';
        }

        var head = document.createElement('div');
        head.className = 'ai-step-head';
        head.appendChild(svgIcon('#i-chev', 'icon ai-chev'));
        if (opts.tool) {
            var t = document.createElement('span');
            t.className = 'tool';
            t.textContent = opts.tool;
            head.appendChild(t);
        }
        var lab = document.createElement('span');
        lab.className = 'ai-step-label';
        lab.textContent = label;
        head.appendChild(lab);
        var time = document.createElement('span');
        time.className = 'ai-step-time';
        head.appendChild(time);

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
            el: step,
            append: function (t) {
                if (opts.markdown) body.innerHTML += renderMarkdown(t);
                else body.textContent += (body.textContent ? '\n' : '') + t;
            },
            set: function (t) {
                if (opts.markdown) body.innerHTML = renderMarkdown(t);
                else body.textContent = t;
            },
            setLabel: function (t) { lab.textContent = t; },
            setTime: function (t) { time.textContent = t; },
            label: function () { return lab.textContent; },
            fail: function () { step.classList.add('err'); },
            body: body,
        };
    }
    function addShot(b64, mime) {
        clearEmptyState();
        var img = document.createElement('img');
        img.className = 'ai-shot';
        img.src = 'data:' + mime + ';base64,' + b64;
        img.onclick = function () { img.classList.toggle('big'); };
        chatEl.appendChild(img);
        scrollDown();
    }

    // ---------- status: the chip, the progress bar, the changed files ----------
    function setChip(text, kind) {
        chipEl.textContent = text;
        chipEl.className = kind ? kind : '';
        chipEl.id = 'ai-chip';
    }
    function setAction(text) { actEl.textContent = text || 'working…'; }
    function setMeta(text) { metaEl.textContent = text || ''; }

    var changed = {};   // path -> 'edit' | 'delete'
    function resetChanges() { changed = {}; paintChanges(); }
    function noteChange(path, kind) { changed[path] = kind; paintChanges(); }
    function paintChanges() {
        var paths = Object.keys(changed);
        filesEl.classList.toggle('show', paths.length > 0);
        if (!paths.length) return;
        filesTitle.textContent = paths.length + (paths.length === 1 ? ' file changed' : ' files changed') + ' — show';
        filesList.innerHTML = '';
        paths.sort().forEach(function (p) {
            var b = document.createElement('span');
            b.className = 'fitem' + (changed[p] === 'delete' ? ' del' : '');
            b.appendChild(icon('file'));
            b.appendChild(document.createTextNode(assetRel(p)));
            b.title = changed[p] === 'delete' ? 'deleted' : 'open in the assets browser';
            b.onclick = function () {
                if (changed[p] !== 'delete' && window.__o2RevealAsset) window.__o2RevealAsset(assetRel(p));
            };
            filesList.appendChild(b);
        });
    }
    filesEl.querySelector('.fhead').onclick = function () {
        filesEl.classList.toggle('open');
        var paths = Object.keys(changed).length;
        filesTitle.textContent = paths + (paths === 1 ? ' file changed' : ' files changed') +
            (filesEl.classList.contains('open') ? ' — hide' : ' — show');
    };
    // ---------- editor-side tool implementations ----------
    // Claude's own tools do the file work on the server; these are the calls
    // that only make sense inside the running editor.

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
        return fetch(o2Base + '/api/assets/file?path=' + encodeURIComponent(a.path)).then(function (r) {
            if (!r.ok) throw new Error('no such scene: ' + a.path + ' (paths are relative to Assets)');
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
        rebuild_assets: toolRebuild, screenshot: toolScreenshot,
        click: toolClick, type_text: toolTypeText, press_key: toolPressKey,
        read_log: toolReadLog, wait: toolWait, play_mode: toolPlayMode,
        open_scene: toolOpenScene, save_scene: toolSaveScene,
        scene_tree: toolSceneTree, view_info: toolViewInfo, run_script: toolRunScript,
    };
    window.__o2aiExec = EXEC;       // debug/testing handles
    window.__o2aiEvents = function () { return events; };

    // Claude writes files on the server; the running editor works off its
    // own MEMFS copy, so pull what changed under Assets into it
    function syncChangedFile(rel) {
        var m = rel.match(/^Assets\/(.+)$/);
        if (!m || !Module || !Module.FS) return;
        var inner = m[1];
        fetch(o2Base + '/api/assets/file?path=' + encodeURIComponent(inner)).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return r.arrayBuffer();
        }).then(function (buf) {
            var full = '/project/Assets/' + inner;
            Module.FS.mkdirTree(full.substring(0, full.lastIndexOf('/')));
            Module.FS.writeFile(full, new Uint8Array(buf));
        }).catch(function (e) { console.warn('[ai] MEMFS sync failed for ' + rel, e); });
    }

    var sessionCwd = '';
    function shortPath(v) {
        return sessionCwd && v.indexOf(sessionCwd + '/') === 0 ? v.slice(sessionCwd.length + 1) : v;
    }

    // ---------- permission prompts ----------
    var permissionBlocks = {};
    function showPermission(ev) {
        if (permissionBlocks[ev.id]) return;
        clearEmptyState();
        var d = document.createElement('div');
        d.className = 'ai-perm';
        var head = document.createElement('div');
        head.className = 'permhead';
        head.textContent = 'Allow ' + toolLabel(ev.tool, ev.input).replace(/^▸ /, '') + '?';
        d.appendChild(head);
        var detail = document.createElement('pre');
        detail.textContent = shortPath(JSON.stringify(ev.input || {}, null, 2));
        d.appendChild(detail);
        var row = document.createElement('div');
        row.className = 'permrow';
        [['Allow', 'allow', false], ['Always allow', 'allow', true], ['Deny', 'deny', false]].forEach(function (b) {
            var btn = document.createElement('button');
            btn.className = 'tbtn' + (b[1] === 'deny' ? ' quiet' : '');
            btn.textContent = b[0];
            if (b[2] && !(ev.suggestions && ev.suggestions.length)) btn.disabled = true;
            btn.onclick = function () {
                post('permission', { id: ev.id, behavior: b[1], always: b[2] }).catch(function () {});
            };
            row.appendChild(btn);
        });
        d.appendChild(row);
        chatEl.appendChild(d);
        permissionBlocks[ev.id] = d;
        scrollDown();
        setChip('waiting for you', 'busy');
        setAction('needs permission: ' + ev.tool.replace(/^mcp__o2__/, ''));
    }
    function resolvePermissionUi(id, behavior) {
        var d = permissionBlocks[id];
        if (!d) return;
        var label = d.querySelector('.permhead').textContent;
        d.replaceWith(addStep((behavior === 'allow' ? '✓ allowed · ' : '⊘ denied · ') + label,
                              '', { kind: behavior === 'allow' ? '' : 'err' }).el);
        delete permissionBlocks[id];
        if (running) { setChip('working', 'busy'); setAction('working…'); }
    }

    function removeDeletedFile(rel) {
        var m = rel.match(/^Assets\/(.+)$/);
        if (!m || !Module || !Module.FS) return;
        try { Module.FS.unlink('/project/Assets/' + m[1]); } catch (e) {}
    }

    // ---------- conversations of this tab ----------
    function loadHistory() {
        fetch(o2Base + '/api/agent/sessions').then(function (r) { return r.json(); }).then(function (j) {
            var list = j.sessions || [];
            histDd.el.style.display = list.length ? '' : 'none';
            if (!list.length) return;
            histDd.options(list.map(function (h) {
                var short = h.title.length > 30 ? h.title.slice(0, 30) + '…' : h.title;
                return { value: h.id, label: short, short: short, title: h.title,
                         sub: '$' + (h.cost || 0).toFixed(2) };
            }));
            histDd.set(j.current || null);
        }).catch(function () {});
    }
    function resumeConversation(id) {
        if (running) { loadHistory(); return; }
        post('resume', { id: id || null }).then(function () {
            chatEl.innerHTML = '';
            resetChanges();
            if (id) addStep('· continuing conversation ' + id.slice(0, 8) + '…',
                            'Its history lives on the server; new messages append to it.', { open: true });
            else showEmptyState();
        }).catch(function (e) { addError(e.message); });
    }
    document.getElementById('ai-new').onclick = function () {
        if (running) return;
        post('reset').then(function () {
            chatEl.innerHTML = '';
            resetChanges();
            showEmptyState();
            loadHistory();
        }).catch(function () {});
    };

    var MAIN_ARG = { Read: 'file_path', Write: 'file_path', Edit: 'file_path', MultiEdit: 'file_path',
                     Bash: 'command', Grep: 'pattern', Glob: 'pattern', Task: 'description', Agent: 'description',
                     WebFetch: 'url', WebSearch: 'query', Skill: 'skill', NotebookEdit: 'notebook_path' };
    // What the tool is doing, in the user's words, for the progress line
    var HUMAN = { Read: 'reading', Write: 'writing', Edit: 'editing', MultiEdit: 'editing', Bash: 'running',
                  Grep: 'searching', Glob: 'listing files', Task: 'subtask', Skill: 'skill',
                  screenshot: 'taking a screenshot', scene_tree: 'reading the scene', view_info: 'reading the view',
                  run_script: 'running a script', open_scene: 'opening a scene', save_scene: 'saving the scene',
                  play_mode: 'toggling play', rebuild_assets: 'rebuilding assets', read_log: 'reading the log',
                  click: 'clicking', type_text: 'typing', press_key: 'pressing a key', wait: 'waiting' };
    function toolLabel(name, args, sub) {
        args = args || {};
        var short = name.replace(/^mcp__o2__/, '');
        var main = MAIN_ARG[name];
        var brief;
        if (main && args[main] !== undefined) {
            var v = shortPath(String(args[main])).replace(/\s+/g, ' ');
            brief = v.length > 90 ? v.slice(0, 90) + '…' : v;
            if (name === 'Grep' && args.path) brief += '  in ' + shortPath(String(args.path));
        } else {
            brief = Object.keys(args).map(function (k) {
                var v = shortPath(String(args[k]));
                return k + ': ' + (v.length > 50 ? v.slice(0, 50) + '…' : v);
            }).join(', ');
        }
        return (sub ? '  ↳ ' : '▸ ') + short + (brief ? '(' + brief + ')' : '()');
    }
    function toolArg(name, args) {
        args = args || {};
        var main = MAIN_ARG[name];
        if (main && args[main] !== undefined) {
            var v = shortPath(String(args[main])).replace(/\s+/g, ' ');
            var extra = name === 'Grep' && args.path ? '  in ' + shortPath(String(args.path)) : '';
            return (v.length > 88 ? v.slice(0, 88) + '…' : v) + extra;
        }
        return Object.keys(args).map(function (k) {
            var v = shortPath(String(args[k]));
            return k + ': ' + (v.length > 44 ? v.slice(0, 44) + '…' : v);
        }).join(', ');
    }

    function humanAction(name, args) {
        var short = name.replace(/^mcp__o2__/, '');
        var verb = HUMAN[short] || short;
        var main = MAIN_ARG[name];
        var what = main && args && args[main] !== undefined ? shortPath(String(args[main])).replace(/\s+/g, ' ') : '';
        if (what.length > 60) what = what.slice(0, 60) + '…';
        return what ? verb + ' ' + what : verb;
    }

    // ---------- the stream from the server ----------
    var events = [];          // everything received, for the Log button
    var source = null;
    var steps = {};           // tool_use id -> ui step
    var bubble = null, bubbleText = '';
    var reviewStep = null, reviewText = '';
    var thinkStep = null, thinkText = '';
    var runStarted = 0, runTools = 0, lastPrompt = '';

    function ensureStream() {
        if (source && source.readyState !== 2) return;
        source = new EventSource(o2Base + '/api/agent/stream');
        source.onmessage = function (e) {
            var ev;
            try { ev = JSON.parse(e.data); } catch (err) { return; }
            events.push(ev);
            if (devMode && ev.type !== 'delta' && ev.type !== 'thinking')
                addStep('◇ ' + ev.type, JSON.stringify(ev, null, 2), { dev: true });
            try { onEvent(ev); } catch (err) { console.error('[ai] event failed', ev, err); }
        };
        source.onerror = function () {
            if (running) { setChip('connection lost…', 'err'); setAction('reconnecting'); }
        };
    }

    var typingEl = null;
    function showTyping() {
        if (typingEl) return;
        clearEmptyState();
        typingEl = document.createElement('div');
        typingEl.className = 'ai-typing';
        typingEl.innerHTML = '<i></i><i></i><i></i>';
        chatEl.appendChild(typingEl);
        scrollDown();
    }
    function hideTyping() {
        if (typingEl) { typingEl.remove(); typingEl = null; }
    }

    function currentBubble() {
        hideTyping();
        if (!bubble) { clearEmptyState(); bubble = addMsg('model', ''); bubbleText = ''; }
        return bubble;
    }

    function tickMeta() {
        if (!running) return;
        var s = Math.round((Date.now() - runStarted) / 1000);
        setMeta(runTools + (runTools === 1 ? ' action' : ' actions') + ' · ' +
                (s < 60 ? s + ' s' : Math.floor(s / 60) + ' min ' + (s % 60) + ' s'));
    }
    setInterval(tickMeta, 1000);

    function onEvent(ev) {
        switch (ev.type) {
            case 'hello':
                if (ev.running && !running) { busy(true); setAction('the agent is still working'); }
                (ev.pending || []).forEach(showPermission);
                loadHistory();
                break;
            case 'init':
                if (ev.cwd) sessionCwd = ev.cwd;
                addStep('· session · ' + ev.model + ' · Claude Code ' + ev.version + ' · ' + (ev.tools || []).length +
                        ' tools · ' + ev.mode + ' · ' + (ev.auth || (ev.apiKeySource === 'none' ? 'host' : 'api key')),
                        JSON.stringify({ tools: ev.tools, mcp: ev.mcp, slash_commands: ev.slash }, null, 2),
                        { dev: true });
                break;
            case 'thinking':
                hideTyping();
                if (!thinkStep) { thinkStep = addStep('thinking…', '', { markdown: true, kind: 'thinking', dev: true }); thinkText = ''; }
                thinkText += ev.text;
                thinkStep.set(thinkText);
                setAction('thinking');
                break;
            case 'delta':
                if (ev.review) { reviewText += ev.text; if (reviewStep) reviewStep.set(reviewText); break; }
                if (ev.sub) break;                      // subagent chatter stays in the steps
                bubbleText += ev.text;
                currentBubble().textContent = bubbleText;
                scrollDown();
                break;
            case 'text':
                if (thinkStep) {
                    thinkStep.setLabel('thinking · ' + (thinkText.replace(/[#*`]/g, '').split('\n')
                        .filter(function (l) { return l.trim(); })[0] || 'reasoning').slice(0, 90));
                    thinkStep = null;
                }
                if (ev.review) {
                    reviewText = ev.text;
                    if (!reviewStep) reviewStep = addStep('self-review of the run', '', { markdown: true, kind: 'review', open: true });
                    reviewStep.set(reviewText);
                    break;
                }
                if (ev.sub) { addStep('  ↳ subtask replied', ev.text, { dev: true }); break; }
                currentBubble().innerHTML = renderMarkdown(ev.text);
                bubble = null; bubbleText = '';
                scrollDown();
                break;
            case 'tool_use': {
                hideTyping();
                if (bubble) { bubble = null; bubbleText = ''; }
                thinkStep = null;
                runTools++;
                // the model's own tool lookups say nothing about the task
                var noise = ev.name === 'ToolSearch';
                steps[ev.id] = { ui: addStep(toolArg(ev.name, ev.input), 'arguments ' + shortPath(JSON.stringify(ev.input || {}, null, 2)),
                                             { tool: (ev.sub ? '↳ ' : '') + ev.name.replace(/^mcp__o2__/, ''), dev: noise }),
                                 started: Date.now(), name: ev.name };
                if (!noise) { setAction(humanAction(ev.name, ev.input)); tickMeta(); }
                break;
            }
            case 'tool_result': {
                var st = steps[ev.id];
                if (!st) break;
                var took = Date.now() - st.started;
                st.ui.append('result (' + took + ' ms)\n' + ev.text);
                st.ui.setTime(took > 1500 ? (took / 1000).toFixed(1) + ' s' : took + ' ms');
                if (ev.is_error) st.ui.fail();
                break;
            }
            case 'tool_request':
                runBrowserTool(ev);
                break;
            case 'fs':
                (ev.changed || (ev.path ? [ev.path] : [])).forEach(function (p) { syncChangedFile(p); noteChange(p, 'edit'); });
                (ev.deleted || []).forEach(function (p) { removeDeletedFile(p); noteChange(p, 'delete'); });
                break;
            case 'queued':
                addStep('· message queued (' + ev.position + ')',
                        'It will be sent as soon as the current turn ends.', { open: true });
                break;
            case 'permission_request':
                hideTyping();
                showPermission(ev);
                break;
            case 'permission_resolved':
                resolvePermissionUi(ev.id, ev.behavior);
                break;
            case 'result': {
                var u = ev.usage || {};
                var cost = ev.cost != null ? '$' + ev.cost.toFixed(3) : '';
                addStep('· ' + ev.subtype + ' · ' + ev.turns + ' turns · ' + Math.round((ev.ms || 0) / 1000) + ' s · ' + cost +
                        ' · in ' + (u.input_tokens || 0) + ' (+' + (u.cache_read_input_tokens || 0) + ' cached) / out ' +
                        (u.output_tokens || 0) + ' tok', JSON.stringify(ev, null, 2), { dev: true });
                lastResult = ev;
                if (ev.denials && ev.denials.length)
                    addStep('⊘ denied tools: ' + ev.denials.join(', '),
                            'The permission guard refused these calls.', { kind: 'err' });
                break;
            }
            case 'error':
                lastError = ev.message;
                break;
            case 'done':
                finishRun(ev);
                break;
        }
    }

    function runBrowserTool(ev) {
        var impl = EXEC[ev.name];
        Promise.resolve().then(function () {
            if (!impl) throw new Error('no such editor tool: ' + ev.name);
            return impl(ev.args || {});
        }).then(function (out) {
            if (out && out.image) addShot(out.image.b64, out.image.mime);
            return post('tool_result', { id: ev.id, result: out });
        }, function (e) {
            return post('tool_result', { id: ev.id, result: { error: String(e && e.message || e) } });
        }).catch(function (e) { console.error('[ai] tool_result post failed', e); });
    }

    function post(what, body) {
        return fetch(o2Base + '/api/agent/' + what, {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body || {}),
        }).then(function (r) {
            return r.json().then(function (j) {
                if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
                return j;
            });
        });
    }

    function busy(on) {
        running = on;
        document.getElementById('btn-ai').classList.toggle('busy', on);
        barEl.classList.toggle('show', on);
        sendBtn.classList.toggle('queue', on);
        sendBtn.title = on ? 'Queued until the current turn ends' : 'Send (Enter)';
        inputEl.placeholder = on
            ? 'Next message — sent when this turn ends…'
            : 'What should the agent do? For example: add a title label to the scene…';
        if (on) { setChip('working', 'busy'); runStarted = Date.now(); runTools = 0; tickMeta(); showTyping(); }
        else { hideTyping(); setChip('ready'); setMeta(''); loadHistory(); }
    }

    // ---------- a run from start to finish ----------
    var pendingReview = false, lastError = null, lastResult = null;

    // A failed turn should read as failed: report it in the user's words,
    // offer the retry, and never chase it with a self-review of nothing
    function finishRun(ev) {
        bubble = null; bubbleText = ''; thinkStep = null;
        for (var id in permissionBlocks) { permissionBlocks[id].remove(); delete permissionBlocks[id]; }

        var failed = lastError || (lastResult && lastResult.subtype !== 'success');
        if (!ev.review && !ev.aborted && !failed && pendingReview) {
            pendingReview = false;
            reviewStep = null; reviewText = '';
            startRun(REVIEW_PROMPT, true);
            return;
        }
        pendingReview = false;
        if (ev.review) reviewStep = null;

        if (ev.aborted) {
            addStep('· stopped', 'The turn was interrupted at your request.', { open: true });
        } else if (failed) {
            var msg = lastError || (lastResult && (lastResult.errors || []).join('\n')) || 'the turn ended with an error';
            var overloaded = /529|overload|rate.?limit|429/i.test(msg);
            var prompt = lastPrompt;
            addError(msg, {
                head: overloaded ? 'The model is overloaded' : 'The turn failed',
                retry: prompt ? function () { runAgent(prompt, true); } : null,
            });
            setChip('error', 'err');
        }
        var summary = null;
        if (!failed && !ev.aborted && lastResult) {
            var c = lastResult.cost != null ? ' · $' + lastResult.cost.toFixed(3) : '';
            summary = 'done · ' + Math.round((lastResult.ms || 0) / 1000) + ' s' + c;
        }
        lastError = null; lastResult = null;
        busy(false);
        if (failed) setChip('error', 'err');
        else if (summary) setChip(summary);
    }

    function credentials() {
        return authMode === 'sub'
            ? { oauthToken: cleanSecret(oauthEl.value) }
            : { apiKey: cleanSecret(keyEl.value), workspaceId: cleanSecret(workspaceEl.value) };
    }

    async function startRun(text, review) {
        var model = modelDd.get() || DEFAULT_MODEL;
        setSetting('o2ai_claude_model', model);
        ensureStream();
        busy(true);
        setAction(review ? 'reviewing its own run' : 'starting: ' + model);
        try {
            await post('start', Object.assign({ text: text, model: model, effort: effortDd.get(),
                                               mode: modeDd.get(), review: !!review }, credentials()));
        } catch (e) {
            addError(e.message, { retry: function () { startRun(text, review); } });
            busy(false);
            setChip('error', 'err');
        }
    }

    function runAgent(userText, isRetry) {
        if (!credential() && !/localhost|127\.0\.0\.1/.test(location.hostname)) {
            openSettings();
            (authMode === 'sub' ? oauthEl : keyEl).focus();
            addError(authMode === 'sub'
                ? 'Paste a subscription token in settings — run `claude setup-token` to mint one.'
                : 'Add your Anthropic API key in settings — the agent runs on it.',
                { head: 'Credential required' });
            return;
        }
        lastPrompt = userText;
        if (!isRetry) addMsg('user', userText);
        if (running) {
            // typed during a turn: the server queues it after the current one
            post('start', Object.assign({ text: userText, model: modelDd.get() || DEFAULT_MODEL,
                                         effort: effortDd.get(), mode: modeDd.get(), review: false }, credentials()))
                .catch(function (e) { addError(e.message); });
            return;
        }
        resetChanges();
        pendingReview = reviewToggle.get();
        startRun(userText, false);
    }

    // ---------- self-review ----------
    // One extra turn at the end of a run: the model critiques its own work,
    // which is where the useful signal for improving the agent lives.
    var REVIEW_PROMPT = [
        'The task is over. Step out of it and review your own run as an engineer would review a colleague.',
        'Answer in markdown, in the language of my first message, under these headings and nothing else:',
        '',
        '## Mistakes',
        'What you actually got wrong: wrong assumptions, calls that failed, things you had to redo.',
        'Write "none" if there were none.',
        '',
        '## Waste',
        'Where the run was inefficient: repeated lookups, reading what you already knew, exploring instead of acting,',
        'tools you should have used earlier or not at all.',
        '',
        '## What would have helped',
        'Knowledge or a tool that was missing and would have shortened the run - phrased so it can be added to the',
        'agent briefing or built as a new tool.',
        '',
        '## Advice for next time',
        'Two or three concrete rules you would follow on a similar task.',
        '',
        'Be blunt and concrete. Do not praise yourself, do not restate what the task was, do not repeat the summary.',
        'Do not use any tools for this.',
    ].join('\n');

    // ---------- UI wiring ----------
    function openDlg() {
        if (window.__o2CloseBrowser) window.__o2CloseBrowser(); // it renders above the chat
        dlg.classList.add('open');
        back.classList.add('open');
        inputEl.focus();
        ensureStream();
        showEmptyState();
        loadHistory();
        if (!modelsLoaded) loadModels();
    }
    function closeDlg() { dlg.classList.remove('open'); back.classList.remove('open'); }
    document.getElementById('btn-ai').onclick = function () {
        if (dlg.classList.contains('open')) closeDlg(); else openDlg();
    };
    document.getElementById('ai-close').onclick = closeDlg;
    back.onclick = closeDlg; // the agent keeps running in the background

    // whole session as JSON: every event the server streamed
    document.getElementById('ai-log').onclick = function () {
        var dump = JSON.stringify({ model: modelDd.get(), events: events }, null, 2);
        console.log('[ai] session log', events);
        var btn = document.getElementById('ai-log');
        navigator.clipboard.writeText(dump).then(function () {
            btn.textContent = 'Copied';
            setTimeout(function () { btn.textContent = 'Copy log'; }, 1200);
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
        pendingReview = false;
        setAction('stopping…');
        post('stop').catch(function () {});
    };
    function send() {
        var t = inputEl.value.trim();
        if (!t) return;
        inputEl.value = '';
        inputEl.style.height = '';
        runAgent(t);
    }
    sendBtn.onclick = send;
    var composerEl = document.getElementById('ai-composer');
    inputEl.addEventListener('focus', function () { composerEl.classList.add('focus'); });
    inputEl.addEventListener('blur', function () { composerEl.classList.remove('focus'); });
    inputEl.addEventListener('input', function () {
        inputEl.style.height = 'auto';
        inputEl.style.height = Math.min(inputEl.scrollHeight, 190) + 'px';
    });
    inputEl.addEventListener('keydown', function (e) {
        if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); send(); }
        e.stopPropagation(); // don't leak typing into the engine
    });
    inputEl.addEventListener('keyup', function (e) { e.stopPropagation(); });
    applyDev();
    showEmptyState();
})();
