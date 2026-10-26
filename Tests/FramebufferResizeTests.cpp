#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace
{
    GLenum g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
    GLuint g_nextFramebuffer = 1;
    GLuint g_nextTexture = 100;
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
        case GL_VIEWPORT:
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
    }

    void APIENTRY FakeBindFramebuffer(GLenum, GLuint)
    {
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

    bool Contains(const std::vector<GLuint>& values, GLuint value)
    {
        return std::find(values.begin(), values.end(), value) != values.end();
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
    const GLuint initialColor = initializedFramebuffer.GetColorAttachmentTexture();
    const GLuint initialDepth = initializedFramebuffer.GetDepthAttachmentTexture();

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
        stateManager.BindTextureUnit(1, GL_TEXTURE_2D, deletedColor);
        stateManager.BindTextureUnit(5, GL_TEXTURE_2D, deletedColor);
        deletedFramebuffer.Invalidate();

        const GLuint nextTexture = g_nextTexture;
        const std::size_t allocationsBeforeReuse = g_allocatedTextures.size();
        g_nextTexture = deletedColor;
        OpenGLFramebuffer reusedFramebuffer(colorOnly);
        if (!reusedFramebuffer.Initialize() ||
            g_allocatedTextures.size() != allocationsBeforeReuse + 1 ||
            g_allocatedTextures.back() != deletedColor)
        {
            return Fail("a reused attachment texture name skipped its allocation binding after deletion");
        }
        g_nextTexture = nextTexture;
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

    std::cout << "Framebuffer resize tests passed\n";
    return EXIT_SUCCESS;
}
