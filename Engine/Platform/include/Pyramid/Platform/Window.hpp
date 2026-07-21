#pragma once

#include <Pyramid/Core/Prerequisites.hpp>

namespace Pyramid
{
    /**
     * @brief Platform-neutral window and graphics-context interface.
     */
    class Window
    {
    public:
        virtual ~Window() = default;

        virtual bool Initialize(const char* title = "Pyramid Game", int width = 800, int height = 600) = 0;
        virtual void Present(bool vsync) = 0;
        virtual void MakeContextCurrent() = 0;
        virtual bool ProcessMessages() = 0;

        virtual void* GetHandle() const = 0;
        virtual int GetWidth() const = 0;
        virtual int GetHeight() const = 0;

        virtual bool ShouldClose() const = 0;
        virtual void SetTitle(const char* title) = 0;
        virtual void SetSize(int width, int height) = 0;
        virtual void SetPosition(int x, int y) = 0;
        virtual void SetVisible(bool visible) = 0;
        virtual bool IsMinimized() const = 0;
        virtual bool IsMaximized() const = 0;
    };
} // namespace Pyramid
