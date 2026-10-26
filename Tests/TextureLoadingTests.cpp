#include <Pyramid/Graphics/OpenGL/OpenGLDevice.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLTexture.hpp>
#include <Pyramid/Graphics/OpenGL/Shader/OpenGLShader.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Texture/TextureCache.hpp>

#include "Fixtures/JPEGFixtures.hpp"
#include "TestGraphicsDevice.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace
{
    struct SamplerState
    {
        GLint minFilter = GL_NEAREST_MIPMAP_LINEAR;
        GLint magFilter = GL_LINEAR;
        GLint wrapS = GL_REPEAT;
        GLint wrapT = GL_REPEAT;
        std::array<GLfloat, 4> border{};
        GLfloat anisotropy = 1.0f;
    };
    std::unordered_map<GLuint, SamplerState> g_samplerStates;
    int g_samplerWrites = 0;
    int g_failSamplerWrite = -1;
    bool g_failEverySamplerWrite = false;
    bool g_failNextMipmap = false;
    int g_anisotropyWrites = 0;
    int g_anisotropyQueries = 0;
    GLint g_driverMajor = 3;
    GLint g_driverMinor = 3;
    GLfloat g_driverMaxAnisotropy = 8.0f;
    GLuint g_nextTexture = 10;
    GLuint g_forcedNextTexture = 0;
    GLenum g_pendingError = GL_NO_ERROR;
    bool g_failNextUpload = false;
    GLuint g_lastUploadTexture = 0;
    GLuint g_lastSubImageTexture = 0;
    GLuint g_lastMipmapTexture = 0;
    GLenum g_lastInternalFormat = 0;
    GLenum g_lastDataFormat = 0;
    GLenum g_lastUploadType = 0;
    GLsizei g_lastUploadWidth = 0;
    GLsizei g_lastUploadHeight = 0;
    const void* g_lastUploadData = nullptr;
    GLenum g_lastUpdateFormat = 0;
    GLenum g_lastUpdateType = 0;
    const void* g_lastUpdateData = nullptr;
    GLint g_lastMagFilter = 0;
    GLint g_lastWrapS = 0;
    GLint g_lastWrapT = 0;
    GLint g_unpackAlignment = 4;
    int g_generatedTextureCount = 0;
    GLint g_lastMinFilter = 0;
    int g_generateMipmapCalls = 0;
    int g_subImageCalls = 0;
    std::vector<GLint> g_unpackAlignments;
    std::vector<GLuint> g_deletedTextures;
    GLenum g_activeTexture = GL_TEXTURE0;
    std::array<GLuint, 32> g_boundTextures{};
    std::vector<GLuint> g_textureBinds;
    std::vector<GLenum> g_textureBindUnits;

    int Fail(const char* message)
    {
        std::cerr << "TextureLoadingTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    GLenum APIENTRY FakeGetError()
    {
        const GLenum error = g_pendingError;
        g_pendingError = GL_NO_ERROR;
        return error;
    }

    void APIENTRY FakeGetIntegerv(GLenum name, GLint* value)
    {
        if (!value)
        {
            return;
        }
        switch (name)
        {
        case GL_UNPACK_ALIGNMENT: *value = g_unpackAlignment; break;
        case GL_MAJOR_VERSION: *value = g_driverMajor; break;
        case GL_MINOR_VERSION: *value = g_driverMinor; break;
        case GL_ACTIVE_TEXTURE: *value = static_cast<GLint>(g_activeTexture); break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *value = 32; break;
        case GL_DEPTH_FUNC: *value = GL_LESS; break;
        case GL_POLYGON_MODE:
            value[0] = GL_FILL;
            value[1] = GL_FILL;
            break;
        case GL_VIEWPORT:
        case GL_SCISSOR_BOX:
            value[0] = value[1] = 0;
            value[2] = 1280;
            value[3] = 720;
            break;
        default: *value = 0; break;
        }
    }

    GLboolean APIENTRY FakeIsEnabled(GLenum)
    {
        return GL_FALSE;
    }

    void APIENTRY FakeGetBooleanv(GLenum, GLboolean* value)
    {
        *value = GL_TRUE;
    }

    bool DriverSupportsAnisotropy()
    {
        return g_driverMajor > 4 || (g_driverMajor == 4 && g_driverMinor >= 6) ||
            GLAD_GL_ARB_texture_filter_anisotropic || GLAD_GL_EXT_texture_filter_anisotropic;
    }

    void APIENTRY FakeGetFloatv(GLenum name, GLfloat* values)
    {
        if (name == GL_MAX_TEXTURE_MAX_ANISOTROPY)
        {
            ++g_anisotropyQueries;
            values[0] = DriverSupportsAnisotropy() ? g_driverMaxAnisotropy : 0.0f;
            if (!DriverSupportsAnisotropy()) g_pendingError = GL_INVALID_ENUM;
            return;
        }
        values[0] = values[1] = values[2] = 0.0f;
        values[3] = 1.0f;
    }

    void APIENTRY FakeGetDoublev(GLenum, GLdouble* value)
    {
        *value = 1.0;
    }

    void APIENTRY FakeGenTextures(GLsizei count, GLuint* textures)
    {
        g_generatedTextureCount += count;
        for (GLsizei index = 0; index < count; ++index)
        {
            textures[index] = g_forcedNextTexture ? g_forcedNextTexture : g_nextTexture++;
            g_forcedNextTexture = 0;
            g_samplerStates[textures[index]] = SamplerState{};
        }
    }

    void APIENTRY FakeDeleteTextures(GLsizei count, const GLuint* textures)
    {
        g_deletedTextures.insert(g_deletedTextures.end(), textures, textures + count);
        for (GLsizei index = 0; index < count; ++index) g_samplerStates.erase(textures[index]);
        for (auto& boundTexture : g_boundTextures)
        {
            if (std::find(textures, textures + count, boundTexture) != textures + count)
            {
                boundTexture = 0;
            }
        }
    }

    void APIENTRY FakeBindTexture(GLenum target, GLuint texture)
    {
        if (target == GL_TEXTURE_2D && g_activeTexture >= GL_TEXTURE0 &&
            g_activeTexture < GL_TEXTURE0 + g_boundTextures.size())
        {
            g_boundTextures[g_activeTexture - GL_TEXTURE0] = texture;
            g_textureBinds.push_back(texture);
            g_textureBindUnits.push_back(g_activeTexture);
        }
    }

    void APIENTRY FakePixelStorei(GLenum name, GLint value)
    {
        if (name == GL_UNPACK_ALIGNMENT)
        {
            g_unpackAlignments.push_back(value);
            g_unpackAlignment = value;
        }
    }

    void APIENTRY FakeTexImage2D(
        GLenum,
        GLint,
        GLint internalFormat,
        GLsizei width,
        GLsizei height,
        GLint,
        GLenum dataFormat,
        GLenum type,
        const void* data)
    {
        g_lastInternalFormat = static_cast<GLenum>(internalFormat);
        g_lastDataFormat = dataFormat;
        g_lastUploadType = type;
        g_lastUploadWidth = width;
        g_lastUploadHeight = height;
        g_lastUploadData = data;
        g_lastUploadTexture = g_boundTextures[g_activeTexture - GL_TEXTURE0];
        if (g_failNextUpload)
        {
            g_pendingError = GL_OUT_OF_MEMORY;
            g_failNextUpload = false;
        }
    }

    void APIENTRY FakeTexSubImage2D(
        GLenum,
        GLint,
        GLint,
        GLint,
        GLsizei,
        GLsizei,
        GLenum format,
        GLenum type,
        const void* data)
    {
        ++g_subImageCalls;
        g_lastUpdateFormat = format;
        g_lastUpdateType = type;
        g_lastUpdateData = data;
        g_lastSubImageTexture = g_boundTextures[g_activeTexture - GL_TEXTURE0];
    }

    bool FailSamplerWrite()
    {
        ++g_samplerWrites;
        if (g_failEverySamplerWrite || g_samplerWrites == g_failSamplerWrite)
        {
            g_pendingError = GL_INVALID_OPERATION;
            return true;
        }
        return false;
    }

    SamplerState& CurrentSampler()
    {
        return g_samplerStates[g_boundTextures[g_activeTexture - GL_TEXTURE0]];
    }

    void APIENTRY FakeTexParameteri(GLenum, GLenum name, GLint value)
    {
        if (FailSamplerWrite()) return;
        auto& sampler = CurrentSampler();
        switch (name)
        {
        case GL_TEXTURE_MIN_FILTER: g_lastMinFilter = sampler.minFilter = value; break;
        case GL_TEXTURE_MAG_FILTER: g_lastMagFilter = sampler.magFilter = value; break;
        case GL_TEXTURE_WRAP_S: g_lastWrapS = sampler.wrapS = value; break;
        case GL_TEXTURE_WRAP_T: g_lastWrapT = sampler.wrapT = value; break;
        default: break;
        }
    }

    void APIENTRY FakeTexParameterfv(GLenum, GLenum name, const GLfloat* values)
    {
        if (FailSamplerWrite()) return;
        if (name == GL_TEXTURE_BORDER_COLOR) std::copy_n(values, 4, CurrentSampler().border.begin());
    }

    void APIENTRY FakeTexParameterf(GLenum, GLenum name, GLfloat value)
    {
        if (name == GL_TEXTURE_MAX_ANISOTROPY) ++g_anisotropyWrites;
        if (FailSamplerWrite()) return;
        if (name == GL_TEXTURE_MAX_ANISOTROPY && DriverSupportsAnisotropy())
            CurrentSampler().anisotropy = value;
        else
            g_pendingError = GL_INVALID_ENUM;
    }

    void APIENTRY FakeGenerateMipmap(GLenum)
    {
        ++g_generateMipmapCalls;
        g_lastMipmapTexture = g_boundTextures[g_activeTexture - GL_TEXTURE0];
        if (g_failNextMipmap)
        {
            g_failNextMipmap = false;
            g_pendingError = GL_INVALID_OPERATION;
        }
    }

    void APIENTRY FakeActiveTexture(GLenum texture)
    {
        g_activeTexture = texture;
    }

    void InstallFakeOpenGL()
    {
        glad_glGetError = FakeGetError;
        glad_glGetIntegerv = FakeGetIntegerv;
        glad_glIsEnabled = FakeIsEnabled;
        glad_glGetBooleanv = FakeGetBooleanv;
        glad_glGetFloatv = FakeGetFloatv;
        glad_glGetDoublev = FakeGetDoublev;
        glad_glGenTextures = FakeGenTextures;
        glad_glDeleteTextures = FakeDeleteTextures;
        glad_glBindTexture = FakeBindTexture;
        glad_glPixelStorei = FakePixelStorei;
        glad_glTexImage2D = FakeTexImage2D;
        glad_glTexSubImage2D = FakeTexSubImage2D;
        glad_glTexParameteri = FakeTexParameteri;
        glad_glTexParameterfv = FakeTexParameterfv;
        glad_glTexParameterf = FakeTexParameterf;
        glad_glGenerateMipmap = FakeGenerateMipmap;
        glad_glActiveTexture = FakeActiveTexture;
    }

    class ForeignTexture final : public Pyramid::ITexture2D
    {
    public:
        explicit ForeignTexture(Pyramid::u32 rendererId) : m_rendererId(rendererId) {}
        void Bind(Pyramid::u32) const override {}
        void Unbind(Pyramid::u32) const override {}
        Pyramid::u32 GetWidth() const override { return 1; }
        Pyramid::u32 GetHeight() const override { return 1; }
        Pyramid::u32 GetRendererID() const override { return m_rendererId; }
        const std::string& GetPath() const override { return m_path; }
        bool IsLoaded() const override { return true; }

    private:
        Pyramid::u32 m_rendererId;
        std::string m_path;
    };

    class BindingShader final : public Pyramid::OpenGLShader
    {
    public:
        bool Compile(const std::string&, const std::string&) override { return true; }
        void Bind() override {}
        void SetUniformInt(const std::string& name, int value) override
        {
            uniformName = name;
            samplerSlot = value;
        }

        std::string uniformName;
        int samplerSlot = -1;
    };

    int TestDeviceTextureBinding()
    {
        using namespace Pyramid;

        OpenGLDevice device(nullptr);
        TextureSpecification specification;
        specification.GenerateMips = false;
        const std::array<u8, 4> pixel = {255, 127, 63, 255};
        auto direct = device.CreateTexture2D(specification, pixel.data());

        TextureCache cache(device);
        TextureResourceSpecification resourceSpecification;
        resourceSpecification.texture = specification;
        resourceSpecification.pixelData = pixel.data();
        resourceSpecification.pixelDataSize = pixel.size();
        auto cached = cache.GetOrCreate(resourceSpecification);
        if (!direct || !direct->IsLoaded() || !cached || !cached->IsLoaded() ||
            cached != cache.GetOrCreate(resourceSpecification))
        {
            return Fail("real OpenGL direct/cache texture setup failed");
        }
        TextureSpecification invalidSpecification = specification;
        invalidSpecification.Width = 0;
        OpenGLTexture2D unloaded(invalidSpecification, nullptr);
        if (unloaded.IsLoaded() || unloaded.GetRendererID() != 0)
        {
            return Fail("unloaded OpenGL texture fixture unexpectedly loaded");
        }

        auto foreign = std::make_shared<ForeignTexture>(cached->GetRendererID());
        auto shader = std::make_shared<BindingShader>();
        Tests::TestGraphicsDevice fixtureDevice;
        fixtureDevice.textureFactory = [&](const TextureSpecification&, const void*) { return foreign; };
        fixtureDevice.shaderFactory = [&]() { return shader; };
        auto foreignResource = TextureResource::Create(fixtureDevice, resourceSpecification);
        ShaderProgramSpecification shaderSpecification;
        shaderSpecification.vertexSource = "#version 330 core\nvoid main(){gl_Position=vec4(0.0);}";
        shaderSpecification.fragmentSource = "#version 330 core\nout vec4 color; void main(){color=vec4(1.0);}";
        auto program = ShaderProgram::Create(fixtureDevice, shaderSpecification);
        MaterialSpecification materialSpecification;
        materialSpecification.shader = program;
        materialSpecification.textures = {{"u_AlbedoMap", 4, cached}};
        materialSpecification.renderState.depthTest = false;
        materialSpecification.renderState.cullMode = MaterialCullMode::None;
        auto material = Material::Create(materialSpecification);
        if (!foreignResource || !program || !material)
        {
            return Fail("material or foreign-wrapper binding fixture setup failed");
        }

        OpenGLStateManager::GetInstance().InvalidateState();
        g_textureBinds.clear();
        g_textureBindUnits.clear();
        device.BindTexture(direct.get(), 3);
        if (g_textureBinds.size() != 1 || g_boundTextures[3] != direct->GetRendererID() ||
            g_textureBindUnits.back() != GL_TEXTURE3)
        {
            return Fail("OpenGLDevice did not bind a direct texture on the requested slot");
        }
        device.BindTexture(direct.get(), 3);
        if (g_textureBinds.size() != 1)
        {
            return Fail("repeated direct binding bypassed the OpenGL state cache");
        }
        device.BindTexture(cached.get(), 3);
        if (g_textureBinds.size() != 2 || g_boundTextures[3] != cached->GetRendererID() ||
            g_textureBindUnits.back() != GL_TEXTURE3 || !device.GetLastError().empty())
        {
            return Fail("OpenGLDevice rejected or failed to bind a cached TextureResource");
        }
        device.BindTexture(cached.get(), 3);
        cached->GetTexture()->Bind(3);
        if (g_textureBinds.size() != 2)
        {
            return Fail("cached wrapper and its OpenGL texture did not share binding state");
        }

        Renderer::CommandBuffer commands;
        commands.RegisterTexture(7, cached.get());
        commands.Begin();
        commands.SetTexture(direct.get(), 6);
        commands.SetTexture(cached.get(), 6);
        commands.SetTexture(u32{7}, 7);
        commands.SetMaterial(material.get());
        commands.End();
        const auto priorBinds = g_textureBinds.size();
        commands.Execute(&device);
        if (g_textureBinds.size() != priorBinds + 4 ||
            g_boundTextures[6] != cached->GetRendererID() ||
            g_boundTextures[7] != cached->GetRendererID() ||
            g_boundTextures[4] != cached->GetRendererID() ||
            shader->uniformName != "u_AlbedoMap" || shader->samplerSlot != 4)
        {
            return Fail("command/material submission did not bind cached texture IDs and sampler slots");
        }

        device.BindTexture(nullptr, 3);
        device.BindTexture(&unloaded, 6);
        if (g_boundTextures[3] != 0 || g_boundTextures[6] != 0)
        {
            return Fail("null and unloaded textures no longer unbind their requested slots");
        }
        device.BindTexture(cached.get(), 31);
        if (g_boundTextures[31] != cached->GetRendererID())
        {
            return Fail("last supported texture slot was rejected");
        }
        const auto acceptedBinds = g_textureBinds.size();
        const auto acceptedUnit = g_activeTexture;
        device.BindTexture(cached.get(), 32);
        if (g_textureBinds.size() != acceptedBinds || g_activeTexture != acceptedUnit ||
            device.GetLastError().find("out of range") == std::string::npos)
        {
            return Fail("out-of-range texture binding changed GL state or lacked a diagnostic");
        }
        device.BindTexture(foreign.get(), 4);
        device.BindTexture(foreignResource.get(), 4);
        if (g_textureBinds.size() != acceptedBinds || g_activeTexture != acceptedUnit ||
            g_boundTextures[4] != cached->GetRendererID() ||
            device.GetLastError().find("invalid OpenGL texture") == std::string::npos)
        {
            return Fail("foreign texture or wrapper bypassed backend validation");
        }
        return EXIT_SUCCESS;
    }

    int TestTextureStateCoherence()
    {
        using namespace Pyramid;

        OpenGLDevice device(nullptr);
        TextureSpecification specification;
        specification.GenerateMips = false;
        const std::array<u8, 4> pixel = {63, 127, 191, 255};
        auto anchor = device.CreateTexture2D(specification, pixel.data());
        auto unaffected = device.CreateTexture2D(specification, pixel.data());
        if (!anchor || !anchor->IsLoaded() || !unaffected || !unaffected->IsLoaded())
        {
            return Fail("texture-state anchor setup failed");
        }
        auto& state = OpenGLStateManager::GetInstance();
        state.InvalidateState();
        device.BindTexture(unaffected.get(), 5);
        device.BindTexture(anchor.get(), 2);
        auto uploaded = device.CreateTexture2D(specification, pixel.data());
        if (!uploaded || !uploaded->IsLoaded() || g_activeTexture != GL_TEXTURE2 ||
            g_boundTextures[2] != 0 || g_boundTextures[5] != unaffected->GetRendererID())
        {
            return Fail("texture upload changed the active selector, untouched unit, or final unbinding");
        }
        device.BindTexture(anchor.get(), 2);
        if (g_boundTextures[2] != anchor->GetRendererID())
        {
            return Fail("binding cache skipped restoring a texture after upload on a nonzero unit");
        }

        uploaded->SetData(pixel.data(), static_cast<u32>(pixel.size()));
        if (!uploaded->GetLastError().empty() ||
            g_lastSubImageTexture != uploaded->GetRendererID() ||
            g_activeTexture != GL_TEXTURE2 || g_boundTextures[2] != 0)
        {
            return Fail("SetData updated the wrong texture or changed its binding side effects");
        }
        device.BindTexture(anchor.get(), 2);
        if (g_boundTextures[2] != anchor->GetRendererID())
        {
            return Fail("binding cache skipped restoring a texture after SetData");
        }
        uploaded->GenerateMipmaps();
        if (!uploaded->GetLastError().empty() ||
            g_lastMipmapTexture != uploaded->GetRendererID() ||
            g_activeTexture != GL_TEXTURE2 || g_boundTextures[2] != 0)
        {
            return Fail("mipmap generation used the wrong texture or changed binding side effects");
        }
        device.BindTexture(anchor.get(), 2);
        if (g_boundTextures[2] != anchor->GetRendererID())
        {
            return Fail("binding cache skipped restoring a texture after mipmap generation");
        }

        anchor->Bind(7);
        device.BindTexture(uploaded.get(), 2);
        if (g_activeTexture != GL_TEXTURE2 || g_boundTextures[2] != uploaded->GetRendererID() ||
            g_boundTextures[7] != anchor->GetRendererID())
        {
            return Fail("direct texture Bind desynchronized the active-unit cache");
        }
        uploaded->Unbind(7);
        if (g_activeTexture != GL_TEXTURE7 || g_boundTextures[7] != 0)
        {
            return Fail("direct texture Unbind did not affect the requested unit");
        }
        device.BindTexture(anchor.get(), 7);
        if (g_boundTextures[7] != anchor->GetRendererID())
        {
            return Fail("direct texture Unbind left a falsely cached binding");
        }

        auto doomed = device.CreateTexture2D(specification, pixel.data());
        if (!doomed || !doomed->IsLoaded())
        {
            return Fail("texture deletion fixture failed to load");
        }
        const GLuint deletedName = doomed->GetRendererID();
        device.BindTexture(doomed.get(), 11);
        device.BindTexture(doomed.get(), 12);
        device.BindTexture(anchor.get(), 2);
        doomed.reset();
        if (g_boundTextures[11] != 0 || g_boundTextures[12] != 0 ||
            g_boundTextures[2] != anchor->GetRendererID())
        {
            return Fail("texture deletion did not unbind only the deleted name across all units");
        }
        g_forcedNextTexture = deletedName;
        auto reused = device.CreateTexture2D(specification, pixel.data());
        if (!reused || !reused->IsLoaded() || reused->GetRendererID() != deletedName)
        {
            return Fail("OpenGL name-reuse fixture did not reuse the deleted name");
        }
        device.BindTexture(reused.get(), 11);
        device.BindTexture(reused.get(), 12);
        if (g_boundTextures[11] != deletedName || g_boundTextures[12] != deletedName)
        {
            return Fail("deleted-name cache entries suppressed binding a reused texture name");
        }

        const std::string filepath = "pyramid_texture_reload_fixture.jpg";
        struct FixtureCleanup
        {
            const std::string& path;
            ~FixtureCleanup() { std::remove(path.c_str()); }
        } cleanup{filepath};
        {
            std::ofstream file(filepath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(Tests::Fixtures::BaselineRGBJPEG),
                static_cast<std::streamsize>(Tests::Fixtures::BaselineRGBJPEGSize));
            if (!file)
            {
                return Fail("could not write reload fixture");
            }
        }
        OpenGLTexture2D reloadable(filepath, false, false);
        if (!reloadable.IsLoaded())
        {
            return Fail("reload fixture failed to load");
        }
        const GLuint originalName = reloadable.GetRendererID();
        device.BindTexture(&reloadable, 10);
        device.BindTexture(anchor.get(), 2);
        if (!reloadable.LoadFromFile(filepath, true, true) ||
            reloadable.GetRendererID() == originalName || g_boundTextures[10] != 0 ||
            g_activeTexture != GL_TEXTURE2 || g_boundTextures[2] != 0)
        {
            return Fail("successful reload did not publish a replacement and retire old bindings");
        }
        device.BindTexture(&reloadable, 10);
        device.BindTexture(anchor.get(), 2);
        if (g_boundTextures[2] != anchor->GetRendererID() ||
            g_boundTextures[10] != reloadable.GetRendererID())
        {
            return Fail("binding cache did not track a successful texture reload");
        }
        const GLuint committedName = reloadable.GetRendererID();
        const auto beforeMissingReload = g_textureBinds.size();
        if (reloadable.LoadFromFile("missing_texture_reload.jpg", false, false) ||
            reloadable.GetRendererID() != committedName ||
            g_textureBinds.size() != beforeMissingReload || g_activeTexture != GL_TEXTURE2 ||
            g_boundTextures[2] != anchor->GetRendererID() ||
            g_boundTextures[10] != committedName)
        {
            return Fail("failed CPU reload changed committed resources or GPU bindings");
        }
        g_failNextUpload = true;
        if (reloadable.LoadFromFile(filepath, false, false) || !reloadable.IsLoaded() ||
            reloadable.GetRendererID() != committedName ||
            reloadable.GetLastError().empty() || g_lastUploadTexture == committedName ||
            std::find(g_deletedTextures.begin(), g_deletedTextures.end(), g_lastUploadTexture) ==
                g_deletedTextures.end() || g_boundTextures[10] != committedName ||
            g_activeTexture != GL_TEXTURE2 || g_boundTextures[2] != 0)
        {
            return Fail("failed GPU reload did not discard only the staged replacement");
        }
        device.BindTexture(anchor.get(), 2);
        if (g_boundTextures[2] != anchor->GetRendererID())
        {
            return Fail("failed GPU reload left a falsely cached binding");
        }
        const auto beforeUnchangedBind = g_textureBinds.size();
        device.BindTexture(unaffected.get(), 5);
        if (g_boundTextures[5] != unaffected->GetRendererID() ||
            g_textureBinds.size() != beforeUnchangedBind)
        {
            return Fail("texture mutation invalidated an unrelated unit's cached binding");
        }

        device.BindTexture(anchor.get(), 9);
        state.InvalidateState();
        state.BindTexture(GL_TEXTURE_2D, uploaded->GetRendererID());
        state.BindTextureUnit(0, GL_TEXTURE_2D, uploaded->GetRendererID());
        if (g_activeTexture != GL_TEXTURE0 || g_boundTextures[0] != uploaded->GetRendererID() ||
            g_boundTextures[9] != uploaded->GetRendererID())
        {
            return Fail("retained invalidated state manager confused the active unit with unit zero");
        }
        return EXIT_SUCCESS;
    }
    int TestAdvertisedTextureFormats()
    {
        using namespace Pyramid;
        std::array<u8, 24> bytes;
        bytes.fill(127);
        std::array<u16, 24> halves;
        halves.fill(0x3800); // IEEE binary16 0.5, supplied as bits rather than integer values.
        std::array<f32, 24> floats;
        floats.fill(0.5f);
        std::array<u16, 6> depth16;
        depth16.fill(0x7fff);
        std::array<u32, 6> depth24;
        depth24.fill(0x7fffffffu);
        std::array<u32, 6> depth24Stencil;
        depth24Stencil.fill(0x7fffffa5u);
        struct Depth32StencilPixel { f32 depth; u32 stencil; };
        static_assert(sizeof(Depth32StencilPixel) == 8, "depth32f/stencil8 transfer must occupy eight bytes");
        std::array<Depth32StencilPixel, 6> depth32Stencil;
        depth32Stencil.fill({0.5f, 0xa5u});

        struct FormatCase
        {
            const char* name;
            TextureFormat format;
            GLenum internalFormat;
            GLenum dataFormat;
            GLenum dataType;
            u32 bytesPerPixel;
            const void* pixels;
            bool depth = false;
            bool srgb = false;
        };
        const FormatCase cases[] = {
            {"R8", TextureFormat::R8, GL_R8, GL_RED, GL_UNSIGNED_BYTE, 1, bytes.data()},
            {"R16F", TextureFormat::R16F, GL_R16F, GL_RED, GL_HALF_FLOAT, 2, halves.data()},
            {"R32F", TextureFormat::R32F, GL_R32F, GL_RED, GL_FLOAT, 4, floats.data()},
            {"RGB8", TextureFormat::RGB8, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 3, bytes.data()},
            {"RGBA8", TextureFormat::RGBA8, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, bytes.data()},
            {"SRGB8", TextureFormat::SRGB8, GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, 3, bytes.data()},
            {"SRGBA8", TextureFormat::SRGBA8, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, bytes.data()},
            {"RGB16F", TextureFormat::RGB16F, GL_RGB16F, GL_RGB, GL_HALF_FLOAT, 6, halves.data()},
            {"RGBA16F", TextureFormat::RGBA16F, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT, 8, halves.data()},
            {"RGB32F", TextureFormat::RGB32F, GL_RGB32F, GL_RGB, GL_FLOAT, 12, floats.data()},
            {"RGBA32F", TextureFormat::RGBA32F, GL_RGBA32F, GL_RGBA, GL_FLOAT, 16, floats.data()},
            {"Depth16", TextureFormat::Depth16, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, 2, depth16.data(), true},
            {"Depth24", TextureFormat::Depth24, GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, 4, depth24.data(), true},
            {"Depth32F", TextureFormat::Depth32F, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, 4, floats.data(), true},
            {"Depth24Stencil8", TextureFormat::Depth24Stencil8, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 4, depth24Stencil.data(), true},
            {"Depth32FStencil8", TextureFormat::Depth32FStencil8, GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV, 8, depth32Stencil.data(), true},
            {"RGB8+sRGB", TextureFormat::RGB8, GL_SRGB8, GL_RGB, GL_UNSIGNED_BYTE, 3, bytes.data(), false, true},
            {"RGBA8+sRGB", TextureFormat::RGBA8, GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE, 4, bytes.data(), false, true}
        };
        for (const auto& test : cases)
        {
            const auto fail = [&](const char* message)
            {
                std::cerr << "Format " << test.name << ": ";
                return Fail(message);
            };
            TextureSpecification specification;
            specification.Width = 3;
            specification.Height = 2;
            specification.Format = test.format;
            specification.GenerateMips = false;
            specification.SRGB = test.srgb;
            g_unpackAlignment = 8;
            OpenGLTexture2D texture(specification, test.pixels);
            if (!texture.IsLoaded() || texture.GetRendererID() == 0 ||
                texture.GetWidth() != 3 || texture.GetHeight() != 2 || texture.GetFormat() != test.format ||
                g_lastInternalFormat != test.internalFormat || g_lastDataFormat != test.dataFormat ||
                g_lastUploadType != test.dataType || g_lastUploadWidth != 3 || g_lastUploadHeight != 2 ||
                g_lastUploadData != test.pixels || g_unpackAlignment != 8)
            {
                return fail("advertised format upload or metadata did not match the CPU transfer contract");
            }
            const u32 byteCount = 6 * test.bytesPerPixel;
            const int beforeUpdates = g_subImageCalls;
            texture.SetData(test.pixels, byteCount - 1);
            if (texture.GetLastError().empty() || g_subImageCalls != beforeUpdates)
            {
                return fail("short SetData upload was not rejected before GL");
            }
            texture.SetData(test.pixels, byteCount + 1);
            if (texture.GetLastError().empty() || g_subImageCalls != beforeUpdates)
            {
                return fail("oversized SetData upload was not rejected before GL");
            }
            texture.SetData(nullptr, byteCount);
            if (texture.GetLastError().empty() || g_subImageCalls != beforeUpdates)
            {
                return fail("null SetData upload was not rejected before GL");
            }
            texture.SetData(test.pixels, byteCount);
            if (!texture.GetLastError().empty() || g_subImageCalls != beforeUpdates + 1 ||
                g_lastUpdateFormat != test.dataFormat || g_lastUpdateType != test.dataType ||
                g_lastUpdateData != test.pixels || g_unpackAlignment != 8)
            {
                return fail("valid SetData used the wrong scalar type or upload byte count");
            }
            OpenGLTexture2D storage(specification, nullptr);
            if (!storage.IsLoaded() || g_lastUploadData != nullptr || g_lastUploadType != test.dataType ||
                g_lastDataFormat != test.dataFormat || g_lastInternalFormat != test.internalFormat)
            {
                return fail("null-data storage allocation did not preserve the format");
            }
            specification.GenerateMips = true;
            const int beforeMips = g_generateMipmapCalls;
            OpenGLTexture2D mipmapped(specification, nullptr);
            if (!mipmapped.IsLoaded() || mipmapped.GetMipLevels() != 2 ||
                g_generateMipmapCalls != beforeMips + 1 || g_lastMinFilter != GL_LINEAR_MIPMAP_LINEAR)
            {
                return fail("null-data allocation did not create its requested complete mip chain");
            }
            const int beforeFactory = g_generatedTextureCount;
            auto depthTarget = ITexture2D::CreateDepthTarget(3, 2, test.format);
            if (test.depth)
            {
                if (!depthTarget || !depthTarget->IsLoaded() || depthTarget->GetFormat() != test.format ||
                    depthTarget->GetMipLevels() != 1 || g_lastUploadType != test.dataType ||
                    g_lastDataFormat != test.dataFormat || g_lastUploadData != nullptr ||
                    g_lastMinFilter != GL_NEAREST || g_lastMagFilter != GL_NEAREST ||
                    g_lastWrapS != GL_CLAMP_TO_EDGE || g_lastWrapT != GL_CLAMP_TO_EDGE)
                {
                    return fail("depth-target factory did not create the requested depth format and sampler");
                }
            }
            else if (depthTarget || g_generatedTextureCount != beforeFactory)
            {
                return fail("depth-target factory accepted a color format or issued GL calls");
            }
            if (test.format != TextureFormat::RGB8 && test.format != TextureFormat::RGBA8 &&
                test.format != TextureFormat::SRGB8 && test.format != TextureFormat::SRGBA8)
            {
                specification.SRGB = true;
                const int beforeInvalid = g_generatedTextureCount;
                OpenGLTexture2D invalidSRGB(specification, nullptr);
                if (invalidSRGB.IsLoaded() || invalidSRGB.GetRendererID() != 0 ||
                    invalidSRGB.GetLastError().find("sRGB") == std::string::npos ||
                    g_generatedTextureCount != beforeInvalid)
                {
                    return fail("invalid sRGB flag was accepted or reached GL allocation");
                }
            }
        }
        struct InvalidCase { TextureFormat format; const char* diagnostic; };
        const InvalidCase invalidCases[] = {
            {TextureFormat::None, "None"},
            {TextureFormat::BC1_RGB, "BC1_RGB"},
            {TextureFormat::BC1_RGBA, "BC1_RGBA"},
            {TextureFormat::BC3_RGBA, "BC3_RGBA"},
            {TextureFormat::BC7_RGBA, "BC7_RGBA"},
            {static_cast<TextureFormat>(9999), "9999"}
        };
        for (const auto& test : invalidCases)
        {
            TextureSpecification specification;
            specification.Format = test.format;
            const int beforeInvalid = g_generatedTextureCount;
            OpenGLTexture2D invalid(specification, nullptr);
            if (invalid.IsLoaded() || invalid.GetRendererID() != 0 ||
                invalid.GetLastError().find(test.diagnostic) == std::string::npos ||
                ITexture2D::CreateDepthTarget(3, 2, test.format) ||
                g_generatedTextureCount != beforeInvalid)
            {
                std::cerr << "Invalid format " << test.diagnostic << ": ";
                return Fail("unsupported format lacked a specific diagnostic or allocated a texture");
            }
        }
        TextureSpecification overflowing;
        overflowing.Width = static_cast<u32>((std::numeric_limits<GLsizei>::max)());
        overflowing.Height = overflowing.Width;
        overflowing.Format = TextureFormat::RGBA32F;
        const int beforeOverflow = g_generatedTextureCount;
        OpenGLTexture2D overflowTexture(overflowing, nullptr);
        if (overflowTexture.IsLoaded() || overflowTexture.GetLastError().empty() ||
            g_generatedTextureCount != beforeOverflow)
        {
            return Fail("overflowing texture byte size reached GL allocation");
        }
        g_unpackAlignment = 4;
        return EXIT_SUCCESS;
    }

    int TestReloadTransferLayout()
    {
        using namespace Pyramid;
        const std::string filepath = "pyramid_texture_type_transition_fixture.jpg";
        struct FixtureCleanup
        {
            const std::string& path;
            ~FixtureCleanup() { std::remove(path.c_str()); }
        } cleanup{filepath};
        {
            std::ofstream file(filepath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(Tests::Fixtures::BaselineRGBJPEG),
                static_cast<std::streamsize>(Tests::Fixtures::BaselineRGBJPEGSize));
            if (!file) return Fail("could not write transfer-layout reload fixture");
        }
        TextureSpecification specification;
        specification.Width = 3;
        specification.Height = 2;
        specification.Format = TextureFormat::R32F;
        specification.GenerateMips = false;
        const std::array<f32, 6> values = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f};
        OpenGLTexture2D texture(specification, values.data());
        if (!texture.IsLoaded()) return Fail("native float reload fixture did not load");
        const GLuint committed = texture.GetRendererID();
        if (texture.LoadFromFile("missing_native_float_reload.jpg", false, false) ||
            texture.GetRendererID() != committed || texture.GetFormat() != TextureFormat::R32F)
        {
            return Fail("failed CPU reload changed native float metadata");
        }
        g_failNextUpload = true;
        if (texture.LoadFromFile(filepath, false, false) || texture.GetRendererID() != committed ||
            texture.GetFormat() != TextureFormat::R32F || texture.GetWidth() != 3 || texture.GetHeight() != 2)
        {
            return Fail("failed GPU reload changed native float metadata");
        }
        texture.SetData(values.data(), static_cast<u32>(sizeof(values)));
        if (!texture.GetLastError().empty() || g_lastUpdateFormat != GL_RED || g_lastUpdateType != GL_FLOAT)
        {
            return Fail("failed reload did not preserve native float SetData transfer type");
        }
        if (!texture.LoadFromFile(filepath, false, false) || texture.GetFormat() != TextureFormat::RGB8 ||
            texture.GetWidth() != 8 || texture.GetHeight() != 8)
        {
            return Fail("successful float-to-file reload did not commit RGB metadata");
        }
        const std::array<u8, 8 * 8 * 3> pixels{};
        texture.SetData(pixels.data(), static_cast<u32>(pixels.size()));
        if (!texture.GetLastError().empty() || g_lastUpdateFormat != GL_RGB ||
            g_lastUpdateType != GL_UNSIGNED_BYTE || g_lastUpdateData != pixels.data())
        {
            return Fail("successful float-to-file reload retained stale scalar type or byte size");
        }
        return EXIT_SUCCESS;
    }
    bool SameSampler(const SamplerState& left, const SamplerState& right)
    {
        return left.minFilter == right.minFilter && left.magFilter == right.magFilter &&
            left.wrapS == right.wrapS && left.wrapT == right.wrapT &&
            left.border == right.border && left.anisotropy == right.anisotropy;
    }

    struct SamplerDriverScope
    {
        GLint major = g_driverMajor;
        GLint minor = g_driverMinor;
        GLfloat maximum = g_driverMaxAnisotropy;
        int arb = GLAD_GL_ARB_texture_filter_anisotropic;
        int ext = GLAD_GL_EXT_texture_filter_anisotropic;
        int loadedMajor = GLVersion.major;
        int loadedMinor = GLVersion.minor;
        SamplerDriverScope()
        {
            GLVersion.major = g_driverMajor;
            GLVersion.minor = g_driverMinor;
        }
        ~SamplerDriverScope()
        {
            g_driverMajor = major;
            g_driverMinor = minor;
            g_driverMaxAnisotropy = maximum;
            GLAD_GL_ARB_texture_filter_anisotropic = arb;
            GLAD_GL_EXT_texture_filter_anisotropic = ext;
            GLVersion.major = loadedMajor;
            GLVersion.minor = loadedMinor;
            g_failSamplerWrite = -1;
            g_failEverySamplerWrite = false;
            g_failNextMipmap = false;
        }
    };

    int TestMutableSamplerRegression()
    {
        using namespace Pyramid;
        SamplerDriverScope driverScope;
        g_driverMajor = 3;
        g_driverMinor = 3;
        GLAD_GL_ARB_texture_filter_anisotropic = 0;
        GLAD_GL_EXT_texture_filter_anisotropic = 0;
        OpenGLDevice device(nullptr);
        TextureSpecification specification;
        specification.Width = specification.Height = 4;
        specification.GenerateMips = false;
        specification.MinFilter = TextureFilter::NearestMipmapNearest;
        specification.MagFilter = TextureFilter::None;
        OpenGLTexture2D texture(specification, nullptr);
        auto anchor = device.CreateTexture2D(specification, nullptr);
        if (!texture.IsLoaded() || !anchor || !anchor->IsLoaded() ||
            g_samplerStates[texture.GetRendererID()].minFilter != GL_NEAREST)
        {
            return Fail("mutable sampler fixture lost its requested mip-filter base fallback");
        }
        const GLuint id = texture.GetRendererID();
        const SamplerState anchorSampler = g_samplerStates[anchor->GetRendererID()];
        device.BindTexture(anchor.get(), 5);
        device.BindTexture(anchor.get(), 2);
        const auto coherentBinding = [&]()
        {
            if (g_activeTexture != GL_TEXTURE2 || g_boundTextures[2] != 0 ||
                g_boundTextures[5] != anchor->GetRendererID() ||
                !SameSampler(anchorSampler, g_samplerStates[anchor->GetRendererID()])) return false;
            const auto binds = g_textureBinds.size();
            device.BindTexture(anchor.get(), 2);
            device.BindTexture(anchor.get(), 5);
            device.BindTexture(anchor.get(), 2);
            return g_boundTextures[2] == anchor->GetRendererID() &&
                g_textureBinds.size() == binds + 1 && g_activeTexture == GL_TEXTURE2;
        };
        texture.SetWrap(TextureWrap::ClampToEdge, TextureWrap::MirroredRepeat);
        if (!texture.GetLastError().empty() || g_samplerStates[id].wrapS != GL_CLAMP_TO_EDGE ||
            g_samplerStates[id].wrapT != GL_MIRRORED_REPEAT || !coherentBinding())
        {
            return Fail("mutable SetWrap did not publish only its texture's sampler coherently");
        }
        texture.SetFilter(TextureFilter::None, TextureFilter::None);
        if (!texture.GetLastError().empty() || g_samplerStates[id].minFilter != GL_LINEAR ||
            g_samplerStates[id].magFilter != GL_LINEAR || !coherentBinding())
        {
            return Fail("None filter aliases no longer map to linear filtering");
        }
        texture.SetWrap(TextureWrap::None, TextureWrap::None);
        if (!texture.GetLastError().empty() || g_samplerStates[id].wrapS != GL_REPEAT ||
            g_samplerStates[id].wrapT != GL_REPEAT || !coherentBinding())
        {
            return Fail("None wrap aliases no longer map to repeating");
        }
        texture.SetWrap(TextureWrap::ClampToBorder, TextureWrap::ClampToBorder);
        if (!coherentBinding()) return Fail("border wrap setup desynchronized bindings");
        texture.SetBorderColor(-0.25f, 0.25f, 2.0f, 1.0f);
        if (!texture.GetLastError().empty() ||
            g_samplerStates[id].border != std::array<GLfloat, 4>{-0.25f, 0.25f, 2.0f, 1.0f} ||
            !coherentBinding())
        {
            return Fail("SetBorderColor lost finite components or wrote another texture");
        }
        const auto nan = (std::numeric_limits<f32>::quiet_NaN)();
        const auto infinity = (std::numeric_limits<f32>::infinity)();
        const std::function<void()> invalidRequests[] = {
            [&]() { texture.SetFilter(static_cast<TextureFilter>(999), TextureFilter::Linear); },
            [&]() { texture.SetFilter(TextureFilter::Linear, static_cast<TextureFilter>(999)); },
            [&]() { texture.SetWrap(static_cast<TextureWrap>(999), TextureWrap::Repeat); },
            [&]() { texture.SetWrap(TextureWrap::Repeat, static_cast<TextureWrap>(999)); },
            [&]() { texture.SetBorderColor(nan, 0.0f, 0.0f, 1.0f); },
            [&]() { texture.SetBorderColor(0.0f, 0.0f, infinity, 1.0f); },
            [&]() { texture.SetMaxAnisotropy(0.0f); },
            [&]() { texture.SetMaxAnisotropy(-1.0f); },
            [&]() { texture.SetMaxAnisotropy(nan); },
            [&]() { texture.SetMaxAnisotropy(infinity); }
        };
        for (const auto& invalid : invalidRequests)
        {
            const SamplerState before = g_samplerStates[id];
            const auto bindings = g_boundTextures;
            const auto active = g_activeTexture;
            const auto writes = g_samplerWrites;
            const auto binds = g_textureBinds.size();
            invalid();
            if (texture.GetLastError().empty() || !SameSampler(before, g_samplerStates[id]) ||
                g_samplerWrites != writes || g_textureBinds.size() != binds ||
                g_boundTextures != bindings || g_activeTexture != active)
            {
                return Fail("invalid sampler request mutated GL state or lacked a diagnostic");
            }
        }
        std::array<TextureSpecification, 7> invalidSpecifications;
        invalidSpecifications.fill(specification);
        invalidSpecifications[0].MinFilter = static_cast<TextureFilter>(999);
        invalidSpecifications[1].MagFilter = static_cast<TextureFilter>(999);
        invalidSpecifications[2].WrapS = static_cast<TextureWrap>(999);
        invalidSpecifications[3].WrapT = static_cast<TextureWrap>(999);
        invalidSpecifications[4].BorderColor[1] = nan;
        invalidSpecifications[5].MaxAnisotropy = infinity;
        invalidSpecifications[6].MaxAnisotropy = 0.5f;
        for (const auto& invalid : invalidSpecifications)
        {
            const auto generated = g_generatedTextureCount;
            OpenGLTexture2D rejected(invalid, nullptr);
            if (rejected.IsLoaded() || rejected.GetLastError().empty() ||
                g_generatedTextureCount != generated)
            {
                return Fail("invalid creation sampler reached GL allocation");
            }
        }
        texture.SetFilter(TextureFilter::NearestMipmapNearest, TextureFilter::Linear);
        if (!coherentBinding()) return Fail("rollback setup desynchronized bindings");
        const SamplerState committed = g_samplerStates[id];
        g_failSamplerWrite = g_samplerWrites + 3;
        texture.SetFilter(TextureFilter::LinearMipmapLinear, TextureFilter::Nearest);
        g_failSamplerWrite = -1;
        if (texture.GetLastError().empty() || !SameSampler(committed, g_samplerStates[id]) || !coherentBinding())
        {
            return Fail("partial sampler failure did not restore its previous complete profile");
        }
        texture.GenerateMipmaps();
        if (!texture.GetLastError().empty() || texture.GetMipLevels() != 3 ||
            g_samplerStates[id].minFilter != GL_NEAREST_MIPMAP_NEAREST || !coherentBinding())
        {
            return Fail("mipmap generation did not retain the pre-failure requested minification filter");
        }
        struct FilterCase { TextureFilter filter; GLint min; GLint mag; };
        const FilterCase filters[] = {
            {TextureFilter::None, GL_LINEAR, GL_LINEAR},
            {TextureFilter::Linear, GL_LINEAR, GL_LINEAR},
            {TextureFilter::Nearest, GL_NEAREST, GL_NEAREST},
            {TextureFilter::LinearMipmapLinear, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR},
            {TextureFilter::LinearMipmapNearest, GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
            {TextureFilter::NearestMipmapLinear, GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
            {TextureFilter::NearestMipmapNearest, GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST}
        };
        for (const auto& filter : filters)
        {
            texture.SetFilter(filter.filter, filter.filter);
            if (!texture.GetLastError().empty() || g_samplerStates[id].minFilter != filter.min ||
                g_samplerStates[id].magFilter != filter.mag || !coherentBinding())
            {
                return Fail("known filter or magnification alias mapped to the wrong GL filter");
            }
        }
        g_failEverySamplerWrite = true;
        texture.SetWrap(TextureWrap::Repeat, TextureWrap::Repeat);
        g_failEverySamplerWrite = false;
        if (texture.GetLastError().find("restor") == std::string::npos)
        {
            return Fail("failed sampler restoration did not report its recovery failure");
        }
        if (!coherentBinding()) return Fail("failed sampler restoration leaked active binding state");
        OpenGLTexture2D pendingMipmaps(specification, nullptr);
        if (!pendingMipmaps.IsLoaded()) return Fail("pending mipmap fixture did not load");
        g_failNextMipmap = true;
        pendingMipmaps.GenerateMipmaps();
        if (pendingMipmaps.GetLastError().empty() || pendingMipmaps.GetMipLevels() != 1 ||
            g_samplerStates[pendingMipmaps.GetRendererID()].minFilter != GL_NEAREST)
        {
            return Fail("failed mipmap generation committed a mip-dependent sampler or level count");
        }
        pendingMipmaps.GenerateMipmaps();
        if (!pendingMipmaps.GetLastError().empty() || pendingMipmaps.GetMipLevels() != 3 ||
            g_samplerStates[pendingMipmaps.GetRendererID()].minFilter != GL_NEAREST_MIPMAP_NEAREST)
        {
            return Fail("successful mipmap retry did not activate its retained requested filter");
        }
        return EXIT_SUCCESS;
    }
    int TestAnisotropyCapabilities()
    {
        using namespace Pyramid;
        SamplerDriverScope driverScope;
        struct CapabilityCase { GLint major; GLint minor; int arb; int ext; GLfloat maximum; bool supported; };
        const CapabilityCase capabilities[] = {
            {3, 3, 0, 0, 8.0f, false},
            {3, 3, 0, 1, 8.0f, true},
            {3, 3, 1, 0, 4.0f, true},
            {4, 6, 0, 0, 16.0f, true}
        };
        for (const auto& capability : capabilities)
        {
            g_driverMajor = capability.major;
            g_driverMinor = capability.minor;
            GLVersion.major = capability.major;
            GLVersion.minor = capability.minor;
            g_driverMaxAnisotropy = capability.maximum;
            GLAD_GL_ARB_texture_filter_anisotropic = capability.arb;
            GLAD_GL_EXT_texture_filter_anisotropic = capability.ext;
            TextureSpecification specification;
            specification.GenerateMips = false;
            specification.MaxAnisotropy = 64.0f;
            const auto beforeQueries = g_anisotropyQueries;
            const auto beforeWrites = g_anisotropyWrites;
            auto texture = ITexture2D::Create(specification, nullptr);
            if (!texture || !texture->IsLoaded() ||
                g_samplerStates[texture->GetRendererID()].anisotropy !=
                    (capability.supported ? capability.maximum : 1.0f))
            {
                return Fail("anisotropy creation did not honor core/ARB/EXT support and maximum clamping");
            }
            texture->SetMaxAnisotropy(2.0f);
            if (!texture->GetLastError().empty() || g_samplerStates[texture->GetRendererID()].anisotropy !=
                (capability.supported ? 2.0f : 1.0f))
            {
                return Fail("mutable anisotropy did not publish its supported value or fallback");
            }
            texture->SetMaxAnisotropy((std::numeric_limits<f32>::max)());
            if (!texture->GetLastError().empty() || g_samplerStates[texture->GetRendererID()].anisotropy !=
                (capability.supported ? capability.maximum : 1.0f))
            {
                return Fail("finite high anisotropy request did not clamp to the driver's maximum");
            }
            texture->SetMaxAnisotropy(1.0f);
            if (!texture->GetLastError().empty() || g_samplerStates[texture->GetRendererID()].anisotropy != 1.0f)
            {
                return Fail("anisotropy reset did not restore ordinary filtering");
            }
            if (capability.supported)
            {
                if (g_anisotropyQueries == beforeQueries || g_anisotropyWrites == beforeWrites)
                    return Fail("advertised anisotropy path never queried or set its supported GL state");
            }
            else if (g_anisotropyQueries != beforeQueries || g_anisotropyWrites != beforeWrites)
            {
                return Fail("unsupported anisotropy fallback issued extension-only GL queries or writes");
            }
        }
        return EXIT_SUCCESS;
    }

    int TestCachedSamplerImmutability()
    {
        using namespace Pyramid;
        SamplerDriverScope driverScope;
        g_driverMajor = 3;
        g_driverMinor = 3;
        g_driverMaxAnisotropy = 4.0f;
        GLAD_GL_ARB_texture_filter_anisotropic = 0;
        GLAD_GL_EXT_texture_filter_anisotropic = 1;
        OpenGLDevice device(nullptr);
        TextureSpecification specification;
        specification.Width = specification.Height = 2;
        specification.MinFilter = TextureFilter::NearestMipmapLinear;
        specification.MagFilter = TextureFilter::NearestMipmapNearest;
        specification.WrapS = TextureWrap::ClampToBorder;
        specification.WrapT = TextureWrap::MirroredRepeat;
        specification.BorderColor[0] = -0.25f;
        specification.BorderColor[1] = 0.25f;
        specification.BorderColor[2] = 2.0f;
        specification.BorderColor[3] = 1.0f;
        specification.MaxAnisotropy = 8.0f;
        const std::array<u8, 16> pixels{};
        auto direct = device.CreateTexture2D(specification, pixels.data());
        TextureResourceSpecification resourceSpecification;
        resourceSpecification.texture = specification;
        resourceSpecification.pixelData = pixels.data();
        resourceSpecification.pixelDataSize = pixels.size();
        TextureCache cache(device);
        auto cached = cache.GetOrCreate(resourceSpecification);
        if (!direct || !direct->IsLoaded() || !cached || !cached->IsLoaded() ||
            !SameSampler(g_samplerStates[direct->GetRendererID()], g_samplerStates[cached->GetRendererID()]) ||
            cached->GetSpecification().MaxAnisotropy != 8.0f ||
            g_samplerStates[cached->GetRendererID()].anisotropy != 4.0f)
        {
            return Fail("direct and cached creation used different native sampler contracts or mutated requested identity");
        }
        using CachedBackendPointer = std::remove_cv_t<std::remove_reference_t<decltype(cached->GetTexture())>>;
        static_assert(std::is_const_v<typename CachedBackendPointer::element_type>,
            "cached textures must expose read-only native backing resources");
        const auto content = cached->GetContentId();
        const auto asset = cached->GetAssetId();
        const auto id = cached->GetRendererID();
        const auto cachedSpecification = cached->GetSpecification();
        const SamplerState before = g_samplerStates[id];
        const auto writes = g_samplerWrites;
        const auto binds = g_textureBinds.size();
        const auto active = g_activeTexture;
        const std::function<void()> mutations[] = {
            [&]() { cached->SetFilter(TextureFilter::Nearest, TextureFilter::Linear); },
            [&]() { cached->SetWrap(TextureWrap::Repeat, TextureWrap::Repeat); },
            [&]() { cached->SetBorderColor(1.0f, 0.0f, 0.0f, 1.0f); },
            [&]() { cached->SetMaxAnisotropy(2.0f); },
            [&]() { cached->GenerateMipmaps(); }
        };
        for (const auto& mutate : mutations)
        {
            mutate();
            if (cached->GetLastError().empty() || cached->GetContentId() != content ||
                cached->GetAssetId() != asset || cached->GetRendererID() != id ||
                cached->GetSpecification().MinFilter != cachedSpecification.MinFilter ||
                cached->GetSpecification().MagFilter != cachedSpecification.MagFilter ||
                cached->GetSpecification().WrapS != cachedSpecification.WrapS ||
                cached->GetSpecification().WrapT != cachedSpecification.WrapT ||
                cached->GetSpecification().MaxAnisotropy != cachedSpecification.MaxAnisotropy ||
                !std::equal(std::begin(cachedSpecification.BorderColor), std::end(cachedSpecification.BorderColor),
                    std::begin(cached->GetSpecification().BorderColor)) ||
                !SameSampler(before, g_samplerStates[id]) || g_samplerWrites != writes ||
                g_textureBinds.size() != binds || g_activeTexture != active)
            {
                return Fail("cached immutable sampler mutated its identity, profile, native state or bindings");
            }
        }
        resourceSpecification.texture.MaxAnisotropy = 16.0f;
        auto differentRequest = cache.GetOrCreate(resourceSpecification);
        if (!differentRequest || differentRequest == cached || differentRequest->GetContentId() == content ||
            differentRequest->GetSpecification().MaxAnisotropy != 16.0f ||
            g_samplerStates[differentRequest->GetRendererID()].anisotropy != 4.0f)
        {
            return Fail("hardware clamping incorrectly merged distinct requested sampler identities");
        }
        resourceSpecification.texture.GenerateMips = false;
        resourceSpecification.texture.MinFilter = TextureFilter::NearestMipmapNearest;
        auto normalizedCache = cache.GetOrCreate(resourceSpecification);
        auto pendingDirect = device.CreateTexture2D(resourceSpecification.texture, pixels.data());
        if (!normalizedCache || !pendingDirect || !pendingDirect->IsLoaded() ||
            normalizedCache->GetSpecification().MinFilter != TextureFilter::Linear ||
            g_samplerStates[normalizedCache->GetRendererID()].minFilter != GL_LINEAR ||
            g_samplerStates[pendingDirect->GetRendererID()].minFilter != GL_NEAREST)
        {
            return Fail("cache ID normalization or direct pending mip-filter behavior changed");
        }
        return EXIT_SUCCESS;
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

    if (TestDeviceTextureBinding() != EXIT_SUCCESS || TestTextureStateCoherence() != EXIT_SUCCESS ||
        TestAdvertisedTextureFormats() != EXIT_SUCCESS || TestReloadTransferLayout() != EXIT_SUCCESS || TestMutableSamplerRegression() != EXIT_SUCCESS ||
        TestAnisotropyCapabilities() != EXIT_SUCCESS || TestCachedSamplerImmutability() != EXIT_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    std::cout << "Texture loading tests passed\n";
    return EXIT_SUCCESS;
}
