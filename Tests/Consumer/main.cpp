#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/Shader/ShaderCache.hpp>
#include <Pyramid/Graphics/Texture/TextureCache.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>

#include <iostream>

int main()
{
    const Pyramid::Math::Vec3 value(1.0f, 2.0f, 3.0f);
    const auto meshId = Pyramid::MeshAssetId::FromString("consumer/mesh");
    const auto shaderId = Pyramid::ShaderAssetId::FromString("consumer/shader");
    const auto textureId = Pyramid::TextureAssetId::FromString("consumer/texture");
    const auto materialId = Pyramid::MaterialAssetId::FromString("consumer/material");
    std::cout << "Pyramid Engine " << PYRAMID_VERSION_STRING
              << " | vector length: " << value.Length()
              << " | mesh id: " << meshId.ToString()
              << " | shader id: " << shaderId.ToString()
              << " | texture id: " << textureId.ToString()
              << " | material id: " << materialId.ToString() << '\n';
    return value.LengthSquared() > 0.0f && meshId.IsValid() && shaderId.IsValid() &&
        textureId.IsValid() && materialId.IsValid()
        ? 0
        : 1;
}
