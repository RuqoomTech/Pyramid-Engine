#include <Pyramid/Graphics/Texture/TextureResource.hpp>

#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Image.hpp>
#include <Pyramid/Util/Log.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <new>
#include <sstream>
#include <utility>

namespace Pyramid
{
    namespace
    {
        constexpr u64 kFnvPrime = 1099511628211ull;
        constexpr u64 kPrimaryOffset = 14695981039346656037ull;
        constexpr u64 kSecondaryOffset = 7809847782465536322ull;

        class StableHasher128
        {
        public:
            void AddByte(u8 value)
            {
                m_high ^= value;
                m_high *= kFnvPrime;
                m_low ^= static_cast<u8>(value ^ 0xa5u);
                m_low *= kFnvPrime;
                m_low ^= m_low >> 32u;
            }

            void AddBytes(const void* data, std::size_t size)
            {
                const auto* bytes = static_cast<const u8*>(data);
                for (std::size_t index = 0; index < size; ++index)
                {
                    AddByte(bytes[index]);
                }
            }

            void AddU32(u32 value)
            {
                for (u32 shift = 0; shift < 32; shift += 8)
                {
                    AddByte(static_cast<u8>((value >> shift) & 0xffu));
                }
            }

            void AddU64(u64 value)
            {
                for (u32 shift = 0; shift < 64; shift += 8)
                {
                    AddByte(static_cast<u8>((value >> shift) & 0xffu));
                }
            }

            void AddFloat(f32 value)
            {
                u32 bits = 0;
                static_assert(sizeof(bits) == sizeof(value));
                std::memcpy(&bits, &value, sizeof(bits));
                AddU32(bits);
            }

            void AddString(std::string_view value)
            {
                AddU64(static_cast<u64>(value.size()));
                AddBytes(value.data(), value.size());
            }

            TextureAssetId Finish() const
            {
                TextureAssetId result{m_high, m_low};
                if (!result.IsValid())
                {
                    result.low = 1;
                }
                return result;
            }

        private:
            u64 m_high = kPrimaryOffset;
            u64 m_low = kSecondaryOffset;
        };

        bool IsMipFilter(TextureFilter filter)
        {
            return filter == TextureFilter::LinearMipmapLinear ||
                filter == TextureFilter::LinearMipmapNearest ||
                filter == TextureFilter::NearestMipmapLinear ||
                filter == TextureFilter::NearestMipmapNearest;
        }

        bool ResolveBaseFormat(TextureFormat input, TextureFormat& output, u32& bytesPerPixel)
        {
            switch (input)
            {
            case TextureFormat::RGB8:
            case TextureFormat::SRGB8:
                output = TextureFormat::RGB8;
                bytesPerPixel = 3;
                return true;
            case TextureFormat::RGBA8:
            case TextureFormat::SRGBA8:
                output = TextureFormat::RGBA8;
                bytesPerPixel = 4;
                return true;
            default:
                return false;
            }
        }

        bool CalculateByteSize(u32 width, u32 height, u32 bytesPerPixel, u64& result)
        {
            if (width == 0 || height == 0 || bytesPerPixel == 0)
            {
                return false;
            }

            const u64 row = static_cast<u64>(width) * bytesPerPixel;
            if (row / bytesPerPixel != width ||
                static_cast<u64>(height) > std::numeric_limits<u64>::max() / row)
            {
                return false;
            }

            result = row * height;
            return true;
        }

        u64 EstimateResidentBytes(
            u32 width,
            u32 height,
            u32 bytesPerPixel,
            bool generateMips)
        {
            u64 total = 0;
            do
            {
                u64 level = 0;
                if (!CalculateByteSize(width, height, bytesPerPixel, level) ||
                    total > std::numeric_limits<u64>::max() - level)
                {
                    return 0;
                }
                total += level;
                if (!generateMips || (width == 1 && height == 1))
                {
                    break;
                }
                width = std::max(1U, width / 2U);
                height = std::max(1U, height / 2U);
            } while (true);
            return total;
        }

        bool IsSamplerStateValid(
            const f32 borderColor[4],
            f32 maxAnisotropy)
        {
            if (!std::isfinite(maxAnisotropy) || maxAnisotropy < 1.0f)
            {
                return false;
            }
            for (u32 index = 0; index < 4; ++index)
            {
                if (!std::isfinite(borderColor[index]))
                {
                    return false;
                }
            }
            return true;
        }

        TextureSpecification NormalizeSpecification(
            const TextureSpecification& input,
            TextureColorSpace colorSpace,
            TextureFormat baseFormat)
        {
            TextureSpecification normalized = input;
            normalized.Format = baseFormat;
            normalized.SRGB = colorSpace == TextureColorSpace::SRGB;
            normalized.FlipY = false;
            if (!normalized.GenerateMips && IsMipFilter(normalized.MinFilter))
            {
                normalized.MinFilter = TextureFilter::Linear;
            }
            return normalized;
        }

        TextureAssetId HashPrepared(
            const TextureSpecification& specification,
            TextureColorSpace colorSpace,
            const std::vector<u8>& pixels)
        {
            StableHasher128 hasher;
            hasher.AddString("Pyramid.Texture.Content.v1");
            hasher.AddU32(specification.Width);
            hasher.AddU32(specification.Height);
            hasher.AddU32(static_cast<u32>(specification.Format));
            hasher.AddU32(static_cast<u32>(colorSpace));
            hasher.AddU32(static_cast<u32>(specification.MinFilter));
            hasher.AddU32(static_cast<u32>(specification.MagFilter));
            hasher.AddU32(static_cast<u32>(specification.WrapS));
            hasher.AddU32(static_cast<u32>(specification.WrapT));
            hasher.AddU32(specification.GenerateMips ? 1U : 0U);
            for (f32 component : specification.BorderColor)
            {
                hasher.AddFloat(component);
            }
            hasher.AddFloat(specification.MaxAnisotropy);
            hasher.AddU64(static_cast<u64>(pixels.size()));
            hasher.AddBytes(pixels.data(), pixels.size());
            return hasher.Finish();
        }

        void FlipRows(std::vector<u8>& pixels, u32 width, u32 height, u32 channels)
        {
            if (height < 2)
            {
                return;
            }
            const std::size_t rowBytes = static_cast<std::size_t>(width) * channels;
            std::vector<u8> row(rowBytes);
            for (u32 y = 0; y < height / 2; ++y)
            {
                u8* top = pixels.data() + static_cast<std::size_t>(y) * rowBytes;
                u8* bottom = pixels.data() + static_cast<std::size_t>(height - 1U - y) * rowBytes;
                std::memcpy(row.data(), top, rowBytes);
                std::memcpy(top, bottom, rowBytes);
                std::memcpy(bottom, row.data(), rowBytes);
            }
        }
    }

    TextureAssetId TextureAssetId::FromString(std::string_view stableName)
    {
        if (stableName.empty())
        {
            return {};
        }
        StableHasher128 hasher;
        hasher.AddString("Pyramid.Texture.Asset.v1");
        hasher.AddString(stableName);
        return hasher.Finish();
    }

    std::string TextureAssetId::ToString() const
    {
        if (!IsValid())
        {
            return {};
        }
        std::ostringstream stream;
        stream << std::hex << std::setfill('0')
               << std::setw(16) << high
               << std::setw(16) << low;
        return stream.str();
    }

    std::size_t TextureAssetIdHash::operator()(const TextureAssetId& identifier) const noexcept
    {
        const u64 mixed = identifier.high ^
            (identifier.low + 0x9e3779b97f4a7c15ull +
             (identifier.high << 6u) + (identifier.high >> 2u));
        return static_cast<std::size_t>(mixed);
    }

    bool TextureResource::Prepare(
        const TextureResourceSpecification& specification,
        PreparedData& prepared,
        std::string& error)
    {
        error.clear();
        prepared = {};

        TextureFormat baseFormat = TextureFormat::None;
        u32 bytesPerPixel = 0;
        if (!ResolveBaseFormat(specification.texture.Format, baseFormat, bytesPerPixel))
        {
            error = "Texture resource supports only RGB8/RGBA8 source pixels";
            return false;
        }
        if (!IsSamplerStateValid(
                specification.texture.BorderColor,
                specification.texture.MaxAnisotropy))
        {
            error = "Texture resource sampler state is invalid";
            return false;
        }

        u64 expectedSize = 0;
        if (!CalculateByteSize(
                specification.texture.Width,
                specification.texture.Height,
                bytesPerPixel,
                expectedSize))
        {
            error = "Texture resource dimensions or byte size are invalid";
            return false;
        }
        if (!specification.pixelData || specification.pixelDataSize != expectedSize)
        {
            error = "Texture resource requires complete tightly packed pixel data";
            return false;
        }
        if (expectedSize > static_cast<u64>(std::numeric_limits<std::size_t>::max()))
        {
            error = "Texture resource pixel data exceeds addressable memory";
            return false;
        }

        try
        {
            prepared.pixels.resize(static_cast<std::size_t>(expectedSize));
            std::memcpy(prepared.pixels.data(), specification.pixelData, prepared.pixels.size());
        }
        catch (const std::bad_alloc&)
        {
            error = "Texture resource pixel allocation failed";
            return false;
        }

        if (specification.texture.FlipY)
        {
            FlipRows(
                prepared.pixels,
                specification.texture.Width,
                specification.texture.Height,
                bytesPerPixel);
        }

        prepared.specification = NormalizeSpecification(
            specification.texture,
            specification.colorSpace,
            baseFormat);
        prepared.colorSpace = specification.colorSpace;
        prepared.sourceType = TextureSourceType::Memory;
        prepared.name = specification.name;
        prepared.baseLevelBytes = expectedSize;
        prepared.estimatedResidentBytes = EstimateResidentBytes(
            prepared.specification.Width,
            prepared.specification.Height,
            bytesPerPixel,
            prepared.specification.GenerateMips);
        prepared.contentId = HashPrepared(
            prepared.specification,
            prepared.colorSpace,
            prepared.pixels);
        return prepared.contentId.IsValid();
    }

    bool TextureResource::Prepare(
        const TextureFileSpecification& specification,
        PreparedData& prepared,
        std::string& error)
    {
        error.clear();
        prepared = {};
        if (specification.filepath.empty())
        {
            error = "Texture file path is empty";
            return false;
        }
        if (!IsSamplerStateValid(specification.borderColor, specification.maxAnisotropy))
        {
            error = "Texture file sampler state is invalid";
            return false;
        }

        Util::ImageData image = Util::Image::Load(specification.filepath);
        if (!image.Data)
        {
            error = "Failed to decode texture image: " + specification.filepath;
            return false;
        }

        const auto freeImage = [&image]()
        {
            Util::Image::Free(image.Data);
            image.Data = nullptr;
        };

        if (image.Width <= 0 || image.Height <= 0 ||
            (image.Channels != 3 && image.Channels != 4))
        {
            freeImage();
            error = "Decoded texture must contain valid RGB or RGBA pixels: " +
                specification.filepath;
            return false;
        }

        const u32 width = static_cast<u32>(image.Width);
        const u32 height = static_cast<u32>(image.Height);
        const u32 channels = static_cast<u32>(image.Channels);
        u64 byteSize = 0;
        if (!CalculateByteSize(width, height, channels, byteSize) ||
            byteSize > static_cast<u64>(std::numeric_limits<std::size_t>::max()))
        {
            freeImage();
            error = "Decoded texture byte size is invalid: " + specification.filepath;
            return false;
        }

        try
        {
            prepared.pixels.assign(image.Data, image.Data + static_cast<std::size_t>(byteSize));
        }
        catch (const std::bad_alloc&)
        {
            freeImage();
            error = "Decoded texture pixel allocation failed: " + specification.filepath;
            return false;
        }
        freeImage();

        if (specification.flipY)
        {
            FlipRows(prepared.pixels, width, height, channels);
        }

        TextureSpecification texture;
        texture.Width = width;
        texture.Height = height;
        texture.Format = channels == 4 ? TextureFormat::RGBA8 : TextureFormat::RGB8;
        texture.MinFilter = specification.minFilter;
        texture.MagFilter = specification.magFilter;
        texture.WrapS = specification.wrapS;
        texture.WrapT = specification.wrapT;
        texture.GenerateMips = specification.generateMips;
        texture.MaxAnisotropy = specification.maxAnisotropy;
        texture.FlipY = false;
        for (u32 index = 0; index < 4; ++index)
        {
            texture.BorderColor[index] = specification.borderColor[index];
        }

        prepared.specification = NormalizeSpecification(
            texture,
            specification.colorSpace,
            texture.Format);
        prepared.colorSpace = specification.colorSpace;
        prepared.sourceType = TextureSourceType::File;
        prepared.path = specification.filepath;
        prepared.name = specification.name;
        prepared.baseLevelBytes = byteSize;
        prepared.estimatedResidentBytes = EstimateResidentBytes(
            width,
            height,
            channels,
            prepared.specification.GenerateMips);
        prepared.contentId = HashPrepared(
            prepared.specification,
            prepared.colorSpace,
            prepared.pixels);
        return prepared.contentId.IsValid();
    }

    TextureAssetId TextureResource::CalculateContentId(
        const TextureResourceSpecification& specification)
    {
        PreparedData prepared;
        std::string error;
        return Prepare(specification, prepared, error) ? prepared.contentId : TextureAssetId{};
    }

    TextureAssetId TextureResource::CalculateContentId(
        const TextureFileSpecification& specification)
    {
        PreparedData prepared;
        std::string error;
        return Prepare(specification, prepared, error) ? prepared.contentId : TextureAssetId{};
    }

    std::shared_ptr<TextureResource> TextureResource::Create(
        IGraphicsDevice& device,
        const TextureResourceSpecification& specification)
    {
        PreparedData prepared;
        std::string error;
        if (!Prepare(specification, prepared, error))
        {
            PYRAMID_LOG_ERROR("Texture resource creation failed: ", error);
            return nullptr;
        }
        const TextureAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : prepared.contentId;
        return CreatePrepared(device, std::move(prepared), assetId);
    }

    std::shared_ptr<TextureResource> TextureResource::CreateFromFile(
        IGraphicsDevice& device,
        const TextureFileSpecification& specification)
    {
        PreparedData prepared;
        std::string error;
        if (!Prepare(specification, prepared, error))
        {
            PYRAMID_LOG_ERROR("Texture resource creation failed: ", error);
            return nullptr;
        }
        const TextureAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : prepared.contentId;
        return CreatePrepared(device, std::move(prepared), assetId);
    }

    std::shared_ptr<TextureResource> TextureResource::CreatePrepared(
        IGraphicsDevice& device,
        PreparedData&& prepared,
        TextureAssetId assetId)
    {
        auto texture = device.CreateTexture2D(
            prepared.specification,
            prepared.pixels.data());
        if (!texture || !texture->IsLoaded())
        {
            PYRAMID_LOG_ERROR(
                "Texture resource creation failed",
                prepared.path.empty() ? "" : " for ",
                prepared.path,
                texture ? ": " + texture->GetLastError() : ": graphics device returned null");
            return nullptr;
        }

        return std::shared_ptr<TextureResource>(new TextureResource(
            std::move(texture),
            prepared.specification,
            assetId,
            prepared.contentId,
            prepared.colorSpace,
            prepared.sourceType,
            prepared.baseLevelBytes,
            prepared.estimatedResidentBytes,
            std::move(prepared.path),
            std::move(prepared.name)));
    }

    TextureResource::TextureResource(
        std::shared_ptr<ITexture2D> texture,
        TextureSpecification specification,
        TextureAssetId assetId,
        TextureAssetId contentId,
        TextureColorSpace colorSpace,
        TextureSourceType sourceType,
        u64 baseLevelBytes,
        u64 estimatedResidentBytes,
        std::string path,
        std::string name)
        : m_texture(std::move(texture)),
          m_specification(specification),
          m_assetId(assetId),
          m_contentId(contentId),
          m_colorSpace(colorSpace),
          m_sourceType(sourceType),
          m_baseLevelBytes(baseLevelBytes),
          m_estimatedResidentBytes(estimatedResidentBytes),
          m_path(std::move(path)),
          m_name(std::move(name))
    {
    }

    void TextureResource::Bind(u32 slot) const { if (m_texture) m_texture->Bind(slot); }
    void TextureResource::Unbind(u32 slot) const { if (m_texture) m_texture->Unbind(slot); }
    u32 TextureResource::GetWidth() const { return m_texture ? m_texture->GetWidth() : 0; }
    u32 TextureResource::GetHeight() const { return m_texture ? m_texture->GetHeight() : 0; }
    u32 TextureResource::GetRendererID() const { return m_texture ? m_texture->GetRendererID() : 0; }
    TextureFormat TextureResource::GetFormat() const
    {
        return m_texture ? m_texture->GetFormat() : TextureFormat::None;
    }
    bool TextureResource::IsLoaded() const { return m_texture && m_texture->IsLoaded(); }
    std::string TextureResource::GetLastError() const
    {
        return m_lastError.empty() && m_texture ? m_texture->GetLastError() : m_lastError;
    }

    void TextureResource::RejectMutation(const char* operation)
    {
        m_lastError = std::string(operation) +
            " is not allowed on immutable TextureResource; replace it through TextureCache";
        PYRAMID_LOG_ERROR("TextureResource: ", m_lastError);
    }

    void TextureResource::SetData(const void*, u32) { RejectMutation("SetData"); }
    void TextureResource::SetSubData(const void*, u32, u32, u32, u32) { RejectMutation("SetSubData"); }
    void TextureResource::SetFilter(TextureFilter, TextureFilter) { RejectMutation("SetFilter"); }
    void TextureResource::SetWrap(TextureWrap, TextureWrap) { RejectMutation("SetWrap"); }
    void TextureResource::SetBorderColor(f32, f32, f32, f32) { RejectMutation("SetBorderColor"); }
    void TextureResource::SetMaxAnisotropy(f32) { RejectMutation("SetMaxAnisotropy"); }
    void TextureResource::GenerateMipmaps() { RejectMutation("GenerateMipmaps"); }
    u32 TextureResource::GetMipLevels() const { return m_texture ? m_texture->GetMipLevels() : 0; }
    bool TextureResource::LoadFromFile(const std::string&, bool, bool)
    {
        RejectMutation("LoadFromFile");
        return false;
    }
}
