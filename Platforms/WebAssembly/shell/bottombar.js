// Bottom bar: assets change status, zip download/upload, reload.

// ---- bottom bar ---------------------------------------------------
(function () {
    var stateEl = document.getElementById('assets-state');
    var logEl = document.getElementById('changes-log');
    var lastChanges = [];

    function refresh() {
        fetch(o2Base + '/api/assets/status').then(function (r) { return r.json(); }).then(function (s) {
            lastChanges = s.changes || [];
            if (!lastChanges.length) {
                stateEl.textContent = 'Assets: no changes';
                stateEl.classList.remove('dirty');
            } else {
                stateEl.textContent = 'Assets: ' + lastChanges.length + ' changed';
                stateEl.classList.add('dirty');
            }
        }).catch(function () {
            stateEl.textContent = 'server unreachable';
            stateEl.classList.add('dirty');
        });
    }

    document.getElementById('btn-changes').onclick = function () { openChangesDialog(); };

    document.getElementById('btn-download').onclick = function () {
        location.href = o2Base + '/api/assets/zip';
    };

    var uploadInput = document.getElementById('upload-input');
    document.getElementById('btn-upload').onclick = function () { uploadInput.click(); };
    uploadInput.onchange = function () {
        var f = uploadInput.files[0];
        if (!f) return;
        modal.confirm('Load assets', 'Replace ALL Assets of this session with "' + f.name + '" and reload the editor?', 'Replace')
            .then(function (ok) {
                if (!ok) { uploadInput.value = ''; return; }
                setStatus('Uploading assets…');
                fetch(o2Base + '/api/assets/upload', { method: 'POST', body: f }).then(function (r) {
                    if (!r.ok) throw new Error('HTTP ' + r.status);
                    // The upload only replaces the sources; the editor reads content from
                    // BuiltAssets, so the reloaded page has to rebuild them once
                    sessionStorage.setItem('o2_rebuild_after_load', '1');
                    return (window.__o2DrainMirror ? window.__o2DrainMirror() : Promise.resolve())
                        .then(function () { location.reload(); });
                }).catch(function (e) {
                    setStatus(null);
                    modal.alert('Upload failed', e.message);
                }).finally(function () { uploadInput.value = ''; });
            });
    };

    document.getElementById('btn-reload').onclick = function () {
        // let the BuiltAssets mirror finish, or the server keeps a half-written build
        setStatus('Flushing changes…');
        (window.__o2DrainMirror ? window.__o2DrainMirror() : Promise.resolve())
            .then(function () { location.reload(); });
    };

    setInterval(refresh, 7000);
    refresh();
})();
