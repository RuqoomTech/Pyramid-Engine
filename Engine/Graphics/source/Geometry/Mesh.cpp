#include <Pyramid/Graphics/Geometry/Mesh.hpp>

#include <Pyramid/Graphics/Buffer/IndexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>

#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

namespace Pyramid
{
    namespace
    {
        constexpr u64 kFnvPrime = 1099511628211ull;
        constexpr u64 kPrimaryOffset = 14695981039346656037ull;
        constexpr u64 kSecondaryOffset = 7809847782465536322ull;

        class StableHasher128
        {
        public:
            void AddByte(u8 value)
            {
                m_high ^= value;
                m_high *= kFnvPrime;

                m_low ^= static_cast<u8>(value ^ 0xa5u);
                m_low *= kFnvPrime;
                m_low ^= m_low >> 32u;
            }

            void AddBytes(const void* data, std::size_t size)
            {
                const auto* bytes = static_cast<const u8*>(data);
                for (std::size_t index = 0; index < size; ++index)
                {
                    AddByte(bytes[index]);
                }
            }

            void AddU32(u32 value)
            {
                for (u32 shift = 0; shift < 32; shift += 8)
                {
                    AddByte(static_cast<u8>((value >> shift) & 0xffu));
                }
            }

            void AddString(std::string_view value)
            {
                AddU32(static_cast<u32>(value.size()));
                AddBytes(value.data(), value.size());
            }

            MeshAssetId Finish() const
            {
                MeshAssetId result{m_high, m_low};
                if (!result.IsValid())
                {
                    result.low = 1;
                }
                return result;
            }

        private:
            u64 m_high = kPrimaryOffset;
            u64 m_low = kSecondaryOffset;
        };

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

        bool ValidateSpecification(
            const MeshSpecification& specification,
            Math::Vec3* localBoundsMin,
            Math::Vec3* localBoundsMax,
            bool logErrors)
        {
            if (!specification.vertexData || specification.vertexDataSize == 0 ||
                specification.vertexCount == 0)
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Mesh creation failed: vertex data and vertex count are required");
                }
                return false;
            }

            const u32 stride = specification.layout.GetStride();
            if (stride == 0 || specification.layout.GetElements().empty())
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR("Mesh creation failed: vertex layout is empty");
                }
                return false;
            }

            const auto requiredBytes =
                static_cast<unsigned long long>(stride) * specification.vertexCount;
            if (requiredBytes > std::numeric_limits<u32>::max() ||
                requiredBytes != specification.vertexDataSize)
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Mesh creation failed: vertexDataSize does not match vertexCount * layout stride");
                }
                return false;
            }

            const bool indexed = specification.indexCount > 0;
            if (indexed != (specification.indexData != nullptr))
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Mesh creation failed: indexData and indexCount must either both be supplied or both be empty");
                }
                return false;
            }

            if (indexed)
            {
                for (u32 index = 0; index < specification.indexCount; ++index)
                {
                    if (specification.indexData[index] >= specification.vertexCount)
                    {
                        if (logErrors)
                        {
                            PYRAMID_LOG_ERROR(
                                "Mesh creation failed: index ", specification.indexData[index],
                                " is outside vertex count ", specification.vertexCount);
                        }
                        return false;
                    }
                }
            }

            const u32 drawCount = indexed ? specification.indexCount : specification.vertexCount;
            if (!IsTopologyCountValid(specification.topology, drawCount))
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Mesh creation failed: draw count is invalid for the selected topology");
                }
                return false;
            }

            Math::Vec3 minimum;
            Math::Vec3 maximum;
            if (!Geometry::CalculateLocalBounds(
                    specification.vertexData,
                    specification.vertexDataSize,
                    specification.layout,
                    minimum,
                    maximum))
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Mesh creation failed: finite position data with a recognized position semantic is required");
                }
                return false;
            }

            if (localBoundsMin)
            {
                *localBoundsMin = minimum;
            }
            if (localBoundsMax)
            {
                *localBoundsMax = maximum;
            }
            return true;
        }

        MeshAssetId HashSpecification(const MeshSpecification& specification)
        {
            StableHasher128 hasher;
            hasher.AddString("Pyramid.Mesh.Content.v1");
            hasher.AddU32(specification.vertexDataSize);
            hasher.AddU32(specification.vertexCount);
            hasher.AddU32(specification.layout.GetStride());
            hasher.AddU32(static_cast<u32>(specification.layout.GetElements().size()));

            for (const auto& element : specification.layout.GetElements())
            {
                hasher.AddString(element.Name);
                hasher.AddU32(static_cast<u32>(element.Type));
                hasher.AddU32(element.Size);
                hasher.AddU32(element.Offset);
                hasher.AddByte(element.Normalized ? 1u : 0u);
            }

            hasher.AddU32(static_cast<u32>(specification.topology));
            hasher.AddBytes(specification.vertexData, specification.vertexDataSize);
            hasher.AddU32(specification.indexCount);
            for (u32 index = 0; index < specification.indexCount; ++index)
            {
                hasher.AddU32(specification.indexData[index]);
            }

            return hasher.Finish();
        }
    }

    MeshAssetId MeshAssetId::FromString(std::string_view stableName)
    {
        if (stableName.empty())
        {
            return {};
        }

        StableHasher128 hasher;
        hasher.AddString("Pyramid.Mesh.Asset.v1");
        hasher.AddString(stableName);
        return hasher.Finish();
    }

    std::string MeshAssetId::ToString() const
    {
        if (!IsValid())
        {
            return {};
        }

        std::ostringstream stream;
        stream << std::hex << std::setfill('0')
               << std::setw(16) << high
               << std::setw(16) << low;
        return stream.str();
    }

    std::size_t MeshAssetIdHash::operator()(const MeshAssetId& identifier) const noexcept
    {
        const u64 mixed = identifier.high ^
            (identifier.low + 0x9e3779b97f4a7c15ull +
             (identifier.high << 6u) + (identifier.high >> 2u));
        return static_cast<std::size_t>(mixed);
    }

    MeshAssetId Mesh::CalculateContentId(const MeshSpecification& specification)
    {
        if (!ValidateSpecification(specification, nullptr, nullptr, false))
        {
            return {};
        }
        return HashSpecification(specification);
    }

    std::shared_ptr<Mesh> Mesh::Create(
        IGraphicsDevice& device,
        const MeshSpecification& specification)
    {
        Math::Vec3 localBoundsMin;
        Math::Vec3 localBoundsMax;
        if (!ValidateSpecification(
                specification,
                &localBoundsMin,
                &localBoundsMax,
                true))
        {
            return nullptr;
        }

        const MeshAssetId contentId = HashSpecification(specification);
        const MeshAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : contentId;

        try
        {
            auto vertexBuffer = device.CreateVertexBuffer();
            auto vertexArray = device.CreateVertexArray();
            if (!vertexBuffer || !vertexArray)
            {
                PYRAMID_LOG_ERROR(
                    "Mesh creation failed: graphics device could not allocate vertex resources");
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
                    PYRAMID_LOG_ERROR(
                        "Mesh creation failed: graphics device could not allocate an index buffer");
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
                specification.vertexDataSize,
                specification.vertexCount,
                specification.indexCount,
                specification.topology,
                assetId,
                contentId,
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
        u32 vertexDataSize,
        u32 vertexCount,
        u32 indexCount,
        PrimitiveTopology topology,
        MeshAssetId assetId,
        MeshAssetId contentId,
        const Math::Vec3& localBoundsMin,
        const Math::Vec3& localBoundsMax,
        std::string name)
        : m_vertexArray(std::move(vertexArray)),
          m_vertexBuffer(std::move(vertexBuffer)),
          m_indexBuffer(std::move(indexBuffer)),
          m_layout(std::move(layout)),
          m_vertexDataSize(vertexDataSize),
          m_vertexCount(vertexCount),
          m_indexCount(indexCount),
          m_topology(topology),
          m_assetId(assetId),
          m_contentId(contentId),
          m_localBoundsMin(localBoundsMin),
          m_localBoundsMax(localBoundsMax),
          m_name(std::move(name))
    {
    }

    bool Mesh::IsValid() const
    {
        return m_assetId.IsValid() && m_contentId.IsValid() &&
               m_vertexArray && m_vertexBuffer && m_vertexCount > 0 &&
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
