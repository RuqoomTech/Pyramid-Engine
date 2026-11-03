#pragma once

#include <Pyramid/Examples/RTSReference/RTSConnection.hpp>
#include <Pyramid/Examples/RTSReference/RTSHostSession.hpp>
#include <Pyramid/Examples/RTSReference/RTSSimulation.hpp>
#include <Pyramid/Examples/RTSReference/RTSSnapshotReplica.hpp>
#include <Pyramid/Graphics/Scene/Entity.hpp>

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Pyramid::Examples::RTSReference
{
    class RTSUnitStore;

    enum class RTSMatchMode
    {
        None,
        Offline,
        Host,
        Client
    };

    /** Complete ASCII decimal port, 1..65535; failure preserves output. */
    [[nodiscard]] bool ParseRTSPort(std::string_view text, u16& output, std::string& error);

    /**
     * Single-threaded reference match, independent of native application/UI state.
     *
     * Scene, unit store, simulation and optional registry are borrowed and must
     * outlive this object. Only explicitly created local units and replica members
     * are removed on reset; other scene objects and cached resources remain owned
     * by their existing owners. Managed units must remain root and childless.
     *
     * Host networking and authoritative simulation continue while localPaused is
     * true. Clients only publish received snapshots into their scene and never run
     * local unit simulation. Call Update with uncapped monotonic elapsed time even
     * while a local menu, focus loss or minimization blocks interaction.
     */
    class RTSMatch final
    {
    public:
        RTSMatch(Scene& scene, RTSUnitStore& units, RTSSimulation& simulation,
            const ResourceRegistry* registry = nullptr);
        ~RTSMatch();
        RTSMatch(const RTSMatch&) = delete;
        RTSMatch& operator=(const RTSMatch&) = delete;

        /** Trusted game content; catalog 1 is the reference local-unit renderer. */
        [[nodiscard]] bool SetCatalog(u32 catalog, const MeshRendererComponent& renderer, std::string& error);
        [[nodiscard]] bool StartOffline(std::string& error);
        [[nodiscard]] bool Host(const Platform::TcpEndpoint& endpoint, u64 epoch,
            const Protocol::ContentId& content, std::string& error);
        [[nodiscard]] bool Join(const Platform::TcpEndpoint& endpoint,
            const Protocol::ContentId& content, std::string& error);
        /** Stops transport first; refuses unsafe deletion of managed foreign hierarchies. */
        [[nodiscard]] bool Stop(std::string& error);
        /** Invalid elapsed time fails nonterminally, without changing accepted state. */
        [[nodiscard]] bool Update(f64 elapsedSeconds, bool localPaused, std::string& error);
        [[nodiscard]] bool QueueCommand(const RTSCommandRequest& command, std::string& error);

        [[nodiscard]] RTSMatchMode GetMode() const { return m_mode; }
        [[nodiscard]] u32 GetLocalOwner() const;
        [[nodiscard]] bool IsReady() const;
        [[nodiscard]] const std::string& GetStatus() const { return m_status; }
        /** Offline/host player-one unit; clients have no locally authoritative unit. */
        [[nodiscard]] Entity GetPrimaryEntity() const;
        [[nodiscard]] u16 GetHostPort() const;
        [[nodiscard]] u64 GetTick() const;
        [[nodiscard]] const RTSSimulationResult& GetLastSimulationResult() const { return m_lastSimulation; }

    private:
        [[nodiscard]] bool ValidateReferenceCatalog(std::string& error);
        [[nodiscard]] bool CreateLocalUnit(u32 owner, const Math::Vec3& position, std::string& error);
        [[nodiscard]] bool StopWithFailure(const std::string& reason, std::string& error);

        Scene& m_scene;
        RTSUnitStore& m_units;
        RTSSimulation& m_simulation;
        const ResourceRegistry* m_registry = nullptr;
        std::map<u32, MeshRendererComponent> m_catalogs;
        RTSSnapshotReplica m_replica;
        RTSHostSession m_hostSession;
        RTSHostConnection m_hostConnection;
        RTSClientConnection m_clientConnection;
        std::vector<Entity> m_localUnits;
        RTSMatchMode m_mode = RTSMatchMode::None;
        std::string m_status = "No active match";
        RTSSimulationResult m_lastSimulation;
        u64 m_nextSequence = 1;
        u64 m_lastSnapshotTick = 0;
        bool m_offlineWasActive = false;
        bool m_hostWasReady = false;
    };
}
