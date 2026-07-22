#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Math/Math.hpp>

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

    bool NearlyEqual(Pyramid::f32 lhs, Pyramid::f32 rhs, Pyramid::f32 tolerance = 0.0001f)
    {
        return std::fabs(lhs - rhs) <= tolerance;
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

    bool SameOrder(const std::vector<Object> &lhs, const std::vector<Object> &rhs)
    {
        return lhs == rhs;
    }
}

int main()
{
    using Pyramid::Math::Vec3;
    using Pyramid::Scene;
    using Pyramid::SceneManagement::AABB;
    using Pyramid::SceneManagement::Octree;
    using Pyramid::SceneManagement::SceneManager;

    bool passed = true;

    const AABB box(Vec3(-1.0f), Vec3(1.0f));
    passed &= Expect(NearlyEqual(box.DistanceSquaredToPoint(Vec3::Zero), 0.0f),
                     "points inside an AABB should have zero squared distance");
    passed &= Expect(NearlyEqual(box.DistanceToPoint(Vec3(4.0f, 1.0f, 1.0f)), 3.0f),
                     "AABB distance should use the closest point on the bounds");
    passed &= Expect(NearlyEqual(box.DistanceSquaredToPoint(Vec3(4.0f, 5.0f, 1.0f)), 25.0f),
                     "AABB squared distance should accumulate separated axes");

    // Its center is far away, but its bounds touch the query point. This must
    // beat an object with a much closer center whose bounds are still farther.
    auto boundsNearest = MakeObject(
        "BoundsNearest",
        Vec3(10.0f, 0.0f, 0.0f),
        Vec3(-10.0f, -0.5f, -0.5f),
        Vec3(10.0f, 0.5f, 0.5f));
    auto centerNearest = MakeObject("CenterNearest", Vec3(1.0f, 0.0f, 0.0f));
    auto third = MakeObject("Third", Vec3(3.0f, 0.0f, 0.0f));
    third->visible = false;
    auto outsideRoot = MakeObject("OutsideRoot", Vec3(30.0f, 0.0f, 0.0f));

    Octree octree(Vec3::Zero, Vec3(20.0f), 6, 1);
    octree.Synchronize({boundsNearest, centerNearest, third, outsideRoot});

    passed &= Expect(octree.FindNearest(Vec3::Zero) == boundsNearest,
                     "nearest-object queries should measure distance to full world bounds");

    const auto firstThree = octree.FindKNearest(Vec3::Zero, 3);
    passed &= Expect(firstThree.size() == 3,
                     "K-nearest should return the requested number when available");
    passed &= Expect(firstThree.size() == 3 &&
                         firstThree[0] == boundsNearest &&
                         firstThree[1] == centerNearest &&
                         firstThree[2] == third,
                     "K-nearest results should be ordered by distance to complete bounds");
    passed &= Expect(firstThree.size() >= 3 && firstThree[2] == third,
                     "nearest gameplay queries should include hidden objects");
    passed &= Expect(octree.FindKNearest(Vec3::Zero, 0).empty(),
                     "K-nearest should reject a zero result count");
    passed &= Expect(octree.FindKNearest(Vec3::Zero, 99).size() == 4,
                     "K-nearest should return every tracked object when K is larger than the scene");
    passed &= Expect(octree.FindNearest(Vec3(29.8f, 0.0f, 0.0f)) == outsideRoot,
                     "nearest queries should include objects retained outside configured root bounds");

    auto scene = std::make_shared<Scene>("Nearest query parity");
    scene->AddRenderObject(boundsNearest);
    scene->AddRenderObject(centerNearest);
    scene->AddRenderObject(third);
    scene->AddRenderObject(outsideRoot);

    SceneManager manager;
    manager.SetActiveScene(scene);

    const auto spatialNearest = manager.GetNearestObject(Vec3::Zero);
    const auto spatialThree = manager.GetKNearestObjects(Vec3::Zero, 3);
    passed &= Expect(spatialNearest == boundsNearest,
                     "scene-manager nearest queries should build and use the octree automatically");
    passed &= Expect(SameOrder(spatialThree, firstThree),
                     "scene-manager K-nearest queries should preserve octree nearest-first ordering");

    manager.EnableSpatialPartitioning(false);
    const auto linearNearest = manager.GetNearestObject(Vec3::Zero);
    const auto linearThree = manager.GetKNearestObjects(Vec3::Zero, 3);
    passed &= Expect(linearNearest == spatialNearest,
                     "nearest-object results should match with spatial partitioning disabled");
    passed &= Expect(SameOrder(linearThree, spatialThree),
                     "K-nearest ordering should match between octree and linear paths");
    passed &= Expect(manager.GetKNearestObjects(Vec3::Zero, 0).empty(),
                     "scene-manager K-nearest should reject a zero result count");

    return passed ? 0 : 1;
}
