#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace Pyramid
{
    class IGraphicsDevice;

    /** Stable 128-bit identifier for a compiled shader asset or exact source set. */
    struct ShaderAssetId
    {
        u64 high = 0;
        u64 low = 0;

        bool IsValid() const { return high != 0 || low != 0; }
        std::string ToString() const;

        static ShaderAssetId FromString(std::string_view stableName);

        bool operator==(const ShaderAssetId& other) const
        {
            return high == other.high && low == other.low;
        }

        bool operator!=(const ShaderAssetId& other) const
        {
            return !(*this == other);
        }
    };

    struct ShaderAssetIdHash
    {
        std::size_t operator()(const ShaderAssetId& identifier) const noexcept;
    };

    enum class ShaderProgramType : u8
    {
        Graphics,
        Compute
    };

    /** Immutable source description used to compile one shader program. */
    struct ShaderProgramSpecification
    {
        std::string vertexSource;
        std::string tessellationControlSource;
        std::string tessellationEvaluationSource;
        std::string geometrySource;
        std::string fragmentSource;
        std::string computeSource;
        std::string name;
        ShaderAssetId assetId;
    };

    /**
     * Engine-owned immutable compiled shader program.
     *
     * Direct Compile* calls are rejected to keep the stable source/content identity
     * trustworthy. Use ShaderCache::Recompile() to replace a stable asset alias
     * transactionally.
     */
    class ShaderProgram final : public IShader
    {
    public:
        static std::shared_ptr<ShaderProgram> Create(
            IGraphicsDevice& device,
            const ShaderProgramSpecification& specification);

        /** Returns an invalid identifier when the stage combination is malformed. */
        static ShaderAssetId CalculateContentId(
            const ShaderProgramSpecification& specification);

        ShaderProgram(const ShaderProgram&) = delete;
        ShaderProgram& operator=(const ShaderProgram&) = delete;
        ShaderProgram(ShaderProgram&&) = delete;
        ShaderProgram& operator=(ShaderProgram&&) = delete;
        ~ShaderProgram() override = default;

        bool IsValid() const { return m_shader && m_assetId.IsValid() && m_contentId.IsValid(); }
        ShaderProgramType GetType() const { return m_type; }
        bool IsCompute() const { return m_type == ShaderProgramType::Compute; }
        bool HasGeometryStage() const { return m_hasGeometryStage; }
        bool HasTessellationStages() const { return m_hasTessellationStages; }
        u64 GetSourceBytes() const { return m_sourceBytes; }
        const std::string& GetName() const { return m_name; }
        ShaderAssetId GetAssetId() const { return m_assetId; }
        ShaderAssetId GetContentId() const { return m_contentId; }

        void Bind() override;
        void Unbind() override;

        bool Compile(const std::string& vertexSrc, const std::string& fragmentSrc) override;
        bool CompileWithGeometry(
            const std::string& vertexSrc,
            const std::string& geometrySrc,
            const std::string& fragmentSrc) override;
        bool CompileWithTessellation(
            const std::string& vertexSrc,
            const std::string& tessControlSrc,
            const std::string& tessEvalSrc,
            const std::string& fragmentSrc) override;
        bool CompileAdvanced(
            const std::string& vertexSrc,
            const std::string& tessControlSrc,
            const std::string& tessEvalSrc,
            const std::string& geometrySrc,
            const std::string& fragmentSrc) override;
        bool CompileCompute(const std::string& computeSrc) override;
        void DispatchCompute(u32 numGroupsX, u32 numGroupsY, u32 numGroupsZ) override;

        void SetUniformInt(const std::string& name, int value) override;
        void SetUniformFloat(const std::string& name, float value) override;
        void SetUniformFloat2(const std::string& name, float v0, float v1) override;
        void SetUniformFloat3(const std::string& name, float v0, float v1, float v2) override;
        void SetUniformFloat4(
            const std::string& name,
            float v0,
            float v1,
            float v2,
            float v3) override;
        void SetUniformMat3(
            const std::string& name,
            const float* matrixPtr,
            bool transpose = false,
            int count = 1) override;
        void SetUniformMat4(
            const std::string& name,
            const float* matrixPtr,
            bool transpose = false,
            int count = 1) override;

        void BindUniformBuffer(
            const std::string& blockName,
            IUniformBuffer* buffer,
            u32 bindingPoint) override;
        void SetUniformBlockBinding(const std::string& blockName, u32 bindingPoint) override;
        void BindShaderStorageBuffer(
            const std::string& blockName,
            IShaderStorageBuffer* buffer,
            u32 bindingPoint) override;
        void SetShaderStorageBlockBinding(
            const std::string& blockName,
            u32 bindingPoint) override;

    private:
        ShaderProgram(
            std::shared_ptr<IShader> shader,
            ShaderProgramType type,
            bool hasGeometryStage,
            bool hasTessellationStages,
            u64 sourceBytes,
            ShaderAssetId assetId,
            ShaderAssetId contentId,
            std::string name);

        static bool CompileSpecification(
            IShader& shader,
            const ShaderProgramSpecification& specification);
        static bool RejectDirectCompilation();

        std::shared_ptr<IShader> m_shader;
        ShaderProgramType m_type = ShaderProgramType::Graphics;
        bool m_hasGeometryStage = false;
        bool m_hasTessellationStages = false;
        u64 m_sourceBytes = 0;
        ShaderAssetId m_assetId;
        ShaderAssetId m_contentId;
        std::string m_name;
    };
}
