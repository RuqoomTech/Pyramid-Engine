# Changelog

All notable changes to Pyramid Engine are documented here. The project is pre-alpha; version numbers describe development milestones rather than API-stability guarantees.

## [Unreleased]

### P0 core freeze

- Removed compute dispatch from the command model: deleted `RenderCommandType::Dispatch`, `CommandBuffer::Dispatch`, `IShader::CompileCompute`/`DispatchCompute` and the `OpenGLShader` implementations, plus the `ShaderProgram` compute path (`computeSource`, `ShaderProgramType::Compute`, `IsCompute`). Compute shaders require OpenGL 4.3-plus while the baseline is 3.3 core; re-add only with a higher-baseline backend decision.
- Removed the occlusion-culling placeholder: deleted `SceneManager::SetOcclusionCullingEnabled`, `OcclusionCull`, and `m_occlusionCullingEnabled`. Visibility is frustum plus octree; occlusion deferred until a device-side query technique earns its own phase.

### Added (FRZ-04)

- Full 3.3-core texture format mapping in `OpenGLTexture2D::ResolveFormats` with a
  lockstep `TextureResource::ResolveBaseFormat` mirror: `RGB16F`, `RGB32F`,
  `RGBA16F`, `RGBA32F` (`GL_FLOAT` uploads), `Depth16`, `Depth24`, `Depth32F`,
  `Depth24Stencil8`, `Depth32FStencil8` (matching depth/stencil types), `R8`,
  `R16F`, `R32F` (`GL_RED` uploads; sampled as `(R, 0, 0, 1)`).
- Driver-gated S3TC upload path: `BC1_RGB`, `BC1_RGBA`, `BC3_RGBA` upload
  pre-compressed blocks via `glCompressedTexImage2D` only when the driver reports
  `EXT_texture_compression_s3tc`; without it creation fails explicitly naming the
  missing extension.
- Type-aware `OpenGLTexture2D::SetSubData(data, xOffset, yOffset, width, height)`
  with region-bounds, overflow, and 4x4 block-alignment validation.

### Removed (one-way contract break, FRZ-04)

- `Pyramid::TextureFormat::BC7_RGBA`: BPTC/BC7 compression requires OpenGL 4.2
  (ARB_texture_compression_bptc) and is unmappable on the locked OpenGL 3.3 core
  baseline. The enumerator is deleted; any reference is now a compile-time error.
  Re-adding BC7 later is a contract migration gated on a higher-baseline backend
  decision.

### Added (FRZ-02)

- Cascaded shadows now render into one `GL_TEXTURE_2D_ARRAY` depth texture
  (2048x2048, one layer per cascade) through a single shared framebuffer using
  `glFramebufferTextureLayer`. `DeferredLightingPass` binds the array exactly
  once with target `GL_TEXTURE_2D_ARRAY` to match the shaders'
  `sampler2DArray`, and uploads the previously missing `u_LightSpaceMatrices`
  and `u_CascadeSplits` alongside `u_CascadeCount`. The array is recreated
  transactionally on cascade-count or resolution change, counts above the
  4-cascade shader bound fail explicitly, and an empty set skips the bind with
  lighting continuing unshadowed. Shadow resolution stays fixed at 2048,
  independent of window size. The forward pipeline renders unshadowed by
  design: its `u_CascadeCount` default of 0 guards the array sampler.

### Added (FRZ-05)

- `Pyramid::IFramebuffer`, the previously undefined framebuffer contract
  `IGraphicsDevice::BindFramebuffer` forward-declared, is now defined in
  `Pyramid/Graphics/Framebuffer.hpp` with a backend-neutral surface (`Bind`,
  `Unbind`, `IsComplete`, `GetWidth`, `GetHeight`, and an opaque `u32`
  `GetNativeHandle`). The header includes no GLAD and no Win32 types.
- `OpenGLDevice::BindFramebuffer` performs real binding instead of failing with
  `"Framebuffer binding not yet implemented"`: a non-null target binds through
  the interface and clears the last-error state, and `nullptr` restores the
  default surface. `OpenGLFramebuffer` implements the interface.
- Every render pass and system call site now binds through the neutral
  `IGraphicsDevice::BindFramebuffer`: the `RenderSystem` per-pass restore,
  `RenderTarget` bind and unbind, the `ShadowMapPass` array target and restores,
  the `DeferredGeometryPass` G-buffer, the `DeferredLightingPass` restore, the
  command buffer's null render-target command, and the UI surface pass.
  `BindFramebufferHandle(u32)` remains only as the device-internal workhorse; no
  pass holds a raw backend handle across the boundary any more. The after-pass
  framebuffer-then-viewport restore order and its explanatory comment are
  unchanged.
- `ITexture2D::CreateDepthTarget(width, height, format)` now creates a real
  sampled depth texture through `OpenGLTexture2D` for `Depth16`, `Depth24`,
  `Depth32F`, `Depth24Stencil8`, and `Depth32FStencil8`, replacing the
  explicit-failure stub. Creation is transactional: the extent is validated
  before allocation and any failure deletes the texture object and returns
  `nullptr` with the dimensions in the diagnostic. Depth targets are never
  mipmapped and pin `GL_TEXTURE_COMPARE_MODE` to `GL_NONE` explicitly.
  Non-depth formats are rejected with a pointer at `OpenGLFramebuffer`
  attachments. Sampled depth textures and framebuffer depth attachments are
  distinct facilities and now coexist; see `docs/Architecture.md`.

### Ecosystem boundary documentation

- Registered Pyramid Engine as an independent Eco runtime consumer and defined
  the Baa gameplay-scripting admission gate without changing the current C++ or
  CMake ownership.
- Recorded the planned data-only Arabic interaction corpus boundary with ArbSh
  and Qalam; no cross-runtime library dependency was introduced.

### Scalable typography and native font sources

- Added signed-distance-field glyph rasterization, version-2 `.pfont` metadata, derivative-smoothed GPU rendering, and per-vertex optical-weight control while retaining compatibility with coverage atlases.
- Added content-addressed processed-font caching with atomic publication, deterministic cache keys, missing-glyph skipping, and corruption-safe rebuilding through `Pyramid::Font`.
- Added a Win32 system-font source that extracts exact installed TrueType families through GDI while rejecting silent Windows substitutions; parsing, Arabic shaping, rasterization, caching, fallback, and rendering remain inside Pyramid-owned libraries.
- Added heading, body, label, button, input, and caption typography roles plus public heading/caption widgets. BasicGame now builds a script-specific installed family from Segoe UI for Latin, Cairo when available or Tahoma for Arabic, and Segoe UI Symbol for icons, with required-glyph and atlas-capacity gates plus Ruqoom-owned 64-pixel SDF fallbacks.
- Added `Platform.SystemFonts`, installed-family cache-hit/coverage validation, `UI.ScalableTypography`, SDF shader assertions, cache tests, and 64-pixel deterministic asset checks; CTest now registers 57 targets on Windows.

### Reference typography quality hotfix

- Replaced the diagnostic 7x7 Arabic pixel-cell outlines with original continuous geometric contours, explicit joining edges, clearer bowls/loops/tails, stable baselines, larger dots and hamza/madda marks, and standalone base-Arabic coverage while preserving the owned presentation-form shaping contract.
- Rebuilt Pyramid Sans and Pyramid Arabic as 64-pixel signed-distance `.pfont` atlases and scaled them to stable logical UI sizes, reducing small-text stair stepping without changing layout dimensions.
- Changed the UI font-atlas sampler from nearest to linear filtering so grayscale CPU-rasterized coverage survives downscaling; solid UI geometry continues to use the atlas white texel.
- Added deterministic `scripts/regenerate-reference-fonts.py`, `Font.ReferenceTypography`, contextual-join/coverage/antialiasing assertions, and font-sampler regression coverage; CTest now registers 55 targets.

### Runtime asset path hotfix

- Added `Pyramid::Platform::GetExecutableDirectory()` and `ResolveRuntimePath()` so bundled assets are resolved beside the running executable instead of depending on the caller's current working directory.
- Fixed direct `BasicGame.exe` launches from repository roots, terminals, shortcuts, and external tools so the bundled Pyramid Sans/Arabic family loads instead of silently falling back to the ASCII debug atlas.
- Migrated `BasicRenderingExample` model lookup to the same runtime-path contract and added `Platform.RuntimePaths`; CTest now registers 54 targets.

### International text layout foundation

- Added an owned extended-grapheme segmenter for combining sequences, emoji ZWJ sequences, Hangul syllable clusters, and regional-indicator pairs; `TextBuffer` caret placement and deletion now preserve those boundaries.
- Added renderer-neutral international layout with paragraph-direction resolution, common Latin/Arabic/numeric bidirectional runs, logical/visual cluster maps, cluster-aware caret and hit testing, visual RTL movement, selection spans, and international word/CJK break opportunities.
- Added owned contextual Arabic presentation-form shaping for the core Arabic alphabet and mirrored RTL punctuation without FreeType, HarfBuzz, ICU, or other runtime middleware.
- Added ordered fallback font families that merge owned processed atlases into one renderer-ready atlas while preserving primary-font precedence, source-font lookup, and same-font kerning.
- Migrated retained UI labels and editors to cluster-aware international layout, including RTL natural alignment, password masking, wrapping, selection geometry, and caret placement.
- Added the Ruqoom-owned `Pyramid Arabic` source and deterministic processed atlas plus Arabic/mixed-direction validation text in `BasicGame`.
- Added `Text.International` and expanded UI text-editing coverage; CTest now registers 53 targets. Full Unicode bidi controls/isolates, complete UAX #14, OpenType GSUB/GPOS, general complex scripts, advanced Arabic ligatures/mark positioning, and native IME presentation remain future work.

### Unicode text editing and clipboard input

- Added platform-neutral committed/composition-ready text events to `Pyramid::Input`, including UTF-16 surrogate assembly, focus-safe reset behavior, and pre-action text-input consumption.
- Added the owned `Clipboard` abstraction, bounded UTF-32/UTF-16 conversion, and a Win32 `CF_UNICODETEXT` backend with line-ending normalization and explicit diagnostics.
- Added renderer-independent `Pyramid::Text::TextBuffer` editing with code-point cursor/selection indices, insertion/deletion, word and line selection, document/line navigation, single-line normalization, read-only mode, and character limits.
- Added retained `TextField`, `PasswordField`, `SearchField`, and `MultilineTextArea` widgets with click/drag selection, double-click word selection, triple-click line selection, caret/selection rendering, scrolling, clipboard shortcuts, submission, and Escape rollback.
- Text-focused UI now reserves character-producing and editing controls without swallowing unrelated function keys; committed Unicode remains accepted for AltGr-style keyboard layouts while gameplay actions such as camera movement are blocked.
- Added a modal `PROFILE SETTINGS` form to `BasicGame` and expanded public/package validation; CTest now registers 52 targets.

### Owned TrueType and processed font pipeline

- Added independently installable `PyramidFont`, exported as `Pyramid::Font`, with bounded SFNT/TrueType table parsing for `head`, `hhea`, `maxp`, `hmtx`, `loca`, `glyf`, `cmap` formats 4/12, `name`, and classic `kern` format 0.
- Added simple and compound quadratic glyph outlines, cycle/depth protection, CPU supersampled rasterization, deterministic atlas packing, metrics, kerning conversion, explicit unsupported/malformed rejection, and no FreeType/HarfBuzz/stb dependency.
- Added the checksummed version-1 `.pfont` runtime format plus `PyramidFontCompiler`; the bundled Ruqoom-owned `Pyramid Sans` source TTF and processed atlas are copied beside `BasicGame`.
- Extended `Pyramid::Text` with dynamic processed atlases, glyph lookup, kerning-aware measurement/wrapping, fallback accounting, and filesystem loading while retaining the embedded emergency debug atlas.
- Added runtime font selection to `Pyramid::UI`; `BasicGame` loads the processed font, falls back safely when unavailable, and reports font family, glyph count, atlas size, and memory in the F1 overlay.
- Added standalone `Pyramid::Font` package-consumer validation and focused parser/rasterizer/atlas/processed-asset tests; CTest now registers 48 targets.

### Retained game screens and runnable Windows outputs

- Added `Pyramid::UI::ScreenStack` with persistent screen ownership, opaque/transparent/modal presentation, deferred push/pop/replace/clear operations, deterministic enter/exit/update/build lifecycle, gameplay-input blocking, and delta-time transition state.
- Added reusable anchored/docked rectangle resolution, safe UI signals, full-frame modal overlays, and renderer-neutral nine-slice draw generation.
- Migrated `BasicGame` to a retained main menu, gameplay HUD, responsive selection/status panels, and a modal pause menu while preserving the F1 immediate debug overlay.
- Added `PYRAMID_BUNDLE_MINGW_RUNTIME`, a CMake runtime-copy target, install-time runtime deployment, and a PowerShell post-build verifier so `BasicGame.exe`, `BasicRenderingExample.exe`, and test executables run directly from `build/*/bin` without manually locating MinGW DLLs.
- Added `UI.GameScreens` lifecycle/layout/event/nine-slice coverage; CTest now registers 47 targets.

### UI diagnostics, text layout, and render-surface restoration

- Fixed the visible `BasicGame` corruption caused by off-screen passes leaving a shadow-map-sized viewport active: `RenderSystem` now restores the default framebuffer and main viewport after every pass, and `UIRenderer` establishes the final DPI-scaled surface viewport explicitly.
- Added strict owned UTF-8 decoding, fallback-glyph accounting, tab expansion, character/word wrapping, horizontal alignment, line spacing, and reusable line/glyph metrics to `Pyramid::Text`.
- Added colored and wrapped labels, persistent collapsing headers, clipped vertical scroll areas, optional scrollbars, wheel control, and stick-to-bottom behavior to `Pyramid::UI`.
- Added bounded thread-safe logger history with severity filtering, capacity control, and clearing for runtime diagnostics without a second logging system.
- Extended UI consumption into the raw RTS interaction path so pointer capture blocks edge scrolling, world selection, and command projection as well as named input actions.
- Reworked the `BasicGame` overlay into collapsible diagnostics, runtime controls, and a scrollable severity-colored log console; corrected the initial camera/floor framing.
- Expanded package, public-API, text, UI, input, RTS interaction, and renderer tests while preserving 46 registered CTest targets.

### Owned UI foundation and runtime debug overlay

- Added independently installable `PyramidText` and `PyramidUI` targets exported as `Pyramid::Text` and `Pyramid::UI`; both remain platform and renderer independent.
- Added a deterministic embedded ASCII debug atlas, glyph metrics, text measurement, multiline glyph-run generation, and a white atlas texel for solid UI geometry without runtime font files.
- Added hybrid UI contexts whose immediate widget facade reconciles retained element state, stable scoped IDs, vertical/horizontal flow layout, clipping, focus, pointer capture, keyboard navigation, DPI metadata, and batched renderer-independent draw lists.
- Added panels, labels, value rows, separators, spacers, buttons, checkboxes, float sliders, progress bars, and textured-image widgets with configurable themes.
- Added `InputConsumptionMask` and pre-action UI-context registration in `Game`, preventing handled mouse, wheel, drag, and focused keyboard controls from leaking into gameplay action contexts.
- Added graphics-device scissor operations plus `UIRenderer` for batched colored/textured quads, embedded-font rendering, conservative DPI-scaled clip rectangles, alpha blending, registered UI textures, and deterministic baseline-state restoration.
- Added resource-specific UI-renderer initialization rollback and non-canonical `ShaderCache::RemoveAlias()`, preserving unrelated cache-only shaders and textures when GPU buffer allocation fails.
- Added an F1 `BasicGame` overlay with live frame, render, resource, input, UI, animation, and camera controls.
- Added standalone installed `Pyramid::UI` consumer validation and focused `Text.DebugFont`, `UI.Context`, input-consumption, and `Graphics.UIRenderer` coverage; CTest now registers 46 targets.

### Build fixes

- Fixed `WindowResizeEventTests` after the input-library extraction by linking it to `Pyramid::Input`, which propagates the required input and foundation headers.

### Imported texture and material resources

- Extended `ModelResourceImporter` with a configurable graphics profile that maps renderer-independent MTL data to a caller-supplied shader, sampler settings, uniform names, render state, fallback material, and missing-texture policy.
- Added transactional `map_Kd` loading through `TextureCache`, immutable MTL material publication through `MaterialCache`, exact-content reuse, stable-ID conflict rejection, and dependency-safe rollback when a later texture, material, or mesh operation fails.
- Added non-canonical `TextureCache::RemoveAlias()` and `MaterialCache::RemoveAlias()` operations with generation invalidation for transaction rollback without removing canonical content.
- Added focused `Graphics.ModelMaterialResourceImport` coverage for MTL conversion, sRGB sampler settings, opacity blending, cache reuse, fallback materials, missing/malformed texture behavior, profile validation, stable-ID conflicts, and forced mid-import rollback.
- Migrated `BasicRendering` to render the bundled OBJ with its imported MTL diffuse texture and immutable imported material rather than manually creating a texture/material; CTest now registers 42 targets.

### Owned model import foundation

- Added the independently installable `PyramidModel` target exported as `Pyramid::Model`; it depends only on `Pyramid::Foundation` and `Pyramid::Math`.
- Added bounded dependency-free OBJ/MTL file and memory import with positive/negative indices, polygon triangulation, object/group/material primitive boundaries, vertex deduplication, vertex colors, source or generated normals, hard/smooth edges, bounds, quoted paths, normalized material/texture dependencies, common MTL properties, and structured diagnostics.
- Added explicit malformed-index, missing-library, duplicate-material, non-finite-data, empty-geometry, count, byte, and diagnostic failure behavior.
- Added transactional `ModelResourceImporter` publication through `MeshCache`, stable imported-mesh IDs, exact-content reuse, alias-conflict rejection, material-slot preservation, and rollback that leaves pre-existing resources intact.
- Added non-canonical `MeshCache::RemoveAlias()` with generation invalidation for transaction-safe publication.
- Replaced the hard-coded `BasicRendering` cube geometry with a bundled imported Pyramid OBJ/MTL asset copied beside the executable.
- Added a standalone installed `Pyramid::Model` consumer and Windows CI validation.

### Owned foundation, math, and input libraries

- Extracted public primitive types, colors, assertions, and logging into the independently installable `PyramidFoundation` target exported as `Pyramid::Foundation`.
- Extracted vectors, matrices, quaternions, and SIMD utilities into `PyramidMath`, exported as `Pyramid::Math` and dependent only on `Pyramid::Foundation`.
- Extracted platform-neutral physical input state and generic action mapping into `PyramidInput`, exported as `Pyramid::Input` and dependent only on `Pyramid::Foundation`.
- Preserved existing public include paths while removing Math, Input, and Utils source ownership from the `PyramidEngine` binary.
- Replaced the placeholder `Mat4::Determinant()` and `Mat4::Inverse()` implementations with pivoted elimination and added focused correctness tests.
- Added an installed foundation/math/input package consumer and expanded Windows CI package validation; CTest now registers 37 tests.

### Owned image library and dependency removal

- Extracted image dispatch, TGA/BMP subsets, PNG, DEFLATE/zlib, and JPEG decoding into the engine-independent `PyramidImage` target exported as `Pyramid::Image`.
- Replaced libjpeg-turbo with a Pyramid-owned bounded 8-bit Huffman JPEG decoder for baseline and progressive streams, grayscale/YCbCr RGB normalization, common 4:4:4/4:2:2/4:2:0 sampling, odd dimensions, and restart markers.
- Added explicit rejection for arithmetic-coded, lossless, hierarchical, 12-bit, and four-component JPEG variants plus malformed/truncated and numeric/allocation validation.
- Removed JPEG package discovery from CMake exports and removed libjpeg-turbo from MSYS2 bootstrap and all Windows CI matrices.
- Added standalone installed-package consumer validation for `Pyramid::Image` and expanded the image corpus; CTest now registers 35 tests.
- Locked GLAD as the sole approved bundled third-party runtime library; future functionality belongs in independently maintained Pyramid/Ruqoom libraries by default.

### RTS reference interaction layer

- Added `Pyramid::RTSReference`, a game-side support target that combines configurable named select/command actions with the generic strategy camera and scene-query APIs without adding RTS concepts to `Pyramid::Engine`.
- Added focus-safe, first-pointer-sample-aware edge scrolling with viewport margins, diagonal normalization, delta-time scaling, and optional camera-distance speed scaling.
- Added nearest selectable-entity ray picking, stable-ID selection state, destroyed-entity invalidation, configurable miss clearing, and one-shot command requests projected onto a validated plane.
- Migrated `BasicGame` to the strategy camera plus left-click selection, right-click commands, edge scrolling, WASD movement, middle-drag/Q/E orbit, wheel zoom, Shift boost, and selection/command diagnostics.
- Exposed `InputState::HasMousePosition()` so higher layers can distinguish a valid client-space pointer sample from the default coordinates.
- Added `Examples.RTSInteraction`; CTest now registers 33 tests.

### Reusable camera controllers

- Added a non-owning `CameraController` interface plus reusable free-fly, target-orbit, and XZ-ground-plane strategy controller implementations.
- Kept physical bindings outside the controller layer: every controller consumes configurable context/action references from `InputActionSystem`.
- Separated per-frame delta actions from rate actions so mouse deltas and held keyboard/controller axes remain frame-rate correct.
- Added finite settings validation, pitch/elevation and distance limits, home-pose capture/reset, synchronization from externally positioned cameras, controller enable/disable state, boost movement, panning, orbiting, and zooming.
- Added `OrbitCameraController` coverage and migrated both examples to action-driven controllers; `BasicGame` now uses the strategy controller as the Step 28 RTS interaction host.
- Added `Graphics.CameraControllers`; CTest now registers 32 tests.

### Generic input action mapping

- Added engine-generic button, one-dimensional, and two-dimensional named actions above the platform `InputState` snapshot.
- Added prioritized input contexts with enable/disable state, active-control consumption, deterministic ordering, and non-consuming observation contexts.
- Added keyboard, mouse-button, mouse-delta, and wheel bindings with scaling, axis selection, optional key/mouse-button chords, and runtime rebinding/removal.
- Integrated one `InputActionSystem` into `Game`; actions are evaluated after native message processing and before every `onUpdate()`.
- Migrated both examples to named actions. `BasicRendering` provides an RTS-style reference profile without placing RTS semantics in the engine module.
- Added `Input.ActionMapping`; CTest now registers 31 tests.

### Win32 input foundation

- Added platform-neutral keyboard and mouse enums plus per-frame `InputState` polling.
- Added held, pressed, and released transitions for keys and five mouse buttons.
- Added client-space pointer position, aggregated movement, and vertical/horizontal wheel deltas.
- Translated Win32 key, system-key, mouse, wheel, capture, and focus messages without exposing native codes publicly.
- Added left/right modifier and keypad-aware key translation.
- Added mouse capture while buttons are held and focus/capture-loss release behavior to prevent stuck input.
- Exposed window input through `Game::GetInput()` and migrated both examples away from simulated controls.
- Added `Platform.InputState`; CTest now registers 30 tests.

### Authoritative entity/component scene model

- Replaced the independent `SceneNode` graph and flat render-object authoring model with stable scene-local `EntityId` values and a lightweight, non-owning `Entity` facade.
- Added mandatory `TransformComponent` data plus optional `MeshRendererComponent` and `LightComponent` values, cycle-safe parent IDs, inherited visibility, recursive transform invalidation, safe scene-lifetime detection, and recursive entity destruction.
- Made entity transforms authoritative; rendering, lights, culling, and spatial queries now consume generated `RenderObject` and `Light` proxies.
- Upgraded `SceneSerializer` to version 2 with deterministic stable IDs, hierarchy, local transforms, mesh-renderer/light components, primary-light persistence, transactional validation, and exact resource-manifest references.
- Migrated `BasicGame`, package-consumer coverage, public API linkage, and focused tests to the entity model; renamed the hierarchy test to `Graphics.EntityScene`.
- Locked the product direction around a Windows-first RTS vertical slice, later Linux support, a full editor, and long-term Baa scripting/reimplementation.

### Versioned render-object scene serialization (superseded by scene format v2)

- Added dependency-free `SceneSerializer` persistence for scene names and flat render-object lists using a deterministic versioned text format.
- Added exact `ResourceManifest` key references for mesh and material handles, including direct-owner conversion through the live `ResourceRegistry`.
- Added transactional loading with transform, quaternion, visibility, shadow-flag, bounds-mode, manifest-key, resource-type, missing-asset, and stale-generation diagnostics.
- Added `Graphics.SceneSerialization` coverage for deterministic round trips, encoded names, manual/automatic bounds, direct and handle-backed resources, parser rejection, and registry validation.

### Versioned resource manifests

- Added dependency-free `ResourceManifest` serialization for typed mesh, shader, texture, and material handles using a deterministic versioned text format.
- Added transactional manifest parsing with schema, resource-type, key, asset-ID, generation, duplicate-key, and unsupported-version diagnostics.
- Added registry restoration reports that distinguish missing assets from stale generations without silently remapping serialized references.
- Added `Graphics.ResourceManifest` coverage for deterministic round trips, typed restoration, parser rollback, malformed data, missing assets, and stale generations.

### Typed generation-checked resource handles

- Added serializable, non-owning `MeshHandle`, `ShaderHandle`, `TextureHandle`, and `MaterialHandle` value types containing a stable asset ID and alias generation.
- Added handle-first registry acquisition, generation-checked resolution, liveness checks, eviction, shader recompilation, texture reload, and material replacement; stale handles resolve to `nullptr` instead of silently adopting replacement content.
- Added persistent per-alias generation tombstones to all four caches so eviction, collection, clearing, direct cache mutation, and alias replacement invalidate previously issued handles even when the same stable ID is later reused.
- Added handle-backed `RenderObject` mesh/material references and renderer resolution through the registered `ResourceRegistry`; cached mesh bounds preserve culling data without retaining a resource owner.
- Added `Graphics.ResourceHandles` coverage for typed identity, non-owning lifetime, direct-cache invalidation, replacement generations, forged/stale handle rejection, scene integration, and registry clearing.

### Central graphics resource registry

- Added `ResourceRegistry`, the authoritative owner of mesh, shader, texture, and material caches for one graphics device.
- Added dependency-safe `CollectUnused()` and `Clear()` passes that release materials before their texture/shader dependencies and expose combined cache statistics.
- Integrated one registry into `Game`; it is created with the graphics device, exposed through `GetResourceRegistry()`, and destroyed before device shutdown while the native context is still valid.
- Migrated both graphical examples and the package consumer to the central registry and added `Graphics.ResourceRegistry` lifecycle, collection, external-owner, and aggregated-statistics coverage.

### Stable material identifiers and resource caching

- Added `MaterialCache`, which owns one resident immutable material per exact shader/texture/uniform/render-state content fingerprint and resolves multiple stable aliases to it.
- Added stable-ID/content conflict detection, strong residency, explicit `Evict()`, `CollectUnused()`, and `Clear()` lifetime controls, plus material-cache activity and residency statistics.
- Added transactional `MaterialCache::Replace()`: replacement content is validated or resolved completely before a caller-defined stable alias changes, failure preserves the previous material, and already resident replacement content is reused.
- Migrated both graphical examples to material-cache acquisition and added `Graphics.MaterialCache` coverage for exact-content sharing, alias conflicts, replacement, failure preservation, external-owner lifetime, statistics, collection, and eviction.

### Engine-owned material resources

- Added deterministic 128-bit `MaterialAssetId` values from caller-owned stable names or immutable shader, texture, uniform, and render-state content.
- Added immutable `Material` resources that own one graphics `ShaderProgram`, sorted `TextureResource` bindings, typed scalar/vector/matrix uniforms, and backend-independent blend/depth/cull/polygon state.
- Added command-buffer material and uniform commands so static material values, texture bindings, state, and dynamic per-draw matrices are applied in execution order.
- Replaced the ad-hoc `Renderer::Material` value struct with `RenderObject::material` ownership and migrated forward/deferred rendering plus both graphical examples.
- Added `Graphics.MaterialResource` coverage for deterministic identity, order-independent content hashing, validation, state/texture/uniform application, and dynamic command-buffer uniforms.

### Stable texture identifiers and resource caching

- Added deterministic 128-bit `TextureAssetId` values from caller-owned stable names or exact decoded pixels plus immutable texture, sampler, mip, and color-space state.
- Added immutable `TextureResource` objects for memory and file sources, with explicit linear/sRGB identity, estimated residency metadata, and mutation rejection that preserves cache identity.
- Added graphics-device-bound `TextureCache` deduplication across stable aliases, conflict detection, strong residency, explicit eviction/collection, and cache statistics.
- Added transactional file reload: decode and GPU creation complete before a stable alias changes, failed reload preserves the previous valid texture, and already resident replacement content is reused without another upload.
- Migrated both graphical examples to stable texture-cache identifiers and added `Graphics.TextureCache` coverage for identifiers, memory/file deduplication, color-space identity, conflicts, reload, failure preservation, residency, and collection.

### Stable shader identifiers and program caching

- Added deterministic 128-bit `ShaderAssetId` values from caller-owned stable names or exact graphics/compute stage source sets; debug names are excluded from content identity.
- Added immutable `ShaderProgram` resources that implement `IShader`, validate legal stage combinations, retain stage/source metadata, and reject direct mutation that would invalidate their identity.
- Added graphics-device-bound `ShaderCache` compile-once reuse across aliases, stable-ID/source conflict detection, strong residency, explicit eviction/collection, and cache statistics.
- Added transactional `ShaderCache::Recompile()`: replacements compile or resolve completely before a stable alias changes, failed compilation preserves the previous program, and already resident replacement source is reused without another compilation.
- Migrated both graphical examples to stable shader-cache identifiers and added `Graphics.ShaderCache` coverage for deterministic IDs, stage routing, forwarding, deduplication, conflicts, recompilation, failure preservation, reuse, and lifetime behavior.

### Stable mesh identifiers and resource caching

- Added deterministic 128-bit `MeshAssetId` values from caller-owned stable names or exact mesh content. Content fingerprints include vertex/index bytes, layout semantics, counts, normalization flags, and primitive topology while excluding debug names.
- Added `MeshCache`, which owns one resident GPU mesh per unique content fingerprint and resolves any number of stable asset-ID aliases to that upload.
- Added hard conflict detection when one explicit asset identifier is reused for different resident geometry.
- Added explicit `Evict()`, `CollectUnused()`, and `Clear()` lifetime controls; cache eviction releases cache ownership without invalidating external `shared_ptr<Mesh>` instances.
- Added mesh/cache residency statistics for hits, misses, creations, failures, conflicts, evictions, identifier aliases, externally referenced resources, and resident geometry bytes.
- Migrated both examples to the cache and added `Graphics.MeshCache` coverage for deterministic IDs, alias deduplication, upload counts, conflicts, residency, collection, eviction, and external-owner lifetime.

### Engine-owned mesh resources

- Added `Mesh` and `MeshSpecification` as the authoritative geometry resource for vertex/index ownership, layout, draw count, primitive topology, and immutable local bounds.
- Replaced `RenderObject::vertexArray` with `RenderObject::mesh`; forward, deferred, shadow, and example rendering now submit mesh metadata rather than inspecting raw vertex-array state.
- Added indexed and non-indexed point/line/triangle topology support to graphics-device and command-buffer draw paths.
- Added strict mesh validation for vertex byte counts, position semantics, finite bounds, topology counts, and index ranges.
- Split command-buffer implementation from render-system ownership and added `Graphics.MeshResource` coverage.

### Geometry-derived render bounds

- Added automatic `RenderObject` local bounds derived from CPU-visible vertex data and position semantics.
- Added manual/automatic bounds modes, a reusable mesh-bounds utility, and focused geometry-bounds coverage.
- OpenGL vertex buffers cache derived bounds and retain vertex bytes only until their layout is attached, avoiding a permanent duplicate of mesh data.
- Scene culling and spatial queries now consume geometry-derived bounds by default while preserving explicit manual overrides and the unit-cube fallback.

### Octree compaction and health metrics

- Added automatic bottom-up octree compaction after removals and object relocation. Empty descendant branches collapse, and subtrees that fit within one node's capacity are promoted safely before child nodes are released.
- Added `Octree::Compact()`, `OctreeCompactionStats`, last-compaction inspection, and compaction results on `OctreeSyncStats`.
- Expanded `OctreeStats` with internal/leaf/empty/occupied node counts, tracked versus stored object counts, occupied depth, configured depth, node occupancy, leaf utilization, empty-leaf ratio, and approximate memory usage.
- Batched synchronization now performs one compaction pass after all movement and removal changes instead of repeatedly compacting per object.
- Added `Graphics.OctreeCompaction` coverage for removal-heavy collapse, movement-driven cleanup, synchronization statistics, no-op compaction, object preservation, and health-metric consistency.

### Transactional octree configuration

- Added `OctreeConfiguration`, `Octree::Configure()`, and `GetConfiguration()` for one atomic bounds/depth/capacity update.
- Reworked octree reconfiguration to build a complete replacement tree first and swap it in only after every tracked object has been reinserted successfully.
- Preserved the previous tree and tracked-object map when configuration validation or replacement construction fails.
- Rejected non-finite centers and non-positive extents, normalized zero node capacity to one, and normalized invalid constructor extents to usable positive values.
- Made individual bounds, depth, and capacity setters delegate to the same validated configuration path.
- Added `Graphics.OctreeConfiguration` coverage for tracked-object preservation, root-overflow objects, invalid settings, constructor normalization, and combined updates.

### Bounds-aware nearest-neighbor queries

- Corrected nearest-object and K-nearest queries to measure distance to the closest point on each complete world-space AABB rather than object origins.
- Added public AABB point-distance helpers and `SceneManager::GetKNearestObjects()`.
- Added nearest-first K-result ordering, zero-count handling, hidden gameplay-object inclusion, and octree/linear parity.
- Added best-first child traversal with AABB lower-bound pruning so branches that cannot improve the current result set are skipped.
- Added `Graphics.NearestQueries` coverage for bounds distance, root-overflow objects, K limits, ordering, and scene-manager parity.

### Bounds-accurate spatial queries

- Corrected point, sphere, box, and ray octree queries to test complete world-space object AABBs instead of object centers or node membership.
- Preserved root-retained objects outside configured octree bounds while keeping child-node pruning.
- Canonicalized reversed AABB endpoints and rejected negative sphere radii, zero-length ray directions, and negative ray distances.
- Deduplicated spatial-query results and ordered ray hits from nearest to farthest.
- Made `SceneManager::QueryScene()` rebuild before the first query, match octree and linear fallback semantics, and return ordered ray-hit distances.
- Added `Graphics.OctreeQueries` coverage for point/sphere/box/ray intersections, root-overflow objects, invalid inputs, hidden gameplay objects, result uniqueness, and spatial/linear parity.

### Incremental octree synchronization

- Added `Octree::Synchronize()` to incrementally insert new objects, remove stale objects, and relocate only objects whose world-space AABBs changed.
- Added `Octree::UpdateIfMoved()`, tracked-object inspection, duplicate-insertion protection, and floating-point tolerance for stable bounds.
- Replaced `SceneManager::UpdateSpatialPartition()` full-tree rebuilds with incremental synchronization after the initial scene build.
- Added per-update spatial insertion, removal, movement, and unchanged-object statistics.
- Hardened octree removal so duplicate legacy entries cannot survive an update.
- Added `Graphics.OctreeUpdates` coverage for movement across branches, bounds-only changes, additions, removals, duplicate snapshots, and stable-scene no-op updates.

### Camera frustum and scene visibility

- Corrected camera orientation conventions so OpenGL forward is local negative Z and view matrices use inverse camera rotation.
- Added robust `LookAt()` handling for zero-length and collinear-up inputs.
- Extracted and normalized six inward-facing world-space frustum planes from the view-projection matrix.
- Added public frustum-plane access plus accurate point, sphere, and AABB visibility tests.
- Added explicit local bounds to `RenderObject` and transformed all eight corners into world-space AABBs.
- Replaced center-point scene visibility with bounds-aware culling.
- Implemented octree node/object frustum rejection, safe storage for objects spanning child boundaries, hidden-object filtering, and camera-independent spatial rebuilds.
- Made scene-manager frustum enable/disable behavior consistent across octree and linear paths.
- Added `Graphics.CameraFrustum` coverage for perspective, orthographic, translated, rotated, scene, scene-manager, and octree classifications.

### Scene transform hierarchy

- Added hierarchy-wide world-transform invalidation for every local transform and parent change.
- Added cycle rejection, duplicate-child prevention, safe detach/reparent behavior, unmanaged-node guards, and cleanup for externally retained children when a parent is destroyed.
- Normalized local and composed world rotations before matrix use.
- Cached world rotation and effective basis scale alongside the world matrix.
- Added parent/children accessors plus point and direction conversion to world space.
- Added `Graphics.EntityScene` coverage for multi-generation TRS composition, cache invalidation, reparenting, detachment, cycle rejection, and destruction behavior.

### Image and texture loading

- Replaced JPEG test-pattern generation with real baseline and progressive JPEG decoding through libjpeg-turbo.
- Normalized JPEG output to tightly packed RGB pixels and added invalid-data, size-overflow, allocation, and scanline failure handling.
- Removed the unused custom JPEG entropy/IDCT/color-conversion pipeline and its misleading public headers/tests.
- Made file-backed OpenGL texture replacement transactional so failed reloads preserve the previous valid GPU object.
- Added explicit texture load state/error reporting, RGB/RGBA format validation, sRGB internal formats, complete mip-filter mapping, border-color parameters, and unpack-alignment restoration.
- Added validated baseline/progressive JPEG fixtures and `Graphics.TextureLoading` coverage for upload state, failed reload preservation, and data-size checks.
- Added libjpeg-turbo to MSYS2 bootstrap, CI, installed package dependencies, and external-consumer resolution.

### Framebuffer resize safety

- Made `OpenGLFramebuffer::Resize()` transactional so failed replacement creation preserves the last valid framebuffer and attachments.
- Added zero-sized extent guards, structural specification validation, duplicate attachment detection, and consistent multisample validation.
- Centralized framebuffer resource cleanup and fixed leaked depth/stencil attachment objects during invalidation.
- Applied each attachment's declared filtering and wrapping parameters during texture creation.
- Replaced the renderer's duplicate raw `RenderTarget` framebuffer implementation with the shared `OpenGLFramebuffer` lifecycle.
- Added `RenderTarget::Resize()`, `RenderPass::Resize()`, and `RenderSystem::Resize()` propagation.
- Added `Game::SetRenderSystem()` so registered window-sized render targets follow valid window resize events automatically.
- Updated deferred G-buffer resizing to preserve the existing buffer if replacement fails.
- Added framebuffer specification and zero-area resize coverage.

### Resize-safe rendering

- Propagated renderable window resize events to the default OpenGL viewport.
- Added `Camera::SetViewportSize()` for perspective and orthographic projection updates.
- Added `Game::SetActiveCamera()` so one non-owning camera follows the window client size automatically.
- Suspended rendering and presentation while the window is minimized or has a zero-sized client area.
- Registered both examples with the active-camera resize path.
- Added camera aspect, projection invalidation, orthographic resize, and zero-area tests.

### Window events

- Added a platform-neutral `WindowResizeEvent` with restored, minimized, and maximized states.
- Added replaceable resize callbacks to the `Window` base interface.
- Translated Win32 `WM_SIZE` messages into deduplicated engine resize events.
- Added the overridable `Game::onWindowResize()` hook and detached it safely during shutdown.
- Added focused callback, replacement, detach, state, and renderable-area tests.

### Rendering diagnostics

- Added a centralized OpenGL diagnostics module for draining and reporting all pending errors.
- Enabled synchronous driver debug callbacks in Debug builds when the current context supports them.
- Requested a Win32 OpenGL debug context in non-Release builds.
- Added readable source, type, severity, and error-name mapping plus focused unit coverage.
- Suppressed high-volume notification messages while preserving warnings and errors.

### Next priorities

- Windows runtime verification for Debug and Release.
- Texture-format and depth-target completion.
- Geometry-derived local bounds during mesh import.

## [0.6.0-pre-alpha] - 2026-07-21

### Build and packaging

- Synchronized project metadata at `0.6.0-pre-alpha`.
- Raised the supported CMake workflow to 3.23+.
- Added an explicit Windows-only platform guard.
- Removed stale `Game`, dependency, tool, and empty subsystem build entries.
- Replaced the Visual Studio presets with Ninja presets for MSYS2 UCRT64 GCC and Clang.
- Added GCC/Clang `-Wall -Wextra -Wpedantic` defaults.
- Added PowerShell helpers for installing and invoking the open-source MinGW toolchain.
- Added relocatable CMake package exports for `Pyramid::Engine` and GLAD.
- Added an external `find_package` consumer.
- Added MSYS2 UCRT64 CI for GCC and Clang Debug/Release builds, tests, install, and consumer validation.

### API reliability

- Converted optional window no-ops into required operations and implemented them for Win32.
- Added definitions for texture size/render-target/color factories.
- Made unsupported depth texture creation fail explicitly.
- Added scene persistence failure paths instead of unresolved symbols.
- Implemented scene box queries, visibility statistics, events, debug statistics, and spatial test-scene creation.
- Completed declared octree operations and spatial helper definitions.
- Removed public transparent, post-process, UI, debug, and factory pass declarations that had no implementations.
- Added `API.PublicApiLinkage` to catch selected missing definitions.

### Rendering and examples

- Standardized bundled examples on GLSL 3.30.
- Required an OpenGL 3.3-or-newer core context and removed legacy-context fallback.
- Added Win32 size tracking and guarded swap-interval use.

### Tests and correctness

- Registered `TestPNGLoader` and `TestJPEGParser` with CTest.
- Corrected invalid PNG CRC/zlib fixture data.
- Corrected the zlib Adler-32 fixture.
- Corrected the JPEG DHT segment length fixture.
- Fixed floating-point absolute-value usage in the IDCT test.
- Fixed a missing `<cstring>` include in image loading.
- Verified 11 maintained tests with zero failures in the non-graphical audit environment.
- Verified all non-Windows engine objects link together under whole-archive linkage.

### Documentation

- Updated all maintained documentation for the new version, OpenGL minimum, test set, package workflow, removed placeholders, and remaining limitations.

## Historical milestones

### 0.6 scene-management milestone — 2025-07

Introduced scene management, octree structures, AABB helpers, spatial-query APIs, and scene statistics. Several algorithms were initially placeholders and remain tracked in the roadmap.

### 0.4 image-processing milestone — 2025-07

Introduced custom TGA, BMP, PNG, zlib/DEFLATE, and JPEG helper components. PNG is operational for the tested subset; the original incomplete JPEG reconstruction path was later replaced by libjpeg-turbo.

### 0.3.9 logging milestone — 2025-06

Moved logging into Utils and added severity filtering, file rotation, structured fields, assertions, and thread synchronization.

### 0.3.8 graphics-resource milestone — 2025-05

Added buffer layouts, shaders, uniforms, textures, assertions, and expanded example rendering.

### 0.3.3 foundation milestone — 2025-01

Established the initial CMake project, Win32/OpenGL application loop, graphics device, math, and examples.
