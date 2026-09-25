#include <Pyramid/Graphics/UI/UIRenderer.hpp>

#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/Buffer/IndexBuffer.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <Pyramid/Graphics/Buffer/VertexBuffer.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>
#include <Pyramid/Graphics/Texture/TextureResource.hpp>
#include <Pyramid/Util/Log.hpp>

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Pyramid
{
    namespace
    {
        constexpr const char* kUIVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_UV;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in vec2 a_TextParameters;

uniform vec2 u_SurfaceSize;

out vec2 v_UV;
out vec4 v_Color;
out vec2 v_TextParameters;

void main()
{
    vec2 normalized = a_Position / u_SurfaceSize;
    vec2 clip = vec2(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
    v_UV = a_UV;
    v_Color = a_Color;
    v_TextParameters = a_TextParameters;
}
)";

        constexpr const char* kUIFragmentShader = R"(
#version 330 core
in vec2 v_UV;
in vec4 v_Color;
in vec2 v_TextParameters;

uniform sampler2D u_Texture;

out vec4 FragColor;

void main()
{
    vec4 texel = texture(u_Texture, v_UV);
    if (v_TextParameters.x > 0.5)
    {
        float signedDistance = texel.a + v_TextParameters.y;
        float smoothing = max(fwidth(signedDistance), 1.0 / 255.0);
        float coverage = smoothstep(0.5 - smoothing, 0.5 + smoothing, signedDistance);
        FragColor = vec4(v_Color.rgb, v_Color.a * coverage);
    }
    else
    {
        FragColor = texel * v_Color;
    }
}
)";

        bool FitsU32(std::size_t value)
        {
            return value <= static_cast<std::size_t>(std::numeric_limits<u32>::max());
        }

        u32 FloorPixel(f32 logical, f32 scale)
        {
            const f64 value = (std::max)(0.0, std::floor(static_cast<f64>(logical) * scale));
            return static_cast<u32>(
                (std::min)(value, static_cast<f64>(std::numeric_limits<u32>::max())));
        }

        u32 CeilPixel(f32 logical, f32 scale)
        {
            const f64 value = (std::max)(0.0, std::ceil(static_cast<f64>(logical) * scale));
            return static_cast<u32>(
                (std::min)(value, static_cast<f64>(std::numeric_limits<u32>::max())));
        }
    } // namespace

    UIRenderer::~UIRenderer()
    {
        Shutdown();
    }

    bool UIRenderer::Initialize(
        IGraphicsDevice& device,
        ResourceRegistry& resources,
        const Text::FontAtlas& debugFont)
    {
        Shutdown();
        if (!debugFont.IsValid() || !device.IsValid())
        {
            PYRAMID_LOG_ERROR("UI renderer initialization failed: invalid device or debug font");
            return false;
        }

        ShaderProgramSpecification shaderSpecification;
        shaderSpecification.vertexSource = kUIVertexShader;
        shaderSpecification.fragmentSource = kUIFragmentShader;
        shaderSpecification.name = "Pyramid UI";
        shaderSpecification.assetId = ShaderAssetId::FromString("pyramid/ui/default-shader");
        const ShaderAssetId shaderContentId =
            ShaderProgram::CalculateContentId(shaderSpecification);
        const bool hadShaderAlias = resources.Shaders().Contains(shaderSpecification.assetId);
        const bool hadShaderContent = resources.Shaders().Contains(shaderContentId);
        auto shader = resources.Shaders().GetOrCreate(shaderSpecification);
        if (!shader)
        {
            PYRAMID_LOG_ERROR("UI renderer initialization failed: shader creation failed");
            return false;
        }

        TextureResourceSpecification textureSpecification;
        textureSpecification.texture.Width = debugFont.width;
        textureSpecification.texture.Height = debugFont.height;
        textureSpecification.texture.Format = TextureFormat::RGBA8;
        // Processed font atlases contain grayscale coverage. Linear sampling preserves
        // that antialiasing when a high-resolution atlas is scaled to UI text size.
        textureSpecification.texture.MinFilter = TextureFilter::Linear;
        textureSpecification.texture.MagFilter = TextureFilter::Linear;
        textureSpecification.texture.WrapS = TextureWrap::ClampToEdge;
        textureSpecification.texture.WrapT = TextureWrap::ClampToEdge;
        textureSpecification.texture.GenerateMips = false;
        textureSpecification.pixelData = debugFont.rgbaPixels.data();
        textureSpecification.pixelDataSize = debugFont.rgbaPixels.size();
        textureSpecification.colorSpace = TextureColorSpace::Linear;
        textureSpecification.assetId =
            TextureAssetId::FromString("pyramid/ui/debug-font-atlas");
        textureSpecification.name = "Pyramid UI Debug Font";
        const TextureAssetId fontContentId =
            TextureResource::CalculateContentId(textureSpecification);
        const bool hadFontAlias = resources.Textures().Contains(textureSpecification.assetId);
        const bool hadFontContent = resources.Textures().Contains(fontContentId);
        auto fontTexture = resources.Textures().GetOrCreate(textureSpecification);
        if (!fontTexture)
        {
            shader.reset();
            if (!hadShaderAlias)
            {
                (void)resources.Shaders().RemoveAlias(shaderSpecification.assetId);
            }
            if (!hadShaderContent)
            {
                (void)resources.Shaders().Evict(shaderContentId);
            }
            PYRAMID_LOG_ERROR("UI renderer initialization failed: font texture creation failed");
            return false;
        }

        auto vertexBuffer = device.CreateVertexBuffer();
        auto indexBuffer = device.CreateIndexBuffer();
        auto vertexArray = device.CreateVertexArray();
        if (!vertexBuffer || !indexBuffer || !vertexArray)
        {
            fontTexture.reset();
            shader.reset();
            if (!hadFontAlias)
            {
                (void)resources.Textures().RemoveAlias(textureSpecification.assetId);
            }
            if (!hadFontContent)
            {
                (void)resources.Textures().Evict(fontContentId);
            }
            if (!hadShaderAlias)
            {
                (void)resources.Shaders().RemoveAlias(shaderSpecification.assetId);
            }
            if (!hadShaderContent)
            {
                (void)resources.Shaders().Evict(shaderContentId);
            }
            PYRAMID_LOG_ERROR("UI renderer initialization failed: buffer allocation failed");
            return false;
        }

        const BufferLayout layout = {
            {ShaderDataType::Float2, "a_Position"},
            {ShaderDataType::Float2, "a_UV"},
            {ShaderDataType::Float4, "a_Color"},
            {ShaderDataType::Float2, "a_TextParameters"},
        };
        vertexArray->AddVertexBuffer(vertexBuffer, layout);
        vertexArray->SetIndexBuffer(indexBuffer);

        m_device = &device;
        m_shader = std::move(shader);
        m_fontTexture = std::move(fontTexture);
        m_vertexBuffer = std::move(vertexBuffer);
        m_indexBuffer = std::move(indexBuffer);
        m_vertexArray = std::move(vertexArray);
        m_textures.clear();
        m_stats = {};
        return true;
    }

    void UIRenderer::Shutdown()
    {
        m_textures.clear();
        m_vertexArray.reset();
        m_indexBuffer.reset();
        m_vertexBuffer.reset();
        m_fontTexture.reset();
        m_shader.reset();
        m_device = nullptr;
        m_stats = {};
    }

    bool UIRenderer::RegisterTexture(
        UI::TextureId id,
        const std::shared_ptr<ITexture2D>& texture)
    {
        if (id == 0 || id == UI::DebugFontTextureId || !texture || !texture->IsLoaded())
        {
            return false;
        }
        const auto found = m_textures.find(id);
        if (found != m_textures.end())
        {
            const std::shared_ptr<ITexture2D> existing = found->second.lock();
            if (existing)
            {
                return existing == texture;
            }
        }
        m_textures[id] = texture;
        return true;
    }

    void UIRenderer::UnregisterTexture(UI::TextureId id)
    {
        m_textures.erase(id);
    }

    bool UIRenderer::Render(const UI::DrawList& drawList, const UI::FrameInfo& frame)
    {
        m_stats = {};
        if (!IsInitialized() || !frame.IsValid())
        {
            return false;
        }
        if (drawList.Empty())
        {
            return true;
        }

        const auto& vertices = drawList.GetVertices();
        const auto& indices = drawList.GetIndices();
        if (vertices.size() >
                static_cast<std::size_t>(std::numeric_limits<u32>::max()) /
                    sizeof(UI::Vertex) ||
            !FitsU32(indices.size()))
        {
            PYRAMID_LOG_ERROR("UI renderer rejected draw data that exceeds 32-bit buffer limits");
            return false;
        }

        m_vertexBuffer->SetData(
            vertices.data(),
            static_cast<u32>(vertices.size() * sizeof(UI::Vertex)));

        const u32 surfaceWidth = CeilPixel(frame.width, frame.dpiScale);
        const u32 surfaceHeight = CeilPixel(frame.height, frame.dpiScale);
        if (surfaceWidth == 0 || surfaceHeight == 0)
        {
            PYRAMID_LOG_ERROR("UI renderer rejected a zero-sized physical surface");
            return false;
        }

        // UI is a final surface-space pass. Establish its physical viewport
        // explicitly because earlier passes may have rendered shadow maps or
        // other off-screen targets with different extents.
        m_device->BindFramebuffer(nullptr);
        m_device->SetViewport(0, 0, surfaceWidth, surfaceHeight);
        m_device->EnableBlend(true);
        m_device->SetBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        m_device->EnableDepthTest(false);
        m_device->EnableCullFace(false);
        m_device->SetWireframeMode(false);
        m_device->EnableScissorTest(true);
        m_device->BindShader(m_shader.get());
        m_shader->SetUniformFloat2("u_SurfaceSize", frame.width, frame.height);
        m_shader->SetUniformInt("u_Texture", 0);
        m_device->BindVertexArray(m_vertexArray.get());

        bool renderedAny = false;
        for (const UI::DrawBatch& batch : drawList.GetBatches())
        {
            if (batch.indexCount == 0 ||
                static_cast<std::size_t>(batch.indexOffset) + batch.indexCount > indices.size())
            {
                ++m_stats.batchesSkipped;
                continue;
            }

            std::shared_ptr<ITexture2D> texture = ResolveTexture(batch.texture);
            if (!texture)
            {
                ++m_stats.batchesSkipped;
                continue;
            }

            const u32 clipX = FloorPixel(batch.clip.x, frame.dpiScale);
            const u32 clipY = FloorPixel(batch.clip.y, frame.dpiScale);
            const u32 clipRight = CeilPixel(
                batch.clip.x + batch.clip.width,
                frame.dpiScale);
            const u32 clipBottom = CeilPixel(
                batch.clip.y + batch.clip.height,
                frame.dpiScale);
            const u32 clipWidth = clipRight > clipX ? clipRight - clipX : 0;
            const u32 clipHeight = clipBottom > clipY ? clipBottom - clipY : 0;
            if (clipWidth == 0 || clipHeight == 0)
            {
                ++m_stats.batchesSkipped;
                continue;
            }

            m_device->SetScissorRect(clipX, clipY, clipWidth, clipHeight);
            m_indexBuffer->SetData(
                indices.data() + batch.indexOffset,
                batch.indexCount);
            m_device->BindTexture(texture.get(), 0);
            m_device->DrawIndexed(batch.indexCount, PrimitiveTopology::Triangles);
            ++m_stats.drawCalls;
            ++m_stats.batchesSubmitted;
            renderedAny = true;
        }

        m_stats.verticesUploaded = static_cast<u32>(vertices.size());
        m_stats.indicesUploaded = static_cast<u32>(indices.size());
        RestoreBaselineState();
        return renderedAny || m_stats.batchesSkipped == 0;
    }

    std::shared_ptr<ITexture2D> UIRenderer::ResolveTexture(UI::TextureId id) const
    {
        if (id == UI::DebugFontTextureId)
        {
            return m_fontTexture ? m_fontTexture->GetTexture() : nullptr;
        }
        const auto found = m_textures.find(id);
        return found == m_textures.end() ? nullptr : found->second.lock();
    }

    void UIRenderer::RestoreBaselineState()
    {
        if (!m_device)
        {
            return;
        }
        m_device->EnableScissorTest(false);
        m_device->BindTexture(nullptr, 0);
        m_device->BindVertexArray(nullptr);
        m_device->BindShader(nullptr);
        m_device->EnableBlend(false);
        m_device->EnableDepthTest(true);
        m_device->EnableCullFace(true);
        m_device->SetWireframeMode(false);
    }
} // namespace Pyramid
