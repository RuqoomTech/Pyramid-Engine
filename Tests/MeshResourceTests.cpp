#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Util/Log.hpp>

#include "TestGraphicsDevice.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    using Pyramid::BufferLayout;
    using Pyramid::Mesh;
    using Pyramid::MeshSpecification;
    using Pyramid::PrimitiveTopology;
    using Pyramid::ShaderDataType;
    using Pyramid::Math::Vec3;
    using Pyramid::u32;

    struct Vertex
    {
        float position[3];
        float color[4];
    };

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
        std::cerr << "MeshResourceTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    MeshSpecification MakeSpecification(
        const std::vector<Vertex>& vertices,
        const std::vector<u32>& indices,
        PrimitiveTopology topology,
        const char* name)
    {
        MeshSpecification specification;
        specification.vertexData = vertices.data();
        specification.vertexDataSize = static_cast<u32>(vertices.size() * sizeof(Vertex));
        specification.vertexCount = static_cast<u32>(vertices.size());
        specification.layout = {
            {ShaderDataType::Float3, "a_Position"},
            {ShaderDataType::Float4, "a_Color"},
        };
        specification.indexData = indices.empty() ? nullptr : indices.data();
        specification.indexCount = static_cast<u32>(indices.size());
        specification.topology = topology;
        specification.name = name;
        return specification;
    }
}

int main()
{
    Pyramid::Util::Logger::GetInstance().EnableConsole(false);
    Pyramid::Tests::TestGraphicsDevice device;

    const std::vector<Vertex> quadVertices = {
        {{-2.0f, -1.0f, 0.0f}, {1, 0, 0, 1}},
        {{3.0f, -1.0f, 0.0f}, {0, 1, 0, 1}},
        {{3.0f, 4.0f, 0.0f}, {0, 0, 1, 1}},
        {{-2.0f, 4.0f, 0.0f}, {1, 1, 1, 1}},
    };
    const std::vector<u32> quadIndices = {0, 1, 2, 0, 2, 3};

    auto indexedMesh = Mesh::Create(
        device,
        MakeSpecification(
            quadVertices,
            quadIndices,
            PrimitiveTopology::Triangles,
            "IndexedQuad"));
    if (!indexedMesh || !indexedMesh->IsValid() || !indexedMesh->IsIndexed())
    {
        return Fail("valid indexed mesh was not created");
    }
    if (indexedMesh->GetVertexCount() != 4 ||
        indexedMesh->GetIndexCount() != 6 ||
        indexedMesh->GetDrawCount() != 6 ||
        indexedMesh->GetTopology() != PrimitiveTopology::Triangles ||
        indexedMesh->GetLayout().GetStride() != sizeof(Vertex) ||
        indexedMesh->GetName() != "IndexedQuad")
    {
        return Fail("indexed mesh metadata is incorrect");
    }

    Vec3 minPoint;
    Vec3 maxPoint;
    indexedMesh->GetLocalBounds(minPoint, maxPoint);
    if (!NearlyEqual(minPoint, Vec3(-2.0f, -1.0f, 0.0f)) ||
        !NearlyEqual(maxPoint, Vec3(3.0f, 4.0f, 0.0f)))
    {
        return Fail("indexed mesh bounds are incorrect");
    }

    const std::vector<Vertex> lineVertices = {
        {{-5.0f, 0.0f, 0.0f}, {1, 1, 1, 1}},
        {{5.0f, 0.0f, 0.0f}, {1, 1, 1, 1}},
    };
    const std::vector<u32> noIndices;
    auto lineMesh = Mesh::Create(
        device,
        MakeSpecification(
            lineVertices,
            noIndices,
            PrimitiveTopology::Lines,
            "Line"));
    if (!lineMesh || lineMesh->IsIndexed() || lineMesh->GetDrawCount() != 2 ||
        lineMesh->GetTopology() != PrimitiveTopology::Lines)
    {
        return Fail("valid non-indexed line mesh was not created");
    }

    Pyramid::RenderObject object;
    object.mesh = indexedMesh;
    if (!object.GetLocalBounds(minPoint, maxPoint) ||
        !NearlyEqual(minPoint, Vec3(-2.0f, -1.0f, 0.0f)) ||
        !NearlyEqual(maxPoint, Vec3(3.0f, 4.0f, 0.0f)))
    {
        return Fail("RenderObject did not consume immutable mesh bounds");
    }

    Pyramid::Renderer::CommandBuffer commandBuffer;
    commandBuffer.Begin();
    commandBuffer.DrawMesh(*indexedMesh, 3);
    commandBuffer.End();
    commandBuffer.Execute(&device);
    if (device.drawCalls != 1 || !device.lastDrawIndexed ||
        device.lastDrawCount != 6 || device.lastInstanceCount != 3 ||
        device.lastTopology != PrimitiveTopology::Triangles ||
        device.boundVertexArray == nullptr)
    {
        return Fail("indexed mesh command submission is incorrect");
    }

    device.ResetDrawState();
    commandBuffer.Begin();
    commandBuffer.DrawMesh(*lineMesh);
    commandBuffer.End();
    commandBuffer.Execute(&device);
    if (device.drawCalls != 1 || device.lastDrawIndexed ||
        device.lastDrawCount != 2 || device.lastInstanceCount != 1 ||
        device.lastFirstVertex != 0 ||
        device.lastTopology != PrimitiveTopology::Lines)
    {
        return Fail("non-indexed mesh command submission is incorrect");
    }

    auto invalidSize = MakeSpecification(
        quadVertices,
        quadIndices,
        PrimitiveTopology::Triangles,
        "InvalidSize");
    --invalidSize.vertexDataSize;
    if (Mesh::Create(device, invalidSize))
    {
        return Fail("mismatched vertex byte count was accepted");
    }

    auto missingPosition = MakeSpecification(
        quadVertices,
        quadIndices,
        PrimitiveTopology::Triangles,
        "MissingPosition");
    missingPosition.layout = {{ShaderDataType::Float3, "a_Normal"}};
    missingPosition.vertexDataSize =
        missingPosition.vertexCount * missingPosition.layout.GetStride();
    if (Mesh::Create(device, missingPosition))
    {
        return Fail("mesh without a position semantic was accepted");
    }

    auto invalidIndex = MakeSpecification(
        quadVertices,
        quadIndices,
        PrimitiveTopology::Triangles,
        "InvalidIndex");
    std::vector<u32> outOfRangeIndices = quadIndices;
    outOfRangeIndices.back() = 99;
    invalidIndex.indexData = outOfRangeIndices.data();
    if (Mesh::Create(device, invalidIndex))
    {
        return Fail("out-of-range mesh index was accepted");
    }

    auto invalidTopology = MakeSpecification(
        lineVertices,
        noIndices,
        PrimitiveTopology::Triangles,
        "InvalidTopology");
    if (Mesh::Create(device, invalidTopology))
    {
        return Fail("invalid topology draw count was accepted");
    }

    auto nonFiniteVertices = quadVertices;
    nonFiniteVertices[0].position[0] = std::numeric_limits<float>::infinity();
    auto nonFinite = MakeSpecification(
        nonFiniteVertices,
        quadIndices,
        PrimitiveTopology::Triangles,
        "NonFinite");
    if (Mesh::Create(device, nonFinite))
    {
        return Fail("non-finite mesh positions were accepted");
    }

    return EXIT_SUCCESS;
}
