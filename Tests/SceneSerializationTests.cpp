#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Graphics/Resources/ResourceManifest.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/SceneSerializer.hpp>

#include "TestGraphicsDevice.hpp"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

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

    int Fail(const char* message)
    {
        std::cerr << "SceneSerializationTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    bool NearlyEqual(float left, float right, float epsilon = 0.0001f)
    {
        return std::fabs(left - right) <= epsilon;
    }

    bool NearlyEqual(const Math::Vec3& left, const Math::Vec3& right)
    {
        return NearlyEqual(left.x, right.x) && NearlyEqual(left.y, right.y) &&
            NearlyEqual(left.z, right.z);
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
    using namespace Pyramid;

    Tests::TestGraphicsDevice device;
    device.shaderFactory = []() { return std::make_shared<TestShader>(); };
    ResourceRegistry registry(device);

    const std::array<Vertex, 3> vertices = {
        Vertex{-2.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
        Vertex{3.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
        Vertex{0.0f, 4.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f}};
    const std::array<u32, 3> indices = {0, 1, 2};

    const MeshHandle meshHandle = registry.AcquireMesh(MakeMeshSpecification(
        vertices, indices, MeshAssetId::FromString("scene/unit-mesh")));

    ShaderProgramSpecification shaderSpecification;
    shaderSpecification.vertexSource = "void main(){}";
    shaderSpecification.fragmentSource = "void main(){}";
    shaderSpecification.assetId = ShaderAssetId::FromString("scene/unit-shader");
    const ShaderHandle shaderHandle = registry.AcquireShader(shaderSpecification);

    MaterialSpecification materialSpecification;
    materialSpecification.shader = registry.Resolve(shaderHandle);
    materialSpecification.assetId = MaterialAssetId::FromString("scene/unit-material");
    const MaterialHandle materialHandle = registry.AcquireMaterial(materialSpecification);

    if (!meshHandle || !shaderHandle || !materialHandle)
    {
        return Fail("resource setup failed");
    }

    ResourceManifest manifest;
    if (!manifest.Add("z.mesh", meshHandle) || !manifest.Add("a.mesh", meshHandle) ||
        !manifest.Add("unit.material", materialHandle))
    {
        return Fail("resource manifest setup failed");
    }

    Scene scene("RTS\tBattlefield\nEncoded");
    Entity army = scene.CreateEntityWithId(EntityId(0x100), "Army Root");
    Entity unit = scene.CreateEntityWithId(EntityId(0x200), "Infantry\nUnit");
    Entity sun = scene.CreateEntityWithId(EntityId(0x300), "Sun");
    unit.SetParent(army);
    army.SetLocalPosition(Math::Vec3(100.0f, 0.0f, 50.0f));
    unit.SetLocalTransform(
        Math::Vec3(2.0f, 0.0f, 4.0f),
        Math::Quat::FromAxisAngle(Math::Vec3::Up, Math::Radians(35.0f)),
        Math::Vec3(1.5f, 2.0f, 1.5f));
    unit.SetVisible(false);

    MeshRendererComponent renderer;
    renderer.mesh = meshHandle;
    renderer.material = materialHandle;
    renderer.castShadows = false;
    renderer.receiveShadows = true;
    renderer.boundsMode = RenderBoundsMode::Manual;
    renderer.localBoundsMin = Math::Vec3(-3.0f, -2.0f, -1.0f);
    renderer.localBoundsMax = Math::Vec3(4.0f, 5.0f, 6.0f);
    if (!unit.SetMeshRenderer(renderer, &registry))
    {
        return Fail("mesh renderer setup failed");
    }

    sun.SetLocalRotation(
        Math::Quat::FromAxisAngle(Math::Vec3::Right, Math::Radians(-45.0f)));
    LightComponent light;
    light.type = LightType::Directional;
    light.localDirection = Math::Vec3(0.0f, -1.0f, 0.0f);
    light.color = Math::Vec3(1.0f, 0.9f, 0.7f);
    light.intensity = 4.0f;
    light.shadowMapSize = 2048;
    sun.SetLight(light);
    scene.SetPrimaryLight(sun);

    const SceneSerializationResult serialized =
        SceneSerializer::Serialize(scene, manifest, registry);
    const SceneSerializationResult serializedAgain =
        SceneSerializer::Serialize(scene, manifest, registry);
    if (!serialized.Succeeded() || serialized.serializedEntities != 3 ||
        serialized.serializedObjects != 1 || serialized.serializedLights != 1 ||
        serialized.text != serializedAgain.text ||
        serialized.text.find("PYRAMID_SCENE\t2\n") != 0 ||
        serialized.text.find("\ta.mesh\tunit.material\t") == std::string::npos ||
        serialized.text.find("\tz.mesh\t") != std::string::npos ||
        serialized.text.find("entity\t0000000000000200\t0000000000000100\t") == std::string::npos)
    {
        return Fail("deterministic entity serialization failed");
    }

    const SceneDeserializationResult restored =
        SceneSerializer::Deserialize(serialized.text, manifest, registry);
    if (!restored.Succeeded() || restored.restoredEntities != 3 ||
        restored.restoredObjects != 1 || restored.restoredLights != 1 ||
        restored.scene->GetName() != scene.GetName() ||
        restored.scene->GetEntityCount() != 3)
    {
        return Fail("entity scene round trip failed");
    }

    Entity restoredArmy = restored.scene->FindEntity(EntityId(0x100));
    Entity restoredUnit = restored.scene->FindEntity(EntityId(0x200));
    Entity restoredSun = restored.scene->FindEntity(EntityId(0x300));
    if (!restoredArmy || !restoredUnit || !restoredSun ||
        restoredUnit.GetParent() != restoredArmy || restoredUnit.IsVisible() ||
        restored.scene->GetPrimaryLightEntity() != restoredSun ||
        !NearlyEqual(restoredUnit.GetTransform()->position, Math::Vec3(2.0f, 0.0f, 4.0f)) ||
        !NearlyEqual(restoredUnit.GetWorldPosition(), Math::Vec3(102.0f, 0.0f, 54.0f)))
    {
        return Fail("stable IDs, hierarchy, visibility, or transforms were not restored");
    }

    const MeshRendererComponent* restoredRenderer = restoredUnit.GetMeshRenderer();
    const LightComponent* restoredLight = restoredSun.GetLight();
    if (!restoredRenderer || restoredRenderer->mesh != meshHandle ||
        restoredRenderer->material != materialHandle || restoredRenderer->castShadows ||
        restoredRenderer->boundsMode != RenderBoundsMode::Manual ||
        !NearlyEqual(restoredRenderer->localBoundsMin, Math::Vec3(-3.0f, -2.0f, -1.0f)) ||
        !restoredLight || restoredLight->type != LightType::Directional ||
        !NearlyEqual(restoredLight->color, light.color) ||
        !NearlyEqual(restoredLight->intensity, light.intensity) ||
        restoredLight->shadowMapSize != 2048)
    {
        return Fail("mesh renderer or light component was not restored");
    }

    std::string unsupported = serialized.text;
    unsupported.replace(unsupported.find("\t2\n"), 3, "\t1\n");
    const auto unsupportedResult =
        SceneSerializer::Deserialize(unsupported, manifest, registry);
    if (unsupportedResult.Succeeded() || unsupportedResult.diagnostics.empty() ||
        unsupportedResult.diagnostics.front().code !=
            SceneSerializationDiagnosticCode::UnsupportedVersion)
    {
        return Fail("legacy flat scene version should be rejected");
    }

    const std::string invalidParent =
        "PYRAMID_SCENE\t2\n"
        "scene\t54657374\n"
        "entity\t0000000000000001\t0000000000000099\t41\t1\t0\t0\t0\t0\t0\t0\t1\t1\t1\t1\n"
        "primary_light\tnone\n";
    const auto invalidParentResult =
        SceneSerializer::Deserialize(invalidParent, manifest, registry);
    if (invalidParentResult.Succeeded())
    {
        return Fail("missing parent entities should be rejected");
    }

    const std::string cycle =
        "PYRAMID_SCENE\t2\n"
        "scene\t54657374\n"
        "entity\t0000000000000001\t0000000000000002\t41\t1\t0\t0\t0\t0\t0\t0\t1\t1\t1\t1\n"
        "entity\t0000000000000002\t0000000000000001\t42\t1\t0\t0\t0\t0\t0\t0\t1\t1\t1\t1\n"
        "primary_light\tnone\n";
    const auto cycleResult = SceneSerializer::Deserialize(cycle, manifest, registry);
    if (cycleResult.Succeeded())
    {
        return Fail("hierarchy cycles should be rejected");
    }

    return EXIT_SUCCESS;
}
