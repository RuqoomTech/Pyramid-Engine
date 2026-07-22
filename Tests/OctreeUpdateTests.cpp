#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Math/Math.hpp>

#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    bool Expect(bool condition, const char *message)
    {
        if (condition)
        {
            return true;
        }

        std::cerr << message << '\n';
        return false;
    }

    bool Contains(
        const std::vector<std::shared_ptr<Pyramid::RenderObject>> &objects,
        const std::shared_ptr<Pyramid::RenderObject> &object)
    {
        return std::find(objects.begin(), objects.end(), object) != objects.end();
    }
}

int main()
{
    using Pyramid::Math::Vec3;
    using Pyramid::RenderObject;
    using Pyramid::SceneManagement::AABB;
    using Pyramid::SceneManagement::Octree;
    using Pyramid::SceneManagement::QueryParams;
    using Pyramid::SceneManagement::QueryType;
    using Pyramid::SceneManagement::SceneManager;
    using Pyramid::SceneManagement::UpdateFlags;

    bool passed = true;

    Octree octree(Vec3::Zero, Vec3(100.0f), 5, 1);
    auto moving = std::make_shared<RenderObject>();
    moving->name = "Moving";
    moving->position = Vec3(-20.0f, -20.0f, -20.0f);

    auto anchor = std::make_shared<RenderObject>();
    anchor->name = "Anchor";
    anchor->position = Vec3(20.0f, 20.0f, 20.0f);

    auto initial = octree.Synchronize({moving, anchor, moving, nullptr});
    passed &= Expect(initial.insertedObjects == 2, "initial synchronization should insert two unique objects");
    passed &= Expect(initial.removedObjects == 0 && initial.movedObjects == 0, "initial synchronization should not remove or move objects");
    passed &= Expect(octree.GetTrackedObjectCount() == 2, "octree should track two unique objects");
    passed &= Expect(octree.Contains(moving) && octree.Contains(anchor), "inserted objects should be tracked");

    auto unchanged = octree.Synchronize({moving, anchor});
    passed &= Expect(!unchanged.HasChanges(), "unchanged objects should not mutate the octree");
    passed &= Expect(unchanged.unchangedObjects == 2, "both stable objects should be reported unchanged");

    const AABB oldRegion(Vec3(-22.0f, -22.0f, -22.0f), Vec3(-18.0f, -18.0f, -18.0f));
    const AABB newRegion(Vec3(28.0f, 28.0f, 28.0f), Vec3(32.0f, 32.0f, 32.0f));
    passed &= Expect(Contains(octree.QueryBox(oldRegion), moving), "moving object should start in the old octree region");

    moving->position = Vec3(30.0f, 30.0f, 30.0f);
    passed &= Expect(!Contains(octree.QueryBox(newRegion), moving), "unsynchronized movement should not appear in a different octree branch");

    auto moved = octree.Synchronize({moving, anchor});
    passed &= Expect(moved.movedObjects == 1, "synchronization should relocate exactly one moved object");
    passed &= Expect(moved.unchangedObjects == 1, "stationary objects should remain untouched during relocation");
    passed &= Expect(!Contains(octree.QueryBox(oldRegion), moving), "relocated object should leave its previous region");
    passed &= Expect(Contains(octree.QueryBox(newRegion), moving), "relocated object should appear in its new region");

    moving->position.x += 0.00001f;
    auto withinTolerance = octree.Synchronize({moving, anchor});
    passed &= Expect(withinTolerance.movedObjects == 0, "sub-tolerance floating-point drift should not reinsert an object");

    moving->scale = Vec3(4.0f, 1.0f, 1.0f);
    auto resizedBounds = octree.Synchronize({moving, anchor});
    passed &= Expect(resizedBounds.movedObjects == 1, "world-bound changes should relocate tracked objects even without position changes");

    auto removed = octree.Synchronize({moving});
    passed &= Expect(removed.removedObjects == 1, "objects absent from the scene snapshot should be removed");
    passed &= Expect(!octree.Contains(anchor) && octree.GetTrackedObjectCount() == 1, "removed objects should no longer be tracked");

    auto newcomer = std::make_shared<RenderObject>();
    newcomer->position = Vec3(0.0f, 10.0f, 0.0f);
    auto added = octree.Synchronize({moving, newcomer});
    passed &= Expect(added.insertedObjects == 1, "new scene objects should be inserted incrementally");
    passed &= Expect(octree.GetTrackedObjectCount() == 2, "incremental insertion should update tracked-object count");

    SceneManager manager;
    auto scene = manager.CreateScene("Moving objects");
    auto managedMover = std::make_shared<RenderObject>();
    managedMover->position = Vec3(-15.0f, 0.0f, 0.0f);
    auto managedAnchor = std::make_shared<RenderObject>();
    managedAnchor->position = Vec3(15.0f, 0.0f, 0.0f);
    scene->AddRenderObject(managedMover);
    scene->AddRenderObject(managedAnchor);

    manager.UpdateSpatialPartition();
    passed &= Expect(manager.GetStats().spatialObjectsInserted == 2, "first scene synchronization should build the active-scene index");

    managedMover->position = Vec3(25.0f, 0.0f, 0.0f);
    manager.Update(0.016f, UpdateFlags::SpatialPartition);
    passed &= Expect(manager.GetStats().spatialObjectsMoved == 1, "scene updates should automatically relocate moved objects");

    QueryParams query;
    query.type = QueryType::Box;
    query.position = Vec3(25.0f, 0.0f, 0.0f);
    query.size = Vec3(4.0f);
    passed &= Expect(Contains(manager.QueryScene(query).objects, managedMover), "scene queries should see automatically synchronized movement");

    scene->RemoveRenderObject(managedAnchor);
    manager.UpdateSpatialPartition();
    passed &= Expect(manager.GetStats().spatialObjectsRemoved == 1, "scene removals should be removed incrementally from the octree");

    auto managedNewcomer = std::make_shared<RenderObject>();
    managedNewcomer->position = Vec3(0.0f, 20.0f, 0.0f);
    scene->AddRenderObject(managedNewcomer);
    manager.UpdateSpatialPartition();
    passed &= Expect(manager.GetStats().spatialObjectsInserted == 1, "scene additions should be inserted incrementally into the octree");

    manager.UpdateSpatialPartition();
    passed &= Expect(
        manager.GetStats().spatialObjectsMoved == 0 &&
            manager.GetStats().spatialObjectsInserted == 0 &&
            manager.GetStats().spatialObjectsRemoved == 0 &&
            manager.GetStats().spatialObjectsUnchanged == 2,
        "a stable scene should not rebuild or relocate octree objects");

    return passed ? 0 : 1;
}
