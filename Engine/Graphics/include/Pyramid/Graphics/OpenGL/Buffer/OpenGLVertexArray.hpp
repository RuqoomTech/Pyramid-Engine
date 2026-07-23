// Engine/Graphics/include/Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexArray.hpp
#pragma once
#include "Pyramid/Graphics/Buffer/VertexArray.hpp"
#include "Pyramid/Graphics/Buffer/BufferLayout.hpp"   // Added
#include "Pyramid/Graphics/Buffer/InstanceBuffer.hpp" // Added for instance buffer support
#include <glad/glad.h>
#include <vector>

namespace Pyramid
{

    class OpenGLVertexArray : public IVertexArray
    {
    public:
        OpenGLVertexArray();
        ~OpenGLVertexArray() override;

        void Bind() const override;
        void Unbind() const override;
        void AddVertexBuffer(const std::shared_ptr<IVertexBuffer> &vertexBuffer, const BufferLayout &layout) override; // Changed
        void SetIndexBuffer(const std::shared_ptr<IIndexBuffer> &indexBuffer) override;
        const std::shared_ptr<IIndexBuffer> &GetIndexBuffer() const override { return m_indexBuffer; }
        bool TryGetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const override;

        // Instance buffer support
        void AddInstanceBuffer(const std::shared_ptr<IInstanceBuffer> &instanceBuffer, const BufferLayout &layout, u32 startingAttributeIndex) override;
        void RemoveInstanceBuffer() override;
        bool HasInstanceBuffer() const override { return m_instanceBuffer != nullptr; }

    private:
        GLuint m_rendererID;
        std::vector<std::shared_ptr<IVertexBuffer>> m_vertexBuffers;
        std::shared_ptr<IIndexBuffer> m_indexBuffer;
        std::shared_ptr<IInstanceBuffer> m_instanceBuffer;
        u32 m_nextAttributeIndex;
    };

} // namespace Pyramid
