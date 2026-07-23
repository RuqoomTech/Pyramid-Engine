#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Texture.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Pyramid
{
    class IGraphicsDevice;
    class TextureCache;

    enum class TextureColorSpace
    {
        Linear = 0,
        SRGB
    };

    enum class TextureSourceType
    {
        Memory = 0,
        File
    };

    struct TextureAssetId
    {
        u64 high = 0;
        u64 low = 0;

        bool IsValid() const { return high != 0 || low != 0; }
        std::string ToString() const;
        static TextureAssetId FromString(std::string_view stableName);

        bool operator==(const TextureAssetId& other) const
        {
            return high == other.high && low == other.low;
        }

        bool operator!=(const TextureAssetId& other) const
        {
            return !(*this == other);
        }
    };

    struct TextureAssetIdHash
    {
        std::size_t operator()(const TextureAssetId& identifier) const noexcept;
    };

    struct TextureResourceSpecification
    {
        TextureSpecification texture;
        const void* pixelData = nullptr;
        u64 pixelDataSize = 0;
        TextureColorSpace colorSpace = TextureColorSpace::Linear;
        TextureAssetId assetId;
        std::string name;
    };

    struct TextureFileSpecification
    {
        std::string filepath;
        TextureColorSpace colorSpace = TextureColorSpace::Linear;
        bool generateMips = true;
        TextureFilter minFilter = TextureFilter::LinearMipmapLinear;
        TextureFilter magFilter = TextureFilter::Linear;
        TextureWrap wrapS = TextureWrap::Repeat;
        TextureWrap wrapT = TextureWrap::Repeat;
        f32 borderColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        f32 maxAnisotropy = 1.0f;
        bool flipY = false;
        TextureAssetId assetId;
        std::string name;
    };

    /**
     * @brief Immutable, cache-safe 2D texture resource.
     *
     * Mutation methods inherited from ITexture2D are deliberately rejected so
     * stable content identifiers cannot drift after cache insertion. Replace a
     * file-backed resource transactionally through TextureCache::Reload().
     */
    class TextureResource final : public ITexture2D
    {
    public:
        static std::shared_ptr<TextureResource> Create(
            IGraphicsDevice& device,
            const TextureResourceSpecification& specification);
        static std::shared_ptr<TextureResource> CreateFromFile(
            IGraphicsDevice& device,
            const TextureFileSpecification& specification);

        static TextureAssetId CalculateContentId(
            const TextureResourceSpecification& specification);
        static TextureAssetId CalculateContentId(
            const TextureFileSpecification& specification);

        TextureResource(const TextureResource&) = delete;
        TextureResource& operator=(const TextureResource&) = delete;
        TextureResource(TextureResource&&) = delete;
        TextureResource& operator=(TextureResource&&) = delete;
        ~TextureResource() override = default;

        void Bind(u32 slot = 0) const override;
        void Unbind(u32 slot = 0) const override;
        u32 GetWidth() const override;
        u32 GetHeight() const override;
        u32 GetRendererID() const override;
        TextureFormat GetFormat() const override;
        const std::string& GetPath() const override { return m_path; }
        bool IsLoaded() const override;
        std::string GetLastError() const override;

        void SetData(const void*, u32) override;
        void SetSubData(const void*, u32, u32, u32, u32) override;
        void SetFilter(TextureFilter, TextureFilter) override;
        void SetWrap(TextureWrap, TextureWrap) override;
        void SetBorderColor(f32, f32, f32, f32) override;
        void SetMaxAnisotropy(f32) override;
        void GenerateMipmaps() override;
        u32 GetMipLevels() const override;
        bool LoadFromFile(const std::string&, bool, bool) override;
        bool SaveToFile(const std::string&) const override { return false; }

        TextureAssetId GetAssetId() const { return m_assetId; }
        TextureAssetId GetContentId() const { return m_contentId; }
        TextureColorSpace GetColorSpace() const { return m_colorSpace; }
        TextureSourceType GetSourceType() const { return m_sourceType; }
        const TextureSpecification& GetSpecification() const { return m_specification; }
        const std::string& GetName() const { return m_name; }
        u64 GetBaseLevelBytes() const { return m_baseLevelBytes; }
        u64 GetEstimatedResidentBytes() const { return m_estimatedResidentBytes; }
        const std::shared_ptr<ITexture2D>& GetTexture() const { return m_texture; }

    private:
        friend class TextureCache;

        struct PreparedData
        {
            TextureSpecification specification;
            std::vector<u8> pixels;
            TextureAssetId contentId;
            TextureSourceType sourceType = TextureSourceType::Memory;
            TextureColorSpace colorSpace = TextureColorSpace::Linear;
            std::string path;
            std::string name;
            u64 baseLevelBytes = 0;
            u64 estimatedResidentBytes = 0;
        };

        static bool Prepare(
            const TextureResourceSpecification& specification,
            PreparedData& prepared,
            std::string& error);
        static bool Prepare(
            const TextureFileSpecification& specification,
            PreparedData& prepared,
            std::string& error);
        static std::shared_ptr<TextureResource> CreatePrepared(
            IGraphicsDevice& device,
            PreparedData&& prepared,
            TextureAssetId assetId);

        TextureResource(
            std::shared_ptr<ITexture2D> texture,
            TextureSpecification specification,
            TextureAssetId assetId,
            TextureAssetId contentId,
            TextureColorSpace colorSpace,
            TextureSourceType sourceType,
            u64 baseLevelBytes,
            u64 estimatedResidentBytes,
            std::string path,
            std::string name);

        void RejectMutation(const char* operation);

        std::shared_ptr<ITexture2D> m_texture;
        TextureSpecification m_specification;
        TextureAssetId m_assetId;
        TextureAssetId m_contentId;
        TextureColorSpace m_colorSpace = TextureColorSpace::Linear;
        TextureSourceType m_sourceType = TextureSourceType::Memory;
        u64 m_baseLevelBytes = 0;
        u64 m_estimatedResidentBytes = 0;
        std::string m_path;
        std::string m_name;
        std::string m_lastError;
    };
}
