#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
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

        // CommandBuffer Implementation
        CommandBuffer::CommandBuffer()
        {
            m_commands.reserve(1024); // Pre-allocate for performance
        }

        CommandBuffer::~CommandBuffer() = default;

        void CommandBuffer::Begin()
        {
            m_recording = true;
            m_commands.clear();
        }

        void CommandBuffer::End()
        {
            m_recording = false;
        }

        void CommandBuffer::Reset()
        {
            m_commands.clear();
            m_recording = false;
        }

        void CommandBuffer::SetRenderTarget(u32 targetId)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::SetRenderTarget;
            cmd.data.setRenderTarget.targetId = targetId;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetShader(u32 shaderId)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::SetShader;
            cmd.data.setShader.shaderId = shaderId;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetTexture(u32 textureId, u32 slot)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::SetTexture;
            cmd.data.setTexture.textureId = textureId;
            cmd.data.setTexture.slot = slot;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetUniformBuffer(u32 bufferId, u32 bindingPoint)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::SetUniformBuffer;
            cmd.data.setUniformBuffer.bufferId = bufferId;
            cmd.data.setUniformBuffer.bindingPoint = bindingPoint;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetVertexArray(u32 vertexArrayId)
        {
            if (!m_recording) return;

            RenderCommand cmd;
            cmd.type = RenderCommandType::SetVertexArray;
            cmd.data.setVertexArray.vertexArrayId = vertexArrayId;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetRenderTarget(RenderTarget* target)
        {
            if (!m_recording) return;

            RenderCommand cmd;
            cmd.type = RenderCommandType::SetRenderTargetPtr;
            cmd.data.setRenderTargetPtr.target = reinterpret_cast<std::uintptr_t>(target);
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetShader(IShader* shader)
        {
            if (!m_recording) return;

            RenderCommand cmd;
            cmd.type = RenderCommandType::SetShaderPtr;
            cmd.data.setShaderPtr.shader = reinterpret_cast<std::uintptr_t>(shader);
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetTexture(ITexture2D* texture, u32 slot)
        {
            if (!m_recording) return;

            RenderCommand cmd;
            cmd.type = RenderCommandType::SetTexturePtr;
            cmd.data.setTexturePtr.texture = reinterpret_cast<std::uintptr_t>(texture);
            cmd.data.setTexturePtr.slot = slot;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetUniformBuffer(IUniformBuffer* buffer, u32 bindingPoint)
        {
            if (!m_recording) return;

            RenderCommand cmd;
            cmd.type = RenderCommandType::SetUniformBufferPtr;
            cmd.data.setUniformBufferPtr.buffer = reinterpret_cast<std::uintptr_t>(buffer);
            cmd.data.setUniformBufferPtr.bindingPoint = bindingPoint;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::SetVertexArray(IVertexArray* vertexArray)
        {
            if (!m_recording) return;

            RenderCommand cmd;
            cmd.type = RenderCommandType::SetVertexArrayPtr;
            cmd.data.setVertexArrayPtr.vertexArray = reinterpret_cast<std::uintptr_t>(vertexArray);
            m_commands.push_back(cmd);
        }

        void CommandBuffer::DrawIndexed(u32 indexCount, u32 instanceCount)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::DrawIndexed;
            cmd.data.draw.indexCount = indexCount;
            cmd.data.draw.instanceCount = instanceCount;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::Dispatch(u32 x, u32 y, u32 z)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::Dispatch;
            cmd.data.dispatch.x = x;
            cmd.data.dispatch.y = y;
            cmd.data.dispatch.z = z;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::ClearTarget(f32 r, f32 g, f32 b, f32 a)
        {
            if (!m_recording) return;
            
            RenderCommand cmd;
            cmd.type = RenderCommandType::ClearTarget;
            cmd.data.clear.r = r;
            cmd.data.clear.g = g;
            cmd.data.clear.b = b;
            cmd.data.clear.a = a;
            m_commands.push_back(cmd);
        }

        void CommandBuffer::RegisterRenderTarget(u32 id, RenderTarget* target)
        {
            if (!target)
            {
                m_renderTargetRegistry.erase(id);
                return;
            }
            m_renderTargetRegistry[id] = target;
        }

        void CommandBuffer::RegisterShader(u32 id, IShader* shader)
        {
            if (!shader)
            {
                m_shaderRegistry.erase(id);
                return;
            }
            m_shaderRegistry[id] = shader;
        }

        void CommandBuffer::RegisterTexture(u32 id, ITexture2D* texture)
        {
            if (!texture)
            {
                m_textureRegistry.erase(id);
                return;
            }
            m_textureRegistry[id] = texture;
        }

        void CommandBuffer::RegisterUniformBuffer(u32 id, IUniformBuffer* buffer)
        {
            if (!buffer)
            {
                m_uniformBufferRegistry.erase(id);
                return;
            }
            m_uniformBufferRegistry[id] = buffer;
        }

        void CommandBuffer::RegisterVertexArray(u32 id, IVertexArray* vertexArray)
        {
            if (!vertexArray)
            {
                m_vertexArrayRegistry.erase(id);
                return;
            }
            m_vertexArrayRegistry[id] = vertexArray;
        }

        void CommandBuffer::Execute(IGraphicsDevice* device)
        {
            if (!device) {
                PYRAMID_LOG_ERROR("Cannot execute command buffer: device is null");
                return;
            }

            for (const auto& cmd : m_commands) {
                switch (cmd.type) {
                    case RenderCommandType::SetRenderTarget: {
                        auto targetIt = m_renderTargetRegistry.find(cmd.data.setRenderTarget.targetId);
                        if (targetIt != m_renderTargetRegistry.end() && targetIt->second)
                        {
                            targetIt->second->Bind();
                        }
                        else
                        {
                            PYRAMID_LOG_WARN("SetRenderTarget command could not resolve targetId=", cmd.data.setRenderTarget.targetId);
                        }
                        break;
                    }

                    case RenderCommandType::SetRenderTargetPtr: {
                        auto* target = reinterpret_cast<RenderTarget*>(cmd.data.setRenderTargetPtr.target);
                        if (target)
                        {
                            target->Bind();
                        }
                        else
                        {
                            device->BindFramebufferHandle(0);
                        }
                        break;
                    }
                        
                    case RenderCommandType::SetShader: {
                        auto shaderIt = m_shaderRegistry.find(cmd.data.setShader.shaderId);
                        if (shaderIt != m_shaderRegistry.end())
                        {
                            device->BindShader(shaderIt->second);
                        }
                        else
                        {
                            PYRAMID_LOG_WARN("SetShader command could not resolve shaderId=", cmd.data.setShader.shaderId);
                        }
                        break;
                    }

                    case RenderCommandType::SetShaderPtr:
                        device->BindShader(reinterpret_cast<IShader*>(cmd.data.setShaderPtr.shader));
                        break;
                        
                    case RenderCommandType::SetTexture: {
                        auto textureIt = m_textureRegistry.find(cmd.data.setTexture.textureId);
                        if (textureIt != m_textureRegistry.end())
                        {
                            device->BindTexture(textureIt->second, cmd.data.setTexture.slot);
                        }
                        else
                        {
                            PYRAMID_LOG_WARN("SetTexture command could not resolve textureId=", cmd.data.setTexture.textureId);
                        }
                        break;
                    }

                    case RenderCommandType::SetTexturePtr:
                        device->BindTexture(reinterpret_cast<ITexture2D*>(cmd.data.setTexturePtr.texture), cmd.data.setTexturePtr.slot);
                        break;
                        
                    case RenderCommandType::SetUniformBuffer: {
                        auto bufferIt = m_uniformBufferRegistry.find(cmd.data.setUniformBuffer.bufferId);
                        if (bufferIt != m_uniformBufferRegistry.end())
                        {
                            device->BindUniformBuffer(bufferIt->second, cmd.data.setUniformBuffer.bindingPoint);
                        }
                        else
                        {
                            PYRAMID_LOG_WARN("SetUniformBuffer command could not resolve bufferId=", cmd.data.setUniformBuffer.bufferId);
                        }
                        break;
                    }

                    case RenderCommandType::SetUniformBufferPtr:
                        device->BindUniformBuffer(reinterpret_cast<IUniformBuffer*>(cmd.data.setUniformBufferPtr.buffer), cmd.data.setUniformBufferPtr.bindingPoint);
                        break;

                    case RenderCommandType::SetVertexArray: {
                        auto vaoIt = m_vertexArrayRegistry.find(cmd.data.setVertexArray.vertexArrayId);
                        if (vaoIt != m_vertexArrayRegistry.end())
                        {
                            device->BindVertexArray(vaoIt->second);
                        }
                        else
                        {
                            PYRAMID_LOG_WARN("SetVertexArray command could not resolve vertexArrayId=", cmd.data.setVertexArray.vertexArrayId);
                        }
                        break;
                    }

                    case RenderCommandType::SetVertexArrayPtr:
                        device->BindVertexArray(reinterpret_cast<IVertexArray*>(cmd.data.setVertexArrayPtr.vertexArray));
                        break;
                        
                    case RenderCommandType::ClearTarget:
                        device->Clear(Color(cmd.data.clear.r, cmd.data.clear.g,
                                          cmd.data.clear.b, cmd.data.clear.a));
                        break;
                    
                    case RenderCommandType::DrawIndexed:
                        if (cmd.data.draw.instanceCount > 1) {
                            device->DrawIndexedInstanced(cmd.data.draw.indexCount, cmd.data.draw.instanceCount);
                        } else {
                            device->DrawIndexed(cmd.data.draw.indexCount);
                        }
                        break;
                        
                    case RenderCommandType::DrawInstanced:
                        device->DrawIndexedInstanced(cmd.data.draw.indexCount, cmd.data.draw.instanceCount);
                        break;
                    
                    case RenderCommandType::Dispatch:
                        PYRAMID_LOG_DEBUG("Compute dispatch command: ", cmd.data.dispatch.x, "x", cmd.data.dispatch.y, "x", cmd.data.dispatch.z);
                        // Dispatch will be handled when compute shader support is added
                        break;
                    
                    default:
                        PYRAMID_LOG_WARN("Unknown render command type: ", static_cast<int>(cmd.type));
                        break;
                }
            }
        }

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
            // Bind shader
            if (material.shader) {
                cmdBuffer.SetShader(material.shader.get());
            }
            
            // Bind textures to their respective slots
            if (material.albedoTexture) {
                cmdBuffer.SetTexture(material.albedoTexture.get(), 0); // Albedo at slot 0
            } else {
                cmdBuffer.SetTexture(static_cast<ITexture2D*>(nullptr), 0);
            }
            
            if (material.normalTexture) {
                cmdBuffer.SetTexture(material.normalTexture.get(), 1); // Normal at slot 1
            } else {
                cmdBuffer.SetTexture(static_cast<ITexture2D*>(nullptr), 1);
            }
            
            if (material.metallicRoughnessTexture) {
                cmdBuffer.SetTexture(material.metallicRoughnessTexture.get(), 2); // MetallicRoughness at slot 2
            } else {
                cmdBuffer.SetTexture(static_cast<ITexture2D*>(nullptr), 2);
            }
            
            if (material.aoTexture) {
                cmdBuffer.SetTexture(material.aoTexture.get(), 3); // AO at slot 3
            } else {
                cmdBuffer.SetTexture(static_cast<ITexture2D*>(nullptr), 3);
            }
            
            if (material.emissiveTexture) {
                cmdBuffer.SetTexture(material.emissiveTexture.get(), 4); // Emissive at slot 4
            } else {
                cmdBuffer.SetTexture(static_cast<ITexture2D*>(nullptr), 4);
            }
            
            // Material properties can be set as shader uniforms if needed
            // This will be handled by the render pass when setting per-material uniforms
        }

    } // namespace Renderer
} // namespace Pyramid
