#include "BasicRendering.hpp"

#include <array>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Util/Log.hpp>
#include <cmath>

// Vertex shader source
const std::string vertexShaderSrc = R"(
    #version 330 core
    layout (location = 0) in vec3 a_Position;
    layout (location = 1) in vec3 a_Normal;
    layout (location = 2) in vec2 a_TexCoord;
    layout (location = 3) in vec3 a_Color;

    layout(std140) uniform SceneData {
        mat4 u_ViewMatrix;
        mat4 u_ProjectionMatrix;
        mat4 u_ViewProjectionMatrix;
        vec4 u_CameraPosition;
        vec4 u_LightDirection;
        vec4 u_LightColor;
        float u_Time;
    };

    layout(std140) uniform MaterialData {
        vec4 u_BaseColor;
        vec4 u_EmissiveColor;
        float u_Metallic;
        float u_Roughness;
    };

    out vec3 v_Color;
    out vec3 v_Normal;
    out vec3 v_WorldPos;
    out vec3 v_ViewPos;

    void main()
    {
        v_Color = a_Color;
        v_Normal = a_Normal;
        v_WorldPos = a_Position;
        v_ViewPos = u_CameraPosition.xyz;

        gl_Position = u_ViewProjectionMatrix * vec4(a_Position, 1.0);
    }
)";

// Fragment shader source
const std::string fragmentShaderSrc = R"(
    #version 330 core
    out vec4 FragColor;

    in vec3 v_Color;
    in vec3 v_Normal;
    in vec3 v_WorldPos;
    in vec3 v_ViewPos;

    layout(std140) uniform SceneData {
        mat4 u_ViewMatrix;
        mat4 u_ProjectionMatrix;
        mat4 u_ViewProjectionMatrix;
        vec4 u_CameraPosition;
        vec4 u_LightDirection;
        vec4 u_LightColor;
        float u_Time;
    };

    layout(std140) uniform MaterialData {
        vec4 u_BaseColor;
        vec4 u_EmissiveColor;
        float u_Metallic;
        float u_Roughness;
    };

    void main()
    {
        // Normalize vectors
        vec3 normal = normalize(v_Normal);
        vec3 lightDir = normalize(-u_LightDirection.xyz);
        vec3 viewDir = normalize(v_ViewPos - v_WorldPos);

        // Basic lighting calculations
        float NdotL = max(dot(normal, lightDir), 0.0);

        // Diffuse lighting
        vec3 diffuse = u_LightColor.rgb * NdotL;

        // Ambient lighting
        vec3 ambient = u_LightColor.rgb * 0.2;

        // Specular lighting (simplified)
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
        vec3 specular = u_LightColor.rgb * spec * u_Metallic;

        // Combine lighting with vertex colors and material properties
        vec3 baseColor = v_Color * u_BaseColor.rgb;
        vec3 finalColor = baseColor * (ambient + diffuse) + specular + u_EmissiveColor.rgb;

        FragColor = vec4(finalColor, 1.0);
    }
)";

BasicRendering::BasicRendering()
    : Game(Pyramid::GraphicsAPI::OpenGL)
{
}

void BasicRendering::onCreate()
{
    PYRAMID_LOG_INFO("Basic Rendering Example starting up");

    Game::onCreate();

    auto device = GetGraphicsDevice();
    if (!device)
    {
        PYRAMID_LOG_ERROR("Graphics device is null in BasicRendering::onCreate!");
        return;
    }

    m_meshCache = std::make_unique<Pyramid::MeshCache>(*device);
    m_shaderCache = std::make_unique<Pyramid::ShaderCache>(*device);
    m_textureCache = std::make_unique<Pyramid::TextureCache>(*device);

    // Initialize all components
    InitializeShaders();
    CreateGeometry();

    const std::array<Pyramid::u8, 4> whitePixel = {255, 255, 255, 255};
    Pyramid::TextureResourceSpecification textureSpecification;
    textureSpecification.texture.Width = 1;
    textureSpecification.texture.Height = 1;
    textureSpecification.texture.Format = Pyramid::TextureFormat::RGBA8;
    textureSpecification.texture.GenerateMips = false;
    textureSpecification.texture.MinFilter = Pyramid::TextureFilter::Linear;
    textureSpecification.pixelData = whitePixel.data();
    textureSpecification.pixelDataSize = whitePixel.size();
    textureSpecification.colorSpace = Pyramid::TextureColorSpace::SRGB;
    textureSpecification.assetId =
        Pyramid::TextureAssetId::FromString("examples/basic-rendering/white");
    textureSpecification.name = "BasicRendering White";
    m_debugTexture = m_textureCache->GetOrCreate(textureSpecification);

    Pyramid::MaterialSpecification materialSpecification;
    materialSpecification.shader = m_shader;
    materialSpecification.textures = {
        {"u_AlbedoMap", 0, m_debugTexture}};
    materialSpecification.uniforms = {
        {"u_BaseColor", Pyramid::Math::Vec4(1.0f)},
        {"u_EmissiveColor", Pyramid::Math::Vec4(0.0f)},
        {"u_Metallic", 0.2f},
        {"u_Roughness", 0.65f}};
    materialSpecification.assetId =
        Pyramid::MaterialAssetId::FromString("examples/basic-rendering/material");
    materialSpecification.name = "BasicRendering Material";
    m_material = Pyramid::Material::Create(materialSpecification);
    if (!m_material)
    {
        PYRAMID_LOG_ERROR("Failed to create the BasicRendering material");
        return;
    }

    SetupCamera();
    SetupUniformBuffers();

    PYRAMID_LOG_INFO("Basic Rendering Example initialized successfully!");
}

void BasicRendering::InitializeShaders()
{
    auto device = GetGraphicsDevice();
    if (!device)
        return;

    Pyramid::ShaderProgramSpecification specification;
    specification.vertexSource = vertexShaderSrc;
    specification.fragmentSource = fragmentShaderSrc;
    specification.name = "BasicRendering Scene";
    specification.assetId =
        Pyramid::ShaderAssetId::FromString("examples/basic-rendering/scene");

    m_shader = m_shaderCache->GetOrCreate(specification);
    if (!m_shader)
    {
        PYRAMID_LOG_ERROR("Failed to create or compile shader!");
        return;
    }

    PYRAMID_LOG_INFO("Shader compiled successfully");
}

void BasicRendering::CreateGeometry()
{
    auto device = GetGraphicsDevice();
    if (!device)
        return;

    // Define vertex structure
    struct Vertex
    {
        float Position[3];
        float Normal[3];
        float TexCoord[2];
        float Color[3];
    };

    // Create a colorful cube
    float size = 1.0f;
    Vertex vertices[] = {
        // Front face (Z+) - Red
        {{-size, -size, size}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{size, -size, size}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}},
        {{size, size, size}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
        {{-size, size, size}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

        // Back face (Z-) - Green
        {{-size, -size, -size}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{size, -size, -size}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
        {{size, size, -size}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-size, size, -size}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

        // Left face (X-) - Blue
        {{-size, -size, -size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-size, -size, size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-size, size, size}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
        {{-size, size, -size}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},

        // Right face (X+) - Yellow
        {{size, -size, -size}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
        {{size, -size, size}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}},
        {{size, size, size}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},
        {{size, size, -size}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 0.0f}},

        // Top face (Y+) - Magenta
        {{-size, size, -size}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}},
        {{-size, size, size}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
        {{size, size, size}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}},
        {{size, size, -size}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 1.0f}},

        // Bottom face (Y-) - Cyan
        {{-size, -size, -size}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}},
        {{-size, -size, size}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 1.0f}},
        {{size, -size, size}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 1.0f}},
        {{size, -size, -size}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}}};

    // Cube indices
    Pyramid::u32 indices[] = {
        // Front face
        0, 1, 2, 2, 3, 0,
        // Back face
        4, 5, 6, 6, 7, 4,
        // Left face
        8, 9, 10, 10, 11, 8,
        // Right face
        12, 13, 14, 14, 15, 12,
        // Top face
        16, 17, 18, 18, 19, 16,
        // Bottom face
        20, 21, 22, 22, 23, 20};

    Pyramid::MeshSpecification specification;
    specification.vertexData = vertices;
    specification.vertexDataSize = sizeof(vertices);
    specification.vertexCount = 24;
    specification.layout = {
        {Pyramid::ShaderDataType::Float3, "a_Position"},
        {Pyramid::ShaderDataType::Float3, "a_Normal"},
        {Pyramid::ShaderDataType::Float2, "a_TexCoord"},
        {Pyramid::ShaderDataType::Float3, "a_Color"}};
    specification.indexData = indices;
    specification.indexCount = 36;
    specification.topology = Pyramid::PrimitiveTopology::Triangles;
    specification.name = "BasicRenderingCube";
    specification.assetId = Pyramid::MeshAssetId::FromString(
        "examples/basic-rendering/cube");

    m_mesh = m_meshCache
        ? m_meshCache->GetOrCreate(specification)
        : Pyramid::Mesh::Create(*device, specification);
    if (!m_mesh)
    {
        PYRAMID_LOG_ERROR("Failed to create cube mesh");
        return;
    }

    PYRAMID_LOG_INFO("Geometry created successfully");
}

void BasicRendering::SetupCamera()
{
    using namespace Pyramid::Math;

    // Create camera with proper parameters
    m_camera = std::make_unique<Pyramid::Camera>(
        Radians(60.0f), // FOV
        16.0f / 9.0f,   // Aspect ratio
        0.1f,           // Near plane
        100.0f          // Far plane
    );

    // Set initial camera position
    m_camera->SetPosition(Vec3(0.0f, 2.0f, 5.0f));
    m_camera->LookAt(Vec3::Zero, Vec3::Up);
    SetActiveCamera(m_camera.get());

    PYRAMID_LOG_INFO("Camera setup completed");
}

void BasicRendering::SetupUniformBuffers()
{
    auto device = GetGraphicsDevice();
    if (!device)
        return;

    // Create uniform buffers
    m_sceneUBO = device->CreateUniformBuffer(sizeof(SceneUniforms));
    m_materialUBO = device->CreateUniformBuffer(sizeof(MaterialUniforms));

    if (!m_sceneUBO || !m_materialUBO)
    {
        PYRAMID_LOG_ERROR("Failed to create uniform buffers!");
        return;
    }

    // Initialize uniform buffer data
    SceneUniforms sceneData = {};
    sceneData.projectionMatrix = Pyramid::Math::Mat4::CreatePerspective(
        Pyramid::Math::Radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    sceneData.lightDirection = Pyramid::Math::Vec4(0.5f, -1.0f, 0.5f, 0.0f);
    sceneData.lightColor = Pyramid::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    sceneData.time = 0.0f;

    MaterialUniforms materialData = {};
    materialData.baseColor = Pyramid::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    materialData.emissiveColor = Pyramid::Math::Vec4(0.0f, 0.0f, 0.0f, 1.0f);
    materialData.metallic = 0.5f;
    materialData.roughness = 0.5f;

    // Upload initial data
    m_sceneUBO->UpdateData(&sceneData, sizeof(SceneUniforms));
    m_materialUBO->UpdateData(&materialData, sizeof(MaterialUniforms));

    // Bind uniform buffers to shader
    if (m_shader)
    {
        m_shader->Bind();
        m_shader->BindUniformBuffer("SceneData", m_sceneUBO.get(), 0);
        m_shader->BindUniformBuffer("MaterialData", m_materialUBO.get(), 1);
        m_shader->Unbind();
    }

    PYRAMID_LOG_INFO("Uniform buffers setup completed");
}

void BasicRendering::onUpdate(float deltaTime)
{
    Game::onUpdate(deltaTime);
    m_time += deltaTime;

    // Update systems
    UpdateCamera(deltaTime);
    UpdateUniformBuffers(deltaTime);
    HandleInput(deltaTime);
}

void BasicRendering::UpdateCamera(float deltaTime)
{
    (void)deltaTime;
    if (!m_camera)
        return;

    using namespace Pyramid::Math;

    // Orbital camera movement - ensure we're using the right trigonometric functions
    float angle = m_time * m_cameraOrbitSpeed;
    Vec3 position(
        cos(angle) * m_cameraOrbitRadius,
        m_cameraHeight,
        sin(angle) * m_cameraOrbitRadius);

    m_camera->SetPosition(position);
    m_camera->LookAt(Vec3::Zero, Vec3::Up);
}

void BasicRendering::UpdateUniformBuffers(float deltaTime)
{
    (void)deltaTime;
    if (!m_sceneUBO || !m_materialUBO || !m_camera)
        return;

    // Update scene data
    SceneUniforms sceneData = {};
    sceneData.viewMatrix = m_camera->GetViewMatrix();
    sceneData.projectionMatrix = m_camera->GetProjectionMatrix();
    sceneData.viewProjectionMatrix = m_camera->GetViewProjectionMatrix();
    sceneData.cameraPosition = Pyramid::Math::Vec4(m_camera->GetPosition(), 1.0f);
    sceneData.lightDirection = Pyramid::Math::Vec4(
        sin(m_time * 0.5f) * 0.5f,
        -0.8f,
        cos(m_time * 0.5f) * 0.5f,
        0.0f);
    sceneData.lightColor = Pyramid::Math::Vec4(1.0f, 0.95f, 0.8f, 1.0f);
    sceneData.time = m_time;

    // Update material data with animated values
    MaterialUniforms materialData = {};
    materialData.baseColor = Pyramid::Math::Vec4(
        0.8f + 0.2f * sin(m_time * 0.7f),
        0.8f + 0.2f * sin(m_time * 0.9f + 2.0f),
        0.8f + 0.2f * sin(m_time * 1.1f + 4.0f),
        1.0f);
    materialData.emissiveColor = Pyramid::Math::Vec4(
        0.1f * sin(m_time * 2.0f),
        0.1f * sin(m_time * 1.5f + 1.0f),
        0.1f * sin(m_time * 1.8f + 3.0f),
        1.0f);
    materialData.metallic = 0.5f + 0.5f * sin(m_time * 0.4f);
    materialData.roughness = 0.5f + 0.4f * sin(m_time * 0.6f + 1.5f);

    // Upload updated data
    m_sceneUBO->UpdateData(&sceneData, sizeof(SceneUniforms));
    m_materialUBO->UpdateData(&materialData, sizeof(MaterialUniforms));
}

void BasicRendering::HandleInput(float deltaTime)
{
    // Simple input simulation for demonstration
    // In a real implementation, this would get input from the window system

    // Simulate pressing '1' to reset camera
    static float resetTimer = 0.0f;
    resetTimer += deltaTime;
    if (resetTimer > 10.0f)
    {
        resetTimer = 0.0f;
        m_cameraOrbitRadius = 5.0f;
        m_cameraHeight = 2.0f;
        PYRAMID_LOG_INFO("Camera reset to default position");
    }
}

void BasicRendering::onRender()
{
    auto device = GetGraphicsDevice();
    if (!device)
        return;

    // Set clear color and clear buffers
    device->SetClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    device->Clear(Pyramid::Color(0.1f, 0.1f, 0.2f, 1.0f));

    // Enable depth testing
    device->EnableDepthTest(true);
    device->SetDepthFunc(0x0201); // GL_LESS

    // Render the scene
    RenderScene();

    Game::onRender();
}

void BasicRendering::RenderScene()
{
    if (!m_material || !m_mesh || !m_sceneUBO || !m_materialUBO)
        return;

    auto device = GetGraphicsDevice();
    if (!device)
        return;

    // Apply the engine-owned material and bind uniform buffers.
    m_material->Apply(*device);
    m_sceneUBO->Bind(0);
    m_materialUBO->Bind(1);

    // Bind the engine-owned mesh and draw with its immutable metadata.
    m_mesh->Bind();
    if (m_mesh->IsIndexed())
    {
        device->DrawIndexed(m_mesh->GetIndexCount(), m_mesh->GetTopology());
    }
    else
    {
        device->DrawArrays(m_mesh->GetVertexCount(), 0, m_mesh->GetTopology());
    }
    m_mesh->Unbind();

    m_material->GetShader()->Unbind();

    // Log debug info periodically
    static float debugTimer = 0.0f;
    debugTimer += 0.016f; // Approximate frame time
    if (debugTimer >= 5.0f)
    {
        debugTimer = 0.0f;
        if (m_camera)
        {
            auto pos = m_camera->GetPosition();
            PYRAMID_LOG_INFO("Rendering: Camera at (", pos.x, ", ", pos.y, ", ", pos.z, ")");
        }
    }
}