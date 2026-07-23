#pragma once
#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Math/Math.hpp>

namespace Pyramid {

/**
 * @brief Interface for vertex buffer implementations
 */
class IVertexBuffer {
public:
    virtual ~IVertexBuffer() = default;

    /**
     * @brief Bind the vertex buffer for rendering
     */
    virtual void Bind() = 0;

    /**
     * @brief Unbind the vertex buffer
     */
    virtual void Unbind() = 0;

    /**
     * @brief Upload vertex data to the buffer
     * @param data Pointer to vertex data
     * @param size Size of data in bytes
     */
    virtual void SetData(const void* data, u32 size) = 0;

    /**
     * @brief Associate a vertex layout so the backend can derive geometry metadata.
     */
    virtual void SetLayout(const BufferLayout &layout) { (void)layout; }

    /**
     * @brief Return cached local-space bounds when the backend could derive them.
     */
    virtual bool TryGetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
    {
        (void)minPoint;
        (void)maxPoint;
        return false;
    }
};

} // namespace Pyramid
