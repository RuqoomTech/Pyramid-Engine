#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>

#include <memory>
#include <unordered_map>

namespace Pyramid
{
    /** Snapshot of material-cache residency and acquisition activity. */
    struct MaterialCacheStats
    {
        u64 cacheHits = 0;
        u64 cacheMisses = 0;
        u64 materialsCreated = 0;
        u64 creationFailures = 0;
        u64 identifierConflicts = 0;
        u64 replacementAttempts = 0;
        u64 replacementSuccesses = 0;
        u64 replacementFailures = 0;
        u64 replacementReuses = 0;
        u64 evictions = 0;

        u32 residentMaterials = 0;
        u32 residentAssetIds = 0;
        u32 externallyReferencedMaterials = 0;
        u32 residentTextureBindings = 0;
        u32 residentUniformValues = 0;
        u64 estimatedResidentMetadataBytes = 0;
    };

    /**
     * Cache for immutable Material resources.
     *
     * Exact material content is represented by one resident object even when it
     * is addressed through several stable aliases. Replace() publishes a new
     * material only after validation and creation succeed, so failure preserves
     * the previous stable mapping and all existing external owners.
     *
     * MaterialCache is intended for the graphics thread and is not internally
     * synchronized. Destroy it before the shader and texture caches whose
     * resources its resident materials reference.
     */
    class MaterialCache final
    {
    public:
        MaterialCache() = default;

        MaterialCache(const MaterialCache&) = delete;
        MaterialCache& operator=(const MaterialCache&) = delete;
        MaterialCache(MaterialCache&&) = delete;
        MaterialCache& operator=(MaterialCache&&) = delete;
        ~MaterialCache() = default;

        /** Return an exact resident material or create one when absent. */
        std::shared_ptr<Material> GetOrCreate(const MaterialSpecification& specification);

        /**
         * Transactionally remap a caller-defined stable identifier.
         * Content-derived identifiers are immutable and cannot be replaced.
         */
        bool Replace(
            MaterialAssetId stableAssetId,
            const MaterialSpecification& replacement);

        std::shared_ptr<Material> Find(MaterialAssetId assetId) const;
        bool Contains(MaterialAssetId assetId) const;

        /** Evict the resolved material and every alias that references it. */
        bool Evict(MaterialAssetId assetId);

        /** Evict materials only owned by the cache. */
        u32 CollectUnused();

        /** Evict every resident material without invalidating external owners. */
        u32 Clear();

        u32 GetResidentCount() const { return static_cast<u32>(m_residents.size()); }
        MaterialCacheStats GetStats() const;

    private:
        struct Resident
        {
            std::shared_ptr<Material> material;
        };

        void RemoveAliasesForContent(MaterialAssetId contentId);

        std::unordered_map<MaterialAssetId, MaterialAssetId, MaterialAssetIdHash>
            m_assetToContent;
        std::unordered_map<MaterialAssetId, Resident, MaterialAssetIdHash> m_residents;

        u64 m_cacheHits = 0;
        u64 m_cacheMisses = 0;
        u64 m_materialsCreated = 0;
        u64 m_creationFailures = 0;
        u64 m_identifierConflicts = 0;
        u64 m_replacementAttempts = 0;
        u64 m_replacementSuccesses = 0;
        u64 m_replacementFailures = 0;
        u64 m_replacementReuses = 0;
        u64 m_evictions = 0;
    };
}
