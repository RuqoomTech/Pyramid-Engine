---
phase: 01-p0-core-freeze
plan: 03
subsystem: graphics
tags: [opengl, shadows, shadow-map-array, cascades, deferred-lighting, sampler2DArray]

requires: []
provides:
  - Array-owning ShadowMapPass with one GL_TEXTURE_2D_ARRAY depth texture (FRZ-02)
  - Single array bind in DeferredLightingPass with light-space-matrix, split, and count uploads (FRZ-02)
  - Tests/ShadowArrayTests.cpp pinning completeness, layer count, single bind, uploads, recreation, empty skip, restore (FRZ-02)
  - Forward-pipeline Q4/A2 scoping rationale: forward renders unshadowed by design (FRZ-02)
affects: [01-04, deferred-lighting, shadow-rendering, framebuffer-restore-convention]

actuals:
  tokens: 12900
  tasks: 3
  commits: 2

tech-stack:
  added: []
  patterns: [single-array-bind-with-full-uploads, transactional-texture-recreation, fake-gl-capture-harness-shadow]

key-files:
  created:
    - Tests/ShadowArrayTests.cpp
  modified:
    - Engine/Graphics/source/Renderer/ShadowMapPass.cpp
    - Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp
    - Engine/Graphics/source/Renderer/DeferredLightingPass.cpp
    - Engine/Graphics/source/Renderer/RenderSystem.cpp
    - Tests/TestGraphicsDevice.hpp
    - Tests/CMakeLists.txt

key-decisions:
  - "Forward pipeline renders unshadowed by design (RESEARCH Q4/A2): ForwardRenderPass::Execute/End carries no shadow binds and forward.frag's array sampler is guarded by u_CascadeCount defaulting 0 — no shared-helper wiring, no new shadow technique; pinned by a test asserting forward Execute binds nothing"
  - "Per-cascade layers populated by rendering each cascade directly into its layer via glFramebufferTextureLayer with one shared FBO; the chosen method is recorded in code comments"
  - "Cascade counts above the 4-cascade shader bound fail explicitly at creation (was silent UB); counts above zero and within GL_MAX_ARRAY_TEXTURE_LAYERS validated, empty sets skip the bind with lighting continuing unshadowed"
  - "After-pass framebuffer plus viewport restore lines preserved in every touched pass without migrating them to the neutral interface — that migration belongs to plan 01-04"
  - "No shader, docs, or CHANGELOG edits in this plan — behavior notes consolidate through plan 01-04 (Wave-1 docs owner)"

patterns-established:
  - "Shadow array shape: one GL_TEXTURE_2D_ARRAY depth texture (DEPTH_COMPONENT24, NEAREST, CLAMP_TO_BORDER white) sized 2048x2048xcascade-count, single BindNativeTexture on unit 5 with target GL_TEXTURE_2D_ARRAY, u_ShadowMaps name and slot kept"
  - "Transactional recreation: build a replacement texture and swap only on success; SetCascadeCount/SetShadowMapResolution commit config only when recreation succeeds, preserving the previous array on failure"

requirements-completed: [FRZ-02]

coverage:
  - id: D1
    description: "ShadowMapPass owns one GL_TEXTURE_2D_ARRAY depth texture with layer count equal to cascade count; all cascades render into array layers"
    requirement: "FRZ-02"
    verification:
      - kind: unit
        ref: "Tests/ShadowArrayTests.cpp array-completeness and layer-count-equals-cascade-count fixtures (ctest --preset test-gcc-debug 58/58 pass incl. Graphics.ShadowArray)"
        status: pass
    human_judgment: false
  - id: D2
    description: "DeferredLightingPass binds the array exactly once with matching sampler type and uploads light-space matrices, cascade splits (count-plus-one), and cascade count"
    requirement: "FRZ-02"
    verification:
      - kind: unit
        ref: "Tests/ShadowArrayTests.cpp single-bind exact-once target assertion plus matrix/count/splits upload-presence fixtures"
        status: pass
    human_judgment: false
  - id: D3
    description: "Empty shadow-map sets skip the bind with lighting continuing unshadowed; zero/over-limit/over-shader-bound counts fail explicitly, never a bind of an invalid handle"
    requirement: "FRZ-02"
    verification:
      - kind: unit
        ref: "Tests/ShadowArrayTests.cpp empty-set skip, over-limit, and failure-preservation fixtures"
        status: pass
    human_judgment: false
  - id: D4
    description: "After-pass framebuffer plus viewport restore convention preserved by every touched pass; cascade layer order matches matrix and split order deterministically"
    requirement: "FRZ-02"
    verification:
      - kind: unit
        ref: "Tests/ShadowArrayTests.cpp restoration assertions and deterministic layer-order fixture"
        status: pass
    human_judgment: false
  - id: D5
    description: "Forward pipeline carries no shadow binds and renders unshadowed by design (Q4/A2 scoping), pinned by test"
    requirement: "FRZ-02"
    verification:
      - kind: unit
        ref: "Tests/ShadowArrayTests.cpp forward-Execute-binds-nothing assertion"
        status: pass
    human_judgment: false
  - id: D6
    description: "Multi-cascade shadows visually verified on a real GPU: no popping at cascade transitions, no new acne beyond baseline, resize clean"
    requirement: "FRZ-02"
    verification: []
    human_judgment: true
    rationale: "Pixel correctness (cascade-transition popping, acne/peter-panning vs baseline, post-resize corruption) cannot be asserted headless — smoke tests are not pixel validation per AGENTS.md. Requires blocking human inspection on real hardware (D-04 bound). User approved this session."

duration: ~25min
completed: 2026-09-13
status: complete
---

# Phase 01 Plan 03: Shadow-Map-Array Binding Summary

**Deferred shadows flow end to end through one GL_TEXTURE_2D_ARRAY bound once with matrices, splits, and count uploaded; empty/invalid inputs behave explicitly; forward stays unshadowed by design; human visual sign-off recorded — FRZ-02 satisfied.**

## Performance

- **Duration:** ~25 min (continuation close-out; production commits `8b37376` + `6c95a4e` pre-existed, verified in git log)
- **Started:** 2026-09-13 (tracer + expansion, prior executors this session)
- **Completed:** 2026-09-13
- **Tasks:** 3 (tracer + expansion + human-verify sign-off)
- **Files modified:** 7 (4 engine, 3 test harness) plus this SUMMARY

## Accomplishments

- Task 1 (tracer, `8b37376`): one-layer shadow array end to end — `ShadowMapPass` owns one `GL_TEXTURE_2D_ARRAY` depth texture (2048x2048x1, `DEPTH_COMPONENT24`, `NEAREST`, `CLAMP_TO_BORDER` white) with a shared FBO; each cascade renders into its layer via `glFramebufferTextureLayer`; `DeferredLightingPass` replaces the first-cascade `GL_TEXTURE_2D` bind with a single `BindNativeTexture` of the array on unit 5 (target `GL_TEXTURE_2D_ARRAY`, `u_ShadowMaps` name and slot kept) plus `u_LightSpaceMatrices` and `u_CascadeCount` uploads; `CheckError` after array creation and first bind; shadow resolution fixed at 2048 independent of window size.
- Task 2 (expansion, `6c95a4e`): generalized to 2048x2048xcascade-count with transactional recreation (replacement built and swapped only on success; `SetCascadeCount`/`SetShadowMapResolution` commit config only when recreation succeeds); `u_CascadeSplits` uploaded with count-plus-one elements via indexed uniform names (`IShader` has no float-array setter); cascade count validated above zero and within `GL_MAX_ARRAY_TEXTURE_LAYERS`; empty input skips the bind with lighting continuing unshadowed.
- Task 3 (human-verify): user ran `Examples/BasicRendering` on a real GPU and **approved** — no popping at cascade transitions, no new acne beyond the pre-change baseline, resize clean. D-04 human-verification bound satisfied; sign-off recorded below.
- Forward-pipeline Q4/A2 resolution (RESEARCH): out-of-scope rationale, no code change — `ForwardRenderPass::Execute/End` carries no shadow binds; `forward.frag` array sampler is guarded by `u_CascadeCount` defaulting 0, so the forward path renders unshadowed by design with shadow support arriving via the deferred path only; pinned by a test asserting forward `Execute` binds nothing.
- Full debug build zero errors; `ctest --preset test-gcc-debug` **58/58 green** including the new `Graphics.ShadowArray` suite (run this session by the prior executors; not re-run in this close-out per instruction).

## Task Commits

Each task was committed atomically:

1. **Task 1: Bind a one-layer shadow array end to end with matrix uploads** - `8b37376` (feat)
2. **Task 2: Expand to N cascades with splits upload and restore preservation** - `6c95a4e` (feat)
3. **Task 3: Human visual inspection of multi-cascade shadows** - approved by user this session (no commit — verification only; sign-off: no cascade-transition popping, no new acne, resize clean)

## Files Created/Modified

- `Tests/ShadowArrayTests.cpp` - New suite (fake-GL harness shape with file-local `g_capture` globals, `Fake` entry points, suite-prefixed `Fail`): array completeness, layer-count-equals-cascade-count, deterministic layer order, single array bind with exact-once target assertion, matrix/count/splits upload presence, recreation-on-count-change swap, failure preservation, over-limit and empty skips, restore assertions, forward-Execute-binds-nothing pin
- `Engine/Graphics/source/Renderer/ShadowMapPass.cpp` - Array ownership (`CreateShadowArrayTexture` with count/extent/shader-bound validation), per-layer population, transactional recreation, empty skip
- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp` - Cascade storage and creation member layout for the array path
- `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp` - Single array bind plus `u_LightSpaceMatrices`, `u_CascadeCount`, `u_CascadeSplits` uploads
- `Engine/Graphics/source/Renderer/RenderSystem.cpp` - Deferred-pipeline wiring of the shadow pass link (deviation fix; see below)
- `Tests/TestGraphicsDevice.hpp` - Harness support for the shadow-array capture fixtures
- `Tests/CMakeLists.txt` - Registration of the new `ShadowArrayTests` suite

## Decisions Made

- Forward pipeline renders unshadowed by design (Q4/A2): no shared-helper wiring, no duplicated bind logic, no new shadow technique — the forward path carries no shadow uniform uploads and the sampler guard (`u_CascadeCount` default 0) makes that explicit rather than accidental.
- Layered `glFramebufferTextureLayer` rendering into one shared FBO (not keep-N-framebuffers plus depth blit); the chosen method is recorded in code comments.
- Counts above the 4-cascade shader bound fail explicitly at creation instead of silent UB (deviation fix).
- Restore lines stay in every touched pass; neutral-interface migration is plan 01-04's job.
- No shader, docs, or `CHANGELOG.md` edits in this plan — 01-04 consolidates (Wave-1 docs owners are 01-01 for CHANGELOG.md and 01-02 for docs/Architecture.md).

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Wired RenderSystem.cpp SetShadowMaps to SetShadowPass**
- **Found during:** Task 1 (tracer wiring)
- **Issue:** The new array bind path would have been dead code — the deferred pipeline never linked the shadow pass output into the lighting pass entry point.
- **Fix:** Wired `SetShadowMaps` through to `SetShadowPass` in `Engine/Graphics/source/Renderer/RenderSystem.cpp` so the single array bind actually executes in the deferred pipeline.
- **Files modified:** `Engine/Graphics/source/Renderer/RenderSystem.cpp`
- **Verification:** ShadowArrayTests bind-target and upload-presence fixtures plus full suite green.
- **Committed in:** `8b37376` (part of task commit)

**2. [Rule 2 - Missing Critical] Cascade counts above the 4-cascade shader bound fail explicitly at creation**
- **Found during:** Task 2 (validation generalization)
- **Issue:** The plan specified validation against zero and `GL_MAX_ARRAY_TEXTURE_LAYERS` but not the tighter shader-side bound (lighting shader declares 4 cascades); a 5+ cascade array would create successfully and then sample out of bounds — silent UB.
- **Fix:** `CreateShadowArrayTexture` rejects counts above the 4-cascade shader bound explicitly at creation; `SetCascadeCount`/`SetShadowMapResolution` preserve the previous array on any recreation failure.
- **Files modified:** `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`
- **Verification:** Over-limit and failure-preservation fixtures in ShadowArrayTests; full suite green.
- **Committed in:** `6c95a4e` (part of task commit)

**Removals (no separate commit):** old `SetShadowMaps`/`GetShadowMaps` single-texture surface and per-cascade FBOs removed as part of the array migration (`8b37376`, `6c95a4e`).

---

**Total deviations:** 2 auto-fixed (both missing-critical)
**Impact on plan:** Both were required for correctness (live wiring, no silent UB). No scope creep: no new engine capabilities, no shader/docs/CHANGELOG edits, no architectural changes (Rule 4 never triggered).

## Threat Mitigations (plan threat register T-03-01…T-03-04)

| Threat | Disposition | How mitigated |
|--------|-------------|---------------|
| T-03-01 (DoS, array creation) | mitigate | Count validated above zero, within `GL_MAX_ARRAY_TEXTURE_LAYERS`, and within the 4-cascade shader bound plus extent checks; transactional recreation preserves the previous texture on failure; empty and oversized inputs covered by tests |
| T-03-02 (Tampering, bind) | mitigate | Bind target asserted equals `GL_TEXTURE_2D_ARRAY` matching `sampler2DArray`; `CheckError` after creation and first bind; single-bind exact-once test |
| T-03-03 (Info disclosure, later passes/UI) | mitigate | After-pass framebuffer plus viewport restore lines preserved in every touched pass; restoration tests extended for the shadow path |
| T-03-04 (Repudiation, layer ordering) | mitigate | Layer index follows matrix and split order deterministically; layer-count and splits-presence tests pin the correspondence |
| T-03-SC (package installs) | accept | No package installs; all entry points from vendored `vendor/glad` — legitimacy gate N/A |

## Issues Encountered

- None blocking. Both production commits built with zero errors and the full 58/58 suite was green in the same session before this close-out.
- Renderer changes required human visual inspection (smoke tests are not pixel validation per AGENTS.md) — satisfied by the Task 3 user sign-off above.

## User Setup Required

None - no external service configuration required.

## Human Visual Sign-off (Task 3, D-04)

- **Verifier:** user, this session, on a real GPU via `Examples/BasicRendering`
- **Result:** approved — proceed to SUMMARY + tracking updates
- **Recorded observations:** no popping at cascade transitions; no shadow acne or peter-panning beyond the pre-change baseline look; resize during shadow rendering leaves no viewport or framebuffer corruption

## Next Phase Readiness

- FRZ-02 complete. Ready for plan 01-04 (FRZ-05: `IFramebuffer`, neutral binding, depth targets) — which owns the Wave-1 docs consolidation including any shadow behavior notes warranted from this plan.
- Plans in this phase must not edit `CHANGELOG.md` outside the 01-04 consolidation.

---
*Phase: 01-p0-core-freeze*
*Completed: 2026-09-13*

## Self-Check: PASSED

- `Tests/ShadowArrayTests.cpp` plus all 6 modified files verified present on disk; task commits `8b37376` (Task 1) and `6c95a4e` (Task 2) verified in `git log --oneline --grep="01-03"`.
- All plan acceptance criteria re-verified against the committed code: no `BindNativeTexture` with `GL_TEXTURE_2D` remains on the shadow path; `u_LightSpaceMatrices`, `u_CascadeCount`, and `u_CascadeSplits` (plus-one) uploads exist; array depth equals cascade count with transactional recreation; count validation precedes creation; restore lines remain in every touched pass; forward-pipeline scoping resolved with SUMMARY rationale plus a binds-nothing test pin.
- Plan-level verification (debug build zero errors; `ctest --preset test-gcc-debug` 58/58 green incl. new `Graphics.ShadowArray`) was run this session by the prior executors and is recorded here without re-run per close-out instruction.
