#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/PrimitiveTopology.hpp>
#include <Pyramid/Math/Math.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace Pyramid
{
    class IGraphicsDevice;
    class IIndexBuffer;
    class IVertexArray;
    class IVertexBuffer;

    namespace Renderer
    {
        class CommandBuffer;
    }

    /**
     * @brief Stable 128-bit identifier for a mesh asset.
     *
     * Caller-defined identifiers can be created from stable names such as asset
     * paths. When a specification does not provide an identifier, Pyramid derives
     * one deterministically from the exact vertex/index bytes, layout, counts, and
     * primitive topology. Debug names are deliberately excluded.
     */
    struct MeshAssetId
    {
        u64 high = 0;
        u64 low = 0;

        bool IsValid() const { return high != 0 || low != 0; }
        std::string ToString() const;

        static MeshAssetId FromString(std::string_view stableName);

        bool operator==(const MeshAssetId& other) const
        {
            return high == other.high && low == other.low;
        }

        bool operator!=(const MeshAssetId& other) const
        {
            return !(*this == other);
        }
    };

    struct MeshAssetIdHash
    {
        std::size_t operator()(const MeshAssetId& identifier) const noexcept;
    };

    /**
     * @brief Immutable source description used to create a GPU-backed mesh.
     *
     * Pyramid copies the supplied vertex/index data into engine-owned GPU buffers.
     * The caller only needs to keep the pointed-to memory alive for the duration of
     * Mesh::Create() or MeshCache::GetOrCreate().
     */
    struct MeshSpecification
    {
        const void* vertexData = nullptr;
        u32 vertexDataSize = 0;
        u32 vertexCount = 0;
        BufferLayout layout;

        const u32* indexData = nullptr;
        u32 indexCount = 0;

        PrimitiveTopology topology = PrimitiveTopology::Triangles;
        std::string name;

        /**
         * Optional stable caller-owned identifier. If invalid, the content
         * fingerprint becomes the asset identifier automatically.
         */
        MeshAssetId assetId;
    };

    /**
     * @brief Engine-owned immutable geometry resource.
     *
     * A mesh owns the vertex array plus its vertex/index buffers and keeps the
     * validated layout, draw count, topology, identifiers, and local bounds
     * together. This prevents RenderObject from depending on mutable raw
     * vertex-array state.
     */
    class Mesh final
    {
    public:
        static std::shared_ptr<Mesh> Create(
            IGraphicsDevice& device,
            const MeshSpecification& specification);

        /**
         * @brief Calculate the deterministic fingerprint for a valid specification.
         * @return Invalid identifier when the specification is malformed.
         */
        static MeshAssetId CalculateContentId(const MeshSpecification& specification);

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh(Mesh&&) = delete;
        Mesh& operator=(Mesh&&) = delete;
        ~Mesh() = default;

        bool IsValid() const;
        void Bind() const;
        void Unbind() const;
        bool IsIndexed() const { return m_indexCount > 0; }

        u32 GetVertexCount() const { return m_vertexCount; }
        u32 GetIndexCount() const { return m_indexCount; }
        u32 GetDrawCount() const { return IsIndexed() ? m_indexCount : m_vertexCount; }
        u64 GetVertexDataSize() const { return m_vertexDataSize; }
        u64 GetIndexDataSize() const
        {
            return static_cast<u64>(m_indexCount) * sizeof(u32);
        }
        u64 GetGeometryDataSize() const
        {
            return GetVertexDataSize() + GetIndexDataSize();
        }
        PrimitiveTopology GetTopology() const { return m_topology; }
        const BufferLayout& GetLayout() const { return m_layout; }
        const std::string& GetName() const { return m_name; }
        MeshAssetId GetAssetId() const { return m_assetId; }
        MeshAssetId GetContentId() const { return m_contentId; }

        void GetLocalBounds(Math::Vec3& minPoint, Math::Vec3& maxPoint) const;

    private:
        friend class Renderer::CommandBuffer;

        const std::shared_ptr<IVertexArray>& GetVertexArray() const { return m_vertexArray; }

        Mesh(
            std::shared_ptr<IVertexArray> vertexArray,
            std::shared_ptr<IVertexBuffer> vertexBuffer,
            std::shared_ptr<IIndexBuffer> indexBuffer,
            BufferLayout layout,
            u32 vertexDataSize,
            u32 vertexCount,
            u32 indexCount,
            PrimitiveTopology topology,
            MeshAssetId assetId,
            MeshAssetId contentId,
            const Math::Vec3& localBoundsMin,
            const Math::Vec3& localBoundsMax,
            std::string name);

        std::shared_ptr<IVertexArray> m_vertexArray;
        std::shared_ptr<IVertexBuffer> m_vertexBuffer;
        std::shared_ptr<IIndexBuffer> m_indexBuffer;
        BufferLayout m_layout;
        u32 m_vertexDataSize = 0;
        u32 m_vertexCount = 0;
        u32 m_indexCount = 0;
        PrimitiveTopology m_topology = PrimitiveTopology::Triangles;
        MeshAssetId m_assetId;
        MeshAssetId m_contentId;
        Math::Vec3 m_localBoundsMin = Math::Vec3(-0.5f);
        Math::Vec3 m_localBoundsMax = Math::Vec3(0.5f);
        std::string m_name;
    };
}
