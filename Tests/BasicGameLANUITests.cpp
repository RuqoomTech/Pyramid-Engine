#include "BasicGameLANScreen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
    using namespace Pyramid;
    using namespace Pyramid::UI;
    using Pyramid::Examples::BasicGameUI::LANMenuScreen;

    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "BasicGame LAN UI test failed: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    bool SameColor(const Color& left, const Color& right)
    {
        return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
    }

    std::vector<Rect> ButtonRects(const Context& ui, std::size_t count = 3)
    {
        std::vector<Rect> rectangles;
        const auto& vertices = ui.GetDrawList().GetVertices();
        for (std::size_t index = 0; index + 3 < vertices.size(); index += 4)
        {
            if (!SameColor(vertices[index].color, ui.GetTheme().background))
            {
                continue;
            }
            f32 minX = vertices[index].position.x;
            f32 maxX = minX;
            f32 minY = vertices[index].position.y;
            f32 maxY = minY;
            for (std::size_t corner = 1; corner < 4; ++corner)
            {
                minX = (std::min)(minX, vertices[index + corner].position.x);
                maxX = (std::max)(maxX, vertices[index + corner].position.x);
                minY = (std::min)(minY, vertices[index + corner].position.y);
                maxY = (std::max)(maxY, vertices[index + corner].position.y);
            }
            if (maxX - minX > 100.0f && maxY - minY >= 18.0f && maxY - minY < 80.0f)
            {
                rectangles.push_back({minX, minY, maxX - minX, maxY - minY});
            }
        }
        std::sort(rectangles.begin(), rectangles.end(), [](const Rect& left, const Rect& right)
        {
            return left.y < right.y;
        });
        Require(rectangles.size() >= count, "screen did not emit all action buttons");
        return {rectangles.end() - static_cast<std::ptrdiff_t>(count), rectangles.end()};
    }
}

void ConfigureUI(Context& ui)
{
    Text::FontAtlas font;
    std::string fontError;
    Require(Text::LoadFontAtlas(PYRAMID_BASIC_GAME_UI_TEST_FONT, font, &fontError) &&
            ui.SetFontAtlas(font), "production fallback font fixture failed");
    Theme theme = ui.GetTheme();
    theme.textScale = 20.0f / font.pixelHeight;
    theme.defaultRowHeight = 30.0f;
    theme.spacing = 8.0f;
    theme.padding = 10.0f;
    theme.background = Color(0.011f, 0.021f, 0.031f, 1.0f);
    ui.SetTheme(theme);
}

void TestLANControls(f32 width, f32 height)
{
    Context ui;
    ConfigureUI(ui);
    InputState input;
    input.SetFocused(true);
    ScreenStack stack;
    unsigned hostCalls = 0;
    unsigned joinCalls = 0;
    unsigned backCalls = 0;
    unsigned statusCalls = 0;
    auto screen = std::make_shared<LANMenuScreen>(
        [&](const std::string& port, std::string& error)
        {
            ++hostCalls;
            Require(port == "7777", "host action changed the default port");
            error = "TCP address must be numeric IPv4; DNS and IPv6 are unsupported.";
            return false;
        },
        [&](const std::string& address, const std::string& port, std::string& error)
        {
            ++joinCalls;
            Require(address == "127.0.0.1" && port == "7777", "join action changed endpoint input");
            error.clear();
            return true;
        },
        [&]()
        {
            ++backCalls;
            Require(stack.Pop(), "LAN back action could not pop its modal screen");
        },
        [&]()
        {
            ++statusCalls;
            return std::string("Match ended: TCP address must be numeric IPv4; DNS and IPv6 are unsupported.");
        });
    Require(stack.Push(screen) && stack.BlocksGameplayInput() && stack.GetTopName() == "LAN",
        "LAN controls must be a modal shared-runtime screen");
    const FrameInfo frame{width, height, 1.0f, 1.0f / 60.0f};
    const auto build = [&]()
    {
        Require(ui.BeginFrame(frame, input), "LAN UI frame was rejected");
        stack.Build(ui);
        (void)ui.EndFrame();
    };
    const auto click = [&](std::size_t action)
    {
        input.BeginFrame();
        input.ProcessMouseButton(MouseButton::Left, false);
        input.ProcessMouseMove(-10.0f, -10.0f);
        build();
        const auto rectangles = ButtonRects(ui);
        input.BeginFrame();
        input.ProcessMouseMove(rectangles[action].x + rectangles[action].width * 0.5f,
            rectangles[action].y + rectangles[action].height * 0.5f);
        input.ProcessMouseButton(MouseButton::Left, true);
        build();
    };
    build();
    Require(!ui.GetDrawList().Empty() && statusCalls > 0, "LAN status/controls were not built");
    click(0);
    Require(hostCalls == 1 && joinCalls == 0 && backCalls == 0 && stack.GetTopName() == "LAN",
        "failed host callback must keep the LAN menu open");
    input.BeginFrame();
    input.ProcessMouseButton(MouseButton::Left, false);
    build();
    bool errorFeedback = false;
    for (const auto& vertex : ui.GetDrawList().GetVertices())
    {
        errorFeedback = errorFeedback || SameColor(vertex.color, Color(1.0f, 0.38f, 0.38f, 1.0f));
    }
    Require(errorFeedback, "LAN callback failure was not published as UI feedback");
    Require(ui.PrepareInput(input).HasAnyMouseConsumption(), "LAN modal leaked pointer input to gameplay");
    click(1);
    Require(joinCalls == 1 && hostCalls == 1, "join button did not dispatch exactly once");
    click(2);
    Require(backCalls == 1 && stack.Empty(), "LAN back callback did not safely transition during Build");

}
void TestMainControls(f32 width, f32 height)
{
    Context ui;
    ConfigureUI(ui);
    InputState input;
    input.SetFocused(true);
    ScreenStack stack;
    std::array<unsigned, 4> calls{};
    using Pyramid::Examples::BasicGameUI::MainMenuScreen;
    auto screen = std::make_shared<MainMenuScreen>(
        [&]() { ++calls[0]; }, [&]() { ++calls[1]; },
        [&]() { ++calls[2]; }, [&]() { ++calls[3]; },
        []()
        {
            return std::string("Match ended: TCP address must be numeric IPv4; DNS and IPv6 are unsupported. ") +
                "This long failure status must not displace the menu actions.";
        });
    Require(stack.Push(screen), "main menu push failed");
    const FrameInfo frame{width, height, 1.0f, 1.0f / 60.0f};
    const auto build = [&]()
    {
        Require(ui.BeginFrame(frame, input), "main menu frame rejected");
        stack.Build(ui);
        (void)ui.EndFrame();
    };
    for (std::size_t action = 0; action < calls.size(); ++action)
    {
        input.BeginFrame();
        input.ProcessMouseButton(MouseButton::Left, false);
        input.ProcessMouseMove(-10.0f, -10.0f);
        build();
        const auto rectangles = ButtonRects(ui, 4);
        input.BeginFrame();
        input.ProcessMouseMove(rectangles[action].x + rectangles[action].width * 0.5f,
            rectangles[action].y + rectangles[action].height * 0.5f);
        input.ProcessMouseButton(MouseButton::Left, true);
        build();
        Require(calls[action] == 1, "main menu action is clipped or did not dispatch once");
    }
}

int main()
{
    TestLANControls(1280.0f, 720.0f);
    TestLANControls(640.0f, 480.0f);
    TestMainControls(1280.0f, 720.0f);
    TestMainControls(640.0f, 480.0f);
    std::cout << "BasicGame production-font menu tests passed\n";
    return EXIT_SUCCESS;
}
