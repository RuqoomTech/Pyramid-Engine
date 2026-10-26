#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Platform/Input.hpp>
#include <Pyramid/Input/InputActions.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/Shader/ShaderCache.hpp>
#include <Pyramid/Graphics/Texture/TextureCache.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Material/MaterialCache.hpp>
#include <Pyramid/Graphics/Resources/ResourceHandle.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Resources/ResourceManifest.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/SceneSerializer.hpp>
#include <Pyramid/Graphics/CameraController.hpp>

#include <array>
#include <iostream>

int main()
{
    const Pyramid::Math::Vec3 value(1.0f, 2.0f, 3.0f);
    const auto meshId = Pyramid::MeshAssetId::FromString("consumer/mesh");
    const auto shaderId = Pyramid::ShaderAssetId::FromString("consumer/shader");
    const auto textureId = Pyramid::TextureAssetId::FromString("consumer/texture");
    const std::array<Pyramid::f32, 2> redPixels{0.25f, 0.75f};
    Pyramid::TextureResourceSpecification redTexture;
    redTexture.texture.Width = 2;
    redTexture.texture.Height = 1;
    redTexture.texture.Format = Pyramid::TextureFormat::R32F;
    redTexture.texture.GenerateMips = false;
    redTexture.pixelData = redPixels.data();
    redTexture.pixelDataSize = sizeof(redPixels);
    const auto redTextureId = Pyramid::TextureResource::CalculateContentId(redTexture);
    redTexture.pixelDataSize -= 1;
    const auto truncatedTextureId = Pyramid::TextureResource::CalculateContentId(redTexture);
    if (!redTextureId.IsValid() || truncatedTextureId.IsValid())
    {
        std::cerr << "Installed texture API must accept R32F pixels and reject truncated data\n";
        return 1;
    }
    const auto materialId = Pyramid::MaterialAssetId::FromString("consumer/material");
    Pyramid::MaterialCache materialCache;
    Pyramid::InputState input;
    input.SetFocused(true);
    input.BeginFrame();
    input.ProcessKey(Pyramid::Key::W, true);
    Pyramid::InputActionSystem inputActions;
    auto* gameplayContext = inputActions.CreateContext("consumer-gameplay");
    const bool actionConfigured = gameplayContext &&
        gameplayContext->AddAction("MoveForward", Pyramid::InputActionType::Axis2D) &&
        gameplayContext->AddBinding(
            "MoveForward",
            Pyramid::InputBinding::KeyBinding(
                Pyramid::Key::W, 1.0f, Pyramid::InputAxisComponent::Y));
    inputActions.Update(input);
    Pyramid::Camera consumerCamera;
    Pyramid::FreeFlyCameraActions cameraActions;
    cameraActions.move = {"consumer-gameplay", "MoveForward"};
    cameraActions.elevate = {};
    cameraActions.lookDelta = {};
    cameraActions.lookRate = {};
    cameraActions.boost = {};
    cameraActions.reset = {};
    Pyramid::FreeFlyCameraController cameraController(cameraActions);
    cameraController.CaptureHome(consumerCamera);
    cameraController.Update(consumerCamera, inputActions, 0.25f);
    Pyramid::ResourceRegistryReleaseStats released;
    const auto meshHandle = Pyramid::MeshHandle::FromParts(meshId, 1);
    const auto materialHandle = Pyramid::MaterialHandle::FromParts(materialId, 1);
    Pyramid::ResourceManifest manifest;
    manifest.Add("consumer.mesh", meshHandle);
    Pyramid::Scene scene("Consumer Scene");
    const Pyramid::Entity entity = scene.CreateEntityWithId(
        Pyramid::EntityId(42),
        "Consumer Entity");
    Pyramid::SceneSerializationResult sceneSerialization;
    std::cout << "Pyramid Engine " << PYRAMID_VERSION_STRING
              << " | vector length: " << value.Length()
              << " | mesh id: " << meshId.ToString()
              << " | shader id: " << shaderId.ToString()
              << " | texture id: " << textureId.ToString()
              << " | material id: " << materialId.ToString()
              << " | mesh generation: " << meshHandle.GetGeneration()
              << " | material generation: " << materialHandle.GetGeneration()
              << " | cached materials: " << materialCache.GetResidentCount()
              << " | manifest entries: " << manifest.GetEntryCount()
              << " | serialized scene objects: " << sceneSerialization.serializedObjects
              << " | input W: " << input.IsKeyDown(Pyramid::Key::W)
              << " | move action: "
              << inputActions.GetActionValue2D("consumer-gameplay", "MoveForward").y
              << " | camera z: " << consumerCamera.GetPosition().z
              << " | released resources: " << released.GetTotal() << '\n';
    return value.LengthSquared() > 0.0f && meshId.IsValid() && shaderId.IsValid() &&
        textureId.IsValid() && materialId.IsValid() && meshHandle.IsValid() &&
        materialHandle.IsValid() && input.IsKeyDown(Pyramid::Key::W) &&
        input.WasKeyPressed(Pyramid::Key::W) && actionConfigured &&
        inputActions.IsActionDown("consumer-gameplay", "MoveForward") &&
        consumerCamera.GetPosition().z < 0.0f &&
        entity.IsValid() && entity.GetId() == Pyramid::EntityId(42) &&
        manifest.GetMeshHandle("consumer.mesh") == meshHandle
        ? 0
        : 1;
}
