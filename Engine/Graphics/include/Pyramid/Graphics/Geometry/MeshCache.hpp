#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>

#include <memory>
#include <unordered_map>

namespace Pyramid
{
    class IGraphicsDevice;

    /**
     * @brief Snapshot of mesh-cache residency and acquisition activity.
     */
    struct MeshCacheStats
    {
        u64 cacheHits = 0;
        u64 cacheMisses = 0;
        u64 meshesCreated = 0;
        u64 creationFailures = 0;
        u64 identifierConflicts = 0;
        u64 evictions = 0;

        u32 residentMeshes = 0;
        u32 residentAssetIds = 0;
        u32 externallyReferencedMeshes = 0;
        u64 residentVertexBytes = 0;
        u64 residentIndexBytes = 0;

        u64 GetResidentGeometryBytes() const
        {
            return residentVertexBytes + residentIndexBytes;
        }
    };

    /**
     * @brief Graphics-device-bound cache for immutable Mesh resources.
     *
     * Exact content is uploaded once even when it is requested through several
     * stable asset identifiers. The cache owns one strong reference per unique
     * geometry resource, so a resident mesh remains alive until it is explicitly
     * evicted, cleared, or removed by CollectUnused(). Existing external
     * shared_ptr instances remain valid after cache eviction.
     *
     * The cache and its meshes must be destroyed before the graphics
     * device/context. MeshCache is intended for use on the graphics thread and is
     * not internally synchronized.
     */
    class MeshCache final
    {
    public:
        explicit MeshCache(IGraphicsDevice& device);

        MeshCache(const MeshCache&) = delete;
        MeshCache& operator=(const MeshCache&) = delete;
        MeshCache(MeshCache&&) = delete;
        MeshCache& operator=(MeshCache&&) = delete;
        ~MeshCache() = default;

        /**
         * @brief Return a resident mesh or upload it once when absent.
         *
         * An invalid specification returns nullptr. Reusing one explicit asset ID
         * with different geometry is rejected while the original entry is resident.
         */
        std::shared_ptr<Mesh> GetOrCreate(const MeshSpecification& specification);

        std::shared_ptr<Mesh> Find(MeshAssetId assetId) const;
        bool Contains(MeshAssetId assetId) const;

        /**
         * @brief Evict the resolved mesh and all identifiers that alias it.
         * @return true when a resident resource was removed.
         */
        bool Evict(MeshAssetId assetId);

        /**
         * @brief Evict resources only owned by the cache.
         * @return number of unique meshes removed.
         */
        u32 CollectUnused();

        /**
         * @brief Evict every resident resource without invalidating external owners.
         * @return number of unique meshes removed.
         */
        u32 Clear();

        u32 GetResidentCount() const { return static_cast<u32>(m_residents.size()); }
        MeshCacheStats GetStats() const;

    private:
        struct Resident
        {
            std::shared_ptr<Mesh> mesh;
        };

        void RemoveAliasesForContent(MeshAssetId contentId);

        IGraphicsDevice* m_device = nullptr;
        std::unordered_map<MeshAssetId, MeshAssetId, MeshAssetIdHash> m_assetToContent;
        std::unordered_map<MeshAssetId, Resident, MeshAssetIdHash> m_residents;

        u64 m_cacheHits = 0;
        u64 m_cacheMisses = 0;
        u64 m_meshesCreated = 0;
        u64 m_creationFailures = 0;
        u64 m_identifierConflicts = 0;
        u64 m_evictions = 0;
    };
}
