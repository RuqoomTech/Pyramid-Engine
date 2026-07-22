#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Math/Math.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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

    bool SameVector(
        const Pyramid::Math::Vec3 &lhs,
        const Pyramid::Math::Vec3 &rhs,
        Pyramid::f32 tolerance = 0.0001f)
    {
        return NearlyEqual(lhs.x, rhs.x, tolerance) &&
               NearlyEqual(lhs.y, rhs.y, tolerance) &&
               NearlyEqual(lhs.z, rhs.z, tolerance);
    }

    bool Contains(const std::vector<Object> &objects, const Object &object)
    {
        return std::find(objects.begin(), objects.end(), object) != objects.end();
    }

    Object MakeObject(const char *name, const Pyramid::Math::Vec3 &position)
    {
        auto object = std::make_shared<Pyramid::RenderObject>();
        object->name = name;
        object->position = position;
        object->SetLocalBounds(Pyramid::Math::Vec3(-0.5f), Pyramid::Math::Vec3(0.5f));
        return object;
    }
}

int main()
{
    using Pyramid::Math::Vec3;
    using Pyramid::SceneManagement::AABB;
    using Pyramid::SceneManagement::Octree;
    using Pyramid::SceneManagement::OctreeConfiguration;

    bool passed = true;

    Octree octree(Vec3::Zero, Vec3(40.0f), 4, 1);
    auto left = MakeObject("Left", Vec3(-12.0f, -12.0f, -12.0f));
    auto right = MakeObject("Right", Vec3(12.0f, 12.0f, 12.0f));
    auto overflow = MakeObject("Overflow", Vec3(80.0f, 0.0f, 0.0f));
    octree.Synchronize({left, right, overflow});

    const auto originalCount = octree.GetTrackedObjectCount();
    passed &= Expect(originalCount == 3, "setup should track three objects");
    passed &= Expect(octree.GetStats().totalNodes > 1, "capacity-one setup should subdivide the tree");

    octree.SetMaxDepth(0);
    passed &= Expect(octree.GetMaxDepth() == 0, "maximum depth should update to zero");
    passed &= Expect(octree.GetStats().totalNodes == 1, "depth-zero configuration should rebuild to one root node");
    passed &= Expect(octree.GetTrackedObjectCount() == originalCount, "depth changes should preserve tracked objects");
    passed &= Expect(Contains(octree.QueryBox(AABB(Vec3(-13.0f), Vec3(-11.0f))), left),
                     "depth rebuild should preserve left-object queries");
    passed &= Expect(octree.FindNearest(Vec3(80.0f, 0.0f, 0.0f)) == overflow,
                     "depth rebuild should preserve root-overflow objects");

    octree.SetMaxObjectsPerNode(0);
    passed &= Expect(octree.GetMaxObjectsPerNode() == 1, "zero capacity should normalize to one object per node");
    passed &= Expect(octree.GetTrackedObjectCount() == originalCount, "capacity changes should preserve tracked objects");

    octree.SetBounds(Vec3(100.0f, 20.0f, -10.0f), Vec3(20.0f, 30.0f, 40.0f));
    const auto movedBounds = octree.GetBounds();
    passed &= Expect(SameVector(movedBounds.GetCenter(), Vec3(100.0f, 20.0f, -10.0f)),
                     "bounds changes should update the root center");
    passed &= Expect(SameVector(movedBounds.GetSize(), Vec3(20.0f, 30.0f, 40.0f)),
                     "bounds changes should update the root size");
    passed &= Expect(octree.GetTrackedObjectCount() == originalCount, "bounds changes should preserve tracked objects");
    passed &= Expect(Contains(octree.QueryPoint(Vec3(12.0f, 12.0f, 12.0f)), right),
                     "objects outside replacement bounds should remain queryable at the root");

    const auto beforeInvalid = octree.GetConfiguration();
    OctreeConfiguration invalid = beforeInvalid;
    invalid.size = Vec3(0.0f, 10.0f, 10.0f);
    passed &= Expect(!octree.Configure(invalid), "zero-sized bounds should be rejected");
    passed &= Expect(SameVector(octree.GetConfiguration().center, beforeInvalid.center) &&
                         SameVector(octree.GetConfiguration().size, beforeInvalid.size),
                     "rejected bounds should preserve the previous configuration");
    passed &= Expect(octree.GetTrackedObjectCount() == originalCount,
                     "rejected bounds should preserve tracked objects");

    invalid = beforeInvalid;
    invalid.center.x = std::numeric_limits<Pyramid::f32>::infinity();
    passed &= Expect(!octree.Configure(invalid), "non-finite centers should be rejected");

    OctreeConfiguration combined;
    combined.center = Vec3(-50.0f, 5.0f, 25.0f);
    combined.size = Vec3(64.0f, 48.0f, 32.0f);
    combined.maxDepth = 6;
    combined.maxObjectsPerNode = 2;
    passed &= Expect(octree.Configure(combined), "valid combined configuration should succeed atomically");

    const auto configured = octree.GetConfiguration();
    passed &= Expect(SameVector(configured.center, combined.center) &&
                         SameVector(configured.size, combined.size) &&
                         configured.maxDepth == combined.maxDepth &&
                         configured.maxObjectsPerNode == combined.maxObjectsPerNode,
                     "combined configuration should update every setting together");
    passed &= Expect(octree.GetTrackedObjectCount() == originalCount,
                     "combined configuration should preserve every tracked object");
    passed &= Expect(octree.Configure(configured), "reapplying the active configuration should be a successful no-op");

    Octree sanitized(Vec3::Zero, Vec3(-10.0f, 0.0f, 5.0f), 3, 0);
    passed &= Expect(sanitized.GetMaxObjectsPerNode() == 1, "constructor should normalize zero capacity");
    passed &= Expect(SameVector(sanitized.GetBounds().GetSize(), Vec3(10.0f, 1.0f, 5.0f)),
                     "constructor should normalize negative and zero extents to usable positive bounds");

    return passed ? 0 : 1;
}
