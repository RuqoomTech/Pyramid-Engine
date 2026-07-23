#include <Pyramid/Graphics/Material/MaterialCache.hpp>

#include <Pyramid/Util/Log.hpp>

#include <cstddef>

namespace Pyramid
{
    namespace
    {
        u64 EstimateMetadataBytes(const Material& material)
        {
            u64 bytes = sizeof(Material);
            bytes += static_cast<u64>(material.GetName().size());

            for (const auto& binding : material.GetTextures())
            {
                bytes += sizeof(MaterialTextureBinding);
                bytes += static_cast<u64>(binding.uniformName.size());
            }

            for (const auto& uniform : material.GetUniforms())
            {
                bytes += sizeof(MaterialUniform);
                bytes += static_cast<u64>(uniform.name.size());
            }

            return bytes;
        }
    }

    std::shared_ptr<Material> MaterialCache::GetOrCreate(
        const MaterialSpecification& specification)
    {
        const MaterialAssetId contentId = Material::CalculateContentId(specification);
        if (!contentId.IsValid())
        {
            ++m_cacheMisses;
            ++m_creationFailures;
            // Material::Create emits the detailed validation error.
            return Material::Create(specification);
        }

        const MaterialAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : contentId;

        const auto alias = m_assetToContent.find(assetId);
        if (alias != m_assetToContent.end())
        {
            if (alias->second != contentId)
            {
                ++m_identifierConflicts;
                PYRAMID_LOG_ERROR(
                    "Material cache rejected asset identifier ", assetId.ToString(),
                    ": it is already associated with different material content");
                return nullptr;
            }

            const auto resident = m_residents.find(contentId);
            if (resident != m_residents.end())
            {
                ++m_cacheHits;
                return resident->second.material;
            }

            // Repair a stale alias defensively.
            m_assetToContent.erase(alias);
        }

        const auto matchingContent = m_residents.find(contentId);
        if (matchingContent != m_residents.end())
        {
            m_assetToContent.emplace(contentId, contentId);
            m_assetToContent.emplace(assetId, contentId);
            ++m_cacheHits;
            return matchingContent->second.material;
        }

        ++m_cacheMisses;
        auto material = Material::Create(specification);
        if (!material)
        {
            ++m_creationFailures;
            return nullptr;
        }

        m_residents.emplace(contentId, Resident{material});
        m_assetToContent.emplace(contentId, contentId);
        m_assetToContent.emplace(assetId, contentId);
        ++m_materialsCreated;
        return material;
    }

    bool MaterialCache::Replace(
        MaterialAssetId stableAssetId,
        const MaterialSpecification& replacement)
    {
        ++m_replacementAttempts;

        if (!stableAssetId.IsValid())
        {
            ++m_replacementFailures;
            return false;
        }

        const auto alias = m_assetToContent.find(stableAssetId);
        if (alias == m_assetToContent.end())
        {
            ++m_replacementFailures;
            PYRAMID_LOG_ERROR(
                "Material cache cannot replace unknown asset identifier ",
                stableAssetId.ToString());
            return false;
        }

        const MaterialAssetId previousContentId = alias->second;
        if (stableAssetId == previousContentId)
        {
            ++m_replacementFailures;
            PYRAMID_LOG_ERROR(
                "Material cache cannot replace a content-derived identifier; "
                "use a stable caller-defined asset ID");
            return false;
        }

        if (replacement.assetId.IsValid() && replacement.assetId != stableAssetId)
        {
            ++m_identifierConflicts;
            ++m_replacementFailures;
            PYRAMID_LOG_ERROR(
                "Material cache replacement rejected a specification with a different asset identifier");
            return false;
        }

        MaterialSpecification normalized = replacement;
        normalized.assetId = stableAssetId;
        const MaterialAssetId replacementContentId =
            Material::CalculateContentId(normalized);
        if (!replacementContentId.IsValid())
        {
            ++m_replacementFailures;
            // Emit the detailed validation error without changing the active alias.
            (void)Material::Create(normalized);
            return false;
        }

        if (replacementContentId == previousContentId)
        {
            ++m_replacementSuccesses;
            ++m_replacementReuses;
            return true;
        }

        const auto matchingContent = m_residents.find(replacementContentId);
        if (matchingContent != m_residents.end())
        {
            m_assetToContent[stableAssetId] = replacementContentId;
            m_assetToContent.emplace(replacementContentId, replacementContentId);
            ++m_cacheHits;
            ++m_replacementSuccesses;
            ++m_replacementReuses;
            return true;
        }

        auto replacementMaterial = Material::Create(normalized);
        if (!replacementMaterial)
        {
            ++m_creationFailures;
            ++m_replacementFailures;
            return false;
        }

        m_residents.emplace(replacementContentId, Resident{replacementMaterial});
        m_assetToContent.emplace(replacementContentId, replacementContentId);
        m_assetToContent[stableAssetId] = replacementContentId;
        ++m_materialsCreated;
        ++m_replacementSuccesses;
        return true;
    }

    std::shared_ptr<Material> MaterialCache::Find(MaterialAssetId assetId) const
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
        return resident == m_residents.end() ? nullptr : resident->second.material;
    }

    bool MaterialCache::Contains(MaterialAssetId assetId) const
    {
        return Find(assetId) != nullptr;
    }

    bool MaterialCache::Evict(MaterialAssetId assetId)
    {
        const auto alias = m_assetToContent.find(assetId);
        if (alias == m_assetToContent.end())
        {
            return false;
        }

        const MaterialAssetId contentId = alias->second;
        const auto removed = m_residents.erase(contentId);
        RemoveAliasesForContent(contentId);
        m_evictions += removed;
        return removed != 0;
    }

    u32 MaterialCache::CollectUnused()
    {
        u32 removed = 0;
        for (auto iterator = m_residents.begin(); iterator != m_residents.end();)
        {
            if (iterator->second.material && iterator->second.material.use_count() == 1)
            {
                const MaterialAssetId contentId = iterator->first;
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

    u32 MaterialCache::Clear()
    {
        const u32 removed = static_cast<u32>(m_residents.size());
        m_residents.clear();
        m_assetToContent.clear();
        m_evictions += removed;
        return removed;
    }

    MaterialCacheStats MaterialCache::GetStats() const
    {
        MaterialCacheStats stats;
        stats.cacheHits = m_cacheHits;
        stats.cacheMisses = m_cacheMisses;
        stats.materialsCreated = m_materialsCreated;
        stats.creationFailures = m_creationFailures;
        stats.identifierConflicts = m_identifierConflicts;
        stats.replacementAttempts = m_replacementAttempts;
        stats.replacementSuccesses = m_replacementSuccesses;
        stats.replacementFailures = m_replacementFailures;
        stats.replacementReuses = m_replacementReuses;
        stats.evictions = m_evictions;
        stats.residentMaterials = static_cast<u32>(m_residents.size());
        stats.residentAssetIds = static_cast<u32>(m_assetToContent.size());

        for (const auto& pair : m_residents)
        {
            const auto& material = pair.second.material;
            if (!material)
            {
                continue;
            }

            if (material.use_count() > 1)
            {
                ++stats.externallyReferencedMaterials;
            }
            stats.residentTextureBindings +=
                static_cast<u32>(material->GetTextures().size());
            stats.residentUniformValues +=
                static_cast<u32>(material->GetUniforms().size());
            stats.estimatedResidentMetadataBytes += EstimateMetadataBytes(*material);
        }

        return stats;
    }

    void MaterialCache::RemoveAliasesForContent(MaterialAssetId contentId)
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
