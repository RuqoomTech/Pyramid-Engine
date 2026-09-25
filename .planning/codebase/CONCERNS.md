# Codebase Concerns

**Analysis Date:** 2026-09-05

## Tech Debt

**Windows-only platform layer:**
- Issue: Entire runtime is Win32/WGL only. `CMakeLists.txt` hard-fails on non-Windows unless `PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE` is set. Only `Engine/Graphics/source/OpenGL/` backend exists; DirectX/Vulkan enum values are reserved and return no device.
- Files: `CMakeLists.txt`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `docs/Architecture.md`
- Impact: No Linux/macOS builds, no headless CI rendering, blocks declared Linux-follows-RTS goal.
- Fix approach: Keep `IGraphicsDevice` boundary in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` backend-neutral; add second backend behind same interface rather than branching Win32 code. Do not leak WGL/GLAD types into public headers.

**Occlusion culling placeholder:**
- Issue: `SceneManager::m_occlusionCullingEnabled` defaults to `false` and no algorithm backs it. Frustum culling is implemented; occlusion culling is not. Must never be described as supported.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`, `Engine/Graphics/source/Scene/SceneManager.cpp`, `docs/Architecture.md`, `docs/ROADMAP.md`
- Impact: Large/occluded scenes pay full draw cost; any caller enabling the flag gets silently wrong expectations.
- Fix approach: Per `docs/ROADMAP.md`, either implement a supported technique (e.g., software Hi-Z or GPU queries) with `Tests/` coverage, or remove the setting and fail explicitly like depth-target creation does in `Engine/Graphics/source/Texture.cpp`.

**Compute dispatch is log-only:**
- Issue: `CommandBuffer::Dispatch()` records `RenderCommandType::Dispatch` but `Engine/Graphics/source/Renderer/CommandBuffer.cpp` only `PYRAMID_LOG_DEBUG`s it with comment "Dispatch will be handled when compute shader support is added". `OpenGLShader::DispatchCompute()` exists but the render-system execution path never calls it.
- Files: `Engine/Graphics/source/Renderer/CommandBuffer.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/Shader.hpp`
- Impact: Any recorded compute work is silently dropped; misleading API.
- Fix approach: Per `docs/ROADMAP.md` P0, either wire `Dispatch` through `OpenGLDevice` execution with state/SSBO barriers and tests, or remove `Dispatch` from the command model and `Shader::CompileCompute`/`DispatchCompute` until a real backend exists.

**Generic framebuffer binding incomplete:**
- Issue: Backend-neutral framebuffer binding outside the OpenGL renderer remains incomplete (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` carries `// TODO: Implement when IFramebuffer interface is available`). Only `OpenGLFramebuffer` lifecycle is trustworthy.
- Files: `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp`, `docs/Architecture.md`
- Impact: Custom render-target code cannot portably bind framebuffers; forces OpenGL-specific paths.
- Fix approach: Complete `IGraphicsDevice::BindFramebuffer()` contract, route `RenderSystem`/`RenderPass` through it, add `Tests/FramebufferResizeTests.cpp`-style binding tests for the neutral path.

**Deferred shadow-map-array binding stub:**
- Issue: `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp` binds only the first cascade with `// TODO: Implement shadow map array binding` while `Engine/Graphics/source/Renderer/ShadowMapPass.cpp` creates N cascades.
- Files: `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`
- Impact: Multi-cascade shadows silently degrade to single-cascade lighting; visual popping/incorrect shadows.
- Fix approach: Finish sampler-array uniform + texture-unit binding in `DeferredLightingPass::SetShadowMaps()`, verify with visual inspection (smoke test is not pixel validation) plus a unit test asserting all cascades are bound.

**Texture-format and depth-target gaps:**
- Issue: Not all advertised `TextureFormat` values are mapped; `ITexture2D::CreateDepthTarget()` explicitly fails with "Depth texture creation is not implemented by OpenGLTexture2D; use OpenGLFramebuffer" in `Engine/Graphics/source/Texture.cpp`. `docs/ROADMAP.md` P0 still requires "map all advertised formats or remove unsupported enum values".
- Files: `Engine/Graphics/source/Texture.cpp`, `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture.hpp`, `Engine/Graphics/source/Texture/TextureResource.cpp`
- Impact: Callers requesting depth textures or exotic formats get runtime failure instead of compile-time signal.
- Fix approach: Either implement depth-texture creation through the texture interface or delete the unsupported enum values and keep the explicit-failure path with `Tests/TextureLoadingTests.cpp` coverage.

**Legacy scene persistence left unsupported:**
- Issue: `SceneManager::LoadScene`/`SaveScene` JSON/XML/Binary overloads log "Scene loading is not implemented" and version-1 flat render-object scenes are intentionally rejected. Only `SceneSerializer` v2 is valid.
- Files: `Engine/Graphics/source/Scene/SceneManager.cpp`, `Engine/Graphics/source/Scene/SceneSerializer.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp`
- Impact: Any old asset or tutorial calling legacy APIs fails at runtime; dead declarations invite misuse.
- Fix approach: Keep rejection explicit; do not resurrect `SceneNode`/flat paths. Remove or document-as-unsupported every legacy overload and keep `Tests/SceneSerializationTests.cpp` as the contract.

**Multisample resolve unverified across drivers:**
- Issue: `Engine/Graphics/source/Renderer/RenderSystem.cpp` centralizes multisample attachment ownership and `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp` validates multisample state, but `docs/ROADMAP.md` still requires verifying multisampled targets and resolve behavior across supported drivers.
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp`, `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`
- Impact: Driver-specific resolve/blit bugs surface only on user hardware.
- Fix approach: Add multisample create/resize/resolve tests in `Tests/FramebufferResizeTests.cpp` plus manual GPU verification on the OpenGL 3.3 minimum before tagging a pre-release.

**Render statistics are not GPU timings:**
- Issue: Command-buffer statistics count commands; `docs/Architecture.md` explicitly notes "render statistics do not yet represent complete GPU execution metrics".
- Files: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Tests/CommandBufferStatsTests.cpp`
- Impact: Profiling/frame-budget decisions based on these numbers mislead.
- Fix approach: Add GPU timer queries behind the device interface, or rename/document current stats as CPU submission counts only.

## Known Bugs

**Off-screen pass viewport leak (fixed, regression-sensitive):**
- Symptoms: Prior `BasicGame` corruption when a shadow-map-sized viewport leaked into world/UI passes. Fixed by restoring default framebuffer + main viewport after every pass and giving `UIRenderer` explicit final-surface ownership.
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/UI/UIRenderer.cpp`, `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`
- Trigger: Any fixed-resolution off-screen pass (e.g., `ShadowMapPass` at 2048) followed by a world or UI pass without explicit viewport re-establishment.
- Workaround: None needed while current restore logic holds; see Fragile Areas. Any new pass must follow the same restore contract and add a viewport/scissor restoration test (see `Tests/UIRendererTests.cpp`, `Tests/FramebufferResizeTests.cpp`).

**Direct `.exe` launch asset fallback (fixed, contract-dependent):**
- Symptoms: Launching `BasicGame.exe` from a repo root/shortcut previously fell back to the ASCII debug atlas instead of loading `Fonts/PyramidSans-64-sdf.pfont`.
- Files: `Engine/Platform/source/RuntimePath.cpp`, `Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp`, `Examples/BasicGame/source/BasicGame.cpp`, `Examples/BasicRendering/BasicRendering.cpp`, `Tests/RuntimePathTests.cpp`
- Trigger: Any code path that resolves a runtime asset via process CWD instead of `Pyramid::Platform::ResolveRuntimePath()`.
- Workaround: Always use `ResolveRuntimePath()`; never add bare relative-path loads for bundled assets. `Tests/RuntimePathTests.cpp` guards the CWD-fallback behavior.

**No open crash-corruption P0s declared:**
- Symptoms: `docs/ROADMAP.md` pre-release exit criteria require "no remaining P0 issue that can corrupt data, crash normal usage, or misrepresent support", with Windows runtime verification (Debug + Release CI on real host, both examples on real GPU, resize/minimize/restore/close/shutdown checks, OpenGL error + screenshot capture) still listed as uncompleted verification rather than a known code bug.
- Files: `docs/ROADMAP.md`, `docs/Architecture.md`, `CHANGELOG.md`
- Trigger: Shipping/tagging before that hardware verification passes.
- Workaround: Do not tag a pre-release until the P0 verification checklist in `docs/ROADMAP.md` is executed on declared OpenGL 3.3 minimum hardware.

## Security Considerations

**Image parsers handle untrusted bytes:**
- Risk: `Pyramid::Image` owns PNG/zlib/DEFLATE, TGA/BMP subsets, and a bounded 8-bit Huffman JPEG decoder (baseline + progressive, restart markers). Malformed/truncated inputs, size-overflow, and allocation-exhaustion inputs can cause OOM, long decode loops, or out-of-bounds reads if limits regress.
- Files: `Libraries/PyramidImage/include/Pyramid/Util/PNGLoader.hpp`, `Libraries/PyramidImage/include/Pyramid/Util/JPEGLoader.hpp`, `Libraries/PyramidImage/include/Pyramid/Util/Inflate.hpp`, `Libraries/PyramidImage/include/Pyramid/Util/ZLib.hpp`, `Libraries/PyramidImage/include/Pyramid/Util/Image.hpp`, `Libraries/PyramidImage/test/TestJPEGRobustness.cpp`
- Current mitigation: Bounded decoders with explicit rejection of arithmetic/lossless/hierarchical/12-bit/four-component JPEG variants, malformed/truncated fixtures, allocation-limit tests, corrected standards-valid corpus fixtures. `TextureResource` file reload is transactional so failed decode preserves the previous GPU texture (`Engine/Graphics/source/Texture/TextureResource.cpp`).
- Recommendations: Keep adding malformed/truncated/allocation-limit fixtures for every codec change; add fuzzing + ASan/UBSan coverage per `docs/ROADMAP.md` ("Add parser fuzzing and sanitizer coverage", "Expand parser fuzzing, very-large allocation-limit fixtures, and PNG interlace coverage"). Never raise limits without a corresponding very-large-input test.

**Model parsers handle untrusted files:**
- Risk: `Pyramid::Model` OBJ/MTL parsing accepts positive/negative indices, polygon triangulation, quoted paths, and material-library references. Malicious OBJ/MTL can exploit unbounded vertex counts, non-finite floats, duplicate materials, or path traversal via `map_Kd`/library paths.
- Files: `Libraries/PyramidModel/include/Pyramid/Model/ObjImporter.hpp`, `Libraries/PyramidModel/include/Pyramid/Model/Model.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`, `Libraries/PyramidModel/test/ObjFailureTests.cpp`
- Current mitigation: `ModelImportLimits` with count/byte/diagnostic caps, explicit malformed-index/missing-library/duplicate-material/non-finite/empty-geometry failures, transactional `ModelResourceImporter` publication with rollback that leaves pre-existing cache entries intact.
- Recommendations: Every OBJ/MTL change must add positive/negative-index, malformed-input, limit, file-resolution, and transactional-upload tests (per `AGENTS.md`). Validate/normalize `map_Kd` paths before hitting `TextureCache`; never allow absolute-path or `..` escape to read outside the asset root.

**Font parsers handle untrusted binaries:**
- Risk: `Pyramid::Font` parses SFNT/TrueType `head`/`hhea`/`maxp`/`hmtx`/`loca`/`glyf`/`cmap`/`name`/`kern` tables including compound glyphs with cycle/depth risks. Corrupt fonts can cause unbounded outlines, atlas exhaustion, or cache poisoning.
- Files: `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `Libraries/PyramidFont/source/`, `scripts/regenerate-reference-fonts.py`
- Current mitigation: Bounded parsing with cycle/depth protection, explicit rejection of CFF/OpenType outlines, collections, variable/color fonts, WOFF, and malformed tables; checksummed versioned `.pfont` runtime format; content-addressed processed-font cache with atomic publication and corruption-safe rebuild.
- Recommendations: Keep the checked-in `Pyramid Sans`/`Pyramid Arabic` assets reproducible via `scripts/regenerate-reference-fonts.py`; gate installed-family bakes with required-glyph and atlas-capacity checks as `Examples/BasicGame/source/BasicGame.cpp` does. Do not accept CFF/variable/color/WOFF without a new bounded parser plus corpus tests.

**Text decoding must not corrupt on hostile input:**
- Risk: Overlong/invalid UTF-8, lone surrogates from Win32 `CF_UNICODETEXT`/UTF-16 assembly, and unsupported codepoints flow through `Pyramid::Text` into UI hit-testing, caret geometry, and atlas lookup.
- Files: `Libraries/PyramidText/include/Pyramid/Text/Text.hpp`, `Libraries/PyramidInput/include/Pyramid/Platform/Clipboard.hpp`, `Libraries/PyramidInput/include/Pyramid/Platform/Input.hpp`, `Libraries/PyramidText/test/TextTests.cpp`
- Current mitigation: Strict owned UTF-8 decoding with malformed-sequence diagnostics, explicit fallback-glyph accounting (never corrupt UTF-8), bounded UTF-32/UTF-16 conversion with line-ending normalization in the Win32 clipboard backend.
- Recommendations: Preserve grapheme-boundary snapping and logical-to-visual cluster maps on every editing/layout change; add malformed-sequence tests for any new decoder path.

**Win32 system-font extraction trusts host fonts:**
- Risk: `Platform.SystemFonts` reads bytes from legally installed host fonts via GDI. A substituted family or tampered installed font could inject unexpected outlines.
- Files: `Engine/Platform/source/Windows/`, `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `Tests/SystemFontTests.cpp`
- Current mitigation: Exact-family selection that rejects silent Windows substitution; all parsing/rasterization/caching stays in Pyramid-owned code; coverage/capacity gates with Ruqoom-owned 64-pixel SDF fallbacks.
- Recommendations: Never bundle another party's font bytes; never bypass the substitution-rejection check; keep `Tests/SystemFontTests.cpp` exact-selection/coverage/cache assertions green.

**No secrets in tree, but asset paths must stay relocatable:**
- Risk: No `.env`/credential files were detected in the scanned tree; the risk is future code exposing source-tree absolute paths through installed interfaces or hard-coding machine-specific asset locations.
- Files: `CMakeLists.txt`, `CMake/PyramidMinGWRuntime.cmake`, `Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp`
- Current mitigation: `AGENTS.md` forbids exposing source-tree absolute paths through installed targets; `ResolveRuntimePath()` prefers executable-adjacent assets with CWD only as dev fallback.
- Recommendations: Audit every new installed header/CMake export for absolute paths; keep secrets (if ever needed) outside the repo and never quote them into `.planning/` docs.

## Performance Bottlenecks

**No occlusion culling; frustum + octree carry the load:**
- Problem: Only normalized frustum planes + bounds-aware octree pruning exist. Overdraw-bound scenes with heavy occlusion still submit everything visible to the frustum.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Engine/Graphics/source/Scene/SceneManager.cpp`, `Tests/CameraFrustumTests.cpp`, `Tests/OctreeQueryTests.cpp`
- Cause: Occlusion path deliberately disabled; no Hi-Z/GPU-query alternative yet.
- Improvement path: Implement or delete per Tech Debt above. Until then, keep octree bounds tight, rely on `Octree::Synchronize()` incremental updates, and profile with real-GPU captures rather than command counts.

**Shared-geometry/shader/texture/material discipline is load-bearing:**
- Problem: Bypassing the caches (parallel uploads, direct `ShaderProgram` mutation, ad-hoc materials, raw `vertexArray` fields) silently multiplies GPU memory and compile/upload cost and breaks content-identity assumptions.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Geometry/MeshCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Material/MaterialCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`
- Cause: Caches deduplicate by exact content fingerprint (geometry bytes + layout + topology; shader stage sources; pixels + sampler + color space; material shader/texture/uniform/state). Parallel ownership defeats dedup.
- Improvement path: Follow `AGENTS.md` rules strictly: shared geometry via `ResourceRegistry::Meshes()`, shared programs via `ResourceRegistry::Shaders()` (never mutate cached `ShaderProgram`), shared sampled textures via `ResourceRegistry::Textures()` (immutable, color space is identity, file changes via transactional reload), scene materials via `ResourceRegistry::Materials()` with per-draw matrices in command-buffer uniforms. Publish graphics via `ModelResourceImporter`, never parallel upload ownership.

**Octree health degrades without sync + compaction:**
- Problem: Moving/inserted/removed objects that skip `Octree::Synchronize()` leave stale placements; removal-heavy workloads without `Compact()` leave empty-branch bloat and inflate traversal/memory.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Tests/OctreeUpdateTests.cpp`, `Tests/OctreeCompactionTests.cpp`, `Tests/OctreeConfigurationTests.cpp`, `Tests/NearestQueryTests.cpp`
- Cause: Incremental sync + batched bottom-up compaction only run when called after movement/removal batches.
- Improvement path: Batch all movement/removal changes then call one `Synchronize()` (which compacts once) per frame; monitor `OctreeStats`/`OctreeCompactionStats` (node counts, occupancy, memory estimate) in debug UI; use transactional `Octree::Configure()` for bounds/depth/capacity changes rather than piecemeal setters.

**Font/SDF pipeline cost if misused:**
- Problem: CPU supersampled coverage/SDF rasterization, deterministic atlas packing, and content-addressed processed-font caching are one-time costs only if cached `.pfont` assets and the 64-pixel SDF atlases are reused. Re-rasterizing per frame or per size defeats the design; wrong sampler (nearest) degrades downscaled coverage quality.
- Files: `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `Libraries/PyramidText/include/Pyramid/Text/Text.hpp`, `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`, `Examples/BasicGame/source/BasicGame.cpp`
- Cause: Historical nearest-filter + per-launch rasterization issues; now fixed with linear sampling + derivative-smoothed SDF shader + deterministic cache keys.
- Improvement path: Ship precompiled 64-pixel SDF `.pfont` files, reuse `Pyramid::Font` content-addressed cache, keep `UIRenderer` linear sampling for coverage atlases and SDF mode + optical-weight bias for SDF atlases. Do not reintroduce FreeType/HarfBuzz/stb or font-middleware DLLs.

## Fragile Areas

**Entity hierarchy transforms:**
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`, `Engine/Graphics/source/Scene.cpp`, `Tests/EntitySceneTests.cpp`, `Tests/SceneTransformTests.cpp`
- Why fragile: Local transforms are authoritative with cached world matrices/rotations/scales, recursive invalidation on local edit/reparent/detach/ancestor change, cycle-safe parenting by `EntityId`, inherited visibility, and generated `RenderObject`/`Light` proxies. Treating proxies as a second transform authority, reintroducing `SceneNode`, or making `RenderObject` transforms authoritative silently forks the scene graph.
- Safe modification: Author only via `Entity` + components; refresh renderer proxies through the scene; preserve stable-ID, cycle-rejection, and hierarchy-invariant tests. Run `Tests/EntitySceneTests.cpp` + `Tests/SceneTransformTests.cpp` on any change.
- Test coverage: Strong focused coverage for stable IDs, TRS composition, invalidation, reparent/detach, cycle rejection, visibility inheritance, component attachment, proxy generation, recursive destruction. Gaps: cameras/environment/RTS components/editor metadata are not yet in the format (see Missing Features).

**Scene serialization v2 invariants:**
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp`, `Engine/Graphics/source/Scene/SceneSerializer.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp`, `Tests/SceneSerializationTests.cpp`
- Why fragile: Deterministic version-2 text stores stable IDs, parent IDs, local transforms, mesh-renderer/light components, primary-light entity, and exact manifest keys + generations. Loading is transactional: malformed data, missing parents, hierarchy cycles, missing assets, or stale generations must prevent publication. Version-1 flat scenes are rejected.
- Safe modification: Never loosen validation to "best effort"; keep transactional parse-then-publish; extend only via later format versions for cameras/environment/gameplay/editor metadata. Update `Tests/SceneSerializationTests.cpp` round-trip, hierarchy-validation, and missing/stale-diagnostic cases together.
- Test coverage: Good for v2 round trips, encoded names, manual/auto bounds, direct + handle-backed resources, parser rejection, registry validation. Gap: no coverage for not-yet-defined v3 extensions.

**Registry handle generations and stale handles:**
- Files: `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceHandle.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Geometry/MeshCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Material/MaterialCache.hpp`, `Tests/ResourceHandleTests.cpp`, `Tests/ResourceManifestTests.cpp`, `Tests/ResourceRegistryTests.cpp`
- Why fragile: Handles are non-owning `(assetId, generation)` values. Every alias bind/remap/removal, collection, clear, and direct cache mutation must advance a persistent generation tombstone; stale handles must resolve to `nullptr`, never to replacement content under a reused stable ID. `ResourceRegistry::CollectUnused()`/`Clear()` order (materials before textures/shaders/meshes) is dependency-critical, as is `Game` constructing the registry after device creation and destroying it before device shutdown.
- Safe modification: Use handle-first acquisition; never mutate cached `ShaderProgram`/`TextureResource`/`Material` in place (they are immutable by design); publish replacements only via transactional `Recompile()`/`Reload()`/`Replace()` then reacquire the alias. Direct cache mutation must call `InvalidateAlias()`. Run handle/manifest/registry suites on any cache change.
- Test coverage: Strong for typed identity, non-owning lifetime, direct-cache invalidation, replacement generations, forged/stale rejection, scene integration, clearing. Keep it that way; any new cache must replicate the tombstone pattern.

**Framebuffer and viewport state leaks:**
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/UI/UIRenderer.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp`, `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp`
- Why fragile: Every pass may bind its own framebuffer/viewport. The current fix (rebind framebuffer 0 + restore main extent after each pass; `UIRenderer` establishes the final DPI-scaled surface explicitly; conservative DPI-scaled scissor rects; deterministic baseline-state restoration) is a convention every new pass must repeat. One pass that skips restoration corrupts all later world/UI framing.
- Safe modification: Never add an off-screen pass without after-pass restore; keep shadow-map resolution independent of window size intentionally; verify resize/minimize/restore paths via `Tests/FramebufferResizeTests.cpp`, `Tests/CameraViewportTests.cpp`, `Tests/WindowResizeEventTests.cpp`, `Tests/UIRendererTests.cpp` plus on-hardware visual checks.
- Test coverage: Focused coverage for transactional `OpenGLFramebuffer::Resize()`, render-target/pass/system resize propagation, UI viewport/scissor/state restoration exists; `docs/ROADMAP.md` still wants comprehensive render-state transition tests.

**Material/texture cache immutability:**
- Files: `Engine/Graphics/include/Pyramid/Graphics/Material/Material.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Material/MaterialCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureResource.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderProgram.hpp`, `Tests/MaterialCacheTests.cpp`, `Tests/MaterialResourceTests.cpp`, `Tests/TextureCacheTests.cpp`
- Why fragile: `Material`, `TextureResource`, and `ShaderProgram` are immutable after publication; content identity includes exact bytes/sources, sampler state, color space, uniforms, and render state. In-place mutation invalidates fingerprints, breaks dedup, and orphans handles. `UIRenderer` init rollback and `RemoveAlias()` (non-canonical) exist precisely to allow transactional failure without disturbing canonical content.
- Safe modification: Acquire shared resources through `ResourceRegistry`; publish changes only through transactional reload/recompile/replace; put per-draw matrices in command-buffer uniforms, not material identity; respect color-space-as-identity for textures. Run material/texture/shader cache suites on any change.

**Input consumption ordering:**
- Files: `Engine/Core/source/Game.cpp`, `Libraries/PyramidInput/include/Pyramid/Input/InputActions.hpp`, `Libraries/PyramidInput/include/Pyramid/Platform/Input.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `Examples/RTSReference/`, `Tests/InputActionTests.cpp`, `Tests/InputStateTests.cpp`, `Tests/RTSInteractionTests.cpp`
- Why fragile: `Game` must evaluate `InputConsumptionMask` from registered UI contexts before `InputActionSystem` contexts each frame; focus loss must release held controls; camera controllers must consume configurable named action references (delta vs. rate distinct) with physical bindings living only in examples/games; RTS names must never be hard-coded into `Pyramid::Input`.
- Safe modification: Keep backend-neutral `InputState` boundary; reserve handled controls with `InputConsumptionMask`; block conflicting gameplay controls for text-focused UI without swallowing unrelated function keys. Test with `Tests/InputActionTests.cpp` + `Tests/RTSInteractionTests.cpp`.

**UI/text ownership boundaries:**
- Files: `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `Libraries/PyramidText/include/Pyramid/Text/Text.hpp`, `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`
- Why fragile: Layout/widget state/hit-testing/focus/capture/draw-list generation must stay in `Pyramid::UI` (no OpenGL/Win32/scenes/game semantics); decoding/shaping/wrapping/metrics/placement stay in `Pyramid::Text`; parsing/rasterization/caching/`.pfont` stay in `Pyramid::Font`; graphics publication goes only through `UIRenderer`. Crossing these lines reintroduces middleware coupling the extraction work removed.
- Safe modification: Enforce dependency direction (`Font` depends only on `Foundation`; `UI` independent of OpenGL/Win32); keep scroll areas in logical coordinates with clip-respecting hit/draw; preserve explicit logical-to-visual cluster maps and never split a cluster during wrap/edit; keep fallback-family order deterministic.
- Test coverage: Focused international-text, text-editing, UI-context, and UI-renderer suites exist; full Unicode/OpenType conformance is explicitly out of scope (see Missing Features).

## Scaling Limits

**Octree single-tree spatial index:**
- Current capacity: One octree per `SceneManager` with configurable bounds/depth/capacity via transactional `Octree::Configure()`; default depth 8, size 1000³ in `Engine/Graphics/source/Scene/SceneManager.cpp`. Health metrics (`OctreeStats`: internal/leaf/empty/occupied counts, occupancy, memory estimate) exposed for tuning.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Engine/Graphics/source/Scene/SceneManager.cpp`, `Tests/OctreeConfigurationTests.cpp`, `Tests/OctreeCompactionTests.cpp`
- Limit: Very large maps / high object counts deepen the tree and inflate traversal + memory; root-overflow objects outside configured bounds bypass child pruning.
- Scaling path: Required RTS milestone items "terrain/large-map rendering and scalable visibility/spatial updates" are still open (`docs/ROADMAP.md` P3.7). Tune bounds/depth/capacity per map, keep incremental `Synchronize()`, and validate with nearest/K-nearest/parity tests before growing scenes.

**Texture/material/shader cache residency:**
- Current capacity: Strong-residency caches with per-cache statistics (hits/misses/creations/failures/conflicts/evictions/aliases/bytes) and explicit `Evict()`/`CollectUnused()`/`Clear()`.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Geometry/MeshCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Material/MaterialCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`
- Limit: Unbounded alias creation without collection pins GPU memory; large textures/SDF atlases without capacity gates exhaust VRAM.
- Scaling path: Use `ResourceRegistry::CollectUnused()` in dependency order during level transitions; enforce atlas-capacity and required-glyph gates for font bakes; monitor F1 overlay resource stats in `Examples/BasicGame/source/BasicGame.cpp`.

**Log-history and UI draw-list growth:**
- Current capacity: Bounded thread-safe logger history with severity filtering/capacity/clearing; `UI::DrawList` batched per frame; scroll areas retain logical coordinates with clipping.
- Files: `Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `Examples/BasicGame/source/BasicGame.cpp`
- Limit: Unbounded log spam or unclipped huge lists inflate per-frame CPU/draw cost.
- Scaling path: Keep logger caps enabled; use collapsing sections, clipped scroll areas, and stick-to-bottom console patterns already in `Pyramid::UI`.

## Dependencies at Risk

**`vendor/glad` (sole bundled runtime library — approved):**
- Risk: Single pinned copy of GLAD loader (`vendor/glad/`). Stale or mismatched generated sources would break WGL extension loading and debug-context negotiation.
- Impact: Context creation, `glDispatchCompute`, debug callbacks, multisample paths all depend on correct generated bindings.
- Migration plan: None needed; policy in `AGENTS.md` locks GLAD as the sole approved bundled runtime. Update only by regenerating from the declared OpenGL 3.3-core profile and re-running `Tests/OpenGLDiagnosticsTests.cpp` + both examples on real hardware.

**MSYS2 UCRT64 MinGW runtime (platform risk):**
- Risk: Native MinGW builds depend on `PYRAMID_BUNDLE_MINGW_RUNTIME` (default ON) copying compiler runtime DLLs beside `build/*/bin` executables via `CMake/PyramidMinGWRuntime.cmake`, plus a PowerShell post-build verifier. If disabled or if the toolchain layout changes, `BasicGame.exe`/`BasicRenderingExample.exe`/test binaries fail to launch without `PATH` edits.
- Impact: "Works on my machine" launch failures; broken install/package consumer validation.
- Migration plan: Keep the option enabled by default; never require users to edit `PATH` to launch examples. Validate with `cmake --preset gcc-debug-tests`, `cmake --build --preset build-gcc-debug-tests`, `ctest --preset test-gcc-debug` plus direct `.exe` launches from outside the build tree. Fix resolution through `Pyramid::Platform::ResolveRuntimePath()`, never CWD assumptions.

**Prohibited-by-default new middleware:**
- Risk: Any new runtime middleware or package-manager dependency violates the ecosystem rule; former JPEG middleware (libjpeg-turbo) was already removed in favor of owned decoders.
- Impact: Build/CI bloat, licensing risk, loss of independent `Pyramid::Foundation`/`Math`/`Input`/`Image`/`Model`/`Font`/`Text`/`UI` package testability.
- Migration plan: Required non-platform functionality belongs in independently maintained Pyramid/Ruqoom libraries per `AGENTS.md`. Keep `Tests/Consumer`, `Tests/LibrariesConsumer`, `Tests/ImageConsumer`, `Tests/ModelConsumer` green.

## Missing Critical Features

**Audio, physics, scripting, DirectX, Vulkan, Linux, macOS:**
- Problem: None of these exist. `AGENTS.md` and `docs/Architecture.md` state explicitly: audio, physics, editor, scripting, DirectX, Vulkan, Linux, macOS are not yet supported. Product target is a Windows RTS vertical slice first; Linux follows it.
- Blocks: Any cross-platform ship, sound, collision/dynamics, or scripted gameplay beyond C++.
- Files: `AGENTS.md`, `docs/Architecture.md`, `docs/ROADMAP.md`, `CMakeLists.txt`

**Full editor:**
- Problem: Only runtime debug UI + F1 overlay + `ScreenStack` game screens exist. Editor widgets (tree views, property grids, splitters, menus, dialogs, docking, scene/resource inspectors), style classes, controller navigation, and reusable list/tree widgets are future work per `docs/ROADMAP.md`.
- Blocks: Visual scene authoring, asset inspection workflows.
- Files: `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/GameUI.hpp`, `Examples/BasicGame/source/BasicGame.cpp`

**RTS gameplay runtime:**
- Problem: Edge scrolling, selection, and command input exist as game-side reference (`Examples/RTSReference/` + `Tests/RTSInteractionTests.cpp`), but RTS components (selectable, team/owner, movement target, health, basic unit state), terrain/large-map rendering, scene-hierarchy instantiation from imported models, and versioned scene extensions for cameras/environment/gameplay/editor metadata are still open (`docs/ROADMAP.md` P3.6–P3.9).
- Blocks: First playable Ruqoom RTS vertical slice (`0.7.0-pre-alpha`) and everything after it.
- Files: `Examples/RTSReference/`, `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`

**International text conformance subset:**
- Problem: Owned subset covers extended-grapheme segmentation, common Latin/Arabic/numeric bidi runs, core Arabic presentation-form shaping, ordered fallback families, international word/CJK breaks, and cluster-aware hit/caret/selection. Explicit bidi controls/isolates, full UAX #14, OpenType GSUB/GPOS, general complex scripts, advanced Arabic ligatures/mark positioning, and Win32 IME pre-edit/candidate presentation are explicitly future work. Committed/clipboard text is done; IME composition remains the next isolated step.
- Blocks: Full Unicode-correct editing/rendering claims; CJK/Indic/complex-script shipping quality.
- Files: `Libraries/PyramidText/include/Pyramid/Text/Text.hpp`, `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `docs/API.md`

**Font format coverage subset:**
- Problem: Only TrueType `glyf` outlines are supported. CFF/OpenType outlines, collections, variable/color fonts, and WOFF are explicitly rejected.
- Blocks: Loading arbitrary retail fonts in those formats.
- Files: `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `docs/Architecture.md`

**Asset pipeline gaps:**
- Problem: Shader preprocessing/dependency tracking/reload, asset packaging and source-checkout-independent path abstraction remain open (`docs/ROADMAP.md` P1–P2). `ResolveRuntimePath()` covers executable-adjacent lookup only.
- Blocks: Hot-reload workflows and shippable asset bundles.
- Files: `Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderCache.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/ShaderPathResolver.hpp`

**Baa scripting (gated, not started):**
- Problem: Baa is the long-term scripting/rewrite target but admission requires a frozen hosted ABI/FFI, ownership/failure semantics for allocation/strings/errors/threads/loading/debugger/hot-reload, one gameplay-only module beside the C++ example, deterministic reload/rollback/containment, and Qalam/Baa-LSP/Takween integration — all unchecked in `docs/ROADMAP.md`.
- Blocks: Nothing current; prevents premature engine reimplementation.
- Files: `docs/ROADMAP.md`

## Test Coverage Gaps

**Render-state transition tests (incomplete):**
- What's not tested: Comprehensive render-state transitions. UI viewport/scissor/state restoration now has focused coverage, but the full matrix is still open per `docs/ROADMAP.md` P0.
- Files: `Tests/UIRendererTests.cpp`, `Tests/FramebufferResizeTests.cpp`, `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`
- Risk: State-leak regressions in new passes/materials escape unit tests and appear only as on-screen corruption.
- Priority: High

**Render-image regression tests (absent):**
- What's not tested: Automated pixel-level render-image regression. Process smoke testing (`scripts/run-smoke.ps1`) proves the process runs; renderer changes explicitly require visual inspection per `AGENTS.md`.
- Files: `scripts/run-smoke.ps1`, `Examples/BasicGame/source/BasicGame.cpp`, `Examples/BasicRendering/BasicRendering.cpp`
- Risk: Shader, lighting, shadow, SDF-sampling, or viewport regressions ship silently.
- Priority: High

**Fuzzing and sanitizer coverage (absent):**
- What's not tested: Parser fuzzing and AddressSanitizer/UndefinedBehaviorSanitizer runs for image/model/font/text parsers. Very-large allocation-limit fixtures and PNG interlace coverage are still listed as expansion items.
- Files: `Libraries/PyramidImage/test/`, `Libraries/PyramidModel/test/ObjFailureTests.cpp`, `Libraries/PyramidFont/`, `CMakePresets.json`
- Risk: Malformed-input memory-safety bugs survive in decoders that face untrusted files.
- Priority: High

**Warnings-as-errors (off):**
- What's not tested: `PYRAMID_WARNINGS_AS_ERRORS` defaults to OFF in `CMakeLists.txt`/`CMakePresets.json`; "warning cleanup followed by warnings-as-errors in CI" is still a P1 item.
- Files: `CMakeLists.txt`, `CMakePresets.json`
- Risk: New warnings accumulate and hide real defects.
- Priority: Medium

**GPU timing and frame statistics (unvalidated):**
- What's not tested: Accurate frame statistics and GPU timings (`docs/ROADMAP.md` P1). Current `Tests/CommandBufferStatsTests.cpp` covers CPU command counts only.
- Files: `Tests/CommandBufferStatsTests.cpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp`
- Risk: Performance work tunes against numbers that do not represent GPU execution.
- Priority: Medium

**Hardware verification (process, not unit tests):**
- What's not tested in CI: Clean Debug + Release CI on the real host, both examples on a supported GPU/driver, resize/minimize/restore/visibility/close/shutdown behavior, OpenGL error + screenshot capture for the reference path, clean install + external-consumer build, GLSL 3.30 on the OpenGL 3.3 minimum.
- Files: `docs/ROADMAP.md`, `.github/`, `Tests/Consumer/`, `Tests/LibrariesConsumer/`, `Tests/ImageConsumer/`, `Tests/ModelConsumer/`
- Risk: Green CI without hardware proof misrepresents the `0.6.0-pre-alpha` baseline; pre-release exit criteria explicitly forbid tagging before this passes.
- Priority: High

**Future-format and gameplay-component tests (not yet definable):**
- What's not tested: Scene-format v3+ (cameras, environment, RTS gameplay components, editor metadata), terrain/large-map visibility, scene-hierarchy instantiation from `ModelResourceImporter` output, style classes/visual transitions/controller navigation/list-tree widgets, IME pre-edit/candidate flows.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`
- Risk: Low today (features do not exist); becomes High the moment any of these land without co-designed tests.
- Priority: Low (now) / High (on implementation)

---

*Concerns audit: 2026-09-05*
