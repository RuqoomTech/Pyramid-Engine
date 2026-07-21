#pragma once

#include <glad/glad.h>

#include <string>

namespace Pyramid::OpenGLDiagnostics
{
    /**
     * Enable OpenGL driver debug output for the current context when the
     * implementation exposes KHR_debug/core debug callbacks.
     *
     * Debug output is enabled only in non-NDEBUG builds. Calling this function
     * in a Release build is safe and returns false.
     */
    bool Initialize();

    /**
     * Detach the callback from the current context and disable debug output.
     */
    void Shutdown();

    /**
     * Return whether the diagnostics callback is currently attached.
     */
    bool IsEnabled();

    /**
     * Drain all pending OpenGL errors for the current context.
     */
    void ClearErrors();

    /**
     * Drain and report every pending OpenGL error.
     *
     * @param operation Human-readable operation that produced the error.
     * @param lastError Receives the combined diagnostic text when non-null.
     * @param logFailure Log the combined diagnostic through Pyramid::Util.
     * @return true when no OpenGL errors were pending.
     */
    bool CheckError(const char *operation, std::string *lastError = nullptr, bool logFailure = true);

    const char *GetErrorName(GLenum errorCode);
    const char *GetDebugSourceName(GLenum source);
    const char *GetDebugTypeName(GLenum type);
    const char *GetDebugSeverityName(GLenum severity);
} // namespace Pyramid::OpenGLDiagnostics
