#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLInstanceBuffer.hpp>
#include <Pyramid/Util/Log.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>

namespace Pyramid
{

    OpenGLInstanceBuffer::OpenGLInstanceBuffer()
        : m_bufferId(0), m_instanceCount(0), m_instanceSize(0), m_bufferSize(0)
    {
        glGenBuffers(1, &m_bufferId);
        PYRAMID_LOG_DEBUG("Created OpenGL instance buffer with ID: ", m_bufferId);
    }

    OpenGLInstanceBuffer::~OpenGLInstanceBuffer()
    {
        if (m_bufferId)
        {
            OpenGLStateManager::GetInstance().ForgetBuffer(m_bufferId);
            glDeleteBuffers(1, &m_bufferId);
            PYRAMID_LOG_DEBUG("Deleted OpenGL instance buffer with ID: ", m_bufferId);
        }
    }

    void OpenGLInstanceBuffer::Bind()
    {
        OpenGLStateManager::GetInstance().BindBuffer(GL_ARRAY_BUFFER, m_bufferId);
    }

    void OpenGLInstanceBuffer::Unbind()
    {
        OpenGLStateManager::GetInstance().BindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void OpenGLInstanceBuffer::SetData(const void* data, u32 size, u32 instanceCount)
    {
        if (instanceCount == 0)
        {
            PYRAMID_LOG_ERROR("Instance count cannot be zero");
            return;
        }

        m_instanceCount = instanceCount;
        m_instanceSize = size / instanceCount;
        m_bufferSize = size;

        Bind();
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW);
        Unbind();

        PYRAMID_LOG_DEBUG("Instance buffer data set: ", instanceCount, " instances, ", 
                         m_instanceSize, " bytes per instance, ", size, " total bytes");
    }

    void OpenGLInstanceBuffer::UpdateData(const void* data, u32 size, u32 offset)
    {
        if (offset + size > m_bufferSize)
        {
            PYRAMID_LOG_ERROR("Update data exceeds buffer size. Offset: ", offset, 
                             ", Size: ", size, ", Buffer size: ", m_bufferSize);
            return;
        }

        Bind();
        glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
        Unbind();

        PYRAMID_LOG_DEBUG("Instance buffer updated: ", size, " bytes at offset ", offset);
    }

} // namespace Pyramid
