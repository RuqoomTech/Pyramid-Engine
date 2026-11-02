#pragma once

#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>

#include <cstddef>
#include <deque>

namespace Pyramid::Examples::RTSReference
{
    class RTSUnitStore;

    struct RTSSimulationResult
    {
        bool valid = true;
        u32 steps = 0;
        std::size_t acceptedCommands = 0;
        std::size_t rejectedCommands = 0;
        std::size_t cancelledMoves = 0;
        f64 droppedSeconds = 0.0;
    };

    /**
     * Single-threaded reference simulation; the borrowed store must outlive it.
     *
     * Queue input once per frame. All queued requests execute in FIFO order before
     * movement on the next due tick, with authority/lifetime checked at execution.
     * Root entities move in straight world-space lines; this is not navigation,
     * collision handling, network scheduling, or cross-platform deterministic replay.
     */
    class RTSSimulation final
    {
    public:
        static constexpr f64 FixedStepSeconds = 1.0 / 32.0;
        static constexpr u32 MaximumStepsPerAdvance = 8;
        static constexpr std::size_t MaximumQueuedCommands = 256;

        explicit RTSSimulation(RTSUnitStore& units);
        RTSSimulation(const RTSSimulation&) = delete;
        RTSSimulation& operator=(const RTSSimulation&) = delete;

        /**
         * Rejects malformed local references/targets, issuer zero, or a full queue
         * without replacing accepted requests. Ownership/health/root status are
         * revalidated on the tick, not promised by successful queue insertion.
         */
        [[nodiscard]] bool QueueMoveCommand(u32 issuer, const RTSCommandRequest& command);

        /**
         * Rejects negative/nonfinite elapsed time without changing any state.
         * Executes at most eight ticks, drops excess whole ticks and reports their
         * duration, retaining only the fractional remainder. This bounds catch-up
         * work even for huge finite elapsed values; no command is polled per tick.
         */
        [[nodiscard]] RTSSimulationResult Advance(f64 elapsedSeconds);

        /** Clears remainder and unexecuted requests, preserving ticks and admitted movement. */
        void Suspend();
        [[nodiscard]] std::size_t GetPendingCommandCount() const { return m_commands.size(); }
        [[nodiscard]] u64 GetTickCount() const { return m_tickCount; }

    private:
        struct QueuedCommand
        {
            u32 issuer;
            RTSCommandRequest request;
        };

        RTSUnitStore& m_units;
        std::deque<QueuedCommand> m_commands;
        f64 m_accumulator = 0.0;
        u64 m_tickCount = 0;
    };
}
