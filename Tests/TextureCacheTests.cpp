#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Texture/TextureCache.hpp>

#include <array>
#include <cstdio>
#include <cstdlib>
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
