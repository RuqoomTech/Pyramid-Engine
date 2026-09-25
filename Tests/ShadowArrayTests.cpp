#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Math/Math.hpp>

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace
{
    GLint g_maxArrayTextureLayers = 256;
    GLenum g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
    GLuint g_nextTexture = 500;
    GLuint g_nextFramebuffer = 600;
    std::vector<GLuint> g_deletedTextures;
    std::vector<GLuint> g_deletedFramebuffers;

    struct TexImage3DCall
    {
        GLenum target = 0;
        GLint level = 0;
        GLint internalFormat = 0;
        GLsizei width = 0;
        GLsizei height = 0;
        GLsizei depth = 0;
        GLint border = 0;
        GLenum format = 0;
        GLenum type = 0;
    };
    std::vector<TexImage3DCall> g_texImage3DCalls;

    struct BindTextureCall
    {
        GLenum target = 0;
        GLuint texture = 0;
    };
    std::vector<BindTextureCall> g_bindTextureCalls;

    struct LayerAttachCall
    {
        GLenum attachment = 0;
        GLuint texture = 0;
        GLint level = 0;
        GLint layer = 0;
    };
    std::vector<LayerAttachCall> g_layerAttachCalls;

    int g_checkStatusCalls = 0;

    int Fail(const char* message)
    {
        std::cerr << "ShadowArrayTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    void ResetCaptures()
    {
        g_maxArrayTextureLayers = 256;
        g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
        g_nextTexture = 500;
        g_nextFramebuffer = 600;
        g_deletedTextures.clear();
        g_deletedFramebuffers.clear();
        g_texImage3DCalls.clear();
        g_bindTextureCalls.clear();
        g_layerAttachCalls.clear();
        g_checkStatusCalls = 0;
    }

    GLenum APIENTRY FakeGetError()
    {
        return GL_NO_ERROR;
    }

    void APIENTRY FakeGetIntegerv(GLenum name, GLint* values)
    {
        if (!values)
        {
            return;
        }

        switch (name)
        {
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
            values[0] = g_maxArrayTextureLayers;
            break;
        case GL_MAX_COLOR_ATTACHMENTS:
            values[0] = 8;
            break;
        case GL_MAX_SAMPLES:
            values[0] = 8;
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
    }

    void APIENTRY FakeBindFramebuffer(GLenum, GLuint)
    {
    }

    GLenum APIENTRY FakeCheckFramebufferStatus(GLenum)
    {
        ++g_checkStatusCalls;
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
    }

    void APIENTRY FakeBindTexture(GLenum target, GLuint texture)
    {
        g_bindTextureCalls.push_back(BindTextureCall{target, texture});
    }

    void APIENTRY FakeTexImage2D(
        GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)
    {
    }

    void APIENTRY FakeTexImage3D(
        GLenum target,
        GLint level,
        GLint internalFormat,
        GLsizei width,
        GLsizei height,
        GLsizei depth,
        GLint border,
        GLenum format,
        GLenum type,
        const void*)
    {
        g_texImage3DCalls.push_back(
            TexImage3DCall{target, level, internalFormat, width, height, depth, border, format, type});
    }

    void APIENTRY FakeTexImage2DMultisample(
        GLenum, GLsizei, GLenum, GLsizei, GLsizei, GLboolean)
    {
    }

    void APIENTRY FakeTexParameteri(GLenum, GLenum, GLint)
    {
    }

    void APIENTRY FakeFramebufferTexture2D(GLenum, GLenum, GLenum, GLuint, GLint)
    {
    }

    void APIENTRY FakeFramebufferTextureLayer(GLenum, GLenum attachment, GLuint texture, GLint level, GLint layer)
    {
        g_layerAttachCalls.push_back(LayerAttachCall{attachment, texture, level, layer});
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

    void APIENTRY FakeViewport(GLint, GLint, GLsizei, GLsizei)
    {
    }

    void InstallFakeOpenGL()
    {
        glad_glGetError = FakeGetError;
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
        glad_glBindTexture = FakeBindTexture;
        glad_glTexImage2D = FakeTexImage2D;
        glad_glTexImage3D = FakeTexImage3D;
        glad_glTexImage2DMultisample = FakeTexImage2DMultisample;
        glad_glTexParameteri = FakeTexParameteri;
        glad_glFramebufferTexture2D = FakeFramebufferTexture2D;
        glad_glFramebufferTextureLayer = FakeFramebufferTextureLayer;
        glad_glDrawBuffers = FakeDrawBuffers;
        glad_glDrawBuffer = FakeDrawBuffer;
        glad_glReadBuffer = FakeReadBuffer;
        glad_glViewport = FakeViewport;
    }

    class RecordingShader final : public Pyramid::IShader
    {
    public:
        struct Mat4Upload
        {
            int count = 0;
            std::vector<float> values;
        };

        void Bind() override { bound = true; }
        void Unbind() override { bound = false; }
        bool Compile(const std::string& vertex, const std::string& fragment) override
        {
            vertexSource = vertex;
            fragmentSource = fragment;
            return true;
        }
        bool CompileWithGeometry(
            const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileWithTessellation(
            const std::string&, const std::string&, const std::string&,
            const std::string&) override { return true; }
        bool CompileAdvanced(
            const std::string&, const std::string&, const std::string&,
            const std::string&, const std::string&) override { return true; }
        void SetUniformInt(const std::string& name, int value) override
        {
            intUniforms[name] = value;
        }
        void SetUniformFloat(const std::string& name, float value) override
        {
            floatUniforms[name] = value;
        }
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string& name, const float* matrix, bool, int count) override
        {
            Mat4Upload upload;
            upload.count = count;
            if (matrix != nullptr && count > 0)
            {
                upload.values.assign(matrix, matrix + static_cast<std::size_t>(count) * 16);
            }
            mat4Uniforms[name] = upload;
        }
        void BindUniformBuffer(
            const std::string&, Pyramid::IUniformBuffer*, Pyramid::u32) override {}
        void SetUniformBlockBinding(const std::string&, Pyramid::u32) override {}
        void BindShaderStorageBuffer(
            const std::string&, Pyramid::IShaderStorageBuffer*, Pyramid::u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, Pyramid::u32) override {}

        bool bound = false;
        std::string vertexSource;
        std::string fragmentSource;
        std::map<std::string, int> intUniforms;
        std::map<std::string, float> floatUniforms;
        std::map<std::string, Mat4Upload> mat4Uniforms;
    };

    int CountSlotBinds(
        const Pyramid::Tests::TestGraphicsDevice& device, Pyramid::u32 slot)
    {
        int count = 0;
        for (const auto& bind : device.nativeTextureBinds)
        {
            if (bind.slot == slot)
            {
                ++count;
            }
        }
        return count;
    }
}

int main()
{
    using namespace Pyramid;
    using Pyramid::Renderer::CommandBuffer;
    using Pyramid::Renderer::DeferredLightingPass;
    using Pyramid::Renderer::ForwardRenderPass;
    using Pyramid::Renderer::ShadowMapPass;

    ResetCaptures();
    InstallFakeOpenGL();

    // One cascade: the tracer configuration. A single layer must flow end to
    // end through the array texture into correctly transformed lighting.
    Tests::TestGraphicsDevice device;
    auto shader = std::make_shared<RecordingShader>();
    device.shaderFactory = [shader]() { return shader; };

    ShadowMapPass shadowPass("Shadow", &device, 1);
    if (shadowPass.GetShadowArrayTexture() == 0)
    {
        return Fail("one-layer shadow array was not created");
    }
    if (shadowPass.GetShadowArrayLayers() != 1)
    {
        return Fail("single-cascade shadow array must report exactly one layer");
    }
    if (shadowPass.GetShadowMapResolution() != 2048)
    {
        return Fail("shadow resolution must stay at the fixed 2048");
    }
    if (g_checkStatusCalls == 0)
    {
        return Fail("shadow array framebuffer completeness was never checked");
    }

    if (g_texImage3DCalls.size() != 1)
    {
        return Fail("single-cascade array creation must issue exactly one glTexImage3D");
    }
    const TexImage3DCall arrayStorage = g_texImage3DCalls.front();
    if (arrayStorage.target != GL_TEXTURE_2D_ARRAY)
    {
        return Fail("shadow array storage must target GL_TEXTURE_2D_ARRAY");
    }
    if (arrayStorage.width != 2048 || arrayStorage.height != 2048)
    {
        return Fail("shadow array resolution must stay window-size-independent at 2048");
    }
    if (arrayStorage.depth != 1)
    {
        return Fail("single-cascade shadow array must allocate exactly one layer");
    }
    if (arrayStorage.internalFormat != GL_DEPTH_COMPONENT24)
    {
        return Fail("shadow array must reuse the per-cascade GL_DEPTH_COMPONENT24 format");
    }

    auto scene = std::make_shared<Scene>("ShadowArray");
    auto light = SceneUtils::CreateDirectionalLight(Math::Vec3(0.5f, -1.0f, 0.5f));
    scene->AddLight(light);
    scene->SetPrimaryLight(light);

    Camera camera;
    CommandBuffer cmd;

    shadowPass.Execute(cmd, *scene, camera);

    bool attachedLayerZero = false;
    for (const auto& attach : g_layerAttachCalls)
    {
        if (attach.texture == shadowPass.GetShadowArrayTexture() &&
            attach.attachment == GL_DEPTH_ATTACHMENT && attach.layer == 0)
        {
            attachedLayerZero = true;
        }
    }
    if (!attachedLayerZero)
    {
        return Fail("single cascade render did not populate array layer zero");
    }
    if (device.boundFramebufferHandle != 0)
    {
        return Fail("shadow pass leaked its framebuffer instead of restoring handle 0");
    }
    if (device.lastNeutralFramebuffer != nullptr)
    {
        return Fail("shadow pass restore did not go through the neutral framebuffer method");
    }
    if (device.neutralFramebufferBinds == 0)
    {
        return Fail("shadow pass made no neutral framebuffer binds at all");
    }

    const FramebufferSpec gbufferSpec = FramebufferUtils::CreateColorDepthSpec(64, 64);
    auto gbuffer = std::make_shared<OpenGLFramebuffer>(gbufferSpec);
    if (!gbuffer->Initialize())
    {
        return Fail("G-buffer fixture did not initialize against the fake OpenGL backend");
    }

    DeferredLightingPass lightingPass("Lighting", &device);
    lightingPass.SetGBuffer(gbuffer);
    lightingPass.SetShadowPass(&shadowPass);
    lightingPass.Begin(cmd);
    lightingPass.Execute(cmd, *scene, camera);
    lightingPass.End(cmd);

    if (CountSlotBinds(device, 5) != 2)
    {
        return Fail("shadow slot must see exactly one array bind plus its End unbind");
    }
    bool sawArrayBind = false;
    for (const auto& bind : device.nativeTextureBinds)
    {
        if (bind.slot == 5 && bind.target == GL_TEXTURE_2D)
        {
            return Fail("shadow path still binds GL_TEXTURE_2D against the sampler2DArray");
        }
        if (bind.slot == 5 && bind.target == GL_TEXTURE_2D_ARRAY &&
            bind.textureId == shadowPass.GetShadowArrayTexture())
        {
            sawArrayBind = true;
        }
    }
    if (!sawArrayBind)
    {
        return Fail("lighting pass did not bind the shadow array with GL_TEXTURE_2D_ARRAY");
    }

    const auto shadowUnit = shader->intUniforms.find("u_ShadowMaps");
    if (shadowUnit == shader->intUniforms.end() || shadowUnit->second != 5)
    {
        return Fail("u_ShadowMaps sampler was not pointed at slot 5");
    }

    const auto matrices = shader->mat4Uniforms.find("u_LightSpaceMatrices");
    if (matrices == shader->mat4Uniforms.end())
    {
        return Fail("u_LightSpaceMatrices upload is missing from the lighting pass");
    }
    if (matrices->second.count != 1 || matrices->second.values.size() != 16)
    {
        return Fail("single-cascade matrix upload must carry exactly one 4x4 matrix");
    }

    const auto cascadeCount = shader->intUniforms.find("u_CascadeCount");
    if (cascadeCount == shader->intUniforms.end() || cascadeCount->second != 1)
    {
        return Fail("u_CascadeCount upload is missing or wrong for the single cascade");
    }

    // Forward pipeline scoping (RESEARCH Q4/A2): the forward pass renders via
    // per-material draws and carries no shadow texture binds or uploads, so it
    // renders unshadowed by design; shadow support arrives via the deferred
    // path only. Pin that behavior: a forward Execute must bind nothing.
    Tests::TestGraphicsDevice forwardDevice;
    ForwardRenderPass forwardPass(&forwardDevice);
    forwardPass.Begin(cmd);
    forwardPass.Execute(cmd, *scene, camera);
    forwardPass.End(cmd);
    if (!forwardDevice.nativeTextureBinds.empty())
    {
        return Fail("forward pass must carry no shadow texture binds (unshadowed by design)");
    }

    // Four cascades: the general path. Layer count must equal cascade count,
    // attaches must follow cascade order, and all cascades still bind through
    // one array exactly once with matrices, splits, and count uploaded.
    ResetCaptures();

    Tests::TestGraphicsDevice multiDevice;
    auto multiShader = std::make_shared<RecordingShader>();
    multiDevice.shaderFactory = [multiShader]() { return multiShader; };

    ShadowMapPass multiShadow("Shadow", &multiDevice, 4);
    if (multiShadow.GetShadowArrayTexture() == 0 || multiShadow.GetShadowArrayLayers() != 4)
    {
        return Fail("four-cascade shadow array must allocate four layers");
    }
    if (g_texImage3DCalls.size() != 1 || g_texImage3DCalls.front().depth != 4)
    {
        return Fail("four-cascade array storage must allocate exactly four layers");
    }

    auto multiScene = std::make_shared<Scene>("ShadowArrayMulti");
    auto multiLight = SceneUtils::CreateDirectionalLight(Math::Vec3(0.5f, -1.0f, 0.5f));
    multiScene->AddLight(multiLight);
    multiScene->SetPrimaryLight(multiLight);

    g_layerAttachCalls.clear();
    multiShadow.Execute(cmd, *multiScene, camera);

    if (g_layerAttachCalls.size() != 4)
    {
        return Fail("four cascades must attach exactly four layers");
    }
    for (u32 i = 0; i < 4; ++i)
    {
        if (g_layerAttachCalls[i].layer != static_cast<GLint>(i) ||
            g_layerAttachCalls[i].texture != multiShadow.GetShadowArrayTexture() ||
            g_layerAttachCalls[i].attachment != GL_DEPTH_ATTACHMENT)
        {
            return Fail("cascade layer order must match matrix and split order");
        }
    }
    if (multiDevice.boundFramebufferHandle != 0)
    {
        return Fail("multi-cascade shadow pass leaked its framebuffer");
    }
    if (multiDevice.lastNeutralFramebuffer != nullptr)
    {
        return Fail("multi-cascade shadow restore did not go through the neutral framebuffer method");
    }
    if (multiShadow.GetLightSpaceMatrices().size() != 4 ||
        multiShadow.GetCascadeSplits().size() != 5)
    {
        return Fail("four cascades must produce four matrices and five splits");
    }

    auto multiGbuffer = std::make_shared<OpenGLFramebuffer>(gbufferSpec);
    if (!multiGbuffer->Initialize())
    {
        return Fail("multi-cascade G-buffer fixture did not initialize");
    }

    DeferredLightingPass multiLighting("Lighting", &multiDevice);
    multiLighting.SetGBuffer(multiGbuffer);
    multiLighting.SetShadowPass(&multiShadow);
    multiLighting.Begin(cmd);
    multiLighting.Execute(cmd, *multiScene, camera);
    multiLighting.End(cmd);

    if (CountSlotBinds(multiDevice, 5) != 2)
    {
        return Fail("four cascades must still bind through one array exactly once");
    }
    for (const auto& bind : multiDevice.nativeTextureBinds)
    {
        if (bind.slot == 5 && bind.target == GL_TEXTURE_2D)
        {
            return Fail("multi-cascade shadow path binds GL_TEXTURE_2D");
        }
    }

    const auto multiMatrices = multiShader->mat4Uniforms.find("u_LightSpaceMatrices");
    if (multiMatrices == multiShader->mat4Uniforms.end() ||
        multiMatrices->second.count != 4 || multiMatrices->second.values.size() != 64)
    {
        return Fail("four-cascade matrix upload must carry four 4x4 matrices");
    }

    const auto multiCount = multiShader->intUniforms.find("u_CascadeCount");
    if (multiCount == multiShader->intUniforms.end() || multiCount->second != 4)
    {
        return Fail("four-cascade count upload must be 4");
    }

    const std::vector<f32>& expectedSplits = multiShadow.GetCascadeSplits();
    for (u32 i = 0; i < 5; ++i)
    {
        const std::string splitName = "u_CascadeSplits[" + std::to_string(i) + "]";
        const auto split = multiShader->floatUniforms.find(splitName);
        if (split == multiShader->floatUniforms.end() || split->second != expectedSplits[i])
        {
            return Fail("cascade split upload is missing or out of order");
        }
    }

    // The lighting End must unbind the shadow slot with the array target.
    bool slot5UnboundWithArrayTarget = false;
    for (const auto& bind : multiDevice.nativeTextureBinds)
    {
        if (bind.slot == 5 && bind.textureId == 0 && bind.target == GL_TEXTURE_2D_ARRAY)
        {
            slot5UnboundWithArrayTarget = true;
        }
    }
    if (!slot5UnboundWithArrayTarget)
    {
        return Fail("lighting End must unbind the shadow slot with the array target");
    }

    // Transactional recreation: a failed replacement preserves the previous
    // array, configuration, and texture instead of serving a half-update.
    const GLuint fourCascadeTexture = multiShadow.GetShadowArrayTexture();
    g_framebufferStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    multiShadow.SetCascadeCount(1);
    g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
    if (multiShadow.GetShadowArrayTexture() != fourCascadeTexture ||
        multiShadow.GetShadowArrayLayers() != 4 || multiShadow.GetCascadeCount() != 4)
    {
        return Fail("failed recreation must preserve the previous array and count");
    }

    // Successful recreation swaps the array and releases the old texture.
    multiShadow.SetCascadeCount(2);
    if (multiShadow.GetShadowArrayLayers() != 2 || multiShadow.GetCascadeCount() != 2)
    {
        return Fail("recreation on count change must swap to two layers");
    }
    if (multiShadow.GetShadowArrayTexture() == fourCascadeTexture)
    {
        return Fail("recreation must allocate a new array texture");
    }
    bool releasedOldArray = false;
    for (GLuint deleted : g_deletedTextures)
    {
        if (deleted == fourCascadeTexture)
        {
            releasedOldArray = true;
        }
    }
    if (!releasedOldArray)
    {
        return Fail("recreation must release the previous array texture");
    }

    // Over-limit counts fail explicitly and preserve the live array.
    const GLuint twoCascadeTexture = multiShadow.GetShadowArrayTexture();
    g_maxArrayTextureLayers = 1;
    multiShadow.SetCascadeCount(4);
    g_maxArrayTextureLayers = 256;
    if (multiShadow.GetShadowArrayTexture() != twoCascadeTexture ||
        multiShadow.GetShadowArrayLayers() != 2 || multiShadow.GetCascadeCount() != 2)
    {
        return Fail("over-limit cascade count must preserve the previous array");
    }

    // Counts above the shader array bound fail explicitly too.
    multiShadow.SetCascadeCount(5);
    if (multiShadow.GetShadowArrayTexture() != twoCascadeTexture ||
        multiShadow.GetCascadeCount() != 2)
    {
        return Fail("above-shader-limit cascade count must preserve the previous array");
    }

    // Empty input: with no shadow pass linked, the bind is skipped and
    // lighting continues unshadowed (quad still drawn, count zero).
    Tests::TestGraphicsDevice emptyDevice;
    auto emptyShader = std::make_shared<RecordingShader>();
    emptyDevice.shaderFactory = [emptyShader]() { return emptyShader; };
    DeferredLightingPass emptyLighting("Lighting", &emptyDevice);
    emptyLighting.SetGBuffer(multiGbuffer);
    emptyLighting.Begin(cmd);
    emptyLighting.Execute(cmd, *multiScene, camera);
    emptyLighting.End(cmd);

    if (CountSlotBinds(emptyDevice, 5) != 1)
    {
        return Fail("empty shadow set must skip the array bind but still unbind the slot");
    }
    for (const auto& bind : emptyDevice.nativeTextureBinds)
    {
        if (bind.slot == 5 && (bind.textureId != 0 || bind.target != GL_TEXTURE_2D_ARRAY))
        {
            return Fail("empty shadow set must never bind a shadow handle");
        }
    }
    const auto emptyCount = emptyShader->intUniforms.find("u_CascadeCount");
    if (emptyCount == emptyShader->intUniforms.end() || emptyCount->second != 0)
    {
        return Fail("empty shadow set must upload a zero cascade count");
    }
    if (emptyShader->mat4Uniforms.find("u_LightSpaceMatrices") != emptyShader->mat4Uniforms.end())
    {
        return Fail("empty shadow set must not upload matrices");
    }
    if (emptyDevice.drawCalls != 1)
    {
        return Fail("lighting must continue (draw its quad) with an empty shadow set");
    }

    // Zero cascades create no array and attach no layers.
    ShadowMapPass zeroShadow("Shadow", &emptyDevice, 0);
    if (zeroShadow.GetShadowArrayTexture() != 0 || zeroShadow.GetShadowArrayLayers() != 0)
    {
        return Fail("zero cascades must create no array texture");
    }
    const std::size_t attachesBefore = g_layerAttachCalls.size();
    zeroShadow.Execute(cmd, *multiScene, camera);
    if (g_layerAttachCalls.size() != attachesBefore)
    {
        return Fail("zero-cascade execute must attach no layers");
    }

    std::cout << "Shadow array tests passed\n";
    return EXIT_SUCCESS;
}
