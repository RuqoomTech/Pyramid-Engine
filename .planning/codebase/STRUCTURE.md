# Codebase Structure

**Analysis Date:** Thu May 14 2026

## Directory Layout

```text
Pyramid-Engine/
├── CMakeLists.txt                  # Root CMake project, options, install rules, subdirectory wiring
├── CMakePresets.json               # CMake configure/build presets
├── AGENTS.md                       # Repository instructions for agents
├── README.md                       # Project overview
├── CHANGELOG.md                    # Release/change notes
├── CMake/                          # CMake helper modules and scripts
├── docs/                           # Architecture, build, API, examples, and smoke-test docs
├── Engine/                         # Main `PyramidEngine` library source tree
│   ├── CMakeLists.txt              # Creates and configures `PyramidEngine`
│   ├── Core/                       # Game lifecycle and common prerequisites
│   ├── Graphics/                   # Graphics API, OpenGL backend, renderer, camera, scene
│   ├── Platform/                   # Platform window/context abstraction and Win32 implementation
│   ├── Math/                       # Vec/Mat/Quat/math/SIMD library
│   ├── Utils/                      # Logging, image loading, compression/bitstream utilities, tests
│   ├── Renderer/                   # Include-directory hook; active renderer code is under `Engine/Graphics`
│   ├── Input/                      # Include-directory hook for future input API
│   ├── Scene/                      # Include-directory hook; active scene code is under `Engine/Graphics`
│   ├── Audio/                      # Include-directory hook for future audio API
│   └── Physics/                    # Include-directory hook for future physics API
├── Examples/                       # Runnable sample applications linked to `PyramidEngine`
│   ├── BasicGame/                  # Engine lifecycle + scene + render-system sample
│   └── BasicRendering/             # Basic rendering sample executable
├── Game/                           # Separate game target stub; currently links an undefined `OpenGL3D` target
├── vendor/                         # Committed third-party dependencies
│   └── glad/                       # Bundled OpenGL loader and CMake target
├── scripts/                        # Project scripts
└── .planning/codebase/             # Generated codebase maps consumed by GSD planning/execution
```

## Directory Purposes

**Root:**
- Purpose: Own project-level build configuration, docs, repository metadata, and generated planning maps.
- Contains: `CMakeLists.txt`, `CMakePresets.json`, `AGENTS.md`, `README.md`, `CHANGELOG.md`, `LICENSE`, `.planning/codebase`.
- Key files: `CMakeLists.txt`, `AGENTS.md`, `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/STRUCTURE.md`.

**`Engine/`:**
- Purpose: Main engine library target source tree.
- Contains: Module subdirectories and `Engine/CMakeLists.txt`.
- Key files: `Engine/CMakeLists.txt` creates `PyramidEngine`, adds all modules, links `glad` and `opengl32`, and defines `PYRAMID_SOURCE_DIR`.

**`Engine/Core/`:**
- Purpose: Engine lifecycle and common prerequisites.
- Contains: `include/Pyramid/Core` public headers and `source` implementations.
- Key files: `Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Core/source/Game.cpp`, `Engine/Core/include/Pyramid/Core/Prerequisites.hpp`, `Engine/Core/source/Prerequisites.cpp`, `Engine/Core/CMakeLists.txt`.

**`Engine/Graphics/`:**
- Purpose: Rendering-facing APIs, OpenGL backend, scene types, camera, render system, render passes, shaders, and graphics resources.
- Contains: `include/Pyramid/Graphics`, `source`, and `shaders`.
- Key files: `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Graphics/source/GraphicsDevice.cpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`, `Engine/Graphics/source/Scene.cpp`, `Engine/Graphics/CMakeLists.txt`.

**`Engine/Graphics/include/Pyramid/Graphics/Buffer/`:**
- Purpose: Public buffer interfaces and layout definitions.
- Contains: Vertex/index/vertex-array/uniform/instance/SSBO buffer interfaces and `BufferLayout`.
- Key files: `Engine/Graphics/include/Pyramid/Graphics/Buffer/VertexBuffer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Buffer/IndexBuffer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Buffer/VertexArray.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Buffer/UniformBuffer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Buffer/BufferLayout.hpp`.

**`Engine/Graphics/include/Pyramid/Graphics/OpenGL/`:**
- Purpose: OpenGL concrete resource classes and state manager headers.
- Contains: `OpenGLDevice`, `OpenGLStateManager`, `OpenGLTexture`, `OpenGLFramebuffer`, buffer implementations, and shader implementation headers.
- Key files: `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/Shader/OpenGLShader.hpp`.

**`Engine/Graphics/source/OpenGL/`:**
- Purpose: OpenGL backend implementation.
- Contains: `OpenGLDevice.cpp`, `OpenGLStateManager.cpp`, `OpenGLTexture.cpp`, `OpenGLFramebuffer.cpp`, `Buffer` implementations, and shader implementation.
- Key files: `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`, `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp`.

**`Engine/Graphics/include/Pyramid/Graphics/Renderer/` and `Engine/Graphics/source/Renderer/`:**
- Purpose: Command-buffer and render-pass based rendering pipeline.
- Contains: `RenderSystem`, `RenderPasses`, shader path resolving, forward/shadow/deferred pass implementations.
- Key files: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`, `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`, `Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`.

**`Engine/Graphics/include/Pyramid/Graphics/Scene/` and `Engine/Graphics/source/Scene/`:**
- Purpose: Advanced scene management and spatial partitioning.
- Contains: `SceneManager` and `Octree` headers/implementations.
- Key files: `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`, `Engine/Graphics/source/Scene/SceneManager.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Engine/Graphics/source/Scene/Octree.cpp`.

**`Engine/Platform/`:**
- Purpose: Platform abstraction and Windows OpenGL window implementation.
- Contains: `include/Pyramid/Platform/Window.hpp`, `include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, and `source/Windows/Win32OpenGLWindow.cpp`.
- Key files: `Engine/Platform/CMakeLists.txt`, `Engine/Platform/include/Pyramid/Platform/Window.hpp`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`.

**`Engine/Math/`:**
- Purpose: Engine math primitives and SIMD/performance helpers.
- Contains: `include/Pyramid/Math` public headers and `source` implementations.
- Key files: `Engine/Math/include/Pyramid/Math/Math.hpp`, `Engine/Math/include/Pyramid/Math/Vec3.hpp`, `Engine/Math/include/Pyramid/Math/Mat4.hpp`, `Engine/Math/include/Pyramid/Math/Quat.hpp`, `Engine/Math/source/MathSIMD.cpp`, `Engine/Math/CMakeLists.txt`.

**`Engine/Utils/`:**
- Purpose: Cross-cutting utilities, image loading, compression helpers, logging, and utility tests.
- Contains: `include/Pyramid/Util`, `source`, and `test`.
- Key files: `Engine/Utils/include/Pyramid/Util/Log.hpp`, `Engine/Utils/source/Log.cpp`, `Engine/Utils/include/Pyramid/Util/Image.hpp`, `Engine/Utils/source/Image.cpp`, `Engine/Utils/source/PNGLoader.cpp`, `Engine/Utils/source/JPEGLoader.cpp`, `Engine/Utils/test/CMakeLists.txt`.

**`Engine/Renderer/`, `Engine/Input/`, `Engine/Scene/`, `Engine/Audio/`, `Engine/Physics/`:**
- Purpose: Module placeholders/include-directory hooks in the current CMake graph.
- Contains: `CMakeLists.txt` files that add include directories to `PyramidEngine`.
- Key files: `Engine/Renderer/CMakeLists.txt`, `Engine/Input/CMakeLists.txt`, `Engine/Scene/CMakeLists.txt`, `Engine/Audio/CMakeLists.txt`, `Engine/Physics/CMakeLists.txt`.

**`Engine/Utils/test/`:**
- Purpose: Executable-based utility tests registered when `PYRAMID_BUILD_TESTS=ON`.
- Contains: PNG and JPEG component/integration test `.cpp` files.
- Key files: `Engine/Utils/test/TestPNGComponents.cpp`, `Engine/Utils/test/TestJPEGSimple.cpp`, `Engine/Utils/test/TestJPEGIntegration.cpp`, `Engine/Utils/test/CMakeLists.txt`.

**`Examples/BasicGame/`:**
- Purpose: Runnable sample using `Pyramid::Game`, `RenderSystem`, `Scene`, `Camera`, shaders, and engine resource factories.
- Contains: `include/BasicGame.hpp`, `source/Main.cpp`, `source/BasicGame.cpp`, and `CMakeLists.txt`.
- Key files: `Examples/BasicGame/source/BasicGame.cpp`, `Examples/BasicGame/include/BasicGame.hpp`, `Examples/BasicGame/CMakeLists.txt`.

**`Examples/BasicRendering/`:**
- Purpose: Standalone basic rendering sample linked against `PyramidEngine`.
- Contains: `Main.cpp`, `BasicRendering.cpp`, `BasicRendering.hpp`, and `CMakeLists.txt`.
- Key files: `Examples/BasicRendering/Main.cpp`, `Examples/BasicRendering/BasicRendering.cpp`, `Examples/BasicRendering/CMakeLists.txt`.

**`docs/`:**
- Purpose: Human-readable architecture, build, API, contribution, examples, and smoke-test references.
- Contains: Top-level docs plus `API_Reference` and `Examples` subdirectories.
- Key files: `docs/Architecture.md`, `docs/ARCHITECTURE_AND_BEST_PRACTICES.md`, `docs/BuildGuide.md`, `docs/API_Reference/README.md`, `docs/SmokeTests.md`.

**`vendor/glad/`:**
- Purpose: Bundled OpenGL loader dependency.
- Contains: `CMakeLists.txt`, include files, and source files for `glad`.
- Key files: `vendor/glad/CMakeLists.txt`.

**`Game/`:**
- Purpose: Separate game target stub outside `Examples`.
- Contains: `CMakeLists.txt` and expected `Main.cpp` target source.
- Key files: `Game/CMakeLists.txt`. The target currently links `OpenGL3D`, not `PyramidEngine`.

## Key File Locations

**Entry Points:**
- `CMakeLists.txt`: Root configure entry point and build option definitions.
- `Engine/CMakeLists.txt`: `PyramidEngine` target entry point.
- `Engine/Core/source/Game.cpp`: Engine runtime lifecycle entry point through `Game::run`.
- `Examples/BasicGame/source/Main.cpp`: BasicGame executable entry point.
- `Examples/BasicRendering/Main.cpp`: BasicRendering executable entry point.

**Configuration:**
- `CMakeLists.txt`: Root project version, C++ standard, options, output directories, install rules.
- `CMakePresets.json`: Preset CMake configurations.
- `Engine/CMakeLists.txt`: Engine target properties, include paths, links, compile definitions.
- `Engine/*/CMakeLists.txt`: Module source/header registration and include directories.
- `Engine/Utils/test/CMakeLists.txt`: Utility test target helper and `add_test` registrations.
- `Examples/BasicGame/CMakeLists.txt`: `BasicGame` executable target configuration.
- `Examples/BasicRendering/CMakeLists.txt`: `BasicRenderingExample` executable target configuration.

**Core Logic:**
- `Engine/Core/include/Pyramid/Core/Game.hpp`: Public game lifecycle API.
- `Engine/Core/source/Game.cpp`: Window/device startup, main loop, shutdown.
- `Engine/Core/include/Pyramid/Core/Prerequisites.hpp`: Engine integer/float aliases, `GraphicsAPI`, `Color`, common forward declarations.
- `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`: Graphics device interface and resource factory API.
- `Engine/Graphics/source/GraphicsDevice.cpp`: Graphics backend factory dispatch.
- `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp`: OpenGL graphics device public implementation header.
- `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`: OpenGL device implementation.
- `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp`: OpenGL state cache interface.
- `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`: OpenGL state cache implementation.
- `Engine/Platform/include/Pyramid/Platform/Window.hpp`: Platform-neutral window interface.
- `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`: Windows message pump and OpenGL context implementation.
- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`: Command buffer, render target, render pass, render system API.
- `Engine/Graphics/source/Renderer/RenderSystem.cpp`: Render-system frame orchestration.
- `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`: Scene, scene node, render object, light, environment API.
- `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`: Advanced scene manager API.
- `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`: Spatial partitioning API.
- `Engine/Math/include/Pyramid/Math/Math.hpp`: Math umbrella header.
- `Engine/Utils/include/Pyramid/Util/Log.hpp`: Logging and assertion macros.

**Testing:**
- `Engine/Utils/test/CMakeLists.txt`: Test target registration.
- `Engine/Utils/test/TestPNGComponents.cpp`: PNG component test executable.
- `Engine/Utils/test/TestPNGLoader.cpp`: PNG loader test source present in tree, not registered in current test CMake file.
- `Engine/Utils/test/TestJPEGSimple.cpp`: JPEG smoke test executable.
- `Engine/Utils/test/TestJPEGHuffman.cpp`: JPEG Huffman test executable.
- `Engine/Utils/test/TestJPEGDequantizer.cpp`: JPEG dequantizer test executable.
- `Engine/Utils/test/TestJPEGIDCT.cpp`: JPEG IDCT test executable.
- `Engine/Utils/test/TestJPEGColorConverter.cpp`: JPEG color conversion test executable.
- `Engine/Utils/test/TestJPEGIntegration.cpp`: JPEG integration test executable.

**Documentation:**
- `docs/Architecture.md`: Existing architecture overview.
- `docs/ARCHITECTURE_AND_BEST_PRACTICES.md`: Detailed rendering/backend architecture notes.
- `docs/BuildGuide.md`: Build instructions.
- `docs/API_Reference/Graphics/RenderSystem.md`: Render-system API documentation.
- `docs/API_Reference/Core/Game.md`: Game API documentation.
- `docs/API_Reference/Utils/Logging.md`: Logging API documentation.

## Naming Conventions

**Files:**
- Engine public C++ headers use `.hpp` under `include/Pyramid/<Module>/`, for example `Engine/Graphics/include/Pyramid/Graphics/Camera.hpp`.
- Engine source files use `.cpp` under `source/`, for example `Engine/Graphics/source/Camera.cpp`.
- OpenGL backend files use `OpenGL` prefix and live under `OpenGL` subdirectories, for example `Engine/Graphics/source/OpenGL/Buffer/OpenGLVertexBuffer.cpp`.
- Test files use `Test<Feature>.cpp`, for example `Engine/Utils/test/TestJPEGIntegration.cpp`.
- Example main files use `Main.cpp`, for example `Examples/BasicGame/source/Main.cpp` and `Examples/BasicRendering/Main.cpp`.
- CMake files are `CMakeLists.txt` at each configured directory.

**Directories:**
- Active engine modules follow `Engine/<Module>/include/Pyramid/<Module>` plus `Engine/<Module>/source`, for example `Engine/Math/include/Pyramid/Math` and `Engine/Math/source`.
- Public graphics subdomains mirror conceptual namespaces or backends, for example `Engine/Graphics/include/Pyramid/Graphics/Renderer`, `Engine/Graphics/include/Pyramid/Graphics/Scene`, and `Engine/Graphics/include/Pyramid/Graphics/OpenGL`.
- Platform-specific implementations live under platform-named directories, for example `Engine/Platform/source/Windows`.
- Examples live under `Examples/<ExampleName>`, for example `Examples/BasicGame`.

## Where to Add New Code

**New Engine Feature:**
- Primary code: Add public API under `Engine/<Module>/include/Pyramid/<Module>/` and implementation under `Engine/<Module>/source/` when the module exists and is active.
- CMake: Register sources/headers in the module CMake file such as `Engine/Graphics/CMakeLists.txt`, `Engine/Math/CMakeLists.txt`, or `Engine/Utils/CMakeLists.txt`.
- Tests: Add executable tests under `Engine/Utils/test/` only for utility/image/compression functionality; add CMake registration in `Engine/Utils/test/CMakeLists.txt`.

**New Graphics Resource Type:**
- Interface: Add public API under `Engine/Graphics/include/Pyramid/Graphics/<Category>/` or `Engine/Graphics/include/Pyramid/Graphics`.
- OpenGL implementation: Add concrete header/source under `Engine/Graphics/include/Pyramid/Graphics/OpenGL/...` and `Engine/Graphics/source/OpenGL/...`.
- Factory: Extend `IGraphicsDevice` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` and implement creation in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`.
- CMake: Add files to `Engine/Graphics/CMakeLists.txt`.

**New Render Pass:**
- Declaration: Add the pass to `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`.
- Implementation: Add `Engine/Graphics/source/Renderer/<PassName>.cpp`.
- Registration: Add source to `Engine/Graphics/CMakeLists.txt` and add pass setup in `RenderSystem::SetupDefaultRenderPasses` or `RenderSystem::SetupDeferredPipeline` in `Engine/Graphics/source/Renderer/RenderSystem.cpp`.

**New Scene / Spatial Feature:**
- Primary code: Use `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp` for core scene API or `Engine/Graphics/include/Pyramid/Graphics/Scene/` for advanced scene-management features.
- Implementation: Use `Engine/Graphics/source/Scene.cpp` or `Engine/Graphics/source/Scene/<Feature>.cpp`.
- CMake: Register files in `Engine/Graphics/CMakeLists.txt`.

**New Platform Window Backend:**
- Interface: Reuse `Engine/Platform/include/Pyramid/Platform/Window.hpp`.
- Implementation: Add headers under `Engine/Platform/include/Pyramid/Platform/<Platform>/` and sources under `Engine/Platform/source/<Platform>/`.
- Factory selection: Update `Engine/Core/source/Game.cpp` or introduce a dedicated platform factory before adding more platform-specific includes to app code.
- CMake: Register files in `Engine/Platform/CMakeLists.txt` and add platform-specific libraries conditionally.

**New Math Type or Operation:**
- Header: Add to `Engine/Math/include/Pyramid/Math/`.
- Source: Add to `Engine/Math/source/` when implementation is not header-only.
- Umbrella: Include the public header from `Engine/Math/include/Pyramid/Math/Math.hpp` when it is part of the common math API.
- CMake: Register files in `Engine/Math/CMakeLists.txt`.

**New Utility / Image Decoder Feature:**
- Header: Add to `Engine/Utils/include/Pyramid/Util/`.
- Source: Add to `Engine/Utils/source/`.
- Tests: Add `Engine/Utils/test/Test<Feature>.cpp` and call `add_utils_test(Test<Feature> Test<Feature>.cpp)` in `Engine/Utils/test/CMakeLists.txt`.
- CMake: Register utility implementation in `Engine/Utils/CMakeLists.txt`.

**New Example:**
- Implementation: Add a directory under `Examples/<ExampleName>/`.
- CMake: Add `Examples/<ExampleName>/CMakeLists.txt` with `add_executable` and `target_link_libraries(<ExampleName> PRIVATE PyramidEngine)`.
- Root wiring: Add `add_subdirectory(Examples/<ExampleName>)` under the `PYRAMID_BUILD_EXAMPLES` block in root `CMakeLists.txt`.
- Docs: Add or update docs under `docs/Examples/`.

**Utilities:**
- Shared diagnostics: Use `Engine/Utils/include/Pyramid/Util/Log.hpp` and `Engine/Utils/source/Log.cpp`.
- Shared math: Use `Engine/Math/include/Pyramid/Math/Math.hpp`.
- Shared graphics helpers: Place renderer-specific helpers under `Engine/Graphics/include/Pyramid/Graphics/Renderer/` and `Engine/Graphics/source/Renderer/`.

## Special Directories

**`vendor/glad/`:**
- Purpose: Bundled OpenGL loader dependency.
- Generated: No.
- Committed: Yes.

**`build/`:**
- Purpose: Local CMake build output directory.
- Generated: Yes.
- Committed: No.

**`.planning/codebase/`:**
- Purpose: Generated codebase maps for GSD commands.
- Generated: Yes.
- Committed: Project-dependent; keep content free of secrets and current-state focused.

**`Engine/Graphics/shaders/`:**
- Purpose: Runtime shader assets for renderer passes.
- Generated: No.
- Committed: Yes.

**`Engine/Utils/test/`:**
- Purpose: Executable-based utility tests.
- Generated: No.
- Committed: Yes.

**`docs/API_Reference/`:**
- Purpose: API documentation grouped by engine subsystem.
- Generated: No.
- Committed: Yes.

---

*Structure analysis: Thu May 14 2026*
