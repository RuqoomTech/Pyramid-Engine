#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Util/Log.hpp>

#include "TestGraphicsDevice.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

namespace
{
    using Pyramid::Mesh;
    using Pyramid::MeshAssetId;
    using Pyramid::MeshCache;
    using Pyramid::MeshSpecification;
    using Pyramid::PrimitiveTopology;
    using Pyramid::ShaderDataType;
    using Pyramid::u32;

    struct Vertex
    {
        float position[3];
        float color[4];
    };

    int Fail(const char* message)
    {
        std::cerr << "MeshCacheTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    MeshSpecification MakeSpecification(
        const std::vector<Vertex>& vertices,
        const std::vector<u32>& indices,
        const char* name)
    {
        MeshSpecification specification;
        specification.vertexData = vertices.data();
        specification.vertexDataSize = static_cast<u32>(vertices.size() * sizeof(Vertex));
        specification.vertexCount = static_cast<u32>(vertices.size());
        specification.layout = {
            {ShaderDataType::Float3, "a_Position"},
            {ShaderDataType::Float4, "a_Color"},
        };
        specification.indexData = indices.data();
        specification.indexCount = static_cast<u32>(indices.size());
        specification.topology = PrimitiveTopology::Triangles;
        specification.name = name;
        return specification;
    }
}

int main()
{
    Pyramid::Util::Logger::GetInstance().EnableConsole(false);
    Pyramid::Tests::TestGraphicsDevice device;
    MeshCache cache(device);

    const std::vector<Vertex> vertices = {
        {{-1.0f, -1.0f, 0.0f}, {1, 0, 0, 1}},
        {{1.0f, -1.0f, 0.0f}, {0, 1, 0, 1}},
        {{0.0f, 1.0f, 0.0f}, {0, 0, 1, 1}},
    };
    const std::vector<u32> indices = {0, 1, 2};

    auto specification = MakeSpecification(vertices, indices, "FirstDebugName");
    const MeshAssetId contentId = Mesh::CalculateContentId(specification);
    if (!contentId.IsValid() ||
        contentId.ToString() != "5399001e73e3d32a9c80f11f1f344c04")
    {
        return Fail("content identifier is invalid or unstable");
    }

    auto renamedSpecification = MakeSpecification(vertices, indices, "DifferentDebugName");
    if (Mesh::CalculateContentId(renamedSpecification) != contentId)
    {
        return Fail("debug names changed the content identifier");
    }

    auto changedVertices = vertices;
    changedVertices[0].position[0] = -2.0f;
    auto changedSpecification = MakeSpecification(changedVertices, indices, "ChangedGeometry");
    if (Mesh::CalculateContentId(changedSpecification) == contentId)
    {
        return Fail("different geometry produced the same content identifier");
    }

    const MeshAssetId stableId = MeshAssetId::FromString("builtin/triangle");
    if (!stableId.IsValid() ||
        stableId.ToString() != "f56c9823ef7037705920beeca3226f41" ||
        stableId != MeshAssetId::FromString("builtin/triangle") ||
        stableId == MeshAssetId::FromString("builtin/other") ||
        MeshAssetId::FromString({}).IsValid())
    {
        return Fail("stable string identifiers are not deterministic");
    }

    specification.assetId = stableId;
    auto first = cache.GetOrCreate(specification);
    if (!first || first->GetAssetId() != stableId || first->GetContentId() != contentId)
    {
        return Fail("explicitly identified mesh was not created correctly");
    }
    if (device.vertexBufferCreations != 1 ||
        device.indexBufferCreations != 1 ||
        device.vertexArrayCreations != 1)
    {
        return Fail("first mesh upload did not allocate exactly one resource set");
    }

    // The same exact content requested without an explicit ID must reuse the
    // resident upload and become findable through its content identifier.
    auto contentAlias = cache.GetOrCreate(renamedSpecification);
    if (contentAlias != first || cache.Find(contentId) != first || !cache.Contains(stableId))
    {
        return Fail("identical geometry was not shared across asset identifiers");
    }
    if (device.vertexBufferCreations != 1 ||
        device.indexBufferCreations != 1 ||
        device.vertexArrayCreations != 1)
    {
        return Fail("cache hit uploaded duplicate GPU geometry");
    }

    // Reusing an explicit ID for different bytes is a hard conflict while the
    // original asset remains resident.
    changedSpecification.assetId = stableId;
    if (cache.GetOrCreate(changedSpecification))
    {
        return Fail("identifier conflict was accepted");
    }
    if (device.vertexBufferCreations != 1)
    {
        return Fail("identifier conflict allocated GPU resources");
    }

    auto stats = cache.GetStats();
    if (stats.cacheHits != 1 || stats.cacheMisses != 1 ||
        stats.meshesCreated != 1 || stats.creationFailures != 0 ||
        stats.identifierConflicts != 1 || stats.residentMeshes != 1 ||
        stats.residentAssetIds != 2 || stats.externallyReferencedMeshes != 1 ||
        stats.residentVertexBytes != specification.vertexDataSize ||
        stats.residentIndexBytes != specification.indexCount * sizeof(u32))
    {
        return Fail("cache statistics are incorrect after sharing");
    }

    // Strong cache ownership keeps the resource alive until explicit collection.
    first.reset();
    contentAlias.reset();
    auto retained = cache.Find(stableId);
    if (!retained || cache.CollectUnused() != 0)
    {
        return Fail("externally referenced resource was collected");
    }
    retained.reset();
    if (cache.CollectUnused() != 1 || cache.GetResidentCount() != 0 ||
        cache.Contains(stableId) || cache.Contains(contentId))
    {
        return Fail("cache-only resource was not collected with all aliases");
    }

    auto recreated = cache.GetOrCreate(specification);
    if (!recreated || device.vertexBufferCreations != 2 ||
        device.indexBufferCreations != 2 || device.vertexArrayCreations != 2)
    {
        return Fail("evicted mesh was not recreated exactly once");
    }

    // Eviction releases cache ownership but not existing shared_ptr owners.
    if (!cache.Evict(contentId) || cache.GetResidentCount() != 0 ||
        !recreated->IsValid() || cache.Evict(contentId))
    {
        return Fail("explicit eviction lifetime is incorrect");
    }

    auto second = cache.GetOrCreate(specification);
    auto otherSpecification = MakeSpecification(changedVertices, indices, "Other");
    auto other = cache.GetOrCreate(otherSpecification);
    if (!second || !other || cache.GetResidentCount() != 2)
    {
        return Fail("multiple resident resources were not created");
    }
    if (cache.Clear() != 2 || cache.GetResidentCount() != 0 ||
        !second->IsValid() || !other->IsValid())
    {
        return Fail("cache clear invalidated external mesh owners");
    }

    auto invalid = specification;
    invalid.vertexData = nullptr;
    if (cache.GetOrCreate(invalid))
    {
        return Fail("invalid specification was cached");
    }

    stats = cache.GetStats();
    if (stats.meshesCreated != 4 || stats.creationFailures != 1 ||
        stats.evictions != 4 || stats.residentMeshes != 0)
    {
        return Fail("final cache statistics are incorrect");
    }

    return EXIT_SUCCESS;
}
