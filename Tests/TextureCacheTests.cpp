#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Texture/TextureCache.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return EXIT_FAILURE;
    }

    class TestTexture final : public Pyramid::ITexture2D
    {
    public:
        TestTexture(Pyramid::TextureSpecification specification, Pyramid::u32 rendererId)
            : m_specification(specification), m_rendererId(rendererId)
        {
        }

        void Bind(Pyramid::u32 = 0) const override {}
        void Unbind(Pyramid::u32 = 0) const override {}
        Pyramid::u32 GetWidth() const override { return m_specification.Width; }
        Pyramid::u32 GetHeight() const override { return m_specification.Height; }
        Pyramid::u32 GetRendererID() const override { return m_rendererId; }
        Pyramid::TextureFormat GetFormat() const override { return m_specification.Format; }
        const std::string& GetPath() const override { return m_path; }
        bool IsLoaded() const override { return true; }
        Pyramid::u32 GetMipLevels() const override
        {
            return m_specification.GenerateMips ? 2U : 1U;
        }

    private:
        Pyramid::TextureSpecification m_specification;
        Pyramid::u32 m_rendererId = 0;
        std::string m_path;
    };

#pragma pack(push, 1)
    struct TgaHeader
    {
        unsigned char idLength = 0;
        unsigned char colorMapType = 0;
        unsigned char imageType = 2;
        unsigned char colorMapSpec[5] = {};
        unsigned short xOrigin = 0;
        unsigned short yOrigin = 0;
        unsigned short width = 2;
        unsigned short height = 1;
        unsigned char bitsPerPixel = 24;
        unsigned char descriptor = 0x20;
    };
#pragma pack(pop)

    int TestUncompressedFormats()
    {
        using namespace Pyramid;
        struct FormatCase
        {
            TextureFormat format;
            u32 bytesPerPixel;
            bool srgbCapable;
        };
        const FormatCase cases[] = {
            {TextureFormat::RGB8, 3, true}, {TextureFormat::RGBA8, 4, true},
            {TextureFormat::SRGB8, 3, true}, {TextureFormat::SRGBA8, 4, true},
            {TextureFormat::R8, 1, false}, {TextureFormat::R16F, 2, false},
            {TextureFormat::R32F, 4, false}, {TextureFormat::RGB16F, 6, false},
            {TextureFormat::RGBA16F, 8, false}, {TextureFormat::RGB32F, 12, false},
            {TextureFormat::RGBA32F, 16, false}, {TextureFormat::Depth16, 2, false},
            {TextureFormat::Depth24, 4, false}, {TextureFormat::Depth32F, 4, false},
            {TextureFormat::Depth24Stencil8, 4, false}, {TextureFormat::Depth32FStencil8, 8, false}};
        for (const auto& test : cases)
        {
            std::vector<u8> pixels(4U * test.bytesPerPixel);
            for (std::size_t index = 0; index < pixels.size(); ++index)
            {
                pixels[index] = static_cast<u8>(index + 1U);
            }
            TextureSpecification uploadedSpecification;
            std::vector<u8> uploadedPixels;
            Tests::TestGraphicsDevice device;
            device.textureFactory = [&](const TextureSpecification& specification, const void* data)
            {
                uploadedSpecification = specification;
                const auto* bytes = static_cast<const u8*>(data);
                uploadedPixels.assign(bytes, bytes + pixels.size());
                return std::make_shared<TestTexture>(specification, 1);
            };
            TextureCache cache(device);
            TextureResourceSpecification request;
            request.texture.Width = 2;
            request.texture.Height = 2;
            request.texture.Format = test.format;
            request.pixelData = pixels.data();
            request.pixelDataSize = pixels.size();
            auto texture = cache.GetOrCreate(request);
            const TextureFormat expectedFormat = test.format == TextureFormat::SRGB8
                ? TextureFormat::RGB8 : test.format == TextureFormat::SRGBA8
                ? TextureFormat::RGBA8 : test.format;
            if (!texture || texture != cache.GetOrCreate(request) || device.textureCreations != 1 ||
                uploadedSpecification.Format != expectedFormat || uploadedSpecification.SRGB ||
                uploadedPixels != pixels || texture->GetBaseLevelBytes() != pixels.size() ||
                texture->GetEstimatedResidentBytes() != 5U * test.bytesPerPixel ||
                cache.GetStats().residentBaseLevelBytes != pixels.size() ||
                cache.GetStats().estimatedResidentTextureBytes != 5U * test.bytesPerPixel)
            {
                std::cerr << "Texture format " << static_cast<u32>(test.format) << '\n';
                return Fail("format upload bytes, normalization, deduplication, or mip residency failed");
            }
            request.pixelDataSize = pixels.size() - 1;
            if (cache.GetOrCreate(request) || device.textureCreations != 1)
            {
                return Fail("truncated uncompressed pixels reached the graphics device");
            }
            std::vector<u8> oversizedPixels = pixels;
            oversizedPixels.push_back(0);
            request.pixelData = oversizedPixels.data();
            request.pixelDataSize = oversizedPixels.size();
            if (cache.GetOrCreate(request) || device.textureCreations != 1)
            {
                return Fail("oversized uncompressed pixels reached the graphics device");
            }
            request.pixelData = pixels.data();
            request.pixelDataSize = pixels.size();
            request.colorSpace = TextureColorSpace::SRGB;
            if (!test.srgbCapable && (cache.GetOrCreate(request) || device.textureCreations != 1))
            {
                return Fail("sRGB single-channel, float, or depth pixels reached the graphics device");
            }
        }

        Tests::TestGraphicsDevice device;
        device.textureFactory = [](const TextureSpecification& specification, const void*)
        {
            return std::make_shared<TestTexture>(specification, 1);
        };
        TextureCache cache(device);
        const std::array<u8, 16> pixels{};
        TextureResourceSpecification request;
        request.texture.Width = 2;
        request.texture.Height = 2;
        request.texture.GenerateMips = false;
        request.pixelData = pixels.data();
        request.pixelDataSize = pixels.size();
        auto rgba = cache.GetOrCreate(request);
        request.texture.Format = TextureFormat::R32F;
        auto scalar = cache.GetOrCreate(request);
        request.texture.Format = TextureFormat::Depth24Stencil8;
        auto depthStencil = cache.GetOrCreate(request);
        if (!rgba || !scalar || !depthStencil || rgba == scalar || scalar == depthStencil ||
            rgba->GetContentId() == scalar->GetContentId() ||
            scalar->GetContentId() == depthStencil->GetContentId() || device.textureCreations != 3)
        {
            return Fail("byte-identical data in distinct formats shared a cache entry");
        }
        const TextureFormat rejectedFormats[] = {TextureFormat::None, TextureFormat::BC1_RGB,
            TextureFormat::BC1_RGBA, TextureFormat::BC3_RGBA, TextureFormat::BC7_RGBA,
            static_cast<TextureFormat>(999)};
        for (TextureFormat format : rejectedFormats)
        {
            request.texture.Format = format;
            if (TextureResource::CalculateContentId(request).IsValid() || cache.GetOrCreate(request) ||
                device.textureCreations != 3)
            {
                return Fail("unsupported format was hashed, copied, or sent to the graphics device");
            }
        }
        request.texture.Format = TextureFormat::R32F;
        request.colorSpace = static_cast<TextureColorSpace>(999);
        if (cache.GetOrCreate(request) || device.textureCreations != 3)
        {
            return Fail("invalid color space reached the graphics device");
        }
        request.colorSpace = TextureColorSpace::Linear;
        request.pixelData = nullptr;
        if (cache.GetOrCreate(request) || device.textureCreations != 3)
        {
            return Fail("null immutable pixel data reached the graphics device");
        }

        request.pixelData = pixels.data();
        request.texture.Format = TextureFormat::RGBA32F;
        request.texture.Width = u32{1} << 30U;
        request.texture.Height = u32{1} << 29U;
        request.pixelDataSize = u64{1} << 63U;
        try
        {
            if (TextureResource::CalculateContentId(request).IsValid() || cache.GetOrCreate(request) ||
                device.textureCreations != 3)
            {
                return Fail("unaddressable texture metadata was copied or sent to the graphics device");
            }
            request.texture.Width = (512U * 1024U * 1024U) / 16U + 1U;
            request.texture.Height = 1;
            request.pixelDataSize = static_cast<u64>(request.texture.Width) * 16U;
            if (TextureResource::CalculateContentId(request).IsValid() || cache.GetOrCreate(request) ||
                device.textureCreations != 3)
            {
                return Fail("texture metadata above the 512 MiB source limit was copied or uploaded");
            }
        }
        catch (const std::exception& error)
        {
            std::cerr << error.what() << '\n';
            return Fail("oversized texture metadata threw instead of failing explicitly");
        }
        request.texture.Width = 2;
        request.texture.Height = 2;
        std::array<u8, 32> rows{};
        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            rows[index] = static_cast<u8>(index);
        }
        request.texture.Format = TextureFormat::RGBA16F;
        request.texture.FlipY = true;
        request.pixelData = rows.data();
        request.pixelDataSize = rows.size();
        const TextureAssetId flipped = TextureResource::CalculateContentId(request);
        std::rotate(rows.begin(), rows.begin() + 16, rows.end());
        request.texture.FlipY = false;
        if (!flipped.IsValid() || flipped != TextureResource::CalculateContentId(request))
        {
            return Fail("half-float row flipping did not preserve complete pixel bytes");
        }
        return EXIT_SUCCESS;
    }

    bool WriteTga(const std::string& path, const std::array<unsigned char, 6>& rgb)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file)
        {
            return false;
        }
        const TgaHeader header;
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        for (std::size_t index = 0; index < rgb.size(); index += 3)
        {
            const std::array<unsigned char, 3> bgr = {
                rgb[index + 2], rgb[index + 1], rgb[index]};
            file.write(reinterpret_cast<const char*>(bgr.data()), bgr.size());
        }
        return static_cast<bool>(file);
    }
}

int main()
{
    using namespace Pyramid;

    if (TestUncompressedFormats() != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    const TextureAssetId named = TextureAssetId::FromString("textures/checker");
    if (!named.IsValid() || named != TextureAssetId::FromString("textures/checker") ||
        named == TextureAssetId::FromString("textures/other") || named.ToString().size() != 32)
    {
        return Fail("stable texture identifiers are not deterministic");
    }

    Tests::TestGraphicsDevice device;
    u32 nextRendererId = 1;
    device.textureFactory = [&nextRendererId](const TextureSpecification& specification, const void*)
    {
        return std::make_shared<TestTexture>(specification, nextRendererId++);
    };

    TextureCache cache(device);
    const std::array<u8, 8> pixels = {255, 0, 0, 255, 0, 255, 0, 255};

    TextureResourceSpecification linear;
    linear.texture.Width = 2;
    linear.texture.Height = 1;
    linear.texture.Format = TextureFormat::RGBA8;
    linear.texture.GenerateMips = false;
    linear.texture.MinFilter = TextureFilter::Linear;
    linear.pixelData = pixels.data();
    linear.pixelDataSize = pixels.size();
    linear.colorSpace = TextureColorSpace::Linear;
    linear.assetId = TextureAssetId::FromString("textures/memory/linear");
    linear.name = "Memory linear";

    const TextureAssetId legacyContent = TextureResource::CalculateContentId(linear);
    if (legacyContent.ToString() != "534c79b55837f6d927378597c101965f")
    {
        return Fail("existing RGBA8 content identity changed");
    }
    TextureResourceSpecification legacyFlags = linear;
    legacyFlags.texture.Format = TextureFormat::SRGBA8;
    legacyFlags.texture.SRGB = true;
    if (TextureResource::CalculateContentId(legacyFlags) != legacyContent)
    {
        return Fail("explicit cache color space no longer overrides legacy sRGB flags and aliases");
    }

    auto first = cache.GetOrCreate(linear);
    auto second = cache.GetOrCreate(linear);
    if (!first || first != second || device.textureCreations != 1 ||
        first->GetColorSpace() != TextureColorSpace::Linear ||
        first->GetBaseLevelBytes() != pixels.size())
    {
        return Fail("memory texture was not cached exactly once");
    }

    TextureResourceSpecification alias = linear;
    alias.assetId = TextureAssetId::FromString("textures/memory/alias");
    auto aliasTexture = cache.GetOrCreate(alias);
    if (aliasTexture != first || device.textureCreations != 1)
    {
        return Fail("identical memory texture aliases did not share one upload");
    }

    TextureResourceSpecification srgb = linear;
    srgb.assetId = TextureAssetId::FromString("textures/memory/srgb");
    srgb.colorSpace = TextureColorSpace::SRGB;
    auto srgbTexture = cache.GetOrCreate(srgb);
    if (!srgbTexture || srgbTexture == first || device.textureCreations != 2 ||
        srgbTexture->GetSpecification().SRGB != true)
    {
        return Fail("color space was not part of texture identity");
    }

    TextureResourceSpecification conflict = linear;
    std::array<u8, 8> differentPixels = pixels;
    differentPixels[0] = 17;
    conflict.pixelData = differentPixels.data();
    auto rejected = cache.GetOrCreate(conflict);
    if (rejected || cache.GetStats().identifierConflicts != 1)
    {
        return Fail("stable identifier/content conflict was not rejected");
    }

    TextureResourceSpecification invalid = linear;
    invalid.pixelDataSize = pixels.size() - 1;
    if (cache.GetOrCreate(invalid))
    {
        return Fail("invalid pixel byte size was accepted");
    }

    const std::string filePath = "pyramid_texture_cache_test.tga";
    const std::array<unsigned char, 6> filePixelsA = {255, 0, 0, 0, 255, 0};
    const std::array<unsigned char, 6> filePixelsB = {0, 0, 255, 255, 255, 0};
    if (!WriteTga(filePath, filePixelsA))
    {
        return Fail("could not write file texture fixture");
    }

    TextureFileSpecification fileSpec;
    fileSpec.filepath = filePath;
    fileSpec.colorSpace = TextureColorSpace::SRGB;
    fileSpec.generateMips = true;
    fileSpec.assetId = TextureAssetId::FromString("textures/file/stable");
    fileSpec.name = "Reloadable file";

    auto fileTextureA = cache.GetOrCreate(fileSpec);
    if (!fileTextureA || device.textureCreations != 3 ||
        fileTextureA->GetSourceType() != TextureSourceType::File ||
        fileTextureA->GetPath() != filePath)
    {
        std::remove(filePath.c_str());
        return Fail("file texture creation failed");
    }

    TextureFileSpecification fileAlias = fileSpec;
    fileAlias.assetId = TextureAssetId::FromString("textures/file/alias");
    auto fileTextureAlias = cache.GetOrCreate(fileAlias);
    if (fileTextureAlias != fileTextureA || device.textureCreations != 3)
    {
        std::remove(filePath.c_str());
        return Fail("file texture deduplication failed");
    }

    if (!cache.Reload(fileSpec.assetId) || cache.Find(fileSpec.assetId) != fileTextureA ||
        device.textureCreations != 3)
    {
        std::remove(filePath.c_str());
        return Fail("same-content reload did not reuse the active texture");
    }

    if (!WriteTga(filePath, filePixelsB))
    {
        std::remove(filePath.c_str());
        return Fail("could not update file texture fixture");
    }
    if (!cache.Reload(fileSpec.assetId))
    {
        std::remove(filePath.c_str());
        return Fail("changed file texture reload failed");
    }
    auto fileTextureB = cache.Find(fileSpec.assetId);
    if (!fileTextureB || fileTextureB == fileTextureA || device.textureCreations != 4 ||
        !fileTextureA->IsLoaded())
    {
        std::remove(filePath.c_str());
        return Fail("transactional reload did not publish a replacement safely");
    }

    std::remove(filePath.c_str());
    if (cache.Reload(fileSpec.assetId) || cache.Find(fileSpec.assetId) != fileTextureB)
    {
        return Fail("failed reload did not preserve the previous valid texture");
    }

    first.reset();
    second.reset();
    aliasTexture.reset();
    srgbTexture.reset();
    fileTextureA.reset();
    fileTextureAlias.reset();
    fileTextureB.reset();

    const u32 collected = cache.CollectUnused();
    if (collected == 0 || cache.GetResidentCount() != 0)
    {
        return Fail("unused texture collection did not clear cache-only resources");
    }

    const auto stats = cache.GetStats();
    if (stats.cacheHits < 3 || stats.cacheMisses < 4 || stats.texturesCreated != 4 ||
        stats.reloadAttempts != 3 || stats.reloadSuccesses != 2 ||
        stats.reloadFailures != 1 || stats.reloadReuses != 1 ||
        stats.residentTextures != 0)
    {
        return Fail("texture cache statistics are inconsistent");
    }

    std::cout << "Texture cache tests passed\n";
    return EXIT_SUCCESS;
}
