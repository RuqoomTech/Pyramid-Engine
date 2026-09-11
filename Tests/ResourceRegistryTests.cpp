#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Util/Log.hpp>

#include "TestGraphicsDevice.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

namespace
{
    using namespace Pyramid;

    class TestShader final : public IShader
    {
    public:
        void Bind() override {}
        void Unbind() override {}
        bool Compile(const std::string&, const std::string&) override { return true; }
        bool CompileWithGeometry(
            const std::string&,
            const std::string&,
            const std::string&) override
        {
            return true;
        }
        bool CompileWithTessellation(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&) override
        {
            return true;
        }
        bool CompileAdvanced(
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&,
            const std::string&) override
        {
            return true;
        }
        void SetUniformInt(const std::string&, int) override {}
        void SetUniformFloat(const std::string&, float) override {}
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string&, const float*, bool, int) override {}
        void BindUniformBuffer(const std::string&, IUniformBuffer*, u32) override {}
        void SetUniformBlockBinding(const std::string&, u32) override {}
        void BindShaderStorageBuffer(
            const std::string&,
            IShaderStorageBuffer*,
            u32) override
        {
        }
        void SetShaderStorageBlockBinding(const std::string&, u32) override {}
    };

    class TestTexture final : public ITexture2D
    {
    public:
        explicit TestTexture(TextureSpecification specification)
            : m_specification(specification)
        {
        }

        void Bind(u32 = 0) const override {}
        void Unbind(u32 = 0) const override {}
        u32 GetWidth() const override { return m_specification.Width; }
        u32 GetHeight() const override { return m_specification.Height; }
        u32 GetRendererID() const override { return 1; }
        TextureFormat GetFormat() const override { return m_specification.Format; }
        const std::string& GetPath() const override { return m_path; }
        bool IsLoaded() const override { return true; }
        u32 GetMipLevels() const override { return 1; }

    private:
        TextureSpecification m_specification;
        std::string m_path;
    };

    struct ResourceSet
    {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<ShaderProgram> shader;
        std::shared_ptr<TextureResource> texture;
        std::shared_ptr<Material> material;
    };

    int Fail(const char* message)
    {
        std::cerr << "ResourceRegistryTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    ResourceSet CreateResourceSet(ResourceRegistry& registry, std::string_view suffix)
    {
        ResourceSet resources;

        const std::array<Vertex, 3> vertices = {
            Vertex{-1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
            Vertex{1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
            Vertex{0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}};
        const std::array<u32, 3> indices = {0, 1, 2};

        MeshSpecification meshSpecification;
        meshSpecification.vertexData = vertices.data();
        meshSpecification.vertexDataSize = sizeof(vertices);
        meshSpecification.vertexCount = static_cast<u32>(vertices.size());
        meshSpecification.layout = {
            {ShaderDataType::Float3, "Position"},
            {ShaderDataType::Float4, "Color"}};
        meshSpecification.indexData = indices.data();
        meshSpecification.indexCount = static_cast<u32>(indices.size());
        meshSpecification.assetId = MeshAssetId::FromString(
            std::string("registry/mesh/") + std::string(suffix));
        resources.mesh = registry.Meshes().GetOrCreate(meshSpecification);

        ShaderProgramSpecification shaderSpecification;
        shaderSpecification.vertexSource = "void main(){}";
        shaderSpecification.fragmentSource = "void main(){}";
        shaderSpecification.assetId = ShaderAssetId::FromString(
            std::string("registry/shader/") + std::string(suffix));
        resources.shader = registry.Shaders().GetOrCreate(shaderSpecification);

        const std::array<u8, 4> pixel = {255, 255, 255, 255};
        TextureResourceSpecification textureSpecification;
        textureSpecification.texture.Width = 1;
        textureSpecification.texture.Height = 1;
        textureSpecification.texture.Format = TextureFormat::RGBA8;
        textureSpecification.texture.GenerateMips = false;
        textureSpecification.pixelData = pixel.data();
        textureSpecification.pixelDataSize = pixel.size();
        textureSpecification.assetId = TextureAssetId::FromString(
            std::string("registry/texture/") + std::string(suffix));
        resources.texture = registry.Textures().GetOrCreate(textureSpecification);

        MaterialSpecification materialSpecification;
        materialSpecification.shader = resources.shader;
        materialSpecification.textures = {{"u_Albedo", 0, resources.texture}};
        materialSpecification.uniforms = {{"u_Roughness", 0.5f}};
        materialSpecification.assetId = MaterialAssetId::FromString(
            std::string("registry/material/") + std::string(suffix));
        resources.material = registry.Materials().GetOrCreate(materialSpecification);

        return resources;
    }
}

int main()
{
    Pyramid::Util::Logger::GetInstance().EnableConsole(false);

    Pyramid::Tests::TestGraphicsDevice device;
    device.shaderFactory = []() { return std::make_shared<TestShader>(); };
    device.textureFactory = [](const TextureSpecification& specification, const void*)
    {
        return std::make_shared<TestTexture>(specification);
    };

    ResourceRegistry registry(device);
    if (&registry.GetDevice() != &device ||
        &registry.Meshes() != &static_cast<const ResourceRegistry&>(registry).Meshes() ||
        &registry.Shaders() != &static_cast<const ResourceRegistry&>(registry).Shaders() ||
        &registry.Textures() != &static_cast<const ResourceRegistry&>(registry).Textures() ||
        &registry.Materials() != &static_cast<const ResourceRegistry&>(registry).Materials())
    {
        return Fail("registry accessors do not expose stable cache instances");
    }

    {
        auto resources = CreateResourceSet(registry, "collect-all");
        if (!resources.mesh || !resources.shader || !resources.texture || !resources.material)
        {
            return Fail("could not create registry resource fixtures");
        }

        const auto statistics = registry.GetStats();
        if (statistics.meshes.residentMeshes != 1 ||
            statistics.shaders.residentPrograms != 1 ||
            statistics.textures.residentTextures != 1 ||
            statistics.materials.residentMaterials != 1 ||
            statistics.GetEstimatedResidentBytes() == 0)
        {
            return Fail("combined registry statistics are inconsistent");
        }
    }

    const auto collectedAll = registry.CollectUnused();
    if (collectedAll.materials != 1 || collectedAll.textures != 1 ||
        collectedAll.shaders != 1 || collectedAll.meshes != 1 ||
        collectedAll.GetTotal() != 4 || !collectedAll.HasChanges())
    {
        return Fail("dependency-safe collection did not release all cache-only resources");
    }

    auto retained = CreateResourceSet(registry, "retained-material");
    if (!retained.material)
    {
        return Fail("could not create retained material fixture");
    }
    retained.mesh.reset();
    retained.shader.reset();
    retained.texture.reset();

    const auto firstPass = registry.CollectUnused();
    if (firstPass.meshes != 1 || firstPass.materials != 0 ||
        firstPass.textures != 0 || firstPass.shaders != 0 ||
        registry.Materials().GetResidentCount() != 1 ||
        registry.Textures().GetResidentCount() != 1 ||
        registry.Shaders().GetResidentCount() != 1)
    {
        return Fail("retained material did not keep shader and texture dependencies resident");
    }

    retained.material.reset();
    const auto secondPass = registry.CollectUnused();
    if (secondPass.materials != 1 || secondPass.textures != 1 ||
        secondPass.shaders != 1 || secondPass.meshes != 0)
    {
        return Fail("dependency caches were not collected after the material owner released");
    }

    auto externallyOwned = CreateResourceSet(registry, "clear-with-owners");
    std::weak_ptr<Mesh> weakMesh = externallyOwned.mesh;
    std::weak_ptr<ShaderProgram> weakShader = externallyOwned.shader;
    std::weak_ptr<TextureResource> weakTexture = externallyOwned.texture;
    std::weak_ptr<Material> weakMaterial = externallyOwned.material;

    const auto cleared = registry.Clear();
    if (cleared.materials != 1 || cleared.textures != 1 ||
        cleared.shaders != 1 || cleared.meshes != 1 ||
        registry.GetStats().materials.residentMaterials != 0 ||
        registry.GetStats().textures.residentTextures != 0 ||
        registry.GetStats().shaders.residentPrograms != 0 ||
        registry.GetStats().meshes.residentMeshes != 0 ||
        weakMesh.expired() || weakShader.expired() || weakTexture.expired() ||
        weakMaterial.expired() || !externallyOwned.material->IsValid())
    {
        return Fail("clear did not preserve externally owned resources");
    }

    externallyOwned = {};
    if (!weakMesh.expired() || !weakShader.expired() ||
        !weakTexture.expired() || !weakMaterial.expired())
    {
        return Fail("external resources did not release after their final owners reset");
    }

    const auto emptyClear = registry.Clear();
    if (emptyClear.HasChanges() || emptyClear.GetTotal() != 0)
    {
        return Fail("clearing an empty registry reported released resources");
    }

    std::weak_ptr<Mesh> destructorMesh;
    std::weak_ptr<ShaderProgram> destructorShader;
    std::weak_ptr<TextureResource> destructorTexture;
    std::weak_ptr<Material> destructorMaterial;
    {
        ResourceRegistry scopedRegistry(device);
        {
            auto scopedResources = CreateResourceSet(scopedRegistry, "destructor");
            destructorMesh = scopedResources.mesh;
            destructorShader = scopedResources.shader;
            destructorTexture = scopedResources.texture;
            destructorMaterial = scopedResources.material;
        }
    }
    if (!destructorMesh.expired() || !destructorShader.expired() ||
        !destructorTexture.expired() || !destructorMaterial.expired())
    {
        return Fail("registry destruction did not release cache-only resources");
    }

    std::cout << "Resource registry tests passed\n";
    return EXIT_SUCCESS;
}
