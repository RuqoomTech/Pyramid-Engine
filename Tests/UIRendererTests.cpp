#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/UI/UIRenderer.hpp>
#include <Pyramid/Text/Text.hpp>
#include <Pyramid/UI/UI.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "UIRenderer test failed: " << message << '\n';
        return EXIT_FAILURE;
    }

    class TestShader final : public Pyramid::IShader
    {
    public:
        void Bind() override { bound = true; }
        void Unbind() override { bound = false; }
        bool Compile(const std::string& vertex, const std::string& fragment) override
        {
            vertexSource = vertex;
            fragmentSource = fragment;
            return true;
        }
        bool CompileWithGeometry(
            const std::string&, const std::string&, const std::string&) override { return true; }
        bool CompileWithTessellation(
            const std::string&, const std::string&, const std::string&,
            const std::string&) override { return true; }
        bool CompileAdvanced(
            const std::string&, const std::string&, const std::string&,
            const std::string&, const std::string&) override { return true; }
        void SetUniformInt(const std::string&, int) override {}
        void SetUniformFloat(const std::string&, float) override {}
        void SetUniformFloat2(const std::string&, float, float) override {}
        void SetUniformFloat3(const std::string&, float, float, float) override {}
        void SetUniformFloat4(const std::string&, float, float, float, float) override {}
        void SetUniformMat3(const std::string&, const float*, bool, int) override {}
        void SetUniformMat4(const std::string&, const float*, bool, int) override {}
        void BindUniformBuffer(
            const std::string&, Pyramid::IUniformBuffer*, Pyramid::u32) override {}
        void SetUniformBlockBinding(const std::string&, Pyramid::u32) override {}
        void BindShaderStorageBuffer(
            const std::string&, Pyramid::IShaderStorageBuffer*, Pyramid::u32) override {}
        void SetShaderStorageBlockBinding(const std::string&, Pyramid::u32) override {}

        bool bound = false;
        std::string vertexSource;
        std::string fragmentSource;
    };

    class TestTexture final : public Pyramid::ITexture2D
    {
    public:
        explicit TestTexture(Pyramid::TextureSpecification specification)
            : m_specification(specification)
        {
        }

        void Bind(Pyramid::u32 slot = 0) const override { lastSlot = slot; }
        void Unbind(Pyramid::u32 = 0) const override {}
        Pyramid::u32 GetWidth() const override { return m_specification.Width; }
        Pyramid::u32 GetHeight() const override { return m_specification.Height; }
        Pyramid::u32 GetRendererID() const override { return 7; }
        Pyramid::TextureFormat GetFormat() const override { return m_specification.Format; }
        const std::string& GetPath() const override { return m_path; }
        bool IsLoaded() const override { return true; }
        Pyramid::u32 GetMipLevels() const override { return 1; }

        mutable Pyramid::u32 lastSlot = 0;

    private:
        Pyramid::TextureSpecification m_specification;
        std::string m_path;
    };
}

int main()
{
    using namespace Pyramid;

    {
        Tests::TestGraphicsDevice failingDevice;
        failingDevice.shaderFactory = []() { return std::make_shared<TestShader>(); };
        failingDevice.textureFactory = [](
            const TextureSpecification& specification,
            const void*)
        {
            return std::make_shared<TestTexture>(specification);
        };
        ResourceRegistry failingResources(failingDevice);

        ShaderProgramSpecification unrelatedShaderSpecification;
        unrelatedShaderSpecification.vertexSource = "unrelated-vertex";
        unrelatedShaderSpecification.fragmentSource = "unrelated-fragment";
        unrelatedShaderSpecification.assetId =
            ShaderAssetId::FromString("tests/ui/unrelated-shader");
        auto unrelatedShader =
            failingResources.Shaders().GetOrCreate(unrelatedShaderSpecification);

        const u8 unrelatedPixels[4] = {12, 34, 56, 255};
        TextureResourceSpecification unrelatedTextureSpecification;
        unrelatedTextureSpecification.texture.Width = 1;
        unrelatedTextureSpecification.texture.Height = 1;
        unrelatedTextureSpecification.texture.Format = TextureFormat::RGBA8;
        unrelatedTextureSpecification.texture.GenerateMips = false;
        unrelatedTextureSpecification.pixelData = unrelatedPixels;
        unrelatedTextureSpecification.pixelDataSize = sizeof(unrelatedPixels);
        unrelatedTextureSpecification.assetId =
            TextureAssetId::FromString("tests/ui/unrelated-texture");
        auto unrelatedTexture =
            failingResources.Textures().GetOrCreate(unrelatedTextureSpecification);
        if (!unrelatedShader || !unrelatedTexture)
        {
            return Fail("unrelated cache setup failed");
        }
        unrelatedShader.reset();
        unrelatedTexture.reset();

        failingDevice.failVertexBufferCreationAt = 1;
        UIRenderer failingRenderer;
        const Text::FontAtlas failingFont = Text::CreateDebugFontAtlas();
        if (failingRenderer.Initialize(failingDevice, failingResources, failingFont))
        {
            return Fail("partial buffer allocation was accepted");
        }
        const ResourceRegistryStats stats = failingResources.GetStats();
        if (stats.shaders.residentPrograms != 1 || stats.textures.residentTextures != 1 ||
            !failingResources.Shaders().Contains(unrelatedShaderSpecification.assetId) ||
            !failingResources.Textures().Contains(unrelatedTextureSpecification.assetId) ||
            failingResources.Shaders().Contains(
                ShaderAssetId::FromString("pyramid/ui/default-shader")) ||
            failingResources.Textures().Contains(
                TextureAssetId::FromString("pyramid/ui/debug-font-atlas")))
        {
            return Fail("failed initialization did not preserve unrelated cache-only resources");
        }
    }

    Tests::TestGraphicsDevice device;
    std::shared_ptr<TestShader> compiledShader;
    device.shaderFactory = [&]()
    {
        compiledShader = std::make_shared<TestShader>();
        return compiledShader;
    };
    TextureSpecification createdFontTextureSpecification;
    bool createdFontTexture = false;
    device.textureFactory = [&](const TextureSpecification& specification, const void*)
    {
        createdFontTextureSpecification = specification;
        createdFontTexture = true;
        return std::make_shared<TestTexture>(specification);
    };

    ResourceRegistry resources(device);
    UIRenderer renderer;
    Text::FontAtlas font;
    std::string fontError;
    if (!Text::LoadFontAtlas(PYRAMID_UI_TEST_FONT, font, &fontError))
    {
        return Fail("processed font atlas failed to load");
    }
    if (font.width != 1024 || font.height != 1024 || font.glyphs.size() != 98 ||
        font.rasterMode != Font::RasterMode::SignedDistanceField)
    {
        return Fail("processed font atlas metadata mismatch");
    }
    if (!renderer.Initialize(device, resources, font) || !renderer.IsInitialized())
    {
        return Fail("initialization failed");
    }
    if (!createdFontTexture ||
        createdFontTextureSpecification.MinFilter != TextureFilter::Linear ||
        createdFontTextureSpecification.MagFilter != TextureFilter::Linear)
    {
        return Fail("font atlas did not use antialiased linear sampling");
    }
    if (!compiledShader ||
        compiledShader->vertexSource.find("a_TextParameters") == std::string::npos ||
        compiledShader->fragmentSource.find("fwidth") == std::string::npos ||
        compiledShader->fragmentSource.find("smoothstep") == std::string::npos)
    {
        return Fail("UI shader did not compile the scalable SDF text path");
    }

    auto customTexture = std::make_shared<TestTexture>(TextureSpecification{});
    auto conflictingTexture = std::make_shared<TestTexture>(TextureSpecification{});
    if (!renderer.RegisterTexture(23, customTexture) ||
        !renderer.RegisterTexture(23, customTexture) ||
        renderer.RegisterTexture(23, conflictingTexture) ||
        renderer.RegisterTexture(0, customTexture) ||
        renderer.RegisterTexture(UI::DebugFontTextureId, customTexture))
    {
        return Fail("texture registration rules failed");
    }

    UI::DrawList drawList;
    drawList.AddQuad(
        {5.0f, 6.0f, 20.0f, 20.0f},
        {font.whitePixelUv.x, font.whitePixelUv.y, 0.0f, 0.0f},
        Color::White,
        UI::DebugFontTextureId,
        {0.0f, 0.0f, 100.0f, 80.0f});
    drawList.AddQuad(
        {30.0f, 6.0f, 20.0f, 20.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        Color::White,
        23,
        {10.25f, 4.25f, 69.5f, 49.5f});

    UI::FrameInfo frame{100.0f, 80.0f, 2.0f, 1.0f / 60.0f};
    if (!renderer.Render(drawList, frame))
    {
        return Fail("valid draw list was rejected");
    }

    const UIRendererStats& stats = renderer.GetStats();
    if (stats.drawCalls != 2 || stats.batchesSubmitted != 2 ||
        stats.batchesSkipped != 0 || stats.verticesUploaded != 8 ||
        stats.indicesUploaded != 12 || device.drawCalls != 2)
    {
        return Fail("render statistics mismatch");
    }
    if (device.scissorEnabled || device.blendEnabled || !device.depthTestEnabled ||
        !device.cullFaceEnabled || device.wireframeEnabled ||
        device.boundShader != nullptr || device.boundVertexArray != nullptr)
    {
        return Fail("render-state baseline was not restored");
    }
    if (device.viewportX != 0 || device.viewportY != 0 ||
        device.viewportWidth != 200 || device.viewportHeight != 160 ||
        device.viewportChanges == 0 || device.boundFramebufferHandle != 0)
    {
        return Fail("UI renderer did not establish the physical surface viewport");
    }
    if (device.scissorX != 20 || device.scissorY != 8 ||
        device.scissorWidth != 140 || device.scissorHeight != 100)
    {
        return Fail("logical clip rectangle was not DPI-scaled");
    }

    UI::DrawList missingTextureList;
    missingTextureList.AddQuad(
        {0.0f, 0.0f, 10.0f, 10.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        Color::White,
        999,
        {0.0f, 0.0f, 20.0f, 20.0f});
    if (renderer.Render(missingTextureList, frame) ||
        renderer.GetStats().batchesSkipped != 1)
    {
        return Fail("missing texture batch was not rejected deterministically");
    }

    renderer.UnregisterTexture(23);
    if (renderer.RegisterTexture(23, nullptr))
    {
        return Fail("null texture registration was accepted");
    }

    renderer.Shutdown();
    if (renderer.IsInitialized() || renderer.Render(drawList, frame))
    {
        return Fail("shutdown renderer accepted work");
    }

    std::cout << "UIRenderer tests passed\n";
    return EXIT_SUCCESS;
}
