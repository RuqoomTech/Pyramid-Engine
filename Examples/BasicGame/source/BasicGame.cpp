#include "BasicGame.hpp"

#include <Pyramid/Graphics/Buffer/BufferLayout.hpp>
#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Graphics/Shader/Shader.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <Pyramid/Graphics/Resources/ResourceRegistry.hpp>
#include <Pyramid/Graphics/UI/UIRenderer.hpp>
#include <Pyramid/Input/InputActions.hpp>
#include <Pyramid/Graphics/CameraController.hpp>
#include <Pyramid/Examples/RTSReference/RTSInteractionController.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Font/Font.hpp>
#include <Pyramid/Platform/RuntimePath.hpp>
#include <Pyramid/Platform/SystemFont.hpp>
#include <Pyramid/Util/Log.hpp>
#include <Pyramid/UI/GameUI.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <vector>
#include <string_view>
#include <utility>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
    constexpr Pyramid::u32 kLocalPlayerId = 1;
    constexpr std::string_view kInputContext = "basic-game";
    constexpr std::string_view kQuitAction = "Quit";
    constexpr std::string_view kPauseAction = "Pause";
    constexpr std::string_view kToggleAnimationAction = "ToggleAnimation";
    constexpr std::string_view kResetCameraAction = "ResetCamera";
    constexpr std::string_view kMoveAction = "Move";
    constexpr std::string_view kOrbitDeltaAction = "OrbitDelta";
    constexpr std::string_view kOrbitRateAction = "OrbitRate";
    constexpr std::string_view kZoomDeltaAction = "ZoomDelta";
    constexpr std::string_view kBoostAction = "Boost";
    constexpr std::string_view kSelectAction = "Select";
    constexpr std::string_view kCommandAction = "Command";
    constexpr std::string_view kToggleDebugUIAction = "ToggleDebugUI";

    Pyramid::Font::BakeOptions MakeSystemFontBake(
        Pyramid::u32 atlasWidth,
        Pyramid::u32 atlasHeight,
        std::vector<Pyramid::Font::CharacterRange> ranges)
    {
        Pyramid::Font::BakeOptions bake;
        bake.pixelHeight = 64.0f;
        bake.atlasWidth = atlasWidth;
        bake.atlasHeight = atlasHeight;
        bake.oversample = 1;
        bake.padding = 1;
        bake.mode = Pyramid::Font::RasterMode::SignedDistanceField;
        bake.distanceRange = 10.0f;
        bake.fallbackCodepoint = U'?';
        bake.missingGlyphPolicy = Pyramid::Font::MissingGlyphPolicy::Skip;
        bake.ranges = std::move(ranges);
        return bake;
    }

    const Pyramid::Font::BakeOptions& LatinSystemFontBake()
    {
        static const Pyramid::Font::BakeOptions bake = MakeSystemFontBake(
            2048,
            1536,
            {
                {U' ', static_cast<char32_t>(0x017F)},
                {static_cast<char32_t>(0x0370), static_cast<char32_t>(0x03FF)},
                {static_cast<char32_t>(0x2000), static_cast<char32_t>(0x206F)},
                {static_cast<char32_t>(0x20AC), static_cast<char32_t>(0x20AC)},
                {static_cast<char32_t>(0x2190), static_cast<char32_t>(0x21FF)},
                {static_cast<char32_t>(0x2713), static_cast<char32_t>(0x2713)}});
        return bake;
    }

    const Pyramid::Font::BakeOptions& ArabicSystemFontBake()
    {
        static const Pyramid::Font::BakeOptions bake = MakeSystemFontBake(
            2048,
            1024,
            {
                {static_cast<char32_t>(0x0600), static_cast<char32_t>(0x06FF)},
                {static_cast<char32_t>(0xFE80), static_cast<char32_t>(0xFEF4)}});
        return bake;
    }

    const Pyramid::Font::BakeOptions& SymbolSystemFontBake()
    {
        static const Pyramid::Font::BakeOptions bake = MakeSystemFontBake(
            1024,
            1024,
            {
                {static_cast<char32_t>(0x2190), static_cast<char32_t>(0x21FF)},
                {static_cast<char32_t>(0x2713), static_cast<char32_t>(0x2713)}});
        return bake;
    }

    const std::vector<std::string_view>& LatinSystemFontCandidates()
    {
        static const std::vector<std::string_view> candidates = {
            "Segoe UI", "Tahoma", "Arial"};
        return candidates;
    }

    const std::vector<std::string_view>& ArabicSystemFontCandidates()
    {
        static const std::vector<std::string_view> candidates = {
            "Cairo",
            "Tahoma",
            "Arial",
            "Sakkal Majalla",
            "Traditional Arabic",
            "Simplified Arabic"};
        return candidates;
    }

    const std::vector<std::string_view>& SymbolSystemFontCandidates()
    {
        static const std::vector<std::string_view> candidates = {
            "Segoe UI Symbol", "Segoe UI", "Arial"};
        return candidates;
    }

    const std::vector<char32_t>& RequiredLatinCodepoints()
    {
        static const std::vector<char32_t> required = {
            U'?', U'A', U'a', U'0', U'\u00E9', U'\u03A9'};
        return required;
    }

    const std::vector<char32_t>& RequiredSymbolCodepoints()
    {
        static const std::vector<char32_t> required = {
            U'\u2190', U'\u2191', U'\u2192', U'\u2193', U'\u2713'};
        return required;
    }

    const std::vector<char32_t>& RequiredArabicCodepoints()
    {
        static const std::vector<char32_t> required = []
        {
            std::vector<char32_t> codepoints = {
                static_cast<char32_t>(0x060C),
                static_cast<char32_t>(0x061B),
                static_cast<char32_t>(0x061F)};
            for (char32_t codepoint = 0x064B; codepoint <= 0x065F; ++codepoint)
            {
                codepoints.push_back(codepoint);
            }
            for (char32_t codepoint = 0x0660; codepoint <= 0x0669; ++codepoint)
            {
                codepoints.push_back(codepoint);
            }
            for (char32_t codepoint = 0xFE80; codepoint <= 0xFEF4; ++codepoint)
            {
                codepoints.push_back(codepoint);
            }
            return codepoints;
        }();
        return required;
    }

    std::string FormatFontDiagnostics(
        const std::vector<Pyramid::Font::Diagnostic>& diagnostics)
    {
        if (diagnostics.empty())
        {
            return "no diagnostic was reported";
        }

        std::ostringstream stream;
        for (std::size_t index = 0; index < diagnostics.size(); ++index)
        {
            if (index > 0)
            {
                stream << "; ";
            }
            stream << diagnostics[index].message;
        }
        return stream.str();
    }

    std::string FindMissingCodepoint(
        const Pyramid::Font::BakedFont& font,
        const std::vector<char32_t>& requiredCodepoints)
    {
        for (const char32_t codepoint : requiredCodepoints)
        {
            if (!font.FindGlyph(codepoint))
            {
                std::ostringstream stream;
                stream << "U+" << std::uppercase << std::hex << std::setfill('0')
                       << std::setw(codepoint <= 0xFFFF ? 4 : 6)
                       << static_cast<Pyramid::u32>(codepoint);
                return stream.str();
            }
        }
        return {};
    }

    void AppendFontAttemptError(
        std::string& error,
        std::string_view candidate,
        const std::string& reason)
    {
        if (!error.empty())
        {
            error += "; ";
        }
        error += "`" + std::string(candidate) + "`: " + reason;
    }

    bool TryLoadSystemFontAtlas(
        std::string_view role,
        const std::vector<std::string_view>& candidates,
        const Pyramid::Font::BakeOptions& bake,
        const std::vector<char32_t>& requiredCodepoints,
        Pyramid::Text::FontAtlas& output,
        std::string& description,
        std::string& error)
    {
        output = {};
        description.clear();
        error.clear();

        const std::filesystem::path cacheDirectory =
            Pyramid::Platform::GetUserCacheDirectory("PyramidEngine/Fonts");
        if (cacheDirectory.empty())
        {
            error = "no writable per-user font cache is available";
            return false;
        }

        for (const std::string_view candidate : candidates)
        {
            Pyramid::Platform::SystemFontRequest request;
            request.preferredFamilies.emplace_back(candidate);
            Pyramid::Platform::SystemFontData source;
            std::string sourceError;
            if (!Pyramid::Platform::LoadSystemFont(request, source, &sourceError))
            {
                AppendFontAttemptError(error, candidate, sourceError);
                continue;
            }

            Pyramid::Font::CachedBakeResult cached =
                Pyramid::Font::LoadOrBakeProcessedFont(
                    source.bytes.data(), source.bytes.size(), bake, cacheDirectory);
            if (!cached.Succeeded())
            {
                AppendFontAttemptError(
                    error,
                    candidate,
                    FormatFontDiagnostics(cached.diagnostics));
                continue;
            }

            const std::string missing =
                FindMissingCodepoint(cached.font, requiredCodepoints);
            if (!missing.empty())
            {
                AppendFontAttemptError(
                    error,
                    candidate,
                    role.empty()
                        ? "required glyph " + missing + " is unavailable"
                        : std::string(role) + " requires unavailable glyph " + missing);
                continue;
            }

            output = Pyramid::Text::CreateFontAtlas(cached.font);
            if (!output.IsValid())
            {
                AppendFontAttemptError(
                    error, candidate, "processed font produced an invalid renderer atlas");
                output = {};
                continue;
            }
            description = std::string(role) + " " + source.resolvedFamily +
                (cached.cacheHit ? " (processed cache hit)" : " (processed and cached)");
            return true;
        }
        return false;
    }

    std::string FormatFloat(float value, int precision = 2)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value;
        return stream.str();
    }

    std::string FormatCount(Pyramid::u64 value)
    {
        return std::to_string(value);
    }

    Pyramid::Color LogColor(Pyramid::Util::LogLevel level)
    {
        switch (level)
        {
            case Pyramid::Util::LogLevel::Warn:
                return Pyramid::Color(0.95f, 0.78f, 0.30f, 1.0f);
            case Pyramid::Util::LogLevel::Error:
            case Pyramid::Util::LogLevel::Critical:
                return Pyramid::Color(1.0f, 0.38f, 0.38f, 1.0f);
            case Pyramid::Util::LogLevel::Debug:
                return Pyramid::Color(0.55f, 0.75f, 0.95f, 1.0f);
            default:
                return Pyramid::Color(0.82f, 0.86f, 0.92f, 1.0f);
        }
    }

    std::string FormatLogEntry(const Pyramid::Util::LogEntry& entry)
    {
        return std::string("[") + Pyramid::Util::LogLevelToString(entry.level) +
            "] " + entry.message;
    }

    class MainMenuScreen final : public Pyramid::UI::Screen
    {
    public:
        MainMenuScreen(
            std::function<void()> start,
            std::function<void()> settings,
            std::function<void()> quit)
            : m_start(std::move(start))
            , m_settings(std::move(settings))
            , m_quit(std::move(quit))
        {
        }

        std::string_view GetName() const override { return "MainMenu"; }
        Pyramid::UI::ScreenPresentation GetPresentation() const override
        {
            return Pyramid::UI::ScreenPresentation::Opaque;
        }

        void Build(Pyramid::UI::Context& ui) override
        {
            const auto& frame = ui.GetFrameInfo();
            ui.Overlay("MAIN MENU BACKDROP", Pyramid::Color(0.025f, 0.03f, 0.05f, 0.90f), true);
            const Pyramid::UI::Rect root{0.0f, 0.0f, frame.width, frame.height};
            const Pyramid::UI::Rect rect = Pyramid::UI::ResolveAnchoredRect(
                root,
                Pyramid::Math::Vec2(440.0f, 370.0f),
                Pyramid::UI::Anchor::Center,
                {18.0f, 18.0f, 18.0f, 18.0f});

            Pyramid::UI::PanelOptions panel;
            panel.position = Pyramid::Math::Vec2(rect.x, rect.y);
            panel.size = Pyramid::Math::Vec2(rect.width, rect.height);
            if (ui.BeginPanel("PYRAMID ENGINE", panel))
            {
                ui.WrappedLabel(
                    "Owned retained game screens running on the same UI runtime as the debug tools.");
                ui.Spacer(12.0f);
                if (ui.Button("START GAME") && m_start)
                {
                    m_start();
                }
                if (ui.Button("PROFILE SETTINGS") && m_settings)
                {
                    m_settings();
                }
                ui.Spacer();
                ui.Caption("F1  DEBUG OVERLAY");
                ui.Caption("ESC  PAUSE DURING GAMEPLAY");
                ui.Label(u8"UNICODE  é  Ω  ✓");
                ui.Label(u8"ARABIC  مملكة الهرم ١٢٣");
                ui.WrappedLabel(u8"MIXED BIDI  Kingdom 123 - مملكة الهرم");
                ui.Spacer();
                if (ui.Button("QUIT") && m_quit)
                {
                    m_quit();
                }
                ui.EndPanel();
            }
        }

    private:
        std::function<void()> m_start;
        std::function<void()> m_settings;
        std::function<void()> m_quit;
    };

    class SettingsScreen final : public Pyramid::UI::Screen
    {
    public:
        SettingsScreen(
            std::string playerName,
            std::string kingdomName,
            std::string notes,
            std::function<void(std::string, std::string, std::string)> save,
            std::function<void()> cancel)
            : m_playerName(std::move(playerName))
            , m_kingdomName(std::move(kingdomName))
            , m_notes(std::move(notes))
            , m_save(std::move(save))
            , m_cancel(std::move(cancel))
        {
        }

        std::string_view GetName() const override { return "Settings"; }
        Pyramid::UI::ScreenPresentation GetPresentation() const override
        {
            return Pyramid::UI::ScreenPresentation::Modal;
        }

        void Build(Pyramid::UI::Context& ui) override
        {
            const auto& frame = ui.GetFrameInfo();
            ui.Overlay(
                "SETTINGS BACKDROP",
                Pyramid::Color(0.01f, 0.015f, 0.025f, 0.74f),
                true);
            const Pyramid::UI::Rect root{0.0f, 0.0f, frame.width, frame.height};
            const Pyramid::UI::Rect rect = Pyramid::UI::ResolveAnchoredRect(
                root,
                Pyramid::Math::Vec2(540.0f, 590.0f),
                Pyramid::UI::Anchor::Center,
                {16.0f, 16.0f, 16.0f, 16.0f});
            Pyramid::UI::PanelOptions panel;
            panel.position = Pyramid::Math::Vec2(rect.x, rect.y);
            panel.size = Pyramid::Math::Vec2(rect.width, rect.height);
            if (!ui.BeginPanel("PROFILE SETTINGS", panel))
            {
                return;
            }

            Pyramid::UI::TextFieldOptions playerOptions;
            playerOptions.placeholder = "Player name";
            playerOptions.maximumCharacters = 48;
            playerOptions.hasError = m_showValidation && m_playerName.empty();
            const auto player = ui.TextField("PLAYER NAME", m_playerName, playerOptions);

            Pyramid::UI::TextFieldOptions kingdomOptions;
            kingdomOptions.placeholder = "Kingdom name";
            kingdomOptions.maximumCharacters = 64;
            kingdomOptions.hasError = m_showValidation && m_kingdomName.empty();
            const auto kingdom = ui.TextField("KINGDOM NAME", m_kingdomName, kingdomOptions);

            Pyramid::UI::TextAreaOptions notesOptions;
            notesOptions.placeholder = "Notes";
            notesOptions.maximumCharacters = 512;
            notesOptions.height = 175.0f;
            const auto notes = ui.MultilineTextArea("NOTES", m_notes, notesOptions);
            (void)notes;

            if (m_showValidation && (m_playerName.empty() || m_kingdomName.empty()))
            {
                ui.LabelColored(
                    "Player and kingdom names are required.",
                    Pyramid::Color(1.0f, 0.38f, 0.38f, 1.0f));
            }
            else
            {
                ui.WrappedLabel(
                    u8"Unicode editing: Kingdom 123 - مملكة الهرم. "
                    "Selection and caret movement stay on grapheme boundaries.");
            }

            if (ui.BeginHorizontal("SETTINGS ACTIONS", 30.0f))
            {
                Pyramid::UI::ItemOptions action;
                action.width = 145.0f;
                const bool saveRequested = ui.Button("SAVE", action) ||
                    player.submitted || kingdom.submitted;
                if (saveRequested)
                {
                    m_showValidation = true;
                    if (!m_playerName.empty() && !m_kingdomName.empty() && m_save)
                    {
                        m_save(m_playerName, m_kingdomName, m_notes);
                    }
                }
                if (ui.Button("CANCEL", action) && m_cancel)
                {
                    m_cancel();
                }
                ui.EndHorizontal();
            }
            ui.EndPanel();
        }

    private:
        std::string m_playerName;
        std::string m_kingdomName;
        std::string m_notes;
        std::function<void(std::string, std::string, std::string)> m_save;
        std::function<void()> m_cancel;
        bool m_showValidation = false;
    };

    class GameplayHUDScreen final : public Pyramid::UI::Screen
    {
    public:
        GameplayHUDScreen(
            std::function<void()> pause,
            std::function<std::string()> selection)
            : m_pause(std::move(pause))
            , m_selection(std::move(selection))
        {
        }

        std::string_view GetName() const override { return "GameplayHUD"; }
        Pyramid::UI::ScreenPresentation GetPresentation() const override
        {
            return Pyramid::UI::ScreenPresentation::Transparent;
        }

        void Build(Pyramid::UI::Context& ui) override
        {
            const auto& frame = ui.GetFrameInfo();
            const Pyramid::UI::Rect root{0.0f, 0.0f, frame.width, frame.height};
            const Pyramid::UI::Rect pauseRect = Pyramid::UI::ResolveAnchoredRect(
                root,
                Pyramid::Math::Vec2(180.0f, 92.0f),
                Pyramid::UI::Anchor::TopRight,
                {12.0f, 12.0f, 12.0f, 12.0f});
            Pyramid::UI::PanelOptions pausePanel;
            pausePanel.position = Pyramid::Math::Vec2(pauseRect.x, pauseRect.y);
            pausePanel.size = Pyramid::Math::Vec2(pauseRect.width, pauseRect.height);
            if (ui.BeginPanel("GAME", pausePanel))
            {
                if (ui.Button("PAUSE  [ESC]") && m_pause)
                {
                    m_pause();
                }
                ui.EndPanel();
            }

            const Pyramid::UI::Rect statusRect = Pyramid::UI::ResolveAnchoredRect(
                root,
                Pyramid::Math::Vec2(320.0f, 126.0f),
                Pyramid::UI::Anchor::BottomLeft,
                {12.0f, 12.0f, 12.0f, 12.0f});
            Pyramid::UI::PanelOptions statusPanel;
            statusPanel.position = Pyramid::Math::Vec2(statusRect.x, statusRect.y);
            statusPanel.size = Pyramid::Math::Vec2(statusRect.width, statusRect.height);
            if (ui.BeginPanel("SELECTION", statusPanel))
            {
                ui.LabelValue("ENTITY", m_selection ? m_selection() : "NONE");
                ui.Label("LEFT CLICK SELECT");
                ui.Label("RIGHT CLICK COMMAND");
                ui.EndPanel();
            }
        }

    private:
        std::function<void()> m_pause;
        std::function<std::string()> m_selection;
    };

    class PauseScreen final : public Pyramid::UI::Screen
    {
    public:
        PauseScreen(
            std::function<void()> resume,
            std::function<void()> resetCamera,
            std::function<void()> mainMenu,
            std::function<void()> quit)
            : m_resume(std::move(resume))
            , m_resetCamera(std::move(resetCamera))
            , m_mainMenu(std::move(mainMenu))
            , m_quit(std::move(quit))
        {
        }

        std::string_view GetName() const override { return "Pause"; }
        Pyramid::UI::ScreenPresentation GetPresentation() const override
        {
            return Pyramid::UI::ScreenPresentation::Modal;
        }

        void Build(Pyramid::UI::Context& ui) override
        {
            const auto& frame = ui.GetFrameInfo();
            ui.Overlay("PAUSE BACKDROP", Pyramid::Color(0.01f, 0.015f, 0.025f, 0.72f), true);
            const Pyramid::UI::Rect root{0.0f, 0.0f, frame.width, frame.height};
            const Pyramid::UI::Rect rect = Pyramid::UI::ResolveAnchoredRect(
                root,
                Pyramid::Math::Vec2(390.0f, 320.0f),
                Pyramid::UI::Anchor::Center,
                {16.0f, 16.0f, 16.0f, 16.0f});
            Pyramid::UI::PanelOptions panel;
            panel.position = Pyramid::Math::Vec2(rect.x, rect.y);
            panel.size = Pyramid::Math::Vec2(rect.width, rect.height);
            if (ui.BeginPanel("PAUSED", panel))
            {
                if (ui.Button("RESUME") && m_resume)
                {
                    m_resume();
                }
                if (ui.Button("RESET CAMERA") && m_resetCamera)
                {
                    m_resetCamera();
                }
                if (ui.Button("RETURN TO MAIN MENU") && m_mainMenu)
                {
                    m_mainMenu();
                }
                if (ui.Button("QUIT") && m_quit)
                {
                    m_quit();
                }
                ui.EndPanel();
            }
        }

    private:
        std::function<void()> m_resume;
        std::function<void()> m_resetCamera;
        std::function<void()> m_mainMenu;
        std::function<void()> m_quit;
    };

    constexpr const char* kForwardVertexShader = R"(
#version 330 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;

uniform mat4 u_Model;
uniform mat4 u_ViewProjection;
uniform mat4 u_NormalMatrix;

out vec4 v_Color;
out vec3 v_Normal;

void main()
{
    vec4 worldPosition = u_Model * vec4(a_Position, 1.0);
    vec3 localNormal = normalize(a_Position);

    v_Color = a_Color;
    v_Normal = normalize((u_NormalMatrix * vec4(localNormal, 0.0)).xyz);

    gl_Position = u_ViewProjection * worldPosition;
}
)";

    constexpr const char* kForwardFragmentShader = R"(
#version 330 core

in vec4 v_Color;
in vec3 v_Normal;

uniform vec4 u_AlbedoColor;

out vec4 FragColor;

void main()
{
    vec3 lightDirection = normalize(vec3(0.5, 1.0, 0.25));
    float ndotl = max(dot(normalize(v_Normal), lightDirection), 0.12);
    vec3 litColor = v_Color.rgb * u_AlbedoColor.rgb * ndotl;

    FragColor = vec4(litColor, 1.0);
}
)";
}

BasicGame::BasicGame()
    : Game(Pyramid::GraphicsAPI::OpenGL)
{
}

BasicGame::~BasicGame()
{
    UnregisterUIContext(&m_debugUI);
    UnregisterUIContext(&m_gameUI);
}

void BasicGame::onCreate()
{
    Game::onCreate();
    if (!IsInitialized())
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: engine initialization failed.");
        quit();
        return;
    }

    if (!SetupInputActions())
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: input action setup failed.");
        quit();
        return;
    }

    auto* device = GetGraphicsDevice();
    if (!device)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: graphics device is null.");
        quit();
        return;
    }

    auto* resources = GetResourceRegistry();
    if (!resources)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: resource registry is null.");
        quit();
        return;
    }

    m_gameUI.SetClipboard(GetClipboard());
    m_debugUI.SetClipboard(GetClipboard());

    m_renderSystem = std::make_unique<Pyramid::Renderer::RenderSystem>();
    if (!m_renderSystem->Initialize(device))
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: render system initialization failed.");
        quit();
        return;
    }
    SetRenderSystem(m_renderSystem.get());

    const std::filesystem::path latinFontPath =
        Pyramid::Platform::ResolveRuntimePath("Fonts/PyramidSans-64-sdf.pfont");
    const std::filesystem::path arabicFontPath =
        Pyramid::Platform::ResolveRuntimePath("Fonts/PyramidArabic-64-sdf.pfont");

    std::string systemFontDescription;
    std::string systemFontError;
    m_runtimeFontLoaded = TryLoadSystemFontAtlas(
        "Latin UI",
        LatinSystemFontCandidates(),
        LatinSystemFontBake(),
        RequiredLatinCodepoints(),
        m_runtimeFont,
        systemFontDescription,
        systemFontError);
    if (m_runtimeFontLoaded)
    {
        PYRAMID_LOG_INFO(
            "Loaded native UI outlines through Pyramid font pipeline: " +
            systemFontDescription);
    }
    else
    {
        std::string bundledError;
        m_runtimeFontLoaded = Pyramid::Text::LoadFontAtlas(
            latinFontPath.generic_string(), m_runtimeFont, &bundledError);
        if (m_runtimeFontLoaded)
        {
            PYRAMID_LOG_WARN(
                "Native UI font unavailable; loaded owned PyramidSans fallback: " +
                systemFontError);
        }
        else
        {
            systemFontError += "; bundled fallback failed: " + bundledError;
        }
    }

    std::string arabicFontDescription;
    std::string arabicFontError;
    m_runtimeArabicFontLoaded = TryLoadSystemFontAtlas(
        "Arabic UI",
        ArabicSystemFontCandidates(),
        ArabicSystemFontBake(),
        RequiredArabicCodepoints(),
        m_runtimeArabicFont,
        arabicFontDescription,
        arabicFontError);
    if (m_runtimeArabicFontLoaded)
    {
        PYRAMID_LOG_INFO(
            "Loaded native Arabic outlines through Pyramid font pipeline: " +
            arabicFontDescription);
    }
    else
    {
        std::string bundledError;
        m_runtimeArabicFontLoaded = Pyramid::Text::LoadFontAtlas(
            arabicFontPath.generic_string(), m_runtimeArabicFont, &bundledError);
        if (m_runtimeArabicFontLoaded)
        {
            PYRAMID_LOG_WARN(
                "Native Arabic UI font unavailable; loaded owned PyramidArabic "
                "fallback: " + arabicFontError);
        }
        else
        {
            arabicFontError += "; bundled fallback failed: " + bundledError;
        }
    }

    std::string symbolFontError;
    if (m_runtimeFontLoaded && !m_runtimeFont.HasGlyph(U'\u2713'))
    {
        std::string symbolFontDescription;
        m_runtimeSymbolFontLoaded = TryLoadSystemFontAtlas(
            "Symbol UI",
            SymbolSystemFontCandidates(),
            SymbolSystemFontBake(),
            RequiredSymbolCodepoints(),
            m_runtimeSymbolFont,
            symbolFontDescription,
            symbolFontError);
        if (m_runtimeSymbolFontLoaded)
        {
            PYRAMID_LOG_INFO(
                "Loaded native UI symbols through Pyramid font pipeline: " +
                symbolFontDescription);
        }
        else
        {
            std::string bundledError;
            m_runtimeSymbolFontLoaded = Pyramid::Text::LoadFontAtlas(
                latinFontPath.generic_string(), m_runtimeSymbolFont, &bundledError);
            if (m_runtimeSymbolFontLoaded)
            {
                PYRAMID_LOG_WARN(
                    "Native UI symbol font unavailable; loaded owned PyramidSans "
                    "symbol fallback: " + symbolFontError);
            }
            else
            {
                symbolFontError += "; bundled symbol fallback failed: " + bundledError;
            }
        }
    }

    if (m_runtimeFontLoaded)
    {
        std::vector<Pyramid::Text::FontAtlas> familyFonts{m_runtimeFont};
        if (m_runtimeArabicFontLoaded)
        {
            familyFonts.push_back(m_runtimeArabicFont);
        }
        if (m_runtimeSymbolFontLoaded)
        {
            familyFonts.push_back(m_runtimeSymbolFont);
        }
        std::string familyError;
        if (Pyramid::Text::BuildFontFamily(
                familyFonts, m_runtimeFontFamily, &familyError))
        {
            (void)m_gameUI.SetFontFamily(m_runtimeFontFamily);
            (void)m_debugUI.SetFontFamily(m_runtimeFontFamily);
            PYRAMID_LOG_INFO(
                "UI font family ready: " + m_runtimeFontFamily.atlas.familyName + " (" +
                std::to_string(m_runtimeFontFamily.atlas.glyphs.size()) +
                " resolved glyphs)");
        }
        else
        {
            (void)m_gameUI.SetFontAtlas(m_runtimeFont);
            (void)m_debugUI.SetFontAtlas(m_runtimeFont);
            PYRAMID_LOG_WARN("UI font family merge failed: " + familyError);
        }
        if (!m_runtimeArabicFontLoaded)
        {
            PYRAMID_LOG_WARN(
                "Arabic UI fonts unavailable at `" +
                arabicFontPath.generic_string() + "`: " + arabicFontError);
        }
        if (!m_runtimeFont.HasGlyph(U'\u2713') && !m_runtimeSymbolFontLoaded)
        {
            PYRAMID_LOG_WARN("UI symbol fonts unavailable: " + symbolFontError);
        }
    }
    else
    {
        PYRAMID_LOG_WARN(
            "Production UI fonts unavailable; using embedded debug font: " +
            systemFontError);
    }

    const float runtimePixelHeight = m_runtimeFontLoaded
        ? m_gameUI.GetFontAtlas().pixelHeight
        : 0.0f;
    auto gameTheme = m_gameUI.GetTheme();
    gameTheme.textScale = runtimePixelHeight > 0.0f
        ? 20.0f / runtimePixelHeight
        : 1.5f;
    gameTheme.defaultRowHeight = 30.0f;
    gameTheme.spacing = 8.0f;
    gameTheme.padding = 10.0f;
    m_gameUI.SetTheme(gameTheme);

    auto debugTheme = m_debugUI.GetTheme();
    debugTheme.textScale = runtimePixelHeight > 0.0f
        ? 16.0f / runtimePixelHeight
        : 1.5f;
    debugTheme.defaultRowHeight = 28.0f;
    debugTheme.spacing = 7.0f;
    m_debugUI.SetTheme(debugTheme);
    m_debugUI.SetEnabled(m_debugUIVisible);

    m_uiRenderer = std::make_unique<Pyramid::UIRenderer>();
    if (!m_uiRenderer->Initialize(*device, *resources, m_gameUI.GetFontAtlas()))
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: UI renderer initialization failed.");
        quit();
        return;
    }
    RegisterUIContext(&m_gameUI);
    RegisterUIContext(&m_debugUI);

    m_scene = std::make_shared<Pyramid::Scene>("BasicGame Scene");
    m_sceneManager = std::make_unique<Pyramid::SceneManagement::SceneManager>();
    m_sceneManager->SetActiveScene(m_scene);

    m_camera = std::make_unique<Pyramid::Camera>(
        Pyramid::Math::Radians(60.0f),
        1280.0f / 720.0f,
        0.1f,
        200.0f);

    const Pyramid::Math::Vec3 cameraFocus(0.0f, -0.25f, 0.0f);
    m_camera->SetPosition(Pyramid::Math::Vec3(7.5f, 6.75f, 9.5f));
    m_camera->LookAt(cameraFocus);

    Pyramid::RTSCameraActions cameraActions;
    cameraActions.move = {std::string(kInputContext), std::string(kMoveAction)};
    cameraActions.orbitDelta = {std::string(kInputContext), std::string(kOrbitDeltaAction)};
    cameraActions.orbitRate = {std::string(kInputContext), std::string(kOrbitRateAction)};
    cameraActions.zoomDelta = {std::string(kInputContext), std::string(kZoomDeltaAction)};
    cameraActions.zoomRate = {};
    cameraActions.boost = {std::string(kInputContext), std::string(kBoostAction)};
    cameraActions.reset = {std::string(kInputContext), std::string(kResetCameraAction)};

    Pyramid::RTSCameraSettings cameraSettings;
    cameraSettings.movementSpeed = 4.0f;
    cameraSettings.orbitSensitivity = 0.005f;
    cameraSettings.zoomSensitivity = 0.75f;
    cameraSettings.minimumDistance = 3.0f;
    cameraSettings.maximumDistance = 30.0f;
    cameraSettings.distanceMovementScale = 0.04f;
    m_cameraController = std::make_unique<Pyramid::RTSCameraController>(
        cameraFocus,
        std::move(cameraActions),
        cameraSettings);
    m_cameraController->CaptureHome(*m_camera);

    Pyramid::Examples::RTSReference::RTSInteractionActions interactionActions;
    interactionActions.select = {std::string(kInputContext), std::string(kSelectAction)};
    interactionActions.command = {std::string(kInputContext), std::string(kCommandAction)};

    Pyramid::Examples::RTSReference::RTSInteractionSettings interactionSettings;
    interactionSettings.edgeMarginPixels = 18.0f;
    interactionSettings.edgeScrollSpeed = 4.0f;
    interactionSettings.edgeScrollDistanceScale = 0.04f;
    interactionSettings.maximumRayDistance = 200.0f;
    m_interactionController =
        std::make_unique<Pyramid::Examples::RTSReference::RTSInteractionController>(
            std::move(interactionActions),
            interactionSettings);
    m_interactionController->SetViewportSize(1280, 720);

    SetActiveCamera(m_camera.get());

    Pyramid::ShaderProgramSpecification shaderSpecification;
    shaderSpecification.vertexSource = kForwardVertexShader;
    shaderSpecification.fragmentSource = kForwardFragmentShader;
    shaderSpecification.name = "BasicGame Forward";
    shaderSpecification.assetId =
        Pyramid::ShaderAssetId::FromString("examples/basic-game/forward");

    m_shader = resources->Shaders().GetOrCreate(shaderSpecification);
    if (!m_shader)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to compile scene shader.");
        quit();
        return;
    }

    const std::array<Pyramid::u8, 16> checkerPixels = {
        255, 255, 255, 255, 40, 40, 40, 255,
        40, 40, 40, 255, 255, 255, 255, 255};
    Pyramid::TextureResourceSpecification textureSpecification;
    textureSpecification.texture.Width = 2;
    textureSpecification.texture.Height = 2;
    textureSpecification.texture.Format = Pyramid::TextureFormat::RGBA8;
    textureSpecification.texture.GenerateMips = false;
    textureSpecification.texture.MinFilter = Pyramid::TextureFilter::Nearest;
    textureSpecification.texture.MagFilter = Pyramid::TextureFilter::Nearest;
    textureSpecification.pixelData = checkerPixels.data();
    textureSpecification.pixelDataSize = checkerPixels.size();
    textureSpecification.colorSpace = Pyramid::TextureColorSpace::SRGB;
    textureSpecification.assetId =
        Pyramid::TextureAssetId::FromString("examples/basic-game/checker");
    textureSpecification.name = "BasicGame Checker";
    m_debugTexture = resources->Textures().GetOrCreate(textureSpecification);
    if (!m_debugTexture)
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to create cached checker texture.");
        quit();
        return;
    }

    if (!SetupScene())
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to create scene objects.");
        quit();
        return;
    }

    Pyramid::Examples::RTSReference::RTSUnitState cubeUnit;
    cubeUnit.owner = kLocalPlayerId;
    cubeUnit.team = kLocalPlayerId;
    if (!m_units.AddUnit(m_cubeEntity, cubeUnit))
    {
        PYRAMID_LOG_CRITICAL("BasicGame aborted: failed to register the demo unit.");
        quit();
        return;
    }
    m_interactionController->SetSelectablePredicate(
        [this](const Pyramid::Entity& entity)
        {
            return m_units.IsSelectable(entity);
        });

    Pyramid::Entity sun = m_scene->CreateEntity("Sun");
    Pyramid::LightComponent sunLight;
    sunLight.type = Pyramid::LightType::Directional;
    sunLight.localDirection = Pyramid::Math::Vec3(-0.5f, -1.0f, -0.25f);
    sunLight.intensity = 1.5f;
    sun.SetLight(sunLight);
    m_scene->SetPrimaryLight(sun);

    auto& environment = m_scene->GetEnvironment();
    environment.skyColor = Pyramid::Math::Vec3(0.09f, 0.12f, 0.18f);
    environment.ambientColor = Pyramid::Math::Vec3(0.12f, 0.12f, 0.14f);

    ShowMainMenu();

    PYRAMID_LOG_INFO(
        "BasicGame ready: retained game screens and RTS reference input are active.");
}

void BasicGame::onUpdate(float deltaTime)
{
    Game::onUpdate(deltaTime);

    const auto& actions = GetInputActions();
    if (actions.WasActionPressed(kInputContext, kQuitAction))
    {
        quit();
        return;
    }

    if (actions.WasActionPressed(kInputContext, kToggleDebugUIAction))
    {
        m_debugUIVisible = !m_debugUIVisible;
        m_debugUI.SetEnabled(m_debugUIVisible);
    }

    if (actions.WasActionPressed(kInputContext, kPauseAction))
    {
        if (m_gameScreens.GetTopName() == "Settings")
        {
            CloseSettings();
        }
        else if (m_gameScreens.GetTopName() == "Pause")
        {
            ResumeGameplay();
        }
        else if (m_gameScreens.GetTopName() == "GameplayHUD")
        {
            OpenPauseMenu();
        }
    }

    m_gameScreens.Update(deltaTime);
    const bool gameplayBlocked = m_gameScreens.BlocksGameplayInput();

    if (!gameplayBlocked &&
        actions.WasActionPressed(kInputContext, kToggleAnimationAction))
    {
        m_animationPaused = !m_animationPaused;
        PYRAMID_LOG_INFO(
            "Cube animation ",
            m_animationPaused ? "paused" : "resumed");
    }

    if (m_gameplayStarted && !gameplayBlocked && !m_animationPaused)
    {
        m_elapsedTime += deltaTime;
    }

    if (m_cubeEntity)
    {
        m_cubeEntity.SetLocalRotation(Pyramid::Math::Quat::FromEuler(
            0.35f * m_elapsedTime,
            0.8f * m_elapsedTime,
            0.0f));
    }

    if (!gameplayBlocked && m_camera && m_cameraController &&
        m_interactionController && m_sceneManager)
    {
        m_sceneManager->Update(deltaTime);
        m_interactionController->Update(
            GetInput(),
            GetInputActions(),
            GetUIInputConsumption(),
            *m_camera,
            *m_sceneManager,
            *m_cameraController,
            deltaTime);

        const Pyramid::EntityId selected =
            m_interactionController->GetSelectedEntityId();
        if (selected != m_reportedSelection)
        {
            if (selected)
            {
                PYRAMID_LOG_INFO("Selected entity ", selected.GetValue());
            }
            else
            {
                PYRAMID_LOG_INFO("Selection cleared");
            }
            m_reportedSelection = selected;
        }

        if (const auto command = m_interactionController->ConsumeCommand())
        {
            if (m_units.ApplyMoveCommand(kLocalPlayerId, *command))
            {
                PYRAMID_LOG_INFO(
                    "Movement target accepted for entity ", command->entity.GetValue(),
                    " at (", command->target.x, ", ", command->target.y, ", ",
                    command->target.z, ")");
            }
            else
            {
                PYRAMID_LOG_WARN(
                    "Movement target rejected for entity ", command->entity.GetValue());
            }
        }
    }

    BuildGameUI(deltaTime);
    BuildDebugUI(deltaTime);
}

void BasicGame::onRender()
{
    if (!m_renderSystem || !m_scene || !m_camera)
    {
        Game::onRender();
        return;
    }

    m_renderSystem->BeginFrame();
    m_renderSystem->Render(*m_scene, *m_camera);
    if (m_uiRenderer)
    {
        (void)m_uiRenderer->Render(m_gameUI.GetDrawList(), m_uiFrame);
        if (m_debugUIVisible)
        {
            (void)m_uiRenderer->Render(m_debugUI.GetDrawList(), m_uiFrame);
        }
    }
    m_renderSystem->EndFrame();
}

void BasicGame::BuildGameUI(float deltaTime)
{
    m_uiFrame.deltaTime = deltaTime;
    if (!m_gameUI.BeginFrame(m_uiFrame, GetInput()))
    {
        return;
    }
    m_gameScreens.Build(m_gameUI);
    (void)m_gameUI.EndFrame();
}

void BasicGame::ShowMainMenu()
{
    m_gameplayStarted = false;
    auto screen = std::make_shared<MainMenuScreen>(
        [this]() { StartGameplay(); },
        [this]() { OpenSettings(); },
        [this]() { quit(); });
    m_gameScreens.Clear();
    (void)m_gameScreens.Push(
        std::move(screen),
        {Pyramid::UI::ScreenTransitionType::Fade, 0.20f});
}

void BasicGame::OpenSettings()
{
    if (m_gameScreens.GetTopName() == "Settings")
    {
        return;
    }
    auto screen = std::make_shared<SettingsScreen>(
        m_playerName,
        m_kingdomName,
        m_profileNotes,
        [this](std::string player, std::string kingdom, std::string notes)
        {
            m_playerName = std::move(player);
            m_kingdomName = std::move(kingdom);
            m_profileNotes = std::move(notes);
            PYRAMID_LOG_INFO(
                "Saved UI profile for " + m_playerName + " / " + m_kingdomName);
            CloseSettings();
        },
        [this]() { CloseSettings(); });
    (void)m_gameScreens.Push(
        std::move(screen),
        {Pyramid::UI::ScreenTransitionType::Fade, 0.12f});
}

void BasicGame::CloseSettings()
{
    if (m_gameScreens.GetTopName() == "Settings")
    {
        (void)m_gameScreens.Pop(
            {Pyramid::UI::ScreenTransitionType::Fade, 0.12f});
    }
}

void BasicGame::StartGameplay()
{
    m_gameplayStarted = true;
    auto screen = std::make_shared<GameplayHUDScreen>(
        [this]() { OpenPauseMenu(); },
        [this]()
        {
            if (!m_interactionController)
            {
                return std::string("NONE");
            }
            const Pyramid::EntityId selected =
                m_interactionController->GetSelectedEntityId();
            return selected
                ? std::string("ENTITY ") + std::to_string(selected.GetValue())
                : std::string("NONE");
        });
    (void)m_gameScreens.Replace(
        std::move(screen),
        {Pyramid::UI::ScreenTransitionType::Fade, 0.15f});
}

void BasicGame::OpenPauseMenu()
{
    if (!m_gameplayStarted || m_gameScreens.GetTopName() == "Pause")
    {
        return;
    }
    auto screen = std::make_shared<PauseScreen>(
        [this]() { ResumeGameplay(); },
        [this]()
        {
            if (m_cameraController && m_camera)
            {
                m_cameraController->Reset(*m_camera);
            }
        },
        [this]() { ReturnToMainMenu(); },
        [this]() { quit(); });
    (void)m_gameScreens.Push(
        std::move(screen),
        {Pyramid::UI::ScreenTransitionType::Fade, 0.12f});
}

void BasicGame::ResumeGameplay()
{
    if (m_gameScreens.GetTopName() == "Pause")
    {
        (void)m_gameScreens.Pop(
            {Pyramid::UI::ScreenTransitionType::Fade, 0.12f});
    }
}

void BasicGame::ReturnToMainMenu()
{
    ShowMainMenu();
}

void BasicGame::BuildDebugUI(float deltaTime)
{
    if (!m_debugUIVisible)
    {
        return;
    }

    m_smoothedFrameTime += (deltaTime - m_smoothedFrameTime) * 0.1f;
    m_uiFrame.deltaTime = deltaTime;
    if (!m_debugUI.BeginFrame(m_uiFrame, GetInput()))
    {
        return;
    }

    const float availableHeight = (std::max)(220.0f, m_uiFrame.height - 24.0f);
    Pyramid::UI::PanelOptions diagnostics;
    diagnostics.position = Pyramid::Math::Vec2(12.0f, 12.0f);
    diagnostics.size = Pyramid::Math::Vec2(
        350.0f,
        (std::min)(620.0f, availableHeight));
    if (m_debugUI.BeginPanel("PYRAMID DEBUG  [F1]", diagnostics))
    {
        if (m_debugUI.CollapsingHeader("PERFORMANCE", m_performanceSectionOpen))
        {
            PYRAMID_LOG_DEBUG("Performance debug section toggled");
        }
        if (m_performanceSectionOpen)
        {
            const float fps = m_smoothedFrameTime > 0.000001f
                ? 1.0f / m_smoothedFrameTime
                : 0.0f;
            m_debugUI.LabelValue("FPS", FormatFloat(fps, 1));
            m_debugUI.LabelValue("FRAME MS", FormatFloat(m_smoothedFrameTime * 1000.0f, 2));
            m_debugUI.ProgressBar(
                "16.67 MS BUDGET",
                m_smoothedFrameTime / (1.0f / 60.0f));
            if (m_renderSystem)
            {
                const auto& renderStats = m_renderSystem->GetStats();
                m_debugUI.LabelValue("DRAW CALLS", FormatCount(renderStats.drawCalls));
                m_debugUI.LabelValue("TRIANGLES", FormatCount(renderStats.triangles));
                m_debugUI.LabelValue("RENDER MS", FormatFloat(renderStats.frameTime, 2));
            }
        }

        if (m_debugUI.CollapsingHeader("RESOURCES", m_resourcesSectionOpen))
        {
            PYRAMID_LOG_DEBUG("Resources debug section toggled");
        }
        if (m_resourcesSectionOpen)
        {
            if (auto* resources = GetResourceRegistry())
            {
                const auto stats = resources->GetStats();
                m_debugUI.LabelValue("MESHES", FormatCount(stats.meshes.residentMeshes));
                m_debugUI.LabelValue("TEXTURES", FormatCount(stats.textures.residentTextures));
                m_debugUI.LabelValue("SHADERS", FormatCount(stats.shaders.residentPrograms));
                m_debugUI.LabelValue("MATERIALS", FormatCount(stats.materials.residentMaterials));
            }
        }

        if (m_debugUI.CollapsingHeader("INPUT", m_inputSectionOpen))
        {
            PYRAMID_LOG_DEBUG("Input debug section toggled");
        }
        if (m_inputSectionOpen)
        {
            const auto mouse = GetInput().GetMousePosition();
            m_debugUI.LabelValue(
                "MOUSE",
                FormatFloat(mouse.x, 0) + ", " + FormatFloat(mouse.y, 0));
            m_debugUI.LabelValue("WHEEL", FormatFloat(GetInput().GetMouseWheelDelta(), 1));
            m_debugUI.LabelValue(
                "TEXT EVENTS",
                FormatCount(GetInput().GetTextInputEventCount()));
            m_debugUI.LabelValue(
                "UI CAPTURE",
                GetUIInputConsumption().HasAnyMouseConsumption() ? "POINTER" :
                (GetUIInputConsumption().IsTextInputConsumed() ? "TEXT" : "NONE"));
        }

        if (m_debugUI.CollapsingHeader("UI FRAME", m_uiSectionOpen))
        {
            PYRAMID_LOG_DEBUG("UI frame debug section toggled");
        }
        if (m_uiSectionOpen)
        {
            const auto uiStats = m_debugUI.GetStats();
            m_debugUI.LabelValue("ELEMENTS", FormatCount(uiStats.retainedElements));
            m_debugUI.LabelValue("VERTICES", FormatCount(uiStats.vertices));
            m_debugUI.LabelValue("BATCHES", FormatCount(uiStats.batches));
            const auto& editing = m_gameUI.GetTextEditDebugInfo();
            m_debugUI.LabelValue("EDIT CURSOR", FormatCount(editing.cursor));
            m_debugUI.LabelValue(
                "EDIT SELECTION",
                std::to_string(editing.selection.begin) + "-" +
                    std::to_string(editing.selection.end));
            if (!editing.clipboardStatus.empty())
            {
                m_debugUI.LabelValue("CLIPBOARD", editing.clipboardStatus);
            }
            const auto& font = m_debugUI.GetFontAtlas();
            m_debugUI.LabelValue("FONT", font.familyName);
            m_debugUI.LabelValue("FONT GLYPHS", FormatCount(font.glyphs.size()));
            m_debugUI.LabelValue(
                "FONT ATLAS",
                std::to_string(font.width) + "x" + std::to_string(font.height));
            m_debugUI.LabelValue(
                "FONT KB",
                FormatFloat(static_cast<float>(font.rgbaPixels.size()) / 1024.0f, 1));
            if (m_uiRenderer)
            {
                const auto& rendererStats = m_uiRenderer->GetStats();
                m_debugUI.LabelValue("UI DRAWS", FormatCount(rendererStats.drawCalls));
                m_debugUI.LabelValue("UI SKIPPED", FormatCount(rendererStats.batchesSkipped));
            }
        }
        m_debugUI.EndPanel();
    }

    Pyramid::UI::PanelOptions controls;
    controls.position = Pyramid::Math::Vec2(374.0f, 12.0f);
    controls.size = Pyramid::Math::Vec2(
        300.0f,
        (std::min)(300.0f, availableHeight));
    if (m_debugUI.BeginPanel("RUNTIME CONTROLS", controls))
    {
        (void)m_debugUI.Checkbox("PAUSE ANIMATION", m_animationPaused);
        if (m_cameraController)
        {
            auto settings = m_cameraController->GetSettings();
            float movementSpeed = settings.movementSpeed;
            if (m_debugUI.SliderFloat(
                    "CAMERA SPEED",
                    movementSpeed,
                    1.0f,
                    20.0f))
            {
                settings.movementSpeed = movementSpeed;
                (void)m_cameraController->SetSettings(settings);
            }
            if (m_debugUI.Button("RESET CAMERA") && m_camera)
            {
                m_cameraController->Reset(*m_camera);
            }
        }
        m_debugUI.WrappedLabel(
            "UI captures the pointer before RTS edge scrolling, selection and commands.");
        m_debugUI.EndPanel();
    }

    const float logX = 686.0f;
    const float logWidth = (std::max)(280.0f, m_uiFrame.width - logX - 12.0f);
    Pyramid::UI::PanelOptions logPanel;
    logPanel.position = Pyramid::Math::Vec2(logX, 12.0f);
    logPanel.size = Pyramid::Math::Vec2(
        logWidth,
        (std::min)(420.0f, availableHeight));
    if (m_debugUI.BeginPanel("RUNTIME LOG", logPanel))
    {
        if (m_debugUI.BeginHorizontal("LOG ACTIONS", 24.0f))
        {
            Pyramid::UI::ItemOptions clearOptions;
            clearOptions.width = 110.0f;
            if (m_debugUI.Button("CLEAR LOG", clearOptions))
            {
                Pyramid::Util::Logger::GetInstance().ClearHistory();
            }
            m_debugUI.EndHorizontal();
        }

        Pyramid::UI::ScrollAreaOptions scrolling;
        scrolling.height = (std::max)(100.0f, logPanel.size.y - 84.0f);
        scrolling.wheelStep = 24.0f;
        scrolling.stickToBottom = true;
        if (m_debugUI.BeginScrollArea("LOG ENTRIES", scrolling))
        {
            const auto entries = Pyramid::Util::Logger::GetInstance().GetRecentEntries(
                100,
                Pyramid::Util::LogLevel::Info);
            Pyramid::u64 index = 0;
            for (const auto& entry : entries)
            {
                m_debugUI.PushId(index++);
                const Pyramid::Color color = LogColor(entry.level);
                m_debugUI.WrappedLabel(FormatLogEntry(entry), {}, &color);
                m_debugUI.PopId();
            }
            if (entries.empty())
            {
                m_debugUI.Label("NO LOG ENTRIES");
            }
            m_debugUI.EndScrollArea();
        }
        m_debugUI.EndPanel();
    }
    (void)m_debugUI.EndFrame();
}

std::shared_ptr<Pyramid::Mesh> BasicGame::CreateColoredCube(float size)
{
    auto* device = GetGraphicsDevice();
    if (!device)
    {
        return nullptr;
    }

    const float s = size * 0.5f;
    const std::vector<Pyramid::Vertex> vertices = {
        {-s, -s, -s, 1.0f, 0.2f, 0.2f, 1.0f},
        {s, -s, -s, 0.2f, 1.0f, 0.2f, 1.0f},
        {s, s, -s, 0.2f, 0.2f, 1.0f, 1.0f},
        {-s, s, -s, 1.0f, 1.0f, 0.2f, 1.0f},
        {-s, -s, s, 1.0f, 0.2f, 1.0f, 1.0f},
        {s, -s, s, 0.2f, 1.0f, 1.0f, 1.0f},
        {s, s, s, 1.0f, 1.0f, 1.0f, 1.0f},
        {-s, s, s, 1.0f, 0.6f, 0.2f, 1.0f},
    };

    const std::vector<Pyramid::u32> indices = {
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        0, 7, 3, 0, 4, 7,
        1, 2, 6, 1, 6, 5,
        3, 7, 6, 3, 6, 2,
        0, 1, 5, 0, 5, 4,
    };

    Pyramid::MeshSpecification specification;
    specification.vertexData = vertices.data();
    specification.vertexDataSize =
        static_cast<Pyramid::u32>(vertices.size() * sizeof(Pyramid::Vertex));
    specification.vertexCount = static_cast<Pyramid::u32>(vertices.size());
    specification.layout = {
        {Pyramid::ShaderDataType::Float3, "a_Position"},
        {Pyramid::ShaderDataType::Float4, "a_Color"},
    };
    specification.indexData = indices.data();
    specification.indexCount = static_cast<Pyramid::u32>(indices.size());
    specification.topology = Pyramid::PrimitiveTopology::Triangles;
    specification.name = "ColoredCube";

    auto* resources = GetResourceRegistry();
    return resources
        ? resources->Meshes().GetOrCreate(specification)
        : nullptr;
}

bool BasicGame::SetupScene()
{
    if (!m_scene || !m_shader)
    {
        return false;
    }

    auto cubeGeometry = CreateColoredCube(1.5f);
    auto floorGeometry = CreateColoredCube(1.5f);
    if (!cubeGeometry || !floorGeometry || cubeGeometry != floorGeometry)
    {
        return false;
    }

    Pyramid::MaterialSpecification cubeMaterialSpecification;
    cubeMaterialSpecification.shader = m_shader;
    cubeMaterialSpecification.uniforms = {
        {"u_AlbedoColor", Pyramid::Math::Vec4(1.0f, 1.0f, 1.0f, 1.0f)}};
    cubeMaterialSpecification.assetId =
        Pyramid::MaterialAssetId::FromString("examples/basic-game/cube-material");
    cubeMaterialSpecification.name = "BasicGame Cube Material";
    auto* resources = GetResourceRegistry();
    if (!resources)
    {
        return false;
    }

    m_cubeMaterial = resources->Materials().GetOrCreate(cubeMaterialSpecification);

    Pyramid::MaterialSpecification floorMaterialSpecification = cubeMaterialSpecification;
    floorMaterialSpecification.uniforms = {
        {"u_AlbedoColor", Pyramid::Math::Vec4(0.35f, 0.42f, 0.55f, 1.0f)}};
    floorMaterialSpecification.assetId =
        Pyramid::MaterialAssetId::FromString("examples/basic-game/floor-material");
    floorMaterialSpecification.name = "BasicGame Floor Material";
    m_floorMaterial = resources->Materials().GetOrCreate(floorMaterialSpecification);

    if (!m_cubeMaterial || !m_floorMaterial)
    {
        return false;
    }

    const auto cubeMeshHandle = resources->GetHandle(cubeGeometry->GetAssetId());
    const auto floorMeshHandle = resources->GetHandle(floorGeometry->GetAssetId());
    const auto cubeMaterialHandle = resources->GetHandle(m_cubeMaterial->GetAssetId());
    const auto floorMaterialHandle = resources->GetHandle(m_floorMaterial->GetAssetId());

    m_cubeEntity = m_scene->CreateEntity("DemoCube");
    Pyramid::MeshRendererComponent cubeRenderer;
    cubeRenderer.mesh = cubeMeshHandle;
    cubeRenderer.material = cubeMaterialHandle;
    if (!m_cubeEntity.SetMeshRenderer(cubeRenderer, resources))
    {
        return false;
    }

    Pyramid::Entity floorEntity = m_scene->CreateEntity("Floor");
    floorEntity.SetLocalTransform(
        Pyramid::Math::Vec3(0.0f, -0.86f, 0.0f),
        Pyramid::Math::Quat::Identity,
        Pyramid::Math::Vec3(5.0f, 0.10f, 5.0f));
    Pyramid::MeshRendererComponent floorRenderer;
    floorRenderer.mesh = floorMeshHandle;
    floorRenderer.material = floorMaterialHandle;
    if (!floorEntity.SetMeshRenderer(floorRenderer, resources))
    {
        return false;
    }

    return true;
}

bool BasicGame::SetupInputActions()
{
    auto* context = GetInputActions().CreateContext(std::string(kInputContext), 0, true);
    if (!context)
    {
        return false;
    }

    bool valid = true;
    valid = context->AddAction(std::string(kQuitAction), Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kQuitAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::F10)) && valid;
    valid = context->AddAction(
        std::string(kPauseAction),
        Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kPauseAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::Escape)) && valid;
    valid = context->AddAction(
        std::string(kToggleDebugUIAction),
        Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kToggleDebugUIAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::F1)) && valid;
    valid = context->AddAction(
        std::string(kToggleAnimationAction),
        Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kToggleAnimationAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::Space)) && valid;
    valid = context->AddAction(
        std::string(kResetCameraAction),
        Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kResetCameraAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::R)) && valid;

    valid = context->AddAction(std::string(kMoveAction), Pyramid::InputActionType::Axis2D) && valid;
    valid = context->AddBinding(
        kMoveAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::A, -1.0f, Pyramid::InputAxisComponent::X)) && valid;
    valid = context->AddBinding(
        kMoveAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::D, 1.0f, Pyramid::InputAxisComponent::X)) && valid;
    valid = context->AddBinding(
        kMoveAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::W, 1.0f, Pyramid::InputAxisComponent::Y)) && valid;
    valid = context->AddBinding(
        kMoveAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::S, -1.0f, Pyramid::InputAxisComponent::Y)) && valid;

    valid = context->AddAction(std::string(kOrbitDeltaAction), Pyramid::InputActionType::Axis2D) && valid;
    auto orbitX = Pyramid::InputBinding::MouseDeltaXBinding(
        1.0f,
        Pyramid::InputAxisComponent::X);
    orbitX.RequireMouseButton(Pyramid::MouseButton::Middle);
    auto orbitY = Pyramid::InputBinding::MouseDeltaYBinding(
        -1.0f,
        Pyramid::InputAxisComponent::Y);
    orbitY.RequireMouseButton(Pyramid::MouseButton::Middle);
    valid = context->AddBinding(kOrbitDeltaAction, orbitX) && valid;
    valid = context->AddBinding(kOrbitDeltaAction, orbitY) && valid;

    valid = context->AddAction(std::string(kOrbitRateAction), Pyramid::InputActionType::Axis2D) && valid;
    valid = context->AddBinding(
        kOrbitRateAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::Q, -1.0f, Pyramid::InputAxisComponent::X)) && valid;
    valid = context->AddBinding(
        kOrbitRateAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::E, 1.0f, Pyramid::InputAxisComponent::X)) && valid;

    valid = context->AddAction(std::string(kZoomDeltaAction), Pyramid::InputActionType::Axis1D) && valid;
    valid = context->AddBinding(
        kZoomDeltaAction,
        Pyramid::InputBinding::MouseWheelBinding(-1.0f)) && valid;

    valid = context->AddAction(std::string(kBoostAction), Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kBoostAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::LeftShift)) && valid;
    valid = context->AddBinding(
        kBoostAction,
        Pyramid::InputBinding::KeyBinding(Pyramid::Key::RightShift)) && valid;

    valid = context->AddAction(std::string(kSelectAction), Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kSelectAction,
        Pyramid::InputBinding::MouseButtonBinding(Pyramid::MouseButton::Left)) && valid;

    valid = context->AddAction(std::string(kCommandAction), Pyramid::InputActionType::Button) && valid;
    valid = context->AddBinding(
        kCommandAction,
        Pyramid::InputBinding::MouseButtonBinding(Pyramid::MouseButton::Right)) && valid;

    return valid;
}

void BasicGame::onWindowResize(const Pyramid::WindowResizeEvent& event)
{
    Game::onWindowResize(event);
    if (m_interactionController && event.HasRenderableArea())
    {
        m_interactionController->SetViewportSize(
            static_cast<Pyramid::u32>(event.width),
            static_cast<Pyramid::u32>(event.height));
    }
    if (event.HasRenderableArea())
    {
        m_uiFrame.width = static_cast<float>(event.width);
        m_uiFrame.height = static_cast<float>(event.height);
    }
}
