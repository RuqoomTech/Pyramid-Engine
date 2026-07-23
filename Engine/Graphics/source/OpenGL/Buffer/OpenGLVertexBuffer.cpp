#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexBuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>

#include <cstring>

namespace Pyramid {

OpenGLVertexBuffer::OpenGLVertexBuffer()
    : m_bufferId(0)
{
    glGenBuffers(1, &m_bufferId);
}

OpenGLVertexBuffer::~OpenGLVertexBuffer()
{
    if (m_bufferId)
        glDeleteBuffers(1, &m_bufferId);
}

void OpenGLVertexBuffer::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_bufferId);
}

void OpenGLVertexBuffer::Unbind()
{
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void OpenGLVertexBuffer::SetData(const void* data, u32 size)
{
    if (m_hasLayout)
    {
        UpdateBounds(data, size);
        m_pendingData.clear();
    }
    else
    {
        m_hasBounds = false;
        m_pendingData.clear();
        if (data && size > 0)
        {
            m_pendingData.resize(size);
            std::memcpy(m_pendingData.data(), data, size);
        }
    }

    Bind();
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

void OpenGLVertexBuffer::SetLayout(const BufferLayout &layout)
{
    m_layout = layout;
    m_hasLayout = !layout.GetElements().empty() && layout.GetStride() > 0;
    m_hasBounds = false;

    if (m_hasLayout && !m_pendingData.empty())
    {
        UpdateBounds(m_pendingData.data(), static_cast<u32>(m_pendingData.size()));
    }
    m_pendingData.clear();
}

bool OpenGLVertexBuffer::TryGetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
{
    if (!m_hasBounds)
    {
        return false;
    }

    minPoint = m_boundsMin;
    maxPoint = m_boundsMax;
    return true;
}

void OpenGLVertexBuffer::UpdateBounds(const void* data, u32 size)
{
    m_hasBounds = m_hasLayout && Geometry::CalculateLocalBounds(
        data,
        size,
        m_layout,
        m_boundsMin,
        m_boundsMax);
}

} // namespace Pyramid
