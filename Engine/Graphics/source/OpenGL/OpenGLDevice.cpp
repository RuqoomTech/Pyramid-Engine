#include <Pyramid/Graphics/OpenGL/OpenGLDevice.hpp>
#include "OpenGLDiagnostics.hpp"
#include <Pyramid/Util/Log.hpp>                      // For logging
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp> // For BufferUsage enum
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLIndexBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexArray.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLUniformBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLInstanceBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLShaderStorageBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/OpenGL/Shader/OpenGLShader.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLTexture.hpp>
#include <Pyramid/Graphics/Texture.hpp> // Added for ITexture2D factory methods
#include <glad/glad.h>

namespace Pyramid
{
    namespace
    {
        constexpr u32 kMaxTextureSlots = 32;

        GLenum ToOpenGLTopology(PrimitiveTopology topology)
        {
            switch (topology)
            {
            case PrimitiveTopology::Points:
                return GL_POINTS;
            case PrimitiveTopology::Lines:
                return GL_LINES;
            case PrimitiveTopology::LineStrip:
                return GL_LINE_STRIP;
            case PrimitiveTopology::Triangles:
                return GL_TRIANGLES;
            case PrimitiveTopology::TriangleStrip:
                return GL_TRIANGLE_STRIP;
            }

            return GL_TRIANGLES;
        }
    } // namespace


    OpenGLDevice::OpenGLDevice(Window *window)
        : m_window(window), m_initialized(false), m_deviceInfoCached(false)
    {
    }

    OpenGLDevice::~OpenGLDevice()
    {
        Shutdown();
    }

    bool OpenGLDevice::Initialize()
    {
        PYRAMID_LOG_INFO("Initializing OpenGL graphics device...");

        if (m_initialized)
        {
            return true;
        }

        if (!m_window)
        {
            m_lastError = "Cannot initialize OpenGL device: window is null";
            PYRAMID_LOG_ERROR(m_lastError);
            return false;
        }

        // Initialize the window only if it does not already exist.
        if (!m_window->GetHandle() && !m_window->Initialize())
        {
            m_lastError = "Failed to initialize window";
            PYRAMID_LOG_ERROR("Window initialization failed in OpenGLDevice::Initialize()");
            return false;
        }

        PYRAMID_LOG_INFO("Window ready, setting up OpenGL context...");

        // Make OpenGL context current
        m_window->MakeContextCurrent();
        OpenGLDiagnostics::ClearErrors();

        auto &stateManager = OpenGLStateManager::GetInstance();
        stateManager.InvalidateState();
        stateManager.ResetToDefaults();
        stateManager.SetViewport(0, 0, m_window->GetWidth(), m_window->GetHeight());

        OpenGLDiagnostics::Initialize();

        if (!OpenGLDiagnostics::CheckError("OpenGLDevice::Initialize", &m_lastError))
        {
            return false;
        }

        m_initialized = true;
        m_deviceInfoCached = false; // Force device info refresh
        m_lastError.clear();

        return true;
    }

    void OpenGLDevice::Shutdown()
    {
        if (m_initialized)
        {
            OpenGLDiagnostics::Shutdown();
            OpenGLStateManager::GetInstance().InvalidateState();
            OpenGLDiagnostics::ClearErrors();

            m_initialized = false;
            m_deviceInfoCached = false;
            m_deviceInfo.clear();
            m_lastError.clear();
        }
    }

    void OpenGLDevice::Clear(const Color &color)
    {
        OpenGLStateManager::GetInstance().SetClearColor(color.r, color.g, color.b, color.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        OpenGLDiagnostics::CheckError("OpenGLDevice::Clear", &m_lastError, false);
    }

    void OpenGLDevice::Present(bool vsync)
    {
        if (!m_window)
        {
            m_lastError = "Cannot present frame: window is null";
            PYRAMID_LOG_ERROR(m_lastError);
            return;
        }

        m_window->Present(vsync);
    }

    void OpenGLDevice::DrawIndexed(u32 count, PrimitiveTopology topology)
    {
        glDrawElements(ToOpenGLTopology(topology), count, GL_UNSIGNED_INT, nullptr);
        OpenGLDiagnostics::CheckError("OpenGLDevice::DrawIndexed", &m_lastError, false);
    }

    void OpenGLDevice::DrawIndexedInstanced(
        u32 indexCount,
        u32 instanceCount,
        PrimitiveTopology topology)
    {
        glDrawElementsInstanced(
            ToOpenGLTopology(topology),
            indexCount,
            GL_UNSIGNED_INT,
            nullptr,
            instanceCount);
        OpenGLDiagnostics::CheckError("OpenGLDevice::DrawIndexedInstanced", &m_lastError, false);
    }

    void OpenGLDevice::DrawArrays(
        u32 vertexCount,
        u32 firstVertex,
        PrimitiveTopology topology)
    {
        glDrawArrays(ToOpenGLTopology(topology), firstVertex, vertexCount);
        OpenGLDiagnostics::CheckError("OpenGLDevice::DrawArrays", &m_lastError, false);
    }

    void OpenGLDevice::DrawArraysInstanced(
        u32 vertexCount,
        u32 instanceCount,
        u32 firstVertex,
        PrimitiveTopology topology)
    {
        glDrawArraysInstanced(
            ToOpenGLTopology(topology),
            firstVertex,
            vertexCount,
            instanceCount);
        OpenGLDiagnostics::CheckError("OpenGLDevice::DrawArraysInstanced", &m_lastError, false);
    }

    void OpenGLDevice::SetViewport(u32 x, u32 y, u32 width, u32 height)
    {
        OpenGLStateManager::GetInstance().SetViewport(x, y, width, height);
    }

    std::shared_ptr<IVertexBuffer> OpenGLDevice::CreateVertexBuffer()
    {
        return std::make_shared<OpenGLVertexBuffer>();
    }

    std::shared_ptr<IIndexBuffer> OpenGLDevice::CreateIndexBuffer()
    {
        return std::make_shared<OpenGLIndexBuffer>();
    }

    std::shared_ptr<IVertexArray> OpenGLDevice::CreateVertexArray()
    {
        return std::make_shared<OpenGLVertexArray>();
    }

    std::shared_ptr<IShader> OpenGLDevice::CreateShader()
    {
        return std::make_shared<OpenGLShader>();
    }

    std::shared_ptr<ITexture2D> OpenGLDevice::CreateTexture2D(const TextureSpecification &specification, const void *data)
    {
        return ITexture2D::Create(specification, data);
    }

    std::shared_ptr<ITexture2D> OpenGLDevice::CreateTexture2D(const std::string &filepath, bool srgb, bool generateMips)
    {
        return ITexture2D::Create(filepath, srgb, generateMips);
    }

    std::shared_ptr<IUniformBuffer> OpenGLDevice::CreateUniformBuffer(size_t size, BufferUsage usage)
    {
        auto buffer = std::make_shared<OpenGLUniformBuffer>();
        if (buffer->Initialize(size, usage))
        {
            return buffer;
        }
        return nullptr;
    }

    std::shared_ptr<IInstanceBuffer> OpenGLDevice::CreateInstanceBuffer()
    {
        return std::make_shared<OpenGLInstanceBuffer>();
    }

    std::shared_ptr<IShaderStorageBuffer> OpenGLDevice::CreateShaderStorageBuffer()
    {
        return std::make_shared<OpenGLShaderStorageBuffer>();
    }

    void OpenGLDevice::EnableBlend(bool enable)
    {
        OpenGLStateManager::GetInstance().EnableBlend(enable);
    }

    void OpenGLDevice::SetBlendFunc(u32 sfactor, u32 dfactor)
    {
        OpenGLStateManager::GetInstance().SetBlendFunc(static_cast<GLenum>(sfactor), static_cast<GLenum>(dfactor));
    }

    void OpenGLDevice::EnableDepthTest(bool enable)
    {
        OpenGLStateManager::GetInstance().EnableDepthTest(enable);
    }

    void OpenGLDevice::SetDepthFunc(u32 func)
    {
        OpenGLStateManager::GetInstance().SetDepthFunc(static_cast<GLenum>(func));
    }

    void OpenGLDevice::EnableDepthClamp(bool enable)
    {
        OpenGLStateManager::GetInstance().EnableDepthClamp(enable);
    }

    void OpenGLDevice::EnableCullFace(bool enable)
    {
        OpenGLStateManager::GetInstance().EnableCullFace(enable);
    }

    void OpenGLDevice::SetCullFace(u32 mode)
    {
        OpenGLStateManager::GetInstance().SetCullFace(static_cast<GLenum>(mode));
    }

    void OpenGLDevice::SetClearColor(f32 r, f32 g, f32 b, f32 a)
    {
        OpenGLStateManager::GetInstance().SetClearColor(r, g, b, a);
    }

    u32 OpenGLDevice::GetStateChangeCount() const
    {
        return OpenGLStateManager::GetInstance().GetStateChangeCount();
    }

    void OpenGLDevice::ResetStateChangeCount()
    {
        OpenGLStateManager::GetInstance().ResetStateChangeCount();
    }

    std::string OpenGLDevice::GetDeviceInfo() const
    {
        if (!m_deviceInfoCached)
        {
            if (m_initialized)
            {
                const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
                const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
                const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
                const char *glslVersion = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

                m_deviceInfo = std::string("OpenGL Device Info:\n") +
                               "  Vendor: " + (vendor ? vendor : "Unknown") + "\n" +
                               "  Renderer: " + (renderer ? renderer : "Unknown") + "\n" +
                               "  Version: " + (version ? version : "Unknown") + "\n" +
                               "  GLSL Version: " + (glslVersion ? glslVersion : "Unknown");
            }
            else
            {
                m_deviceInfo = "OpenGL Device not initialized";
            }
            m_deviceInfoCached = true;
        }
        return m_deviceInfo;
    }

    bool OpenGLDevice::IsValid() const
    {
        return m_initialized && m_window && m_window->GetHandle();
    }

    std::string OpenGLDevice::GetLastError() const
    {
        return m_lastError;
    }

    void OpenGLDevice::SetWireframeMode(bool enable)
    {
        OpenGLStateManager::GetInstance().SetPolygonMode(enable ? GL_LINE : GL_FILL);
        OpenGLDiagnostics::CheckError("OpenGLDevice::SetWireframeMode", &m_lastError, false);
    }

    void OpenGLDevice::SetPolygonMode(u32 mode)
    {
        OpenGLStateManager::GetInstance().SetPolygonMode(static_cast<GLenum>(mode));
        OpenGLDiagnostics::CheckError("OpenGLDevice::SetPolygonMode", &m_lastError, false);
    }

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

    void OpenGLDevice::BindShader(IShader *shader)
    {
        if (shader)
        {
            shader->Bind();
        }
        else
        {
            OpenGLStateManager::GetInstance().UseProgram(0);
        }
    }

    void OpenGLDevice::BindVertexArray(IVertexArray *vao)
    {
        if (vao)
        {
            vao->Bind();
        }
        else
        {
            OpenGLStateManager::GetInstance().BindVertexArray(0);
        }
    }

    void OpenGLDevice::BindTexture(ITexture2D *texture, u32 slot)
    {
        if (slot >= kMaxTextureSlots)
        {
            m_lastError = "Texture slot out of range: " + std::to_string(slot);
            PYRAMID_LOG_ERROR(m_lastError);
            return;
        }

        if (texture)
        {
            // Cast to OpenGL texture to access OpenGL-specific binding
            OpenGLTexture2D *glTexture = dynamic_cast<OpenGLTexture2D *>(texture);
            if (glTexture)
            {
                OpenGLStateManager::GetInstance().BindTextureUnit(slot, GL_TEXTURE_2D, glTexture->GetRendererID());
            }
            else
            {
                m_lastError = "Failed to bind texture: invalid OpenGL texture";
                PYRAMID_LOG_ERROR(m_lastError);
            }
        }
        else
        {
            OpenGLStateManager::GetInstance().BindTextureUnit(slot, GL_TEXTURE_2D, 0);
        }
    }

    void OpenGLDevice::BindNativeTexture(u32 textureId, u32 slot, u32 target)
    {
        if (slot >= kMaxTextureSlots)
        {
            m_lastError = "Native texture slot out of range: " + std::to_string(slot);
            PYRAMID_LOG_ERROR(m_lastError);
            return;
        }

        OpenGLStateManager::GetInstance().BindTextureUnit(slot, static_cast<GLenum>(target), textureId);
    }

    void OpenGLDevice::SetTextureBorderColor(u32 textureId, u32 target, f32 r, f32 g, f32 b, f32 a)
    {
        const GLfloat borderColor[] = {r, g, b, a};
        OpenGLStateManager::GetInstance().BindTextureUnit(0, static_cast<GLenum>(target), textureId);
        glTexParameterfv(static_cast<GLenum>(target), GL_TEXTURE_BORDER_COLOR, borderColor);
        OpenGLDiagnostics::CheckError("OpenGLDevice::SetTextureBorderColor", &m_lastError);
    }

    void OpenGLDevice::BindUniformBuffer(IUniformBuffer *buffer, u32 bindingPoint)
    {
        if (buffer)
        {
            buffer->Bind(bindingPoint);
        }
        else
        {
            glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, 0);
            OpenGLStateManager::GetInstance().BindBuffer(GL_UNIFORM_BUFFER, 0);
        }
    }

    void OpenGLDevice::ClearBuffers(u32 clearMask)
    {
        glClear(static_cast<GLbitfield>(clearMask));
        OpenGLDiagnostics::CheckError("OpenGLDevice::ClearBuffers", &m_lastError, false);
    }

} // namespace Pyramid
