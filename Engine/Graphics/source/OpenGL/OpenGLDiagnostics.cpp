#include "OpenGLDiagnostics.hpp"

#include <Pyramid/Util/Log.hpp>

#include <sstream>

namespace Pyramid::OpenGLDiagnostics
{
    namespace
    {
        bool g_debugOutputEnabled = false;

        Util::LogLevel GetLogLevel(GLenum severity)
        {
            switch (severity)
            {
            case GL_DEBUG_SEVERITY_HIGH:
                return Util::LogLevel::Critical;
            case GL_DEBUG_SEVERITY_MEDIUM:
                return Util::LogLevel::Error;
            case GL_DEBUG_SEVERITY_LOW:
                return Util::LogLevel::Warn;
            case GL_DEBUG_SEVERITY_NOTIFICATION:
                return Util::LogLevel::Debug;
            default:
                return Util::LogLevel::Warn;
            }
        }

        void APIENTRY DebugMessageCallback(
            GLenum source,
            GLenum type,
            GLuint id,
            GLenum severity,
            GLsizei length,
            const GLchar *message,
            const void *)
        {
            const std::string messageText = message
                ? std::string(message, length > 0 ? static_cast<std::size_t>(length) : std::char_traits<char>::length(message))
                : std::string("No driver message");

            std::ostringstream stream;
            stream << "OpenGL driver message [id=" << id
                   << ", source=" << GetDebugSourceName(source)
                   << ", type=" << GetDebugTypeName(type)
                   << ", severity=" << GetDebugSeverityName(severity)
                   << "]: " << messageText;

            Util::Logger::GetInstance().Log(
                GetLogLevel(severity),
                stream.str(),
                Util::SourceLocation{"OpenGL", "DebugMessageCallback", 0});
        }
    } // namespace

    bool Initialize()
    {
#ifdef NDEBUG
        return false;
#else
        if (g_debugOutputEnabled)
        {
            return true;
        }

        if (!glDebugMessageCallback)
        {
            PYRAMID_LOG_WARN("OpenGL debug output is unavailable on this driver/context");
            return false;
        }

        ClearErrors();

        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugMessageCallback, nullptr);

        // Driver notifications are usually high-volume status messages. Keep
        // warnings and errors enabled while suppressing notification noise.
        if (glDebugMessageControl)
        {
            glDebugMessageControl(
                GL_DONT_CARE,
                GL_DONT_CARE,
                GL_DEBUG_SEVERITY_NOTIFICATION,
                0,
                nullptr,
                GL_FALSE);
        }

        std::string error;
        if (!CheckError("OpenGLDiagnostics::Initialize", &error, false))
        {
            glDebugMessageCallback(nullptr, nullptr);
            glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDisable(GL_DEBUG_OUTPUT);
            PYRAMID_LOG_WARN("Failed to enable OpenGL debug output: ", error);
            return false;
        }

        g_debugOutputEnabled = true;
        PYRAMID_LOG_INFO("OpenGL debug callback enabled");
        return true;
#endif
    }

    void Shutdown()
    {
#ifndef NDEBUG
        if (!g_debugOutputEnabled)
        {
            return;
        }

        if (glDebugMessageCallback)
        {
            glDebugMessageCallback(nullptr, nullptr);
        }

        glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDisable(GL_DEBUG_OUTPUT);
        ClearErrors();
        g_debugOutputEnabled = false;
#endif
    }

    bool IsEnabled()
    {
        return g_debugOutputEnabled;
    }

    void ClearErrors()
    {
        while (glGetError() != GL_NO_ERROR)
        {
        }
    }

    bool CheckError(const char *operation, std::string *lastError, bool logFailure)
    {
        std::ostringstream stream;
        bool hasError = false;
        GLenum errorCode = GL_NO_ERROR;

        while ((errorCode = glGetError()) != GL_NO_ERROR)
        {
            if (!hasError)
            {
                stream << (operation ? operation : "OpenGL operation") << " failed";
            }

            stream << (hasError ? ", " : ": ")
                   << GetErrorName(errorCode)
                   << " (0x" << std::hex << errorCode << std::dec << ')';
            hasError = true;
        }

        if (!hasError)
        {
            if (lastError)
            {
                lastError->clear();
            }
            return true;
        }

        const std::string diagnostic = stream.str();
        if (lastError)
        {
            *lastError = diagnostic;
        }

        if (logFailure)
        {
            PYRAMID_LOG_ERROR(diagnostic);
        }

        return false;
    }

    const char *GetErrorName(GLenum errorCode)
    {
        switch (errorCode)
        {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_STACK_OVERFLOW:
            return "GL_STACK_OVERFLOW";
        case GL_STACK_UNDERFLOW:
            return "GL_STACK_UNDERFLOW";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default:
            return "GL_UNKNOWN_ERROR";
        }
    }

    const char *GetDebugSourceName(GLenum source)
    {
        switch (source)
        {
        case GL_DEBUG_SOURCE_API:
            return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            return "window-system";
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            return "shader-compiler";
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            return "third-party";
        case GL_DEBUG_SOURCE_APPLICATION:
            return "application";
        case GL_DEBUG_SOURCE_OTHER:
            return "other";
        default:
            return "unknown";
        }
    }

    const char *GetDebugTypeName(GLenum type)
    {
        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR:
            return "error";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            return "deprecated-behavior";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            return "undefined-behavior";
        case GL_DEBUG_TYPE_PORTABILITY:
            return "portability";
        case GL_DEBUG_TYPE_PERFORMANCE:
            return "performance";
        case GL_DEBUG_TYPE_MARKER:
            return "marker";
        case GL_DEBUG_TYPE_PUSH_GROUP:
            return "push-group";
        case GL_DEBUG_TYPE_POP_GROUP:
            return "pop-group";
        case GL_DEBUG_TYPE_OTHER:
            return "other";
        default:
            return "unknown";
        }
    }

    const char *GetDebugSeverityName(GLenum severity)
    {
        switch (severity)
        {
        case GL_DEBUG_SEVERITY_HIGH:
            return "high";
        case GL_DEBUG_SEVERITY_MEDIUM:
            return "medium";
        case GL_DEBUG_SEVERITY_LOW:
            return "low";
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "notification";
        default:
            return "unknown";
        }
    }
} // namespace Pyramid::OpenGLDiagnostics
