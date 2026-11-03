#include <Pyramid/Examples/RTSReference/RTSMatch.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Util/Log.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace Pyramid::Examples::RTSReference
{
    namespace
    {
        bool Fail(std::string& error, const char* message)
        {
            error = message;
            return false;
        }

        bool HasForeignHierarchy(Entity entity)
        {
            return entity && (entity.GetParent() || !entity.GetChildren().empty());
        }
    }

    bool ParseRTSPort(std::string_view text, u16& output, std::string& error)
    {
        if (text.empty() || text.size() > 5)
        {
            return Fail(error, "Port must contain one to five ASCII digits in 1..65535.");
        }
        u32 value = 0;
        for (char digit : text)
        {
            if (digit < '0' || digit > '9')
            {
                return Fail(error, "Port must contain only ASCII digits, without signs or spaces.");
            }
            value = value * 10 + static_cast<u32>(digit - '0');
        }
        if (value == 0 || value > (std::numeric_limits<u16>::max)())
        {
            return Fail(error, "Port must be in the range 1..65535.");
        }
        output = static_cast<u16>(value);
        error.clear();
        return true;
    }

    RTSMatch::RTSMatch(Scene& scene, RTSUnitStore& units, RTSSimulation& simulation,
        const ResourceRegistry* registry)
        : m_scene(scene)
        , m_units(units)
        , m_simulation(simulation)
        , m_registry(registry)
        , m_replica(scene, units, registry)
        , m_hostSession(units, simulation)
        , m_hostConnection(m_hostSession)
    {
    }

    RTSMatch::~RTSMatch()
    {
        std::string error;
        if (!Stop(error))
        {
            PYRAMID_LOG_WARN("RTS match cleanup blocked: ", error);
        }
    }

    bool RTSMatch::SetCatalog(
        u32 catalog, const MeshRendererComponent& renderer, std::string& error)
    {
        if (!m_replica.SetCatalog(catalog, renderer, error))
        {
            return false;
        }
        m_catalogs[catalog] = renderer;
        error.clear();
        return true;
    }

    bool RTSMatch::ValidateReferenceCatalog(std::string& error)
    {
        const auto catalog = m_catalogs.find(1);
        if (catalog == m_catalogs.end())
        {
            return Fail(error, "Register reference unit catalog 1 before starting a match.");
        }
        // A cached catalog is not permission to reuse expired resource generations.
        return m_replica.SetCatalog(1, catalog->second, error);
    }

    bool RTSMatch::CreateLocalUnit(u32 owner, const Math::Vec3& position, std::string& error)
    {
        Entity entity = m_scene.CreateEntity("Reference Player " + std::to_string(owner));
        if (!entity)
        {
            return Fail(error, "Scene could not create the reference match unit.");
        }
        const auto catalog = m_catalogs.find(1);
        if (catalog == m_catalogs.end() ||
            !entity.SetLocalPosition(position) ||
            !entity.SetMeshRenderer(catalog->second, m_registry))
        {
            (void)m_scene.DestroyEntity(entity);
            return Fail(error, "Reference unit renderer or transform could not be published.");
        }
        RTSUnitState initial;
        initial.owner = owner;
        initial.team = owner;
        if (!m_units.AddUnit(entity, initial))
        {
            (void)m_scene.DestroyEntity(entity);
            return Fail(error, "Reference unit gameplay state could not be registered.");
        }
        m_localUnits.push_back(entity);
        return true;
    }

    bool RTSMatch::Stop(std::string& error)
    {
        // A match owns the whole borrowed simulation queue, but never the whole scene.
        m_hostConnection.Stop();
        m_clientConnection.Stop();
        m_hostSession.End();
        m_simulation.Suspend();
        m_mode = RTSMatchMode::None;
        m_offlineWasActive = false;
        m_hostWasReady = false;
        m_lastSimulation = {};

        for (Entity entity : m_localUnits)
        {
            (void)m_units.ClearMovementTarget(entity);
            if (HasForeignHierarchy(entity))
            {
                m_status = "Match stopped; cleanup blocked by an externally edited unit hierarchy";
                return Fail(error, "Detach foreign parents/children from managed match units before cleanup.");
            }
        }
        if (!m_replica.Clear(error))
        {
            m_status = "Match stopped; replica cleanup blocked: " + error;
            return false;
        }
        for (Entity entity : m_localUnits)
        {
            if (entity)
            {
                (void)m_units.RemoveUnit(entity);
                (void)m_scene.DestroyEntity(entity);
            }
        }
        m_localUnits.clear();
        m_units.PruneInvalidEntities();
        m_nextSequence = 1;
        m_lastSnapshotTick = 0;
        m_status = "No active match";
        error.clear();
        return true;
    }

    bool RTSMatch::StopWithFailure(const std::string& reason, std::string& error)
    {
        const std::string diagnostic = reason;
        std::string cleanupError;
        const bool cleaned = Stop(cleanupError);
        error = diagnostic;
        if (!cleaned)
        {
            error += " Cleanup also failed: " + cleanupError;
        }
        m_status = "Match ended: " + error;
        return false;
    }

    bool RTSMatch::StartOffline(std::string& error)
    {
        if (!ValidateReferenceCatalog(error) || !Stop(error))
        {
            return false;
        }
        if (!CreateLocalUnit(1, Math::Vec3::Zero, error))
        {
            return StopWithFailure(error, error);
        }
        m_mode = RTSMatchMode::Offline;
        m_status = "Offline - player 1";
        error.clear();
        return true;
    }

    bool RTSMatch::Host(const Platform::TcpEndpoint& endpoint, u64 epoch,
        const Protocol::ContentId& content, std::string& error)
    {
        if (!ValidateReferenceCatalog(error) || !Stop(error))
        {
            return false;
        }
        if (!m_hostSession.Begin(epoch, content, error))
        {
            return StopWithFailure(error, error);
        }
        Protocol::Hello hello;
        hello.content = content;
        if (!m_hostSession.AddPeer(11, 1, hello, error))
        {
            return StopWithFailure(error, error);
        }
        for (u32 owner : {1U, 2U})
        {
            const f32 x = owner == 1 ? -2.0f : 2.0f;
            if (!CreateLocalUnit(owner, Math::Vec3(x, 0.0f, 0.0f), error))
            {
                return StopWithFailure(error, error);
            }
            u64 networkId = 0;
            if (!m_hostSession.BindUnit(m_localUnits.back(), 1, networkId, error))
            {
                return StopWithFailure(error, error);
            }
        }
        if (!m_hostConnection.Listen(endpoint, 22, 2, error))
        {
            return StopWithFailure(error, error);
        }
        m_mode = RTSMatchMode::Host;
        m_status = "Hosting on port " + std::to_string(m_hostConnection.GetLocalPort()) +
            " - waiting for player 2";
        error.clear();
        return true;
    }

    bool RTSMatch::Join(const Platform::TcpEndpoint& endpoint,
        const Protocol::ContentId& content, std::string& error)
    {
        if (!Stop(error))
        {
            return false;
        }
        if (!m_clientConnection.Connect(endpoint, content, error))
        {
            return StopWithFailure(error, error);
        }
        m_mode = RTSMatchMode::Client;
        m_status = "Joining " + endpoint.address + ":" + std::to_string(endpoint.port) +
            " as player 2";
        error.clear();
        return true;
    }

    bool RTSMatch::Update(f64 elapsedSeconds, bool localPaused, std::string& error)
    {
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
        {
            return Fail(error, "Match elapsed time must be finite and nonnegative.");
        }
        m_lastSimulation = {};
        if (m_mode == RTSMatchMode::None)
        {
            error.clear();
            return true;
        }
        if (m_mode == RTSMatchMode::Offline)
        {
            if (localPaused)
            {
                m_simulation.Suspend();
                m_offlineWasActive = false;
                m_status = "Offline paused - player 1";
            }
            else
            {
                m_lastSimulation = m_simulation.Advance(m_offlineWasActive ? elapsedSeconds : 0.0);
                m_offlineWasActive = true;
                m_status = "Offline - player 1";
            }
            error.clear();
            return true;
        }
        if (m_mode == RTSMatchMode::Host)
        {
            if (!m_hostConnection.Poll(elapsedSeconds, error))
            {
                return StopWithFailure(error, error);
            }
            if (m_hostConnection.GetState() != RTSConnectionState::Ready)
            {
                m_simulation.Suspend();
                m_hostWasReady = false;
                m_status = "Hosting on port " + std::to_string(m_hostConnection.GetLocalPort()) +
                    " - waiting for player 2";
                error.clear();
                return true;
            }
            m_status = "Host ready - player 1; simulation continues while local menus are open";
            m_lastSimulation = m_simulation.Advance(m_hostWasReady ? elapsedSeconds : 0.0);
            if (!m_hostWasReady)
            {
                m_lastSnapshotTick = m_simulation.GetTickCount();
            }
            m_hostWasReady = true;
            const u64 tick = m_simulation.GetTickCount();
            if (tick >= m_lastSnapshotTick && tick - m_lastSnapshotTick >= 4)
            {
                std::string publicationError;
                const RTSSnapshotPublication result = m_hostConnection.TryPublishSnapshot(publicationError);
                if (result == RTSSnapshotPublication::Queued)
                {
                    m_lastSnapshotTick = tick;
                }
                else if (result == RTSSnapshotPublication::Backpressure)
                {
                    // Retry only transient queue pressure without replacing partial work.
                    m_status = "Host ready; snapshot publication pending: " + publicationError;
                }
                else
                {
                    return StopWithFailure(publicationError, error);
                }
            }
            error.clear();
            return true;
        }
        if (!m_clientConnection.Poll(elapsedSeconds, error))
        {
            return StopWithFailure(error, error);
        }
        const Protocol::Message* message = m_clientConnection.GetLatestSnapshot();
        if (message)
        {
            const auto* snapshot = std::get_if<Protocol::FullSnapshot>(&message->body);
            if (!snapshot)
            {
                return StopWithFailure("Client connection did not publish a full snapshot.", error);
            }
            if (m_replica.GetSessionId() == 0 && !m_replica.Begin(message->session, error))
            {
                return StopWithFailure(error, error);
            }
            if (!m_replica.HasSnapshot() || snapshot->tick > m_replica.GetLastTick())
            {
                if (!m_replica.Apply(*message, error))
                {
                    return StopWithFailure(error, error);
                }
            }
        }
        m_status = IsReady() ? "Client ready - player 2; authoritative snapshots only"
            : "Joining as player 2 - waiting for host acceptance";
        error.clear();
        return true;
    }

    bool RTSMatch::QueueCommand(const RTSCommandRequest& command, std::string& error)
    {
        if (!IsReady())
        {
            return Fail(error, "Wait for the match to become ready before issuing commands.");
        }
        if (!command.entityRef || command.entity != command.entityRef.GetId())
        {
            return Fail(error, "Command does not refer to the selected live entity lifetime.");
        }
        const RTSUnitState* state = m_units.FindUnit(command.entityRef);
        if (!state || !state->IsAlive() || state->owner != GetLocalOwner() ||
            command.entityRef.GetParent())
        {
            return Fail(error, "Commands require a live root unit owned by the local player.");
        }
        if (m_mode == RTSMatchMode::Client)
        {
            const u64 networkId = m_replica.FindNetworkId(command.entityRef);
            if (networkId == 0 || m_nextSequence == 0)
            {
                return Fail(error, "Selected replica has no current network identity, or command sequences are exhausted.");
            }
            const Protocol::MoveCommand wire{m_nextSequence, networkId, command.target};
            if (!m_clientConnection.QueueMove(wire, error))
            {
                return false;
            }
            m_nextSequence = m_nextSequence == (std::numeric_limits<u64>::max)()
                ? 0 : m_nextSequence + 1;
        }
        else
        {
            if (std::none_of(m_localUnits.begin(), m_localUnits.end(),
                [&command](Entity entity) { return entity == command.entityRef; }))
            {
                return Fail(error, "Command unit is not a member of this reference match.");
            }
            if (m_mode == RTSMatchMode::Host &&
                (!std::isfinite(command.target.x) || !std::isfinite(command.target.y) ||
                    !std::isfinite(command.target.z) ||
                    std::fabs(command.target.x) > Protocol::MaximumCoordinate ||
                    std::fabs(command.target.y) > Protocol::MaximumCoordinate ||
                    std::fabs(command.target.z) > Protocol::MaximumCoordinate))
            {
                return Fail(error, "Host movement target exceeds the finite network coordinate range.");
            }
            if (!m_simulation.QueueMoveCommand(GetLocalOwner(), command))
            {
                return Fail(error, "Simulation command queue is full or the request is malformed.");
            }
        }
        error.clear();
        return true;
    }

    u32 RTSMatch::GetLocalOwner() const
    {
        if (m_mode == RTSMatchMode::Client) return 2;
        return m_mode == RTSMatchMode::Offline || m_mode == RTSMatchMode::Host ? 1 : 0;
    }

    bool RTSMatch::IsReady() const
    {
        if (m_mode == RTSMatchMode::Offline) return true;
        if (m_mode == RTSMatchMode::Host)
        {
            return m_hostSession.IsActive() && m_hostConnection.GetState() == RTSConnectionState::Ready;
        }
        return m_mode == RTSMatchMode::Client &&
            m_clientConnection.GetState() == RTSConnectionState::Ready && m_replica.HasSnapshot();
    }

    Entity RTSMatch::GetPrimaryEntity() const
    {
        if ((m_mode == RTSMatchMode::Offline || m_mode == RTSMatchMode::Host) &&
            !m_localUnits.empty() && m_localUnits.front())
        {
            return m_localUnits.front();
        }
        return {};
    }

    u16 RTSMatch::GetHostPort() const
    {
        return m_mode == RTSMatchMode::Host ? m_hostConnection.GetLocalPort() : 0;
    }

    u64 RTSMatch::GetTick() const
    {
        return m_mode == RTSMatchMode::Client ? m_replica.GetLastTick() : m_simulation.GetTickCount();
    }
}
