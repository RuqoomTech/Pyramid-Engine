#include <Pyramid/Graphics/OpenGL/OpenGLTexture.hpp>

#include "Fixtures/JPEGFixtures.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    GLuint g_nextTexture = 10;
    GLenum g_lastInternalFormat = 0;
    GLenum g_lastDataFormat = 0;
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
        GLenum,
        const void*)
    {
        g_lastInternalFormat = static_cast<GLenum>(internalFormat);
        g_lastDataFormat = dataFormat;
    }

    void APIENTRY FakeTexSubImage2D(
        GLenum,
        GLint,
        GLint,
        GLint,
        GLsizei,
        GLsizei,
        GLenum,
        GLenum,
        const void*)
    {
        ++g_subImageCalls;
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
        glad_glTexSubImage2D = FakeTexSubImage2D;
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

    std::cout << "Texture loading tests passed\n";
    return EXIT_SUCCESS;
}
