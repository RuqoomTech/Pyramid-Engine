#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Math/Math.hpp>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
    bool NearlyEqual(Pyramid::f32 lhs, Pyramid::f32 rhs, Pyramid::f32 epsilon = 0.001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool NearlyEqual(
        const Pyramid::Math::Vec3 &lhs,
        const Pyramid::Math::Vec3 &rhs,
        Pyramid::f32 epsilon = 0.001f)
    {
        return NearlyEqual(lhs.x, rhs.x, epsilon) &&
               NearlyEqual(lhs.y, rhs.y, epsilon) &&
               NearlyEqual(lhs.z, rhs.z, epsilon);
    }

    bool Expect(bool condition, const char *message)
    {
        if (condition)
        {
            return true;
        }

        std::cerr << message << '\n';
        return false;
    }

    template <typename T>
    bool Contains(const std::vector<std::shared_ptr<T>> &objects, const std::shared_ptr<T> &object)
    {
        return std::find(objects.begin(), objects.end(), object) != objects.end();
    }
}

int main()
{
    using Pyramid::Camera;
    using Pyramid::RenderObject;
    using Pyramid::Scene;
    using Pyramid::Math::Quat;
    using Pyramid::Math::Radians;
    using Pyramid::Math::Vec3;
    using Pyramid::SceneManagement::AABB;
    using Pyramid::SceneManagement::Octree;
    namespace SpatialUtils = Pyramid::SceneManagement::SpatialUtils;

    bool passed = true;

    Camera perspective(Radians(90.0f), 1.0f, 1.0f, 10.0f);
    passed &= Expect(
        NearlyEqual(perspective.GetForward(), Vec3(0.0f, 0.0f, -1.0f)),
        "default OpenGL camera should face local negative Z");
    passed &= Expect(perspective.IsPointVisible(Vec3(0.0f, 0.0f, -2.0f)), "front point should be visible");
    passed &= Expect(!perspective.IsPointVisible(Vec3(0.0f, 0.0f, 2.0f)), "point behind camera should be rejected");
    passed &= Expect(!perspective.IsPointVisible(Vec3(0.0f, 0.0f, -11.0f)), "point beyond far plane should be rejected");
    passed &= Expect(!perspective.IsPointVisible(Vec3(3.0f, 0.0f, -2.0f)), "point beyond side plane should be rejected");
    passed &= Expect(perspective.IsPointVisible(Vec3(1.99f, 0.0f, -2.0f)), "point inside side plane should be visible");

    passed &= Expect(
        perspective.IsSphereVisible(Vec3(2.4f, 0.0f, -2.0f), 0.5f),
        "sphere intersecting a side plane should remain visible");
    passed &= Expect(
        !perspective.IsSphereVisible(Vec3(3.0f, 0.0f, -2.0f), 0.25f),
        "sphere fully outside a side plane should be rejected");
    passed &= Expect(
        !perspective.IsSphereVisible(Vec3(0.0f, 0.0f, -2.0f), -1.0f),
        "negative sphere radii should be rejected");

    passed &= Expect(
        perspective.IsAABBVisible(Vec3(-0.5f, -0.5f, -3.5f), Vec3(0.5f, 0.5f, -2.5f)),
        "box inside the perspective frustum should be visible");
    passed &= Expect(
        perspective.IsAABBVisible(Vec3(4.5f, -0.5f, -5.5f), Vec3(5.5f, 0.5f, -4.5f)),
        "box intersecting a perspective side plane should remain visible");
    passed &= Expect(
        !perspective.IsAABBVisible(Vec3(6.0f, -0.5f, -5.0f), Vec3(7.0f, 0.5f, -4.0f)),
        "box fully outside a perspective side plane should be rejected");
    passed &= Expect(
        perspective.IsAABBVisible(Vec3(-0.25f, -0.25f, -1.25f), Vec3(0.25f, 0.25f, -0.75f)),
        "box crossing the near plane should remain visible");
    passed &= Expect(
        !perspective.IsAABBVisible(Vec3(-0.25f, -0.25f, -0.8f), Vec3(0.25f, 0.25f, -0.2f)),
        "box fully before the near plane should be rejected");

    for (const auto &plane : perspective.GetFrustumPlanes())
    {
        const auto normalLength = Vec3(plane.x, plane.y, plane.z).Length();
        passed &= Expect(NearlyEqual(normalLength, 1.0f), "frustum-plane normals should be normalized");
    }

    Camera translated(Radians(60.0f), 1.0f, 0.1f, 100.0f);
    translated.SetPosition(Vec3(0.0f, 0.0f, 5.0f));
    translated.LookAt(Vec3::Zero);
    passed &= Expect(NearlyEqual(translated.GetForward(), Vec3(0.0f, 0.0f, -1.0f)), "LookAt should orient the camera toward its target");
    passed &= Expect(translated.IsPointVisible(Vec3::Zero), "LookAt target should be inside the translated frustum");

    Pyramid::f32 screenX = 0.0f;
    Pyramid::f32 screenY = 0.0f;
    passed &= Expect(translated.WorldToScreen(Vec3::Zero, screenX, screenY), "LookAt target should project in front of the camera");
    passed &= Expect(NearlyEqual(screenX, 0.5f) && NearlyEqual(screenY, 0.5f), "LookAt target should project to screen center");

    Vec3 rayOrigin;
    Vec3 rayDirection;
    translated.ScreenToWorldRay(0.5f, 0.5f, rayOrigin, rayDirection);
    passed &= Expect(NearlyEqual(rayOrigin, translated.GetPosition()), "perspective ray should originate at the camera");
    passed &= Expect(NearlyEqual(rayDirection, translated.GetForward()), "center-screen ray should follow camera forward");

    Camera rotated(Radians(60.0f), 1.0f, 0.1f, 100.0f);
    rotated.SetRotation(0.0f, Radians(90.0f), 0.0f);
    passed &= Expect(NearlyEqual(rotated.GetForward(), Vec3(-1.0f, 0.0f, 0.0f)), "yaw should rotate camera forward consistently");
    passed &= Expect(rotated.IsPointVisible(Vec3(-5.0f, 0.0f, 0.0f)), "rotated camera should see along its new forward axis");
    passed &= Expect(!rotated.IsPointVisible(Vec3(5.0f, 0.0f, 0.0f)), "rotated camera should reject points behind it");

    Camera sideLook(Radians(60.0f), 1.0f, 0.1f, 100.0f);
    sideLook.LookAt(Vec3(-5.0f, 0.0f, 0.0f));
    passed &= Expect(NearlyEqual(sideLook.GetForward(), Vec3(-1.0f, 0.0f, 0.0f)), "LookAt should support non-default world directions");
    passed &= Expect(sideLook.IsPointVisible(Vec3(-5.0f, 0.0f, 0.0f)), "side-facing LookAt target should be visible");

    Camera verticalLook(Radians(60.0f), 1.0f, 0.1f, 100.0f);
    verticalLook.LookAt(Vec3(0.0f, 5.0f, 0.0f), Vec3::Up);
    passed &= Expect(NearlyEqual(verticalLook.GetForward(), Vec3::Up), "LookAt should recover from an up vector parallel to the target direction");
    passed &= Expect(verticalLook.IsPointVisible(Vec3(0.0f, 5.0f, 0.0f)), "vertical LookAt target should be visible");

    Camera resizedFrustum(Radians(90.0f), 2.0f, 1.0f, 10.0f);
    passed &= Expect(resizedFrustum.IsPointVisible(Vec3(3.0f, 0.0f, -2.0f)), "wide viewport should widen perspective side planes");
    resizedFrustum.SetViewportSize(800, 800);
    passed &= Expect(!resizedFrustum.IsPointVisible(Vec3(3.0f, 0.0f, -2.0f)), "viewport changes should invalidate cached frustum planes");

    Camera orthographic;
    orthographic.SetOrthographic(-2.0f, 2.0f, -1.0f, 1.0f, 1.0f, 10.0f);
    passed &= Expect(orthographic.IsPointVisible(Vec3(1.9f, 0.9f, -2.0f)), "point inside orthographic extents should be visible");
    passed &= Expect(!orthographic.IsPointVisible(Vec3(2.1f, 0.0f, -2.0f)), "point outside orthographic extents should be rejected");
    passed &= Expect(
        orthographic.IsSphereVisible(Vec3(2.2f, 0.0f, -2.0f), 0.25f),
        "sphere crossing an orthographic plane should remain visible");

    auto inside = std::make_shared<RenderObject>();
    inside->name = "Inside";
    inside->position = Vec3(0.0f, 0.0f, -5.0f);

    auto intersecting = std::make_shared<RenderObject>();
    intersecting->name = "Intersecting";
    intersecting->position = Vec3(6.0f, 0.0f, -5.0f);
    intersecting->SetLocalBounds(Vec3(-2.0f, -0.5f, -0.5f), Vec3(2.0f, 0.5f, 0.5f));

    auto outside = std::make_shared<RenderObject>();
    outside->name = "Outside";
    outside->position = Vec3(8.0f, 0.0f, -5.0f);

    auto hidden = std::make_shared<RenderObject>();
    hidden->name = "Hidden";
    hidden->position = Vec3(0.0f, 0.0f, -4.0f);
    hidden->visible = false;

    Scene scene("Frustum scene");
    scene.AddRenderObject(inside);
    scene.AddRenderObject(intersecting);
    scene.AddRenderObject(outside);
    scene.AddRenderObject(hidden);

    const auto visibleObjects = scene.GetVisibleObjects(perspective);
    passed &= Expect(Contains(visibleObjects, inside), "scene culling should retain inside objects");
    passed &= Expect(Contains(visibleObjects, intersecting), "scene culling should use transformed bounds rather than center points");
    passed &= Expect(!Contains(visibleObjects, outside), "scene culling should reject outside objects");
    passed &= Expect(!Contains(visibleObjects, hidden), "scene culling should reject hidden objects");

    auto managedScene = std::make_shared<Scene>("Managed frustum scene");
    managedScene->AddRenderObject(inside);
    managedScene->AddRenderObject(intersecting);
    managedScene->AddRenderObject(outside);
    managedScene->AddRenderObject(hidden);

    Pyramid::SceneManagement::SceneManager sceneManager;
    sceneManager.SetActiveScene(managedScene);
    const auto managerOctreeVisible = sceneManager.GetVisibleObjects(perspective);
    passed &= Expect(Contains(managerOctreeVisible, inside), "scene manager should rebuild and query its octree automatically");
    passed &= Expect(Contains(managerOctreeVisible, intersecting), "scene manager octree culling should retain intersecting bounds");
    passed &= Expect(!Contains(managerOctreeVisible, outside), "scene manager octree culling should reject outside bounds");
    passed &= Expect(!Contains(managerOctreeVisible, hidden), "scene manager should filter hidden octree objects");

    sceneManager.EnableSpatialPartitioning(false);
    const auto managerLinearVisible = sceneManager.GetVisibleObjects(perspective);
    passed &= Expect(Contains(managerLinearVisible, inside) && Contains(managerLinearVisible, intersecting),
                     "linear scene-manager culling should match the octree result");
    passed &= Expect(!Contains(managerLinearVisible, outside), "linear scene-manager culling should reject outside objects");

    sceneManager.SetFrustumCullingEnabled(false);
    const auto managerUnculled = sceneManager.GetVisibleObjects(perspective);
    passed &= Expect(Contains(managerUnculled, outside), "disabling frustum culling should retain visible outside objects");
    passed &= Expect(!Contains(managerUnculled, hidden), "disabling frustum culling should not expose hidden objects");

    auto rotatedBoundsObject = std::make_shared<RenderObject>();
    rotatedBoundsObject->position = Vec3(0.0f, 0.0f, -5.0f);
    rotatedBoundsObject->rotation = Quat::FromAxisAngle(Vec3::Forward, Radians(90.0f));
    rotatedBoundsObject->SetLocalBounds(Vec3(-2.0f, -0.5f, -0.5f), Vec3(2.0f, 0.5f, 0.5f));
    Vec3 boundsMin;
    Vec3 boundsMax;
    rotatedBoundsObject->GetWorldBounds(boundsMin, boundsMax);
    passed &= Expect(
        NearlyEqual(boundsMax.y - boundsMin.y, 4.0f) && NearlyEqual(boundsMax.x - boundsMin.x, 1.0f),
        "world AABB should include local rotation and scale");

    const auto spatialPlanes = SpatialUtils::CalculateFrustumPlanes(perspective);
    passed &= Expect(
        SpatialUtils::AABBInFrustum(AABB(Vec3(-0.5f, -0.5f, -5.5f), Vec3(0.5f, 0.5f, -4.5f)), spatialPlanes),
        "spatial AABB test should accept visible boxes");
    passed &= Expect(
        !SpatialUtils::AABBInFrustum(AABB(Vec3(8.0f, -0.5f, -5.5f), Vec3(9.0f, 0.5f, -4.5f)), spatialPlanes),
        "spatial AABB test should reject outside boxes");

    Octree octree(Vec3(0.0f, 0.0f, -5.0f), Vec3(40.0f), 4, 1);
    octree.Insert(inside);
    octree.Insert(intersecting);
    octree.Insert(outside);
    octree.Insert(hidden);
    const auto octreeVisible = octree.QueryFrustum(spatialPlanes);
    passed &= Expect(Contains(octreeVisible, inside), "octree frustum query should retain inside objects");
    passed &= Expect(Contains(octreeVisible, intersecting), "octree should retain objects spanning child boundaries and the frustum");
    passed &= Expect(!Contains(octreeVisible, outside), "octree frustum query should reject outside objects");
    passed &= Expect(!Contains(octreeVisible, hidden), "octree frustum query should reject hidden objects");

    return passed ? 0 : 1;
}
