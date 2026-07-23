#pragma once

namespace Pyramid
{
    /**
     * @brief Primitive assembly mode used by mesh resources and draw commands.
     */
    enum class PrimitiveTopology
    {
        Points,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip
    };
}
