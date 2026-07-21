#include "OpenGL/OpenGLDiagnostics.hpp"

#include <cstring>
#include <iostream>

namespace
{
    bool ExpectEqual(const char *actual, const char *expected, const char *label)
    {
        if (std::strcmp(actual, expected) == 0)
        {
            return true;
        }

        std::cerr << label << ": expected '" << expected << "', got '" << actual << "'\n";
        return false;
    }
}

int main()
{
    using namespace Pyramid::OpenGLDiagnostics;

    bool passed = true;
    passed &= ExpectEqual(GetErrorName(GL_INVALID_ENUM), "GL_INVALID_ENUM", "error name");
    passed &= ExpectEqual(GetErrorName(0xFFFFFFFFu), "GL_UNKNOWN_ERROR", "unknown error name");
    passed &= ExpectEqual(GetDebugSourceName(GL_DEBUG_SOURCE_SHADER_COMPILER), "shader-compiler", "debug source");
    passed &= ExpectEqual(GetDebugTypeName(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR), "undefined-behavior", "debug type");
    passed &= ExpectEqual(GetDebugSeverityName(GL_DEBUG_SEVERITY_HIGH), "high", "debug severity");
    passed &= ExpectEqual(GetDebugSeverityName(0xFFFFFFFFu), "unknown", "unknown severity");

    return passed ? 0 : 1;
}
