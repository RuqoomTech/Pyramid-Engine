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
- `Pyramid/Graphics/Shader/ShaderProgram.hpp`
- `Pyramid/Graphics/Shader/ShaderCache.hpp`
- `Pyramid/Graphics/Texture.hpp`
- `Pyramid/Graphics/Texture/TextureResource.hpp`
- `Pyramid/Graphics/Texture/TextureCache.hpp`
- `Pyramid/Graphics/Geometry/Vertex.hpp`
- `Pyramid/Graphics/Geometry/Mesh.hpp`
- `Pyramid/Graphics/Geometry/MeshCache.hpp`
- `Pyramid/Graphics/Material/Material.hpp`
- `Pyramid/Graphics/Material/MaterialCache.hpp`

### Shader programs

```cpp
Pyramid::ShaderProgramSpecification shaderSpec;
shaderSpec.vertexSource = vertexSource;
shaderSpec.fragmentSource = fragmentSource;
shaderSpec.name = "Player Forward";
shaderSpec.assetId =
    Pyramid::ShaderAssetId::FromString("shaders/player-forward");

Pyramid::ShaderCache shaderCache(*device);
auto shader = shaderCache.GetOrCreate(shaderSpec);
```

`ShaderProgram` is an immutable compiled resource that implements `IShader`, so it can be assigned directly to materials and command buffers. Exact stage-source sets receive a deterministic content identifier and compile only once even when requested through several caller-defined stable aliases. Graphics specifications require vertex and fragment stages; tessellation control/evaluation stages must be paired; compute programs cannot mix with graphics stages. Debug names are excluded from content identity.

Changing a stable asset uses transactional replacement:

```cpp
Pyramid::ShaderProgramSpecification replacement = shaderSpec;
replacement.fragmentSource = updatedFragmentSource;
replacement.assetId = {};

if (shaderCache.Recompile(shaderSpec.assetId, replacement))
    shader = shaderCache.Find(shaderSpec.assetId);
```

`Recompile()` compiles or resolves the replacement before changing the stable alias. Failure leaves the previous cached program active. Existing external owners of the old program remain valid and must reacquire the stable alias when they want the replacement. Content-derived identifiers are immutable and cannot be recompiled. `Evict()`, `CollectUnused()`, `Clear()`, and `GetStats()` provide explicit lifetime and diagnostics controls. Destroy the cache before its graphics device/context and use it from the graphics thread.

### Geometry

Typical geometry setup:

```cpp
auto* device = GetGraphicsDevice();

Pyramid::MeshSpecification meshSpec;
meshSpec.vertexData = vertexData;
meshSpec.vertexDataSize = vertexBytes;
meshSpec.vertexCount = vertexCount;
meshSpec.layout = {
    {Pyramid::ShaderDataType::Float3, "a_Position"},
    {Pyramid::ShaderDataType::Float4, "a_Color"}
};
meshSpec.indexData = indexData;
meshSpec.indexCount = indexCount;
meshSpec.topology = Pyramid::PrimitiveTopology::Triangles;
meshSpec.name = "PlayerMesh";
meshSpec.assetId = Pyramid::MeshAssetId::FromString("meshes/player");

Pyramid::MeshCache meshCache(*device);
auto mesh = meshCache.GetOrCreate(meshSpec);
renderObject->mesh = mesh;
```

`Mesh` owns the created vertex array, vertex buffer, and optional index buffer. Its validated layout, vertex/index counts, primitive topology, identifiers, and local bounds are immutable after creation. Indexed and non-indexed meshes are supported for points, lines, line strips, triangles, and triangle strips. Creation rejects mismatched byte counts, missing/invalid position semantics, non-finite positions, incompatible topology counts, and out-of-range indices.

`MeshAssetId::FromString()` creates a deterministic 128-bit identifier from a stable caller-owned name such as an asset path. If `MeshSpecification::assetId` is left invalid, `Mesh::CalculateContentId()` derives the asset identifier from the exact geometry bytes and immutable draw metadata. Debug names are excluded, so renaming a mesh does not create another GPU upload.

`MeshCache` is bound to one graphics device and owns one strong reference per unique content fingerprint. Requests through different stable IDs share the same resident mesh when their geometry is identical. Reusing a resident explicit ID for different geometry fails rather than silently returning the wrong resource. `Find()` resolves either a stable alias or the content ID. `Evict()` removes the mesh and all aliases from the cache while existing external `shared_ptr` owners remain valid; `CollectUnused()` removes resources owned only by the cache, and `Clear()` removes all cache ownership. `GetStats()` reports residency, bytes, hits, misses, conflicts, failures, creations, and evictions. Destroy the cache before its graphics device/context and call it from the graphics thread. Direct `Mesh::Create()` remains available for intentionally uncached one-off geometry.

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

For shared sampled textures, prefer the immutable cache path:

```cpp
Pyramid::TextureCache textureCache(*device);

Pyramid::TextureFileSpecification fileSpec;
fileSpec.filepath = "Assets/albedo.png";
fileSpec.colorSpace = Pyramid::TextureColorSpace::SRGB;
fileSpec.assetId = Pyramid::TextureAssetId::FromString("textures/player/albedo");

auto albedo = textureCache.GetOrCreate(fileSpec);
```

`TextureCache` fingerprints exact decoded pixels plus dimensions, format, mip policy, sampler state, border color, anisotropy request, and explicit linear/sRGB intent. Identical memory or file requests share one GPU upload across aliases. Reusing one stable ID for different resident content fails. `Reload()` is transactional for caller-defined file aliases: a replacement is decoded and uploaded before the alias changes; failure preserves the previous resource. `Evict()`, `CollectUnused()`, `Clear()`, and `GetStats()` provide explicit residency control. Cached `TextureResource` objects are immutable; reacquire the stable alias after a successful reload. Destroy the cache before the graphics device/context.

The direct `ITexture2D` path remains available for intentionally uncached or mutable textures. RGB8/RGBA8 files support JPEG/PNG/TGA/BMP. `IsLoaded()` and `GetLastError()` expose state, RGB rows use safe unpack alignment, sRGB uploads select sRGB internal formats, and mipmapped filters are mapped completely. Cached file specifications can flip decoded rows before upload; direct `TextureSpecification::FlipY` and anisotropic filtering are not yet applied consistently by every path. `CreateDepthTarget` returns `nullptr`; use `OpenGLFramebuffer` for depth attachments.

### Materials

```cpp
Pyramid::MaterialSpecification materialSpec;
materialSpec.shader = shader;
materialSpec.textures = {
    {"u_AlbedoMap", 0, albedo}
};
materialSpec.uniforms = {
    {"u_AlbedoColor", Pyramid::Math::Vec4(1.0f)},
    {"u_Metallic", 0.0f},
    {"u_Roughness", 0.5f}
};
materialSpec.renderState.blendMode = Pyramid::MaterialBlendMode::Opaque;
materialSpec.renderState.depthTest = true;
materialSpec.renderState.cullMode = Pyramid::MaterialCullMode::Back;
materialSpec.assetId =
    Pyramid::MaterialAssetId::FromString("materials/player");

Pyramid::MaterialCache materialCache;
auto material = materialCache.GetOrCreate(materialSpec);
renderObject->material = material;
```

`Material` is immutable and owns exact references to one graphics `ShaderProgram` and zero or more `TextureResource` objects. Its content identity includes shader and texture content IDs, sampler uniform names and slots, typed uniforms, and blend/depth/cull/polygon state while excluding the debug name. Creation rejects compute shaders, duplicate slots or names, unloaded textures, and non-finite values. `CommandBuffer::SetMaterial()` applies the material in draw order; dynamic object/camera matrices are recorded separately with typed `SetUniform*()` commands so they do not alter material identity.

`MaterialCache` keeps one strong resident reference per exact material content fingerprint. Different stable IDs share one `Material` when shader, textures, uniforms, and fixed state are identical. Reusing one stable ID for different resident content is rejected. `Find()`, `Evict()`, `CollectUnused()`, `Clear()`, and `GetStats()` provide explicit lookup, lifetime, and diagnostics control. Destroy the material cache before the shader and texture caches that own its referenced resources.

Stable aliases can be replaced transactionally:

```cpp
Pyramid::MaterialSpecification replacement = materialSpec;
replacement.uniforms = {
    {"u_AlbedoColor", Pyramid::Math::Vec4(0.8f, 0.9f, 1.0f, 1.0f)},
    {"u_Metallic", 0.2f},
    {"u_Roughness", 0.35f}
};
replacement.assetId = {};

if (materialCache.Replace(materialSpec.assetId, replacement))
    material = materialCache.Find(materialSpec.assetId);
```

`Replace()` validates or resolves the complete replacement before changing the stable alias. Failure preserves the previously active material, while existing external owners of older material versions remain valid. Content-derived identifiers are immutable and cannot be replaced.

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

`RenderObject` holds a shared `Mesh` resource and uses its immutable local AABB in `RenderBoundsMode::Automatic`. `SetLocalBounds()` switches to manual mode; `UseAutomaticBounds()` restores mesh-derived behavior. Missing geometry falls back to the unit cube. `GetWorldBounds()` transforms all eight corners through translation, normalized rotation, and scale. `Scene`, `SceneManager`, and octree queries use these AABBs; objects spanning octree child boundaries remain at the parent node to avoid false rejection.

`Octree::Synchronize()` accepts the active scene's current render-object snapshot and incrementally inserts additions, removes stale entries, and relocates only objects whose world AABBs changed. `UpdateIfMoved()` performs the same bounds comparison for one object. `SceneManager::Update()` includes this synchronization when `UpdateFlags::SpatialPartition` is set; the default `UpdateFlags::All` therefore keeps moving objects current each frame without a full rebuild. `SceneStats` reports the most recent inserted, removed, moved, and unchanged counts.

`Octree::QueryPoint()`, `QuerySphere()`, `QueryBox()`, and `QueryRay()` test complete world-space AABBs and return unique objects. Ray hits are ordered nearest-first. `SceneManager::QueryScene()` uses the same semantics with or without spatial partitioning; ray results populate `QueryResult::distances` in object order. Spatial queries include hidden objects because they are gameplay/selection queries rather than rendering visibility filters. Negative sphere radii, zero-length ray directions, and negative ray distances return no hits.

`AABB::DistanceSquaredToPoint()` and `DistanceToPoint()` measure the shortest distance to a box, returning zero for points inside it. `Octree::FindNearest()` and `FindKNearest()` use that world-bound distance, not the object's origin. K-nearest results are ordered nearest-first, `k == 0` returns an empty result, and counts larger than the scene return all objects. Octree traversal orders child nodes by their minimum possible point distance and prunes branches that cannot improve the current candidate set. `SceneManager::GetNearestObject()` and `GetKNearestObjects()` provide identical semantics in octree and linear modes.

`OctreeConfiguration` groups root center/size, maximum depth, and node capacity. `Octree::Configure()` validates the complete request and transactionally constructs a replacement tree before changing live state; every tracked object, including root-retained objects outside the new bounds, is reinserted into the replacement. Invalid centers or extents leave the existing tree untouched. `SetBounds()`, `SetMaxDepth()`, and `SetMaxObjectsPerNode()` delegate to the same path, while `GetConfiguration()` returns the active normalized values. A zero capacity is normalized to one; zero maximum depth is valid and produces a root-only tree.

`Octree::Compact()` performs bottom-up structural cleanup. A node releases its children when they contain no objects or when the complete subtree fits within that node's configured capacity; descendant objects are promoted before child ownership is released. Removal, relocation, and batched synchronization invoke this automatically, while `GetLastCompactionStats()` and `OctreeSyncStats::compaction` report collapsed nodes and promoted objects. `GetStats()` reports occupied/configured depth, internal and leaf counts, empty and occupied leaves, tracked versus physically stored objects, internal-node occupancy, maximum node occupancy, average leaf depth and occupancy, leaf utilization, empty-leaf ratio, and approximate memory use.

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
