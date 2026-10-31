#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Renderer/ShaderPathResolver.hpp>
#include <Pyramid/Util/Log.hpp>
#include <glad/glad.h>
#include <cmath>
#include <algorithm>

namespace Pyramid
{
    namespace Renderer
    {

        ShadowMapPass::ShadowMapPass(const std::string& name, IGraphicsDevice* device, u32 cascadeCount)
            : RenderPass(RenderPassType::Shadow, name)
            , m_device(device)
            , m_cascadeCount(cascadeCount >= 1 && cascadeCount <= MaxShadowCascades ? cascadeCount : 4)
            , m_shadowMapResolution(2048)
            , m_depthBias(0.005f)
        {
            PYRAMID_LOG_INFO("ShadowMapPass created with ", cascadeCount, " cascades");

            if (cascadeCount < 1 || cascadeCount > MaxShadowCascades)
            {
                PYRAMID_LOG_WARN("Invalid initial cascade count; using four cascades");
            }
            // Initialize cascade splits vector
            m_cascadeSplits.resize(m_cascadeCount + 1);
            m_lightSpaceMatrices.resize(m_cascadeCount);

            // Create shadow shader
            m_shadowShader = m_device ? m_device->CreateShader() : nullptr;

            const std::string vertSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/shadow.vert");
            const std::string fragSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/shadow.frag");

            if (!m_shadowShader || vertSrc.empty() || fragSrc.empty())
            {
                PYRAMID_LOG_ERROR("Failed to open shadow shader files");
            }
            else
            {
                if (!m_shadowShader->Compile(vertSrc, fragSrc))
                {
                    PYRAMID_LOG_ERROR("Failed to compile shadow shaders");
                }
                else
                {
                    m_shaderReady = true;
                }
            }

            // Create shadow map framebuffers
            CreateShadowMaps();
        }

        bool ShadowMapPass::CreateShadowMaps()
        {
            m_hasValidShadowData = false;
            if (!m_device) return false;
            std::vector<std::shared_ptr<OpenGLFramebuffer>> replacements;

            PYRAMID_LOG_INFO("Creating ", m_cascadeCount, " shadow maps at ",
                           m_shadowMapResolution, "x", m_shadowMapResolution);

            for (u32 i = 0; i < m_cascadeCount; i++)
            {
                // Create depth-only framebuffer specification
                FramebufferSpec spec;
                spec.width = m_shadowMapResolution;
                spec.height = m_shadowMapResolution;
                spec.samples = 1; // No MSAA for shadow maps
                spec.swapChainTarget = false;

                // Depth attachment only
                FramebufferAttachmentSpec depthSpec;
                depthSpec.type = FramebufferAttachmentType::Depth;
                depthSpec.internalFormat = GL_DEPTH_COMPONENT24;
                depthSpec.format = GL_DEPTH_COMPONENT;
                depthSpec.dataType = GL_FLOAT;
                depthSpec.minFilter = GL_NEAREST;
                depthSpec.magFilter = GL_NEAREST;
                depthSpec.wrapS = GL_CLAMP_TO_BORDER;
                depthSpec.wrapT = GL_CLAMP_TO_BORDER;
                depthSpec.multisampled = false;
                depthSpec.samples = 1;

                spec.attachments.push_back(depthSpec);

                // Create framebuffer
                auto shadowMap = std::make_shared<OpenGLFramebuffer>(spec);
                if (!shadowMap->Initialize())
                {
                    PYRAMID_LOG_ERROR("Failed to initialize shadow map framebuffer ", i);
                    return false;
                }

                // Set border color to 1.0 (outside shadow = not in shadow)
                GLuint depthTexture = shadowMap->GetDepthAttachmentTexture();
                m_device->SetTextureBorderColor(depthTexture, GL_TEXTURE_2D, 1.0f, 1.0f, 1.0f, 1.0f);

                replacements.push_back(shadowMap);

                PYRAMID_LOG_DEBUG("Shadow map cascade ", i, " created successfully");
            }
            m_shadowMaps.swap(replacements);
            return true;
        }

        void ShadowMapPass::CalculateCascadeSplits(const Camera& camera)
        {
            f32 nearPlane = camera.GetNearPlane();
            f32 farPlane = camera.GetFarPlane();
            f32 clipRange = farPlane - nearPlane;

            // Practical split scheme (blend between logarithmic and uniform)
            f32 lambda = 0.75f; // Blend factor

            m_cascadeSplits[0] = nearPlane;

            for (u32 i = 1; i < m_cascadeCount; i++)
            {
                f32 p = static_cast<f32>(i) / static_cast<f32>(m_cascadeCount);

                // Logarithmic split
                f32 logSplit = nearPlane > 0.0f ? nearPlane * std::pow(farPlane / nearPlane, p) : nearPlane + clipRange * p;

                // Uniform split
                f32 uniformSplit = nearPlane + clipRange * p;

                // Practical split (blend)
                f32 split = lambda * logSplit + (1.0f - lambda) * uniformSplit;
                m_cascadeSplits[i] = split;
            }

            m_cascadeSplits[m_cascadeCount] = farPlane;

            PYRAMID_LOG_DEBUG("Cascade splits: Near=", nearPlane, " Far=", farPlane);
            for (u32 i = 0; i < m_cascadeSplits.size(); i++)
            {
                PYRAMID_LOG_DEBUG("  Split[", i, "] = ", m_cascadeSplits[i]);
            }
        }

        Math::Mat4 ShadowMapPass::CalculateLightSpaceMatrix(const Camera& camera,
                                                            f32 nearPlane,
                                                            f32 farPlane,
                                                            const Math::Vec3& lightDir)
        {
            // Get frustum corners in world space for this cascade
            Math::Mat4 proj = camera.GetProjectionMatrix();
            Math::Mat4 view = camera.GetViewMatrix();
            Math::Mat4 invViewProj = (proj * view).Inverse();

            // Calculate frustum corners in NDC space
            std::vector<Math::Vec4> frustumCorners;
            for (u32 z = 0; z < 2; z++)
            {
                for (u32 y = 0; y < 2; y++)
                {
                    for (u32 x = 0; x < 2; x++)
                    {
                        Math::Vec4 corner(
                            2.0f * x - 1.0f,
                            2.0f * y - 1.0f,
                            2.0f * z - 1.0f,
                            1.0f
                        );

                        Math::Vec4 worldCorner = invViewProj * corner;
                        worldCorner = worldCorner / worldCorner.w;
                        frustumCorners.push_back(worldCorner);
                    }
                }
            }

            // Adjust frustum corners for cascade near/far planes
            f32 cameraNear = camera.GetNearPlane();
            f32 cameraFar = camera.GetFarPlane();

            for (u32 i = 0; i < 4; i++)
            {
                Math::Vec4 cornerRay = frustumCorners[i + 4] - frustumCorners[i];
                Math::Vec4 nearCornerRay = frustumCorners[i] + cornerRay * ((nearPlane - cameraNear) / (cameraFar - cameraNear));
                Math::Vec4 farCornerRay = frustumCorners[i] + cornerRay * ((farPlane - cameraNear) / (cameraFar - cameraNear));
                frustumCorners[i] = nearCornerRay;
                frustumCorners[i + 4] = farCornerRay;
            }

            // Calculate frustum center
            Math::Vec3 center(0.0f, 0.0f, 0.0f);
            for (const auto& corner : frustumCorners)
            {
                center = center + Math::Vec3(corner.x, corner.y, corner.z);
            }
            center = center / static_cast<f32>(frustumCorners.size());

            // Create light view matrix
            Math::Vec3 lightDirNorm = lightDir.Normalized();
            Math::Vec3 lightPos = center - lightDirNorm * 50.0f; // Offset from center
            Math::Vec3 up = std::abs(lightDirNorm.y) > 0.99f ? Math::Vec3(1.0f, 0.0f, 0.0f) : Math::Vec3(0.0f, 1.0f, 0.0f);

            Math::Mat4 lightView = Math::Mat4::CreateLookAt(lightPos, center, up);

            // Calculate orthographic projection bounds
            f32 minX = std::numeric_limits<f32>::max();
            f32 maxX = std::numeric_limits<f32>::lowest();
            f32 minY = std::numeric_limits<f32>::max();
            f32 maxY = std::numeric_limits<f32>::lowest();
            f32 minZ = std::numeric_limits<f32>::max();
            f32 maxZ = std::numeric_limits<f32>::lowest();

            for (const auto& corner : frustumCorners)
            {
                Math::Vec4 lightSpaceCorner = lightView * corner;
                minX = std::min(minX, lightSpaceCorner.x);
                maxX = std::max(maxX, lightSpaceCorner.x);
                minY = std::min(minY, lightSpaceCorner.y);
                maxY = std::max(maxY, lightSpaceCorner.y);
                minZ = std::min(minZ, lightSpaceCorner.z);
                maxZ = std::max(maxZ, lightSpaceCorner.z);
            }

            // Extend Z range to include shadow casters behind the camera
            f32 zExtension = (maxZ - minZ) * 2.0f;
            minZ = minZ - zExtension;

            // Create orthographic projection
            Math::Mat4 lightProjection = Math::Mat4::CreateOrthographic(minX, maxX, minY, maxY, -maxZ, -minZ);

            return lightProjection * lightView;
        }

        void ShadowMapPass::Begin(CommandBuffer& cmd)
        {
            (void)cmd;
            m_hasValidShadowData = false;
            // Enable depth testing for shadow rendering
            if (m_device)
            {
                m_device->EnableDepthTest(true);
                m_device->SetDepthFunc(GL_LESS);
            }

            // Enable depth clamping to prevent shadow acne at far distances
            if (m_device)
            {
                m_device->EnableDepthClamp(true);
            }

            // Enable front face culling to reduce peter-panning
            if (m_device)
            {
                m_device->EnableCullFace(true);
                m_device->SetCullFace(GL_FRONT);
            }

            PYRAMID_LOG_DEBUG("ShadowMapPass::Begin");
        }

        void ShadowMapPass::Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera)
        {
            (void)cmd;
            // Calculate cascade splits based on camera frustum
            m_hasValidShadowData = false;
            if (!m_device || !m_shaderReady || m_shadowMaps.size() != m_cascadeCount ||
                !std::isfinite(camera.GetNearPlane()) || !std::isfinite(camera.GetFarPlane()) ||
                camera.GetFarPlane() <= camera.GetNearPlane()) return;
            for (const auto& target : m_shadowMaps)
            {
                if (!target || !target->IsInitialized() || target->IsMultisampled() ||
                    target->GetDepthAttachmentTexture() == 0) return;
            }
            if (!m_customCascadeSplits) CalculateCascadeSplits(camera);

            // Get primary directional light
            auto primaryLight = scene.GetPrimaryLight();
            if (!primaryLight || !primaryLight->enabled || !primaryLight->castShadows)
            {
                PYRAMID_LOG_WARN("No primary light with shadows enabled");
                return;
            }

            const auto direction = primaryLight->direction;
            if (!std::isfinite(direction.x) || !std::isfinite(direction.y) ||
                !std::isfinite(direction.z) || direction.LengthSquared() <= 1e-8f) return;
            Math::Vec3 lightDir = direction.Normalized();

            // Get all shadow-casting objects
            auto allObjects = scene.GetRenderObjects();
            std::vector<std::shared_ptr<RenderObject>> shadowCasters;
            for (const auto& obj : allObjects)
            {
                const auto mesh = obj ? obj->ResolveMesh(m_resources) : nullptr;
                if (obj && obj->visible && obj->castShadows && mesh && mesh->IsValid())
                {
                    shadowCasters.push_back(obj);
                }
            }

            PYRAMID_LOG_DEBUG("Rendering shadows for ", shadowCasters.size(), " objects across ",
                            m_cascadeCount, " cascades");

            // Render shadow map for each cascade
            for (u32 i = 0; i < m_cascadeCount; i++)
            {
                if (i >= m_shadowMaps.size())
                {
                    PYRAMID_LOG_ERROR("Shadow map index out of range: ", i);
                    continue;
                }

                // Calculate light space matrix for this cascade
                f32 nearPlane = m_cascadeSplits[i];
                f32 farPlane = m_cascadeSplits[i + 1];
                m_lightSpaceMatrices[i] = CalculateLightSpaceMatrix(camera, nearPlane, farPlane, lightDir);
                for (const f32 value : m_lightSpaceMatrices[i].m)
                {
                    if (!std::isfinite(value)) return;
                }

                // Bind shadow map framebuffer
                m_device->BindFramebuffer(m_shadowMaps[i].get());

                // Clear depth buffer
                m_device->ClearBuffers(GL_DEPTH_BUFFER_BIT);

                // Set viewport to shadow map resolution
                if (m_device)
                {
                    m_device->SetViewport(0, 0, m_shadowMapResolution, m_shadowMapResolution);
                }

                // Bind shadow shader
                if (m_shadowShader)
                {
                    m_device->BindShader(m_shadowShader.get());
                    m_shadowShader->SetUniformMat4("u_LightSpaceMatrix", m_lightSpaceMatrices[i].m);
                }

                // Render shadow casters
                for (const auto& object : shadowCasters)
                {
                    // Calculate model matrix
                    Math::Mat4 model = object->GetTransformMatrix();

                    // Set model matrix uniform
                    if (m_shadowShader)
                    {
                        m_shadowShader->SetUniformMat4("u_Model", model.m);
                    }

                    const auto mesh = object->ResolveMesh(m_resources);
                    if (!mesh || !mesh->IsValid())
                    {
                        continue;
                    }

                    mesh->Bind();
                    if (mesh->IsIndexed())
                    {
                        m_device->DrawIndexed(
                            mesh->GetIndexCount(),
                            mesh->GetTopology());
                    }
                    else
                    {
                        m_device->DrawArrays(
                            mesh->GetVertexCount(),
                            0,
                            mesh->GetTopology());
                    }
                }

                // Unbind shadow map framebuffer
                m_device->BindFramebuffer(nullptr);

                PYRAMID_LOG_DEBUG("Cascade ", i, " rendered (", nearPlane, " - ", farPlane, ")");
            }
            m_hasValidShadowData = true;
        }

        void ShadowMapPass::End(CommandBuffer& cmd)
        {
            (void)cmd;
            // Reset culling to back face
            if (m_device)
            {
                m_device->SetCullFace(GL_BACK);
            }

            // Disable depth clamping
            if (m_device)
            {
                m_device->EnableDepthClamp(false);
            }

            PYRAMID_LOG_DEBUG("ShadowMapPass::End");
        }

        void ShadowMapPass::SetCascadeCount(u32 count)
        {
            if (count < 1 || count > MaxShadowCascades)
            {
                PYRAMID_LOG_WARN("Invalid cascade count: ", count, " (must be 1-8)");
                return;
            }

            const u32 previousCount = m_cascadeCount;
            m_cascadeCount = count;
            if (!CreateShadowMaps())
            {
                m_cascadeCount = previousCount;
                return;
            }
            m_cascadeSplits.resize(count + 1);
            m_lightSpaceMatrices.resize(count);
            m_customCascadeSplits = false;

            PYRAMID_LOG_INFO("Cascade count set to ", count);
        }

        void ShadowMapPass::SetShadowMapResolution(u32 resolution)
        {
            if (resolution < 256 || resolution > 8192)
            {
                PYRAMID_LOG_WARN("Invalid shadow map resolution: ", resolution, " (must be 256-8192)");
                return;
            }

            const u32 previousResolution = m_shadowMapResolution;
            m_shadowMapResolution = resolution;
            if (!CreateShadowMaps())
            {
                m_shadowMapResolution = previousResolution;
                return;
            }

            PYRAMID_LOG_INFO("Shadow map resolution set to ", resolution);
        }

        void ShadowMapPass::SetCascadeSplits(const std::vector<f32>& splits)
        {
            if (splits.size() != m_cascadeCount + 1)
            {
                PYRAMID_LOG_WARN("Invalid cascade splits size: ", splits.size(),
                               " (expected ", m_cascadeCount + 1, ")");
                return;
            }

            for (size_t index = 0; index < splits.size(); ++index)
            {
                if (!std::isfinite(splits[index]) || (index && splits[index] <= splits[index - 1]))
                {
                    PYRAMID_LOG_WARN("Cascade splits must be finite and strictly increasing");
                    return;
                }
            }
            m_cascadeSplits = splits;
            m_customCascadeSplits = true;
            m_hasValidShadowData = false;
            PYRAMID_LOG_INFO("Custom cascade splits set");
        }

        void ShadowMapPass::SetDepthBias(f32 bias)
        {
            if (!std::isfinite(bias) || bias < 0.0f)
            {
                PYRAMID_LOG_WARN("Shadow bias must be finite and non-negative");
                return;
            }
            m_depthBias = bias;
            m_hasValidShadowData = false;
            PYRAMID_LOG_DEBUG("Depth bias set to ", bias);
        }

    } // namespace Renderer
} // namespace Pyramid
