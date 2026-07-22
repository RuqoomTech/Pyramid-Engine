# Architecture

## Scope

Pyramid Engine is a monolithic C++17 library with a Win32/WGL platform implementation and an OpenGL backend. CMake exports the installed library as `Pyramid::Engine`.

## Layers

| Layer | Namespace | Responsibility |
|---|---|---|
| Core | `Pyramid` | Application lifecycle, common types, and graphics API selection |
| Platform | `Pyramid` | Window, message pump, and OpenGL context |
| Graphics resources | `Pyramid` | Device, buffers, arrays, shaders, textures, framebuffers, and camera |
| Renderer | `Pyramid::Renderer` | Command recording, materials, render targets, and render passes |
| Scene | `Pyramid` | Scene graph, render objects, lights, and environment |
| Spatial management | `Pyramid::SceneManagement` | Scene manager, octree, AABB, and query helpers |
| Math | `Pyramid::Math` | Vectors, matrices, quaternions, geometry, and SIMD helpers |
| Utilities | `Pyramid::Util` | Logging, image loading, bit reading, DEFLATE, zlib, and libjpeg integration |

Input, audio, physics, editor, scripting, and asset-pipeline systems are not currently present.

## Application lifecycle

1. `Game` creates `Win32OpenGLWindow` for `GraphicsAPI::OpenGL`.
2. The window creates a temporary WGL context to load extensions.
3. It attempts core contexts from OpenGL 4.6 down to 3.3.
4. Context creation fails if no OpenGL 3.3-or-newer core context is available.
5. `IGraphicsDevice::Create` creates `OpenGLDevice`.
6. `Game` attaches a platform-neutral resize callback after window/device construction.
7. `Game::run()` calls `onCreate()`, processes messages, computes clamped delta time, updates, renders, and shuts down.

Derived `onCreate()` implementations must call `Game::onCreate()` before creating graphics resources.

## Window contract

`Window` is a strict interface. Initialization, presentation, context activation, close state, title, size, position, visibility, and minimized/maximized queries are all required operations.

The base interface owns a replaceable resize callback and emits platform-neutral `WindowResizeEvent` values. `Win32OpenGLWindow` maps `WM_SIZE` to restored, minimized, or maximized states, updates its cached client dimensions before delivery, and suppresses duplicate events. During message processing, `Game` updates the default viewport, active camera, and registered render system before forwarding delivery to `onWindowResize()` on the game thread. Minimized and zero-sized events suspend rendering without recreating GPU targets.

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

Known constraints:

- compute `Dispatch` commands are logged but not executed;
- generic framebuffer binding outside the OpenGL renderer remains incomplete;
- shadow-map resolution is intentionally independent of window size;
- render statistics do not yet represent complete GPU execution metrics.

## Scene and spatial management

`Scene` owns render objects, lights, environment settings, and a root hierarchy node. `SceneNode` stores local TRS and lazily cached local/world matrices. Nodes participating in a hierarchy require `std::shared_ptr` ownership, matching the `enable_shared_from_this` parent-link model; unmanaged nodes reject hierarchy mutation. Local edits, reparenting, detachment, and parent destruction recursively invalidate descendant world caches. Parent-before-local matrix composition is used throughout; hierarchy cycles and duplicate direct children are rejected. Reparenting preserves local TRS rather than preserving world space.

World rotation is the normalized quaternion chain. World scale is reported as the positive length of each transformed basis vector; rotated non-uniform parent scale can introduce shear and therefore has no unique signed TRS decomposition.

`SceneManager` owns named scenes, selects the active scene, manages an octree, and exposes point, sphere, box, ray, nearest-object, K-nearest, and visibility queries. Spatial and nearest-neighbor queries test full world-space AABBs and produce the same results in octree and linear modes; ray and K-nearest hits are nearest-first. Public scene-manager methods link consistently. Unsupported persistence operations return `false` and log an error instead of producing linker failures.

The camera extracts six normalized inward-facing frustum planes from its view-projection matrix. Camera point, sphere, and AABB tests share those planes. `RenderObject` transforms explicit local bounds into a world-space AABB, and both linear scene visibility and octree queries use that same bound. Objects spanning octree child boundaries remain in the parent node, allowing child-node rejection without hiding intersecting geometry.

The octree supports insertion, removal, incremental synchronization, explicit rebuilding, configuration, bounds-aware nearest-neighbor queries, and public spatial helpers. Nearest traversal visits children by the minimum point-to-node distance and prunes branches once their lower bound exceeds the farthest retained candidate. It tracks the last world AABB for each object. During the normal `SceneManager::Update()` path, new and removed scene objects are reconciled and only objects with changed bounds are removed and reinserted; stable objects do not mutate the tree. A full rebuild remains reserved for active-scene or octree-configuration changes. Local bounds default to a unit cube and must currently be assigned by asset code; imported meshes do not yet derive them automatically. Occlusion culling remains a placeholder and is disabled by default.

## Textures and framebuffers

The basic specification and file constructors create `OpenGLTexture2D` instances. Size-based, render-target, and solid-color convenience factories are defined for the basic color-texture path.

Depth formats are not mapped by `OpenGLTexture2D`; `CreateDepthTarget` therefore logs an error and returns `nullptr`. Depth attachments should be created through `OpenGLFramebuffer` until the texture-format mapping is completed.

`OpenGLFramebuffer` owns one framebuffer and its attachments through a single cleanup path. Resizing creates and validates a replacement first, then swaps it into service; failed replacement creation leaves the previous framebuffer intact. `Renderer::RenderTarget` delegates to this same implementation rather than maintaining a second raw OpenGL lifecycle. `RenderSystem::Resize()` propagates window dimensions to managed targets and passes, while fixed-resolution shadow maps remain unchanged.

## Image loading

`Image::Load` dispatches by extension:

- TGA: narrow uncompressed RGB/RGBA subset;
- BMP: narrow uncompressed 24/32-bit subset;
- PNG: custom non-interlaced path with DEFLATE/zlib handling;
- JPEG: baseline and progressive decoding through libjpeg-turbo, normalized to tightly packed RGB output.

`ImageData::Data` is manually owned and must be released through `Image::Free`.

## Logging

The logger supports severity filtering, console/file output, rotation, structured fields, source locations, assertions, and thread synchronization. Engine subsystems use `PYRAMID_LOG_*` rather than direct console output.

## Build and package model

- `PyramidEngine` is the engine target; `Pyramid::Engine` is its build-tree alias and installed name.
- GLAD is a public dependency because OpenGL implementation headers expose GLAD types.
- JPEG is a public link dependency for static-package consumers; the installed package resolves it through CMake `FindJPEG`.
- Public headers are installed separately rather than exported through `INTERFACE_SOURCES`, keeping the package relocatable.
- CMake package configuration and version files support `find_package(PyramidEngine CONFIG REQUIRED)`.
- Windows CI validates an external consumer after installation.

## Dependency direction

```text
Application
    ↓
Core Game lifecycle
    ↓
Platform Window + Graphics Device
    ↓
Renderer / Scene / Camera / Resources
    ↓
Math + Utilities + GLAD + Win32/OpenGL
```

Avoid introducing platform handles into generic interfaces or renderer-specific ownership into scene data without an explicit lifetime model.
