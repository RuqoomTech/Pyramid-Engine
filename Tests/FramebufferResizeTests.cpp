#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLDevice.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Scene.hpp>

#include "TestGraphicsDevice.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    GLenum g_framebufferStatus = GL_FRAMEBUFFER_COMPLETE;
    GLuint g_nextFramebuffer = 1;
    GLuint g_nextTexture = 100;
    std::vector<GLuint> g_deletedFramebuffers;
    std::vector<GLuint> g_deletedTextures;
    GLenum g_lastBindTarget = 0;
    GLuint g_lastBoundFramebuffer = 0;
    int g_bindFramebufferCalls = 0;

    int Fail(const char* message)
    {
        std::cerr << "FramebufferResizeTests failure: " << message << '\n';
        return EXIT_FAILURE;
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
        case GL_MAX_COLOR_ATTACHMENTS:
            values[0] = 8;
            break;
        case GL_MAX_SAMPLES:
            values[0] = 8;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            values[0] = 16;
            break;
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
            values[0] = 256;
            break;
        case GL_ACTIVE_TEXTURE:
            values[0] = GL_TEXTURE0;
            break;
        case GL_DRAW_FRAMEBUFFER_BINDING:
        case GL_READ_FRAMEBUFFER_BINDING:
            // Report what was last bound so the state-manager cache stays
            // coherent with the fake driver's own binding history.
            values[0] = static_cast<GLint>(g_lastBoundFramebuffer);
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

    void APIENTRY FakeBindFramebuffer(GLenum target, GLuint framebuffer)
    {
        g_lastBindTarget = target;
        g_lastBoundFramebuffer = framebuffer;
        ++g_bindFramebufferCalls;
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

    void APIENTRY FakeTexImage3D(
        GLenum, GLint, GLint, GLsizei, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)
    {
    }

    void APIENTRY FakeFramebufferTextureLayer(GLenum, GLenum, GLuint, GLint, GLint)
    {
    }

    void APIENTRY FakeObjectLabel(GLenum, GLuint, GLsizei, const GLchar*)
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
        glad_glObjectLabel = FakeObjectLabel;
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

    // Backend-neutral binding (FRZ-05). A framebuffer object must bind for
    // real through IGraphicsDevice::BindFramebuffer, the null branch must
    // restore the default surface, and a successful bind must not leave a
    // stale device error behind.
    IFramebuffer* neutralTarget = &initializedFramebuffer;
    if (neutralTarget->GetWidth() != 800 || neutralTarget->GetHeight() != 600)
    {
        return Fail("neutral framebuffer size did not match the backend specification");
    }
    if (neutralTarget->GetNativeHandle() != static_cast<u32>(stableFramebuffer))
    {
        return Fail("neutral framebuffer handle did not expose the backend identity");
    }
    if (!neutralTarget->IsComplete())
    {
        return Fail("neutral framebuffer reported incomplete against the fake backend");
    }

    OpenGLDevice device(nullptr);

    // The state manager caches framebuffer bindings, so start untracked to
    // guarantee the bind reaches the driver.
    OpenGLStateManager::GetInstance().InvalidateState();
    g_lastBindTarget = 0;
    g_lastBoundFramebuffer = 0xFFFFFFFFu;
    g_bindFramebufferCalls = 0;

    device.BindFramebuffer(neutralTarget);
    if (g_lastBoundFramebuffer != stableFramebuffer)
    {
        return Fail("neutral bind of a real framebuffer object did not bind that framebuffer");
    }
    if (g_lastBindTarget != GL_FRAMEBUFFER)
    {
        return Fail("neutral bind did not target GL_FRAMEBUFFER");
    }
    if (g_bindFramebufferCalls != 1)
    {
        return Fail("neutral bind of a real object issued an unexpected number of framebuffer binds");
    }
    if (OpenGLStateManager::GetInstance().GetBoundFramebuffer(GL_DRAW_FRAMEBUFFER) !=
        stableFramebuffer)
    {
        return Fail("neutral bind of a real object did not become the draw framebuffer");
    }

    // Error-state symmetry: a failure recorded by an earlier call must be
    // cleared by a successful neutral bind.
    device.BindTexture(nullptr, 9999);
    if (device.GetLastError().empty())
    {
        return Fail("out-of-range texture slot did not record a device error");
    }
    device.BindFramebuffer(neutralTarget);
    if (!device.GetLastError().empty())
    {
        const std::string stale = "successful neutral bind left a stale device error: " +
                                  device.GetLastError();
        return Fail(stale.c_str());
    }

    // Null binds the default surface.
    OpenGLStateManager::GetInstance().InvalidateState();
    g_lastBoundFramebuffer = 0xFFFFFFFFu;
    device.BindFramebuffer(nullptr);
    if (g_lastBoundFramebuffer != 0)
    {
        return Fail("null neutral bind did not restore the default framebuffer");
    }
    if (OpenGLStateManager::GetInstance().GetBoundFramebuffer(GL_DRAW_FRAMEBUFFER) != 0)
    {
        return Fail("null neutral bind did not make the default framebuffer current");
    }

    // Every touched call site must route through the neutral method.
    Pyramid::Tests::TestGraphicsDevice testDevice;
    testDevice.shaderFactory = [] { return std::make_shared<Pyramid::Tests::TestShader>(); };

    // RenderTarget::Bind and Unbind.
    RenderTargetSpec neutralTargetSpec;
    neutralTargetSpec.width = 320;
    neutralTargetSpec.height = 240;
    Renderer::RenderTarget routedTarget(neutralTargetSpec);
    if (!routedTarget.Initialize(&testDevice))
    {
        return Fail("render target did not initialize against the fake graphics device");
    }

    u32 neutralBinds = testDevice.neutralFramebufferBinds;
    routedTarget.Bind();
    if (testDevice.neutralFramebufferBinds != neutralBinds + 1 ||
        testDevice.lastNeutralFramebuffer == nullptr)
    {
        return Fail("render target bind did not route through the neutral device method");
    }
    if (testDevice.boundFramebufferHandle == 0)
    {
        return Fail("render target bind did not bind a real target");
    }

    routedTarget.Unbind();
    if (testDevice.neutralFramebufferBinds != neutralBinds + 2 ||
        testDevice.lastNeutralFramebuffer != nullptr || testDevice.boundFramebufferHandle != 0)
    {
        return Fail("render target unbind did not restore the default surface neutrally");
    }

    // A null render-target command restores the default surface neutrally.
    Renderer::CommandBuffer targetCommand;
    targetCommand.Begin();
    targetCommand.SetRenderTarget(nullptr);
    targetCommand.End();
    neutralBinds = testDevice.neutralFramebufferBinds;
    targetCommand.Execute(&testDevice);
    if (testDevice.neutralFramebufferBinds != neutralBinds + 1 ||
        testDevice.lastNeutralFramebuffer != nullptr || testDevice.boundFramebufferHandle != 0)
    {
        return Fail("null render-target command did not restore the default surface neutrally");
    }

    // DeferredGeometryPass binds its G-buffer and restores the default surface.
    Renderer::DeferredGeometryPass geometryPass("DeferredGeometry", &testDevice, 128, 96);
    if (!geometryPass.GetGBuffer())
    {
        return Fail("deferred geometry pass did not create its G-buffer");
    }

    Renderer::CommandBuffer geometryCommand;
    geometryCommand.Begin();
    neutralBinds = testDevice.neutralFramebufferBinds;
    geometryPass.Begin(geometryCommand);
    if (testDevice.neutralFramebufferBinds != neutralBinds + 1 ||
        testDevice.lastNeutralFramebuffer != geometryPass.GetGBuffer().get())
    {
        return Fail("deferred geometry pass did not bind its G-buffer through the neutral method");
    }
    geometryCommand.End();
    geometryPass.End(geometryCommand);
    if (testDevice.lastNeutralFramebuffer != nullptr || testDevice.boundFramebufferHandle != 0)
    {
        return Fail("deferred geometry pass did not restore the default surface neutrally");
    }

    // RenderSystem restores the main surface through the neutral method after
    // every pass, in framebuffer-then-viewport order.
    Renderer::RenderSystem renderSystem;
    if (!renderSystem.Initialize(&testDevice))
    {
        return Fail("render system did not initialize against the fake graphics device");
    }
    if (!renderSystem.Resize(320, 240))
    {
        return Fail("render system did not accept a valid resize");
    }

    Scene scene;
    Camera camera;
    renderSystem.BeginFrame();
    neutralBinds = testDevice.neutralFramebufferBinds;
    renderSystem.Render(scene, camera);
    if (testDevice.neutralFramebufferBinds <= neutralBinds)
    {
        return Fail("render system did not restore the surface through the neutral method");
    }
    if (testDevice.lastNeutralFramebuffer != nullptr || testDevice.boundFramebufferHandle != 0)
    {
        return Fail("render system left a non-default framebuffer bound after its passes");
    }
    if (testDevice.viewportWidth != 320 || testDevice.viewportHeight != 240)
    {
        return Fail("render system did not restore the main viewport after its passes");
    }
    renderSystem.EndFrame();

    std::cout << "Framebuffer resize tests passed\n";
    return EXIT_SUCCESS;
}
