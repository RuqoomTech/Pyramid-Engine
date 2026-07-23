#pragma once

#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/Shader/ShaderCache.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Scene.hpp>

#include <memory>

namespace Pyramid
{
    class Mesh;
    class MeshCache;
    class ShaderCache;
    class ShaderProgram;
}

class BasicGame final : public Pyramid::Game
{
public:
    BasicGame();
    ~BasicGame() override = default;

protected:
    void onCreate() override;
    void onUpdate(float deltaTime) override;
    void onRender() override;

private:
    std::shared_ptr<Pyramid::Mesh> CreateColoredCube(float size);
    bool SetupScene();
    void UpdateCamera(float deltaTime);

    std::unique_ptr<Pyramid::MeshCache> m_meshCache;
    std::unique_ptr<Pyramid::ShaderCache> m_shaderCache;
    std::unique_ptr<Pyramid::Renderer::RenderSystem> m_renderSystem;
    std::shared_ptr<Pyramid::Scene> m_scene;
    std::unique_ptr<Pyramid::Camera> m_camera;
    std::shared_ptr<Pyramid::RenderObject> m_cube;
    std::shared_ptr<Pyramid::ShaderProgram> m_shader;

    float m_elapsedTime = 0.0f;
};
