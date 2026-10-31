#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLDevice.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Scene.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cmath>
#include <limits>
#include <string>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace
{
    GLenum g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
    std::size_t g_debugLabelCalls = 0;
    GLenum g_debugLabelIdentifier = 0;
    GLuint g_debugLabelObject = 0;
    std::string g_debugLabelText;
    GLuint g_nextFramebuffer = 1;
    GLuint g_nextTexture = 100;
    GLuint g_drawFramebuffer = 0;
    GLuint g_readFramebuffer = 0;
    std::size_t g_framebufferBindCalls = 0;
    std::size_t g_viewportCalls = 0;
    std::array<GLint, 4> g_viewport = {0, 0, 1280, 720};
    std::vector<GLuint> g_attachmentFramebuffers;
    std::vector<GLuint> g_deletedFramebuffers;
    std::vector<GLuint> g_deletedTextures;
    GLenum g_activeTexture = GL_TEXTURE0;
    std::array<std::unordered_map<GLenum, GLuint>, 16> g_boundTextures;
    std::vector<GLuint> g_allocatedTextures;
    std::vector<GLuint> g_mipmappedTextures;

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
        case GL_MAX_SAMPLES:
            values[0] = 8;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            values[0] = 16;
            break;
        case GL_ACTIVE_TEXTURE:
            values[0] = static_cast<GLint>(g_activeTexture);
            break;
        case GL_DRAW_FRAMEBUFFER_BINDING:
            values[0] = static_cast<GLint>(g_drawFramebuffer);
            break;
        case GL_READ_FRAMEBUFFER_BINDING:
            values[0] = static_cast<GLint>(g_readFramebuffer);
            break;
        case GL_VIEWPORT:
            std::copy(g_viewport.begin(), g_viewport.end(), values);
            break;
        case GL_SCISSOR_BOX:
            values[0] = 0;
            values[1] = 0;
            values[2] = 1280;
            values[3] = 720;
            break;
        case GL_POLYGON_MODE:
            values[0] = GL_FILL;
            values[1] = GL_FILL;
            break;
        default:
            values[0] = 0;
            break;
        }
    }

    GLboolean APIENTRY FakeIsEnabled(GLenum)
    {
        return GL_FALSE;
    }

    void APIENTRY FakeGetBooleanv(GLenum, GLboolean* value)
    {
        if (value)
        {
            *value = GL_TRUE;
        }
    }

    void APIENTRY FakeGetFloatv(GLenum, GLfloat* values)
    {
        if (values)
        {
            values[0] = 0.0f;
            values[1] = 0.0f;
            values[2] = 0.0f;
            values[3] = 1.0f;
        }
    }

    void APIENTRY FakeGetDoublev(GLenum, GLdouble* value)
    {
        if (value)
        {
            *value = 1.0;
        }
    }

    void APIENTRY FakeGenFramebuffers(GLsizei count, GLuint* framebuffers)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            framebuffers[index] = g_nextFramebuffer++;
        }
    }

    void APIENTRY FakeDeleteFramebuffers(GLsizei count, const GLuint* framebuffers)
    {
        g_deletedFramebuffers.insert(
            g_deletedFramebuffers.end(), framebuffers, framebuffers + count);
        for (GLsizei index = 0; index < count; ++index)
        {
            if (g_drawFramebuffer == framebuffers[index]) g_drawFramebuffer = 0;
            if (g_readFramebuffer == framebuffers[index]) g_readFramebuffer = 0;
        }
    }

    void APIENTRY FakeBindFramebuffer(GLenum target, GLuint framebuffer)
    {
        ++g_framebufferBindCalls;
        if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER)
        {
            g_drawFramebuffer = framebuffer;
        }
        if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER)
        {
            g_readFramebuffer = framebuffer;
        }
    }

    GLenum APIENTRY FakeCheckFramebufferStatus(GLenum)
    {
        return g_framebufferStatus;
    }

    void APIENTRY FakeGenTextures(GLsizei count, GLuint* textures)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            textures[index] = g_nextTexture++;
        }
    }

    void APIENTRY FakeDeleteTextures(GLsizei count, const GLuint* textures)
    {
        g_deletedTextures.insert(g_deletedTextures.end(), textures, textures + count);
        for (auto& unitBindings : g_boundTextures)
        {
            for (auto& binding : unitBindings)
            {
                if (binding.second != 0 &&
                    std::find(textures, textures + count, binding.second) != textures + count)
                {
                    binding.second = 0;
                }
            }
        }
    }

    void APIENTRY FakeActiveTexture(GLenum texture)
    {
        g_activeTexture = texture;
    }

    void APIENTRY FakeBindTexture(GLenum target, GLuint texture)
    {
        g_boundTextures[g_activeTexture - GL_TEXTURE0][target] = texture;
    }

    void APIENTRY FakeTexImage2D(
        GLenum target, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)
    {
        g_allocatedTextures.push_back(g_boundTextures[g_activeTexture - GL_TEXTURE0][target]);
    }

    void APIENTRY FakeTexImage2DMultisample(
        GLenum target, GLsizei, GLenum, GLsizei, GLsizei, GLboolean)
    {
        g_allocatedTextures.push_back(g_boundTextures[g_activeTexture - GL_TEXTURE0][target]);
    }

    void APIENTRY FakeGenerateMipmap(GLenum target)
    {
        g_mipmappedTextures.push_back(g_boundTextures[g_activeTexture - GL_TEXTURE0][target]);
    }

    void APIENTRY FakeTexParameteri(GLenum, GLenum, GLint)
    {
    }

    void APIENTRY FakeFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint)
    {
        g_attachmentFramebuffers.push_back(g_drawFramebuffer);
    }

    void APIENTRY FakeDrawBuffers(GLsizei, const GLenum*)
    {
    }

    void APIENTRY FakeDrawBuffer(GLenum)
    {
    }

    void APIENTRY FakeReadBuffer(GLenum)
    {
    }

    void APIENTRY FakeViewport(GLint x, GLint y, GLsizei width, GLsizei height)
    {
        ++g_viewportCalls;
        g_viewport = {x, y, width, height};
    }

    void APIENTRY FakeObjectLabel(GLenum identifier, GLuint object, GLsizei length, const GLchar* label)
    {
        ++g_debugLabelCalls;
        g_debugLabelIdentifier = identifier;
        g_debugLabelObject = object;
        g_debugLabelText.assign(label, static_cast<std::size_t>(length));
    }

    void InstallFakeOpenGL()
    {
        glad_glGetIntegerv = FakeGetIntegerv;
        glad_glIsEnabled = FakeIsEnabled;
        glad_glGetBooleanv = FakeGetBooleanv;
        glad_glGetFloatv = FakeGetFloatv;
        glad_glGetDoublev = FakeGetDoublev;
        glad_glGenFramebuffers = FakeGenFramebuffers;
        glad_glDeleteFramebuffers = FakeDeleteFramebuffers;
        glad_glBindFramebuffer = FakeBindFramebuffer;
        glad_glCheckFramebufferStatus = FakeCheckFramebufferStatus;
        glad_glGenTextures = FakeGenTextures;
        glad_glDeleteTextures = FakeDeleteTextures;
        glad_glActiveTexture = FakeActiveTexture;
        glad_glBindTexture = FakeBindTexture;
        glad_glTexImage2D = FakeTexImage2D;
        glad_glTexImage2DMultisample = FakeTexImage2DMultisample;
        glad_glGenerateMipmap = FakeGenerateMipmap;
        glad_glTexParameteri = FakeTexParameteri;
        glad_glFramebufferTexture2D = FakeFramebufferTexture2D;
        glad_glDrawBuffers = FakeDrawBuffers;
        glad_glDrawBuffer = FakeDrawBuffer;
        glad_glReadBuffer = FakeReadBuffer;
        glad_glViewport = FakeViewport;
    }

    bool HasFramebufferBindings(GLuint draw, GLuint read)
    {
        auto& state = Pyramid::OpenGLStateManager::GetInstance();
        return g_drawFramebuffer == draw && g_readFramebuffer == read &&
            state.GetBoundFramebuffer(GL_DRAW_FRAMEBUFFER) == draw &&
            state.GetBoundFramebuffer(GL_READ_FRAMEBUFFER) == read;
    }

    bool HasViewport(GLint x, GLint y, GLint width, GLint height)
    {
        GLint cachedX, cachedY, cachedWidth, cachedHeight;
        Pyramid::OpenGLStateManager::GetInstance().GetViewport(
            cachedX, cachedY, cachedWidth, cachedHeight);
        return g_viewport == std::array<GLint, 4>{x, y, width, height} &&
            cachedX == x && cachedY == y && cachedWidth == width && cachedHeight == height;
    }

    bool Contains(const std::vector<GLuint>& values, GLuint value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
    }
    class ShadowTestUniformBuffer final : public Pyramid::IUniformBuffer
    {
    public:
        bool Initialize(size_t size, Pyramid::BufferUsage usage) override
        {
            m_data.resize(size);
            m_usage = usage;
            return true;
        }
        void UpdateData(const void* data, size_t size, size_t offset) override
        {
            if (!data || offset > m_data.size() || size > m_data.size() - offset)
            {
                validWrites = false;
                return;
            }
            std::memcpy(m_data.data() + offset, data, size);
        }
        void Bind(Pyramid::u32) override {}
        void Unbind() override {}
        size_t GetSize() const override { return m_data.size(); }
        Pyramid::BufferUsage GetUsage() const override { return m_usage; }
        void* Map(Pyramid::BufferAccess) override { m_mapped = true; return m_data.data(); }
        void Unmap() override { m_mapped = false; }
        bool IsMapped() const override { return m_mapped; }
        bool validWrites = true;
    private:
        std::vector<Pyramid::u8> m_data;
        Pyramid::BufferUsage m_usage = Pyramid::BufferUsage::Dynamic;
        bool m_mapped = false;
    };
    class ShadowRecordingShader final : public Pyramid::IShader
    {
    public:
        void Bind() override {}
        void Unbind() override {}
        bool Compile(const std::string& vertex, const std::string& fragment) override
        {
            ++compileCalls;
            vertexSource = vertex;
            fragmentSource = fragment;
            return compileSuccess;
        }
        bool CompileWithGeometry(const std::string&, const std::string&, const std::string&) override { return false; }
        bool CompileWithTessellation(const std::string&, const std::string&, const std::string&, const std::string&) override { return false; }
        bool CompileAdvanced(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&) override { return false; }
        bool CompileCompute(const std::string&) override { return false; }
        void DispatchCompute(Pyramid::u32, Pyramid::u32, Pyramid::u32) override {}
        void SetUniformInt(const std::string& name, int value) override { ints[name] = value; }
        void SetUniformFloat(const std::string& name, float value) override { floats[name] = value; }
        void SetUniformFloat2(const std::string& name, float x, float y) override { vectors[name] = {x, y, 0, 0}; }
        void SetUniformFloat3(const std::string& name, float x, float y, float z) override { vectors[name] = {x, y, z, 0}; }
        void SetUniformFloat4(const std::string& name, float x, float y, float z, float w) override { vectors[name] = {x, y, z, w}; }
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string& name, const float* data, bool transpose, int count) override
        {
            matrices[name] = std::vector<float>(data, data + 16 * count);
            matrixCounts[name] = count;
            matrixTransposes[name] = transpose;
        }
        void BindUniformBuffer(const std::string&, Pyramid::IUniformBuffer*, Pyramid::u32) override {}
        void SetUniformBlockBinding(const std::string&, Pyramid::u32) override {}
        void BindShaderStorageBuffer(const std::string&, Pyramid::IShaderStorageBuffer*, Pyramid::u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, Pyramid::u32) override {}

        bool compileSuccess = true;
        int compileCalls = 0;
        std::string vertexSource, fragmentSource;
        std::unordered_map<std::string, int> ints, matrixCounts;
        std::unordered_map<std::string, bool> matrixTransposes;
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, std::array<float, 4>> vectors;
        std::unordered_map<std::string, std::vector<float>> matrices;
    };

    Pyramid::Tests::TestGraphicsDevice::NativeTextureBinding LastNativeTexture(
        const Pyramid::Tests::TestGraphicsDevice& device, Pyramid::u32 slot)
    {
        for (auto binding = device.nativeTextureBindings.rbegin(); binding != device.nativeTextureBindings.rend(); ++binding)
        {
            if (binding->slot == slot) return *binding;
        }
        return {std::numeric_limits<Pyramid::u32>::max(), slot, 0};
    }

    bool ShadowSlotsCleared(const Pyramid::Tests::TestGraphicsDevice& device)
    {
        for (Pyramid::u32 slot = 5; slot < 5 + Pyramid::Renderer::MaxShadowCascades; ++slot)
        {
            const auto binding = LastNativeTexture(device, slot);
            if (binding.texture != 0 || binding.target != GL_TEXTURE_2D) return false;
        }
        return true;
    }

    bool CascadeContainsFrustum(const Pyramid::Renderer::ShadowMapPass& producer, const Pyramid::Camera& camera)
    {
        using namespace Pyramid;
        const auto& splits = producer.GetCascadeSplits();
        const auto& matrices = producer.GetLightSpaceMatrices();
        if (splits.size() != matrices.size() + 1) return false;
        const auto inverse = camera.GetViewProjectionMatrix().Inverse();
        for (std::size_t cascade = 0; cascade < matrices.size(); ++cascade)
        {
            for (int y : {-1, 1}) for (int x : {-1, 1})
            {
                auto nearCorner = inverse * Math::Vec4(static_cast<float>(x), static_cast<float>(y), -1, 1);
                auto farCorner = inverse * Math::Vec4(static_cast<float>(x), static_cast<float>(y), 1, 1);
                nearCorner = nearCorner / nearCorner.w;
                farCorner = farCorner / farCorner.w;
                for (std::size_t side = 0; side < 2; ++side)
                {
                    const float ratio = (splits[cascade + side] - camera.GetNearPlane()) /
                        (camera.GetFarPlane() - camera.GetNearPlane());
                    const auto point = nearCorner + (farCorner - nearCorner) * ratio;
                    const auto projected = matrices[cascade] * point;
                    if (!std::isfinite(projected.w) || std::abs(projected.w) < 0.00001f) return false;
                    for (float coordinate : {projected.x / projected.w, projected.y / projected.w, projected.z / projected.w})
                    {
                        if (!std::isfinite(coordinate) || std::abs(coordinate) > 1.005f) return false;
                    }
                }
            }
        }
        return true;
    }

    int TestDeferredShadowBinding()
    {
        using namespace Pyramid;
        using namespace Pyramid::Renderer;
        Tests::TestGraphicsDevice device;
        std::vector<std::shared_ptr<ShadowRecordingShader>> shaders;
        device.shaderFactory = [&]()
        {
            auto shader = std::make_shared<ShadowRecordingShader>();
            shaders.push_back(shader);
            return shader;
        };
        auto gBuffer = std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateGBufferSpec(64, 64));
        if (!gBuffer->Initialize()) return Fail("deferred shadow fixture G-buffer initialization failed");
        DeferredLightingPass lighting("Shadow binding test", &device);
        const auto shader = shaders.front();
        if (shader->compileCalls != 1 || shader->vertexSource.empty() || shader->fragmentSource.empty())
            return Fail("deferred lighting did not load shader sources; mock compilation cannot validate GLSL");
        lighting.SetGBuffer(gBuffer);
        Scene scene;
        auto lightEntity = scene.CreateEntity("Shadow light");
        LightComponent light;
        light.localDirection = Math::Vec3(0.3f, -1.0f, 0.2f);
        if (!lightEntity.SetLight(light)) return Fail("deferred shadow fixture light was rejected");
        scene.SetPrimaryLight(lightEntity);
        Camera camera(Math::Radians(60.0f), 1.5f, 0.5f, 80.0f);
        camera.SetPosition(Math::Vec3(3, 4, 5));
        camera.LookAt(Math::Vec3(0, 1, 0));
        CommandBuffer commands;
        std::vector<std::shared_ptr<OpenGLFramebuffer>> maps;
        std::vector<Math::Mat4> matrices;
        std::vector<f32> splits = {0.5f};
        for (u32 index = 0; index < MaxShadowCascades; ++index)
        {
            auto map = std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateShadowMapSpec(32, 32));
            if (!map->Initialize()) return Fail("deferred shadow fixture depth map initialization failed");
            maps.push_back(map);
            auto matrix = Math::Mat4::Identity;
            matrix.m[12] = static_cast<float>(index) + 0.25f;
            matrices.push_back(matrix);
            splits.push_back(10.0f * static_cast<float>(index + 1));
        }
        const auto Execute = [&]() { lighting.Execute(commands, scene, camera); };
        for (u32 count : {1u, 4u, MaxShadowCascades})
        {
            const std::vector<std::shared_ptr<OpenGLFramebuffer>> selectedMaps(maps.begin(), maps.begin() + count);
            const std::vector<Math::Mat4> selectedMatrices(matrices.begin(), matrices.begin() + count);
            const std::vector<f32> selectedSplits(splits.begin(), splits.begin() + count + 1);
            if (!lighting.SetShadowData(selectedMaps, selectedMatrices, selectedSplits, 0.0125f))
                return Fail("valid deferred cascade data was rejected");
            const u32 drawsBefore = device.drawCalls;
            Execute();
            if (shader->ints["u_CascadeCount"] != static_cast<int>(count) || device.drawCalls != drawsBefore + 1)
                return Fail("deferred lighting lost valid cascade count or fullscreen draw");
            for (u32 index = 0; index < MaxShadowCascades; ++index)
            {
                const auto binding = LastNativeTexture(device, 5 + index);
                const std::string suffix = "[" + std::to_string(index) + "]";
                if (binding.target != GL_TEXTURE_2D || binding.texture != (index < count ? maps[index]->GetDepthAttachmentTexture() : 0))
                    return Fail("deferred cascade native texture/target/unit mapping is incorrect");
                if (index < count && (shader->ints["u_ShadowMaps" + suffix] != static_cast<int>(5 + index) ||
                    shader->matrices["u_LightSpaceMatrices" + suffix] != std::vector<float>(matrices[index].m, matrices[index].m + 16) ||
                    shader->matrixCounts["u_LightSpaceMatrices" + suffix] != 1 || shader->matrixTransposes["u_LightSpaceMatrices" + suffix] ||
                    shader->floats["u_CascadeSplits" + suffix] != splits[index]))
                    return Fail("deferred cascade sampler/matrix/split metadata was not forwarded exactly");
            }
            if (shader->floats["u_CascadeSplits[" + std::to_string(count) + "]"] != splits[count] ||
                shader->floats["u_ShadowBias"] != 0.0125f ||
                shader->matrices["u_View"] != std::vector<float>(camera.GetViewMatrix().m, camera.GetViewMatrix().m + 16) ||
                shader->vectors["u_CameraPosition"] != std::array<float, 4>{3, 4, 5, 0})
                return Fail("deferred shadow view/camera, final split or bias metadata is incorrect");
            for (u32 slot = 0; slot < 4; ++slot)
                if (LastNativeTexture(device, slot).texture != gBuffer->GetColorAttachmentTexture(slot))
                    return Fail("shadow binding overwrote G-buffer color texture units");
            if (LastNativeTexture(device, 4).texture != gBuffer->GetDepthAttachmentTexture())
                return Fail("shadow binding overwrote G-buffer depth texture unit");
        }
        device.BindNativeTexture(987, 15, GL_TEXTURE_2D);
        lighting.End(commands);
        for (u32 slot = 0; slot < 5 + MaxShadowCascades; ++slot)
            if (LastNativeTexture(device, slot).texture != 0 || LastNativeTexture(device, slot).target != GL_TEXTURE_2D)
                return Fail("deferred End retained a consumed texture slot");
        if (LastNativeTexture(device, 15).texture != 987) return Fail("deferred End changed an unrelated texture unit");

        const auto PublishValid = [&]() { return lighting.SetShadowData(maps, matrices, splits, 0.01f); };
        const auto Rejected = [&](const auto& candidateMaps, const auto& candidateMatrices, const auto& candidateSplits, float bias)
        {
            if (!PublishValid() || lighting.SetShadowData(candidateMaps, candidateMatrices, candidateSplits, bias)) return false;
            Execute();
            return shader->ints["u_CascadeCount"] == 0 && ShadowSlotsCleared(device);
        };
        auto invalidMaps = maps;
        invalidMaps[0].reset();
        if (!Rejected(invalidMaps, matrices, splits, 0.01f)) return Fail("null shadow map did not disable stale data");
        invalidMaps = maps;
        invalidMaps[0] = std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateShadowMapSpec(32, 32));
        if (!Rejected(invalidMaps, matrices, splits, 0.01f)) return Fail("uninitialized shadow map was accepted");
        invalidMaps[0] = std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateColorOnlySpec(32, 32));
        if (!invalidMaps[0]->Initialize() || !Rejected(invalidMaps, matrices, splits, 0.01f))
            return Fail("depthless shadow map was accepted");
        invalidMaps[0] = std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateMultisampledSpec(32, 32, 4));
        if (!invalidMaps[0]->Initialize() || !Rejected(invalidMaps, matrices, splits, 0.01f))
            return Fail("multisampled shadow map was accepted");
        auto invalidMatrices = matrices;
        invalidMatrices.pop_back();
        if (!Rejected(maps, invalidMatrices, splits, 0.01f)) return Fail("mismatched shadow matrix count was accepted");
        invalidMatrices = matrices;
        invalidMatrices[0].m[3] = std::numeric_limits<float>::quiet_NaN();
        if (!Rejected(maps, invalidMatrices, splits, 0.01f)) return Fail("non-finite shadow matrix was accepted");
        auto invalidSplits = splits;
        invalidSplits.pop_back();
        if (!Rejected(maps, matrices, invalidSplits, 0.01f)) return Fail("mismatched cascade splits were accepted");
        invalidSplits = splits;
        invalidSplits[2] = invalidSplits[1];
        if (!Rejected(maps, matrices, invalidSplits, 0.01f)) return Fail("unordered cascade splits were accepted");
        invalidSplits = splits;
        invalidSplits.back() = std::numeric_limits<float>::infinity();
        if (!Rejected(maps, matrices, invalidSplits, 0.01f) ||
            !Rejected(maps, matrices, splits, -0.01f) ||
            !Rejected(maps, matrices, splits, std::numeric_limits<float>::quiet_NaN()))
            return Fail("non-finite split or invalid bias was accepted");
        invalidMaps = maps;
        invalidMaps.push_back(maps.back());
        invalidMatrices = matrices;
        invalidMatrices.push_back(matrices.back());
        invalidSplits = splits;
        invalidSplits.push_back(splits.back() + 10.0f);
        if (!Rejected(invalidMaps, invalidMatrices, invalidSplits, 0.01f)) return Fail("excessive shadow cascade count was accepted");

        if (!PublishValid()) return Fail("valid shadow metadata could not be restored");
        maps[0]->Invalidate();
        Execute();
        if (shader->ints["u_CascadeCount"] != 0 || !ShadowSlotsCleared(device))
            return Fail("post-publication framebuffer invalidation retained shadow sampling");
        maps[0] = std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateShadowMapSpec(32, 32));
        if (!maps[0]->Initialize() || !PublishValid()) return Fail("replacement shadow map failed");
        const auto oldDepth = maps[0]->GetDepthAttachmentTexture();
        if (!maps[0]->Resize(48, 48)) return Fail("shadow map resize failed");
        Execute();
        if (oldDepth == maps[0]->GetDepthAttachmentTexture() || LastNativeTexture(device, 5).texture != maps[0]->GetDepthAttachmentTexture())
            return Fail("deferred lighting retained pre-resize shadow texture name");
        if (!lighting.SetShadowData({}, {}, {}, 0)) return Fail("empty shadow metadata did not clear successfully");
        Execute();
        if (shader->ints["u_CascadeCount"] != 0 || !ShadowSlotsCleared(device)) return Fail("empty shadow data retained prior bindings");
        if (!PublishValid()) return Fail("legacy shadow setter fixture could not restore data");
        lighting.SetShadowMaps(maps);
        Execute();
        if (shader->ints["u_CascadeCount"] != 0 || !ShadowSlotsCleared(device)) return Fail("legacy maps-only setter reused stale matrices");
        if (!PublishValid()) return Fail("disabled-light fixture could not restore shadow data");
        light.castShadows = false;
        if (!lightEntity.SetLight(light)) return Fail("shadow-disabled light update failed");
        Execute();
        if (shader->ints["u_CascadeCount"] != 0 || !ShadowSlotsCleared(device)) return Fail("shadow-disabled light sampled old shadows");
        light.castShadows = true;
        if (!lightEntity.SetLight(light)) return Fail("shadow-enabled light update failed");
        scene.SetPrimaryLight(Entity{});
        Execute();
        if (shader->ints["u_CascadeCount"] != 0 || !ShadowSlotsCleared(device)) return Fail("missing light sampled old shadows");
        scene.SetPrimaryLight(lightEntity);
        const u32 drawsBeforeMissingGBuffer = device.drawCalls;
        lighting.SetGBuffer(nullptr);
        Execute();
        lighting.SetGBuffer(std::make_shared<OpenGLFramebuffer>(FramebufferUtils::CreateGBufferSpec(64, 64)));
        Execute();
        lighting.SetGBuffer(maps[1]);
        Execute();
        if (device.drawCalls != drawsBeforeMissingGBuffer) return Fail("missing/incomplete G-buffer submitted a fullscreen draw");
        lighting.SetGBuffer(gBuffer);

        ShadowMapPass producer("Producer binding test", &device, 4);
        producer.SetShadowMapResolution(256);
        producer.Begin(commands);
        if (producer.HasValidShadowData()) return Fail("shadow producer published ready data before rendering");
        producer.Execute(commands, scene, camera);
        if (!producer.HasValidShadowData() || !CascadeContainsFrustum(producer, camera))
            return Fail("shadow producer did not publish complete cascades containing their camera slices");
        const std::vector<f32> customSplits = {0.5f, 4, 12, 30, 80};
        producer.SetCascadeSplits(customSplits);
        producer.Begin(commands);
        producer.Execute(commands, scene, camera);
        if (!producer.HasValidShadowData() || producer.GetCascadeSplits() != customSplits || !CascadeContainsFrustum(producer, camera))
            return Fail("custom cascade splits were overwritten or did not bound their camera slices");
        auto malformedSplits = customSplits;
        malformedSplits[1] = std::numeric_limits<float>::quiet_NaN();
        producer.SetCascadeSplits(malformedSplits);
        const auto oldBias = producer.GetDepthBias();
        producer.SetDepthBias(std::numeric_limits<float>::quiet_NaN());
        if (producer.GetCascadeSplits() != customSplits || producer.GetDepthBias() != oldBias)
            return Fail("malformed producer splits or bias corrupted accepted settings");
        const auto previousMap = producer.GetShadowMaps().front();
        g_framebufferStatus = GL_FRAMEBUFFER_UNSUPPORTED;
        producer.SetCascadeCount(3);
        producer.SetShadowMapResolution(512);
        g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
        if (producer.GetShadowMaps().size() != 4 || producer.GetShadowMaps().front() != previousMap ||
            producer.GetShadowMaps().front()->GetWidth() != 256)
            return Fail("failed shadow recreation replaced a working cascade set");
        producer.SetCascadeCount(0);
        producer.SetCascadeCount(MaxShadowCascades + 1);
        if (producer.GetShadowMaps().size() != 4) return Fail("invalid shadow count changed the working cascade set");
        producer.Begin(commands);
        scene.SetPrimaryLight(Entity{});
        producer.Execute(commands, scene, camera);
        if (producer.HasValidShadowData()) return Fail("shadow producer retained prior-frame readiness without a light");
        ShadowMapPass boundedProducer("Bounded constructor test", &device, std::numeric_limits<u32>::max());
        if (boundedProducer.GetShadowMaps().empty() || boundedProducer.GetShadowMaps().size() > MaxShadowCascades)
            return Fail("shadow constructor did not bound cascade allocation");
        const auto failedShader = std::make_shared<ShadowRecordingShader>();
        failedShader->compileSuccess = false;
        device.shaderFactory = [&]() { return failedShader; };
        DeferredLightingPass failedLighting("Failed lighting shader test", &device);
        failedLighting.SetGBuffer(gBuffer);
        const auto drawsBeforeFailure = device.drawCalls;
        failedLighting.Execute(commands, scene, camera);
        if (failedShader->compileCalls != 1 || device.drawCalls != drawsBeforeFailure)
            return Fail("failed shader compilation submitted a deferred lighting draw");
        return EXIT_SUCCESS;
    }
    int TestDeferredPipelineRefresh()
    {
        using namespace Pyramid;
        using namespace Pyramid::Renderer;
        Tests::TestGraphicsDevice device;
        std::vector<std::shared_ptr<ShadowRecordingShader>> shaders;
        std::vector<std::shared_ptr<ShadowTestUniformBuffer>> buffers;
        device.shaderFactory = [&]()
        {
            auto shader = std::make_shared<ShadowRecordingShader>();
            shaders.push_back(shader);
            return shader;
        };
        device.uniformBufferFactory = [&](size_t size, BufferUsage usage)
        {
            auto buffer = std::make_shared<ShadowTestUniformBuffer>();
            buffer->Initialize(size, usage);
            buffers.push_back(buffer);
            return buffer;
        };
        RenderSystem renderer;
        if (!renderer.Initialize(&device)) return Fail("deferred pipeline refresh fixture failed initialization");
        renderer.SetupDeferredPipeline();
        const auto shader = shaders.back();
        auto shadow = std::dynamic_pointer_cast<ShadowMapPass>(renderer.GetRenderPass(RenderPassType::Shadow));
        auto geometry = std::dynamic_pointer_cast<DeferredGeometryPass>(renderer.GetRenderPass(RenderPassType::GBuffer));
        if (!shadow || !geometry) return Fail("built-in deferred setup omitted expected producer passes");
        shadow->SetShadowMapResolution(256);
        Scene scene;
        auto entity = scene.CreateEntity("Pipeline refresh light");
        LightComponent light;
        light.localDirection = Math::Vec3(0.3f, -1, 0.2f);
        if (!entity.SetLight(light)) return Fail("pipeline refresh light fixture failed");
        scene.SetPrimaryLight(entity);
        Camera camera(Math::Radians(60.0f), 1.5f, 0.5f, 80.0f);
        const auto Frame = [&]()
        {
            device.nativeTextureBindings.clear();
            renderer.BeginFrame();
            renderer.Render(scene, camera);
            renderer.EndFrame();
        };
        const auto WasBound = [&](u32 texture, u32 slot)
        {
            return std::any_of(device.nativeTextureBindings.begin(), device.nativeTextureBindings.end(),
                [&](const auto& binding) { return binding.texture == texture && binding.slot == slot && binding.target == GL_TEXTURE_2D; });
        };
        Frame();
        if (!shadow->HasValidShadowData() || shader->ints["u_CascadeCount"] != 4 ||
            !WasBound(shadow->GetShadowMaps()[0]->GetDepthAttachmentTexture(), 5))
            return Fail("built-in deferred pipeline did not connect current shadow data to lighting");
        const auto previousDepth = shadow->GetShadowMaps()[0]->GetDepthAttachmentTexture();
        shadow->SetCascadeCount(1);
        shadow->SetShadowMapResolution(512);
        Frame();
        const auto currentDepth = shadow->GetShadowMaps()[0]->GetDepthAttachmentTexture();
        if (currentDepth == previousDepth || shader->ints["u_CascadeCount"] != 1 ||
            !WasBound(currentDepth, 5) || WasBound(previousDepth, 5))
            return Fail("built-in deferred lighting retained stale cascade count or recreated texture ownership");
        shadow->SetEnabled(false);
        Frame();
        if (shader->ints["u_CascadeCount"] != 0 || !ShadowSlotsCleared(device))
            return Fail("disabled built-in shadow pass retained prior-frame deferred sampling");
        const auto previousGBuffer = geometry->GetGBuffer()->GetColorAttachmentTexture(0);
        if (!renderer.Resize(320, 240)) return Fail("pipeline resize fixture rejected valid surface dimensions");
        const auto currentGBuffer = geometry->GetGBuffer()->GetColorAttachmentTexture(0);
        Frame();
        if (currentGBuffer == previousGBuffer || !WasBound(currentGBuffer, 0) || WasBound(previousGBuffer, 0))
            return Fail("built-in deferred lighting retained the pre-resize G-buffer");
        for (const auto& buffer : buffers)
            if (!buffer->validWrites) return Fail("deferred refresh fixture received an invalid global uniform update");
        return EXIT_SUCCESS;
    }
}

int main()
{
    using namespace Pyramid;
    using Pyramid::Renderer::RenderTarget;
    using Pyramid::Renderer::RenderTargetSpec;

    if (!FramebufferUtils::IsValidExtent(1280, 720))
    {
        return Fail("valid extent was rejected");
    }
    if (FramebufferUtils::IsValidExtent(0, 720) ||
        FramebufferUtils::IsValidExtent(1280, 0))
    {
        return Fail("zero-sized extent was accepted");
    }

    const FramebufferSpec colorDepth = FramebufferUtils::CreateColorDepthSpec(640, 480);
    if (!FramebufferUtils::ValidateSpecStructure(colorDepth))
    {
        return Fail("valid color-depth specification was rejected");
    }

    FramebufferSpec duplicateColor = FramebufferUtils::CreateColorOnlySpec(640, 480);
    duplicateColor.attachments.push_back(duplicateColor.attachments.front());
    if (FramebufferUtils::ValidateSpecStructure(duplicateColor))
    {
        return Fail("duplicate color attachment index was accepted");
    }

    FramebufferSpec mixedDepthStencil = FramebufferUtils::CreateColorDepthSpec(640, 480);
    FramebufferAttachmentSpec combinedDepthStencil;
    combinedDepthStencil.type = FramebufferAttachmentType::DepthStencil;
    combinedDepthStencil.internalFormat = GL_DEPTH24_STENCIL8;
    combinedDepthStencil.format = GL_DEPTH_STENCIL;
    combinedDepthStencil.dataType = GL_UNSIGNED_INT_24_8;
    mixedDepthStencil.attachments.push_back(combinedDepthStencil);
    if (FramebufferUtils::ValidateSpecStructure(mixedDepthStencil))
    {
        return Fail("conflicting depth attachments were accepted");
    }

    FramebufferSpec invalidSamples = FramebufferUtils::CreateMultisampledSpec(640, 480, 4);
    invalidSamples.attachments.front().samples = 2;
    if (FramebufferUtils::ValidateSpecStructure(invalidSamples))
    {
        return Fail("mismatched multisample count was accepted");
    }

    FramebufferSpec swapChainSpec;
    swapChainSpec.width = 1280;
    swapChainSpec.height = 720;
    swapChainSpec.swapChainTarget = true;
    if (!FramebufferUtils::ValidateSpecStructure(swapChainSpec))
    {
        return Fail("swap-chain specification without owned attachments was rejected");
    }

    OpenGLFramebuffer uninitializedFramebuffer(colorDepth);
    if (uninitializedFramebuffer.Resize(0, 480))
    {
        return Fail("zero-width framebuffer resize succeeded");
    }
    if (uninitializedFramebuffer.GetWidth() != 640 ||
        uninitializedFramebuffer.GetHeight() != 480)
    {
        return Fail("rejected framebuffer resize changed stored dimensions");
    }
    if (!uninitializedFramebuffer.Resize(640, 480))
    {
        return Fail("same-size framebuffer resize should be a no-op success");
    }

    RenderTargetSpec targetSpec;
    targetSpec.width = 800;
    targetSpec.height = 600;
    RenderTarget target(targetSpec);

    if (!target.Resize(1024, 768))
    {
        return Fail("uninitialized render target did not accept a valid deferred resize");
    }
    if (target.GetWidth() != 1024 || target.GetHeight() != 768)
    {
        return Fail("render target dimensions were not updated");
    }
    if (target.Resize(0, 768))
    {
        return Fail("zero-width render target resize succeeded");
    }
    if (target.GetWidth() != 1024 || target.GetHeight() != 768)
    {
        return Fail("rejected render target resize changed stored dimensions");
    }

    InstallFakeOpenGL();

    auto& stateManager = OpenGLStateManager::GetInstance();
    OpenGLDevice openGLDevice(nullptr);
    IGraphicsDevice& device = openGLDevice;
    constexpr GLuint sampledTexture = 71;
    stateManager.BindTextureUnit(0, GL_TEXTURE_2D, sampledTexture);
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D, sampledTexture);

    OpenGLFramebuffer initializedFramebuffer(colorDepth);
    if (!initializedFramebuffer.Initialize())
    {
        return Fail("framebuffer did not initialize against the fake OpenGL backend");
    }
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D, sampledTexture);
    if (g_activeTexture != GL_TEXTURE3 ||
        g_boundTextures[3][GL_TEXTURE_2D] != sampledTexture ||
        g_boundTextures[0][GL_TEXTURE_2D] != sampledTexture)
    {
        return Fail("attachment creation left sampled texture bindings inconsistent with the cache");
    }

    const GLuint initialFramebuffer = initializedFramebuffer.GetFramebufferID();
    const auto previousVersion = GLVersion;
    const auto previousDebugSupport = GLAD_GL_KHR_debug;
    const auto previousObjectLabel = glad_glObjectLabel;
    GLVersion.major = 3;
    GLVersion.minor = 3;
    GLAD_GL_KHR_debug = 0;
    glad_glObjectLabel = FakeObjectLabel;
    initializedFramebuffer.SetDebugLabel("Unavailable label");
    const bool unsupportedLabelSkipped = g_debugLabelCalls == 0;
    GLAD_GL_KHR_debug = 1;
    initializedFramebuffer.SetDebugLabel("Supported framebuffer label");
    const bool supportedLabelForwarded = g_debugLabelCalls == 1 &&
        g_debugLabelIdentifier == GL_FRAMEBUFFER && g_debugLabelObject == initialFramebuffer &&
        g_debugLabelText == "Supported framebuffer label";
    GLVersion = previousVersion;
    GLAD_GL_KHR_debug = previousDebugSupport;
    glad_glObjectLabel = previousObjectLabel;
    if (!unsupportedLabelSkipped || !supportedLabelForwarded)
    {
        return Fail("framebuffer debug labels ignored context capability or lost the supported label");
    }
    const GLuint initialColor = initializedFramebuffer.GetColorAttachmentTexture();
    const GLuint initialDepth = initializedFramebuffer.GetDepthAttachmentTexture();

    device.SetViewport(11, 17, 321, 123);
    const std::size_t bindsBeforeInvalid = g_framebufferBindCalls;
    const std::size_t viewportsBeforeInvalid = g_viewportCalls;
    device.BindFramebuffer(&uninitializedFramebuffer);
    if (device.GetLastError().empty() || !HasFramebufferBindings(0, 0) ||
        !HasViewport(11, 17, 321, 123) || g_framebufferBindCalls != bindsBeforeInvalid ||
        g_viewportCalls != viewportsBeforeInvalid)
    {
        return Fail("uninitialized generic framebuffer binding did not report failure and preserve state");
    }
    device.BindFramebuffer(&initializedFramebuffer);
    if (!device.GetLastError().empty() ||
        !HasFramebufferBindings(initialFramebuffer, initialFramebuffer) ||
        !HasViewport(0, 0, 640, 480))
    {
        return Fail("generic framebuffer binding did not select both targets, apply its viewport, or clear the old error");
    }
    const std::size_t bindsBeforeRepeat = g_framebufferBindCalls;
    const std::size_t viewportsBeforeRepeat = g_viewportCalls;
    device.BindFramebuffer(&initializedFramebuffer);
    if (g_framebufferBindCalls != bindsBeforeRepeat || g_viewportCalls != viewportsBeforeRepeat)
    {
        return Fail("repeated generic framebuffer binding bypassed the state cache");
    }
    stateManager.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    if (!HasFramebufferBindings(initialFramebuffer, 0))
    {
        return Fail("test fixture did not preserve independent draw/read framebuffer bindings");
    }
    device.BindFramebuffer(&initializedFramebuffer);
    if (!HasFramebufferBindings(initialFramebuffer, initialFramebuffer))
    {
        return Fail("generic framebuffer binding did not reunify split draw/read targets");
    }
    device.BindFramebuffer(nullptr);
    if (!HasFramebufferBindings(0, 0) || !HasViewport(0, 0, 640, 480) ||
        g_viewportCalls != viewportsBeforeRepeat)
    {
        return Fail("default framebuffer binding did not select zero or unexpectedly changed viewport");
    }
    device.SetViewport(0, 0, 1280, 720);
    if (!HasViewport(0, 0, 1280, 720))
    {
        return Fail("explicit main-surface viewport restoration failed");
    }
    initializedFramebuffer.Bind();
    if (!HasFramebufferBindings(initialFramebuffer, initialFramebuffer) ||
        !HasViewport(0, 0, 640, 480))
    {
        return Fail("legacy direct framebuffer binding lost its target viewport behavior");
    }

    if (!initializedFramebuffer.Resize(800, 600))
    {
        return Fail("valid initialized framebuffer resize failed");
    }
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D, sampledTexture);
    if (g_activeTexture != GL_TEXTURE3 ||
        g_boundTextures[3][GL_TEXTURE_2D] != sampledTexture ||
        g_boundTextures[0][GL_TEXTURE_2D] != sampledTexture)
    {
        return Fail("attachment resize left sampled texture bindings inconsistent with the cache");
    }
    if (initializedFramebuffer.GetWidth() != 800 ||
        initializedFramebuffer.GetHeight() != 600)
    {
        return Fail("successful framebuffer resize did not update dimensions");
    }
    if (initializedFramebuffer.GetFramebufferID() == initialFramebuffer ||
        initializedFramebuffer.GetColorAttachmentTexture() == initialColor ||
        initializedFramebuffer.GetDepthAttachmentTexture() == initialDepth)
    {
        return Fail("successful framebuffer resize did not replace owned objects");
    }
    if (!Contains(g_deletedFramebuffers, initialFramebuffer) ||
        !Contains(g_deletedTextures, initialColor) ||
        !Contains(g_deletedTextures, initialDepth))
    {
        return Fail("successful framebuffer resize did not release old objects");
    }

    const GLuint stableFramebuffer = initializedFramebuffer.GetFramebufferID();
    const GLuint stableColor = initializedFramebuffer.GetColorAttachmentTexture();
    const GLuint stableDepth = initializedFramebuffer.GetDepthAttachmentTexture();
    device.BindFramebuffer(&initializedFramebuffer);
    if (!HasFramebufferBindings(stableFramebuffer, stableFramebuffer) ||
        !HasViewport(0, 0, 800, 600))
    {
        return Fail("generic binding after resize used stale framebuffer ownership or viewport dimensions");
    }

    initializedFramebuffer.GenerateMipmaps();
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D, sampledTexture);
    if (g_mipmappedTextures.size() != 1 || g_mipmappedTextures.back() != stableColor ||
        g_activeTexture != GL_TEXTURE3 ||
        g_boundTextures[3][GL_TEXTURE_2D] != sampledTexture ||
        g_boundTextures[0][GL_TEXTURE_2D] != sampledTexture)
    {
        return Fail("attachment mipmap generation used the wrong texture or left a stale sampled binding");
    }

    const std::size_t deletedBeforeFailure = g_deletedTextures.size();
    g_framebufferStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    if (initializedFramebuffer.Resize(1024, 768))
    {
        return Fail("incomplete replacement framebuffer was accepted");
    }
    g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D, sampledTexture);
    if (g_deletedTextures.size() != deletedBeforeFailure + 2 ||
        Contains(g_deletedTextures, stableColor) || Contains(g_deletedTextures, stableDepth) ||
        g_activeTexture != GL_TEXTURE3 ||
        g_boundTextures[3][GL_TEXTURE_2D] != sampledTexture ||
        g_boundTextures[0][GL_TEXTURE_2D] != sampledTexture)
    {
        return Fail("failed resize did not release temporary textures or corrupted surviving sampled bindings");
    }

    if (initializedFramebuffer.GetWidth() != 800 ||
        initializedFramebuffer.GetHeight() != 600 ||
        initializedFramebuffer.GetFramebufferID() != stableFramebuffer ||
        initializedFramebuffer.GetColorAttachmentTexture() != stableColor ||
        initializedFramebuffer.GetDepthAttachmentTexture() != stableDepth)
    {
        return Fail("failed framebuffer resize did not preserve the previous valid state");
    }

    {
        const FramebufferSpec colorOnly = FramebufferUtils::CreateColorOnlySpec(32, 32);
        OpenGLFramebuffer deletedFramebuffer(colorOnly);
        if (!deletedFramebuffer.Initialize())
        {
            return Fail("deletion regression framebuffer did not initialize");
        }
        const GLuint deletedColor = deletedFramebuffer.GetColorAttachmentTexture();
        const GLuint deletedNativeFramebuffer = deletedFramebuffer.GetFramebufferID();
        device.BindFramebuffer(&deletedFramebuffer);
        stateManager.BindFramebuffer(GL_READ_FRAMEBUFFER, stableFramebuffer);
        if (!HasFramebufferBindings(deletedNativeFramebuffer, stableFramebuffer))
        {
            return Fail("deletion fixture did not establish an unrelated live read framebuffer");
        }
        stateManager.BindTextureUnit(1, GL_TEXTURE_2D, deletedColor);
        stateManager.BindTextureUnit(5, GL_TEXTURE_2D, deletedColor);
        deletedFramebuffer.Invalidate();
        if (!HasFramebufferBindings(0, stableFramebuffer))
        {
            return Fail("framebuffer deletion retained its draw binding or lost the unrelated live read binding");
        }
        const std::size_t bindsBeforeInvalidated = g_framebufferBindCalls;
        const std::size_t viewportsBeforeInvalidated = g_viewportCalls;
        device.BindFramebuffer(&deletedFramebuffer);
        if (device.GetLastError().empty() || !HasFramebufferBindings(0, stableFramebuffer) ||
            !HasViewport(0, 0, 32, 32) || g_framebufferBindCalls != bindsBeforeInvalidated ||
            g_viewportCalls != viewportsBeforeInvalidated)
        {
            return Fail("invalidated framebuffer binding did not report failure and preserve state");
        }

        const GLuint nextTexture = g_nextTexture;
        const GLuint nextFramebuffer = g_nextFramebuffer;
        const std::size_t allocationsBeforeReuse = g_allocatedTextures.size();
        const std::size_t attachmentsBeforeReuse = g_attachmentFramebuffers.size();
        g_nextTexture = deletedColor;
        g_nextFramebuffer = deletedNativeFramebuffer;
        OpenGLFramebuffer reusedFramebuffer(colorOnly);
        if (!reusedFramebuffer.Initialize() ||
            g_allocatedTextures.size() != allocationsBeforeReuse + 1 ||
            g_allocatedTextures.back() != deletedColor)
        {
            return Fail("a reused attachment texture name skipped its allocation binding after deletion");
        }
        g_nextTexture = nextTexture;
        g_nextFramebuffer = nextFramebuffer;
        if (reusedFramebuffer.GetFramebufferID() != deletedNativeFramebuffer ||
            g_attachmentFramebuffers.size() != attachmentsBeforeReuse + 1 ||
            g_attachmentFramebuffers.back() != deletedNativeFramebuffer)
        {
            return Fail("a reused framebuffer name skipped its required attachment binding after deletion");
        }
        device.BindFramebuffer(&reusedFramebuffer);
        if (!device.GetLastError().empty() ||
            !HasFramebufferBindings(deletedNativeFramebuffer, deletedNativeFramebuffer))
        {
            return Fail("reused framebuffer could not be bound through the generic interface");
        }
        stateManager.BindTextureUnit(1, GL_TEXTURE_2D, deletedColor);
        stateManager.BindTextureUnit(5, GL_TEXTURE_2D, deletedColor);
        if (g_boundTextures[1][GL_TEXTURE_2D] != deletedColor ||
            g_boundTextures[5][GL_TEXTURE_2D] != deletedColor ||
            g_boundTextures[0][GL_TEXTURE_2D] != sampledTexture)
        {
            return Fail("attachment deletion left its reused name cached on another texture unit");
        }
    }

    stateManager.BindTextureUnit(3, GL_TEXTURE_2D, sampledTexture);
    constexpr GLuint sampledMultisampleTexture = 72;
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D_MULTISAMPLE, sampledMultisampleTexture);
    OpenGLFramebuffer multisampledFramebuffer(
        FramebufferUtils::CreateMultisampledSpec(32, 32, 4));
    if (!multisampledFramebuffer.Initialize())
    {
        return Fail("multisampled framebuffer did not initialize");
    }
    stateManager.BindTextureUnit(3, GL_TEXTURE_2D_MULTISAMPLE, sampledMultisampleTexture);
    const std::size_t mipmapsBeforeMultisample = g_mipmappedTextures.size();
    multisampledFramebuffer.GenerateMipmaps();
    if (g_mipmappedTextures.size() != mipmapsBeforeMultisample ||
        g_activeTexture != GL_TEXTURE3 ||
        g_boundTextures[3][GL_TEXTURE_2D] != sampledTexture ||
        g_boundTextures[3][GL_TEXTURE_2D_MULTISAMPLE] != sampledMultisampleTexture)
    {
        return Fail("multisampled attachments corrupted a sampled target binding or attempted invalid mipmaps");
    }

    device.BindFramebuffer(&initializedFramebuffer);
    OpenGLFramebuffer swapChainFramebuffer(swapChainSpec);
    if (!swapChainFramebuffer.Initialize())
    {
        return Fail("swap-chain framebuffer did not initialize");
    }
    device.BindFramebuffer(&swapChainFramebuffer);
    if (!HasFramebufferBindings(0, 0) || !HasViewport(0, 0, 1280, 720))
    {
        return Fail("generic swap-chain binding did not select zero and its target viewport");
    }
    if (!swapChainFramebuffer.Resize(1920, 1080))
    {
        return Fail("swap-chain framebuffer resize failed");
    }
    device.BindFramebuffer(&swapChainFramebuffer);
    if (!HasFramebufferBindings(0, 0) || !HasViewport(0, 0, 1920, 1080))
    {
        return Fail("generic resized swap-chain binding did not apply its new viewport");
    }

    if (TestDeferredShadowBinding() != EXIT_SUCCESS) return EXIT_FAILURE;
    if (TestDeferredPipelineRefresh() != EXIT_SUCCESS) return EXIT_FAILURE;
    std::cout << "Framebuffer binding, resize and deferred shadow tests passed\n";
    return EXIT_SUCCESS;
}
