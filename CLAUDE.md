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
   tests that build real property/viewer widgets, which headless can't).
3. Report done only on a green build + green run — never "should compile, tests unaffected".

For a reported bug, work test-first: write a failing test that reproduces it, fix until it passes,
then run the rest of the suite. Add a test for any new code path, or flag the gap.

Run editor tests via `ctest`, not the raw binary (known order-dependent segfault when some
`LayerActionsNotification.*` tests share one process). New tests go in the matching tier; shared
helpers live in `EditorTestScene.h` (`namespace Editor::Tests`) and `o2/Tests/Sources/Support/`.

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
