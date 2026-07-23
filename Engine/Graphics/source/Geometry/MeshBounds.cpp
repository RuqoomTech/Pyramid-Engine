#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

namespace Pyramid::Geometry
{
    namespace
    {
        std::string NormalizeSemantic(const std::string& semantic)
        {
            std::string normalized;
            normalized.reserve(semantic.size());

            for (const unsigned char character : semantic)
            {
                if (std::isalnum(character) != 0)
                {
                    normalized.push_back(static_cast<char>(std::tolower(character)));
                }
            }

            return normalized;
        }

        bool IsPositionSemantic(const std::string& semantic)
        {
            const std::string normalized = NormalizeSemantic(semantic);
            constexpr const char* suffix = "position";
            constexpr size_t suffixLength = 8;

            return normalized.size() >= suffixLength &&
                   normalized.compare(normalized.size() - suffixLength, suffixLength, suffix) == 0;
        }

        u32 PositionComponentCount(ShaderDataType type)
        {
            switch (type)
            {
            case ShaderDataType::Float2:
                return 2;
            case ShaderDataType::Float3:
                return 3;
            case ShaderDataType::Float4:
                return 4;
            default:
                return 0;
            }
        }
    }

    bool CalculateLocalBounds(
        const void* vertexData,
        u32 vertexDataSize,
        const BufferLayout& layout,
        Math::Vec3& minPoint,
        Math::Vec3& maxPoint)
    {
        if (!vertexData || vertexDataSize == 0 || layout.GetStride() == 0)
        {
            return false;
        }

        const BufferElement* positionElement = nullptr;
        u32 positionComponents = 0;
        for (const auto& element : layout.GetElements())
        {
            const u32 components = PositionComponentCount(element.Type);
            if (components != 0 && IsPositionSemantic(element.Name))
            {
                positionElement = &element;
                positionComponents = components;
                break;
            }
        }

        if (!positionElement)
        {
            return false;
        }

        const u32 stride = layout.GetStride();
        const u32 positionBytes = positionComponents * static_cast<u32>(sizeof(f32));
        if (positionElement->Offset > stride || positionBytes > stride - positionElement->Offset)
        {
            return false;
        }

        if (vertexDataSize < stride || vertexDataSize % stride != 0)
        {
            return false;
        }

        const auto* bytes = static_cast<const u8*>(vertexData);
        const u32 vertexCount = vertexDataSize / stride;
        const f32 maximum = std::numeric_limits<f32>::max();
        Math::Vec3 calculatedMin(maximum);
        Math::Vec3 calculatedMax(-maximum);

        for (u32 vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            f32 components[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            const u8* positionAddress =
                bytes + static_cast<size_t>(vertexIndex) * stride + positionElement->Offset;
            std::memcpy(components, positionAddress, positionBytes);

            const Math::Vec3 position(components[0], components[1], components[2]);
            if (!std::isfinite(position.x) ||
                !std::isfinite(position.y) ||
                !std::isfinite(position.z))
            {
                return false;
            }

            calculatedMin.x = Math::Min(calculatedMin.x, position.x);
            calculatedMin.y = Math::Min(calculatedMin.y, position.y);
            calculatedMin.z = Math::Min(calculatedMin.z, position.z);
            calculatedMax.x = Math::Max(calculatedMax.x, position.x);
            calculatedMax.y = Math::Max(calculatedMax.y, position.y);
            calculatedMax.z = Math::Max(calculatedMax.z, position.z);
        }

        minPoint = calculatedMin;
        maxPoint = calculatedMax;
        return true;
    }
}
