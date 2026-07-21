#include <Pyramid/Platform/Windows/Win32OpenGLWindow.hpp>
#include <Pyramid/Util/Log.hpp> // For logging OpenGL context creation
#include <glad/glad.h>
#include <glad/glad_wgl.h>
#include <string> // For strlen
#include <vector> // For dynamic buffer for wide string conversion

// Ensure <windows.h> is available for MultiByteToWideChar,
// It's usually pulled in by glad_wgl.h or other Windows-specific headers.
// If not, it would need to be explicitly included, but that's unlikely here.

namespace Pyramid
{

    LRESULT CALLBACK Win32OpenGLWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        Win32OpenGLWindow *window = nullptr;
        if (msg == WM_CREATE)
        {
            CREATESTRUCT *createStruct = reinterpret_cast<CREATESTRUCT *>(lParam);
            window = reinterpret_cast<Win32OpenGLWindow *>(createStruct->lpCreateParams);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        }
        else
        {
            window = reinterpret_cast<Win32OpenGLWindow *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        }

        switch (msg)
        {
        case WM_CLOSE:
            if (window)
                window->m_shouldClose = true;
            return 0;

        case WM_SIZE:
            if (window)
            {
                window->m_width = static_cast<int>(LOWORD(lParam));
                window->m_height = static_cast<int>(HIWORD(lParam));
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    Win32OpenGLWindow::Win32OpenGLWindow()
        : m_hwnd(nullptr), m_hdc(nullptr), m_hglrc(nullptr), m_width(800), m_height(600), m_shouldClose(false)
    {
    }

    Win32OpenGLWindow::~Win32OpenGLWindow()
    {
        if (m_hglrc)
        {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(m_hglrc);
        }

        if (m_hdc)
            ReleaseDC(m_hwnd, m_hdc);

        if (m_hwnd)
            DestroyWindow(m_hwnd);
    }

    bool Win32OpenGLWindow::Initialize(const char *title, int width, int height)
    {
        PYRAMID_LOG_INFO("Initializing Win32 OpenGL window (", width, "x", height, ")...");
        
        m_width = width;
        m_height = height;

        if (!RegisterWindowClass())
        {
            PYRAMID_LOG_ERROR("Failed to register window class");
            return false;
        }
        
        PYRAMID_LOG_INFO("Window class registered successfully");

        // Create the window
        RECT windowRect = {0, 0, width, height};
        AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

        // Convert title to wide string
        std::wstring wTitle;
        if (title)
        {
            int titleLen = static_cast<int>(strlen(title));
            if (titleLen > 0)
            {
                int wideLen = MultiByteToWideChar(CP_UTF8, 0, title, titleLen, nullptr, 0);
                if (wideLen > 0)
                {
                    std::vector<wchar_t> wideBuf(wideLen);
                    MultiByteToWideChar(CP_UTF8, 0, title, titleLen, wideBuf.data(), wideLen);
                    wTitle.assign(wideBuf.data(), wideLen);
                }
            }
        }
        if (wTitle.empty())
        {
            wTitle = L"Pyramid Game"; // Default title if conversion fails or input is null/empty
        }

        m_hwnd = CreateWindowExW(
            0,                                  // Extended style
            L"PyramidWindowClass",              // Class name
            wTitle.c_str(),                     // Window title (Changed)
            WS_OVERLAPPEDWINDOW,                // Style
            CW_USEDEFAULT,                      // X position
            CW_USEDEFAULT,                      // Y position
            windowRect.right - windowRect.left, // Width
            windowRect.bottom - windowRect.top, // Height
            nullptr,                            // Parent window
            nullptr,                            // Menu
            GetModuleHandle(nullptr),           // Instance handle
            this                                // Additional application data
        );

        if (!m_hwnd)
        {
            PYRAMID_LOG_ERROR("Failed to create window");
            return false;
        }
        
        PYRAMID_LOG_INFO("Window created successfully");

        // Get the device context
        m_hdc = GetDC(m_hwnd);
        if (!m_hdc)
        {
            PYRAMID_LOG_ERROR("Failed to get device context");
            return false;
        }
        
        PYRAMID_LOG_INFO("Device context obtained successfully");

        // Create OpenGL context
        PYRAMID_LOG_INFO("Creating OpenGL context...");
        if (!CreateOpenGLContext())
        {
            PYRAMID_LOG_ERROR("Failed to create OpenGL context");
            return false;
        }
        
        PYRAMID_LOG_INFO("OpenGL context created successfully");

        // Show the window
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        return true;
    }

    bool Win32OpenGLWindow::RegisterWindowClass()
    {
        // Check if the class is already registered
        WNDCLASSEXW existingClass = {};
        if (GetClassInfoExW(GetModuleHandle(nullptr), L"PyramidWindowClass", &existingClass))
        {
            PYRAMID_LOG_INFO("Window class already registered, reusing existing class");
            return true; // Class already exists, no need to register again
        }
        
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
        wc.lpfnWndProc = Win32OpenGLWindow::WndProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = L"PyramidWindowClass";

        ATOM result = RegisterClassExW(&wc);
        if (result == 0)
        {
            DWORD error = GetLastError();
            if (error == ERROR_CLASS_ALREADY_EXISTS)
            {
                PYRAMID_LOG_INFO("Window class already exists (ERROR_CLASS_ALREADY_EXISTS), continuing...");
                return true; // This is actually fine
            }
            else
            {
                PYRAMID_LOG_ERROR("Failed to register window class, error code: ", error);
                return false;
            }
        }
        
        PYRAMID_LOG_INFO("Window class registered successfully");
        return true;
    }

    bool Win32OpenGLWindow::CreateOpenGLContext()
    {
        // First create a temporary context to get WGL extensions
        PIXELFORMATDESCRIPTOR pfd = {
            sizeof(PIXELFORMATDESCRIPTOR), // Size of this struct
            1,                             // Version
            PFD_DRAW_TO_WINDOW |           // Support window
                PFD_SUPPORT_OPENGL |       // Support OpenGL
                PFD_DOUBLEBUFFER,          // Double buffered
            PFD_TYPE_RGBA,                 // RGBA type
            32,                            // 32-bit color depth
            0, 0, 0, 0, 0, 0,              // Color bits ignored
            0,                             // No alpha buffer
            0,                             // Shift bit ignored
            0,                             // No accumulation buffer
            0, 0, 0, 0,                    // Accumulation bits ignored
            24,                            // 24-bit z-buffer
            8,                             // 8-bit stencil buffer
            0,                             // No auxiliary buffer
            PFD_MAIN_PLANE,                // Main drawing layer
            0,                             // Reserved
            0, 0, 0                        // Layer masks ignored
        };

        int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
        if (!pixelFormat)
            return false;

        if (!SetPixelFormat(m_hdc, pixelFormat, &pfd))
            return false;

        // Create temporary context
        HGLRC tempContext = wglCreateContext(m_hdc);
        if (!tempContext)
            return false;

        if (!wglMakeCurrent(m_hdc, tempContext))
        {
            wglDeleteContext(tempContext);
            return false;
        }

        // Initialize GLAD to get WGL extensions
        if (!gladLoadWGL(m_hdc))
        {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(tempContext);
            return false;
        }

        // Now create the actual OpenGL 4.6 context using WGL_ARB_create_context
        if (wglCreateContextAttribsARB)
        {
            // Try OpenGL 4.6 first, then fallback to lower versions
            const int versions[][2] = {
                {4, 6}, {4, 5}, {4, 4}, {4, 3}, {4, 2}, {4, 1}, {4, 0}, {3, 3}};

            for (const auto &version : versions)
            {
                const int contextAttribs[] = {
                    WGL_CONTEXT_MAJOR_VERSION_ARB, version[0],
                    WGL_CONTEXT_MINOR_VERSION_ARB, version[1],
                    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
                    0};

                m_hglrc = wglCreateContextAttribsARB(m_hdc, nullptr, contextAttribs);
                if (m_hglrc)
                {
                    // Success! Clean up temporary context
                    wglMakeCurrent(nullptr, nullptr);
                    wglDeleteContext(tempContext);

                    // Make the new context current
                    if (!wglMakeCurrent(m_hdc, m_hglrc))
                    {
                        wglDeleteContext(m_hglrc);
                        m_hglrc = nullptr;
                        return false;
                    }

                    // Initialize GLAD with the new context
                    if (!gladLoadGL())
                    {
                        wglMakeCurrent(nullptr, nullptr);
                        wglDeleteContext(m_hglrc);
                        m_hglrc = nullptr;
                        return false;
                    }

                    // Enhanced OpenGL context information logging
                    LogOpenGLContextInfo();

                    return true;
                }
            }
        }

        PYRAMID_LOG_ERROR("OpenGL 3.3 core or newer is required");
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(tempContext);
        return false;
    }

    void Win32OpenGLWindow::Present(bool vsync)
    {
        if (wglSwapIntervalEXT)
            wglSwapIntervalEXT(vsync ? 1 : 0);

        if (m_hdc)
            SwapBuffers(m_hdc);
    }

    void Win32OpenGLWindow::MakeContextCurrent()
    {
        if (m_hdc && m_hglrc)
            wglMakeCurrent(m_hdc, m_hglrc);
    }

    bool Win32OpenGLWindow::ProcessMessages()
    {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return false;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        return !m_shouldClose;
    }

    void Win32OpenGLWindow::SetTitle(const char* title)
    {
        if (!m_hwnd || !title)
            return;

        const int length = static_cast<int>(strlen(title));
        if (length <= 0)
            return;

        const int wideLength = MultiByteToWideChar(CP_UTF8, 0, title, length, nullptr, 0);
        if (wideLength <= 0)
            return;

        std::vector<wchar_t> buffer(static_cast<size_t>(wideLength) + 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, title, length, buffer.data(), wideLength);
        SetWindowTextW(m_hwnd, buffer.data());
    }

    void Win32OpenGLWindow::SetSize(int width, int height)
    {
        if (!m_hwnd || width <= 0 || height <= 0)
            return;

        RECT rect = {0, 0, width, height};
        const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_STYLE));
        AdjustWindowRect(&rect, style, FALSE);
        SetWindowPos(
            m_hwnd,
            nullptr,
            0,
            0,
            rect.right - rect.left,
            rect.bottom - rect.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void Win32OpenGLWindow::SetPosition(int x, int y)
    {
        if (m_hwnd)
            SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void Win32OpenGLWindow::SetVisible(bool visible)
    {
        if (m_hwnd)
            ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
    }

    bool Win32OpenGLWindow::IsMinimized() const
    {
        return m_hwnd && IsIconic(m_hwnd) != FALSE;
    }

    bool Win32OpenGLWindow::IsMaximized() const
    {
        return m_hwnd && IsZoomed(m_hwnd) != FALSE;
    }

    void Win32OpenGLWindow::LogOpenGLContextInfo()
    {
        // Basic OpenGL information
        const char *version_str = reinterpret_cast<const char *>(glGetString(GL_VERSION));
        const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
        const char *vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));
        const char *glsl_version = reinterpret_cast<const char *>(glGetString(GL_SHADING_LANGUAGE_VERSION));

        PYRAMID_LOG_INFO("=== OpenGL Context Information ===");
        PYRAMID_LOG_INFO("OpenGL Version: ", version_str ? version_str : "Unknown");
        PYRAMID_LOG_INFO("Renderer: ", renderer ? renderer : "Unknown");
        PYRAMID_LOG_INFO("Vendor: ", vendor ? vendor : "Unknown");
        PYRAMID_LOG_INFO("GLSL Version: ", glsl_version ? glsl_version : "Unknown");

        // Get OpenGL version numbers
        GLint major, minor;
        glGetIntegerv(GL_MAJOR_VERSION, &major);
        glGetIntegerv(GL_MINOR_VERSION, &minor);
        PYRAMID_LOG_INFO("OpenGL Version (parsed): ", major, ".", minor);

        // Check for key OpenGL 4.6 features
        if (major >= 4)
        {
            PYRAMID_LOG_INFO("=== OpenGL 4.x Features Available ===");

            // Check for specific features that are important for our engine
            if (major > 4 || (major == 4 && minor >= 6))
            {
                PYRAMID_LOG_INFO("✅ OpenGL 4.6 features available");
                PYRAMID_LOG_INFO("  - SPIR-V shader support");
                PYRAMID_LOG_INFO("  - Anisotropic filtering");
                PYRAMID_LOG_INFO("  - Polygon offset clamp");
            }

            if (major > 4 || (major == 4 && minor >= 5))
            {
                PYRAMID_LOG_INFO("✅ OpenGL 4.5 features available");
                PYRAMID_LOG_INFO("  - Direct State Access (DSA)");
                PYRAMID_LOG_INFO("  - Clip control");
            }

            if (major > 4 || (major == 4 && minor >= 4))
            {
                PYRAMID_LOG_INFO("✅ OpenGL 4.4 features available");
                PYRAMID_LOG_INFO("  - Buffer storage");
                PYRAMID_LOG_INFO("  - Multi-bind");
            }

            if (major > 4 || (major == 4 && minor >= 3))
            {
                PYRAMID_LOG_INFO("✅ OpenGL 4.3 features available");
                PYRAMID_LOG_INFO("  - Compute shaders");
                PYRAMID_LOG_INFO("  - Shader storage buffer objects");
                PYRAMID_LOG_INFO("  - Multi-draw indirect");
            }
        }

        // Check hardware limits important for our engine
        GLint max_texture_size, max_texture_units, max_uniform_buffer_bindings;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
        glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &max_uniform_buffer_bindings);

        PYRAMID_LOG_INFO("=== Hardware Limits ===");
        PYRAMID_LOG_INFO("Max Texture Size: ", max_texture_size, "x", max_texture_size);
        PYRAMID_LOG_INFO("Max Texture Units: ", max_texture_units);
        PYRAMID_LOG_INFO("Max UBO Bindings: ", max_uniform_buffer_bindings);

        PYRAMID_LOG_INFO("===================================");
    }

} // namespace Pyramid
