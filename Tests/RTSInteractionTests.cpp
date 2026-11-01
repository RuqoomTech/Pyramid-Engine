#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>
#include <Pyramid/Examples/RTSReference/RTSUnitStore.hpp>
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
    Require(command->entityRef == unit && units.ApplyMoveCommand(1, *command) &&
            units.FindUnit(unit)->movementTarget &&
            NearlyEqual(*units.FindUnit(unit)->movementTarget, command->target),
        "real controller requests must retain identity and pass owner admission");
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
