#include <Pyramid/Graphics/Geometry/Mesh.hpp>

#include <Pyramid/Graphics/Buffer/IndexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>

#include <limits>
#include <new>
#include <utility>

namespace Pyramid
{
    namespace
    {
        bool IsTopologyCountValid(PrimitiveTopology topology, u32 drawCount)
        {
            switch (topology)
            {
            case PrimitiveTopology::Points:
                return drawCount >= 1;
            case PrimitiveTopology::Lines:
                return drawCount >= 2 && (drawCount % 2u) == 0u;
            case PrimitiveTopology::LineStrip:
                return drawCount >= 2;
            case PrimitiveTopology::Triangles:
                return drawCount >= 3 && (drawCount % 3u) == 0u;
            case PrimitiveTopology::TriangleStrip:
                return drawCount >= 3;
            }

            return false;
        }

        bool ValidateSpecification(const MeshSpecification& specification)
        {
            if (!specification.vertexData || specification.vertexDataSize == 0 ||
                specification.vertexCount == 0)
            {
                PYRAMID_LOG_ERROR("Mesh creation failed: vertex data and vertex count are required");
                return false;
            }

            const u32 stride = specification.layout.GetStride();
            if (stride == 0 || specification.layout.GetElements().empty())
            {
                PYRAMID_LOG_ERROR("Mesh creation failed: vertex layout is empty");
                return false;
            }

            const auto requiredBytes =
                static_cast<unsigned long long>(stride) * specification.vertexCount;
            if (requiredBytes > std::numeric_limits<u32>::max() ||
                requiredBytes != specification.vertexDataSize)
            {
                PYRAMID_LOG_ERROR(
                    "Mesh creation failed: vertexDataSize does not match vertexCount * layout stride");
                return false;
            }

            const bool indexed = specification.indexCount > 0;
            if (indexed != (specification.indexData != nullptr))
            {
                PYRAMID_LOG_ERROR(
                    "Mesh creation failed: indexData and indexCount must either both be supplied or both be empty");
                return false;
            }

            if (indexed)
            {
                for (u32 index = 0; index < specification.indexCount; ++index)
                {
                    if (specification.indexData[index] >= specification.vertexCount)
                    {
                        PYRAMID_LOG_ERROR(
                            "Mesh creation failed: index ", specification.indexData[index],
                            " is outside vertex count ", specification.vertexCount);
                        return false;
                    }
                }
            }

            const u32 drawCount = indexed ? specification.indexCount : specification.vertexCount;
            if (!IsTopologyCountValid(specification.topology, drawCount))
            {
                PYRAMID_LOG_ERROR("Mesh creation failed: draw count is invalid for the selected topology");
                return false;
            }

            return true;
        }
    }

    std::shared_ptr<Mesh> Mesh::Create(
        IGraphicsDevice& device,
        const MeshSpecification& specification)
    {
        if (!ValidateSpecification(specification))
        {
            return nullptr;
        }

        Math::Vec3 localBoundsMin;
        Math::Vec3 localBoundsMax;
        if (!Geometry::CalculateLocalBounds(
                specification.vertexData,
                specification.vertexDataSize,
                specification.layout,
                localBoundsMin,
                localBoundsMax))
        {
            PYRAMID_LOG_ERROR(
                "Mesh creation failed: finite position data with a recognized position semantic is required");
            return nullptr;
        }

        try
        {
            auto vertexBuffer = device.CreateVertexBuffer();
            auto vertexArray = device.CreateVertexArray();
            if (!vertexBuffer || !vertexArray)
            {
                PYRAMID_LOG_ERROR("Mesh creation failed: graphics device could not allocate vertex resources");
                return nullptr;
            }

            vertexBuffer->SetData(specification.vertexData, specification.vertexDataSize);
            vertexArray->AddVertexBuffer(vertexBuffer, specification.layout);

            std::shared_ptr<IIndexBuffer> indexBuffer;
            if (specification.indexCount > 0)
            {
                indexBuffer = device.CreateIndexBuffer();
                if (!indexBuffer)
                {
                    PYRAMID_LOG_ERROR("Mesh creation failed: graphics device could not allocate an index buffer");
                    return nullptr;
                }

                indexBuffer->SetData(specification.indexData, specification.indexCount);
                vertexArray->SetIndexBuffer(indexBuffer);
            }

            return std::shared_ptr<Mesh>(new Mesh(
                std::move(vertexArray),
                std::move(vertexBuffer),
                std::move(indexBuffer),
                specification.layout,
                specification.vertexCount,
                specification.indexCount,
                specification.topology,
                localBoundsMin,
                localBoundsMax,
                specification.name));
        }
        catch (const std::bad_alloc&)
        {
            PYRAMID_LOG_ERROR("Mesh creation failed: memory allocation failed");
            return nullptr;
        }
    }

    Mesh::Mesh(
        std::shared_ptr<IVertexArray> vertexArray,
        std::shared_ptr<IVertexBuffer> vertexBuffer,
        std::shared_ptr<IIndexBuffer> indexBuffer,
        BufferLayout layout,
        u32 vertexCount,
        u32 indexCount,
        PrimitiveTopology topology,
        const Math::Vec3& localBoundsMin,
        const Math::Vec3& localBoundsMax,
        std::string name)
        : m_vertexArray(std::move(vertexArray)),
          m_vertexBuffer(std::move(vertexBuffer)),
          m_indexBuffer(std::move(indexBuffer)),
          m_layout(std::move(layout)),
          m_vertexCount(vertexCount),
          m_indexCount(indexCount),
          m_topology(topology),
          m_localBoundsMin(localBoundsMin),
          m_localBoundsMax(localBoundsMax),
          m_name(std::move(name))
    {
    }

    bool Mesh::IsValid() const
    {
        return m_vertexArray && m_vertexBuffer && m_vertexCount > 0 &&
               (!IsIndexed() || m_indexBuffer != nullptr) && GetDrawCount() > 0;
    }

    void Mesh::Bind() const
    {
        if (m_vertexArray)
        {
            m_vertexArray->Bind();
        }
    }

    void Mesh::Unbind() const
    {
        if (m_vertexArray)
        {
            m_vertexArray->Unbind();
        }
    }

    void Mesh::GetLocalBounds(Math::Vec3& minPoint, Math::Vec3& maxPoint) const
    {
        minPoint = m_localBoundsMin;
        maxPoint = m_localBoundsMax;
    }
}
