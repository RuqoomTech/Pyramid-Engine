#pragma once

#include <Pyramid/Graphics/Texture.hpp>

namespace Pyramid::Detail
{
    struct TextureFormatInfo
    {
        u32 bytesPerPixel = 0;
        bool depth = false;
        bool srgbCapable = false;
    };

    // Canonical CPU transfer sizes, also used for format-based residency estimates.
    // Depth24 transfers in a full u32. Actual driver allocation is implementation-dependent.
    inline TextureFormatInfo GetTextureFormatInfo(TextureFormat format)
    {
        switch (format)
        {
        case TextureFormat::RGB8:
        case TextureFormat::SRGB8:
            return {3, false, true};
        case TextureFormat::RGBA8:
        case TextureFormat::SRGBA8:
            return {4, false, true};
        case TextureFormat::R8:
            return {1, false, false};
        case TextureFormat::R16F:
            return {2, false, false};
        case TextureFormat::RGB16F:
            return {6, false, false};
        case TextureFormat::RGBA16F:
            return {8, false, false};
        case TextureFormat::R32F:
            return {4, false, false};
        case TextureFormat::RGB32F:
            return {12, false, false};
        case TextureFormat::RGBA32F:
            return {16, false, false};
        case TextureFormat::Depth16:
            return {2, true, false};
        case TextureFormat::Depth24:
        case TextureFormat::Depth32F:
        case TextureFormat::Depth24Stencil8:
            return {4, true, false};
        case TextureFormat::Depth32FStencil8:
            return {8, true, false};
        default:
            return {};
        }
    }
}
