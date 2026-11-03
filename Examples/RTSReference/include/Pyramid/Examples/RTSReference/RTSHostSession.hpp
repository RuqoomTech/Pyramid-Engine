#pragma once

#include <Pyramid/Examples/RTSReference/RTSProtocol.hpp>
#include <Pyramid/Graphics/Scene/Entity.hpp>

#include <cstddef>
#include <map>
#include <string>

namespace Pyramid::Examples::RTSReference
{
    class RTSUnitStore;
    class RTSSimulation;

    /**
     * Local host-side admission/capture adapter, not a transport or client scene.
     *
     * Borrowed store/simulation must outlive this single-threaded adapter. A session
     * exclusively owns the simulation command queue. End (also on destruction)
     * discards all pending requests and clears movement on bound live units.
     * Disconnect terminates this entire reference session; removing one peer alone
     * cannot revoke requests that the simulation already captured.
     */
    class RTSHostSession final
    {
    public:
        static constexpr std::size_t MaximumPeers = 2;

        RTSHostSession(RTSUnitStore& units, RTSSimulation& simulation);
        ~RTSHostSession();
        RTSHostSession(const RTSHostSession&) = delete;
        RTSHostSession& operator=(const RTSHostSession&) = delete;

        /** Requires a fresh increasing nonzero epoch and nonzero compatibility ID. */
        [[nodiscard]] bool Begin(u64 session, const Protocol::ContentId& content, std::string& error);
        void End();
        [[nodiscard]] bool IsActive() const { return m_active; }
        [[nodiscard]] u64 GetSessionId() const { return m_session; }
        [[nodiscard]] const Protocol::ContentId& GetContentId() const { return m_content; }

        /** Native connection assignment supplies owner; it never comes from a command. */
        [[nodiscard]] bool AddPeer(
            u64 peer, u32 owner, const Protocol::Hello& hello, std::string& error);
        /** Assigns a non-reused session-local network ID to a captured entity lifetime. */
        [[nodiscard]] bool BindUnit(Entity entity, u32 catalog, u64& networkId, std::string& error);
        /** Reclaims only expired entity bindings; never reuses their network IDs. */
        std::size_t PruneInvalidUnits();

        /**
         * Strictly increasing sequences (gaps allowed) advance only after enqueue.
         * Queue success is not execution acceptance; the tick revalidates authority.
         * Failed calls leave sequence and pending simulation work unchanged.
         */
        [[nodiscard]] bool SubmitMove(u64 peer, const Protocol::Message& message, std::string& error);
        /** Atomic full roster capture; lastQueuedSequence acknowledges enqueue only. */
        [[nodiscard]] bool CaptureSnapshot(u64 peer, Protocol::Message& output, std::string& error) const;

    private:
        struct Peer
        {
            u32 owner = 0;
            u64 lastSequence = 0;
        };

        struct UnitBinding
        {
            Entity entity;
            u32 catalog = 0;
        };

        RTSUnitStore& m_units;
        RTSSimulation& m_simulation;
        Protocol::ContentId m_content{};
        std::map<u64, Peer> m_peers;
        std::map<u64, UnitBinding> m_bindings;
        u64 m_session = 0;
        u64 m_lastSession = 0;
        u64 m_nextUnit = 1;
        bool m_active = false;
    };
}
