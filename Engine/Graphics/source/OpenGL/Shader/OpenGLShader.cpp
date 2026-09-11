#include <Pyramid/Graphics/OpenGL/Shader/OpenGLShader.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLStateManager.hpp>
#include <Pyramid/Graphics/Buffer/UniformBuffer.hpp>
#include <Pyramid/Graphics/Buffer/ShaderStorageBuffer.hpp>
#include <Pyramid/Util/Log.hpp> // Updated to use Utils logging system
#include <vector>
// #include <iostream> // No longer needed directly if using Log macros

namespace Pyramid
{

    OpenGLShader::OpenGLShader()
        : m_programId(0)
    {
    }

    OpenGLShader::~OpenGLShader()
    {
        if (m_programId)
            glDeleteProgram(m_programId);
    }

    void OpenGLShader::Bind()
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
    }

    void OpenGLShader::Unbind()
    {
        OpenGLStateManager::GetInstance().UseProgram(0);
    }

    bool OpenGLShader::Compile(const std::string &vertexSrc, const std::string &fragmentSrc)
    {
        // Create shaders
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);

        // Compile vertex shader
        const char *source = vertexSrc.c_str();
        glShaderSource(vertexShader, 1, &source, nullptr);
        glCompileShader(vertexShader);

        // Check vertex shader compilation
        GLint isCompiled = 0;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
        if (!isCompiled)
        {
            GLint maxLength = 0;
            glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);

            glDeleteShader(vertexShader);
            PYRAMID_LOG_ERROR("Vertex shader compilation failed:\n", infoLog.data()); // Changed
            return false;
        }

        // Compile fragment shader
        source = fragmentSrc.c_str();
        glShaderSource(fragmentShader, 1, &source, nullptr);
        glCompileShader(fragmentShader);

        // Check fragment shader compilation
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
        if (!isCompiled)
        {
            GLint maxLength = 0;
            glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);

            glDeleteShader(fragmentShader);
            glDeleteShader(vertexShader);
            PYRAMID_LOG_ERROR("Fragment shader compilation failed:\n", infoLog.data()); // Changed
            return false;
        }

        // Create program and link shaders
        m_programId = glCreateProgram();
        glAttachShader(m_programId, vertexShader);
        glAttachShader(m_programId, fragmentShader);
        glLinkProgram(m_programId);

        // Check program linking
        GLint isLinked = 0;
        glGetProgramiv(m_programId, GL_LINK_STATUS, &isLinked);
        if (!isLinked)
        {
            GLint maxLength = 0;
            glGetProgramiv(m_programId, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(m_programId, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(m_programId);
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            PYRAMID_LOG_ERROR("Shader program linking failed:\n", infoLog.data()); // Changed
            return false;
        }

        // Cleanup
        glDetachShader(m_programId, vertexShader);
        glDetachShader(m_programId, fragmentShader);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        return true;
    }

    GLuint OpenGLShader::CompileShader(GLenum type, const std::string &source)
    {
        GLuint shader = glCreateShader(type);
        const char *src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        // Check compilation status
        GLint isCompiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
        if (!isCompiled)
        {
            GLint maxLength = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

            glDeleteShader(shader);

            const char *shaderTypeName = "Unknown";
            switch (type)
            {
            case GL_VERTEX_SHADER:
                shaderTypeName = "Vertex";
                break;
            case GL_FRAGMENT_SHADER:
                shaderTypeName = "Fragment";
                break;
            case GL_GEOMETRY_SHADER:
                shaderTypeName = "Geometry";
                break;
            case GL_TESS_CONTROL_SHADER:
                shaderTypeName = "Tessellation Control";
                break;
            case GL_TESS_EVALUATION_SHADER:
                shaderTypeName = "Tessellation Evaluation";
                break;
            case GL_COMPUTE_SHADER:
                shaderTypeName = "Compute";
                break;
            }

            PYRAMID_LOG_ERROR(shaderTypeName, " shader compilation failed:\n", infoLog.data());
            return 0;
        }

        return shader;
    }

    bool OpenGLShader::LinkProgram(GLuint program)
    {
        glLinkProgram(program);

        // Check linking status
        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
        if (!isLinked)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

            PYRAMID_LOG_ERROR("Shader program linking failed:\n", infoLog.data());
            return false;
        }

        return true;
    }

    void OpenGLShader::DeleteShader(GLuint shader)
    {
        if (shader != 0)
        {
            glDeleteShader(shader);
        }
    }

    GLint OpenGLShader::GetUniformLocation(const std::string &name)
    {
        if (m_uniformLocationCache.find(name) != m_uniformLocationCache.end())
            return m_uniformLocationCache[name];

        GLint location = glGetUniformLocation(m_programId, name.c_str());
        if (location == -1)
            PYRAMID_LOG_WARN("Uniform '", name, "' not found in shader program ", m_programId); // Changed

        m_uniformLocationCache[name] = location;
        return location;
    }

    void OpenGLShader::SetUniformInt(const std::string &name, int value)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniform1i(location, value);
    }

    void OpenGLShader::SetUniformFloat(const std::string &name, float value)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniform1f(location, value);
    }

    void OpenGLShader::SetUniformFloat2(const std::string &name, float v0, float v1)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniform2f(location, v0, v1);
    }

    void OpenGLShader::SetUniformFloat3(const std::string &name, float v0, float v1, float v2)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniform3f(location, v0, v1, v2);
    }

    void OpenGLShader::SetUniformFloat4(const std::string &name, float v0, float v1, float v2, float v3)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniform4f(location, v0, v1, v2, v3);
    }

    void OpenGLShader::SetUniformMat3(const std::string &name, const float *matrix_ptr, bool transpose, int count)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniformMatrix3fv(location, count, transpose ? GL_TRUE : GL_FALSE, matrix_ptr);
    }

    void OpenGLShader::SetUniformMat4(const std::string &name, const float *matrix_ptr, bool transpose, int count)
    {
        OpenGLStateManager::GetInstance().UseProgram(m_programId);
        GLint location = GetUniformLocation(name);
        if (location != -1)
            glUniformMatrix4fv(location, count, transpose ? GL_TRUE : GL_FALSE, matrix_ptr);
    }

    void OpenGLShader::BindUniformBuffer(const std::string &blockName, IUniformBuffer *buffer, u32 bindingPoint)
    {
        if (!buffer)
        {
            PYRAMID_LOG_ERROR("Uniform buffer is null");
            return;
        }

        // Set the uniform block binding point
        SetUniformBlockBinding(blockName, bindingPoint);

        // Bind the buffer to the binding point
        buffer->Bind(bindingPoint);
    }

    void OpenGLShader::SetUniformBlockBinding(const std::string &blockName, u32 bindingPoint)
    {
        if (m_programId == 0)
        {
            PYRAMID_LOG_ERROR("Shader program not compiled");
            return;
        }

        // Get the uniform block index
        GLuint blockIndex = glGetUniformBlockIndex(m_programId, blockName.c_str());
        if (blockIndex == GL_INVALID_INDEX)
        {
            PYRAMID_LOG_WARN("Uniform block '", blockName, "' not found in shader program ", m_programId);
            return;
        }

        // Set the binding point for the uniform block
        glUniformBlockBinding(m_programId, blockIndex, bindingPoint);

        PYRAMID_LOG_INFO("Bound uniform block '", blockName, "' to binding point ", bindingPoint);
    }

    bool OpenGLShader::CompileWithGeometry(const std::string &vertexSrc, const std::string &geometrySrc, const std::string &fragmentSrc)
    {
        // Delete existing program if it exists
        if (m_programId != 0)
        {
            glDeleteProgram(m_programId);
            m_uniformLocationCache.clear();
        }

        // Compile shaders
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
        if (vertexShader == 0)
            return false;

        GLuint geometryShader = CompileShader(GL_GEOMETRY_SHADER, geometrySrc);
        if (geometryShader == 0)
        {
            DeleteShader(vertexShader);
            return false;
        }

        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        if (fragmentShader == 0)
        {
            DeleteShader(vertexShader);
            DeleteShader(geometryShader);
            return false;
        }

        // Create and link program
        m_programId = glCreateProgram();
        glAttachShader(m_programId, vertexShader);
        glAttachShader(m_programId, geometryShader);
        glAttachShader(m_programId, fragmentShader);

        bool linkSuccess = LinkProgram(m_programId);

        // Cleanup shaders
        glDetachShader(m_programId, vertexShader);
        glDetachShader(m_programId, geometryShader);
        glDetachShader(m_programId, fragmentShader);
        DeleteShader(vertexShader);
        DeleteShader(geometryShader);
        DeleteShader(fragmentShader);

        if (!linkSuccess)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
            return false;
        }

        PYRAMID_LOG_INFO("Shader program with geometry shader compiled successfully");
        return true;
    }

    bool OpenGLShader::CompileWithTessellation(const std::string &vertexSrc, const std::string &tessControlSrc,
                                               const std::string &tessEvalSrc, const std::string &fragmentSrc)
    {
        // Delete existing program if it exists
        if (m_programId != 0)
        {
            glDeleteProgram(m_programId);
            m_uniformLocationCache.clear();
        }

        // Compile shaders
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
        if (vertexShader == 0)
            return false;

        GLuint tessControlShader = CompileShader(GL_TESS_CONTROL_SHADER, tessControlSrc);
        if (tessControlShader == 0)
        {
            DeleteShader(vertexShader);
            return false;
        }

        GLuint tessEvalShader = CompileShader(GL_TESS_EVALUATION_SHADER, tessEvalSrc);
        if (tessEvalShader == 0)
        {
            DeleteShader(vertexShader);
            DeleteShader(tessControlShader);
            return false;
        }

        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        if (fragmentShader == 0)
        {
            DeleteShader(vertexShader);
            DeleteShader(tessControlShader);
            DeleteShader(tessEvalShader);
            return false;
        }

        // Create and link program
        m_programId = glCreateProgram();
        glAttachShader(m_programId, vertexShader);
        glAttachShader(m_programId, tessControlShader);
        glAttachShader(m_programId, tessEvalShader);
        glAttachShader(m_programId, fragmentShader);

        bool linkSuccess = LinkProgram(m_programId);

        // Cleanup shaders
        glDetachShader(m_programId, vertexShader);
        glDetachShader(m_programId, tessControlShader);
        glDetachShader(m_programId, tessEvalShader);
        glDetachShader(m_programId, fragmentShader);
        DeleteShader(vertexShader);
        DeleteShader(tessControlShader);
        DeleteShader(tessEvalShader);
        DeleteShader(fragmentShader);

        if (!linkSuccess)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
            return false;
        }

        PYRAMID_LOG_INFO("Shader program with tessellation shaders compiled successfully");
        return true;
    }

    bool OpenGLShader::CompileAdvanced(const std::string &vertexSrc, const std::string &tessControlSrc,
                                       const std::string &tessEvalSrc, const std::string &geometrySrc,
                                       const std::string &fragmentSrc)
    {
        // Delete existing program if it exists
        if (m_programId != 0)
        {
            glDeleteProgram(m_programId);
            m_uniformLocationCache.clear();
        }

        std::vector<GLuint> shaders;

        // Compile vertex shader (required)
        GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
        if (vertexShader == 0)
            return false;
        shaders.push_back(vertexShader);

        // Compile tessellation control shader (optional)
        GLuint tessControlShader = 0;
        if (!tessControlSrc.empty())
        {
            tessControlShader = CompileShader(GL_TESS_CONTROL_SHADER, tessControlSrc);
            if (tessControlShader == 0)
            {
                for (GLuint shader : shaders)
                    DeleteShader(shader);
                return false;
            }
            shaders.push_back(tessControlShader);
        }

        // Compile tessellation evaluation shader (optional)
        GLuint tessEvalShader = 0;
        if (!tessEvalSrc.empty())
        {
            tessEvalShader = CompileShader(GL_TESS_EVALUATION_SHADER, tessEvalSrc);
            if (tessEvalShader == 0)
            {
                for (GLuint shader : shaders)
                    DeleteShader(shader);
                return false;
            }
            shaders.push_back(tessEvalShader);
        }

        // Compile geometry shader (optional)
        GLuint geometryShader = 0;
        if (!geometrySrc.empty())
        {
            geometryShader = CompileShader(GL_GEOMETRY_SHADER, geometrySrc);
            if (geometryShader == 0)
            {
                for (GLuint shader : shaders)
                    DeleteShader(shader);
                return false;
            }
            shaders.push_back(geometryShader);
        }

        // Compile fragment shader (required)
        GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
        if (fragmentShader == 0)
        {
            for (GLuint shader : shaders)
                DeleteShader(shader);
            return false;
        }
        shaders.push_back(fragmentShader);

        // Create and link program
        m_programId = glCreateProgram();
        for (GLuint shader : shaders)
        {
            glAttachShader(m_programId, shader);
        }

        bool linkSuccess = LinkProgram(m_programId);

        // Cleanup shaders
        for (GLuint shader : shaders)
        {
            glDetachShader(m_programId, shader);
            DeleteShader(shader);
        }

        if (!linkSuccess)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
            return false;
        }

        PYRAMID_LOG_INFO("Advanced shader program compiled successfully with ", shaders.size(), " stages");
        return true;
    }

    void OpenGLShader::BindShaderStorageBuffer(const std::string &blockName, IShaderStorageBuffer *buffer, u32 bindingPoint)
    {
        if (!buffer)
        {
            PYRAMID_LOG_ERROR("Shader storage buffer is null");
            return;
        }

        // Set the storage block binding point
        SetShaderStorageBlockBinding(blockName, bindingPoint);

        // Bind the buffer to the binding point
        buffer->Bind(bindingPoint);
    }

    void OpenGLShader::SetShaderStorageBlockBinding(const std::string &blockName, u32 bindingPoint)
    {
        if (m_programId == 0)
        {
            PYRAMID_LOG_ERROR("Cannot set storage block binding: no program loaded");
            return;
        }

        // Get the storage block index
        GLuint blockIndex = glGetProgramResourceIndex(m_programId, GL_SHADER_STORAGE_BLOCK, blockName.c_str());
        if (blockIndex == GL_INVALID_INDEX)
        {
            PYRAMID_LOG_ERROR("Shader storage block '", blockName, "' not found in program ", m_programId);
            return;
        }

        // Set the binding point for the storage block
        glShaderStorageBlockBinding(m_programId, blockIndex, bindingPoint);

        PYRAMID_LOG_INFO("Bound shader storage block '", blockName, "' to binding point ", bindingPoint);
    }

} // namespace Pyramid
