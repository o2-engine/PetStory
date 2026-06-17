# PetStory — Claude project memory

Shared rules and notes for any Claude session working in this repo. Auto-loaded into context.
Commit changes here; private/host-specific notes stay in `~/.claude/...` user memory, not in this file.

## Editor Actions refactor (long-running initiative)

**Goal:** every mutation of scene/assets/state in the o2 editor must flow through an
[`Editor::IAction`](o2/Editor/Sources/o2Editor/Actions/IAction.h) (Undo/Redo). All actions must be
covered by tests in [`o2EditorTests`](o2/Tests/EditorTests/).

The editor is in a *mixed* state during migration — old direct mutations and new IAction-based code
coexist and both must keep the editor functional. Migrate features one at a time, only when the
owner asks. Do not refactor the rest of the editor speculatively.

**IAction contract:**
- `Redo()` / `Undo()` — final state transitions.
- `Append(other)` — coalesces incremental follow-up actions. Calls `TryMerge(other)`; on success
  also calls `other->Redo()` so the world reflects the step. On failure asserts and skips Redo.
- `TryMerge(other)` — protected, returns `false` by default. Override in derived actions:
  `DynamicCast<T>(other)`, verify same target/scope, update `this` final state from `other`,
  return `true`.
- Net effect of a coalesced drag: history holds ONE action; its `Undo` jumps from final back to
  original `from`, its `Redo` replays the merged span.

## Action owns the mutation

When migrating a feature to `IAction`, the scene/state change must happen INSIDE the action —
typically via `mainAction->Append(step)`, where `Append` calls `step->Redo()` to apply the change.
Construct `step` capturing current state as `before`, set `done = before + delta` purely in memory,
then call `Append`. The runtime mutation path and the Redo path are then literally the same code.

**Wrong:**
```cpp
MoveSelectedObjects(delta);          // direct scene mutation
AppendStep(mTransformAction);        // action just snapshots the post-state
```

**Right:**
```cpp
auto step = mmake<TransformAction>(selected);     // before = current
step->doneTransforms = step->beforeTransforms;
for (auto& t : step->doneTransforms)
    t.transform.origin += delta;
mTransformAction->Append(step);                    // Append → TryMerge → step->Redo() applies
```

If a tool keeps a direct mutation route and only "informs" the action of the result, the runtime
path diverges from the Undo/Redo replay path. Tool-side / UI-side side-effects unrelated to scene
state (handle positions, gizmo cache, focus, dirty flags) stay in the tool, called AFTER `Append`.
Only the scene/asset/state mutation must live inside the action.

## Picking the right action for a mutation

- WidgetLayer layout/size → [`WidgetLayerLayoutAction`](o2/Editor/Sources/o2Editor/Actions/WidgetLayerLayout.h),
  not `TransformAction` (its `SetTransform` corrupts a layer's offsets via a stale absolute position).
- `PropertyChangeAction` snapshots leaf paths (`"layout/offsetMax"`), not whole serializable members —
  delta-serialization drops default-valued fields, leaving an empty before/after.

## Always build and run tests after changes

After every code change in this repo:

1. **Build** the affected target: `cmake --build --preset windows --target <Target> -j 8`
   (use `mac` / `linux` preset on those hosts). If it doesn't compile, fix before reporting.
2. **Run tests:** `ctest --test-dir build --output-on-failure -C Debug --parallel 4`. Pick the
   right binary for what you changed:
   - `o2UtilTests` — pure value-types (Math, Vec2F, Color, Pool, Map, Vector, Curve…). No
     `Application`, no asset tree load. ~70 ms cold start.
   - `o2SystemTests` — Scene / Actor / Animation / Assets-metadata / Events / Input / Scripting.
     Headless `Application::Initialize` (no window, no render). ~135 ms cold start.
   - `o2RenderTests` — Sprite / Camera / Material / Mesh and all UI widgets (Label, DropDown,
     Button, Window, …). Full `Application::Initialize` with window + GL. ~640 ms cold start.
   - `o2EditorTests` — editor-only, IAction tests etc.
3. **Only after both pass** report the change as done.

"Should compile, tests not affected" is not acceptable — only an actual green build + green test
run is. If a build/test step is multi-minute, run it in the background and report when done. If a
new code path lacks a test, add one in the same change or flag the gap explicitly — do not silently
leave it untested.

Verify editor tests through **`ctest`** (each test runs in its own process via
`gtest_discover_tests`), not by running the `o2EditorTests` binary directly across the whole suite:
there is a pre-existing order-dependent segfault when several `LayerActionsNotification.*` tests share
one process (they pass run individually). A `139` from the raw binary across the suite is that known
issue, not your change.

### Test categorization

When adding a new test, drop the .cpp file into the right tier:

- pure value-type / algorithmic — `o2/Tests/Sources/Util/`
- needs scene/asset/event/scripting subsystems but never draws — `o2/Tests/Sources/Systems/`
- needs Render / Window / UI styles — `o2/Tests/Sources/Rendered/`

Shared helpers (`TestComponent`, `SceneTestHelpers`, `UITestHelpers`, `TestScriptObject`) live in
`o2/Tests/Sources/Support/` and are linked into every binary that needs them via the
`o2TestsSupport` static lib.

If only a couple of tests in a Systems-tier file depend on rendering, split them out into a new
`.cpp` in `Sources/Rendered/` (small dedicated file is fine). Don't add a runtime
`if (IsHeadless()) GTEST_SKIP()` — the binary split exists exactly so each test runs at the right
init level. Skipping at runtime hides which tier a test belongs to and silently drops coverage.

## Test scaffolding lives in `EditorTestScene.h`

Common test helpers — `SceneCleanGuard`, `TickScene`, `MakeActor`, `SetActorPos`, `AsEditable`,
`NearV` — live in [`o2/Tests/EditorTests/Sources/support/EditorTestScene.h`](o2/Tests/EditorTests/Sources/support/EditorTestScene.h)
under `namespace Editor::Tests`. Tests bring it in with `using namespace Editor::Tests;`. Extend
this header instead of re-declaring the same helpers per test file.

Pitfall when adding helpers: `o2Scene`, `mmake` are **macros** — write them without `o2::` prefix.
Real types (`Actor`, `Vec2F`, `Ref<>`, `Vector<>`, `DynamicCast<>`) live in `namespace o2` and need
`o2::` qualification (the support header intentionally avoids `using namespace o2;`). Otherwise
MSVC errors with `'o2': symbol to the left of '::' must be a type`.

## Comments

Default — no comment. Don't add multi-line explanatory comments, especially essays describing the
*rationale* of a fix, history of the problem, or ABI details right above the code. Reasoning goes
into the PR description / chat, not the source.

A short single-line comment is OK only when "why" is genuinely non-obvious — a hidden invariant, a
workaround with a bug ID, surprising behavior. No `// because of MSVC ABI` / `// to avoid ICE` /
`// added for X flow` in code — those belong in the PR.

Applies to test files too. Older files with rationale/REPRO blocks are legacy, not a precedent to
copy — keep new tests to a one-line header at most.

## Version control

By default Claude does not run `git commit`, `git push`, `git add`, `gh pr create` etc. Make code
changes via Edit/Write and stop at "files modified" — the contributor reviews the diff and commits
themselves. If a session explicitly authorizes git operations, follow that authorization for that
session only; never extend it to future sessions.

## ScriptValueImpl / MSVC PMF — ongoing platform note

MSVC's PMF dispatch is broken for multi-path virtual-inheritance classes
(`Actor → SceneEditableObject + ISceneDrawable → virtual ISerializable`) on the JerryScript binding
path. Same code works on Mac/iOS/Android/WASM.

**Current approach:** bypass MSVC PMF dispatch on the JS path entirely.
- `SIGNATURE` macro in [Type.h](o2/Framework/Sources/o2/Utils/Reflection/Type.h) emits a stateless
  generic lambda `[](thisclass* _obj, auto... _args) { return _obj->NAME(_args...); }` alongside
  the PMF. PMF stays as NTTP for reflection; the lambda is the runtime call path for JS.
- `ScriptPrototypeProcessor::FunctionProcessor::Signature<pointer>(..., _callable)` in
  [ScriptValueImpl.h](o2/Framework/Sources/o2/Scripts/JerryScript/ScriptValueImpl.h) uses
  `ThunkMethodHandler<Callable, PMF>::Handle` as the jerry callback. The handler
  default-constructs the stateless lambda (C++20) and calls `callable(obj, args...)` — direct
  `obj->NAME(args...)`, no PMF runtime dispatch.
- 4-arg NTTP `Signature<pointer>(..., _callable)` overloads added in
  [Reflection.h](o2/Framework/Sources/o2/Utils/Reflection/Reflection.h) and
  [BaseTypeProcessor.h](o2/Framework/Sources/o2/Utils/Reflection/BaseTypeProcessor.h); reflection
  ignores the lambda.
- No `#ifdef _MSC_VER` in the fix — the thunk approach is portable.

Stateless-lambda default-construct relies on C++20 (`CMAKE_CXX_STANDARD 20`). When iterating on
this area: if a change causes a *new* ICE, revert to the last version that compiled before
iterating further; keep `Ref<T>` / multi-inheritance / 16-byte MFP / `mmake` alignment
(sizeof(RefCounter)=12) in mind as dangerous interactions.
