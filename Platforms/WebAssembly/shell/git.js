// Publishing the session's changes: the commit half of the Changes window.
//
// The session has no git of its own (the server keeps a work clone for that,
// see o2editor/src/git.mjs); this only asks. The model that writes the message
// also picks which of the changes belong in it, and what comes back is a
// proposal — branch, message, ticked files — that the person edits before
// anything is pushed. The push itself goes out on the installation's account
// to a branch of its own, never to the base branch.
//
// The whole UI is built here, so changing it needs no wasm relink. It appears
// only where email sign-in is on and the instance has a repository.

(function () {
    var dlg = document.getElementById('changes');
    if (!dlg) return;

    var info = null;
    var proposal = null;
    var busy = false;
    var lastNote = '';   // Back returns to the note the person typed, not a blank one

    // ---- chrome -------------------------------------------------------
    var foot = document.createElement('div');
    foot.id = 'changes-foot';
    foot.style.display = 'none';
    var where = document.createElement('span');
    where.className = 'cf-where';
    var grow = document.createElement('span');
    grow.className = 'grow';
    var note = document.createElement('span');
    note.className = 'cf-note';
    var commitBtn = document.createElement('button');
    commitBtn.className = 'cf-btn';
    commitBtn.innerHTML = '<svg class="icon" viewBox="0 0 16 16"><use href="#i-branch"/></svg><span>Commit & push…</span>';
    foot.appendChild(where);
    foot.appendChild(grow);
    foot.appendChild(note);
    foot.appendChild(commitBtn);
    dlg.appendChild(foot);

    var pane = document.createElement('div');
    pane.id = 'ch-commit';
    dlg.appendChild(pane);

    function esc(s) {
        return String(s == null ? '' : s).replace(/[&<>"]/g, function (c) {
            return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
        });
    }

    function api(what, body) {
        return fetch(o2Base + '/api/git/' + what, {
            method: body ? 'POST' : 'GET',
            headers: body ? { 'Content-Type': 'application/json' } : {},
            body: body ? JSON.stringify(body) : undefined,
        }).then(function (r) {
            return r.json().catch(function () { return {}; }).then(function (j) {
                if (!r.ok) throw new Error(j.error || ('HTTP ' + r.status));
                return j;
            });
        });
    }

    // ---- footer state -------------------------------------------------
    function refresh() {
        api('info').then(function (j) {
            info = j;
            if (!j.repo) { foot.style.display = 'none'; return; }
            foot.style.display = 'flex';
            where.textContent = j.repo + ' · base ' + j.branch + (j.base ? ' @' + j.base : '');
            note.textContent = j.enabled ? '' : (j.reason || '');
            commitBtn.style.display = j.enabled ? '' : 'none';
        }).catch(function () { foot.style.display = 'none'; });
    }

    var openDialog = window.openChangesDialog;
    window.openChangesDialog = function () {
        closePane();
        if (openDialog) openDialog.apply(null, arguments);
        refresh();
    };

    // ---- the commit pane ----------------------------------------------
    function closePane() { pane.classList.remove('open'); pane.innerHTML = ''; proposal = null; }

    commitBtn.onclick = function () { openPane(); };

    function header(title) {
        return '<div class="cc-head"><svg class="icon" viewBox="0 0 16 16"><use href="#i-branch"/></svg>' +
               '<span>' + esc(title) + '</span><span class="grow"></span>' +
               '<button class="cc-x" id="cc-close">✕</button></div>';
    }

    function openPane() {
        pane.classList.add('open');
        pane.innerHTML = header('Commit to ' + (info ? info.repo : 'the repository')) +
            '<div class="cc-body">' +
              '<p class="cc-hint">The model reads the diff, picks the changes worth committing and writes the ' +
              'message. Nothing is pushed until you look at what it chose.</p>' +
              '<label class="cc-label" for="cc-note">What were you working on? <span class="muted">(optional — it helps the message)</span></label>' +
              '<textarea id="cc-note" rows="3" placeholder="e.g. new enemy animations and the level 3 layout">' +
              esc(lastNote) + '</textarea>' +
              '<div class="cc-row"><button class="cf-btn" id="cc-prepare">Prepare commit</button>' +
              '<span class="cc-status" id="cc-status"></span></div>' +
            '</div>';
        document.getElementById('cc-close').onclick = closePane;
        document.getElementById('cc-prepare').onclick = prepare;
        document.getElementById('cc-note').focus();
    }

    function status(text, kind) {
        var el = document.getElementById('cc-status');
        if (!el) return;
        el.textContent = text || '';
        el.className = 'cc-status' + (kind ? ' ' + kind : '');
    }

    function credentials() {
        // the same key the agent panel runs on; the server falls back to the
        // cookies it is kept in when the panel is not loaded
        try { return window.o2AiCredentials ? window.o2AiCredentials() : {}; } catch (e) { return {}; }
    }

    function model() {
        try { return window.o2AiModel ? window.o2AiModel() : ''; } catch (e) { return ''; }
    }

    function prepare() {
        if (busy) return;
        busy = true;
        var btn = document.getElementById('cc-prepare');
        if (btn) btn.disabled = true;
        status('asking ' + (model() || 'the model') + '… this takes a few seconds', 'work');
        var c = credentials();
        lastNote = (document.getElementById('cc-note') || {}).value || '';
        api('propose', { note: lastNote,
                         model: model(), apiKey: c.apiKey, oauthToken: c.oauthToken, workspaceId: c.workspaceId })
            .then(function (j) { busy = false; proposal = j; renderProposal(j); })
            .catch(function (e) {
                busy = false;
                if (btn) btn.disabled = false;
                status(e.message, 'err');
            });
    }

    function sizeText(f) {
        if (f.status === 'A') return '+' + kb(f.after);
        if (f.status === 'D') return '−' + kb(f.before);
        var d = f.after - f.before;
        return kb(f.after) + (d ? ' (' + (d > 0 ? '+' : '−') + kb(Math.abs(d)) + ')' : '');
    }
    function kb(n) { return n >= 1024 ? Math.round(n / 1024) + ' KB' : n + ' B'; }

    function renderProposal(j) {
        var rows = j.files.map(function (f, i) {
            return '<label class="cc-file' + (f.include ? '' : ' off') + '">' +
                   '<input type="checkbox" data-i="' + i + '"' + (f.include ? ' checked' : '') + '>' +
                   '<span class="badge ' + f.status + '">' + f.status + '</span>' +
                   '<span class="cc-path" title="' + esc(f.path) + '">' + esc(f.path) + '</span>' +
                   '<span class="cc-size">' + esc(sizeText(f)) + '</span>' +
                   '<span class="cc-why" title="' + esc(f.reason) + '">' + esc(f.reason) + '</span></label>';
        }).join('');

        pane.innerHTML = header('Commit to ' + (info ? info.repo : 'the repository')) +
            '<div class="cc-body">' +
              '<div class="cc-grid">' +
                '<label class="cc-label" for="cc-branch">Branch</label>' +
                '<input id="cc-branch" spellcheck="false" value="' + esc(j.branch) + '">' +
                '<label class="cc-label" for="cc-subject">Subject</label>' +
                '<input id="cc-subject" maxlength="72" value="' + esc(j.subject) + '">' +
              '</div>' +
              '<label class="cc-label" for="cc-msg">Message</label>' +
              '<textarea id="cc-msg" rows="6">' + esc(j.body) + '</textarea>' +
              '<div class="cc-filehead"><span id="cc-count"></span>' +
                '<span class="grow"></span>' +
                '<button class="cc-link" id="cc-all">all</button>' +
                '<button class="cc-link" id="cc-none">none</button>' +
                '<button class="cc-link" id="cc-model">model\'s pick</button></div>' +
              '<div class="cc-files">' + rows + '</div>' +
              '<div class="cc-warn" id="cc-warn" style="display:none"></div>' +
              '<div class="cc-row">' +
                '<button class="cf-btn" id="cc-push">Commit & push</button>' +
                '<button class="cf-btn ghost" id="cc-back">Back</button>' +
                '<span class="cc-status" id="cc-status">written by ' + esc(j.model) + ' · pushed as ' + esc(info && info.email) + '</span>' +
              '</div>' +
            '</div>';

        document.getElementById('cc-close').onclick = closePane;
        document.getElementById('cc-back').onclick = openPane;
        document.getElementById('cc-push').onclick = push;
        var boxes = pane.querySelectorAll('.cc-files input');
        var ticked = {};

        // An asset and its .meta are one thing: the uid lives in the .meta and
        // a commit that splits them breaks every reference silently
        function orphans() {
            var out = [];
            j.files.forEach(function (f, i) {
                var other = /\.meta$/.test(f.path) ? f.path.replace(/\.meta$/, '') : f.path + '.meta';
                if (!(other in ticked)) return;
                if (ticked[f.path] && !ticked[other]) out.push(f.path + ' without ' + other.split('/').pop());
            });
            return out;
        }

        function count() {
            var n = 0;
            boxes.forEach(function (b) {
                b.parentNode.classList.toggle('off', !b.checked);
                ticked[j.files[+b.dataset.i].path] = b.checked;
                if (b.checked) n++;
            });
            document.getElementById('cc-count').textContent = n + ' of ' + j.files.length + ' files';
            var go = document.getElementById('cc-push');
            if (go) go.disabled = !n;
            var warn = document.getElementById('cc-warn');
            var bad = orphans();
            warn.style.display = bad.length ? '' : 'none';
            warn.textContent = bad.length
                ? 'An asset and its .meta belong together — references break otherwise: ' + bad.join(', ')
                : '';
        }
        boxes.forEach(function (b) { b.onchange = count; });
        document.getElementById('cc-all').onclick = function () { boxes.forEach(function (b) { b.checked = true; }); count(); };
        document.getElementById('cc-none').onclick = function () { boxes.forEach(function (b) { b.checked = false; }); count(); };
        document.getElementById('cc-model').onclick = function () {
            boxes.forEach(function (b) { b.checked = j.files[+b.dataset.i].include; });
            count();
        };
        count();
    }

    function push() {
        if (busy || !proposal) return;
        var paths = [];
        pane.querySelectorAll('.cc-files input').forEach(function (b) {
            if (b.checked) paths.push(proposal.files[+b.dataset.i].path);
        });
        if (!paths.length) return;
        busy = true;
        document.getElementById('cc-push').disabled = true;
        document.getElementById('cc-back').disabled = true;
        // the first publish of a container clones the repository, which is the
        // slow part; saying so beats a button that looks stuck
        status('committing and pushing ' + paths.length + ' files… (the first one also clones the repo)', 'work');
        api('commit', {
            branch: (document.getElementById('cc-branch') || {}).value || proposal.branch,
            subject: (document.getElementById('cc-subject') || {}).value || proposal.subject,
            bodyText: (document.getElementById('cc-msg') || {}).value || '',
            paths: paths, model: proposal.model,
        }).then(function (j) { busy = false; renderResult(j); })
          .catch(function (e) {
              busy = false;
              document.getElementById('cc-push').disabled = false;
              document.getElementById('cc-back').disabled = false;
              status(e.message, 'err');
          });
    }

    function renderResult(j) {
        pane.innerHTML = header('Pushed') +
            '<div class="cc-body">' +
              '<div class="cc-done">✓ ' + j.files.length + ' files committed as ' + esc(j.commit) + '</div>' +
              '<p class="cc-hint">Branch <b>' + esc(j.branch) + '</b> in ' + esc(j.repo) +
              ', from base ' + esc(j.base) + '. Your session keeps its changes — this only copied them out.</p>' +
              ((j.ignored && j.ignored.length)
                  ? '<p class="cc-hint">Left out, the repository ignores them: ' +
                    j.ignored.map(esc).join(', ') + '</p>' : '') +
              (j.url ? '<p><a class="cc-open" href="' + esc(j.url) + '" target="_blank" rel="noopener">Open a pull request ↗</a></p>' : '') +
              '<div class="cc-row"><button class="cf-btn" id="cc-done">Done</button></div>' +
            '</div>';
        document.getElementById('cc-close').onclick = closePane;
        document.getElementById('cc-done').onclick = closePane;
    }
})();
