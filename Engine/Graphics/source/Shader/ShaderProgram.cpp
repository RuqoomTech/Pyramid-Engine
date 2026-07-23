#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>

#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Util/Log.hpp>

#include <iomanip>
#include <new>
#include <sstream>
#include <utility>

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
                for (std::size_t index = 0; index < size; ++index)
                {
                    AddByte(bytes[index]);
                }
            }

            void AddU32(u32 value)
            {
                for (u32 shift = 0; shift < 32; shift += 8)
                {
                    AddByte(static_cast<u8>((value >> shift) & 0xffu));
                }
            }

            void AddString(std::string_view value)
            {
                AddU32(static_cast<u32>(value.size()));
                AddBytes(value.data(), value.size());
            }

            ShaderAssetId Finish() const
            {
                ShaderAssetId result{m_high, m_low};
                if (!result.IsValid())
                {
                    result.low = 1;
                }
                return result;
            }

        private:
            u64 m_high = kPrimaryOffset;
            u64 m_low = kSecondaryOffset;
        };

        bool ValidateSpecification(
            const ShaderProgramSpecification& specification,
            bool logErrors)
        {
            const bool hasCompute = !specification.computeSource.empty();
            const bool hasVertex = !specification.vertexSource.empty();
            const bool hasFragment = !specification.fragmentSource.empty();
            const bool hasGeometry = !specification.geometrySource.empty();
            const bool hasTessControl = !specification.tessellationControlSource.empty();
            const bool hasTessEvaluation = !specification.tessellationEvaluationSource.empty();
            const bool hasAnyGraphics = hasVertex || hasFragment || hasGeometry ||
                                        hasTessControl || hasTessEvaluation;

            if (hasCompute)
            {
                if (hasAnyGraphics)
                {
                    if (logErrors)
                    {
                        PYRAMID_LOG_ERROR(
                            "Shader program creation failed: compute source cannot be combined with graphics stages");
                    }
                    return false;
                }
                return true;
            }

            if (!hasVertex || !hasFragment)
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Shader program creation failed: graphics programs require vertex and fragment sources");
                }
                return false;
            }

            if (hasTessControl != hasTessEvaluation)
            {
                if (logErrors)
                {
                    PYRAMID_LOG_ERROR(
                        "Shader program creation failed: tessellation control and evaluation stages must be supplied together");
                }
                return false;
            }

            return true;
        }

        ShaderAssetId HashSpecification(const ShaderProgramSpecification& specification)
        {
            StableHasher128 hasher;
            hasher.AddString("Pyramid.ShaderProgram.Content.v1");
            hasher.AddString(specification.vertexSource);
            hasher.AddString(specification.tessellationControlSource);
            hasher.AddString(specification.tessellationEvaluationSource);
            hasher.AddString(specification.geometrySource);
            hasher.AddString(specification.fragmentSource);
            hasher.AddString(specification.computeSource);
            return hasher.Finish();
        }

        u64 CalculateSourceBytes(const ShaderProgramSpecification& specification)
        {
            return static_cast<u64>(specification.vertexSource.size()) +
                   static_cast<u64>(specification.tessellationControlSource.size()) +
                   static_cast<u64>(specification.tessellationEvaluationSource.size()) +
                   static_cast<u64>(specification.geometrySource.size()) +
                   static_cast<u64>(specification.fragmentSource.size()) +
                   static_cast<u64>(specification.computeSource.size());
        }
    }

    ShaderAssetId ShaderAssetId::FromString(std::string_view stableName)
    {
        if (stableName.empty())
        {
            return {};
        }

        StableHasher128 hasher;
        hasher.AddString("Pyramid.ShaderProgram.Asset.v1");
        hasher.AddString(stableName);
        return hasher.Finish();
    }

    std::string ShaderAssetId::ToString() const
    {
        if (!IsValid())
        {
            return {};
        }

        std::ostringstream stream;
        stream << std::hex << std::setfill('0')
               << std::setw(16) << high
               << std::setw(16) << low;
        return stream.str();
    }

    std::size_t ShaderAssetIdHash::operator()(const ShaderAssetId& identifier) const noexcept
    {
        const u64 mixed = identifier.high ^
            (identifier.low + 0x9e3779b97f4a7c15ull +
             (identifier.high << 6u) + (identifier.high >> 2u));
        return static_cast<std::size_t>(mixed);
    }

    ShaderAssetId ShaderProgram::CalculateContentId(
        const ShaderProgramSpecification& specification)
    {
        if (!ValidateSpecification(specification, false))
        {
            return {};
        }
        return HashSpecification(specification);
    }

    std::shared_ptr<ShaderProgram> ShaderProgram::Create(
        IGraphicsDevice& device,
        const ShaderProgramSpecification& specification)
    {
        if (!ValidateSpecification(specification, true))
        {
            return nullptr;
        }

        const ShaderAssetId contentId = HashSpecification(specification);
        const ShaderAssetId assetId = specification.assetId.IsValid()
            ? specification.assetId
            : contentId;

        try
        {
            auto shader = device.CreateShader();
            if (!shader)
            {
                PYRAMID_LOG_ERROR(
                    "Shader program creation failed: graphics device could not allocate a shader");
                return nullptr;
            }

            if (!CompileSpecification(*shader, specification))
            {
                PYRAMID_LOG_ERROR(
                    "Shader program creation failed",
                    specification.name.empty() ? "" : " for '",
                    specification.name.empty() ? "" : specification.name,
                    specification.name.empty() ? "" : "'");
                return nullptr;
            }

            const bool compute = !specification.computeSource.empty();
            return std::shared_ptr<ShaderProgram>(new ShaderProgram(
                std::move(shader),
                compute ? ShaderProgramType::Compute : ShaderProgramType::Graphics,
                !specification.geometrySource.empty(),
                !specification.tessellationControlSource.empty(),
                CalculateSourceBytes(specification),
                assetId,
                contentId,
                specification.name));
        }
        catch (const std::bad_alloc&)
        {
            PYRAMID_LOG_ERROR("Shader program creation failed: memory allocation failed");
            return nullptr;
        }
    }

    ShaderProgram::ShaderProgram(
        std::shared_ptr<IShader> shader,
        ShaderProgramType type,
        bool hasGeometryStage,
        bool hasTessellationStages,
        u64 sourceBytes,
        ShaderAssetId assetId,
        ShaderAssetId contentId,
        std::string name)
        : m_shader(std::move(shader)),
          m_type(type),
          m_hasGeometryStage(hasGeometryStage),
          m_hasTessellationStages(hasTessellationStages),
          m_sourceBytes(sourceBytes),
          m_assetId(assetId),
          m_contentId(contentId),
          m_name(std::move(name))
    {
    }

    bool ShaderProgram::CompileSpecification(
        IShader& shader,
        const ShaderProgramSpecification& specification)
    {
        if (!specification.computeSource.empty())
        {
            return shader.CompileCompute(specification.computeSource);
        }

        const bool hasGeometry = !specification.geometrySource.empty();
        const bool hasTessellation = !specification.tessellationControlSource.empty();
        if (hasGeometry || hasTessellation)
        {
            return shader.CompileAdvanced(
                specification.vertexSource,
                specification.tessellationControlSource,
                specification.tessellationEvaluationSource,
                specification.geometrySource,
                specification.fragmentSource);
        }

        return shader.Compile(specification.vertexSource, specification.fragmentSource);
    }

    bool ShaderProgram::RejectDirectCompilation()
    {
        PYRAMID_LOG_ERROR(
            "ShaderProgram is immutable; use ShaderCache::Recompile() for transactional replacement");
        return false;
    }

    void ShaderProgram::Bind()
    {
        if (m_shader)
        {
            m_shader->Bind();
        }
    }

    void ShaderProgram::Unbind()
    {
        if (m_shader)
        {
            m_shader->Unbind();
        }
    }

    bool ShaderProgram::Compile(const std::string&, const std::string&)
    {
        return RejectDirectCompilation();
    }

    bool ShaderProgram::CompileWithGeometry(
        const std::string&,
        const std::string&,
        const std::string&)
    {
        return RejectDirectCompilation();
    }

    bool ShaderProgram::CompileWithTessellation(
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&)
    {
        return RejectDirectCompilation();
    }

    bool ShaderProgram::CompileAdvanced(
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&,
        const std::string&)
    {
        return RejectDirectCompilation();
    }

    bool ShaderProgram::CompileCompute(const std::string&)
    {
        return RejectDirectCompilation();
    }

    void ShaderProgram::DispatchCompute(u32 x, u32 y, u32 z)
    {
        if (m_shader)
        {
            m_shader->DispatchCompute(x, y, z);
        }
    }

    void ShaderProgram::SetUniformInt(const std::string& name, int value)
    {
        if (m_shader)
        {
            m_shader->SetUniformInt(name, value);
        }
    }

    void ShaderProgram::SetUniformFloat(const std::string& name, float value)
    {
        if (m_shader)
        {
            m_shader->SetUniformFloat(name, value);
        }
    }

    void ShaderProgram::SetUniformFloat2(
        const std::string& name,
        float v0,
        float v1)
    {
        if (m_shader)
        {
            m_shader->SetUniformFloat2(name, v0, v1);
        }
    }

    void ShaderProgram::SetUniformFloat3(
        const std::string& name,
        float v0,
        float v1,
        float v2)
    {
        if (m_shader)
        {
            m_shader->SetUniformFloat3(name, v0, v1, v2);
        }
    }

    void ShaderProgram::SetUniformFloat4(
        const std::string& name,
        float v0,
        float v1,
        float v2,
        float v3)
    {
        if (m_shader)
        {
            m_shader->SetUniformFloat4(name, v0, v1, v2, v3);
        }
    }

    void ShaderProgram::SetUniformMat3(
        const std::string& name,
        const float* matrixPtr,
        bool transpose,
        int count)
    {
        if (m_shader)
        {
            m_shader->SetUniformMat3(name, matrixPtr, transpose, count);
        }
    }

    void ShaderProgram::SetUniformMat4(
        const std::string& name,
        const float* matrixPtr,
        bool transpose,
        int count)
    {
        if (m_shader)
        {
            m_shader->SetUniformMat4(name, matrixPtr, transpose, count);
        }
    }

    void ShaderProgram::BindUniformBuffer(
        const std::string& blockName,
        IUniformBuffer* buffer,
        u32 bindingPoint)
    {
        if (m_shader)
        {
            m_shader->BindUniformBuffer(blockName, buffer, bindingPoint);
        }
    }

    void ShaderProgram::SetUniformBlockBinding(
        const std::string& blockName,
        u32 bindingPoint)
    {
        if (m_shader)
        {
            m_shader->SetUniformBlockBinding(blockName, bindingPoint);
        }
    }

    void ShaderProgram::BindShaderStorageBuffer(
        const std::string& blockName,
        IShaderStorageBuffer* buffer,
        u32 bindingPoint)
    {
        if (m_shader)
        {
            m_shader->BindShaderStorageBuffer(blockName, buffer, bindingPoint);
        }
    }

    void ShaderProgram::SetShaderStorageBlockBinding(
        const std::string& blockName,
        u32 bindingPoint)
    {
        if (m_shader)
        {
            m_shader->SetShaderStorageBlockBinding(blockName, bindingPoint);
        }
    }
}
