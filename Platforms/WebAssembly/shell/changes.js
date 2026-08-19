// Changes dialog: before/after comparison against the pristine snapshot.

// ---- changes dialog: before/after comparison ----------------------
(function () {
    var dlg = document.getElementById('changes');
    var back = document.getElementById('changes-back');
    var listEl = document.getElementById('changes-list');
    var nameEl = document.getElementById('cv-name');
    var viewEl = document.getElementById('cv-view');
    var current = null;
    var diffEditor = null;

    function close() {
        dlg.classList.remove('open');
        back.classList.remove('open');
        resetView();
    }
    back.onclick = close;
    document.getElementById('ch-close').onclick = close;

    function resetView() {
        if (diffEditor) { diffEditor.dispose(); diffEditor = null; }
        viewEl.innerHTML = '<div class="placeholder">Before / after preview appears here</div>';
        nameEl.textContent = 'Select a change to compare';
        current = null;
    }

    window.openChangesDialog = function () {
        dlg.classList.add('open');
        back.classList.add('open');
        resetView();
        listEl.innerHTML = '<div class="ch-empty">Loading…</div>';
        fetch('/api/assets/status').then(function (r) { return r.json(); }).then(function (st) {
            var changes = st.changes || [];
            listEl.innerHTML = '';
            if (!changes.length) {
                listEl.innerHTML = '<div class="ch-empty">No changes against the pristine Assets</div>';
                return;
            }
            changes.forEach(function (c) {
                var row = document.createElement('div');
                row.className = 'ch-row';
                var badge = document.createElement('span');
                badge.className = 'badge ' + c.status;
                badge.textContent = c.status;
                row.appendChild(badge);
                var pathEl = document.createElement('span');
                pathEl.className = 'ch-path';
                pathEl.textContent = c.path;
                pathEl.title = c.path;
                row.appendChild(pathEl);
                row.onclick = function () {
                    listEl.querySelectorAll('.ch-row.selected').forEach(function (e) { e.classList.remove('selected'); });
                    row.classList.add('selected');
                    showDiff(c);
                };
                listEl.appendChild(row);
            });
        }).catch(function (e) {
            listEl.innerHTML = '<div class="ch-empty">Failed to load: ' + e.message + '</div>';
        });
    };

    function fileUrl(path, pristine) {
        return '/api/assets/file?path=' + encodeURIComponent(path) +
               (pristine ? '&base=pristine' : '') + '&t=' + Date.now();
    }

    function showDiff(c) {
        current = c;
        if (diffEditor) { diffEditor.dispose(); diffEditor = null; }
        nameEl.textContent = c.status + '  ' + c.path;
        var kind = fileKind(c.path);

        if (kind === 'image') {
            viewEl.innerHTML = '';
            var box = document.createElement('div');
            box.className = 'cv-imgs';
            ['Before (pristine)', 'After (session)'].forEach(function (cap, i) {
                var side = document.createElement('div');
                side.className = 'cv-side';
                var caption = document.createElement('div');
                caption.className = 'cv-cap';
                caption.textContent = cap;
                side.appendChild(caption);
                var missing = (i === 0 && c.status === 'A') || (i === 1 && c.status === 'D');
                if (missing) {
                    var m = document.createElement('div');
                    m.className = 'cv-missing';
                    m.textContent = i === 0 ? 'File did not exist' : 'File deleted';
                    side.appendChild(m);
                } else {
                    var ib = document.createElement('div');
                    ib.className = 'cv-imgbox';
                    var img = document.createElement('img');
                    img.src = fileUrl(c.path, i === 0);
                    img.onerror = function () {
                        ib.outerHTML = '<div class="cv-missing">Failed to load</div>';
                    };
                    ib.appendChild(img);
                    side.appendChild(ib);
                }
                box.appendChild(side);
            });
            viewEl.appendChild(box);
            return;
        }

        if (kind === 'text') {
            viewEl.innerHTML = '<div class="placeholder">Loading…</div>';
            var get = function (pristine, skip) {
                if (skip) return Promise.resolve('');
                return fetch(fileUrl(c.path, pristine)).then(function (r) {
                    return r.ok ? r.text() : '';
                });
            };
            Promise.all([
                get(true, c.status === 'A'),
                get(false, c.status === 'D'),
            ]).then(function (texts) {
                if (current !== c) return;
                viewEl.innerHTML = '<div id="cv-diff"></div>';
                loadMonaco().then(function (monaco) {
                    if (current !== c) return;
                    diffEditor = monaco.editor.createDiffEditor(document.getElementById('cv-diff'), {
                        readOnly: true,
                        renderSideBySide: true,
                        automaticLayout: true,
                        theme: 'vs',
                        fontSize: 12,
                        minimap: { enabled: false },
                    });
                    diffEditor.setModel({
                        original: monaco.editor.createModel(texts[0], undefined),
                        modified: monaco.editor.createModel(texts[1], undefined),
                    });
                }).catch(function () {
                    if (current !== c) return;
                    // CDN unavailable: plain side-by-side text
                    viewEl.innerHTML = '<div class="cv-imgs">' +
                        '<div class="cv-side"><div class="cv-cap">Before (pristine)</div><textarea readonly style="flex:1;border:none;resize:none;font:12px ui-monospace,Menlo,monospace;padding:8px"></textarea></div>' +
                        '<div class="cv-side"><div class="cv-cap">After (session)</div><textarea readonly style="flex:1;border:none;resize:none;font:12px ui-monospace,Menlo,monospace;padding:8px"></textarea></div></div>';
                    var tas = viewEl.querySelectorAll('textarea');
                    tas[0].value = texts[0];
                    tas[1].value = texts[1];
                });
            });
            return;
        }

        viewEl.innerHTML = '<div class="placeholder">Binary file — no preview (' +
            (c.status === 'A' ? 'added' : c.status === 'D' ? 'deleted' : 'modified') + ')</div>';
    }
})();
