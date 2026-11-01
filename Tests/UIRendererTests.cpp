#include "TestGraphicsDevice.hpp"

#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>
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
        bool CompileCompute(const std::string&) override { return true; }
        void DispatchCompute(Pyramid::u32, Pyramid::u32, Pyramid::u32) override {}
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
    Tests::TestVertexArray worldVertexArray;
    device.BindFramebufferHandle(47);
    device.SetViewport(7, 11, 2048, 2048);
    device.EnableBlend(true);
    device.SetBlendFunc(GL_SRC_ALPHA, GL_ONE);
    device.EnableDepthTest(true);
    device.SetDepthFunc(GL_GREATER);
    device.EnableCullFace(true);
    device.SetCullFace(GL_FRONT);
    device.SetPolygonMode(GL_LINE);
    device.EnableScissorTest(true);
    device.SetScissorRect(3, 5, 7, 11);
    device.BindShader(compiledShader.get());
    device.BindVertexArray(&worldVertexArray);
    device.BindTexture(customTexture.get(), 0);
    u32 uiDraws = 0;
    bool uiDrawStateCorrect = true;
    device.onDraw = [&]()
    {
        ++uiDraws;
        const bool correctClip = uiDraws == 1
            ? device.scissorX == 0 && device.scissorY == 0 &&
                device.scissorWidth == 200 && device.scissorHeight == 160
            : device.scissorX == 20 && device.scissorY == 8 &&
                device.scissorWidth == 140 && device.scissorHeight == 100;
        uiDrawStateCorrect = uiDrawStateCorrect && correctClip &&
            device.boundFramebufferHandle == 0 && device.viewportX == 0 &&
            device.viewportY == 0 && device.viewportWidth == 200 && device.viewportHeight == 160 &&
            device.blendEnabled && device.blendSource == GL_SRC_ALPHA &&
            device.blendDestination == GL_ONE_MINUS_SRC_ALPHA && !device.depthTestEnabled &&
            !device.cullFaceEnabled && device.scissorEnabled && device.polygonMode == GL_FILL &&
            !device.wireframeEnabled && device.boundShader && device.boundVertexArray &&
            !device.boundTextures.empty() && device.boundTextures[0] && device.lastDrawIndexed;
    };
    if (!renderer.Render(drawList, frame))
    {
        return Fail("valid draw list was rejected");
    }

    device.onDraw = {};
    if (uiDraws != 2 || !uiDrawStateCorrect)
    {
        return Fail("UI draws inherited off-screen/material state or lost their active clips");
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
        device.boundShader != nullptr || device.boundVertexArray != nullptr ||
        device.polygonMode != GL_FILL || device.boundTextures.empty() || device.boundTextures[0])
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

    ShaderProgramSpecification worldShaderSpec;
    worldShaderSpec.vertexSource = "#version 330 core\nvoid main() { gl_Position = vec4(0.0); }";
    worldShaderSpec.fragmentSource = "#version 330 core\nout vec4 FragColor; void main() { FragColor = vec4(1.0); }";
    const auto worldShader = resources.Shaders().GetOrCreate(worldShaderSpec);
    MaterialSpecification worldMaterialSpec;
    worldMaterialSpec.shader = worldShader;
    worldMaterialSpec.renderState.blendMode = MaterialBlendMode::Additive;
    worldMaterialSpec.renderState.depthTest = true;
    worldMaterialSpec.renderState.depthCompare = MaterialDepthCompare::GreaterEqual;
    worldMaterialSpec.renderState.cullMode = MaterialCullMode::Front;
    worldMaterialSpec.renderState.polygonMode = MaterialPolygonMode::Point;
    const auto worldMaterial = resources.Materials().GetOrCreate(worldMaterialSpec);
    if (!worldMaterial) return Fail("following-world material fixture failed creation");
    worldMaterial->Apply(device);
    device.BindVertexArray(&worldVertexArray);
    u32 worldDraws = 0;
    bool worldDrawStateCorrect = false;
    device.onDraw = [&]()
    {
        ++worldDraws;
        worldDrawStateCorrect = !device.scissorEnabled && device.blendEnabled &&
            device.blendSource == GL_SRC_ALPHA && device.blendDestination == GL_ONE &&
            device.depthTestEnabled && device.depthFunction == GL_GEQUAL &&
            device.cullFaceEnabled && device.cullFaceMode == GL_FRONT &&
            device.polygonMode == GL_POINT && !device.wireframeEnabled &&
            device.boundShader == worldShader.get() && device.boundVertexArray == &worldVertexArray;
    };
    device.DrawArrays(3, 0, PrimitiveTopology::Triangles);
    device.onDraw = {};
    if (worldDraws != 1 || !worldDrawStateCorrect)
    {
        return Fail("a following material draw inherited UI clipping or fixed state");
    }

    device.BindFramebufferHandle(91);
    device.SetViewport(17, 19, 256, 128);
    const u32 drawsBeforeNoWork = device.drawCalls;
    const u32 viewportsBeforeNoWork = device.viewportChanges;
    const u32 bindsBeforeNoWork = device.framebufferBindCalls;
    const auto WorldStateUnchanged = [&]()
    {
        return device.drawCalls == drawsBeforeNoWork && device.viewportChanges == viewportsBeforeNoWork &&
            device.framebufferBindCalls == bindsBeforeNoWork && device.boundFramebufferHandle == 91 &&
            device.viewportX == 17 && device.viewportY == 19 && device.viewportWidth == 256 &&
            device.viewportHeight == 128 && device.blendEnabled && device.polygonMode == GL_POINT &&
            device.depthTestEnabled && device.cullFaceEnabled && !device.scissorEnabled &&
            device.boundShader == worldShader.get() && device.boundVertexArray == &worldVertexArray;
    };
    UI::DrawList emptyList;
    UI::FrameInfo invalidFrame = frame;
    invalidFrame.width = 0;
    if (!renderer.Render(emptyList, frame) || !WorldStateUnchanged() ||
        renderer.Render(drawList, invalidFrame) || !WorldStateUnchanged())
    {
        return Fail("empty or rejected UI work changed the current world render state");
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
    if (device.drawCalls != drawsBeforeNoWork || device.scissorEnabled || device.blendEnabled ||
        !device.depthTestEnabled || !device.cullFaceEnabled || device.polygonMode != GL_FILL ||
        device.boundShader || device.boundVertexArray || device.boundTextures[0] ||
        device.boundFramebufferHandle != 0 || device.viewportWidth != 200 || device.viewportHeight != 160)
    {
        return Fail("all-skipped UI batches did not clean their temporary surface state");
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
