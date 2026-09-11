# Phase 1: P0 Core Freeze - Context

**Gathered:** 2026-09-11
**Status:** Ready for planning

## Phase Boundary

Phase 1 freezes engine correctness: every P0 stub (FRZ-01…FRZ-05) is either implemented with real behavior and tests, or explicitly removed with documented rationale. Nothing on the agent path may silently misrepresent support when this phase closes. New capabilities belong in other phases — this phase only finishes or removes what exists.

## Implementation Decisions

### Compute dispatch fate (FRZ-01)

- **D-01:** Implement-vs-remove is delegated to researcher/planner discretion — **Reversibility:** costly — removing `Dispatch` from the command model touches the command-model contract and `Shader::CompileCompute`/`DispatchCompute` surface; re-adding later re-opens that contract
- Bounding facts the decision MUST respect: OpenGL 3.3 core has no compute shaders (`glDispatchCompute` needs 4.3+), so a real implementation cannot ride the 3.3 baseline; the no-silent-no-op rule (PROJECT.md Constraints) forbids keeping the current log-and-drop behavior — the outcome is implement-behind-a-higher-baseline, remove, or keep-reserved-with-explicit-failure, never silent drop.

### Occlusion culling fate (FRZ-03)

- **D-02:** Implement-vs-remove is delegated to researcher/planner discretion — **Reversibility:** costly — removing `SceneManager::m_occlusionCullingEnabled` deletes a public setting; re-adding it later is a new API decision
- Bounding facts: normalized frustum planes plus bounds-aware octree pruning already carry visibility; if kept, the technique needs observable effect plus `Tests/` coverage (software Hi-Z or GPU queries per `docs/ROADMAP.md`); if removed, removal must fail explicitly like depth-target creation does, never silently.

### Texture, depth, and framebuffer completion (FRZ-04, FRZ-05)

- **D-03:** Map-all-vs-prune and depth-via-texture-interface-vs-explicit-failure are delegated to researcher/planner discretion — **Reversibility:** one-way — deleting `TextureFormat` enum values breaks the published texture contract; re-adding values is a contract migration
- Bounding facts: no unmapped enum value may remain reachable from agent-visible API; `ITexture2D::CreateDepthTarget()` currently fails explicitly with a pointer to `OpenGLFramebuffer` — either complete creation through the texture interface or keep the explicit-failure path with test coverage; backend-neutral `BindFramebuffer` (`IGraphicsDevice`) must become trustworthy, with `RenderSystem`/`RenderPass` routed through it.

### Hardware verification scope

- **D-04:** Code-only-vs-full-hardware-verification is delegated to researcher/planner discretion
- Bounding facts: `docs/ROADMAP.md` P0 lists Windows runtime verification (clean Debug + Release CI on the real host, both examples on a supported GPU/driver, resize/minimize/restore/visibility/close/shutdown checks, OpenGL error + screenshot capture) and forbids tagging a pre-release before it passes; the phase MUST at minimum leave every code change covered by `Tests/` (fail-visibly culture), with renderer changes flagged for human visual inspection since smoke tests are not pixel validation.

### the agent's Discretion

All four areas above were explicitly delegated ("You decide" on every question). The researcher owns implement-vs-remove recommendations with evidence; the planner owns task-level approach. The bounding facts under each decision are hard constraints, not suggestions.

## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase scope and requirements

- `.planning/ROADMAP.md` §Phase 1 — goal, requirements FRZ-01…FRZ-05, success criteria, `**Mode:** mvp`
- `.planning/REQUIREMENTS.md` §P0 Core Freeze — FRZ-01…FRZ-05 requirement text
- `.planning/PROJECT.md` §Constraints — no silent no-op defaults; OpenGL 3.3 core baseline; `Engine/` owns only Core, Graphics, Win32/WGL Platform
- `docs/ROADMAP.md` §P0 — the authoritative stub list (compute dispatch, neutral framebuffer binding, shadow-map-array, texture-format mapping, occlusion decision) plus Windows runtime verification checklist and pre-release exit criteria

### Architecture and ownership rules

- `docs/Architecture.md` — renderer passes, resource ownership, what frustum (implemented) vs occlusion (not) means
- `AGENTS.md` — ownership boundaries, test culture (fail visibly, malformed fixtures, no false-success skips), `Tests/PublicApiLinkage.cpp` for new symbols

### Codebase maps (fresh 2026-09-05)

- `.planning/codebase/CONCERNS.md` — exact file paths for all five stubs plus fragile-area contracts (framebuffer/viewport restore convention, cache immutability, handle generations)

## Existing Code Insights

### Reusable Assets

- Explicit-failure precedent: `Engine/Graphics/source/Texture.cpp` depth-target failure message — the pattern to copy for any kept-reserved API
- `Tests/FramebufferResizeTests.cpp`, `Tests/UIRendererTests.cpp`, `Tests/TextureLoadingTests.cpp` — existing suites to extend for binding/format tests
- `OpenGLShader::DispatchCompute()` in `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp` — exists but unreachable; either wire or remove with it
- `ShadowMapPass` (N cascades) + `DeferredLightingPass::SetShadowMaps()` (binds first only) in `Engine/Graphics/source/Renderer/` — the array-binding gap is localized here

### Established Patterns

- Transactional publication with rollback (OBJ imports, texture reload, scene serialization) — any new creation path (depth textures, converter output) follows the same apply-or-rollback shape
- Content-addressed immutable caches — do not bypass with parallel uploads while touching texture/material code
- After-pass framebuffer + viewport restore convention (`RenderSystem.cpp`, `UIRenderer.cpp`) — any pass touched by this phase must preserve it, with a restoration test

### Integration Points

- `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` (`IGraphicsDevice`) — the backend-neutral boundary all binding work routes through; no WGL/GLAD leakage into public headers
- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` (command execution) — where `Dispatch` is dropped today
- `Tests/PublicApiLinkage.cpp` — every new or removed public symbol must be reflected here

## Specific Ideas

No specific requirements — open to standard approaches within the bounding facts above.

## Deferred Ideas

None — discussion stayed within phase scope.

---

*Phase: 1-P0 Core Freeze*
*Context gathered: 2026-09-11*
