// Engine/Graphics/source/OpenGL/Buffer/OpenGLVertexArray.cpp
#include "Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexArray.hpp"
#include "Pyramid/Graphics/Buffer/VertexBuffer.hpp"
#include "Pyramid/Graphics/Buffer/IndexBuffer.hpp"
#include "Pyramid/Graphics/Buffer/BufferLayout.hpp" // Added
#include <Pyramid/Util/Log.hpp>                     // Added for logging
#include <glad/glad.h>
#include <limits>

namespace Pyramid
{

    // Helper function to convert ShaderDataType to OpenGL base type
    static GLenum ShaderDataTypeToOpenGLBaseType(ShaderDataType type)
    {
        switch (type)
        {
        case ShaderDataType::Float:
            return GL_FLOAT;
        case ShaderDataType::Float2:
            return GL_FLOAT;
        case ShaderDataType::Float3:
            return GL_FLOAT;
        case ShaderDataType::Float4:
            return GL_FLOAT;
        case ShaderDataType::Mat3:
            return GL_FLOAT;
        case ShaderDataType::Mat4:
            return GL_FLOAT;
        case ShaderDataType::Int:
            return GL_INT;
        case ShaderDataType::Int2:
            return GL_INT;
        case ShaderDataType::Int3:
            return GL_INT;
        case ShaderDataType::Int4:
            return GL_INT;
        case ShaderDataType::Bool:
            return GL_BOOL;
        default:
            break;
        }
        // PYRAMID_CORE_ASSERT(false, "Unknown ShaderDataType!");
        return 0;
    }

    OpenGLVertexArray::OpenGLVertexArray()
        : m_rendererID(0), m_nextAttributeIndex(0)
    {
        glGenVertexArrays(1, &m_rendererID);
    }

    OpenGLVertexArray::~OpenGLVertexArray()
    {
        if (m_rendererID)
            glDeleteVertexArrays(1, &m_rendererID);
    }

    void OpenGLVertexArray::Bind() const
    {
        glBindVertexArray(m_rendererID);
    }

    void OpenGLVertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    void OpenGLVertexArray::AddVertexBuffer(const std::shared_ptr<IVertexBuffer> &vertexBuffer, const BufferLayout &layout)
    {
        if (layout.GetElements().empty())
        {
            // PYRAMID_CORE_WARN("Vertex Buffer has no layout!");
            return;
        }

        glBindVertexArray(m_rendererID);
        vertexBuffer->Bind();

        u32 attributeIndex = m_nextAttributeIndex; // Use next available attribute index
        for (const auto &element : layout)
        {
            glEnableVertexAttribArray(attributeIndex);
            
            // Handle integer types with glVertexAttribIPointer
            bool isIntegerType = (element.Type == ShaderDataType::Int ||
                                  element.Type == ShaderDataType::Int2 ||
                                  element.Type == ShaderDataType::Int3 ||
                                  element.Type == ShaderDataType::Int4);
            
            // Handle matrix types by decomposing into multiple attributes
            if (element.Type == ShaderDataType::Mat3)
            {
                // Mat3 is 3x vec3
                for (u32 i = 0; i < 3; i++)
                {
                    glEnableVertexAttribArray(attributeIndex);
                    glVertexAttribPointer(
                        attributeIndex,
                        3, // vec3
                        GL_FLOAT,
                        element.Normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        (const void *)(intptr_t)(element.Offset + sizeof(float) * 3 * i)
                    );
                    attributeIndex++;
                }
                continue; // Skip the regular increment
            }
            else if (element.Type == ShaderDataType::Mat4)
            {
                // Mat4 is 4x vec4
                for (u32 i = 0; i < 4; i++)
                {
                    glEnableVertexAttribArray(attributeIndex);
                    glVertexAttribPointer(
                        attributeIndex,
                        4, // vec4
                        GL_FLOAT,
                        element.Normalized ? GL_TRUE : GL_FALSE,
                        layout.GetStride(),
                        (const void *)(intptr_t)(element.Offset + sizeof(float) * 4 * i)
                    );
                    attributeIndex++;
                }
                continue; // Skip the regular increment
            }
            else if (isIntegerType)
            {
                // Use glVertexAttribIPointer for integer types
                glVertexAttribIPointer(
                    attributeIndex,
                    ShaderDataTypeComponentCount(element.Type),
                    ShaderDataTypeToOpenGLBaseType(element.Type),
                    layout.GetStride(),
                    (const void *)(intptr_t)element.Offset
                );
            }
            else
            {
                // Use regular glVertexAttribPointer for float types
                glVertexAttribPointer(
                    attributeIndex,
                    ShaderDataTypeComponentCount(element.Type),
                    ShaderDataTypeToOpenGLBaseType(element.Type),
                    element.Normalized ? GL_TRUE : GL_FALSE,
                    layout.GetStride(),
                    (const void *)(intptr_t)element.Offset
                );
            }
            attributeIndex++;
        }
        m_nextAttributeIndex = attributeIndex; // Update next available index
        vertexBuffer->SetLayout(layout);
        m_vertexBuffers.push_back(vertexBuffer);
    }

    bool OpenGLVertexArray::TryGetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
    {
        const f32 maximum = std::numeric_limits<f32>::max();
        Math::Vec3 combinedMin(maximum);
        Math::Vec3 combinedMax(-maximum);
        bool hasBounds = false;

        for (const auto &vertexBuffer : m_vertexBuffers)
        {
            if (!vertexBuffer)
            {
                continue;
            }

            Math::Vec3 bufferMin;
            Math::Vec3 bufferMax;
            if (!vertexBuffer->TryGetLocalBounds(bufferMin, bufferMax))
            {
                continue;
            }

            combinedMin.x = Math::Min(combinedMin.x, bufferMin.x);
            combinedMin.y = Math::Min(combinedMin.y, bufferMin.y);
            combinedMin.z = Math::Min(combinedMin.z, bufferMin.z);
            combinedMax.x = Math::Max(combinedMax.x, bufferMax.x);
            combinedMax.y = Math::Max(combinedMax.y, bufferMax.y);
            combinedMax.z = Math::Max(combinedMax.z, bufferMax.z);
            hasBounds = true;
        }

        if (!hasBounds)
        {
            return false;
        }

        minPoint = combinedMin;
        maxPoint = combinedMax;
        return true;
    }

    void OpenGLVertexArray::SetIndexBuffer(const std::shared_ptr<IIndexBuffer> &indexBuffer)
    {
        glBindVertexArray(m_rendererID);
        indexBuffer->Bind();
        m_indexBuffer = indexBuffer;
    }

    void OpenGLVertexArray::AddInstanceBuffer(const std::shared_ptr<IInstanceBuffer> &instanceBuffer, const BufferLayout &layout, u32 startingAttributeIndex)
    {
        if (layout.GetElements().empty())
        {
            PYRAMID_LOG_WARN("Instance buffer has no layout!");
            return;
        }

        glBindVertexArray(m_rendererID);
        instanceBuffer->Bind();

        u32 attributeIndex = startingAttributeIndex;
        for (const auto &element : layout)
        {
            glEnableVertexAttribArray(attributeIndex);
            glVertexAttribPointer(
                attributeIndex,
                ShaderDataTypeComponentCount(element.Type),
                ShaderDataTypeToOpenGLBaseType(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                layout.GetStride(),
                (const void *)(intptr_t)element.Offset);

            // Set the attribute divisor to 1 for per-instance data
            glVertexAttribDivisor(attributeIndex, 1);
            attributeIndex++;
        }

        m_instanceBuffer = instanceBuffer;
        PYRAMID_LOG_DEBUG("Added instance buffer with ", layout.GetElements().size(), " attributes starting at index ", startingAttributeIndex);
    }

    void OpenGLVertexArray::RemoveInstanceBuffer()
    {
        if (!m_instanceBuffer)
        {
            PYRAMID_LOG_WARN("No instance buffer to remove");
            return;
        }

        // We would need to track which attributes were used for instancing to disable them properly
        // For now, we'll just clear the instance buffer reference
        m_instanceBuffer.reset();
        PYRAMID_LOG_DEBUG("Removed instance buffer");
    }

} // namespace Pyramid
