// Engine/Graphics/include/Pyramid/Graphics/Buffer/VertexArray.hpp
// Engine/Graphics/include/Pyramid/Graphics/Buffer/VertexArray.hpp
#pragma once
#include "Pyramid/Core/Prerequisites.hpp"
#include "Pyramid/Graphics/Buffer/BufferLayout.hpp" // Added
#include <memory>
#include <vector>                                  // For GetVertexBuffers (if added later)
#include "Pyramid/Graphics/Buffer/IndexBuffer.hpp" // Added full include
#include "Pyramid/Math/Math.hpp"

namespace Pyramid
{
    class IVertexBuffer;   // Still okay to forward declare if only used in pointers/references by IVertexArray itself
    class IInstanceBuffer; // Forward declaration for instance buffer
    // class IIndexBuffer; // Removed forward declaration

    class IVertexArray
    {
    public:
        virtual ~IVertexArray() = default;

        virtual void Bind() const = 0;
        virtual void Unbind() const = 0;

        virtual void AddVertexBuffer(const std::shared_ptr<IVertexBuffer> &vertexBuffer, const BufferLayout &layout) = 0; // Changed
        // virtual const std::vector<std::shared_ptr<IVertexBuffer>>& GetVertexBuffers() const = 0; // Optional future addition

        virtual void SetIndexBuffer(const std::shared_ptr<IIndexBuffer> &indexBuffer) = 0;
        virtual const std::shared_ptr<IIndexBuffer> &GetIndexBuffer() const = 0;

        /**
         * @brief Derive local-space bounds from attached vertex buffers when CPU data is available.
         */
        virtual bool TryGetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
        {
            (void)minPoint;
            (void)maxPoint;
            return false;
        }

        // Instance buffer support for instanced rendering
        virtual void AddInstanceBuffer(const std::shared_ptr<IInstanceBuffer> &instanceBuffer, const BufferLayout &layout, u32 startingAttributeIndex) = 0;
        virtual void RemoveInstanceBuffer() = 0;
        virtual bool HasInstanceBuffer() const = 0;
    };
}
