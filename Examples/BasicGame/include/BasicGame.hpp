#pragma once

#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>
#include <Pyramid/Examples/RTSReference/RTSMatch.hpp>
#include <Pyramid/Examples/RTSReference/RTSSimulation.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/CameraController.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Graphics/UI/UIRenderer.hpp>
#include <Pyramid/UI/UI.hpp>
#include <Pyramid/UI/GameUI.hpp>
#include <Pyramid/Text/Text.hpp>

#include <memory>
#include <string>

namespace Pyramid
{
    class Mesh;
    class ShaderProgram;
    class TextureResource;
    class Material;
}

class BasicGame final : public Pyramid::Game
{
public:
    BasicGame();
    ~BasicGame() override;

protected:
    void onCreate() override;
    void onUpdate(float deltaTime) override;
    void onRender() override;
    void onWindowResize(const Pyramid::WindowResizeEvent& event) override;

private:
    std::shared_ptr<Pyramid::Mesh> CreateColoredCube(float size);
    bool SetupScene();
    bool SetupInputActions();
    void BuildDebugUI(float deltaTime);
    void BuildGameUI(float deltaTime);
    void ShowMainMenu();
    void StartGameplay();
    void StartOfflineGameplay();
    void OpenLANMenu();
    void ClearMatchInteraction();
    std::string GetMatchStatus() const;
    void OpenPauseMenu();
    void OpenSettings();
    void CloseSettings();
    void ResumeGameplay();
    void ReturnToMainMenu();

    std::unique_ptr<Pyramid::Renderer::RenderSystem> m_renderSystem;
    std::unique_ptr<Pyramid::UIRenderer> m_uiRenderer;
    Pyramid::UI::Context m_gameUI;
    Pyramid::UI::Context m_debugUI;
    Pyramid::Text::FontAtlas m_runtimeFont;
    Pyramid::Text::FontAtlas m_runtimeArabicFont;
    Pyramid::Text::FontAtlas m_runtimeSymbolFont;
    Pyramid::Text::FontFamily m_runtimeFontFamily;
    Pyramid::UI::ScreenStack m_gameScreens;
    std::shared_ptr<Pyramid::Scene> m_scene;
    std::unique_ptr<Pyramid::SceneManagement::SceneManager> m_sceneManager;
    Pyramid::Examples::RTSReference::RTSUnitStore m_units;
    Pyramid::Examples::RTSReference::RTSSimulation m_simulation{m_units};
    std::unique_ptr<Pyramid::Examples::RTSReference::RTSMatch> m_match;
    Pyramid::MeshRendererComponent m_unitRenderer;
    std::unique_ptr<Pyramid::Camera> m_camera;
    std::unique_ptr<Pyramid::RTSCameraController> m_cameraController;
    std::unique_ptr<Pyramid::Examples::RTSReference::RTSInteractionController>
        m_interactionController;
    Pyramid::Entity m_cubeEntity;
    Pyramid::EntityId m_reportedSelection;
    std::shared_ptr<Pyramid::ShaderProgram> m_shader;
    std::shared_ptr<Pyramid::TextureResource> m_debugTexture;
    std::shared_ptr<Pyramid::Material> m_cubeMaterial;
    std::shared_ptr<Pyramid::Material> m_floorMaterial;

    Pyramid::UI::FrameInfo m_uiFrame{1280.0f, 720.0f, 1.0f, 0.0f};
    float m_elapsedTime = 0.0f;
    float m_smoothedFrameTime = 1.0f / 60.0f;
    bool m_debugUIVisible = false;
    bool m_runtimeFontLoaded = false;
    bool m_runtimeArabicFontLoaded = false;
    bool m_runtimeSymbolFontLoaded = false;
    bool m_gameplayStarted = false;
    Pyramid::u64 m_hostEpoch = 0;
    bool m_animationPaused = false;
    bool m_performanceSectionOpen = true;
    bool m_resourcesSectionOpen = true;
    bool m_inputSectionOpen = true;
    bool m_uiSectionOpen = true;
    std::string m_matchMessage;
    std::string m_playerName = "Omar";
    std::string m_kingdomName = "Pyramid Kingdom";
    std::string m_profileNotes = "Unicode text input is ready.";
};
