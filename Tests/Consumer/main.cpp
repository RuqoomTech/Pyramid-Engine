#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>

#include <iostream>

int main()
{
    const Pyramid::Math::Vec3 value(1.0f, 2.0f, 3.0f);
    std::cout << "Pyramid Engine " << PYRAMID_VERSION_STRING
              << " | vector length: " << value.Length() << '\n';
    return value.LengthSquared() > 0.0f ? 0 : 1;
}
