<!-- refreshed: 2026-09-05 -->
# Architecture

**Analysis Date:** 2026-09-05

## System Overview

```text
┌─────────────────────────────────────────────────────────────┐
│                    Game / Example Layer                      │
│  `Examples/BasicGame/`  `Examples/BasicRendering/`          │
│  `Examples/RTSReference/` (game-side, not installed API)    │
├──────────────────┬──────────────────┬───────────────────────┤
│  Scene Authoring │  Cameras/Input   │      UI Screens       │
│  `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`       │
│  `Engine/Graphics/include/Pyramid/Graphics/CameraController.hpp` │
│  `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`            │
└────────┬─────────┴────────┬─────────┴──────────┬────────────┘
         │                  │                     │
         ▼                  ▼                     ▼
┌─────────────────────────────────────────────────────────────┐
│                    Engine Core + Platform                     │
│  `Engine/Core/include/Pyramid/Core/Game.hpp`                │
│  `Engine/Platform/include/Pyramid/Platform/Window.hpp`      │
│  `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp` │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│                   Graphics + Resource Layer                  │
│  `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` │
│  `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp` │
│  `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp` │
│  `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp` │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│  Owned Libraries (outside engine binary ownership)           │
│  `Libraries/PyramidFoundation/` `Libraries/PyramidMath/`    │
│  `Libraries/PyramidInput/` `Libraries/PyramidImage/`        │
│  `Libraries/PyramidModel/` `Libraries/PyramidFont/`         │
│  `Libraries/PyramidText/` `Libraries/PyramidUI/`            │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│  Native Backend / GPU / Filesystem                           │
│  `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`           │
│  `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`     │
│  `vendor/glad/` (sole bundled runtime library)              │
└─────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| Game | Owns window, graphics device, resource registry; runs single-threaded loop; evaluates UI consumption then action contexts before `onUpdate`; gates rendering on renderable surface | `Engine/Core/include/Pyramid/Core/Game.hpp` |
| Game loop body | Window-message pump, delta clamp, `PrepareInput` merge, `InputActionSystem::Update`, `onUpdate`, `onRender` or sleep when minimized | `Engine/Core/source/Game.cpp` |
| Window abstraction | Platform-neutral window, context, `InputState`, `Clipboard`, resize callback dispatch | `Engine/Platform/include/Pyramid/Platform/Window.hpp` |
| Win32/WGL backend | Win32 message pump, WGL context, real keyboard/mouse polling, focus-loss release, resize events, clipboard owner | `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp` |
| Graphics device interface | Backend-neutral draw, state, buffer/texture/shader/framebuffer factory, scissor, viewport, present | `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` |
| OpenGL device | OpenGL 3.3 core implementation, state manager, framebuffer/texture/buffer/shader objects | `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` |
| RenderSystem | Owns render passes, command buffers, camera/lighting UBOs, window-sized target resize, frame begin/render/end | `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` |
| Render passes | Forward, deferred geometry/lighting, shadow-map, transparent/post/UI pass slots; `Begin`/`Execute`/`End` on `CommandBuffer` | `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp` |
| CommandBuffer | Recorded `RenderCommand` list, material/uniform helpers, ID registries, `Execute(IGraphicsDevice*)` | `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` |
| Scene (authoritative) | `Entity` + components store, stable `EntityId`, hierarchy by IDs, generated `RenderObject`/`Light` proxies, primary light, environment | `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp` |
| Entity facade | Non-owning handle: name, visibility, local/world transforms, parent/children, mesh/light components | `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp` |
| SceneManager | Spatial-partitioned scene queries, frustum/occlusion toggles, LOD, incremental octree sync, events, stats | `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp` |
| Octree | AABB tree, point/sphere/box/ray/frustum queries, nearest/K-nearest, transactional `Configure`, compaction | `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp` |
| SceneSerializer | Deterministic version-2 entity/component round trip, manifest-keyed mesh/material refs, hierarchy validation, stale/missing diagnostics | `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp` |
| ResourceRegistry | Central owner of immutable caches; handle-first acquisition; dependency-safe `CollectUnused`/`Clear`; graphics-thread only | `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp` |
| Resource handles | Serializable non-owning `assetId+generation`; stale resolves to null, never to a replacement | `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceHandle.hpp` |
| ResourceManifest | Versioned deterministic typed handle list; transactional parse; `Restore` validates generations | `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp` |
| Mesh | Immutable GPU geometry: VAO/VBO/IBO, layout, topology, asset/content IDs, local bounds | `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp` |
| Material | Immutable shader+texture+uniform+state bundle; `Apply` binds fixed state | `Engine/Graphics/include/Pyramid/Graphics/Material/Material.hpp` |
| ShaderProgram | Immutable compiled program wrapper; direct `Compile*` rejected; replace via `ShaderCache::Recompile` | `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderProgram.hpp` |
| TextureResource | Immutable cache-safe 2D texture; mutation rejected; file reload is transactional | `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureResource.hpp` |
| ModelResourceImporter | Transactional CPU `ImportedModel` → GPU cache publication for meshes/textures/materials | `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp` |
| Camera controllers | Generic `CameraController` interface; `FreeFly`/`Orbit`/`RTS` variants driven by configurable named actions | `Engine/Graphics/include/Pyramid/Graphics/CameraController.hpp` |
| UIRenderer | Graphics adapter: consumes renderer-neutral `UI::DrawList`, uploads vertices/indices, resolves textures, restores baseline state | `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp` |
| RTSReference | Game-side edge-scroll/selection/command coordinator; lives outside `Pyramid::Engine` | `Examples/RTSReference/include/Pyramid/Examples/RTSReference/RTSInteractionController.hpp` |
| InputActionSystem | Engine-generic named contexts/actions/bindings with priority and consumption | `Libraries/PyramidInput/include/Pyramid/Input/InputActions.hpp` |
| UI Context | Hybrid immediate/retained layout, focus, pointer capture, hit test, draw-list generation; OpenGL-independent | `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp` |
| GameUI screens | Retained `Screen`/`ScreenStack`, anchors/docks, signals; game menus share UI runtime with debug UI | `Libraries/PyramidUI/include/Pyramid/UI/GameUI.hpp` |
| Text pipeline | UTF-8 decode, grapheme segmenter, `TextBuffer`, metrics, international/bidi layout, font atlas/family | `Libraries/PyramidText/include/Pyramid/Text/Text.hpp` |
| Font pipeline | Owned TrueType/SFNT parse, CPU coverage/SDF raster, atlas bake, content-addressed cache, `.pfont` format | `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp` |
| Model parsing | CPU OBJ/MTL parsing to `ImportedModel` with diagnostics; no graphics/Win32/OpenGL dependency | `Libraries/PyramidModel/include/Pyramid/Model/Model.hpp` |
| Image codecs | Owned PNG/JPEG/ZLib/inflate/bit-reader/Huffman; CPU decode only | `Libraries/PyramidImage/include/Pyramid/Util/Image.hpp` |
| RuntimePath | Exe-relative asset resolution, per-user cache dir; no CWD-dependent launches | `Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp` |

## Pattern Overview

**Overall:** Layered modular engine with independently packaged owned libraries, immutable content-addressed graphics resources, and an authoritative entity-component scene.

**Key Characteristics:**
- Engine binary owns only Core, Graphics, and Win32/WGL Platform; all foundational types, math, input, codecs, CPU model parsing, font, text, and UI live in `Libraries/` as separate CMake packages.
- Graphics resources are immutable value-identified assets behind non-owning generational handles; caches deduplicate by content and release in dependency-safe order.
- Scene authoring is `Entity` plus `TransformComponent` / `MeshRendererComponent` / `LightComponent`; renderer and octree consume generated proxies, never an independent transform graph.
- Rendering is pass-based over recorded command buffers; UI is renderer-neutral draw lists published through a graphics adapter.
- Input is backend-neutral at the public boundary: native messages feed `InputState`, UI reserves controls via `InputConsumptionMask`, then generic `InputActionSystem` contexts evaluate before game update.

## Layers

**Engine Core:**
- Purpose: Application lifecycle, main loop, owned device/registry lifetime, resize fan-out, UI-context registration.
- Location: `Engine/Core/include/Pyramid/Core/`, `Engine/Core/source/Game.cpp`
- Contains: `Game` base class (`onCreate`/`onUpdate`/`onRender`/`onWindowResize`), active camera/render-system wiring.
- Depends on: `Pyramid::Input`, `Pyramid::UI`, `IGraphicsDevice`, `ResourceRegistry`, `Window`.
- Used by: `Examples/BasicGame/source/BasicGame.cpp`, `Examples/BasicRendering/BasicRendering.cpp`.

**Graphics:**
- Purpose: Backend-neutral rendering API, OpenGL 3.3 core backend, passes, command buffers, scene/octree/serialization, immutable resource caches, camera controllers, UI publication.
- Location: `Engine/Graphics/include/Pyramid/Graphics/`, `Engine/Graphics/source/`
- Contains: `GraphicsDevice`, `OpenGL/*`, `Renderer/*`, `Buffer/*`, `Geometry/*`, `Shader/*`, `Material/*`, `Texture/*`, `Resources/*`, `Scene/*`, `Scene.hpp`, `Camera.hpp`, `CameraController.hpp`, `UI/UIRenderer.hpp`, `Model/ModelResourceImporter.hpp`, `Engine/Graphics/shaders/`.
- Depends on: `PyramidFoundation`, `PyramidMath`, `PyramidInput`, `PyramidImage`, `PyramidModel`, `PyramidFont`, `PyramidText`, `PyramidUI`, `glad`, Win32/OpenGL system libs.
- Used by: `Game`, examples, `Tests/` renderer/scene/resource suites.

**Win32/WGL Platform:**
- Purpose: Native window, GL context, message pump, input/clipboard source, resize events, system-font bytes, runtime-path resolution.
- Location: `Engine/Platform/include/Pyramid/Platform/`, `Engine/Platform/source/`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`
- Contains: `Window.hpp`, `Windows/Win32OpenGLWindow.hpp`, `RuntimePath.hpp`, `SystemFont.hpp`.
- Depends on: `PyramidInput` (`InputState`, `Clipboard`), Win32 `user32`/`gdi32`, `OpenGL::GL`.
- Used by: `Game` construction, teardown, per-frame `ProcessMessages`, `GetInput`, `GetClipboard`.

**Owned Libraries:**
- Purpose: Independently testable, installable packages with no engine/graphics/Win32 dependencies (except narrowly scoped platform byte exposure).
- Location: `Libraries/PyramidFoundation/`, `Libraries/PyramidMath/`, `Libraries/PyramidInput/`, `Libraries/PyramidImage/`, `Libraries/PyramidModel/`, `Libraries/PyramidFont/`, `Libraries/PyramidText/`, `Libraries/PyramidUI/`
- Contains: Prerequisites/types/logging; vectors/matrices/quats; `InputState`/`Clipboard`/`InputActions`; PNG/JPEG/ZLib; OBJ/MTL CPU model; TrueType/SDF/`.pfont`; Unicode/text layout; UI layout/widgets/draw lists.
- Depends on: Lower libraries only (e.g., `PyramidText` on `PyramidFont`+`PyramidMath`; `PyramidUI` on `PyramidText`+`PyramidInput`); never on `PyramidEngine`, OpenGL, GLAD, or Win32 semantics.
- Used by: Engine, examples, tools, consumer tests.

**Examples (graphical references):**
- Purpose: Canonical engine usage: game loop subclass, input contexts, camera profiles, scene setup, resource acquisition, UI screens, render submission.
- Location: `Examples/BasicGame/`, `Examples/BasicRendering/`
- Contains: `BasicGame` (`Examples/BasicGame/include/BasicGame.hpp`, `Examples/BasicGame/source/BasicGame.cpp`), `BasicRendering` (`Examples/BasicRendering/BasicRendering.hpp`, `Examples/BasicRendering/BasicRendering.cpp`), Win32 `WinMain` entry points.
- Depends on: Installed `Pyramid::Engine` plus owned libraries and `RTSReference` where needed.
- Used by: Manual validation, smoke scripts (`scripts/run-smoke.ps1`, `scripts/run-example.ps1`); not installed as engine API.

**RTSReference (game-side support):**
- Purpose: Reusable RTS edge-scroll/selection/command reference without polluting the generic engine API with game semantics.
- Location: `Examples/RTSReference/include/Pyramid/Examples/RTSReference/`, `Examples/RTSReference/source/RTSInteractionController.cpp`
- Contains: `RTSInteractionController`, `RTSActionReference`, `RTSInteractionSettings`, `RTSCommandRequest`.
- Depends on: `Camera`, `CameraController`, `SceneManager`, `InputState`, `InputActionSystem`.
- Used by: `Examples/BasicGame/source/BasicGame.cpp`; focused behavior protected by `Tests/RTSInteractionTests.cpp`. Never linked into installed engine API.

**Tooling and Assets:**
- Purpose: Offline font compilation, runtime shaders/fonts/models/branding, build helpers.
- Location: `Tools/PyramidFontCompiler/main.cpp`, `Engine/Graphics/shaders/`, `Examples/BasicGame/Assets/`, `Examples/BasicRendering/Assets/`, `assets/branding/`, `scripts/`, `CMake/`, `vendor/glad/`
- Contains: `.pfont` compiler, GLSL 3.30 forward/deferred/shadow shaders, Pyramid Sans/Arabic reference fonts, smoke/build scripts, per-package CMake configs.
- Depends on: Owned libraries for tools; runtime assets resolved via `Pyramid::Platform::ResolveRuntimePath`.
- Used by: Build, install, and exe-adjacent runtime deployment.

## Data Flow

### Primary Request Path

1. Native message pump produces backend-neutral input — `Win32OpenGLWindow::ProcessMessages` updates `InputState`, clipboard text events, and emits `WindowResizeEvent` (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, `Libraries/PyramidInput/include/Pyramid/Platform/Input.hpp`)
2. UI pre-pass reserves handled controls — `Game::run` merges each registered `UI::Context::PrepareInput(GetInput())` into `InputConsumptionMask` (`Engine/Core/source/Game.cpp:306-316`)
3. Generic action evaluation — `InputActionSystem::Update(input, consumed)` evaluates prioritized consuming contexts once per frame (`Engine/Core/source/Game.cpp:320`, `Libraries/PyramidInput/include/Pyramid/Input/InputActions.hpp`)
4. Game update — `Game::onUpdate(deltaTime)` runs cameras (`CameraController::Update`), RTS interaction (`RTSInteractionController::Update`), and `SceneManager::Update` including incremental octree sync (`Examples/BasicGame/source/BasicGame.cpp:1033-1130`, `Engine/Graphics/source/Scene/SceneManager.cpp`)
5. Scene proxy sync — `Entity` hierarchy/transforms/visibility/components synchronize to `RenderObject`/`Light` proxies for renderer and octree (`Engine/Graphics/include/Pyramid/Graphics/Scene.hpp:108-227`, `Engine/Graphics/source/Scene.cpp`)
6. Render submission — `RenderSystem::BeginFrame` → `Render(scene, camera)` over `Forward`/`Shadow`/`Deferred*` passes via `CommandBuffer::Execute(device)` → `EndFrame`; `UIRenderer::Render(drawList, frame)` publishes UI without leaking framebuffer/viewport state (`Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/UI/UIRenderer.cpp`)
7. Presentation and resize fan-out — `IGraphicsDevice::Present`, `SetViewport`; `Game::HandleWindowResize` syncs device, active camera, and render-system targets; minimized zero-area frames sleep instead of presenting (`Engine/Core/source/Game.cpp:220-246`)

### Asset Import Flow

1. CPU parse with diagnostics — `Pyramid::Model::ObjImporter::ImportFile` yields `ImportedModel` primitives/materials/diagnostics (`Libraries/PyramidModel/include/Pyramid/Model/ObjImporter.hpp`, `Examples/BasicRendering/BasicRendering.cpp:208-229`)
2. Transactional GPU publication — `ModelResourceImporter::ImportModel` / `UploadMeshes` publishes through `ResourceRegistry::Meshes/Shaders/Textures/Materials` with rollback of only that operation's aliases (`Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`)
3. Handle-first use — Callers hold `MeshHandle`/`MaterialHandle`/`TextureHandle`/`ShaderHandle`; per-draw matrices go in command-buffer uniforms, never in material identity; stale handles resolve to null (`Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceHandle.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:82-104`)

### UI Frame Flow

1. Game builds retained screens — `ScreenStack::Build(context)` and direct `Context` widget calls reconcile retained element state (`Libraries/PyramidUI/include/Pyramid/UI/GameUI.hpp`, `Examples/BasicGame/source/BasicGame.cpp:1153-1162`)
2. Layout, hit test, focus, pointer capture, and clip-aware draw generation stay in `Pyramid::UI` with logical coordinates preserved for scrollable content (`Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`)
3. Graphics publication only through `UIRenderer`, which resolves `TextureId`s, uploads vertices/indices, binds immutable font texture, applies scissor, and restores baseline state (`Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`)

### Persistence Flow

1. Serialize — `SceneSerializer::Serialize(scene, manifest, registry)` emits deterministic version-2 text with stable `EntityId`s, parent IDs, transforms, components, primary light, and exact manifest keys (`Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp`)
2. Deserialize — `SceneSerializer::Deserialize` validates hierarchy (cycles, duplicate/invalid IDs), checks manifest generations, restores generation-checked handles, and reports missing/stale resources without silent substitution (`Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp:42-81`)

**State Management:**
- Per-frame `InputState` snapshot with pressed/released deltas valid for one update; held states persist until release or focus loss (`Engine/Core/include/Pyramid/Core/Game.hpp:106-124`)
- `InputActionSystem` runtime states reset per `Update`; `InputConsumptionMask` cleared and re-merged every frame (`Engine/Core/source/Game.cpp:309-320`)
- Scene entity records cache dirty-flagged local/world matrices with cycle-safe hierarchy invalidation (`Engine/Graphics/include/Pyramid/Graphics/Scene.hpp:168-216`)
- Octree tracks per-object AABBs and incrementally inserts/removes/relocates plus compaction (`Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp:168-290`)
- Registry caches are graphics-thread-owned, not internally synchronized; teardown order is materials → textures → shaders → meshes → device → window (`Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:46-56`, `Engine/Core/source/Game.cpp:79-102`)

## Key Abstractions

**Entity + Components (authoritative authoring):**
- Purpose: Stable, serialization-safe scene graph without a separate transform-node class.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`
- Pattern: Non-owning `Entity` facade over `Scene::EntityRecord`; exactly one `TransformComponent` per entity; optional `MeshRendererComponent` and `LightComponent`; parent/children by `EntityId`; recursive destruction; inherited visibility.

**RenderObject / Light Proxies (renderer-facing):**
- Purpose: Flat, bounds-aware draw and light data derived from entities for culling and passes.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp:22-90`, `Engine/Graphics/source/Scene.cpp`
- Pattern: `Scene::SynchronizeRenderProxies` / `SynchronizeLightProxies`; geometry uses `Mesh`, never raw vertex-array fields on `RenderObject`; handles plus resolved `shared_ptr` for binding.

**Immutable Resources + Generational Handles:**
- Purpose: Shareable, deduplicated GPU resources with safe stale-reference semantics.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Material/Material.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderProgram.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureResource.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp`
- Pattern: `*Specification` → `Create` → content/asset IDs → `*Cache::GetOrCreate` → `ResourceHandle`; color space is texture identity; cached instances are immutable; file changes publish via transactional reload/replace; direct cache mutation invalidates handles.

**CommandBuffer + RenderPasses + RenderSystem:**
- Purpose: Efficient, testable GPU submission with pluggable pipeline stages.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`, `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`, `Engine/Graphics/source/Renderer/CommandBuffer.cpp`
- Pattern: `RenderPass::Begin/Execute/End(CommandBuffer&, Scene, Camera)`; `CommandBuffer` records state/texture/material/uniform/draw/clear commands and executes against `IGraphicsDevice`; off-screen passes never leak framebuffer/viewport state.

**Backend-Neutral Input + Consumption:**
- Purpose: Keep physical bindings in games/examples while engine stays game-agnostic.
- Examples: `Libraries/PyramidInput/include/Pyramid/Platform/Input.hpp`, `Libraries/PyramidInput/include/Pyramid/Input/InputActions.hpp`, `Engine/Core/source/Game.cpp:309-320`
- Pattern: `InputState` snapshot → `InputConsumptionMask` (UI first refusal) → prioritized `InputContext` evaluation → named `Button`/`Axis1D`/`Axis2D` states; never hard-code RTS/game action names in the input module; controllers consume `CameraActionReference`/`RTSActionReference` pairs.

**Camera Controllers (optional utilities):**
- Purpose: Reusable free-fly/orbit/strategy cameras without engine-wide camera assumptions.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/CameraController.hpp`, `Engine/Graphics/source/CameraController.cpp`
- Pattern: `CameraController::Update(camera, actions, dt)` / `Synchronize` / `CaptureHome` / `Reset`; distinguish per-frame delta input from time-scaled rate input; reference profiles live in examples/games.

**UI Context + DrawList + UIRenderer Split:**
- Purpose: Debug and game UI share one OpenGL-independent runtime; only the adapter touches the GPU.
- Examples: `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/GameUI.hpp`, `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`
- Pattern: `BeginFrame(frame, input)` → widgets/screens → `EndFrame` → `DrawList` (vertices/indices/batches with clips) → `UIRenderer::Render`; UI layers reserve controls with `InputConsumptionMask` before action evaluation.

**Font → Text → UI Text Stack:**
- Purpose: Owned international text without FreeType/HarfBuzz middleware.
- Examples: `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`, `Libraries/PyramidText/include/Pyramid/Text/Text.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`
- Pattern: `Font` parses/rasterizes/bakes/caches `.pfont`; `Text` decodes UTF-8, segments graphemes, manages `TextBuffer` with scalar indices snapped to grapheme boundaries, and produces renderer-neutral glyph runs with logical-to-visual cluster maps; `UI` draws text layouts and blocks conflicting gameplay controls for text widgets without consuming unrelated function keys.

## Entry Points

**BasicGame WinMain:**
- Location: `Examples/BasicGame/source/Main.cpp`
- Triggers: Windows process launch (`WinMain`); constructs `BasicGame` and calls `run`.
- Responsibilities: Exception boundary with message-box reporting; delegates lifecycle to `Game::run`.

**BasicRendering WinMain:**
- Location: `Examples/BasicRendering/Main.cpp`
- Triggers: Windows process launch (`WinMain`); constructs `BasicRendering` and calls `run`.
- Responsibilities: Minimal render-pipeline demo bootstrap; delegates lifecycle to `Game::run`.

**Game base:**
- Location: `Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Core/source/Game.cpp`
- Triggers: Subclass construction then `run()`; virtual `onCreate`/`onUpdate`/`onRender`/`onWindowResize`.
- Responsibilities: Creates `Win32OpenGLWindow` + `IGraphicsDevice::Create` + `ResourceRegistry`; installs resize callback; owns teardown order; subclass `BasicGame::onCreate` wires input contexts, render system, fonts, UI renderer, scene, cameras, RTS reference, and registry-backed shader/texture/material acquisition (`Examples/BasicGame/source/BasicGame.cpp:680-1031`); `BasicRendering::onCreate` wires shader, OBJ import, camera, and UBOs (`Examples/BasicRendering/BasicRendering.cpp:129-170`).

**Font compiler tool:**
- Location: `Tools/PyramidFontCompiler/main.cpp`
- Triggers: Offline CMake-built CLI invocation.
- Responsibilities: Bakes source outlines into versioned `.pfont` runtime assets via the owned font pipeline.

## Architectural Constraints

- **Threading:** Single game-thread loop; window messages, input evaluation, scene update, and render submission run sequentially in `Game::run`. `ResourceRegistry` is documented for the graphics thread and is not internally synchronized (`Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:52-55`).
- **Global state:** Retained `Pyramid::Util::Logger` singleton history backs the debug log panel (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`, `Examples/BasicGame/source/BasicGame.cpp:1446-1448`); scene lifetime tokens (`Scene::m_lifetimeToken`) gate `Entity` validity; caches hold the only GPU ownership with non-owning external handles.
- **Circular imports:** Public headers use forward declarations (`Camera`, `Scene`, `ResourceRegistry`, `IGraphicsDevice`, `Window`) to keep `Game`, `RenderSystem`, `Entity`, and cache layers acyclic; `Engine/CMakeLists.txt` links libraries one way from engine to owned libraries, never the reverse.
- **Platform scope:** Windows-only Win32/WGL today; Linux/macOS, DirectX, Vulkan, audio, physics, editor, and scripting are explicitly out of scope; Baa is the long-term scripting/rewrite target.
- **Graphics baseline:** OpenGL 3.3 core or newer; examples and engine shaders target GLSL 3.30; frustum culling is implemented while occlusion culling remains disabled/not complete (`Engine/Graphics/source/Scene/SceneManager.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp:193-194`).
- **Ownership:** Foundational types/logging, math, physical/action input, image codecs, and CPU asset-format parsing must remain outside the engine binary in `Libraries/`; `Engine/` builds only Core, Graphics, and Win32/WGL Platform. New runtime middleware and package-manager dependencies are prohibited by default; `vendor/glad` is the sole approved bundled runtime library.
- **No silent no-ops:** Do not add required interface methods with silent no-op defaults; every public declaration must be implemented, removed, or documented as an explicit failure (guarded by `Tests/PublicApiLinkage.cpp`).
- **No absolute install paths:** Do not expose source-tree absolute paths through installed target interfaces; runtime assets beside executables resolve through `Pyramid::Platform::ResolveRuntimePath`, never via process CWD.

## Anti-Patterns

### Mutating Cached GPU Resources In Place

**What happens:** Calling setters directly on a cached `ShaderProgram`, `TextureResource`, or `Material` instance, recompiling identical stage source repeatedly, or creating parallel uploads for byte-identical mesh specifications.
**Why it's wrong:** It breaks content-identity deduplication, corrupts shared cache entries, and leaks GPU residency.
**Do this instead:** Acquire shared geometry via `ResourceRegistry::Meshes()`, programs via `ResourceRegistry::Shaders()`, sampled textures via `ResourceRegistry::Textures()`, and scene materials via `ResourceRegistry::Materials()`; publish changes through transactional `RecompileShader`/`ReloadTexture`/`ReplaceMaterial` and treat color space as texture identity (`Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:70-123`).

### Putting Backend Calls in UI or Game Semantics in Engine

**What happens:** Issuing OpenGL calls inside `Pyramid::UI`, calling Win32 APIs from UI widgets, hard-coding RTS action names into the input module, or adding selection/command/unit/edge-scroll concepts to `Pyramid::Engine`.
**Why it's wrong:** It couples the renderer-neutral UI and generic input layers to one backend or one game genre and breaks independent testing.
**Do this instead:** Keep layout/state/hit-testing/focus/capture/draw generation in `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp` and publish only through `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`; keep physical bindings and RTS profiles in examples/games and the game-side `Examples/RTSReference/include/Pyramid/Examples/RTSReference/RTSInteractionController.hpp`; keep camera controllers on configurable `CameraActionReference` pairs (`Engine/Graphics/include/Pyramid/Graphics/CameraController.hpp:19-28`).

## Error Handling

**Strategy:** Fail visibly with actionable diagnostics; never false-success skip; transactional operations roll back to the pre-call state.

**Patterns:**
- Boolean + diagnostic-vector results for parsing/import/persistence: `Model::ImportedModel::diagnostics`, `ModelResourceImporter` transactional upload, `SceneSerializer` line-keyed `SceneSerializationDiagnostic`, `ResourceManifestDiagnostic`, font `Diagnostic` (`Libraries/PyramidModel/include/Pyramid/Model/Model.hpp:11-25`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp:18-81`).
- Null/empty guards at entry points: `Game::onCreate` aborts when window/device/registry are null; `BasicGame::onCreate` quits on failed input, device, registry, render-system, shader, texture, or scene setup (`Engine/Core/source/Game.cpp:104-134`, `Examples/BasicGame/source/BasicGame.cpp:680-712`).
- Generation-checked resolution returns `nullptr` for stale/mismatched handles instead of rebinding a replacement (`Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:95-104`).
- Structured logging via `PYRAMID_LOG_INFO/WARN/ERROR/CRITICAL/DEBUG` from `Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp` throughout `Engine/Core/source/Game.cpp` and examples.

## Cross-Cutting Concerns

**Logging:** `Pyramid::Util::Logger` singleton with leveled `PYRAMID_LOG_*` macros and retrievable history for the debug UI log panel (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`, `Libraries/PyramidFoundation/source/Log.cpp`, `Examples/BasicGame/source/BasicGame.cpp:1428-1466`).
**Validation:** Allocation/element limits and malformed/truncated fixtures for parsers/codecs; hierarchy validation (duplicate IDs, invalid parents, cycles) in deserialization; `Configure` validates full octree requests before atomic swap; camera/UI numeric validation (`IsValid`, finite/non-negative checks) in `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp:31-56` and `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp:236-240`.
**Authentication:** Not applicable; no auth provider, user identity, or networked sessions in this codebase.
**Resize/minimize:** `WindowResizeEvent::HasRenderableArea` gates viewport, camera, and render-target work; `Game` tracks `m_renderSurfaceAvailable` and sleeps while minimized (`Engine/Platform/include/Pyramid/Platform/Window.hpp:22-38`, `Engine/Core/source/Game.cpp:220-246`).
**Runtime deployment:** MinGW runtime DLLs bundle beside executables via `CMake/PyramidMinGWRuntime.cmake` with `PYRAMID_BUNDLE_MINGW_RUNTIME` on by default; exe-adjacent fonts/models/shaders resolve through `Pyramid::Platform::ResolveRuntimePath` (`Engine/Platform/source/RuntimePath.cpp`).

---

*Architecture analysis: 2026-09-05*
