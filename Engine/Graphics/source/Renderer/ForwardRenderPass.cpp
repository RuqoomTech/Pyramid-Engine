#include <Pyramid/Graphics/Renderer/RenderPasses.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Util/Log.hpp>
#include <glad/glad.h>

namespace Pyramid
{
    namespace Renderer
    {

        ForwardRenderPass::ForwardRenderPass(IGraphicsDevice* device)
            : RenderPass(RenderPassType::Forward, "ForwardRenderPass")
            , m_device(device)
        {
            PYRAMID_LOG_INFO("ForwardRenderPass created");
        }

        void ForwardRenderPass::Begin(CommandBuffer& cmd)
        {
            // Clear color and depth buffers
            cmd.ClearTarget(m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w);

            // Set wireframe mode if enabled
            if (m_device)
            {
                m_device->SetPolygonMode(m_wireframe ? GL_LINE : GL_FILL);
                m_device->EnableDepthTest(true);
                m_device->SetDepthFunc(GL_LESS);
                m_device->EnableCullFace(true);
                m_device->SetCullFace(GL_BACK);
            }

            PYRAMID_LOG_DEBUG("ForwardRenderPass::Begin - Clear color: ",
                m_clearColor.x, ", ", m_clearColor.y, ", ", m_clearColor.z, ", ", m_clearColor.w);
        }

        void ForwardRenderPass::Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera)
        {
            // Get visible objects from the scene
            auto visibleObjects = scene.GetVisibleObjects(camera);

            PYRAMID_LOG_DEBUG("ForwardRenderPass::Execute - Rendering ", visibleObjects.size(), " visible objects");

            // Render each visible object
            for (const auto& object : visibleObjects)
            {
                if (!object || !object->visible) continue;

                // Skip objects without geometry
                if (!object->mesh || !object->mesh->IsValid()) {
                    PYRAMID_LOG_WARN("Object '", object->name, "' has no valid mesh, skipping");
                    continue;
                }

                if (!object->material || !object->material->IsValid())
                {
                    PYRAMID_LOG_WARN("Object '", object->name, "' has no valid material, skipping");
                    continue;
                }

                const Math::Mat4 model = object->GetTransformMatrix();
                const Math::Mat4 viewProj = camera.GetViewProjectionMatrix();
                const Math::Mat4 normalMatrix = model.Inverse().Transpose();

                cmd.SetMaterial(object->material.get());
                cmd.SetUniformInt("u_HasAlbedoMap", 0);
                cmd.SetUniformInt("u_HasNormalMap", 0);
                cmd.SetUniformInt("u_HasMetallicRoughnessMap", 0);
                for (const auto& binding : object->material->GetTextures())
                {
                    if (binding.uniformName == "u_AlbedoMap") cmd.SetUniformInt("u_HasAlbedoMap", 1);
                    else if (binding.uniformName == "u_NormalMap") cmd.SetUniformInt("u_HasNormalMap", 1);
                    else if (binding.uniformName == "u_MetallicRoughnessMap") cmd.SetUniformInt("u_HasMetallicRoughnessMap", 1);
                }
                cmd.SetUniformMat4("u_Model", model);
                cmd.SetUniformMat4("u_ViewProjection", viewProj);
                cmd.SetUniformMat4("u_NormalMatrix", normalMatrix);

                cmd.DrawMesh(*object->mesh);
            }
        }

        void ForwardRenderPass::End(CommandBuffer& cmd)
        {
            (void)cmd;
            // Reset wireframe mode to fill
            if (m_wireframe && m_device) {
                m_device->SetPolygonMode(GL_FILL);
            }

            PYRAMID_LOG_DEBUG("ForwardRenderPass::End");
        }

    } // namespace Renderer
} // namespace Pyramid
