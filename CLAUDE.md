# PetStory — Claude project memory

Shared rules for any Claude session in this repo. Auto-loaded. Commit changes here; private/host
notes stay in `~/.claude/...`, not in this file.

## Editor Actions refactor (ongoing)

Every mutation of scene/assets/state in the o2 editor must flow through an `Editor::IAction`
(undo/redo), with the mutation owned by the action (runtime path == Redo path) and covered by tests.
The editor is mid-migration — old direct mutations and IAction code coexist. Migrate features one at
a time, only when the owner asks; don't refactor the rest speculatively.

## Build & test after every change

1. Build the affected target: `cmake --build --preset mac --target <Target> -j 8` (`windows` /
   `linux` on those hosts). Fix compile errors before reporting.
2. Run `ctest --test-dir build --output-on-failure -C Debug --parallel 4`. Pick the target you
   touched: `o2UtilTests` (value types), `o2SystemTests` (scene/assets, headless), `o2RenderTests`
   (render / UI widgets), `o2EditorTests` (editor, headless), `o2EditorUITests` (non-headless — for
   tests that build real property/viewer widgets, which headless can't), `GameTests` /
   `GameUITests` (game code).
3. Report done only on a green build + green run — never "should compile, tests unaffected".

For a reported bug, work test-first: write a failing test that reproduces it, fix until it passes,
then run the rest of the suite. Add a test for any new code path, or flag the gap.

### Batch test mode (default)

CTest registers one entry per gtest *suite*; the suite runs whole in one process
(`--gtest_filter=Suite.*`), so Application/render init is paid per suite, not per test case
(~271 processes instead of ~1956; full run ~38 s vs ~206 s on Mac Debug). Entries are named
`<Target>/<Suite>`: `ctest -R '^o2EditorTests/'` runs one binary, `ctest -R '/Actor$'` one suite.
Implemented in `o2/CMake/O2TestSuites.cmake` + `O2TestSuitesDiscovery.cmake` (discovery at ctest
startup); call sites use `o2_gtest_discover_tests(...)`. Configure with `-DO2_TESTS_BATCH=OFF` for
the classic one-process-per-test-case registration (names `Suite.Case`) when hunting cross-test
pollution. Consequence: tests of one suite share a process — clean up global state (scene,
subscriptions to `o2Scene`/global signals) via guards/destructors.

Run editor tests via `ctest`, not the raw binary: a whole-binary `o2EditorUITests` run still has an
order-dependent segfault (`CreateRemoveActionUndoUI.CreateUndoThroughComponentContext` crashes on a
dangling `dynamic_cast` when a foreign suite ran before its own two earlier tests; green as a lone
suite, which is what batch ctest runs). New tests go in the matching tier; shared helpers live in
`EditorTestScene.h` (`namespace Editor::Tests`) and `o2/Tests/Sources/Support/`.

## Image generation tools (MCP `imagegen`)

For generating game sprites/assets use the MCP server `imagegen` (registered in `.mcp.json`,
implemented in `o2/Tools/ImageGen/`, model Gemini Nano Banana 2). Tools:

- `generate_image(prompt, out_path, aspect?, size?, ref_paths?)` — text-to-image; pass style
  references via `ref_paths` (upscale tiny references smoothly first — a NEAREST-upscaled or
  pixelated reference makes the model copy the pixelation).
- `edit_image(image_path, prompt, out_path, ref_paths?)` — targeted edit, preserves the rest.
- `generate_transparent_image(prompt, out_path, ...)` — RGBA sprite via white/black double
  render + alpha recovery. Does not work for near-white subjects on white (e.g. light UI icons) —
  for flat icons generate on pure white and key the background out with a border flood-fill instead.
- `extract_region(image_path, rect=[x,y,w,h], out_path, transparent?)` — crop a sprite out of a
  sheet; `transparent` re-renders the subject (resolution may change).

Prompts should be in English. Outputs are PNG; results return a preview. CLI equivalents and
details: `o2/Tools/ImageGen/README.md`. API key: `o2/Tools/ImageGen/api_key.txt` (gitignored)
or `GEMINI_API_KEY`.

## PSD tools (MCP `psd`)

For working with PSD mockups use the MCP server `psd` (registered in `.mcp.json`, implemented in
`o2/Tools/PsdTool/`, needs `pip install psd-tools`). Tools:

- `psd_structure(psd_path, include_hidden?)` — layer tree with kinds, bboxes, opacity; layers
  listed bottom-to-top (draw order).
- `psd_render(psd_path, out_path, scale?)` — composite to PNG, returns a preview for viewing.
- `psd_extract_layers(psd_path, out_dir, layers?, include_hidden?)` — per-layer RGBA PNGs;
  filter by name or slash path (`Panel/Buttons/PlayBtn`).
- `psd_layer_positions(psd_path)` — flat placement list incl. o2 world positions (canvas-center
  origin, y up).
- `psd_to_o2_prefab(psd_path, out_dir, atlas?, scale?, ...)` — builds an o2 `.proto` prefab
  replicating the PSD: groups → container actors, layers → actors with ImageComponent; hierarchy,
  order, positions and opacity preserved. Layer images + `.meta` go to `Assets/<out_dir>/`.

Details and coordinate mapping: `o2/Tools/PsdTool/README.md`. The demo import
`Assets/PsdImport/UiMock/` is validated by the `PsdImportUI` suite in GameUITests.

## Code style

No inline method implementations in game headers (`Sources/`): declare in the
header, implement in the cpp — including trivial getters and empty virtual
hooks. Header-only is fine only for constants (`static constexpr`).

## Image diff (MCP `imagediff`)

For pixel-comparing two screenshots (e.g. a UI screenshot against the PSD
mockup reference) use the MCP server `imagediff` (registered in `.mcp.json`,
implemented in `o2/Tools/ImageDiff/`): tool
`image_diff(a_path, b_path, out_path?, threshold?, region?)` returns changed
pixel stats and a difference map — the reference dimmed to grayscale with
differing pixels highlighted in red. CLI equivalent:
`python3 o2/Tools/ImageDiff/image_diff.py`. Verify visual changes (animations,
layout fixes) by pixels, not only by property values.

## Comments

Default: no comment. No multi-line rationale/history/ABI essays above code — that goes in the PR. A
short single-line comment only when "why" is non-obvious (hidden invariant, workaround). Same for
tests: a one-line header at most.

Never write a comment that restates the code. If it mirrors the line (`value = nullptr; // clear it`),
delete it. Even a "why" comment must not narrate what the code does.

## Communication

Write the final summary at the end of work in Russian, concise. Record any persistent guidance the
contributor asks me to remember here in this file, not in host/private memory.

## Version control

Don't run `git commit` / `push` / `add` / `gh pr create` etc. by default — make changes and stop at
"files modified"; the contributor reviews and commits. Git authorization is per-session only, never
carried to future sessions.

## Web editor and its agent

The editor also runs in the browser (`cmake --preset wasm-editor`, served by the o2editor backend),
and Claude Code can work on a project there — either this local Claude Code driving a locally running
web editor, or the agent built into the page, which is Claude Code running on the server over a
private copy of this repository. Both see the editor through the MCP server `o2` / `o2editor`
(registered in `.mcp.json`): `screenshot`, `scene_tree`, `view_info`, `run_script`, `open_scene`,
`save_scene`, `play_mode`, `rebuild_assets`, `read_log`, `click` / `type_text` / `press_key` (play mode
only), `wait`.

Rules that hold there:

- Only `Assets/` is writable on the server; the rest of the repository is read-only reference and
  nothing compiles — deliver through assets, scripts and the editor. Locally the usual rules apply.
- The editor reads asset content from the built copy: after changing files under `Assets/` call
  `rebuild_assets` once per batch (10-20 s freeze), then check `read_log` for build errors and
  script exceptions before looking at a screenshot.
- Read the scene with `scene_tree` / `view_info`, never by guessing from a picture; open the scene
  you changed with `open_scene` before `play_mode`. The editor chrome ignores synthetic input —
  `click` / `type_text` / `press_key` exist only to play the game in the Game window.
- `run_script` executes JavaScript inside the engine (globals `sceneRoots`, `findActor(path)`,
  `eachActor(fn)`, the `o2` namespace) without a script asset or play mode; it is not a text
  processor and has no Node/require/filesystem.
- Scripts: a `.js` asset defines a class named like the file, assigned to the global without
  let/const (`Name = class Name extends o2.Component { ... }`); lifecycle hooks are exactly
  `OnStart()`, `OnEnabled()`, `OnDisabled()`, `Update(dt)`; log with `print()`; never `Dump()` or
  enumerate the JS global under browserjs. A window script reports to C++ through the injected
  property named exactly `action`.
- Never change an asset uid, an actor Id or a PrototypeLink number, and move a `.meta` together with
  its file — references are by uid and break silently.
- Particles: an emitter needs `"mPlaying": true` (and `"mLoop": "Repeat"` for a loop); `Play()` is
  not scriptable; the particle image must be part of the built assets.
