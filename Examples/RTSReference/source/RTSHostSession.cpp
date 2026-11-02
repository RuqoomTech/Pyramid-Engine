#include <Pyramid/Examples/RTSReference/RTSHostSession.hpp>
#include <Pyramid/Examples/RTSReference/RTSSimulation.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>

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

        bool MakeUnitSnapshot(
            const RTSUnitStore& units, Entity entity, u32 catalog,
            Protocol::UnitSnapshot& output, std::string& error)
        {
            const RTSUnitState* state = units.FindUnit(entity);
            const TransformComponent* transform = entity.GetTransform();
            if (!state || !transform)
            {
                return Fail(error, "Unit binding no longer has live gameplay state.");
            }
            if (entity.GetParent())
            {
                return Fail(error, "Version-1 snapshots support root units only; detach the unit before capture.");
            }
            output.catalog = catalog;
            output.owner = state->owner;
            output.team = state->team;
            output.maximumHealth = state->maximumHealth;
            output.health = state->health;
            output.movementSpeed = state->movementSpeed;
            output.position = transform->position;
            output.rotation = transform->rotation;
            output.scale = transform->scale;
            output.selectable = state->selectable;
            output.movementTarget = state->movementTarget;
            return true;
        }
    }

    RTSHostSession::RTSHostSession(RTSUnitStore& units, RTSSimulation& simulation)
        : m_units(units), m_simulation(simulation)
    {
    }

    RTSHostSession::~RTSHostSession()
    {
        End();
    }

    bool RTSHostSession::Begin(u64 session, const Protocol::ContentId& content, std::string& error)
    {
        if (m_active)
        {
            return Fail(error, "End the active host session before beginning another.");
        }
        if (session == 0 || session <= m_lastSession)
        {
            return Fail(error, "Host session epoch must be nonzero and greater than every previous epoch.");
        }
        if (std::all_of(content.begin(), content.end(), [](u8 value) { return value == 0; }))
        {
            return Fail(error, "Host content compatibility ID must not be all zero.");
        }
        m_simulation.Suspend();
        m_session = session;
        m_lastSession = session;
        m_content = content;
        m_nextUnit = 1;
        m_active = true;
        error.clear();
        return true;
    }

    void RTSHostSession::End()
    {
        if (!m_active)
        {
            return;
        }
        m_simulation.Suspend();
        for (const auto& binding : m_bindings)
        {
            (void)m_units.ClearMovementTarget(binding.second.entity);
        }
        m_peers.clear();
        m_bindings.clear();
        m_session = 0;
        m_content = {};
        m_active = false;
    }

    bool RTSHostSession::AddPeer(
        u64 peer, u32 owner, const Protocol::Hello& hello, std::string& error)
    {
        if (!m_active)
        {
            return Fail(error, "Begin a host session before assigning peers.");
        }
        if (peer == 0 || owner == 0 || m_peers.find(peer) != m_peers.end())
        {
            return Fail(error, "Peer and owner must be nonzero, and peer IDs must be unique.");
        }
        if (m_peers.size() >= MaximumPeers)
        {
            return Fail(error, "The reference host session supports at most two peers.");
        }
        for (const auto& existing : m_peers)
        {
            if (existing.second.owner == owner)
            {
                return Fail(error, "Each reference peer requires a distinct owner assignment.");
            }
        }
        Protocol::Error diagnostic;
        if (!Protocol::ValidateCompatibility(hello, m_content, diagnostic))
        {
            error = "Peer compatibility rejected: " + diagnostic.message;
            return false;
        }
        m_peers.emplace(peer, Peer{owner, 0});
        error.clear();
        return true;
    }

    bool RTSHostSession::BindUnit(Entity entity, u32 catalog, u64& networkId, std::string& error)
    {
        if (!m_active)
        {
            return Fail(error, "Begin a host session before binding units.");
        }
        if (m_bindings.size() >= Protocol::MaximumUnits || m_nextUnit == 0)
        {
            return Fail(error, "Network unit binding limit or identifier space exhausted; prune expired bindings or end the session.");
        }
        for (const auto& binding : m_bindings)
        {
            if (binding.second.entity == entity)
            {
                return Fail(error, "This entity lifetime already has a network unit binding.");
            }
        }
        Protocol::UnitSnapshot state;
        state.unit = m_nextUnit;
        if (!MakeUnitSnapshot(m_units, entity, catalog, state, error))
        {
            return false;
        }
        Protocol::FullSnapshot roster;
        roster.units.push_back(state);
        const Protocol::Message candidate{m_session, std::move(roster)};
        Protocol::Error diagnostic;
        if (!Protocol::Validate(candidate, diagnostic))
        {
            error = "Unit binding cannot be represented by the protocol: " + diagnostic.message;
            return false;
        }
        const u64 assigned = m_nextUnit;
        m_bindings.emplace(assigned, UnitBinding{entity, catalog});
        m_nextUnit = assigned == (std::numeric_limits<u64>::max)() ? 0 : assigned + 1;
        networkId = assigned;
        error.clear();
        return true;
    }

    std::size_t RTSHostSession::PruneInvalidUnits()
    {
        std::size_t removed = 0;
        for (auto binding = m_bindings.begin(); binding != m_bindings.end();)
        {
            if (!binding->second.entity)
            {
                binding = m_bindings.erase(binding);
                ++removed;
            }
            else
            {
                ++binding;
            }
        }
        return removed;
    }

    bool RTSHostSession::SubmitMove(
        u64 peer, const Protocol::Message& message, std::string& error)
    {
        if (!m_active || message.session != m_session)
        {
            return Fail(error, "Move request belongs to an inactive or different host session.");
        }
        auto sender = m_peers.find(peer);
        if (sender == m_peers.end())
        {
            return Fail(error, "Move request has no assigned host peer.");
        }
        const auto* command = std::get_if<Protocol::MoveCommand>(&message.body);
        if (!command)
        {
            return Fail(error, "Only a MoveCommand message can enter the host command queue.");
        }
        Protocol::Error diagnostic;
        if (!Protocol::Validate(message, diagnostic))
        {
            error = "Malformed move request: " + diagnostic.message;
            return false;
        }
        if (command->sequence <= sender->second.lastSequence)
        {
            return Fail(error, "Move sequence must increase without replay or wrap; gaps are allowed.");
        }
        const auto binding = m_bindings.find(command->unit);
        if (binding == m_bindings.end())
        {
            return Fail(error, "Move request refers to an unknown network unit ID.");
        }
        const Entity entity = binding->second.entity;
        const RTSUnitState* state = m_units.FindUnit(entity);
        if (!state || !state->IsAlive() || state->owner != sender->second.owner)
        {
            return Fail(error, "Move request targets an expired, dead, or non-owned unit.");
        }
        const TransformComponent* transform = entity.GetTransform();
        if (!transform || entity.GetParent() ||
            !std::isfinite(transform->position.x) || !std::isfinite(transform->position.y) ||
            !std::isfinite(transform->position.z))
        {
            return Fail(error, "Move request requires a live root unit with a finite position.");
        }
        const RTSCommandRequest local{entity.GetId(), command->target, entity};
        if (!m_simulation.QueueMoveCommand(sender->second.owner, local))
        {
            return Fail(error, "Simulation command queue is full; the same sequence may be retried.");
        }
        sender->second.lastSequence = command->sequence;
        error.clear();
        return true;
    }

    bool RTSHostSession::CaptureSnapshot(
        u64 peer, Protocol::Message& output, std::string& error) const
    {
        const auto recipient = m_peers.find(peer);
        if (!m_active || recipient == m_peers.end())
        {
            return Fail(error, "Snapshot capture requires an active assigned peer.");
        }
        Protocol::FullSnapshot snapshot;
        snapshot.tick = m_simulation.GetTickCount();
        snapshot.lastQueuedSequence = recipient->second.lastSequence;
        snapshot.units.reserve(m_bindings.size());
        for (const auto& binding : m_bindings)
        {
            const Entity entity = binding.second.entity;
            if (!entity || !m_units.FindUnit(entity))
            {
                continue;
            }
            Protocol::UnitSnapshot state;
            state.unit = binding.first;
            if (!MakeUnitSnapshot(m_units, entity, binding.second.catalog, state, error))
            {
                return false;
            }
            snapshot.units.push_back(std::move(state));
        }
        Protocol::Message candidate{m_session, std::move(snapshot)};
        Protocol::Error diagnostic;
        if (!Protocol::Validate(candidate, diagnostic))
        {
            error = "Snapshot capture rejected invalid host state: " + diagnostic.message;
            return false;
        }
        output = std::move(candidate);
        error.clear();
        return true;
    }
}
