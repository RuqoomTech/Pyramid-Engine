#include <Pyramid/Graphics/Shader/ShaderCache.hpp>
#include <Pyramid/Util/Log.hpp>

#include "TestGraphicsDevice.hpp"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using Pyramid::IShader;
    using Pyramid::ShaderAssetId;
    using Pyramid::ShaderCache;
    using Pyramid::ShaderProgram;
    using Pyramid::ShaderProgramSpecification;
    using Pyramid::ShaderProgramType;
    using Pyramid::u32;

    enum class CompilePath
    {
        None,
        Graphics,
        Advanced,
        Compute
    };

    class FakeShader final : public IShader
    {
    public:
        void Bind() override { ++binds; }
        void Unbind() override { ++unbinds; }

        bool Compile(const std::string& vertex, const std::string& fragment) override
        {
            path = CompilePath::Graphics;
            sources = vertex + fragment;
            return ShouldCompile();
        }

        bool CompileWithGeometry(
            const std::string& vertex,
            const std::string& geometry,
            const std::string& fragment) override
        {
            path = CompilePath::Advanced;
            sources = vertex + geometry + fragment;
            return ShouldCompile();
        }

        bool CompileWithTessellation(
            const std::string& vertex,
            const std::string& tessControl,
            const std::string& tessEvaluation,
            const std::string& fragment) override
        {
            path = CompilePath::Advanced;
            sources = vertex + tessControl + tessEvaluation + fragment;
            return ShouldCompile();
        }

        bool CompileAdvanced(
            const std::string& vertex,
            const std::string& tessControl,
            const std::string& tessEvaluation,
            const std::string& geometry,
            const std::string& fragment) override
        {
            path = CompilePath::Advanced;
            sources = vertex + tessControl + tessEvaluation + geometry + fragment;
            return ShouldCompile();
        }

        bool CompileCompute(const std::string& compute) override
        {
            path = CompilePath::Compute;
            sources = compute;
            return ShouldCompile();
        }

        void DispatchCompute(u32, u32, u32) override { ++dispatches; }
        void SetUniformInt(const std::string&, int) override { ++uniformWrites; }
        void SetUniformFloat(const std::string&, float) override { ++uniformWrites; }
        void SetUniformFloat2(const std::string&, float, float) override { ++uniformWrites; }
        void SetUniformFloat3(const std::string&, float, float, float) override { ++uniformWrites; }
        void SetUniformFloat4(const std::string&, float, float, float, float) override
        {
            ++uniformWrites;
        }
        void SetUniformMat3(const std::string&, const float*, bool, int) override
        {
            ++uniformWrites;
        }
        void SetUniformMat4(const std::string&, const float*, bool, int) override
        {
            ++uniformWrites;
        }
        void BindUniformBuffer(const std::string&, Pyramid::IUniformBuffer*, u32) override {}
        void SetUniformBlockBinding(const std::string&, u32) override {}
        void BindShaderStorageBuffer(
            const std::string&,
            Pyramid::IShaderStorageBuffer*,
            u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, u32) override {}

        CompilePath path = CompilePath::None;
        std::string sources;
        u32 binds = 0;
        u32 unbinds = 0;
        u32 dispatches = 0;
        u32 uniformWrites = 0;

    private:
        bool ShouldCompile() const
        {
            return sources.find("FAIL") == std::string::npos;
        }
    };

    int Fail(const char* message)
    {
        std::cerr << "ShaderCacheTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    ShaderProgramSpecification MakeGraphics(
        std::string fragment = "fragment-v1",
        std::string name = "DebugShader")
    {
        ShaderProgramSpecification specification;
        specification.vertexSource = "vertex-v1";
        specification.fragmentSource = std::move(fragment);
        specification.name = std::move(name);
        return specification;
    }
}

int main()
{
    Pyramid::Util::Logger::GetInstance().EnableConsole(false);

    Pyramid::Tests::TestGraphicsDevice device;
    std::vector<std::shared_ptr<FakeShader>> createdShaders;
    device.shaderFactory = [&createdShaders]() {
        auto shader = std::make_shared<FakeShader>();
        createdShaders.push_back(shader);
        return shader;
    };

    ShaderCache cache(device);

    auto specification = MakeGraphics();
    const ShaderAssetId contentId = ShaderProgram::CalculateContentId(specification);
    if (!contentId.IsValid() ||
        contentId.ToString() != "2f8ef7c76be6f19e266653755c982e10")
    {
        return Fail("graphics content identifier is invalid or unstable");
    }

    auto renamed = MakeGraphics("fragment-v1", "DifferentDebugName");
    if (ShaderProgram::CalculateContentId(renamed) != contentId)
    {
        return Fail("debug name changed the content identifier");
    }

    auto changed = MakeGraphics("fragment-v2", "Changed");
    const ShaderAssetId changedContentId = ShaderProgram::CalculateContentId(changed);
    if (!changedContentId.IsValid() || changedContentId == contentId)
    {
        return Fail("different source did not change the content identifier");
    }

    const ShaderAssetId stableId = ShaderAssetId::FromString("shaders/basic");
    if (!stableId.IsValid() ||
        stableId != ShaderAssetId::FromString("shaders/basic") ||
        stableId == ShaderAssetId::FromString("shaders/other") ||
        ShaderAssetId::FromString({}).IsValid() ||
        stableId.ToString() != "ac97d5593aa6370740d859f09d37a7ef")
    {
        return Fail("stable shader identifiers are not deterministic");
    }

    auto invalidGraphics = specification;
    invalidGraphics.fragmentSource.clear();
    if (ShaderProgram::CalculateContentId(invalidGraphics).IsValid())
    {
        return Fail("graphics specification without fragment source was accepted");
    }

    auto invalidTessellation = specification;
    invalidTessellation.tessellationControlSource = "tess-control";
    if (ShaderProgram::CalculateContentId(invalidTessellation).IsValid())
    {
        return Fail("unpaired tessellation stage was accepted");
    }

    auto invalidCompute = specification;
    invalidCompute.computeSource = "compute";
    if (ShaderProgram::CalculateContentId(invalidCompute).IsValid())
    {
        return Fail("mixed compute and graphics specification was accepted");
    }

    specification.assetId = stableId;
    auto first = cache.GetOrCreate(specification);
    if (!first || !first->IsValid() || first->GetAssetId() != stableId ||
        first->GetContentId() != contentId || first->GetType() != ShaderProgramType::Graphics ||
        first->GetSourceBytes() != specification.vertexSource.size() +
                                   specification.fragmentSource.size() ||
        device.shaderCreations != 1 || createdShaders.size() != 1 ||
        createdShaders[0]->path != CompilePath::Graphics)
    {
        return Fail("first graphics program was not compiled correctly");
    }

    auto contentAlias = cache.GetOrCreate(renamed);
    if (contentAlias != first || cache.Find(contentId) != first ||
        cache.Find(stableId) != first || device.shaderCreations != 1)
    {
        return Fail("identical shader source was not compiled once");
    }

    first->Bind();
    first->SetUniformFloat("u_Value", 1.0f);
    first->Unbind();
    if (createdShaders[0]->binds != 1 || createdShaders[0]->uniformWrites != 1 ||
        createdShaders[0]->unbinds != 1)
    {
        return Fail("shader program did not forward runtime operations");
    }

    if (first->Compile("other", "other"))
    {
        return Fail("immutable shader program accepted direct recompilation");
    }

    changed.assetId = stableId;
    if (cache.GetOrCreate(changed) || device.shaderCreations != 1)
    {
        return Fail("identifier/source conflict was not rejected");
    }

    changed.assetId = {};
    if (!cache.Recompile(stableId, changed) || device.shaderCreations != 2)
    {
        return Fail("transactional recompilation did not compile a replacement");
    }

    auto replacement = cache.Find(stableId);
    if (!replacement || replacement == first || replacement->GetContentId() != changedContentId ||
        cache.Find(contentId) != first || !first->IsValid())
    {
        return Fail("successful recompilation did not preserve the old program safely");
    }

    auto failing = MakeGraphics("FAIL-fragment", "Broken");
    auto beforeFailure = cache.Find(stableId);
    if (cache.Recompile(stableId, failing) || device.shaderCreations != 3 ||
        cache.Find(stableId) != beforeFailure || !beforeFailure->IsValid())
    {
        return Fail("failed recompilation changed the active stable alias");
    }

    if (!cache.Recompile(stableId, changed) || device.shaderCreations != 3 ||
        cache.Find(stableId) != beforeFailure)
    {
        return Fail("same-source recompilation was not reused without compilation");
    }

    auto alternate = MakeGraphics("fragment-v3", "Alternate");
    const ShaderAssetId alternateId = ShaderAssetId::FromString("shaders/alternate");
    alternate.assetId = alternateId;
    auto alternateProgram = cache.GetOrCreate(alternate);
    if (!alternateProgram || device.shaderCreations != 4)
    {
        return Fail("alternate shader was not compiled");
    }

    alternate.assetId = {};
    if (!cache.Recompile(stableId, alternate) || device.shaderCreations != 4 ||
        cache.Find(stableId) != alternateProgram)
    {
        return Fail("recompilation did not reuse already compiled source");
    }

    if (cache.Recompile(contentId, changed))
    {
        return Fail("content-derived identifier was allowed to recompile");
    }

    ShaderProgramSpecification compute;
    compute.computeSource = "compute-v1";
    compute.name = "Compute";
    auto computeProgram = cache.GetOrCreate(compute);
    if (!computeProgram || !computeProgram->IsCompute() ||
        createdShaders.back()->path != CompilePath::Compute)
    {
        return Fail("compute program did not use the compute compilation path");
    }

    auto advanced = MakeGraphics("fragment-advanced", "Advanced");
    advanced.geometrySource = "geometry";
    advanced.tessellationControlSource = "tess-control";
    advanced.tessellationEvaluationSource = "tess-evaluation";
    auto advancedProgram = cache.GetOrCreate(advanced);
    if (!advancedProgram || !advancedProgram->HasGeometryStage() ||
        !advancedProgram->HasTessellationStages() ||
        createdShaders.back()->path != CompilePath::Advanced)
    {
        return Fail("advanced program did not use the advanced compilation path");
    }

    auto stats = cache.GetStats();
    if (stats.cacheHits < 2 || stats.cacheMisses != 4 || stats.programsCreated != 5 ||
        stats.compilationFailures != 1 || stats.identifierConflicts != 1 ||
        stats.recompilationAttempts != 5 || stats.recompilationSuccesses != 3 ||
        stats.recompilationFailures != 2 || stats.recompilationReuses != 2 ||
        stats.residentPrograms != 5 || stats.residentAssetIds < 7 ||
        stats.residentSourceBytes == 0)
    {
        return Fail("shader cache statistics are incorrect");
    }

    first.reset();
    contentAlias.reset();
    replacement.reset();
    beforeFailure.reset();
    alternateProgram.reset();
    computeProgram.reset();
    advancedProgram.reset();

    const u32 collected = cache.CollectUnused();
    if (collected != 5 || cache.GetResidentCount() != 0)
    {
        return Fail("unused shader programs were not collected");
    }

    return EXIT_SUCCESS;
}
