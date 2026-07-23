#include <Pyramid/Graphics/Material/Material.hpp>

#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>
#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <new>
#include <set>
#include <sstream>
#include <utility>
#include <type_traits>

namespace Pyramid
{
    namespace
    {
        constexpr u64 kFnvPrime = 1099511628211ull;
        constexpr u64 kPrimaryOffset = 14695981039346656037ull;
        constexpr u64 kSecondaryOffset = 7809847782465536322ull;

        class StableHasher128
        {
        public:
            void AddByte(u8 value)
            {
                m_high ^= value;
                m_high *= kFnvPrime;
                m_low ^= static_cast<u8>(value ^ 0xa5u);
                m_low *= kFnvPrime;
                m_low ^= m_low >> 32u;
            }
            void AddBytes(const void* data, std::size_t size)
            {
                const auto* bytes = static_cast<const u8*>(data);
                for (std::size_t index = 0; index < size; ++index) AddByte(bytes[index]);
            }
            void AddU32(u32 value)
            {
                for (u32 shift = 0; shift < 32; shift += 8)
                    AddByte(static_cast<u8>((value >> shift) & 0xffu));
            }
            void AddU64(u64 value)
            {
                for (u32 shift = 0; shift < 64; shift += 8)
                    AddByte(static_cast<u8>((value >> shift) & 0xffu));
            }
            void AddFloat(f32 value)
            {
                u32 bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                AddU32(bits);
            }
            void AddString(std::string_view value)
            {
                AddU64(static_cast<u64>(value.size()));
                AddBytes(value.data(), value.size());
            }
            MaterialAssetId Finish() const
            {
                MaterialAssetId result{m_high, m_low};
                if (!result.IsValid()) result.low = 1;
                return result;
            }
        private:
            u64 m_high = kPrimaryOffset;
            u64 m_low = kSecondaryOffset;
        };

        bool IsFinite(const MaterialUniformValue& value)
        {
            return std::visit([](const auto& item)
            {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, i32>) return true;
                else if constexpr (std::is_same_v<T, f32>) return std::isfinite(item);
                else
                {
                    const f32* data = nullptr;
                    std::size_t count = 0;
                    if constexpr (std::is_same_v<T, Math::Vec2>) { data = &item.x; count = 2; }
                    else if constexpr (std::is_same_v<T, Math::Vec3>) { data = &item.x; count = 3; }
                    else if constexpr (std::is_same_v<T, Math::Vec4>) { data = &item.x; count = 4; }
                    else if constexpr (std::is_same_v<T, Math::Mat3>) { data = item.m; count = 9; }
                    else if constexpr (std::is_same_v<T, Math::Mat4>) { data = item.m; count = 16; }
                    for (std::size_t index = 0; index < count; ++index)
                        if (!std::isfinite(data[index])) return false;
                    return true;
                }
            }, value);
        }

        bool Validate(const MaterialSpecification& specification, bool logErrors)
        {
            auto fail = [logErrors](const char* message)
            {
                if (logErrors) PYRAMID_LOG_ERROR("Material creation failed: ", message);
                return false;
            };
            if (!specification.shader || !specification.shader->IsValid())
                return fail("a valid shader program is required");
            if (specification.shader->IsCompute())
                return fail("compute shader programs cannot be used as render materials");

            std::set<u32> slots;
            std::set<std::string> names;
            for (const auto& binding : specification.textures)
            {
                if (binding.slot >= 32) return fail("texture slots must be in the range 0-31");
                if (!binding.texture || !binding.texture->IsLoaded()) return fail("all texture bindings must reference loaded texture resources");
                if (binding.uniformName.empty()) return fail("texture uniform names cannot be empty");
                if (!slots.insert(binding.slot).second) return fail("texture slots must be unique");
                if (!names.insert(binding.uniformName).second) return fail("texture uniform names must be unique");
            }
            for (const auto& uniform : specification.uniforms)
            {
                if (uniform.name.empty()) return fail("uniform names cannot be empty");
                if (!names.insert(uniform.name).second) return fail("uniform and sampler names must be unique");
                if (!IsFinite(uniform.value)) return fail("uniform values must be finite");
            }
            return true;
        }

        void HashUniformValue(StableHasher128& hasher, const MaterialUniformValue& value)
        {
            hasher.AddU32(static_cast<u32>(value.index()));
            std::visit([&hasher](const auto& item)
            {
                using T = std::decay_t<decltype(item)>;
                if constexpr (std::is_same_v<T, i32>) hasher.AddU32(static_cast<u32>(item));
                else if constexpr (std::is_same_v<T, f32>) hasher.AddFloat(item);
                else if constexpr (std::is_same_v<T, Math::Vec2>) { hasher.AddFloat(item.x); hasher.AddFloat(item.y); }
                else if constexpr (std::is_same_v<T, Math::Vec3>) { hasher.AddFloat(item.x); hasher.AddFloat(item.y); hasher.AddFloat(item.z); }
                else if constexpr (std::is_same_v<T, Math::Vec4>) { hasher.AddFloat(item.x); hasher.AddFloat(item.y); hasher.AddFloat(item.z); hasher.AddFloat(item.w); }
                else if constexpr (std::is_same_v<T, Math::Mat3>) for (f32 element : item.m) hasher.AddFloat(element);
                else if constexpr (std::is_same_v<T, Math::Mat4>) for (f32 element : item.m) hasher.AddFloat(element);
            }, value);
        }

        MaterialAssetId HashSpecification(const MaterialSpecification& specification)
        {
            StableHasher128 hasher;
            hasher.AddString("Pyramid.Material.Content.v1");
            const auto shaderId = specification.shader->GetContentId();
            hasher.AddU64(shaderId.high);
            hasher.AddU64(shaderId.low);
            hasher.AddU32(static_cast<u32>(specification.renderState.blendMode));
            hasher.AddU32(specification.renderState.depthTest ? 1u : 0u);
            hasher.AddU32(static_cast<u32>(specification.renderState.depthCompare));
            hasher.AddU32(static_cast<u32>(specification.renderState.cullMode));
            hasher.AddU32(static_cast<u32>(specification.renderState.polygonMode));

            auto textures = specification.textures;
            std::sort(textures.begin(), textures.end(), [](const auto& left, const auto& right)
            {
                return left.slot != right.slot ? left.slot < right.slot : left.uniformName < right.uniformName;
            });
            hasher.AddU32(static_cast<u32>(textures.size()));
            for (const auto& binding : textures)
            {
                hasher.AddString(binding.uniformName);
                hasher.AddU32(binding.slot);
                const auto textureId = binding.texture->GetContentId();
                hasher.AddU64(textureId.high);
                hasher.AddU64(textureId.low);
            }

            auto uniforms = specification.uniforms;
            std::sort(uniforms.begin(), uniforms.end(), [](const auto& left, const auto& right)
            {
                return left.name < right.name;
            });
            hasher.AddU32(static_cast<u32>(uniforms.size()));
            for (const auto& uniform : uniforms)
            {
                hasher.AddString(uniform.name);
                HashUniformValue(hasher, uniform.value);
            }
            return hasher.Finish();
        }

        void ApplyUniform(IShader& shader, const MaterialUniform& uniform)
        {
            std::visit([&](const auto& value)
            {
                using T = std::decay_t<decltype(value)>;
                if constexpr (std::is_same_v<T, i32>) shader.SetUniformInt(uniform.name, value);
                else if constexpr (std::is_same_v<T, f32>) shader.SetUniformFloat(uniform.name, value);
                else if constexpr (std::is_same_v<T, Math::Vec2>) shader.SetUniformFloat2(uniform.name, value.x, value.y);
                else if constexpr (std::is_same_v<T, Math::Vec3>) shader.SetUniformFloat3(uniform.name, value.x, value.y, value.z);
                else if constexpr (std::is_same_v<T, Math::Vec4>) shader.SetUniformFloat4(uniform.name, value.x, value.y, value.z, value.w);
                else if constexpr (std::is_same_v<T, Math::Mat3>) shader.SetUniformMat3(uniform.name, value.m);
                else if constexpr (std::is_same_v<T, Math::Mat4>) shader.SetUniformMat4(uniform.name, value.m);
            }, uniform.value);
        }

        u32 ToDepthFunction(MaterialDepthCompare compare)
        {
            switch (compare)
            {
            case MaterialDepthCompare::Never: return GL_NEVER;
            case MaterialDepthCompare::Less: return GL_LESS;
            case MaterialDepthCompare::LessEqual: return GL_LEQUAL;
            case MaterialDepthCompare::Equal: return GL_EQUAL;
            case MaterialDepthCompare::Greater: return GL_GREATER;
            case MaterialDepthCompare::GreaterEqual: return GL_GEQUAL;
            case MaterialDepthCompare::Always: return GL_ALWAYS;
            }
            return GL_LESS;
        }
    }

    MaterialAssetId MaterialAssetId::FromString(std::string_view stableName)
    {
        if (stableName.empty()) return {};
        StableHasher128 hasher;
        hasher.AddString("Pyramid.Material.Asset.v1");
        hasher.AddString(stableName);
        return hasher.Finish();
    }

    std::string MaterialAssetId::ToString() const
    {
        if (!IsValid()) return {};
        std::ostringstream stream;
        stream << std::hex << std::setfill('0') << std::setw(16) << high << std::setw(16) << low;
        return stream.str();
    }

    std::size_t MaterialAssetIdHash::operator()(const MaterialAssetId& identifier) const noexcept
    {
        const u64 mixed = identifier.high ^ (identifier.low + 0x9e3779b97f4a7c15ull +
            (identifier.high << 6u) + (identifier.high >> 2u));
        return static_cast<std::size_t>(mixed);
    }

    MaterialAssetId Material::CalculateContentId(const MaterialSpecification& specification)
    {
        return Validate(specification, false) ? HashSpecification(specification) : MaterialAssetId{};
    }

    std::shared_ptr<Material> Material::Create(const MaterialSpecification& specification)
    {
        if (!Validate(specification, true)) return nullptr;
        const auto contentId = HashSpecification(specification);
        const auto assetId = specification.assetId.IsValid() ? specification.assetId : contentId;
        try
        {
            auto textures = specification.textures;
            std::sort(textures.begin(), textures.end(), [](const auto& left, const auto& right) { return left.slot < right.slot; });
            auto uniforms = specification.uniforms;
            std::sort(uniforms.begin(), uniforms.end(), [](const auto& left, const auto& right) { return left.name < right.name; });
            return std::shared_ptr<Material>(new Material(
                specification.shader, std::move(textures), std::move(uniforms),
                specification.renderState, assetId, contentId, specification.name));
        }
        catch (const std::bad_alloc&)
        {
            PYRAMID_LOG_ERROR("Material creation failed: allocation failure");
            return nullptr;
        }
    }

    Material::Material(
        std::shared_ptr<ShaderProgram> shader,
        std::vector<MaterialTextureBinding> textures,
        std::vector<MaterialUniform> uniforms,
        MaterialRenderState renderState,
        MaterialAssetId assetId,
        MaterialAssetId contentId,
        std::string name)
        : m_shader(std::move(shader)), m_textures(std::move(textures)),
          m_uniforms(std::move(uniforms)), m_renderState(renderState),
          m_assetId(assetId), m_contentId(contentId), m_name(std::move(name))
    {
    }

    void Material::Apply(IGraphicsDevice& device, bool bindShader, IShader* overrideShader) const
    {
        IShader* shader = overrideShader ? overrideShader : m_shader.get();
        if (bindShader) device.BindShader(shader);

        switch (m_renderState.blendMode)
        {
        case MaterialBlendMode::Opaque: device.EnableBlend(false); break;
        case MaterialBlendMode::Alpha: device.EnableBlend(true); device.SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
        case MaterialBlendMode::Additive: device.EnableBlend(true); device.SetBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
        }
        device.EnableDepthTest(m_renderState.depthTest);
        device.SetDepthFunc(ToDepthFunction(m_renderState.depthCompare));
        device.EnableCullFace(m_renderState.cullMode != MaterialCullMode::None);
        if (m_renderState.cullMode == MaterialCullMode::Front) device.SetCullFace(GL_FRONT);
        else if (m_renderState.cullMode == MaterialCullMode::Back) device.SetCullFace(GL_BACK);
        switch (m_renderState.polygonMode)
        {
        case MaterialPolygonMode::Fill: device.SetPolygonMode(GL_FILL); break;
        case MaterialPolygonMode::Line: device.SetPolygonMode(GL_LINE); break;
        case MaterialPolygonMode::Point: device.SetPolygonMode(GL_POINT); break;
        }

        for (const auto& binding : m_textures)
        {
            device.BindTexture(binding.texture.get(), binding.slot);
            if (shader) shader->SetUniformInt(binding.uniformName, static_cast<i32>(binding.slot));
        }
        if (shader)
            for (const auto& uniform : m_uniforms) ApplyUniform(*shader, uniform);
    }
}
