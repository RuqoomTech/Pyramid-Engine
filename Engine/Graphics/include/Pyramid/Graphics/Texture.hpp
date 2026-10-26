#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <string>
#include <memory> // For std::shared_ptr

namespace Pyramid {

enum class TextureFormat
{
    None = 0,
    // Values participate in resource content identity; keep existing codes stable.
    // RGB/RGBA component order, tightly packed unsigned-byte input.
    RGB8 = 1,
    RGBA8 = 2,
    SRGB8 = 3,
    SRGBA8 = 4,
    // Tightly packed IEEE binary16 bit patterns (u16) or binary32 (f32) input.
    RGB16F = 5,
    RGBA16F = 6,
    RGB32F = 7,
    RGBA32F = 8,
    // Depth16: normalized u16; Depth24: normalized u32; Depth32F: f32.
    Depth16 = 9,
    Depth24 = 10,
    Depth32F = 11,
    // Packed u32: high 24 bits depth, low 8 bits stencil.
    Depth24Stencil8 = 12,
    // Per pixel: f32 depth followed by u32 (low 8 bits stencil, high 24 ignored).
    Depth32FStencil8 = 13,
    // Reserved, explicitly unsupported by the current 2D texture upload API.
    BC1_RGB = 14,
    BC1_RGBA = 15,
    BC3_RGBA = 16,
    BC7_RGBA = 17,
    // Red-channel sampling; the engine does not add grayscale swizzling.
    R8 = 18,
    R16F = 19,
    R32F = 20
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
    bool GenerateMips = true; // Allocate the full chain, also for null initial data.
    bool SRGB = false; // Direct textures: 8-bit RGB/RGBA family only. Caches use their colorSpace field.
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

    // Whole-level updates use the TextureFormat encoding and require an exact byte count.
    // Other dynamic operations remain optional.
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
    // Creation failures return an unloaded object with GetLastError(). Data is
    // tightly packed in the TextureFormat encoding; raw pointers carry no length.
    // The caller must supply Width * Height complete pixels when data is non-null.
    static std::shared_ptr<ITexture2D> Create(const TextureSpecification& specification, const void* data = nullptr);
    static std::shared_ptr<ITexture2D> Create(const std::string& filepath, bool srgb = false, bool generateMips = true);
    static std::shared_ptr<ITexture2D> Create(u32 width, u32 height, TextureFormat format = TextureFormat::RGBA8);
    
    // Allocate storage only; renderability is validated when attached to a framebuffer.
    static std::shared_ptr<ITexture2D> CreateRenderTarget(u32 width, u32 height, TextureFormat format = TextureFormat::RGBA8);
    // Depth/depth-stencil only, single level with nearest filtering; invalid formats return nullptr.
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
