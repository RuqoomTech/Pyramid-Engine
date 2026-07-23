#include <Pyramid/Graphics/Geometry/MeshCache.hpp>

#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>

namespace Pyramid
{
    MeshCache::MeshCache(IGraphicsDevice& device)
        : m_device(&device)
    {
    }

    std::shared_ptr<Mesh> MeshCache::GetOrCreate(const MeshSpecification& specification)
    {
        const MeshAssetId contentId = Mesh::CalculateContentId(specification);
        if (!contentId.IsValid())
        {
            ++m_cacheMisses;
            ++m_creationFailures;
            // Mesh::Create emits the detailed validation error.
            return Mesh::Create(*m_device, specification);
        }

        const MeshAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : contentId;

        const auto alias = m_assetToContent.find(assetId);
        if (alias != m_assetToContent.end())
        {
            if (alias->second != contentId)
            {
                ++m_identifierConflicts;
                PYRAMID_LOG_ERROR(
                    "Mesh cache rejected asset identifier ", assetId.ToString(),
                    ": it is already associated with different geometry");
                return nullptr;
            }

            const auto resident = m_residents.find(contentId);
            if (resident != m_residents.end())
            {
                ++m_cacheHits;
                return resident->second.mesh;
            }

            // Repair an impossible stale alias defensively.
            m_assetToContent.erase(alias);
        }

        const auto matchingContent = m_residents.find(contentId);
        if (matchingContent != m_residents.end())
        {
            m_assetToContent.emplace(contentId, contentId);
            m_assetToContent.emplace(assetId, contentId);
            ++m_cacheHits;
            return matchingContent->second.mesh;
        }

        ++m_cacheMisses;
        auto mesh = Mesh::Create(*m_device, specification);
        if (!mesh)
        {
            ++m_creationFailures;
            return nullptr;
        }

        m_residents.emplace(contentId, Resident{mesh});
        m_assetToContent.emplace(contentId, contentId);
        m_assetToContent.emplace(assetId, contentId);
        ++m_meshesCreated;
        return mesh;
    }

    std::shared_ptr<Mesh> MeshCache::Find(MeshAssetId assetId) const
    {
        if (!assetId.IsValid())
        {
            return nullptr;
        }

        const auto alias = m_assetToContent.find(assetId);
        if (alias == m_assetToContent.end())
        {
            return nullptr;
        }

        const auto resident = m_residents.find(alias->second);
        return resident == m_residents.end() ? nullptr : resident->second.mesh;
    }

    bool MeshCache::Contains(MeshAssetId assetId) const
    {
        return Find(assetId) != nullptr;
    }

    bool MeshCache::Evict(MeshAssetId assetId)
    {
        const auto alias = m_assetToContent.find(assetId);
        if (alias == m_assetToContent.end())
        {
            return false;
        }

        const MeshAssetId contentId = alias->second;
        const auto removed = m_residents.erase(contentId);
        RemoveAliasesForContent(contentId);
        m_evictions += removed;
        return removed != 0;
    }

    u32 MeshCache::CollectUnused()
    {
        u32 removed = 0;
        for (auto iterator = m_residents.begin(); iterator != m_residents.end();)
        {
            if (iterator->second.mesh && iterator->second.mesh.use_count() == 1)
            {
                const MeshAssetId contentId = iterator->first;
                iterator = m_residents.erase(iterator);
                RemoveAliasesForContent(contentId);
                ++removed;
            }
            else
            {
                ++iterator;
            }
        }

        m_evictions += removed;
        return removed;
    }

    u32 MeshCache::Clear()
    {
        const u32 removed = static_cast<u32>(m_residents.size());
        m_residents.clear();
        m_assetToContent.clear();
        m_evictions += removed;
        return removed;
    }

    MeshCacheStats MeshCache::GetStats() const
    {
        MeshCacheStats stats;
        stats.cacheHits = m_cacheHits;
        stats.cacheMisses = m_cacheMisses;
        stats.meshesCreated = m_meshesCreated;
        stats.creationFailures = m_creationFailures;
        stats.identifierConflicts = m_identifierConflicts;
        stats.evictions = m_evictions;
        stats.residentMeshes = static_cast<u32>(m_residents.size());
        stats.residentAssetIds = static_cast<u32>(m_assetToContent.size());

        for (const auto& pair : m_residents)
        {
            const auto& mesh = pair.second.mesh;
            if (!mesh)
            {
                continue;
            }

            if (mesh.use_count() > 1)
            {
                ++stats.externallyReferencedMeshes;
            }
            stats.residentVertexBytes += mesh->GetVertexDataSize();
            stats.residentIndexBytes += mesh->GetIndexDataSize();
        }

        return stats;
    }

    void MeshCache::RemoveAliasesForContent(MeshAssetId contentId)
    {
        for (auto iterator = m_assetToContent.begin(); iterator != m_assetToContent.end();)
        {
            if (iterator->second == contentId)
            {
                iterator = m_assetToContent.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }
}
