#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Math/Math.hpp>

#include <cmath>
#include <algorithm>

namespace Pyramid
{
    namespace
    {
        constexpr f32 FrustumTolerance = 0.0001f;

        f32 PlaneDistance(const Math::Vec4 &plane, const Math::Vec3 &point)
        {
            return plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
        }

        Math::Quat NormalizeCameraRotation(const Math::Quat &rotation)
        {
            return Math::IsZero(rotation.LengthSquared())
                       ? Math::Quat::Identity
                       : rotation.Normalized();
        }
    }

    Camera::Camera()
        : Camera(Math::Radians(60.0f), 16.0f / 9.0f, 0.1f, 1000.0f)
    {
    }

    Camera::Camera(f32 fov, f32 aspectRatio, f32 nearPlane, f32 farPlane, ProjectionType type)
        : m_projectionType(type), m_fov(fov), m_aspectRatio(aspectRatio), m_nearPlane(nearPlane), m_farPlane(farPlane)
    {
        UpdateViewMatrix();
        UpdateProjectionMatrix();
    }

    void Camera::SetPosition(const Math::Vec3 &position)
    {
        m_position = position;
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::SetRotation(f32 pitch, f32 yaw, f32 roll)
    {
        m_rotation = NormalizeCameraRotation(Math::Quat::FromEuler(pitch, yaw, roll));
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::SetRotation(const Math::Quat &rotation)
    {
        m_rotation = NormalizeCameraRotation(rotation);
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::LookAt(const Math::Vec3 &target, const Math::Vec3 &up)
    {
        const Math::Vec3 targetDirection = target - m_position;
        if (targetDirection.LengthSquared() <= Math::EPSILON)
        {
            return;
        }

        const Math::Vec3 forward = targetDirection.Normalized();
        Math::Vec3 correctedUp = up;
        if (correctedUp.LengthSquared() <= Math::EPSILON)
        {
            correctedUp = Math::Vec3::Up;
        }
        correctedUp.Normalize();

        if (Math::Abs(forward.Dot(correctedUp)) >= 1.0f - Math::EPSILON)
        {
            correctedUp = Math::Abs(forward.Dot(Math::Vec3::Up)) < 1.0f - Math::EPSILON
                              ? Math::Vec3::Up
                              : Math::Vec3::Right;
        }

        m_rotation = NormalizeCameraRotation(Math::Quat::LookRotation(forward, correctedUp));
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::MoveForward(f32 distance)
    {
        m_position += GetForward() * distance;
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::MoveRight(f32 distance)
    {
        m_position += GetRight() * distance;
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::MoveUp(f32 distance)
    {
        m_position += GetUp() * distance;
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::Rotate(f32 pitchDelta, f32 yawDelta, f32 rollDelta)
    {
        Math::Quat deltaRotation = Math::Quat::FromEuler(pitchDelta, yawDelta, rollDelta);
        m_rotation = m_rotation * deltaRotation;
        m_rotation = m_rotation.Normalized();

        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    bool Camera::SetViewportSize(u32 width, u32 height)
    {
        if (width == 0 || height == 0)
        {
            return false;
        }

        const f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);
        if (std::abs(m_aspectRatio - aspectRatio) <= Math::EPSILON)
        {
            return true;
        }

        m_aspectRatio = aspectRatio;

        if (m_projectionType == ProjectionType::Orthographic)
        {
            const f32 centerX = (m_orthoLeft + m_orthoRight) * 0.5f;
            const f32 centerY = (m_orthoBottom + m_orthoTop) * 0.5f;
            const f32 halfHeight = (m_orthoTop - m_orthoBottom) * 0.5f;
            const f32 halfWidth = halfHeight * aspectRatio;

            m_orthoLeft = centerX - halfWidth;
            m_orthoRight = centerX + halfWidth;
            m_orthoBottom = centerY - halfHeight;
            m_orthoTop = centerY + halfHeight;
        }

        m_projectionMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
        return true;
    }

    void Camera::SetPerspective(f32 fov, f32 aspectRatio, f32 nearPlane, f32 farPlane)
    {
        m_projectionType = ProjectionType::Perspective;
        m_fov = fov;
        m_aspectRatio = aspectRatio;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;

        m_projectionMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::SetOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 nearPlane, f32 farPlane)
    {
        m_projectionType = ProjectionType::Orthographic;
        m_orthoLeft = left;
        m_orthoRight = right;
        m_orthoBottom = bottom;
        m_orthoTop = top;

        const f32 height = top - bottom;
        if (std::abs(height) > Math::EPSILON)
        {
            m_aspectRatio = (right - left) / height;
        }

        m_nearPlane = nearPlane;
        m_farPlane = farPlane;

        m_projectionMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::SetOrthographic(f32 width, f32 height, f32 nearPlane, f32 farPlane)
    {
        f32 halfWidth = width * 0.5f;
        f32 halfHeight = height * 0.5f;
        SetOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
    }

    const Math::Mat4 &Camera::GetViewMatrix() const
    {
        if (m_viewMatrixDirty)
        {
            UpdateViewMatrix();
        }
        return m_viewMatrix;
    }

    const Math::Mat4 &Camera::GetProjectionMatrix() const
    {
        if (m_projectionMatrixDirty)
        {
            UpdateProjectionMatrix();
        }
        return m_projectionMatrix;
    }

    const Math::Mat4 &Camera::GetViewProjectionMatrix() const
    {
        if (m_viewProjectionMatrixDirty)
        {
            UpdateViewProjectionMatrix();
        }
        return m_viewProjectionMatrix;
    }

    const Math::Mat4 &Camera::GetInverseViewMatrix() const
    {
        if (m_viewMatrixDirty)
        {
            UpdateViewMatrix();
        }
        return m_inverseViewMatrix;
    }

    Math::Vec3 Camera::GetForward() const
    {
        // OpenGL cameras look down their local negative Z axis.
        return m_rotation.RotateVector(-Math::Vec3::Forward).Normalized();
    }

    Math::Vec3 Camera::GetRight() const
    {
        return m_rotation.RotateVector(Math::Vec3::Right);
    }

    Math::Vec3 Camera::GetUp() const
    {
        return m_rotation.RotateVector(Math::Vec3::Up);
    }

    void Camera::ScreenToWorldRay(f32 screenX, f32 screenY, Math::Vec3 &rayOrigin, Math::Vec3 &rayDirection) const
    {
        // Convert screen coordinates to NDC (-1 to 1)
        f32 ndcX = 2.0f * screenX - 1.0f;
        f32 ndcY = 1.0f - 2.0f * screenY; // Flip Y

        rayOrigin = m_position;

        if (m_projectionType == ProjectionType::Perspective)
        {
            // Perspective projection
            f32 tanHalfFov = Math::Tan(m_fov * 0.5f);
            f32 rayX = ndcX * tanHalfFov * m_aspectRatio;
            f32 rayY = ndcY * tanHalfFov;

            Math::Vec3 localDirection(rayX, rayY, -1.0f);
            rayDirection = m_rotation.RotateVector(localDirection).Normalized();
        }
        else
        {
            // Orthographic projection
            f32 worldX = ndcX * (m_orthoRight - m_orthoLeft) * 0.5f;
            f32 worldY = ndcY * (m_orthoTop - m_orthoBottom) * 0.5f;

            Math::Vec3 offset = GetRight() * worldX + GetUp() * worldY;
            rayOrigin = m_position + offset;
            rayDirection = GetForward();
        }
    }

    bool Camera::WorldToScreen(const Math::Vec3 &worldPos, f32 &screenX, f32 &screenY) const
    {
        Math::Vec4 clipPos = GetViewProjectionMatrix() * Math::Vec4(worldPos, 1.0f);

        if (clipPos.w <= 0.0f)
        {
            return false; // Behind camera
        }

        // Perspective divide
        Math::Vec3 ndcPos = Math::Vec3(clipPos.x, clipPos.y, clipPos.z) / clipPos.w;

        // Convert to screen coordinates
        screenX = (ndcPos.x + 1.0f) * 0.5f;
        screenY = (1.0f - ndcPos.y) * 0.5f; // Flip Y

        return ndcPos.z >= -1.0f && ndcPos.z <= 1.0f; // Within depth range
    }

    bool Camera::IsPointVisible(const Math::Vec3 &point) const
    {
        const auto &planes = GetFrustumPlanes();
        for (const auto &plane : planes)
        {
            if (PlaneDistance(plane, point) < -FrustumTolerance)
            {
                return false;
            }
        }
        return true;
    }

    bool Camera::IsSphereVisible(const Math::Vec3 &center, f32 radius) const
    {
        if (radius < 0.0f)
        {
            return false;
        }

        const auto &planes = GetFrustumPlanes();
        for (const auto &plane : planes)
        {
            if (PlaneDistance(plane, center) < -radius - FrustumTolerance)
            {
                return false;
            }
        }
        return true;
    }

    bool Camera::IsAABBVisible(const Math::Vec3 &minPoint, const Math::Vec3 &maxPoint) const
    {
        const Math::Vec3 boxMin(
            Math::Min(minPoint.x, maxPoint.x),
            Math::Min(minPoint.y, maxPoint.y),
            Math::Min(minPoint.z, maxPoint.z));
        const Math::Vec3 boxMax(
            Math::Max(minPoint.x, maxPoint.x),
            Math::Max(minPoint.y, maxPoint.y),
            Math::Max(minPoint.z, maxPoint.z));

        const auto &planes = GetFrustumPlanes();
        for (const auto &plane : planes)
        {
            // The support point furthest along the inward-facing plane normal.
            const Math::Vec3 positiveVertex(
                plane.x >= 0.0f ? boxMax.x : boxMin.x,
                plane.y >= 0.0f ? boxMax.y : boxMin.y,
                plane.z >= 0.0f ? boxMax.z : boxMin.z);

            if (PlaneDistance(plane, positiveVertex) < -FrustumTolerance)
            {
                return false;
            }
        }
        return true;
    }

    const FrustumPlanes &Camera::GetFrustumPlanes() const
    {
        if (m_frustumPlanesDirty)
        {
            UpdateFrustumPlanes();
        }
        return m_frustumPlanes;
    }

    void Camera::UpdateViewMatrix() const
    {
        // m_rotation describes the camera's local-to-world orientation. The view
        // transform therefore uses the inverse rotation followed by translation.
        const Math::Quat normalizedRotation = NormalizeCameraRotation(m_rotation);
        const Math::Mat4 inverseRotation = normalizedRotation.Inverse().ToMatrix4();
        const Math::Mat4 inverseTranslation = Math::Mat4::CreateTranslation(-m_position);

        m_viewMatrix = inverseRotation * inverseTranslation;
        m_inverseViewMatrix = Math::Mat4::CreateTranslation(m_position) * normalizedRotation.ToMatrix4();

        m_viewMatrixDirty = false;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::UpdateProjectionMatrix() const
    {
        if (m_projectionType == ProjectionType::Perspective)
        {
            m_projectionMatrix = Math::Mat4::CreatePerspective(m_fov, m_aspectRatio, m_nearPlane, m_farPlane);
        }
        else
        {
            m_projectionMatrix = Math::Mat4::CreateOrthographic(m_orthoLeft, m_orthoRight,
                                                                m_orthoBottom, m_orthoTop,
                                                                m_nearPlane, m_farPlane);
        }

        m_projectionMatrixDirty = false;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::UpdateViewProjectionMatrix() const
    {
        if (m_viewMatrixDirty)
            UpdateViewMatrix();
        if (m_projectionMatrixDirty)
            UpdateProjectionMatrix();

        m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
        m_viewProjectionMatrixDirty = false;
        m_frustumPlanesDirty = true;
    }

    void Camera::UpdateFrustumPlanes() const
    {
        const Math::Mat4 &viewProjection = GetViewProjectionMatrix();
        const Math::Vec4 row0 = viewProjection.GetRow(0);
        const Math::Vec4 row1 = viewProjection.GetRow(1);
        const Math::Vec4 row2 = viewProjection.GetRow(2);
        const Math::Vec4 row3 = viewProjection.GetRow(3);

        m_frustumPlanes[static_cast<std::size_t>(FrustumPlane::Left)] = row3 + row0;
        m_frustumPlanes[static_cast<std::size_t>(FrustumPlane::Right)] = row3 - row0;
        m_frustumPlanes[static_cast<std::size_t>(FrustumPlane::Bottom)] = row3 + row1;
        m_frustumPlanes[static_cast<std::size_t>(FrustumPlane::Top)] = row3 - row1;
        m_frustumPlanes[static_cast<std::size_t>(FrustumPlane::Near)] = row3 + row2;
        m_frustumPlanes[static_cast<std::size_t>(FrustumPlane::Far)] = row3 - row2;

        for (auto &plane : m_frustumPlanes)
        {
            const f32 normalLength = Math::Vec3(plane.x, plane.y, plane.z).Length();
            if (normalLength > Math::EPSILON)
            {
                plane *= 1.0f / normalLength;
            }
            else
            {
                // Invalid projection state must reject geometry rather than leave
                // an unnormalized plane with undefined classification behavior.
                plane = Math::Vec4(0.0f, 0.0f, 0.0f, -1.0f);
            }
        }

        m_frustumPlanesDirty = false;
    }

} // namespace Pyramid
