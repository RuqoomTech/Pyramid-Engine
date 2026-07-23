#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>
#include <Pyramid/Graphics/Texture/TextureResource.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Pyramid
{
    class IGraphicsDevice;

    struct MaterialAssetId
    {
        u64 high = 0;
        u64 low = 0;

        bool IsValid() const { return high != 0 || low != 0; }
        std::string ToString() const;
        static MaterialAssetId FromString(std::string_view stableName);

        bool operator==(const MaterialAssetId& other) const
        {
            return high == other.high && low == other.low;
        }
        bool operator!=(const MaterialAssetId& other) const { return !(*this == other); }
    };

    struct MaterialAssetIdHash
    {
        std::size_t operator()(const MaterialAssetId& identifier) const noexcept;
    };

    enum class MaterialBlendMode : u8
    {
        Opaque,
        Alpha,
        Additive
    };

    enum class MaterialDepthCompare : u8
    {
        Never,
        Less,
        LessEqual,
        Equal,
        Greater,
        GreaterEqual,
        Always
    };

    enum class MaterialCullMode : u8
    {
        None,
        Front,
        Back
    };

    enum class MaterialPolygonMode : u8
    {
        Fill,
        Line,
        Point
    };

    struct MaterialRenderState
    {
        MaterialBlendMode blendMode = MaterialBlendMode::Opaque;
        bool depthTest = true;
        MaterialDepthCompare depthCompare = MaterialDepthCompare::Less;
        MaterialCullMode cullMode = MaterialCullMode::Back;
        MaterialPolygonMode polygonMode = MaterialPolygonMode::Fill;

        bool operator==(const MaterialRenderState& other) const
        {
            return blendMode == other.blendMode && depthTest == other.depthTest &&
                depthCompare == other.depthCompare && cullMode == other.cullMode &&
                polygonMode == other.polygonMode;
        }
    };

    using MaterialUniformValue = std::variant<
        i32,
        f32,
        Math::Vec2,
        Math::Vec3,
        Math::Vec4,
        Math::Mat3,
        Math::Mat4>;

    struct MaterialUniform
    {
        std::string name;
        MaterialUniformValue value;
    };

    struct MaterialTextureBinding
    {
        std::string uniformName;
        u32 slot = 0;
        std::shared_ptr<TextureResource> texture;
    };

    struct MaterialSpecification
    {
        std::shared_ptr<ShaderProgram> shader;
        std::vector<MaterialTextureBinding> textures;
        std::vector<MaterialUniform> uniforms;
        MaterialRenderState renderState;
        MaterialAssetId assetId;
        std::string name;
    };

    /** Immutable material resource with stable shader, texture, uniform, and state identity. */
    class Material final
    {
    public:
        static std::shared_ptr<Material> Create(const MaterialSpecification& specification);
        static MaterialAssetId CalculateContentId(const MaterialSpecification& specification);

        Material(const Material&) = delete;
        Material& operator=(const Material&) = delete;
        Material(Material&&) = delete;
        Material& operator=(Material&&) = delete;
        ~Material() = default;

        bool IsValid() const { return m_shader && m_assetId.IsValid() && m_contentId.IsValid(); }
        MaterialAssetId GetAssetId() const { return m_assetId; }
        MaterialAssetId GetContentId() const { return m_contentId; }
        const std::string& GetName() const { return m_name; }
        const std::shared_ptr<ShaderProgram>& GetShader() const { return m_shader; }
        const std::vector<MaterialTextureBinding>& GetTextures() const { return m_textures; }
        const std::vector<MaterialUniform>& GetUniforms() const { return m_uniforms; }
        const MaterialRenderState& GetRenderState() const { return m_renderState; }

        /** Applies shader, fixed render state, texture bindings, and static uniforms. */
        void Apply(IGraphicsDevice& device, bool bindShader = true, IShader* overrideShader = nullptr) const;

    private:
        Material(
            std::shared_ptr<ShaderProgram> shader,
            std::vector<MaterialTextureBinding> textures,
            std::vector<MaterialUniform> uniforms,
            MaterialRenderState renderState,
            MaterialAssetId assetId,
            MaterialAssetId contentId,
            std::string name);

        std::shared_ptr<ShaderProgram> m_shader;
        std::vector<MaterialTextureBinding> m_textures;
        std::vector<MaterialUniform> m_uniforms;
        MaterialRenderState m_renderState;
        MaterialAssetId m_assetId;
        MaterialAssetId m_contentId;
        std::string m_name;
    };
}
