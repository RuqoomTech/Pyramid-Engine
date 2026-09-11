#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Util/Log.hpp>

#include "TestGraphicsDevice.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_set>

namespace
{
    using namespace Pyramid;

    class TestShader final : public IShader
    {
    public:
        void Bind() override {}
        void Unbind() override {}
        bool Compile(const std::string&, const std::string&) override { return true; }
        bool CompileWithGeometry(const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileWithTessellation(const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileAdvanced(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
        void SetUniformInt(const std::string&, int) override {}
        void SetUniformFloat(const std::string&, float) override {}
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string&, const float*, bool, int) override {}
        void BindUniformBuffer(const std::string&, IUniformBuffer*, u32) override {}
        void SetUniformBlockBinding(const std::string&, u32) override {}
        void BindShaderStorageBuffer(const std::string&, IShaderStorageBuffer*, u32) override {}
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

    int Fail(const char* message)
    {
        std::cerr << "ResourceHandleTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    MeshSpecification MakeMeshSpecification(
        const std::array<Vertex, 3>& vertices,
        const std::array<u32, 3>& indices,
        MeshAssetId assetId)
    {
        MeshSpecification specification;
        specification.vertexData = vertices.data();
        specification.vertexDataSize = sizeof(vertices);
        specification.vertexCount = static_cast<u32>(vertices.size());
        specification.layout = {
            {ShaderDataType::Float3, "Position"},
            {ShaderDataType::Float4, "Color"}};
        specification.indexData = indices.data();
        specification.indexCount = static_cast<u32>(indices.size());
        specification.assetId = assetId;
        return specification;
    }
}

int main()
{
    static_assert(!std::is_same<MeshHandle, MaterialHandle>::value, "handles must remain typed");
    Pyramid::Util::Logger::GetInstance().EnableConsole(false);

    Pyramid::Tests::TestGraphicsDevice device;
    device.shaderFactory = []() { return std::make_shared<TestShader>(); };
    device.textureFactory = [](const TextureSpecification& specification, const void*)
    {
        return std::make_shared<TestTexture>(specification);
    };

    ResourceRegistry registry(device);
    if (MeshHandle{}.IsValid() || ShaderHandle{}.IsValid() ||
        TextureHandle{}.IsValid() || MaterialHandle{}.IsValid())
    {
        return Fail("default handles must be invalid");
    }

    const std::array<Vertex, 3> vertices = {
        Vertex{-2.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        Vertex{2.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        Vertex{0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}};
    const std::array<u32, 3> indices = {0, 1, 2};
    const MeshAssetId meshId = MeshAssetId::FromString("handles/mesh");
    const auto meshSpecification = MakeMeshSpecification(vertices, indices, meshId);

    const MeshHandle meshHandle = registry.AcquireMesh(meshSpecification);
    const auto mesh = registry.Resolve(meshHandle);
    if (!meshHandle || !mesh || !registry.IsAlive(meshHandle) ||
        meshHandle.GetAssetId() != meshId || meshHandle.GetGeneration() == 0)
    {
        return Fail("mesh handle acquisition or resolution failed");
    }

    std::unordered_set<MeshHandle, MeshHandleHash> handleSet;
    handleSet.insert(meshHandle);
    if (handleSet.count(meshHandle) != 1)
    {
        return Fail("mesh handle hashing failed");
    }

    RenderObject object;
    if (!object.SetMeshHandle(meshHandle, registry) || object.mesh ||
        object.ResolveMesh(&registry) != mesh)
    {
        return Fail("render object did not adopt a mesh handle");
    }
    Math::Vec3 boundsMin;
    Math::Vec3 boundsMax;
    if (!object.GetLocalBounds(boundsMin, boundsMax) ||
        boundsMin.x != -2.0f || boundsMax.x != 2.0f || boundsMax.y != 3.0f)
    {
        return Fail("handle-backed render bounds were not retained");
    }

    if (!registry.Meshes().Evict(meshId) || registry.Resolve(meshHandle) ||
        registry.IsAlive(meshHandle))
    {
        return Fail("direct cache eviction did not stale the mesh handle");
    }

    const MeshHandle replacementMeshHandle = registry.AcquireMesh(meshSpecification);
    if (!replacementMeshHandle || replacementMeshHandle == meshHandle ||
        replacementMeshHandle.GetGeneration() == meshHandle.GetGeneration() ||
        registry.Resolve(meshHandle) || !registry.Resolve(replacementMeshHandle))
    {
        return Fail("recreating one asset ID did not issue a new generation");
    }

    ShaderProgramSpecification shaderSpecification;
    shaderSpecification.vertexSource = "void main(){}";
    shaderSpecification.fragmentSource = "void main(){}";
    shaderSpecification.assetId = ShaderAssetId::FromString("handles/shader");
    const ShaderHandle shaderHandle = registry.AcquireShader(shaderSpecification);
    const auto shader = registry.Resolve(shaderHandle);
    if (!shaderHandle || !shader)
    {
        return Fail("shader handle acquisition failed");
    }

    ShaderProgramSpecification replacementShader = shaderSpecification;
    replacementShader.fragmentSource = "void main(){ int replacement = 1; }";
    replacementShader.assetId = {};
    const ShaderHandle recompiled = registry.RecompileShader(shaderHandle, replacementShader);
    if (!recompiled || recompiled.GetGeneration() == shaderHandle.GetGeneration() ||
        registry.Resolve(shaderHandle) || !registry.Resolve(recompiled))
    {
        return Fail("shader recompilation did not invalidate the previous generation");
    }

    const std::array<u8, 4> pixel = {255, 255, 255, 255};
    TextureResourceSpecification textureSpecification;
    textureSpecification.texture.Width = 1;
    textureSpecification.texture.Height = 1;
    textureSpecification.texture.Format = TextureFormat::RGBA8;
    textureSpecification.texture.GenerateMips = false;
    textureSpecification.pixelData = pixel.data();
    textureSpecification.pixelDataSize = pixel.size();
    textureSpecification.assetId = TextureAssetId::FromString("handles/texture");
    const TextureHandle textureHandle = registry.AcquireTexture(textureSpecification);
    const auto texture = registry.Resolve(textureHandle);
    if (!textureHandle || !texture)
    {
        return Fail("texture handle acquisition failed");
    }

    MaterialSpecification materialSpecification;
    materialSpecification.shader = registry.Resolve(recompiled);
    materialSpecification.textures = {{"u_Albedo", 0, texture}};
    materialSpecification.uniforms = {{"u_Roughness", 0.5f}};
    materialSpecification.assetId = MaterialAssetId::FromString("handles/material");
    const MaterialHandle materialHandle = registry.AcquireMaterial(materialSpecification);
    if (!materialHandle || !object.SetMaterialHandle(materialHandle, registry) ||
        object.material || !object.ResolveMaterial(&registry))
    {
        return Fail("material handle acquisition or scene adoption failed");
    }

    MaterialSpecification replacementMaterial = materialSpecification;
    replacementMaterial.uniforms = {{"u_Roughness", 0.25f}};
    replacementMaterial.assetId = {};
    const MaterialHandle replaced = registry.ReplaceMaterial(
        materialHandle,
        replacementMaterial);
    if (!replaced || replaced.GetGeneration() == materialHandle.GetGeneration() ||
        registry.Resolve(materialHandle) || !registry.Resolve(replaced))
    {
        return Fail("material replacement did not invalidate the previous generation");
    }

    const MeshAssetId collectedId = MeshAssetId::FromString("handles/collect-unused");
    const MeshHandle collectedHandle = registry.AcquireMesh(
        MakeMeshSpecification(vertices, indices, collectedId));
    if (!collectedHandle || registry.Meshes().CollectUnused() == 0 ||
        registry.Resolve(collectedHandle))
    {
        return Fail("non-owning handles incorrectly kept cache-only resources resident");
    }

    const auto forged = MeshHandle::FromParts(
        replacementMeshHandle.GetAssetId(),
        replacementMeshHandle.GetGeneration() + 1);
    if (registry.Resolve(forged) || registry.IsAlive(forged))
    {
        return Fail("forged generation unexpectedly resolved");
    }

    registry.Clear();
    if (registry.Resolve(replacementMeshHandle) || registry.Resolve(recompiled) ||
        registry.Resolve(textureHandle) || registry.Resolve(replaced))
    {
        return Fail("registry clear did not invalidate all issued handles");
    }

    std::cout << "Resource handle tests passed\n";
    return EXIT_SUCCESS;
}
