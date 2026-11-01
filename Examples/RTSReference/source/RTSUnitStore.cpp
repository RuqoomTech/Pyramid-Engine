#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>

#include <cmath>

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
            (initial.movementTarget &&
                (!initial.IsAlive() || initial.owner == 0 ||
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
        if (!state || !state->IsAlive() || state->owner != issuer)
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
