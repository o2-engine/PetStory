#!/usr/bin/env node
// stdio MCP server: lets a local Claude Code drive the web editor open in a
// browser tab, with the same tools the server-side agent has (screenshot,
// scene_tree, run_script, play mode, asset rebuild...). Zero dependencies.
//
// It talks to a running o2editor backend (O2EDITOR_URL, default
// http://localhost:8091) and targets the tab that talked to it last, or the
// session named in O2SID. Registered in .mcp.json as `o2editor`.

import http from 'node:http';
import https from 'node:https';

const BASE = (process.env.O2EDITOR_URL || 'http://localhost:8091').replace(/\/$/, '');
const SID = process.env.O2SID || '';
const TOKEN = process.env.O2EDITOR_TOKEN || '';   // the AUTH_TOKEN of a gated instance

const num = { type: 'number' }, str = { type: 'string' }, bool = { type: 'boolean' };
const TOOLS = [
    { name: 'screenshot', description: 'Screenshot of the editor canvas at half size (image + size info).', input: {} },
    { name: 'scene_tree', description: 'Hierarchy of the open scene: names, paths, types, transforms, widget layouts, components, children. Reports which scene is open.',
      input: { path: { ...str, description: 'start at this actor path' }, depth: { ...num, description: 'levels of children, default 3' } } },
    { name: 'view_info', description: 'Canvas size, play state, the Game window rectangle, the camera and the world-to-pixel formula.', input: {} },
    { name: 'run_script', description: 'Run JavaScript inside the running engine; the value of the last expression comes back as text. Globals: sceneRoots, findActor(path), eachActor(fn), the o2 namespace.',
      input: { code: { ...str, description: 'JavaScript source' } }, required: ['code'] },
    { name: 'open_scene', description: 'Open a scene by its path under Assets, e.g. Main.scn.', input: { path: str }, required: ['path'] },
    { name: 'save_scene', description: 'Save the scene open in the editor.', input: {} },
    { name: 'play_mode', description: 'Start (on=true) or stop (on=false) the editor play mode.', input: { on: bool } },
    { name: 'rebuild_assets', description: 'Rebuild the project assets inside the editor so file changes under Assets take effect (10-20 s). force=true rebuilds from scratch.', input: { force: bool } },
    { name: 'read_log', description: 'The engine log: asset build errors, script exceptions, prints.', input: { lines: num, filter: str } },
    { name: 'click', description: 'Mouse click in the Game window (play mode only), full-size canvas pixels.', input: { x: num, y: num, button: str, double: bool }, required: ['x', 'y'] },
    { name: 'type_text', description: 'Type text into the running game (play mode only).', input: { text: str }, required: ['text'] },
    { name: 'press_key', description: 'Press a key in the running game (play mode only), e.g. Enter, Escape, ArrowLeft, Ctrl+Z.', input: { key: str }, required: ['key'] },
    { name: 'wait', description: 'Wait for the editor to settle, max 5000 ms.', input: { ms: num }, required: ['ms'] },
];

function request(method, path, body) {
    return new Promise((resolve, reject) => {
        const u = new URL(BASE + path);
        const lib = u.protocol === 'https:' ? https : http;
        const data = body ? JSON.stringify(body) : null;
        const headers = { 'Content-Type': 'application/json' };
        if (TOKEN) headers.Cookie = 'o2token=' + TOKEN;
        const req = lib.request(u, { method, headers }, res => {
            let out = '';
            res.on('data', c => out += c);
            res.on('end', () => {
                try { resolve({ status: res.statusCode, json: JSON.parse(out) }); }
                catch { resolve({ status: res.statusCode, json: { error: out.slice(0, 300) } }); }
            });
        });
        req.on('error', reject);
        req.setTimeout(200000, () => req.destroy(new Error('timeout')));
        if (data) req.write(data);
        req.end();
    });
}

async function callTool(name, args) {
    const r = await request('POST', '/api/agent/browser_tool', { sid: SID || undefined, name, args: args || {} });
    if (r.status !== 200) throw new Error(r.json.error || ('HTTP ' + r.status));
    const out = r.json.result;
    const content = [];
    if (out && out.image) {
        content.push({ type: 'image', data: out.image.b64, mimeType: out.image.mime });
        content.push({ type: 'text', text: JSON.stringify(out.result || {}) });
    } else {
        content.push({ type: 'text', text: typeof out === 'string' ? out : JSON.stringify(out) });
    }
    return { content, isError: !!(out && out.error) };
}

// ---- JSON-RPC over stdio (MCP 2024-11-05)
let buf = '';
process.stdin.setEncoding('utf8');
process.stdin.on('data', chunk => {
    buf += chunk;
    let nl;
    while ((nl = buf.indexOf('\n')) >= 0) {
        const line = buf.slice(0, nl).trim();
        buf = buf.slice(nl + 1);
        if (line) handle(line);
    }
});

function reply(id, result, error) {
    const msg = error ? { jsonrpc: '2.0', id, error } : { jsonrpc: '2.0', id, result };
    process.stdout.write(JSON.stringify(msg) + '\n');
}

async function handle(line) {
    let m;
    try { m = JSON.parse(line); } catch { return; }
    if (m.id === undefined) return;   // notifications need no answer
    try {
        switch (m.method) {
            case 'initialize':
                reply(m.id, { protocolVersion: '2024-11-05', capabilities: { tools: {} },
                              serverInfo: { name: 'o2editor', version: '1.0.0' },
                              instructions: 'Tools act on the o2 web editor open in a browser tab served by ' + BASE + '.' });
                break;
            case 'tools/list':
                reply(m.id, { tools: TOOLS.map(t => ({ name: t.name, description: t.description,
                                                       inputSchema: { type: 'object', properties: t.input, required: t.required || [] } })) });
                break;
            case 'tools/call':
                reply(m.id, await callTool(m.params.name, m.params.arguments));
                break;
            case 'ping':
                reply(m.id, {});
                break;
            default:
                reply(m.id, null, { code: -32601, message: 'unknown method ' + m.method });
        }
    } catch (e) {
        reply(m.id, { content: [{ type: 'text', text: 'error: ' + e.message }], isError: true });
    }
}
