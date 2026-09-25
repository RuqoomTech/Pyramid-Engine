#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Renderer/ShaderPathResolver.hpp>
#include <Pyramid/Util/Log.hpp>
#include "../OpenGL/OpenGLDiagnostics.hpp"
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
            , m_cascadeCount(cascadeCount)
            , m_shadowMapResolution(2048)
            , m_depthBias(0.005f)
        {
            PYRAMID_LOG_INFO("ShadowMapPass created with ", cascadeCount, " cascades");

            // Initialize cascade splits vector
            m_cascadeSplits.resize(cascadeCount + 1);
            m_lightSpaceMatrices.resize(cascadeCount);

            // Create shadow shader
            m_shadowShader = m_device->CreateShader();

            const std::string vertSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/shadow.vert");
            const std::string fragSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/shadow.frag");

            if (vertSrc.empty() || fragSrc.empty())
            {
                PYRAMID_LOG_ERROR("Failed to open shadow shader files");
            }
            else
            {
                if (!m_shadowShader->Compile(vertSrc, fragSrc))
                {
                    PYRAMID_LOG_ERROR("Failed to compile shadow shaders");
                }
            }

            // Create the single shadow array texture plus its framebuffer
            CreateShadowArrayTexture(m_cascadeCount, m_shadowMapResolution);
        }

        ShadowMapPass::~ShadowMapPass()
        {
            DestroyShadowArray();
        }

        void ShadowMapPass::DestroyShadowArray()
        {
            if (m_shadowArrayTarget)
            {
                GLuint arrayFBO = static_cast<GLuint>(m_shadowArrayTarget->GetNativeHandle());
                // Drop the neutral view first so it can never outlive the
                // framebuffer object it observes.
                m_shadowArrayTarget.reset();
                if (arrayFBO != 0)
                {
                    glDeleteFramebuffers(1, &arrayFBO);
                }
            }

            if (m_shadowArrayTexture != 0)
            {
                glDeleteTextures(1, &m_shadowArrayTexture);
                m_shadowArrayTexture = 0;
            }

            m_shadowArrayLayers = 0;
            m_shadowArrayResolution = 0;
        }

        bool ShadowMapPass::CreateShadowArrayTexture(u32 cascadeCount, u32 resolution)
        {
            if (cascadeCount == 0)
            {
                PYRAMID_LOG_ERROR("Cannot create shadow array with zero cascades; lighting continues unshadowed");
                DestroyShadowArray();
                return false;
            }

            if (resolution == 0)
            {
                PYRAMID_LOG_ERROR("Cannot create shadow array with zero resolution");
                DestroyShadowArray();
                return false;
            }

            // Cascade layers must fit the driver array limit; without a valid
            // array the lighting pass skips its bind and continues unshadowed.
            GLint maxArrayLayers = 0;
            glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayLayers);
            if (maxArrayLayers <= 0 || cascadeCount > static_cast<u32>(maxArrayLayers))
            {
                PYRAMID_LOG_ERROR("Shadow cascade count ", cascadeCount,
                                  " exceeds GL_MAX_ARRAY_TEXTURE_LAYERS (", maxArrayLayers,
                                  "); keeping the previous array");
                return false;
            }

            // The lighting shaders declare room for kMaxShaderCascades; larger
            // counts would overflow u_LightSpaceMatrices/u_CascadeSplits, so
            // they fail explicitly here instead of sampling out of bounds.
            if (cascadeCount > kMaxShaderCascades)
            {
                PYRAMID_LOG_ERROR("Shadow cascade count ", cascadeCount,
                                  " exceeds the shader limit of ", kMaxShaderCascades,
                                  " cascades; keeping the previous array");
                return false;
            }

            PYRAMID_LOG_INFO("Creating shadow array with ", cascadeCount, " layers at ",
                             resolution, "x", resolution);

            // One depth texture array with a layer per cascade. Parameters reuse
            // the previous per-cascade values (DEPTH_COMPONENT24, NEAREST
            // filtering, CLAMP_TO_BORDER) so sampling behavior is unchanged.
            // Resolution is fixed here and never follows the window size.
            GLuint arrayTexture = 0;
            glGenTextures(1, &arrayTexture);
            glBindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture);
            glTexImage3D(GL_TEXTURE_2D_ARRAY,
                         0,
                         GL_DEPTH_COMPONENT24,
                         static_cast<GLsizei>(resolution),
                         static_cast<GLsizei>(resolution),
                         static_cast<GLsizei>(cascadeCount),
                         0,
                         GL_DEPTH_COMPONENT,
                         GL_FLOAT,
                         nullptr);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
            glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

            // Set border color to 1.0 (outside shadow = not in shadow)
            m_device->SetTextureBorderColor(arrayTexture, GL_TEXTURE_2D_ARRAY, 1.0f, 1.0f, 1.0f, 1.0f);

            // One framebuffer shared by all layers. Each cascade attaches its
            // own layer before rendering (layered rendering): no per-cascade
            // framebuffers and no per-frame copies. Depth-only, so no color
            // buffers are drawn or read.
            GLuint arrayFBO = 0;
            glGenFramebuffers(1, &arrayFBO);
            // Build the neutral view before binding it: the layered depth
            // attachment attaches to the bound target, so the pass holds this
            // as an IFramebuffer and never as a raw handle. The view is
            // non-owning, so a failed attempt simply discards it.
            auto replacementTarget = std::make_unique<OpenGLLayeredFramebuffer>(
                static_cast<u32>(arrayFBO), resolution, resolution);
            m_device->BindFramebuffer(replacementTarget.get());
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, arrayTexture, 0, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);

            const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            m_device->BindFramebuffer(nullptr);

            std::string glError;
            const bool glClean =
                OpenGLDiagnostics::CheckError("ShadowMapPass::CreateShadowArray", &glError, false);
            if (!glClean || status != GL_FRAMEBUFFER_COMPLETE)
            {
                PYRAMID_LOG_ERROR("Shadow array framebuffer is not complete (status ",
                                  static_cast<u32>(status), "): ", glError,
                                  "; keeping the previous array");
                replacementTarget.reset();
                if (arrayFBO != 0)
                {
                    glDeleteFramebuffers(1, &arrayFBO);
                }
                if (arrayTexture != 0)
                {
                    glDeleteTextures(1, &arrayTexture);
                }
                m_device->BindFramebuffer(nullptr);
                return false;
            }

            // Commit: the replacement is complete, so release the previous
            // array and swap the new one into service.
            DestroyShadowArray();
            m_shadowArrayTexture = arrayTexture;
            m_shadowArrayTarget = std::move(replacementTarget);
            m_shadowArrayLayers = cascadeCount;
            m_shadowArrayResolution = resolution;

            PYRAMID_LOG_DEBUG("Shadow array created with ", m_shadowArrayLayers, " layers");
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
                f32 logSplit = nearPlane * std::pow(farPlane / nearPlane, p);

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
            for (u32 x = 0; x < 2; x++)
            {
                for (u32 y = 0; y < 2; y++)
                {
                    for (u32 z = 0; z < 2; z++)
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
            Math::Mat4 lightProjection = Math::Mat4::CreateOrthographic(minX, maxX, minY, maxY, minZ, maxZ);

            return lightProjection * lightView;
        }

        void ShadowMapPass::Begin(CommandBuffer& cmd)
        {
            (void)cmd;
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
            CalculateCascadeSplits(camera);

            // Get primary directional light
            auto primaryLight = scene.GetPrimaryLight();
            if (!primaryLight || !primaryLight->enabled || !primaryLight->castShadows)
            {
                PYRAMID_LOG_WARN("No primary light with shadows enabled");
                return;
            }

            Math::Vec3 lightDir = primaryLight->direction.Normalized();

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

            if (m_shadowArrayTexture == 0 || !m_shadowArrayTarget)
            {
                PYRAMID_LOG_DEBUG("ShadowMapPass::Execute skipped: no valid shadow array");
                return;
            }

            // Render each cascade directly into its array layer
            for (u32 i = 0; i < m_cascadeCount; i++)
            {
                // Calculate light space matrix for this cascade
                f32 nearPlane = m_cascadeSplits[i];
                f32 farPlane = m_cascadeSplits[i + 1];
                m_lightSpaceMatrices[i] = CalculateLightSpaceMatrix(camera, nearPlane, farPlane, lightDir);

                // Attach layer i of the shadow array, then render into it
                m_device->BindFramebuffer(m_shadowArrayTarget.get());
                glFramebufferTextureLayer(GL_FRAMEBUFFER,
                                          GL_DEPTH_ATTACHMENT,
                                          m_shadowArrayTexture,
                                          0,
                                          static_cast<GLint>(i));

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
                    m_shadowShader->SetUniformMat4("u_LightSpaceMatrix", m_lightSpaceMatrices[i].m);
                    m_device->BindShader(m_shadowShader.get());
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

                PYRAMID_LOG_DEBUG("Cascade ", i, " rendered into array layer ", i,
                                  " (", nearPlane, " - ", farPlane, ")");
            }

            // Restore the default framebuffer after the pass. RenderSystem
            // re-establishes the main viewport after every pass; that restore
            // convention is preserved and is now routed through the neutral
            // BindFramebuffer interface rather than a raw handle.
            m_device->BindFramebuffer(nullptr);
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
            if (count < 1 || count > 8)
            {
                PYRAMID_LOG_WARN("Invalid cascade count: ", count, " (must be 1-8)");
                return;
            }

            // Transactional recreation: the previous array and configuration
            // stay live unless the replacement for the new count succeeds.
            if (!CreateShadowArrayTexture(count, m_shadowMapResolution))
            {
                return;
            }

            m_cascadeCount = count;
            m_cascadeSplits.resize(count + 1);
            m_lightSpaceMatrices.resize(count);

            PYRAMID_LOG_INFO("Cascade count set to ", count);
        }

        void ShadowMapPass::SetShadowMapResolution(u32 resolution)
        {
            if (resolution < 256 || resolution > 8192)
            {
                PYRAMID_LOG_WARN("Invalid shadow map resolution: ", resolution, " (must be 256-8192)");
                return;
            }

            // Transactional recreation: the previous array stays live unless
            // the replacement at the new resolution succeeds.
            if (!CreateShadowArrayTexture(m_cascadeCount, resolution))
            {
                return;
            }

            m_shadowMapResolution = resolution;

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

            m_cascadeSplits = splits;
            PYRAMID_LOG_INFO("Custom cascade splits set");
        }

        void ShadowMapPass::SetDepthBias(f32 bias)
        {
            m_depthBias = bias;
            PYRAMID_LOG_DEBUG("Depth bias set to ", bias);
        }

    } // namespace Renderer
} // namespace Pyramid
