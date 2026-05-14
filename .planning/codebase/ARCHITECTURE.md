<!-- refreshed: Thu May 14 2026 -->
# Architecture

**Analysis Date:** Thu May 14 2026

## System Overview

```text
┌─────────────────────────────────────────────────────────────┐
│                   Application / Examples                     │
├──────────────────────────────┬──────────────────────────────┤
│ `Examples/BasicGame`         │ `Examples/BasicRendering`    │
│ subclasses `Pyramid::Game`   │ direct sample executable     │
└───────────────┬──────────────┴───────────────┬──────────────┘
                │                              │
                ▼                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      Core Lifecycle                          │
│ `Engine/Core/include/Pyramid/Core/Game.hpp`                  │
│ `Engine/Core/source/Game.cpp`                                │
└───────────────┬──────────────────────────────┬──────────────┘
                │ owns                         │ creates
                ▼                              ▼
┌──────────────────────────────┐  ┌───────────────────────────┐
│ Platform Window              │  │ Graphics Device API        │
│ `Engine/Platform/...`        │  │ `Engine/Graphics/...`      │
│ Win32 OpenGL implementation  │  │ OpenGL backend             │
└───────────────┬──────────────┘  └──────────────┬────────────┘
                │                                │
                ▼                                ▼
┌─────────────────────────────────────────────────────────────┐
│ Rendering, Scene, Resources, Math, Utilities                 │
│ `Engine/Graphics`, `Engine/Math`, `Engine/Utils`             │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ Native / Vendor Layer                                        │
│ OpenGL + Win32 + `vendor/glad`                               │
└─────────────────────────────────────────────────────────────┘
```

## Component Responsibilities

| Component | Responsibility | File |
|-----------|----------------|------|
| `Pyramid::Game` | Owns the window, owns the graphics device, runs the frame loop, and exposes `onCreate`, `onUpdate`, and `onRender` override points. | `Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Core/source/Game.cpp` |
| `Pyramid::Window` | Platform-neutral window and graphics-context interface used by `Game` and `OpenGLDevice`. | `Engine/Platform/include/Pyramid/Platform/Window.hpp` |
| `Pyramid::Win32OpenGLWindow` | Windows-only window creation, message pump, OpenGL context creation, and swap/present. | `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp` |
| `Pyramid::IGraphicsDevice` | Graphics API abstraction and factory surface for buffers, shaders, textures, framebuffers, draw commands, and state control. | `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Graphics/source/GraphicsDevice.cpp` |
| `Pyramid::OpenGLDevice` | OpenGL implementation of `IGraphicsDevice`; delegates state caching to `OpenGLStateManager` and presents through `Window`. | `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` |
| `Pyramid::Renderer::RenderSystem` | Frame orchestration, command buffers, render targets, render pass ordering, global camera/lighting UBO updates, and frame presentation. | `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp` |
| `Pyramid::Renderer::RenderPass` and subclasses | Encapsulate forward, shadow, deferred geometry, and deferred lighting passes. | `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`, `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`, `Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp` |
| `Pyramid::Scene` | Holds scene graph nodes, render objects, lights, primary light, environment settings, and simple visibility queries. | `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`, `Engine/Graphics/source/Scene.cpp` |
| `Pyramid::SceneManagement::SceneManager` | Advanced scene lifecycle, multiple scenes, octree-backed spatial queries, update flags, stats, and scene events. | `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`, `Engine/Graphics/source/Scene/SceneManager.cpp` |
| `Pyramid::SceneManagement::Octree` | Spatial partitioning for point, box, sphere, ray, frustum, and nearest-object queries. | `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Engine/Graphics/source/Scene/Octree.cpp` |
| `Pyramid::Camera` | View/projection state, movement helpers, lazy matrix caching, and frustum support for rendering and culling. | `Engine/Graphics/include/Pyramid/Graphics/Camera.hpp`, `Engine/Graphics/source/Camera.cpp` |
| `Pyramid::Math` types | Vectors, matrices, quaternions, common numeric helpers, performance helpers, and SIMD routines. | `Engine/Math/include/Pyramid/Math/Math.hpp`, `Engine/Math/source/MathSIMD.cpp` |
| `Pyramid::Util::Logger` | Thread-safe singleton logging, file/console sinks, source locations, structured logging, and assertion macros. | `Engine/Utils/include/Pyramid/Util/Log.hpp`, `Engine/Utils/source/Log.cpp` |
| Image utilities | Self-contained image decode utilities for PNG/JPEG and supporting bitstream/DEFLATE/Huffman logic. | `Engine/Utils/include/Pyramid/Util/Image.hpp`, `Engine/Utils/source/PNGLoader.cpp`, `Engine/Utils/source/JPEGLoader.cpp` |

## Pattern Overview

**Overall:** Layered C++17 engine library with interface-based graphics/platform boundaries and OpenGL as the active backend.

**Key Characteristics:**
- Put public API headers under module `include/Pyramid/...` directories and implementation under matching `source/` directories.
- Add engine code to the single `PyramidEngine` library target by editing the module `CMakeLists.txt`, not by creating separate per-module libraries.
- Keep application code in `Examples/...` or `Game/...`; engine code should not depend on examples.
- Use `IGraphicsDevice` factory methods for GPU resources instead of constructing OpenGL classes in game code.
- Use `Pyramid::Renderer::RenderSystem` for pass-based rendering; direct `IGraphicsDevice::Clear`/`Present` is only the default fallback path in `Game::onRender`.

## Layers

**Application / Example Layer:**
- Purpose: Demonstrate and consume the engine library.
- Location: `Examples/BasicGame`, `Examples/BasicRendering`, `Game`
- Contains: Executables, app-specific subclasses, app-owned scene/camera/render system objects, and demo shaders.
- Depends on: `PyramidEngine`, public headers from `Engine/*/include`, and for `Examples/BasicGame` vendor include paths in `Examples/BasicGame/CMakeLists.txt`.
- Used by: Developers running samples and validating runtime behavior.

**Core Layer:**
- Purpose: Bootstrap the engine and run the main lifecycle.
- Location: `Engine/Core`
- Contains: `Pyramid::Game`, basic type aliases, `GraphicsAPI`, and `Color`.
- Depends on: `Engine/Graphics` interfaces, `Engine/Platform` interfaces, and `Engine/Utils` logging.
- Used by: `Examples/BasicGame/source/BasicGame.cpp` and future game applications.

**Platform Layer:**
- Purpose: Hide native window/context details behind `Pyramid::Window`.
- Location: `Engine/Platform`
- Contains: Platform-neutral `Window` interface and the current `Win32OpenGLWindow` implementation.
- Depends on: `Engine/Core` prerequisites, Win32 APIs, OpenGL context support, and `Engine/Utils` logging.
- Used by: `Engine/Core/source/Game.cpp` and `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`.

**Graphics API Layer:**
- Purpose: Expose API-independent GPU operations and implement them through OpenGL.
- Location: `Engine/Graphics`
- Contains: `IGraphicsDevice`, buffer interfaces, shader interface, texture/framebuffer abstractions, OpenGL concrete implementations, state manager, renderer, camera, and scene types.
- Depends on: `Engine/Core`, `Engine/Math`, `Engine/Utils`, `Engine/Platform`, `vendor/glad`, and `opengl32`.
- Used by: `Pyramid::Game`, `RenderSystem`, examples, and scene/render utilities.

**Rendering Layer:**
- Purpose: Organize GPU work into recorded commands and ordered render passes.
- Location: `Engine/Graphics/include/Pyramid/Graphics/Renderer`, `Engine/Graphics/source/Renderer`
- Contains: `CommandBuffer`, `RenderTarget`, `RenderPass`, `RenderSystem`, material/lighting structs, shadow/forward/deferred pass implementations, shader path resolution.
- Depends on: `IGraphicsDevice`, scene objects, camera matrices, graphics resources, OpenGL framebuffer/state details for internal render-target work.
- Used by: `Examples/BasicGame/source/BasicGame.cpp` and future runtime render loops.

**Scene Layer:**
- Purpose: Store renderable world data and provide spatial queries/culling inputs.
- Location: `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Scene`, `Engine/Graphics/source/Scene`
- Contains: `Scene`, `SceneNode`, `RenderObject`, `Light`, `Environment`, `SceneManager`, `Octree`, `AABB`, and scene utility factories.
- Depends on: `Engine/Math`, `Engine/Core`, rendering material definitions, and optional camera visibility logic.
- Used by: `RenderSystem` passes, examples, and scene-management callers.

**Math Layer:**
- Purpose: Provide engine numeric primitives and transformations.
- Location: `Engine/Math`
- Contains: `Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, `Quat`, common math functions, SIMD/performance helpers, umbrella `Math.hpp`.
- Depends on: `Engine/Core` prerequisites and `Engine/Utils` logging for SIMD feature reporting.
- Used by: Graphics, scene, camera, rendering, examples, and utility geometry code.

**Utilities Layer:**
- Purpose: Provide cross-cutting support systems that are not tied to rendering lifecycle.
- Location: `Engine/Utils`
- Contains: Logging/assertions, `Image`, PNG/JPEG loaders, bit readers, Huffman/DEFLATE/ZLib helpers, and executable tests under `Engine/Utils/test`.
- Depends on: `Engine/Core` prerequisites and standard C++ library facilities.
- Used by: All engine modules for logging and texture/image loading.

**Build Layer:**
- Purpose: Compose the engine, examples, tests, install headers, and vendor dependency.
- Location: `CMakeLists.txt`, `Engine/CMakeLists.txt`, module `CMakeLists.txt` files, `vendor/glad/CMakeLists.txt`
- Contains: CMake options, target source registration, target include directories, target links, test executable helper, output directories, and install rules.
- Depends on: CMake 3.16+, C++17 compiler, `opengl32` on Windows.
- Used by: Local builds, CI, and consumers installing `PyramidEngine`.

## Data Flow

### Primary Game Loop Path

1. An executable constructs an app class such as `BasicGame`, which calls `Game(GraphicsAPI::OpenGL)` (`Examples/BasicGame/source/BasicGame.cpp:64`).
2. `Game::Game` creates `Win32OpenGLWindow`, initializes it at 1280x720, and creates an `IGraphicsDevice` through `IGraphicsDevice::Create` (`Engine/Core/source/Game.cpp:20`, `Engine/Core/source/Game.cpp:37`, `Engine/Core/source/Game.cpp:45`).
3. `IGraphicsDevice::Create` selects `OpenGLDevice` for `GraphicsAPI::OpenGL` (`Engine/Graphics/source/GraphicsDevice.cpp:9`).
4. `Game::run` calls `onCreate`, initializes frame timing, processes window messages, calls `onUpdate(deltaTime)`, then calls `onRender()` (`Engine/Core/source/Game.cpp:120`).
5. A render-enabled app creates `RenderSystem`, `Scene`, `Camera`, shaders, vertex/index buffers, and render objects during `onCreate` (`Examples/BasicGame/source/BasicGame.cpp:87`).
6. Each frame, `BasicGame::onRender` calls `RenderSystem::BeginFrame`, `RenderSystem::Render`, then `RenderSystem::EndFrame` (`Examples/BasicGame/source/BasicGame.cpp:147`).
7. `RenderSystem::Render` updates camera/lighting uniforms, executes enabled passes in pass-type order, records commands into `CommandBuffer`, executes them on `IGraphicsDevice`, and records frame time (`Engine/Graphics/source/Renderer/RenderSystem.cpp:587`).
8. `RenderSystem::EndFrame` executes trailing commands and presents the frame through `IGraphicsDevice::Present` (`Engine/Graphics/source/Renderer/RenderSystem.cpp:615`).
9. `OpenGLDevice::Present` calls `Window::Present`, so swapping stays in the platform implementation (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:154`).

### Engine Initialization Path

1. `Game::Game` creates the native window before the graphics device (`Engine/Core/source/Game.cpp:19`).
2. `Win32OpenGLWindow::Initialize` owns native window/context setup (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`).
3. `Game::onCreate` calls `m_graphicsDevice->Initialize()` (`Engine/Core/source/Game.cpp:91`).
4. `OpenGLDevice::Initialize` validates the window, makes the OpenGL context current, resets `OpenGLStateManager`, sets the viewport from window size, and marks the device initialized (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:86`).
5. App override code must call `Game::onCreate()` before creating renderer resources, as shown in `Examples/BasicGame/source/BasicGame.cpp:71`.

### Resource Creation Path

1. Game or renderer code requests resources from `IGraphicsDevice` (`Examples/BasicGame/source/BasicGame.cpp:189`).
2. `OpenGLDevice` creates concrete OpenGL resources such as `OpenGLVertexBuffer`, `OpenGLIndexBuffer`, `OpenGLVertexArray`, `OpenGLShader`, `OpenGLUniformBuffer`, `OpenGLInstanceBuffer`, and `OpenGLShaderStorageBuffer` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:189`).
3. Texture creation flows through `ITexture2D::Create`, which is implemented in `Engine/Graphics/source/Texture.cpp` and returns OpenGL-backed textures.
4. Commands bind resources through `IGraphicsDevice` methods such as `BindShader`, `BindTexture`, `BindUniformBuffer`, and `BindVertexArray` from `CommandBuffer::Execute` (`Engine/Graphics/source/Renderer/RenderSystem.cpp:235`).

### Scene and Spatial Query Path

1. `Scene` stores `RenderObject` and `Light` shared pointers (`Engine/Graphics/include/Pyramid/Graphics/Scene.hpp:193`).
2. `SceneManager` creates, stores, and activates named scenes (`Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp:104`).
3. When spatial partitioning is enabled, `SceneManager` owns an `Octree` and rebuilds/updates it for scene objects (`Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp:117`).
4. Queries dispatch by type through `QueryScene`, `GetVisibleObjects`, `GetObjectsInRadius`, `GetObjectsInBox`, and `GetNearestObject` (`Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp:124`).
5. `Octree` performs point, sphere, box, ray, frustum, and nearest-neighbor searches over `RenderObject` bounds (`Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp:147`).

**State Management:**
- Runtime ownership uses RAII: `Game` owns `std::unique_ptr<Window>` and `std::unique_ptr<IGraphicsDevice>` (`Engine/Core/include/Pyramid/Core/Game.hpp:82`).
- Shared scene resources use `std::shared_ptr`, including `RenderObject`, `Light`, buffers, shaders, textures, render passes, and render targets (`Engine/Graphics/include/Pyramid/Graphics/Scene.hpp:206`, `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp:259`).
- OpenGL state is centralized through singleton `OpenGLStateManager` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:116`, `Engine/Graphics/source/Renderer/RenderSystem.cpp:401`).
- Logging state is centralized through singleton `Pyramid::Util::Logger` (`Engine/Utils/include/Pyramid/Util/Log.hpp:95`).

## Key Abstractions

**`Pyramid::Game`:**
- Purpose: Base class for runnable applications.
- Examples: `Engine/Core/include/Pyramid/Core/Game.hpp`, `Examples/BasicGame/include/BasicGame.hpp`
- Pattern: Template-method lifecycle with virtual `onCreate`, `onUpdate`, and `onRender`; call `Game::onCreate()` first in overrides.

**`Pyramid::Window`:**
- Purpose: Platform-independent window and GL context contract.
- Examples: `Engine/Platform/include/Pyramid/Platform/Window.hpp`, `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`
- Pattern: Interface plus platform-specific implementation selected in core startup.

**`Pyramid::IGraphicsDevice`:**
- Purpose: API-neutral rendering and resource creation contract.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp`
- Pattern: Abstract interface with static factory and OpenGL implementation.

**Graphics Resource Interfaces:**
- Purpose: Hide OpenGL buffer/shader/texture concrete types from game code.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Buffer/VertexBuffer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Buffer/IndexBuffer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Buffer/VertexArray.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Shader/Shader.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Texture.hpp`
- Pattern: Public `I*` interfaces, OpenGL implementations under `Engine/Graphics/include/Pyramid/Graphics/OpenGL` and `Engine/Graphics/source/OpenGL`.

**`Pyramid::Renderer::CommandBuffer`:**
- Purpose: Record render commands before executing them against an `IGraphicsDevice`.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp`
- Pattern: Begin/end recording, append typed commands, resolve registered IDs or pointer commands, then execute.

**`Pyramid::Renderer::RenderPass`:**
- Purpose: Encapsulate ordered rendering stages.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`, `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`
- Pattern: Virtual `Begin`, `Execute`, `End` implemented by stage-specific subclasses; `RenderSystem::AddRenderPass` sorts by `RenderPassType`.

**`Pyramid::Scene` and `Pyramid::SceneNode`:**
- Purpose: Store scene graph transforms, renderable objects, lights, and environment data.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Scene.hpp`, `Engine/Graphics/source/Scene.cpp`
- Pattern: Shared-pointer graph and object containers with lazy transform matrices.

**`Pyramid::SceneManagement::Octree`:**
- Purpose: Accelerate spatial queries and visibility-related operations.
- Examples: `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`, `Engine/Graphics/source/Scene/Octree.cpp`
- Pattern: Tree of `OctreeNode` objects with `AABB` bounds and stored `RenderObject` pointers.

**`Pyramid::Math` Aggregator:**
- Purpose: Provide one include for math primitives.
- Examples: `Engine/Math/include/Pyramid/Math/Math.hpp`, `Engine/Math/include/Pyramid/Math/Vec3.hpp`, `Engine/Math/include/Pyramid/Math/Mat4.hpp`
- Pattern: Public types live in `Pyramid` namespace with an aggregator header in `Pyramid/Math/Math.hpp`.

**`Pyramid::Util::Logger`:**
- Purpose: Engine-wide diagnostics and assertions.
- Examples: `Engine/Utils/include/Pyramid/Util/Log.hpp`, `Engine/Utils/source/Log.cpp`
- Pattern: Thread-safe singleton accessed through macros such as `PYRAMID_LOG_INFO` and `PYRAMID_ASSERT`.

## Entry Points

**Top-level CMake project:**
- Location: `CMakeLists.txt`
- Triggers: CMake configure from repository root.
- Responsibilities: Set C++17, define build options, add `vendor/glad`, add `Engine`, optionally add examples/tools, configure install rules.

**Engine CMake target composition:**
- Location: `Engine/CMakeLists.txt`
- Triggers: `add_subdirectory(Engine)` from root `CMakeLists.txt`.
- Responsibilities: Create `PyramidEngine`, include all engine module subdirectories, link `glad` and `opengl32`, define `PYRAMID_SOURCE_DIR`.

**Game lifecycle:**
- Location: `Engine/Core/source/Game.cpp`
- Triggers: Application calls `Pyramid::Game::run()`.
- Responsibilities: Initialize graphics, run update/render loop, process window messages, shut down graphics device.

**BasicGame executable:**
- Location: `Examples/BasicGame/source/Main.cpp`, `Examples/BasicGame/source/BasicGame.cpp`
- Triggers: Built target `BasicGame` from `Examples/BasicGame/CMakeLists.txt`.
- Responsibilities: Exercise the engine API with scene/camera/render-system driven rendering.

**BasicRendering executable:**
- Location: `Examples/BasicRendering/Main.cpp`, `Examples/BasicRendering/BasicRendering.cpp`
- Triggers: Built target `BasicRenderingExample` from `Examples/BasicRendering/CMakeLists.txt`.
- Responsibilities: Provide a standalone rendering sample linked against `PyramidEngine`.

**Utility tests:**
- Location: `Engine/Utils/test/CMakeLists.txt`
- Triggers: Configure with `PYRAMID_BUILD_TESTS=ON`.
- Responsibilities: Create executable tests such as `TestPNGComponents`, `TestJPEGSimple`, and JPEG component tests; register them with `add_test`.

## CMake Target Hierarchy

```text
Pyramid (root project, `CMakeLists.txt`)
├── glad (`vendor/glad/CMakeLists.txt`)
├── PyramidEngine (`Engine/CMakeLists.txt`)
│   ├── Core sources (`Engine/Core/CMakeLists.txt`)
│   ├── Graphics + Renderer + Scene sources (`Engine/Graphics/CMakeLists.txt`)
│   ├── Platform sources (`Engine/Platform/CMakeLists.txt`)
│   ├── Math sources (`Engine/Math/CMakeLists.txt`)
│   ├── Utils sources and optional tests (`Engine/Utils/CMakeLists.txt`)
│   ├── Header-only/include-only module hooks (`Engine/Renderer`, `Engine/Input`, `Engine/Scene`, `Engine/Audio`, `Engine/Physics`)
│   └── links: `glad`, `opengl32`
├── BasicGame (`Examples/BasicGame/CMakeLists.txt`, when `PYRAMID_BUILD_EXAMPLES=ON`)
├── BasicRenderingExample (`Examples/BasicRendering/CMakeLists.txt`, when `PYRAMID_BUILD_EXAMPLES=ON`)
└── Utils tests (`Engine/Utils/test/CMakeLists.txt`, when `PYRAMID_BUILD_TESTS=ON`)
```

Use these CMake patterns:
- Register engine implementation files with `target_sources(PyramidEngine PRIVATE ...)` in the owning module CMake file, as in `Engine/Graphics/CMakeLists.txt`.
- Register public headers with `target_sources(PyramidEngine PUBLIC ...)` when the module already follows that pattern, as in `Engine/Core/CMakeLists.txt`.
- Add public include directories to `PyramidEngine` with `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>` and `$<INSTALL_INTERFACE:include>`, as in `Engine/Math/CMakeLists.txt`.
- Add executable examples with `target_link_libraries(<Example> PRIVATE PyramidEngine)`, as in `Examples/BasicGame/CMakeLists.txt`.

## Architectural Constraints

- **Threading:** The main lifecycle is a single-threaded event/render loop in `Game::run` (`Engine/Core/source/Game.cpp:146`). Logging uses mutex protection (`Engine/Utils/include/Pyramid/Util/Log.hpp:147`). No worker-thread job system is present in the active engine modules.
- **Platform support:** Runtime window/context implementation is Windows OpenGL only through `Win32OpenGLWindow` (`Engine/Core/source/Game.cpp:22`). `GraphicsAPI` lists DirectX and Vulkan values, but the factory returns only `OpenGL` (`Engine/Graphics/source/GraphicsDevice.cpp:11`).
- **Graphics backend:** `PyramidEngine` links `opengl32` and `glad` privately (`Engine/CMakeLists.txt:29`). Add non-OpenGL backends behind `IGraphicsDevice` and do not require examples to include backend-specific headers.
- **Global state:** OpenGL state is a singleton in `OpenGLStateManager` and logging is a singleton in `Logger` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:116`, `Engine/Utils/include/Pyramid/Util/Log.hpp:98`). Keep new global mutable state out of application-facing APIs unless it is a deliberate engine service.
- **Ownership:** `OpenGLDevice` stores a non-owning `Window*` (`Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp:75`). `Game` owns the window and graphics device; preserve destruction order by shutting down graphics before window destruction (`Engine/Core/source/Game.cpp:60`).
- **Headers vs sources:** Public engine includes must remain under `Engine/<Module>/include/Pyramid/...`; source-only helpers belong under `Engine/<Module>/source` and should not be included by examples.
- **Circular imports:** Header cycles are reduced with forward declarations in `GraphicsDevice.hpp` (`Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp:6`). Preserve forward declarations for resource interfaces and include concrete OpenGL headers in `.cpp` files where possible.
- **Namespace layout:** Most engine types live directly in `Pyramid`; renderer types live in `Pyramid::Renderer`; advanced scene management lives in `Pyramid::SceneManagement`; logging internals live in `Pyramid::Util` (`Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp:22`, `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp:22`, `Engine/Utils/include/Pyramid/Util/Log.hpp:30`).

## Anti-Patterns

### Constructing OpenGL Resources in Application Code

**What happens:** Application code can include OpenGL implementation headers directly because they are public in `Engine/Graphics/CMakeLists.txt`.
**Why it's wrong:** It couples game code to the active backend and bypasses `IGraphicsDevice`, making future backend selection through `GraphicsAPI` harder.
**Do this instead:** Request buffers, shaders, textures, UBOs, instance buffers, and SSBOs through `IGraphicsDevice` as shown in `Examples/BasicGame/source/BasicGame.cpp:189` and implemented in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:189`.

### Presenting Twice in a Custom Render Loop

**What happens:** A `Game::onRender` override can call `Game::onRender()` after using `RenderSystem`, causing both the fallback clear/present and render-system present paths to run.
**Why it's wrong:** `Game::onRender` directly clears and presents (`Engine/Core/source/Game.cpp:110`), while `RenderSystem::EndFrame` also presents (`Engine/Graphics/source/Renderer/RenderSystem.cpp:626`).
**Do this instead:** If using `RenderSystem`, call only `BeginFrame`, `Render`, and `EndFrame`; reserve `Game::onRender()` for fallback behavior when renderer state is unavailable, as in `Examples/BasicGame/source/BasicGame.cpp:149`.

### Adding Active Engine Code to Include-Only Placeholder Modules

**What happens:** `Engine/Renderer`, `Engine/Input`, `Engine/Scene`, `Engine/Audio`, and `Engine/Physics` currently only add include directories and no sources (`Engine/Renderer/CMakeLists.txt`, `Engine/Input/CMakeLists.txt`, `Engine/Scene/CMakeLists.txt`, `Engine/Audio/CMakeLists.txt`, `Engine/Physics/CMakeLists.txt`).
**Why it's wrong:** Adding source files under those modules without updating their CMake files leaves code uncompiled; duplicating concepts already under `Engine/Graphics/Renderer` or `Engine/Graphics/Scene` fragments architecture.
**Do this instead:** For rendering and scene functionality, extend `Engine/Graphics/include/Pyramid/Graphics/Renderer`, `Engine/Graphics/source/Renderer`, `Engine/Graphics/include/Pyramid/Graphics/Scene`, and `Engine/Graphics/source/Scene`, then register files in `Engine/Graphics/CMakeLists.txt`.

### Letting Backend State Bypass the State Manager

**What happens:** New OpenGL code can call raw binding/state functions directly.
**Why it's wrong:** `OpenGLStateManager` tracks state-change counts and cached bindings; direct state calls create cache drift and reduce the value of `GetStateChangeCount` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:279`).
**Do this instead:** Route OpenGL state changes through `OpenGLStateManager` from backend code, following `OpenGLDevice::SetViewport`, `OpenGLDevice::EnableBlend`, and `OpenGLDevice::SetClearColor` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:184`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:239`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:274`).

## Error Handling

**Strategy:** Engine code reports recoverable failures through boolean returns or null smart pointers and logs detailed diagnostics with `PYRAMID_LOG_*`; assertions are available for debug-only invariants.

**Patterns:**
- Constructors or initializers validate dependencies and return early with log messages, as in `Game::Game` (`Engine/Core/source/Game.cpp:30`) and `OpenGLDevice::Initialize` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:95`).
- Resource factories return `nullptr` on creation/initialization failure, as in `OpenGLDevice::CreateUniformBuffer` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:219`).
- OpenGL operations capture errors into `m_lastError` using helper functions in `OpenGLDevice.cpp` (`Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:53`).
- Command execution logs unresolved resource IDs but continues processing other commands (`Engine/Graphics/source/Renderer/RenderSystem.cpp:252`).
- App code should call `quit()` after critical initialization failures, as shown in `Examples/BasicGame/source/BasicGame.cpp:88`.

## Cross-Cutting Concerns

**Logging:** Use `PYRAMID_LOG_TRACE`, `PYRAMID_LOG_DEBUG`, `PYRAMID_LOG_INFO`, `PYRAMID_LOG_WARN`, `PYRAMID_LOG_ERROR`, and `PYRAMID_LOG_CRITICAL` from `Engine/Utils/include/Pyramid/Util/Log.hpp`. Configure the singleton with `PYRAMID_CONFIGURE_LOGGER` when needed.

**Validation:** Use boolean initialization checks, null pointer checks, `IsInitialized`, `IsValid`, and `PYRAMID_ASSERT`/`PYRAMID_CORE_ASSERT` for invariants (`Engine/Core/include/Pyramid/Core/Game.hpp:51`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp:223`, `Engine/Utils/include/Pyramid/Util/Log.hpp:251`).

**Authentication:** Not applicable. This is a local C++ engine repository with no detected authentication subsystem.

**External assets/shaders:** Graphics shaders live under `Engine/Graphics/shaders` and are resolved by `ShaderPathResolver` (`Engine/Graphics/include/Pyramid/Graphics/Renderer/ShaderPathResolver.hpp`, `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp`). Inline sample shaders are used in `Examples/BasicGame/source/BasicGame.cpp`.

---

*Architecture analysis: Thu May 14 2026*
