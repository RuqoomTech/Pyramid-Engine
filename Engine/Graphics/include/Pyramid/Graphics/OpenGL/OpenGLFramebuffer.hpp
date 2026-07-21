#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <glad/glad.h>
#include <string>
#include <vector>

namespace Pyramid
{

    /**
     * @brief Framebuffer attachment types.
     */
    enum class FramebufferAttachmentType
    {
        Color = 0,
        Depth,
        Stencil,
        DepthStencil
    };

    /**
     * @brief Framebuffer attachment specification.
     */
    struct FramebufferAttachmentSpec
    {
        FramebufferAttachmentType type = FramebufferAttachmentType::Color;
        GLenum internalFormat = GL_RGBA8;
        GLenum format = GL_RGBA;
        GLenum dataType = GL_UNSIGNED_BYTE;
        bool multisampled = false;
        u32 samples = 1;

        // For color attachments.
        u32 colorAttachmentIndex = 0;

        // Texture parameters. Ignored for multisampled attachments.
        GLenum minFilter = GL_LINEAR;
        GLenum magFilter = GL_LINEAR;
        GLenum wrapS = GL_CLAMP_TO_EDGE;
        GLenum wrapT = GL_CLAMP_TO_EDGE;
    };

    /**
     * @brief Framebuffer specification.
     */
    struct FramebufferSpec
    {
        u32 width = 1920;
        u32 height = 1080;
        std::vector<FramebufferAttachmentSpec> attachments;
        bool swapChainTarget = false;
        u32 samples = 1;
    };

    /**
     * @brief OpenGL framebuffer object with resize-safe attachment recreation.
     */
    class OpenGLFramebuffer
    {
    public:
        explicit OpenGLFramebuffer(const FramebufferSpec& spec);
        ~OpenGLFramebuffer();

        OpenGLFramebuffer(const OpenGLFramebuffer&) = delete;
        OpenGLFramebuffer& operator=(const OpenGLFramebuffer&) = delete;
        OpenGLFramebuffer(OpenGLFramebuffer&&) = delete;
        OpenGLFramebuffer& operator=(OpenGLFramebuffer&&) = delete;

        bool Initialize();
        void Bind() const;
        void Unbind() const;
        void Clear(f32 r = 0.0f, f32 g = 0.0f, f32 b = 0.0f, f32 a = 1.0f) const;
        void ClearAttachment(u32 attachmentIndex, const void* value) const;

        /**
         * @brief Recreate all owned attachments at a new size.
         *
         * The existing framebuffer remains valid if the replacement cannot be
         * created. Zero-sized extents are rejected without changing state.
         */
        bool Resize(u32 width, u32 height);

        /**
         * @brief Release all owned OpenGL resources and mark the object uninitialized.
         */
        void Invalidate();

        GLuint GetColorAttachmentTexture(u32 index = 0) const;
        GLuint GetDepthAttachmentTexture() const;
        GLuint GetStencilAttachmentTexture() const;

        void SetDrawBuffers(const std::vector<u32>& colorAttachments) const;
        void SetReadBuffer(u32 colorAttachment) const;

        void BlitTo(const OpenGLFramebuffer& target,
                    u32 srcX0, u32 srcY0, u32 srcX1, u32 srcY1,
                    u32 dstX0, u32 dstY0, u32 dstX1, u32 dstY1,
                    GLbitfield mask = GL_COLOR_BUFFER_BIT,
                    GLenum filter = GL_LINEAR) const;

        void ResolveMultisampleTo(const OpenGLFramebuffer& target) const;
        void ResolveMultisampleTo(const OpenGLFramebuffer& target, u32 colorAttachment) const;

        void BlitColorAttachmentTo(const OpenGLFramebuffer& target, u32 srcAttachment, u32 dstAttachment,
                                   u32 srcX0, u32 srcY0, u32 srcX1, u32 srcY1,
                                   u32 dstX0, u32 dstY0, u32 dstX1, u32 dstY1,
                                   GLenum filter = GL_LINEAR) const;
        void BlitDepthTo(const OpenGLFramebuffer& target,
                         u32 srcX0, u32 srcY0, u32 srcX1, u32 srcY1,
                         u32 dstX0, u32 dstY0, u32 dstX1, u32 dstY1) const;

        bool IsComplete() const;
        bool IsInitialized() const { return m_initialized; }
        bool IsMultisampled() const { return m_spec.samples > 1; }
        u32 GetColorAttachmentCount() const;

        const FramebufferSpec& GetSpecification() const { return m_spec; }
        u32 GetWidth() const { return m_spec.width; }
        u32 GetHeight() const { return m_spec.height; }
        GLuint GetFramebufferID() const { return m_framebufferID; }

        void SaveColorAttachmentToFile(u32 attachmentIndex, const std::string& filepath) const;
        std::vector<u8> ReadColorAttachmentPixels(u32 attachmentIndex) const;
        void SaveDepthAttachmentToFile(const std::string& filepath) const;
        std::vector<f32> ReadDepthAttachmentPixels() const;

        void GenerateMipmaps(u32 colorAttachment = 0) const;
        void SetDebugLabel(const std::string& label) const;
        std::string GetDebugInfo() const;

    private:
        void CreateAttachments();
        void CreateColorAttachment(const FramebufferAttachmentSpec& spec);
        void CreateDepthAttachment(const FramebufferAttachmentSpec& spec);
        void CreateStencilAttachment(const FramebufferAttachmentSpec& spec);
        void CreateDepthStencilAttachment(const FramebufferAttachmentSpec& spec);

        GLuint CreateTexture2D(u32 width, u32 height, const FramebufferAttachmentSpec& spec);
        GLuint CreateRenderbuffer(u32 width, u32 height, GLenum internalFormat,
                                  bool multisampled, u32 samples);

        void AttachTexture2D(GLuint texture, GLenum attachment,
                             GLenum textureTarget = GL_TEXTURE_2D, u32 mipLevel = 0);
        void AttachRenderbuffer(GLuint renderbuffer, GLenum attachment);
        void ReleaseResources();
        void SwapState(OpenGLFramebuffer& other) noexcept;

        static GLenum GetColorAttachmentEnum(u32 index);
        static std::string GetFramebufferStatusString(GLenum status);

    private:
        FramebufferSpec m_spec;
        GLuint m_framebufferID = 0;

        std::vector<GLuint> m_colorAttachmentTextures;
        std::vector<GLuint> m_colorAttachmentRenderbuffers;
        GLuint m_depthAttachmentTexture = 0;
        GLuint m_depthAttachmentRenderbuffer = 0;
        GLuint m_stencilAttachmentTexture = 0;
        GLuint m_stencilAttachmentRenderbuffer = 0;
        GLuint m_depthStencilAttachmentTexture = 0;
        GLuint m_depthStencilAttachmentRenderbuffer = 0;

        bool m_initialized = false;
    };

    namespace FramebufferUtils
    {
        FramebufferSpec CreateColorOnlySpec(u32 width, u32 height, GLenum format = GL_RGBA8);
        FramebufferSpec CreateColorDepthSpec(u32 width, u32 height, GLenum colorFormat = GL_RGBA8);
        FramebufferSpec CreateGBufferSpec(u32 width, u32 height);
        FramebufferSpec CreateShadowMapSpec(u32 width, u32 height);
        FramebufferSpec CreateHDRSpec(u32 width, u32 height);
        FramebufferSpec CreateMultisampledSpec(u32 width, u32 height, u32 samples,
                                               GLenum format = GL_RGBA8);

        /** Validate dimensions without requiring an active OpenGL context. */
        bool IsValidExtent(u32 width, u32 height);

        /** Validate structural constraints without querying OpenGL limits. */
        bool ValidateSpecStructure(const FramebufferSpec& spec);

        /** Validate structure and active-context OpenGL limits. */
        bool ValidateSpec(const FramebufferSpec& spec);

        u32 GetMaxColorAttachments();
        u32 GetMaxSamples();
        bool IsFormatSupported(GLenum internalFormat);
    }

} // namespace Pyramid
