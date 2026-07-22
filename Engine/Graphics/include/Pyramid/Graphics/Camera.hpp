#pragma once

#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Core/Prerequisites.hpp>

#include <array>
#include <cstddef>

namespace Pyramid
{

    /**
     * @brief Camera projection types
     */
    enum class ProjectionType
    {
        Perspective,
        Orthographic
    };

    /**
     * @brief Camera-frustum plane order.
     *
     * Plane equations use the form ax + by + cz + d = 0 and point inward.
     */
    enum class FrustumPlane : u32
    {
        Left = 0,
        Right,
        Bottom,
        Top,
        Near,
        Far,
        Count
    };

    using FrustumPlanes = std::array<Math::Vec4, static_cast<std::size_t>(FrustumPlane::Count)>;

    /**
     * @brief Camera class for 3D rendering
     *
     * Provides view and projection matrix calculations with support for
     * both perspective and orthographic projections. Includes frustum
     * culling support and various camera movement utilities.
     */
    class Camera
    {
    public:
        /**
         * @brief Default constructor - creates perspective camera
         */
        Camera();

        /**
         * @brief Constructor with projection parameters
         * @param fov Field of view in radians (perspective only)
         * @param aspectRatio Aspect ratio (width/height)
         * @param nearPlane Near clipping plane distance
         * @param farPlane Far clipping plane distance
         * @param type Projection type
         */
        Camera(f32 fov, f32 aspectRatio, f32 nearPlane, f32 farPlane, ProjectionType type = ProjectionType::Perspective);

        /**
         * @brief Destructor
         */
        ~Camera() = default;

        // Transform operations
        /**
         * @brief Set camera position
         * @param position World position
         */
        void SetPosition(const Math::Vec3 &position);

        /**
         * @brief Set camera rotation using Euler angles
         * @param pitch Pitch angle in radians
         * @param yaw Yaw angle in radians
         * @param roll Roll angle in radians
         */
        void SetRotation(f32 pitch, f32 yaw, f32 roll = 0.0f);

        /**
         * @brief Set camera rotation using quaternion
         * @param rotation Rotation quaternion
         */
        void SetRotation(const Math::Quat &rotation);

        /**
         * @brief Look at a target position
         * @param target Target position to look at
         * @param up Up vector (default: world up)
         */
        void LookAt(const Math::Vec3 &target, const Math::Vec3 &up = Math::Vec3::Up);

        // Movement operations
        /**
         * @brief Move camera forward/backward
         * @param distance Distance to move (positive = forward)
         */
        void MoveForward(f32 distance);

        /**
         * @brief Move camera right/left
         * @param distance Distance to move (positive = right)
         */
        void MoveRight(f32 distance);

        /**
         * @brief Move camera up/down
         * @param distance Distance to move (positive = up)
         */
        void MoveUp(f32 distance);

        /**
         * @brief Rotate camera around local axes
         * @param pitchDelta Pitch rotation delta in radians
         * @param yawDelta Yaw rotation delta in radians
         * @param rollDelta Roll rotation delta in radians
         */
        void Rotate(f32 pitchDelta, f32 yawDelta, f32 rollDelta = 0.0f);

        // Projection settings
        /**
         * @brief Update projection state for a render-surface size.
         * @param width Renderable surface width in pixels.
         * @param height Renderable surface height in pixels.
         * @return true when the dimensions are valid and projection state was updated.
         *
         * Perspective cameras update their aspect ratio. Orthographic cameras
         * preserve their vertical span and adjust the horizontal span. Zero-sized
         * surfaces are rejected so minimize events cannot corrupt projection state.
         */
        bool SetViewportSize(u32 width, u32 height);

        /**
         * @brief Set perspective projection parameters
         * @param fov Field of view in radians
         * @param aspectRatio Aspect ratio (width/height)
         * @param nearPlane Near clipping plane
         * @param farPlane Far clipping plane
         */
        void SetPerspective(f32 fov, f32 aspectRatio, f32 nearPlane, f32 farPlane);

        /**
         * @brief Set orthographic projection parameters
         * @param left Left clipping plane
         * @param right Right clipping plane
         * @param bottom Bottom clipping plane
         * @param top Top clipping plane
         * @param nearPlane Near clipping plane
         * @param farPlane Far clipping plane
         */
        void SetOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane);

        /**
         * @brief Set orthographic projection with width/height
         * @param width Orthographic width
         * @param height Orthographic height
         * @param nearPlane Near clipping plane
         * @param farPlane Far clipping plane
         */
        void SetOrthographic(f32 width, f32 height, f32 nearPlane, f32 farPlane);

        // Matrix accessors
        /**
         * @brief Get view matrix
         * @return View matrix
         */
        const Math::Mat4 &GetViewMatrix() const;

        /**
         * @brief Get projection matrix
         * @return Projection matrix
         */
        const Math::Mat4 &GetProjectionMatrix() const;

        /**
         * @brief Get combined view-projection matrix
         * @return View-projection matrix
         */
        const Math::Mat4 &GetViewProjectionMatrix() const;

        /**
         * @brief Get inverse view matrix
         * @return Inverse view matrix
         */
        const Math::Mat4 &GetInverseViewMatrix() const;

        // Transform accessors
        /**
         * @brief Get camera position
         * @return World position
         */
        const Math::Vec3 &GetPosition() const { return m_position; }

        /**
         * @brief Get camera rotation quaternion
         * @return Rotation quaternion
         */
        const Math::Quat &GetRotation() const { return m_rotation; }

        /**
         * @brief Get forward direction vector
         * @return Forward direction (normalized)
         */
        Math::Vec3 GetForward() const;

        /**
         * @brief Get right direction vector
         * @return Right direction (normalized)
         */
        Math::Vec3 GetRight() const;

        /**
         * @brief Get up direction vector
         * @return Up direction (normalized)
         */
        Math::Vec3 GetUp() const;

        // Projection accessors
        /**
         * @brief Get projection type
         * @return Projection type
         */
        ProjectionType GetProjectionType() const { return m_projectionType; }

        /**
         * @brief Get field of view (perspective only)
         * @return FOV in radians
         */
        f32 GetFOV() const { return m_fov; }

        /**
         * @brief Get aspect ratio
         * @return Aspect ratio
         */
        f32 GetAspectRatio() const { return m_aspectRatio; }

        /**
         * @brief Get near clipping plane distance
         * @return Near plane distance
         */
        f32 GetNearPlane() const { return m_nearPlane; }

        /**
         * @brief Get far clipping plane distance
         * @return Far plane distance
         */
        f32 GetFarPlane() const { return m_farPlane; }

        // Utility functions
        /**
         * @brief Convert screen coordinates to world ray
         * @param screenX Screen X coordinate (0-1)
         * @param screenY Screen Y coordinate (0-1)
         * @param rayOrigin Output ray origin
         * @param rayDirection Output ray direction
         */
        void ScreenToWorldRay(f32 screenX, f32 screenY, Math::Vec3 &rayOrigin, Math::Vec3 &rayDirection) const;

        /**
         * @brief Convert world position to screen coordinates
         * @param worldPos World position
         * @param screenX Output screen X coordinate
         * @param screenY Output screen Y coordinate
         * @return true if position is in front of camera
         */
        bool WorldToScreen(const Math::Vec3 &worldPos, f32 &screenX, f32 &screenY) const;

        /**
         * @brief Check if a point is inside the camera frustum
         * @param point World position to test
         * @return true if point is visible
         */
        bool IsPointVisible(const Math::Vec3 &point) const;

        /**
         * @brief Check if a sphere is inside the camera frustum
         * @param center Sphere center
         * @param radius Sphere radius
         * @return true if sphere is visible (fully or partially)
         */
        bool IsSphereVisible(const Math::Vec3 &center, f32 radius) const;

        /**
         * @brief Check whether an axis-aligned world-space box intersects the camera frustum.
         * @param minPoint Minimum world-space corner. Component order is normalized internally.
         * @param maxPoint Maximum world-space corner. Component order is normalized internally.
         * @return true when the box is fully or partially visible.
         */
        bool IsAABBVisible(const Math::Vec3 &minPoint, const Math::Vec3 &maxPoint) const;

        /**
         * @brief Get the six normalized inward-facing world-space frustum planes.
         */
        const FrustumPlanes &GetFrustumPlanes() const;

    private:
        void UpdateViewMatrix() const;
        void UpdateProjectionMatrix() const;
        void UpdateViewProjectionMatrix() const;
        void UpdateFrustumPlanes() const;

        // Transform
        Math::Vec3 m_position = Math::Vec3::Zero;
        Math::Quat m_rotation = Math::Quat::Identity;

        // Projection parameters
        ProjectionType m_projectionType = ProjectionType::Perspective;
        f32 m_fov = Math::Radians(60.0f);
        f32 m_aspectRatio = 16.0f / 9.0f;
        f32 m_nearPlane = 0.1f;
        f32 m_farPlane = 1000.0f;

        // Orthographic parameters
        f32 m_orthoLeft = -10.0f;
        f32 m_orthoRight = 10.0f;
        f32 m_orthoBottom = -10.0f;
        f32 m_orthoTop = 10.0f;

        // Cached matrices
        mutable Math::Mat4 m_viewMatrix = Math::Mat4::Identity;
        mutable Math::Mat4 m_projectionMatrix = Math::Mat4::Identity;
        mutable Math::Mat4 m_viewProjectionMatrix = Math::Mat4::Identity;
        mutable Math::Mat4 m_inverseViewMatrix = Math::Mat4::Identity;

        // Frustum planes for culling (ax + by + cz + d = 0)
        mutable FrustumPlanes m_frustumPlanes{};

        // Dirty flags
        mutable bool m_viewMatrixDirty = true;
        mutable bool m_projectionMatrixDirty = true;
        mutable bool m_viewProjectionMatrixDirty = true;
        mutable bool m_frustumPlanesDirty = true;
    };

} // namespace Pyramid
