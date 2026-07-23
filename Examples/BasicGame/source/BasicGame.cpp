#include "BasicGame.hpp"

#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Util/Log.hpp>

#include <array>
#include <cmath>
#include <vector>

namespace
{
    constexpr const char* kForwardVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_Model;
uniform mat4 u_ViewProjection;
uniform mat4 u_NormalMatrix;

out vec4 v_Color;
out vec3 v_Normal;

void main()
{
    vec4 worldPosition = u_Model * vec4(a_Position, 1.0);
    vec3 localNormal = normalize(a_Position);

    v_Color = a_Color;
    v_Normal = normalize((u_NormalMatrix * vec4(localNormal, 0.0)).xyz);

    gl_Position = u_ViewProjection * worldPosition;
}
)";

    constexpr const char* kForwardFragmentShader = R"(
#version 330 core

in vec4 v_Color;
in vec3 v_Normal;

uniform vec4 u_AlbedoColor;

out vec4 FragColor;

void main()
{
    vec3 lightDirection = normalize(vec3(0.5, 1.0, 0.25));
    float ndotl = max(dot(normalize(v_Normal), lightDirection), 0.12);
    vec3 litColor = v_Color.rgb * u_AlbedoColor.rgb * ndotl;

    FragColor = vec4(litColor, 1.0);
}
)";
}

BasicGame::BasicGame()
    : Game(Pyramid::GraphicsAPI::OpenGL)
{
}

void BasicGame::onCreate()
{
    Game::onCreate();
    if (!IsInitialized())
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: engine initialization failed.");
        quit();
        return;
    }

    auto* device = GetGraphicsDevice();
    if (!device)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: graphics device is null.");
        quit();
        return;
    }

    m_meshCache = std::make_unique<Pyramid::MeshCache>(*device);
    m_shaderCache = std::make_unique<Pyramid::ShaderCache>(*device);
    m_textureCache = std::make_unique<Pyramid::TextureCache>(*device);

    m_renderSystem = std::make_unique<Pyramid::Renderer::RenderSystem>();
    if (!m_renderSystem->Initialize(device))
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: render system initialization failed.");
        quit();
        return;
    }

    m_scene = std::make_shared<Pyramid::Scene>("BasicGame Scene");
    m_camera = std::make_unique<Pyramid::Camera>(
        Pyramid::Math::Radians(60.0f),
        1280.0f / 720.0f,
        0.1f,
        200.0f);

    m_camera->SetPosition(Pyramid::Math::Vec3(0.0f, 2.5f, 6.0f));
    m_camera->LookAt(Pyramid::Math::Vec3::Zero);
    SetActiveCamera(m_camera.get());

    Pyramid::ShaderProgramSpecification shaderSpecification;
    shaderSpecification.vertexSource = kForwardVertexShader;
    shaderSpecification.fragmentSource = kForwardFragmentShader;
    shaderSpecification.name = "BasicGame Forward";
    shaderSpecification.assetId =
        Pyramid::ShaderAssetId::FromString("examples/basic-game/forward");

    m_shader = m_shaderCache->GetOrCreate(shaderSpecification);
    if (!m_shader)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to compile scene shader.");
        quit();
        return;
    }

    const std::array<Pyramid::u8, 16> checkerPixels = {
        255, 255, 255, 255, 40, 40, 40, 255,
        40, 40, 40, 255, 255, 255, 255, 255};
    Pyramid::TextureResourceSpecification textureSpecification;
    textureSpecification.texture.Width = 2;
    textureSpecification.texture.Height = 2;
    textureSpecification.texture.Format = Pyramid::TextureFormat::RGBA8;
    textureSpecification.texture.GenerateMips = false;
    textureSpecification.texture.MinFilter = Pyramid::TextureFilter::Nearest;
    textureSpecification.texture.MagFilter = Pyramid::TextureFilter::Nearest;
    textureSpecification.pixelData = checkerPixels.data();
    textureSpecification.pixelDataSize = checkerPixels.size();
    textureSpecification.colorSpace = Pyramid::TextureColorSpace::SRGB;
    textureSpecification.assetId =
        Pyramid::TextureAssetId::FromString("examples/basic-game/checker");
    textureSpecification.name = "BasicGame Checker";
    m_debugTexture = m_textureCache->GetOrCreate(textureSpecification);
    if (!m_debugTexture)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to create cached checker texture.");
        quit();
        return;
    }

    if (!SetupScene())
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to create scene objects.");
        quit();
        return;
    }

    auto sun = Pyramid::SceneUtils::CreateDirectionalLight(Pyramid::Math::Vec3(-0.5f, -1.0f, -0.25f));
    sun->intensity = 1.5f;
    m_scene->AddLight(sun);
    m_scene->SetPrimaryLight(sun);

    auto& environment = m_scene->GetEnvironment();
    environment.skyColor = Pyramid::Math::Vec3(0.09f, 0.12f, 0.18f);
    environment.ambientColor = Pyramid::Math::Vec3(0.12f, 0.12f, 0.14f);

    PYRAMID_LOG_INFO("BasicGame ready: engine API pipeline active (Scene + Camera + RenderSystem).");
}

void BasicGame::onUpdate(float deltaTime)
{
    m_elapsedTime += deltaTime;

    if (m_cube)
    {
        m_cube->rotation = Pyramid::Math::Quat::FromEuler(
            0.35f * m_elapsedTime,
            0.8f * m_elapsedTime,
            0.0f);
    }

    UpdateCamera(deltaTime);
}

void BasicGame::onRender()
{
    if (!m_renderSystem || !m_scene || !m_camera)
    {
        Game::onRender();
        return;
    }

    m_renderSystem->BeginFrame();
    m_renderSystem->Render(*m_scene, *m_camera);
    m_renderSystem->EndFrame();
}

std::shared_ptr<Pyramid::Mesh> BasicGame::CreateColoredCube(float size)
{
    auto* device = GetGraphicsDevice();
    if (!device)
    {
        return nullptr;
    }

    const float s = size * 0.5f;
    const std::vector<Pyramid::Vertex> vertices = {
        {-s, -s, -s, 1.0f, 0.2f, 0.2f, 1.0f},
        {s, -s, -s, 0.2f, 1.0f, 0.2f, 1.0f},
        {s, s, -s, 0.2f, 0.2f, 1.0f, 1.0f},
        {-s, s, -s, 1.0f, 1.0f, 0.2f, 1.0f},
        {-s, -s, s, 1.0f, 0.2f, 1.0f, 1.0f},
        {s, -s, s, 0.2f, 1.0f, 1.0f, 1.0f},
        {s, s, s, 1.0f, 1.0f, 1.0f, 1.0f},
        {-s, s, s, 1.0f, 0.6f, 0.2f, 1.0f},
    };

    const std::vector<Pyramid::u32> indices = {
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        0, 7, 3, 0, 4, 7,
        1, 2, 6, 1, 6, 5,
        3, 7, 6, 3, 6, 2,
        0, 1, 5, 0, 5, 4,
    };

    Pyramid::MeshSpecification specification;
    specification.vertexData = vertices.data();
    specification.vertexDataSize =
        static_cast<Pyramid::u32>(vertices.size() * sizeof(Pyramid::Vertex));
    specification.vertexCount = static_cast<Pyramid::u32>(vertices.size());
    specification.layout = {
        {Pyramid::ShaderDataType::Float3, "a_Position"},
        {Pyramid::ShaderDataType::Float4, "a_Color"},
    };
    specification.indexData = indices.data();
    specification.indexCount = static_cast<Pyramid::u32>(indices.size());
    specification.topology = Pyramid::PrimitiveTopology::Triangles;
    specification.name = "ColoredCube";

    return m_meshCache
        ? m_meshCache->GetOrCreate(specification)
        : Pyramid::Mesh::Create(*device, specification);
}

bool BasicGame::SetupScene()
{
    if (!m_scene || !m_shader)
    {
        return false;
    }

    auto cubeGeometry = CreateColoredCube(1.5f);
    auto floorGeometry = CreateColoredCube(1.5f);
    if (!cubeGeometry || !floorGeometry || cubeGeometry != floorGeometry)
    {
        return false;
    }

    m_cube = std::make_shared<Pyramid::RenderObject>();
    m_cube->name = "DemoCube";
    m_cube->mesh = cubeGeometry;
    m_cube->position = Pyramid::Math::Vec3::Zero;
    m_cube->scale = Pyramid::Math::Vec3::One;
    m_cube->material.shader = m_shader;
    m_cube->material.albedo = Pyramid::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);

    auto floorObject = std::make_shared<Pyramid::RenderObject>();
    floorObject->name = "Floor";
    floorObject->mesh = floorGeometry;
    floorObject->position = Pyramid::Math::Vec3(0.0f, -1.2f, 0.0f);
    floorObject->scale = Pyramid::Math::Vec3(6.0f, 0.15f, 6.0f);
    floorObject->material.shader = m_shader;
    floorObject->material.albedo = Pyramid::Math::Vec4(0.35f, 0.42f, 0.55f, 1.0f);

    m_scene->AddRenderObject(m_cube);
    m_scene->AddRenderObject(floorObject);

    return true;
}

void BasicGame::UpdateCamera(float deltaTime)
{
    (void)deltaTime;

    if (!m_camera)
    {
        return;
    }

    const float orbitRadius = 6.0f;
    const float orbitSpeed = 0.35f;
    const float angle = m_elapsedTime * orbitSpeed;

    const Pyramid::Math::Vec3 position(
        std::cos(angle) * orbitRadius,
        2.1f + std::sin(angle * 0.6f) * 0.4f,
        std::sin(angle) * orbitRadius);

    m_camera->SetPosition(position);
    m_camera->LookAt(Pyramid::Math::Vec3::Zero);
}
