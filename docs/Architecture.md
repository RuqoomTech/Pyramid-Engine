# Architecture

## Scope

Pyramid currently builds one `PyramidEngine` library and two example executables. It is a layered native engine prototype, not yet a complete cross-platform runtime.

```text
Application / Examples
        |
        v
Pyramid::Game ---------------------- main loop and lifecycle
        |
        +--> Win32OpenGLWindow ----- Win32 messages and WGL context
        |
        +--> IGraphicsDevice ------- OpenGL resource/state abstraction
                 |
                 +--> RenderSystem - command buffers and render passes
                 +--> Scene/Camera -- visible objects, lights, transforms
                 +--> Buffers, shaders, textures, framebuffers

Shared services: Pyramid::Math and Pyramid::Util
```

## Modules

| Module | Namespace | State |
|---|---|---|
| Core | `Pyramid` | Implemented: common types, `Color`, `GraphicsAPI`, and `Game` |
| Platform | `Pyramid` | Implemented only for Win32/WGL |
| Graphics device/resources | `Pyramid` | OpenGL implementation present; other API enum values unsupported |
| Renderer | `Pyramid::Renderer` | Forward/shadow pipeline used by `BasicGame`; deferred components present but less exercised |
| Scene | `Pyramid` | Scene nodes, render objects, lights, environment, camera-visible object collection |
| Scene management | `Pyramid::SceneManagement` | Octree queries implemented; several declared management features incomplete |
| Math | `Pyramid::Math` | Vectors, matrices, quaternions, helpers, and SIMD routines |
| Utilities | `Pyramid::Util` | Logging and in-tree image decoders |
| Audio/Input/Physics | — | Placeholder CMake modules; no public engine implementation |

## Startup and frame lifecycle

1. `Game` creates `Win32OpenGLWindow` for `GraphicsAPI::OpenGL`.
2. The window bootstraps WGL with a temporary context, then attempts core contexts from OpenGL 4.6 down to 3.3. If that path is unavailable, the temporary legacy context can remain active.
3. `IGraphicsDevice::Create` constructs `OpenGLDevice`.
4. `Game::run()` invokes virtual `onCreate()`.
5. The base `Game::onCreate()` initializes the graphics device and enables the loop.
6. Each frame processes Win32 messages, computes a clamped delta time, then calls `onUpdate(deltaTime)` and `onRender()`.
7. Presentation occurs through `IGraphicsDevice::Present()` or `RenderSystem::EndFrame()`.

A derived `onCreate()` must call `Game::onCreate()` first. A derived `onRender()` replaces the base implementation and therefore owns clearing/presentation unless it delegates to `RenderSystem`.

## Rendering architecture

### Device layer

`IGraphicsDevice` exposes creation and binding for vertex/index/instance/uniform/storage buffers, vertex arrays, shaders, textures, viewports, draw calls, and common raster state. The only factory implementation is `OpenGLDevice`.

`OpenGLStateManager` caches OpenGL state to reduce redundant binds. Resource classes use RAII-style ownership, although several interfaces also expose optional no-op defaults that should be tightened as the API stabilizes.

### Command buffers

`Pyramid::Renderer::CommandBuffer` records resource bindings, clears, indexed/instanced draws, and dispatch requests. ID-based commands require registration; pointer-based commands bind resources directly.

Compute `Dispatch` is currently diagnostic only and does not call `glDispatchCompute`.

### Render passes

The default pipeline created by `RenderSystem::Initialize()` is:

1. `ShadowMapPass`
2. `ForwardRenderPass`

`SetupDeferredPipeline()` replaces it with shadow, deferred geometry, and deferred lighting passes. The deferred pipeline currently uses a fixed 1920×1080 setup and still has incomplete shadow-map-array binding. `TransparentPass`, `PostProcessPass`, `UIRenderPass`, `DebugRenderPass`, and `RenderPassFactory` are declared in public headers but have no definitions in this snapshot.

`BeginFrame()` starts command recording, `Render()` executes enabled passes in enum order, and `EndFrame()` executes trailing commands and presents.

### Scene data

`Scene` owns nodes, render objects, lights, environment data, and a primary light. `Camera` maintains view/projection matrices and visibility helpers.

`SceneManager` can manage an active scene and an octree, and supports point, sphere, ray, nearest-object, and frustum-oriented query paths. Current limitations include:

- missing definitions for declared load/save and serialization functions;
- missing box-query, visibility-update, debug-draw, and test-scene definitions;
- placeholder transform update and occlusion-culling behavior;
- statistics fields that remain zero because source counts are not connected.

Treat `SceneManager` as experimental until these gaps are closed and covered by tests.

## Math and SIMD

The math module provides `Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, `Quat`, common constants/helpers, and SIMD support. It is integrated into camera, scene, and rendering types. Performance claims should be based on reproducible benchmarks; the repository currently contains implementation helpers but no maintained benchmark suite.

## Image loading

`Pyramid::Util::Image::Load` dispatches by file extension:

- TGA: uncompressed true-color, 24/32-bit
- BMP: uncompressed `BITMAPINFOHEADER`, 24/32-bit
- PNG: in-tree chunk/filter/zlib/DEFLATE path
- JPEG: in-tree baseline decoder path

The caller owns `ImageData::Data` and must release it with `Image::Free`. PNG rejects interlaced images and the final conversion path currently accepts only 8-bit samples. JPEG marker parsing and helper stages exist, but the loader still produces a generated test pattern instead of decoded image blocks; JPEG files must therefore be treated as unsupported for real texture content.

## Logging

`Pyramid::Util::Logger` is a process-wide singleton with independent console/file thresholds, optional source/thread/timestamp fields, file rotation, structured fields, stream-style macros, and assertion integration. The singleton is mutex-protected; logging remains synchronous.

## Dependencies and build boundaries

- GLAD is vendored and built as a separate target.
- OpenGL is linked through Windows `opengl32`.
- No external window, math, image, physics, or audio library is linked.
- `CMake/Dependencies.cmake` is stale and not included by the active build.
- `Game/CMakeLists.txt` is legacy and not added by the root build.

## Ownership conventions

- Engine roots and major services generally use `std::unique_ptr`.
- Shareable GPU resources, scenes, nodes, render objects, and passes generally use `std::shared_ptr`.
- Raw pointers in command buffers and device interfaces are non-owning.
- Image pixel memory is manually owned and must be freed explicitly.

Avoid extending raw-pointer lifetime assumptions. Prefer handles or clearly documented non-owning views as the resource model matures.
