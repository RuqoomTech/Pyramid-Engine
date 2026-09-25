#include "Pyramid/Graphics/Texture.hpp"
#include "Pyramid/Graphics/OpenGL/OpenGLTexture.hpp"
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
        // Only the mapped depth and packed depth-stencil formats are sampled
        // depth textures. Anything else is rejected with a corrective pointer
        // rather than silently created as a color texture.
        if (!OpenGLTexture2D::IsDepthStencilFormat(format))
        {
            PYRAMID_LOG_ERROR(
                "CreateDepthTarget requires a mapped depth or packed depth-stencil format, got format ",
                static_cast<int>(format), " at ", width, "x", height,
                "; use OpenGLFramebuffer attachments for render-target depth");
            return nullptr;
        }

        TextureSpecification specification;
        specification.Width = width;
        specification.Height = height;
        specification.Format = format;
        // A depth pyramid is never generated: filtering between depth levels
        // changes depth and shadow semantics, so only the base level exists.
        specification.GenerateMips = false;
        specification.MinFilter = TextureFilter::Linear;
        specification.MagFilter = TextureFilter::Linear;
        specification.WrapS = TextureWrap::ClampToEdge;
        specification.WrapT = TextureWrap::ClampToEdge;

        // Transactional creation: OpenGLTexture2D validates the extent before
        // allocating, uploads the level, and deletes the texture object and
        // clears the handle if any step fails, so a rejected request leaves
        // nothing half-made.
        std::shared_ptr<OpenGLTexture2D> texture =
            std::make_shared<OpenGLTexture2D>(specification, nullptr);
        if (!texture->IsLoaded() || texture->GetRendererID() == 0)
        {
            PYRAMID_LOG_ERROR(
                "Failed to create ", width, "x", height, " depth target with format ",
                static_cast<int>(format), ": ", texture->GetLastError());
            return nullptr;
        }

        // Ownership: the caller owns this sampled depth texture. Sharing goes
        // through ResourceRegistry::Textures(), which never mutates a cached
        // TextureResource in place.
        return texture;
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
