#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Math/Math.hpp>

#include <cmath>

namespace Pyramid
{

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
        m_rotation = Math::Quat::FromEuler(pitch, yaw, roll);
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::SetRotation(const Math::Quat &rotation)
    {
        m_rotation = rotation;
        m_viewMatrixDirty = true;
        m_viewProjectionMatrixDirty = true;
        m_frustumPlanesDirty = true;
    }

    void Camera::LookAt(const Math::Vec3 &target, const Math::Vec3 &up)
    {
        Math::Vec3 forward = (target - m_position).Normalized();
        Math::Vec3 right = forward.Cross(up).Normalized();
        Math::Vec3 newUp = right.Cross(forward).Normalized();

        // Create rotation matrix from basis vectors
        Math::Mat4 rotMatrix = Math::Mat4::Identity;
        rotMatrix.m[0] = right.x;
        rotMatrix.m[4] = right.y;
        rotMatrix.m[8] = right.z;
        rotMatrix.m[1] = newUp.x;
        rotMatrix.m[5] = newUp.y;
        rotMatrix.m[9] = newUp.z;
        rotMatrix.m[2] = -forward.x;
        rotMatrix.m[6] = -forward.y;
        rotMatrix.m[10] = -forward.z;

        // Convert to quaternion
        m_rotation = Math::Quat::FromMatrix(rotMatrix);

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
        return m_rotation.RotateVector(Math::Vec3::Forward);
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
        // Simple frustum culling - for now just check if in front of camera
        Math::Vec3 toPoint = point - m_position;
        return toPoint.Dot(GetForward()) > m_nearPlane;
    }

    bool Camera::IsSphereVisible(const Math::Vec3 &center, f32 radius) const
    {
        // Simple sphere culling - for now just check distance from camera
        f32 distance = (center - m_position).Length();
        return distance <= (m_farPlane + radius) && distance >= (m_nearPlane - radius);
    }

    void Camera::UpdateViewMatrix() const
    {
        // Create view matrix from position and rotation
        Math::Mat4 rotationMatrix = m_rotation.ToMatrix4();
        Math::Mat4 translationMatrix = Math::Mat4::CreateTranslation(-m_position);

        m_viewMatrix = rotationMatrix * translationMatrix;
        m_inverseViewMatrix = m_viewMatrix.Inverse();

        m_viewMatrixDirty = false;
        m_viewProjectionMatrixDirty = true;
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
    }

    void Camera::UpdateViewProjectionMatrix() const
    {
        if (m_viewMatrixDirty)
            UpdateViewMatrix();
        if (m_projectionMatrixDirty)
            UpdateProjectionMatrix();

        m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
        m_viewProjectionMatrixDirty = false;
    }

    void Camera::UpdateFrustumPlanes() const
    {
        // Ensure view-projection matrix is up to date
        if (m_viewProjectionMatrixDirty)
            UpdateViewProjectionMatrix();

        const Math::Mat4& vp = m_viewProjectionMatrix;

        // Extract frustum planes from view-projection matrix
        // Plane equations are in the form: ax + by + cz + d = 0
        
        // Left plane: row4 + row1
        m_frustumPlanes[0] = Math::Vec4(
            vp.m[3] + vp.m[0],   // a
            vp.m[7] + vp.m[4],   // b
            vp.m[11] + vp.m[8],  // c
            vp.m[15] + vp.m[12]  // d
        );

        // Right plane: row4 - row1
        m_frustumPlanes[1] = Math::Vec4(
            vp.m[3] - vp.m[0],   // a
            vp.m[7] - vp.m[4],   // b
            vp.m[11] - vp.m[8],  // c
            vp.m[15] - vp.m[12]  // d
        );

        // Bottom plane: row4 + row2
        m_frustumPlanes[2] = Math::Vec4(
            vp.m[3] + vp.m[1],   // a
            vp.m[7] + vp.m[5],   // b
            vp.m[11] + vp.m[9],  // c
            vp.m[15] + vp.m[13]  // d
        );

        // Top plane: row4 - row2
        m_frustumPlanes[3] = Math::Vec4(
            vp.m[3] - vp.m[1],   // a
            vp.m[7] - vp.m[5],   // b
            vp.m[11] - vp.m[9],  // c
            vp.m[15] - vp.m[13]  // d
        );

        // Near plane: row4 + row3
        m_frustumPlanes[4] = Math::Vec4(
            vp.m[3] + vp.m[2],   // a
            vp.m[7] + vp.m[6],   // b
            vp.m[11] + vp.m[10], // c
            vp.m[15] + vp.m[14]  // d
        );

        // Far plane: row4 - row3
        m_frustumPlanes[5] = Math::Vec4(
            vp.m[3] - vp.m[2],   // a
            vp.m[7] - vp.m[6],   // b
            vp.m[11] - vp.m[10], // c
            vp.m[15] - vp.m[14]  // d
        );

        // Normalize the planes
        for (int i = 0; i < 6; ++i)
        {
            Math::Vec3 normal(m_frustumPlanes[i].x, m_frustumPlanes[i].y, m_frustumPlanes[i].z);
            f32 length = normal.Length();
            
            if (length > Math::EPSILON)
            {
                f32 invLength = 1.0f / length;
                m_frustumPlanes[i] = m_frustumPlanes[i] * invLength;
            }
        }

        m_frustumPlanesDirty = false;
    }

} // namespace Pyramid
