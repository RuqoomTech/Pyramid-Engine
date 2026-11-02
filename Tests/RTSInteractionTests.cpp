#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
#include <Pyramid/Examples/RTSReference/RTSSimulation.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "RTS interaction test failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    bool NearlyEqual(Pyramid::f32 left, Pyramid::f32 right, Pyramid::f32 epsilon = 0.001f)
    {
        return std::fabs(left - right) <= epsilon;
    }

    bool NearlyEqual(
        const Pyramid::Math::Vec3& left,
        const Pyramid::Math::Vec3& right,
        Pyramid::f32 epsilon = 0.001f)
    {
        return NearlyEqual(left.x, right.x, epsilon) &&
            NearlyEqual(left.y, right.y, epsilon) &&
            NearlyEqual(left.z, right.z, epsilon);
    }

    void AttachRenderable(
        Pyramid::Entity entity,
        const Pyramid::Math::Vec3& position = Pyramid::Math::Vec3::Zero,
        Pyramid::f32 halfExtent = 1.0f)
    {
        Require(entity.SetLocalPosition(position), "renderable fixture position failed");
        Pyramid::MeshRendererComponent renderer;
        renderer.boundsMode = Pyramid::RenderBoundsMode::Manual;
        renderer.localBoundsMin = Pyramid::Math::Vec3(-halfExtent);
        renderer.localBoundsMax = Pyramid::Math::Vec3(halfExtent);
        Require(entity.SetMeshRenderer(renderer), "renderable fixture component failed");
    }

    void TestUnitState()
    {
        using namespace Pyramid;
        using namespace Pyramid::Examples::RTSReference;

        Scene scene("Unit State Test");
        RTSUnitStore units;
        Entity unit = scene.CreateEntity("Unit");
        const Math::Vec3 originalPosition(2.0f, 3.0f, 4.0f);
        Require(unit.SetLocalPosition(originalPosition), "unit fixture position failed");
        RTSUnitState initial;
        initial.owner = 1;
        initial.team = 7;
        initial.maximumHealth = 80;
        initial.health = 60;
        Require(units.AddUnit(unit, initial), "valid unit registration failed");
        const RTSUnitState* state = units.FindUnit(unit);
        Require(state && state->owner == 1 && state->team == 7 &&
                state->maximumHealth == 80 && state->health == 60 &&
                state->selectable && state->IsAlive() && !state->movementTarget,
            "registered state differs from the definition");
        Require(!units.AddUnit(unit) && units.FindUnit(unit)->health == 60,
            "duplicate registration must preserve existing state");
        Require(!units.AddUnit({}) && !units.FindUnit({}) && !units.RemoveUnit({}),
            "empty entity references must be rejected");

        const Entity invalidUnit = scene.CreateEntity("Invalid Definition");
        RTSUnitState invalid = initial;
        invalid.maximumHealth = 0;
        Require(!units.AddUnit(invalidUnit, invalid), "zero maximum health was accepted");
        invalid = initial;
        invalid.health = invalid.maximumHealth + 1;
        Require(!units.AddUnit(invalidUnit, invalid), "health above maximum was accepted");
        invalid = initial;
        invalid.movementTarget = Math::Vec3(std::numeric_limits<f32>::quiet_NaN(), 0.0f, 0.0f);
        Require(!units.AddUnit(invalidUnit, invalid), "nonfinite initial target was accepted");
        invalid = initial;
        invalid.health = 0;
        invalid.movementTarget = Math::Vec3::Zero;
        Require(!units.AddUnit(invalidUnit, invalid), "dead initial movement was accepted");
        invalid = initial;
        invalid.owner = 0;
        invalid.movementTarget = Math::Vec3::Zero;
        Require(!units.AddUnit(invalidUnit, invalid) && !units.FindUnit(invalidUnit),
            "invalid neutral initial movement must not publish a unit");

        RTSUnitState withTarget = initial;
        withTarget.movementTarget = Math::Vec3(4.0f, 0.0f, 3.0f);
        Require(units.AddUnit(invalidUnit, withTarget) &&
                units.FindUnit(invalidUnit)->movementTarget &&
                NearlyEqual(*units.FindUnit(invalidUnit)->movementTarget, *withTarget.movementTarget),
            "valid initial movement target should survive transactional registration");
        Scene foreignScene("Foreign Unit State");
        const Entity foreign = foreignScene.CreateEntityWithId(unit.GetId(), "Same Numeric ID");
        Require(static_cast<bool>(foreign), "foreign same-ID fixture failed");
        Require(!units.FindUnit(foreign) && !units.RemoveUnit(foreign) &&
                !units.ApplyDamage(foreign, 1) && !units.SetOwnership(foreign, 2, 2) &&
                !units.SetSelectable(foreign, false) && !units.ClearMovementTarget(foreign),
            "foreign entities must not access another scene's unit state");

        const Math::Vec3 firstTarget(8.0f, 0.0f, -2.0f);
        const RTSCommandRequest move{unit.GetId(), firstTarget, unit};
        Require(units.ApplyMoveCommand(1, move), "owner move request was rejected");
        Require(units.FindUnit(unit)->movementTarget &&
                NearlyEqual(*units.FindUnit(unit)->movementTarget, firstTarget),
            "accepted target was not recorded");
        RTSCommandRequest rejected{unit.GetId(), Math::Vec3(99.0f), unit};
        Require(!units.ApplyMoveCommand(2, rejected) && !units.ApplyMoveCommand(0, rejected),
            "wrong or neutral issuer must not command a unit");
        rejected.entity = foreign.GetId();
        rejected.entityRef = foreign;
        Require(!units.ApplyMoveCommand(1, rejected), "foreign request identity was accepted");
        rejected.entityRef = unit;
        rejected.entity = invalidUnit.GetId();
        Require(!units.ApplyMoveCommand(1, rejected), "mismatched request ID was accepted");
        rejected.entity = unit.GetId();
        rejected.entityRef = {};
        Require(!units.ApplyMoveCommand(1, rejected), "ID-only move request was accepted");
        rejected.entityRef = unit;
        const f32 infinity = std::numeric_limits<f32>::infinity();
        const Math::Vec3 invalidTargets[] = {
            Math::Vec3(infinity, 0.0f, 0.0f),
            Math::Vec3(0.0f, -infinity, 0.0f),
            Math::Vec3(0.0f, 0.0f, std::numeric_limits<f32>::quiet_NaN())};
        for (const Math::Vec3& target : invalidTargets)
        {
            rejected.target = target;
            Require(!units.ApplyMoveCommand(1, rejected), "nonfinite move request was accepted");
        }
        Require(NearlyEqual(*units.FindUnit(unit)->movementTarget, firstTarget) &&
                units.FindUnit(unit)->health == 60,
            "rejected requests changed accepted state");
        Require(NearlyEqual(unit.GetWorldPosition(), originalPosition),
            "recording a movement target must not execute movement");

        Require(units.SetSelectable(unit, false) && !units.IsSelectable(unit),
            "selection policy change was not applied");
        Require(units.ApplyMoveCommand(1, move),
            "selection policy must not replace command ownership checks");
        Require(units.SetSelectable(unit, true) && units.IsSelectable(unit),
            "selection policy could not be restored");
        Require(units.SetOwnership(unit, 1, 7) && units.FindUnit(unit)->movementTarget,
            "unchanged ownership should preserve the existing order");
        Require(units.SetOwnership(unit, 2, 7) && !units.FindUnit(unit)->movementTarget,
            "ownership transfer must clear the previous owner's order");
        Require(!units.ApplyMoveCommand(1, move) && units.ApplyMoveCommand(2, move),
            "ownership transfer did not change command admission");
        Require(units.SetOwnership(unit, 2, 8) && !units.FindUnit(unit)->movementTarget,
            "team change must invalidate the previous order");
        Require(units.ApplyMoveCommand(2, move) && units.ClearMovementTarget(unit) &&
                !units.FindUnit(unit)->movementTarget && units.ClearMovementTarget(unit),
            "clearing an existing or empty target failed");
        Require(units.ApplyMoveCommand(2, move) && units.ApplyDamage(unit, 0) &&
                units.FindUnit(unit)->health == 60 && units.ApplyDamage(unit, 10) &&
                units.FindUnit(unit)->health == 50,
            "bounded damage did not preserve or reduce health correctly");
        Require(units.ApplyDamage(unit, std::numeric_limits<u32>::max()) &&
                units.FindUnit(unit)->health == 0 && !units.FindUnit(unit)->IsAlive() &&
                !units.FindUnit(unit)->movementTarget && !units.IsSelectable(unit) &&
                !units.ApplyMoveCommand(2, move),
            "lethal damage must saturate health, clear orders, and block selection/commands");
        Require(units.ApplyDamage(unit, 1) && units.FindUnit(unit)->health == 0 &&
                NearlyEqual(unit.GetWorldPosition(), originalPosition),
            "damage to a dead unit must not underflow or change its transform");

        Require(units.RemoveUnit(unit) && !units.FindUnit(unit) && unit,
            "removing gameplay state must leave the entity alive");
        Require(units.AddUnit(unit), "default unit registration failed");
        Require(units.FindUnit(unit)->owner == 0 && units.FindUnit(unit)->team == 0 &&
                units.FindUnit(unit)->health == 100 && units.FindUnit(unit)->maximumHealth == 100 &&
                units.IsSelectable(unit) && !units.ApplyMoveCommand(0, move) &&
                !units.ApplyMoveCommand(1, move),
            "default neutral state must be selectable but not commandable");
        units.Clear();
        Require(!units.FindUnit(unit) && unit && units.PruneInvalidEntities() == 0,
            "clearing gameplay state must preserve scene entities");
    }

    void TestUnitLifetimes()
    {
        using namespace Pyramid;
        using namespace Pyramid::Examples::RTSReference;

        RTSUnitStore units;
        RTSUnitState initial;
        initial.owner = 1;
        Scene scene("Unit Lifetimes");
        const Entity original = scene.CreateEntity("Original");
        Require(units.AddUnit(original, initial), "lifetime unit registration failed");
        const RTSCommandRequest oldCommand{original.GetId(), Math::Vec3(5.0f), original};
        Require(scene.DestroyEntity(original), "lifetime fixture destruction failed");
        const Entity replacement = scene.CreateEntityWithId(original.GetId(), "Replacement");
        Require(replacement && !units.FindUnit(original) && !units.FindUnit(replacement) &&
                !units.RemoveUnit(original) && !units.ApplyDamage(original, 1) &&
                !units.SetOwnership(original, 2, 1) && !units.SetSelectable(original, false) &&
                !units.ClearMovementTarget(original) && !units.IsSelectable(original),
            "destroyed or same-ID replacement facades must not access old gameplay state");
        Require(!units.AddUnit(replacement, initial) && units.PruneInvalidEntities() == 1 &&
                units.AddUnit(replacement, initial),
            "stale registration must be pruned before explicit ID reuse");
        Require(!units.ApplyMoveCommand(1, oldCommand) && !units.FindUnit(replacement)->movementTarget,
            "a delayed command must not retarget a same-ID replacement");

        const Entity parent = scene.CreateEntity("Parent");
        Entity child = scene.CreateEntity("Child");
        Require(child.SetParent(parent) && units.AddUnit(child, initial),
            "recursive lifetime fixture failed");
        Require(scene.DestroyEntity(parent) && !units.FindUnit(child) &&
                units.PruneInvalidEntities() == 1,
            "recursive scene destruction must expire child gameplay state");
        scene.Clear();
        Require(!units.FindUnit(replacement) && units.PruneInvalidEntities() == 1,
            "scene clear must expire registered unit facades");

        Entity expired;
        {
            Scene temporary("Temporary Scene");
            expired = temporary.CreateEntity("Temporary Unit");
            Require(units.AddUnit(expired, initial), "temporary unit registration failed");
        }
        Require(!units.FindUnit(expired) && !units.ApplyDamage(expired, 1) &&
                units.PruneInvalidEntities() == 1,
            "unit state must not extend a scene or entity lifetime");
    }

    void TestSimulationTimingAndQueue()
    {
        using namespace Pyramid;
        using namespace Pyramid::Examples::RTSReference;
        constexpr f64 step = RTSSimulation::FixedStepSeconds;

        const auto runPartitioned = [](bool divided)
        {
            Scene scene("Partitioned Simulation");
            RTSUnitStore units;
            const Entity unit = scene.CreateEntity("Moving Unit");
            RTSUnitState initial;
            initial.owner = 1;
            Require(units.AddUnit(unit, initial), "partition fixture registration failed");
            RTSSimulation simulation(units);
            Require(simulation.QueueMoveCommand(1, {unit.GetId(), Math::Vec3(100.0f, 0.0f, 0.0f), unit}),
                "partition fixture enqueue failed");
            std::size_t accepted = 0;
            if (divided)
            {
                for (u32 frame = 0; frame < 12; ++frame)
                {
                    const auto result = simulation.Advance(RTSSimulation::FixedStepSeconds * 0.5);
                    Require(result.valid && result.droppedSeconds == 0.0,
                        "subtick partition unexpectedly failed or dropped time");
                    accepted += result.acceptedCommands;
                }
            }
            else
            {
                const auto result = simulation.Advance(RTSSimulation::FixedStepSeconds * 6.0);
                Require(result.valid && result.steps == 6 && result.droppedSeconds == 0.0,
                    "single-frame partition did not execute six ticks");
                accepted = result.acceptedCommands;
            }
            Require(simulation.GetTickCount() == 6 && accepted == 1 &&
                    simulation.GetPendingCommandCount() == 0,
                "elapsed-time partitions must execute the request exactly once");
            return unit.GetWorldPosition();
        };
        Require(NearlyEqual(runPartitioned(false), runPartitioned(true)),
            "equal elapsed time must produce equal movement across frame partitions");

        Scene scene("Simulation Queue");
        RTSUnitStore units;
        const Entity unit = scene.CreateEntity("Queued Unit");
        RTSUnitState initial;
        initial.owner = 1;
        Require(units.AddUnit(unit, initial), "queue fixture registration failed");
        RTSSimulation simulation(units);
        const RTSCommandRequest first{unit.GetId(), Math::Vec3(-100.0f, 0.0f, 0.0f), unit};
        const RTSCommandRequest last{unit.GetId(), Math::Vec3(100.0f, 0.0f, 0.0f), unit};
        Require(simulation.QueueMoveCommand(1, first) &&
                simulation.QueueMoveCommand(2, first) && simulation.QueueMoveCommand(1, last),
            "FIFO fixture enqueue failed");
        const auto noTime = simulation.Advance(0.0);
        const auto half = simulation.Advance(step * 0.5);
        Require(noTime.valid && noTime.steps == 0 && half.steps == 0 &&
                simulation.GetPendingCommandCount() == 3 && !units.FindUnit(unit)->movementTarget &&
                NearlyEqual(unit.GetWorldPosition(), Math::Vec3::Zero),
            "zero/subtick frames must preserve queued intent without executing movement");
        const auto tick = simulation.Advance(step * 0.5);
        Require(tick.valid && tick.steps == 1 && tick.acceptedCommands == 2 &&
                tick.rejectedCommands == 1 && simulation.GetPendingCommandCount() == 0 &&
                units.FindUnit(unit)->movementTarget &&
                NearlyEqual(*units.FindUnit(unit)->movementTarget, last.target) &&
                NearlyEqual(unit.GetWorldPosition(), Math::Vec3(0.125f, 0.0f, 0.0f)),
            "tick must drain FIFO once, reject unauthorized intent, then move toward the last accepted target");
        const auto following = simulation.Advance(step * 3.0);
        Require(following.steps == 3 && following.acceptedCommands == 0 &&
                following.rejectedCommands == 0 && simulation.GetTickCount() == 4,
            "catch-up ticks must not replay input edges or consumed commands");

        RTSCommandRequest malformed = last;
        Require(!simulation.QueueMoveCommand(0, last), "neutral issuer was enqueued");
        malformed.entityRef = {};
        Require(!simulation.QueueMoveCommand(1, malformed), "ID-only request was enqueued");
        malformed = last;
        malformed.entity = EntityId(999999);
        Require(!simulation.QueueMoveCommand(1, malformed), "mismatched request was enqueued");
        malformed = last;
        malformed.target.x = std::numeric_limits<f32>::infinity();
        Require(!simulation.QueueMoveCommand(1, malformed) && simulation.GetPendingCommandCount() == 0,
            "malformed queue insertion must fail without publishing intent");

        for (std::size_t index = 0; index < RTSSimulation::MaximumQueuedCommands; ++index)
        {
            const Math::Vec3 target(1000.0f + static_cast<f32>(index), 0.0f, 0.0f);
            Require(simulation.QueueMoveCommand(1, {unit.GetId(), target, unit}),
                "queue rejected a request below capacity");
        }
        Require(simulation.GetPendingCommandCount() == RTSSimulation::MaximumQueuedCommands &&
                !simulation.QueueMoveCommand(1, first),
            "full queue must reject the newest request without replacing accepted intent");
        const auto full = simulation.Advance(step);
        Require(full.acceptedCommands == RTSSimulation::MaximumQueuedCommands &&
                full.rejectedCommands == 0 && simulation.GetPendingCommandCount() == 0 &&
                NearlyEqual(units.FindUnit(unit)->movementTarget->x,
                    1000.0f + static_cast<f32>(RTSSimulation::MaximumQueuedCommands - 1)) &&
                simulation.QueueMoveCommand(1, last),
            "draining a full FIFO must preserve its final order and free capacity");

        const u64 ticksBeforeInvalid = simulation.GetTickCount();
        const Math::Vec3 positionBeforeInvalid = unit.GetWorldPosition();
        for (f64 elapsed : {-1.0, std::numeric_limits<f64>::quiet_NaN(),
                std::numeric_limits<f64>::infinity()})
        {
            const auto invalid = simulation.Advance(elapsed);
            Require(!invalid.valid && invalid.steps == 0 && invalid.acceptedCommands == 0 &&
                    simulation.GetTickCount() == ticksBeforeInvalid &&
                    simulation.GetPendingCommandCount() == 1 &&
                    NearlyEqual(unit.GetWorldPosition(), positionBeforeInvalid),
                "invalid elapsed time must preserve the accumulator, queue, ticks, and scene");
        }
        Require(simulation.Advance(step).acceptedCommands == 1,
            "invalid time must not corrupt the next valid simulation tick");
        simulation.Suspend();
        const u64 ticksBeforeSpike = simulation.GetTickCount();
        const auto spike = simulation.Advance(step * (RTSSimulation::MaximumStepsPerAdvance + 5.5));
        Require(spike.valid && spike.steps == RTSSimulation::MaximumStepsPerAdvance &&
                spike.droppedSeconds == step * 5.0 &&
                simulation.GetTickCount() == ticksBeforeSpike + RTSSimulation::MaximumStepsPerAdvance,
            "overload must cap executed ticks and report dropped whole-step time");
        Require(simulation.Advance(step * 0.5).steps == 1,
            "overload must retain its fractional remainder for the next frame");
        simulation.Suspend();
        const auto huge = simulation.Advance(std::numeric_limits<f64>::max());
        Require(huge.valid && huge.steps == RTSSimulation::MaximumStepsPerAdvance &&
                std::isfinite(huge.droppedSeconds) && huge.droppedSeconds > 0.0 &&
                simulation.Advance(0.0).steps == 0,
            "huge finite elapsed time must remain bounded without overflowing timing state");

        simulation.Suspend();
        Require(simulation.QueueMoveCommand(1, first) && simulation.Advance(step * 0.5).steps == 0,
            "suspension fixture enqueue failed");
        const u64 ticksBeforeSuspend = simulation.GetTickCount();
        const Math::Vec3 admittedTarget = *units.FindUnit(unit)->movementTarget;
        simulation.Suspend();
        Require(simulation.GetPendingCommandCount() == 0 &&
                simulation.GetTickCount() == ticksBeforeSuspend &&
                NearlyEqual(*units.FindUnit(unit)->movementTarget, admittedTarget) &&
                simulation.Advance(step * 0.5).steps == 0,
            "suspension must discard pending intent and fractional time while retaining admitted state");
        const auto resumed = simulation.Advance(step * 0.5);
        Require(resumed.steps == 1 && resumed.acceptedCommands == 0 &&
                NearlyEqual(*units.FindUnit(unit)->movementTarget, admittedTarget),
            "resume must not catch up suspended time or replay abandoned requests");
    }

    void TestQueuedAuthorityChanges()
    {
        using namespace Pyramid;
        using namespace Pyramid::Examples::RTSReference;
        Scene scene("Queued Authority Changes");
        RTSUnitStore units;
        RTSUnitState initial;
        initial.owner = 1;
        const Entity transferred = scene.CreateEntity("Transferred");
        const Entity dead = scene.CreateEntity("Dead Before Tick");
        const Entity old = scene.CreateEntity("Replaced Before Tick");
        Require(units.AddUnit(transferred, initial) && units.AddUnit(dead, initial) &&
                units.AddUnit(old, initial), "authority fixture registration failed");
        RTSSimulation simulation(units);
        for (Entity entity : {transferred, dead, old})
        {
            Require(simulation.QueueMoveCommand(1, {entity.GetId(), Math::Vec3(8.0f), entity}),
                "authority fixture enqueue failed");
        }
        Require(units.SetOwnership(transferred, 2, 2) && units.ApplyDamage(dead, 100) &&
                scene.DestroyEntity(old), "queued authority mutation failed");
        const Entity replacement = scene.CreateEntityWithId(old.GetId(), "Replacement");
        Require(replacement && units.PruneInvalidEntities() == 1 && units.AddUnit(replacement, initial),
            "queued replacement registration failed");
        const auto result = simulation.Advance(RTSSimulation::FixedStepSeconds);
        Require(result.steps == 1 && result.acceptedCommands == 0 && result.rejectedCommands == 3 &&
                simulation.GetPendingCommandCount() == 0 &&
                !units.FindUnit(transferred)->movementTarget && !units.FindUnit(dead)->movementTarget &&
                !units.FindUnit(replacement)->movementTarget &&
                NearlyEqual(replacement.GetWorldPosition(), Math::Vec3::Zero),
            "execution must revalidate current ownership, health, and original entity lifetime");
        Require(simulation.QueueMoveCommand(1, {replacement.GetId(), Math::Vec3(8.0f), replacement}),
            "scene-clear request fixture failed");
        scene.Clear();
        const auto cleared = simulation.Advance(RTSSimulation::FixedStepSeconds);
        Require(cleared.acceptedCommands == 0 && cleared.rejectedCommands == 1 &&
                simulation.GetPendingCommandCount() == 0,
            "scene clear must reject and consume queued intent without reviving entities");
    }

    void TestUnitMovement()
    {
        using namespace Pyramid;
        using namespace Pyramid::Examples::RTSReference;
        constexpr f64 step = RTSSimulation::FixedStepSeconds;
        Scene scene("Movement Boundaries");
        RTSUnitStore units;
        Entity unit = scene.CreateEntity("Moving Root");
        const Math::Quat rotation = Math::Quat::FromEuler(0.2f, 0.3f, 0.4f);
        const Math::Vec3 scale(2.0f, 3.0f, 4.0f);
        Require(unit.SetLocalRotation(rotation) && unit.SetLocalScale(scale),
            "movement transform fixture failed");
        Entity visualChild = scene.CreateEntity("Visual Child");
        AttachRenderable(visualChild, Math::Vec3(1.0f, 0.0f, 0.0f));
        Require(visualChild.SetParent(unit), "visual child parenting failed");
        const Math::Vec3 childBefore = visualChild.GetWorldPosition();
        RTSUnitState initial;
        initial.owner = 1;
        Require(units.AddUnit(unit, initial), "movement unit registration failed");
        RTSSimulation simulation(units);
        Require(simulation.QueueMoveCommand(1, {unit.GetId(), Math::Vec3(3.0f, 0.0f, 4.0f), unit}),
            "diagonal movement enqueue failed");
        const auto diagonal = simulation.Advance(step);
        const Math::Vec3 expected(0.075f, 0.0f, 0.1f);
        const TransformComponent* transform = unit.GetTransform();
        Require(diagonal.acceptedCommands == 1 && diagonal.cancelledMoves == 0 &&
                NearlyEqual(unit.GetWorldPosition(), expected) &&
                NearlyEqual(visualChild.GetWorldPosition() - childBefore, expected) &&
                transform && NearlyEqual(transform->scale, scale) &&
                NearlyEqual(transform->rotation.x, rotation.x) &&
                NearlyEqual(transform->rotation.y, rotation.y) &&
                NearlyEqual(transform->rotation.z, rotation.z) &&
                NearlyEqual(transform->rotation.w, rotation.w),
            "normalized movement must preserve rotation/scale and propagate through scene hierarchy");
        const Math::Vec3 arrival = expected + Math::Vec3(0.02f, 0.0f, 0.0f);
        Require(simulation.QueueMoveCommand(1, {unit.GetId(), arrival, unit}),
            "arrival movement enqueue failed");
        const auto arrived = simulation.Advance(step);
        Require(arrived.cancelledMoves == 0 && NearlyEqual(unit.GetWorldPosition(), arrival) &&
                !units.FindUnit(unit)->movementTarget,
            "movement must snap to a nearby target without overshoot and clear the completed order");
        (void)simulation.Advance(step * 2.0);
        Require(NearlyEqual(unit.GetWorldPosition(), arrival), "idle ticks changed an arrived unit");

        Entity immobile = scene.CreateEntity("Immobile");
        RTSUnitState stationary = initial;
        stationary.movementSpeed = 0.0f;
        Require(units.AddUnit(immobile, stationary) &&
                simulation.QueueMoveCommand(1, {immobile.GetId(), Math::Vec3(4.0f), immobile}),
            "zero-speed fixture registration failed");
        const auto stopped = simulation.Advance(step);
        Require(stopped.cancelledMoves == 0 && units.FindUnit(immobile)->movementTarget &&
                NearlyEqual(immobile.GetWorldPosition(), Math::Vec3::Zero),
            "zero speed must preserve intent without cancellation or movement");
        Require(simulation.QueueMoveCommand(1, {immobile.GetId(), Math::Vec3::Zero, immobile}),
            "zero-distance enqueue failed");
        (void)simulation.Advance(step);
        Require(!units.FindUnit(immobile)->movementTarget,
            "a zero-distance order must finish even when movement speed is zero");
        const Entity invalidSpeed = scene.CreateEntity("Invalid Speed");
        for (f32 speed : {-1.0f, std::numeric_limits<f32>::infinity(),
                std::numeric_limits<f32>::quiet_NaN()})
        {
            RTSUnitState invalid = initial;
            invalid.movementSpeed = speed;
            Require(!units.AddUnit(invalidSpeed, invalid) && !units.FindUnit(invalidSpeed),
                "negative or nonfinite movement speed must fail transactional registration");
        }

        Entity child = scene.CreateEntity("Parented Unit");
        Require(child.SetParent(unit), "parented movement fixture failed");
        RTSUnitState withTarget = initial;
        withTarget.movementTarget = Math::Vec3(6.0f);
        Require(!units.AddUnit(child, withTarget) && units.AddUnit(child, initial) &&
                !units.ApplyMoveCommand(1, {child.GetId(), Math::Vec3(6.0f), child}),
            "parented units must reject world-space movement without writing local coordinates");
        Require(simulation.QueueMoveCommand(1, {child.GetId(), Math::Vec3(6.0f), child}),
            "parented request should reach execution admission");
        Require(simulation.Advance(step).rejectedCommands == 1 && !units.FindUnit(child)->movementTarget,
            "tick admission must reject unsupported parented movement");
        Entity reparented = scene.CreateEntity("Reparented Unit");
        Require(units.AddUnit(reparented, initial) &&
                simulation.QueueMoveCommand(1, {reparented.GetId(), Math::Vec3(6.0f), reparented}) &&
                reparented.SetParent(unit), "queued reparent fixture failed");
        Require(simulation.Advance(step).rejectedCommands == 1 &&
                !units.FindUnit(reparented)->movementTarget && reparented.ClearParent(),
            "reparenting after enqueue must invalidate execution admission");
        Require(simulation.QueueMoveCommand(1, {reparented.GetId(), Math::Vec3(6.0f), reparented}) &&
                simulation.Advance(step).acceptedCommands == 1 && reparented.SetParent(unit),
            "active reparent fixture failed");
        const Math::Vec3 localBefore = reparented.GetTransform()->position;
        Require(simulation.Advance(step).cancelledMoves == 1 &&
                !units.FindUnit(reparented)->movementTarget &&
                NearlyEqual(reparented.GetTransform()->position, localBefore),
            "reparenting an active unit must cancel intent without corrupting its local transform");

        Entity extreme = scene.CreateEntity("Extreme Coordinates");
        const f32 largest = std::numeric_limits<f32>::max();
        Require(extreme.SetLocalPosition(Math::Vec3(largest, 0.0f, 0.0f)),
            "extreme coordinate fixture failed");
        RTSUnitState fast = initial;
        fast.movementSpeed = largest;
        Require(units.AddUnit(extreme, fast) &&
                simulation.QueueMoveCommand(1, {extreme.GetId(), Math::Vec3(-largest, 0.0f, 0.0f), extreme}),
            "extreme coordinate move fixture failed");
        const auto extremeTick = simulation.Advance(step);
        Require(extremeTick.cancelledMoves == 0 && std::isfinite(extreme.GetWorldPosition().x) &&
                extreme.GetWorldPosition().x < largest && extreme.GetWorldPosition().x > 0.0f,
            "finite opposite-extreme coordinates must not overflow float subtraction or normalization");
        Entity precisionLimited = scene.CreateEntity("Float Precision Limit");
        Require(precisionLimited.SetLocalPosition(Math::Vec3(largest, 0.0f, 0.0f)) &&
                units.AddUnit(precisionLimited, initial) &&
                simulation.QueueMoveCommand(1,
                    {precisionLimited.GetId(), Math::Vec3(-largest, 0.0f, 0.0f), precisionLimited}),
            "precision limit fixture failed");
        Require(simulation.Advance(step).cancelledMoves == 1 &&
                !units.FindUnit(precisionLimited)->movementTarget &&
                precisionLimited.GetWorldPosition().x == largest,
            "unrepresentable float progress must cancel visibly rather than claim movement");
        Entity malformed = scene.CreateEntity("Malformed Position");
        Require(units.AddUnit(malformed, initial) &&
                units.ApplyMoveCommand(1, {malformed.GetId(), Math::Vec3(1.0f), malformed}) &&
                malformed.SetLocalPosition(Math::Vec3(std::numeric_limits<f32>::quiet_NaN(), 0.0f, 0.0f)),
            "malformed active position fixture failed");
        Require(!units.ApplyMoveCommand(1, {malformed.GetId(), Math::Vec3(2.0f), malformed}) &&
                simulation.Advance(step).cancelledMoves == 1 &&
                !units.FindUnit(malformed)->movementTarget,
            "invalid active positions must reject new orders and cancel existing movement safely");
    }
    void ReleaseMouse(Pyramid::InputState& input)
    {
        input.ProcessMouseButton(Pyramid::MouseButton::Left, false);
        input.ProcessMouseButton(Pyramid::MouseButton::Right, false);
        input.ProcessMouseButton(Pyramid::MouseButton::Middle, false);
    }
}

int main()
{
    using namespace Pyramid;
    using namespace Pyramid::Examples::RTSReference;

    TestUnitState();
    TestUnitLifetimes();
    TestSimulationTimingAndQueue();
    TestQueuedAuthorityChanges();
    TestUnitMovement();

    InputState input;
    InputActionSystem actions;
    input.SetFocused(true);

    InputContext* context = actions.CreateContext("reference-rts", 0, true);
    Require(context != nullptr, "input context creation failed");
    Require(context->AddAction("Select", InputActionType::Button),
        "select action creation failed");
    Require(context->AddBinding(
        "Select",
        InputBinding::MouseButtonBinding(MouseButton::Left)),
        "select binding creation failed");
    Require(context->AddAction("Command", InputActionType::Button),
        "command action creation failed");
    Require(context->AddBinding(
        "Command",
        InputBinding::MouseButtonBinding(MouseButton::Right)),
        "command binding creation failed");

    RTSInteractionActions interactionActions;
    interactionActions.select = {"reference-rts", "Select"};
    interactionActions.command = {"reference-rts", "Command"};

    RTSInteractionSettings settings;
    settings.edgeMarginPixels = 20.0f;
    settings.edgeScrollSpeed = 4.0f;
    settings.edgeScrollDistanceScale = 0.0f;
    settings.maximumRayDistance = 100.0f;

    RTSInteractionController interaction(interactionActions, settings);
    interaction.SetViewportSize(1000, 800);

    InputState noPointer;
    noPointer.SetFocused(true);
    const InputActionVector2 noPointerAxis = interaction.GetEdgeScrollAxis(noPointer);
    Require(NearlyEqual(noPointerAxis.x, 0.0f) && NearlyEqual(noPointerAxis.y, 0.0f),
        "edge scrolling should wait for the first pointer sample");

    input.ProcessMouseMove(0.0f, 0.0f);
    const InputActionVector2 cornerAxis = interaction.GetEdgeScrollAxis(input);
    Require(NearlyEqual(cornerAxis.x, -1.0f) && NearlyEqual(cornerAxis.y, 1.0f),
        "top-left corner should produce a diagonal edge axis");

    input.ProcessMouseMove(500.0f, 400.0f);
    const InputActionVector2 centerAxis = interaction.GetEdgeScrollAxis(input);
    Require(NearlyEqual(centerAxis.x, 0.0f) && NearlyEqual(centerAxis.y, 0.0f),
        "center pointer should not edge scroll");

    auto scene = std::make_shared<Scene>("RTS Interaction Test");

    const Entity blocker = scene->CreateEntity("DecorativeBlocker");
    Require(static_cast<bool>(blocker), "blocker entity creation failed");
    AttachRenderable(blocker, Math::Vec3(0.0f, 2.5f, 5.0f), 0.75f);

    const Entity unit = scene->CreateEntity("SelectableUnit");
    Require(static_cast<bool>(unit), "unit entity creation failed");
    AttachRenderable(unit);
    RTSUnitStore units;
    RTSUnitState initialUnit;
    initialUnit.owner = 1;
    initialUnit.team = 1;
    Require(units.AddUnit(unit, initialUnit), "selectable unit registration failed");

    SceneManagement::SceneManager sceneManager;
    sceneManager.SetActiveScene(scene);

    Camera camera(
        Math::Radians(60.0f),
        1000.0f / 800.0f,
        0.1f,
        100.0f);
    camera.SetPosition(Math::Vec3(0.0f, 5.0f, 10.0f));
    camera.LookAt(Math::Vec3::Zero);

    RTSCameraActions cameraActions;
    cameraActions.move = {};
    cameraActions.orbitDelta = {};
    cameraActions.orbitRate = {};
    cameraActions.zoomDelta = {};
    cameraActions.zoomRate = {};
    cameraActions.boost = {};
    cameraActions.reset = {};

    RTSCameraSettings cameraSettings;
    cameraSettings.distanceMovementScale = 0.0f;
    RTSCameraController cameraController(Math::Vec3::Zero, cameraActions, cameraSettings);
    cameraController.CaptureHome(camera);

    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseMove(0.0f, 400.0f);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 1.0f);
    Require(NearlyEqual(cameraController.GetFocusPoint(), Math::Vec3(-4.0f, 0.0f, 0.0f)),
        "left-edge scrolling should move the strategy focus left at a time-scaled speed");
    Require(NearlyEqual(
        camera.GetForward().Dot(
            (cameraController.GetFocusPoint() - camera.GetPosition()).Normalized()),
        1.0f),
        "edge scrolling should preserve the camera focus relationship");

    cameraController.Reset(camera);
    Require(NearlyEqual(cameraController.GetFocusPoint(), Math::Vec3::Zero),
        "camera reset should restore the interaction test focus");

    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseMove(0.0f, 400.0f);
    InputConsumptionMask uiConsumption;
    uiConsumption.ConsumeAllMouse();
    actions.Update(input, uiConsumption);
    interaction.Update(
        input,
        actions,
        uiConsumption,
        camera,
        sceneManager,
        cameraController,
        1.0f);
    Require(NearlyEqual(cameraController.GetFocusPoint(), Math::Vec3::Zero),
        "UI-consumed pointer input leaked into RTS edge scrolling");

    interaction.SetSelectablePredicate(
        [&units](const Entity& entity)
        {
            return units.IsSelectable(entity);
        });

    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseMove(500.0f, 400.0f);
    input.ProcessMouseButton(MouseButton::Left, true);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
    Require(interaction.GetSelectedEntityId() == unit.GetId(),
        "selection should skip a nearer non-selectable renderable and pick the unit behind it");
    Require(interaction.GetSelectedEntity(sceneManager) == unit,
        "selected entity lookup should resolve through the active scene");

    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseMove(500.0f, 400.0f);
    input.ProcessMouseButton(MouseButton::Right, true);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
    Require(interaction.HasPendingCommand(),
        "a command press with a selection should emit a command request");
    const auto command = interaction.ConsumeCommand();
    Require(command.has_value(), "command consumption should return the pending request");
    Require(command->entity == unit.GetId(), "command should target the selected entity");
    Require(NearlyEqual(command->target, Math::Vec3::Zero, 0.01f),
        "center-screen command should project to the ground-plane origin");
    RTSSimulation inputSimulation(units);
    Require(command->entityRef == unit && inputSimulation.QueueMoveCommand(1, *command),
        "real controller requests must retain identity and enter the simulation queue");
    const auto inputTicks = inputSimulation.Advance(RTSSimulation::FixedStepSeconds * 3.0);
    Require(inputTicks.steps == 3 && inputTicks.acceptedCommands == 1 &&
            inputTicks.rejectedCommands == 0 && inputSimulation.GetPendingCommandCount() == 0,
        "one real input edge must execute exactly once across multiple catch-up ticks");
    Require(!interaction.ConsumeCommand().has_value(),
        "command requests should be one-shot after consumption");

    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseMove(850.0f, 100.0f);
    input.ProcessMouseButton(MouseButton::Left, true);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
    Require(!interaction.GetSelectedEntityId(),
        "an empty selection click should clear the previous selection");

    input.BeginFrame();
    ReleaseMouse(input);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);

    input.BeginFrame();
    input.ProcessMouseMove(500.0f, 400.0f);
    input.ProcessMouseButton(MouseButton::Left, true);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
    Require(interaction.GetSelectedEntityId() == unit.GetId(),
        "unit should be selectable again before stale-entity validation");

    Require(scene->DestroyEntity(unit), "selected unit destruction failed");
    const Entity replacement = scene->CreateEntityWithId(unit.GetId(), "Replacement Unit");
    Require(static_cast<bool>(replacement), "same-ID selectable replacement creation failed");
    AttachRenderable(replacement);
    Require(units.PruneInvalidEntities() == 1 && units.AddUnit(replacement, initialUnit),
        "same-ID selectable replacement registration failed");
    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseButton(MouseButton::Right, true);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
    Require(!interaction.GetSelectedEntityId(),
        "destroyed selections should be cleared before command processing");
    Require(!interaction.HasPendingCommand(),
        "destroyed selections must not emit commands");

    const auto click = [&](MouseButton button)
    {
        input.BeginFrame();
        ReleaseMouse(input);
        input.ProcessMouseMove(500.0f, 400.0f);
        actions.Update(input);
        interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
        input.BeginFrame();
        input.ProcessMouseButton(button, true);
        actions.Update(input);
        interaction.Update(input, actions, camera, sceneManager, cameraController, 0.0f);
    };

    sceneManager.RebuildSpatialPartition();
    click(MouseButton::Left);
    Require(interaction.GetSelectedEntity(sceneManager) == replacement,
        "replacement should require and accept a fresh selection");
    Require(units.SetSelectable(replacement, false), "selection policy fixture failed");
    click(MouseButton::Right);
    Require(!interaction.GetSelectedEntityId() && !interaction.HasPendingCommand(),
        "changing selectability must invalidate an existing selection before commands");
    Require(units.SetSelectable(replacement, true), "selection policy restore failed");
    click(MouseButton::Left);
    click(MouseButton::Right);
    Require(interaction.HasPendingCommand(), "delayed-command fixture was not emitted");

    Require(scene->DestroyEntity(replacement), "queued request fixture destruction failed");
    const Entity current = scene->CreateEntityWithId(unit.GetId(), "Current Unit");
    Require(static_cast<bool>(current), "queued request replacement creation failed");
    AttachRenderable(current);
    Require(units.PruneInvalidEntities() == 1 && units.AddUnit(current, initialUnit),
        "queued request replacement registration failed");
    const auto delayed = interaction.ConsumeCommand();
    Require(delayed && delayed->entityRef == replacement &&
            !units.ApplyMoveCommand(1, *delayed) && !units.FindUnit(current)->movementTarget,
        "queued controller requests must not target a replacement registered before consumption");

    sceneManager.RebuildSpatialPartition();
    click(MouseButton::Left);
    Require(interaction.GetSelectedEntity(sceneManager) == current,
        "scene-switch fixture selection failed");
    auto otherScene = std::make_shared<Scene>("Other Active RTS Scene");
    const Entity otherUnit = otherScene->CreateEntityWithId(current.GetId(), "Other Scene Unit");
    Require(static_cast<bool>(otherUnit), "same-ID scene-switch fixture creation failed");
    AttachRenderable(otherUnit);
    RTSUnitStore otherUnits;
    Require(otherUnits.AddUnit(otherUnit, initialUnit), "other scene unit registration failed");
    interaction.SetSelectablePredicate(
        [&units, &otherUnits](const Entity& entity)
        {
            return units.IsSelectable(entity) || otherUnits.IsSelectable(entity);
        });
    sceneManager.SetActiveScene(otherScene);
    Require(!interaction.GetSelectedEntity(sceneManager),
        "selection must not resolve its numeric ID in a different active scene");
    click(MouseButton::Right);
    Require(!interaction.GetSelectedEntityId() && !interaction.HasPendingCommand(),
        "switching scenes must clear selection before command generation");
    sceneManager.SetActiveScene(scene);
    Require(!interaction.GetSelectedEntity(sceneManager),
        "returning to the original scene must not revive cleared selection");
    click(MouseButton::Left);
    Require(interaction.GetSelectedEntity(sceneManager) == current,
        "unit should be selectable before lethal damage");
    Require(units.ApplyDamage(current, std::numeric_limits<u32>::max()),
        "lethal selection fixture damage failed");
    click(MouseButton::Right);
    Require(!interaction.GetSelectedEntityId() && !interaction.HasPendingCommand(),
        "dead selected units must be cleared before command processing");
    // The controller outlives this captured store; clear the callback before teardown.
    interaction.SetSelectablePredicate({});

    RTSInteractionSettings invalid = interaction.GetSettings();
    invalid.commandPlaneNormal = Math::Vec3::Zero;
    Require(!interaction.SetSettings(invalid),
        "zero command-plane normals should be rejected");
    Require(NearlyEqual(interaction.GetSettings().edgeScrollSpeed, 4.0f),
        "failed settings updates should preserve the previous settings");

    interaction.SetEnabled(false);
    const Math::Vec3 disabledFocus = cameraController.GetFocusPoint();
    input.BeginFrame();
    ReleaseMouse(input);
    input.ProcessMouseMove(0.0f, 400.0f);
    actions.Update(input);
    interaction.Update(input, actions, camera, sceneManager, cameraController, 1.0f);
    Require(NearlyEqual(cameraController.GetFocusPoint(), disabledFocus),
        "disabled RTS interaction should not move the camera focus");

    std::cout << "RTS interaction tests passed\n";
    return EXIT_SUCCESS;
}
