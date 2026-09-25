# Phase 1: P0 Core Freeze - Research

**Researched:** 2026-09-11
**Domain:** Brownfield C++17 / OpenGL 3.3 correctness freeze (command model, shadow binding, culling, texture formats, framebuffer abstraction)
**Confidence:** HIGH (in-repo stubs read line-by-line this session; GL version facts checked against Khronos registry)

## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01 (FRZ-01):** Implement-vs-remove is delegated to researcher/planner discretion — **Reversibility:** costly — removing `Dispatch` from the command model touches the command-model contract and `Shader::CompileCompute`/`DispatchCompute` surface; re-adding later re-opens that contract
- Bounding facts the decision MUST respect: OpenGL 3.3 core has no compute shaders (`glDispatchCompute` needs 4.3+), so a real implementation cannot ride the 3.3 baseline; the no-silent-no-op rule (PROJECT.md Constraints) forbids keeping the current log-and-drop behavior — the outcome is implement-behind-a-higher-baseline, remove, or keep-reserved-with-explicit-failure, never silent drop.
- **D-02 (FRZ-03):** Implement-vs-remove is delegated to researcher/planner discretion — **Reversibility:** costly — removing `SceneManager::m_occlusionCullingEnabled` deletes a public setting; re-adding it later is a new API decision
- Bounding facts: normalized frustum planes plus bounds-aware octree pruning already carry visibility; if kept, the technique needs observable effect plus `Tests/` coverage (software Hi-Z or GPU queries per `docs/ROADMAP.md`); if removed, removal must fail explicitly like depth-target creation does, never silently.
- **D-03 (FRZ-04, FRZ-05):** Map-all-vs-prune and depth-via-texture-interface-vs-explicit-failure are delegated to researcher/planner discretion — **Reversibility:** one-way — deleting `TextureFormat` enum values breaks the published texture contract; re-adding values is a contract migration
- Bounding facts: no unmapped enum value may remain reachable from agent-visible API; `ITexture2D::CreateDepthTarget()` currently fails explicitly with a pointer to `OpenGLFramebuffer` — either complete creation through the texture interface or keep the explicit-failure path with test coverage; backend-neutral `BindFramebuffer` (`IGraphicsDevice`) must become trustworthy, with `RenderSystem`/`RenderPass` routed through it.
- **D-04 (hardware verification):** Code-only-vs-full-hardware-verification is delegated to researcher/planner discretion
- Bounding facts: `docs/ROADMAP.md` P0 lists Windows runtime verification (clean Debug + Release CI on the real host, both examples on a supported GPU/driver, resize/minimize/restore/visibility/close/shutdown checks, OpenGL error + screenshot capture) and forbids tagging a pre-release before it passes; the phase MUST at minimum leave every code change covered by `Tests/` (fail-visibly culture), with renderer changes flagged for human visual inspection since smoke tests are not pixel validation.

### the agent's Discretion

All four areas above were explicitly delegated ("You decide" on every question). The researcher owns implement-vs-remove recommendations with evidence; the planner owns task-level approach. The bounding facts under each decision are hard constraints, not suggestions.

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.

## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| FRZ-01 | Agent builds never hit a log-only compute-dispatch stub — dispatch is implemented or removed from the command model | §FRZ-01: Khronos version table proves 4.3+ requirement; in-repo drop site + unreachable impl located; primary recommendation REMOVE with exact symbol list |
| FRZ-02 | Deferred shadow-map-array binding is complete (or explicitly removed with documented rationale) | §FRZ-02: shader side already correct GLSL 3.30 `sampler2DArray`; CPU single-bind gap located; 3.3-core array-texture fix path with GLAD entry points verified present |
| FRZ-03 | Occlusion culling uses a supported technique or the setting is removed (no placeholder flag) | §FRZ-03: no-op `OcclusionCull` located; GPU-query technique verified 3.3-available but architecturally mismatched to `SceneManager`; primary recommendation REMOVE with rationale |
| FRZ-04 | Every advertised texture format is mapped, or unsupported enum values are removed | §FRZ-04: 21-value enum quoted verbatim; only 4 mapped; per-format 3.3-core mapping table with prune list (BC7) and gate list (S3TC) |
| FRZ-05 | Backend-neutral framebuffer binding is complete, including depth-texture creation through the texture interface | §FRZ-05: `IFramebuffer` proven undefined (forward-decl only); stub error string quoted; `IFramebuffer` definition + routing plan; depth-target creation path |

## Summary

Phase 1 is a pure contract-honesty phase: five places where the engine advertises more than it does. All five gaps were read in source this session and all five are fixable on the locked OpenGL 3.3 baseline with zero new dependencies — except FRZ-01 compute, which is *unimplementable* on 3.3 by Khronos specification and must therefore be removed (or kept reserved with an explicit failure, never the current silent drop).

The highest-leverage finding is that the shaders are already written for the fixed state: both `forward.frag` and `deferred_lighting.frag` declare `uniform sampler2DArray u_ShadowMaps` with the overloaded `texture()` sampler call that is correct GLSL 3.30, while the C++ side binds a single `GL_TEXTURE_2D` — a sampler-type/texture-target mismatch that makes multi-cascade shadows silently wrong. The fix is CPU-side only (one `GL_TEXTURE_2D_ARRAY` depth texture, cascade layers, single bind), using GLAD entry points verified present in the vendored headers. Similarly, GPU occlusion queries (`glGenQueries`/`glBeginQuery`/`GL_ANY_SAMPLES_PASSED`) are verified present in GLAD and core since OpenGL 1.5, so a keep-occlusion option is technically viable — but `SceneManager` has no device access and its query API is synchronous per-frame, so a *correct* async implementation is a multi-hundred-line subsystem, not a freeze task. Removal is the proportionate freeze outcome.

For textures, the mapping work is mechanical but wide: 4 of 21 enum values map today; depth, float, and single-channel formats are all 3.3-core and mappable; S3TC-compressed BC1/BC3 need a runtime extension check (GLAD even ships the `GLAD_GL_EXT_texture_compression_s3tc` flag); BC7 needs OpenGL 4.2 and must be pruned or hard-failed. The upload path hardcodes `GL_UNSIGNED_BYTE`, so every non-8-bit mapping also requires a type-correct `glTexImage2D` call — the plan must include that or the new mappings will upload garbage.

**Primary recommendation:** Remove `Dispatch`/`CompileCompute`/`DispatchCompute` and the occlusion-culling setting outright (compile-time signal beats runtime failure); implement shadow-map-array binding via a single `GL_TEXTURE_2D_ARRAY` depth texture; map all 3.3-core texture formats + implement `CreateDepthTarget` through the texture interface while pruning BC7 and gating S3TC behind a runtime extension check; define the missing `IFramebuffer` interface and route all binding through it. Keep hardware verification at the pre-release gate, not inside the phase — every change gets `Tests/` coverage plus a human-visual-inspection flag.

## Architectural Responsibility Map

| Capability | Primary Tier | Secondary Tier | Rationale |
|------------|-------------|----------------|-----------|
| Compute dispatch contract | API / Backend (`IGraphicsDevice`, `IShader`, `CommandBuffer`) | — | The command model is the backend-neutral contract agents program against; the fix is deleting surface, not GPU work |
| Shadow-map-array binding | API / Backend (`DeferredLightingPass`, `ShadowMapPass`, GL texture objects) | — | Pure renderer-internal resource wiring; shaders already expect the array |
| Occlusion-culling decision | API / Backend (`SceneManager`) | — | The flag lives on the scene manager; removal is a header+cpp deletion with linkage-test updates |
| Texture-format mapping | API / Backend (`OpenGLTexture2D`, `TextureResource`/`TextureCache`) | — | Format resolution + upload live in the GL texture backend and the content-addressed cache layer |
| Framebuffer binding + depth targets | API / Backend (`IGraphicsDevice`, `OpenGLFramebuffer`, `RenderSystem`/`RenderPass`) | — | Interface definition plus routing existing call sites off raw `BindFramebufferHandle(u32)` |
| Verification of all five | Tests (`Tests/*.cpp` via CTest) + human visual inspection | — | Fail-visibly culture; smoke tests are not pixel validation per AGENTS.md |

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| OpenGL 3.3 core / GLSL 3.30 | Locked baseline [VERIFIED: AGENTS.md] | Rendering API floor all fixes must run on | PROJECT.md constraint — shifting it needs explicit discussion; context negotiation tries 4.6→3.3 and hard-fails below 3.3 [VERIFIED: Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:616-617,682] |
| `vendor/glad` | Pinned in-tree loader, sole approved runtime lib [VERIFIED: AGENTS.md] | All GL entry points (`glTexImage3D`, `glFramebufferTextureLayer`, `glBlitFramebuffer`, `glGenQueries`, `glBeginQuery`, `glGetStringi`, `GL_TEXTURE_2D_ARRAY`, `GL_ANY_SAMPLES_PASSED`, `GL_TEXTURE_COMPARE_MODE`, S3TC tokens + `GLAD_GL_EXT_texture_compression_s3tc` flag) verified present in headers [VERIFIED: vendor/glad/include/glad/glad.h:1652,3100-3101,3400-3410,1429,1989,3991-3998,3937-3938,6291-6293,13277-13279] | No new loader, no extensions lib, no package-manager dependency permitted |
| CTest (one binary per `Tests/*.cpp`, `add_test(NAME <Suite>.<Name>)`) | CMake 4.3.2 / CTest 4.3.2 present on this machine [VERIFIED: tool probe this session] | Verification harness for every FRZ change | Established pattern; `FramebufferResizeTests.cpp` demonstrates headless fake-GL stubs so GL-adjacent logic is testable without a GPU |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| `PYRAMID_LOG_*` diagnostics | In-tree (`Pyramid::Foundation`) | Explicit-failure messages replacing silent stubs | Every kept-reserved API and every removal pointer (copy the `Texture.cpp` depth-target message shape) |
| `OpenGLDiagnostics::CheckError` / `ClearErrors` | In-tree | GL error capture after every new GL call sequence | After array-texture creation, per-layer attach/blit, format-mapping uploads, query setup |
| `OpenGLStateManager` singleton | In-tree | Canonical GL state application point (`BindFramebuffer`, `UseProgram`, scissor/viewport) | `OpenGLDevice::BindFramebuffer` implementation and any new bind path must go through it, not raw GL |
| Transactional create-then-swap | Established codebase pattern (framebuffer `Resize`, texture reload, `ModelResourceImporter`) | Any new creation path (depth textures, array texture, converted formats) | Apply-or-rollback: validate replacement first, swap into service only on success, preserve previous object on failure |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Remove `Dispatch` (recommended) | Keep reserved with explicit failure (`PYRAMID_LOG_ERROR` + no-op returning failure, like `CreateDepthTarget` today) | Keeps API surface stable for a future 4.3 backend, but preserves dead surface agents must discover by trial; removal gives a compile-time signal which is strictly more agent-legible. Keep-reserved is acceptable only if the planner judges the command-model churn too risky for an MVP freeze. |
| Remove occlusion setting (recommended) | Implement GPU `GL_ANY_SAMPLES_PASSED` queries with frame-delayed results | Queries are 3.3-available, but correct use needs async result handling (never wait same-frame), bounding-box proxy rendering, and device access `SceneManager` lacks — a new subsystem, contradicting "no new capabilities". Implement only if the planner can scope it to a tracer (e.g., boolean query on already-rendered proxies) with `Tests/` coverage. Software Hi-Z is explicitly the worse option: a hand-rolled CPU rasterizer duplicating what the driver does for free. |
| Map all 3.3-core formats (recommended) | Prune the enum to the 4 mapped values | Pruning is a one-way contract break (CONTEXT D-03) that deletes HDR/depth/single-channel expressiveness the deferred pipeline (RGBA16F G-buffer) already relies on conceptually. Map-first, prune-only-BC7, gate-S3TC. |
| Code+CI in phase, HW at pre-release gate (recommended) | Full hardware verification inside Phase 1 | The P0 HW checklist (both examples on GPU, resize/minimize/restore, screenshots) gates the *tag*, not the freeze; blocking code tasks on GPU access serializes the phase. Planner: add a `checkpoint:human-verify` visual task per renderer-affecting plan instead. |

**Installation:**
```bash
# No new packages. This phase installs nothing: vendor/glad is in-tree,
# all fixes use core OpenGL 3.3 entry points already exposed by the loader.
```

## Package Legitimacy Audit

No external packages are installed or recommended by this research. The only runtime library in play is the vendored `vendor/glad` loader, which is the AGENTS.md-approved sole bundled third-party runtime library — not a registry dependency, so the package-legitimacy seam does not apply (no ecosystem, no version to `npm view`/`pip index`/`cargo search`). The `gsd-tools query classify-confidence` seam was probed this session and is not implemented in the installed toolchain (`Unknown command: classify-confidence`), so provenance below follows the contract tiers directly: `[VERIFIED: <file>:<lines>]` = read this session, `[CITED: <url>]` = official Khronos documentation, `[ASSUMED]` = training knowledge needing confirmation.

**Packages removed due to SLOP verdict:** none.
**Packages flagged as suspicious (SUS):** none.

## Architecture Patterns

### System Architecture Diagram

```
Agent code / Examples
        │  ITexture2D::Create* · IGraphicsDevice::* · CommandBuffer::*
        ▼
┌───────────────────────── Backend-neutral boundary ─────────────────────────┐
│ IGraphicsDevice · IShader · ITexture2D · RenderCommandType · RenderTarget   │
│ NEW this phase: IFramebuffer (defined) · mapped TextureFormat set          │
└─────────────────────────────────────────────────────────────────────────────┘
        │                                                  │
        ▼                                                  ▼
┌─ OpenGL backend ────────────────────┐   ┌─ Renderer passes ────────────────┐
│ OpenGLDevice (BindFramebuffer fixed)│   │ ShadowMapPass: N depth FBOs ─┐   │
│ OpenGLTexture2D (full format map +  │   │   → 1 GL_TEXTURE_2D_ARRAY    │   │
│   depth-target creation)            │   │ DeferredLightingPass: binds  │   │
│ OpenGLFramebuffer (IFramebuffer     │◄──│   array once, sets matrices/ │   │
│   impl; layered attach capable)     │   │   splits/count               │   │
│ OpenGLStateManager (all binds)      │   │ ForwardRenderPass: unchanged │   │
└─────────────────────────────────────┘   │ RenderSystem: restore via    │   │
                                          │   IGraphicsDevice (not raw   │   │
        ┌─ Scene ────────────────────┐    │   handle calls)              │   │
        │ SceneManager MINUS occlusion│    └──────────────────────────────┘
        │ Frustum + octree (unchanged │
        │ authority for visibility)   │
        └─────────────────────────────┘
        │  GetVisibleObjects / proxies / SceneSerializer v2 (untouched)
        ▼
Tests/ (CTest, fake-GL pattern) + human visual inspection flag
```

A reader traces the primary use case (deferred shadows) as: `RenderSystem::Render` → `ShadowMapPass::Execute` renders N cascades into array layers → `DeferredLightingPass::Execute` binds the single array texture + uploads `u_LightSpaceMatrices`/`u_CascadeSplits`/`u_CascadeCount` → shader samples `sampler2DArray` → `RenderSystem` restores main surface through the neutral interface.

### Recommended Project Structure

Brownfield phase — no new top-level structure. All changes land in existing files:

```
Engine/Graphics/include/Pyramid/Graphics/
├── GraphicsDevice.hpp        # uses new IFramebuffer (defined, not forward-decl)
├── Framebuffer.hpp           # NEW: IFramebuffer interface (or inside GraphicsDevice.hpp)
├── Texture.hpp               # pruned/gated TextureFormat set
└── Renderer/RenderSystem.hpp # RenderCommandType minus Dispatch (if removed)
Engine/Graphics/source/
├── Renderer/CommandBuffer.cpp
├── Renderer/DeferredLightingPass.cpp
├── Renderer/ShadowMapPass.cpp
├── Renderer/RenderSystem.cpp
├── OpenGL/OpenGLDevice.cpp
├── OpenGL/OpenGLTexture.cpp
├── Texture.cpp
└── Scene/SceneManager.cpp
Tests/
├── PublicApiLinkage.cpp      # every added/removed symbol reflected here
├── FramebufferResizeTests.cpp# extend (neutral binding, restore, array creation)
├── TextureLoadingTests.cpp   # extend (per-format mapping, depth targets)
└── <new> ShadowArrayTests / OcclusionRemovalTests (Wave 0 gaps)
```

### Pattern 1: Explicit failure, never silent drop (the phase's master pattern)

**What:** Any capability that cannot be honored on the 3.3 baseline logs `PYRAMID_LOG_ERROR` with a corrective pointer and returns a failure value — the established shape is:
```cpp
// Source: Engine/Graphics/source/Texture.cpp:38-45 [VERIFIED]
std::shared_ptr<ITexture2D> ITexture2D::CreateDepthTarget(u32 width, u32 height, TextureFormat format)
{
    (void)width;
    (void)height;
    (void)format;
    PYRAMID_LOG_ERROR("Depth texture creation is not implemented by OpenGLTexture2D; use OpenGLFramebuffer");
    return nullptr;
}
```
**When to use:** For any kept-reserved API (if the planner keeps rather than removes compute/occlusion/compressed formats), and as the template message shape for pruned-format diagnostics.
**Removal variant:** Deleting the symbol entirely is stronger than this pattern (compile-time > runtime signal). Prefer deletion for `Dispatch` and the occlusion setter; use this pattern for driver-dependent cases (S3TC without extension) where compile-time signaling is impossible.

### Pattern 2: Transactional creation with rollback

**What:** Validate/build the replacement object fully, then swap into service; on any failure preserve the previous object and report. Precedents: `OpenGLFramebuffer::Resize` (replacement-first, old stays valid), `TextureCache::Reload` (decode + GPU create before remap), `ModelResourceImporter` (rollback removes only non-canonical aliases).
**When to use:** `CreateDepthTarget` implementation, array-texture (re)creation on cascade-count/resolution change, every new format-mapping upload path. Tests must assert the old object survives a failed creation (mirror the existing resize/reload negative tests).

### Pattern 3: After-pass surface restore through the neutral interface

**What:** `RenderSystem::Render` re-establishes the main surface after every pass:
```cpp
// Source: Engine/Graphics/source/Renderer/RenderSystem.cpp:300-304 [VERIFIED]
m_device->BindFramebufferHandle(0);
m_device->SetViewport(0, 0, m_width, m_height);
```
**When to use:** Unchanged as a convention, but FRZ-05 must migrate these two lines (and the `ShadowMapPass`/`DeferredGeometryPass` raw-handle binds) onto the new neutral `BindFramebuffer(IFramebuffer*)` / `SetViewport` path so the restore contract is backend-portable. Any touched pass keeps the convention and keeps a restoration test (`Tests/UIRendererTests.cpp`, `Tests/FramebufferResizeTests.cpp` pattern).

### Pattern 4: Immutable content-addressed resources

**What:** `TextureResource`/`Material`/`ShaderProgram` are immutable after publication; identity includes exact bytes, sampler state, and color space; changes publish via transactional reload/replace; per-draw data goes in command-buffer uniforms, never in resource identity.
**When to use:** New depth-texture and mapped-format resources must be acquired/published through `ResourceRegistry::Textures()` and friends — no parallel upload ownership, no in-place mutation of cached instances (see AGENTS.md constraints).

### Anti-Patterns to Avoid

- **Log-and-drop commands:** the exact FRZ-01 sin (`PYRAMID_LOG_DEBUG` + `break` in `CommandBuffer::Execute`). Any new unhandled command type must be a loud error or a compile-time absence. [VERIFIED: Engine/Graphics/source/Renderer/CommandBuffer.cpp:560-563]
- **Sampler/target mismatch:** binding `GL_TEXTURE_2D` where the shader declares `sampler2DArray` (current FRZ-02 state). After the fix, assert at bind time that the bound target matches the sampler type; GL error checks catch the rest. [VERIFIED: Engine/Graphics/source/Renderer/DeferredLightingPass.cpp:133-141]
- **Raw-handle binding leaking past the device boundary:** `BindFramebufferHandle(u32)` calls inside passes are OpenGL-isms. Keep the method for internal use if needed, but all pass/system code the agent path touches routes through `IFramebuffer`. [VERIFIED: Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:368-380]
- **Hardcoded `GL_UNSIGNED_BYTE` uploads for non-8-bit formats:** the current `CreateTextureObject`/`SetData` assume byte data [VERIFIED: Engine/Graphics/source/OpenGL/OpenGLTexture.cpp:414-423 and :214-223]. Every float/depth mapping must pair its internal format with the correct `type` argument, or uploads will silently reinterpret bytes.
- **Reintroducing parallel upload ownership** for shadow/depth/format work instead of going through the mesh/texture/material caches and `ModelResourceImporter` (AGENTS.md ownership rules).

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| GPU compute on the 3.3 baseline | CPU emulation of `Dispatch`, or a "compute-like" command shim | Remove the API; revisit only with a 4.3+ backend decision | `glDispatchCompute` does not exist below 4.3 [CITED: Khronos ref pages]; a shim would be the next silent-misrepresentation stub. GLAD exposes the pointer but it is NULL on 3.3 contexts — calling it crashes [VERIFIED: vendor/glad/include/glad/glad.h:8826-8827 + dynamic `load("glDispatchCompute")` in glad.c:5543] |
| Occlusion visibility | CPU software rasterizer / Hi-Z pyramid in `SceneManager` | Delete the flag now; later, GPU `GL_ANY_SAMPLES_PASSED` queries behind the device interface | Driver already rasterizes; a hand-rolled Hi-Z duplicates it, needs its own depth pyramid, tests, and tuning — a feature, not a freeze task. Queries are core since 1.5 [CITED: Khronos ARB_occlusion_query2 spec] with loader support verified [VERIFIED: glad.h:3400-3410,1989] |
| Cascade packing | Manual atlas-packing of N shadow maps into one 2D texture with custom UV math | Single `GL_TEXTURE_2D_ARRAY` depth texture (`glTexImage3D`, layers = cascade count) | Array textures are 3.0-core with `GL_MAX_ARRAY_TEXTURE_LAYERS` ≥ 256 [CITED: Khronos wiki]; atlas packing breaks the shaders' `textureSize(u_ShadowMaps,0)`-based PCF texel math and bleeds filtering across cascade borders |
| Texture compression | Owned BCn encoder/decoder | Driver `EXT_texture_compression_s3tc` path with runtime flag check (`GLAD_GL_EXT_texture_compression_s3tc`), prune BC7 | BPTC/BC7 is core only in 4.2 [CITED: ARB_texture_compression_bptc]; S3TC upload of pre-compressed blocks is a format mapping, not a codec — no image-parsing code involved |
| Depth sampling | Custom depth-pack/unpack shaders | Depth-component texture formats + `GL_TEXTURE_COMPARE_MODE` where shadow comparison is wanted | Core 3.3 depth texturing (§3.8.9) with compare-mode tokens already in the loader [VERIFIED: glad.h:1429]; hand packing loses hardware PCF and compare filtering |

**Key insight:** Every FRZ item tempts a clever middle layer (shim, software fallback, atlas, custom codec). On a locked 3.3 baseline with a no-silent-no-op rule, the correct move is always the boring one: use the core GL feature that already exists, or delete the surface that has none.

## FRZ-01: Compute Dispatch — implement or remove

**Current state (all read this session):**
- `CommandBuffer::Dispatch(u32,u32,u32)` records `RenderCommandType::Dispatch` [VERIFIED: Engine/Graphics/source/Renderer/CommandBuffer.cpp:259-269]; `Execute` drops it with a debug log and a "will be handled" comment [VERIFIED: same file :560-563, quoted in Patterns].
- `IShader::CompileCompute` / `DispatchCompute` are required interface methods [VERIFIED: Engine/Graphics/include/Pyramid/Graphics/Shader/Shader.hpp:67-80]; `OpenGLShader` implements both with a real `glDispatchCompute` + `glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT)` [VERIFIED: Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp:558-573] — but nothing on the render-system path ever calls it.
- `glDispatchCompute` is OpenGL 4.3+: the Khronos version-support table shows ✔ only for 4.3/4.4/4.5(/4.6), `—` for 3.3 and below [CITED: https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDispatchCompute.xhtml]. The engine's context negotiation accepts 4.6 down to a 3.3 minimum and guarantees only 3.3 [VERIFIED: Win32OpenGLWindow.cpp:616-617,682].
- Therefore a "real implementation" would execute only when the driver happens to hand back ≥4.3, while the declared baseline (AGENTS.md/PROJECT.md: OpenGL 3.3 core) cannot run it — that is exactly the shifting-baseline outcome CONTEXT forbids without explicit discussion.

**Recommendation (owned): REMOVE.** Delete `CommandBuffer::Dispatch`, `RenderCommandType::Dispatch` (+ its `dispatch` union member), `IShader::CompileCompute`/`DispatchCompute` (+ SSBO helpers only if compute-exclusive — `BindShaderStorageBuffer`/`SetShaderStorageBlockBinding` and `CreateShaderStorageBuffer` are also compute-adjacent; planner to audit whether anything non-compute uses them, default to removing the compute-only surface and keeping generic buffer objects), and the `OpenGLShader` implementations. Update `Tests/PublicApiLinkage.cpp` to drop the symbols (linkage test fails loudly on drift — that is the removal's own verification). Document rationale in `docs/ROADMAP.md` P0 (one line: compute requires 4.3+, baseline is 3.3, re-add with a higher-baseline backend decision). Fallback the planner may choose: keep-reserved-with-explicit-failure (`PYRAMID_LOG_ERROR` + documented failure return), but removal is strictly better for agents.

**Executor pitfalls:** `RenderCommand` is a union — removing the `dispatch` member is safe, but keep initializer discipline (`RenderCommand cmd{}` vs unbraced) consistent; `GetDrawStats` ignores non-draw commands so stats are unaffected; check `CommandBufferStatsTests.cpp` still passes unchanged.

## FRZ-02: Deferred shadow-map-array binding

**Current state:**
- Both shaders already expect an array: `uniform sampler2DArray u_ShadowMaps;` with `u_LightSpaceMatrices[4]`, `u_CascadeSplits[5]`, `u_CascadeCount` [VERIFIED: Engine/Graphics/shaders/deferred_lighting.frag:12-16 and Engine/Graphics/shaders/forward.frag:14-20], sampled via the overloaded `texture(u_ShadowMaps, vec3(uv, cascadeIndex))` with `textureSize(u_ShadowMaps, 0)` PCF texel sizing [VERIFIED: deferred_lighting.frag:96-99]. `sampler2DArray` + `texture()` overload is correct GLSL 3.30 (array samplers core since GLSL 1.30/GL 3.0; legacy `texture2DArray` never existed in core) [CITED: Khronos forums + EXT_texture_array + wiki Array Texture].
- CPU side binds only cascade 0 as a plain 2D texture with an explicit TODO [VERIFIED: DeferredLightingPass.cpp:133-141]:
```cpp
// For now, bind first shadow map cascade
// TODO: Implement shadow map array binding
GLuint shadowMap = m_shadowMaps[0]->GetDepthAttachmentTexture();
m_device->BindNativeTexture(shadowMap, 5, GL_TEXTURE_2D);
m_lightingShader->SetUniformInt("u_ShadowMaps", 5);
```
- Additionally, the deferred `Execute` body uploads camera/light/bias/count/SSAO/IBL uniforms but never uploads `u_LightSpaceMatrices` or `u_CascadeSplits`, which the shadow math indexes per-cascade [VERIFIED: full read of DeferredLightingPass.cpp:100-180 — matrices come from `ShadowMapPass::GetLightSpaceMatrices()` and splits from its `m_cascadeSplits`, but no `SetShadowMaps`-companion setter or upload call exists in the pass].
- `ShadowMapPass` renders each cascade into its own depth-only `GL_DEPTH_COMPONENT24` framebuffer at fixed 2048 (intentionally window-size-independent) [VERIFIED: ShadowMapPass.cpp:61-100, and `RenderSystem::SetupDefaultRenderPasses`/`SetupDeferredPipeline` wire `shadowPass->GetShadowMaps()` into the lighting pass :502-518].

**Recommendation (owned): IMPLEMENT on 3.3 core.** (a) Own one `GL_TEXTURE_2D_ARRAY` depth texture (2048×2048×N, `GL_DEPTH_COMPONENT24`, `GL_NEAREST`, `GL_CLAMP_TO_BORDER` white, matching current per-cascade parameters) created via `glTexImage3D` — all entry points/tokens verified in GLAD [VERIFIED: glad.h:1652,3100-3101]. (b) Populate layers per frame: preferred is rendering each cascade directly into its layer via `glFramebufferTextureLayer` (loader present [VERIFIED: glad.h:3997-3998]); fallback is keep-N-framebuffers + depth `glBlitFramebuffer` into layers (loader present [VERIFIED: glad.h:3991-3992]) — planner picks one, blit is lower-risk. (c) Bind once with target `GL_TEXTURE_2D_ARRAY` on the existing unit, upload `u_LightSpaceMatrices` (from `GetLightSpaceMatrices()`), `u_CascadeSplits` (+1 element), `u_CascadeCount`, keep `u_ShadowBias`. (d) Preserve the after-pass restore convention and keep shadow resolution independent of window size (intentional per Architecture.md). (e) Tests: array-completeness + layer-count unit test in the fake-GL style, a "all cascades bound exactly once" assertion, and uniform-upload presence; flag human visual inspection (side-by-side cascade transitions, acne/peter-panning check) since smoke tests are not pixel validation.
- Explicitly out: removing the array (shaders already standardize on it; forward+deferred share the convention — deleting means rewriting both shaders for single-cascade, a worse contract).

## FRZ-03: Occlusion culling — supported technique or remove setting

**Current state:**
- `bool m_occlusionCullingEnabled = false;` with inline setter `SetOcclusionCullingEnabled` [VERIFIED: Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp:165,194]; when enabled, `GetVisibleObjects` filters through `OcclusionCull`, which unconditionally returns `false` [VERIFIED: Engine/Graphics/source/Scene/SceneManager.cpp:299-308 and :541-547]:
```cpp
bool SceneManager::OcclusionCull(const std::shared_ptr<RenderObject> &object, const Camera &camera)
{
    (void)object;
    (void)camera;
    // Occlusion culling is not implemented yet.
    return false;
}
```
- Frustum culling (normalized planes, world-bounds tested) plus bounds-aware octree pruning already carry visibility [VERIFIED: SceneManager.cpp:280-287 paths; Architecture.md §Scene].
- A keep-implementation would use GPU occlusion queries: `ARB_occlusion_query` promoted to core OpenGL 1.5 [CITED: ARB_occlusion_query2 spec IP-status/history], boolean `ANY_SAMPLES_PASSED` form written against the 3.2 spec [CITED: same], loader support verified [VERIFIED: glad.h], standard pattern = depth pre-pass of occluders → color/depth-masked bounding-proxy queries → consume results a frame late, never wait same-frame (GPU Gems 1 Ch.29 / Gems 2 Ch.6 coherent-hierarchical-culling) [CITED: NVIDIA developer articles].

**Recommendation (owned): REMOVE.** Delete the setter, the member, the `OcclusionCull` declaration/definition, and the filter block; document in `docs/ROADMAP.md` + `docs/Architecture.md` (one line each: visibility = frustum + octree; occlusion deferred until a device-side query technique earns its own phase). Rationale: correct queries need async result plumbing, proxy geometry, and device access that `SceneManager` (a scene/spatial class) does not have — building that inside a freeze phase violates the "no new capabilities" boundary and risks a second, subtler misrepresentation (one-frame-late flicker, pipeline stalls from naive wait). Removal converts a silent lie into a compile-time signal, which satisfies "fail explicitly, never silently" in the strongest form. Reflect removals in `Tests/PublicApiLinkage.cpp` if the setter is referenced there (grep at plan time).

## FRZ-04: Texture-format mapping

**Current state:**
- The advertised enum has 21 values [VERIFIED: Engine/Graphics/include/Pyramid/Graphics/Texture.hpp:9-37 — quote]:
```cpp
enum class TextureFormat
{
    None = 0,
    // Standard 8-bit formats
    RGB8,
    RGBA8,
    SRGB8,
    SRGBA8,
    // High dynamic range formats
    RGB16F,
    RGBA16F,
    RGB32F,
    RGBA32F,
    // Depth/stencil formats
    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8,
    Depth32FStencil8,
    // Compressed formats
    BC1_RGB,     // DXT1
    BC1_RGBA,    // DXT1 with alpha
    BC3_RGBA,    // DXT5
    BC7_RGBA,    // High quality
    // Single channel formats
    R8,
    R16F,
    R32F
};
```
- `OpenGLTexture2D::ResolveFormats` maps exactly RGB8/SRGB8/RGBA8/SRGBA8 and returns `false` for everything else → constructor sets `"Unsupported OpenGLTexture2D format"` [VERIFIED: Engine/Graphics/source/OpenGL/OpenGLTexture.cpp:290-314, :52-61]. `TextureResource::ResolveBaseFormat` mirrors the same 4-format subset [VERIFIED: Engine/Graphics/source/Texture/TextureResource.cpp:98-115]. File loading only ever produces RGB8/RGBA8 [VERIFIED: OpenGLTexture.cpp:111-116].
- Uploads assume bytes: `glTexImage2D(..., dataFormat, GL_UNSIGNED_BYTE, data)` [VERIFIED: OpenGLTexture.cpp:414-423] and `SetData`/`glTexSubImage2D` likewise [VERIFIED: same file :214-223]; `CalculateByteSize` overflow guards exist and must be reused for wider types.

**Recommendation (owned): MAP everything 3.3-core; PRUNE BC7; GATE S3TC.** Per-format disposition (GL enum pairings are standard core-profile mappings [ASSUMED] — planner verifies each against `glad.h` tokens + spec tables 3.12/3.13/3.14 at implementation; GL-error-check tests catch mistakes):

| Format(s) | Disposition | Mapping direction [ASSUMED] |
|-----------|-------------|------------------------------|
| RGB16F, RGBA16F, RGB32F, RGBA32F | MAP (float color is 3.0-core) | `GL_RGB16F/GL_RGBA16F/GL_RGB32F/GL_RGBA32F` + `GL_FLOAT`, bytes 6/8/12/16 px; fix filtering (float linear filtering is core; mip generation allowed) |
| Depth16, Depth24, Depth32F | MAP (sized depth is 3.3-core) | `GL_DEPTH_COMPONENT16/24/32F` + `GL_UNSIGNED_SHORT/GL_UNSIGNED_INT/GL_FLOAT`; 1 "byte" concept becomes 2/4/4; no mipmaps; `GL_TEXTURE_COMPARE_MODE` left `NONE` for sampled depth, `COMPARE_REF_TO_TEXTURE` only where shadow comparison is wanted |
| Depth24Stencil8, Depth32FStencil8 | MAP (packed depth-stencil is core) | `GL_DEPTH24_STENCIL8` / `GL_DEPTH32F_STENCIL8` + `GL_DEPTH_STENCIL`/`GL_UNSIGNED_INT_24_8` (and float-32 variant); sampled access returns depth in R per ARB_stencil_texturing-era rules — document the sampling behavior explicitly |
| R8, R16F, R32F | MAP (R-only is 3.0-core via texture-RG) | `GL_R8/GL_R16F/GL_R32F` + `GL_RED` + byte/half/float; single channel reads as `(R,0,0,1)` — document, since agents may expect RGB |
| BC1_RGB, BC1_RGBA, BC3_RGBA | GATE behind runtime `GLAD_GL_EXT_texture_compression_s3tc` check [VERIFIED flag exists: glad.h:13277-13279; S3TC tokens present :6291-6293] | Upload pre-compressed blocks via `glCompressedTexImage2D`; when the extension is absent → explicit `PYRAMID_LOG_ERROR` + failure (the sanctioned runtime-failure case, with test forcing the absent path) |
| BC7_RGBA | REMOVE from the enum (one-way contract break — note in CHANGELOG + docs) | BPTC went core in OpenGL 4.2 [CITED: ARB_texture_compression_bptc]; unsupportable on the 3.3 baseline |
| None | Keep as sentinel; every public entry point rejects it explicitly | — |

Also required in the same plan: extend `TextureResource::ResolveBaseFormat` + content-identity (bytes-per-pixel feeds cache sizing), make `SetData`/`SetSubData` type-aware, extend `TextureCacheTests`/`TextureLoadingTests` with per-format round trips (valid + malformed/oversized fixtures per AGENTS.md parser discipline — dimensions arrive from callers and must hit `IsValidExtent` + `CalculateByteSize`), and keep `SRGB` intent handling for the sRGB pair.

## FRZ-05: Backend-neutral framebuffer binding + depth-texture creation

**Current state:**
- `IGraphicsDevice::BindFramebuffer(class IFramebuffer*)` is declared [VERIFIED: Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp:271] but `IFramebuffer` has **no definition anywhere** — a repo-wide search finds only that declaration and the override in `OpenGLDevice.hpp:74` [VERIFIED: grep over Engine/Graphics/include]. The method is therefore uncallable with any real object: a permanently-unusable virtual on the neutral boundary.
- The override is an explicit stub: non-null input sets `m_lastError = "Framebuffer binding not yet implemented"` and binds nothing; null binds framebuffer 0 [VERIFIED: Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:368-380 — quote]:
```cpp
void OpenGLDevice::BindFramebuffer(IFramebuffer *framebuffer)
{
    if (framebuffer)
    {
        // TODO: Implement when IFramebuffer interface is available
        m_lastError = "Framebuffer binding not yet implemented";
    }
    else
    {
        // Bind default framebuffer
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
```
- All real binding today goes through raw `BindFramebufferHandle(u32)` with `GLuint`s from `OpenGLFramebuffer::GetFramebufferID()` at every pass/system site (`ShadowMapPass`, `DeferredGeometryPass`, `RenderSystem` restore) [VERIFIED: grep over source/Renderer].
- `ITexture2D::CreateDepthTarget` fails explicitly with a pointer to `OpenGLFramebuffer` [VERIFIED: Texture.cpp:38-45, quoted in Patterns].

**Recommendation (owned): IMPLEMENT both halves.**
1. Define `class IFramebuffer` (new `Framebuffer.hpp` or inside `GraphicsDevice.hpp` — planner picks; must stay GLAD/Win32-free in the public header per AGENTS.md): minimal surface `Bind()/Unbind()/IsComplete()/GetSize()` plus a non-leaking native accessor if passes need IDs (prefer returning an opaque `u32` handle owned by the device rather than a raw `GLuint` in the interface). `OpenGLFramebuffer` implements it. `OpenGLDevice::BindFramebuffer` routes through `OpenGLStateManager::BindFramebuffer` (null → 0), clears/sets `m_lastError` symmetrically, and is covered by fake-GL binding tests.
2. Route `RenderSystem::Render` restore, `RenderTarget::Bind/Unbind`, and all pass binds through the neutral method; keep `BindFramebufferHandle(u32)` only as the device-internal workhorse (or remove if call sites fully migrate — planner decides; either way no pass holds a raw `GLuint` across the boundary).
3. Implement `CreateDepthTarget(width, height, depthFormat)` through `OpenGLTexture2D` for the mapped depth/stencil set (reuses FRZ-04 mappings; depth compare mode `NONE` default, documented), transactional (validate → create → return; failure preserves nothing half-made and logs the corrective pointer), with `Tests/` coverage for each depth format + zero/oversized extents. Keep `OpenGLFramebuffer` depth attachments as the render-target path — the two coexist (sampled depth texture vs. attachment), documented in `docs/Architecture.md` §Textures.
4. Ownership check: all touched code is `Engine/` Core/Graphics — inside the engine boundary; no `Libraries/` or game-layer leakage.

## Common Pitfalls

### Pitfall 1: Calling a NULL GLAD pointer on a 3.3 context
**What goes wrong:** Crash (null-function-pointer call) instead of graceful degradation.
**Why it happens:** GLAD loads entry points dynamically per context; `glDispatchCompute` resolves only on ≥4.3 contexts. Any "keep compute reserved" path that touches the pointer without a version/pointer guard explodes on the minimum spec machine.
**How to avoid:** Removal eliminates the risk. If the planner keeps anything compute-adjacent, guard with a runtime GL-version check *and* a null-pointer check, both tested with the function pointer forced null.
**Warning signs:** Code that calls `glXxx` for >3.3 features without checking `GLAD_GL_VERSION_4_X` flags first.

### Pitfall 2: Sampler-type / texture-target mismatch (the FRZ-02 bug shape)
**What goes wrong:** Incomplete texture → black shadows or undefined sampling, no error at bind time on some drivers.
**Why it happens:** `u_ShadowMaps` is `sampler2DArray` but the bound object is `GL_TEXTURE_2D`. Types must match exactly, including for depth-array-vs-plain-depth.
**How to avoid:** After the fix, assert bind-target == `GL_TEXTURE_2D_ARRAY` in a debug check; always run `CheckError` after array creation and first bind; visual-inspect cascade transitions.
**Warning signs:** `BindNativeTexture(x, slot, GL_TEXTURE_2D)` anywhere near shadow code after the fix.

### Pitfall 3: Forgetting the missing uniform uploads
**What goes wrong:** Array bound correctly, but `u_LightSpaceMatrices`/`u_CascadeSplits` stay default → all fragments sample cascade 0 with identity transforms → shadows look "stuck".
**Why it happens:** The current deferred `Execute` never uploads them; a fix that only changes the texture bind leaves half the gap.
**How to avoid:** The FRZ-02 plan's done-criteria must list all three uploads (matrices, splits, count) plus the bind; test asserts each `SetUniform*` location is found (non-`-1`) or failure is explicit.

### Pitfall 4: Byte-size arithmetic on new formats
**What goes wrong:** Buffer over-read/over-write or `SetData` size-mismatch rejections for float/depth textures.
**Why it happens:** `m_BytesPerPixel`/`CalculateByteSize`/`SetData(expectedSize != size)` paths assume the 3-or-4-byte 8-bit world; half-float, packed depth-stencil, and block-compressed sizes break the assumption (compressed size is blocks, not pixels).
**How to avoid:** Reuse `CalculateByteSize` with per-format bytes, add a separate block-size path for S3TC, keep the overflow guards, add very-large-extent tests that must fail explicitly, not crash.

### Pitfall 5: Same-frame occlusion-query waits (if planner keeps FRZ-03 implemented)
**What goes wrong:** CPU stall + GPU starvation; culling costs more than it saves.
**Why it happens:** `GetQueryObjectuiv(QUERY_RESULT)` blocks until the GPU drains. Naive "query then immediately branch" serializes the pipeline.
**How to avoid:** Only implement with frame-delayed consumption (`QUERY_RESULT_AVAILABLE` polling, temporal coherence). If that does not fit the phase budget, remove instead.

### Pitfall 6: Breaking the after-pass restore convention
**What goes wrong:** Fixed-resolution shadow/array passes leak viewport or FBO into world/UI passes (the historical `BasicGame` corruption class).
**Why it happens:** A new array-texture FBO or layered attach that skips the `RenderSystem` restore lines.
**How to avoid:** Route restores through the neutral interface (FRZ-05) and extend the viewport/scissor restoration tests for every touched pass.

### Pitfall 7: Mutating immutable caches while adding formats
**What goes wrong:** Fingerprint/dedup breakage, orphaned handles, stale-handle resurrection.
**Why it happens:** Temptation to "just update" a cached texture in place for the new format/depth paths.
**How to avoid:** Publish only via transactional reload/replace; per-draw data in command-buffer uniforms; run handle/manifest/registry suites (`ResourceHandleTests`, `ResourceManifestTests`, `ResourceRegistryTests`) on any cache-adjacent change.

## Code Examples

### Neutral framebuffer bind (target shape for FRZ-05)
```cpp
// Shape: mirrors the existing null-branch, extended to real objects.
// Source pattern: Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:368-380 [VERIFIED]
void OpenGLDevice::BindFramebuffer(IFramebuffer* framebuffer)
{
    if (framebuffer)
    {
        framebuffer->Bind(); // OpenGLFramebuffer::Bind via OpenGLStateManager
        m_lastError.clear();
    }
    else
    {
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}
```

### Shadow-array creation + single bind (target shape for FRZ-02)
```cpp
// All tokens/entry points verified in vendored GLAD [VERIFIED: glad.h:1652,3100-3101,3997-3998]:
GLuint shadowArray = 0;
glGenTextures(1, &shadowArray);
glBindTexture(GL_TEXTURE_2D_ARRAY, shadowArray);
glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24,
             2048, 2048, cascadeCount, 0,
             GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
// Per cascade: attach layer i via glFramebufferTextureLayer, render, then:
m_device->BindNativeTexture(shadowArray, 5, GL_TEXTURE_2D_ARRAY);
m_lightingShader->SetUniformInt("u_ShadowMaps", 5);
// PLUS: upload u_LightSpaceMatrices (GetLightSpaceMatrices),
// u_CascadeSplits, u_CascadeCount — currently missing [VERIFIED absent: DeferredLightingPass.cpp:100-180].
```

### Format mapping extension (target shape for FRZ-04)
```cpp
// Extend the existing switch; current 4-format subset quoted from
// Engine/Graphics/source/OpenGL/OpenGLTexture.cpp:290-314 [VERIFIED].
// New arms pair internalFormat + dataFormat + pixel size together, e.g.:
//   RGBA16F  -> GL_RGBA16F,  GL_RGBA,           8
//   Depth24  -> GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, 4  (type per arm, not
//               hardcoded GL_UNSIGNED_BYTE as today [VERIFIED: :414-423])
// S3TC arms additionally require: if (!GLAD_GL_EXT_texture_compression_s3tc)
//   { PYRAMID_LOG_ERROR("...; driver lacks EXT_texture_compression_s3tc"); return false; }
//   [VERIFIED flag: glad.h:13277-13279] and glCompressedTexImage2D upload.
```

### Removal hygiene (FRZ-01 / FRZ-03)
```cpp
// 1. Delete declaration (Shader.hpp / SceneManager.hpp) + definition +
//    command-model enum value + union member.
// 2. Remove from Tests/PublicApiLinkage.cpp (it pins CreateDepthTarget-style
//    symbols today, e.g. `g_createDepthTarget` [VERIFIED: Tests/PublicApiLinkage.cpp:214]).
// 3. One-line rationale in docs/ROADMAP.md P0 + CHANGELOG.md entry.
// The linkage test failing on leftover references IS the removal verification.
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Legacy `texture2DArray()` sampler functions | Overloaded `texture(sampler2DArray, vec3)` in GLSL ≥1.30 | GLSL 1.30 (2009) | The engine's shaders are already modern here — no shader rewrite needed for FRZ-02 [CITED: Khronos forums] |
| `EXT_texture_array` extension | Core array textures since GL 3.0, `GL_MAX_ARRAY_TEXTURE_LAYERS` ≥256 | GL 3.0 | Array-shadow fix needs no extension checks on the 3.3 baseline |
| `ARB_occlusion_query` extension | Core since 1.5; boolean `ANY_SAMPLES_PASSED` since 3.3-era | 2003 / 2009 | Keep-implementation would be core-only, but async plumbing still required |
| S3TC via `EXT_texture_compression_s3tc` | Still extension-gated (never core) | — | Runtime flag check is mandatory, not optional |
| BPTC/BC7 via extension | Core in 4.2+ only | 2011 | Unmappable on 3.3 → prune |
| Flat render-object scenes, `SceneNode` | Stable-ID `Entity` + components, serializer v2 | Current baseline | Freeze must not resurrect legacy paths; `OcclusionCull` removal follows the same "delete dead surface" precedent as the removed render-pass classes |

**Deprecated/outdated:**
- Per-cascade single-texture shadow binding: what the code does today; replaced by the array path above.
- `RenderCommandType::Dispatch` + `CompileCompute`/`DispatchCompute`: dead surface; removed this phase.
- `SetOcclusionCullingEnabled` flag: placeholder; removed this phase.

## Assumptions Log

| # | Claim | Section | Risk if Wrong |
|---|-------|---------|---------------|
| A1 | Per-format GL internal-format/type pairings in the FRZ-04 table (e.g., Depth32F→`GL_DEPTH_COMPONENT32F`/`GL_FLOAT`) | FRZ-04 | Medium — wrong pairing fails loudly via GL errors in tests; planner verifies each against `glad.h` + spec tables 3.12–3.14 before coding |
| A2 | `ForwardRenderPass` never binds/uploads shadow uniforms (inferred from content search showing no `ShadowMaps`/`BindNativeTexture` matches in that file) | FRZ-02 | Medium — if it does bind somewhere, the default shadow+forward pipeline may already sample shadows and the fix scope changes; planner confirms with a full read of `ForwardRenderPass.cpp:81-102` |
| A3 | `RenderTarget::Bind` delegates to `m_framebuffer->Bind()` (content-search result, not a session `Read`) | FRZ-05 | Low — routing plan is unaffected; planner confirms during implementation |
| A4 | `glFramebufferTextureLayer` direct-to-layer rendering works for depth-array layers on all target 3.3 drivers (vs. the blit fallback) | FRZ-02 | Medium — mitigated by planning the blit fallback as the default if any doubt; both entry points verified in GLAD |
| A5 | Nothing outside `SceneManager` reads `m_occlusionCullingEnabled` (setter is inline in the header; no other references found in content searches) | FRZ-03 | Low — planner runs a repo-wide `SetOcclusionCullingEnabled` search; any example/test reference becomes part of the removal plan |
| A6 | MSYS2 UCRT64 MinGW toolchain present for `gcc-debug-tests` preset runs (cmake/ctest verified; compiler not probed) | Environment | Low — CI presets are documented in AGENTS.md; executor's first build proves it |

## Open Questions (RESOLVED)

1. **Keep-reserved vs. remove for compute — any agent-path consumer of GPU compute? — RESOLVED: remove; gate in 01-01 Task 1**
   - What we know: Nothing in `Examples/`, `Tests/`, or the renderer calls `Dispatch`; GL 3.3 cannot run it. Removal is safe by evidence.
   - What's unclear: Whether any downstream roadmap item (before a 4.3-backend decision) assumes compute exists.
   - Recommendation: Default to remove; planner adds one grep task over `docs/` + `Examples/` for "compute" references as a pre-removal gate.

2. **Exact `IFramebuffer` surface (minimal vs. capability-rich)? — RESOLVED: start minimal; 01-04 Task 1**
   - What we know: Needs at minimum bind/unbind/identity for the device + passes; must not leak GLAD types into public headers.
   - What's unclear: Whether passes need size/completeness queries through the interface or can keep those on `OpenGLFramebuffer` directly.
   - Recommendation: Planner starts minimal (`Bind/Unbind/IsComplete/GetWidth/GetHeight/GetNativeHandle-as-u32`) and expands only on demonstrated need — no speculative interface growth (AGENTS.md: no required methods with silent no-op defaults).

3. **S3TC gate vs. prune for BC1/BC3? — RESOLVED: gate (prune BC7 only); 01-02 Task 3**
   - What we know: Extension flag is queryable at runtime; content pipeline (PNG/JPEG decoders) never produces pre-compressed blocks, so S3TC matters only for hand-supplied compressed uploads.
   - What's unclear: Whether any agent-visible flow needs compressed upload this milestone.
   - Recommendation: Gate (explicit failure when absent) rather than prune — keeps the enum stable while honest; pruning BC7 only. Revisit if the gate's tests prove burdensome.

4. **Does the forward pipeline need array shadows too, or is deferred the only consumer? — RESOLVED: conditional scoping in 01-03 Task 1**
   - What we know: Both shaders declare the array; only the deferred pass binds anything.
   - What's unclear: Whether the default shadow+forward pipeline is *intended* to sample shadows (A2 above).
   - Recommendation: Planner reads `ForwardRenderPass.cpp:81-102` fully; if forward shadows are intended, the FRZ-02 plan wires the array there as well (shared helper, not duplicated code).

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| CMake | Configure/presets | ✓ | 4.3.2 | — |
| CTest | `ctest --preset test-gcc-debug` | ✓ | 4.3.2 | — |
| Python 3.11 | Font-regen script (not needed this phase) | ✓ | 3.11.15 | — |
| MinGW UCRT64 GCC | `gcc-debug-tests` builds | ? (not probed) | — | CI presets per AGENTS.md; first build proves it |
| Real GPU + drivers | HW verification checklist | ✗ in this session | — | Phase stays code+CI; HW gates the pre-release tag (D-04 recommendation) |
| New runtime libs | None — GLAD covers all entry points | — | — | N/A; no installs |

**Missing dependencies with no fallback:** none for code+CI work.
**Missing dependencies with fallback:** GPU hardware (fallback: fake-GL unit tests + `checkpoint:human-verify` visual tasks + pre-release HW gate).

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | CTest, one executable per `Tests/*.cpp` via `add_test(NAME <Suite>.<Name> COMMAND ...)` [VERIFIED: Tests/CMakeLists.txt:1-77] |
| Config file | `CMakePresets.json` (preset names per AGENTS.md build commands); `Tests/CMakeLists.txt` registers suites |
| Quick run command | `ctest --preset test-gcc-debug -R "<Suite>.<Name>"` (per-test, <30s target) |
| Full suite command | `ctest --preset test-gcc-debug` (per wave merge / phase gate) |

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FRZ-01 | No `Dispatch` symbol/command; `CommandBuffer` records only executable commands | unit (linkage + command recording) | `ctest --preset test-gcc-debug -R "API.PublicApiLinkage"` + extended command-buffer test | ❌ Wave 0 (extend command-buffer coverage; linkage edit) |
| FRZ-02 | Array texture complete; all cascades bound once; matrices/splits/count uploaded | unit (fake-GL) + human visual | `ctest --preset test-gcc-debug -R "Graphics.FramebufferResize"` (extend) + new shadow-array suite | ❌ Wave 0 (new suite or extended resize suite) |
| FRZ-03 | Occlusion setter gone; visibility == frustum+octree result | unit | `ctest --preset test-gcc-debug -R "Graphics.(CameraFrustum\|OctreeQueries)"` (existing parity) + linkage | ❌ Wave 0 (removal assertion; existing suites guard parity) |
| FRZ-04 | Every kept format uploads with correct type; pruned formats are compile-time gone; S3TC failure explicit when extension absent | unit (fake-GL + fixtures) | `ctest --preset test-gcc-debug -R "Graphics.TextureLoading"` (extend per-format) | ❌ Wave 0 (per-format cases, malformed/oversized fixtures) |
| FRZ-05 | Neutral bind binds real FBO and default on null; passes route through it; depth targets created via texture interface per format | unit (fake-GL) | `ctest --preset test-gcc-debug -R "Graphics.FramebufferResize"` (extend: neutral bind, restore, depth targets) | ❌ Wave 0 (neutral-bind + depth-target cases) |

### Sampling Rate
- **Per task commit:** `ctest --preset test-gcc-debug -R "<affected suite>"` (fast subset)
- **Per wave merge:** `ctest --preset test-gcc-debug` (full suite green)
- **Phase gate:** Full suite green + `Tests/PublicApiLinkage` green + human visual sign-off on renderer-affecting plans before `/gsd-verify-work`

### Wave 0 Gaps
- [ ] Extend command-buffer tests (record/execute/count with `Dispatch` removed; unknown-command loud-failure path if any default branch remains)
- [ ] New/extended shadow-array tests (array completeness, layer count == cascade count, single-bind assertion, matrix/split/count upload presence) in fake-GL style of `FramebufferResizeTests.cpp`
- [ ] Extend `TextureLoadingTests`/`TextureCacheTests` (per-format mapping incl. float/depth/R, `SetData` type-awareness, malformed/oversized/limit fixtures, S3TC absent-extension failure)
- [ ] Extend `FramebufferResizeTests` (neutral `BindFramebuffer` real+null, restore-after-pass via neutral path, `CreateDepthTarget` per depth format + invalid extents)
- [ ] Removal assertions (`SetOcclusionCullingEnabled`/`Dispatch`/`CompileCompute` absent from linkage; frustum/octree parity suites green unchanged)
- [ ] `checkpoint:human-verify` visual tasks for FRZ-02 (cascade transitions, acne) — smoke tests are not pixel validation

## Security Domain

| ASVS Category | Applies | Standard Control |
|---------------|---------|------------------|
| V2 Authentication | No | — (engine core, no identity) |
| V3 Session Management | No | — |
| V4 Access Control | No | — |
| V5 Input Validation | **Yes** | Explicit extent/format validation on every new creation path (`IsValidExtent`, `CalculateByteSize` overflow guards, `None`/out-of-range enum rejection with `PYRAMID_LOG_ERROR`); malformed/oversized fixtures per AGENTS.md |
| V6 Cryptography | No | — (never hand-roll; nothing cryptographic in scope) |

### Known Threat Patterns for C++17/OpenGL 3.3 engine core

| Pattern | STRIDE | Standard Mitigation |
|---------|--------|---------------------|
| Integer overflow in texture byte-size math (w×h×bpp, block sizes) | Tampering / DoS | Reuse `CalculateByteSize`/`IsValidExtent` guards; separate block-size path for compressed; very-large-input tests that must fail explicitly |
| Null GLAD function pointer call on <4.3 contexts | DoS (crash) | Removal eliminates it; any kept >3.3 entry point gets version+null guards |
| Stale-handle resurrection via cache changes | Tampering | Generation-tombstone discipline unchanged; run handle/manifest/registry suites on cache-adjacent work |
| Silent capability misrepresentation to agents | Repudiation (trust) | The phase itself is the mitigation: explicit failure or removal everywhere |

## Project Constraints (from AGENTS.md)

Planner must verify compliance against these (authoritative, same weight as locked decisions):
- `Engine/` owns only Core, Graphics, Win32/WGL Platform; no foundational/codec/model/font/text/UI logic enters the engine binary — all FRZ work stays in `Engine/Graphics` + `Engine/Platform` surface usage.
- `vendor/glad` is the sole approved bundled runtime; no new middleware or package-manager dependencies (this research proposes none).
- C++17, four spaces, braces on new lines; `PascalCase` types/methods, `camelCase` locals/params, `m_` fields; RAII + `PYRAMID_LOG_*`.
- Shared geometry/shaders/textures/materials go through `ResourceRegistry` caches; never mutate cached instances; no parallel upload ownership; per-draw matrices in command-buffer uniforms.
- No required interface methods with silent no-op defaults; every public symbol implemented, removed, or documented as explicit failure; linkage-sensitive symbols reflected in `Tests/PublicApiLinkage.cpp`.
- Tests fail visibly with valid fixtures, no false-success skips, clean temp files, actionable context; renderer changes require human visual inspection.
- Do not expose source-tree absolute paths through installed interfaces; docs go to the maintained compact set (`docs/ROADMAP.md`, `CHANGELOG.md`), no new overlapping guides.

## Sources

### Primary (HIGH confidence)
- In-repo source read this session: `CommandBuffer.cpp`, `RenderSystem.hpp/.cpp`, `DeferredLightingPass.cpp`, `ShadowMapPass.cpp`, `RenderPasses.hpp`, `GraphicsDevice.hpp`, `OpenGLDevice.cpp`, `OpenGLShader.cpp`, `Texture.hpp`, `Texture.cpp`, `OpenGLTexture.cpp/.hpp`, `TextureResource.cpp`, `SceneManager.hpp/.cpp`, `Win32OpenGLWindow.cpp` (context negotiation), `ForwardRenderPass.cpp` (partial), `deferred_lighting.frag`, `forward.frag`, `OpenGLFramebuffer.hpp`, `Tests/CMakeLists.txt`, `Tests/PublicApiLinkage.cpp` (partial), `Tests/FramebufferResizeTests.cpp` (partial), `vendor/glad/include/glad/glad.h` (token/entry verification), `AGENTS.md`, `.planning/*` (CONTEXT, REQUIREMENTS, PROJECT, ROADMAP, codebase/CONCERNS, docs/ROADMAP, docs/Architecture).
- Khronos OpenGL 4 reference pages — `glDispatchCompute` version-support table (4.3+ only): https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDispatchCompute.xhtml
- Khronos ARB_occlusion_query2 specification (core-since-1.5 history, `ANY_SAMPLES_PASSED` semantics): https://registry.khronos.org/OpenGL/extensions/ARB/ARB_occlusion_query2.txt
- Khronos wiki — Array Texture (core since 3.0, layer limits, `GL_TEXTURE_2D_ARRAY` usage): https://wikis.khronos.org/opengl/Array_Texture
- Khronos EXT_texture_array + ARB_texture_compression_bptc specifications (sampler types; BPTC written against 3.2 as extension, core in 4.2).

### Secondary (MEDIUM confidence)
- NVIDIA GPU Gems 1 Ch.29 (Efficient Occlusion Culling) + Gems 2 Ch.6 (Hardware Occlusion Queries Made Useful) — async query discipline patterns informing the remove-vs-implement judgment.
- OpenGL 3.3 core specification table-of-contents (sized depth/stencil §3.13, compressed formats §3.14, depth texturing §3.8.9) corroborating 3.3-core format availability.

### Tertiary (LOW confidence)
- None relied upon. Items that would normally sit here (exact per-format enum pairings, layered-render driver universality) are instead tagged `[ASSUMED]` inline with explicit planner verification tasks.

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — no new libraries; GLAD coverage verified token-by-token in the vendored header.
- Architecture: HIGH — all five gaps located to exact files/lines; fix shapes follow established in-repo patterns (transactional creation, explicit failure, restore convention).
- Pitfalls: HIGH — each pitfall is grounded in a read stub or a cited spec constraint.
- Recommendations (remove vs. implement): MEDIUM — facts are HIGH, but the choice itself is delegated judgment; each recommendation names its fallback and the evidence that would flip it.

**Research date:** 2026-09-11
**Valid until:** 2026-10-11 (stable domain: locked baseline + Khronos specs; re-verify only if the 3.3-baseline constraint changes)

<!--
Provenance note for planner/auditor: `gsd-tools query classify-confidence` and
`query research-plan` seams are not implemented in the installed toolchain
(this session: `Unknown command`), so confidence tiers were assigned directly
per the contract (tool-read + authoritative source = VERIFIED; official docs
reference = CITED; training knowledge = ASSUMED with Assumptions Log entries).
No external packages are introduced, so no legitimacy-gate run was applicable.
-->
