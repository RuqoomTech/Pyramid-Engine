#pragma once
#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/PrimitiveTopology.hpp>
#include <memory>
#include <string> // Added for std::string

// Forward declaration to avoid circular dependency
namespace Pyramid
{
    enum class BufferUsage;
}

namespace Pyramid
{
    // Forward declarations
    class Window;
    class IShader;
    class ITexture2D;            // Added
    struct TextureSpecification; // Added

    /**
     * @brief Interface for graphics device implementations
     */
    class IGraphicsDevice
    {
    public:
        virtual ~IGraphicsDevice() = default;

        /**
         * @brief Initialize the graphics device
         * @return true if successful
         */
        virtual bool Initialize() = 0;

        /**
         * @brief Shutdown the graphics device
         */
        virtual void Shutdown() = 0;

        /**
         * @brief Clear the screen with a specified color
         * @param color The color to clear with
         */
        virtual void Clear(const Color &color) = 0;

        /**
         * @brief Present the rendered frame
         * @param vsync Enable/disable vertical sync
         */
        virtual void Present(bool vsync) = 0;

        /**
         * @brief Draw indexed primitives
         * @param count The number of indices to draw
         */
        virtual void DrawIndexed(
            u32 count,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) = 0;

        /**
         * @brief Draw indexed primitives with instancing
         * @param indexCount The number of indices to draw per instance
         * @param instanceCount The number of instances to draw
         */
        virtual void DrawIndexedInstanced(
            u32 indexCount,
            u32 instanceCount,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) = 0;

        /**
         * @brief Draw arrays with instancing
         * @param vertexCount The number of vertices to draw per instance
         * @param instanceCount The number of instances to draw
         * @param firstVertex The first vertex to start drawing from
         */
        virtual void DrawArrays(
            u32 vertexCount,
            u32 firstVertex = 0,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) = 0;

        virtual void DrawArraysInstanced(
            u32 vertexCount,
            u32 instanceCount,
            u32 firstVertex = 0,
            PrimitiveTopology topology = PrimitiveTopology::Triangles) = 0;

        /**
         * @brief Set the viewport dimensions
         * @param x The x-coordinate of the viewport
         * @param y The y-coordinate of the viewport
         * @param width The width of the viewport
         * @param height The height of the viewport
         */
        virtual void SetViewport(u32 x, u32 y, u32 width, u32 height) = 0;

        /**
         * @brief Create a vertex buffer
         * @return std::shared_ptr<IVertexBuffer> The created vertex buffer
         */
        virtual std::shared_ptr<class IVertexBuffer> CreateVertexBuffer() = 0;

        /**
         * @brief Create an index buffer
         * @return std::shared_ptr<IIndexBuffer> The created index buffer
         */
        virtual std::shared_ptr<class IIndexBuffer> CreateIndexBuffer() = 0;

        /**
         * @brief Create a vertex array
         * @return std::shared_ptr<IVertexArray> The created vertex array
         */
        virtual std::shared_ptr<class IVertexArray> CreateVertexArray() = 0;

        /**
         * @brief Create a shader program
         * @return std::shared_ptr<IShader> The created shader program
         */
        virtual std::shared_ptr<IShader> CreateShader() = 0;

        /**
         * @brief Create a 2D Texture from specification and data
         * @param specification The texture specification
         * @param data Optional raw pixel data
         * @return std::shared_ptr<ITexture2D> The created texture
         */
        virtual std::shared_ptr<ITexture2D> CreateTexture2D(const TextureSpecification &specification, const void *data = nullptr) = 0; // Added

        /**
         * @brief Create a 2D Texture from a filepath
         * @param filepath Path to the image file
         * @param srgb Whether the texture should be treated as sRGB
         * @param generateMips Whether to generate mipmaps
         * @return std::shared_ptr<ITexture2D> The created texture
         */
        virtual std::shared_ptr<ITexture2D> CreateTexture2D(const std::string &filepath, bool srgb = false, bool generateMips = true) = 0; // Added

        /**
         * @brief Create a uniform buffer object
         * @param size Size of the buffer in bytes
         * @param usage Buffer usage pattern (static, dynamic, stream)
         * @return std::shared_ptr<IUniformBuffer> The created uniform buffer, or nullptr on failure
         */
        virtual std::shared_ptr<class IUniformBuffer> CreateUniformBuffer(size_t size, BufferUsage usage) = 0;

        /**
         * @brief Create a uniform buffer object with dynamic usage (convenience overload)
         * @param size Size of the buffer in bytes
         * @return std::shared_ptr<IUniformBuffer> The created uniform buffer, or nullptr on failure
         */
        std::shared_ptr<class IUniformBuffer> CreateUniformBuffer(size_t size);

        /**
         * @brief Create an instance buffer object for instanced rendering
         * @return std::shared_ptr<IInstanceBuffer> The created instance buffer, or nullptr on failure
         */
        virtual std::shared_ptr<class IInstanceBuffer> CreateInstanceBuffer() = 0;

        /**
         * @brief Create a shader storage buffer object for compute shaders
         * @return std::shared_ptr<IShaderStorageBuffer> The created SSBO, or nullptr on failure
         */
        virtual std::shared_ptr<class IShaderStorageBuffer> CreateShaderStorageBuffer() = 0;

        // State management methods
        /**
         * @brief Enable or disable blending
         * @param enable True to enable blending, false to disable
         */
        virtual void EnableBlend(bool enable) = 0;

        /**
         * @brief Set blend function
         * @param sfactor Source factor
         * @param dfactor Destination factor
         */
        virtual void SetBlendFunc(u32 sfactor, u32 dfactor) = 0;

        /**
         * @brief Enable or disable depth testing
         * @param enable True to enable depth testing, false to disable
         */
        virtual void EnableDepthTest(bool enable) = 0;

        /**
         * @brief Set depth function
         * @param func Depth comparison function
         */
        virtual void SetDepthFunc(u32 func) = 0;
        /**
         * @brief Enable or disable depth clamping
         * @param enable True to enable depth clamping, false to disable
         */
        virtual void EnableDepthClamp(bool enable) = 0;

        /**
         * @brief Enable or disable face culling
         * @param enable True to enable culling, false to disable
         */
        virtual void EnableCullFace(bool enable) = 0;

        /**
         * @brief Set which faces to cull
         * @param mode Culling mode (front, back, front and back)
         */
        virtual void SetCullFace(u32 mode) = 0;

        /**
         * @brief Set clear color
         * @param r Red component (0.0 to 1.0)
         * @param g Green component (0.0 to 1.0)
         * @param b Blue component (0.0 to 1.0)
         * @param a Alpha component (0.0 to 1.0)
         */
        virtual void SetClearColor(f32 r, f32 g, f32 b, f32 a) = 0;

        /**
         * @brief Get the number of state changes since last reset
         * @return Number of state changes
         */
        virtual u32 GetStateChangeCount() const = 0;

        /**
         * @brief Reset the state change counter
         */
        virtual void ResetStateChangeCount() = 0;

        // Performance and debugging methods
        /**
         * @brief Get graphics device information
         * @return String containing device info (vendor, renderer, version)
         */
        virtual std::string GetDeviceInfo() const = 0;

        /**
         * @brief Check if the graphics device is valid and ready
         * @return true if device is ready for rendering
         */
        virtual bool IsValid() const = 0;

        /**
         * @brief Get the last error message (if any)
         * @return Error message string, empty if no error
         */
        virtual std::string GetLastError() const = 0;

        /**
         * @brief Enable or disable wireframe mode
         * @param enable True to enable wireframe, false for solid
         */
        virtual void SetWireframeMode(bool enable) = 0;

        /**
         * @brief Set polygon mode
         * @param mode Polygon mode (fill, line, point)
         */
        virtual void SetPolygonMode(u32 mode) = 0;

        /**
         * @brief Bind a framebuffer for rendering
         * @param framebuffer Framebuffer to bind (nullptr for default)
         */
        virtual void BindFramebuffer(class IFramebuffer *framebuffer) = 0;
        /**
         * @brief Bind a native framebuffer handle
         * @param framebufferId OpenGL framebuffer ID (0 for default)
         */
        virtual void BindFramebufferHandle(u32 framebufferId) = 0;

        /**
         * @brief Bind a shader for rendering
         * @param shader Shader to bind (nullptr to unbind)
         */
        virtual void BindShader(IShader *shader) = 0;

        /**
         * @brief Bind a vertex array object
         * @param vao Vertex array to bind (nullptr to unbind)
         */
        virtual void BindVertexArray(class IVertexArray *vao) = 0;

        /**
         * @brief Bind a texture to a specific slot
         * @param texture Texture to bind (nullptr to unbind)
         * @param slot Texture slot (0-31 typically)
         */
        virtual void BindTexture(ITexture2D *texture, u32 slot) = 0;
        /**
         * @brief Bind a native texture handle
         * @param textureId API-native texture ID
         * @param slot Texture unit slot
         * @param target API-native texture target (e.g. GL_TEXTURE_2D)
         */
        virtual void BindNativeTexture(u32 textureId, u32 slot, u32 target) = 0;
        /**
         * @brief Set texture border color for a native texture
         * @param textureId API-native texture ID
         * @param target API-native texture target
         * @param r Red channel
         * @param g Green channel
         * @param b Blue channel
         * @param a Alpha channel
         */
        virtual void SetTextureBorderColor(u32 textureId, u32 target, f32 r, f32 g, f32 b, f32 a) = 0;

        /**
         * @brief Bind a uniform buffer to a binding point
         * @param buffer Uniform buffer to bind (nullptr to unbind)
         * @param bindingPoint Binding point index
         */
        virtual void BindUniformBuffer(class IUniformBuffer *buffer, u32 bindingPoint) = 0;
        /**
         * @brief Clear selected framebuffer attachments
         * @param clearMask API-native clear bit mask
         */
        virtual void ClearBuffers(u32 clearMask) = 0;

        /**
         * @brief Create a graphics device for the specified API
         * @param api The graphics API to use
         * @param window The window to associate with the graphics device
         * @return std::unique_ptr<IGraphicsDevice> The created graphics device
         */
        static std::unique_ptr<IGraphicsDevice> Create(GraphicsAPI api, Window *window);
    };

} // namespace Pyramid
