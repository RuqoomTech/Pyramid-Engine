#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Util/Log.hpp>
#include <glad/glad.h>
#include <algorithm>
#include <chrono>

namespace Pyramid
{
    namespace Renderer
    {

        // RenderTarget Implementation
        RenderTarget::RenderTarget(const RenderTargetSpec& spec)
            : m_spec(spec)
        {
        }

        RenderTarget::~RenderTarget() = default;

        bool RenderTarget::Initialize(IGraphicsDevice* device)
        {
            if (!device)
            {
                PYRAMID_LOG_ERROR("Cannot initialize render target: device is null");
                return false;
            }

            if (m_initialized)
            {
                return true;
            }

            if (!FramebufferUtils::IsValidExtent(m_spec.width, m_spec.height))
            {
                PYRAMID_LOG_ERROR(
                    "Cannot initialize render target with non-renderable extent ",
                    m_spec.width, "x", m_spec.height);
                return false;
            }

            FramebufferSpec framebufferSpec;
            framebufferSpec.width = m_spec.width;
            framebufferSpec.height = m_spec.height;
            framebufferSpec.samples = m_spec.multisampled ? m_spec.samples : 1;

            for (u32 index = 0; index < m_spec.colorAttachments; ++index)
            {
                FramebufferAttachmentSpec colorAttachment;
                colorAttachment.type = FramebufferAttachmentType::Color;
                colorAttachment.internalFormat = GL_RGBA16F;
                colorAttachment.format = GL_RGBA;
                colorAttachment.dataType = GL_FLOAT;
                colorAttachment.colorAttachmentIndex = index;
                colorAttachment.multisampled = m_spec.multisampled;
                colorAttachment.samples = framebufferSpec.samples;
                framebufferSpec.attachments.push_back(colorAttachment);
            }

            if (m_spec.hasDepthStencil)
            {
                FramebufferAttachmentSpec depthStencilAttachment;
                depthStencilAttachment.type = FramebufferAttachmentType::DepthStencil;
                depthStencilAttachment.internalFormat = GL_DEPTH24_STENCIL8;
                depthStencilAttachment.format = GL_DEPTH_STENCIL;
                depthStencilAttachment.dataType = GL_UNSIGNED_INT_24_8;
                depthStencilAttachment.minFilter = GL_NEAREST;
                depthStencilAttachment.magFilter = GL_NEAREST;
                depthStencilAttachment.multisampled = m_spec.multisampled;
                depthStencilAttachment.samples = framebufferSpec.samples;
                framebufferSpec.attachments.push_back(depthStencilAttachment);
            }

            m_framebuffer = std::make_unique<OpenGLFramebuffer>(framebufferSpec);
            if (!m_framebuffer->Initialize())
            {
                PYRAMID_LOG_ERROR("Failed to initialize render target framebuffer");
                m_framebuffer.reset();
                return false;
            }

            m_initialized = true;
            PYRAMID_LOG_INFO(
                "Render target initialized: ", m_spec.width, "x", m_spec.height,
                " with ", m_spec.colorAttachments, " color attachments");
            return true;
        }

        bool RenderTarget::Resize(u32 width, u32 height)
        {
            if (!FramebufferUtils::IsValidExtent(width, height))
            {
                PYRAMID_LOG_WARN(
                    "Ignoring render target resize to non-renderable extent ",
                    width, "x", height);
                return false;
            }

            if (m_spec.width == width && m_spec.height == height)
            {
                return true;
            }

            if (!m_initialized || !m_framebuffer)
            {
                m_spec.width = width;
                m_spec.height = height;
                return true;
            }

            if (!m_framebuffer->Resize(width, height))
            {
                PYRAMID_LOG_ERROR(
                    "Render target resize failed; preserving ",
                    m_spec.width, "x", m_spec.height);
                return false;
            }

            m_spec.width = width;
            m_spec.height = height;
            return true;
        }

        void RenderTarget::Bind()
        {
            if (!m_framebuffer)
            {
                PYRAMID_LOG_ERROR("Cannot bind an uninitialized render target");
                return;
            }
            m_framebuffer->Bind();
        }

        void RenderTarget::Unbind()
        {
            if (m_framebuffer)
            {
                m_framebuffer->Unbind();
            }
        }

        void RenderTarget::Clear(f32 r, f32 g, f32 b, f32 a)
        {
            if (!m_framebuffer)
            {
                PYRAMID_LOG_ERROR("Cannot clear an uninitialized render target");
                return;
            }
            m_framebuffer->Clear(r, g, b, a);
        }

        bool RenderTarget::IsComplete() const
        {
            return m_framebuffer && m_framebuffer->IsComplete();
        }

        u32 RenderTarget::GetColorTexture(u32 index) const
        {
            return m_framebuffer ? m_framebuffer->GetColorAttachmentTexture(index) : 0;
        }

        u32 RenderTarget::GetDepthTexture() const
        {
            return m_framebuffer ? m_framebuffer->GetDepthAttachmentTexture() : 0;
        }

        // RenderPass Implementation
        RenderPass::RenderPass(RenderPassType type, const std::string& name)
            : m_type(type), m_name(name)
        {
        }

        bool RenderPass::Resize(u32 width, u32 height)
        {
            if (!m_renderTarget)
            {
                return true;
            }
            return m_renderTarget->Resize(width, height);
        }

        // RenderSystem Implementation
        RenderSystem::RenderSystem()
        {
            m_mainCommandBuffer = std::make_unique<CommandBuffer>();
            m_shadowCommandBuffer = std::make_unique<CommandBuffer>();
        }

        RenderSystem::~RenderSystem()
        {
            Shutdown();
        }

        bool RenderSystem::Initialize(IGraphicsDevice* device)
        {
            if (!device) {
                PYRAMID_LOG_ERROR("Cannot initialize render system: device is null");
                return false;
            }

            m_device = device;

            // Create global uniform buffers
            struct CameraUniforms {
                Math::Mat4 viewMatrix;
                Math::Mat4 projectionMatrix;
                Math::Mat4 viewProjectionMatrix;
                Math::Vec4 cameraPosition;
                Math::Vec4 cameraDirection;
                f32 nearPlane;
                f32 farPlane;
                f32 fov;
                f32 aspectRatio;
            };

            m_cameraUBO = device->CreateUniformBuffer(sizeof(CameraUniforms));
            if (!m_cameraUBO) {
                PYRAMID_LOG_ERROR("Failed to create camera uniform buffer");
                return false;
            }

            m_lightingUBO = device->CreateUniformBuffer(sizeof(LightingData));
            if (!m_lightingUBO) {
                PYRAMID_LOG_ERROR("Failed to create lighting uniform buffer");
                return false;
            }

            // Register global buffers for ID-based command paths.
            m_mainCommandBuffer->RegisterUniformBuffer(0, m_cameraUBO.get());
            m_mainCommandBuffer->RegisterUniformBuffer(1, m_lightingUBO.get());
            m_shadowCommandBuffer->RegisterUniformBuffer(0, m_cameraUBO.get());
            m_shadowCommandBuffer->RegisterUniformBuffer(1, m_lightingUBO.get());

            // Setup default render passes
            SetupDefaultRenderPasses();

            PYRAMID_LOG_INFO("Render system initialized successfully");
            PYRAMID_LOG_INFO("  Camera UBO size: ", sizeof(CameraUniforms), " bytes");
            PYRAMID_LOG_INFO("  Lighting UBO size: ", sizeof(LightingData), " bytes");
            PYRAMID_LOG_INFO("  Default render passes: ", m_renderPasses.size());

            return true;
        }

        void RenderSystem::Shutdown()
        {
            m_renderPasses.clear();
            m_renderTargets.clear();
            m_cameraUBO.reset();
            m_lightingUBO.reset();
            m_device = nullptr;

            PYRAMID_LOG_INFO("Render system shut down");
        }

        void RenderSystem::BeginFrame()
        {
            if (!m_device) return;

            // Reset statistics
            m_stats = {};

            // Begin main command buffer
            m_mainCommandBuffer->Begin();
        }

        void RenderSystem::Render(const Scene& scene, const Camera& camera)
        {
            if (!m_device) return;

            auto frameStart = std::chrono::high_resolution_clock::now();

            // Execute render passes in order
            for (auto& pass : m_renderPasses) {
                if (!pass->IsEnabled()) continue;

                // Keep global UBO state in sync for each pass execution.
                UpdateGlobalUniforms(camera);
                pass->Begin(*m_mainCommandBuffer);
                pass->Execute(*m_mainCommandBuffer, scene, camera);
                m_mainCommandBuffer->End();
                m_mainCommandBuffer->Execute(m_device);
                pass->End(*m_mainCommandBuffer);

                // Prepare command buffer for next pass.
                m_mainCommandBuffer->Reset();
                m_mainCommandBuffer->Begin();
            }

            // Calculate frame time
            auto frameEnd = std::chrono::high_resolution_clock::now();
            m_stats.frameTime = std::chrono::duration<f32, std::milli>(frameEnd - frameStart).count();
        }

        void RenderSystem::EndFrame()
        {
            if (!m_device) return;

            // Execute any trailing commands that were recorded after the last pass.
            m_mainCommandBuffer->End();
            if (m_mainCommandBuffer->GetCommandCount() > 0)
            {
                m_mainCommandBuffer->Execute(m_device);
            }

            // Present frame
            m_device->Present(m_vsyncEnabled);

            // Reset command buffer for next frame
            m_mainCommandBuffer->Reset();
        }

        bool RenderSystem::Resize(u32 width, u32 height)
        {
            if (!FramebufferUtils::IsValidExtent(width, height))
            {
                PYRAMID_LOG_WARN(
                    "Ignoring render system resize to non-renderable extent ",
                    width, "x", height);
                return false;
            }

            bool success = true;
            for (auto& [id, target] : m_renderTargets)
            {
                if (target && !target->Resize(width, height))
                {
                    PYRAMID_LOG_ERROR("Failed to resize render target id=", id);
                    success = false;
                }
            }

            for (auto& pass : m_renderPasses)
            {
                if (pass && !pass->Resize(width, height))
                {
                    PYRAMID_LOG_ERROR("Failed to resize render pass: ", pass->GetName());
                    success = false;
                }
            }

            m_width = width;
            m_height = height;

            if (success)
            {
                PYRAMID_LOG_INFO("Render system resized to ", width, "x", height);
            }
            else
            {
                PYRAMID_LOG_ERROR(
                    "Render system resize was incomplete; failed targets preserved their previous attachments");
            }

            return success;
        }

        u32 RenderSystem::CreateRenderTarget(const RenderTargetSpec& spec)
        {
            if (!m_device)
            {
                PYRAMID_LOG_ERROR("Cannot create render target before render system initialization");
                return 0;
            }

            auto target = std::make_shared<RenderTarget>(spec);
            if (!target->Initialize(m_device))
            {
                PYRAMID_LOG_ERROR("Failed to initialize render target");
                return 0;
            }

            const u32 id = m_nextRenderTargetId++;
            m_renderTargets[id] = target;

            m_mainCommandBuffer->RegisterRenderTarget(id, target.get());
            m_shadowCommandBuffer->RegisterRenderTarget(id, target.get());

            return id;
        }

        std::shared_ptr<RenderTarget> RenderSystem::GetRenderTarget(u32 id)
        {
            auto it = m_renderTargets.find(id);
            if (it == m_renderTargets.end())
            {
                return nullptr;
            }
            return it->second;
        }

        void RenderSystem::SetMultisampling(bool enabled, u32 samples)
        {
            m_multisamplingEnabled = enabled;
            m_msaaSamples = (samples == 0) ? 1 : samples;
        }

        void RenderSystem::AddRenderPass(std::shared_ptr<RenderPass> pass)
        {
            if (!pass) return;

            // Insert pass in correct order based on type
            auto insertPos = std::lower_bound(m_renderPasses.begin(), m_renderPasses.end(), pass,
                [](const std::shared_ptr<RenderPass>& a, const std::shared_ptr<RenderPass>& b) {
                    return static_cast<int>(a->GetType()) < static_cast<int>(b->GetType());
                });

            m_renderPasses.insert(insertPos, pass);
            PYRAMID_LOG_INFO("Added render pass: ", pass->GetName());
        }

        void RenderSystem::RemoveRenderPass(RenderPassType type)
        {
            auto it = std::find_if(m_renderPasses.begin(), m_renderPasses.end(),
                [type](const std::shared_ptr<RenderPass>& pass) {
                    return pass->GetType() == type;
                });

            if (it != m_renderPasses.end()) {
                PYRAMID_LOG_INFO("Removed render pass: ", (*it)->GetName());
                m_renderPasses.erase(it);
            }
        }

        std::shared_ptr<RenderPass> RenderSystem::GetRenderPass(RenderPassType type)
        {
            auto it = std::find_if(m_renderPasses.begin(), m_renderPasses.end(),
                [type](const std::shared_ptr<RenderPass>& pass) {
                    return pass->GetType() == type;
                });

            return (it != m_renderPasses.end()) ? *it : nullptr;
        }

        void RenderSystem::SetupDefaultRenderPasses()
        {
            // Create shadow map pass (renders first)
            auto shadowPass = std::make_shared<ShadowMapPass>("Shadow", m_device, 4);
            shadowPass->SetShadowMapResolution(2048);
            shadowPass->SetDepthBias(0.005f);
            AddRenderPass(shadowPass);

            // Create forward render pass
            auto forwardPass = std::make_shared<ForwardRenderPass>(m_device);
            forwardPass->SetClearColor(Math::Vec4(0.1f, 0.1f, 0.15f, 1.0f));
            AddRenderPass(forwardPass);

            // Future: Add post-process, UI passes

            PYRAMID_LOG_INFO("Default render passes initialized with shadows");
        }

        void RenderSystem::SetupDeferredPipeline()
        {
            // Clear existing render passes
            m_renderPasses.clear();

            const u32 width = m_width;
            const u32 height = m_height;

            // Shadow pass
            auto shadowPass = std::make_shared<ShadowMapPass>("Shadow", m_device, 4);
            shadowPass->SetShadowMapResolution(2048);
            shadowPass->SetDepthBias(0.005f);
            AddRenderPass(shadowPass);

            // Deferred geometry pass
            auto geometryPass = std::make_shared<DeferredGeometryPass>("DeferredGeometry", m_device, width, height);
            AddRenderPass(geometryPass);

            // Deferred lighting pass
            auto lightingPass = std::make_shared<DeferredLightingPass>("DeferredLighting", m_device);

            // Connect G-Buffer from geometry pass to lighting pass
            lightingPass->SetGBuffer(geometryPass->GetGBuffer());

            // Connect shadow maps from shadow pass to lighting pass
            lightingPass->SetShadowMaps(shadowPass->GetShadowMaps());

            AddRenderPass(lightingPass);

            PYRAMID_LOG_INFO("Deferred rendering pipeline initialized");
        }

        void RenderSystem::UpdateGlobalUniforms(const Camera& camera)
        {
            // Update camera UBO
            if (m_cameraUBO) {
                struct CameraUniforms {
                    Math::Mat4 viewMatrix;
                    Math::Mat4 projectionMatrix;
                    Math::Mat4 viewProjectionMatrix;
                    Math::Vec4 cameraPosition;
                    Math::Vec4 cameraDirection;
                    f32 nearPlane;
                    f32 farPlane;
                    f32 fov;
                    f32 aspectRatio;
                };

                CameraUniforms cameraData;
                cameraData.viewMatrix = camera.GetViewMatrix();
                cameraData.projectionMatrix = camera.GetProjectionMatrix();
                cameraData.viewProjectionMatrix = camera.GetViewProjectionMatrix();
                cameraData.cameraPosition = Math::Vec4(camera.GetPosition(), 1.0f);
                cameraData.cameraDirection = Math::Vec4(camera.GetForward(), 0.0f);
                cameraData.nearPlane = camera.GetNearPlane();
                cameraData.farPlane = camera.GetFarPlane();
                cameraData.fov = camera.GetFOV();
                cameraData.aspectRatio = camera.GetAspectRatio();

                m_cameraUBO->UpdateData(&cameraData, sizeof(CameraUniforms), 0);
                m_mainCommandBuffer->SetUniformBuffer(m_cameraUBO.get(), 0);
            }

            // Update lighting UBO
            if (m_lightingUBO) {
                LightingData lightingData;
                // Use default lighting for now - can be extended to use scene lights
                m_lightingUBO->UpdateData(&lightingData, sizeof(LightingData), 0);
                m_mainCommandBuffer->SetUniformBuffer(m_lightingUBO.get(), 1);
            }
        }

        void RenderSystem::BindMaterial(CommandBuffer& cmdBuffer, const Material& material)
        {
            cmdBuffer.SetMaterial(&material);
        }

    } // namespace Renderer
} // namespace Pyramid
