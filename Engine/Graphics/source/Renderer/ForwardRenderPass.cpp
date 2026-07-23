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

                // Bind shader if available
                if (object->material.shader)
                {
                    // Calculate matrices
                    Math::Mat4 model = object->GetTransformMatrix();
                    Math::Mat4 viewProj = camera.GetViewProjectionMatrix();

                    // Set per-object uniforms
                    object->material.shader->SetUniformMat4("u_Model", model.m);
                    object->material.shader->SetUniformMat4("u_ViewProjection", viewProj.m);

                    // Calculate normal matrix (inverse transpose of upper-left 3x3)
                    Math::Mat4 normalMatrix = model.Inverse().Transpose();
                    object->material.shader->SetUniformMat4("u_NormalMatrix", normalMatrix.m);

                    // Set material albedo color
                    object->material.shader->SetUniformFloat4("u_AlbedoColor",
                        object->material.albedo.x,
                        object->material.albedo.y,
                        object->material.albedo.z,
                        object->material.albedo.w);

                    cmd.SetShader(object->material.shader.get());

                    // Bind albedo texture if available
                    if (object->material.albedoTexture)
                    {
                        cmd.SetTexture(object->material.albedoTexture.get(), 0);
                        object->material.shader->SetUniformInt("u_AlbedoMap", 0);
                        object->material.shader->SetUniformInt("u_HasAlbedoMap", 1);
                    }
                    else
                    {
                        cmd.SetTexture(static_cast<ITexture2D*>(nullptr), 0);
                        object->material.shader->SetUniformInt("u_HasAlbedoMap", 0);
                    }

                    // Bind normal texture if available
                    if (object->material.normalTexture)
                    {
                        cmd.SetTexture(object->material.normalTexture.get(), 1);
                        object->material.shader->SetUniformInt("u_NormalMap", 1);
                    }
                    else
                    {
                        cmd.SetTexture(static_cast<ITexture2D*>(nullptr), 1);
                    }

                    // Bind metallic-roughness texture if available
                    if (object->material.metallicRoughnessTexture)
                    {
                        cmd.SetTexture(object->material.metallicRoughnessTexture.get(), 2);
                        object->material.shader->SetUniformInt("u_MetallicRoughnessMap", 2);
                    }
                    else
                    {
                        cmd.SetTexture(static_cast<ITexture2D*>(nullptr), 2);
                    }
                }
                else
                {
                    PYRAMID_LOG_WARN("Object '", object->name, "' has no shader, skipping");
                    continue;
                }

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
