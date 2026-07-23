#pragma once
#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Graphics/Shader/ShaderCache.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/Texture/TextureCache.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Material/MaterialCache.hpp>
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <memory>

/**
 * @brief Basic Rendering Example for Pyramid Engine
 *
 * This example demonstrates the fundamental rendering capabilities of the Pyramid Engine:
 * - Setting up a basic rendering pipeline
 * - Creating and rendering 3D geometry
 * - Using shaders and uniform buffers
 * - Implementing camera controls
 * - Basic lighting calculations
 *
 * This example is designed to be simple and educational, making it easy for
 * new users to understand the core concepts of the engine.
 */
class BasicRendering : public Pyramid::Game
{
public:
    explicit BasicRendering();
    virtual ~BasicRendering() = default;

protected:
    void onCreate() override;
    void onUpdate(float deltaTime) override;
    void onRender() override;

private:
    // Initialization methods
    void InitializeShaders();
    void CreateGeometry();
    void SetupCamera();
    void SetupUniformBuffers();

    // Update methods
    void UpdateCamera(float deltaTime);
    void UpdateUniformBuffers(float deltaTime);

    // Rendering methods
    void RenderScene();

    // Input handling
    void HandleInput(float deltaTime);

    // Core rendering components
    std::unique_ptr<Pyramid::MeshCache> m_meshCache;
    std::unique_ptr<Pyramid::ShaderCache> m_shaderCache;
    std::unique_ptr<Pyramid::TextureCache> m_textureCache;
    std::unique_ptr<Pyramid::MaterialCache> m_materialCache;
    std::shared_ptr<Pyramid::ShaderProgram> m_shader;
    std::shared_ptr<Pyramid::Mesh> m_mesh;
    std::shared_ptr<Pyramid::TextureResource> m_debugTexture;
    std::shared_ptr<Pyramid::Material> m_material;
    std::shared_ptr<Pyramid::IUniformBuffer> m_sceneUBO;
    std::shared_ptr<Pyramid::IUniformBuffer> m_materialUBO;

    // Camera system
    std::unique_ptr<Pyramid::Camera> m_camera;

    // Timing and animation
    float m_time = 0.0f;

    // Camera controls
    float m_cameraOrbitRadius = 5.0f;
    float m_cameraOrbitSpeed = 0.5f;
    float m_cameraHeight = 2.0f;

    // Uniform data structures
    struct SceneUniforms
    {
        Pyramid::Math::Mat4 viewMatrix;
        Pyramid::Math::Mat4 projectionMatrix;
        Pyramid::Math::Mat4 viewProjectionMatrix;
        Pyramid::Math::Vec4 cameraPosition;
        Pyramid::Math::Vec4 lightDirection;
        Pyramid::Math::Vec4 lightColor;
        float time;
        float padding[3]; // Ensure 16-byte alignment
    };

    struct MaterialUniforms
    {
        Pyramid::Math::Vec4 baseColor;
        Pyramid::Math::Vec4 emissiveColor;
        float metallic;
        float roughness;
        float padding[2]; // Ensure 16-byte alignment
    };
};