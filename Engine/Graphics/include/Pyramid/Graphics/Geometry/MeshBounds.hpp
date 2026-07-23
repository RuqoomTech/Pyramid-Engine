#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Math/Math.hpp>

namespace Pyramid::Geometry
{
    /**
     * @brief Derive an axis-aligned local-space bound from interleaved vertex data.
     *
     * The layout must contain a Float2, Float3, or Float4 element whose semantic
     * name ends in "position" after punctuation and case are ignored (for example
     * Position, a_Position, or inPosition). Float2 positions use z = 0 and Float4
     * positions ignore w.
     *
     * @return true when all vertices contain finite positions and bounds were produced.
     */
    bool CalculateLocalBounds(
        const void* vertexData,
        u32 vertexDataSize,
        const BufferLayout& layout,
        Math::Vec3& minPoint,
        Math::Vec3& maxPoint);
}
