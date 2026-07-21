#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
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
        case GL_MAX_SAMPLES:
            values[0] = 8;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            values[0] = 16;
            break;
        case GL_ACTIVE_TEXTURE:
            values[0] = GL_TEXTURE0;
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
    }

    void APIENTRY FakeBindTexture(GLenum, GLuint)
    {
    }

    void APIENTRY FakeTexImage2D(
        GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)
    {
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
        glad_glBindTexture = FakeBindTexture;
        glad_glTexImage2D = FakeTexImage2D;
        glad_glTexImage2DMultisample = FakeTexImage2DMultisample;
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

    OpenGLFramebuffer initializedFramebuffer(colorDepth);
    if (!initializedFramebuffer.Initialize())
    {
        return Fail("framebuffer did not initialize against the fake OpenGL backend");
    }

    const GLuint initialFramebuffer = initializedFramebuffer.GetFramebufferID();
    const GLuint initialColor = initializedFramebuffer.GetColorAttachmentTexture();
    const GLuint initialDepth = initializedFramebuffer.GetDepthAttachmentTexture();

    if (!initializedFramebuffer.Resize(800, 600))
    {
        return Fail("valid initialized framebuffer resize failed");
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

    g_framebufferStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
    if (initializedFramebuffer.Resize(1024, 768))
    {
        return Fail("incomplete replacement framebuffer was accepted");
    }
    g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;

    if (initializedFramebuffer.GetWidth() != 800 ||
        initializedFramebuffer.GetHeight() != 600 ||
        initializedFramebuffer.GetFramebufferID() != stableFramebuffer ||
        initializedFramebuffer.GetColorAttachmentTexture() != stableColor ||
        initializedFramebuffer.GetDepthAttachmentTexture() != stableDepth)
    {
        return Fail("failed framebuffer resize did not preserve the previous valid state");
    }

    std::cout << "Framebuffer resize tests passed\n";
    return EXIT_SUCCESS;
}
