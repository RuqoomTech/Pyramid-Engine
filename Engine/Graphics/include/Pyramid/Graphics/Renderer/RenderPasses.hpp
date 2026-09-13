#pragma once

#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Math/Math.hpp>

namespace Pyramid
{
    namespace Renderer
    {

        /**
         * @brief Forward rendering pass for opaque geometry
         */
        class ForwardRenderPass : public RenderPass
        {
        public:
            explicit ForwardRenderPass(IGraphicsDevice* device);
            ~ForwardRenderPass() override = default;

            void Begin(CommandBuffer& cmd) override;
            void Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera) override;
            void End(CommandBuffer& cmd) override;

            // Configuration
            void SetClearColor(const Math::Vec4& color) { m_clearColor = color; }
            void SetWireframeMode(bool enabled) { m_wireframe = enabled; }

        private:
            IGraphicsDevice* m_device = nullptr;
            Math::Vec4 m_clearColor = Math::Vec4(0.2f, 0.3f, 0.3f, 1.0f);
            bool m_wireframe = false;
        };

        /**
         * @brief Deferred geometry pass (G-Buffer generation)
         */
        class DeferredGeometryPass : public RenderPass
        {
        public:
            DeferredGeometryPass(const std::string& name, IGraphicsDevice* device, u32 width, u32 height);
            ~DeferredGeometryPass() override = default;

            void Begin(CommandBuffer& cmd) override;
            void Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera) override;
            void End(CommandBuffer& cmd) override;

            bool Resize(u32 width, u32 height) override;
            
            std::shared_ptr<Pyramid::OpenGLFramebuffer> GetGBuffer() const { return m_gBuffer; }

            // G-Buffer layout:
            // RT0: Albedo (RGB) + Metallic (A)      - GL_RGBA8
            // RT1: Normal (RGB) + Roughness (A)     - GL_RGBA16F
            // RT2: WorldPos (RGB) + AO (A)          - GL_RGBA16F
            // RT3: Emissive (RGB) + Flags (A)       - GL_RGBA16F
            // Depth: Depth24Stencil8

        private:
            void CreateGBuffer();
            
            IGraphicsDevice* m_device;
            u32 m_width;
            u32 m_height;
            std::shared_ptr<Pyramid::OpenGLFramebuffer> m_gBuffer;
            std::shared_ptr<IShader> m_geometryShader;
        };

        /**
         * @brief Shadow mapping pass with cascaded shadow maps (forward declaration)
         *
         * Defined below. The deferred lighting pass holds a non-owning link to
         * the shadow pass so per-frame matrices and splits stay fresh without
         * routing them through RenderSystem every frame.
         */
        class ShadowMapPass;

        /**
         * @brief Deferred lighting pass
         */
        class DeferredLightingPass : public RenderPass
        {
        public:
            DeferredLightingPass(const std::string& name, IGraphicsDevice* device);
            ~DeferredLightingPass() override = default;

            void Begin(CommandBuffer& cmd) override;
            void Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera) override;
            void End(CommandBuffer& cmd) override;

            void SetGBuffer(std::shared_ptr<Pyramid::OpenGLFramebuffer> gBuffer);
            void SetShadowPass(const ShadowMapPass* shadowPass);
            void SetEnableSSAO(bool enable) { m_enableSSAO = enable; }
            void SetEnableIBL(bool enable) { m_enableIBL = enable; }

        private:
            void CreateFullscreenQuad();

            static constexpr u32 kShadowMapSlot = 5u;

            IGraphicsDevice* m_device;
            std::shared_ptr<Pyramid::OpenGLFramebuffer> m_gBuffer;
            const ShadowMapPass* m_shadowPass = nullptr;
            std::shared_ptr<IShader> m_lightingShader;
            std::shared_ptr<IVertexArray> m_fullscreenQuad;
            bool m_enableSSAO = false;
            bool m_enableIBL = false;
        };

        /**
         * @brief Shadow mapping pass with cascaded shadow maps in a texture array
         *
         * Owns a single GL_TEXTURE_2D_ARRAY depth texture (one layer per
         * cascade) plus one framebuffer object. Every frame each cascade
         * renders directly into its array layer; the deferred lighting pass
         * binds the array once and indexes layers by cascade. Shadow
         * resolution is fixed at construction time and never follows the
         * window size.
         */
        class ShadowMapPass : public RenderPass
        {
        public:
            ShadowMapPass(const std::string& name, IGraphicsDevice* device, u32 cascadeCount = 4);
            ~ShadowMapPass() override;

            void Begin(CommandBuffer& cmd) override;
            void Execute(CommandBuffer& cmd, const Scene& scene, const Camera& camera) override;
            void End(CommandBuffer& cmd) override;

            // Configuration
            void SetCascadeCount(u32 count);
            void SetShadowMapResolution(u32 resolution);
            void SetCascadeSplits(const std::vector<f32>& splits);
            void SetDepthBias(f32 bias);

            // Accessors
            GLuint GetShadowArrayTexture() const { return m_shadowArrayTexture; }
            u32 GetShadowArrayLayers() const { return m_shadowArrayLayers; }
            u32 GetCascadeCount() const { return m_cascadeCount; }
            u32 GetShadowMapResolution() const { return m_shadowMapResolution; }
            const std::vector<Math::Mat4>& GetLightSpaceMatrices() const { return m_lightSpaceMatrices; }
            const std::vector<f32>& GetCascadeSplits() const { return m_cascadeSplits; }

        private:
            void CreateShadowArray();
            void DestroyShadowArray();
            void CalculateCascadeSplits(const Camera& camera);
            Math::Mat4 CalculateLightSpaceMatrix(const Camera& camera, f32 nearPlane, f32 farPlane, const Math::Vec3& lightDir);

            IGraphicsDevice* m_device;
            u32 m_cascadeCount;
            u32 m_shadowMapResolution;
            std::vector<f32> m_cascadeSplits;
            std::vector<Math::Mat4> m_lightSpaceMatrices;
            GLuint m_shadowArrayTexture = 0;
            GLuint m_shadowArrayFBO = 0;
            u32 m_shadowArrayLayers = 0;
            u32 m_shadowArrayResolution = 0;
            f32 m_depthBias;
            std::shared_ptr<IShader> m_shadowShader;
        };

    } // namespace Renderer
} // namespace Pyramid
