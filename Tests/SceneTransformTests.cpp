#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Math/Math.hpp>

#include <cmath>
#include <iostream>
#include <memory>

namespace
{
    bool NearlyEqual(Pyramid::f32 lhs, Pyramid::f32 rhs, Pyramid::f32 epsilon = 0.001f)
    {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool NearlyEqual(const Pyramid::Math::Vec3 &lhs, const Pyramid::Math::Vec3 &rhs, Pyramid::f32 epsilon = 0.001f)
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
}

int main()
{
    using Pyramid::SceneNode;
    using Pyramid::Math::Quat;
    using Pyramid::Math::Radians;
    using Pyramid::Math::Vec3;

    bool passed = true;

    const auto root = std::make_shared<SceneNode>("Root");
    const auto child = std::make_shared<SceneNode>("Child");
    const auto grandchild = std::make_shared<SceneNode>("Grandchild");

    root->SetLocalTransform(
        Vec3(10.0f, 0.0f, 0.0f),
        Quat::FromAxisAngle(Vec3::Forward, Radians(90.0f)),
        Vec3(2.0f, 2.0f, 2.0f));
    child->SetLocalPosition(Vec3(1.0f, 0.0f, 0.0f));
    grandchild->SetLocalPosition(Vec3(0.0f, 1.0f, 0.0f));

    root->AddChild(child);
    child->AddChild(grandchild);

    passed &= Expect(root->GetChildCount() == 1, "root should own one direct child");
    passed &= Expect(child->GetParent() == root, "child should expose its parent");
    passed &= Expect(grandchild->GetParent() == child, "grandchild should expose its parent");
    passed &= Expect(
        NearlyEqual(child->GetWorldPosition(), Vec3(10.0f, 2.0f, 0.0f)),
        "parent translation, rotation, and scale should affect child world position");
    passed &= Expect(
        NearlyEqual(grandchild->GetWorldPosition(), Vec3(8.0f, 2.0f, 0.0f)),
        "hierarchical transforms should compose through multiple generations");
    passed &= Expect(
        NearlyEqual(root->TransformPointToWorld(Vec3::Right), Vec3(10.0f, 2.0f, 0.0f)),
        "point transformation should include translation, rotation, and scale");
    passed &= Expect(
        NearlyEqual(root->TransformDirectionToWorld(Vec3::Right), Vec3(0.0f, 2.0f, 0.0f)),
        "direction transformation should exclude translation");
    passed &= Expect(
        NearlyEqual(child->GetWorldRotation().RotateVector(Vec3::Right), Vec3::Up),
        "world rotation should compose the parent and local quaternion chain");

    // Prime all caches, then mutate the root. Descendant results must not remain stale.
    (void)grandchild->GetWorldTransform();
    root->SetLocalPosition(Vec3(20.0f, 0.0f, 0.0f));
    passed &= Expect(
        NearlyEqual(grandchild->GetWorldPosition(), Vec3(18.0f, 2.0f, 0.0f)),
        "ancestor changes should invalidate every descendant world transform");

    root->SetLocalScale(Vec3(3.0f, 4.0f, 5.0f));
    passed &= Expect(
        NearlyEqual(root->GetWorldScale(), Vec3(3.0f, 4.0f, 5.0f)),
        "world scale should reflect transformed basis magnitudes");

    const Quat doubledRotation = Quat::FromAxisAngle(Vec3::Up, Radians(45.0f)) * 2.0f;
    child->SetLocalRotation(doubledRotation);
    passed &= Expect(
        child->GetLocalRotation().IsNormalized(),
        "local rotations should be normalized before matrix construction");
    passed &= Expect(
        child->GetWorldRotation().IsNormalized(),
        "composed world rotations should remain normalized");
    const Vec3 expectedRotatedDirection =
        root->GetLocalRotation().RotateVector(child->GetLocalRotation().RotateVector(Vec3::Right));
    passed &= Expect(
        NearlyEqual(child->GetWorldRotation().RotateVector(Vec3::Right), expectedRotatedDirection),
        "world rotation should retain the correct parent-before-local order");

    // Reparenting preserves local TRS but must recompute world state immediately.
    const auto secondParent = std::make_shared<SceneNode>("SecondParent");
    secondParent->SetLocalPosition(Vec3(-5.0f, 0.0f, 0.0f));
    child->SetLocalTransform(Vec3(1.0f, 0.0f, 0.0f), Quat::Identity, Vec3::One);
    secondParent->AddChild(child);

    passed &= Expect(root->GetChildCount() == 0, "reparenting should detach the child from its old parent");
    passed &= Expect(secondParent->GetChildCount() == 1, "reparenting should attach the child once");
    passed &= Expect(child->GetParent() == secondParent, "reparenting should update the parent link");
    passed &= Expect(
        NearlyEqual(child->GetWorldPosition(), Vec3(-4.0f, 0.0f, 0.0f)),
        "reparenting should invalidate cached world transforms");

    secondParent->AddChild(child);
    passed &= Expect(secondParent->GetChildCount() == 1, "adding the same child twice should not duplicate it");

    child->SetParent(nullptr);
    passed &= Expect(!child->HasParent(), "detaching should clear the parent link");
    passed &= Expect(secondParent->GetChildCount() == 0, "detaching should remove the old parent's child entry");
    passed &= Expect(
        NearlyEqual(child->GetWorldPosition(), child->GetLocalPosition()),
        "a detached node's world transform should equal its local transform");

    // Cycles must be rejected without disturbing the valid hierarchy.
    root->AddChild(child);
    child->AddChild(grandchild);
    grandchild->AddChild(root);
    passed &= Expect(!root->HasParent(), "cycle creation should leave the root detached");
    passed &= Expect(root->GetParent() == nullptr, "cycle rejection should preserve the root parent");
    passed &= Expect(grandchild->GetChildCount() == 0, "cycle rejection should not add an ancestor as a child");

    // Hierarchy participation requires shared ownership, but unmanaged nodes must fail safely.
    SceneNode unmanagedParent("Unmanaged");
    const auto unmanagedChild = std::make_shared<SceneNode>("UnmanagedChild");
    unmanagedParent.AddChild(unmanagedChild);
    passed &= Expect(
        !unmanagedChild->HasParent() && unmanagedParent.GetChildCount() == 0,
        "an unmanaged parent should reject hierarchy mutation without throwing");

    // Externally retained children must become roots when their parent is destroyed.
    auto retainedChild = std::make_shared<SceneNode>("Retained");
    retainedChild->SetLocalPosition(Vec3(3.0f, 4.0f, 5.0f));
    {
        auto temporaryParent = std::make_shared<SceneNode>("Temporary");
        temporaryParent->SetLocalPosition(Vec3(10.0f, 0.0f, 0.0f));
        temporaryParent->AddChild(retainedChild);
        passed &= Expect(
            NearlyEqual(retainedChild->GetWorldPosition(), Vec3(13.0f, 4.0f, 5.0f)),
            "temporary parent should affect retained child before destruction");
    }

    passed &= Expect(!retainedChild->HasParent(), "destroying a parent should detach externally retained children");
    passed &= Expect(
        NearlyEqual(retainedChild->GetWorldPosition(), Vec3(3.0f, 4.0f, 5.0f)),
        "parent destruction should invalidate the retained child's world transform");

    return passed ? 0 : 1;
}
