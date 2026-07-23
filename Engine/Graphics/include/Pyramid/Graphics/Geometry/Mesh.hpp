#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/PrimitiveTopology.hpp>
#include <Pyramid/Math/Math.hpp>

#include <memory>
#include <string>

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
     * @brief Immutable source description used to create a GPU-backed mesh.
     *
     * Pyramid copies the supplied vertex/index data into engine-owned GPU buffers.
     * The caller only needs to keep the pointed-to memory alive for the duration of
     * Mesh::Create().
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
    };

    /**
     * @brief Engine-owned immutable geometry resource.
     *
     * A mesh owns the vertex array plus its vertex/index buffers and keeps the
     * validated layout, draw count, topology, and local bounds together. This
     * prevents RenderObject from depending on mutable raw vertex-array state.
     */
    class Mesh final
    {
    public:
        static std::shared_ptr<Mesh> Create(
            IGraphicsDevice& device,
            const MeshSpecification& specification);

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
        PrimitiveTopology GetTopology() const { return m_topology; }
        const BufferLayout& GetLayout() const { return m_layout; }
        const std::string& GetName() const { return m_name; }

        void GetLocalBounds(Math::Vec3& minPoint, Math::Vec3& maxPoint) const;

    private:
        friend class Renderer::CommandBuffer;

        const std::shared_ptr<IVertexArray>& GetVertexArray() const { return m_vertexArray; }

        Mesh(
            std::shared_ptr<IVertexArray> vertexArray,
            std::shared_ptr<IVertexBuffer> vertexBuffer,
            std::shared_ptr<IIndexBuffer> indexBuffer,
            BufferLayout layout,
            u32 vertexCount,
            u32 indexCount,
            PrimitiveTopology topology,
            const Math::Vec3& localBoundsMin,
            const Math::Vec3& localBoundsMax,
            std::string name);

        std::shared_ptr<IVertexArray> m_vertexArray;
        std::shared_ptr<IVertexBuffer> m_vertexBuffer;
        std::shared_ptr<IIndexBuffer> m_indexBuffer;
        BufferLayout m_layout;
        u32 m_vertexCount = 0;
        u32 m_indexCount = 0;
        PrimitiveTopology m_topology = PrimitiveTopology::Triangles;
        Math::Vec3 m_localBoundsMin = Math::Vec3(-0.5f);
        Math::Vec3 m_localBoundsMax = Math::Vec3(0.5f);
        std::string m_name;
    };
}
