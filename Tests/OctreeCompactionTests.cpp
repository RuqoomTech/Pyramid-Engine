#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
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

    bool NearlyEqual(Pyramid::f32 lhs, Pyramid::f32 rhs, Pyramid::f32 tolerance = 0.0001f)
    {
        return std::fabs(lhs - rhs) <= tolerance;
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

    bool passed = true;

    Octree octree(Vec3::Zero, Vec3(128.0f), 6, 1);
    std::vector<Object> objects{
        MakeObject("A", Vec3(-40.0f, -40.0f, -40.0f)),
        MakeObject("B", Vec3(40.0f, -40.0f, -40.0f)),
        MakeObject("C", Vec3(-40.0f, 40.0f, -40.0f)),
        MakeObject("D", Vec3(40.0f, 40.0f, -40.0f)),
        MakeObject("E", Vec3(-40.0f, -40.0f, 40.0f)),
        MakeObject("F", Vec3(40.0f, -40.0f, 40.0f)),
        MakeObject("G", Vec3(-40.0f, 40.0f, 40.0f)),
        MakeObject("H", Vec3(40.0f, 40.0f, 40.0f))};

    const auto initialSync = octree.Synchronize(objects);
    passed &= Expect(initialSync.insertedObjects == objects.size(), "setup should insert every object");

    const auto initialStats = octree.GetStats();
    passed &= Expect(initialStats.totalNodes > 1, "capacity-one octree should subdivide");
    passed &= Expect(initialStats.internalNodes > 0, "subdivided octree should report internal nodes");
    passed &= Expect(initialStats.leafNodes == initialStats.occupiedLeafNodes + initialStats.emptyLeafNodes,
                     "leaf health counters should be internally consistent");
    passed &= Expect(initialStats.totalObjects == objects.size() && initialStats.trackedObjects == objects.size(),
                     "health metrics should report physical and tracked object counts");
    passed &= Expect(initialStats.configuredMaxDepth == 6 && initialStats.maxDepth <= 6,
                     "health metrics should distinguish configured and occupied depth");
    passed &= Expect(initialStats.maxObjectsInNode >= 1, "health metrics should report peak node occupancy");
    passed &= Expect(initialStats.averageDepth > 0.0f, "subdivided leaves should have positive average depth");
    passed &= Expect(initialStats.memoryUsage > 0.0f, "health metrics should estimate memory usage");

    const Object survivor = objects.back();
    for (std::size_t index = 0; index + 1 < objects.size(); ++index)
    {
        octree.Remove(objects[index]);
    }

    const auto removalCompaction = octree.GetLastCompactionStats();
    const auto compactedStats = octree.GetStats();
    passed &= Expect(removalCompaction.HasChanges(), "large removals should automatically compact empty branches");
    passed &= Expect(removalCompaction.collapsedNodes > 0, "automatic compaction should report collapsed nodes");
    passed &= Expect(removalCompaction.promotedObjects == 1, "the remaining descendant should be promoted to the root");
    passed &= Expect(compactedStats.totalNodes == 1 && compactedStats.leafNodes == 1,
                     "one remaining object should compact to a root-only tree");
    passed &= Expect(compactedStats.internalNodes == 0 && compactedStats.emptyLeafNodes == 0,
                     "root-only occupied trees should have no internal or empty nodes");
    passed &= Expect(compactedStats.totalObjects == 1 && compactedStats.trackedObjects == 1,
                     "compaction must preserve object tracking");
    passed &= Expect(compactedStats.maxDepth == 0 && NearlyEqual(compactedStats.averageObjectsPerLeaf, 1.0f),
                     "root-only health metrics should report depth zero and one object per leaf");
    passed &= Expect(Contains(octree.QueryBox(AABB(Vec3(39.0f), Vec3(41.0f))), survivor),
                     "the promoted survivor should remain queryable");

    const auto noOpCompaction = octree.Compact();
    passed &= Expect(!noOpCompaction.HasChanges(), "compacting an already compact tree should be a no-op");

    Octree movingTree(Vec3::Zero, Vec3(128.0f), 6, 1);
    auto mover = MakeObject("Mover", Vec3(-40.0f, -40.0f, -40.0f));
    auto anchor = MakeObject("Anchor", Vec3::Zero);
    movingTree.Synchronize({mover, anchor});
    const auto nodesBeforeMove = movingTree.GetStats().totalNodes;

    mover->position = Vec3::Zero;
    passed &= Expect(movingTree.UpdateIfMoved(mover), "moving an object should update its octree placement");
    const auto movementCompaction = movingTree.GetLastCompactionStats();
    passed &= Expect(movementCompaction.HasChanges(), "movement should compact the emptied source branch");
    passed &= Expect(movingTree.GetStats().totalNodes < nodesBeforeMove,
                     "movement compaction should reduce stale structural nodes");
    passed &= Expect(Contains(movingTree.QueryBox(AABB(Vec3(-1.0f), Vec3(1.0f))), mover) &&
                         Contains(movingTree.QueryBox(AABB(Vec3(-1.0f), Vec3(1.0f))), anchor),
                     "movement compaction should preserve both objects at their new bounds");

    Octree synchronizedTree(Vec3::Zero, Vec3(128.0f), 6, 1);
    auto left = MakeObject("Left", Vec3(-40.0f, -40.0f, -40.0f));
    auto right = MakeObject("Right", Vec3(40.0f, 40.0f, 40.0f));
    synchronizedTree.Synchronize({left, right});
    const auto reduced = synchronizedTree.Synchronize({right});
    passed &= Expect(reduced.removedObjects == 1, "synchronization should report removed objects");
    passed &= Expect(reduced.compaction.HasChanges(), "synchronization should expose automatic compaction statistics");
    passed &= Expect(synchronizedTree.GetStats().totalNodes == 1,
                     "synchronization removals should leave a compact root-only tree");

    synchronizedTree.Clear();
    const auto clearedStats = synchronizedTree.GetStats();
    passed &= Expect(clearedStats.totalNodes == 1 && clearedStats.leafNodes == 1 && clearedStats.emptyLeafNodes == 1,
                     "clearing should retain one empty root leaf");
    passed &= Expect(clearedStats.totalObjects == 0 && clearedStats.trackedObjects == 0,
                     "clearing should reset object health counters");
    passed &= Expect(!synchronizedTree.GetLastCompactionStats().HasChanges(),
                     "clearing should reset the last compaction result");

    return passed ? 0 : 1;
}
