#pragma once
#include <Pyramid/Core/Prerequisites.hpp>
#include <glad/glad.h>
#include <unordered_map>
#include <array>

namespace Pyramid
{

    /**
     * @brief OpenGL state management for performance optimization
     * Caches OpenGL state to minimize redundant API calls
     */
    class OpenGLStateManager
    {
    public:
        static OpenGLStateManager& GetInstance();

        // Shader state
        void UseProgram(GLuint program);
        GLuint GetCurrentProgram() const { return m_currentProgram; }

        // Buffer binding state
        void BindVertexArray(GLuint vao);
        void BindBuffer(GLenum target, GLuint buffer);
        GLuint GetBoundBuffer(GLenum target) const;

        // Texture state
        void ActiveTexture(GLenum texture);
        void BindTexture(GLenum target, GLuint texture);
        void BindTextureUnit(u32 unit, GLenum target, GLuint texture);
        /** Forget a deleted name on all units/targets without changing GL or unrelated cache entries. */
        void ForgetTexture(GLuint texture);

        // Framebuffer state
        void BindFramebuffer(GLenum target, GLuint framebuffer);
        GLuint GetBoundFramebuffer(GLenum target) const;

        // Viewport state
        void SetViewport(i32 x, i32 y, i32 width, i32 height);
        void GetViewport(i32& x, i32& y, i32& width, i32& height) const;

        // Blend state
        void EnableBlend(bool enable);
        void SetBlendFunc(GLenum sfactor, GLenum dfactor);
        void SetBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha);
        void SetBlendEquation(GLenum mode);

        // Depth state
        void EnableDepthTest(bool enable);
        void EnableDepthClamp(bool enable);
        void SetDepthFunc(GLenum func);
        void SetDepthMask(bool flag);

        // Stencil state
        void EnableStencilTest(bool enable);
        void SetStencilFunc(GLenum func, i32 ref, u32 mask);
        void SetStencilOp(GLenum fail, GLenum zfail, GLenum zpass);

        // Culling state
        void EnableCullFace(bool enable);
        void SetCullFace(GLenum mode);
        void SetFrontFace(GLenum mode);
        void SetPolygonMode(GLenum mode);

        // Scissor state
        void EnableScissorTest(bool enable);
        void SetScissor(i32 x, i32 y, i32 width, i32 height);

        // Clear state
        void SetClearColor(f32 r, f32 g, f32 b, f32 a);
        void SetClearDepth(f64 depth);
        void SetClearStencil(i32 stencil);

        // Utility functions
        void InvalidateState(); // Force refresh of all cached state
        void ResetToDefaults(); // Reset to OpenGL default state
        u32 GetStateChangeCount() const { return m_stateChangeCount; }
        void ResetStateChangeCount() { m_stateChangeCount = 0; }

    private:
        OpenGLStateManager() = default;
        ~OpenGLStateManager() = default;
        OpenGLStateManager(const OpenGLStateManager&) = delete;
        OpenGLStateManager& operator=(const OpenGLStateManager&) = delete;

        void InitializeState();

        // Shader state
        GLuint m_currentProgram = 0;

        // Buffer state
        GLuint m_currentVAO = 0;
        std::unordered_map<GLenum, GLuint> m_boundBuffers;

        // Texture state
        GLenum m_activeTexture = GL_TEXTURE0;
        static constexpr u32 MAX_TEXTURE_UNITS = 32;
        u32 m_runtimeTextureUnitCount = MAX_TEXTURE_UNITS;
        std::array<std::unordered_map<GLenum, GLuint>, MAX_TEXTURE_UNITS> m_boundTextures;

        // Framebuffer state
        std::unordered_map<GLenum, GLuint> m_boundFramebuffers;

        // Viewport state
        struct ViewportState
        {
            i32 x = 0, y = 0, width = 0, height = 0;
        } m_viewport;

        // Blend state
        struct BlendState
        {
            bool enabled = false;
            GLenum srcRGB = GL_ONE, dstRGB = GL_ZERO;
            GLenum srcAlpha = GL_ONE, dstAlpha = GL_ZERO;
            GLenum equation = GL_FUNC_ADD;
        } m_blendState;

        // Depth state
        struct DepthState
        {
            bool testEnabled = false;
            bool clampEnabled = false;
            GLenum func = GL_LESS;
            bool maskEnabled = true;
        } m_depthState;

        // Stencil state
        struct StencilState
        {
            bool testEnabled = false;
            GLenum func = GL_ALWAYS;
            i32 ref = 0;
            u32 mask = 0xFFFFFFFF;
            GLenum fail = GL_KEEP, zfail = GL_KEEP, zpass = GL_KEEP;
        } m_stencilState;

        // Culling state
        struct CullState
        {
            bool enabled = false;
            GLenum mode = GL_BACK;
            GLenum frontFace = GL_CCW;
            GLenum polygonMode = GL_FILL;
        } m_cullState;

        // Scissor state
        struct ScissorState
        {
            bool enabled = false;
            i32 x = 0, y = 0, width = 0, height = 0;
        } m_scissorState;

        // Clear state
        struct ClearState
        {
            f32 colorR = 0.0f, colorG = 0.0f, colorB = 0.0f, colorA = 0.0f;
            f64 depth = 1.0;
            i32 stencil = 0;
        } m_clearState;

        // Statistics
        mutable u32 m_stateChangeCount = 0;
        bool m_initialized = false;
    };

} // namespace Pyramid
