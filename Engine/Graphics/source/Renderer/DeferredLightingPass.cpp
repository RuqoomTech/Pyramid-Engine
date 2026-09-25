#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/Renderer/ShaderPathResolver.hpp>
#include <Pyramid/Util/Log.hpp>
#include "../OpenGL/OpenGLDiagnostics.hpp"
#include <glad/glad.h>
#include <string>

namespace Pyramid
{
    namespace Renderer
    {

        DeferredLightingPass::DeferredLightingPass(const std::string& name, IGraphicsDevice* device)
            : RenderPass(RenderPassType::Lighting, name)
            , m_device(device)
        {
            PYRAMID_LOG_INFO("DeferredLightingPass created");
            
            // Create lighting shader
            m_lightingShader = m_device->CreateShader();
            
            const std::string vertSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/deferred_lighting.vert");
            const std::string fragSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/deferred_lighting.frag");

            if (vertSrc.empty() || fragSrc.empty())
            {
                PYRAMID_LOG_ERROR("Failed to open deferred lighting shader files");
            }
            else
            {
                if (!m_lightingShader->Compile(vertSrc, fragSrc))
                {
                    PYRAMID_LOG_ERROR("Failed to compile deferred lighting shaders");
                }
                else
                {
                    PYRAMID_LOG_INFO("Deferred lighting shaders compiled successfully");
                }
            }
            
            // Create fullscreen quad
            CreateFullscreenQuad();
        }

        void DeferredLightingPass::CreateFullscreenQuad()
        {
            // Fullscreen triangle (more efficient than quad - covers entire screen with one triangle)
            float vertices[] = {
                // Positions      // TexCoords
                -1.0f, -1.0f,     0.0f, 0.0f,
                 3.0f, -1.0f,     2.0f, 0.0f,
                -1.0f,  3.0f,     0.0f, 2.0f
            };
            
            auto vbo = m_device->CreateVertexBuffer();
            vbo->SetData(vertices, sizeof(vertices));
            
            BufferLayout layout = {
                { ShaderDataType::Float2, "a_Position" },
                { ShaderDataType::Float2, "a_TexCoord" }
            };
            
            m_fullscreenQuad = m_device->CreateVertexArray();
            m_fullscreenQuad->AddVertexBuffer(vbo, layout);
            
            PYRAMID_LOG_DEBUG("Fullscreen quad created for deferred lighting");
        }

        void DeferredLightingPass::Begin(CommandBuffer& cmd)
        {
            // Bind default framebuffer (render to screen)
            m_device->BindFramebuffer(nullptr);
            
            // Clear screen
            m_device->SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            m_device->ClearBuffers(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // Disable depth testing (fullscreen quad doesn't need it)
            if (m_device)
            {
                m_device->EnableDepthTest(false);
            }
            
            // Disable face culling
            if (m_device)
            {
                m_device->EnableCullFace(false);
            }
            
            PYRAMID_LOG_DEBUG("DeferredLightingPass::Begin");
        }

        void DeferredLightingPass::Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera)
        {
            if (!m_lightingShader || !m_fullscreenQuad || !m_gBuffer)
            {
                PYRAMID_LOG_WARN("DeferredLightingPass not properly initialized");
                return;
            }
            
            // Bind lighting shader
            m_device->BindShader(m_lightingShader.get());
            
            // Bind G-Buffer textures
            GLuint albedoMetallic = m_gBuffer->GetColorAttachmentTexture(0);
            GLuint normalRoughness = m_gBuffer->GetColorAttachmentTexture(1);
            GLuint positionAO = m_gBuffer->GetColorAttachmentTexture(2);
            GLuint emissive = m_gBuffer->GetColorAttachmentTexture(3);
            GLuint depth = m_gBuffer->GetDepthAttachmentTexture();
            
            m_device->BindNativeTexture(albedoMetallic, 0, GL_TEXTURE_2D);
            m_lightingShader->SetUniformInt("u_GAlbedoMetallic", 0);
            
            m_device->BindNativeTexture(normalRoughness, 1, GL_TEXTURE_2D);
            m_lightingShader->SetUniformInt("u_GNormalRoughness", 1);
            
            m_device->BindNativeTexture(positionAO, 2, GL_TEXTURE_2D);
            m_lightingShader->SetUniformInt("u_GPositionAO", 2);
            
            m_device->BindNativeTexture(emissive, 3, GL_TEXTURE_2D);
            m_lightingShader->SetUniformInt("u_GEmissive", 3);
            
            m_device->BindNativeTexture(depth, 4, GL_TEXTURE_2D);
            m_lightingShader->SetUniformInt("u_GDepth", 4);
            
            // Bind the single shadow array texture when a shadow pass provides
            // one. The bound target must be GL_TEXTURE_2D_ARRAY to match the
            // shader's sampler2DArray; binding a plain GL_TEXTURE_2D here
            // samples undefined data on some drivers.
            const GLuint shadowArray =
                (m_shadowPass != nullptr) ? m_shadowPass->GetShadowArrayTexture() : 0;
            const u32 shadowLayers =
                (m_shadowPass != nullptr) ? m_shadowPass->GetShadowArrayLayers() : 0;
            if (shadowArray != 0 && shadowLayers > 0)
            {
                m_device->BindNativeTexture(shadowArray, kShadowMapSlot, GL_TEXTURE_2D_ARRAY);
                m_lightingShader->SetUniformInt("u_ShadowMaps", static_cast<int>(kShadowMapSlot));

                // Upload the per-cascade transforms from the shadow pass so
                // fragments address the intended layer instead of sticking to
                // cascade zero with identity transforms.
                const std::vector<Math::Mat4>& matrices = m_shadowPass->GetLightSpaceMatrices();
                const u32 matrixCount =
                    (matrices.size() < shadowLayers)
                        ? static_cast<u32>(matrices.size())
                        : shadowLayers;
                if (matrixCount > 0)
                {
                    m_lightingShader->SetUniformMat4("u_LightSpaceMatrices",
                                                     matrices.front().m,
                                                     false,
                                                     static_cast<int>(matrixCount));
                }

                // Upload the cascade split distances (one more entry than
                // layers). IShader has no float-array setter, so each element
                // uploads by indexed name; this keeps the backend-neutral
                // interface unchanged during the freeze.
                const std::vector<f32>& splits = m_shadowPass->GetCascadeSplits();
                const u32 splitUploads = shadowLayers + 1;
                for (u32 i = 0; i < splitUploads && i < splits.size(); ++i)
                {
                    m_lightingShader->SetUniformFloat("u_CascadeSplits[" + std::to_string(i) + "]",
                                                      splits[i]);
                }
                m_lightingShader->SetUniformInt("u_CascadeCount", static_cast<int>(shadowLayers));

                OpenGLDiagnostics::CheckError("DeferredLightingPass::Execute shadow array bind");
            }
            else
            {
                // No valid shadow array: skip the bind entirely (never bind an
                // invalid handle) and let lighting continue unshadowed.
                m_lightingShader->SetUniformInt("u_CascadeCount", 0);
            }
            
            // Set camera uniforms
            Math::Vec3 camPos = camera.GetPosition();
            m_lightingShader->SetUniformFloat3("u_CameraPosition", camPos.x, camPos.y, camPos.z);
            
            // Get primary directional light from scene
            auto primaryLight = scene.GetPrimaryLight();
            if (primaryLight && primaryLight->enabled)
            {
                Math::Vec3 lightDir = primaryLight->direction.Normalized();
                m_lightingShader->SetUniformFloat3("u_LightDirection", lightDir.x, lightDir.y, lightDir.z);
                m_lightingShader->SetUniformFloat3("u_LightColor", 
                    primaryLight->color.x, 
                    primaryLight->color.y, 
                    primaryLight->color.z);
                m_lightingShader->SetUniformFloat("u_LightIntensity", primaryLight->intensity);
            }
            else
            {
                // Default lighting
                m_lightingShader->SetUniformFloat3("u_LightDirection", 0.5f, -1.0f, 0.5f);
                m_lightingShader->SetUniformFloat3("u_LightColor", 1.0f, 1.0f, 1.0f);
                m_lightingShader->SetUniformFloat("u_LightIntensity", 1.0f);
            }
            
            // Set shadow parameters (cascade count is uploaded alongside the
            // shadow array bind above so the two can never disagree)
            m_lightingShader->SetUniformFloat("u_ShadowBias", 0.005f);
            
            // Set technique flags
            m_lightingShader->SetUniformInt("u_EnableSSAO", m_enableSSAO ? 1 : 0);
            m_lightingShader->SetUniformInt("u_EnableIBL", m_enableIBL ? 1 : 0);
            
            // Draw fullscreen quad
            m_device->BindVertexArray(m_fullscreenQuad.get());
            m_device->DrawArraysInstanced(3, 1, 0);
            
            PYRAMID_LOG_DEBUG("DeferredLightingPass::Execute - Fullscreen lighting applied");
        }

        void DeferredLightingPass::End(CommandBuffer& cmd)
        {
            // Re-enable depth testing for subsequent passes
            if (m_device)
            {
                m_device->EnableDepthTest(true);
            }
            
            // Unbind textures. The shadow slot holds a texture array, so it
            // must be unbound with the array target, not GL_TEXTURE_2D.
            for (u32 i = 0; i < kShadowMapSlot; i++)
            {
                m_device->BindNativeTexture(0, i, GL_TEXTURE_2D);
            }
            m_device->BindNativeTexture(0, kShadowMapSlot, GL_TEXTURE_2D_ARRAY);
            
            PYRAMID_LOG_DEBUG("DeferredLightingPass::End");
        }

        void DeferredLightingPass::SetGBuffer(std::shared_ptr<OpenGLFramebuffer> gBuffer)
        {
            m_gBuffer = gBuffer;
            PYRAMID_LOG_DEBUG("G-Buffer set for deferred lighting pass");
        }

        void DeferredLightingPass::SetShadowPass(const ShadowMapPass* shadowPass)
        {
            m_shadowPass = shadowPass;
            const u32 layers = (shadowPass != nullptr) ? shadowPass->GetShadowArrayLayers() : 0;
            PYRAMID_LOG_DEBUG("Shadow pass set for deferred lighting pass (", layers, " array layers)");
        }

    } // namespace Renderer
} // namespace Pyramid
