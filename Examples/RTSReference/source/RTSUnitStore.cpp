#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>

#include <cmath>
#include <limits>

namespace Pyramid::Examples::RTSReference
{
    namespace
    {
        [[nodiscard]] bool IsFinite(const Math::Vec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                std::isfinite(value.z);
        }
    }

    bool RTSUnitStore::AddUnit(Entity entity, const RTSUnitState& initial)
    {
        if (!entity || initial.maximumHealth == 0 ||
            initial.health > initial.maximumHealth ||
            !std::isfinite(initial.movementSpeed) || initial.movementSpeed < 0.0f ||
            (initial.movementTarget &&
                (!initial.IsAlive() || initial.owner == 0 || entity.GetParent() ||
                    !IsFinite(entity.GetTransform()->position) ||
                    !IsFinite(*initial.movementTarget))))
        {
            return false;
        }

        return m_units.emplace(entity.GetId(), Entry{entity, initial}).second;
    }

    bool RTSUnitStore::RemoveUnit(Entity entity)
    {
        if (!FindUnit(entity))
        {
            return false;
        }
        m_units.erase(entity.GetId());
        return true;
    }

    const RTSUnitState* RTSUnitStore::FindUnit(Entity entity) const
    {
        if (!entity)
        {
            return nullptr;
        }
        const auto found = m_units.find(entity.GetId());
        return found != m_units.end() && found->second.entity == entity
            ? &found->second.state
            : nullptr;
    }

    RTSUnitState* RTSUnitStore::FindMutableUnit(Entity entity)
    {
        if (!entity)
        {
            return nullptr;
        }
        const auto found = m_units.find(entity.GetId());
        return found != m_units.end() && found->second.entity == entity
            ? &found->second.state
            : nullptr;
    }

    bool RTSUnitStore::IsSelectable(Entity entity) const
    {
        const RTSUnitState* state = FindUnit(entity);
        return state && state->IsAlive() && state->selectable;
    }

    bool RTSUnitStore::ApplyDamage(Entity entity, u32 amount)
    {
        RTSUnitState* state = FindMutableUnit(entity);
        if (!state)
        {
            return false;
        }
        state->health = amount >= state->health ? 0 : state->health - amount;
        if (!state->IsAlive())
        {
            state->movementTarget.reset();
        }
        return true;
    }

    bool RTSUnitStore::SetOwnership(Entity entity, u32 owner, u32 team)
    {
        RTSUnitState* state = FindMutableUnit(entity);
        if (!state)
        {
            return false;
        }
        if (state->owner != owner || state->team != team)
        {
            state->movementTarget.reset();
        }
        state->owner = owner;
        state->team = team;
        return true;
    }

    bool RTSUnitStore::SetSelectable(Entity entity, bool selectable)
    {
        RTSUnitState* state = FindMutableUnit(entity);
        if (!state)
        {
            return false;
        }
        state->selectable = selectable;
        return true;
    }

    bool RTSUnitStore::ApplyMoveCommand(u32 issuer, const RTSCommandRequest& command)
    {
        if (issuer == 0 || !command.entityRef ||
            command.entity != command.entityRef.GetId() || !IsFinite(command.target))
        {
            return false;
        }
        RTSUnitState* state = FindMutableUnit(command.entityRef);
        if (!state || !state->IsAlive() || state->owner != issuer ||
            command.entityRef.GetParent() ||
            !IsFinite(command.entityRef.GetTransform()->position))
        {
            return false;
        }
        state->movementTarget = command.target;
        return true;
    }

    bool RTSUnitStore::ClearMovementTarget(Entity entity)
    {
        RTSUnitState* state = FindMutableUnit(entity);
        if (!state)
        {
            return false;
        }
        state->movementTarget.reset();
        return true;
    }

    std::size_t RTSUnitStore::StepMovement(f64 stepSeconds)
    {
        std::size_t cancelled = 0;
        for (auto& item : m_units)
        {
            Entry& entry = item.second;
            RTSUnitState& state = entry.state;
            if (!state.movementTarget)
            {
                continue;
            }
            const auto cancel = [&state, &cancelled]()
            {
                state.movementTarget.reset();
                ++cancelled;
            };
            if (!entry.entity || !state.IsAlive() || state.owner == 0 ||
                entry.entity.GetParent() || !IsFinite(*state.movementTarget) ||
                !std::isfinite(state.movementSpeed) || state.movementSpeed < 0.0f)
            {
                cancel();
                continue;
            }

            // Root local position is world position. Never interpret a world
            // target in parent-local space or rely on a silently failed inverse.
            const Math::Vec3 current = entry.entity.GetTransform()->position;
            const Math::Vec3 target = *state.movementTarget;
            if (!IsFinite(current))
            {
                cancel();
                continue;
            }

            // f32 subtraction and squared length overflow for finite endpoints.
            const f64 dx = static_cast<f64>(target.x) - current.x;
            const f64 dy = static_cast<f64>(target.y) - current.y;
            const f64 dz = static_cast<f64>(target.z) - current.z;
            const f64 distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const f64 travel = static_cast<f64>(state.movementSpeed) * stepSeconds;
            if (!std::isfinite(distance) || !std::isfinite(travel))
            {
                cancel();
                continue;
            }
            const bool arrived = distance <= travel;
            if (!arrived && state.movementSpeed == 0.0f)
            {
                continue;
            }

            Math::Vec3 next = target;
            if (!arrived)
            {
                const f64 fraction = travel / distance;
                const f64 x = static_cast<f64>(current.x) + dx * fraction;
                const f64 y = static_cast<f64>(current.y) + dy * fraction;
                const f64 z = static_cast<f64>(current.z) + dz * fraction;
                const f64 maximum = (std::numeric_limits<f32>::max)();
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                    std::fabs(x) > maximum || std::fabs(y) > maximum ||
                    std::fabs(z) > maximum)
                {
                    cancel();
                    continue;
                }
                next = Math::Vec3(static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(z));
                if (next.x == current.x && next.y == current.y && next.z == current.z)
                {
                    // The requested displacement is below scene f32 precision.
                    // Cancel visibly instead of reporting endless zero progress.
                    cancel();
                    continue;
                }
            }

            if (!entry.entity.SetLocalPosition(next))
            {
                cancel();
                continue;
            }
            if (arrived)
            {
                state.movementTarget.reset();
            }
        }
        return cancelled;
    }

    std::size_t RTSUnitStore::PruneInvalidEntities()
    {
        std::size_t removed = 0;
        for (auto entry = m_units.begin(); entry != m_units.end();)
        {
            if (!entry->second.entity)
            {
                entry = m_units.erase(entry);
                ++removed;
            }
            else
            {
                ++entry;
            }
        }
        return removed;
    }

    void RTSUnitStore::Clear()
    {
        m_units.clear();
    }
}
