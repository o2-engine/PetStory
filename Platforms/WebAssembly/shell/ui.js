// Shared UI helpers: modal dialogs, Monaco loader, file kinds, icons.

// ---- modal dialogs (replace prompt/confirm/alert) ----------------
var modal = (function () {
    var back = document.getElementById('modal-back');
    var title = document.getElementById('modal-title');
    var msg = document.getElementById('modal-msg');
    var input = document.getElementById('modal-input');
    var buttons = document.getElementById('modal-buttons');
    var active = null;

    function close(result) {
        back.classList.remove('open');
        var a = active; active = null;
        if (a) a(result);
    }
    back.addEventListener('mousedown', function (e) { if (e.target === back) close(null); });
    input.addEventListener('keydown', function (e) {
        if (e.key === 'Enter') close(input.value.trim() || null);
        if (e.key === 'Escape') close(null);
    });

    function show(opts) {
        return new Promise(function (resolve) {
            active = resolve;
            title.textContent = opts.title || 'PetStory Editor';
            msg.textContent = opts.message || '';
            msg.style.display = opts.message ? '' : 'none';
            input.style.display = opts.input ? '' : 'none';
            input.value = opts.value || '';
            buttons.innerHTML = '';
            (opts.buttons || []).forEach(function (b) {
                var el = document.createElement('button');
                el.className = 'tbtn' + (b.primary ? '' : ' quiet');
                el.textContent = b.label;
                el.onclick = function () { close(b.value === 'INPUT' ? (input.value.trim() || null) : b.value); };
                buttons.appendChild(el);
            });
            back.classList.add('open');
            if (opts.input) setTimeout(function () { input.focus(); input.select(); }, 30);
        });
    }
    return {
        prompt: function (t, m, v) {
            return show({ title: t, message: m, input: true, value: v,
                buttons: [{ label: 'Cancel', value: null }, { label: 'OK', value: 'INPUT', primary: true }] });
        },
        confirm: function (t, m, okLabel) {
            return show({ title: t, message: m,
                buttons: [{ label: 'Cancel', value: null }, { label: okLabel || 'OK', value: true, primary: true }] });
        },
        alert: function (t, m) {
            return show({ title: t, message: m, buttons: [{ label: 'OK', value: true, primary: true }] });
        },
    };
})();


// ---- shared Monaco loader + file-kind helpers --------------------
var IMG_RE = /\.(png|jpe?g|gif|webp|svg)$/i;
var TEXT_RE = /\.(txt|json|js|ts|xml|md|csv|ini|cfg|atlas|scn|proto|meta|mtl|mat|anim|fntstyle|frag|vert|glsl|metal|shader|yaml|yml|lua|cpp|h|hpp|c)$/i;
function svgIcon(href, cls) {
    var ns = 'http://www.w3.org/2000/svg';
    var s = document.createElementNS(ns, 'svg');
    s.setAttribute('class', cls || 'icon');
    s.setAttribute('viewBox', '0 0 16 16');
    var u = document.createElementNS(ns, 'use');
    u.setAttribute('href', href);
    s.appendChild(u);
    return s;
}
function fileKind(name) {
    if (IMG_RE.test(name)) return 'image';
    if (TEXT_RE.test(name)) return 'text';
    return 'binary';
}
var monacoLoaded = null;
function loadMonaco() {
    if (monacoLoaded) return monacoLoaded;
    monacoLoaded = new Promise(function (resolve, reject) {
        var base = 'https://cdn.jsdelivr.net/npm/monaco-editor@0.52.2/min';
        var sc = document.createElement('script');
        sc.src = base + '/vs/loader.js';
        sc.onload = function () {
            window.require.config({ paths: { vs: base + '/vs' } });
            window.require(['vs/editor/editor.main'], function () { resolve(window.monaco); });
        };
        sc.onerror = function () { reject(new Error('monaco load failed')); };
        document.head.appendChild(sc);
    });
    return monacoLoaded;
}
