#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Material/MaterialCache.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>
#include <Pyramid/Graphics/Texture/TextureResource.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return EXIT_FAILURE;
    }

    class TestShader final : public Pyramid::IShader
    {
    public:
        void Bind() override {}
        void Unbind() override {}
        bool Compile(const std::string&, const std::string&) override { return true; }
        bool CompileWithGeometry(const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileWithTessellation(const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileAdvanced(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileCompute(const std::string&) override { return true; }
        void DispatchCompute(Pyramid::u32, Pyramid::u32, Pyramid::u32) override {}
        void SetUniformInt(const std::string&, int) override {}
        void SetUniformFloat(const std::string&, float) override {}
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string&, const float*, bool, int) override {}
        void BindUniformBuffer(const std::string&, Pyramid::IUniformBuffer*, Pyramid::u32) override {}
        void SetUniformBlockBinding(const std::string&, Pyramid::u32) override {}
        void BindShaderStorageBuffer(const std::string&, Pyramid::IShaderStorageBuffer*, Pyramid::u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, Pyramid::u32) override {}
    };

    class TestTexture final : public Pyramid::ITexture2D
    {
    public:
        explicit TestTexture(Pyramid::TextureSpecification specification)
            : m_specification(specification)
        {
        }

        void Bind(Pyramid::u32 = 0) const override {}
        void Unbind(Pyramid::u32 = 0) const override {}
        Pyramid::u32 GetWidth() const override { return m_specification.Width; }
        Pyramid::u32 GetHeight() const override { return m_specification.Height; }
        Pyramid::u32 GetRendererID() const override { return 1; }
        Pyramid::TextureFormat GetFormat() const override { return m_specification.Format; }
        const std::string& GetPath() const override { return m_path; }
        bool IsLoaded() const override { return true; }
        Pyramid::u32 GetMipLevels() const override { return 1; }

    private:
        Pyramid::TextureSpecification m_specification;
        std::string m_path;
    };
}

int main()
{
    using namespace Pyramid;

    Tests::TestGraphicsDevice device;
    device.shaderFactory = []()
    {
        return std::make_shared<TestShader>();
    };
    device.textureFactory = [](const TextureSpecification& specification, const void*)
    {
        return std::make_shared<TestTexture>(specification);
    };

    ShaderProgramSpecification shaderSpecification;
    shaderSpecification.vertexSource = "void main(){}";
    shaderSpecification.fragmentSource = "void main(){}";
    shaderSpecification.assetId = ShaderAssetId::FromString("shaders/material-cache");
    auto shader = ShaderProgram::Create(device, shaderSpecification);

    const std::array<u8, 4> pixel = {255, 255, 255, 255};
    TextureResourceSpecification textureSpecification;
    textureSpecification.texture.Width = 1;
    textureSpecification.texture.Height = 1;
    textureSpecification.texture.Format = TextureFormat::RGBA8;
    textureSpecification.texture.GenerateMips = false;
    textureSpecification.pixelData = pixel.data();
    textureSpecification.pixelDataSize = pixel.size();
    textureSpecification.assetId = TextureAssetId::FromString("textures/material-cache");
    auto texture = TextureResource::Create(device, textureSpecification);

    if (!shader || !texture)
    {
        return Fail("could not create material-cache resource fixtures");
    }

    MaterialSpecification original;
    original.shader = shader;
    original.textures = {{"u_AlbedoMap", 0, texture}};
    original.uniforms = {
        {"u_Roughness", 0.25f},
        {"u_Tint", Math::Vec4(1.0f)}};
    original.assetId = MaterialAssetId::FromString("materials/cache/stable");
    original.name = "Original";

    MaterialCache cache;
    auto first = cache.GetOrCreate(original);
    auto second = cache.GetOrCreate(original);
    if (!first || first != second || cache.GetResidentCount() != 1)
    {
        return Fail("exact material requests were not deduplicated");
    }

    MaterialSpecification aliasSpecification = original;
    aliasSpecification.assetId = MaterialAssetId::FromString("materials/cache/alias");
    aliasSpecification.name = "Alias debug name";
    auto alias = cache.GetOrCreate(aliasSpecification);
    if (alias != first || !cache.Contains(aliasSpecification.assetId))
    {
        return Fail("stable material aliases did not share one resident resource");
    }

    MaterialSpecification conflict = original;
    conflict.uniforms[0].value = 0.5f;
    if (cache.GetOrCreate(conflict) || cache.GetStats().identifierConflicts != 1)
    {
        return Fail("stable identifier/content conflict was not rejected");
    }

    MaterialSpecification alternate = conflict;
    alternate.assetId = MaterialAssetId::FromString("materials/cache/alternate");
    auto alternateMaterial = cache.GetOrCreate(alternate);
    if (!alternateMaterial || alternateMaterial == first || cache.GetResidentCount() != 2)
    {
        return Fail("different material content did not create a second resident");
    }

    MaterialSpecification replacementReuse = alternate;
    replacementReuse.assetId = {};
    if (!cache.Replace(original.assetId, replacementReuse) ||
        cache.Find(original.assetId) != alternateMaterial || !first->IsValid())
    {
        return Fail("transactional replacement did not reuse resident content");
    }
    if (!cache.Replace(original.assetId, replacementReuse))
    {
        return Fail("same-content replacement was not treated as a successful reuse");
    }

    MaterialSpecification replacementCreate = original;
    replacementCreate.assetId = {};
    replacementCreate.uniforms[0].value = 0.9f;
    if (!cache.Replace(original.assetId, replacementCreate))
    {
        return Fail("transactional replacement could not create new material content");
    }
    auto replacementMaterial = cache.Find(original.assetId);
    if (!replacementMaterial || replacementMaterial == first ||
        replacementMaterial == alternateMaterial || !first->IsValid())
    {
        return Fail("new replacement was not published without invalidating old owners");
    }

    MaterialSpecification invalidReplacement = replacementCreate;
    invalidReplacement.uniforms.push_back({"u_Invalid", std::nanf("")});
    if (cache.Replace(original.assetId, invalidReplacement) ||
        cache.Find(original.assetId) != replacementMaterial)
    {
        return Fail("failed replacement did not preserve the active material");
    }

    MaterialSpecification wrongIdentifier = replacementCreate;
    wrongIdentifier.assetId = MaterialAssetId::FromString("materials/cache/wrong");
    if (cache.Replace(original.assetId, wrongIdentifier) ||
        cache.Find(original.assetId) != replacementMaterial)
    {
        return Fail("replacement with a different stable identifier was accepted");
    }

    if (cache.Replace(first->GetContentId(), replacementCreate))
    {
        return Fail("content-derived material identifier was replaceable");
    }
    if (cache.Replace(MaterialAssetId::FromString("materials/cache/missing"), replacementCreate))
    {
        return Fail("unknown material identifier was replaceable");
    }

    MaterialSpecification invalidCreate;
    if (cache.GetOrCreate(invalidCreate))
    {
        return Fail("invalid material specification was cached");
    }

    const auto residentStats = cache.GetStats();
    if (residentStats.cacheHits < 3 || residentStats.cacheMisses != 3 ||
        residentStats.materialsCreated != 3 || residentStats.creationFailures != 1 ||
        residentStats.identifierConflicts != 2 ||
        residentStats.replacementAttempts != 7 || residentStats.replacementSuccesses != 3 ||
        residentStats.replacementFailures != 4 || residentStats.replacementReuses != 2 ||
        residentStats.residentMaterials != 3 || residentStats.residentAssetIds < 6 ||
        residentStats.externallyReferencedMaterials != 3 ||
        residentStats.residentTextureBindings != 3 ||
        residentStats.residentUniformValues != 6 ||
        residentStats.estimatedResidentMetadataBytes == 0)
    {
        return Fail("material cache statistics are inconsistent");
    }

    if (!cache.Evict(original.assetId) || cache.Contains(original.assetId) ||
        !replacementMaterial->IsValid())
    {
        return Fail("material eviction did not preserve external owners");
    }

    first.reset();
    second.reset();
    alias.reset();
    alternateMaterial.reset();
    replacementMaterial.reset();

    if (cache.CollectUnused() != 2 || cache.GetResidentCount() != 0)
    {
        return Fail("unused material collection did not clear cache-only residents");
    }

    if (cache.Clear() != 0)
    {
        return Fail("clearing an empty material cache reported removed resources");
    }

    const auto finalStats = cache.GetStats();
    if (finalStats.evictions != 3 || finalStats.residentMaterials != 0 ||
        finalStats.residentAssetIds != 0)
    {
        return Fail("material cache final residency statistics are incorrect");
    }

    std::cout << "Material cache tests passed\n";
    return EXIT_SUCCESS;
}
