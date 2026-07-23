#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexArray.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexBuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include <Pyramid/Graphics/Scene.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace
{
    using Pyramid::BufferLayout;
    using Pyramid::Math::Vec3;
    using Pyramid::ShaderDataType;
    using Pyramid::u32;

    bool NearlyEqual(float left, float right, float epsilon = 0.0001f)
    {
        return std::fabs(left - right) <= epsilon;
    }

    bool NearlyEqual(const Vec3& left, const Vec3& right)
    {
        return NearlyEqual(left.x, right.x) &&
               NearlyEqual(left.y, right.y) &&
               NearlyEqual(left.z, right.z);
    }

    int Fail(const char* message)
    {
        std::cerr << "RenderObjectBoundsTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    Pyramid::u32 g_nextBuffer = 1;
    Pyramid::u32 g_nextVertexArray = 100;

    void APIENTRY FakeGenBuffers(GLsizei count, GLuint* buffers)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            buffers[index] = g_nextBuffer++;
        }
    }

    void APIENTRY FakeDeleteBuffers(GLsizei, const GLuint*) {}
    void APIENTRY FakeBindBuffer(GLenum, GLuint) {}
    void APIENTRY FakeBufferData(GLenum, GLsizeiptr, const void*, GLenum) {}

    void APIENTRY FakeGenVertexArrays(GLsizei count, GLuint* arrays)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            arrays[index] = g_nextVertexArray++;
        }
    }

    void APIENTRY FakeDeleteVertexArrays(GLsizei, const GLuint*) {}
    void APIENTRY FakeBindVertexArray(GLuint) {}
    void APIENTRY FakeEnableVertexAttribArray(GLuint) {}
    void APIENTRY FakeVertexAttribPointer(
        GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) {}
    void APIENTRY FakeVertexAttribIPointer(
        GLuint, GLint, GLenum, GLsizei, const void*) {}

    void InstallFakeOpenGL()
    {
        glad_glGenBuffers = FakeGenBuffers;
        glad_glDeleteBuffers = FakeDeleteBuffers;
        glad_glBindBuffer = FakeBindBuffer;
        glad_glBufferData = FakeBufferData;
        glad_glGenVertexArrays = FakeGenVertexArrays;
        glad_glDeleteVertexArrays = FakeDeleteVertexArrays;
        glad_glBindVertexArray = FakeBindVertexArray;
        glad_glEnableVertexAttribArray = FakeEnableVertexAttribArray;
        glad_glVertexAttribPointer = FakeVertexAttribPointer;
        glad_glVertexAttribIPointer = FakeVertexAttribIPointer;
    }

}

int main()
{
    struct Vertex3
    {
        float color[4];
        float position[3];
    };

    const BufferLayout layout3 = {
        {ShaderDataType::Float4, "a_Color"},
        {ShaderDataType::Float3, "a_Position"},
    };
    const std::vector<Vertex3> vertices3 = {
        {{1, 0, 0, 1}, {-2.0f, 4.0f, 1.0f}},
        {{0, 1, 0, 1}, {3.0f, -1.0f, 7.0f}},
        {{0, 0, 1, 1}, {0.5f, 2.0f, -5.0f}},
    };

    Vec3 minPoint;
    Vec3 maxPoint;
    if (!Pyramid::Geometry::CalculateLocalBounds(
            vertices3.data(),
            static_cast<u32>(vertices3.size() * sizeof(Vertex3)),
            layout3,
            minPoint,
            maxPoint))
    {
        return Fail("interleaved Float3 positions were not recognized");
    }
    if (!NearlyEqual(minPoint, Vec3(-2.0f, -1.0f, -5.0f)) ||
        !NearlyEqual(maxPoint, Vec3(3.0f, 4.0f, 7.0f)))
    {
        return Fail("interleaved Float3 bounds are incorrect");
    }

    struct Vertex2
    {
        float position[2];
    };
    const std::vector<Vertex2> vertices2 = {{{-4.0f, 2.0f}}, {{6.0f, -3.0f}}};
    const BufferLayout layout2 = {{ShaderDataType::Float2, "inPosition"}};
    if (!Pyramid::Geometry::CalculateLocalBounds(
            vertices2.data(),
            static_cast<u32>(vertices2.size() * sizeof(Vertex2)),
            layout2,
            minPoint,
            maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-4.0f, -3.0f, 0.0f)) ||
        !NearlyEqual(maxPoint, Vec3(6.0f, 2.0f, 0.0f)))
    {
        return Fail("Float2 position bounds are incorrect");
    }

    struct Vertex4
    {
        float position[4];
    };
    const std::vector<Vertex4> vertices4 = {
        {{-1.0f, 2.0f, 3.0f, 10.0f}},
        {{5.0f, -6.0f, 7.0f, 0.0f}},
    };
    const BufferLayout layout4 = {{ShaderDataType::Float4, "vertex_position"}};
    if (!Pyramid::Geometry::CalculateLocalBounds(
            vertices4.data(),
            static_cast<u32>(vertices4.size() * sizeof(Vertex4)),
            layout4,
            minPoint,
            maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-1.0f, -6.0f, 3.0f)) ||
        !NearlyEqual(maxPoint, Vec3(5.0f, 2.0f, 7.0f)))
    {
        return Fail("Float4 position bounds are incorrect");
    }

    const BufferLayout missingPosition = {{ShaderDataType::Float3, "a_Normal"}};
    if (Pyramid::Geometry::CalculateLocalBounds(
            vertices3.data(),
            static_cast<u32>(vertices3.size() * sizeof(Vertex3)),
            missingPosition,
            minPoint,
            maxPoint))
    {
        return Fail("a layout without a position semantic produced bounds");
    }

    auto invalidVertices = vertices3;
    invalidVertices[1].position[0] = std::numeric_limits<float>::quiet_NaN();
    if (Pyramid::Geometry::CalculateLocalBounds(
            invalidVertices.data(),
            static_cast<u32>(invalidVertices.size() * sizeof(Vertex3)),
            layout3,
            minPoint,
            maxPoint))
    {
        return Fail("non-finite vertex positions were accepted");
    }

    if (Pyramid::Geometry::CalculateLocalBounds(
            vertices3.data(),
            static_cast<u32>(vertices3.size() * sizeof(Vertex3) - 1),
            layout3,
            minPoint,
            maxPoint))
    {
        return Fail("partial vertex data was accepted");
    }

    InstallFakeOpenGL();
    auto vertexBuffer = std::make_shared<Pyramid::OpenGLVertexBuffer>();
    auto vertexArray = std::make_shared<Pyramid::OpenGLVertexArray>();
    vertexBuffer->SetData(
        vertices3.data(),
        static_cast<u32>(vertices3.size() * sizeof(Vertex3)));
    vertexArray->AddVertexBuffer(vertexBuffer, layout3);

    Pyramid::RenderObject object;
    object.vertexArray = vertexArray;

    if (!object.GetLocalBounds(minPoint, maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-2.0f, -1.0f, -5.0f)) ||
        !NearlyEqual(maxPoint, Vec3(3.0f, 4.0f, 7.0f)))
    {
        return Fail("RenderObject did not use automatic geometry bounds");
    }

    object.SetLocalBounds(Vec3(-10.0f), Vec3(10.0f));
    if (object.GetBoundsMode() != Pyramid::RenderBoundsMode::Manual ||
        object.GetLocalBounds(minPoint, maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-10.0f)) ||
        !NearlyEqual(maxPoint, Vec3(10.0f)))
    {
        return Fail("manual bounds did not override geometry bounds");
    }

    object.UseAutomaticBounds();
    if (object.GetBoundsMode() != Pyramid::RenderBoundsMode::Automatic ||
        !object.GetLocalBounds(minPoint, maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-2.0f, -1.0f, -5.0f)))
    {
        return Fail("automatic bounds could not be restored");
    }

    const std::vector<Vertex3> replacementVertices = {
        {{1, 1, 1, 1}, {-8.0f, -7.0f, -6.0f}},
        {{1, 1, 1, 1}, {9.0f, 10.0f, 11.0f}},
    };
    vertexBuffer->SetData(
        replacementVertices.data(),
        static_cast<u32>(replacementVertices.size() * sizeof(Vertex3)));
    if (!object.GetLocalBounds(minPoint, maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-8.0f, -7.0f, -6.0f)) ||
        !NearlyEqual(maxPoint, Vec3(9.0f, 10.0f, 11.0f)))
    {
        return Fail("automatic bounds did not reflect updated vertex data");
    }

    object.position = Vec3(5.0f, 0.0f, 0.0f);
    object.scale = Vec3(2.0f, 1.0f, 0.5f);
    object.GetWorldBounds(minPoint, maxPoint);
    if (!NearlyEqual(minPoint, Vec3(-11.0f, -7.0f, -3.0f)) ||
        !NearlyEqual(maxPoint, Vec3(23.0f, 10.0f, 5.5f)))
    {
        return Fail("world bounds did not use geometry-derived local bounds");
    }

    Pyramid::RenderObject fallback;
    if (fallback.GetLocalBounds(minPoint, maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-0.5f)) ||
        !NearlyEqual(maxPoint, Vec3(0.5f)))
    {
        return Fail("missing geometry did not use the unit-cube fallback");
    }

    return EXIT_SUCCESS;
}
