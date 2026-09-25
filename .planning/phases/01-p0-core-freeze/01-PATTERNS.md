# Phase 1: P0 Core Freeze - Pattern Map

**Mapped:** 2026-09-11
**Files analyzed:** 18 (13 modify, 2 new tests, 2 docs, 1 new header)
**Analogs found:** 16 / 18

## File Classification

| New/Modified File | Role | Data Flow | Closest Analog | Match Quality |
|-------------------|------|-----------|----------------|---------------|
| `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` (enum `Dispatch` + union member + `Dispatch()` decl) | contract/model | record-execute | itself, lines 50-92, 145 | exact |
| `Engine/Graphics/source/Renderer/CommandBuffer.cpp` (`Dispatch` record + `Execute` drop) | service | record-execute | itself, lines 259-269, 560-567 | exact |
| `Engine/Graphics/include/Pyramid/Graphics/Shader/Shader.hpp` (`CompileCompute`/`DispatchCompute`) | interface | request-response | itself, lines 67-80 | exact |
| `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp` (compute impl) | backend service | request-response | itself, lines 558-573 | exact |
| `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp` (occlusion flag/setter/decl) | service interface | query (visibility) | itself, lines 162-177, 191-194 | exact |
| `Engine/Graphics/source/Scene/SceneManager.cpp` (`OcclusionCull` + filter block) | service | query (visibility) | itself, lines 277-311, 541-547 | exact |
| `Engine/Graphics/source/Renderer/ShadowMapPass.cpp` (per-cascade FBOs → array) | renderer service | batch (per-frame) | itself, lines 61-100 | exact |
| `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp` (shadow bind + uniform upload) | renderer service | batch (per-frame) | itself, lines 100-180 | exact |
| `Engine/Graphics/include/Pyramid/Graphics/Texture.hpp` (`TextureFormat` enum) | model | CRUD | itself (21-value enum, RESEARCH §FRZ-04) | exact |
| `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp` (`ResolveFormats` + upload) | backend resource | file-I/O/upload | itself, lines 14-78, 290-314, 384-429 | exact |
| `Engine/Graphics/source/Texture/TextureResource.cpp` (`ResolveBaseFormat` + `CalculateByteSize`) | service/cache | CRUD | itself, lines 98-133 | exact |
| `Engine/Graphics/source/Texture.cpp` (`CreateDepthTarget` factory) | factory/service | CRUD | itself, lines 12-45 | exact |
| `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` (`BindFramebuffer` decl) | interface | request-response | itself, lines 267-276 | exact |
| `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` (`BindFramebuffer` stub) | backend service | request-response | itself, lines 368-385 | exact |
| `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp` (`Initialize`/`Resize`, `FramebufferUtils`) | backend resource | CRUD | itself, lines 23-80, 109, 209, 933+ | exact |
| `Engine/Graphics/source/Renderer/RenderSystem.cpp` (restore lines) | renderer service | batch (per-frame) | itself, lines 285-309 | exact |
| `Engine/Graphics/include/Pyramid/Graphics/Framebuffer.hpp` (NEW `IFramebuffer`) | interface | request-response | `GraphicsDevice.hpp` lines 267-276 + `OpenGLFramebuffer` public surface | role-match |
| `Tests/PublicApiLinkage.cpp` (add/remove symbols) | test | batch | itself, lines 197-249 | exact |
| `Tests/FramebufferResizeTests.cpp` (extend: neutral bind, restore, depth targets) | test | batch | itself, lines 1-90 (fake-GL pattern) | exact |
| `Tests/TextureLoadingTests.cpp` (extend: per-format) | test | batch | itself, lines 1-70 (fake-GL pattern) | exact |
| `Tests/<new> ShadowArrayTests` / occlusion-removal assertions | test | batch | `Tests/FramebufferResizeTests.cpp` fake-GL harness | role-match |
| `docs/ROADMAP.md`, `CHANGELOG.md` (rationale entries) | docs | — | existing P0 section + prior removal entries | role-match |

## Pattern Assignments

### `RenderSystem.hpp` + `CommandBuffer.cpp` (FRZ-01 removal — contract/model + service, record-execute)

**Analog:** themselves (deletion task; linkage test is the verification)

**Command-model contract to delete** (`RenderSystem.hpp` lines 65, 89, 145):
```cpp
Dispatch,       // Compute shader dispatch
// ...
struct { u32 x, y, z; } dispatch;
void Dispatch(u32 x, u32 y, u32 z);
```

**Record site to delete** (`CommandBuffer.cpp` lines 259-269):
```cpp
void CommandBuffer::Dispatch(u32 x, u32 y, u32 z)
{
    if (!m_recording) return;

    RenderCommand cmd;
    cmd.type = RenderCommandType::Dispatch;
    cmd.data.dispatch.x = x;
    cmd.data.dispatch.y = y;
    cmd.data.dispatch.z = z;
    m_commands.push_back(cmd);
}
```
Copy the shape of the neighboring `ClearTarget` (lines 271-282: `RenderCommand cmd; cmd.type = ...; cmd.data...; m_commands.push_back(cmd);`) if any new
record method is ever needed — do not invent a new recording shape.

**Drop site to delete** (`CommandBuffer.cpp` lines 560-567):
```cpp
case RenderCommandType::Dispatch:
    PYRAMID_LOG_DEBUG("Compute dispatch command: ", cmd.data.dispatch.x, "x", cmd.data.dispatch.y, "x", cmd.data.dispatch.z);
    // Dispatch will be handled when compute shader support is added
    break;

default:
    PYRAMID_LOG_WARN("Unknown render command type: ", static_cast<int>(cmd.type));
    break;
```
Keep the `default:` loud-`WARN` arm — it is the sanctioned shape for unhandled commands. Never reintroduce a `LOG_DEBUG + break` arm.

**Interface methods to delete** (`Shader.hpp` lines 67-80):
```cpp
virtual bool CompileCompute(const std::string &computeSrc) = 0;
virtual void DispatchCompute(u32 numGroupsX, u32 numGroupsY, u32 numGroupsZ) = 0;
```

**Backend impl to delete with it** (`OpenGLShader.cpp` lines 558-573):
```cpp
void OpenGLShader::DispatchCompute(u32 numGroupsX, u32 numGroupsY, u32 numGroupsZ)
{
    if (m_programId == 0)
    {
        PYRAMID_LOG_ERROR("Cannot dispatch compute shader: no program loaded");
        return;
    }

    OpenGLStateManager::GetInstance().UseProgram(m_programId);
    glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);

    // Add memory barrier to ensure compute shader writes are visible
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    PYRAMID_LOG_DEBUG("Dispatched compute shader with groups: ", numGroupsX, "x", numGroupsY, "x", numGroupsZ);
}
```
Audit `BindShaderStorageBuffer`/`SetShaderStorageBlockBinding`/`CreateShaderStorageBuffer` at plan time: remove only if compute-exclusive,
keep generic buffer objects. Style: C++17, four spaces, braces on new lines, `PascalCase` methods, `m_` fields.

---

### `SceneManager.hpp` + `SceneManager.cpp` (FRZ-03 removal — service, visibility query)

**Analog:** themselves (deletion task; frustum + octree suites guard parity)

**Header surface to delete** (`SceneManager.hpp` lines 165, 177, 194):
```cpp
void SetOcclusionCullingEnabled(bool enabled) { m_occlusionCullingEnabled = enabled; }
// ...
bool OcclusionCull(const std::shared_ptr<RenderObject> &object, const Camera &camera);
// ...
bool m_occlusionCullingEnabled = false;
```
Note the neighbor `SetFrustumCullingEnabled` (line 164) stays — it is the authority that remains.

**Filter block to delete** (`SceneManager.cpp` lines 299-308):
```cpp
// Apply additional culling if enabled
if (m_occlusionCullingEnabled)
{
    auto it = std::remove_if(visibleObjects.begin(), visibleObjects.end(),
                             [this, &camera](const std::shared_ptr<RenderObject> &obj)
                             {
                                 return OcclusionCull(obj, camera);
                             });
    visibleObjects.erase(it, visibleObjects.end());
}
```

**Stub definition to delete** (`SceneManager.cpp` lines 541-547):
```cpp
bool SceneManager::OcclusionCull(const std::shared_ptr<RenderObject> &object, const Camera &camera)
{
    (void)object;
    (void)camera;
    // Occlusion culling is not implemented yet.
    return false;
}
```

**Visibility authority that remains** (`SceneManager.cpp` lines 277-297):
```cpp
if (m_frustumCullingEnabled)
{
    if (m_spatialPartitioningEnabled && m_octree)
    {
        visibleObjects = m_octree->QueryFrustum(camera.GetFrustumPlanes());
    }
    else
    {
        visibleObjects = m_activeScene->GetVisibleObjects(camera);
    }
}
```
Copy this branching shape (flag → octree path vs. scene path) for any future visibility work; do not add a second filter stage.

---

### `ShadowMapPass.cpp` + `DeferredLightingPass.cpp` (FRZ-02 implement — renderer service, per-frame batch)

**Analog:** themselves + `RenderSystem.cpp` restore convention

**Per-cascade creation to consolidate** (`ShadowMapPass.cpp` lines 61-100):
```cpp
for (u32 i = 0; i < m_cascadeCount; i++)
{
    // Create depth-only framebuffer specification
    FramebufferSpec spec;
    spec.width = m_shadowMapResolution;
    spec.height = m_shadowMapResolution;
    spec.samples = 1; // No MSAA for shadow maps
    spec.swapChainTarget = false;

    // Depth attachment only
    FramebufferAttachmentSpec depthSpec;
    depthSpec.type = FramebufferAttachmentType::Depth;
    depthSpec.internalFormat = GL_DEPTH_COMPONENT24;
    depthSpec.format = GL_DEPTH_COMPONENT;
    depthSpec.dataType = GL_FLOAT;
    depthSpec.minFilter = GL_NEAREST;
    depthSpec.magFilter = GL_NEAREST;
    depthSpec.wrapS = GL_CLAMP_TO_BORDER;
    depthSpec.wrapT = GL_CLAMP_TO_BORDER;
    depthSpec.multisampled = false;
    depthSpec.samples = 1;

    spec.attachments.push_back(depthSpec);

    // Create framebuffer
    auto shadowMap = std::make_shared<OpenGLFramebuffer>(spec);
    if (!shadowMap->Initialize())
    {
        PYRAMID_LOG_ERROR("Failed to initialize shadow map framebuffer ", i);
        continue;
    }

    // Set border color to 1.0 (outside shadow = not in shadow)
    GLuint depthTexture = shadowMap->GetDepthAttachmentTexture();
    m_device->SetTextureBorderColor(depthTexture, GL_TEXTURE_2D, 1.0f, 1.0f, 1.0f, 1.0f);

    m_shadowMaps.push_back(shadowMap);
}
```
Keep: fixed 2048 resolution (window-size-independent, intentional), `GL_DEPTH_COMPONENT24`, `GL_NEAREST`, `GL_CLAMP_TO_BORDER` + white border.
New array texture reuses these parameters with `glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT24, 2048, 2048, cascadeCount, ...)`
(all tokens/entry points verified in `vendor/glad/include/glad/glad.h` per RESEARCH).

**Single-cascade bind to replace** (`DeferredLightingPass.cpp` lines 133-141):
```cpp
// Bind shadow maps if available
if (!m_shadowMaps.empty())
{
    // For now, bind first shadow map cascade
    // TODO: Implement shadow map array binding
    GLuint shadowMap = m_shadowMaps[0]->GetDepthAttachmentTexture();
    m_device->BindNativeTexture(shadowMap, 5, GL_TEXTURE_2D);
    m_lightingShader->SetUniformInt("u_ShadowMaps", 5);
}
```
Target shape (from RESEARCH §Code Examples): `m_device->BindNativeTexture(shadowArray, 5, GL_TEXTURE_2D_ARRAY);` on the same unit 5,
same `u_ShadowMaps` uniform name. Copy the G-buffer bind idiom directly above (lines 118-131:
`m_device->BindNativeTexture(x, slot, GL_TEXTURE_2D); m_lightingShader->SetUniformInt(name, slot);`) with target swapped.

**Uniform-upload idiom to extend** (`DeferredLightingPass.cpp` lines 167-173):
```cpp
// Set shadow parameters
m_lightingShader->SetUniformFloat("u_ShadowBias", 0.005f);
m_lightingShader->SetUniformInt("u_CascadeCount", static_cast<int>(m_shadowMaps.size()));

// Set technique flags
m_lightingShader->SetUniformInt("u_EnableSSAO", m_enableSSAO ? 1 : 0);
m_lightingShader->SetUniformInt("u_EnableIBL", m_enableIBL ? 1 : 0);
```
The fix MUST add the two missing uploads in the same idiom: `u_LightSpaceMatrices` (from `ShadowMapPass::GetLightSpaceMatrices()`)
and `u_CascadeSplits` (from its `m_cascadeSplits`, +1 element). Done-criteria must list all three (matrices, splits, count) plus the bind.

**After-pass restore to preserve** (`RenderSystem.cpp` lines 300-304):
```cpp
// Render passes may bind off-screen targets and change the viewport.
// Re-establish the main render surface before the next pass and
// before direct overlays such as Pyramid::UI render after Render().
m_device->BindFramebufferHandle(0);
m_device->SetViewport(0, 0, m_width, m_height);
```
FRZ-05 migrates `BindFramebufferHandle(0)` onto the neutral `BindFramebuffer(nullptr)`; the comment + two-line shape stays.

---

### `OpenGLTexture.cpp` + `TextureResource.cpp` (FRZ-04 mapping — backend resource + cache service, upload)

**Analog:** themselves (extend the 4-format switch; pair type with format)

**Switch to extend** (`OpenGLTexture.cpp` lines 290-314):
```cpp
bool OpenGLTexture2D::ResolveFormats(
    TextureFormat format,
    bool srgb,
    GLenum& internalFormat,
    GLenum& dataFormat,
    u32& bytesPerPixel)
{
    switch (format)
    {
    case TextureFormat::RGB8:
    case TextureFormat::SRGB8:
        internalFormat = (srgb || format == TextureFormat::SRGB8) ? GL_SRGB8 : GL_RGB8;
        dataFormat = GL_RGB;
        bytesPerPixel = 3;
        return true;
    case TextureFormat::RGBA8:
    case TextureFormat::SRGBA8:
        internalFormat = (srgb || format == TextureFormat::SRGBA8) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        dataFormat = GL_RGBA;
        bytesPerPixel = 4;
        return true;
    default:
        return false;
    }
}
```
New arms follow the same `internalFormat + dataFormat + bytesPerPixel + return true` shape (RESEARCH §FRZ-04 table for the pairings;
planner verifies each against `glad.h` + spec tables 3.12/3.13/3.14). BC7 has NO arm (pruned from enum); S3TC arms add the runtime guard:
```cpp
if (!GLAD_GL_EXT_texture_compression_s3tc)
{
    PYRAMID_LOG_ERROR("...; driver lacks EXT_texture_compression_s3tc");
    return false;
}
```

**Hardcoded type to fix at both upload sites** (`OpenGLTexture.cpp` lines 414-423 and 214-223):
```cpp
glTexImage2D(
    GL_TEXTURE_2D,
    0,
    static_cast<GLint>(internalFormat),
    static_cast<GLsizei>(specification.Width),
    static_cast<GLsizei>(specification.Height),
    0,
    dataFormat,
    GL_UNSIGNED_BYTE,   // <-- must become per-format type (GL_FLOAT etc.)
    data);
```
Every non-8-bit mapping pairs its `type` argument with the format; keep the surrounding `glPixelStorei(GL_UNPACK_ALIGNMENT, 1)` /
`OpenGLDiagnostics::CheckError` / `ApplyParameters` sequence (`SetData` lines 208-240 show the full shape incl. error capture).

**Validation + overflow guards to reuse** (`OpenGLTexture.cpp` lines 17-40):
```cpp
bool IsValidExtent(u32 width, u32 height)
{
    return width > 0 && height > 0 &&
        width <= static_cast<u32>(std::numeric_limits<GLsizei>::max()) &&
        height <= static_cast<u32>(std::numeric_limits<GLsizei>::max());
}

bool CalculateByteSize(u32 width, u32 height, u32 bytesPerPixel, std::size_t& size)
{
    if (!IsValidExtent(width, height) || bytesPerPixel == 0)
    {
        return false;
    }

    const std::size_t rowSize = static_cast<std::size_t>(width) * bytesPerPixel;
    if (rowSize / bytesPerPixel != width ||
        static_cast<std::size_t>(height) > std::numeric_limits<std::size_t>::max() / rowSize)
    {
        return false;
    }

    size = rowSize * height;
    return true;
}
```

**Constructor validation order to copy** (`OpenGLTexture.cpp` lines 43-78):
```cpp
OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification, const void* data)
    : m_Specification(specification)
{
    if (!IsValidExtent(specification.Width, specification.Height))
    {
        SetError("Texture dimensions must be greater than zero and fit OpenGL limits");
        return;
    }

    if (!ResolveFormats(...))
    {
        SetError("Unsupported OpenGLTexture2D format");
        return;
    }
    // ... CreateTextureObject, SetError(error) on failure
```

**Cache mirror to extend in lockstep** (`TextureResource.cpp` lines 98-133):
```cpp
bool ResolveBaseFormat(TextureFormat input, TextureFormat& output, u32& bytesPerPixel)
{
    switch (input)
    {
    case TextureFormat::RGB8:
    case TextureFormat::SRGB8:
        output = TextureFormat::RGB8;
        bytesPerPixel = 3;
        return true;
    case TextureFormat::RGBA8:
    case TextureFormat::SRGBA8:
        output = TextureFormat::RGBA8;
        bytesPerPixel = 4;
        return true;
    default:
        return false;
    }
}

bool CalculateByteSize(u32 width, u32 height, u32 bytesPerPixel, u64& result)
{
    if (width == 0 || height == 0 || bytesPerPixel == 0)
    {
        return false;
    }

    const u64 row = static_cast<u64>(width) * bytesPerPixel;
    if (row / bytesPerPixel != width ||
        static_cast<u64>(height) > std::numeric_limits<u64>::max() / row)
    {
        return false;
    }

    result = row * height;
    return true;
}
```
AGENTS.md ownership: publish new depth/mapped-format resources through `ResourceRegistry::Textures()`; never mutate cached `TextureResource`
in place; content identity includes bytes + sampler + color space.

---

### `GraphicsDevice.hpp` + `OpenGLDevice.cpp` + `OpenGLFramebuffer.cpp` + `Texture.cpp` (FRZ-05 — interface + backend, request-response/CRUD)

**Analog:** themselves (define missing interface; route binds; implement factory)

**Neutral declaration to honor** (`GraphicsDevice.hpp` lines 267-276):
```cpp
/**
 * @brief Bind a framebuffer for rendering
 * @param framebuffer Framebuffer to bind (nullptr for default)
 */
virtual void BindFramebuffer(class IFramebuffer *framebuffer) = 0;
/**
 * @brief Bind a native framebuffer handle
 * @param framebufferId OpenGL framebuffer ID (0 for default)
 */
virtual void BindFramebufferHandle(u32 framebufferId) = 0;
```
New `Framebuffer.hpp` (or in-`GraphicsDevice.hpp` section) defines `class IFramebuffer` GLAD/Win32-free per AGENTS.md.
Planner starts minimal: `Bind()/Unbind()/IsComplete()/GetWidth()/GetHeight()` + opaque `u32` native handle — no speculative growth
(no required methods with silent no-op defaults).

**Stub to implement** (`OpenGLDevice.cpp` lines 368-385):
```cpp
void OpenGLDevice::BindFramebuffer(IFramebuffer *framebuffer)
{
    if (framebuffer)
    {
        // TODO: Implement when IFramebuffer interface is available
        m_lastError = "Framebuffer binding not yet implemented";
    }
    else
    {
        // Bind default framebuffer
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void OpenGLDevice::BindFramebufferHandle(u32 framebufferId)
{
    OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, framebufferId);
}
```
Target shape: non-null branch calls `framebuffer->Bind()` (via `OpenGLStateManager`) and clears `m_lastError`; null branch unchanged.
Copy the sibling null-tolerant binds directly below (lines 387-409: `BindShader`/`BindVertexArray` null → `UseProgram(0)` /
`BindVertexArray(0)`). All binds go through `OpenGLStateManager`, never raw GL.

**Transactional creation precedent** (`OpenGLFramebuffer.cpp` lines 23-80):
```cpp
bool OpenGLFramebuffer::Initialize()
{
    if (m_initialized)
    {
        PYRAMID_LOG_WARN("Framebuffer already initialized");
        return true;
    }

    if (!FramebufferUtils::ValidateSpec(m_spec))
    {
        PYRAMID_LOG_ERROR("Invalid framebuffer specification");
        return false;
    }

    if (m_spec.swapChainTarget)
    {
        m_initialized = true;
        return true;
    }

    glGenFramebuffers(1, &m_framebufferID);
    if (m_framebufferID == 0)
    {
        PYRAMID_LOG_ERROR("Failed to allocate framebuffer object");
        return false;
    }

    OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
    CreateAttachments();
    // ...
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        PYRAMID_LOG_ERROR(
            "Framebuffer not complete at ", m_spec.width, "x", m_spec.height,
            ": ", GetFramebufferStatusString(status));
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
        ReleaseResources();
        return false;
    }
```
Copy this validate → allocate → completeness-check → rollback (`ReleaseResources`, restore bind 0) shape for `CreateDepthTarget`
and the shadow-array (re)creation. `FramebufferUtils::IsValidExtent` (`OpenGLFramebuffer.cpp:933`, used at lines 109, 209) is the
extent guard for framebuffer-adjacent paths.

**Factory to implement** (`Texture.cpp` lines 38-45):
```cpp
std::shared_ptr<ITexture2D> ITexture2D::CreateDepthTarget(u32 width, u32 height, TextureFormat format)
{
    (void)width;
    (void)height;
    (void)format;
    PYRAMID_LOG_ERROR("Depth texture creation is not implemented by OpenGLTexture2D; use OpenGLFramebuffer");
    return nullptr;
}
```
This is the explicit-failure master pattern — the message shape (`PYRAMID_LOG_ERROR` + corrective pointer + `nullptr`) to copy for
S3TC-absent and any kept-reserved path. The FRZ-05 implementation routes through `OpenGLTexture2D` for the mapped depth/stencil set,
transactional (failure leaves nothing half-made), compare mode `NONE` default, documented. Compare the sibling factories above it
(lines 12-36: `Create` → `std::make_shared<OpenGLTexture2D>(...)`) for the success-path shape.

---

### `Tests/PublicApiLinkage.cpp` (test, batch)

**Analog:** itself, lines 197-249

**Pin idiom to extend/shrink** (lines 206-215):
```cpp
volatile CreateTextureFromSpec g_createTextureFromSpec =
    static_cast<CreateTextureFromSpec>(&ITexture2D::Create);
volatile CreateTextureFromFile g_createTextureFromFile =
    static_cast<CreateTextureFromFile>(&ITexture2D::Create);
volatile CreateTextureBySize g_createTextureBySize =
    static_cast<CreateTextureBySize>(&ITexture2D::Create);

volatile decltype(&ITexture2D::CreateRenderTarget) g_createRenderTarget = &ITexture2D::CreateRenderTarget;
volatile decltype(&ITexture2D::CreateDepthTarget) g_createDepthTarget = &ITexture2D::CreateDepthTarget;
volatile decltype(&ITexture2D::CreateFromColor) g_createFromColor = &ITexture2D::CreateFromColor;
```
Removal: delete the `g_…` line for each removed symbol (linkage test failing on leftovers IS the removal verification).
Addition: add one `volatile decltype(&…)` line per new public symbol (`IFramebuffer` methods used across the boundary).
Overload sets need the `using Alias = …; static_cast<Alias>(…)` disambiguation shape (lines 202-211).

---

### `Tests/FramebufferResizeTests.cpp` + `Tests/TextureLoadingTests.cpp` (test, batch — fake-GL harness to copy)

**Analog:** themselves (headless fake-GL stubs; no GPU needed)

**Harness shape** (`FramebufferResizeTests.cpp` lines 11-21, 23-59):
```cpp
GLenum g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
GLuint g_nextFramebuffer = 1;
GLuint g_nextTexture = 100;
std::vector<GLuint> g_deletedFramebuffers;
std::vector<GLuint> g_deletedTextures;

int Fail(const char* message)
{
    std::cerr << "FramebufferResizeTests failure: " << message << '\n';
    return EXIT_FAILURE;
}

void APIENTRY FakeGetIntegerv(GLenum name, GLint* values)
{
    if (!values)
    {
        return;
    }

    switch (name)
    {
    case GL_MAX_COLOR_ATTACHMENTS:
        values[0] = 8;
        break;
    // ...
    default:
        values[0] = 0;
        break;
    }
}
```

**Texture harness shape** (`TextureLoadingTests.cpp` lines 15-67):
```cpp
GLuint g_nextTexture = 10;
GLenum g_lastInternalFormat = 0;
GLenum g_lastDataFormat = 0;
GLint g_lastMinFilter = 0;
int g_generateMipmapCalls = 0;
int g_subImageCalls = 0;
std::vector<GLint> g_unpackAlignments;
std::vector<GLuint> g_deletedTextures;

int Fail(const char* message)
{
    std::cerr << "TextureLoadingTests failure: " << message << '\n';
    return EXIT_FAILURE;
}

GLenum APIENTRY FakeGetError()
{
    return GL_NO_ERROR;
}

void APIENTRY FakeTexImage2D(GLenum, /* ... */)
```
New/extended suites (shadow-array completeness + layer-count + single-bind + matrix/split/count upload presence; per-format
mapping incl. float/depth/R + `SetData` type-awareness + malformed/oversized/limit fixtures + S3TC-absent failure; neutral
`BindFramebuffer` real+null + restore-via-neutral + `CreateDepthTarget` per depth format + zero/oversized extents; removal
assertions for `Dispatch`/`CompileCompute`/`SetOcclusionCullingEnabled`) all copy this: file-local `g_…` capture globals,
`Fake*` GL entry points, `Fail()` with suite-prefixed `std::cerr`, `EXIT_FAILURE` on mismatch — fail visibly, no false-success
skips, clean temp files (AGENTS.md test culture). Run subsets via `ctest --preset test-gcc-debug -R "<Suite>.<Name>"`.

---

## Shared Patterns

### Explicit failure, never silent drop
**Source:** `Engine/Graphics/source/Texture.cpp:38-45`
**Apply to:** S3TC-without-extension, `None`/out-of-range formats, any kept-reserved API, pruned-format diagnostics
```cpp
PYRAMID_LOG_ERROR("Depth texture creation is not implemented by OpenGLTexture2D; use OpenGLFramebuffer");
return nullptr;
```
Style rules: message names the responsible backend + the corrective pointer. `PYRAMID_LOG_ERROR` for failures,
`PYRAMID_LOG_WARN` for unknown-command-type (CommandBuffer.cpp:566), `PYRAMID_LOG_DEBUG` for routine traces only.
Removal (compile-time signal) is preferred over this pattern wherever the surface is statically deletable (FRZ-01, FRZ-03).

### Transactional creation with rollback
**Source:** `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp:23-80` (`Initialize`); also `Resize` replacement-first, `TextureCache::Reload`, `ModelResourceImporter`
**Apply to:** `CreateDepthTarget`, shadow-array (re)creation, every new format-mapping upload
Validate spec/extent first → allocate → completeness/`CheckError` check → on failure restore prior state (`ReleaseResources()`, rebind 0),
preserve previous object, log with context (dimensions + status string). Tests assert the old object survives failed creation.

### After-pass surface restore through the neutral interface
**Source:** `Engine/Graphics/source/Renderer/RenderSystem.cpp:300-304`
**Apply to:** every pass touched by FRZ-02/FRZ-05 (`ShadowMapPass`, `DeferredGeometryPass`, `DeferredLightingPass`)
```cpp
m_device->BindFramebufferHandle(0);
m_device->SetViewport(0, 0, m_width, m_height);
```
Post-FRZ-05 the first line becomes the neutral `BindFramebuffer(nullptr)`; keep the comment, keep both lines together,
keep a restoration test per touched pass (`FramebufferResizeTests` / `UIRendererTests` pattern).

### Immutable content-addressed resources
**Source:** `Engine/Graphics/source/Texture/TextureResource.cpp` identity (`ResolveBaseFormat` + `CalculateContentId`); AGENTS.md constraints
**Apply to:** all depth-texture / mapped-format / shadow-array resource work
No in-place mutation of cached instances; no parallel upload ownership outside `ResourceRegistry::Textures()` /
`ModelResourceImporter` + mesh cache; per-draw data in command-buffer uniforms, never in resource identity.
Run `ResourceHandleTests`, `ResourceManifestTests`, `ResourceRegistryTests` on any cache-adjacent change.

### Input validation on creation paths (V5)
**Source:** `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp:17-40` (`IsValidExtent` + `CalculateByteSize`); `OpenGLFramebuffer.cpp:933+` (`FramebufferUtils`)
**Apply to:** every new creation path (array texture, depth targets, converted formats, compressed uploads — separate block-size path for S3TC)
Zero/oversized extents fail explicitly with `PYRAMID_LOG_ERROR`, never crash; malformed/truncated/oversized fixtures required per AGENTS.md
parser discipline. `CheckError`/`ClearErrors` (`OpenGLDiagnostics`) after every new GL call sequence.

### C++ conventions (AGENTS.md)
**Source:** repo-wide; exemplified by all files above
**Apply to:** all modified/new engine files
C++17, four spaces, braces on new lines; `PascalCase` types/methods, `camelCase` locals/params, `m_` fields; RAII + explicit ownership;
`Engine/` owns only Core/Graphics/Win32-WGL — no `Libraries/` logic enters the engine; `vendor/glad` is the sole runtime lib, no new
dependencies; no required interface methods with silent no-op defaults; no source-tree absolute paths in installed interfaces.

## No Analog Found

| File | Role | Data Flow | Reason |
|------|------|-----------|--------|
| (none — all covered) | — | — | Every FRZ touch point is a modification of an existing file whose current text is its own analog; the one new header (`Framebuffer.hpp`) and new test suites copy the adjacent interface/test shapes listed above. For GL enum pairings, planner verifies each against `glad.h` tokens + spec tables 3.12–3.14 (RESEARCH A1, `[ASSUMED]` mappings) rather than copying from the codebase. |

## Metadata

**Analog search scope:** `Engine/Graphics/include`, `Engine/Graphics/source` (Renderer, OpenGL, Scene, Texture), `Tests/`, `vendor/glad` (via RESEARCH verification)
**Files scanned:** ~25 (19 read this session incl. CONTEXT/RESEARCH/AGENTS.md; remainder via RESEARCH `[VERIFIED]` reads)
**Pattern extraction date:** 2026-09-11
