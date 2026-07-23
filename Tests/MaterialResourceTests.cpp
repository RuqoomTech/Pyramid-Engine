#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>
#include <Pyramid/Graphics/Texture/TextureResource.hpp>
#include <glad/glad.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "FAIL: " << message << '\n';
        return EXIT_FAILURE;
    }

    class RecordingShader final : public Pyramid::IShader
    {
    public:
        void Bind() override { ++bindCount; }
        void Unbind() override {}
        bool Compile(const std::string&, const std::string&) override { return true; }
        bool CompileWithGeometry(const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileWithTessellation(const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileAdvanced(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileCompute(const std::string&) override { return true; }
        void DispatchCompute(Pyramid::u32, Pyramid::u32, Pyramid::u32) override {}

        void SetUniformInt(const std::string& name, int value) override
        {
            ints[name] = value;
            order.push_back(name);
        }
        void SetUniformFloat(const std::string& name, float value) override
        {
            floats[name] = value;
            order.push_back(name);
        }
        void SetUniformFloat2(const std::string& name, float x, float y) override
        {
            vectors[name] = {x, y, 0.0f, 0.0f};
            order.push_back(name);
        }
        void SetUniformFloat3(const std::string& name, float x, float y, float z) override
        {
            vectors[name] = {x, y, z, 0.0f};
            order.push_back(name);
        }
        void SetUniformFloat4(const std::string& name, float x, float y, float z, float w) override
        {
            vectors[name] = {x, y, z, w};
            order.push_back(name);
        }
        void SetUniformMat3(const std::string& name, const float*, bool, int) override
        {
            order.push_back(name);
        }
        void SetUniformMat4(const std::string& name, const float*, bool, int) override
        {
            order.push_back(name);
        }
        void BindUniformBuffer(const std::string&, Pyramid::IUniformBuffer*, Pyramid::u32) override {}
        void SetUniformBlockBinding(const std::string&, Pyramid::u32) override {}
        void BindShaderStorageBuffer(const std::string&, Pyramid::IShaderStorageBuffer*, Pyramid::u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, Pyramid::u32) override {}

        int bindCount = 0;
        std::unordered_map<std::string, int> ints;
        std::unordered_map<std::string, float> floats;
        std::unordered_map<std::string, std::array<float, 4>> vectors;
        std::vector<std::string> order;
    };

    class RecordingTexture final : public Pyramid::ITexture2D
    {
    public:
        explicit RecordingTexture(Pyramid::TextureSpecification specification)
            : m_specification(specification)
        {
        }
        void Bind(Pyramid::u32 slot = 0) const override { lastSlot = slot; ++bindCount; }
        void Unbind(Pyramid::u32 = 0) const override {}
        Pyramid::u32 GetWidth() const override { return m_specification.Width; }
        Pyramid::u32 GetHeight() const override { return m_specification.Height; }
        Pyramid::u32 GetRendererID() const override { return 1; }
        Pyramid::TextureFormat GetFormat() const override { return m_specification.Format; }
        const std::string& GetPath() const override { return path; }
        bool IsLoaded() const override { return true; }
        Pyramid::u32 GetMipLevels() const override { return 1; }

        mutable Pyramid::u32 lastSlot = 0;
        mutable int bindCount = 0;
        Pyramid::TextureSpecification m_specification;
        std::string path;
    };
}

int main()
{
    using namespace Pyramid;

    const MaterialAssetId stable = MaterialAssetId::FromString("materials/test");
    if (!stable.IsValid() || stable != MaterialAssetId::FromString("materials/test") ||
        stable == MaterialAssetId::FromString("materials/other") || stable.ToString().size() != 32)
    {
        return Fail("stable material identifiers are not deterministic");
    }

    Tests::TestGraphicsDevice device;
    std::shared_ptr<RecordingShader> recordingShader;
    device.shaderFactory = [&]()
    {
        recordingShader = std::make_shared<RecordingShader>();
        return recordingShader;
    };
    std::shared_ptr<RecordingTexture> recordingTexture;
    device.textureFactory = [&](const TextureSpecification& specification, const void*)
    {
        recordingTexture = std::make_shared<RecordingTexture>(specification);
        return recordingTexture;
    };

    ShaderProgramSpecification shaderSpecification;
    shaderSpecification.vertexSource = "void main(){}";
    shaderSpecification.fragmentSource = "void main(){}";
    shaderSpecification.assetId = ShaderAssetId::FromString("shaders/material-test");
    auto shader = ShaderProgram::Create(device, shaderSpecification);
    if (!shader || !recordingShader)
    {
        return Fail("could not create shader fixture");
    }

    const std::array<u8, 4> pixel = {255, 255, 255, 255};
    TextureResourceSpecification textureSpecification;
    textureSpecification.texture.Width = 1;
    textureSpecification.texture.Height = 1;
    textureSpecification.texture.Format = TextureFormat::RGBA8;
    textureSpecification.texture.GenerateMips = false;
    textureSpecification.pixelData = pixel.data();
    textureSpecification.pixelDataSize = pixel.size();
    textureSpecification.assetId = TextureAssetId::FromString("textures/material-test");
    auto texture = TextureResource::Create(device, textureSpecification);
    if (!texture || !recordingTexture)
    {
        return Fail("could not create texture fixture");
    }

    MaterialSpecification specification;
    specification.shader = shader;
    specification.textures = {{"u_AlbedoMap", 3, texture}};
    specification.uniforms = {
        {"u_Roughness", 0.4f},
        {"u_Tint", Math::Vec4(0.1f, 0.2f, 0.3f, 1.0f)},
        {"u_Mode", i32{7}}};
    specification.renderState.blendMode = MaterialBlendMode::Alpha;
    specification.renderState.depthTest = true;
    specification.renderState.depthCompare = MaterialDepthCompare::LessEqual;
    specification.renderState.cullMode = MaterialCullMode::Front;
    specification.renderState.polygonMode = MaterialPolygonMode::Line;
    specification.assetId = stable;
    specification.name = "Test Material";

    const MaterialAssetId contentId = Material::CalculateContentId(specification);
    auto material = Material::Create(specification);
    if (!material || !material->IsValid() || material->GetAssetId() != stable ||
        material->GetContentId() != contentId || material->GetName() != "Test Material")
    {
        return Fail("valid material resource was not created");
    }

    MaterialSpecification reordered = specification;
    std::reverse(reordered.uniforms.begin(), reordered.uniforms.end());
    reordered.name = "Ignored Debug Name";
    reordered.assetId = {};
    if (Material::CalculateContentId(reordered) != contentId)
    {
        return Fail("material content identity depends on insertion order or debug name");
    }

    MaterialSpecification changed = specification;
    changed.uniforms[0].value = 0.8f;
    if (Material::CalculateContentId(changed) == contentId)
    {
        return Fail("uniform values are missing from material identity");
    }

    MaterialSpecification invalidSlot = specification;
    invalidSlot.textures[0].slot = 32;
    if (Material::Create(invalidSlot))
    {
        return Fail("out-of-range texture slots were accepted");
    }

    MaterialSpecification duplicateSlot = specification;
    duplicateSlot.textures.push_back({"u_NormalMap", 3, texture});
    if (Material::Create(duplicateSlot))
    {
        return Fail("duplicate texture slots were accepted");
    }

    MaterialSpecification duplicateName = specification;
    duplicateName.uniforms.push_back({"u_AlbedoMap", i32{0}});
    if (Material::Create(duplicateName))
    {
        return Fail("sampler/uniform name conflicts were accepted");
    }

    MaterialSpecification invalidUniform = specification;
    invalidUniform.uniforms.push_back({"u_Invalid", std::nanf("")});
    if (Material::Create(invalidUniform))
    {
        return Fail("non-finite uniform values were accepted");
    }

    Renderer::CommandBuffer commands;
    commands.Begin();
    commands.SetMaterial(material.get());
    commands.SetUniformFloat("u_Dynamic", 2.5f);
    commands.End();
    commands.Execute(&device);

    if (device.boundShader != shader.get() || !device.blendEnabled ||
        device.blendSource != GL_SRC_ALPHA || device.blendDestination != GL_ONE_MINUS_SRC_ALPHA ||
        !device.depthTestEnabled || device.depthFunction != GL_LEQUAL ||
        !device.cullFaceEnabled || device.cullFaceMode != GL_FRONT ||
        device.polygonMode != GL_LINE)
    {
        return Fail("material render state was not applied");
    }
    if (device.boundTextures.size() <= 3 || device.boundTextures[3] != texture.get() ||
        recordingTexture->lastSlot != 3 || recordingShader->ints["u_AlbedoMap"] != 3)
    {
        return Fail("material texture binding was not applied");
    }
    if (std::abs(recordingShader->floats["u_Roughness"] - 0.4f) > 0.0001f ||
        std::abs(recordingShader->floats["u_Dynamic"] - 2.5f) > 0.0001f ||
        recordingShader->ints["u_Mode"] != 7 ||
        std::abs(recordingShader->vectors["u_Tint"][2] - 0.3f) > 0.0001f)
    {
        return Fail("material or command-buffer uniforms were not applied");
    }

    std::cout << "Material resource tests passed\n";
    return EXIT_SUCCESS;
}
