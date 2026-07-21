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
    bool IsRenderSurfaceAvailable() const;
};
```

Only `GraphicsAPI::OpenGL` is supported. A derived `onCreate()` must call `Game::onCreate()` first.

## Window

Header: `Pyramid/Platform/Window.hpp`

The window interface requires initialization, presentation, context activation, message processing, close-state reporting, title/size/position/visibility mutation, and minimized/maximized queries. The checked-in implementation is `Win32OpenGLWindow`.

`Window::SetResizeCallback()` receives platform-neutral `WindowResizeEvent` values while `ProcessMessages()` dispatches native messages. Each event includes client width, client height, and a `Restored`, `Minimized`, or `Maximized` state. `WindowResizeEvent::HasRenderableArea()` is false for minimized or zero-sized windows. `Game` installs this callback, updates the default graphics viewport, synchronizes the registered active camera, suspends rendering for non-renderable client areas, and then forwards the event to the overridable `onWindowResize()` hook.

```cpp
void MyGame::onWindowResize(const Pyramid::WindowResizeEvent& event)
{
    Game::onWindowResize(event);
    if (!event.HasRenderableArea())
        return;

    // Default viewport and the camera registered with SetActiveCamera()
    // are already synchronized. Resize custom framebuffers here.
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

The basic RGB8/RGBA8 color path is the reliable texture path. Advanced declared formats and specification fields are not all mapped. `CreateDepthTarget` currently returns `nullptr` with an error; use `OpenGLFramebuffer` for depth attachments.

## Renderer

Headers:

- `Pyramid/Graphics/Renderer/RenderSystem.hpp`
- `Pyramid/Graphics/Renderer/RenderPasses.hpp`

```cpp
Pyramid::Renderer::RenderSystem renderer;
if (!renderer.Initialize(device))
    return;

renderer.BeginFrame();
renderer.Render(scene, camera);
renderer.EndFrame();
```

Implemented public pass classes are `ForwardRenderPass`, `ShadowMapPass`, `DeferredGeometryPass`, and `DeferredLightingPass`. Compute dispatch is not operational.

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

The camera also provides quaternion/Euler rotation, local movement, view/projection matrices, coordinate conversion, and point/sphere visibility helpers.

## Scene

Headers:

- `Pyramid/Graphics/Scene.hpp`
- `Pyramid/Graphics/Scene/SceneManager.hpp`
- `Pyramid/Graphics/Scene/Octree.hpp`

```cpp
auto manager = Pyramid::SceneManagement::SceneUtils::CreateSceneManager();
auto scene = manager->CreateScene("Main");
manager->SetActiveScene(scene);
manager->RebuildSpatialPartition();

auto nearby = manager->GetObjectsInRadius(position, 10.0f);
auto boxed = manager->GetObjectsInBox(minBounds, maxBounds);
auto nearest = manager->GetNearestObject(position);
```

Event callbacks, box queries, visibility-stat updates, test-scene creation, and octree configuration have definitions and are covered by linkage validation.

`LoadScene` and `SaveScene` deliberately return `false`; serialization is not implemented. Frustum and occlusion algorithms are not production-ready.

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

PNG is the most thoroughly tested real-image path. JPEG output is still a generated pattern after marker parsing and must not be used as decoded texture content.

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
