// Engine/Graphics/source/OpenGL/Buffer/OpenGLIndexBuffer.cpp
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLIndexBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>

namespace Pyramid
{

    OpenGLIndexBuffer::OpenGLIndexBuffer()
        : m_bufferId(0), m_count(0)
    {
        glGenBuffers(1, &m_bufferId);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer()
    {
        if (m_bufferId)
        {
            OpenGLStateManager::GetInstance().ForgetBuffer(m_bufferId);
            glDeleteBuffers(1, &m_bufferId);
        }
    }

    void OpenGLIndexBuffer::Bind()
    {
        OpenGLStateManager::GetInstance().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_bufferId);
    }

    void OpenGLIndexBuffer::Unbind()
    {
        OpenGLStateManager::GetInstance().BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void OpenGLIndexBuffer::SetData(const void *data, u32 count)
    {
        m_count = count;
        // Storage upload must not depend on a bound VAO. Element binding is VAO state.
        OpenGLStateManager::GetInstance().BindBuffer(GL_COPY_WRITE_BUFFER, m_bufferId);
        glBufferData(GL_COPY_WRITE_BUFFER, count * sizeof(u32), data, GL_STATIC_DRAW);
    }

} // namespace Pyramid