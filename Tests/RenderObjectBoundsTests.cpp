#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include "TestGraphicsDevice.hpp"
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

    Pyramid::Tests::TestGraphicsDevice device;
    Pyramid::MeshSpecification meshSpecification;
    meshSpecification.vertexData = vertices3.data();
    meshSpecification.vertexDataSize =
        static_cast<u32>(vertices3.size() * sizeof(Vertex3));
    meshSpecification.vertexCount = static_cast<u32>(vertices3.size());
    meshSpecification.layout = layout3;
    meshSpecification.topology = Pyramid::PrimitiveTopology::Triangles;
    meshSpecification.name = "RenderObjectBoundsMesh";

    auto mesh = Pyramid::Mesh::Create(device, meshSpecification);
    if (!mesh)
    {
        return Fail("mesh creation failed");
    }

    Pyramid::RenderObject object;
    object.mesh = mesh;

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

    object.position = Vec3(5.0f, 0.0f, 0.0f);
    object.scale = Vec3(2.0f, 1.0f, 0.5f);
    object.GetWorldBounds(minPoint, maxPoint);
    if (!NearlyEqual(minPoint, Vec3(1.0f, -1.0f, -2.5f)) ||
        !NearlyEqual(maxPoint, Vec3(11.0f, 4.0f, 3.5f)))
    {
        return Fail("world bounds did not use immutable mesh bounds");
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
