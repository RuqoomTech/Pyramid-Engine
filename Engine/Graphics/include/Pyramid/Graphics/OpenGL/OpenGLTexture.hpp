#pragma once

#include "Pyramid/Graphics/Texture.hpp"

#include <glad/glad.h>

namespace Pyramid
{
    class OpenGLTexture2D final : public ITexture2D
    {
    public:
        OpenGLTexture2D(const TextureSpecification& specification, const void* data);
        OpenGLTexture2D(const std::string& filepath, bool srgb, bool generateMips);
        ~OpenGLTexture2D() override;

        OpenGLTexture2D(const OpenGLTexture2D&) = delete;
        OpenGLTexture2D& operator=(const OpenGLTexture2D&) = delete;

        void Bind(u32 slot = 0) const override;
        void Unbind(u32 slot = 0) const override;

        u32 GetWidth() const override { return m_Specification.Width; }
        u32 GetHeight() const override { return m_Specification.Height; }
        u32 GetRendererID() const override { return m_RendererID; }
        TextureFormat GetFormat() const override { return m_Specification.Format; }
        const std::string& GetPath() const override { return m_Filepath; }
        bool IsLoaded() const override { return m_IsLoaded; }
        std::string GetLastError() const override { return m_LastError; }

        void SetData(const void* data, u32 size) override;
        void GenerateMipmaps() override;
        u32 GetMipLevels() const override;
        bool LoadFromFile(const std::string& filepath, bool srgb = false, bool generateMips = true) override;

    private:
        static bool ResolveFormats(
            TextureFormat format,
            bool srgb,
            GLenum& internalFormat,
            GLenum& dataFormat,
            u32& bytesPerPixel);
        static GLenum ToGLMinFilter(TextureFilter filter, bool hasMipmaps);
        static GLenum ToGLMagFilter(TextureFilter filter);
        static GLenum ToGLWrap(TextureWrap wrap);
        static bool HasMipmapFilter(TextureFilter filter);

        bool CreateTextureObject(
            const TextureSpecification& specification,
            GLenum internalFormat,
            GLenum dataFormat,
            const void* data,
            GLuint& texture,
            std::string& error) const;
        void ApplyParameters(const TextureSpecification& specification) const;
        void SetError(const std::string& error);

        TextureSpecification m_Specification;
        std::string m_Filepath;
        std::string m_LastError;
        GLuint m_RendererID = 0;
        GLenum m_InternalFormat = GL_RGBA8;
        GLenum m_DataFormat = GL_RGBA;
        u32 m_BytesPerPixel = 4;
        bool m_IsLoaded = false;
    };
} // namespace Pyramid
