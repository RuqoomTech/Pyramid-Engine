#pragma once

#include <Pyramid/Graphics/Buffer/IndexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>

#include <cstring>
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

    class TestGraphicsDevice final : public IGraphicsDevice
    {
    public:
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

        void SetViewport(u32, u32, u32, u32) override {}

        std::shared_ptr<IVertexBuffer> CreateVertexBuffer() override
        {
            ++vertexBufferCreations;
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

        std::shared_ptr<IShader> CreateShader() override { return nullptr; }
        std::shared_ptr<ITexture2D> CreateTexture2D(const TextureSpecification&, const void*) override
        {
            return nullptr;
        }
        std::shared_ptr<ITexture2D> CreateTexture2D(const std::string&, bool, bool) override
        {
            return nullptr;
        }
        std::shared_ptr<IUniformBuffer> CreateUniformBuffer(size_t, BufferUsage) override
        {
            return nullptr;
        }
        std::shared_ptr<IInstanceBuffer> CreateInstanceBuffer() override { return nullptr; }
        std::shared_ptr<IShaderStorageBuffer> CreateShaderStorageBuffer() override { return nullptr; }

        void EnableBlend(bool) override {}
        void SetBlendFunc(u32, u32) override {}
        void EnableDepthTest(bool) override {}
        void SetDepthFunc(u32) override {}
        void EnableDepthClamp(bool) override {}
        void EnableCullFace(bool) override {}
        void SetCullFace(u32) override {}
        void SetClearColor(f32, f32, f32, f32) override {}
        u32 GetStateChangeCount() const override { return 0; }
        void ResetStateChangeCount() override {}
        std::string GetDeviceInfo() const override { return "TestGraphicsDevice"; }
        bool IsValid() const override { return true; }
        std::string GetLastError() const override { return {}; }
        void SetWireframeMode(bool) override {}
        void SetPolygonMode(u32) override {}
        void BindFramebuffer(IFramebuffer*) override {}
        void BindFramebufferHandle(u32) override {}
        void BindShader(IShader*) override {}
        void BindVertexArray(IVertexArray* vertexArray) override { boundVertexArray = vertexArray; }
        void BindTexture(ITexture2D*, u32) override {}
        void BindNativeTexture(u32, u32, u32) override {}
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

        u32 drawCalls = 0;
        bool lastDrawIndexed = false;
        u32 lastDrawCount = 0;
        u32 lastInstanceCount = 0;
        u32 lastFirstVertex = 0;
        PrimitiveTopology lastTopology = PrimitiveTopology::Triangles;
        IVertexArray* boundVertexArray = nullptr;
        u32 vertexBufferCreations = 0;
        u32 indexBufferCreations = 0;
        u32 vertexArrayCreations = 0;

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
