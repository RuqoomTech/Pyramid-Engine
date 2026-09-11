#pragma once
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <glad/glad.h>
#include <string>
#include <unordered_map>

namespace Pyramid
{

    /**
     * @brief OpenGL implementation of shader program
     */
    class OpenGLShader : public IShader
    {
    public:
        OpenGLShader();
        ~OpenGLShader() override;

        void Bind() override;
        void Unbind() override;
        bool Compile(const std::string &vertexSrc, const std::string &fragmentSrc) override;
        bool CompileWithGeometry(const std::string &vertexSrc, const std::string &geometrySrc, const std::string &fragmentSrc) override;
        bool CompileWithTessellation(const std::string &vertexSrc, const std::string &tessControlSrc,
                                     const std::string &tessEvalSrc, const std::string &fragmentSrc) override;
        bool CompileAdvanced(const std::string &vertexSrc, const std::string &tessControlSrc,
                             const std::string &tessEvalSrc, const std::string &geometrySrc,
                              const std::string &fragmentSrc) override;

        // Uniform setters
        void SetUniformInt(const std::string &name, int value) override;
        void SetUniformFloat(const std::string &name, float value) override;
        void SetUniformFloat2(const std::string &name, float v0, float v1) override;
        void SetUniformFloat3(const std::string &name, float v0, float v1, float v2) override;
        void SetUniformFloat4(const std::string &name, float v0, float v1, float v2, float v3) override;
        void SetUniformMat3(const std::string &name, const float *matrix_ptr, bool transpose = false, int count = 1) override;
        void SetUniformMat4(const std::string &name, const float *matrix_ptr, bool transpose = false, int count = 1) override;

        // Uniform buffer methods
        void BindUniformBuffer(const std::string &blockName, class IUniformBuffer *buffer, u32 bindingPoint) override;
        void SetUniformBlockBinding(const std::string &blockName, u32 bindingPoint) override;

        // Shader storage buffer methods
        void BindShaderStorageBuffer(const std::string &blockName, class IShaderStorageBuffer *buffer, u32 bindingPoint) override;
        void SetShaderStorageBlockBinding(const std::string &blockName, u32 bindingPoint) override;

    private:
        GLint GetUniformLocation(const std::string &name); // Not const as it modifies cache

        // Helper methods for shader compilation
        GLuint CompileShader(GLenum type, const std::string &source);
        bool LinkProgram(GLuint program);
        void DeleteShader(GLuint shader);

        GLuint m_programId;
        std::unordered_map<std::string, GLint> m_uniformLocationCache;
    };

} // namespace Pyramid
