#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Math/Math.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    using Object = std::shared_ptr<Pyramid::RenderObject>;

    bool Expect(bool condition, const char *message)
    {
        if (condition)
        {
            return true;
        }

        std::cerr << message << '\n';
        return false;
    }

    bool Contains(const std::vector<Object> &objects, const Object &object)
    {
        return std::find(objects.begin(), objects.end(), object) != objects.end();
    }

    bool Unique(const std::vector<Object> &objects)
    {
        for (std::size_t index = 0; index < objects.size(); ++index)
        {
            const auto remaining = objects.begin() + static_cast<std::ptrdiff_t>(index + 1);
            if (std::find(remaining, objects.end(), objects[index]) != objects.end())
            {
                return false;
            }
        }
        return true;
    }

    bool SameObjects(std::vector<Object> lhs, std::vector<Object> rhs)
    {
        const auto byAddress = [](const Object &first, const Object &second)
        {
            return first.get() < second.get();
        };
        std::sort(lhs.begin(), lhs.end(), byAddress);
        std::sort(rhs.begin(), rhs.end(), byAddress);
        return lhs == rhs;
    }

    Object MakeObject(
        const char *name,
        const Pyramid::Math::Vec3 &position,
        const Pyramid::Math::Vec3 &localMin = Pyramid::Math::Vec3(-0.5f),
        const Pyramid::Math::Vec3 &localMax = Pyramid::Math::Vec3(0.5f))
    {
        auto object = std::make_shared<Pyramid::RenderObject>();
        object->name = name;
        object->position = position;
        object->SetLocalBounds(localMin, localMax);
        return object;
    }
}

int main()
{
    using Pyramid::Math::Vec3;
    using Pyramid::Scene;
    using Pyramid::SceneManagement::AABB;
    using Pyramid::SceneManagement::Octree;
    using Pyramid::SceneManagement::QueryParams;
    using Pyramid::SceneManagement::QueryType;
    using Pyramid::SceneManagement::SceneManager;

    bool passed = true;

    Octree octree(Vec3::Zero, Vec3(10.0f), 5, 1);
    auto spanning = MakeObject(
        "Spanning",
        Vec3(5.0f, 3.0f, 0.0f),
        Vec3(-4.0f, -4.0f, -1.0f),
        Vec3(4.0f, 0.0f, 1.0f));
    auto outsideRoot = MakeObject("OutsideRoot", Vec3(20.0f, 0.0f, 0.0f));
    outsideRoot->visible = false;
    auto miss = MakeObject("Miss", Vec3(0.0f, 5.0f, 0.0f));

    octree.Synchronize({spanning, outsideRoot, miss});

    const auto pointHits = octree.QueryPoint(Vec3(1.5f, 0.0f, 0.0f));
    passed &= Expect(Contains(pointHits, spanning), "point query should test the complete object AABB");
    passed &= Expect(!Contains(pointHits, miss), "point query should not return every object stored in a visited node");
    passed &= Expect(Unique(pointHits), "point query results should be unique");

    const auto outsidePointHits = octree.QueryPoint(Vec3(20.0f, 0.0f, 0.0f));
    passed &= Expect(Contains(outsidePointHits, outsideRoot), "root queries should find objects outside configured octree bounds");
    passed &= Expect(Contains(outsidePointHits, outsideRoot), "spatial queries should not filter hidden gameplay objects");

    const auto sphereHits = octree.QuerySphere(Vec3::Zero, 1.1f);
    passed &= Expect(Contains(sphereHits, spanning), "sphere query should include an object whose bounds intersect the sphere");
    passed &= Expect(!Contains(sphereHits, miss), "sphere query should reject non-intersecting bounds");
    passed &= Expect(octree.QuerySphere(Vec3::Zero, -1.0f).empty(), "negative sphere radii should be rejected");
    passed &= Expect(Contains(octree.QuerySphere(Vec3(20.0f, 0.0f, 0.0f), 0.1f), outsideRoot),
                     "sphere queries outside root bounds should inspect root-retained objects");

    const AABB reversedBox(Vec3(1.2f, 0.5f, 0.5f), Vec3(0.0f, -0.5f, -0.5f));
    passed &= Expect(reversedBox.min.x == 0.0f && reversedBox.max.x == 1.2f,
                     "AABB construction should canonicalize reversed endpoints");
    const auto boxHits = octree.QueryBox(reversedBox);
    passed &= Expect(Contains(boxHits, spanning), "box query should use AABB intersection rather than object centers");
    passed &= Expect(!Contains(boxHits, miss), "box query should reject non-intersecting objects");
    passed &= Expect(Contains(octree.QueryBox(AABB(Vec3(19.0f, -1.0f, -1.0f), Vec3(21.0f, 1.0f, 1.0f))), outsideRoot),
                     "box queries outside root bounds should inspect root-retained objects");

    const auto rayHits = octree.QueryRay(Vec3::Zero, Vec3(2.0f, 0.0f, 0.0f), 25.0f);
    passed &= Expect(rayHits.size() == 2, "ray query should return both intersected object bounds");
    passed &= Expect(rayHits.size() >= 2 && rayHits[0] == spanning && rayHits[1] == outsideRoot,
                     "ray hits should be unique and ordered nearest to farthest");
    passed &= Expect(octree.QueryRay(Vec3::Zero, Vec3::Right, 0.5f).empty(),
                     "ray query should respect maximum distance");
    passed &= Expect(octree.QueryRay(Vec3::Zero, Vec3::Zero, 25.0f).empty(),
                     "zero-length ray directions should be rejected");
    passed &= Expect(octree.QueryRay(Vec3::Zero, Vec3::Right, -1.0f).empty(),
                     "negative ray distances should be rejected");

    auto scene = std::make_shared<Scene>("Query parity");
    scene->AddRenderObject(spanning);
    scene->AddRenderObject(outsideRoot);
    scene->AddRenderObject(miss);

    SceneManager manager;
    manager.SetActiveScene(scene);

    QueryParams pointParams;
    pointParams.type = QueryType::Point;
    pointParams.position = Vec3(1.5f, 0.0f, 0.0f);

    QueryParams sphereParams;
    sphereParams.type = QueryType::Sphere;
    sphereParams.position = Vec3::Zero;
    sphereParams.radius = 1.1f;

    QueryParams boxParams;
    boxParams.type = QueryType::Box;
    boxParams.position = Vec3(0.6f, 0.0f, 0.0f);
    boxParams.size = Vec3(1.2f, 1.0f, 1.0f);

    QueryParams rayParams;
    rayParams.type = QueryType::Ray;
    rayParams.position = Vec3::Zero;
    rayParams.direction = Vec3(2.0f, 0.0f, 0.0f);
    rayParams.maxDistance = 25.0f;

    const auto spatialPoint = manager.QueryScene(pointParams);
    const auto spatialSphere = manager.QueryScene(sphereParams);
    const auto spatialBox = manager.QueryScene(boxParams);
    const auto spatialRay = manager.QueryScene(rayParams);

    passed &= Expect(Contains(spatialPoint.objects, spanning), "scene manager should build its octree before the first spatial query");
    passed &= Expect(spatialRay.distances.size() == spatialRay.objects.size(), "ray query distances should match returned objects");
    passed &= Expect(spatialRay.distances.size() >= 2 && spatialRay.distances[0] < spatialRay.distances[1],
                     "scene-manager ray distances should be nearest-first");

    manager.EnableSpatialPartitioning(false);
    const auto linearPoint = manager.QueryScene(pointParams);
    const auto linearSphere = manager.QueryScene(sphereParams);
    const auto linearBox = manager.QueryScene(boxParams);
    const auto linearRay = manager.QueryScene(rayParams);

    passed &= Expect(SameObjects(spatialPoint.objects, linearPoint.objects),
                     "point queries should match with spatial partitioning enabled or disabled");
    passed &= Expect(SameObjects(spatialSphere.objects, linearSphere.objects),
                     "sphere queries should match with spatial partitioning enabled or disabled");
    passed &= Expect(SameObjects(spatialBox.objects, linearBox.objects),
                     "box queries should match with spatial partitioning enabled or disabled");
    passed &= Expect(spatialRay.objects == linearRay.objects,
                     "ray queries should preserve nearest-first ordering in both query paths");
    passed &= Expect(linearRay.distances.size() == linearRay.objects.size(),
                     "linear ray queries should report hit distances");

    return passed ? 0 : 1;
}
