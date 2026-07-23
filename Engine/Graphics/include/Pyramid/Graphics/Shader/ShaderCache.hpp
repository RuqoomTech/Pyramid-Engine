#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>

#include <memory>
#include <unordered_map>

namespace Pyramid
{
    class IGraphicsDevice;

    struct ShaderCacheStats
    {
        u64 cacheHits = 0;
        u64 cacheMisses = 0;
        u64 programsCreated = 0;
        u64 compilationFailures = 0;
        u64 identifierConflicts = 0;
        u64 recompilationAttempts = 0;
        u64 recompilationSuccesses = 0;
        u64 recompilationFailures = 0;
        u64 recompilationReuses = 0;
        u64 evictions = 0;

        u32 residentPrograms = 0;
        u32 residentAssetIds = 0;
        u32 externallyReferencedPrograms = 0;
        u64 residentSourceBytes = 0;
    };

    /**
     * Graphics-device-bound cache for immutable ShaderProgram resources.
     *
     * Exact source sets compile once and may be referenced through multiple stable
     * asset identifiers. Recompile() compiles a replacement before changing the
     * requested stable alias, so failure leaves the old program fully intact.
     * Existing external owners of the old program remain valid after replacement.
     */
    class ShaderCache final
    {
    public:
        explicit ShaderCache(IGraphicsDevice& device);

        ShaderCache(const ShaderCache&) = delete;
        ShaderCache& operator=(const ShaderCache&) = delete;
        ShaderCache(ShaderCache&&) = delete;
        ShaderCache& operator=(ShaderCache&&) = delete;
        ~ShaderCache() = default;

        std::shared_ptr<ShaderProgram> GetOrCreate(
            const ShaderProgramSpecification& specification);

        /**
         * Transactionally remap one stable asset identifier to replacement source.
         * Content-derived identifiers are immutable and cannot be recompiled.
         */
        bool Recompile(
            ShaderAssetId stableAssetId,
            const ShaderProgramSpecification& replacement);

        std::shared_ptr<ShaderProgram> Find(ShaderAssetId assetId) const;
        bool Contains(ShaderAssetId assetId) const;
        bool Evict(ShaderAssetId assetId);
        u32 CollectUnused();
        u32 Clear();

        u32 GetResidentCount() const { return static_cast<u32>(m_residents.size()); }
        ShaderCacheStats GetStats() const;

    private:
        struct Resident
        {
            std::shared_ptr<ShaderProgram> program;
        };

        void RemoveAliasesForContent(ShaderAssetId contentId);

        IGraphicsDevice* m_device = nullptr;
        std::unordered_map<ShaderAssetId, ShaderAssetId, ShaderAssetIdHash> m_assetToContent;
        std::unordered_map<ShaderAssetId, Resident, ShaderAssetIdHash> m_residents;

        u64 m_cacheHits = 0;
        u64 m_cacheMisses = 0;
        u64 m_programsCreated = 0;
        u64 m_compilationFailures = 0;
        u64 m_identifierConflicts = 0;
        u64 m_recompilationAttempts = 0;
        u64 m_recompilationSuccesses = 0;
        u64 m_recompilationFailures = 0;
        u64 m_recompilationReuses = 0;
        u64 m_evictions = 0;
    };
}
