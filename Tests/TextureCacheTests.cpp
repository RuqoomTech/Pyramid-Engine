#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Texture/TextureCache.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
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
        TestTexture(Pyramid::TextureSpecification specification, Pyramid::u32 rendererId,
            std::vector<Pyramid::u8> pixels = {})
            : m_specification(specification), m_rendererId(rendererId), m_pixels(std::move(pixels))
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

        void SetData(const void* data, Pyramid::u32 size) override
        {
            ++mutationCalls;
            const auto* bytes = static_cast<const Pyramid::u8*>(data);
            m_pixels.assign(bytes, bytes + size);
        }
        void SetSubData(const void* data, Pyramid::u32, Pyramid::u32,
            Pyramid::u32, Pyramid::u32) override
        {
            ++mutationCalls;
            if (data && !m_pixels.empty())
            {
                m_pixels[0] = *static_cast<const Pyramid::u8*>(data);
            }
        }
        void SetFilter(Pyramid::TextureFilter minFilter, Pyramid::TextureFilter magFilter) override
        {
            ++mutationCalls;
            m_specification.MinFilter = minFilter;
            m_specification.MagFilter = magFilter;
        }
        void SetWrap(Pyramid::TextureWrap wrapS, Pyramid::TextureWrap wrapT) override
        {
            ++mutationCalls;
            m_specification.WrapS = wrapS;
            m_specification.WrapT = wrapT;
        }
        void SetBorderColor(Pyramid::f32 red, Pyramid::f32 green,
            Pyramid::f32 blue, Pyramid::f32 alpha) override
        {
            ++mutationCalls;
            m_specification.BorderColor[0] = red;
            m_specification.BorderColor[1] = green;
            m_specification.BorderColor[2] = blue;
            m_specification.BorderColor[3] = alpha;
        }
        void SetMaxAnisotropy(Pyramid::f32 value) override
        {
            ++mutationCalls;
            m_specification.MaxAnisotropy = value;
        }
        void GenerateMipmaps() override
        {
            ++mutationCalls;
            m_specification.GenerateMips = true;
        }
        bool LoadFromFile(const std::string& path, bool, bool) override
        {
            ++mutationCalls;
            m_path = path;
            return true;
        }
        const Pyramid::TextureSpecification& GetSpecification() const { return m_specification; }
        const std::vector<Pyramid::u8>& GetPixels() const { return m_pixels; }
        Pyramid::u32 mutationCalls = 0;

    private:
        Pyramid::TextureSpecification m_specification;
        Pyramid::u32 m_rendererId = 0;
        std::vector<Pyramid::u8> m_pixels;
        std::string m_path;
    };

    static_assert(std::is_same<
        decltype(std::declval<const Pyramid::TextureResource&>().GetTexture()),
        const std::shared_ptr<const Pyramid::ITexture2D>&>::value,
        "Cached textures must expose a const pointee so native setters cannot bypass immutability");

    bool SameSpecification(const Pyramid::TextureSpecification& left,
        const Pyramid::TextureSpecification& right)
    {
        return left.Width == right.Width && left.Height == right.Height &&
            left.Format == right.Format && left.MinFilter == right.MinFilter &&
            left.MagFilter == right.MagFilter && left.WrapS == right.WrapS &&
            left.WrapT == right.WrapT && left.GenerateMips == right.GenerateMips &&
            left.SRGB == right.SRGB && left.FlipY == right.FlipY &&
            left.MaxAnisotropy == right.MaxAnisotropy &&
            std::equal(std::begin(left.BorderColor), std::end(left.BorderColor),
                std::begin(right.BorderColor));
    }

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

    int TestSamplerImmutabilityAndIdentity()
    {
        using namespace Pyramid;
        const std::vector<u8> pixels{255, 0, 0, 255, 0, 255, 0, 255};
        Tests::TestGraphicsDevice device;
        std::shared_ptr<TestTexture> native;
        device.textureFactory = [&](const TextureSpecification& specification, const void* data)
        {
            const auto* bytes = static_cast<const u8*>(data);
            native = std::make_shared<TestTexture>(specification, device.textureCreations,
                std::vector<u8>(bytes, bytes + pixels.size()));
            return native;
        };
        TextureCache cache(device);
        TextureResourceSpecification request;
        request.texture.Width = 2;
        request.texture.Height = 1;
        request.texture.GenerateMips = false;
        request.texture.MinFilter = TextureFilter::Linear;
        request.texture.WrapS = TextureWrap::ClampToBorder;
        request.texture.BorderColor[0] = 0.25f;
        request.texture.MaxAnisotropy = 4.0f;
        request.pixelData = pixels.data();
        request.pixelDataSize = pixels.size();
        auto resource = cache.GetOrCreate(request);
        if (!resource || !native)
        {
            return Fail("could not create immutable sampler fixture");
        }
        const TextureSpecification initialSpecification = resource->GetSpecification();
        const TextureAssetId initialContent = resource->GetContentId();
        const TextureAssetId initialAsset = resource->GetAssetId();
        const u32 initialRendererId = resource->GetRendererID();
        const auto unchanged = [&](const char* operation)
        {
            const bool matches = resource->GetLastError().find(operation) != std::string::npos &&
                resource->IsLoaded() && resource->GetContentId() == initialContent &&
                resource->GetAssetId() == initialAsset && resource->GetRendererID() == initialRendererId &&
                resource->GetWidth() == 2 && resource->GetHeight() == 1 &&
                resource->GetMipLevels() == 1 && resource->GetPath().empty() &&
                SameSpecification(resource->GetSpecification(), initialSpecification) &&
                SameSpecification(native->GetSpecification(), initialSpecification) &&
                native->GetPixels() == pixels && native->GetPath().empty() &&
                native->mutationCalls == 0 && cache.GetOrCreate(request) == resource &&
                device.textureCreations == 1;
            if (!matches)
            {
                std::cerr << "Immutable operation: " << operation << '\n';
            }
            return matches;
        };
        ITexture2D& inherited = *resource;
        const std::array<u8, 8> replacement{1, 2, 3, 4, 5, 6, 7, 8};
        inherited.SetData(replacement.data(), static_cast<u32>(replacement.size()));
        if (!unchanged("SetData"))
        {
            return Fail("cached pixel replacement changed native state or identity");
        }
        inherited.SetSubData(replacement.data(), 0, 0, 1, 1);
        if (!unchanged("SetSubData"))
        {
            return Fail("cached subimage mutation reached the native texture");
        }
        inherited.SetFilter(TextureFilter::Nearest, TextureFilter::Nearest);
        if (!unchanged("SetFilter"))
        {
            return Fail("cached filter mutation changed native state or identity");
        }
        inherited.SetWrap(TextureWrap::ClampToEdge, TextureWrap::MirroredRepeat);
        if (!unchanged("SetWrap"))
        {
            return Fail("cached wrap mutation changed native state or identity");
        }
        inherited.SetBorderColor(0.8f, 0.7f, 0.6f, 0.5f);
        if (!unchanged("SetBorderColor"))
        {
            return Fail("cached border mutation changed native state or identity");
        }
        inherited.SetMaxAnisotropy(16.0f);
        if (!unchanged("SetMaxAnisotropy"))
        {
            return Fail("cached anisotropy mutation changed native state or identity");
        }
        inherited.GenerateMipmaps();
        if (!unchanged("GenerateMipmaps"))
        {
            return Fail("cached mip generation changed native state or identity");
        }
        if (inherited.LoadFromFile("must-not-load.tga", true, true) || !unchanged("LoadFromFile"))
        {
            return Fail("cached file replacement was accepted or changed native state");
        }
        if (inherited.SaveToFile("must-not-save.tga") || !unchanged("LoadFromFile"))
        {
            return Fail("unsupported cached save succeeded or changed native state");
        }

        // Identity describes requested immutable state, independently of device capability.
        TextureAssetId clampIdentity8;
        TextureAssetId clampIdentity16;
        for (f32 deviceLimit : {4.0f, 1.0f})
        {
            Tests::TestGraphicsDevice limitedDevice;
            limitedDevice.textureFactory = [&](const TextureSpecification& specification, const void*)
            {
                TextureSpecification effective = specification;
                effective.MaxAnisotropy = std::min(effective.MaxAnisotropy, deviceLimit);
                return std::make_shared<TestTexture>(effective, limitedDevice.textureCreations);
            };
            TextureCache limitedCache(limitedDevice);
            request.texture.MaxAnisotropy = 8.0f;
            auto eight = limitedCache.GetOrCreate(request);
            request.texture.MaxAnisotropy = 16.0f;
            auto sixteen = limitedCache.GetOrCreate(request);
            if (!eight || !sixteen || eight == sixteen ||
                eight->GetContentId() == sixteen->GetContentId() ||
                eight->GetSpecification().MaxAnisotropy != 8.0f ||
                sixteen->GetSpecification().MaxAnisotropy != 16.0f ||
                static_cast<const TestTexture&>(*eight->GetTexture()).GetSpecification().MaxAnisotropy != deviceLimit ||
                static_cast<const TestTexture&>(*sixteen->GetTexture()).GetSpecification().MaxAnisotropy != deviceLimit ||
                limitedCache.GetOrCreate(request) != sixteen || limitedDevice.textureCreations != 2)
            {
                return Fail("anisotropy request identity depended on device clamp or fallback");
            }
            request.texture.MaxAnisotropy = 8.0f;
            if (limitedCache.GetOrCreate(request) != eight || limitedDevice.textureCreations != 2)
            {
                return Fail("clamped anisotropy request did not deduplicate");
            }
            if (deviceLimit == 4.0f)
            {
                clampIdentity8 = eight->GetContentId();
                clampIdentity16 = sixteen->GetContentId();
            }
            else if (clampIdentity8 != eight->GetContentId() || clampIdentity16 != sixteen->GetContentId())
            {
                return Fail("anisotropy content identifiers changed across device capabilities");
            }
        }

        request.texture.GenerateMips = true;
        request.texture.MaxAnisotropy = 4.0f;
        const TextureAssetId baseline = TextureResource::CalculateContentId(request);
        std::vector<TextureResourceSpecification> variants;
        auto variant = request;
        variant.texture.MinFilter = TextureFilter::NearestMipmapNearest;
        variants.push_back(variant);
        variant = request;
        variant.texture.MagFilter = TextureFilter::Nearest;
        variants.push_back(variant);
        variant = request;
        variant.texture.WrapS = TextureWrap::ClampToEdge;
        variants.push_back(variant);
        variant = request;
        variant.texture.WrapT = TextureWrap::MirroredRepeat;
        variants.push_back(variant);
        for (u32 channel = 0; channel < 4; ++channel)
        {
            variant = request;
            variant.texture.BorderColor[channel] += 0.125f;
            variants.push_back(variant);
        }
        for (const auto& sampler : variants)
        {
            const TextureAssetId identifier = TextureResource::CalculateContentId(sampler);
            if (!identifier.IsValid() || identifier == baseline)
            {
                return Fail("filter, wrap, or border channel was omitted from sampler identity");
            }
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
    int TestSamplerValidationAndLegacyAliases()
    {
        using namespace Pyramid;
        const std::array<u8, 8> pixels{255, 0, 0, 255, 0, 255, 0, 255};
        const std::string path = "pyramid_texture_cache_sampler_test.tga";
        if (!WriteTga(path, {255, 0, 0, 0, 255, 0}))
        {
            return Fail("could not write sampler-validation image fixture");
        }
        struct FixtureCleanup
        {
            const std::string& path;
            ~FixtureCleanup() { std::remove(path.c_str()); }
        } cleanup{path};
        Tests::TestGraphicsDevice device;
        device.textureFactory = [&](const TextureSpecification& specification, const void*)
        {
            return std::make_shared<TestTexture>(specification, device.textureCreations);
        };
        TextureCache cache(device);
        TextureResourceSpecification request;
        request.texture.Width = 2;
        request.texture.Height = 1;
        request.pixelData = pixels.data();
        request.pixelDataSize = pixels.size();
        const auto fromFile = [&](const TextureSpecification& sampler)
        {
            TextureFileSpecification file;
            file.filepath = path;
            file.generateMips = sampler.GenerateMips;
            file.minFilter = sampler.MinFilter;
            file.magFilter = sampler.MagFilter;
            file.wrapS = sampler.WrapS;
            file.wrapT = sampler.WrapT;
            file.maxAnisotropy = sampler.MaxAnisotropy;
            std::copy(std::begin(sampler.BorderColor), std::end(sampler.BorderColor), file.borderColor);
            return file;
        };
        std::vector<TextureSpecification> invalid;
        for (int unknown : {-1, 999})
        {
            auto sampler = request.texture;
            sampler.MinFilter = static_cast<TextureFilter>(unknown);
            invalid.push_back(sampler);
            sampler = request.texture;
            sampler.MagFilter = static_cast<TextureFilter>(unknown);
            invalid.push_back(sampler);
            sampler = request.texture;
            sampler.WrapS = static_cast<TextureWrap>(unknown);
            invalid.push_back(sampler);
            sampler = request.texture;
            sampler.WrapT = static_cast<TextureWrap>(unknown);
            invalid.push_back(sampler);
        }
        const f32 nan = std::numeric_limits<f32>::quiet_NaN();
        const f32 infinity = std::numeric_limits<f32>::infinity();
        for (f32 value : {0.0f, 0.5f, -1.0f, nan, infinity, -infinity})
        {
            auto sampler = request.texture;
            sampler.MaxAnisotropy = value;
            invalid.push_back(sampler);
        }
        for (u32 channel = 0; channel < 4; ++channel)
        {
            for (f32 value : {nan, infinity, -infinity})
            {
                auto sampler = request.texture;
                sampler.BorderColor[channel] = value;
                invalid.push_back(sampler);
            }
        }
        for (std::size_t index = 0; index < invalid.size(); ++index)
        {
            request.texture = invalid[index];
            const auto file = fromFile(request.texture);
            if (TextureResource::CalculateContentId(request).IsValid() ||
                TextureResource::CalculateContentId(file).IsValid() || cache.GetOrCreate(request) ||
                cache.GetOrCreate(file) || device.textureCreations != 0 || device.textureFileCreations != 0)
            {
                std::cerr << "Invalid sampler profile: " << index << '\n';
                return Fail("unknown or nonfinite sampler state was hashed or reached the graphics device");
            }
        }

        // Preserve defined legacy aliases; only unknown enum values are invalid.
        request.texture = {};
        request.texture.Width = 2;
        request.texture.Height = 1;
        const TextureFilter filters[] = {TextureFilter::None, TextureFilter::Linear,
            TextureFilter::Nearest, TextureFilter::LinearMipmapLinear,
            TextureFilter::LinearMipmapNearest, TextureFilter::NearestMipmapLinear,
            TextureFilter::NearestMipmapNearest};
        for (TextureFilter filter : filters)
        {
            request.texture.MinFilter = filter;
            request.texture.MagFilter = filter;
            auto memory = cache.GetOrCreate(request);
            auto file = cache.GetOrCreate(fromFile(request.texture));
            if (!memory || !file || memory->GetSpecification().MinFilter != filter ||
                memory->GetSpecification().MagFilter != filter ||
                file->GetSpecification().MinFilter != filter || file->GetSpecification().MagFilter != filter ||
                cache.GetOrCreate(request) != memory)
            {
                return Fail("defined legacy filter aliases were rejected or renormalized");
            }
        }
        request.texture.MinFilter = TextureFilter::Linear;
        request.texture.MagFilter = TextureFilter::Linear;
        const TextureWrap wraps[] = {TextureWrap::None, TextureWrap::Repeat,
            TextureWrap::MirroredRepeat, TextureWrap::ClampToEdge, TextureWrap::ClampToBorder};
        for (TextureWrap wrap : wraps)
        {
            request.texture.WrapS = wrap;
            request.texture.WrapT = wrap;
            auto memory = cache.GetOrCreate(request);
            auto file = cache.GetOrCreate(fromFile(request.texture));
            if (!memory || !file || memory->GetSpecification().WrapS != wrap ||
                memory->GetSpecification().WrapT != wrap ||
                file->GetSpecification().WrapS != wrap || file->GetSpecification().WrapT != wrap)
            {
                return Fail("defined legacy wrap aliases were rejected or renormalized");
            }
        }
        request.texture.GenerateMips = false;
        request.texture.MinFilter = TextureFilter::Linear;
        const TextureAssetId noMips = TextureResource::CalculateContentId(request);
        for (TextureFilter filter : {TextureFilter::LinearMipmapLinear, TextureFilter::LinearMipmapNearest,
                TextureFilter::NearestMipmapLinear, TextureFilter::NearestMipmapNearest})
        {
            request.texture.MinFilter = filter;
            auto memory = cache.GetOrCreate(request);
            if (!memory || memory->GetContentId() != noMips ||
                memory->GetSpecification().MinFilter != TextureFilter::Linear)
            {
                return Fail("legacy no-mips cache normalization changed content identity");
            }
        }
        request.texture.MinFilter = TextureFilter::Linear;
        request.texture.MaxAnisotropy = std::numeric_limits<f32>::max();
        request.texture.BorderColor[0] = -4.0f;
        request.texture.BorderColor[1] = 8.0f;
        if (!cache.GetOrCreate(request) || !cache.GetOrCreate(fromFile(request.texture)))
        {
            return Fail("finite border values or device-clamped finite anisotropy were rejected");
        }
        return EXIT_SUCCESS;
    }
}

int main()
{
    using namespace Pyramid;

    if (TestUncompressedFormats() != EXIT_SUCCESS ||
        TestSamplerImmutabilityAndIdentity() != EXIT_SUCCESS ||
        TestSamplerValidationAndLegacyAliases() != EXIT_SUCCESS)
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
