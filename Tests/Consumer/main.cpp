#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>

#include <iostream>

int main()
{
    const Pyramid::Math::Vec3 value(1.0f, 2.0f, 3.0f);
    const auto meshId = Pyramid::MeshAssetId::FromString("consumer/mesh");
    std::cout << "Pyramid Engine " << PYRAMID_VERSION_STRING
              << " | vector length: " << value.Length()
              << " | mesh id: " << meshId.ToString() << '\n';
    return value.LengthSquared() > 0.0f && meshId.IsValid() ? 0 : 1;
}
