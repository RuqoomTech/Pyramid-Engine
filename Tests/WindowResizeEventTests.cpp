#include <Pyramid/Platform/Window.hpp>

#include <iostream>
#include <utility>

namespace
{
    class TestWindow final : public Pyramid::Window
    {
    public:
        void Emit(const Pyramid::WindowResizeEvent& event)
        {
            DispatchResizeEvent(event);
        }

        bool Initialize(const char*, int, int) override { return true; }
        void Present(bool) override {}
        void MakeContextCurrent() override {}
        bool ProcessMessages() override { return true; }
        void* GetHandle() const override { return nullptr; }
        int GetWidth() const override { return 0; }
        int GetHeight() const override { return 0; }
        bool ShouldClose() const override { return false; }
        void SetTitle(const char*) override {}
        void SetSize(int, int) override {}
        void SetPosition(int, int) override {}
        void SetVisible(bool) override {}
        bool IsMinimized() const override { return false; }
        bool IsMaximized() const override { return false; }
    };

    bool Expect(bool condition, const char* message)
    {
        if (condition)
        {
            return true;
        }

        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using Pyramid::WindowResizeEvent;
    using Pyramid::WindowResizeState;

    bool passed = true;
    TestWindow window;

    int callbackCount = 0;
    WindowResizeEvent received;
    window.SetResizeCallback(
        [&](const WindowResizeEvent& event)
        {
            ++callbackCount;
            received = event;
        });

    window.Emit({1600, 900, WindowResizeState::Maximized});

    passed &= Expect(callbackCount == 1, "resize callback should run exactly once");
    passed &= Expect(received.width == 1600, "resize callback should preserve width");
    passed &= Expect(received.height == 900, "resize callback should preserve height");
    passed &= Expect(
        received.state == WindowResizeState::Maximized,
        "resize callback should preserve window state");
    passed &= Expect(received.HasRenderableArea(), "maximized non-zero window should be renderable");

    const WindowResizeEvent minimized{0, 0, WindowResizeState::Minimized};
    passed &= Expect(!minimized.HasRenderableArea(), "minimized zero-size window should not be renderable");

    bool replacementCalled = false;
    window.SetResizeCallback(
        [&](const WindowResizeEvent& event)
        {
            replacementCalled = event.width == 1024 && event.height == 768;
        });
    window.Emit({1024, 768, WindowResizeState::Restored});

    passed &= Expect(callbackCount == 1, "replacing a callback should detach the old callback");
    passed &= Expect(replacementCalled, "replacement callback should receive resize events");

    window.SetResizeCallback({});
    replacementCalled = false;
    window.Emit({800, 600, WindowResizeState::Restored});
    passed &= Expect(!replacementCalled, "empty callback should detach resize delivery");

    int selfDetachCount = 0;
    window.SetResizeCallback(
        [&](const WindowResizeEvent&)
        {
            ++selfDetachCount;
            window.SetResizeCallback({});
        });
    window.Emit({1280, 720, WindowResizeState::Restored});
    window.Emit({1366, 768, WindowResizeState::Restored});
    passed &= Expect(selfDetachCount == 1, "callback should be able to detach itself safely");

    return passed ? 0 : 1;
}
