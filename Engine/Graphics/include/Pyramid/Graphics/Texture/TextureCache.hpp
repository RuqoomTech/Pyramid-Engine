#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Texture/TextureResource.hpp>

#include <memory>
#include <unordered_map>

namespace Pyramid
{
    class IGraphicsDevice;

    struct TextureCacheStats
    {
        u64 cacheHits = 0;
        u64 cacheMisses = 0;
        u64 texturesCreated = 0;
        u64 creationFailures = 0;
        u64 identifierConflicts = 0;
        u64 reloadAttempts = 0;
        u64 reloadSuccesses = 0;
        u64 reloadFailures = 0;
        u64 reloadReuses = 0;
        u64 evictions = 0;

        u32 residentTextures = 0;
        u32 residentAssetIds = 0;
        u32 externallyReferencedTextures = 0;
        u64 residentBaseLevelBytes = 0;
        u64 estimatedResidentTextureBytes = 0;
    };

    /**
     * @brief Graphics-device-bound cache for immutable TextureResource objects.
     *
     * Exact decoded pixels plus texture/sampler/color-space state are uploaded
     * once. File-backed stable aliases can be reloaded transactionally. The cache
     * is intended for the graphics thread and must be destroyed before the device
     * and graphics context.
     */
    class TextureCache final
    {
    public:
        explicit TextureCache(IGraphicsDevice& device);

        TextureCache(const TextureCache&) = delete;
        TextureCache& operator=(const TextureCache&) = delete;
        TextureCache(TextureCache&&) = delete;
        TextureCache& operator=(TextureCache&&) = delete;
        ~TextureCache() = default;

        std::shared_ptr<TextureResource> GetOrCreate(
            const TextureResourceSpecification& specification);
        std::shared_ptr<TextureResource> GetOrCreate(
            const TextureFileSpecification& specification);

        /** Reload the original file associated with a stable caller-defined ID. */
        bool Reload(TextureAssetId stableAssetId);

        /** Transactionally replace a stable file alias with a new file request. */
        bool Reload(
            TextureAssetId stableAssetId,
            const TextureFileSpecification& replacement);

        std::shared_ptr<TextureResource> Find(TextureAssetId assetId) const;
        bool Contains(TextureAssetId assetId) const;
        bool Evict(TextureAssetId assetId);
        u32 CollectUnused();
        u32 Clear();

        u32 GetResidentCount() const { return static_cast<u32>(m_residents.size()); }
        TextureCacheStats GetStats() const;

    private:
        struct AliasRecord
        {
            TextureAssetId contentId;
            bool hasFileSource = false;
            TextureFileSpecification fileSource;
        };

        struct Resident
        {
            std::shared_ptr<TextureResource> texture;
        };

        std::shared_ptr<TextureResource> GetOrCreatePrepared(
            TextureResource::PreparedData&& prepared,
            TextureAssetId requestedAssetId,
            const TextureFileSpecification* fileSource);
        bool ReloadPrepared(
            TextureAssetId stableAssetId,
            TextureResource::PreparedData&& prepared,
            const TextureFileSpecification& fileSource);
        void RemoveAliasesForContent(TextureAssetId contentId);

        IGraphicsDevice* m_device = nullptr;
        std::unordered_map<TextureAssetId, AliasRecord, TextureAssetIdHash> m_aliases;
        std::unordered_map<TextureAssetId, Resident, TextureAssetIdHash> m_residents;

        u64 m_cacheHits = 0;
        u64 m_cacheMisses = 0;
        u64 m_texturesCreated = 0;
        u64 m_creationFailures = 0;
        u64 m_identifierConflicts = 0;
        u64 m_reloadAttempts = 0;
        u64 m_reloadSuccesses = 0;
        u64 m_reloadFailures = 0;
        u64 m_reloadReuses = 0;
        u64 m_evictions = 0;
    };
}
