#include <Pyramid/Examples/RTSReference/RTSSimulation.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>

#include <cmath>
#include <limits>

namespace Pyramid::Examples::RTSReference
{
    RTSSimulation::RTSSimulation(RTSUnitStore& units)
        : m_units(units)
    {
    }

    bool RTSSimulation::QueueMoveCommand(u32 issuer, const RTSCommandRequest& command)
    {
        if (m_commands.size() >= MaximumQueuedCommands || issuer == 0 ||
            !command.entityRef || command.entity != command.entityRef.GetId() ||
            !std::isfinite(command.target.x) || !std::isfinite(command.target.y) ||
            !std::isfinite(command.target.z))
        {
            return false;
        }
        m_commands.push_back({issuer, command});
        return true;
    }

    RTSSimulationResult RTSSimulation::Advance(f64 elapsedSeconds)
    {
        RTSSimulationResult result;
        if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0)
        {
            result.valid = false;
            return result;
        }

        // Split before addition/division so huge finite elapsed time cannot
        // overflow an elapsed/step integer conversion or the small accumulator.
        const f64 remainder = std::fmod(elapsedSeconds, FixedStepSeconds);
        f64 wholeSeconds = elapsedSeconds - remainder;
        m_accumulator += remainder;
        bool carryTick = m_accumulator >= FixedStepSeconds;
        if (carryTick)
        {
            m_accumulator -= FixedStepSeconds;
        }

        while (result.steps < MaximumStepsPerAdvance &&
            (wholeSeconds >= FixedStepSeconds || carryTick))
        {
            if (wholeSeconds >= FixedStepSeconds)
            {
                wholeSeconds -= FixedStepSeconds;
            }
            else
            {
                carryTick = false;
            }

            while (!m_commands.empty())
            {
                const QueuedCommand& command = m_commands.front();
                if (m_units.ApplyMoveCommand(command.issuer, command.request))
                {
                    ++result.acceptedCommands;
                }
                else
                {
                    ++result.rejectedCommands;
                }
                m_commands.pop_front();
            }
            result.cancelledMoves += m_units.StepMovement(FixedStepSeconds);
            ++result.steps;
            ++m_tickCount;
        }

        result.droppedSeconds = wholeSeconds;
        if (carryTick)
        {
            const f64 maximum = (std::numeric_limits<f64>::max)();
            result.droppedSeconds = maximum - wholeSeconds < FixedStepSeconds
                ? maximum
                : wholeSeconds + FixedStepSeconds;
        }
        return result;
    }

    void RTSSimulation::Suspend()
    {
        m_accumulator = 0.0;
        m_commands.clear();
    }
}
