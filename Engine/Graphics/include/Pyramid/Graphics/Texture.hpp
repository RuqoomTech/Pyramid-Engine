#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <string>
#include <memory> // For std::shared_ptr

namespace Pyramid {

enum class TextureFormat
{
    None = 0,
    // Standard 8-bit formats
    RGB8,
    RGBA8,
    SRGB8,
    SRGBA8,
    // High dynamic range formats
    RGB16F,
    RGBA16F,
    RGB32F,
    RGBA32F,
    // Depth/stencil formats
    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8,
    Depth32FStencil8,
    // Compressed formats
    BC1_RGB,     // DXT1
    BC1_RGBA,    // DXT1 with alpha
    BC3_RGBA,    // DXT5
    BC7_RGBA,    // High quality
    // Single channel formats
    R8,
    R16F,
    R32F
};

enum class TextureFilter
{
    None = 0,
    Linear,
    Nearest,
    LinearMipmapLinear,
    LinearMipmapNearest,
    NearestMipmapLinear,
    NearestMipmapNearest
};

enum class TextureWrap
{
    None = 0,
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

struct TextureSpecification
{
    u32 Width = 1;
    u32 Height = 1;
    TextureFormat Format = TextureFormat::RGBA8;
    TextureFilter MinFilter = TextureFilter::LinearMipmapLinear;
    TextureFilter MagFilter = TextureFilter::Linear;
    TextureWrap WrapS = TextureWrap::Repeat;
    TextureWrap WrapT = TextureWrap::Repeat;
    bool GenerateMips = true;
    bool SRGB = false;
    f32 BorderColor[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // For ClampToBorder
    f32 MaxAnisotropy = 1.0f; // Anisotropic filtering level
    bool FlipY = false; // For texture coordinate system differences
};

class ITexture
{
public:
    virtual ~ITexture() = default;

    virtual void Bind(u32 slot = 0) const = 0;
    virtual void Unbind(u32 slot = 0) const = 0;

    virtual u32 GetWidth() const = 0;
    virtual u32 GetHeight() const = 0;
    virtual u32 GetRendererID() const = 0;
    virtual TextureFormat GetFormat() const { return TextureFormat::RGBA8; }

    virtual const std::string& GetPath() const = 0;
    virtual bool IsLoaded() const { return true; }
    virtual std::string GetLastError() const { return ""; }

    // Dynamic texture updates (optional - can have default implementations)
    virtual void SetData(const void*, u32) {}
    virtual void SetSubData(const void*, u32, u32, u32, u32) {}
    
    // Texture parameters (optional - can have default implementations)
    virtual void SetFilter(TextureFilter, TextureFilter) {}
    virtual void SetWrap(TextureWrap, TextureWrap) {}
    virtual void SetBorderColor(f32, f32, f32, f32) {}
    virtual void SetMaxAnisotropy(f32) {}
};

class ITexture2D : public ITexture
{
public:
    // Factory methods
    static std::shared_ptr<ITexture2D> Create(const TextureSpecification& specification, const void* data = nullptr);
    static std::shared_ptr<ITexture2D> Create(const std::string& filepath, bool srgb = false, bool generateMips = true);
    static std::shared_ptr<ITexture2D> Create(u32 width, u32 height, TextureFormat format = TextureFormat::RGBA8);
    
    // Utility methods for common formats
    static std::shared_ptr<ITexture2D> CreateRenderTarget(u32 width, u32 height, TextureFormat format = TextureFormat::RGBA8);
    static std::shared_ptr<ITexture2D> CreateDepthTarget(u32 width, u32 height, TextureFormat format = TextureFormat::Depth24Stencil8);
    static std::shared_ptr<ITexture2D> CreateFromColor(u32 width, u32 height, const Color& color);
    
    // Mipmap management (optional implementations)
    virtual void GenerateMipmaps() {}
    virtual u32 GetMipLevels() const { return 1; }
    
    // Texture streaming and loading (optional implementations)
    virtual bool LoadFromFile(const std::string&, bool = false, bool = true) { return false; }
    virtual bool SaveToFile(const std::string&) const { return false; }
};

} // namespace Pyramid
