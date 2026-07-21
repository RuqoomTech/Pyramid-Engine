#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Util/Log.hpp>
#include <algorithm>
#include <fstream>
#include <unordered_set>

namespace Pyramid
{

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpec& spec)
        : m_spec(spec)
    {
        m_colorAttachmentTextures.reserve(8);
        m_colorAttachmentRenderbuffers.reserve(8);
    }

    OpenGLFramebuffer::~OpenGLFramebuffer()
    {
        ReleaseResources();
    }

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

        std::vector<GLenum> drawBuffers;
        for (const auto& attachment : m_spec.attachments)
        {
            if (attachment.type == FramebufferAttachmentType::Color)
            {
                drawBuffers.push_back(GetColorAttachmentEnum(attachment.colorAttachmentIndex));
            }
        }

        if (!drawBuffers.empty())
        {
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        }
        else
        {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }

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

        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);

        m_initialized = true;
        PYRAMID_LOG_INFO(
            "Framebuffer created successfully: ", m_spec.width, "x", m_spec.height,
            " with ", m_spec.attachments.size(), " attachments");
        return true;
    }

    void OpenGLFramebuffer::Bind() const
    {
        if (!m_initialized)
        {
            PYRAMID_LOG_ERROR("Cannot bind an uninitialized framebuffer");
            return;
        }

        if (m_spec.swapChainTarget)
        {
            OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        else
        {
            OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
        }

        if (FramebufferUtils::IsValidExtent(m_spec.width, m_spec.height))
        {
            OpenGLStateManager::GetInstance().SetViewport(
                0, 0, static_cast<i32>(m_spec.width), static_cast<i32>(m_spec.height));
        }
    }

    void OpenGLFramebuffer::Unbind() const
    {
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::Clear(f32 r, f32 g, f32 b, f32 a) const
    {
        if (!m_initialized)
        {
            PYRAMID_LOG_ERROR("Cannot clear an uninitialized framebuffer");
            return;
        }

        Bind();
        OpenGLStateManager::GetInstance().SetClearColor(r, g, b, a);

        GLbitfield clearMask = 0;
        for (const auto& attachment : m_spec.attachments)
        {
            switch (attachment.type)
            {
            case FramebufferAttachmentType::Color:
                clearMask |= GL_COLOR_BUFFER_BIT;
                break;
            case FramebufferAttachmentType::Depth:
                clearMask |= GL_DEPTH_BUFFER_BIT;
                break;
            case FramebufferAttachmentType::Stencil:
                clearMask |= GL_STENCIL_BUFFER_BIT;
                break;
            case FramebufferAttachmentType::DepthStencil:
                clearMask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
                break;
            }
        }

        if (clearMask != 0)
        {
            glClear(clearMask);
        }
    }

    void OpenGLFramebuffer::ClearAttachment(u32 attachmentIndex, const void* value) const
    {
        if (!m_initialized)
        {
            PYRAMID_LOG_ERROR("Cannot clear an attachment on an uninitialized framebuffer");
            return;
        }

        if (!value)
        {
            PYRAMID_LOG_ERROR("Cannot clear framebuffer attachment with a null value");
            return;
        }

        if (attachmentIndex >= m_spec.attachments.size())
        {
            PYRAMID_LOG_ERROR("Invalid attachment index: ", attachmentIndex);
            return;
        }

        Bind();

        const auto& attachment = m_spec.attachments[attachmentIndex];
        if (attachment.type != FramebufferAttachmentType::Color)
        {
            PYRAMID_LOG_WARN("ClearAttachment currently supports color attachments only");
            return;
        }

        if (attachment.dataType == GL_FLOAT || attachment.dataType == GL_HALF_FLOAT)
        {
            glClearBufferfv(
                GL_COLOR, attachment.colorAttachmentIndex,
                static_cast<const GLfloat*>(value));
        }
        else if (attachment.dataType == GL_INT)
        {
            glClearBufferiv(
                GL_COLOR, attachment.colorAttachmentIndex,
                static_cast<const GLint*>(value));
        }
        else
        {
            glClearBufferuiv(
                GL_COLOR, attachment.colorAttachmentIndex,
                static_cast<const GLuint*>(value));
        }
    }

    bool OpenGLFramebuffer::Resize(u32 width, u32 height)
    {
        if (!FramebufferUtils::IsValidExtent(width, height))
        {
            PYRAMID_LOG_WARN(
                "Ignoring framebuffer resize to non-renderable extent ",
                width, "x", height);
            return false;
        }

        if (m_spec.width == width && m_spec.height == height)
        {
            return true;
        }

        if (m_spec.swapChainTarget)
        {
            m_spec.width = width;
            m_spec.height = height;
            return true;
        }

        FramebufferSpec replacementSpec = m_spec;
        replacementSpec.width = width;
        replacementSpec.height = height;

        OpenGLFramebuffer replacement(replacementSpec);
        if (!replacement.Initialize())
        {
            PYRAMID_LOG_ERROR(
                "Framebuffer resize failed; preserving existing ",
                m_spec.width, "x", m_spec.height, " attachments");
            return false;
        }

        SwapState(replacement);
        PYRAMID_LOG_INFO("Framebuffer resized to ", width, "x", height);
        return true;
    }

    void OpenGLFramebuffer::Invalidate()
    {
        ReleaseResources();
    }

    void OpenGLFramebuffer::ReleaseResources()
    {
        if (m_framebufferID != 0)
        {
            glDeleteFramebuffers(1, &m_framebufferID);
            m_framebufferID = 0;
        }

        if (!m_colorAttachmentTextures.empty())
        {
            glDeleteTextures(
                static_cast<GLsizei>(m_colorAttachmentTextures.size()),
                m_colorAttachmentTextures.data());
            m_colorAttachmentTextures.clear();
        }

        if (!m_colorAttachmentRenderbuffers.empty())
        {
            glDeleteRenderbuffers(
                static_cast<GLsizei>(m_colorAttachmentRenderbuffers.size()),
                m_colorAttachmentRenderbuffers.data());
            m_colorAttachmentRenderbuffers.clear();
        }

        if (m_depthAttachmentTexture != 0)
        {
            glDeleteTextures(1, &m_depthAttachmentTexture);
            m_depthAttachmentTexture = 0;
        }
        if (m_depthAttachmentRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &m_depthAttachmentRenderbuffer);
            m_depthAttachmentRenderbuffer = 0;
        }
        if (m_stencilAttachmentTexture != 0)
        {
            glDeleteTextures(1, &m_stencilAttachmentTexture);
            m_stencilAttachmentTexture = 0;
        }
        if (m_stencilAttachmentRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &m_stencilAttachmentRenderbuffer);
            m_stencilAttachmentRenderbuffer = 0;
        }
        if (m_depthStencilAttachmentTexture != 0)
        {
            glDeleteTextures(1, &m_depthStencilAttachmentTexture);
            m_depthStencilAttachmentTexture = 0;
        }
        if (m_depthStencilAttachmentRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &m_depthStencilAttachmentRenderbuffer);
            m_depthStencilAttachmentRenderbuffer = 0;
        }

        m_initialized = false;
    }

    void OpenGLFramebuffer::SwapState(OpenGLFramebuffer& other) noexcept
    {
        using std::swap;
        swap(m_spec, other.m_spec);
        swap(m_framebufferID, other.m_framebufferID);
        swap(m_colorAttachmentTextures, other.m_colorAttachmentTextures);
        swap(m_colorAttachmentRenderbuffers, other.m_colorAttachmentRenderbuffers);
        swap(m_depthAttachmentTexture, other.m_depthAttachmentTexture);
        swap(m_depthAttachmentRenderbuffer, other.m_depthAttachmentRenderbuffer);
        swap(m_stencilAttachmentTexture, other.m_stencilAttachmentTexture);
        swap(m_stencilAttachmentRenderbuffer, other.m_stencilAttachmentRenderbuffer);
        swap(m_depthStencilAttachmentTexture, other.m_depthStencilAttachmentTexture);
        swap(m_depthStencilAttachmentRenderbuffer, other.m_depthStencilAttachmentRenderbuffer);
        swap(m_initialized, other.m_initialized);
    }

    GLuint OpenGLFramebuffer::GetColorAttachmentTexture(u32 index) const
    {
        if (index >= m_colorAttachmentTextures.size())
        {
            PYRAMID_LOG_ERROR("Color attachment index out of range: ", index);
            return 0;
        }
        return m_colorAttachmentTextures[index];
    }

    GLuint OpenGLFramebuffer::GetDepthAttachmentTexture() const
    {
        return m_depthStencilAttachmentTexture != 0 ? m_depthStencilAttachmentTexture : m_depthAttachmentTexture;
    }

    GLuint OpenGLFramebuffer::GetStencilAttachmentTexture() const
    {
        return m_depthStencilAttachmentTexture != 0 ? m_depthStencilAttachmentTexture : m_stencilAttachmentTexture;
    }

    void OpenGLFramebuffer::SetDrawBuffers(const std::vector<u32> &colorAttachments) const
    {
        Bind();

        std::vector<GLenum> drawBuffers;
        drawBuffers.reserve(colorAttachments.size());

        for (u32 attachment : colorAttachments)
        {
            drawBuffers.push_back(GetColorAttachmentEnum(attachment));
        }

        if (!drawBuffers.empty())
        {
            glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
        }
        else
        {
            glDrawBuffer(GL_NONE);
        }
    }

    void OpenGLFramebuffer::SetReadBuffer(u32 colorAttachment) const
    {
        Bind();
        glReadBuffer(GetColorAttachmentEnum(colorAttachment));
    }

    void OpenGLFramebuffer::BlitTo(const OpenGLFramebuffer &target,
                                   u32 srcX0, u32 srcY0, u32 srcX1, u32 srcY1,
                                   u32 dstX0, u32 dstY0, u32 dstX1, u32 dstY1,
                                   GLbitfield mask, GLenum filter) const
    {
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_READ_FRAMEBUFFER, m_framebufferID);
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_DRAW_FRAMEBUFFER, target.GetFramebufferID());

        glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
                          dstX0, dstY0, dstX1, dstY1,
                          mask, filter);

        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::ResolveMultisampleTo(const OpenGLFramebuffer &target) const
    {
        if (!IsMultisampled())
        {
            PYRAMID_LOG_WARN("Source framebuffer is not multisampled, using regular blit");
        }

        BlitTo(target, 0, 0, m_spec.width, m_spec.height,
               0, 0, target.GetWidth(), target.GetHeight(),
               GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
               GL_NEAREST);
    }

    void OpenGLFramebuffer::ResolveMultisampleTo(const OpenGLFramebuffer &target, u32 colorAttachment) const
    {
        if (!IsMultisampled())
        {
            PYRAMID_LOG_WARN("Source framebuffer is not multisampled, using regular blit");
        }

        // Set specific read and draw buffers for the color attachment
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_READ_FRAMEBUFFER, m_framebufferID);
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_DRAW_FRAMEBUFFER, target.GetFramebufferID());

        glReadBuffer(GetColorAttachmentEnum(colorAttachment));
        glDrawBuffer(GetColorAttachmentEnum(colorAttachment));

        glBlitFramebuffer(0, 0, m_spec.width, m_spec.height,
                          0, 0, target.GetWidth(), target.GetHeight(),
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);

        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::BlitColorAttachmentTo(const OpenGLFramebuffer &target, u32 srcAttachment, u32 dstAttachment,
                                                  u32 srcX0, u32 srcY0, u32 srcX1, u32 srcY1,
                                                  u32 dstX0, u32 dstY0, u32 dstX1, u32 dstY1,
                                                  GLenum filter) const
    {
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_READ_FRAMEBUFFER, m_framebufferID);
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_DRAW_FRAMEBUFFER, target.GetFramebufferID());

        glReadBuffer(GetColorAttachmentEnum(srcAttachment));
        glDrawBuffer(GetColorAttachmentEnum(dstAttachment));

        glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
                          dstX0, dstY0, dstX1, dstY1,
                          GL_COLOR_BUFFER_BIT, filter);

        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::BlitDepthTo(const OpenGLFramebuffer &target,
                                        u32 srcX0, u32 srcY0, u32 srcX1, u32 srcY1,
                                        u32 dstX0, u32 dstY0, u32 dstX1, u32 dstY1) const
    {
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_READ_FRAMEBUFFER, m_framebufferID);
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_DRAW_FRAMEBUFFER, target.GetFramebufferID());

        glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1,
                          dstX0, dstY0, dstX1, dstY1,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    bool OpenGLFramebuffer::IsComplete() const
    {
        if (!m_initialized)
        {
            return false;
        }

        if (m_spec.swapChainTarget)
        {
            return true;
        }

        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, m_framebufferID);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        OpenGLStateManager::GetInstance().BindFramebuffer(GL_FRAMEBUFFER, 0);

        return status == GL_FRAMEBUFFER_COMPLETE;
    }

    u32 OpenGLFramebuffer::GetColorAttachmentCount() const
    {
        u32 count = 0;
        for (const auto &attachment : m_spec.attachments)
        {
            if (attachment.type == FramebufferAttachmentType::Color)
            {
                count++;
            }
        }
        return count;
    }

    void OpenGLFramebuffer::SaveColorAttachmentToFile(u32 attachmentIndex, const std::string &filepath) const
    {
        if (attachmentIndex >= m_colorAttachmentTextures.size())
        {
            PYRAMID_LOG_ERROR("Color attachment index out of range: ", attachmentIndex);
            return;
        }

        // Read pixels from the attachment
        auto pixels = ReadColorAttachmentPixels(attachmentIndex);
        if (pixels.empty())
        {
            PYRAMID_LOG_ERROR("Failed to read pixels from color attachment ", attachmentIndex);
            return;
        }

        // Save to file (simple PPM format for now)
        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            PYRAMID_LOG_ERROR("Failed to open file for writing: ", filepath);
            return;
        }

        // Write PPM header
        file << "P6\n"
             << m_spec.width << " " << m_spec.height << "\n255\n";

        // Write pixel data (assuming RGBA format, convert to RGB)
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            file.write(reinterpret_cast<const char *>(&pixels[i]), 3); // Write RGB, skip A
        }

        file.close();
        PYRAMID_LOG_INFO("Saved framebuffer attachment to: ", filepath);
    }

    std::vector<u8> OpenGLFramebuffer::ReadColorAttachmentPixels(u32 attachmentIndex) const
    {
        if (attachmentIndex >= m_colorAttachmentTextures.size())
        {
            PYRAMID_LOG_ERROR("Color attachment index out of range: ", attachmentIndex);
            return {};
        }

        Bind();
        SetReadBuffer(attachmentIndex);

        std::vector<u8> pixels(m_spec.width * m_spec.height * 4); // Assuming RGBA
        glReadPixels(0, 0, m_spec.width, m_spec.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        return pixels;
    }

    void OpenGLFramebuffer::CreateAttachments()
    {
        for (const auto &attachmentSpec : m_spec.attachments)
        {
            switch (attachmentSpec.type)
            {
            case FramebufferAttachmentType::Color:
                CreateColorAttachment(attachmentSpec);
                break;
            case FramebufferAttachmentType::Depth:
                CreateDepthAttachment(attachmentSpec);
                break;
            case FramebufferAttachmentType::Stencil:
                CreateStencilAttachment(attachmentSpec);
                break;
            case FramebufferAttachmentType::DepthStencil:
                CreateDepthStencilAttachment(attachmentSpec);
                break;
            }
        }
    }

    void OpenGLFramebuffer::CreateColorAttachment(const FramebufferAttachmentSpec &spec)
    {
        GLuint texture = CreateTexture2D(m_spec.width, m_spec.height, spec);

        if (texture == 0)
        {
            PYRAMID_LOG_ERROR("Failed to create color attachment texture");
            return;
        }

        // Ensure we have enough space in the vector
        while (m_colorAttachmentTextures.size() <= spec.colorAttachmentIndex)
        {
            m_colorAttachmentTextures.push_back(0);
        }

        m_colorAttachmentTextures[spec.colorAttachmentIndex] = texture;
        
        // Use correct texture target based on multisampling
        GLenum textureTarget = (spec.multisampled && spec.samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        AttachTexture2D(texture, GetColorAttachmentEnum(spec.colorAttachmentIndex), textureTarget);

        PYRAMID_LOG_DEBUG("Created color attachment ", spec.colorAttachmentIndex, " with texture ID ", texture);
    }

    void OpenGLFramebuffer::CreateDepthAttachment(const FramebufferAttachmentSpec &spec)
    {
        GLuint texture = CreateTexture2D(m_spec.width, m_spec.height, spec);

        if (texture == 0)
        {
            PYRAMID_LOG_ERROR("Failed to create depth attachment texture");
            return;
        }

        m_depthAttachmentTexture = texture;
        
        // Use correct texture target based on multisampling
        GLenum textureTarget = (spec.multisampled && spec.samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        AttachTexture2D(texture, GL_DEPTH_ATTACHMENT, textureTarget);

        PYRAMID_LOG_DEBUG("Created depth attachment with texture ID ", texture);
    }

    void OpenGLFramebuffer::CreateStencilAttachment(const FramebufferAttachmentSpec &spec)
    {
        GLuint texture = CreateTexture2D(m_spec.width, m_spec.height, spec);

        if (texture == 0)
        {
            PYRAMID_LOG_ERROR("Failed to create stencil attachment texture");
            return;
        }

        m_stencilAttachmentTexture = texture;
        
        // Use correct texture target based on multisampling
        GLenum textureTarget = (spec.multisampled && spec.samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        AttachTexture2D(texture, GL_STENCIL_ATTACHMENT, textureTarget);

        PYRAMID_LOG_DEBUG("Created stencil attachment with texture ID ", texture);
    }

    void OpenGLFramebuffer::CreateDepthStencilAttachment(const FramebufferAttachmentSpec &spec)
    {
        GLuint texture = CreateTexture2D(m_spec.width, m_spec.height, spec);

        if (texture == 0)
        {
            PYRAMID_LOG_ERROR("Failed to create depth-stencil attachment texture");
            return;
        }

        m_depthStencilAttachmentTexture = texture;
        
        // Use correct texture target based on multisampling
        GLenum textureTarget = (spec.multisampled && spec.samples > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        AttachTexture2D(texture, GL_DEPTH_STENCIL_ATTACHMENT, textureTarget);

        PYRAMID_LOG_DEBUG("Created depth-stencil attachment with texture ID ", texture);
    }

    GLuint OpenGLFramebuffer::CreateTexture2D(
        u32 width, u32 height, const FramebufferAttachmentSpec& spec)
    {
        GLuint texture = 0;
        glGenTextures(1, &texture);
        if (texture == 0)
        {
            return 0;
        }

        const bool multisampled = spec.multisampled && spec.samples > 1;
        const GLenum target = multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        glBindTexture(target, texture);

        if (multisampled)
        {
            glTexImage2DMultisample(
                GL_TEXTURE_2D_MULTISAMPLE,
                static_cast<GLsizei>(spec.samples),
                spec.internalFormat,
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height),
                GL_TRUE);
        }
        else
        {
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                static_cast<GLint>(spec.internalFormat),
                static_cast<GLsizei>(width),
                static_cast<GLsizei>(height),
                0,
                spec.format,
                spec.dataType,
                nullptr);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(spec.minFilter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(spec.magFilter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(spec.wrapS));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(spec.wrapT));
        }

        glBindTexture(target, 0);
        return texture;
    }

    GLuint OpenGLFramebuffer::CreateRenderbuffer(u32 width, u32 height, GLenum internalFormat,
                                                 bool multisampled, u32 samples)
    {
        GLuint renderbuffer;
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);

        if (multisampled && samples > 1)
        {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internalFormat, width, height);
        }
        else
        {
            glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, width, height);
        }

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        return renderbuffer;
    }

    void OpenGLFramebuffer::AttachTexture2D(GLuint texture, GLenum attachment, GLenum textureTarget, u32 mipLevel)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textureTarget, texture, mipLevel);
    }

    void OpenGLFramebuffer::AttachRenderbuffer(GLuint renderbuffer, GLenum attachment)
    {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, renderbuffer);
    }

    GLenum OpenGLFramebuffer::GetColorAttachmentEnum(u32 index)
    {
        return GL_COLOR_ATTACHMENT0 + index;
    }

    std::string OpenGLFramebuffer::GetFramebufferStatusString(GLenum status)
    {
        switch (status)
        {
        case GL_FRAMEBUFFER_COMPLETE:
            return "Complete";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "Incomplete attachment";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "Missing attachment";
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            return "Incomplete draw buffer";
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            return "Incomplete read buffer";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "Unsupported";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "Incomplete multisample";
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            return "Incomplete layer targets";
        default:
            return "Unknown error";
        }
    }

    // FramebufferUtils Implementation
    namespace FramebufferUtils
    {

        FramebufferSpec CreateColorOnlySpec(u32 width, u32 height, GLenum format)
        {
            FramebufferSpec spec;
            spec.width = width;
            spec.height = height;

            FramebufferAttachmentSpec colorAttachment;
            colorAttachment.type = FramebufferAttachmentType::Color;
            colorAttachment.internalFormat = format;
            colorAttachment.format = (format == GL_RGBA8) ? GL_RGBA : GL_RGB;
            colorAttachment.dataType = GL_UNSIGNED_BYTE;
            colorAttachment.colorAttachmentIndex = 0;

            spec.attachments.push_back(colorAttachment);
            return spec;
        }

        FramebufferSpec CreateColorDepthSpec(u32 width, u32 height, GLenum colorFormat)
        {
            FramebufferSpec spec;
            spec.width = width;
            spec.height = height;

            // Color attachment
            FramebufferAttachmentSpec colorAttachment;
            colorAttachment.type = FramebufferAttachmentType::Color;
            colorAttachment.internalFormat = colorFormat;
            colorAttachment.format = (colorFormat == GL_RGBA8) ? GL_RGBA : GL_RGB;
            colorAttachment.dataType = GL_UNSIGNED_BYTE;
            colorAttachment.colorAttachmentIndex = 0;

            // Depth attachment
            FramebufferAttachmentSpec depthAttachment;
            depthAttachment.type = FramebufferAttachmentType::Depth;
            depthAttachment.internalFormat = GL_DEPTH_COMPONENT24;
            depthAttachment.format = GL_DEPTH_COMPONENT;
            depthAttachment.dataType = GL_FLOAT;

            spec.attachments.push_back(colorAttachment);
            spec.attachments.push_back(depthAttachment);
            return spec;
        }

        FramebufferSpec CreateGBufferSpec(u32 width, u32 height)
        {
            FramebufferSpec spec;
            spec.width = width;
            spec.height = height;

            // G-Buffer layout for deferred rendering:
            // RT0: Albedo (RGB) + Metallic (A)
            FramebufferAttachmentSpec albedoMetallic;
            albedoMetallic.type = FramebufferAttachmentType::Color;
            albedoMetallic.internalFormat = GL_RGBA8;
            albedoMetallic.format = GL_RGBA;
            albedoMetallic.dataType = GL_UNSIGNED_BYTE;
            albedoMetallic.colorAttachmentIndex = 0;

            // RT1: Normal (RGB) + Roughness (A)
            FramebufferAttachmentSpec normalRoughness;
            normalRoughness.type = FramebufferAttachmentType::Color;
            normalRoughness.internalFormat = GL_RGBA16F;
            normalRoughness.format = GL_RGBA;
            normalRoughness.dataType = GL_HALF_FLOAT;
            normalRoughness.colorAttachmentIndex = 1;

            // RT2: Motion Vectors (RG) + AO (B) + Material ID (A)
            FramebufferAttachmentSpec motionAO;
            motionAO.type = FramebufferAttachmentType::Color;
            motionAO.internalFormat = GL_RGBA16F;
            motionAO.format = GL_RGBA;
            motionAO.dataType = GL_HALF_FLOAT;
            motionAO.colorAttachmentIndex = 2;

            // RT3: Emissive (RGB) + Flags (A)
            FramebufferAttachmentSpec emissive;
            emissive.type = FramebufferAttachmentType::Color;
            emissive.internalFormat = GL_RGBA16F;
            emissive.format = GL_RGBA;
            emissive.dataType = GL_HALF_FLOAT;
            emissive.colorAttachmentIndex = 3;

            // Depth-Stencil
            FramebufferAttachmentSpec depthStencil;
            depthStencil.type = FramebufferAttachmentType::DepthStencil;
            depthStencil.internalFormat = GL_DEPTH24_STENCIL8;
            depthStencil.format = GL_DEPTH_STENCIL;
            depthStencil.dataType = GL_UNSIGNED_INT_24_8;

            spec.attachments.push_back(albedoMetallic);
            spec.attachments.push_back(normalRoughness);
            spec.attachments.push_back(motionAO);
            spec.attachments.push_back(emissive);
            spec.attachments.push_back(depthStencil);

            return spec;
        }

        FramebufferSpec CreateShadowMapSpec(u32 width, u32 height)
        {
            FramebufferSpec spec;
            spec.width = width;
            spec.height = height;

            // Depth-only for shadow mapping
            FramebufferAttachmentSpec depthAttachment;
            depthAttachment.type = FramebufferAttachmentType::Depth;
            depthAttachment.internalFormat = GL_DEPTH_COMPONENT32F;
            depthAttachment.format = GL_DEPTH_COMPONENT;
            depthAttachment.dataType = GL_FLOAT;
            depthAttachment.minFilter = GL_LINEAR;
            depthAttachment.magFilter = GL_LINEAR;
            depthAttachment.wrapS = GL_CLAMP_TO_BORDER;
            depthAttachment.wrapT = GL_CLAMP_TO_BORDER;

            spec.attachments.push_back(depthAttachment);
            return spec;
        }

        FramebufferSpec CreateHDRSpec(u32 width, u32 height)
        {
            FramebufferSpec spec;
            spec.width = width;
            spec.height = height;

            // HDR color attachment
            FramebufferAttachmentSpec hdrColor;
            hdrColor.type = FramebufferAttachmentType::Color;
            hdrColor.internalFormat = GL_RGBA16F;
            hdrColor.format = GL_RGBA;
            hdrColor.dataType = GL_HALF_FLOAT;
            hdrColor.colorAttachmentIndex = 0;

            // Depth attachment
            FramebufferAttachmentSpec depthAttachment;
            depthAttachment.type = FramebufferAttachmentType::Depth;
            depthAttachment.internalFormat = GL_DEPTH_COMPONENT24;
            depthAttachment.format = GL_DEPTH_COMPONENT;
            depthAttachment.dataType = GL_FLOAT;

            spec.attachments.push_back(hdrColor);
            spec.attachments.push_back(depthAttachment);
            return spec;
        }

        FramebufferSpec CreateMultisampledSpec(u32 width, u32 height, u32 samples, GLenum format)
        {
            FramebufferSpec spec;
            spec.width = width;
            spec.height = height;
            spec.samples = samples;

            // Multisampled color attachment
            FramebufferAttachmentSpec colorAttachment;
            colorAttachment.type = FramebufferAttachmentType::Color;
            colorAttachment.internalFormat = format;
            colorAttachment.format = (format == GL_RGBA8) ? GL_RGBA : GL_RGB;
            colorAttachment.dataType = GL_UNSIGNED_BYTE;
            colorAttachment.multisampled = true;
            colorAttachment.samples = samples;
            colorAttachment.colorAttachmentIndex = 0;

            // Multisampled depth attachment
            FramebufferAttachmentSpec depthAttachment;
            depthAttachment.type = FramebufferAttachmentType::Depth;
            depthAttachment.internalFormat = GL_DEPTH_COMPONENT24;
            depthAttachment.format = GL_DEPTH_COMPONENT;
            depthAttachment.dataType = GL_FLOAT;
            depthAttachment.multisampled = true;
            depthAttachment.samples = samples;

            spec.attachments.push_back(colorAttachment);
            spec.attachments.push_back(depthAttachment);
            return spec;
        }

        bool IsValidExtent(u32 width, u32 height)
        {
            return width > 0 && height > 0;
        }

        bool ValidateSpecStructure(const FramebufferSpec& spec)
        {
            if (!IsValidExtent(spec.width, spec.height))
            {
                PYRAMID_LOG_ERROR(
                    "Invalid framebuffer dimensions: ", spec.width, "x", spec.height);
                return false;
            }

            if (spec.swapChainTarget)
            {
                return true;
            }

            if (spec.attachments.empty())
            {
                PYRAMID_LOG_ERROR("Framebuffer must have at least one attachment");
                return false;
            }

            if (spec.samples == 0)
            {
                PYRAMID_LOG_ERROR("Framebuffer sample count cannot be zero");
                return false;
            }

            u32 depthAttachmentCount = 0;
            u32 stencilAttachmentCount = 0;
            bool hasCombinedDepthStencil = false;
            std::unordered_set<u32> colorAttachmentIndices;

            for (const auto& attachment : spec.attachments)
            {
                if (attachment.samples == 0)
                {
                    PYRAMID_LOG_ERROR("Framebuffer attachment sample count cannot be zero");
                    return false;
                }

                const bool usesMultisampling = attachment.multisampled || attachment.samples > 1;
                if (usesMultisampling)
                {
                    if (!attachment.multisampled || attachment.samples <= 1)
                    {
                        PYRAMID_LOG_ERROR(
                            "Multisampled attachments must set multisampled=true and samples > 1");
                        return false;
                    }

                    if (spec.samples <= 1 || attachment.samples != spec.samples)
                    {
                        PYRAMID_LOG_ERROR(
                            "Framebuffer and attachment sample counts must match: framebuffer=",
                            spec.samples, ", attachment=", attachment.samples);
                        return false;
                    }
                }
                else if (spec.samples > 1)
                {
                    PYRAMID_LOG_ERROR(
                        "All attachments must be multisampled when framebuffer samples > 1");
                    return false;
                }

                switch (attachment.type)
                {
                case FramebufferAttachmentType::Color:
                    if (!colorAttachmentIndices.insert(attachment.colorAttachmentIndex).second)
                    {
                        PYRAMID_LOG_ERROR(
                            "Duplicate color attachment index: ", attachment.colorAttachmentIndex);
                        return false;
                    }
                    break;
                case FramebufferAttachmentType::Depth:
                    ++depthAttachmentCount;
                    break;
                case FramebufferAttachmentType::Stencil:
                    ++stencilAttachmentCount;
                    break;
                case FramebufferAttachmentType::DepthStencil:
                    ++depthAttachmentCount;
                    ++stencilAttachmentCount;
                    hasCombinedDepthStencil = true;
                    break;
                }
            }

            if (depthAttachmentCount > 1)
            {
                PYRAMID_LOG_ERROR("Framebuffer can only have one depth attachment");
                return false;
            }

            if (stencilAttachmentCount > 1)
            {
                PYRAMID_LOG_ERROR("Framebuffer can only have one stencil attachment");
                return false;
            }

            if (hasCombinedDepthStencil &&
                (depthAttachmentCount != 1 || stencilAttachmentCount != 1))
            {
                PYRAMID_LOG_ERROR(
                    "A combined depth-stencil attachment cannot be mixed with separate depth or stencil attachments");
                return false;
            }

            return true;
        }

        bool ValidateSpec(const FramebufferSpec& spec)
        {
            if (!ValidateSpecStructure(spec))
            {
                return false;
            }

            if (spec.swapChainTarget)
            {
                return true;
            }

            const u32 maxColorAttachments = GetMaxColorAttachments();
            const u32 maxSamples = GetMaxSamples();

            if (spec.samples > maxSamples)
            {
                PYRAMID_LOG_ERROR(
                    "Framebuffer sample count exceeds active-context maximum: ",
                    spec.samples, " > ", maxSamples);
                return false;
            }

            for (const auto& attachment : spec.attachments)
            {
                if (attachment.type == FramebufferAttachmentType::Color &&
                    attachment.colorAttachmentIndex >= maxColorAttachments)
                {
                    PYRAMID_LOG_ERROR(
                        "Color attachment index exceeds maximum: ",
                        attachment.colorAttachmentIndex, " >= ", maxColorAttachments);
                    return false;
                }
            }

            return true;
        }

        u32 GetMaxColorAttachments()
        {
            static u32 maxColorAttachments = 0;
            if (maxColorAttachments == 0)
            {
                GLint max;
                glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &max);
                maxColorAttachments = static_cast<u32>(max);
            }
            return maxColorAttachments;
        }

        u32 GetMaxSamples()
        {
            static u32 maxSamples = 0;
            if (maxSamples == 0)
            {
                GLint max;
                glGetIntegerv(GL_MAX_SAMPLES, &max);
                maxSamples = static_cast<u32>(max);
            }
            return maxSamples;
        }

        bool IsFormatSupported(GLenum internalFormat)
        {
            // Check if the format is supported by querying OpenGL
            GLint supported;
            glGetInternalformativ(GL_TEXTURE_2D, internalFormat, GL_INTERNALFORMAT_SUPPORTED, 1, &supported);
            return supported == GL_TRUE;
        }

    } // namespace FramebufferUtils

    // Additional utility method implementations
    void OpenGLFramebuffer::SaveDepthAttachmentToFile(const std::string &filepath) const
    {
        if (m_depthAttachmentTexture == 0 && m_depthStencilAttachmentTexture == 0)
        {
            PYRAMID_LOG_ERROR("No depth attachment to save");
            return;
        }

        auto pixels = ReadDepthAttachmentPixels();
        if (pixels.empty())
        {
            PYRAMID_LOG_ERROR("Failed to read depth attachment pixels");
            return;
        }

        // Convert depth values to 8-bit grayscale for saving
        std::vector<u8> grayscalePixels;
        grayscalePixels.reserve(pixels.size());

        for (f32 depth : pixels)
        {
            grayscalePixels.push_back(static_cast<u8>(depth * 255.0f));
        }

        // Save as simple grayscale format (this is a basic implementation)
        std::ofstream file(filepath, std::ios::binary);
        if (file.is_open())
        {
            // Write simple header for raw grayscale
            file.write(reinterpret_cast<const char *>(grayscalePixels.data()), grayscalePixels.size());
            file.close();
            PYRAMID_LOG_INFO("Depth attachment saved to: ", filepath);
        }
        else
        {
            PYRAMID_LOG_ERROR("Failed to open file for writing: ", filepath);
        }
    }

    std::vector<f32> OpenGLFramebuffer::ReadDepthAttachmentPixels() const
    {
        if (m_depthAttachmentTexture == 0 && m_depthStencilAttachmentTexture == 0)
        {
            PYRAMID_LOG_ERROR("No depth attachment to read");
            return {};
        }

        Bind();

        std::vector<f32> pixels(m_spec.width * m_spec.height);
        glReadBuffer(GL_NONE);
        glReadPixels(0, 0, m_spec.width, m_spec.height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());

        Unbind();
        return pixels;
    }

    void OpenGLFramebuffer::GenerateMipmaps(u32 colorAttachment) const
    {
        if (colorAttachment >= m_colorAttachmentTextures.size() || m_colorAttachmentTextures[colorAttachment] == 0)
        {
            PYRAMID_LOG_ERROR("Invalid color attachment index for mipmap generation: ", colorAttachment);
            return;
        }

        GLuint texture = m_colorAttachmentTextures[colorAttachment];
        glBindTexture(GL_TEXTURE_2D, texture);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        PYRAMID_LOG_DEBUG("Generated mipmaps for color attachment ", colorAttachment);
    }

    void OpenGLFramebuffer::SetDebugLabel(const std::string &label) const
    {
        if (m_framebufferID != 0)
        {
            glObjectLabel(GL_FRAMEBUFFER, m_framebufferID, static_cast<GLsizei>(label.length()), label.c_str());
        }
    }

    std::string OpenGLFramebuffer::GetDebugInfo() const
    {
        std::string info = "Framebuffer Debug Info:\n";
        info += "  ID: " + std::to_string(m_framebufferID) + "\n";
        info += "  Size: " + std::to_string(m_spec.width) + "x" + std::to_string(m_spec.height) + "\n";
        info += "  Samples: " + std::to_string(m_spec.samples) + "\n";
        info += "  Attachments: " + std::to_string(m_spec.attachments.size()) + "\n";
        info += "  Color Attachments: " + std::to_string(m_colorAttachmentTextures.size()) + "\n";
        info += "  Has Depth: " + std::string((m_depthAttachmentTexture != 0 || m_depthStencilAttachmentTexture != 0) ? "Yes" : "No") + "\n";
        info += "  Complete: " + std::string(IsComplete() ? "Yes" : "No") + "\n";
        return info;
    }

} // namespace Pyramid
