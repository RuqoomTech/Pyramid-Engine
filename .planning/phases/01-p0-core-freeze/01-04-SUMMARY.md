---
phase: 01-p0-core-freeze
plan: 04
subsystem: graphics
tags: [opengl, framebuffer, backend-neutral, depth-texture, texture-format-mapping, render-passes]

requires:
  - phase: 01-p0-core-freeze/01-01
    provides: "Clean compute/occlusion removals and the conflict-free Tests/PublicApiLinkage.cpp baseline this plan's pins extend"
  - phase: 01-p0-core-freeze/01-02
    provides: "Verified 3.3-core depth and packed depth-stencil format triples in OpenGLTexture2D::ResolveFormats that CreateDepthTarget reuses, plus the CHANGELOG entry text consolidated here"
  - phase: 01-p0-core-freeze/01-03
    provides: "ShadowMapPass array target and DeferredLightingPass array restores that this plan migrates onto the neutral bind without changing single-bind or upload behavior"
provides:
  - "IFramebuffer defined in a public GLAD-free and Win32-free header, with OpenGLFramebuffer implementing it (FRZ-05)"
  - "Real OpenGLDevice::BindFramebuffer replacing the not-yet-implemented stub, with symmetric error state (FRZ-05)"
  - "Every render pass and system framebuffer bind routed through IGraphicsDevice::BindFramebuffer; BindFramebufferHandle reduced to a device-internal workhorse (FRZ-05)"
  - "ITexture2D::CreateDepthTarget creating real sampled depth textures transactionally for the mapped depth set (FRZ-05)"
  - "Neutral-restore coverage for RenderTarget, CommandBuffer, DeferredGeometryPass, and RenderSystem plus per-depth-format depth-target fixtures"
  - "docs/Architecture.md and CHANGELOG.md entries for the routing change, the depth-target contract, and the consolidated FRZ-02/FRZ-04 notes"
affects: [phase-01-verification, render-pass-routing, sampled-depth-usage, resource-registry-ownership]

actuals:
  tokens: 13903
  tasks: 3
  commits: 3

tech-stack:
  added: []
  patterns:
    - "neutral-bind-only: passes hold IFramebuffer objects, never raw backend handles; BindFramebufferHandle stays device-internal"
    - "non-owning-layered-framebuffer-view: a target the backend created outside the attachment-spec path joins the neutral contract without gaining attachment management"
    - "explicit-compare-mode-pin: depth textures set GL_TEXTURE_COMPARE_MODE to GL_NONE at creation instead of trusting a driver default"
    - "fake-gl-harness-must-stub-every-used-entry-point: an unstubbed glad pointer is a null call, not a no-op"

key-files:
  created:
    - Engine/Graphics/include/Pyramid/Graphics/Framebuffer.hpp
  modified:
    - Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp
    - Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp
    - Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLTexture.hpp
    - Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp
    - Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp
    - Engine/Graphics/source/OpenGL/OpenGLDevice.cpp
    - Engine/Graphics/source/OpenGL/OpenGLTexture.cpp
    - Engine/Graphics/source/Renderer/CommandBuffer.cpp
    - Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp
    - Engine/Graphics/source/Renderer/DeferredLightingPass.cpp
    - Engine/Graphics/source/Renderer/RenderSystem.cpp
    - Engine/Graphics/source/Renderer/ShadowMapPass.cpp
    - Engine/Graphics/source/Texture.cpp
    - Engine/Graphics/source/UI/UIRenderer.cpp
    - Tests/FramebufferResizeTests.cpp
    - Tests/PublicApiLinkage.cpp
    - Tests/ShadowArrayTests.cpp
    - Tests/TestGraphicsDevice.hpp
    - Tests/TextureLoadingTests.cpp
    - Tests/UIRendererTests.cpp
    - docs/Architecture.md
    - CHANGELOG.md

key-decisions:
  - "Added a non-owning OpenGLLayeredFramebuffer implementing IFramebuffer so the plan 01-03 shadow array can bind neutrally: the attachment-spec path cannot express a GL_TEXTURE_2D_ARRAY layer attach, and teaching OpenGLFramebuffer layered attachments would have been a new capability disguised as a freeze fix"
  - "ShadowMapPass builds the layered view before binding and commits it only after the completeness check, so the pass never holds the raw handle even during creation, and a failed attempt leaves the previous array in service"
  - "RenderTarget now keeps the IGraphicsDevice handed to Initialize and binds through BindFramebuffer; Unbind stays a silent no-op on an uninitialized target exactly as before, since nothing was bound to restore"
  - "Compare mode is pinned explicitly to GL_NONE in ApplyParameters for depth formats rather than relying on the driver default, so the plan's compare-mode-NONE contract is observable and a later sampler change cannot silently turn comparison on"
  - "Depth targets are never mipmapped: filtering between depth levels changes depth and shadow semantics, so CreateDepthTarget fixes GenerateMips false rather than inheriting the specification default"
  - "The test device's BindFramebuffer records binds instead of being a silent no-op; a required interface method with an empty body is exactly what AGENTS.md forbids and it also made the migration unobservable to tests"
  - "Plan 01-03 behavior was preserved verbatim: single GL_TEXTURE_2D_ARRAY bind on unit 5, u_LightSpaceMatrices, u_CascadeCount, and plus-one u_CascadeSplits uploads, and the after-pass restore shape all still present and pinned"
  - "Build output was relocated off the full D: drive to the approved temp directory using the same gcc-debug-tests preset via -B; same compiler, generator, and options, only a different binary directory"

patterns-established:
  - "Neutral bind contract: IFramebuffer (Bind, Unbind, IsComplete, GetWidth, GetHeight, opaque u32 GetNativeHandle) in a header that includes only Pyramid/Core/Prerequisites.hpp, so the neutral boundary leaks no backend tokens"
  - "Restore shape: comment plus framebuffer-then-viewport in that exact order, with the framebuffer line expressed as BindFramebuffer(nullptr)"
  - "Transactional creation: validate extent, allocate, upload, then roll back (delete the object, clear the handle) so a rejected request leaves nothing half-made and logs with the requested dimensions"

requirements-completed: [FRZ-05]

coverage:
  - id: D1
    description: "An agent holding an IFramebuffer object binds it through IGraphicsDevice::BindFramebuffer and gets real binding with symmetric error state, never a not-yet-implemented stub"
    requirement: "FRZ-05"
    verification:
      - kind: unit
        ref: "Graphics.FramebufferResize neutral-bind real-object, null-bind default, and error-state-symmetry fixtures; Graphics.FramebufferResize and API.PublicApiLinkage both Passed (ctest --preset test-gcc-debug -R 'Framebuffer|PublicApi', 2/2)"
        status: pass
      - kind: other
        ref: "repo-wide search for 'Framebuffer binding not yet implemented' and 'Implement when IFramebuffer interface is available' across Engine/Tests/Examples/Libraries returns zero matches; Framebuffer.hpp include list is Prereqisites only"
        status: pass
    human_judgment: false
  - id: D2
    description: "RenderSystem restore, RenderTarget bind and unbind, and every pass bind route through the neutral method; no pass or system call site holds a raw GLuint across the boundary"
    requirement: "FRZ-05"
    verification:
      - kind: unit
        ref: "Graphics.FramebufferResize RenderTarget Bind/Unbind, CommandBuffer null-target, DeferredGeometryPass Begin/End, and RenderSystem.Render restore fixtures; Graphics.FramebufferResize, Graphics.RenderObjectBounds, Graphics.UIRenderer all Passed (ctest --preset test-gcc-debug -R 'Framebuffer|Render', 3/3)"
        status: pass
      - kind: other
        ref: "repo-wide search for BindFramebufferHandle( call sites in Engine returns only the interface declaration, the OpenGLDevice override declaration, and the OpenGLDevice definition; no pass or renderer call site remains"
        status: pass
    human_judgment: false
  - id: D3
    description: "Plan 01-03 shadow-array behavior preserved across the routing change: single array bind with matching sampler target, matrix, split, and count uploads, deterministic layer order, and the after-pass restore"
    requirement: "FRZ-05"
    verification:
      - kind: unit
        ref: "Graphics.ShadowArray array-completeness, layer-count, single-bind exact-once target, matrix/count/splits upload-presence, recreation, empty-skip, and restoration fixtures, all within the 58/58 green full debug suite"
        status: pass
    human_judgment: false
  - id: D4
    description: "Creating a depth texture through ITexture2D::CreateDepthTarget yields a real sampled depth texture for each mapped depth or stencil format, transactionally, with explicit failure on invalid extents"
    requirement: "FRZ-05"
    verification:
      - kind: unit
        ref: "Graphics.TextureLoading per-depth-format CreateDepthTarget fixtures (triple, metadata, compare-mode NONE, no mipmap), non-depth rejection, and zero/oversized explicit-failure fixtures; Graphics.TextureLoading Passed"
        status: pass
      - kind: integration
        ref: "ctest --preset test-gcc-debug -R 'Texture|Resource' 9/9 pass including Graphics.ResourceHandles, Graphics.ResourceManifest, and Graphics.ResourceRegistry; full debug suite 58/58"
        status: pass
    human_judgment: false
  - id: D5
    description: "docs/Architecture.md distinguishes sampled depth texture from depth attachment usage and records the neutral bind contract; CHANGELOG.md carries consolidated entries for plans 01-02, 01-03, and 01-04"
    requirement: "FRZ-05"
    verification:
      - kind: other
        ref: "docs/Architecture.md lines 122, 133, 135 (neutral bind, CreateDepthTarget contract, sampled-texture-versus-attachment coexistence) and CHANGELOG.md Added (FRZ-04), Added (FRZ-02), Added (FRZ-05) sections"
        status: pass
    human_judgment: false
  - id: D6
    description: "Framebuffer routing migration and depth targets confirmed visually on a real GPU across both examples, including resize, minimize-restore, and close, with screenshots captured for the record"
    requirement: "FRZ-05"
    verification: []
    human_judgment: true
    rationale: "Renderer changes require human visual inspection because process smoke testing is not pixel validation (AGENTS.md, and the plan's D-04 must-have). Off-screen state leaking into the world or UI pass, viewport corruption after a routing change, and shadow regressions are only observable as pixels on real hardware. This is the plan's Task 4 blocking checkpoint and it has NOT been performed: no human sign-off exists yet, so the D-04 bound is unsatisfied and the plan is recorded as halted rather than complete."

duration: ~75min
completed: 2026-09-25
status: halted
plan_head_before: 73bd04bbee7ceb0f36d7d298987cd601ba493d19
---

# Phase 01 Plan 04: Neutral Framebuffer Binding and Depth Targets Summary

**`IFramebuffer` is defined and every render pass binds through the backend-neutral `IGraphicsDevice::BindFramebuffer` instead of a raw handle, while `CreateDepthTarget` now returns a real transactionally-created sampled depth texture — FRZ-05 automated work complete, D-04 human visual sign-off still outstanding.**

## Performance

- **Duration:** ~75 min (Tasks 1-3; Task 4 stopped at its blocking checkpoint)
- **Started:** 2026-09-25T19:55:14Z
- **Completed:** 2026-09-25
- **Tasks:** 3 of 4 executed (Task 4 is a human-verify checkpoint, not executed by the agent)
- **Files modified:** 23 plus 1 created (24 total, including this SUMMARY)

## Accomplishments

- **Task 1 (`3e66bac`):** `Pyramid::IFramebuffer` is defined in a new public header whose only include is `Pyramid/Core/Prerequisites.hpp` — no GLAD, no Win32 — with the minimal surface `Bind`, `Unbind`, `IsComplete`, `GetWidth`, `GetHeight`, and an opaque `u32 GetNativeHandle`. `OpenGLFramebuffer` implements it. `OpenGLDevice::BindFramebuffer` performs real binding: a non-null target binds through the interface and clears `m_lastError`, `nullptr` restores the default surface. The `"Framebuffer binding not yet implemented"` stub and its TODO are gone from the tree. Six linkage pins added in the existing `volatile decltype` idiom.
- **Task 2 (`2843b37`):** every render pass and system framebuffer bind routes through the neutral method — the `RenderSystem` per-pass restore (framebuffer-then-viewport order and its two-line comment preserved exactly), `RenderTarget::Bind`/`Unbind`, the `ShadowMapPass` array target and its restores, `DeferredGeometryPass` G-buffer bind and restore, `DeferredLightingPass` restore, the command buffer's null render-target command, and the `UIRenderer` final surface. `BindFramebufferHandle(u32)` survives only as the device-internal workhorse; no pass holds a `GLuint` across the boundary. Plan 01-03's single-array-bind and matrix/split/count uploads are unchanged and still pinned.
- **Task 3 (`513fa72`):** `ITexture2D::CreateDepthTarget` replaces the explicit-failure stub with a real sampled depth texture through `OpenGLTexture2D` for `Depth16`, `Depth24`, `Depth32F`, `Depth24Stencil8`, and `Depth32FStencil8`, reusing plan 01-02's verified triples. Creation is transactional (validate extent, allocate, upload, roll back), non-depth formats are rejected with a pointer at framebuffer attachments, depth targets are never mipmapped, and `GL_TEXTURE_COMPARE_MODE` is pinned to `GL_NONE` explicitly. `docs/Architecture.md` documents sampled-depth-texture versus depth-attachment coexistence; `CHANGELOG.md` consolidates the FRZ-02, FRZ-04, and FRZ-05 entries as the Wave-2 docs owner.
- **Verification:** debug build zero errors; `ctest --preset test-gcc-debug` **58/58 green**, matching plan 01-03's baseline exactly (no regression, no new test target). Both `BasicGame.exe` and `BasicRenderingExample.exe` build with the migration.
- **Task 4 not performed:** the D-04 human visual inspection is a blocking `checkpoint:human-verify` and was not run. No visual sign-off is claimed anywhere in this SUMMARY.

## Task Commits

Each task was committed atomically:

1. **Task 1: Define IFramebuffer and bind it end to end through the device** - `3e66bac` (feat)
2. **Task 2: Route call sites through neutral bind** - `2843b37` (feat)
3. **Task 3: Implement depth targets through the texture interface** - `513fa72` (feat)
4. **Task 4: Human visual inspection** - not performed; blocking checkpoint awaiting a human on real GPU hardware

**Plan metadata:** this SUMMARY commit.

_Note: the commit range `73bd04b..HEAD` contains a fourth commit, `8411e1f` "Refine Phase 01 planning and ownership", which the orchestrator made concurrently and which is not part of this plan's production work. The three commits above are this plan's._

## Files Created/Modified

- `Engine/Graphics/include/Pyramid/Graphics/Framebuffer.hpp` - **New.** `IFramebuffer` contract, backend-neutral by construction.
- `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` - Includes the real header instead of forward-declaring; documents `BindFramebuffer` as the only call-site bind and `BindFramebufferHandle` as the device-internal workhorse.
- `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp` - `OpenGLFramebuffer : public IFramebuffer` with overrides; adds the non-owning `OpenGLLayeredFramebuffer` view.
- `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLTexture.hpp` - Declares the shared `IsDepthStencilFormat` predicate.
- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp` - `ShadowMapPass` holds a `std::unique_ptr<OpenGLLayeredFramebuffer>` instead of a `GLuint` framebuffer.
- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` - `RenderTarget` keeps its `IGraphicsDevice*`; bind/unbind contract documented.
- `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` - Real `BindFramebuffer` with symmetric error clearing.
- `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp` - `IsDepthStencilFormat`; explicit compare-mode pin in `ApplyParameters`.
- `Engine/Graphics/source/Renderer/CommandBuffer.cpp` - Null render-target command restores the default surface neutrally.
- `Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp` - G-buffer bound as an object; restore routed neutrally.
- `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp` - Restore routed neutrally.
- `Engine/Graphics/source/Renderer/RenderSystem.cpp` - Neutral per-pass restore; `RenderTarget::Bind`/`Unbind` route through the device.
- `Engine/Graphics/source/Renderer/ShadowMapPass.cpp` - Array target created, bound, and destroyed through the neutral view; restore lines as 01-03 left them.
- `Engine/Graphics/source/Texture.cpp` - `CreateDepthTarget` implemented with transactional creation and dimension-bearing diagnostics.
- `Engine/Graphics/source/UI/UIRenderer.cpp` - Final surface established through the neutral method.
- `Tests/PublicApiLinkage.cpp` - Six pins for the new `IFramebuffer` surface.
- `Tests/FramebufferResizeTests.cpp` - Neutral-bind real/null/error-symmetry plus neutral-restore coverage for RenderTarget, CommandBuffer, DeferredGeometryPass, and RenderSystem; fake GL extended (`glGetError`, `glTexImage3D`, `glFramebufferTextureLayer`, `glObjectLabel`, array-layer query, coherent framebuffer-binding queries).
- `Tests/TestGraphicsDevice.hpp` - `BindFramebuffer` now records instead of silently no-oping; adds shared `TestShader` and `TestUniformBuffer`.
- `Tests/ShadowArrayTests.cpp` - Shadow restore assertions now prove the neutral method was used.
- `Tests/TextureLoadingTests.cpp` - Per-depth-format `CreateDepthTarget` fixtures with compare-mode capture, non-depth rejection, zero/oversized explicit failure.
- `Tests/UIRendererTests.cpp` - UI surface assertion proves the neutral method was used.
- `docs/Architecture.md` - Neutral bind contract; `CreateDepthTarget` contract; sampled depth texture versus depth attachment coexistence.
- `CHANGELOG.md` - Consolidated `Added (FRZ-04)`, `Removed (FRZ-04)`, `Added (FRZ-02)`, and `Added (FRZ-05)` entries.

## Decisions Made

- **Non-owning layered view instead of teaching `OpenGLFramebuffer` layered attachments.** A `GL_TEXTURE_2D_ARRAY` layer attach cannot be expressed by the attachment-spec path, so either the shadow array kept a raw handle (failing the plan's own must-have) or the backend gained a new attachment capability — which the plan's prohibitions forbid as a "new engine capability disguised as a freeze fix". The view adds no attachment management and owns nothing.
- **RenderTarget stores its device rather than a second raw-OpenGL lifecycle.** This is what makes `RenderTarget::Bind` route through `BindFramebuffer` instead of calling the concrete backend object directly. Lifetime is safe: targets are owned by `RenderSystem`, which is initialized with the device.
- **Compare mode pinned rather than inherited.** The plan requires compare mode `NONE`; relying on a driver default would satisfy the letter of it while leaving the contract unobservable and one sampler change away from breaking.
- **Test-device `BindFramebuffer` made observable.** It was a required interface method with an empty body — the exact anti-pattern AGENTS.md names — and it also meant no test could see the migration at all.
- **Build relocated off the full D: drive.** See the environment note below.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added `OpenGLLayeredFramebuffer` so the shadow array can bind neutrally**
- **Found during:** Task 2 (call-site migration)
- **Issue:** The plan required migrating `ShadowMapPass`'s array binds onto `BindFramebuffer` with `IFramebuffer` objects, but the pass owned a bare `GLuint` created with `glGenFramebuffers` because the attachment-spec path cannot express a `GL_TEXTURE_2D_ARRAY` layer attach. Without an object there was nothing to pass, and the plan's "no pass holds a raw GLuint" acceptance criterion was unsatisfiable.
- **Fix:** Added a small non-owning `OpenGLLayeredFramebuffer : public IFramebuffer` in the OpenGL backend, and made `ShadowMapPass` build the view before binding and commit it only after the completeness check.
- **Files modified:** `OpenGLFramebuffer.hpp`, `RenderPasses.hpp`, `ShadowMapPass.cpp`
- **Verification:** Repo-wide search shows zero `BindFramebufferHandle(` call sites in passes; `Graphics.ShadowArray` and `Graphics.FramebufferResize` green.
- **Committed in:** `2843b37`

**2. [Rule 2 - Missing Critical] Migrated three call sites the plan's file list omitted**
- **Found during:** Task 2
- **Issue:** `DeferredGeometryPass.cpp` (G-buffer bind plus restore), `UIRenderer.cpp` (final surface), and `CommandBuffer.cpp` (null render-target restore) all bound framebuffers, but none was in the plan's Task 2 file list. `DeferredGeometryPass` passed `m_gBuffer->GetFramebufferID()` — a raw handle straight across the boundary — so the plan's acceptance criterion was unsatisfiable without it.
- **Fix:** Routed all three through `BindFramebuffer`.
- **Files modified:** `DeferredGeometryPass.cpp`, `UIRenderer.cpp`, `CommandBuffer.cpp`
- **Verification:** Repo-wide search confirms no remaining call sites; `Graphics.UIRenderer` green; `Graphics.FramebufferResize` drives the `DeferredGeometryPass` and command-buffer paths.
- **Committed in:** `2843b37`

**3. [Rule 3 - Blocking] Removed the silent no-op on `TestGraphicsDevice::BindFramebuffer` and added two shared fakes**
- **Found during:** Task 2
- **Issue:** `Tests/TestGraphicsDevice.hpp` implemented a required interface method as `{}` — a prohibited silent no-op — which also made the entire migration unobservable to the suites that need to prove it. Driving `RenderSystem` additionally needed a working `IUniformBuffer` (it refuses to initialize without one) and a non-null `IShader` (pass constructors dereference it), and both fakes did not exist.
- **Fix:** `BindFramebuffer` now records the bind, the bound handle, and a call count; added `TestShader` and `TestUniformBuffer` to the shared harness.
- **Files modified:** `Tests/TestGraphicsDevice.hpp`
- **Verification:** `Graphics.FramebufferResize` proves the migration end to end; `Graphics.ShadowArray` and `Graphics.UIRenderer` pass with strengthened assertions.
- **Committed in:** `2843b37`

**4. [Rule 3 - Blocking] Made the compare-mode contract explicit in `OpenGLTexture.cpp`**
- **Found during:** Task 3
- **Issue:** Task 3's file list omitted `OpenGLTexture.cpp`, but "compare mode `NONE` default" is only observable if something sets it; the plan also required a per-format test, and a test that asserts a driver default the fake cannot model proves nothing.
- **Fix:** `ApplyParameters` pins `GL_TEXTURE_COMPARE_MODE` to `GL_NONE` for the depth/stencil format set, and the new test captures and asserts it.
- **Files modified:** `OpenGLTexture.cpp`, `OpenGLTexture.hpp`, `Tests/TextureLoadingTests.cpp`
- **Verification:** `Graphics.TextureLoading` asserts compare mode `GL_NONE` for all five depth formats.
- **Committed in:** `513fa72`

**5. [Rule 3 - Blocking] Extended the fake GL surface after a hard crash exposed an engine-side latent bug**
- **Found during:** Task 2
- **Issue:** Driving `DeferredGeometryPass` under the fake GL harness died with an access violation (exit `-1073741819`). Cause: `OpenGLFramebuffer::SetDebugLabel` calls `glObjectLabel` with no `GLAD_GL_KHR_debug` guard, so on the locked 3.3 baseline the pointer is `NULL` and the call is a null-function-pointer dereference — RESEARCH §Common Pitfalls 1 exactly. This is a real engine defect, not a harness bug, but it is pre-existing and outside this plan's scope.
- **Fix (in scope):** Added the missing `glObjectLabel` stub to the test harness. **Not fixed (out of scope):** the engine-side guard — recorded in `.planning/phases/01-p0-core-freeze/deferred-items.md` with rationale and a suggested owner.
- **Files modified:** `Tests/FramebufferResizeTests.cpp`, `.planning/phases/01-p0-core-freeze/deferred-items.md`
- **Verification:** `Graphics.FramebufferResize` runs the full pass cleanly.
- **Committed in:** `2843b37` (harness), `8411e1f` (deferred-items.md, swept in by the orchestrator's concurrent commit)

**6. [Rule 3 - Blocking] Made the fake framebuffer-binding queries state-coherent**
- **Found during:** Task 1
- **Issue:** The fake `glGetIntegerv` returned 0 for `GL_DRAW_FRAMEBUFFER_BINDING`, so the state manager's lazy cache initialization believed framebuffer 0 was bound and skipped the very `glBindFramebuffer` call the null-bind test was meant to observe. The first version of the test failed for that reason, not because of a product defect.
- **Fix:** The fake now reports what it last bound, matching real GL semantics.
- **Files modified:** `Tests/FramebufferResizeTests.cpp`
- **Verification:** `Graphics.FramebufferResize` passes with a call-count assertion of exactly 1 for a real neutral bind.
- **Committed in:** `3e66bac`

**7. [Rule 3 - Blocking] Relocated build output off a full D: drive**
- **Found during:** Task 1
- **Issue:** `cmake --build --preset build-gcc-debug-tests` failed with `ar.exe: unable to copy file 'lib\libPyramidEngined.a'; reason: No space left on device`, preceded by `cc1plus.exe: out of memory allocating 1048568 bytes`. `D:` had 0.05 GB free; the canonical build directory alone is 0.78 GB. This is an environment condition, not a code problem.
- **Fix:** Configured and built the *same* `gcc-debug-tests` preset into `C:\Users\user\AppData\Local\Temp\opencode\pyramid-gcc-debug-tests` via `cmake --preset gcc-debug-tests -B <dir>`, then ran `ctest --preset test-gcc-debug --test-dir <dir>`. Same compiler (MSYS2 UCRT64 GCC 16.2.0), same generator, same cache variables, same test registrations — only the binary directory differs. No substitute toolchain and no altered flags.
- **Files modified:** none (build output only)
- **Verification:** Build exits 0; `ctest` 58/58; both example binaries produced.
- **Impact:** The canonical `build/gcc-debug-tests` tree on `D:` is now stale. It should be deleted or rebuilt once disk space is available.

---

**Total deviations:** 7 auto-fixed (2 missing critical, 5 blocking)
**Impact on plan:** All seven were required to satisfy the plan's own acceptance criteria or to make them observable. No scope creep, no new engine capability, no architectural change (Rule 4 never triggered). One genuine engine defect was found and deliberately deferred rather than fixed, per the scope boundary.

## Issues Encountered

- **The D: drive is full (0.05 GB free).** Every build and test in this plan ran from a relocated build tree. The user should free space and rebuild the canonical tree; the results reported here are real and were produced by the documented toolchain.
- **Latent engine defect found and deferred:** `OpenGLFramebuffer::SetDebugLabel` calls `glObjectLabel` unguarded, which is a null-pointer call on the locked OpenGL 3.3 baseline. Recorded in `.planning/phases/01-p0-core-freeze/deferred-items.md`. This is worth a decision before the pre-release tag, since `docs/ROADMAP.md` gates tagging on real-hardware runtime verification and a crash on a 3.3 context would surface exactly there.
- **A concurrent orchestrator commit (`8411e1f`) landed inside this plan's commit range** and swept the refined planning files plus `deferred-items.md` into git. It is not plan production work. It was verified to match what was executed: the refinement is precisely the 4-task split (1 interface, 2 routing, 3 depth targets, 4 human gate) followed here.
- Pre-existing `unused parameter 'cmd'` warnings in `DeferredLightingPass.cpp` (3 sites) are out of scope and were left untouched, consistent with plan 01-01's treatment.

## User Setup Required

None - no external service configuration required.

## Outstanding: Task 4 Human Visual Inspection (D-04, not performed)

This is the plan's blocking `checkpoint:human-verify`. **It has not been run and no sign-off is claimed.** Per AGENTS.md, renderer changes require visual inspection because process smoke testing is not pixel validation. A human must run both examples on a real GPU and confirm:

- **Launch command** (MSYS2 UCRT64 shell, or directly — the MinGW runtime DLLs are bundled beside the binaries):
  `& 'C:\Users\user\AppData\Local\Temp\opencode\pyramid-gcc-debug-tests\bin\BasicGame.exe'`
  `& 'C:\Users\user\AppData\Local\Temp\opencode\pyramid-gcc-debug-tests\bin\BasicRenderingExample.exe'`
  (If the canonical tree is rebuilt on `D:` first, use `build/gcc-debug-tests/bin/` instead.)
- **What to observe:**
  1. Rendering is correct with no viewport corruption and no leaked off-screen state leaking into the UI pass.
  2. Resize, minimize-restore, and close behave per the `docs/ROADMAP.md` P0 Windows runtime verification checklist subset.
  3. Shadowed rendering still looks correct after the routing migration (no cascade-transition popping, no new acne, no peter-panning).
  4. Screenshots captured from the reference rendering path for the record.
- **Resume signal:** type "approved" or describe rendering defects with the lifecycle step where they appear.

Until then, FRZ-05 is code-complete and test-covered but its D-04 hardware-verification bound is unsatisfied, which is why this plan is recorded `status: halted` rather than `complete`.

## Threat Mitigations (plan threat register T-04-01…T-04-04)

| Threat | Disposition | How mitigated |
|--------|-------------|---------------|
| T-04-01 (Tampering, `BindFramebuffer`) | mitigate | Non-null branch binds through the target and clears `m_lastError`; null branch binds the default surface; fake-GL tests prove real binding (target and call count), the null path, and error-state symmetry |
| T-04-02 (DoS, `CreateDepthTarget` extents) | mitigate | `IsDepthStencilFormat` gates the format; `IsValidExtent` gates the extent before allocation; failure deletes the object and returns `nullptr`; zero, oversized, and non-depth fixtures fail explicitly with no upload and no leaked texture object |
| T-04-03 (Tampering, call sites) | mitigate | Repo-wide search shows zero `BindFramebufferHandle(` call sites outside the device; framebuffer-then-viewport order and the restore comment preserved; neutral-restore tests for RenderTarget, CommandBuffer, DeferredGeometryPass, RenderSystem, ShadowMapPass, and UIRenderer; human visual sign-off still required for leak coverage |
| T-04-04 (Spoofing, depth-texture cache identity) | mitigate | Plan 01-02 triples reused unchanged; ownership documented as caller-owned with sharing through `ResourceRegistry::Textures()`; `ResourceHandles`, `ResourceManifest`, and `ResourceRegistry` green; no cached `TextureResource` mutated |
| T-04-SC (package installs) | accept | No package installs; `vendor/glad` is the sole runtime library — legitimacy gate not applicable |

## Next Phase Readiness

- FRZ-01, FRZ-02, FRZ-03, and FRZ-04 are complete; FRZ-05 is code- and test-complete. Phase 01 has no remaining plan.
- **Blocker for closing the phase:** the D-04 human visual inspection (Task 4). `docs/ROADMAP.md` forbids tagging a pre-release before the P0 Windows runtime verification checklist passes, and the deferred `glObjectLabel` defect is exactly the kind of real-hardware-only crash that checklist exists to catch.
- The stale `build/gcc-debug-tests` tree on the full `D:` drive should be deleted or rebuilt.

---
*Phase: 01-p0-core-freeze*
*Completed: 2026-09-25*

## Self-Check: PASSED

- All 24 files listed in `key-files` (23 modified plus `Framebuffer.hpp` created) plus `deferred-items.md` verified present on disk.
- Task commits `3e66bac`, `2843b37`, and `513fa72` verified present via `git log --oneline --all`.
- All Task 1-3 acceptance criteria re-verified against the committed code by source assertion and by a real test run: no backend includes in `Framebuffer.hpp`; `OpenGLFramebuffer` implements the interface; both stub strings absent tree-wide; six linkage pins present; zero `BindFramebufferHandle(` call sites outside the device; `CreateDepthTarget` routes through `OpenGLTexture2D`; docs and CHANGELOG entries present; `ctest --preset test-gcc-debug` 58/58 green.
- Plan-level verification re-run in this session against the committed tree: debug build zero errors, 58/58 tests passing, both example binaries produced.
- Task 4 is recorded as **not performed**; no visual verification is claimed.
