#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/Renderer/ShaderPathResolver.hpp>
#include <Pyramid/Util/Log.hpp>
#include <glad/glad.h>
#include <cmath>
#include <utility>

namespace Pyramid
{
    namespace Renderer
    {
        namespace
        {
            bool IsShadowDataValid(
                const std::vector<std::shared_ptr<OpenGLFramebuffer>>& shadowMaps,
                const std::vector<Math::Mat4>& lightSpaceMatrices,
                const std::vector<f32>& cascadeSplits,
                f32 depthBias)
            {
                const auto count = shadowMaps.size();
                if (count == 0 || count > MaxShadowCascades ||
                    lightSpaceMatrices.size() != count || cascadeSplits.size() != count + 1 ||
                    !std::isfinite(depthBias) || depthBias < 0.0f)
                {
                    return false;
                }
                for (std::size_t index = 0; index < cascadeSplits.size(); ++index)
                {
                    if (!std::isfinite(cascadeSplits[index]) ||
                        (index > 0 && cascadeSplits[index] <= cascadeSplits[index - 1]))
                    {
                        return false;
                    }
                }
                for (std::size_t index = 0; index < count; ++index)
                {
                    const auto& map = shadowMaps[index];
                    if (!map || !map->IsInitialized() || map->IsMultisampled() ||
                        map->GetDepthAttachmentTexture() == 0)
                    {
                        return false;
                    }
                    for (f32 value : lightSpaceMatrices[index].m)
                    {
                        if (!std::isfinite(value)) return false;
                    }
                }
                return true;
            }

            bool IsGBufferReady(const std::shared_ptr<OpenGLFramebuffer>& gBuffer)
            {
                if (!gBuffer || !gBuffer->IsInitialized() || gBuffer->IsMultisampled() ||
                    gBuffer->GetColorAttachmentCount() < 4 || gBuffer->GetDepthAttachmentTexture() == 0)
                {
                    return false;
                }
                for (u32 index = 0; index < 4; ++index)
                {
                    if (gBuffer->GetColorAttachmentTexture(index) == 0) return false;
                }
                return true;
            }
        }

        DeferredLightingPass::DeferredLightingPass(const std::string& name, IGraphicsDevice* device)
            : RenderPass(RenderPassType::Lighting, name)
            , m_device(device)
        {
            if (!m_device)
            {
                PYRAMID_LOG_ERROR("Cannot create deferred lighting pass without a graphics device");
                return;
            }
            m_lightingShader = m_device->CreateShader();
            if (!m_lightingShader)
            {
                PYRAMID_LOG_ERROR("Failed to create deferred lighting shader");
                return;
            }
            const std::string vertSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/deferred_lighting.vert");
            const std::string fragSrc = ShaderPathResolver::LoadTextFile("Engine/Graphics/shaders/deferred_lighting.frag");
            if (vertSrc.empty() || fragSrc.empty())
            {
                PYRAMID_LOG_ERROR("Failed to open deferred lighting shader files");
                return;
            }
            m_shaderReady = m_lightingShader->Compile(vertSrc, fragSrc);
            if (!m_shaderReady)
            {
                PYRAMID_LOG_ERROR("Failed to compile deferred lighting shaders");
                return;
            }
            CreateFullscreenQuad();
        }

        void DeferredLightingPass::CreateFullscreenQuad()
        {
            // One fullscreen triangle, including its texture coordinates.
            const f32 vertices[] = {
                -1.0f, -1.0f, 0.0f, 0.0f,
                 3.0f, -1.0f, 2.0f, 0.0f,
                -1.0f,  3.0f, 0.0f, 2.0f
            };
            auto vbo = m_device->CreateVertexBuffer();
            auto vao = m_device->CreateVertexArray();
            if (!vbo || !vao)
            {
                PYRAMID_LOG_ERROR("Failed to create deferred lighting fullscreen geometry");
                return;
            }
            vbo->SetData(vertices, sizeof(vertices));
            const BufferLayout layout = {
                {ShaderDataType::Float2, "a_Position"},
                {ShaderDataType::Float2, "a_TexCoord"}
            };
            vao->AddVertexBuffer(vbo, layout);
            m_fullscreenQuad = std::move(vao);
        }

        void DeferredLightingPass::Begin(CommandBuffer& cmd)
        {
            (void)cmd;
            if (!m_device) return;
            m_device->BindFramebuffer(nullptr);
            m_device->SetClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            m_device->ClearBuffers(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            m_device->EnableDepthTest(false);
            m_device->EnableCullFace(false);
        }

        void DeferredLightingPass::Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera)
        {
            (void)cmd;
            if (!m_device || !m_shaderReady || !m_lightingShader || !m_fullscreenQuad || !IsGBufferReady(m_gBuffer))
            {
                PYRAMID_LOG_WARN("Deferred lighting requires a compiled shader, fullscreen geometry and a complete sampled G-buffer");
                return;
            }
            m_device->BindShader(m_lightingShader.get());
            const char* gBufferUniforms[] = {
                "u_GAlbedoMetallic", "u_GNormalRoughness", "u_GPositionAO", "u_GEmissive"
            };
            for (u32 index = 0; index < 4; ++index)
            {
                m_device->BindNativeTexture(m_gBuffer->GetColorAttachmentTexture(index), index, GL_TEXTURE_2D);
                m_lightingShader->SetUniformInt(gBufferUniforms[index], static_cast<int>(index));
            }
            m_device->BindNativeTexture(m_gBuffer->GetDepthAttachmentTexture(), 4, GL_TEXTURE_2D);
            m_lightingShader->SetUniformInt("u_GDepth", 4);

            // The lighting draw is immediate; supply camera data immediately too.
            const Math::Vec3 camPos = camera.GetPosition();
            m_lightingShader->SetUniformFloat3("u_CameraPosition", camPos.x, camPos.y, camPos.z);
            m_lightingShader->SetUniformMat4("u_View", camera.GetViewMatrix().m);

            const auto primaryLight = scene.GetPrimaryLight();
            if (primaryLight && primaryLight->enabled)
            {
                const Math::Vec3 lightDir = primaryLight->direction.Normalized();
                m_lightingShader->SetUniformFloat3("u_LightDirection", lightDir.x, lightDir.y, lightDir.z);
                m_lightingShader->SetUniformFloat3("u_LightColor",
                    primaryLight->color.x, primaryLight->color.y, primaryLight->color.z);
                m_lightingShader->SetUniformFloat("u_LightIntensity", primaryLight->intensity);
            }
            else
            {
                m_lightingShader->SetUniformFloat3("u_LightDirection", 0.5f, -1.0f, 0.5f);
                m_lightingShader->SetUniformFloat3("u_LightColor", 1.0f, 1.0f, 1.0f);
                m_lightingShader->SetUniformFloat("u_LightIntensity", 1.0f);
            }

            const bool useShadows = primaryLight && primaryLight->enabled && primaryLight->castShadows &&
                IsShadowDataValid(m_shadowMaps, m_lightSpaceMatrices, m_cascadeSplits, m_shadowBias);
            const u32 cascadeCount = useShadows ? static_cast<u32>(m_shadowMaps.size()) : 0;
            m_lightingShader->SetUniformInt("u_CascadeCount", static_cast<int>(cascadeCount));
            m_lightingShader->SetUniformFloat("u_ShadowBias", m_shadowBias);
            for (u32 index = 0; index < MaxShadowCascades; ++index)
            {
                m_device->BindNativeTexture(index < cascadeCount ? m_shadowMaps[index]->GetDepthAttachmentTexture() : 0,
                    5 + index, GL_TEXTURE_2D);
                const std::string suffix = "[" + std::to_string(index) + "]";
                m_lightingShader->SetUniformInt("u_ShadowMaps" + suffix, static_cast<int>(5 + index));
                if (index < cascadeCount)
                {
                    m_lightingShader->SetUniformMat4("u_LightSpaceMatrices" + suffix, m_lightSpaceMatrices[index].m);
                    m_lightingShader->SetUniformFloat("u_CascadeSplits" + suffix, m_cascadeSplits[index]);
                }
            }
            if (cascadeCount > 0)
            {
                m_lightingShader->SetUniformFloat("u_CascadeSplits[" + std::to_string(cascadeCount) + "]",
                    m_cascadeSplits[cascadeCount]);
            }
            m_lightingShader->SetUniformInt("u_EnableSSAO", m_enableSSAO ? 1 : 0);
            m_lightingShader->SetUniformInt("u_EnableIBL", m_enableIBL ? 1 : 0);
            m_device->BindVertexArray(m_fullscreenQuad.get());
            m_device->DrawArraysInstanced(3, 1, 0);
        }

        void DeferredLightingPass::End(CommandBuffer& cmd)
        {
            (void)cmd;
            if (!m_device) return;
            m_device->EnableDepthTest(true);
            for (u32 index = 0; index < 5 + MaxShadowCascades; ++index)
            {
                m_device->BindNativeTexture(0, index, GL_TEXTURE_2D);
            }
        }

        void DeferredLightingPass::SetGBuffer(std::shared_ptr<OpenGLFramebuffer> gBuffer)
        {
            m_gBuffer = std::move(gBuffer);
        }

        void DeferredLightingPass::SetShadowMaps(const std::vector<std::shared_ptr<OpenGLFramebuffer>>& shadowMaps)
        {
            m_shadowMaps = shadowMaps;
            m_lightSpaceMatrices.clear();
            m_cascadeSplits.clear();
            if (!shadowMaps.empty())
            {
                PYRAMID_LOG_WARN("Deferred shadow maps need matrices and split distances; use SetShadowData to enable shadows");
            }
        }

        bool DeferredLightingPass::SetShadowData(
            const std::vector<std::shared_ptr<OpenGLFramebuffer>>& shadowMaps,
            const std::vector<Math::Mat4>& lightSpaceMatrices,
            const std::vector<f32>& cascadeSplits,
            f32 depthBias)
        {
            if (shadowMaps.empty() && lightSpaceMatrices.empty() && cascadeSplits.empty())
            {
                SetShadowMaps({});
                m_shadowBias = 0.005f;
                return true;
            }
            if (!IsShadowDataValid(shadowMaps, lightSpaceMatrices, cascadeSplits, depthBias))
            {
                SetShadowMaps({});
                m_shadowBias = 0.005f;
                PYRAMID_LOG_ERROR("Invalid deferred shadow data: require 1-8 initialized depth targets, matching finite matrices, increasing finite splits and nonnegative finite bias; shadows disabled");
                return false;
            }
            m_shadowMaps = shadowMaps;
            m_lightSpaceMatrices = lightSpaceMatrices;
            m_cascadeSplits = cascadeSplits;
            m_shadowBias = depthBias;
            return true;
        }
    }
}
