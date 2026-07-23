#pragma once
#include "Pyramid/Graphics/GraphicsDevice.hpp"
#include "Pyramid/Platform/Window.hpp"
#include "Pyramid/Graphics/Shader/Shader.hpp"
#include "Pyramid/Graphics/Texture.hpp" // Added for ITexture2D and TextureSpecification
#include <memory>
#include <string> // For std::string in CreateTexture2D

namespace Pyramid
{

    /**
     * @brief OpenGL implementation of the graphics device
     */
    class OpenGLDevice : public IGraphicsDevice
    {
    public:
        explicit OpenGLDevice(Window *window); // Changed
        ~OpenGLDevice() override;

        bool Initialize() override;
        void Shutdown() override;
        void Clear(const Color &color) override;
        void Present(bool vsync) override;
        void DrawIndexed(
            u32 count,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) override;
        void DrawIndexedInstanced(
            u32 indexCount,
            u32 instanceCount,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) override;
        void DrawArrays(
            u32 vertexCount,
            u32 firstVertex = 0,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) override;
        void DrawArraysInstanced(
            u32 vertexCount,
            u32 instanceCount,
            u32 firstVertex = 0,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) override;
        void SetViewport(u32 x, u32 y, u32 width, u32 height) override;

        // Add factory method implementations
        std::shared_ptr<IVertexBuffer> CreateVertexBuffer() override;
        std::shared_ptr<IIndexBuffer> CreateIndexBuffer() override;
        std::shared_ptr<IVertexArray> CreateVertexArray() override;
        std::shared_ptr<IShader> CreateShader() override;
        std::shared_ptr<ITexture2D> CreateTexture2D(const TextureSpecification &specification, const void *data = nullptr) override;    // Added
        std::shared_ptr<ITexture2D> CreateTexture2D(const std::string &filepath, bool srgb = false, bool generateMips = true) override; // Added
        std::shared_ptr<class IUniformBuffer> CreateUniformBuffer(size_t size, BufferUsage usage) override;
        std::shared_ptr<class IInstanceBuffer> CreateInstanceBuffer() override;
        std::shared_ptr<class IShaderStorageBuffer> CreateShaderStorageBuffer() override;

        // State management methods
        void EnableBlend(bool enable) override;
        void SetBlendFunc(u32 sfactor, u32 dfactor) override;
        void EnableDepthTest(bool enable) override;
        void SetDepthFunc(u32 func) override;
        void EnableDepthClamp(bool enable) override;
        void EnableCullFace(bool enable) override;
        void SetCullFace(u32 mode) override;
        void SetClearColor(f32 r, f32 g, f32 b, f32 a) override;
        u32 GetStateChangeCount() const override;
        void ResetStateChangeCount() override;

        // Performance and debugging methods
        std::string GetDeviceInfo() const override;
        bool IsValid() const override;
        std::string GetLastError() const override;
        void SetWireframeMode(bool enable) override;
        void SetPolygonMode(u32 mode) override;
        void BindFramebuffer(class IFramebuffer *framebuffer) override;
        void BindFramebufferHandle(u32 framebufferId) override;
        void BindShader(IShader *shader) override;
        void BindVertexArray(class IVertexArray *vao) override;
        void BindTexture(ITexture2D *texture, u32 slot) override;
        void BindNativeTexture(u32 textureId, u32 slot, u32 target) override;
        void SetTextureBorderColor(u32 textureId, u32 target, f32 r, f32 g, f32 b, f32 a) override;
        void BindUniformBuffer(class IUniformBuffer *buffer, u32 bindingPoint) override;
        void ClearBuffers(u32 clearMask) override;

        /**
         * @brief Get the window associated with this device
         * @return Window* The window instance
         */
        Window *GetWindow() const { return m_window; }

    private:
        Window *m_window;
        mutable std::string m_lastError;
        bool m_initialized;
        mutable std::string m_deviceInfo;
        mutable bool m_deviceInfoCached;
    };

} // namespace Pyramid
