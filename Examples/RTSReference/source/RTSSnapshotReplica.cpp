#include <Pyramid/Examples/RTSReference/RTSSnapshotReplica.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Util/Log.hpp>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Pyramid::Examples::RTSReference
{
    namespace
    {
        bool Fail(std::string& error, const char* message)
        {
            error = message;
            return false;
        }

        bool Finite(const Math::Vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        void Canonicalize(Math::Vec3& minimum, Math::Vec3& maximum)
        {
            if (minimum.x > maximum.x) std::swap(minimum.x, maximum.x);
            if (minimum.y > maximum.y) std::swap(minimum.y, maximum.y);
            if (minimum.z > maximum.z) std::swap(minimum.z, maximum.z);
        }

        RTSUnitState UnitState(const Protocol::UnitSnapshot& unit)
        {
            RTSUnitState state;
            state.owner = unit.owner;
            state.team = unit.team;
            state.maximumHealth = unit.maximumHealth;
            state.health = unit.health;
            state.selectable = unit.selectable;
            state.movementSpeed = unit.movementSpeed;
            state.movementTarget = unit.movementTarget;
            return state;
        }

        struct PreviousState
        {
            Entity entity;
            TransformComponent transform;
            MeshRendererComponent renderer;
        };
    }

    RTSSnapshotReplica::RTSSnapshotReplica(
        Scene& scene, RTSUnitStore& units, const ResourceRegistry* registry)
        : m_scene(scene), m_units(units), m_registry(registry)
    {
    }

    RTSSnapshotReplica::~RTSSnapshotReplica()
    {
        std::string error;
        if (!Clear(error))
        {
            PYRAMID_LOG_WARN("Replica cleanup requires native hierarchy repair: ", error);
        }
    }

    bool RTSSnapshotReplica::ValidateRenderer(
        const MeshRendererComponent& renderer, std::string& error) const
    {
        if ((renderer.boundsMode != RenderBoundsMode::Automatic &&
                renderer.boundsMode != RenderBoundsMode::Manual) ||
            !Finite(renderer.localBoundsMin) || !Finite(renderer.localBoundsMax) ||
            !Finite(renderer.meshBoundsMin) || !Finite(renderer.meshBoundsMax))
        {
            return Fail(error, "Replica catalog requires a known bounds mode and finite bounds.");
        }
        const bool meshIdentity = renderer.mesh.GetAssetId().IsValid();
        const bool materialIdentity = renderer.material.GetAssetId().IsValid();
        if (meshIdentity != (renderer.mesh.GetGeneration() != 0) ||
            materialIdentity != (renderer.material.GetGeneration() != 0))
        {
            return Fail(error, "Replica catalog contains a malformed resource handle generation.");
        }
        if ((renderer.mesh || renderer.material) && !m_registry)
        {
            return Fail(error, "Replica catalog resource handles require a live resource registry.");
        }
        if (renderer.mesh)
        {
            const auto mesh = m_registry->Resolve(renderer.mesh);
            if (!mesh || !mesh->IsValid())
            {
                return Fail(error, "Replica catalog mesh handle is stale or invalid; refresh the catalog mapping.");
            }
            Math::Vec3 minimum;
            Math::Vec3 maximum;
            mesh->GetLocalBounds(minimum, maximum);
            if (!Finite(minimum) || !Finite(maximum))
            {
                return Fail(error, "Replica catalog mesh bounds are not finite.");
            }
        }
        if (renderer.material && !m_registry->Resolve(renderer.material))
        {
            return Fail(error, "Replica catalog material handle is stale; refresh the catalog mapping.");
        }
        return true;
    }

    bool RTSSnapshotReplica::ValidateBinding(
        const Binding& binding, bool allowExpired, std::string& error) const
    {
        if (!binding.entity)
        {
            return allowExpired || Fail(error,
                "Replica entity lifetime expired outside reconciliation; Clear before applying another snapshot.");
        }
        if (binding.entity.GetScene() != &m_scene || binding.entity.GetParent() ||
            !binding.entity.GetChildren().empty())
        {
            return Fail(error,
                "Replica entities must remain root and childless; repair foreign hierarchy changes before reconciliation.");
        }
        return true;
    }

    bool RTSSnapshotReplica::SetCatalog(
        u32 catalog, const MeshRendererComponent& input, std::string& error)
    {
        if (catalog == 0)
        {
            return Fail(error, "Replica catalog identifier must be nonzero.");
        }
        if (!ValidateRenderer(input, error))
        {
            return false;
        }
        MeshRendererComponent renderer = input;
        Canonicalize(renderer.localBoundsMin, renderer.localBoundsMax);
        Canonicalize(renderer.meshBoundsMin, renderer.meshBoundsMax);
        m_catalogs[catalog] = renderer;
        error.clear();
        return true;
    }

    bool RTSSnapshotReplica::Begin(u64 session, std::string& error)
    {
        if (session == 0 || m_session != 0)
        {
            return Fail(error, "Replica Begin requires a nonzero session and an inactive replica; Clear before changing epoch.");
        }
        m_session = session;
        m_lastTick = 0;
        m_lastQueuedSequence = 0;
        m_hasSnapshot = false;
        error.clear();
        return true;
    }

    bool RTSSnapshotReplica::Apply(const Protocol::Message& message, std::string& error)
    {
        if (m_session == 0 || message.session != m_session)
        {
            return Fail(error, "Snapshot belongs to an inactive or different replica session.");
        }
        const auto* snapshot = std::get_if<Protocol::FullSnapshot>(&message.body);
        if (!snapshot)
        {
            return Fail(error, "Replica reconciliation requires a FullSnapshot message.");
        }
        Protocol::Error diagnostic;
        if (!Protocol::Validate(message, diagnostic))
        {
            error = "Snapshot validation failed at byte " + std::to_string(diagnostic.offset) +
                ": " + diagnostic.message;
            return false;
        }
        if (m_hasSnapshot && (snapshot->tick <= m_lastTick ||
            snapshot->lastQueuedSequence < m_lastQueuedSequence))
        {
            return Fail(error, "Snapshot tick must increase and its queued acknowledgement must not regress.");
        }
        for (const auto& item : m_bindings)
        {
            if (!ValidateBinding(item.second, false, error))
            {
                return false;
            }
            if (!item.second.entity.GetTransform() || !item.second.entity.GetMeshRenderer() ||
                !m_units.FindUnit(item.second.entity))
            {
                return Fail(error, "Replica components or gameplay state were removed externally; Clear before reconciling.");
            }
        }
        for (const auto& unit : snapshot->units)
        {
            const auto catalog = m_catalogs.find(unit.catalog);
            if (catalog == m_catalogs.end())
            {
                return Fail(error, "Snapshot references an unknown client catalog entry.");
            }
            if (!ValidateRenderer(catalog->second, error))
            {
                return false;
            }
        }

        // ponytail: copy the game-owned store for rollback; add a narrower staged
        // write set only if snapshot profiling shows this bounded path matters.
        RTSUnitStore stagedUnits = m_units;
        std::map<u64, Binding> nextBindings;
        std::vector<Entity> created;
        std::vector<PreviousState> previous;
        created.reserve(snapshot->units.size());
        previous.reserve(snapshot->units.size());
        const auto discardCreated = [&]()
        {
            for (Entity entity : created)
            {
                (void)m_scene.DestroyEntity(entity);
            }
        };

        for (const auto& unit : snapshot->units)
        {
            const auto existing = m_bindings.find(unit.unit);
            if (existing != m_bindings.end())
            {
                const Entity entity = existing->second.entity;
                RTSUnitState* state = stagedUnits.FindMutableUnit(entity);
                if (!state)
                {
                    discardCreated();
                    return Fail(error, "Retained replica gameplay state could not be staged.");
                }
                *state = UnitState(unit);
                nextBindings.emplace(unit.unit, Binding{entity, unit.catalog});
                previous.push_back({entity, *entity.GetTransform(), *entity.GetMeshRenderer()});
                continue;
            }
            Entity entity = m_scene.CreateEntity("RTS Replica " + std::to_string(unit.unit));
            if (!entity)
            {
                discardCreated();
                return Fail(error, "Client scene could not allocate an automatic entity identifier.");
            }
            created.push_back(entity);
            if (!entity.SetLocalTransform(unit.position, unit.rotation, unit.scale) ||
                !entity.SetMeshRenderer(m_catalogs.at(unit.catalog), m_registry) ||
                !stagedUnits.AddUnit(entity, UnitState(unit)))
            {
                discardCreated();
                return Fail(error, "Client replica entity or unit state could not be staged.");
            }
            nextBindings.emplace(unit.unit, Binding{entity, unit.catalog});
        }
        for (const auto& item : m_bindings)
        {
            if (nextBindings.find(item.first) == nextBindings.end())
            {
                // Identity equality, not numeric ID alone, protects unrelated replacements.
                const auto state = stagedUnits.m_units.find(item.second.entity.GetId());
                if (state != stagedUnits.m_units.end() && state->second.entity == item.second.entity)
                {
                    stagedUnits.m_units.erase(state);
                }
            }
        }

        std::size_t changed = 0;
        for (const auto& unit : snapshot->units)
        {
            if (m_bindings.find(unit.unit) == m_bindings.end())
            {
                continue;
            }
            Entity entity = nextBindings.at(unit.unit).entity;
            ++changed;
            if (!entity.SetLocalTransform(unit.position, unit.rotation, unit.scale) ||
                !entity.SetMeshRenderer(m_catalogs.at(unit.catalog), m_registry))
            {
                for (std::size_t index = 0; index < changed; ++index)
                {
                    Entity rollback = previous[index].entity;
                    (void)rollback.SetLocalTransform(previous[index].transform.position,
                        previous[index].transform.rotation, previous[index].transform.scale);
                    // Restore opaque prior handles even if native code refreshed their cache.
                    (void)rollback.SetMeshRenderer(previous[index].renderer, nullptr);
                }
                discardCreated();
                return Fail(error, "Client replica publication failed; retained state was restored.");
            }
        }

        // Every removal was prevalidated live/root/childless, and this single-thread
        // commit invokes no callbacks. Scene destruction cannot reject those facades.
        for (const auto& item : m_bindings)
        {
            if (nextBindings.find(item.first) == nextBindings.end())
            {
                (void)m_scene.DestroyEntity(item.second.entity);
            }
        }
        m_units.m_units.swap(stagedUnits.m_units);
        m_bindings.swap(nextBindings);
        m_lastTick = snapshot->tick;
        m_lastQueuedSequence = snapshot->lastQueuedSequence;
        m_hasSnapshot = true;
        error.clear();
        return true;
    }

    Entity RTSSnapshotReplica::FindEntity(u64 networkId) const
    {
        const auto found = m_bindings.find(networkId);
        return found != m_bindings.end() && found->second.entity ? found->second.entity : Entity{};
    }

    u64 RTSSnapshotReplica::FindNetworkId(Entity entity) const
    {
        if (!entity)
        {
            return 0;
        }
        for (const auto& item : m_bindings)
        {
            if (item.second.entity == entity)
            {
                return item.first;
            }
        }
        return 0;
    }

    bool RTSSnapshotReplica::Clear(std::string& error)
    {
        for (const auto& item : m_bindings)
        {
            if (!ValidateBinding(item.second, true, error))
            {
                return false;
            }
        }
        if (!m_bindings.empty())
        {
            RTSUnitStore stagedUnits = m_units;
            for (const auto& item : m_bindings)
            {
                const auto state = stagedUnits.m_units.find(item.second.entity.GetId());
                if (state != stagedUnits.m_units.end() && state->second.entity == item.second.entity)
                {
                    stagedUnits.m_units.erase(state);
                }
            }
            for (const auto& item : m_bindings)
            {
                if (item.second.entity)
                {
                    (void)m_scene.DestroyEntity(item.second.entity);
                }
            }
            m_units.m_units.swap(stagedUnits.m_units);
            m_bindings.clear();
        }
        m_session = 0;
        m_lastTick = 0;
        m_lastQueuedSequence = 0;
        m_hasSnapshot = false;
        error.clear();
        return true;
    }
}
