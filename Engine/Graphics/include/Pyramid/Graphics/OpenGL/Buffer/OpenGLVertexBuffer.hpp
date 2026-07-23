#pragma once
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <glad/glad.h>
#include <vector>

namespace Pyramid {

/**
 * @brief OpenGL implementation of vertex buffer
 */
class OpenGLVertexBuffer : public IVertexBuffer {
public:
    OpenGLVertexBuffer();
    virtual ~OpenGLVertexBuffer();

    void Bind() override;
    void Unbind() override;
    void SetData(const void* data, u32 size) override;
    void SetLayout(const BufferLayout &layout) override;
    bool TryGetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const override;

private:
    void UpdateBounds(const void* data, u32 size);

    GLuint m_bufferId;
    BufferLayout m_layout;
    std::vector<u8> m_pendingData;
    Math::Vec3 m_boundsMin = Math::Vec3::Zero;
    Math::Vec3 m_boundsMax = Math::Vec3::Zero;
    bool m_hasLayout = false;
    bool m_hasBounds = false;
};

} // namespace Pyramid
