#include <Pyramid/Graphics/OpenGL/OpenGLTexture.hpp>

#include "Fixtures/JPEGFixtures.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
    GLuint g_nextTexture = 10;
    GLenum g_lastInternalFormat = 0;
    GLenum g_lastDataFormat = 0;
    GLenum g_lastDataType = 0;
    GLenum g_lastSubImageDataType = 0;
    GLenum g_lastCompressedInternalFormat = 0;
    GLsizei g_lastCompressedImageSize = 0;
    GLenum g_lastCompressedSubFormat = 0;
    GLsizei g_lastCompressedSubImageSize = 0;
    int g_compressedImageCalls = 0;
    int g_compressedSubImageCalls = 0;
    GLint g_lastMinFilter = 0;
    int g_generateMipmapCalls = 0;
    int g_subImageCalls = 0;
    std::vector<GLint> g_unpackAlignments;
    std::vector<GLuint> g_deletedTextures;

    int Fail(const char* message)
    {
        std::cerr << "TextureLoadingTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    GLenum APIENTRY FakeGetError()
    {
        return GL_NO_ERROR;
    }

    void APIENTRY FakeGetIntegerv(GLenum name, GLint* value)
    {
        if (!value)
        {
            return;
        }
        *value = name == GL_UNPACK_ALIGNMENT ? 4 : 0;
    }

    void APIENTRY FakeGenTextures(GLsizei count, GLuint* textures)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            textures[index] = g_nextTexture++;
        }
    }

    void APIENTRY FakeDeleteTextures(GLsizei count, const GLuint* textures)
    {
        g_deletedTextures.insert(g_deletedTextures.end(), textures, textures + count);
    }

    void APIENTRY FakeBindTexture(GLenum, GLuint)
    {
    }

    void APIENTRY FakePixelStorei(GLenum name, GLint value)
    {
        if (name == GL_UNPACK_ALIGNMENT)
        {
            g_unpackAlignments.push_back(value);
        }
    }

    void APIENTRY FakeTexImage2D(
        GLenum,
        GLint,
        GLint internalFormat,
        GLsizei,
        GLsizei,
        GLint,
        GLenum dataFormat,
        GLenum dataType,
        const void*)
    {
        g_lastInternalFormat = static_cast<GLenum>(internalFormat);
        g_lastDataFormat = dataFormat;
        g_lastDataType = dataType;
    }

    void APIENTRY FakeTexSubImage2D(
        GLenum,
        GLint,
        GLint,
        GLint,
        GLsizei,
        GLsizei,
        GLenum,
        GLenum dataType,
        const void*)
    {
        ++g_subImageCalls;
        g_lastSubImageDataType = dataType;
    }

    void APIENTRY FakeCompressedTexImage2D(
        GLenum,
        GLint,
        GLenum internalFormat,
        GLsizei,
        GLsizei,
        GLint,
        GLsizei imageSize,
        const void*)
    {
        ++g_compressedImageCalls;
        g_lastCompressedInternalFormat = internalFormat;
        g_lastCompressedImageSize = imageSize;
    }

    void APIENTRY FakeCompressedTexSubImage2D(
        GLenum,
        GLint,
        GLint,
        GLint,
        GLsizei,
        GLsizei,
        GLenum format,
        GLsizei imageSize,
        const void*)
    {
        ++g_compressedSubImageCalls;
        g_lastCompressedSubFormat = format;
        g_lastCompressedSubImageSize = imageSize;
    }

    void APIENTRY FakeTexParameteri(GLenum, GLenum name, GLint value)
    {
        if (name == GL_TEXTURE_MIN_FILTER)
        {
            g_lastMinFilter = value;
        }
    }

    void APIENTRY FakeTexParameterfv(GLenum, GLenum, const GLfloat*)
    {
    }

    void APIENTRY FakeGenerateMipmap(GLenum)
    {
        ++g_generateMipmapCalls;
    }

    void APIENTRY FakeActiveTexture(GLenum)
    {
    }

    void InstallFakeOpenGL()
    {
        glad_glGetError = FakeGetError;
        glad_glGetIntegerv = FakeGetIntegerv;
        glad_glGenTextures = FakeGenTextures;
        glad_glDeleteTextures = FakeDeleteTextures;
        glad_glBindTexture = FakeBindTexture;
        glad_glPixelStorei = FakePixelStorei;
        glad_glTexImage2D = FakeTexImage2D;
        glad_glCompressedTexImage2D = FakeCompressedTexImage2D;
        glad_glTexSubImage2D = FakeTexSubImage2D;
        glad_glCompressedTexSubImage2D = FakeCompressedTexSubImage2D;
        glad_glTexParameteri = FakeTexParameteri;
        glad_glTexParameterfv = FakeTexParameterfv;
        glad_glGenerateMipmap = FakeGenerateMipmap;
        glad_glActiveTexture = FakeActiveTexture;
    }
}

int main()
{
    using Pyramid::OpenGLTexture2D;
    using Pyramid::TextureFormat;
    using Pyramid::Tests::Fixtures::BaselineRGBJPEG;
    using Pyramid::Tests::Fixtures::BaselineRGBJPEGSize;

    InstallFakeOpenGL();

    const std::string filepath = "pyramid_texture_fixture.jpg";
    {
        std::ofstream file(filepath, std::ios::binary);
        if (!file)
        {
            return Fail("could not create JPEG texture fixture");
        }
        file.write(
            reinterpret_cast<const char*>(BaselineRGBJPEG),
            static_cast<std::streamsize>(BaselineRGBJPEGSize));
    }

    GLuint textureId = 0;
    {
        OpenGLTexture2D texture(filepath, true, false);
        std::remove(filepath.c_str());

        if (!texture.IsLoaded() || texture.GetRendererID() == 0)
        {
            return Fail("valid JPEG texture did not load");
        }
        if (texture.GetWidth() != 8 || texture.GetHeight() != 8 ||
            texture.GetFormat() != TextureFormat::RGB8)
        {
            return Fail("loaded texture metadata is incorrect");
        }
        if (g_lastInternalFormat != GL_SRGB8 || g_lastDataFormat != GL_RGB)
        {
            return Fail("sRGB JPEG upload used incorrect OpenGL formats");
        }
        if (g_lastMinFilter != GL_LINEAR || g_generateMipmapCalls != 0)
        {
            return Fail("non-mipmapped texture used an incomplete minification state");
        }
        if (g_unpackAlignments.size() < 2 || g_unpackAlignments.front() != 1 ||
            g_unpackAlignments.back() != 4)
        {
            return Fail("texture upload did not set and restore unpack alignment");
        }

        textureId = texture.GetRendererID();
        if (texture.LoadFromFile("missing_texture.jpg", false, true))
        {
            return Fail("missing replacement texture unexpectedly loaded");
        }
        if (!texture.IsLoaded() || texture.GetRendererID() != textureId ||
            texture.GetLastError().empty())
        {
            return Fail("failed reload did not preserve the previous valid texture");
        }

        std::vector<unsigned char> pixels(8U * 8U * 3U, 127);
        texture.SetData(pixels.data(), static_cast<Pyramid::u32>(pixels.size() - 1));
        if (texture.GetLastError().empty() || g_subImageCalls != 0)
        {
            return Fail("invalid SetData size was not rejected");
        }

        texture.SetData(pixels.data(), static_cast<Pyramid::u32>(pixels.size()));
        if (!texture.GetLastError().empty() || g_subImageCalls != 1)
        {
            return Fail("valid SetData update failed");
        }
    }

    if (std::find(g_deletedTextures.begin(), g_deletedTextures.end(), textureId) ==
        g_deletedTextures.end())
    {
        return Fail("texture object was not released");
    }

    {
        Pyramid::TextureSpecification floatSpec;
        floatSpec.Width = 4;
        floatSpec.Height = 4;
        floatSpec.Format = TextureFormat::RGBA16F;
        floatSpec.GenerateMips = false;
        floatSpec.MinFilter = Pyramid::TextureFilter::Linear;
        std::vector<unsigned char> floatPixels(4U * 4U * 8U, 0);
        OpenGLTexture2D floatTexture(floatSpec, floatPixels.data());
        if (!floatTexture.IsLoaded() || floatTexture.GetRendererID() == 0)
        {
            return Fail("RGBA16F texture did not load");
        }
        if (g_lastInternalFormat != GL_RGBA16F || g_lastDataFormat != GL_RGBA ||
            g_lastDataType != GL_FLOAT)
        {
            return Fail("RGBA16F upload used incorrect OpenGL format triple");
        }

        floatTexture.SetData(floatPixels.data(), static_cast<Pyramid::u32>(floatPixels.size()));
        if (!floatTexture.GetLastError().empty() || g_subImageCalls != 2)
        {
            return Fail("RGBA16F SetData update failed");
        }
        if (g_lastSubImageDataType != GL_FLOAT)
        {
            return Fail("RGBA16F SetData upload used an incorrect pixel type");
        }

        std::vector<unsigned char> shortPixels(floatPixels.size() - 1, 0);
        floatTexture.SetData(shortPixels.data(), static_cast<Pyramid::u32>(shortPixels.size()));
        if (floatTexture.GetLastError().empty())
        {
            return Fail("RGBA16F SetData size mismatch was not rejected");
        }
    }

    {
        Pyramid::TextureSpecification malformedSpec;
        malformedSpec.Width = 0;
        malformedSpec.Height = 4;
        malformedSpec.Format = TextureFormat::RGBA16F;
        OpenGLTexture2D malformedTexture(malformedSpec, nullptr);
        if (malformedTexture.IsLoaded() || malformedTexture.GetLastError().empty())
        {
            return Fail("zero-extent RGBA16F texture did not fail explicitly");
        }
    }

    {
        Pyramid::TextureSpecification oversizedSpec;
        oversizedSpec.Width = static_cast<Pyramid::u32>(std::numeric_limits<GLsizei>::max()) + 1U;
        oversizedSpec.Height = 4;
        oversizedSpec.Format = TextureFormat::RGBA16F;
        OpenGLTexture2D oversizedTexture(oversizedSpec, nullptr);
        if (oversizedTexture.IsLoaded() || oversizedTexture.GetLastError().empty())
        {
            return Fail("oversized-extent RGBA16F texture did not fail explicitly");
        }
    }

    {
        struct FormatExpectation
        {
            Pyramid::TextureFormat format;
            GLenum internalFormat;
            GLenum dataFormat;
            GLenum dataType;
            Pyramid::u32 bytesPerPixel;
        };

        const FormatExpectation kFormatTable[] = {
            { TextureFormat::RGB16F, GL_RGB16F, GL_RGB, GL_FLOAT, 6 },
            { TextureFormat::RGB32F, GL_RGB32F, GL_RGB, GL_FLOAT, 12 },
            { TextureFormat::RGBA32F, GL_RGBA32F, GL_RGBA, GL_FLOAT, 16 },
            { TextureFormat::Depth16, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2 },
            { TextureFormat::Depth24, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4 },
            { TextureFormat::Depth32F, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4 },
            { TextureFormat::Depth24Stencil8, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4 },
            { TextureFormat::Depth32FStencil8, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8 },
            { TextureFormat::R8, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1 },
            { TextureFormat::R16F, GL_R16F, GL_RED, GL_FLOAT, 2 },
            { TextureFormat::R32F, GL_R32F, GL_RED, GL_FLOAT, 4 },
        };

        for (const FormatExpectation& expected : kFormatTable)
        {
            const std::string label = "format table entry " + std::to_string(static_cast<int>(expected.format));
            Pyramid::TextureSpecification spec;
            spec.Width = 4;
            spec.Height = 4;
            spec.Format = expected.format;
            spec.GenerateMips = false;
            spec.MinFilter = Pyramid::TextureFilter::Linear;
            const std::size_t byteSize = static_cast<std::size_t>(16) * expected.bytesPerPixel;
            std::vector<unsigned char> pixels(byteSize, 0);
            OpenGLTexture2D texture(spec, pixels.data());
            if (!texture.IsLoaded() || texture.GetRendererID() == 0)
            {
                return Fail(("mapped texture did not load: " + label).c_str());
            }
            if (g_lastInternalFormat != expected.internalFormat ||
                g_lastDataFormat != expected.dataFormat ||
                g_lastDataType != expected.dataType)
            {
                return Fail(("upload used incorrect OpenGL format triple: " + label).c_str());
            }

            const int subImageCallsBefore = g_subImageCalls;
            texture.SetData(pixels.data(), static_cast<Pyramid::u32>(pixels.size()));
            if (!texture.GetLastError().empty() || g_subImageCalls != subImageCallsBefore + 1)
            {
                return Fail(("valid SetData update failed: " + label).c_str());
            }
            if (g_lastSubImageDataType != expected.dataType)
            {
                return Fail(("SetData upload used an incorrect pixel type: " + label).c_str());
            }

            texture.SetData(pixels.data(), static_cast<Pyramid::u32>(pixels.size() - 1));
            if (texture.GetLastError().empty() || g_subImageCalls != subImageCallsBefore + 1)
            {
                return Fail(("truncated SetData size was not rejected: " + label).c_str());
            }
        }
    }

    {
        const int savedS3TC = GLAD_GL_EXT_texture_compression_s3tc;
        GLAD_GL_EXT_texture_compression_s3tc = 0;
        Pyramid::TextureSpecification spec;
        spec.Width = 4;
        spec.Height = 4;
        spec.Format = TextureFormat::BC1_RGB;
        spec.GenerateMips = false;
        spec.MinFilter = Pyramid::TextureFilter::Linear;
        std::vector<unsigned char> blocks(8, 0);
        OpenGLTexture2D absentTexture(spec, blocks.data());
        const bool absentLoaded = absentTexture.IsLoaded();
        const bool absentDiagnosed = !absentTexture.GetLastError().empty();
        GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
        if (absentLoaded || !absentDiagnosed)
        {
            return Fail("S3TC texture without driver support did not fail explicitly");
        }
        if (g_compressedImageCalls != 0)
        {
            return Fail("S3TC texture without driver support reached the compressed upload");
        }
    }

    {
        const int savedS3TC = GLAD_GL_EXT_texture_compression_s3tc;
        GLAD_GL_EXT_texture_compression_s3tc = 1;

        struct CompressedExpectation
        {
            Pyramid::TextureFormat format;
            GLenum internalFormat;
            std::size_t byteSize;
        };
        const CompressedExpectation kCompressedTable[] = {
            { TextureFormat::BC1_RGB, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 8 },
            { TextureFormat::BC1_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8 },
            { TextureFormat::BC3_RGBA, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 16 },
        };
        for (const CompressedExpectation& expected : kCompressedTable)
        {
            const std::string label =
                "compressed entry " + std::to_string(static_cast<int>(expected.format));
            Pyramid::TextureSpecification spec;
            spec.Width = 4;
            spec.Height = 4;
            spec.Format = expected.format;
            spec.GenerateMips = false;
            spec.MinFilter = Pyramid::TextureFilter::Linear;
            std::vector<unsigned char> blocks(expected.byteSize, 0);
            const int imageCallsBefore = g_compressedImageCalls;
            OpenGLTexture2D texture(spec, blocks.data());
            if (!texture.IsLoaded() || texture.GetRendererID() == 0)
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail(("S3TC texture did not load with driver support: " + label).c_str());
            }
            if (g_compressedImageCalls != imageCallsBefore + 1 ||
                g_lastCompressedInternalFormat != expected.internalFormat ||
                g_lastCompressedImageSize != static_cast<GLsizei>(expected.byteSize))
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail(("S3TC upload used an incorrect block call: " + label).c_str());
            }

            const int subCallsBefore = g_compressedSubImageCalls;
            texture.SetData(blocks.data(), static_cast<Pyramid::u32>(blocks.size()));
            if (!texture.GetLastError().empty() || g_compressedSubImageCalls != subCallsBefore + 1)
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail(("S3TC SetData update failed: " + label).c_str());
            }
            if (g_lastCompressedSubFormat != expected.internalFormat ||
                g_lastCompressedSubImageSize != static_cast<GLsizei>(expected.byteSize))
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail(("S3TC SetData used an incorrect block call: " + label).c_str());
            }

            texture.SetData(blocks.data(), static_cast<Pyramid::u32>(blocks.size() - 1));
            if (texture.GetLastError().empty() || g_compressedSubImageCalls != subCallsBefore + 1)
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail(("truncated S3TC SetData size was not rejected: " + label).c_str());
            }
        }

        {
            Pyramid::TextureSpecification spec;
            spec.Width = 8;
            spec.Height = 8;
            spec.Format = TextureFormat::BC1_RGBA;
            spec.GenerateMips = false;
            spec.MinFilter = Pyramid::TextureFilter::Linear;
            std::vector<unsigned char> blocks(32, 0);
            OpenGLTexture2D texture(spec, blocks.data());
            if (!texture.IsLoaded())
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail("S3TC fixture texture did not load for sub-region tests");
            }
            std::vector<unsigned char> region(8, 0);
            texture.SetSubData(region.data(), 2, 0, 4, 4);
            if (texture.GetLastError().empty())
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail("misaligned compressed sub-region was not rejected");
            }
            const int subCallsBefore = g_compressedSubImageCalls;
            texture.SetSubData(region.data(), 4, 4, 4, 4);
            if (!texture.GetLastError().empty() || g_compressedSubImageCalls != subCallsBefore + 1)
            {
                GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
                return Fail("aligned compressed sub-region update failed");
            }
        }
        GLAD_GL_EXT_texture_compression_s3tc = savedS3TC;
    }

    {
        Pyramid::TextureSpecification noneSpec;
        noneSpec.Width = 4;
        noneSpec.Height = 4;
        noneSpec.Format = TextureFormat::None;
        OpenGLTexture2D noneTexture(noneSpec, nullptr);
        if (noneTexture.IsLoaded() || noneTexture.GetLastError().empty() ||
            noneTexture.GetRendererID() != 0)
        {
            return Fail("None-format texture was not rejected as an explicit sentinel");
        }
    }

    {
        Pyramid::TextureSpecification zeroDepthSpec;
        zeroDepthSpec.Width = 0;
        zeroDepthSpec.Height = 4;
        zeroDepthSpec.Format = TextureFormat::Depth24;
        OpenGLTexture2D zeroDepthTexture(zeroDepthSpec, nullptr);
        if (zeroDepthTexture.IsLoaded() || zeroDepthTexture.GetLastError().empty() ||
            zeroDepthTexture.GetRendererID() != 0)
        {
            return Fail("zero-extent depth texture did not fail with a diagnostic and no handle");
        }
    }

    {
        Pyramid::TextureSpecification maxSpec;
        maxSpec.Width = static_cast<Pyramid::u32>(std::numeric_limits<GLsizei>::max());
        maxSpec.Height = 1;
        maxSpec.Format = TextureFormat::R8;
        maxSpec.GenerateMips = false;
        maxSpec.MinFilter = Pyramid::TextureFilter::Linear;
        OpenGLTexture2D maxTexture(maxSpec, nullptr);
        if (!maxTexture.IsLoaded() || maxTexture.GetRendererID() == 0)
        {
            return Fail("maximum-valid-extent texture did not behave like a valid extent");
        }
        if (g_lastInternalFormat != GL_R8 || g_lastDataFormat != GL_RED ||
            g_lastDataType != GL_UNSIGNED_BYTE)
        {
            return Fail("maximum-valid-extent upload used an incorrect format triple");
        }
    }

    {
        Pyramid::TextureSpecification spec;
        spec.Width = 8;
        spec.Height = 8;
        spec.Format = TextureFormat::RGBA8;
        spec.GenerateMips = false;
        spec.MinFilter = Pyramid::TextureFilter::Linear;
        std::vector<unsigned char> pixels(8U * 8U * 4U, 0);
        OpenGLTexture2D texture(spec, pixels.data());
        if (!texture.IsLoaded())
        {
            return Fail("sub-region fixture texture did not load");
        }

        std::vector<unsigned char> region(4U * 4U * 4U, 0);
        const int subImageCallsBefore = g_subImageCalls;
        texture.SetSubData(region.data(), 0, 0, 4, 4);
        if (!texture.GetLastError().empty() || g_subImageCalls != subImageCallsBefore + 1)
        {
            return Fail("valid SetSubData update failed");
        }
        if (g_lastSubImageDataType != GL_UNSIGNED_BYTE)
        {
            return Fail("SetSubData upload used an incorrect pixel type");
        }

        texture.SetSubData(region.data(), 6, 6, 4, 4);
        if (texture.GetLastError().empty() || g_subImageCalls != subImageCallsBefore + 1)
        {
            return Fail("out-of-bounds SetSubData region was not rejected");
        }

        texture.SetSubData(region.data(), 0, 0, 0, 4);
        if (texture.GetLastError().empty() || g_subImageCalls != subImageCallsBefore + 1)
        {
            return Fail("zero-size SetSubData region was not rejected");
        }

        texture.SetSubData(nullptr, 0, 0, 4, 4);
        if (texture.GetLastError().empty() || g_subImageCalls != subImageCallsBefore + 1)
        {
            return Fail("null SetSubData payload was not rejected");
        }
    }

    std::cout << "Texture loading tests passed\n";
    return EXIT_SUCCESS;
}
