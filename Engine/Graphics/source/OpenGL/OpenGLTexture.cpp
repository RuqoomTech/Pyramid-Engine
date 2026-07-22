#include "Pyramid/Graphics/OpenGL/OpenGLTexture.hpp"

#include "OpenGLDiagnostics.hpp"

#include <Pyramid/Util/Image.hpp>
#include <Pyramid/Util/Log.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Pyramid
{
    namespace
    {
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
    } // namespace

    OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification, const void* data)
        : m_Specification(specification)
    {
        if (!IsValidExtent(specification.Width, specification.Height))
        {
            SetError("Texture dimensions must be greater than zero and fit OpenGL limits");
            return;
        }

        if (!ResolveFormats(
                specification.Format,
                specification.SRGB,
                m_InternalFormat,
                m_DataFormat,
                m_BytesPerPixel))
        {
            SetError("Unsupported OpenGLTexture2D format");
            return;
        }

        TextureSpecification normalized = specification;
        if (!normalized.GenerateMips && HasMipmapFilter(normalized.MinFilter))
        {
            normalized.MinFilter = TextureFilter::Linear;
        }

        GLuint texture = 0;
        std::string error;
        if (!CreateTextureObject(normalized, m_InternalFormat, m_DataFormat, data, texture, error))
        {
            SetError(error);
            return;
        }

        m_Specification = normalized;
        m_RendererID = texture;
        m_IsLoaded = true;
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& filepath, bool srgb, bool generateMips)
    {
        LoadFromFile(filepath, srgb, generateMips);
    }

    OpenGLTexture2D::~OpenGLTexture2D()
    {
        if (m_RendererID)
        {
            glDeleteTextures(1, &m_RendererID);
        }
    }

    bool OpenGLTexture2D::LoadFromFile(const std::string& filepath, bool srgb, bool generateMips)
    {
        Util::ImageData image = Util::Image::Load(filepath);
        if (!image.Data)
        {
            SetError("Failed to decode texture image: " + filepath);
            return false;
        }

        if (image.Width <= 0 || image.Height <= 0)
        {
            Util::Image::Free(image.Data);
            SetError("Decoded texture has invalid dimensions: " + filepath);
            return false;
        }

        TextureSpecification specification;
        specification.Width = static_cast<u32>(image.Width);
        specification.Height = static_cast<u32>(image.Height);
        specification.GenerateMips = generateMips;
        specification.SRGB = srgb;
        specification.Format = image.Channels == 4 ? TextureFormat::RGBA8 : TextureFormat::RGB8;

        if (image.Channels != 3 && image.Channels != 4)
        {
            const int channels = image.Channels;
            Util::Image::Free(image.Data);
            SetError(
                "Texture image must decode to RGB or RGBA pixels; got " +
                std::to_string(channels) + " channels for " + filepath);
            return false;
        }

        if (!generateMips)
        {
            specification.MinFilter = TextureFilter::Linear;
        }

        GLenum internalFormat = GL_RGBA8;
        GLenum dataFormat = GL_RGBA;
        u32 bytesPerPixel = 0;
        if (!ResolveFormats(specification.Format, srgb, internalFormat, dataFormat, bytesPerPixel))
        {
            Util::Image::Free(image.Data);
            SetError("Unsupported decoded texture format for " + filepath);
            return false;
        }

        GLuint replacement = 0;
        std::string error;
        const bool created = CreateTextureObject(
            specification,
            internalFormat,
            dataFormat,
            image.Data,
            replacement,
            error);
        Util::Image::Free(image.Data);

        if (!created)
        {
            SetError(error + " (" + filepath + ")");
            return false;
        }

        const GLuint previous = m_RendererID;
        m_RendererID = replacement;
        m_Specification = specification;
        m_Filepath = filepath;
        m_InternalFormat = internalFormat;
        m_DataFormat = dataFormat;
        m_BytesPerPixel = bytesPerPixel;
        m_IsLoaded = true;
        m_LastError.clear();

        if (previous)
        {
            glDeleteTextures(1, &previous);
        }

        PYRAMID_LOG_INFO(
            "Loaded OpenGL texture: ", filepath, " (", m_Specification.Width, "x",
            m_Specification.Height, ", ", m_BytesPerPixel, " bytes per pixel)");
        return true;
    }

    void OpenGLTexture2D::SetData(const void* data, u32 size)
    {
        if (!m_IsLoaded || !m_RendererID)
        {
            SetError("Cannot update an unloaded texture");
            return;
        }
        if (!data)
        {
            SetError("Texture update data is null");
            return;
        }

        std::size_t expectedSize = 0;
        if (!CalculateByteSize(
                m_Specification.Width,
                m_Specification.Height,
                m_BytesPerPixel,
                expectedSize) ||
            expectedSize != size)
        {
            SetError(
                "Texture update size mismatch: expected " + std::to_string(expectedSize) +
                " bytes, got " + std::to_string(size));
            return;
        }

        GLint previousAlignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        OpenGLDiagnostics::ClearErrors();

        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            static_cast<GLsizei>(m_Specification.Width),
            static_cast<GLsizei>(m_Specification.Height),
            m_DataFormat,
            GL_UNSIGNED_BYTE,
            data);

        if (m_Specification.GenerateMips)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::string error;
        if (!OpenGLDiagnostics::CheckError("OpenGLTexture2D::SetData", &error, false))
        {
            SetError(error);
            return;
        }

        m_LastError.clear();
    }

    void OpenGLTexture2D::GenerateMipmaps()
    {
        if (!m_IsLoaded || !m_RendererID)
        {
            SetError("Cannot generate mipmaps for an unloaded texture");
            return;
        }

        OpenGLDiagnostics::ClearErrors();
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        std::string error;
        if (!OpenGLDiagnostics::CheckError("OpenGLTexture2D::GenerateMipmaps", &error, false))
        {
            SetError(error);
            return;
        }

        m_Specification.GenerateMips = true;
        m_LastError.clear();
    }

    u32 OpenGLTexture2D::GetMipLevels() const
    {
        if (!m_IsLoaded || !m_Specification.GenerateMips)
        {
            return m_IsLoaded ? 1U : 0U;
        }

        const u32 largestDimension = std::max(m_Specification.Width, m_Specification.Height);
        return 1U + static_cast<u32>(std::floor(std::log2(static_cast<double>(largestDimension))));
    }

    void OpenGLTexture2D::Bind(u32 slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    void OpenGLTexture2D::Unbind(u32 slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

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

    GLenum OpenGLTexture2D::ToGLMinFilter(TextureFilter filter, bool hasMipmaps)
    {
        if (!hasMipmaps && HasMipmapFilter(filter))
        {
            return filter == TextureFilter::NearestMipmapLinear ||
                    filter == TextureFilter::NearestMipmapNearest
                ? GL_NEAREST
                : GL_LINEAR;
        }

        switch (filter)
        {
        case TextureFilter::Nearest:
            return GL_NEAREST;
        case TextureFilter::Linear:
            return GL_LINEAR;
        case TextureFilter::LinearMipmapLinear:
            return GL_LINEAR_MIPMAP_LINEAR;
        case TextureFilter::LinearMipmapNearest:
            return GL_LINEAR_MIPMAP_NEAREST;
        case TextureFilter::NearestMipmapLinear:
            return GL_NEAREST_MIPMAP_LINEAR;
        case TextureFilter::NearestMipmapNearest:
            return GL_NEAREST_MIPMAP_NEAREST;
        case TextureFilter::None:
        default:
            return GL_LINEAR;
        }
    }

    GLenum OpenGLTexture2D::ToGLMagFilter(TextureFilter filter)
    {
        switch (filter)
        {
        case TextureFilter::Nearest:
        case TextureFilter::NearestMipmapLinear:
        case TextureFilter::NearestMipmapNearest:
            return GL_NEAREST;
        default:
            return GL_LINEAR;
        }
    }

    GLenum OpenGLTexture2D::ToGLWrap(TextureWrap wrap)
    {
        switch (wrap)
        {
        case TextureWrap::MirroredRepeat:
            return GL_MIRRORED_REPEAT;
        case TextureWrap::ClampToEdge:
            return GL_CLAMP_TO_EDGE;
        case TextureWrap::ClampToBorder:
            return GL_CLAMP_TO_BORDER;
        case TextureWrap::Repeat:
        case TextureWrap::None:
        default:
            return GL_REPEAT;
        }
    }

    bool OpenGLTexture2D::HasMipmapFilter(TextureFilter filter)
    {
        return filter == TextureFilter::LinearMipmapLinear ||
            filter == TextureFilter::LinearMipmapNearest ||
            filter == TextureFilter::NearestMipmapLinear ||
            filter == TextureFilter::NearestMipmapNearest;
    }

    bool OpenGLTexture2D::CreateTextureObject(
        const TextureSpecification& specification,
        GLenum internalFormat,
        GLenum dataFormat,
        const void* data,
        GLuint& texture,
        std::string& error) const
    {
        texture = 0;
        error.clear();

        if (!IsValidExtent(specification.Width, specification.Height))
        {
            error = "Texture dimensions are invalid";
            return false;
        }

        GLint previousAlignment = 4;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
        OpenGLDiagnostics::ClearErrors();

        glGenTextures(1, &texture);
        if (!texture)
        {
            error = "OpenGL did not allocate a texture object";
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            static_cast<GLint>(internalFormat),
            static_cast<GLsizei>(specification.Width),
            static_cast<GLsizei>(specification.Height),
            0,
            dataFormat,
            GL_UNSIGNED_BYTE,
            data);

        ApplyParameters(specification);

        if (specification.GenerateMips && data)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (!OpenGLDiagnostics::CheckError("OpenGLTexture2D::CreateTextureObject", &error, false))
        {
            glDeleteTextures(1, &texture);
            texture = 0;
            return false;
        }

        return true;
    }

    void OpenGLTexture2D::ApplyParameters(const TextureSpecification& specification) const
    {
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            static_cast<GLint>(ToGLMinFilter(specification.MinFilter, specification.GenerateMips)));
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MAG_FILTER,
            static_cast<GLint>(ToGLMagFilter(specification.MagFilter)));
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            static_cast<GLint>(ToGLWrap(specification.WrapS)));
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            static_cast<GLint>(ToGLWrap(specification.WrapT)));

        if (specification.WrapS == TextureWrap::ClampToBorder ||
            specification.WrapT == TextureWrap::ClampToBorder)
        {
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, specification.BorderColor);
        }
    }

    void OpenGLTexture2D::SetError(const std::string& error)
    {
        m_LastError = error;
        if (!m_IsLoaded)
        {
            m_RendererID = 0;
        }
        PYRAMID_LOG_ERROR("OpenGLTexture2D: ", error);
    }
} // namespace Pyramid
