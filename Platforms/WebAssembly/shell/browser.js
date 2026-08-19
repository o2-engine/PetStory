// Assets browser: folder tree, preview and text editing, file operations.

// ---- file browser -------------------------------------------------
(function () {
    var browser = document.getElementById('browser');
    var treeEl = document.getElementById('tree');
    var pane = document.getElementById('pane');
    var paneView = document.getElementById('pane-view');
    var paneName = document.getElementById('pane-name');
    var saveBtn = document.getElementById('v-save');
    var ctx = document.getElementById('ctx');

    var curDir = '';                 // current directory (from the tree selection)
    var selected = null;             // {path, name, isDir, kind}
    var editorApi = null, openedPath = null;

    function iconFor(name, isDir, open) {
        if (isDir) return open ? '#i-folder-open' : '#i-folder';
        var ext = name.slice(name.lastIndexOf('.') + 1).toLowerCase();
        return {
            scn: '#i-f-scene', proto: '#i-f-proto', mat: '#i-f-mat',
            metal: '#i-f-shader', frag: '#i-f-shader', vert: '#i-f-shader', glsl: '#i-f-shader',
            js: '#i-f-script', ts: '#i-f-script', lua: '#i-f-script',
            json: '#i-f-json', atlas: '#i-f-atlas', anim: '#i-f-anim',
            ttf: '#i-f-font', otf: '#i-f-font', fntstyle: '#i-f-font',
            ogg: '#i-f-sound', wav: '#i-f-sound', mp3: '#i-f-sound',
            meta: '#i-f-meta', txt: '#i-f-text', md: '#i-f-text', xml: '#i-f-text',
        }[ext] || '#i-f-generic';
    }
    function thumbOrIcon(path, name, isDir, cls) {
        if (!isDir && IMG_RE.test(name)) {
            var img = document.createElement('img');
            img.loading = 'lazy';
            img.src = '/api/assets/file?path=' + encodeURIComponent(path);
            img.onerror = function () { img.replaceWith(svgIcon('#i-f-generic', cls)); };
            return img;
        }
        return svgIcon(iconFor(name, isDir), cls);
    }
    function joinPath(a, b) { return a ? a + '/' + b : b; }
    function parentOf(p) { var i = p.lastIndexOf('/'); return i < 0 ? '' : p.slice(0, i); }
    function baseOf(p) { var i = p.lastIndexOf('/'); return i < 0 ? p : p.slice(i + 1); }
    function api(list) { return fetch('/api/fs/list?dir=' + encodeURIComponent(list)).then(function (r) { return r.json(); }); }
    function assetsOp(body) {
        return fetch('/api/assets/op', {
            method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body),
        }).then(function (r) {
            if (!r.ok) return r.json().then(function (j) { throw new Error(j.error || ('HTTP ' + r.status)); });
        });
    }

    // ---------------- tree (lazy) ----------------
    // node: {path, name, isDir, kind, el, kidsEl, loaded}
    var nodes = {}; // path -> node (dirs and files)

    function makeRow(node, depth) {
        var row = document.createElement('div');
        row.className = 't-row';
        row.style.paddingLeft = (depth * 14 + 4) + 'px';
        row.draggable = !!node.path;

        var chev = document.createElement('span');
        chev.className = 'chev' + (node.isDir ? '' : ' leaf');
        chev.appendChild(svgIcon('#i-chev'));
        row.appendChild(chev);

        row.appendChild(thumbOrIcon(node.path, node.name, node.isDir, 'icon'));
        if (!node.isDir && IMG_RE.test(node.name)) row.lastChild.className = 'thumb';

        var label = document.createElement('span');
        label.className = 't-label';
        label.textContent = node.name;
        row.appendChild(label);

        chev.onclick = function (e) { e.stopPropagation(); if (node.isDir) toggle(node); };
        row.onclick = function () {
            selectNode(node);
            if (node.isDir) openDir(node.path, false);
            else { openDir(parentOf(node.path), false); previewFile(node.path, node.kind); }
        };
        row.ondblclick = function () { if (node.isDir) toggle(node, true); };
        row.oncontextmenu = function (e) {
            e.preventDefault();
            selectNode(node);
            showCtx(e, node);
        };
        attachDragSource(row, node);
        if (node.isDir) attachDropTarget(row, function () { return node.path; });
        return row;
    }

    function makeNode(parentKidsEl, path, name, isDir, kind, depth) {
        var node = { path: path, name: name, isDir: isDir, kind: kind, loaded: false, depth: depth };
        var holder = document.createElement('div');
        holder.className = 't-node';
        node.holder = holder;
        node.el = makeRow(node, depth);
        holder.appendChild(node.el);
        if (isDir) {
            node.kidsEl = document.createElement('div');
            node.kidsEl.className = 't-kids';
            holder.appendChild(node.kidsEl);
        }
        parentKidsEl.appendChild(holder);
        nodes[path] = node;
        return node;
    }

    function loadKids(node) {
        return api(node.path).then(function (d) {
            node.kidsEl.innerHTML = '';
            (d.dirs || []).forEach(function (f) {
                makeNode(node.kidsEl, joinPath(node.path, f.name), f.name, true, null, node.depth + 1);
            });
            (d.files || []).forEach(function (f) {
                makeNode(node.kidsEl, joinPath(node.path, f.name), f.name, false, f.kind, node.depth + 1);
            });
            node.loaded = true;
        });
    }

    function toggle(node, forceOpen) {
        var isOpen = node.holder.classList.contains('expanded');
        if (isOpen && !forceOpen) {
            node.holder.classList.remove('expanded');
            node.el.classList.remove('expanded');
            return Promise.resolve();
        }
        node.holder.classList.add('expanded');
        node.el.classList.add('expanded');
        return node.loaded ? Promise.resolve() : loadKids(node);
    }

    function selectNode(node) {
        selected = node ? { path: node.path, name: node.name, isDir: node.isDir, kind: node.kind } : null;
        document.querySelectorAll('.t-row.selected').forEach(function (e) { e.classList.remove('selected'); });
        if (node && node.el) node.el.classList.add('selected');
    }

    var rootNode;
    function buildTree() {
        treeEl.innerHTML = '';
        nodes = {};
        rootNode = makeNode(treeEl, '', 'Assets', true, null, 0);
        nodes[''] = rootNode;
        return toggle(rootNode, true);
    }

    // expand the tree down to `dir`
    function revealDir(dir) {
        var parts = dir ? dir.split('/') : [];
        var chain = Promise.resolve();
        var cur = '';
        parts.forEach(function (part) {
            var p = cur ? cur + '/' + part : part;
            chain = chain.then(function () {
                var n = nodes[p];
                return n && n.isDir ? toggle(n, true) : null;
            });
            cur = p;
        });
        return chain;
    }

    // refresh a directory's children in the tree (after fs changes)
    function refreshTreeDir(dir) {
        var n = nodes[dir];
        if (!n || !n.isDir) return Promise.resolve();
        // drop stale descendants from the index
        Object.keys(nodes).forEach(function (p) {
            if (p !== dir && (dir === '' ? p !== '' : p.indexOf(dir + '/') === 0) && p !== dir)
                delete nodes[p];
        });
        if (!n.holder.classList.contains('expanded')) { n.loaded = false; return Promise.resolve(); }
        return loadKids(n);
    }

    // remember the current directory and optionally expand the tree to it
    function openDir(dir, reveal) {
        curDir = dir;
        return reveal ? revealDir(dir) : Promise.resolve();
    }

    // ---------------- drag'n'drop ----------------
    function attachDragSource(el, node) {
        el.addEventListener('dragstart', function (e) {
            if (!node.path) { e.preventDefault(); return; }
            e.dataTransfer.setData('application/x-o2-path', node.path);
            e.dataTransfer.effectAllowed = 'move';
        });
    }
    function attachDropTarget(el, getDir) {
        el.addEventListener('dragover', function (e) {
            if (e.dataTransfer.types.indexOf('application/x-o2-path') >= 0 || e.dataTransfer.types.indexOf('Files') >= 0) {
                e.preventDefault();
                el.classList.add('drop-target');
                e.dataTransfer.dropEffect = e.dataTransfer.types.indexOf('Files') >= 0 ? 'copy' : 'move';
            }
        });
        el.addEventListener('dragleave', function () { el.classList.remove('drop-target'); });
        el.addEventListener('drop', function (e) {
            e.preventDefault();
            e.stopPropagation();
            el.classList.remove('drop-target');
            var dir = getDir();
            if (e.dataTransfer.files && e.dataTransfer.files.length) {
                uploadFiles(Array.prototype.slice.call(e.dataTransfer.files), dir);
                return;
            }
            var src = e.dataTransfer.getData('application/x-o2-path');
            if (!src) return;
            var dst = joinPath(dir, baseOf(src));
            if (src === dst || dst.indexOf(src + '/') === 0) return; // no-op or into itself
            assetsOp({ op: 'move', path: src, path2: dst })
                .then(function () { return afterChange([parentOf(src), dir]); })
                .catch(function (e2) { modal.alert('Move failed', e2.message); });
        });
    }
    function uploadFiles(files, dir) {
        (function next() {
            var f = files.shift();
            if (!f) { afterChange([dir]); return; }
            fetch('/api/assets/file?path=' + encodeURIComponent(joinPath(dir, f.name)), { method: 'PUT', body: f })
                .then(next);
        })();
    }

    // re-sync UI after a mutation: refresh the affected tree dirs
    function afterChange(dirs) {
        var uniq = {};
        (dirs || []).forEach(function (d) { uniq[d] = 1; });
        var chain = Promise.resolve();
        Object.keys(uniq).forEach(function (d) {
            chain = chain.then(function () { return refreshTreeDir(d); });
        });
        return chain;
    }

    // ---------------- context menu ----------------
    function mi(icon, label, fn) {
        var el = document.createElement('div');
        el.className = 'mi';
        el.appendChild(svgIcon(icon));
        el.appendChild(document.createTextNode(label));
        el.onclick = function () { hideCtx(); fn(); };
        return el;
    }
    function sep() { var s = document.createElement('div'); s.className = 'sep'; return s; }
    function hideCtx() { ctx.style.display = 'none'; }
    document.addEventListener('mousedown', function (e) { if (!ctx.contains(e.target)) hideCtx(); });

    function showCtx(e, node) {
        ctx.innerHTML = '';
        var dirOfNode = node.isDir ? node.path : parentOf(node.path);

        if (!node.isDir) {
            ctx.appendChild(mi('#i-open', 'Open', function () { previewFile(node.path, node.kind); }));
            ctx.appendChild(mi('#i-down', 'Download', function () {
                location.href = '/api/assets/file?path=' + encodeURIComponent(node.path) + '&download=1';
            }));
            ctx.appendChild(sep());
        }
        if (node.path) {
            ctx.appendChild(mi('#i-rename', 'Rename', function () {
                modal.prompt('Rename', 'New name:', node.name).then(function (name) {
                    if (!name || name === node.name) return;
                    assetsOp({ op: 'move', path: node.path, path2: joinPath(parentOf(node.path), name) })
                        .then(function () { return afterChange([parentOf(node.path)]); })
                        .catch(function (e2) { modal.alert('Rename failed', e2.message); });
                });
            }));
            ctx.appendChild(mi('#i-copy', 'Duplicate', function () {
                var copyName = node.name.replace(/(\.[^.]*)?$/, '_copy$1');
                assetsOp({ op: 'copy', path: node.path, path2: joinPath(parentOf(node.path), copyName) })
                    .then(function () { return afterChange([parentOf(node.path)]); })
                    .catch(function (e2) { modal.alert('Copy failed', e2.message); });
            }));
            ctx.appendChild(mi('#i-move', 'Move to…', function () {
                modal.prompt('Move', 'Target path inside Assets:', node.path).then(function (target) {
                    if (!target || target === node.path) return;
                    assetsOp({ op: 'move', path: node.path, path2: target })
                        .then(function () { return afterChange([parentOf(node.path), parentOf(target)]); })
                        .catch(function (e2) { modal.alert('Move failed', e2.message); });
                });
            }));
            ctx.appendChild(sep());
            ctx.appendChild(mi('#i-trash', 'Delete', function () {
                modal.confirm('Delete', 'Delete "' + node.name + '"' + (node.isDir ? ' and everything inside' : '') + '?', 'Delete')
                    .then(function (ok) {
                        if (!ok) return;
                        if (openedPath === node.path) closePane();
                        assetsOp({ op: 'delete', path: node.path })
                            .then(function () { return afterChange([parentOf(node.path)]); })
                            .catch(function (e2) { modal.alert('Delete failed', e2.message); });
                    });
            }));
            ctx.appendChild(sep());
        }
        ctx.appendChild(mi('#i-newfile', 'New file here', function () { newFile(dirOfNode); }));
        ctx.appendChild(mi('#i-newfolder', 'New folder here', function () { newFolder(dirOfNode); }));

        ctx.style.display = 'block';
        var x = Math.min(e.clientX, innerWidth - ctx.offsetWidth - 8);
        var y = Math.min(e.clientY, innerHeight - ctx.offsetHeight - 8);
        ctx.style.left = x + 'px';
        ctx.style.top = y + 'px';
    }
    // ---------------- preview / edit ----------------
    function closePane() {
        editorApi = null; openedPath = null;
        saveBtn.style.display = 'none';
        paneView.innerHTML = '<div class="placeholder">Select a file to preview</div>';
        paneName.textContent = '';
    }
    document.getElementById('v-close').onclick = closePane;

    function monacoLanguage(name) {
        var ext = name.slice(name.lastIndexOf('.') + 1).toLowerCase();
        return { js: 'javascript', ts: 'typescript', json: 'json', scn: 'json', proto: 'json',
                 meta: 'json', mat: 'json', anim: 'json', fntstyle: 'json', atlas: 'json',
                 xml: 'xml', md: 'markdown', html: 'html', css: 'css',
                 cpp: 'cpp', h: 'cpp', c: 'cpp', lua: 'lua', yml: 'yaml', yaml: 'yaml',
                 metal: 'cpp', frag: 'cpp', vert: 'cpp', glsl: 'cpp' }[ext] || 'plaintext';
    }

    function previewFile(path, kind) {
        editorApi = null;
        saveBtn.style.display = 'none';
        openedPath = path;
        paneName.innerHTML = '';
        paneName.appendChild(svgIcon(iconFor(baseOf(path), false)));
        paneName.appendChild(document.createTextNode(path));

        if (IMG_RE.test(path)) {
            paneView.innerHTML = '<div class="imgbox"><img alt=""></div>';
            paneView.querySelector('img').src = '/api/assets/file?path=' + encodeURIComponent(path) + '&t=' + Date.now();
        } else if (kind === 'text') {
            paneView.innerHTML = '<div class="placeholder">Loading…</div>';
            fetch('/api/assets/file?path=' + encodeURIComponent(path)).then(function (r) { return r.text(); })
                .then(function (text) {
                    if (openedPath !== path) return;
                    paneView.innerHTML = '<div id="monaco-holder"></div>';
                    saveBtn.style.display = '';
                    loadMonaco().then(function (monaco) {
                        if (openedPath !== path) return;
                        var ed = monaco.editor.create(document.getElementById('monaco-holder'), {
                            value: text,
                            language: monacoLanguage(path),
                            theme: 'vs',
                            automaticLayout: true,
                            minimap: { enabled: false },
                            fontSize: 12,
                        });
                        editorApi = { getValue: function () { return ed.getValue(); } };
                    }).catch(function () {
                        if (openedPath !== path) return;
                        paneView.innerHTML = '<textarea spellcheck="false"></textarea>';
                        var ta = paneView.querySelector('textarea');
                        ta.value = text;
                        editorApi = { getValue: function () { return ta.value; } };
                    });
                });
        } else {
            paneView.innerHTML = '<div class="placeholder">Binary file — no preview</div>';
        }
    }

    saveBtn.onclick = function () {
        if (!editorApi || !openedPath) return;
        var path = openedPath;
        var content = editorApi.getValue();
        fetch('/api/assets/file?path=' + encodeURIComponent(path), {
            method: 'PUT',
            body: content,
        }).then(function (r) {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            saveBtn.style.background = '#3B8E6B';
            setTimeout(function () { saveBtn.style.background = ''; }, 800);
            // the running engine reads sources from MEMFS: mirror the
            // change there and rebuild assets so it applies without reload
            try {
                if (Module.FS && Module.FS.analyzePath('/project/Assets/' + path).exists) {
                    Module.FS.writeFile('/project/Assets/' + path, content);
                    if (Module._o2_web_rebuild_assets)
                        setTimeout(function () { Module._o2_web_rebuild_assets(); }, 100);
                }
            } catch (e) { console.warn('engine FS sync failed', e); }
        }).catch(function (e) { modal.alert('Save failed', e.message); });
    };

    // ---------------- toolbar ----------------
    function newFile(dir) {
        modal.prompt('New file', 'File name:', 'new.txt').then(function (name) {
            if (!name) return;
            fetch('/api/assets/file?path=' + encodeURIComponent(joinPath(dir, name)), { method: 'PUT', body: '' })
                .then(function () { return afterChange([dir]); });
        });
    }
    function newFolder(dir) {
        modal.prompt('New folder', 'Folder name:', 'NewFolder').then(function (name) {
            if (name) assetsOp({ op: 'mkdir', path: joinPath(dir, name) })
                .then(function () { return afterChange([dir]); });
        });
    }
    document.getElementById('b-newfile').onclick = function () { newFile(curDir); };
    document.getElementById('b-newfolder').onclick = function () { newFolder(curDir); };
    var bUpload = document.getElementById('b-upload-input');
    document.getElementById('b-uploadfile').onclick = function () { bUpload.click(); };
    bUpload.onchange = function () {
        var files = Array.prototype.slice.call(bUpload.files);
        bUpload.value = '';
        uploadFiles(files, curDir);
    };
    document.getElementById('b-refresh').onclick = function () {
        buildTree().then(function () { return openDir(curDir, true); });
    };

    var backdrop = document.getElementById('browser-back');
    function closeBrowser() {
        hideCtx();
        browser.classList.remove('open');
        backdrop.classList.remove('open');
    }
    backdrop.onclick = closeBrowser;
    window.__o2CloseBrowser = closeBrowser;
    document.getElementById('btn-browser').onclick = function () {
        if (browser.classList.contains('open')) { closeBrowser(); return; }
        browser.classList.add('open');
        backdrop.classList.add('open');
        buildTree().then(function () { return openDir(curDir, true); });
    };
    document.getElementById('b-close').onclick = closeBrowser;

    // hook for the wasm editor: double-clicking a script asset opens it
    // here, in the browser's text editor pane
    function openAssetInBrowser(path, kind) {
        browser.classList.add('open');
        backdrop.classList.add('open');
        return buildTree().then(function () { return openDir(parentOf(path), true); })
            .then(function () {
                previewFile(path, kind || fileKind(baseOf(path)));
                var n = nodes[path];
                if (n) { selectNode(n); n.el.scrollIntoView({ block: 'nearest' }); }
            });
    }
    window.__o2OpenAssetTextEditor = function (path) { openAssetInBrowser(path, 'text'); };
    window.__o2RevealAsset = openAssetInBrowser; // used by AI chat file links
})();
