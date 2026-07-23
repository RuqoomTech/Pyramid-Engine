#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/Shader/ShaderCache.hpp>

#include <iostream>

int main()
{
    const Pyramid::Math::Vec3 value(1.0f, 2.0f, 3.0f);
    const auto meshId = Pyramid::MeshAssetId::FromString("consumer/mesh");
    const auto shaderId = Pyramid::ShaderAssetId::FromString("consumer/shader");
    std::cout << "Pyramid Engine " << PYRAMID_VERSION_STRING
              << " | vector length: " << value.Length()
              << " | mesh id: " << meshId.ToString()
              << " | shader id: " << shaderId.ToString() << '\n';
    return value.LengthSquared() > 0.0f && meshId.IsValid() && shaderId.IsValid()
        ? 0
        : 1;
}
