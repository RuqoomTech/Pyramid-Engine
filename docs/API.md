# API overview

Public headers under `Engine/*/include` are the signature-level source of truth. `PYRAMID_VERSION_STRING` is exported as a target compile definition and currently evaluates to `0.6.0-pre-alpha`.

## Application

Header: `Pyramid/Core/Game.hpp`

```cpp
class Game
{
public:
    explicit Game(GraphicsAPI api = GraphicsAPI::OpenGL);
    void run();
    void quit();
    bool IsInitialized() const;

protected:
    virtual void onCreate();
    virtual void onUpdate(float deltaTime);
    virtual void onRender();
    virtual void onWindowResize(const WindowResizeEvent& event);
    IGraphicsDevice* GetGraphicsDevice() const;
    void SetActiveCamera(Camera* camera);
    Camera* GetActiveCamera() const;
    void SetRenderSystem(Renderer::RenderSystem* renderSystem);
    Renderer::RenderSystem* GetRenderSystem() const;
    bool IsRenderSurfaceAvailable() const;
};
```

Only `GraphicsAPI::OpenGL` is supported. A derived `onCreate()` must call `Game::onCreate()` first.

## Window

Header: `Pyramid/Platform/Window.hpp`

The window interface requires initialization, presentation, context activation, message processing, close-state reporting, title/size/position/visibility mutation, and minimized/maximized queries. The checked-in implementation is `Win32OpenGLWindow`.

`Window::SetResizeCallback()` receives platform-neutral `WindowResizeEvent` values while `ProcessMessages()` dispatches native messages. Each event includes client width, client height, and a `Restored`, `Minimized`, or `Maximized` state. `WindowResizeEvent::HasRenderableArea()` is false for minimized or zero-sized windows. `Game` installs this callback, updates the default graphics viewport, synchronizes the registered active camera and render system, suspends rendering for non-renderable client areas, and then forwards the event to the overridable `onWindowResize()` hook.

```cpp
void MyGame::onWindowResize(const Pyramid::WindowResizeEvent& event)
{
    Game::onWindowResize(event);
    if (!event.HasRenderableArea())
        return;

    // Default viewport, the camera registered with SetActiveCamera(), and the
    // RenderSystem registered with SetRenderSystem() are already synchronized.
    // Resize standalone framebuffers here.
}
```

## Graphics device and resources

Primary headers:

- `Pyramid/Graphics/GraphicsDevice.hpp`
- `Pyramid/Graphics/Buffer/*.hpp`
- `Pyramid/Graphics/Shader/Shader.hpp`
- `Pyramid/Graphics/Texture.hpp`
- `Pyramid/Graphics/Geometry/Vertex.hpp`

Typical geometry setup:

```cpp
auto* device = GetGraphicsDevice();
auto vertices = device->CreateVertexBuffer();
auto indices = device->CreateIndexBuffer();
auto array = device->CreateVertexArray();

vertices->SetData(vertexData, vertexBytes);
indices->SetData(indexData, indexCount);

Pyramid::BufferLayout layout = {
    {Pyramid::ShaderDataType::Float3, "a_Position"},
    {Pyramid::ShaderDataType::Float4, "a_Color"}
};

array->AddVertexBuffer(vertices, layout);
array->SetIndexBuffer(indices);
```

### Textures

```cpp
Pyramid::TextureSpecification spec;
spec.Width = 512;
spec.Height = 512;
spec.Format = Pyramid::TextureFormat::RGBA8;
spec.GenerateMips = false;

auto texture = Pyramid::ITexture2D::Create(spec, pixels);
auto blank = Pyramid::ITexture2D::Create(512, 512);
auto target = Pyramid::ITexture2D::CreateRenderTarget(1280, 720);
auto white = Pyramid::ITexture2D::CreateFromColor(1, 1, Pyramid::Color::White);
```

The reliable texture path is RGB8/RGBA8, including file-backed JPEG/PNG/TGA/BMP images. File reload is transactional: decode or OpenGL upload failure preserves the previous valid texture. `IsLoaded()` and `GetLastError()` expose state, RGB rows use safe unpack alignment, sRGB uploads select sRGB internal formats, and mipmapped filters are mapped completely. Advanced declared formats, anisotropy, and `FlipY` are not yet implemented. `CreateDepthTarget` returns `nullptr` with an error; use `OpenGLFramebuffer` for depth attachments.

## Renderer

Headers:

- `Pyramid/Graphics/Renderer/RenderSystem.hpp`
- `Pyramid/Graphics/Renderer/RenderPasses.hpp`

```cpp
Pyramid::Renderer::RenderSystem renderer;
if (!renderer.Initialize(device))
    return;

// Inside a Pyramid::Game-derived class. The pointer is non-owning.
SetRenderSystem(&renderer);

renderer.BeginFrame();
renderer.Render(scene, camera);
renderer.EndFrame();
```

Implemented public pass classes are `ForwardRenderPass`, `ShadowMapPass`, `DeferredGeometryPass`, and `DeferredLightingPass`. `RenderSystem::Resize()` propagates valid dimensions through managed render targets and window-sized passes. Individual `RenderTarget` and `OpenGLFramebuffer` objects also expose `Resize()`. Resize operations reject zero-sized extents; framebuffer recreation is transactional, so a failed replacement preserves the previous valid attachments. Compute dispatch is not operational.

## Camera

Header: `Pyramid/Graphics/Camera.hpp`

```cpp
Pyramid::Camera camera(
    Pyramid::Math::Radians(60.0f),
    1280.0f / 720.0f,
    0.1f,
    200.0f);

camera.SetPosition({0.0f, 2.5f, 6.0f});
camera.LookAt(Pyramid::Math::Vec3::Zero);

// Inside a Pyramid::Game-derived class:
SetActiveCamera(&camera);
```

`Camera::SetViewportSize(width, height)` updates perspective aspect ratio or preserves an orthographic camera's vertical span while adjusting its horizontal span. It rejects zero-sized surfaces. `Game::SetActiveCamera()` stores a non-owning pointer and applies this update automatically for the current window and later resize events.

The camera uses OpenGL's local negative-Z forward convention. `GetFrustumPlanes()` returns six normalized inward-facing world-space planes. `IsPointVisible()`, `IsSphereVisible()`, and `IsAABBVisible()` classify full near/far/side-plane intersections rather than using distance-only approximations.

```cpp
const auto& planes = camera.GetFrustumPlanes();
bool visible = camera.IsAABBVisible(worldBoundsMin, worldBoundsMax);
```

## Scene

Headers:

- `Pyramid/Graphics/Scene.hpp`
- `Pyramid/Graphics/Scene/SceneManager.hpp`
- `Pyramid/Graphics/Scene/Octree.hpp`

```cpp
auto scene = std::make_shared<Pyramid::Scene>("Main");
auto parent = scene->CreateNode("Vehicle");
auto child = scene->CreateNode("Wheel");

parent->SetLocalTransform(position, rotation, scale);
parent->AddChild(child); // The child's local transform is preserved.
child->SetLocalPosition({1.0f, -0.5f, 0.0f});

auto world = child->GetWorldTransform();
auto worldPosition = child->GetWorldPosition();
auto pointInWorld = child->TransformPointToWorld(localPoint);
```

`SceneNode` caches local and world matrices. Any local transform or hierarchy change invalidates the complete descendant subtree, so previously queried child matrices cannot remain stale. Reparenting and detaching preserve local TRS, duplicate direct children are ignored, and operations that would create a hierarchy cycle are rejected. Hierarchy nodes must be owned by `std::shared_ptr`; unmanaged nodes reject parent/child mutation safely. Local rotations are normalized. `GetWorldScale()` reports positive effective basis magnitudes; a hierarchy containing rotated non-uniform scale can produce shear, so no unique signed TRS decomposition exists for that matrix.

```cpp
auto manager = Pyramid::SceneManagement::SceneUtils::CreateSceneManager();
auto managedScene = manager->CreateScene("Managed");
manager->SetActiveScene(managedScene);
manager->RebuildSpatialPartition();

auto nearby = manager->GetObjectsInRadius(position, 10.0f);
auto boxed = manager->GetObjectsInBox(minBounds, maxBounds);
auto nearest = manager->GetNearestObject(position);
auto nearestFive = manager->GetKNearestObjects(position, 5);
```

`RenderObject` exposes local bounds with a unit-cube default. `GetWorldBounds()` transforms all eight corners through the object's translation, normalized rotation, and scale. `Scene`, `SceneManager`, and octree frustum queries use these AABBs; objects that span octree child boundaries remain at the parent node to avoid false rejection. Imported meshes do not yet populate local bounds automatically.

`Octree::Synchronize()` accepts the active scene's current render-object snapshot and incrementally inserts additions, removes stale entries, and relocates only objects whose world AABBs changed. `UpdateIfMoved()` performs the same bounds comparison for one object. `SceneManager::Update()` includes this synchronization when `UpdateFlags::SpatialPartition` is set; the default `UpdateFlags::All` therefore keeps moving objects current each frame without a full rebuild. `SceneStats` reports the most recent inserted, removed, moved, and unchanged counts.

`Octree::QueryPoint()`, `QuerySphere()`, `QueryBox()`, and `QueryRay()` test complete world-space AABBs and return unique objects. Ray hits are ordered nearest-first. `SceneManager::QueryScene()` uses the same semantics with or without spatial partitioning; ray results populate `QueryResult::distances` in object order. Spatial queries include hidden objects because they are gameplay/selection queries rather than rendering visibility filters. Negative sphere radii, zero-length ray directions, and negative ray distances return no hits.

`AABB::DistanceSquaredToPoint()` and `DistanceToPoint()` measure the shortest distance to a box, returning zero for points inside it. `Octree::FindNearest()` and `FindKNearest()` use that world-bound distance, not the object's origin. K-nearest results are ordered nearest-first, `k == 0` returns an empty result, and counts larger than the scene return all objects. Octree traversal orders child nodes by their minimum possible point distance and prunes branches that cannot improve the current candidate set. `SceneManager::GetNearestObject()` and `GetKNearestObjects()` provide identical semantics in octree and linear modes.

Event callbacks, scene and octree query entry points, visibility-stat updates, test-scene creation, and octree configuration have definitions and are covered by linkage validation.

`LoadScene` and `SaveScene` deliberately return `false`; serialization is not implemented. Scene-node attachment does not yet replace the renderer's separate `RenderObject` transform path. Occlusion culling remains unimplemented and disabled by default.

## Math

Umbrella header: `Pyramid/Math/Math.hpp`

```cpp
using namespace Pyramid::Math;

Vec3 position(1.0f, 2.0f, 3.0f);
Mat4 model = Mat4::CreateTranslation(position)
           * Mat4::CreateRotationY(Radians(30.0f));
```

Concrete headers define exact conventions and available operations for vectors, matrices, quaternions, geometry, interpolation, and SIMD helpers.

## Images

Header: `Pyramid/Util/Image.hpp`

```cpp
auto image = Pyramid::Util::Image::Load("assets/texture.png");
if (image.Data)
{
    // Consume image.Width, image.Height, image.Channels, and image.Data.
    Pyramid::Util::Image::Free(image.Data);
}
```

PNG uses the engine's custom non-interlaced decoder. JPEG uses libjpeg-turbo and is tested with baseline RGB and progressive grayscale fixtures; output is normalized to tightly packed 8-bit RGB. `ImageData::Data` remains manually owned and must be released with `Image::Free()`.

## Logging

Header: `Pyramid/Util/Log.hpp`

```cpp
Pyramid::Util::LoggerConfig config;
config.consoleLevel = Pyramid::Util::LogLevel::Info;
config.fileLevel = Pyramid::Util::LogLevel::Debug;
config.logFilePath = "pyramid_game.log";
Pyramid::Util::Logger::GetInstance().Configure(config);

PYRAMID_LOG_INFO("Loaded ", objectCount, " objects");
PYRAMID_ASSERT(resource != nullptr, "Resource must exist");
```
