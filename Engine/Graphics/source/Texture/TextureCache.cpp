#include <Pyramid/Graphics/Texture/TextureCache.hpp>

#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>

#include <utility>

namespace Pyramid
{
    TextureCache::TextureCache(IGraphicsDevice& device)
        : m_device(&device)
    {
    }

    std::shared_ptr<TextureResource> TextureCache::GetOrCreate(
        const TextureResourceSpecification& specification)
    {
        TextureResource::PreparedData prepared;
        std::string error;
        if (!TextureResource::Prepare(specification, prepared, error))
        {
            ++m_cacheMisses;
            ++m_creationFailures;
            PYRAMID_LOG_ERROR("Texture cache rejected memory texture: ", error);
            return nullptr;
        }
        return GetOrCreatePrepared(
            std::move(prepared),
            specification.assetId,
            nullptr);
    }

    std::shared_ptr<TextureResource> TextureCache::GetOrCreate(
        const TextureFileSpecification& specification)
    {
        TextureResource::PreparedData prepared;
        std::string error;
        if (!TextureResource::Prepare(specification, prepared, error))
        {
            ++m_cacheMisses;
            ++m_creationFailures;
            PYRAMID_LOG_ERROR("Texture cache rejected file texture: ", error);
            return nullptr;
        }
        return GetOrCreatePrepared(
            std::move(prepared),
            specification.assetId,
            &specification);
    }

    std::shared_ptr<TextureResource> TextureCache::GetOrCreatePrepared(
        TextureResource::PreparedData&& prepared,
        TextureAssetId requestedAssetId,
        const TextureFileSpecification* fileSource)
    {
        const TextureAssetId contentId = prepared.contentId;
        const TextureAssetId assetId = requestedAssetId.IsValid()
            ? requestedAssetId
            : contentId;

        const auto alias = m_aliases.find(assetId);
        if (alias != m_aliases.end())
        {
            if (alias->second.contentId != contentId)
            {
                ++m_identifierConflicts;
                PYRAMID_LOG_ERROR(
                    "Texture cache rejected asset identifier ", assetId.ToString(),
                    ": it is already associated with different pixels or texture state");
                return nullptr;
            }
            const auto resident = m_residents.find(contentId);
            if (resident != m_residents.end())
            {
                ++m_cacheHits;
                return resident->second.texture;
            }
            m_aliases.erase(alias);
        }

        const auto matching = m_residents.find(contentId);
        if (matching != m_residents.end())
        {
            m_aliases.emplace(contentId, AliasRecord{contentId, false, {}});
            AliasRecord requested{contentId, fileSource != nullptr, {}};
            if (fileSource)
            {
                requested.fileSource = *fileSource;
            }
            m_aliases[assetId] = std::move(requested);
            ++m_cacheHits;
            return matching->second.texture;
        }

        ++m_cacheMisses;
        auto texture = TextureResource::CreatePrepared(*m_device, std::move(prepared), assetId);
        if (!texture)
        {
            ++m_creationFailures;
            return nullptr;
        }

        m_residents.emplace(contentId, Resident{texture});
        m_aliases.emplace(contentId, AliasRecord{contentId, false, {}});
        AliasRecord requested{contentId, fileSource != nullptr, {}};
        if (fileSource)
        {
            requested.fileSource = *fileSource;
        }
        m_aliases[assetId] = std::move(requested);
        ++m_texturesCreated;
        return texture;
    }

    bool TextureCache::Reload(TextureAssetId stableAssetId)
    {
        ++m_reloadAttempts;
        const auto alias = m_aliases.find(stableAssetId);
        if (alias == m_aliases.end() || !alias->second.hasFileSource)
        {
            ++m_reloadFailures;
            PYRAMID_LOG_ERROR(
                "Texture cache cannot reload unknown or non-file asset identifier ",
                stableAssetId.ToString());
            return false;
        }
        if (stableAssetId == alias->second.contentId)
        {
            ++m_reloadFailures;
            PYRAMID_LOG_ERROR(
                "Texture cache cannot reload a content-derived identifier; use a stable caller-defined asset ID");
            return false;
        }

        const TextureFileSpecification source = alias->second.fileSource;
        TextureResource::PreparedData prepared;
        std::string error;
        if (!TextureResource::Prepare(source, prepared, error))
        {
            ++m_reloadFailures;
            PYRAMID_LOG_ERROR("Texture cache reload failed: ", error);
            return false;
        }
        return ReloadPrepared(stableAssetId, std::move(prepared), source);
    }

    bool TextureCache::Reload(
        TextureAssetId stableAssetId,
        const TextureFileSpecification& replacement)
    {
        ++m_reloadAttempts;
        if (!stableAssetId.IsValid())
        {
            ++m_reloadFailures;
            return false;
        }
        const auto alias = m_aliases.find(stableAssetId);
        if (alias == m_aliases.end() || stableAssetId == alias->second.contentId)
        {
            ++m_reloadFailures;
            PYRAMID_LOG_ERROR(
                "Texture cache reload requires an existing caller-defined stable asset ID");
            return false;
        }
        if (replacement.assetId.IsValid() && replacement.assetId != stableAssetId)
        {
            ++m_reloadFailures;
            ++m_identifierConflicts;
            PYRAMID_LOG_ERROR(
                "Texture cache reload rejected a replacement with a different asset identifier");
            return false;
        }

        TextureFileSpecification normalized = replacement;
        normalized.assetId = stableAssetId;
        TextureResource::PreparedData prepared;
        std::string error;
        if (!TextureResource::Prepare(normalized, prepared, error))
        {
            ++m_reloadFailures;
            PYRAMID_LOG_ERROR("Texture cache reload failed: ", error);
            return false;
        }
        return ReloadPrepared(stableAssetId, std::move(prepared), normalized);
    }

    bool TextureCache::ReloadPrepared(
        TextureAssetId stableAssetId,
        TextureResource::PreparedData&& prepared,
        const TextureFileSpecification& fileSource)
    {
        auto alias = m_aliases.find(stableAssetId);
        if (alias == m_aliases.end())
        {
            ++m_reloadFailures;
            return false;
        }

        const TextureAssetId previousContentId = alias->second.contentId;
        const TextureAssetId replacementContentId = prepared.contentId;
        if (replacementContentId == previousContentId)
        {
            alias->second.hasFileSource = true;
            alias->second.fileSource = fileSource;
            ++m_reloadSuccesses;
            ++m_reloadReuses;
            return true;
        }

        const auto existing = m_residents.find(replacementContentId);
        if (existing != m_residents.end())
        {
            alias->second.contentId = replacementContentId;
            alias->second.hasFileSource = true;
            alias->second.fileSource = fileSource;
            m_aliases.emplace(
                replacementContentId,
                AliasRecord{replacementContentId, false, {}});
            ++m_cacheHits;
            ++m_reloadSuccesses;
            ++m_reloadReuses;
            return true;
        }

        auto replacement = TextureResource::CreatePrepared(
            *m_device,
            std::move(prepared),
            stableAssetId);
        if (!replacement)
        {
            ++m_creationFailures;
            ++m_reloadFailures;
            return false;
        }

        m_residents.emplace(replacementContentId, Resident{replacement});
        m_aliases.emplace(
            replacementContentId,
            AliasRecord{replacementContentId, false, {}});
        alias = m_aliases.find(stableAssetId);
        alias->second.contentId = replacementContentId;
        alias->second.hasFileSource = true;
        alias->second.fileSource = fileSource;
        ++m_texturesCreated;
        ++m_reloadSuccesses;
        return true;
    }

    std::shared_ptr<TextureResource> TextureCache::Find(TextureAssetId assetId) const
    {
        if (!assetId.IsValid())
        {
            return nullptr;
        }
        const auto alias = m_aliases.find(assetId);
        if (alias == m_aliases.end())
        {
            return nullptr;
        }
        const auto resident = m_residents.find(alias->second.contentId);
        return resident == m_residents.end() ? nullptr : resident->second.texture;
    }

    bool TextureCache::Contains(TextureAssetId assetId) const
    {
        return Find(assetId) != nullptr;
    }

    bool TextureCache::Evict(TextureAssetId assetId)
    {
        const auto alias = m_aliases.find(assetId);
        if (alias == m_aliases.end())
        {
            return false;
        }
        const TextureAssetId contentId = alias->second.contentId;
        const auto removed = m_residents.erase(contentId);
        RemoveAliasesForContent(contentId);
        m_evictions += removed;
        return removed != 0;
    }

    u32 TextureCache::CollectUnused()
    {
        u32 removed = 0;
        for (auto iterator = m_residents.begin(); iterator != m_residents.end();)
        {
            if (iterator->second.texture && iterator->second.texture.use_count() == 1)
            {
                const TextureAssetId contentId = iterator->first;
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

    u32 TextureCache::Clear()
    {
        const u32 removed = static_cast<u32>(m_residents.size());
        m_residents.clear();
        m_aliases.clear();
        m_evictions += removed;
        return removed;
    }

    TextureCacheStats TextureCache::GetStats() const
    {
        TextureCacheStats stats;
        stats.cacheHits = m_cacheHits;
        stats.cacheMisses = m_cacheMisses;
        stats.texturesCreated = m_texturesCreated;
        stats.creationFailures = m_creationFailures;
        stats.identifierConflicts = m_identifierConflicts;
        stats.reloadAttempts = m_reloadAttempts;
        stats.reloadSuccesses = m_reloadSuccesses;
        stats.reloadFailures = m_reloadFailures;
        stats.reloadReuses = m_reloadReuses;
        stats.evictions = m_evictions;
        stats.residentTextures = static_cast<u32>(m_residents.size());
        stats.residentAssetIds = static_cast<u32>(m_aliases.size());

        for (const auto& pair : m_residents)
        {
            const auto& texture = pair.second.texture;
            if (!texture)
            {
                continue;
            }
            if (texture.use_count() > 1)
            {
                ++stats.externallyReferencedTextures;
            }
            stats.residentBaseLevelBytes += texture->GetBaseLevelBytes();
            stats.estimatedResidentTextureBytes += texture->GetEstimatedResidentBytes();
        }
        return stats;
    }

    void TextureCache::RemoveAliasesForContent(TextureAssetId contentId)
    {
        for (auto iterator = m_aliases.begin(); iterator != m_aliases.end();)
        {
            if (iterator->second.contentId == contentId)
            {
                iterator = m_aliases.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }
}
