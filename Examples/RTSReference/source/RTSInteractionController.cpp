#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>

#include <Pyramid/Graphics/Scene/SceneManager.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace Pyramid::Examples::RTSReference
{
    namespace
    {
        constexpr f32 InteractionEpsilon = 0.000001f;

        [[nodiscard]] bool IsFinite(f32 value)
        {
            return std::isfinite(value);
        }

        [[nodiscard]] bool IsFinite(const Math::Vec3& value)
        {
            return IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
        }

        [[nodiscard]] bool WasPressed(
            const InputActionSystem& actions,
            const RTSActionReference& reference)
        {
            return reference.IsBound() &&
                actions.WasActionPressed(reference.context, reference.action);
        }
    }

    RTSInteractionController::RTSInteractionController(
        RTSInteractionActions actions,
        RTSInteractionSettings settings)
        : m_actions(std::move(actions))
    {
        if (!SetSettings(settings))
        {
            m_settings = {};
        }
    }

    bool RTSInteractionController::SetSettings(const RTSInteractionSettings& settings)
    {
        const f32 normalLengthSquared = settings.commandPlaneNormal.LengthSquared();
        if (!IsFinite(settings.edgeMarginPixels) || settings.edgeMarginPixels < 0.0f ||
            !IsFinite(settings.edgeScrollSpeed) || settings.edgeScrollSpeed < 0.0f ||
            !IsFinite(settings.edgeScrollDistanceScale) ||
                settings.edgeScrollDistanceScale < 0.0f ||
            !IsFinite(settings.maximumRayDistance) || settings.maximumRayDistance <= 0.0f ||
            !IsFinite(settings.commandPlaneNormal) || normalLengthSquared <= InteractionEpsilon ||
            !IsFinite(settings.commandPlaneDistance))
        {
            return false;
        }

        m_settings = settings;
        m_settings.commandPlaneNormal.Normalize();
        return true;
    }

    void RTSInteractionController::SetViewportSize(u32 width, u32 height)
    {
        m_viewportWidth = width;
        m_viewportHeight = height;
    }

    void RTSInteractionController::Update(
        const InputState& input,
        const InputActionSystem& actions,
        Camera& camera,
        SceneManagement::SceneManager& sceneManager,
        RTSCameraController& cameraController,
        f32 deltaTime)
    {
        const InputConsumptionMask noConsumption;
        Update(
            input,
            actions,
            noConsumption,
            camera,
            sceneManager,
            cameraController,
            deltaTime);
    }

    void RTSInteractionController::Update(
        const InputState& input,
        const InputActionSystem& actions,
        const InputConsumptionMask& consumption,
        Camera& camera,
        SceneManagement::SceneManager& sceneManager,
        RTSCameraController& cameraController,
        f32 deltaTime)
    {
        if (!m_enabled || !input.HasFocus() || !IsFinite(deltaTime) || deltaTime < 0.0f)
        {
            return;
        }

        if (!consumption.HasAnyMouseConsumption())
        {
            ApplyEdgeScroll(input, cameraController, deltaTime);
        }
        cameraController.Update(camera, actions, deltaTime);

        const Entity selected = GetSelectedEntity(sceneManager);
        if (!selected)
        {
            m_selectedEntity = {};
        }

        f32 screenX = 0.0f;
        f32 screenY = 0.0f;
        const bool hasPointer = !consumption.HasAnyMouseConsumption() &&
            TryGetNormalizedPointer(input, screenX, screenY);

        if (WasPressed(actions, m_actions.select))
        {
            const Entity picked = hasPointer
                ? PickEntity(camera, sceneManager, screenX, screenY)
                : Entity{};
            if (hasPointer && (picked || m_settings.clearSelectionOnMiss))
            {
                m_selectedEntity = picked;
            }
        }

        if (WasPressed(actions, m_actions.command) && m_selectedEntity && hasPointer)
        {
            Math::Vec3 target;
            if (ProjectCommandTarget(camera, screenX, screenY, target))
            {
                m_pendingCommand = RTSCommandRequest{
                    m_selectedEntity.GetId(), target, m_selectedEntity};
            }
        }
    }

    InputActionVector2 RTSInteractionController::GetEdgeScrollAxis(
        const InputState& input) const
    {
        InputActionVector2 axis;
        if (!m_enabled || !input.HasFocus() || !input.HasMousePosition() ||
            m_viewportWidth == 0 || m_viewportHeight == 0 ||
            m_settings.edgeMarginPixels <= 0.0f)
        {
            return axis;
        }

        const MousePosition position = input.GetMousePosition();
        const f32 width = static_cast<f32>(m_viewportWidth);
        const f32 height = static_cast<f32>(m_viewportHeight);
        const f32 horizontalMargin = (std::min)(m_settings.edgeMarginPixels, width * 0.5f);
        const f32 verticalMargin = (std::min)(m_settings.edgeMarginPixels, height * 0.5f);

        if (position.x >= 0.0f && position.x < horizontalMargin)
        {
            axis.x = -1.0f;
        }
        else if (position.x <= width && position.x > width - horizontalMargin)
        {
            axis.x = 1.0f;
        }

        if (position.y >= 0.0f && position.y < verticalMargin)
        {
            axis.y = 1.0f;
        }
        else if (position.y <= height && position.y > height - verticalMargin)
        {
            axis.y = -1.0f;
        }

        return axis;
    }

    Entity RTSInteractionController::GetSelectedEntity(
        const SceneManagement::SceneManager& sceneManager) const
    {
        const auto scene = sceneManager.GetActiveScene();
        return scene && m_selectedEntity &&
            scene.get() == m_selectedEntity.GetScene() && IsSelectable(m_selectedEntity)
            ? m_selectedEntity
            : Entity{};
    }

    std::optional<RTSCommandRequest> RTSInteractionController::ConsumeCommand()
    {
        auto command = std::move(m_pendingCommand);
        m_pendingCommand.reset();
        return command;
    }

    bool RTSInteractionController::TryGetNormalizedPointer(
        const InputState& input,
        f32& screenX,
        f32& screenY) const
    {
        if (!input.HasMousePosition() || m_viewportWidth == 0 || m_viewportHeight == 0)
        {
            return false;
        }

        const MousePosition position = input.GetMousePosition();
        if (!IsFinite(position.x) || !IsFinite(position.y))
        {
            return false;
        }

        const f32 width = static_cast<f32>(m_viewportWidth);
        const f32 height = static_cast<f32>(m_viewportHeight);
        if (position.x < 0.0f || position.y < 0.0f || position.x > width || position.y > height)
        {
            return false;
        }

        screenX = std::clamp(position.x / width, 0.0f, 1.0f);
        screenY = std::clamp(position.y / height, 0.0f, 1.0f);
        return true;
    }

    void RTSInteractionController::ApplyEdgeScroll(
        const InputState& input,
        RTSCameraController& cameraController,
        f32 deltaTime) const
    {
        InputActionVector2 axis = GetEdgeScrollAxis(input);
        const f32 lengthSquared = axis.x * axis.x + axis.y * axis.y;
        if (lengthSquared <= InteractionEpsilon || deltaTime <= 0.0f)
        {
            return;
        }

        if (lengthSquared > 1.0f)
        {
            const f32 inverseLength = 1.0f / std::sqrt(lengthSquared);
            axis.x *= inverseLength;
            axis.y *= inverseLength;
        }

        const f32 yaw = cameraController.GetYaw();
        const Math::Vec3 right(std::cos(yaw), 0.0f, std::sin(yaw));
        const Math::Vec3 forward(std::sin(yaw), 0.0f, -std::cos(yaw));
        Math::Vec3 direction = right * axis.x + forward * axis.y;
        if (direction.LengthSquared() <= InteractionEpsilon)
        {
            return;
        }
        direction.Normalize();

        const f32 speed = m_settings.edgeScrollSpeed +
            cameraController.GetDistance() * m_settings.edgeScrollDistanceScale;
        cameraController.SetFocusPoint(
            cameraController.GetFocusPoint() + direction * (speed * deltaTime));
    }

    Entity RTSInteractionController::PickEntity(
        const Camera& camera,
        SceneManagement::SceneManager& sceneManager,
        f32 screenX,
        f32 screenY) const
    {
        Math::Vec3 rayOrigin;
        Math::Vec3 rayDirection;
        camera.ScreenToWorldRay(screenX, screenY, rayOrigin, rayDirection);

        SceneManagement::QueryParams query;
        query.type = SceneManagement::QueryType::Ray;
        query.position = rayOrigin;
        query.direction = rayDirection;
        query.maxDistance = m_settings.maximumRayDistance;

        const auto result = sceneManager.QueryScene(query);
        const auto scene = sceneManager.GetActiveScene();
        if (!scene)
        {
            return {};
        }

        for (const auto& object : result.objects)
        {
            if (!object || !object->entityId)
            {
                continue;
            }

            const Entity entity = scene->FindEntity(object->entityId);
            if (IsSelectable(entity))
            {
                return entity;
            }
        }

        return {};
    }

    bool RTSInteractionController::ProjectCommandTarget(
        const Camera& camera,
        f32 screenX,
        f32 screenY,
        Math::Vec3& target) const
    {
        Math::Vec3 rayOrigin;
        Math::Vec3 rayDirection;
        camera.ScreenToWorldRay(screenX, screenY, rayOrigin, rayDirection);

        const f32 denominator = m_settings.commandPlaneNormal.Dot(rayDirection);
        if (std::fabs(denominator) <= InteractionEpsilon)
        {
            return false;
        }

        const f32 distance =
            (m_settings.commandPlaneDistance -
                m_settings.commandPlaneNormal.Dot(rayOrigin)) /
            denominator;
        if (!IsFinite(distance) || distance < 0.0f ||
            distance > m_settings.maximumRayDistance)
        {
            return false;
        }

        target = rayOrigin + rayDirection * distance;
        return IsFinite(target);
    }

    bool RTSInteractionController::IsSelectable(const Entity& entity) const
    {
        if (!entity || !entity.IsEffectivelyVisible() || !entity.HasMeshRenderer())
        {
            return false;
        }
        return !m_selectablePredicate || m_selectablePredicate(entity);
    }
}
