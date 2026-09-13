# Architecture

## Scope

Pyramid is a C++17 engine ecosystem with a Win32/WGL `Pyramid::Engine` target and independently testable owned libraries. `Pyramid::Foundation`, `Pyramid::Math`, `Pyramid::Input`, `Pyramid::Image`, and `Pyramid::Model` are exported as separate relocatable packages, so tools that only parse images or models do not inherit OpenGL, GLAD, or platform dependencies.

## Layers

| Layer | Namespace | Responsibility |
|---|---|---|
| Core | `Pyramid` | Application lifecycle, common types, and graphics API selection |
| Platform | `Pyramid` | Window, message pump, native input translation, and OpenGL context |
| Input | `Pyramid` | Named actions, bindings, prioritized contexts, consumption, and rebinding |
| Graphics resources | `Pyramid` | Device, buffers, arrays, immutable meshes, mesh/shader caches, textures, framebuffers, camera, and optional camera controllers |
| Renderer | `Pyramid::Renderer` | Command recording, materials, render targets, and render passes |
| Scene | `Pyramid` | Stable-ID entities, hierarchical transforms, optional components, renderer proxies, and environment |
| Spatial management | `Pyramid::SceneManagement` | Scene manager, octree, AABB, and query helpers |
| PyramidFoundation | `Pyramid`, `Pyramid::Util` | Primitive aliases, colors, assertions, and logging |
| PyramidMath | `Pyramid::Math` | Vectors, matrices, quaternions, geometry, and SIMD helpers |
| PyramidInput | `Pyramid` | Physical input snapshots and generic named action mapping |
| PyramidImage | `Pyramid::Util` | Standalone image dispatch, TGA/BMP subsets, PNG, DEFLATE/zlib, and owned JPEG decoding |
| PyramidModel | `Pyramid::Model` | Renderer-independent imported-model data and bounded OBJ/MTL parsing |
| RTS reference | `Pyramid::Examples::RTSReference` | Game-side edge scrolling, selectable filtering, ray picking, and command requests; not installed with `Pyramid::Engine` |

Direct Win32 keyboard/mouse polling, engine-generic action mapping, reusable free-fly/orbit/strategy camera controllers, and a separate game-side RTS interaction reference are present. Audio, physics, editor, scripting, and the asset pipeline are not currently present.

## Application lifecycle

1. `Game` creates `Win32OpenGLWindow` for `GraphicsAPI::OpenGL`.
2. The window creates a temporary WGL context to load extensions.
3. It attempts core contexts from OpenGL 4.6 down to 3.3.
4. Context creation fails if no OpenGL 3.3-or-newer core context is available.
5. `IGraphicsDevice::Create` creates `OpenGLDevice`.
6. `Game` attaches a platform-neutral resize callback after window/device construction.
7. `Game::run()` calls `onCreate()`, processes messages, computes clamped delta time, evaluates input-action contexts, updates, renders, and shuts down.

Derived `onCreate()` implementations must call `Game::onCreate()` before creating graphics resources.

## Window contract

`Window` is a strict interface. Initialization, presentation, context activation, close state, title, size, position, visibility, and minimized/maximized queries are all required operations.

`Pyramid::Platform::GetExecutableDirectory()` and `ResolveRuntimePath()` define runtime asset location independently of the process current working directory. Build and install rules place executable-owned assets beside the binary; the resolver prefers that location and retains the working directory only as a development fallback. Examples and future launchers must use this contract rather than duplicate Win32 path lookup or assume a terminal location.

The base interface owns a replaceable resize callback and emits platform-neutral `WindowResizeEvent` values. `Win32OpenGLWindow` maps `WM_SIZE` to restored, minimized, or maximized states, updates its cached client dimensions before delivery, and suppresses duplicate events. During message processing, `Game` updates the default viewport, active camera, and registered render system before forwarding delivery to `onWindowResize()` on the game thread. Minimized and zero-sized events suspend rendering without recreating GPU targets.

## Input lifecycle

`InputState` is platform-neutral and owned by the native window. At the start of each `Window::ProcessMessages()` call, transient press/release flags, pointer deltas, and wheel deltas are cleared. Win32 then translates `WM_KEY*`, `WM_MOUSE*`, capture, and focus messages into the state object.

Held states persist across frames. Repeated native key-down messages do not create repeated presses. Focus loss releases every held key and mouse button, clears mouse motion, and resets the next pointer sample baseline. After message processing, `Game` evaluates its `InputActionSystem` before `onUpdate()`. Named contexts map the snapshot to button, one-dimensional, and two-dimensional actions. Context priority and control consumption allow modal UI/editor/gameplay layers without embedding game-specific concepts in the platform backend. The controller layer consumes configurable context/action references and does not create physical bindings. Separate delta and rate action references preserve correct semantics for mouse movement versus held axes. `BasicGame` demonstrates the strategy controller plus the separate `Examples/RTSReference` layer for edge scrolling, selection, and command requests; `BasicRendering` remains the lower-level strategy-camera rendering reference. Pointer validity is explicit through `InputState::HasMousePosition()`, preventing edge motion or click rays before the first client-space sample. RTS action names, selectable policy, and command semantics remain outside the engine library.

## Graphics device

`IGraphicsDevice` is the backend-neutral resource and draw interface. Only its OpenGL implementation exists. DirectX and Vulkan enum values are reserved and return no device.

OpenGL resources use RAII wrappers, but raw pointers passed into binding and command APIs are non-owning. Their owners must outlive command execution.

## Renderer

`RenderSystem` records and executes command buffers. The public render-pass set contains only implemented types:

- `ForwardRenderPass`;
- `ShadowMapPass`;
- `DeferredGeometryPass`;
- `DeferredLightingPass`.

The default pipeline uses shadow and forward rendering. The deferred setup uses shadow, geometry, and lighting passes.


`ResourceRegistry` is the application-level owner of mesh, shader, texture, and material caches for one graphics device. `Game` constructs it immediately after device creation and explicitly destroys it before graphics-device shutdown. Registry maintenance runs in dependency order—materials, textures, shaders, then meshes—so material-held references are released before dependent GPU resources are considered unused. Reverse member destruction preserves the same order even after an explicit clear. External `shared_ptr` owners remain valid across registry eviction or replacement. Typed resource handles provide a second, non-owning reference model: each handle stores an asset ID plus the cache alias generation that was current when issued. Every alias bind, remap, removal, collection, clear, or direct cache mutation advances a persistent generation tombstone, so stale serialized handles cannot resolve to newly created content under a reused stable ID. `ResourceManifest` provides the persistence boundary for those handles: deterministic versioned text stores exact typed IDs and generations, transactional parsing rejects unsupported or malformed data, and registry restoration reports missing assets separately from stale aliases. `SceneSerializer` builds on that boundary for deterministic entity-scene persistence: version 2 stores stable IDs, hierarchy, local transforms, optional renderer/light components, and exact resource-manifest keys; loading validates exact generations before publishing a complete replacement scene.

`ShaderProgram` wraps one immutable compiled `IShader` with stable caller/content identifiers and stage metadata. `ShaderCache` is bound to one graphics device, compiles exact source sets once, shares them across aliases, rejects stable-ID/source conflicts, and keeps strong residency until collection or eviction. Transactional recompilation creates or resolves a replacement program before remapping one stable alias; failed compilation preserves both the old cache mapping and all existing external owners. External users reacquire the stable alias to adopt a successful replacement.

Known constraints:

- compute `Dispatch` commands are logged but not executed;
- generic framebuffer binding outside the OpenGL renderer remains incomplete;
- shadow-map resolution is intentionally independent of window size;
- render statistics do not yet represent complete GPU execution metrics.

## Scene and spatial management

`Scene` is the authoritative hybrid entity/component model. Each entity owns a non-zero, scene-local `EntityId`, a name, a visibility flag, and exactly one `TransformComponent`. `MeshRendererComponent` and `LightComponent` are optional. Parent-child relationships are stored by entity ID; there is no independent `SceneNode` graph.

Local transforms are authoritative. World transforms are cached with parent-before-local composition and recursively invalidated after local edits, reparenting, detachment, or ancestor changes. Parenting rejects self-links, duplicate children, missing entities, and cycles. Destroying an entity recursively destroys its descendants. Visibility is inherited through the hierarchy.

`RenderObject` and `Light` are renderer/spatial proxies generated from entity components. They are not the authoring model. Mesh-renderer proxies receive the entity world matrix, effective visibility, immutable mesh/material handles, shadow flags, and component bounds. Light proxies receive world position/orientation and `LightComponent` data. Transitional raw-proxy import helpers remain for low-level tests, but new code should create entities and attach components.

`SceneSerializer` version 2 stores stable entity IDs, parent IDs, names, local transforms, visibility, mesh-renderer components, light components, and the primary-light entity. Mesh and material references use exact `ResourceManifest` keys and generation-checked registry handles. Parsing and scene construction are transactional; malformed data, missing parents, hierarchy cycles, missing assets, and stale generations prevent publication. Version 1 flat render-object scenes are intentionally rejected during this pre-alpha transition.

`SceneManager` consumes the scene's generated render proxies for frustum culling, octree synchronization, and spatial queries. Query results also expose the corresponding entities. The octree operates on complete world AABBs, updates moved objects incrementally, compacts empty branches, and supports transactional configuration plus health metrics. Renderer proxies must be refreshed through the scene rather than treated as a second transform authority.

Legacy `SceneManager::LoadScene` and `SaveScene` JSON/XML/Binary methods remain unsupported; use `SceneSerializer`. Cameras, environment settings, gameplay components, and editor metadata are future scene-format extensions. Occlusion culling remains unimplemented and disabled by default.

## Model import

`Pyramid::Model` owns renderer-independent imported-model data and the dependency-free OBJ/MTL parser. It depends only on foundation and math. OBJ parsing supports positive and negative indices, quoted material-library paths, polygon fan triangulation, vertex-color extensions, object/group/material primitive boundaries, deduplication, source normals, generated area-weighted smoothing normals, hard edges when smoothing is disabled, bounds, and configurable allocation/diagnostic limits. MTL parsing records ambient/diffuse/specular colors, opacity, shininess, illumination model, and normalized diffuse-texture paths. Missing declared libraries and malformed references fail explicitly by default.

The engine-facing `ModelResourceImporter` is the graphics bridge for renderer-independent `ImportedModel` data. Its mesh-only path validates primitives and stable IDs before publication through `MeshCache`. Its complete path consumes a caller-defined shader/material profile, loads MTL diffuse images through `TextureCache`, maps imported properties to typed immutable `Material` uniforms/render state, and returns mesh/material/texture handles for each primitive. Exact resident content is reused, stable-ID conflicts fail explicitly, and rollback removes only non-canonical aliases and content introduced by the failed transaction. The importer does not select a shader policy, mutate the CPU model package, create entities, or instantiate a scene hierarchy.

## Text and UI

`Pyramid::Font` owns bounded SFNT/TrueType parsing, Unicode-to-glyph mapping, simple and compound quadratic outlines, classic kerning, coverage and signed-distance rasterization, deterministic atlas packing, content-addressed processed-font caching, and the checksummed versioned `.pfont` runtime format. It depends only on `Pyramid::Foundation`; source-font parsing, caching, and offline compilation never enter the UI or graphics layers. Version 2 records the raster mode and distance range while loading legacy version-1 coverage assets. The first parser supports TrueType `glyf` outlines and explicitly rejects unsupported CFF/OpenType outlines, collections, variable/color fonts, WOFF, and malformed tables.

The platform layer may expose bytes from fonts legally installed on the host. The Win32 implementation uses GDI only to select and extract an exact TrueType face; a substituted family is rejected. `Pyramid::Font` still performs parsing, validation, rasterization, deterministic caching, and processed-asset publication. `BasicGame` uses bounded script-specific bakes and coverage gates: Segoe UI/Tahoma/Arial for Latin, Cairo when installed followed by Tahoma and other compatible Arabic faces, and Segoe UI Symbol for icons. The ordered installed family falls back to the bundled Ruqoom-owned reference faces without bundling another party's font or introducing font middleware.

`Pyramid::Text` consumes baked font data and owns strict UTF conversion, extended-grapheme segmentation, grapheme-safe logical editing, ordered fallback-family construction, common Latin/Arabic/numeric bidirectional runs, contextual Arabic presentation-form shaping, international word/CJK break opportunities, and renderer-neutral glyph/cluster/caret/selection geometry. The family builder merges processed sources into one atlas for the existing renderer while retaining source-font identity and primary-font precedence. The embedded ASCII atlas remains an emergency diagnostic fallback. This is a deliberately bounded owned foundation rather than a claim of full UAX/OpenType conformance: explicit bidi controls/isolates, complete UAX #14, GSUB/GPOS, general complex scripts, advanced Arabic ligatures/mark positioning, and native IME composition remain later milestones.

`Pyramid::UI` is a hybrid retained/immediate runtime. Widget calls reconcile stable scoped IDs into retained element state, then shared layout, focus, hit-testing, pointer capture, nested clipping, persistent collapsing/scroll state, theme resolution, and draw generation produce a renderer-independent `UI::DrawList`. Logical item rectangles remain stable inside scroll areas while visible retained rectangles, hit testing, and generated batches are intersected with the active clip. Multiple contexts can coexist for debug, game, and future editor surfaces without parallel widget implementations.

`UI::ScreenStack` is the application-level retained routing layer. It owns persistent screens, distinguishes opaque, transparent, and modal presentation, defers structural changes requested during callbacks, and exposes whether the active screen blocks gameplay. Anchors and incremental docking resolve responsive rectangles without introducing a CSS engine; the same draw list supports nine-slice composition for future themed controls.

The native window backend feeds physical controls and a separate committed/composition-ready Unicode text stream into `InputState`. UTF-16 surrogate assembly and focus-loss cleanup remain platform-neutral; `Clipboard` is an input-package service whose Win32 implementation uses bounded `CF_UNICODETEXT` conversion. UI owns no native clipboard calls.

`Pyramid::UI` retains one `TextBuffer` per editable widget and exposes `TextField`, `PasswordField`, `SearchField`, and `MultilineTextArea`. Pointer hit testing uses glyph advances, and draw generation adds selection rectangles, placeholder text, password masking, scrolling, and a blinking caret. Clipboard shortcuts and editing commands mutate the retained buffer transactionally; Escape restores the focus snapshot.

Before named action contexts evaluate, `Game` asks each registered UI context for an `InputConsumptionMask`. Controls handled by UI are seeded into `InputActionSystem`, so clicks, drags, wheel input, committed text, character-producing keys, and focused editing/navigation keys do not leak into gameplay. `Game` also exposes that merged mask to game-side systems that intentionally read raw physical input; the RTS reference uses it to block edge scrolling, selection, and commands while UI owns the pointer. UI libraries never depend on `Game`; registration is an engine integration convenience.

`UIRenderer` is the engine graphics adapter. It owns the UI shader, linearly sampled font-atlas texture, dynamic buffers, texture-ID registration, alpha-blended batching, the explicit final-surface framebuffer/viewport, conservative DPI-scaled scissor rectangles, and deterministic baseline-state restoration. Coverage atlases retain normal alpha sampling. Signed-distance atlases pass a mode and optical-weight bias per vertex; the fragment shader uses screen-space derivatives and `smoothstep` so one 64-pixel source remains crisp across ordinary UI sizes and DPI scales. `Pyramid::UI::Typography` provides heading, body, label, button, input, and caption roles without coupling layout code to OpenGL. Initialization is transactional: failures remove only shader/texture aliases and content introduced by that attempt while preserving unrelated or previously resident cache resources. `Pyramid::UI` never includes GLAD or issues graphics calls.

Every render pass may bind its own framebuffer and viewport. `RenderSystem` therefore rebinds framebuffer zero and restores the main render extent after each pass; final direct overlays establish their own physical surface viewport again. This prevents fixed-resolution shadow maps and other off-screen targets from corrupting subsequent world framing or UI clipping.

## Textures and framebuffers

The direct specification and file constructors create mutable `OpenGLTexture2D` instances. Shared sampled assets should use immutable `TextureResource` objects through a graphics-device-bound `TextureCache`. Content identity includes decoded pixels, dimensions, base format, mip policy, sampler state, and explicit linear/sRGB intent. Stable aliases map to strong resident resources; exact content is uploaded once across aliases, conflicts are rejected, and file reload publishes a replacement only after decode and GPU creation succeed. Existing external owners remain valid after reload or eviction.

Material resources sit above shader and texture resources. A `Material` stores one immutable graphics shader, sorted texture/sampler bindings, typed static uniforms, and backend-independent fixed render state. `RenderObject` owns a shared material reference; command buffers apply it at execution time before recording dynamic per-draw uniforms and mesh submission. This keeps content identity stable while avoiding the previous mismatch where uniforms were updated before the queued shader bind executed. `MaterialCache` deduplicates exact content across stable aliases and publishes replacement mappings only after the new material has been fully validated or resolved, while external owners retain older versions safely.


Size-based, render-target, and solid-color convenience factories remain available for intentionally uncached textures. `OpenGLTexture2D` maps every 3.3-core advertised format with a type-correct upload triple: float color (`RGB16F`, `RGBA16F`, `RGB32F`, `RGBA32F`) uploads with `GL_FLOAT`, sized depth (`Depth16`, `Depth24`, `Depth32F`) with its matching unsigned or float type, and packed depth-stencil (`Depth24Stencil8`, `Depth32FStencil8`) with the combined depth-stencil type. Single-channel formats (`R8`, `R16F`, `R32F`) upload through `GL_RED`; sampling a single-channel texture reads as `(R, 0, 0, 1)`, so agents expecting replicated RGB must swizzle explicitly. Sampling a packed depth-stencil texture returns depth in R per the core depth-texture rules; stencil is not directly sampleable. `BC1_RGB`, `BC1_RGBA`, and `BC3_RGBA` upload pre-compressed blocks via `glCompressedTexImage2D` only when the driver reports `EXT_texture_compression_s3tc`; without the extension creation fails explicitly naming the missing extension. `BC7_RGBA` was pruned from `TextureFormat` because BPTC compression needs OpenGL 4.2 and is unmappable on the locked 3.3 baseline. `CreateDepthTarget` still logs an error and returns `nullptr`: depth attachments should be created through `OpenGLFramebuffer` until texture-format mapping is completed.

`OpenGLFramebuffer` owns one framebuffer and its attachments through a single cleanup path. Resizing creates and validates a replacement first, then swaps it into service; failed replacement creation leaves the previous framebuffer intact. `Renderer::RenderTarget` delegates to this same implementation rather than maintaining a second raw OpenGL lifecycle. `RenderSystem::Resize()` propagates window dimensions to managed targets and passes, while fixed-resolution shadow maps remain unchanged.

## Image loading

`Image::Load` dispatches by extension:

- TGA: narrow uncompressed RGB/RGBA subset;
- BMP: narrow uncompressed 24/32-bit subset;
- PNG: custom non-interlaced path with DEFLATE/zlib handling;
- JPEG: owned 8-bit Huffman baseline/progressive decoding, grayscale and YCbCr normalization to tightly packed RGB, common 4:4:4/4:2:2/4:2:0 sampling, restart markers, bounded allocation, and explicit rejection of arithmetic, lossless, hierarchical, 12-bit, and four-component variants.

`ImageData::Data` is manually owned and must be released through `Image::Free`.

## Logging

`Pyramid::Foundation` owns the logger and public primitive types. The logger supports severity filtering, console/file output, rotation, structured fields, source locations, assertions, and thread synchronization. Engine subsystems use `PYRAMID_LOG_*` rather than direct console output.

## Build and package model

- `PyramidEngine` is the engine target; `Pyramid::Engine` is its build-tree alias and installed name.
- `PyramidFoundation`, `PyramidMath`, `PyramidInput`, `PyramidImage`, `PyramidModel`, `PyramidFont`, `PyramidText`, and `PyramidUI` are engine-independent targets exported as `Pyramid::Foundation`, `Pyramid::Math`, `Pyramid::Input`, `Pyramid::Image`, `Pyramid::Model`, `Pyramid::Font`, `Pyramid::Text`, and `Pyramid::UI`. `Pyramid::Engine` links them publicly so existing headers remain transitively available.
- GLAD is the sole approved bundled third-party runtime library and remains public because OpenGL implementation headers expose GLAD types.
- The installed package has no JPEG or codec package dependency.
- Public headers are installed separately rather than exported through `INTERFACE_SOURCES`, keeping the package relocatable.
- CMake package configuration and version files support independent engine, foundation, math, input, image, model, font, text, and UI consumers. The engine package resolves every owned library as an explicit package dependency.
- Windows CI validates separate engine, foundation/math/input, image, model, font, and UI consumers after installation.
- Native MinGW build trees and installations copy the compiler runtime DLLs into their `bin` directory through `PyramidMinGWRuntime`; Pyramid executables therefore do not depend on an interactive MSYS2 shell or user-modified `PATH`.

## Dependency direction

```text
Application
    ↓
Pyramid::Engine ─────→ Pyramid::Foundation
    │                  Pyramid::Math
    │                  Pyramid::Input
    │                  Pyramid::Image
    │                  Pyramid::Model
    │                  Pyramid::Text
    │                  Pyramid::UI
    ↓
Core / Graphics / Win32-WGL Platform
    ↓
GLAD + Win32/OpenGL
```

Avoid introducing platform handles into generic interfaces or renderer-specific ownership into scene data without an explicit lifetime model. GLAD is the only approved bundled third-party runtime library. New codec, importer, audio, physics, serialization, or tooling functionality must be implemented as a Pyramid/Ruqoom-owned library with an independent API and focused tests unless an explicit architectural decision changes this policy.
