#pragma once

#include <Pyramid/Graphics/Scene/Entity.hpp>

#include <cstddef>
#include <map>
#include <optional>

namespace Pyramid::Examples::RTSReference
{
    struct RTSCommandRequest;
    class RTSSimulation;

    /** Game-owned state; transforms remain authoritative in the entity scene. */
    struct RTSUnitState
    {
        u32 owner = 0;
        u32 team = 0;
        u32 maximumHealth = 100;
        u32 health = 100;
        bool selectable = true;
        /** Finite nonnegative world units per second; zero deliberately holds position. */
        f32 movementSpeed = 4.0f;
        std::optional<Math::Vec3> movementTarget;

        [[nodiscard]] bool IsAlive() const { return health > 0; }
    };

    /**
     * Single-threaded, game-owned unit state for one gameplay scene.
     *
     * Entity facades are non-owning and retain their original lifetime identity.
     * Every lookup/mutation requires a live matching facade. Failed bool operations
     * leave state unchanged. Remove expired records with PruneInvalidEntities()
     * before registering a replacement with the same numeric ID.
     *
     * Owner zero is neutral and cannot issue commands. Team is metadata only;
     * selection does not imply command authority. State mutations other than move
     * admission are trusted game/host operations, not a network trust boundary.
     * Movement is root-only; reparenting an active unit cancels its order on the
     * next tick. Idle/dead units stay still, zero speed holds a live target, and
     * invalid or unrepresentable displacement cancels the order with diagnostics
     * counted by RTSSimulationResult::cancelledMoves. Entries advance by EntityId.
     */
    class RTSUnitStore final
    {
    public:
        [[nodiscard]] bool AddUnit(Entity entity, const RTSUnitState& initial = {});
        [[nodiscard]] bool RemoveUnit(Entity entity);

        /** Borrowed state; invalidated by removal, pruning, clear, or store destruction. */
        [[nodiscard]] const RTSUnitState* FindUnit(Entity entity) const;
        [[nodiscard]] bool IsSelectable(Entity entity) const;

        [[nodiscard]] bool ApplyDamage(Entity entity, u32 amount);
        /** Changes to owner or team cancel the previous movement target. */
        [[nodiscard]] bool SetOwnership(Entity entity, u32 owner, u32 team);
        [[nodiscard]] bool SetSelectable(Entity entity, bool selectable);
        /** Validates owner, lifetime, health, root transform, and target; records intent only. */
        [[nodiscard]] bool ApplyMoveCommand(u32 issuer, const RTSCommandRequest& command);
        [[nodiscard]] bool ClearMovementTarget(Entity entity);

        /** Returns the number of expired entries removed; never destroys entities. */
        std::size_t PruneInvalidEntities();
        void Clear();

    private:
        friend class RTSSimulation;

        struct Entry
        {
            Entity entity;
            RTSUnitState state;
        };

        [[nodiscard]] RTSUnitState* FindMutableUnit(Entity entity);

        /** Positive finite fixed step supplied only by RTSSimulation; returns cancelled orders. */
        std::size_t StepMovement(f64 stepSeconds);

        std::map<EntityId, Entry> m_units;
    };
}
