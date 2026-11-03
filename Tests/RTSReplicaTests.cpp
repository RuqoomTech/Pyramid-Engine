#include <Pyramid/Examples/RTSReference/RTSSnapshotReplica.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>

#include "TestGraphicsDevice.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using namespace Pyramid;
    using namespace Pyramid::Examples::RTSReference;

    void Require(bool condition, const std::string& message)
    {
        if (!condition)
        {
            std::cerr << "RTS replica test failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    bool Near(const Math::Vec3& left, const Math::Vec3& right)
    {
        return std::fabs(left.x - right.x) < 0.0001f &&
            std::fabs(left.y - right.y) < 0.0001f && std::fabs(left.z - right.z) < 0.0001f;
    }

    MeshRendererComponent Renderer(f32 extent = 0.5f)
    {
        MeshRendererComponent renderer;
        renderer.boundsMode = RenderBoundsMode::Manual;
        renderer.localBoundsMin = Math::Vec3(-extent);
        renderer.localBoundsMax = Math::Vec3(extent);
        return renderer;
    }

    Protocol::UnitSnapshot Unit(u64 id, f32 x = 0.0f, u32 catalog = 1)
    {
        Protocol::UnitSnapshot unit;
        unit.unit = id;
        unit.catalog = catalog;
        unit.owner = 1;
        unit.team = 1;
        unit.position = Math::Vec3(x, 0.0f, 0.0f);
        return unit;
    }

    Protocol::Message Snapshot(u64 tick, std::vector<Protocol::UnitSnapshot> roster, u64 ack = 0)
    {
        return {77, Protocol::FullSnapshot{tick, ack, std::move(roster)}};
    }

    void TestStateAndAtomicMembership()
    {
        Scene scene("Replica Scene");
        const Entity floor = scene.CreateEntity("Unrelated Floor");
        Entity unrelated = scene.CreateEntity("Unrelated Gameplay");
        RTSUnitStore units;
        RTSUnitState unrelatedState;
        unrelatedState.owner = 9;
        Require(units.AddUnit(unrelated, unrelatedState), "unrelated game state fixture failed");
        RTSSnapshotReplica replica(scene, units);
        std::string error;
        Require(!replica.Begin(0, error) && !error.empty() && replica.Begin(77, error),
            "replica epoch validation failed");
        Require(replica.SetCatalog(1, Renderer(), error) && replica.SetCatalog(2, Renderer(1.0f), error),
            "catalog fixture: " + error);
        Require(replica.Apply(Snapshot(0, {Unit(1001, 1), Unit(1002, 2)}, 1), error),
            "initial snapshot: " + error);
        Entity first = replica.FindEntity(1001);
        const Entity second = replica.FindEntity(1002);
        Require(first && second && replica.GetEntityCount() == 2 && replica.HasSnapshot() &&
                replica.GetLastTick() == 0 && first.GetId() != EntityId(1001) &&
                replica.FindNetworkId(first) == 1001 && replica.FindNetworkId(unrelated) == 0,
            "wire IDs must map to captured new scene lifetimes only");
        const auto sceneCount = scene.GetEntities().size();
        auto changed = Unit(1001, 30, 2);
        changed.owner = 2;
        changed.team = 8;
        changed.maximumHealth = 120;
        changed.health = 90;
        changed.movementSpeed = 8.0f;
        changed.scale = Math::Vec3(2.0f, 3.0f, 4.0f);
        changed.rotation = Math::Quat::FromEuler(0.1f, 0.2f, 0.3f);
        changed.movementTarget = Math::Vec3(40.0f, 0.0f, 0.0f);
        changed.selectable = false;
        auto mixed = Snapshot(1, {changed, Unit(1003, 3), Unit(1004, 4, 999)}, 2);
        Require(!replica.Apply(mixed, error) && !error.empty() && replica.GetLastTick() == 0 &&
                scene.GetEntities().size() == sceneCount && replica.FindEntity(1001) == first &&
                replica.FindEntity(1002) == second && !replica.FindEntity(1003) &&
                Near(first.GetWorldPosition(), Math::Vec3(1.0f, 0.0f, 0.0f)) &&
                units.FindUnit(first)->owner == 1 && units.FindUnit(first)->health == 100,
            "mixed invalid roster must preserve scene, membership, state, and tick");
        Require(replica.SetCatalog(999, Renderer(), error) && replica.Apply(mixed, error),
            "repaired rejected tick must remain eligible: " + error);
        Require(replica.FindEntity(1001) == first && !second && replica.GetEntityCount() == 3 &&
                replica.GetLastTick() == 1 && Near(first.GetWorldPosition(), changed.position),
            "retained membership must keep its facade; omitted membership must expire");
        const auto* state = units.FindUnit(first);
        Require(state && state->owner == 2 && state->team == 8 && state->maximumHealth == 120 &&
                state->health == 90 && state->movementSpeed == 8.0f && !state->selectable &&
                state->movementTarget && Near(*state->movementTarget, *changed.movementTarget) &&
                Near(first.GetTransform()->scale, changed.scale) &&
                first.GetMeshRenderer()->localBoundsMax.x == 1.0f,
            "full authoritative metadata, transform, and catalog were not projected exactly");
        Require(!units.IsSelectable(first), "replicated selection policy was not applied");

        SceneManagement::SceneManager manager;
        std::shared_ptr<Scene> borrowedScene(&scene, [](Scene*) {});
        manager.SetActiveScene(borrowedScene);
        const Math::Vec3 publishedPosition = first.GetWorldPosition();
        manager.Update(0.25f);
        Require(Near(first.GetWorldPosition(), publishedPosition),
            "presentation/spatial updates must not simulate a replicated movement target");
        SceneManagement::QueryParams query;
        query.type = SceneManagement::QueryType::Sphere;
        query.position = changed.position;
        query.radius = 0.25f;
        const auto result = manager.QueryScene(query);
        Require(std::find(result.entities.begin(), result.entities.end(), first) != result.entities.end(),
            "spatial query did not follow authoritative replicated transform/components");

        Require(!replica.Apply(mixed, error) && replica.GetLastTick() == 1,
            "equal-tick snapshot must not reapply state");
        auto wrongEpoch = Snapshot(2, {Unit(1001)}, 2);
        wrongEpoch.session = 78;
        Require(!replica.Apply(wrongEpoch, error), "wrong epoch snapshot was accepted");
        Require(!replica.Apply(Snapshot(2, {Unit(1001)}, 1), error),
            "regressing queue acknowledgement was accepted");
        changed.health = 0;
        changed.movementTarget.reset();
        changed.selectable = true;
        Require(replica.Apply(Snapshot(2, {changed}, 3), error) && !units.IsSelectable(first) &&
                units.FindUnit(first)->health == 0,
            "dead replicated units must block selection while retaining their entity");
        changed.health = 120;
        Require(replica.Apply(Snapshot(3, {changed}, 3), error) && units.IsSelectable(first),
            "a newer authoritative snapshot must support healing/revival state");
        Require(replica.Apply(Snapshot(4, {}, 3), error) && !first &&
                replica.GetEntityCount() == 0 && floor && unrelated &&
                units.FindUnit(unrelated)->owner == 9,
            "empty full roster must remove managed units only");
        Require(replica.Apply(Snapshot(5, {Unit(1001)}, 3), error), "wire membership reappearance failed");
        const Entity reappeared = replica.FindEntity(1001);
        Require(reappeared && reappeared != first && !first,
            "reappearing wire IDs must get fresh client entity lifetimes");
        Require(replica.Clear(error) && !reappeared && floor && unrelated &&
                !replica.HasSnapshot() && replica.GetSessionId() == 0 &&
                scene.GetEntities().size() == 2 && units.FindUnit(unrelated),
            "replica clear must preserve props and unrelated gameplay state");
    }

    void TestExternalLifetimeAndHierarchy()
    {
        Scene scene("Replica Lifetime");
        RTSUnitStore units;
        RTSSnapshotReplica replica(scene, units);
        std::string error;
        Require(replica.SetCatalog(1, Renderer(), error) && replica.Begin(77, error) &&
                replica.Apply(Snapshot(0, {Unit(1001)}), error), "lifetime fixture failed");
        Entity original = replica.FindEntity(1001);
        Entity child = scene.CreateEntity("Foreign Child");
        Require(child.SetParent(original), "foreign hierarchy fixture failed");
        Require(!replica.Apply(Snapshot(1, {}), error) && original && child &&
                !replica.Clear(error) && original && child,
            "foreign descendants must not be deleted by snapshot cleanup");
        Require(child.ClearParent() && replica.Apply(Snapshot(1, {Unit(1001, 2)}), error),
            "repaired hierarchy must permit retry of the same tick");
        Require(scene.DestroyEntity(original), "external replica destruction failed");
        Entity replacement = scene.CreateEntityWithId(original.GetId(), "Unrelated Same ID");
        Require(replacement && replacement.SetLocalPosition(Math::Vec3(99.0f)) &&
                units.PruneInvalidEntities() == 1, "same-ID lifetime fixture failed");
        RTSUnitState foreign;
        foreign.owner = 9;
        Require(units.AddUnit(replacement, foreign), "same-ID foreign unit state fixture failed");
        Require(!replica.Apply(Snapshot(2, {Unit(1001, 4)}), error) &&
                !replica.FindEntity(1001) && replica.FindNetworkId(replacement) == 0 &&
                replacement.GetWorldPosition().x == 99.0f,
            "expired network bindings must not adopt local same-ID replacements");
        Require(replica.Clear(error) && replacement && units.FindUnit(replacement) &&
                units.FindUnit(replacement)->owner == 9 && child,
            "clearing expired bindings must preserve same-ID replacement state");
    }

    void TestCatalogGenerations()
    {
        Tests::TestGraphicsDevice device;
        ResourceRegistry registry(device);
        Scene scene("Replica Resources");
        RTSUnitStore units;
        RTSSnapshotReplica replica(scene, units, &registry);
        std::string error;
        const std::array<Vertex, 3> vertices = {
            Vertex{-1, -1, 0, 1, 0, 0, 1}, Vertex{1, -1, 0, 0, 1, 0, 1}, Vertex{0, 1, 0, 0, 0, 1, 1}};
        MeshSpecification specification;
        specification.vertexData = vertices.data();
        specification.vertexDataSize = sizeof(vertices);
        specification.vertexCount = 3;
        specification.layout = {{ShaderDataType::Float3, "Position"}, {ShaderDataType::Float4, "Color"}};
        specification.assetId = MeshAssetId::FromString("replica/triangle");
        auto renderer = Renderer();
        renderer.mesh = registry.AcquireMesh(specification);
        Require(renderer.mesh && replica.SetCatalog(1, renderer, error) && replica.Begin(77, error) &&
                replica.Apply(Snapshot(0, {Unit(1001)}), error), "resource replica fixture: " + error);
        const Entity retained = replica.FindEntity(1001);
        const auto count = scene.GetEntities().size();
        const auto oldHandle = renderer.mesh;
        Require(registry.Evict(oldHandle), "catalog mesh eviction failed");
        renderer.mesh = registry.AcquireMesh(specification);
        Require(renderer.mesh && renderer.mesh != oldHandle && !registry.Resolve(oldHandle),
            "same asset alias did not change generation");
        Require(!replica.Apply(Snapshot(1, {Unit(1001, 3), Unit(1002)}), error) &&
                replica.GetLastTick() == 0 && replica.FindEntity(1001) == retained &&
                retained.GetWorldPosition().x == 0 && scene.GetEntities().size() == count,
            "stale catalog handles must reject before scene publication");
        Require(replica.SetCatalog(1, renderer, error) &&
                replica.Apply(Snapshot(1, {Unit(1001, 3), Unit(1002)}), error) &&
                replica.FindEntity(1001) == retained && retained.GetMeshRenderer()->mesh == renderer.mesh,
            "explicit catalog generation refresh must permit atomic retry");
        auto invalid = Renderer();
        invalid.material = MaterialHandle::FromParts(MaterialAssetId::FromString("missing"), 1);
        Require(!replica.SetCatalog(2, invalid, error), "unresolved material generation was accepted");
        invalid = Renderer();
        invalid.localBoundsMin.x = std::numeric_limits<f32>::quiet_NaN();
        Require(!replica.SetCatalog(2, invalid, error), "malformed catalog bounds were accepted");
        Scene withoutRegistry("No Registry");
        RTSUnitStore detachedUnits;
        RTSSnapshotReplica detached(withoutRegistry, detachedUnits);
        Require(!detached.SetCatalog(1, renderer, error),
            "resource-backed catalogs must not bypass registry resolution");
    }
}

int main()
{
    TestStateAndAtomicMembership();
    TestExternalLifetimeAndHierarchy();
    TestCatalogGenerations();
    std::cout << "RTS replica tests passed\n";
    return EXIT_SUCCESS;
}
