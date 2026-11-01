#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLIndexBuffer.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexArray.hpp>
#include <Pyramid/Graphics/OpenGL/Buffer/OpenGLVertexBuffer.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    using Pyramid::OpenGLStateManager;
    struct StencilFace
    {
        GLint function = GL_ALWAYS, reference = 0;
        GLuint mask = ~0u;
        GLint fail = GL_KEEP, depthFail = GL_KEEP, depthPass = GL_KEEP;
    };
    struct NativeState
    {
        GLuint program = 17, vao = 11, arrayBuffer = 71, copyWriteBuffer = 0;
        std::unordered_map<GLuint, GLuint> elements{{11, 81}, {22, 82}};
        GLenum activeTexture = GL_TEXTURE0;
        std::array<std::unordered_map<GLenum, GLuint>, 16> textures;
        GLuint drawFramebuffer = 41, readFramebuffer = 42;
        std::array<GLint, 4> viewport{5, 7, 800, 600}, scissor{2, 3, 40, 50};
        std::unordered_map<GLenum, bool> enabled{{GL_BLEND, true}, {GL_DEPTH_TEST, true},
            {GL_DEPTH_CLAMP, true}, {GL_STENCIL_TEST, true}, {GL_CULL_FACE, true}, {GL_SCISSOR_TEST, true}};
        GLenum srcRGB = GL_SRC_ALPHA, dstRGB = GL_ONE_MINUS_SRC_ALPHA, srcAlpha = GL_ONE, dstAlpha = GL_ONE;
        GLenum equationRGB = GL_FUNC_ADD, equationAlpha = GL_FUNC_REVERSE_SUBTRACT;
        GLenum depthFunction = GL_GREATER;
        GLboolean depthWrite = GL_FALSE;
        StencilFace front, back;
        GLenum cull = GL_FRONT, frontFace = GL_CW, polygon = GL_LINE;
        std::array<GLfloat, 4> clearColor{0.3f, 0.2f, 0.1f, 0.4f};
        GLdouble clearDepth = 0.25;
        GLint clearStencil = 7;
    } g_native;
    unsigned g_mutations = 0;
    GLuint g_nextBuffer = 100, g_nextVAO = 200;
    GLuint g_uploadedBuffer = 0;
    GLenum g_uploadTarget = 0;
    std::vector<unsigned char> g_upload;

    void Require(bool condition, const std::string& message)
    {
        if (!condition) throw std::runtime_error(message);
    }
    GLuint ElementBuffer() { return g_native.vao ? g_native.elements[g_native.vao] : 0; }
    bool& Capability(GLenum capability)
    {
        const auto found = g_native.enabled.find(capability);
        Require(found != g_native.enabled.end(), "Unexpected capability enum " + std::to_string(capability));
        return found->second;
    }
    void APIENTRY GetInteger(GLenum name, GLint* value)
    {
        switch (name)
        {
        case GL_CURRENT_PROGRAM: *value = static_cast<GLint>(g_native.program); return;
        case GL_VERTEX_ARRAY_BINDING: *value = static_cast<GLint>(g_native.vao); return;
        case GL_ARRAY_BUFFER_BINDING: *value = static_cast<GLint>(g_native.arrayBuffer); return;
        case GL_ELEMENT_ARRAY_BUFFER_BINDING: *value = static_cast<GLint>(ElementBuffer()); return;
        case GL_COPY_WRITE_BUFFER: *value = static_cast<GLint>(g_native.copyWriteBuffer); return;
        case GL_ACTIVE_TEXTURE: *value = static_cast<GLint>(g_native.activeTexture); return;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: *value = 16; return;
        case GL_TEXTURE_BINDING_2D: *value = static_cast<GLint>(g_native.textures[g_native.activeTexture - GL_TEXTURE0][GL_TEXTURE_2D]); return;
        case GL_TEXTURE_BINDING_CUBE_MAP: *value = static_cast<GLint>(g_native.textures[g_native.activeTexture - GL_TEXTURE0][GL_TEXTURE_CUBE_MAP]); return;
        case GL_DRAW_FRAMEBUFFER_BINDING: *value = static_cast<GLint>(g_native.drawFramebuffer); return;
        case GL_READ_FRAMEBUFFER_BINDING: *value = static_cast<GLint>(g_native.readFramebuffer); return;
        case GL_VIEWPORT: std::copy(g_native.viewport.begin(), g_native.viewport.end(), value); return;
        case GL_SCISSOR_BOX: std::copy(g_native.scissor.begin(), g_native.scissor.end(), value); return;
        case GL_BLEND_SRC_RGB: *value = static_cast<GLint>(g_native.srcRGB); return;
        case GL_BLEND_DST_RGB: *value = static_cast<GLint>(g_native.dstRGB); return;
        case GL_BLEND_SRC_ALPHA: *value = static_cast<GLint>(g_native.srcAlpha); return;
        case GL_BLEND_DST_ALPHA: *value = static_cast<GLint>(g_native.dstAlpha); return;
        case GL_BLEND_EQUATION_RGB: *value = static_cast<GLint>(g_native.equationRGB); return;
        case GL_BLEND_EQUATION_ALPHA: *value = static_cast<GLint>(g_native.equationAlpha); return;
        case GL_DEPTH_FUNC: *value = static_cast<GLint>(g_native.depthFunction); return;
        case GL_STENCIL_FUNC: *value = g_native.front.function; return;
        case GL_STENCIL_REF: *value = g_native.front.reference; return;
        case GL_STENCIL_VALUE_MASK: *value = static_cast<GLint>(g_native.front.mask); return;
        case GL_STENCIL_FAIL: *value = g_native.front.fail; return;
        case GL_STENCIL_PASS_DEPTH_FAIL: *value = g_native.front.depthFail; return;
        case GL_STENCIL_PASS_DEPTH_PASS: *value = g_native.front.depthPass; return;
        case GL_STENCIL_BACK_FUNC: *value = g_native.back.function; return;
        case GL_STENCIL_BACK_REF: *value = g_native.back.reference; return;
        case GL_STENCIL_BACK_VALUE_MASK: *value = static_cast<GLint>(g_native.back.mask); return;
        case GL_STENCIL_BACK_FAIL: *value = g_native.back.fail; return;
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL: *value = g_native.back.depthFail; return;
        case GL_STENCIL_BACK_PASS_DEPTH_PASS: *value = g_native.back.depthPass; return;
        case GL_CULL_FACE_MODE: *value = static_cast<GLint>(g_native.cull); return;
        case GL_FRONT_FACE: *value = static_cast<GLint>(g_native.frontFace); return;
        case GL_POLYGON_MODE: value[0] = value[1] = static_cast<GLint>(g_native.polygon); return;
        case GL_STENCIL_CLEAR_VALUE: *value = g_native.clearStencil; return;
        default: throw std::runtime_error("Unsupported fake GL integer query " + std::to_string(name));
        }
    }
    GLboolean APIENTRY IsEnabled(GLenum capability) { return Capability(capability) ? GL_TRUE : GL_FALSE; }
    void APIENTRY GetBoolean(GLenum name, GLboolean* value)
    {
        Require(name == GL_DEPTH_WRITEMASK, "Unexpected boolean query"); *value = g_native.depthWrite;
    }
    void APIENTRY GetFloat(GLenum name, GLfloat* value)
    {
        Require(name == GL_COLOR_CLEAR_VALUE, "Unexpected float query");
        std::copy(g_native.clearColor.begin(), g_native.clearColor.end(), value);
    }
    void APIENTRY GetDouble(GLenum name, GLdouble* value)
    {
        Require(name == GL_DEPTH_CLEAR_VALUE, "Unexpected double query"); *value = g_native.clearDepth;
    }
    void APIENTRY UseProgram(GLuint name) { ++g_mutations; g_native.program = name; }
    void APIENTRY BindVAO(GLuint name) { ++g_mutations; g_native.vao = name; }
    void APIENTRY BindBuffer(GLenum target, GLuint name)
    {
        ++g_mutations;
        if (target == GL_ARRAY_BUFFER) g_native.arrayBuffer = name;
        else if (target == GL_COPY_WRITE_BUFFER) g_native.copyWriteBuffer = name;
        else if (target == GL_ELEMENT_ARRAY_BUFFER)
        {
            Require(g_native.vao != 0, "Illegal element-buffer bind while VAO zero is active in core profile");
            g_native.elements[g_native.vao] = name;
        }
        else throw std::runtime_error("Unsupported fake buffer binding target");
    }
    void APIENTRY ActivateTexture(GLenum unit)
    {
        Require(unit >= GL_TEXTURE0 && unit < GL_TEXTURE0 + 16, "Out-of-range active texture unit");
        ++g_mutations; g_native.activeTexture = unit;
    }
    void APIENTRY BindTexture(GLenum target, GLuint name)
    {
        Require(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP, "Unexpected fake texture target");
        ++g_mutations; g_native.textures[g_native.activeTexture - GL_TEXTURE0][target] = name;
    }
    void APIENTRY BindFramebuffer(GLenum target, GLuint name)
    {
        Require(target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER,
            "Unexpected framebuffer target");
        ++g_mutations;
        if (target != GL_READ_FRAMEBUFFER) g_native.drawFramebuffer = name;
        if (target != GL_DRAW_FRAMEBUFFER) g_native.readFramebuffer = name;
    }
    void APIENTRY Viewport(GLint x, GLint y, GLsizei w, GLsizei h) { ++g_mutations; g_native.viewport = {x, y, w, h}; }
    void APIENTRY Enable(GLenum capability) { ++g_mutations; Capability(capability) = true; }
    void APIENTRY Disable(GLenum capability) { ++g_mutations; Capability(capability) = false; }
    void APIENTRY BlendFunc(GLenum source, GLenum destination)
    {
        ++g_mutations; g_native.srcRGB = g_native.srcAlpha = source; g_native.dstRGB = g_native.dstAlpha = destination;
    }
    void APIENTRY BlendFuncSeparate(GLenum sr, GLenum dr, GLenum sa, GLenum da)
    {
        ++g_mutations; g_native.srcRGB = sr; g_native.dstRGB = dr; g_native.srcAlpha = sa; g_native.dstAlpha = da;
    }
    void APIENTRY BlendEquation(GLenum mode) { ++g_mutations; g_native.equationRGB = g_native.equationAlpha = mode; }
    void APIENTRY DepthFunc(GLenum mode) { ++g_mutations; g_native.depthFunction = mode; }
    void APIENTRY DepthMask(GLboolean flag) { ++g_mutations; g_native.depthWrite = flag; }
    void APIENTRY StencilFunc(GLenum function, GLint reference, GLuint mask)
    {
        ++g_mutations;
        for (auto* face : {&g_native.front, &g_native.back}) { face->function = function; face->reference = reference; face->mask = mask; }
    }
    void APIENTRY StencilOp(GLenum fail, GLenum depthFail, GLenum depthPass)
    {
        ++g_mutations;
        for (auto* face : {&g_native.front, &g_native.back}) { face->fail = fail; face->depthFail = depthFail; face->depthPass = depthPass; }
    }
    void APIENTRY CullFace(GLenum mode) { ++g_mutations; g_native.cull = mode; }
    void APIENTRY FrontFace(GLenum mode) { ++g_mutations; g_native.frontFace = mode; }
    void APIENTRY PolygonMode(GLenum face, GLenum mode)
    {
        Require(face == GL_FRONT_AND_BACK, "Core polygon mode must affect both faces"); ++g_mutations; g_native.polygon = mode;
    }
    void APIENTRY Scissor(GLint x, GLint y, GLsizei w, GLsizei h) { ++g_mutations; g_native.scissor = {x, y, w, h}; }
    void APIENTRY ClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a) { ++g_mutations; g_native.clearColor = {r, g, b, a}; }
    void APIENTRY ClearDepth(GLdouble value) { ++g_mutations; g_native.clearDepth = value; }
    void APIENTRY ClearStencil(GLint value) { ++g_mutations; g_native.clearStencil = value; }
    void APIENTRY GenBuffers(GLsizei count, GLuint* names) { for (GLsizei i = 0; i < count; ++i) names[i] = g_nextBuffer++; }
    void APIENTRY DeleteBuffers(GLsizei count, const GLuint* names)
    {
        for (GLsizei i = 0; i < count; ++i)
        {
            if (g_native.arrayBuffer == names[i]) g_native.arrayBuffer = 0;
            if (g_native.copyWriteBuffer == names[i]) g_native.copyWriteBuffer = 0;
            if (g_native.vao && ElementBuffer() == names[i]) g_native.elements[g_native.vao] = 0;
        }
    }
    void APIENTRY GenVAOs(GLsizei count, GLuint* names)
    {
        for (GLsizei i = 0; i < count; ++i) { names[i] = g_nextVAO++; g_native.elements[names[i]] = 0; }
    }
    void APIENTRY DeleteVAOs(GLsizei count, const GLuint* names)
    {
        for (GLsizei i = 0; i < count; ++i)
        {
            if (g_native.vao == names[i]) g_native.vao = 0;
            g_native.elements.erase(names[i]);
        }
    }
    void APIENTRY BufferData(GLenum target, GLsizeiptr size, const void* data, GLenum)
    {
        g_uploadedBuffer = target == GL_COPY_WRITE_BUFFER ? g_native.copyWriteBuffer :
            target == GL_ARRAY_BUFFER ? g_native.arrayBuffer : ElementBuffer();
        Require(g_uploadedBuffer != 0 && size >= 0, "Buffer storage upload has no bound destination");
        g_uploadTarget = target;
        g_upload.resize(static_cast<std::size_t>(size));
        if (data && size) std::memcpy(g_upload.data(), data, static_cast<std::size_t>(size));
    }
    void InstallFakeGL()
    {
        glad_glGetIntegerv = GetInteger; glad_glIsEnabled = IsEnabled;
        glad_glGetBooleanv = GetBoolean; glad_glGetFloatv = GetFloat; glad_glGetDoublev = GetDouble;
        glad_glUseProgram = UseProgram; glad_glBindVertexArray = BindVAO; glad_glBindBuffer = BindBuffer;
        glad_glActiveTexture = ActivateTexture; glad_glBindTexture = BindTexture; glad_glBindFramebuffer = BindFramebuffer;
        glad_glViewport = Viewport; glad_glEnable = Enable; glad_glDisable = Disable;
        glad_glBlendFunc = BlendFunc; glad_glBlendFuncSeparate = BlendFuncSeparate; glad_glBlendEquation = BlendEquation;
        glad_glDepthFunc = DepthFunc; glad_glDepthMask = DepthMask; glad_glStencilFunc = StencilFunc; glad_glStencilOp = StencilOp;
        glad_glCullFace = CullFace; glad_glFrontFace = FrontFace; glad_glPolygonMode = PolygonMode;
        glad_glScissor = Scissor; glad_glClearColor = ClearColor; glad_glClearDepth = ClearDepth; glad_glClearStencil = ClearStencil;
        glad_glGenBuffers = GenBuffers; glad_glDeleteBuffers = DeleteBuffers; glad_glBufferData = BufferData;
        glad_glGenVertexArrays = GenVAOs; glad_glDeleteVertexArrays = DeleteVAOs;
    }
    template<class Action, class Check>
    void Transition(OpenGLStateManager& state, const char* label, Action action, Check check)
    {
        const unsigned before = g_mutations;
        action();
        Require(check(), std::string(label) + ": native state differs from requested state");
        Require(g_mutations > before, std::string(label) + ": required native update was suppressed");
        const unsigned after = g_mutations;
        action();
        Require(check() && g_mutations == after, std::string(label) + ": repeated state was not suppressed");
        Require(state.GetStateChangeCount() == g_mutations,
            std::string(label) + ": state-change counter does not count submitted native mutations");
    }
    void BeginCounters(OpenGLStateManager& state) { g_mutations = 0; state.ResetStateChangeCount(); }

    void TestInitializationAndTransitions(OpenGLStateManager& state)
    {
        GLint x, y, w, h;
        state.GetViewport(x, y, w, h);
        Require(state.GetCurrentProgram() == 17 && state.GetBoundBuffer(GL_ARRAY_BUFFER) == 71 &&
            state.GetBoundBuffer(GL_ELEMENT_ARRAY_BUFFER) == 81 &&
            state.GetBoundFramebuffer(GL_DRAW_FRAMEBUFFER) == 41 &&
            state.GetBoundFramebuffer(GL_READ_FRAMEBUFFER) == 42 &&
            std::array<GLint, 4>{x, y, w, h} == g_native.viewport,
            "Initial cache queries did not reflect preexisting native state");
        Require(g_mutations == 0, "Cache initialization mutated native state");
        BeginCounters(state);
        Transition(state, "RGB/alpha blend equation alias", [&] { state.SetBlendEquation(GL_FUNC_ADD); },
            [] { return g_native.equationRGB == GL_FUNC_ADD && g_native.equationAlpha == GL_FUNC_ADD; });
        Transition(state, "Front/back stencil function alias", [&] { state.SetStencilFunc(GL_ALWAYS, 0, ~0u); },
            [] { return g_native.front.function == GL_ALWAYS && g_native.back.function == GL_ALWAYS &&
                g_native.front.reference == 0 && g_native.back.reference == 0 && g_native.back.mask == ~0u; });
        Transition(state, "Front/back stencil operations alias", [&] { state.SetStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); },
            [] { return g_native.front.fail == GL_KEEP && g_native.back.fail == GL_KEEP &&
                g_native.back.depthFail == GL_KEEP && g_native.back.depthPass == GL_KEEP; });
        Transition(state, "Program", [&] { state.UseProgram(23); }, [] { return g_native.program == 23; });
        Transition(state, "VAO switch", [&] { state.BindVertexArray(22); }, [] { return g_native.vao == 22; });
        Require(state.GetBoundBuffer(GL_ELEMENT_ARRAY_BUFFER) == 82, "VAO switch retained the previous VAO element binding");
        Transition(state, "Element buffer", [&] { state.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 88); }, [] { return ElementBuffer() == 88; });
        state.BindVertexArray(11);
        Require(state.GetBoundBuffer(GL_ELEMENT_ARRAY_BUFFER) == 81, "Returning to a VAO did not restore its own element binding");
        Transition(state, "Same element name on another VAO", [&] { state.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 88); },
            [] { return ElementBuffer() == 88; });
        Require(g_native.elements[22] == 88, "Updating one VAO changed another VAO's element binding");
        Transition(state, "Array buffer", [&] { state.BindBuffer(GL_ARRAY_BUFFER, 44); }, [] { return g_native.arrayBuffer == 44; });
        Transition(state, "Active texture", [&] { state.ActiveTexture(GL_TEXTURE3); }, [] { return g_native.activeTexture == GL_TEXTURE3; });
        Transition(state, "Texture binding", [&] { state.BindTexture(GL_TEXTURE_2D, 50); },
            [] { return g_native.textures[3][GL_TEXTURE_2D] == 50; });
        Transition(state, "Texture unit binding", [&] { state.BindTextureUnit(4, GL_TEXTURE_CUBE_MAP, 60); },
            [] { return g_native.activeTexture == GL_TEXTURE4 && g_native.textures[4][GL_TEXTURE_CUBE_MAP] == 60; });
        Transition(state, "Read framebuffer", [&] { state.BindFramebuffer(GL_READ_FRAMEBUFFER, 61); },
            [] { return g_native.readFramebuffer == 61 && g_native.drawFramebuffer == 41; });
        Transition(state, "Combined framebuffer alias", [&] { state.BindFramebuffer(GL_FRAMEBUFFER, 62); },
            [] { return g_native.readFramebuffer == 62 && g_native.drawFramebuffer == 62; });
        Transition(state, "Draw framebuffer", [&] { state.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 63); },
            [] { return g_native.readFramebuffer == 62 && g_native.drawFramebuffer == 63; });
        Transition(state, "Viewport", [&] { state.SetViewport(3, 4, 320, 240); },
            [] { return g_native.viewport == std::array<GLint, 4>{3, 4, 320, 240}; });
        const GLenum capabilities[] = {GL_BLEND, GL_DEPTH_TEST, GL_DEPTH_CLAMP, GL_STENCIL_TEST, GL_CULL_FACE, GL_SCISSOR_TEST};
        for (const GLenum capability : capabilities)
        {
            const auto action = [&]()
            {
                switch (capability)
                {
                case GL_BLEND: state.EnableBlend(false); break;
                case GL_DEPTH_TEST: state.EnableDepthTest(false); break;
                case GL_DEPTH_CLAMP: state.EnableDepthClamp(false); break;
                case GL_STENCIL_TEST: state.EnableStencilTest(false); break;
                case GL_CULL_FACE: state.EnableCullFace(false); break;
                case GL_SCISSOR_TEST: state.EnableScissorTest(false); break;
                }
            };
            Transition(state, "Capability disable", action, [&] { return !Capability(capability); });
        }
        Transition(state, "Separate blend factors", [&] { state.SetBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ZERO, GL_ONE); },
            [] { return g_native.srcRGB == GL_ONE && g_native.dstRGB == GL_ZERO &&
                g_native.srcAlpha == GL_ZERO && g_native.dstAlpha == GL_ONE; });
        Transition(state, "Combined blend factors", [&] { state.SetBlendFunc(GL_ONE, GL_ZERO); },
            [] { return g_native.srcRGB == GL_ONE && g_native.srcAlpha == GL_ONE &&
                g_native.dstRGB == GL_ZERO && g_native.dstAlpha == GL_ZERO; });
        Transition(state, "Depth comparison", [&] { state.SetDepthFunc(GL_LEQUAL); }, [] { return g_native.depthFunction == GL_LEQUAL; });
        Transition(state, "Depth write", [&] { state.SetDepthMask(true); }, [] { return g_native.depthWrite == GL_TRUE; });
        Transition(state, "Stencil function", [&] { state.SetStencilFunc(GL_EQUAL, 5, 0x3f); },
            [] { return g_native.front.function == GL_EQUAL && g_native.back.function == GL_EQUAL &&
                g_native.front.reference == 5 && g_native.back.reference == 5 && g_native.front.mask == 0x3f && g_native.back.mask == 0x3f; });
        Transition(state, "Stencil operations", [&] { state.SetStencilOp(GL_REPLACE, GL_INCR, GL_DECR); },
            [] { return g_native.front.fail == GL_REPLACE && g_native.back.fail == GL_REPLACE &&
                g_native.front.depthFail == GL_INCR && g_native.back.depthFail == GL_INCR &&
                g_native.front.depthPass == GL_DECR && g_native.back.depthPass == GL_DECR; });
        Transition(state, "Cull mode", [&] { state.SetCullFace(GL_BACK); }, [] { return g_native.cull == GL_BACK; });
        Transition(state, "Front winding", [&] { state.SetFrontFace(GL_CCW); }, [] { return g_native.frontFace == GL_CCW; });
        Transition(state, "Polygon mode", [&] { state.SetPolygonMode(GL_FILL); }, [] { return g_native.polygon == GL_FILL; });
        Transition(state, "Scissor rectangle", [&] { state.SetScissor(7, 9, 12, 14); },
            [] { return g_native.scissor == std::array<GLint, 4>{7, 9, 12, 14}; });
        Transition(state, "Clear color", [&] { state.SetClearColor(1, 0, 1, 0); },
            [] { return g_native.clearColor == std::array<GLfloat, 4>{1, 0, 1, 0}; });
        Transition(state, "Clear depth", [&] { state.SetClearDepth(0.75); }, [] { return g_native.clearDepth == 0.75; });
        Transition(state, "Clear stencil", [&] { state.SetClearStencil(9); }, [] { return g_native.clearStencil == 9; });
    }

    template<class External, class Action, class Check>
    void AfterInvalidation(OpenGLStateManager& state, const char* label, External external, Action action, Check check)
    {
        external();
        state.InvalidateState();
        BeginCounters(state);
        Transition(state, label, action, check);
    }
    void TestRetainedReference(OpenGLStateManager& state)
    {
        AfterInvalidation(state, "Retained program after invalidation", [] { g_native.program = 90; },
            [&] { state.UseProgram(0); }, [] { return g_native.program == 0; });
        g_native.program = 91; state.InvalidateState();
        const unsigned beforeQuery = g_mutations;
        Require(state.GetCurrentProgram() == 91 && g_mutations == beforeQuery, "Retained program getter did not resynchronize without mutations");
        AfterInvalidation(state, "Retained VAO after invalidation", [] { g_native.vao = 22; },
            [&] { state.BindVertexArray(0); }, [] { return g_native.vao == 0; });
        AfterInvalidation(state, "Retained buffer after invalidation", [] { g_native.arrayBuffer = 90; },
            [&] { state.BindBuffer(GL_ARRAY_BUFFER, 0); }, [] { return g_native.arrayBuffer == 0; });
        AfterInvalidation(state, "Retained framebuffer after invalidation", [] { g_native.drawFramebuffer = 93; g_native.readFramebuffer = 94; },
            [&] { state.BindFramebuffer(GL_FRAMEBUFFER, 0); }, [] { return !g_native.drawFramebuffer && !g_native.readFramebuffer; });
        AfterInvalidation(state, "Retained viewport after invalidation", [] { g_native.viewport = {1, 2, 3, 4}; },
            [&] { state.SetViewport(3, 4, 320, 240); }, [] { return g_native.viewport == std::array<GLint, 4>{3, 4, 320, 240}; });
        AfterInvalidation(state, "Retained blending after invalidation", [] { Capability(GL_BLEND) = true; },
            [&] { state.EnableBlend(false); }, [] { return !Capability(GL_BLEND); });
        AfterInvalidation(state, "Retained depth after invalidation", [] { g_native.depthFunction = GL_GREATER; },
            [&] { state.SetDepthFunc(GL_LEQUAL); }, [] { return g_native.depthFunction == GL_LEQUAL; });
        AfterInvalidation(state, "Retained stencil after invalidation", [] { g_native.back.depthPass = GL_INVERT; },
            [&] { state.SetStencilOp(GL_REPLACE, GL_INCR, GL_DECR); }, [] { return g_native.back.depthPass == GL_DECR; });
        AfterInvalidation(state, "Retained culling after invalidation", [] { g_native.cull = GL_FRONT; },
            [&] { state.SetCullFace(GL_BACK); }, [] { return g_native.cull == GL_BACK; });
        AfterInvalidation(state, "Retained scissor after invalidation", [] { g_native.scissor = {1, 1, 1, 1}; },
            [&] { state.SetScissor(7, 9, 12, 14); }, [] { return g_native.scissor == std::array<GLint, 4>{7, 9, 12, 14}; });
        AfterInvalidation(state, "Retained clear values after invalidation", [] { g_native.clearDepth = 0.1; },
            [&] { state.SetClearDepth(0.75); }, [] { return g_native.clearDepth == 0.75; });
        g_native.arrayBuffer = 94; g_native.vao = 22; g_native.elements[22] = 95;
        g_native.drawFramebuffer = 96; g_native.readFramebuffer = 97; g_native.viewport = {8, 9, 10, 11};
        state.InvalidateState();
        GLint x, y, w, h;
        state.GetViewport(x, y, w, h);
        Require(state.GetBoundBuffer(GL_ARRAY_BUFFER) == 94 && state.GetBoundBuffer(GL_ELEMENT_ARRAY_BUFFER) == 95 &&
            state.GetBoundFramebuffer(GL_DRAW_FRAMEBUFFER) == 96 && state.GetBoundFramebuffer(GL_READ_FRAMEBUFFER) == 97 &&
            std::array<GLint, 4>{x, y, w, h} == g_native.viewport,
            "Retained binding/viewport getters returned stale state");
    }

    void TestBaseline(OpenGLStateManager& state)
    {
        g_native = NativeState{};
        g_native.back.function = GL_NOTEQUAL; g_native.back.reference = 3; g_native.back.mask = 7;
        g_native.front.fail = GL_REPLACE; g_native.back.depthPass = GL_INVERT;
        g_native.textures[0][GL_TEXTURE_2D] = 31; g_native.textures[0][GL_TEXTURE_CUBE_MAP] = 32;
        g_native.textures[3][GL_TEXTURE_2D] = 33; g_native.activeTexture = GL_TEXTURE3;
        const auto viewport = g_native.viewport, scissor = g_native.scissor;
        const auto inactiveVAOElements = g_native.elements[22];
        state.InvalidateState(); BeginCounters(state);
        state.ResetToDefaults();
        Require(g_native.program == 0 && g_native.vao == 0 && g_native.arrayBuffer == 0 &&
            g_native.drawFramebuffer == 0 && g_native.readFramebuffer == 0,
            "Baseline did not release tracked global bindings");
        Require(g_native.elements[22] == inactiveVAOElements, "Baseline changed an inactive VAO's element buffer");
        for (const auto& capability : g_native.enabled) Require(!capability.second, "Baseline retained an enabled tracked capability");
        Require(g_native.activeTexture == GL_TEXTURE0 && g_native.textures[0][GL_TEXTURE_2D] == 0 &&
            g_native.textures[0][GL_TEXTURE_CUBE_MAP] == 0 && g_native.textures[3][GL_TEXTURE_2D] == 33,
            "Baseline violated its unit-zero texture reset boundary");
        Require(g_native.viewport == viewport && g_native.scissor == scissor, "Baseline unexpectedly changed viewport/scissor dimensions");
        Require(g_native.srcRGB == GL_ONE && g_native.srcAlpha == GL_ONE && g_native.dstRGB == GL_ZERO && g_native.dstAlpha == GL_ZERO &&
            g_native.equationRGB == GL_FUNC_ADD && g_native.equationAlpha == GL_FUNC_ADD,
            "Baseline did not reset both blend channels");
        Require(g_native.depthFunction == GL_LESS && g_native.depthWrite == GL_TRUE &&
            g_native.cull == GL_BACK && g_native.frontFace == GL_CCW && g_native.polygon == GL_FILL,
            "Baseline retained dirty depth/cull/rasterization state");
        for (const auto* face : {&g_native.front, &g_native.back})
            Require(face->function == GL_ALWAYS && face->reference == 0 && face->mask == ~0u &&
                face->fail == GL_KEEP && face->depthFail == GL_KEEP && face->depthPass == GL_KEEP,
                "Baseline failed to reset both stencil faces and all stencil operations");
        Require(g_native.clearColor == std::array<GLfloat, 4>{0, 0, 0, 0} && g_native.clearDepth == 1.0 && g_native.clearStencil == 0,
            "Baseline retained dirty clear values");
        Require(state.GetStateChangeCount() == g_mutations, "Baseline state-change count differs from actual native mutations");
        const unsigned firstReset = g_mutations;
        state.ResetToDefaults();
        Require(g_mutations == firstReset, "Repeating baseline reset submitted redundant native mutations");
    }

    void TestOwnedWrappers(OpenGLStateManager& state)
    {
        using namespace Pyramid;
        state.BindVertexArray(0);
        const std::array<u32, 3> indices{0, 1, 2};
        const GLuint indexName = g_nextBuffer;
        auto index = std::make_shared<OpenGLIndexBuffer>();
        index->SetData(indices.data(), static_cast<u32>(indices.size()));
        Require(g_native.vao == 0 && index->GetCount() == 3 && g_uploadTarget == GL_COPY_WRITE_BUFFER &&
            g_uploadedBuffer == indexName && g_upload.size() == sizeof(indices) &&
            std::memcmp(g_upload.data(), indices.data(), sizeof(indices)) == 0,
            "Owned index upload depended on a VAO or wrote a different native buffer");
        {
            OpenGLVertexArray first;
            first.SetIndexBuffer(index);
            first.Bind();
            const GLuint firstName = g_native.vao;
            Require(firstName != 0 && ElementBuffer() == indexName && state.GetBoundBuffer(GL_ELEMENT_ARRAY_BUFFER) == indexName,
                "Owned VAO attachment/binding bypassed the element cache");
            OpenGLVertexArray second;
            second.Bind();
            Require(g_native.vao != firstName && ElementBuffer() == 0 && state.GetBoundBuffer(GL_ELEMENT_ARRAY_BUFFER) == 0,
                "Owned VAO switch inherited another object's element buffer");
            index->Bind();
            Require(ElementBuffer() == indexName, "Owned index bind was suppressed after switching VAOs");
            second.Unbind();
            Require(g_native.vao == 0, "Owned VAO unbind left a different native VAO active");
        }
        state.BindBuffer(GL_COPY_WRITE_BUFFER, 901);
        GLuint deletedBuffer = 0;
        {
            deletedBuffer = g_nextBuffer;
            OpenGLVertexBuffer buffer;
            buffer.Bind();
            Require(g_native.arrayBuffer == deletedBuffer, "Owned vertex buffer did not bind its native name");
        }
        Require(g_native.arrayBuffer == 0 && g_native.copyWriteBuffer == 901,
            "Owned buffer deletion changed unrelated bindings or retained a deleted native name");
        const GLuint nextBuffer = g_nextBuffer;
        g_nextBuffer = deletedBuffer;
        {
            OpenGLVertexBuffer reused;
            reused.Bind();
            Require(g_native.arrayBuffer == deletedBuffer && state.GetBoundBuffer(GL_ARRAY_BUFFER) == deletedBuffer,
                "Reused buffer name suppressed the required native bind");
        }
        g_nextBuffer = nextBuffer;
        GLuint deletedVAO = 0;
        {
            deletedVAO = g_nextVAO;
            OpenGLVertexArray vao;
            vao.Bind();
            Require(g_native.vao == deletedVAO, "Owned VAO bind was not observable in native state");
        }
        Require(g_native.vao == 0, "Owned VAO deletion did not model native automatic unbinding");
        const GLuint nextVAO = g_nextVAO;
        g_nextVAO = deletedVAO;
        {
            OpenGLVertexArray reused;
            reused.Bind();
            Require(g_native.vao == deletedVAO, "Reused VAO name suppressed the required native bind");
            state.BindVertexArray(0);
            Require(g_native.vao == 0, "State-manager reset failed after direct owned-VAO binding");
        }
        g_nextVAO = nextVAO;
    }
}

int main()
{
    try
    {
        g_native.back.function = GL_NOTEQUAL; g_native.back.reference = 3; g_native.back.mask = 7;
        g_native.back.fail = GL_REPLACE; g_native.back.depthFail = GL_INCR; g_native.back.depthPass = GL_DECR;
        InstallFakeGL();
        auto& state = OpenGLStateManager::GetInstance();
        TestInitializationAndTransitions(state);
        TestRetainedReference(state);
        TestBaseline(state);
        TestOwnedWrappers(state);
        std::cout << "OpenGL tracked-state transition tests passed (fake native state; no pixel/driver validation)\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "OpenGLStateTransitionTests failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}