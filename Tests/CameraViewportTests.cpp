#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Math/Math.hpp>

#include <cmath>
#include <iostream>

namespace
{
    bool NearlyEqual(Pyramid::f32 lhs, Pyramid::f32 rhs, Pyramid::f32 epsilon = 0.0001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool MatrixChanged(const Pyramid::Math::Mat4& lhs, const Pyramid::Math::Mat4& rhs)
    {
        for (int index = 0; index < 16; ++index)
        {
            if (!NearlyEqual(lhs.m[index], rhs.m[index]))
            {
                return true;
            }
        }
        return false;
    }
}

int main()
{
    using Pyramid::Camera;
    using Pyramid::ProjectionType;
    using Pyramid::Math::Radians;

    Camera perspective(Radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
    const Pyramid::Math::Mat4 originalPerspective = perspective.GetProjectionMatrix();

    if (!perspective.SetViewportSize(800, 800))
    {
        std::cerr << "Perspective camera rejected a valid viewport.\n";
        return 1;
    }

    if (!NearlyEqual(perspective.GetAspectRatio(), 1.0f))
    {
        std::cerr << "Perspective aspect ratio was not updated.\n";
        return 1;
    }

    if (!MatrixChanged(originalPerspective, perspective.GetProjectionMatrix()))
    {
        std::cerr << "Perspective projection matrix was not invalidated.\n";
        return 1;
    }

    const Pyramid::Math::Mat4 squarePerspective = perspective.GetProjectionMatrix();
    if (perspective.SetViewportSize(0, 720))
    {
        std::cerr << "Zero-width viewport should be rejected.\n";
        return 1;
    }

    if (!NearlyEqual(perspective.GetAspectRatio(), 1.0f) ||
        MatrixChanged(squarePerspective, perspective.GetProjectionMatrix()))
    {
        std::cerr << "Invalid viewport corrupted perspective projection state.\n";
        return 1;
    }

    Camera orthographic;
    orthographic.SetOrthographic(20.0f, 10.0f, 0.1f, 100.0f);
    if (!NearlyEqual(orthographic.GetAspectRatio(), 2.0f))
    {
        std::cerr << "Orthographic aspect ratio was not initialized from its extents.\n";
        return 1;
    }

    const Pyramid::Math::Mat4 originalOrthographic = orthographic.GetProjectionMatrix();
    if (!orthographic.SetViewportSize(1000, 1000))
    {
        std::cerr << "Orthographic camera rejected a valid viewport.\n";
        return 1;
    }

    if (orthographic.GetProjectionType() != ProjectionType::Orthographic ||
        !NearlyEqual(orthographic.GetAspectRatio(), 1.0f))
    {
        std::cerr << "Orthographic resize changed projection type or aspect incorrectly.\n";
        return 1;
    }

    if (!MatrixChanged(originalOrthographic, orthographic.GetProjectionMatrix()))
    {
        std::cerr << "Orthographic projection matrix was not updated.\n";
        return 1;
    }

    return 0;
}
