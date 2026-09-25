#pragma once

#include <Pyramid/Graphics/Buffer/IndexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>

#include <cstring>
#include <functional>
#include <memory>
#include <vector>

namespace Pyramid::Tests
{
    class TestVertexBuffer final : public IVertexBuffer
    {
    public:
        void Bind() override {}
        void Unbind() override {}

        void SetData(const void* data, u32 size) override
        {
            m_data.resize(size);
            if (data && size > 0)
            {
                std::memcpy(m_data.data(), data, size);
            }
            RecalculateBounds();
        }

        void SetLayout(const BufferLayout& layout) override
        {
            m_layout = layout;
            RecalculateBounds();
        }

        bool TryGetLocalBounds(Math::Vec3& minPoint, Math::Vec3& maxPoint) const override
        {
            if (!m_hasBounds)
            {
                return false;
            }

            minPoint = m_minPoint;
            maxPoint = m_maxPoint;
            return true;
        }

    private:
        void RecalculateBounds()
        {
            m_hasBounds = Geometry::CalculateLocalBounds(
                m_data.data(),
                static_cast<u32>(m_data.size()),
                m_layout,
                m_minPoint,
                m_maxPoint);
        }

        std::vector<u8> m_data;
        BufferLayout m_layout;
        Math::Vec3 m_minPoint;
        Math::Vec3 m_maxPoint;
        bool m_hasBounds = false;
    };

    class TestIndexBuffer final : public IIndexBuffer
    {
    public:
        void Bind() override {}
        void Unbind() override {}

        void SetData(const void* data, u32 count) override
        {
            m_indices.resize(count);
            if (data && count > 0)
            {
                std::memcpy(m_indices.data(), data, count * sizeof(u32));
            }
        }

        u32 GetCount() const override { return static_cast<u32>(m_indices.size()); }

    private:
        std::vector<u32> m_indices;
    };

    class TestVertexArray final : public IVertexArray
    {
    public:
        void Bind() const override {}
        void Unbind() const override {}

        void AddVertexBuffer(
            const std::shared_ptr<IVertexBuffer>& vertexBuffer,
            const BufferLayout& layout) override
        {
            if (vertexBuffer)
            {
                vertexBuffer->SetLayout(layout);
                m_vertexBuffers.push_back(vertexBuffer);
            }
        }

        void SetIndexBuffer(const std::shared_ptr<IIndexBuffer>& indexBuffer) override
        {
            m_indexBuffer = indexBuffer;
        }

        const std::shared_ptr<IIndexBuffer>& GetIndexBuffer() const override
        {
            return m_indexBuffer;
        }

        bool TryGetLocalBounds(Math::Vec3& minPoint, Math::Vec3& maxPoint) const override
        {
            bool found = false;
            for (const auto& buffer : m_vertexBuffers)
            {
                Math::Vec3 bufferMin;
                Math::Vec3 bufferMax;
                if (!buffer || !buffer->TryGetLocalBounds(bufferMin, bufferMax))
                {
                    continue;
                }

                if (!found)
                {
                    minPoint = bufferMin;
                    maxPoint = bufferMax;
                    found = true;
                }
                else
                {
                    minPoint.x = Math::Min(minPoint.x, bufferMin.x);
                    minPoint.y = Math::Min(minPoint.y, bufferMin.y);
                    minPoint.z = Math::Min(minPoint.z, bufferMin.z);
                    maxPoint.x = Math::Max(maxPoint.x, bufferMax.x);
                    maxPoint.y = Math::Max(maxPoint.y, bufferMax.y);
                    maxPoint.z = Math::Max(maxPoint.z, bufferMax.z);
                }
            }
            return found;
        }

        void AddInstanceBuffer(
            const std::shared_ptr<IInstanceBuffer>&,
            const BufferLayout&,
            u32) override
        {
            m_hasInstanceBuffer = true;
        }

        void RemoveInstanceBuffer() override { m_hasInstanceBuffer = false; }
        bool HasInstanceBuffer() const override { return m_hasInstanceBuffer; }

    private:
        std::vector<std::shared_ptr<IVertexBuffer>> m_vertexBuffers;
        std::shared_ptr<IIndexBuffer> m_indexBuffer;
        bool m_hasInstanceBuffer = false;
    };

    /**
     * @brief Minimal recording shader.
     *
     * Render passes compile their shaders in their constructors, so a harness
     * that drives a pass needs a non-null IShader. Uniform locations are
     * reported as found so presence-only assertions stay meaningful.
     */
    class TestShader final : public IShader
    {
    public:
        void Bind() override { ++bindCalls; }
        void Unbind() override { ++unbindCalls; }
        bool Compile(const std::string&, const std::string&) override
        {
            ++compileCalls;
            return true;
        }
        bool CompileWithGeometry(
            const std::string&, const std::string&, const std::string&) override
        {
            ++compileCalls;
            return true;
        }
        bool CompileWithTessellation(
            const std::string&, const std::string&, const std::string&,
            const std::string&) override
        {
            ++compileCalls;
            return true;
        }
        bool CompileAdvanced(
            const std::string&, const std::string&, const std::string&,
            const std::string&, const std::string&) override
        {
            ++compileCalls;
            return true;
        }
        void SetUniformInt(const std::string&, int) override {}
        void SetUniformFloat(const std::string&, float) override {}
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string&, const float*, bool, int) override {}
        void BindUniformBuffer(const std::string&, IUniformBuffer*, u32) override {}
        void SetUniformBlockBinding(const std::string&, u32) override {}
        void BindShaderStorageBuffer(const std::string&, IShaderStorageBuffer*, u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, u32) override {}

        u32 bindCalls = 0;
        u32 unbindCalls = 0;
        u32 compileCalls = 0;
    };

    /**
     * @brief CPU-only uniform buffer.
     *
     * RenderSystem::Initialize refuses to start without uniform buffers, so a
     * harness that drives the render system needs a working IUniformBuffer.
     */
    class TestUniformBuffer final : public IUniformBuffer
    {
    public:
        bool Initialize(size_t size, BufferUsage usage = BufferUsage::Dynamic) override
        {
            m_size = size;
            m_usage = usage;
            m_initialized = true;
            return true;
        }
        void UpdateData(const void* data, size_t size, size_t offset = 0) override
        {
            ++updateCalls;
            if (data && size > 0)
            {
                m_storage.resize(offset + size);
                std::memcpy(m_storage.data() + offset, data, size);
            }
        }
        void Bind(u32 bindingPoint) override { lastBindingPoint = bindingPoint; }
        void Unbind() override { lastBindingPoint = 0xFFFFFFFFu; }
        size_t GetSize() const override { return m_size; }
        BufferUsage GetUsage() const override { return m_usage; }
        void* Map(BufferAccess = BufferAccess::WriteOnly) override
        {
            m_mapped = true;
            return m_storage.data();
        }
        void Unmap() override { m_mapped = false; }
        bool IsMapped() const override { return m_mapped; }

        u32 updateCalls = 0;
        u32 lastBindingPoint = 0xFFFFFFFFu;

    private:
        std::vector<u8> m_storage;
        size_t m_size = 0;
        BufferUsage m_usage = BufferUsage::Dynamic;
        bool m_initialized = false;
        bool m_mapped = false;
    };

    class TestGraphicsDevice final : public IGraphicsDevice
    {
    public:
        struct NativeTextureBind
        {
            u32 textureId = 0;
            u32 slot = 0;
            u32 target = 0;
        };

        bool Initialize() override { return true; }
        void Shutdown() override {}
        void Clear(const Color&) override {}
        void Present(bool) override {}

        void DrawIndexed(u32 count, PrimitiveTopology topology) override
        {
            RecordDraw(true, count, 1, 0, topology);
        }

        void DrawIndexedInstanced(
            u32 indexCount,
            u32 instanceCount,
            PrimitiveTopology topology) override
        {
            RecordDraw(true, indexCount, instanceCount, 0, topology);
        }

        void DrawArrays(u32 vertexCount, u32 firstVertex, PrimitiveTopology topology) override
        {
            RecordDraw(false, vertexCount, 1, firstVertex, topology);
        }

        void DrawArraysInstanced(
            u32 vertexCount,
            u32 instanceCount,
            u32 firstVertex,
            PrimitiveTopology topology) override
        {
            RecordDraw(false, vertexCount, instanceCount, firstVertex, topology);
        }

        void SetViewport(u32 x, u32 y, u32 width, u32 height) override
        {
            viewportX = x;
            viewportY = y;
            viewportWidth = width;
            viewportHeight = height;
            ++viewportChanges;
        }

        std::shared_ptr<IVertexBuffer> CreateVertexBuffer() override
        {
            ++vertexBufferCreations;
            if (failVertexBufferCreationAt != 0 &&
                vertexBufferCreations == failVertexBufferCreationAt)
            {
                return nullptr;
            }
            return std::make_shared<TestVertexBuffer>();
        }

        std::shared_ptr<IIndexBuffer> CreateIndexBuffer() override
        {
            ++indexBufferCreations;
            return std::make_shared<TestIndexBuffer>();
        }

        std::shared_ptr<IVertexArray> CreateVertexArray() override
        {
            ++vertexArrayCreations;
            return std::make_shared<TestVertexArray>();
        }

        std::shared_ptr<IShader> CreateShader() override
        {
            ++shaderCreations;
            return shaderFactory ? shaderFactory() : nullptr;
        }
        std::shared_ptr<ITexture2D> CreateTexture2D(
            const TextureSpecification& specification,
            const void* data) override
        {
            ++textureCreations;
            return textureFactory ? textureFactory(specification, data) : nullptr;
        }
        std::shared_ptr<ITexture2D> CreateTexture2D(
            const std::string& filepath,
            bool srgb,
            bool generateMips) override
        {
            ++textureFileCreations;
            return textureFileFactory
                ? textureFileFactory(filepath, srgb, generateMips)
                : nullptr;
        }
        std::shared_ptr<IUniformBuffer> CreateUniformBuffer(size_t size, BufferUsage usage) override
        {
            ++uniformBufferCreations;
            auto buffer = std::make_shared<TestUniformBuffer>();
            buffer->Initialize(size, usage);
            return buffer;
        }
        std::shared_ptr<IInstanceBuffer> CreateInstanceBuffer() override { return nullptr; }
        std::shared_ptr<IShaderStorageBuffer> CreateShaderStorageBuffer() override { return nullptr; }

        void EnableBlend(bool enable) override { blendEnabled = enable; }
        void EnableScissorTest(bool enable) override { scissorEnabled = enable; }
        void SetScissorRect(u32 x, u32 y, u32 width, u32 height) override
        {
            scissorX = x; scissorY = y; scissorWidth = width; scissorHeight = height;
        }
        void SetBlendFunc(u32 source, u32 destination) override
        {
            blendSource = source;
            blendDestination = destination;
        }
        void EnableDepthTest(bool enable) override { depthTestEnabled = enable; }
        void SetDepthFunc(u32 function) override { depthFunction = function; }
        void EnableDepthClamp(bool) override {}
        void EnableCullFace(bool enable) override { cullFaceEnabled = enable; }
        void SetCullFace(u32 mode) override { cullFaceMode = mode; }
        void SetClearColor(f32, f32, f32, f32) override {}
        u32 GetStateChangeCount() const override { return 0; }
        void ResetStateChangeCount() override {}
        std::string GetDeviceInfo() const override { return "TestGraphicsDevice"; }
        bool IsValid() const override { return true; }
        std::string GetLastError() const override { return {}; }
        void SetWireframeMode(bool enable) override { wireframeEnabled = enable; }
        void SetPolygonMode(u32 mode) override { polygonMode = mode; }
        void BindFramebuffer(IFramebuffer* framebuffer) override
        {
            // The neutral bind is the only bind passes and the renderer may
            // use, so it must be observable rather than a silent no-op. Record
            // the same state BindFramebufferHandle produced so restore
            // assertions stay meaningful after the migration.
            ++neutralFramebufferBinds;
            lastNeutralFramebuffer = framebuffer;
            boundFramebufferHandle = framebuffer ? framebuffer->GetNativeHandle() : 0;
            if (framebuffer)
            {
                framebuffer->Bind();
            }
        }
        void BindFramebufferHandle(u32 handle) override { boundFramebufferHandle = handle; }
        void BindShader(IShader* shader) override
        {
            boundShader = shader;
            if (shader) shader->Bind();
        }
        void BindVertexArray(IVertexArray* vertexArray) override { boundVertexArray = vertexArray; }
        void BindTexture(ITexture2D* texture, u32 slot) override
        {
            if (slot >= boundTextures.size()) boundTextures.resize(slot + 1, nullptr);
            boundTextures[slot] = texture;
            if (texture) texture->Bind(slot);
        }
        void BindNativeTexture(u32 textureId, u32 slot, u32 target) override
        {
            nativeTextureBinds.push_back(NativeTextureBind{textureId, slot, target});
        }
        void SetTextureBorderColor(u32, u32, f32, f32, f32, f32) override {}
        void BindUniformBuffer(IUniformBuffer*, u32) override {}
        void ClearBuffers(u32) override {}

        void ResetDrawState()
        {
            drawCalls = 0;
            lastDrawIndexed = false;
            lastDrawCount = 0;
            lastInstanceCount = 0;
            lastFirstVertex = 0;
            lastTopology = PrimitiveTopology::Triangles;
            boundVertexArray = nullptr;
        }

        u32 failVertexBufferCreationAt = 0;

        u32 viewportX = 0;
        u32 viewportY = 0;
        u32 viewportWidth = 0;
        u32 viewportHeight = 0;
        u32 viewportChanges = 0;
        u32 boundFramebufferHandle = 0;
        u32 neutralFramebufferBinds = 0;
        IFramebuffer* lastNeutralFramebuffer = nullptr;
        u32 drawCalls = 0;
        bool lastDrawIndexed = false;
        u32 lastDrawCount = 0;
        u32 lastInstanceCount = 0;
        u32 lastFirstVertex = 0;
        PrimitiveTopology lastTopology = PrimitiveTopology::Triangles;
        IVertexArray* boundVertexArray = nullptr;
        bool blendEnabled = false;
        bool scissorEnabled = false;
        u32 scissorX = 0;
        u32 scissorY = 0;
        u32 scissorWidth = 0;
        u32 scissorHeight = 0;
        u32 blendSource = 0;
        u32 blendDestination = 0;
        bool depthTestEnabled = false;
        u32 depthFunction = 0;
        bool cullFaceEnabled = false;
        u32 cullFaceMode = 0;
        bool wireframeEnabled = false;
        u32 polygonMode = 0;
        IShader* boundShader = nullptr;
        std::vector<ITexture2D*> boundTextures;
        std::vector<NativeTextureBind> nativeTextureBinds;
        u32 vertexBufferCreations = 0;
        u32 indexBufferCreations = 0;
        u32 vertexArrayCreations = 0;
        u32 shaderCreations = 0;
        u32 textureCreations = 0;
        u32 textureFileCreations = 0;
        u32 uniformBufferCreations = 0;
        std::function<std::shared_ptr<IShader>()> shaderFactory;
        std::function<std::shared_ptr<ITexture2D>(const TextureSpecification&, const void*)>
            textureFactory;
        std::function<std::shared_ptr<ITexture2D>(const std::string&, bool, bool)>
            textureFileFactory;

    private:
        void RecordDraw(
            bool indexed,
            u32 count,
            u32 instanceCount,
            u32 firstVertex,
            PrimitiveTopology topology)
        {
            ++drawCalls;
            lastDrawIndexed = indexed;
            lastDrawCount = count;
            lastInstanceCount = instanceCount;
            lastFirstVertex = firstVertex;
            lastTopology = topology;
        }
    };
}
