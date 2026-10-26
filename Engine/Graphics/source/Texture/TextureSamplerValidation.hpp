#pragma once

#include <Pyramid/Graphics/Texture.hpp>
#include <cmath>

namespace Pyramid::Detail
{
    inline bool IsTextureSamplerStateValid(
        TextureFilter minFilter, TextureFilter magFilter,
        TextureWrap wrapS, TextureWrap wrapT,
        const f32 borderColor[4], f32 maxAnisotropy)
    {
        const auto knownFilter = [](TextureFilter filter)
        {
            return filter >= TextureFilter::None && filter <= TextureFilter::NearestMipmapNearest;
        };
        const auto knownWrap = [](TextureWrap wrap)
        {
            return wrap >= TextureWrap::None && wrap <= TextureWrap::ClampToBorder;
        };
        if (!knownFilter(minFilter) || !knownFilter(magFilter) ||
            !knownWrap(wrapS) || !knownWrap(wrapT) ||
            !std::isfinite(maxAnisotropy) || maxAnisotropy < 1.0f)
        {
            return false;
        }
        for (u32 index = 0; index < 4; ++index)
        {
            if (!std::isfinite(borderColor[index]))
            {
                return false;
            }
        }
        return true;
    }

    inline bool IsTextureSamplerStateValid(const TextureSpecification& specification)
    {
        return IsTextureSamplerStateValid(
            specification.MinFilter, specification.MagFilter,
            specification.WrapS, specification.WrapT,
            specification.BorderColor, specification.MaxAnisotropy);
    }
}