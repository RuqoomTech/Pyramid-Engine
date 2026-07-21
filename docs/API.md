# API overview

This is a compact map of the supported public surface. The public headers under `Engine/*/include` remain the signature-level source of truth.

## Namespaces

| Namespace | Primary responsibility |
|---|---|
| `Pyramid` | Core, platform, graphics resources, camera, and scene types |
| `Pyramid::Renderer` | Command buffers, materials, render targets, passes, and `RenderSystem` |
| `Pyramid::SceneManagement` | `SceneManager`, octree integration, and spatial query types |
| `Pyramid::Math` | Vectors, matrices, quaternions, geometry, and SIMD helpers |
| `Pyramid::Util` | Logging and image loading |

## Core application

Header: `Engine/Core/include/Pyramid/Core/Game.hpp`

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
    IGraphicsDevice* GetGraphicsDevice() const;
};
```

Only `GraphicsAPI::OpenGL` is implemented. Call the base `onCreate()` before using the device.

## Graphics device and resources

Primary headers:

- `Pyramid/Graphics/GraphicsDevice.hpp`
- `Pyramid/Graphics/Buffer/*.hpp`
- `Pyramid/Graphics/Shader/Shader.hpp`
- `Pyramid/Graphics/Texture.hpp`
- `Pyramid/Graphics/Geometry/Vertex.hpp`

`IGraphicsDevice` supports initialization/shutdown, clear/present, indexed and instanced draws, viewport/raster state, resource factories, resource binding, and device diagnostics.

Typical resource setup:

```cpp
auto* device = GetGraphicsDevice();
auto vertexBuffer = device->CreateVertexBuffer();
auto indexBuffer = device->CreateIndexBuffer();
auto vertexArray = device->CreateVertexArray();
auto shader = device->CreateShader();

vertexBuffer->SetData(vertices.data(),
                      static_cast<Pyramid::u32>(vertices.size() * sizeof(Pyramid::Vertex)));
indexBuffer->SetData(indices.data(),
                     static_cast<Pyramid::u32>(indices.size()));

Pyramid::BufferLayout layout = {
    { Pyramid::ShaderDataType::Float3, "a_Position" },
    { Pyramid::ShaderDataType::Float4, "a_Color" }
};

vertexArray->AddVertexBuffer(vertexBuffer, layout);
vertexArray->SetIndexBuffer(indexBuffer);
```

Use the checked-in examples for complete shader and geometry setup.

### Textures

`TextureSpecification` declares dimensions, format, filtering, wrapping, mip generation, sRGB intent, border color, anisotropy, and vertical flipping. The current OpenGL implementation fully maps only the basic RGB8/RGBA8 path; several other fields and formats are declared but ignored or fall back to defaults.

```cpp
Pyramid::TextureSpecification spec;
spec.Width = 512;
spec.Height = 512;
spec.Format = Pyramid::TextureFormat::RGBA8;
spec.GenerateMips = true;

auto texture = device->CreateTexture2D(spec, pixelData);
```

Several optional `ITexture` methods have default no-op implementations. `TextureSpecification::SRGB`, border color, anisotropy, and `FlipY` are not applied by the specification-based constructor, and convenience factories such as `CreateRenderTarget`, `CreateDepthTarget`, and `CreateFromColor` have no definitions. Use the device factory paths exercised by the examples and verify `OpenGLTexture2D` before relying on advanced texture behavior.

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

The default renderer installs shadow and forward passes. `SetupDeferredPipeline()` opts into the deferred path. `CommandBuffer::Dispatch()` is not an operational compute API yet.

## Camera

Header: `Pyramid/Graphics/Camera.hpp`

`Camera` supports perspective/orthographic projection, position and quaternion/Euler rotation, `LookAt`, local movement, view/projection accessors, screen/world conversion, and point/sphere visibility helpers.

```cpp
Pyramid::Camera camera(
    Pyramid::Math::Radians(60.0f),
    1280.0f / 720.0f,
    0.1f,
    200.0f);

camera.SetPosition({0.0f, 2.5f, 6.0f});
camera.LookAt(Pyramid::Math::Vec3::Zero);
```

## Scene and spatial queries

Headers:

- `Pyramid/Graphics/Scene.hpp`
- `Pyramid/Graphics/Scene/SceneManager.hpp`
- `Pyramid/Graphics/Scene/Octree.hpp`

`Scene` is the rendering data container. It exposes render-object, node, light, environment, and primary-light management.

`SceneManager` lives in `Pyramid::SceneManagement`, not directly in `Pyramid`:

```cpp
auto manager = Pyramid::SceneManagement::SceneUtils::CreateSceneManager();
manager->SetActiveScene(scene);
manager->RebuildSpatialPartition();

auto nearby = manager->GetObjectsInRadius(position, 10.0f);
auto visible = manager->GetVisibleObjects(camera);
```

Do not rely on scene serialization, box queries, visibility updates, debug drawing, or occlusion culling until their missing/placeholder implementations are completed.

## Math

Umbrella header: `Pyramid/Math/Math.hpp`

Core types are `Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, and `Quat`. The library includes arithmetic, products, normalization, transforms, projections, interpolation, geometry helpers, constants, and SIMD-oriented helpers.

```cpp
using namespace Pyramid::Math;

Vec3 position(1.0f, 2.0f, 3.0f);
Mat4 model = Mat4::CreateTranslation(position)
           * Mat4::CreateRotationY(Radians(30.0f));
```

Check the concrete headers for exact factory names and coordinate conventions when adding new rendering code.

## Images

Header: `Pyramid/Util/Image.hpp`

```cpp
auto image = Pyramid::Util::Image::Load("assets/texture.png");
if (image.Data)
{
    // Use image.Width, image.Height, and image.Channels.
    Pyramid::Util::Image::Free(image.Data);
}
```

Loading is extension-based. TGA/BMP support narrow uncompressed subsets; PNG is non-interlaced and effectively 8-bit at output; JPEG currently returns a generated test pattern rather than decoded image content. See [Architecture](ARCHITECTURE.md#image-loading).

## Logging

Header: `Pyramid/Util/Log.hpp`

```cpp
Pyramid::Util::LoggerConfig config;
config.consoleLevel = Pyramid::Util::LogLevel::Info;
config.fileLevel = Pyramid::Util::LogLevel::Debug;
config.logFilePath = "pyramid_game.log";
Pyramid::Util::Logger::GetInstance().Configure(config);

PYRAMID_LOG_INFO("Loaded ", objectCount, " objects");
PYRAMID_WARN_STREAM() << "Fallback path selected";
```

Assertions are active when `NDEBUG` is not defined:

```cpp
PYRAMID_ASSERT(resource != nullptr, "Resource must exist");
PYRAMID_CORE_ASSERT(device->IsValid(), "Graphics device is invalid");
```

## Platform window

Header: `Pyramid/Platform/Window.hpp`

`Window` defines initialization, presentation, context activation, message processing, native handle access, and dimensions. Only `Win32OpenGLWindow` exists. Optional mutator/query methods in the interface default to no-ops or `false` and are not overridden by the current implementation.
