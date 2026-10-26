#include "Pyramid/Graphics/Texture.hpp"
#include "Pyramid/Graphics/OpenGL/OpenGLTexture.hpp"
#include "Texture/TextureFormatInfo.hpp"
#include <Pyramid/Util/Log.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Pyramid
{
    std::shared_ptr<ITexture2D> ITexture2D::Create(const TextureSpecification& specification, const void* data)
    {
        return std::make_shared<OpenGLTexture2D>(specification, data);
    }

    std::shared_ptr<ITexture2D> ITexture2D::Create(const std::string& filepath, bool srgb, bool generateMips)
    {
        return std::make_shared<OpenGLTexture2D>(filepath, srgb, generateMips);
    }

    std::shared_ptr<ITexture2D> ITexture2D::Create(u32 width, u32 height, TextureFormat format)
    {
        TextureSpecification specification;
        specification.Width = width;
        specification.Height = height;
        specification.Format = format;
        specification.GenerateMips = false;
        specification.MinFilter = TextureFilter::Linear;
        return Create(specification, nullptr);
    }

    std::shared_ptr<ITexture2D> ITexture2D::CreateRenderTarget(u32 width, u32 height, TextureFormat format)
    {
        return Create(width, height, format);
    }

    std::shared_ptr<ITexture2D> ITexture2D::CreateDepthTarget(u32 width, u32 height, TextureFormat format)
    {
        if (!Detail::GetTextureFormatInfo(format).depth)
        {
            PYRAMID_LOG_ERROR("CreateDepthTarget requires a depth or depth-stencil texture format; got ",
                              static_cast<u32>(format));
            return nullptr;
        }

        TextureSpecification specification;
        specification.Width = width;
        specification.Height = height;
        specification.Format = format;
        specification.GenerateMips = false;
        specification.MinFilter = TextureFilter::Nearest;
        specification.MagFilter = TextureFilter::Nearest;
        specification.WrapS = TextureWrap::ClampToEdge;
        specification.WrapT = TextureWrap::ClampToEdge;
        return Create(specification, nullptr);
    }

    std::shared_ptr<ITexture2D> ITexture2D::CreateFromColor(u32 width, u32 height, const Color& color)
    {
        const auto toByte = [](f32 value) -> u8
        {
            const f32 clamped = std::max(0.0f, std::min(1.0f, value));
            return static_cast<u8>(std::lround(clamped * 255.0f));
        };

        const std::array<u8, 4> pixel = {
            toByte(color.r), toByte(color.g), toByte(color.b), toByte(color.a)};

        std::vector<u8> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * pixel.size());
        for (size_t offset = 0; offset < pixels.size(); offset += pixel.size())
        {
            std::copy(pixel.begin(), pixel.end(), pixels.begin() + static_cast<std::ptrdiff_t>(offset));
        }

        TextureSpecification specification;
        specification.Width = width;
        specification.Height = height;
        specification.Format = TextureFormat::RGBA8;
        specification.GenerateMips = false;
        specification.MinFilter = TextureFilter::Linear;
        return Create(specification, pixels.data());
    }
} // namespace Pyramid
