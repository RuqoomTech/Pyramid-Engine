#include "Pyramid/Graphics/OpenGL/OpenGLTexture.hpp"
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include "../Texture/TextureFormatInfo.hpp"
#include "../Texture/TextureSamplerValidation.hpp"

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
        std::string UnsupportedFormatLabel(TextureFormat format)
        {
            switch (format)
            {
            case TextureFormat::None: return "None";
            case TextureFormat::BC1_RGB: return "BC1_RGB";
            case TextureFormat::BC1_RGBA: return "BC1_RGBA";
            case TextureFormat::BC3_RGBA: return "BC3_RGBA";
            case TextureFormat::BC7_RGBA: return "BC7_RGBA";
            default: return std::to_string(static_cast<u32>(format));
            }
        }

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

        if (!Detail::IsTextureSamplerStateValid(specification))
        {
            SetError("Texture sampler requires known filter/wrap values, finite border color, and anisotropy >= 1");
            return;
        }
        const auto formatInfo = Detail::GetTextureFormatInfo(specification.Format);
        if (formatInfo.bytesPerPixel != 0 && specification.SRGB && !formatInfo.srgbCapable)
        {
            SetError("sRGB requires an 8-bit RGB/RGBA texture format; got " +
                     std::to_string(static_cast<u32>(specification.Format)));
            return;
        }

        if (!ResolveFormats(
                specification.Format,
                specification.SRGB,
                m_InternalFormat,
                m_DataFormat,
                m_DataType,
                m_BytesPerPixel))
        {
            SetError("Unsupported OpenGLTexture2D format " + UnsupportedFormatLabel(specification.Format) +
                     "; None, compressed BC, and unknown formats are not implemented");
            return;
        }

        GLuint texture = 0;
        std::string error;
        if (!CreateTextureObject(specification, m_InternalFormat, m_DataFormat, m_DataType, data, texture, error))
        {
            SetError(error);
            return;
        }

        m_Specification = specification;
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
            OpenGLStateManager::GetInstance().ForgetTexture(m_RendererID);
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
        GLenum dataType = GL_UNSIGNED_BYTE;
        u32 bytesPerPixel = 0;
        if (!ResolveFormats(specification.Format, srgb, internalFormat, dataFormat, dataType, bytesPerPixel))
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
            dataType,
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
        m_DataType = dataType;
        m_BytesPerPixel = bytesPerPixel;
        m_IsLoaded = true;
        m_LastError.clear();

        if (previous)
        {
            glDeleteTextures(1, &previous);
            OpenGLStateManager::GetInstance().ForgetTexture(previous);
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

        auto& state = OpenGLStateManager::GetInstance();
        state.BindTexture(GL_TEXTURE_2D, m_RendererID);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(
            GL_TEXTURE_2D,
            0,
            0,
            0,
            static_cast<GLsizei>(m_Specification.Width),
            static_cast<GLsizei>(m_Specification.Height),
            m_DataFormat,
            m_DataType,
            data);

        if (m_Specification.GenerateMips)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        state.BindTexture(GL_TEXTURE_2D, 0);

        std::string error;
        if (!OpenGLDiagnostics::CheckError("OpenGLTexture2D::SetData", &error, false))
        {
            SetError(error);
            return;
        }

        m_LastError.clear();
    }

    void OpenGLTexture2D::SetFilter(TextureFilter minFilter, TextureFilter magFilter)
    {
        auto specification = m_Specification;
        specification.MinFilter = minFilter;
        specification.MagFilter = magFilter;
        UpdateSampler(specification, "OpenGLTexture2D::SetFilter");
    }

    void OpenGLTexture2D::SetWrap(TextureWrap wrapS, TextureWrap wrapT)
    {
        auto specification = m_Specification;
        specification.WrapS = wrapS;
        specification.WrapT = wrapT;
        UpdateSampler(specification, "OpenGLTexture2D::SetWrap");
    }

    void OpenGLTexture2D::SetBorderColor(f32 r, f32 g, f32 b, f32 a)
    {
        auto specification = m_Specification;
        specification.BorderColor[0] = r;
        specification.BorderColor[1] = g;
        specification.BorderColor[2] = b;
        specification.BorderColor[3] = a;
        UpdateSampler(specification, "OpenGLTexture2D::SetBorderColor");
    }

    void OpenGLTexture2D::SetMaxAnisotropy(f32 anisotropy)
    {
        auto specification = m_Specification;
        specification.MaxAnisotropy = anisotropy;
        UpdateSampler(specification, "OpenGLTexture2D::SetMaxAnisotropy");
    }

    void OpenGLTexture2D::GenerateMipmaps()
    {
        auto specification = m_Specification;
        specification.GenerateMips = true;
        UpdateSampler(specification, "OpenGLTexture2D::GenerateMipmaps", true);
    }

    bool OpenGLTexture2D::UpdateSampler(
        const TextureSpecification& specification, const char* operation, bool generateMipmaps)
    {
        if (!m_IsLoaded || !m_RendererID)
        {
            SetError(std::string(operation) + ": cannot change an unloaded texture");
            return false;
        }
        if (!Detail::IsTextureSamplerStateValid(specification))
        {
            SetError(std::string(operation) +
                     ": requires known filter/wrap values, finite border color, and anisotropy >= 1");
            return false;
        }

        OpenGLDiagnostics::ClearErrors();
        auto& state = OpenGLStateManager::GetInstance();
        state.BindTexture(GL_TEXTURE_2D, m_RendererID);
        if (generateMipmaps)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        std::string error;
        const bool applied = ApplyParameters(specification, error);
        state.BindTexture(GL_TEXTURE_2D, 0);
        std::string glError;
        const bool nativeSuccess = OpenGLDiagnostics::CheckError(operation, &glError, false);
        if (!applied || !nativeSuccess)
        {
            if (!error.empty() && !glError.empty())
            {
                error += "; ";
            }
            error += glError;
            state.BindTexture(GL_TEXTURE_2D, m_RendererID);
            std::string restoreError;
            const bool restored = ApplyParameters(m_Specification, restoreError);
            state.BindTexture(GL_TEXTURE_2D, 0);
            std::string restoreGlError;
            const bool restoreSuccess = OpenGLDiagnostics::CheckError(
                "OpenGLTexture2D sampler restoration", &restoreGlError, false);
            if (!restored || !restoreSuccess)
            {
                error += "; sampler restoration failed: " + restoreError + restoreGlError;
            }
            SetError(error);
            return false;
        }
        m_Specification = specification;
        m_LastError.clear();
        return true;
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
        OpenGLStateManager::GetInstance().BindTextureUnit(slot, GL_TEXTURE_2D, m_RendererID);
    }

    void OpenGLTexture2D::Unbind(u32 slot) const
    {
        OpenGLStateManager::GetInstance().BindTextureUnit(slot, GL_TEXTURE_2D, 0);
    }

    bool OpenGLTexture2D::ResolveFormats(
        TextureFormat format,
        bool srgb,
        GLenum& internalFormat,
        GLenum& dataFormat,
        GLenum& dataType,
        u32& bytesPerPixel)
    {
        const auto info = Detail::GetTextureFormatInfo(format);
        bytesPerPixel = info.bytesPerPixel;
        if (bytesPerPixel == 0 || (srgb && !info.srgbCapable))
        {
            return false;
        }
        dataType = GL_UNSIGNED_BYTE;
        switch (format)
        {
        case TextureFormat::RGB8:
        case TextureFormat::SRGB8:
            internalFormat = (srgb || format == TextureFormat::SRGB8) ? GL_SRGB8 : GL_RGB8;
            dataFormat = GL_RGB;
            break;
        case TextureFormat::RGBA8:
        case TextureFormat::SRGBA8:
            internalFormat = (srgb || format == TextureFormat::SRGBA8) ? GL_SRGB8_ALPHA8 : GL_RGBA8;
            dataFormat = GL_RGBA;
            break;
        case TextureFormat::R8:
            internalFormat = GL_R8;
            dataFormat = GL_RED;
            break;
        case TextureFormat::R16F:
            internalFormat = GL_R16F;
            dataFormat = GL_RED;
            dataType = GL_HALF_FLOAT;
            break;
        case TextureFormat::RGB16F:
            internalFormat = GL_RGB16F;
            dataFormat = GL_RGB;
            dataType = GL_HALF_FLOAT;
            break;
        case TextureFormat::RGBA16F:
            internalFormat = GL_RGBA16F;
            dataFormat = GL_RGBA;
            dataType = GL_HALF_FLOAT;
            break;
        case TextureFormat::R32F:
            internalFormat = GL_R32F;
            dataFormat = GL_RED;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::RGB32F:
            internalFormat = GL_RGB32F;
            dataFormat = GL_RGB;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::RGBA32F:
            internalFormat = GL_RGBA32F;
            dataFormat = GL_RGBA;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::Depth16:
            internalFormat = GL_DEPTH_COMPONENT16;
            dataFormat = GL_DEPTH_COMPONENT;
            dataType = GL_UNSIGNED_SHORT;
            break;
        case TextureFormat::Depth24:
            internalFormat = GL_DEPTH_COMPONENT24;
            dataFormat = GL_DEPTH_COMPONENT;
            dataType = GL_UNSIGNED_INT;
            break;
        case TextureFormat::Depth32F:
            internalFormat = GL_DEPTH_COMPONENT32F;
            dataFormat = GL_DEPTH_COMPONENT;
            dataType = GL_FLOAT;
            break;
        case TextureFormat::Depth24Stencil8:
            internalFormat = GL_DEPTH24_STENCIL8;
            dataFormat = GL_DEPTH_STENCIL;
            dataType = GL_UNSIGNED_INT_24_8;
            break;
        case TextureFormat::Depth32FStencil8:
            internalFormat = GL_DEPTH32F_STENCIL8;
            dataFormat = GL_DEPTH_STENCIL;
            dataType = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
            break;
        default:
            return false;
        }
        return true;
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
        GLenum dataType,
        const void* data,
        GLuint& texture,
        std::string& error) const
    {
        texture = 0;
        error.clear();

        std::size_t byteSize = 0;
        if (!CalculateByteSize(specification.Width, specification.Height,
                               Detail::GetTextureFormatInfo(specification.Format).bytesPerPixel,
                               byteSize))
        {
            error = "Texture dimensions or transfer byte size are invalid or overflow addressable memory";
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

        auto& state = OpenGLStateManager::GetInstance();
        state.BindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            static_cast<GLint>(internalFormat),
            static_cast<GLsizei>(specification.Width),
            static_cast<GLsizei>(specification.Height),
            0,
            dataFormat,
            dataType,
            data);

        const bool parametersApplied = ApplyParameters(specification, error);

        if (specification.GenerateMips)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
        state.BindTexture(GL_TEXTURE_2D, 0);

        std::string glError;
        const bool nativeSuccess = OpenGLDiagnostics::CheckError(
            "OpenGLTexture2D::CreateTextureObject", &glError, false);
        if (!parametersApplied || !nativeSuccess)
        {
            if (!error.empty() && !glError.empty())
            {
                error += "; ";
            }
            error += glError;
            glDeleteTextures(1, &texture);
            state.ForgetTexture(texture);
            texture = 0;
            return false;
        }

        return true;
    }

    bool OpenGLTexture2D::ApplyParameters(const TextureSpecification& specification, std::string& error) const
    {
        const bool anisotropySupported = GLVersion.major > 4 ||
            (GLVersion.major == 4 && GLVersion.minor >= 6) ||
            GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic;
        f32 effectiveAnisotropy = 1.0f;
        if (anisotropySupported)
        {
            GLfloat maximum = 0.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maximum);
            if (!std::isfinite(maximum) || maximum < 1.0f)
            {
                error = "Driver reported an invalid maximum texture anisotropy";
                return false;
            }
            effectiveAnisotropy = std::min(specification.MaxAnisotropy, maximum);
        }
        else if (specification.MaxAnisotropy > 1.0f)
        {
            PYRAMID_LOG_WARN("Texture anisotropy is unavailable; using 1x for requested ",
                             specification.MaxAnisotropy);
        }
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

        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, specification.BorderColor);
        if (anisotropySupported)
        {
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, effectiveAnisotropy);
        }
        return true;
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
