#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/CameraController.hpp>
#include <Pyramid/Graphics/Scene/Entity.hpp>
#include <Pyramid/Input/InputActions.hpp>
#include <Pyramid/Platform/Input.hpp>

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace Pyramid
{
    namespace SceneManagement
    {
        class SceneManager;
    }

    namespace Examples::RTSReference
    {
        /** Named game action consumed by the RTS reference interaction layer. */
        struct RTSActionReference
        {
            std::string context;
            std::string action;

            [[nodiscard]] bool IsBound() const
            {
                return !context.empty() && !action.empty();
            }
        };

        struct RTSInteractionActions
        {
            RTSActionReference select{"rts", "Select"};
            RTSActionReference command{"rts", "Command"};
        };

        struct RTSInteractionSettings
        {
            f32 edgeMarginPixels = 16.0f;
            f32 edgeScrollSpeed = 8.0f;
            f32 edgeScrollDistanceScale = 0.05f;
            f32 maximumRayDistance = 1000.0f;
            Math::Vec3 commandPlaneNormal = Math::Vec3::Up;
            f32 commandPlaneDistance = 0.0f;
            bool clearSelectionOnMiss = true;
        };

        /** Local intent, not a wire format; admission must validate the captured facade. */
        struct RTSCommandRequest
        {
            EntityId entity;
            Math::Vec3 target = Math::Vec3::Zero;
            /** Must be live and match entity, including its original lifetime identity. */
            Entity entityRef;
        };

        /**
         * Game-side RTS interaction coordinator.
         *
         * This helper intentionally lives outside Pyramid::Engine. It combines
         * physical pointer position, named actions, the optional strategy camera,
         * and scene queries into one reference profile without adding selectable,
         * command, unit, or edge-scroll concepts to the generic engine API.
         */
        class RTSInteractionController final
        {
        public:
            using SelectablePredicate = std::function<bool(const Entity&)>;

            explicit RTSInteractionController(
                RTSInteractionActions actions = {},
                RTSInteractionSettings settings = {});

            void SetEnabled(bool enabled) { m_enabled = enabled; }
            [[nodiscard]] bool IsEnabled() const { return m_enabled; }

            [[nodiscard]] bool SetSettings(const RTSInteractionSettings& settings);
            [[nodiscard]] const RTSInteractionSettings& GetSettings() const
            {
                return m_settings;
            }

            void SetActions(RTSInteractionActions actions)
            {
                m_actions = std::move(actions);
            }
            [[nodiscard]] const RTSInteractionActions& GetActions() const
            {
                return m_actions;
            }

            void SetViewportSize(u32 width, u32 height);
            [[nodiscard]] u32 GetViewportWidth() const { return m_viewportWidth; }
            [[nodiscard]] u32 GetViewportHeight() const { return m_viewportHeight; }

            void SetSelectablePredicate(SelectablePredicate predicate)
            {
                m_selectablePredicate = std::move(predicate);
            }

            /**
             * Updates edge scrolling, the strategy camera, selection, and command input.
             *
             * The camera is updated before click rays are evaluated, so picking uses
             * the same pose that will render the current frame.
             */
            void Update(
                const InputState& input,
                const InputActionSystem& actions,
                Camera& camera,
                SceneManagement::SceneManager& sceneManager,
                RTSCameraController& cameraController,
                f32 deltaTime);

            /**
             * UI-aware update. Physical pointer behavior such as edge scrolling is
             * suppressed when an earlier UI/editor layer reserved mouse input.
             */
            void Update(
                const InputState& input,
                const InputActionSystem& actions,
                const InputConsumptionMask& consumption,
                Camera& camera,
                SceneManagement::SceneManager& sceneManager,
                RTSCameraController& cameraController,
                f32 deltaTime);

            [[nodiscard]] InputActionVector2 GetEdgeScrollAxis(const InputState& input) const;

            [[nodiscard]] EntityId GetSelectedEntityId() const
            {
                return m_selectedEntity ? m_selectedEntity.GetId() : EntityId{};
            }
            /** Returns empty outside the captured scene or when selection eligibility changed. */
            [[nodiscard]] Entity GetSelectedEntity(const SceneManagement::SceneManager& sceneManager) const;
            void ClearSelection() { m_selectedEntity = {}; }

            [[nodiscard]] bool HasPendingCommand() const
            {
                return m_pendingCommand.has_value();
            }
            /** One-shot local intent; consumers must revalidate its captured entity and authority. */
            [[nodiscard]] std::optional<RTSCommandRequest> ConsumeCommand();

        private:
            [[nodiscard]] bool TryGetNormalizedPointer(
                const InputState& input,
                f32& screenX,
                f32& screenY) const;
            void ApplyEdgeScroll(
                const InputState& input,
                RTSCameraController& cameraController,
                f32 deltaTime) const;
            [[nodiscard]] Entity PickEntity(
                const Camera& camera,
                SceneManagement::SceneManager& sceneManager,
                f32 screenX,
                f32 screenY) const;
            [[nodiscard]] bool ProjectCommandTarget(
                const Camera& camera,
                f32 screenX,
                f32 screenY,
                Math::Vec3& target) const;
            [[nodiscard]] bool IsSelectable(const Entity& entity) const;

            RTSInteractionActions m_actions;
            RTSInteractionSettings m_settings;
            SelectablePredicate m_selectablePredicate;
            Entity m_selectedEntity;
            std::optional<RTSCommandRequest> m_pendingCommand;
            u32 m_viewportWidth = 0;
            u32 m_viewportHeight = 0;
            bool m_enabled = true;
        };
    }
}
