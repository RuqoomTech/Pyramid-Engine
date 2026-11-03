#pragma once

#include <Pyramid/Examples/RTSReference/RTSProtocol.hpp>
#include <Pyramid/Graphics/Scene/Entity.hpp>

#include <cstddef>
#include <map>
#include <string>

namespace Pyramid::Examples::RTSReference
{
    class RTSUnitStore;

    /**
     * Single-threaded projection of full host snapshots into one client scene.
     *
     * The scene, unit store, and optional resource registry must outlive this
     * object. Only identity/catalog bindings are retained here; transforms and
     * gameplay state remain in the scene and unit store. Managed entities must
     * remain root and childless. The client must not simulate those units.
     *
     * Ordinary validation/setter failures preserve the accepted membership,
     * state, and freshness counters. Allocation failures/native crashes are not
     * contained. Successful Apply/Clear commits invalidate all borrowed state
     * pointers returned by the associated RTSUnitStore, including unrelated ones.
     */
    class RTSSnapshotReplica final
    {
    public:
        RTSSnapshotReplica(Scene& scene, RTSUnitStore& units,
            const ResourceRegistry* registry = nullptr);
        ~RTSSnapshotReplica();
        RTSSnapshotReplica(const RTSSnapshotReplica&) = delete;
        RTSSnapshotReplica& operator=(const RTSSnapshotReplica&) = delete;

        /**
         * Trusted native catalog mapping. Updates can explicitly refresh cache
         * generations; every Apply revalidates used resources. A null registry
         * permits only handle-free CPU fixtures. Mapping semantics must agree
         * with the session's content compatibility identity.
         */
        [[nodiscard]] bool SetCatalog(u32 catalog, const MeshRendererComponent& renderer,
            std::string& error);
        /** Requires a nonzero session and no active session; Clear first to change epoch. */
        [[nodiscard]] bool Begin(u64 session, std::string& error);
        [[nodiscard]] bool Apply(const Protocol::Message& snapshot, std::string& error);

        [[nodiscard]] Entity FindEntity(u64 networkId) const;
        [[nodiscard]] u64 FindNetworkId(Entity entity) const;
        /**
         * Removes managed entities/state only; catalogs survive. Expired bindings
         * never adopt or destroy same-local-ID replacements. Foreign hierarchy
         * changes fail preflight rather than deleting unrelated descendants.
         */
        [[nodiscard]] bool Clear(std::string& error);

        [[nodiscard]] bool HasSnapshot() const { return m_hasSnapshot; }
        [[nodiscard]] u64 GetLastTick() const { return m_lastTick; }
        [[nodiscard]] u64 GetSessionId() const { return m_session; }
        [[nodiscard]] std::size_t GetEntityCount() const { return m_bindings.size(); }

    private:
        struct Binding
        {
            Entity entity;
            u32 catalog = 0;
        };

        [[nodiscard]] bool ValidateRenderer(const MeshRendererComponent& renderer,
            std::string& error) const;
        [[nodiscard]] bool ValidateBinding(const Binding& binding, bool allowExpired,
            std::string& error) const;

        Scene& m_scene;
        RTSUnitStore& m_units;
        const ResourceRegistry* m_registry = nullptr;
        std::map<u32, MeshRendererComponent> m_catalogs;
        std::map<u64, Binding> m_bindings;
        u64 m_session = 0;
        u64 m_lastTick = 0;
        u64 m_lastQueuedSequence = 0;
        bool m_hasSnapshot = false;
    };
}
