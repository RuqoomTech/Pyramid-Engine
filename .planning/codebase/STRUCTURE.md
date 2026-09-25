# Codebase Structure

**Analysis Date:** 2026-09-05

## Directory Layout

```
Pyramid-Engine/
├── Engine/             # PyramidEngine library: Core, Graphics, Win32/WGL Platform only
│   ├── Core/           # Game loop, application lifecycle
│   ├── Graphics/       # Renderer, scene, resources, cameras, shaders
│   └── Platform/       # Win32 window, runtime paths, system fonts
├── Libraries/          # Independently testable owned packages (outside engine binary)
│   ├── PyramidFoundation/  # Types, Color, logging
│   ├── PyramidMath/        # Vec/Mat/Quat, SIMD
│   ├── PyramidInput/       # InputState, Clipboard, InputActions
│   ├── PyramidImage/       # PNG/JPEG/ZLib CPU codecs
│   ├── PyramidModel/       # CPU OBJ/MTL parsing
│   ├── PyramidFont/        # TrueType/SFNT, raster, atlas, .pfont
│   ├── PyramidText/        # Unicode, layout, shaping/bidi subset
│   └── PyramidUI/          # Layout, widgets, draw lists, screens
├── Examples/           # Graphical references + game-side support (not installed API)
│   ├── BasicGame/      # Full game reference (screens, RTS, debug UI)
│   ├── BasicRendering/ # Minimal pipeline reference (OBJ import + draw)
│   └── RTSReference/   # Reusable game-side RTS interaction helper
├── Tests/              # Focused suites + installed-package consumers
├── Tools/              # Offline CLIs (font compiler)
│   └── PyramidFontCompiler/
├── vendor/             # Sole bundled runtime: glad
│   └── glad/
├── assets/             # Repo branding (icons, rc)
├── Engine/Graphics/shaders/    # GLSL 3.30 engine shaders
├── Examples/*/Assets/  # Exe-adjacent runtime fonts/models
├── CMake/              # Per-package configs + MinGW runtime helper
├── scripts/            # Build, smoke, example, font-regeneration helpers
├── docs/               # Maintained compact docs (BUILDING, API, Architecture, ROADMAP)
├── .planning/          # GSD planning state + codebase maps
└── CMakeLists.txt      # Root build: vendor → Libraries → Engine → Tools → Examples → Tests
```

## Directory Purposes

**Engine/:**
- Purpose: Builds the `PyramidEngine` (`Pyramid::Engine`) C++17 library.
- Contains: Three owned modules only — Core, Graphics, Platform. No foundational types, math, input, codecs, or CPU format parsing live here.
- Key files: `Engine/CMakeLists.txt`, `Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Platform/include/Pyramid/Platform/Window.hpp`

**Engine/Core/:**
- Purpose: Application lifecycle and main loop.
- Contains: `include/Pyramid/Core/Game.hpp`, `source/Game.cpp`, `CMakeLists.txt`.
- Key files: `Engine/Core/include/Pyramid/Core/Game.hpp` — `Game::run/onCreate/onUpdate/onRender`, device/registry ownership, UI-context registration, resize fan-out.

**Engine/Graphics/:**
- Purpose: Backend-neutral rendering plus OpenGL 3.3 core backend, passes, scene, immutable resources.
- Contains: `include/Pyramid/Graphics/` (Buffer, Shader, Material, Texture, Geometry, Resources, Renderer, Scene, UI, OpenGL, Model, Camera, Scene), `source/` mirrors, `shaders/`, `CMakeLists.txt`.
- Key files: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceHandle.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Material/Material.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderProgram.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture/TextureResource.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`, `Engine/Graphics/include/Pyramid/Graphics/CameraController.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Camera.hpp`, `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`

**Engine/Platform/:**
- Purpose: Win32/WGL native layer, runtime-path and system-font bridges.
- Contains: `include/Pyramid/Platform/Window.hpp`, `include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, `include/Pyramid/Platform/RuntimePath.hpp`, `include/Pyramid/Platform/SystemFont.hpp`, `source/RuntimePath.cpp`, `source/SystemFont.cpp`, `source/Windows/Win32OpenGLWindow.cpp`.
- Key files: `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp` — message pump, polling, clipboard owner, resize dispatch.

**Libraries/PyramidFoundation/:**
- Purpose: Shared scalar types, `Color`, `GraphicsAPI`, logging diagnostics.
- Contains: `include/Pyramid/Core/Prerequisites.hpp`, `include/Pyramid/Util/Log.hpp`, `source/Log.cpp`, `source/Prerequisites.cpp`.
- Key files: `Libraries/PyramidFoundation/include/Pyramid/Core/Prerequisites.hpp`, `Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`

**Libraries/PyramidMath/:**
- Purpose: Owned math: `Vec2/3/4`, `Mat3/4`, `Quat`, common/SIMD helpers.
- Contains: `include/Pyramid/Math/Math.hpp`, `include/Pyramid/Math/Vec2.hpp`, `include/Pyramid/Math/Vec3.hpp`, `include/Pyramid/Math/Vec4.hpp`, `include/Pyramid/Math/Mat3.hpp`, `include/Pyramid/Math/Mat4.hpp`, `include/Pyramid/Math/Quat.hpp`, `source/Vec2.cpp`, `source/Vec3.cpp`, `source/Vec4.cpp`, `source/Mat3.cpp`, `source/Mat4.cpp`, `source/Quat.cpp`.

**Libraries/PyramidInput/:**
- Purpose: Backend-neutral physical input plus generic named actions; Unicode clipboard contract.
- Contains: `include/Pyramid/Platform/Input.hpp`, `include/Pyramid/Platform/Clipboard.hpp`, `include/Pyramid/Input/InputActions.hpp`, `source/Input.cpp`, `source/InputActions.cpp`, `source/Clipboard.cpp`.
- Key files: `Libraries/PyramidInput/include/Pyramid/Input/InputActions.hpp` — `InputConsumptionMask`, `InputBinding`, `InputContext`, `InputActionSystem`.

**Libraries/PyramidImage/:**
- Purpose: Owned CPU image codecs with no graphics dependency.
- Contains: `include/Pyramid/Util/Image.hpp`, `include/Pyramid/Util/PNGLoader.hpp`, `include/Pyramid/Util/JPEGLoader.hpp`, `include/Pyramid/Util/ZLib.hpp`, `include/Pyramid/Util/Inflate.hpp`, `include/Pyramid/Util/BitReader.hpp`, `include/Pyramid/Util/HuffmanDecoder.hpp`, matching `source/*.cpp`.

**Libraries/PyramidModel/:**
- Purpose: CPU asset-format parsing (OBJ/MTL) to renderer-independent `ImportedModel`.
- Contains: `include/Pyramid/Model/Model.hpp`, `include/Pyramid/Model/ObjImporter.hpp`, `source/Model.cpp`, `source/ObjImporter.cpp`.
- Key files: `Libraries/PyramidModel/include/Pyramid/Model/Model.hpp`, `Libraries/PyramidModel/include/Pyramid/Model/ObjImporter.hpp`

**Libraries/PyramidFont/:**
- Purpose: Owned TrueType/SFNT parse, outlines, CPU coverage/SDF raster, atlas bake/cache, `.pfont` format.
- Contains: `include/Pyramid/Font/Font.hpp`, `source/Font.cpp`.
- Key files: `Libraries/PyramidFont/include/Pyramid/Font/Font.hpp`

**Libraries/PyramidText/:**
- Purpose: Renderer-neutral text: UTF-8, graphemes, `TextBuffer`, metrics, international/bidi layout, atlas/family.
- Contains: `include/Pyramid/Text/Text.hpp`, `source/Text.cpp`, `source/Unicode.cpp`, `source/TextEditing.cpp`, `source/International.cpp`.
- Key files: `Libraries/PyramidText/include/Pyramid/Text/Text.hpp`

**Libraries/PyramidUI/:**
- Purpose: OpenGL/Win32/scene-independent UI runtime: layout, widgets, hit test, focus, capture, draw lists, screens.
- Contains: `include/Pyramid/UI/UI.hpp`, `include/Pyramid/UI/GameUI.hpp`, `source/UI.cpp`, `source/TextEditing.cpp`, `source/GameUI.cpp`.
- Key files: `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp`, `Libraries/PyramidUI/include/Pyramid/UI/GameUI.hpp`

**Examples/BasicGame/:**
- Purpose: Graphical reference game: retained screens, RTS camera + interaction, debug UI, font pipeline, registry-backed resources.
- Contains: `include/BasicGame.hpp`, `source/BasicGame.cpp`, `source/Main.cpp` (`WinMain`), `Assets/Fonts/`, `CMakeLists.txt`.
- Key files: `Examples/BasicGame/source/Main.cpp`, `Examples/BasicGame/include/BasicGame.hpp`, `Examples/BasicGame/source/BasicGame.cpp`

**Examples/BasicRendering/:**
- Purpose: Minimal pipeline reference: shader compile, OBJ import via `ModelResourceImporter`, UBO scene data, direct draw.
- Contains: `BasicRendering.hpp`, `BasicRendering.cpp`, `Main.cpp` (`WinMain`), `Assets/`, `CMakeLists.txt`.
- Key files: `Examples/BasicRendering/Main.cpp`, `Examples/BasicRendering/BasicRendering.hpp`, `Examples/BasicRendering/BasicRendering.cpp`

**Examples/RTSReference/:**
- Purpose: Game-side reusable RTS support; explicitly not part of installed engine API.
- Contains: `include/Pyramid/Examples/RTSReference/RTSInteractionController.hpp`, `source/RTSInteractionController.cpp`, `CMakeLists.txt`.
- Key files: `Examples/RTSReference/include/Pyramid/Examples/RTSReference/RTSInteractionController.hpp`

**Tests/:**
- Purpose: Focused behavior suites plus installed-package linkage/consumer validation.
- Contains: `EntitySceneTests.cpp`, `SceneSerializationTests.cpp`, `RTSInteractionTests.cpp`, `ResourceRegistryTests.cpp`, `ResourceHandleTests.cpp`, `ResourceManifestTests.cpp`, `MeshCacheTests.cpp`, `MaterialCacheTests.cpp`, `ShaderCacheTests.cpp`, `TextureCacheTests.cpp`, `Octree*Tests.cpp`, `Camera*Tests.cpp`, `Input*Tests.cpp`, `UIRendererTests.cpp`, `PublicApiLinkage.cpp`, `TestGraphicsDevice.hpp`, `Fixtures/`, `Consumer/`, `LibrariesConsumer/`, `ImageConsumer/`, `ModelConsumer/`, `FontConsumer/`, `UIConsumer/`, `CMakeLists.txt`.

**Tools/PyramidFontCompiler/:**
- Purpose: Offline `.pfont` baking CLI over the owned font pipeline.
- Contains: `Tools/PyramidFontCompiler/main.cpp`, `Tools/PyramidFontCompiler/CMakeLists.txt`.

**vendor/glad/:**
- Purpose: Sole approved bundled third-party runtime (OpenGL loader).
- Contains: `vendor/glad/include/`, loader sources, `CMakeLists.txt` built as `glad` target.

**CMake/, scripts/, assets/, docs/:**
- Purpose: Installable per-package configs, build/smoke/font helpers, branding, maintained docs.
- Contains: `CMake/PyramidEngineConfig.cmake.in`, `CMake/PyramidFoundationConfig.cmake.in`, `CMake/PyramidMathConfig.cmake.in`, `CMake/PyramidInputConfig.cmake.in`, `CMake/PyramidImageConfig.cmake.in`, `CMake/PyramidModelConfig.cmake.in`, `CMake/PyramidFontConfig.cmake.in`, `CMake/PyramidTextConfig.cmake.in`, `CMake/PyramidUIConfig.cmake.in`, `CMake/PyramidMinGWRuntime.cmake`, `scripts/run-smoke.ps1`, `scripts/run-example.ps1`, `scripts/regenerate-reference-fonts.py`, `Engine/Graphics/shaders/forward.vert`, `Engine/Graphics/shaders/forward.frag`, `Engine/Graphics/shaders/deferred_geometry.vert`, `Engine/Graphics/shaders/deferred_lighting.vert`, `assets/branding/pyramid-engine.rc`.

## Key File Locations

**Entry Points:**
- `Examples/BasicGame/source/Main.cpp`: `WinMain` → `BasicGame::run`
- `Examples/BasicRendering/Main.cpp`: `WinMain` → `BasicRendering::run`
- `Engine/Core/include/Pyramid/Core/Game.hpp`: `Game` base, `run/onCreate/onUpdate/onRender`, registry/camera/render-system accessors
- `Engine/Core/source/Game.cpp`: Loop body, UI-consumption merge, action evaluation, resize fan-out, teardown order
- `Tools/PyramidFontCompiler/main.cpp`: Offline font-baking CLI

**Configuration:**
- `CMakeLists.txt`: Root project, `PYRAMID_BUILD_TESTS/EXAMPLES/TOOLS`, install/export rules for engine + 8 libraries
- `CMakePresets.json`: `gcc-debug-tests`, `build-gcc-debug-tests`, `test-gcc-debug`, release twins
- `Engine/CMakeLists.txt`: `PyramidEngine` target, public links to all owned libraries, Win32 `OpenGL::GL`/`gdi32`/`user32`
- `Engine/Core/CMakeLists.txt`, `Engine/Graphics/CMakeLists.txt`, `Engine/Platform/CMakeLists.txt`: Per-module `target_sources` lists
- `Libraries/*/CMakeLists.txt`: Eight standalone `PyramidFoundation/Math/Image/Input/Model/Font/Text/UI` packages
- `Examples/RTSReference/CMakeLists.txt`, `Examples/BasicGame/CMakeLists.txt`, `Examples/BasicRendering/CMakeLists.txt`: Reference builds (RTS built when examples or tests enabled)
- `CMake/PyramidEngineConfig.cmake.in` plus `CMake/Pyramid{Foundation,Math,Input,Image,Model,Font,Text,UI}Config.cmake.in`: Installed `find_package` configs
- `CMake/PyramidMinGWRuntime.cmake`: `PYRAMID_BUNDLE_MINGW_RUNTIME` exe-adjacent DLL deploy

**Core Logic:**
- `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`: Authoritative `Scene`, `RenderObject`/`Light` proxies, `Environment`
- `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`: `EntityId`, `TransformComponent`, `MeshRendererComponent`, `LightComponent`, `Entity` facade
- `Engine/Graphics/source/Scene.cpp`: Proxy sync, hierarchy transforms, visibility inheritance
- `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp` + `Engine/Graphics/source/Scene/SceneManager.cpp`: Queries, culling, LOD, events
- `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp` + `Engine/Graphics/source/Scene/Octree.cpp`: Spatial tree
- `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp` + `Engine/Graphics/source/Scene/SceneSerializer.cpp`: Version-2 persistence
- `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp` + `Engine/Graphics/source/Resources/ResourceRegistry.cpp`: Cache owner
- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` + `Engine/Graphics/source/Renderer/RenderSystem.cpp`: Pipeline owner
- `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`, `Engine/Graphics/source/Renderer/CommandBuffer.cpp`: Pass implementations
- `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp` + `Engine/Graphics/source/UI/UIRenderer.cpp`: UI publication adapter
- `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp` + `Engine/Graphics/source/Model/ModelResourceImporter.cpp`: CPU→GPU publication

**Platform:**
- `Engine/Platform/include/Pyramid/Platform/Window.hpp`: Neutral window/context/input/clipboard/resize contract
- `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp` + `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`: Win32/WGL backend
- `Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp` + `Engine/Platform/source/RuntimePath.cpp`: Exe-relative resolution, user cache dir
- `Engine/Platform/include/Pyramid/Platform/SystemFont.hpp` + `Engine/Platform/source/SystemFont.cpp`: System-font byte exposure (parsing stays Pyramid-owned)

**Testing:**
- `Tests/PublicApiLinkage.cpp`: Protects selected public symbols; every public declaration must be implemented/removed/explicit failure
- `Tests/EntitySceneTests.cpp`: Stable IDs, cycle-safe hierarchy, visibility, attachment, proxies, recursive destroy
- `Tests/SceneSerializationTests.cpp`: Deterministic v2 round trips, manifest refs, hierarchy validation, missing/stale diagnostics
- `Tests/RTSInteractionTests.cpp`: Game-side selection/command/edge-scroll (not engine behavior)
- `Tests/ResourceRegistryTests.cpp`, `Tests/ResourceHandleTests.cpp`, `Tests/ResourceManifestTests.cpp`: Caches, generational handles, manifests
- `Tests/MeshCacheTests.cpp`, `Tests/MeshResourceTests.cpp`, `Tests/ShaderCacheTests.cpp`, `Tests/TextureCacheTests.cpp`, `Tests/TextureLoadingTests.cpp`, `Tests/MaterialCacheTests.cpp`, `Tests/MaterialResourceTests.cpp`, `Tests/ModelResourceImporterTests.cpp`, `Tests/ModelMaterialResourceImporterTests.cpp`: Resource pipeline
- `Tests/OctreeUpdateTests.cpp`, `Tests/OctreeQueryTests.cpp`, `Tests/OctreeConfigurationTests.cpp`, `Tests/OctreeCompactionTests.cpp`, `Tests/NearestQueryTests.cpp`: Spatial updates/queries/config/compaction
- `Tests/CameraControllerTests.cpp`, `Tests/CameraFrustumTests.cpp`, `Tests/CameraViewportTests.cpp`, `Tests/RenderObjectBoundsTests.cpp`, `Tests/FramebufferResizeTests.cpp`, `Tests/WindowResizeEventTests.cpp`, `Tests/RuntimePathTests.cpp`: Cameras, bounds, resize, paths
- `Tests/InputStateTests.cpp`, `Tests/InputActionTests.cpp`, `Tests/InputTextEventTests.cpp`, `Tests/ClipboardEncodingTests.cpp`: Platform input, generic actions, text events
- `Tests/Consumer/`, `Tests/LibrariesConsumer/`, `Tests/ImageConsumer/`, `Tests/ModelConsumer/`, `Tests/FontConsumer/`, `Tests/UIConsumer/`: Installed-package validation
- `Tests/Fixtures/`: Malformed/truncated/standards-valid fixtures for parser/codec coverage

## Naming Conventions

**Files:**
- Headers use `PascalCase.hpp` matching the type: `Game.hpp`, `RenderSystem.hpp`, `ResourceRegistry.hpp`, `RTSInteractionController.hpp`, `ModelResourceImporter.hpp` (`Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`)
- Sources mirror headers in `source/`: `Game.cpp`, `RenderSystem.cpp`, `ResourceRegistry.cpp`, `Win32OpenGLWindow.cpp` (`Engine/Core/source/Game.cpp`)
- Tests suffix the area under test: `EntitySceneTests.cpp`, `OctreeQueryTests.cpp`, `ResourceRegistryTests.cpp` (`Tests/EntitySceneTests.cpp`)
- Shaders pair by pass: `forward.vert`/`forward.frag`, `deferred_geometry.vert`, `deferred_lighting.vert`, `shadow.vert` (`Engine/Graphics/shaders/forward.vert`)
- Runtime fonts version the raster mode and size: `PyramidSans-64-sdf.pfont`, `PyramidArabic-48.pfont` (`Examples/BasicGame/Assets/Fonts/PyramidSans-64-sdf.pfont`)
- CMake per-module lists: `Engine/Core/CMakeLists.txt`, `Libraries/PyramidUI/CMakeLists.txt`, `Examples/RTSReference/CMakeLists.txt`

**Directories:**
- `PascalCase` for public include roots and owned packages: `Libraries/PyramidFoundation/`, `Libraries/PyramidText/`, `Tools/PyramidFontCompiler/`; `Examples/RTSReference/include/Pyramid/Examples/RTSReference/`
- Functional grouping under engine includes: `Engine/Graphics/include/Pyramid/Graphics/Buffer/`, `.../Shader/`, `.../Material/`, `.../Texture/`, `.../Geometry/`, `.../Resources/`, `.../Renderer/`, `.../Scene/`, `.../OpenGL/`, `.../UI/`, `.../Model/`; sources mirror under `Engine/Graphics/source/Renderer/`, `.../Scene/`, `.../OpenGL/Buffer/`
- Lowercase for build/tooling/docs: `scripts/`, `assets/branding/`, `docs/`, `vendor/glad/`, `CMake/`

**Code (for placement context):**
- Types and public methods use `PascalCase` (`ResourceRegistry`, `AcquireMesh`, `GetDrawStats`); locals/parameters use `camelCase`; member fields use `m_` prefix — see `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:57-148`
- Namespaces follow ownership: `Pyramid::Renderer`, `Pyramid::SceneManagement`, `Pyramid::UI`, `Pyramid::Text`, `Pyramid::Font`, `Pyramid::Model`, `Pyramid::Examples::RTSReference`

## Where to Add New Code

**New Engine Graphics Feature (stays in engine binary):**
- Primary code: `Engine/Graphics/include/Pyramid/Graphics/<Area>/` for headers plus `Engine/Graphics/source/<Area>/` for implementation; register both in `Engine/Graphics/CMakeLists.txt`
- Example areas: new pass under `Engine/Graphics/include/Pyramid/Graphics/Renderer/` + `Engine/Graphics/source/Renderer/`; new cache under `.../Resources/` or `.../Geometry/` wired through `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`
- Tests: focused suite in `Tests/<Area>Tests.cpp` plus linkage symbols in `Tests/PublicApiLinkage.cpp` when adding public API
- Rule: Graphics publication must use `ModelResourceImporter` and existing mesh/material/shader/texture caches; never create parallel upload ownership or reintroduce raw vertex-array fields on `RenderObject` (use `Mesh` in `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`)

**New Core / Platform Behavior:**
- Primary code: `Engine/Core/include/Pyramid/Core/` + `Engine/Core/source/` for loop/lifecycle; `Engine/Platform/include/Pyramid/Platform/` + `Engine/Platform/source/` (and `source/Windows/` for Win32 specifics) for native behavior; register in `Engine/Core/CMakeLists.txt` / `Engine/Platform/CMakeLists.txt`
- Tests: `Tests/WindowResizeEventTests.cpp`, `Tests/RuntimePathTests.cpp`, `Tests/FramebufferResizeTests.cpp` patterns for resize/paths/surfaces
- Rule: Keep the public window/input boundary backend-neutral (`Engine/Platform/include/Pyramid/Platform/Window.hpp`); feed `InputState`, evaluate generic `InputActionSystem` before updates, release held controls on focus loss; resolve exe-adjacent assets only through `Pyramid::Platform::ResolveRuntimePath` (`Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp`)

**New Foundational / Math / Input Primitive (must stay outside engine binary):**
- Primary code: `Libraries/PyramidFoundation/include/` for types/logging; `Libraries/PyramidMath/include/Pyramid/Math/` + `source/` for math; `Libraries/PyramidInput/include/` + `source/` for physical/action input and clipboard contract
- Tests: `Tests/InputStateTests.cpp`, `Tests/InputActionTests.cpp`, consumer coverage in `Tests/LibrariesConsumer/`
- Rule: Never hard-code game-specific action names into the input module; controllers consume configurable named references and physical bindings live in examples/games

**New Image / Model Format Support (must stay outside engine binary):**
- Primary code: `Libraries/PyramidImage/include/Pyramid/Util/` + `source/` for codecs; `Libraries/PyramidModel/include/Pyramid/Model/` + `source/` for CPU format parsing; GPU publication only via `Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`
- Tests: valid + malformed/truncated + limit + representative corpus fixtures under `Tests/Fixtures/`; importer suites `Tests/ModelResourceImporterTests.cpp`, `Tests/ModelMaterialResourceImporterTests.cpp`; consumer `Tests/ImageConsumer/`, `Tests/ModelConsumer/`
- Rule: `Pyramid::Model` must not depend on graphics, Win32, OpenGL, or GLAD; geometry passed to scenes must use `Mesh`

**New Font / Text Capability (must stay outside engine binary):**
- Primary code: `Libraries/PyramidFont/include/Pyramid/Font/` + `source/Font.cpp` for parsing/raster/atlas/`.pfont`; `Libraries/PyramidText/include/Pyramid/Text/` + `source/` for decoding, shaping/bidi subset, wrapping, editing state
- Tests: `Tests/FontConsumer/`, text/clipboard suites (`Tests/InputTextEventTests.cpp`, `Tests/ClipboardEncodingTests.cpp`), UI text-editing coverage
- Rule: `Pyramid::Font` stays independent of UI/graphics/Win32/FreeType/HarfBuzz; `Pyramid::Text` stays renderer-neutral with explicit fallback, grapheme-snapped indices, and logical-to-visual cluster maps

**New UI Widget / Screen (must stay outside engine binary):**
- Primary code: `Libraries/PyramidUI/include/Pyramid/UI/UI.hpp` + `source/UI.cpp` for layout/widgets/draw lists; `Libraries/PyramidUI/include/Pyramid/UI/GameUI.hpp` + `source/GameUI.cpp` for screens/anchors/signals
- Tests: `Tests/UIConsumer/`, `Tests/UIRendererTests.cpp` for publication (not widget logic in engine)
- Rule: Layout, state, hit testing, focus, capture, and draw generation belong in `Pyramid::UI`; graphics publication goes only through `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp` — never place OpenGL calls in `Pyramid::UI` or call Win32 directly from widgets

**New Shared Resource Type:**
- Primary code: `Engine/Graphics/include/Pyramid/Graphics/<Type>/` + cache + `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceHandle.hpp` alias + `ResourceRegistry` accessor (`Meshes/Shaders/Textures/Materials` pattern in `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp:70-80`); manifest support in `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp` when serializable
- Tests: `Tests/ResourceRegistryTests.cpp`, `Tests/ResourceHandleTests.cpp`, `Tests/ResourceManifestTests.cpp`, plus per-cache suites
- Rule: Handles stay non-owning; every alias bind/remap/removal advances generation; direct mutation invalidates handles; stale handles resolve to null

**New Scene / Serialization Capability:**
- Primary code: `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`, `Engine/Graphics/source/Scene.cpp` for authoring; `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp` for persistence; `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp` + `Octree.hpp` for queries
- Tests: `Tests/EntitySceneTests.cpp`, `Tests/SceneSerializationTests.cpp`, `Tests/SceneTransformTests.cpp`, octree suites
- Rule: Author with `Entity` plus components; never reintroduce `SceneNode` or make `RenderObject` transforms authoritative; keep stable IDs and hierarchy invariants serialization-safe

**New Game Behavior / RTS Semantic:**
- Primary code: game-owned layers — `Examples/BasicGame/`, `Examples/BasicRendering/`, or a new game directory; reusable game-side helpers go in `Examples/RTSReference/include/Pyramid/Examples/RTSReference/` + `source/`, never in `Engine/`
- Tests: `Tests/RTSInteractionTests.cpp` pattern for game-side interaction
- Rule: Selection, command, unit, ownership, and edge-scroll semantics remain in game/reference layers, not `Pyramid::Engine`

**New Tool:**
- Primary code: `Tools/<ToolName>/main.cpp` + `Tools/<ToolName>/CMakeLists.txt` wired from the root `CMakeLists.txt` behind `PYRAMID_BUILD_TOOLS`
- Reference: `Tools/PyramidFontCompiler/main.cpp`, `Tools/PyramidFontCompiler/CMakeLists.txt`

## Special Directories

**vendor/glad:**
- Purpose: Bundled OpenGL loader headers/sources backing `Engine/Graphics/source/OpenGL/`.
- Generated: No (third-party snapshot).
- Committed: Yes; sole approved bundled runtime library.

**Engine/Graphics/shaders:**
- Purpose: GLSL 3.30 engine passes (`forward`, `deferred_geometry`, `deferred_lighting`, `shadow`).
- Generated: No.
- Committed: Yes; resolved at build/runtime via `Engine/Graphics/include/Pyramid/Graphics/Renderer/ShaderPathResolver.hpp` and `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp`.

**Examples/BasicGame/Assets, Examples/BasicRendering/Assets:**
- Purpose: Exe-adjacent runtime fonts/models deployed beside build/install executables (e.g., `PyramidSans-64-sdf.pfont`, `pyramid.obj`).
- Generated: Reference `.pfont` atlases reproduce through `scripts/regenerate-reference-fonts.py` (source outlines Ruqoom-owned); per-user processed fonts cache via `Pyramid::Platform::GetUserCacheDirectory`.
- Committed: Yes (reference assets); must be resolved through `Pyramid::Platform::ResolveRuntimePath`, never via CWD.

**Tests/Fixtures:**
- Purpose: Valid, malformed, truncated, limit, and corpus fixtures for parser/codec/scene coverage.
- Generated: No.
- Committed: Yes; tests must clean temporary files they create.

**Tests/Consumer, Tests/LibrariesConsumer, Tests/ImageConsumer, Tests/ModelConsumer, Tests/FontConsumer, Tests/UIConsumer:**
- Purpose: Installed-package linkage checks proving the engine and standalone libraries are consumable after `install`.
- Generated: No.
- Committed: Yes.

**.planning/:**
- Purpose: GSD planning state, codebase maps (`ARCHITECTURE.md`, `STRUCTURE.md`, `CONVENTIONS.md`, `TESTING.md`, `CONCERNS.md`), phase artifacts.
- Generated: Yes (working state).
- Committed: Selectively per workflow; never commit secrets.

---

*Structure analysis: 2026-09-05*
