#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>

#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Util/Log.hpp>

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

        void CommandBuffer::DrawIndexed(
            u32 indexCount,
            u32 instanceCount,
            PrimitiveTopology topology)
        {
            if (!m_recording || indexCount == 0 || instanceCount == 0) return;

            RenderCommand cmd{};
            cmd.type = RenderCommandType::DrawIndexed;
            cmd.data.draw.count = indexCount;
            cmd.data.draw.instanceCount = instanceCount;
            cmd.data.draw.firstVertex = 0;
            cmd.data.draw.topology = static_cast<u32>(topology);
            m_commands.push_back(cmd);
        }

        void CommandBuffer::DrawArrays(
            u32 vertexCount,
            u32 firstVertex,
            u32 instanceCount,
            PrimitiveTopology topology)
        {
            if (!m_recording || vertexCount == 0 || instanceCount == 0) return;

            RenderCommand cmd{};
            cmd.type = RenderCommandType::DrawArrays;
            cmd.data.draw.count = vertexCount;
            cmd.data.draw.instanceCount = instanceCount;
            cmd.data.draw.firstVertex = firstVertex;
            cmd.data.draw.topology = static_cast<u32>(topology);
            m_commands.push_back(cmd);
        }

        void CommandBuffer::DrawMesh(const Mesh& mesh, u32 instanceCount)
        {
            if (!m_recording || !mesh.IsValid() || instanceCount == 0) return;

            SetVertexArray(mesh.GetVertexArray().get());
            if (mesh.IsIndexed())
            {
                DrawIndexed(mesh.GetIndexCount(), instanceCount, mesh.GetTopology());
            }
            else
            {
                DrawArrays(mesh.GetVertexCount(), 0, instanceCount, mesh.GetTopology());
            }
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
                    {
                        const auto topology = static_cast<PrimitiveTopology>(cmd.data.draw.topology);
                        if (cmd.data.draw.instanceCount > 1)
                        {
                            device->DrawIndexedInstanced(
                                cmd.data.draw.count,
                                cmd.data.draw.instanceCount,
                                topology);
                        }
                        else
                        {
                            device->DrawIndexed(cmd.data.draw.count, topology);
                        }
                        break;
                    }

                    case RenderCommandType::DrawArrays:
                    {
                        const auto topology = static_cast<PrimitiveTopology>(cmd.data.draw.topology);
                        if (cmd.data.draw.instanceCount > 1)
                        {
                            device->DrawArraysInstanced(
                                cmd.data.draw.count,
                                cmd.data.draw.instanceCount,
                                cmd.data.draw.firstVertex,
                                topology);
                        }
                        else
                        {
                            device->DrawArrays(
                                cmd.data.draw.count,
                                cmd.data.draw.firstVertex,
                                topology);
                        }
                        break;
                    }

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

    }
}
