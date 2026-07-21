#pragma once
#include <Pyramid/Platform/Window.hpp>
#include <Windows.h>

namespace Pyramid
{

    /**
     * @brief Win32-specific OpenGL window implementation
     */
    class Win32OpenGLWindow : public Window
    {
    public:
        Win32OpenGLWindow();
        virtual ~Win32OpenGLWindow();

        bool Initialize(const char *title = "Pyramid Game", int width = 800, int height = 600) override;
        void Present(bool vsync) override;
        void MakeContextCurrent() override;
        bool ProcessMessages() override;
        void *GetHandle() const override { return m_hwnd; }
        int GetWidth() const override { return m_width; }
        int GetHeight() const override { return m_height; }
        bool ShouldClose() const override { return m_shouldClose; }
        void SetTitle(const char* title) override;
        void SetSize(int width, int height) override;
        void SetPosition(int x, int y) override;
        void SetVisible(bool visible) override;
        bool IsMinimized() const override;
        bool IsMaximized() const override;

    private:
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
        bool RegisterWindowClass();
        bool CreateOpenGLContext();
        void LogOpenGLContextInfo();

        HWND m_hwnd;
        HDC m_hdc;
        HGLRC m_hglrc;
        int m_width;
        int m_height;
        bool m_shouldClose;
        WindowResizeState m_resizeState;
    };

} // namespace Pyramid
