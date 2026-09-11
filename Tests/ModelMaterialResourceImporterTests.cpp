#include <Pyramid/Graphics/Model/ModelResourceImporter.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Model/ObjImporter.hpp>
#include <Pyramid/Util/Log.hpp>

#include "TestGraphicsDevice.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <variant>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "ModelMaterialResourceImporterTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    class TestShader final : public Pyramid::IShader
    {
    public:
        void Bind() override {}
        void Unbind() override {}
        bool Compile(const std::string&, const std::string&) override { return true; }
        bool CompileWithGeometry(
            const std::string&,
            const std::string&,
            const std::string&) override { return true; }
        bool CompileWithTessellation(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&) override { return true; }
        bool CompileAdvanced(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&) override { return true; }
        void SetUniformInt(const std::string&, int) override {}
        void SetUniformFloat(const std::string&, float) override {}
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string&, const float*, bool, int) override {}
        void BindUniformBuffer(const std::string&, Pyramid::IUniformBuffer*, Pyramid::u32) override {}
        void SetUniformBlockBinding(const std::string&, Pyramid::u32) override {}
        void BindShaderStorageBuffer(
            const std::string&,
            Pyramid::IShaderStorageBuffer*,
            Pyramid::u32) override {}
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

    private:
        Pyramid::TextureSpecification m_specification;
        std::string m_path;
    };

    bool WriteTga(const std::filesystem::path& path, bool alternate = false)
    {
        std::filesystem::create_directories(path.parent_path());
        std::array<unsigned char, 18> header{};
        header[2] = 2;
        header[12] = 2;
        header[14] = 2;
        header[16] = 24;
        header[17] = 0x20;
        std::array<unsigned char, 12> pixels = {
            0, 0, 255,
            0, 255, 0,
            255, 0, 0,
            0, 255, 255};
        if (alternate)
        {
            pixels[0] = 127;
        }

        std::ofstream file(path, std::ios::binary);
        if (!file)
        {
            return false;
        }
        file.write(
            reinterpret_cast<const char*>(header.data()),
            static_cast<std::streamsize>(header.size()));
        file.write(
            reinterpret_cast<const char*>(pixels.data()),
            static_cast<std::streamsize>(pixels.size()));
        return static_cast<bool>(file);
    }

    Pyramid::Model::ImportedModel MakeModel(const std::filesystem::path& root)
    {
        Pyramid::Model::ObjImportRequest request;
        request.sourcePath = (root / "model.obj").generic_string();
        request.objText = R"OBJ(
mtllib model.mtl
v 0 0 0
v 1 0 0
v 0 1 0
v 2 0 0
v 3 0 0
v 2 1 0
vt 0 0
vt 1 0
vt 0 1
usemtl Textured
f 1/1 2/2 3/3
usemtl Plain
f 4/1 5/2 6/3
)OBJ";
        request.materialLibraries.push_back({
            (root / "model.mtl").generic_string(),
            R"MTL(
newmtl Textured
Ka 0.1 0.2 0.3
Kd 0.8 0.6 0.4
Ks 0.3 0.2 0.1
Ns 24
d 0.75
illum 2
map_Kd diffuse.tga
newmtl Plain
Kd 0.2 0.4 0.6
Ns 4
illum 1
)MTL"});
        return Pyramid::Model::ObjImporter::Import(request);
    }

    Pyramid::ShaderHandle CreateShader(Pyramid::ResourceRegistry& resources)
    {
        Pyramid::ShaderProgramSpecification specification;
        specification.vertexSource = "void main(){}";
        specification.fragmentSource = "void main(){}";
        specification.assetId = Pyramid::ShaderAssetId::FromString("tests/model-material/shader");
        return resources.AcquireShader(specification);
    }

    Pyramid::ModelResourceImportOptions MakeOptions(
        Pyramid::ShaderHandle shader,
        std::string prefix)
    {
        Pyramid::ModelResourceImportOptions options;
        options.assetPrefix = std::move(prefix);
        options.materialProfile.shader = shader;
        options.materialProfile.diffuseTexture.generateMips = false;
        options.materialProfile.diffuseTexture.minFilter = Pyramid::TextureFilter::Linear;
        options.materialProfile.diffuseTexture.colorSpace = Pyramid::TextureColorSpace::SRGB;
        return options;
    }

    const Pyramid::MaterialUniform* FindUniform(
        const Pyramid::Material& material,
        const std::string& name)
    {
        for (const auto& uniform : material.GetUniforms())
        {
            if (uniform.name == name)
            {
                return &uniform;
            }
        }
        return nullptr;
    }

    bool HasWarning(const Pyramid::ModelResourceImportResult& result)
    {
        for (const auto& diagnostic : result.diagnostics)
        {
            if (!diagnostic.IsError())
            {
                return true;
            }
        }
        return false;
    }
}

int main()
{
    using namespace Pyramid;

    Util::Logger::GetInstance().EnableConsole(false);
    const std::filesystem::path root = "pyramid_step32_model_fixtures";
    std::filesystem::remove_all(root);
    if (!WriteTga(root / "diffuse.tga") ||
        !WriteTga(root / "alternate.tga", true))
    {
        return Fail("could not write diffuse texture fixtures");
    }
    {
        std::ofstream malformed(root / "malformed.tga", std::ios::binary);
        malformed << "not-an-image";
        if (!malformed)
        {
            std::filesystem::remove_all(root);
            return Fail("could not write malformed texture fixture");
        }
    }

    const auto model = MakeModel(root);
    if (!model.IsValid() || model.materials.size() != 2 || model.primitives.size() != 2)
    {
        std::filesystem::remove_all(root);
        return Fail("OBJ/MTL integration fixture did not parse");
    }

    Tests::TestGraphicsDevice device;
    device.shaderFactory = []() { return std::make_shared<TestShader>(); };
    device.textureFactory = [](const TextureSpecification& specification, const void*)
    {
        return std::make_shared<TestTexture>(specification);
    };
    ResourceRegistry resources(device);
    const ShaderHandle shader = CreateShader(resources);
    if (!shader)
    {
        std::filesystem::remove_all(root);
        return Fail("could not create shader profile fixture");
    }

    auto options = MakeOptions(shader, "tests/model-material/valid");
    const auto imported = ModelResourceImporter::ImportModel(resources, model, options);
    if (!imported.IsSuccess() || imported.renderables.size() != 2 ||
        imported.materials.size() != 2 || device.textureCreations != 1 ||
        device.vertexBufferCreations != 2)
    {
        std::filesystem::remove_all(root);
        return Fail("valid model resources were not published");
    }

    const auto texturedMaterial = resources.Resolve(imported.materials[0].material);
    const auto texturedTexture = resources.Resolve(imported.materials[0].diffuseTexture);
    if (!texturedMaterial || !texturedTexture || texturedMaterial->GetTextures().size() != 1 ||
        texturedTexture->GetColorSpace() != TextureColorSpace::SRGB ||
        texturedTexture->GetSpecification().GenerateMips ||
        texturedMaterial->GetRenderState().blendMode != MaterialBlendMode::Alpha)
    {
        std::filesystem::remove_all(root);
        return Fail("texture settings or alpha material conversion is incorrect");
    }

    const auto* baseColor = FindUniform(*texturedMaterial, "u_BaseColor");
    const auto* opacity = FindUniform(*texturedMaterial, "u_Opacity");
    const auto* hasTexture = FindUniform(*texturedMaterial, "u_HasAlbedoMap");
    if (!baseColor || !opacity || !hasTexture ||
        !std::holds_alternative<Math::Vec4>(baseColor->value) ||
        !std::holds_alternative<f32>(opacity->value) ||
        !std::holds_alternative<i32>(hasTexture->value) ||
        std::get<i32>(hasTexture->value) != 1)
    {
        std::filesystem::remove_all(root);
        return Fail("MTL properties were not converted to material uniforms");
    }

    const auto plainMaterial = resources.Resolve(imported.materials[1].material);
    const auto* plainHasTexture = plainMaterial
        ? FindUniform(*plainMaterial, "u_HasAlbedoMap")
        : nullptr;
    if (!plainMaterial || !plainMaterial->GetTextures().empty() ||
        !plainHasTexture || std::get<i32>(plainHasTexture->value) != 0)
    {
        std::filesystem::remove_all(root);
        return Fail("untextured MTL material was converted incorrectly");
    }

    const auto firstTextureHandle = imported.materials[0].diffuseTexture;
    const auto firstMaterialHandle = imported.materials[0].material;
    const auto repeated = ModelResourceImporter::ImportModel(resources, model, options);
    if (!repeated.IsSuccess() || device.textureCreations != 1 ||
        device.vertexBufferCreations != 2 ||
        repeated.materials[0].diffuseTexture != firstTextureHandle ||
        repeated.materials[0].material != firstMaterialHandle)
    {
        std::filesystem::remove_all(root);
        return Fail("repeat import did not reuse cached texture, material, and mesh resources");
    }

    {
        const std::string conflictPrefix = "tests/model-material/texture-conflict";
        TextureFileSpecification residentTexture;
        residentTexture.filepath = (root / "alternate.tga").generic_string();
        residentTexture.colorSpace = TextureColorSpace::SRGB;
        residentTexture.generateMips = false;
        residentTexture.minFilter = TextureFilter::Linear;
        residentTexture.assetId = TextureAssetId::FromString(
            conflictPrefix + "/material/0/diffuse-texture");
        if (!resources.Textures().GetOrCreate(residentTexture))
        {
            std::filesystem::remove_all(root);
            return Fail("could not create stable texture conflict fixture");
        }

        auto conflictOptions = MakeOptions(shader, conflictPrefix);
        conflictOptions.materialProfile.missingTextureBehavior =
            ModelMissingTextureBehavior::Ignore;
        const auto textureConflict = ModelResourceImporter::ImportModel(
            resources,
            model,
            conflictOptions);
        if (textureConflict.IsSuccess() || textureConflict.GetErrorCount() == 0)
        {
            std::filesystem::remove_all(root);
            return Fail("stable texture conflict was ignored by missing-texture policy");
        }
    }

    auto conflicting = model;
    conflicting.materials[0].diffuseColor.x = 0.1f;
    const auto conflict = ModelResourceImporter::ImportModel(resources, conflicting, options);
    if (conflict.IsSuccess() || conflict.GetErrorCount() == 0 ||
        resources.Resolve(firstMaterialHandle) != texturedMaterial)
    {
        std::filesystem::remove_all(root);
        return Fail("stable material identifier conflict was not rejected safely");
    }

    const auto countsBeforeMissing = resources.GetStats();
    auto missing = model;
    missing.materials[0].diffuseTexture = (root / "missing.tga").generic_string();
    auto missingOptions = MakeOptions(shader, "tests/model-material/missing-error");
    const auto missingError = ModelResourceImporter::ImportModel(
        resources,
        missing,
        missingOptions);
    const auto countsAfterMissing = resources.GetStats();
    if (missingError.IsSuccess() || missingError.GetErrorCount() == 0 ||
        countsAfterMissing.meshes.residentMeshes != countsBeforeMissing.meshes.residentMeshes ||
        countsAfterMissing.textures.residentTextures != countsBeforeMissing.textures.residentTextures ||
        countsAfterMissing.materials.residentMaterials != countsBeforeMissing.materials.residentMaterials)
    {
        std::filesystem::remove_all(root);
        return Fail("missing-texture error did not preserve previous resources");
    }

    auto malformed = model;
    malformed.materials[0].diffuseTexture =
        (root / "malformed.tga").generic_string();
    auto malformedOptions = MakeOptions(
        shader,
        "tests/model-material/malformed-error");
    const auto malformedError = ModelResourceImporter::ImportModel(
        resources,
        malformed,
        malformedOptions);
    const auto countsAfterMalformed = resources.GetStats();
    if (malformedError.IsSuccess() || malformedError.GetErrorCount() == 0 ||
        countsAfterMalformed.meshes.residentMeshes != countsBeforeMissing.meshes.residentMeshes ||
        countsAfterMalformed.textures.residentTextures != countsBeforeMissing.textures.residentTextures ||
        countsAfterMalformed.materials.residentMaterials != countsBeforeMissing.materials.residentMaterials)
    {
        std::filesystem::remove_all(root);
        return Fail("malformed-texture error did not preserve previous resources");
    }

    auto ignoreOptions = MakeOptions(shader, "tests/model-material/missing-ignore");
    ignoreOptions.materialProfile.missingTextureBehavior = ModelMissingTextureBehavior::Ignore;
    const auto ignored = ModelResourceImporter::ImportModel(resources, missing, ignoreOptions);
    if (!ignored.IsSuccess() || !HasWarning(ignored) || ignored.materials[0].diffuseTexture)
    {
        std::filesystem::remove_all(root);
        return Fail("missing-texture ignore behavior did not publish an untextured material");
    }

    auto staleProfile = MakeOptions({}, "tests/model-material/no-shader");
    const auto noShader = ModelResourceImporter::ImportModel(resources, model, staleProfile);
    if (noShader.IsSuccess() || noShader.GetErrorCount() == 0)
    {
        std::filesystem::remove_all(root);
        return Fail("invalid shader profile was accepted");
    }

    auto duplicateUniforms = MakeOptions(
        shader,
        "tests/model-material/duplicate-uniforms");
    duplicateUniforms.materialProfile.uniforms.opacity =
        duplicateUniforms.materialProfile.uniforms.diffuseColor;
    const auto duplicateUniformResult = ModelResourceImporter::ImportModel(
        resources,
        model,
        duplicateUniforms);
    if (duplicateUniformResult.IsSuccess() ||
        duplicateUniformResult.GetErrorCount() == 0)
    {
        std::filesystem::remove_all(root);
        return Fail("duplicate material profile uniform names were accepted");
    }

    auto invalidSampler = MakeOptions(
        shader,
        "tests/model-material/invalid-sampler");
    invalidSampler.materialProfile.diffuseTexture.maxAnisotropy = 0.0f;
    const auto invalidSamplerResult = ModelResourceImporter::ImportModel(
        resources,
        model,
        invalidSampler);
    if (invalidSamplerResult.IsSuccess() || invalidSamplerResult.GetErrorCount() == 0)
    {
        std::filesystem::remove_all(root);
        return Fail("invalid texture import settings were accepted");
    }

    {
        Tests::TestGraphicsDevice rollbackDevice;
        rollbackDevice.shaderFactory = []() { return std::make_shared<TestShader>(); };
        rollbackDevice.textureFactory = [](const TextureSpecification& specification, const void*)
        {
            return std::make_shared<TestTexture>(specification);
        };
        ResourceRegistry rollbackResources(rollbackDevice);
        const ShaderHandle rollbackShader = CreateShader(rollbackResources);
        rollbackDevice.failVertexBufferCreationAt = 2;
        const auto rollbackOptions = MakeOptions(
            rollbackShader,
            "tests/model-material/rollback");
        const auto failed = ModelResourceImporter::ImportModel(
            rollbackResources,
            model,
            rollbackOptions);
        const auto rollbackStats = rollbackResources.GetStats();
        if (failed.IsSuccess() || failed.GetErrorCount() == 0 ||
            rollbackStats.meshes.residentMeshes != 0 ||
            rollbackStats.textures.residentTextures != 0 ||
            rollbackStats.materials.residentMaterials != 0 ||
            rollbackStats.shaders.residentPrograms != 1)
        {
            std::filesystem::remove_all(root);
            return Fail("failed mesh publication did not roll back imported materials and textures");
        }
    }

    {
        Model::ImportedModel unassigned = model;
        unassigned.materials.clear();
        unassigned.primitives.resize(1);
        unassigned.primitives[0].materialIndex = -1;

        MaterialSpecification fallbackSpecification;
        fallbackSpecification.shader = resources.Resolve(shader);
        fallbackSpecification.assetId = MaterialAssetId::FromString(
            "tests/model-material/fallback");
        fallbackSpecification.name = "Fallback";
        const MaterialHandle fallback = resources.AcquireMaterial(fallbackSpecification);

        auto fallbackOptions = MakeOptions(shader, "tests/model-material/unassigned");
        fallbackOptions.materialProfile.fallbackMaterial = fallback;
        const auto fallbackImport = ModelResourceImporter::ImportModel(
            resources,
            unassigned,
            fallbackOptions);
        if (!fallbackImport.IsSuccess() || fallbackImport.renderables.size() != 1 ||
            fallbackImport.renderables[0].material != fallback)
        {
            std::filesystem::remove_all(root);
            return Fail("configured fallback material was not assigned to an unassigned primitive");
        }
    }

    std::filesystem::remove_all(root);
    return EXIT_SUCCESS;
}
