// Session id, boot overlay, project snapshot and the emscripten Module.

console.log('[shell] editor shell; build-tag=editor-4-modular');

// ---- session bootstrap (per browser tab) --------------------------
var sid = sessionStorage.getItem('o2sid');
if (!sid) {
    sid = Math.random().toString(36).slice(2) + Math.random().toString(36).slice(2);
    sessionStorage.setItem('o2sid', sid);
}
document.cookie = 'o2sid=' + sid + '; Path=/; SameSite=Strict';
window.o2fsEndpoint = '/api';

var statusEl = document.getElementById('status');
var statusText = document.getElementById('status-text');
var progressBar = document.querySelector('#progress > span');
function setStatus(text, frac) {
    if (!text) { statusEl.classList.add('hidden'); return; }
    statusEl.classList.remove('hidden');
    statusText.textContent = text;
    if (typeof frac === 'number')
        progressBar.style.width = (frac * 100).toFixed(1) + '%';
}


// ---- snapshot streaming ------------------------------------------
function loadSnapshot(done, fail) {
    fetch('/api/fs/snapshot').then(function (resp) {
        if (!resp.ok) throw new Error('snapshot HTTP ' + resp.status);
        var total = +resp.headers.get('X-Uncompressed-Length') || 0;
        var reader = resp.body.getReader();
        var chunks = [], loaded = 0;
        function pump() {
            return reader.read().then(function (r) {
                if (r.done) return;
                chunks.push(r.value);
                loaded += r.value.length;
                setStatus('Downloading project… ' + (loaded / 1048576).toFixed(1) + ' MB', total ? loaded / total : 0);
                return pump();
            });
        }
        return pump().then(function () {
            var buf = new Uint8Array(loaded), off = 0;
            for (var i = 0; i < chunks.length; i++) { buf.set(chunks[i], off); off += chunks[i].length; }
            return buf;
        });
    }).then(function (buf) {
        var magic = String.fromCharCode.apply(null, buf.subarray(0, 8));
        if (magic !== 'O2SNAP01') throw new Error('bad snapshot magic');
        var indexLen = new DataView(buf.buffer, buf.byteOffset + 8, 4).getUint32(0, true);
        var index = JSON.parse(new TextDecoder().decode(buf.subarray(12, 12 + indexLen)));
        var off = 12 + indexLen;

        setStatus('Unpacking project…', 1);
        var FS = Module.FS;
        var madeDirs = {};
        function mkdirs(dir) {
            if (madeDirs[dir]) return;
            FS.mkdirTree(dir);
            madeDirs[dir] = true;
        }
        for (var i = 0; i < index.files.length; i++) {
            var f = index.files[i];
            var full = '/project/' + f.p;
            mkdirs(full.substring(0, full.lastIndexOf('/')));
            FS.writeFile(full, buf.subarray(off, off + f.s));
            if (f.m) try { FS.utime(full, f.m, f.m); } catch (e) {}
            off += f.s;
        }
        mkdirs('/project/Bin/WebAssembly');
        console.log('[shell] snapshot unpacked:', index.files.length, 'files');
        done();
    }).catch(function (e) {
        console.error('[shell] snapshot failed', e);
        setStatus('Failed to load project: ' + e.message);
        fail(e);
    });
}

// ---- patches for the AI agent's screenshot/input tools ------------
// keep the WebGL back buffer readable so canvas screenshots work
(function () {
    var orig = HTMLCanvasElement.prototype.getContext;
    HTMLCanvasElement.prototype.getContext = function (type, attrs) {
        if (this.id === 'canvas' && (type === 'webgl2' || type === 'webgl'))
            attrs = Object.assign({}, attrs, { preserveDrawingBuffer: true });
        return orig.call(this, type, attrs);
    };
    // synthetic PointerEvents have no active pointer: capture calls throw
    var sc = Element.prototype.setPointerCapture;
    Element.prototype.setPointerCapture = function (id) { try { return sc.call(this, id); } catch (e) {} };
    var rc = Element.prototype.releasePointerCapture;
    Element.prototype.releasePointerCapture = function (id) { try { return rc.call(this, id); } catch (e) {} };
})();

// Ring buffer of the engine's own output, so the AI agent (and anyone debugging)
// can read what the editor printed without scraping the Log window off a screenshot
var engineLogLines = [];
function engineLog(kind, text) {
    engineLogLines.push((kind === 'err' ? 'ERR ' : '') + text);
    if (engineLogLines.length > 500) engineLogLines.splice(0, engineLogLines.length - 500);
}

var Module = {
    canvas: (function () {
        var c = document.getElementById('canvas');
        c.addEventListener('webglcontextlost', function (e) {
            console.error('[shell] webglcontextlost');
            setStatus('WebGL context lost — reload the page');
            e.preventDefault();
        }, false);
        return c;
    })(),
    print: function (text) { console.log('[wasm.out]', text); engineLog('out', text); },
    printErr: function (text) { console.error('[wasm.err]', text); engineLog('err', text); },
    setStatus: function (text) {
        if (text) console.log('[shell.setStatus]', text);
        var m = text && text.match(/([^\(]+)\((\d+(\.\d+)?)\/(\d+)\)/);
        if (m) setStatus(m[1].trim(), parseFloat(m[2]) / parseFloat(m[4]));
        else if (text) setStatus(text);
        else setStatus(null);
    },
    preRun: [function () {
        Module.addRunDependency('project-snapshot');
        loadSnapshot(
            function () { Module.removeRunDependency('project-snapshot'); },
            function () { /* startup halts with the error shown */ });
    }],
    onRuntimeInitialized: function () {
        console.log('[shell.onRuntimeInitialized]');
        // Assets restored from a zip are only sources: build them before the user
        // wonders why the editor still shows the old project
        if (sessionStorage.getItem('o2_rebuild_after_load')) {
            sessionStorage.removeItem('o2_rebuild_after_load');
            setStatus('Building the restored assets…');
            setTimeout(function () {
                try { Module._o2_web_rebuild_assets(); }
                catch (e) { console.error('[shell] rebuild after upload failed', e); }
                setStatus(null);
            }, 1500);
        }
    },
    onAbort: function (reason) { console.error('[shell.onAbort]', reason); setStatus('Crashed: ' + reason); },
    onExit: function (code) { console.log('[shell.onExit]', code); }
};
setStatus('Downloading editor…');
window.onerror = function (e) { console.error('[shell]', e); };
