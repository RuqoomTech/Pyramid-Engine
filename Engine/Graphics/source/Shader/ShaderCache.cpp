#include <Pyramid/Graphics/Shader/ShaderCache.hpp>

#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>

namespace Pyramid
{
    ShaderCache::ShaderCache(IGraphicsDevice& device)
        : m_device(&device)
    {
    }

    std::shared_ptr<ShaderProgram> ShaderCache::GetOrCreate(
        const ShaderProgramSpecification& specification)
    {
        const ShaderAssetId contentId = ShaderProgram::CalculateContentId(specification);
        if (!contentId.IsValid())
        {
            ++m_cacheMisses;
            ++m_compilationFailures;
            return ShaderProgram::Create(*m_device, specification);
        }

        const ShaderAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : contentId;

        const auto alias = m_assetToContent.find(assetId);
        if (alias != m_assetToContent.end())
        {
            if (alias->second != contentId)
            {
                ++m_identifierConflicts;
                PYRAMID_LOG_ERROR(
                    "Shader cache rejected asset identifier ", assetId.ToString(),
                    ": it is already associated with different source");
                return nullptr;
            }

            const auto resident = m_residents.find(contentId);
            if (resident != m_residents.end())
            {
                ++m_cacheHits;
                return resident->second.program;
            }

            m_assetToContent.erase(alias);
        }

        const auto matchingContent = m_residents.find(contentId);
        if (matchingContent != m_residents.end())
        {
            m_assetToContent.emplace(contentId, contentId);
            m_assetToContent.emplace(assetId, contentId);
            ++m_cacheHits;
            return matchingContent->second.program;
        }

        ++m_cacheMisses;
        auto program = ShaderProgram::Create(*m_device, specification);
        if (!program)
        {
            ++m_compilationFailures;
            return nullptr;
        }

        m_residents.emplace(contentId, Resident{program});
        m_assetToContent.emplace(contentId, contentId);
        m_assetToContent.emplace(assetId, contentId);
        ++m_programsCreated;
        return program;
    }

    bool ShaderCache::Recompile(
        ShaderAssetId stableAssetId,
        const ShaderProgramSpecification& replacement)
    {
        ++m_recompilationAttempts;

        if (!stableAssetId.IsValid())
        {
            ++m_recompilationFailures;
            return false;
        }

        const auto alias = m_assetToContent.find(stableAssetId);
        if (alias == m_assetToContent.end())
        {
            ++m_recompilationFailures;
            PYRAMID_LOG_ERROR(
                "Shader cache cannot recompile unknown asset identifier ",
                stableAssetId.ToString());
            return false;
        }

        const ShaderAssetId previousContentId = alias->second;
        if (stableAssetId == previousContentId)
        {
            ++m_recompilationFailures;
            PYRAMID_LOG_ERROR(
                "Shader cache cannot recompile a content-derived identifier; use a stable caller-defined asset ID");
            return false;
        }

        if (replacement.assetId.IsValid() && replacement.assetId != stableAssetId)
        {
            ++m_recompilationFailures;
            ++m_identifierConflicts;
            PYRAMID_LOG_ERROR(
                "Shader cache recompilation rejected a replacement with a different asset identifier");
            return false;
        }

        ShaderProgramSpecification normalized = replacement;
        normalized.assetId = stableAssetId;
        const ShaderAssetId replacementContentId =
            ShaderProgram::CalculateContentId(normalized);
        if (!replacementContentId.IsValid())
        {
            ++m_recompilationFailures;
            // Create emits the detailed stage validation error without touching mappings.
            (void)ShaderProgram::Create(*m_device, normalized);
            return false;
        }

        if (replacementContentId == previousContentId)
        {
            ++m_recompilationSuccesses;
            ++m_recompilationReuses;
            return true;
        }

        const auto matchingContent = m_residents.find(replacementContentId);
        if (matchingContent != m_residents.end())
        {
            m_assetToContent[stableAssetId] = replacementContentId;
            m_assetToContent.emplace(replacementContentId, replacementContentId);
            ++m_cacheHits;
            ++m_recompilationSuccesses;
            ++m_recompilationReuses;
            return true;
        }

        auto replacementProgram = ShaderProgram::Create(*m_device, normalized);
        if (!replacementProgram)
        {
            ++m_compilationFailures;
            ++m_recompilationFailures;
            return false;
        }

        m_residents.emplace(replacementContentId, Resident{replacementProgram});
        m_assetToContent.emplace(replacementContentId, replacementContentId);
        m_assetToContent[stableAssetId] = replacementContentId;
        ++m_programsCreated;
        ++m_recompilationSuccesses;
        return true;
    }

    std::shared_ptr<ShaderProgram> ShaderCache::Find(ShaderAssetId assetId) const
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
        return resident == m_residents.end() ? nullptr : resident->second.program;
    }

    bool ShaderCache::Contains(ShaderAssetId assetId) const
    {
        return Find(assetId) != nullptr;
    }

    bool ShaderCache::Evict(ShaderAssetId assetId)
    {
        const auto alias = m_assetToContent.find(assetId);
        if (alias == m_assetToContent.end())
        {
            return false;
        }

        const ShaderAssetId contentId = alias->second;
        const auto removed = m_residents.erase(contentId);
        RemoveAliasesForContent(contentId);
        m_evictions += removed;
        return removed != 0;
    }

    u32 ShaderCache::CollectUnused()
    {
        u32 removed = 0;
        for (auto iterator = m_residents.begin(); iterator != m_residents.end();)
        {
            if (iterator->second.program && iterator->second.program.use_count() == 1)
            {
                const ShaderAssetId contentId = iterator->first;
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

    u32 ShaderCache::Clear()
    {
        const u32 removed = static_cast<u32>(m_residents.size());
        m_residents.clear();
        m_assetToContent.clear();
        m_evictions += removed;
        return removed;
    }

    ShaderCacheStats ShaderCache::GetStats() const
    {
        ShaderCacheStats stats;
        stats.cacheHits = m_cacheHits;
        stats.cacheMisses = m_cacheMisses;
        stats.programsCreated = m_programsCreated;
        stats.compilationFailures = m_compilationFailures;
        stats.identifierConflicts = m_identifierConflicts;
        stats.recompilationAttempts = m_recompilationAttempts;
        stats.recompilationSuccesses = m_recompilationSuccesses;
        stats.recompilationFailures = m_recompilationFailures;
        stats.recompilationReuses = m_recompilationReuses;
        stats.evictions = m_evictions;
        stats.residentPrograms = static_cast<u32>(m_residents.size());
        stats.residentAssetIds = static_cast<u32>(m_assetToContent.size());

        for (const auto& pair : m_residents)
        {
            const auto& program = pair.second.program;
            if (!program)
            {
                continue;
            }

            if (program.use_count() > 1)
            {
                ++stats.externallyReferencedPrograms;
            }
            stats.residentSourceBytes += program->GetSourceBytes();
        }

        return stats;
    }

    void ShaderCache::RemoveAliasesForContent(ShaderAssetId contentId)
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
