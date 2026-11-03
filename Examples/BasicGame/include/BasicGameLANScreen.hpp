#pragma once

#include <Pyramid/UI/GameUI.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace Pyramid::Examples::BasicGameUI
{
    /** Game-owned menus built on the shared, renderer-independent UI runtime. */
    class MainMenuScreen final : public UI::Screen
    {
    public:
        MainMenuScreen(
            std::function<void()> start,
            std::function<void()> lan,
            std::function<void()> settings,
            std::function<void()> quit,
            std::function<std::string()> status)
            : m_start(std::move(start))
            , m_lan(std::move(lan))
            , m_settings(std::move(settings))
            , m_quit(std::move(quit))
            , m_status(std::move(status))
        {
        }

        std::string_view GetName() const override { return "MainMenu"; }
        UI::ScreenPresentation GetPresentation() const override
        {
            return UI::ScreenPresentation::Opaque;
        }

        void Build(UI::Context& ui) override
        {
            const auto& frame = ui.GetFrameInfo();
            const auto& theme = ui.GetTheme();
            ui.Overlay("MAIN MENU BACKDROP", Color(0.025f, 0.03f, 0.05f, 0.90f), true);
            const UI::Rect rect = UI::ResolveAnchoredRect(
                {0.0f, 0.0f, frame.width, frame.height},
                Math::Vec2(440.0f, 460.0f), UI::Anchor::Center,
                {18.0f, 18.0f, 18.0f, 18.0f});
            UI::PanelOptions panel;
            panel.position = Math::Vec2(rect.x, rect.y);
            panel.size = Math::Vec2(rect.width, rect.height);
            if (!ui.BeginPanel("PYRAMID ENGINE", panel))
            {
                return;
            }

            // Fixed actions stay visible even when diagnostics or translated text grows.
            if (ui.Button("START GAME") && m_start)
            {
                m_start();
            }
            if (ui.Button("LAN MULTIPLAYER") && m_lan)
            {
                m_lan();
            }
            if (ui.Button("PROFILE SETTINGS") && m_settings)
            {
                m_settings();
            }
            if (ui.Button("QUIT") && m_quit)
            {
                m_quit();
            }

            UI::ScrollAreaOptions details;
            details.height = (std::max)(1.0f, rect.height - theme.defaultRowHeight -
                2.0f * theme.padding - theme.spacing -
                4.0f * (theme.defaultRowHeight + theme.spacing));
            if (ui.BeginScrollArea("MAIN MENU DETAILS", details))
            {
                if (m_status)
                {
                    ui.WrappedLabel(m_status());
                }
                ui.WrappedLabel("Owned retained game screens running on the same UI runtime as the debug tools.");
                ui.Caption("F1  DEBUG OVERLAY");
                ui.Caption("ESC  OPEN MATCH MENU");
                ui.Label(u8"UNICODE  é  Ω  ✓");
                ui.Label(u8"ARABIC  مملكة الهرم ١٢٣");
                ui.WrappedLabel(u8"MIXED BIDI  Kingdom 123 - مملكة الهرم");
                ui.EndScrollArea();
            }
            ui.EndPanel();
        }

    private:
        std::function<void()> m_start;
        std::function<void()> m_lan;
        std::function<void()> m_settings;
        std::function<void()> m_quit;
        std::function<std::string()> m_status;
    };

    class LANMenuScreen final : public UI::Screen
    {
    public:
        LANMenuScreen(
            std::function<bool(const std::string&, std::string&)> host,
            std::function<bool(const std::string&, const std::string&, std::string&)> join,
            std::function<void()> back,
            std::function<std::string()> status)
            : m_host(std::move(host))
            , m_join(std::move(join))
            , m_back(std::move(back))
            , m_status(std::move(status))
        {
        }

        std::string_view GetName() const override { return "LAN"; }
        UI::ScreenPresentation GetPresentation() const override
        {
            return UI::ScreenPresentation::Modal;
        }

        void Build(UI::Context& ui) override
        {
            const auto& frame = ui.GetFrameInfo();
            const auto& theme = ui.GetTheme();
            ui.Overlay("LAN BACKDROP", Color(0.01f, 0.015f, 0.025f, 0.78f), true);
            const UI::Rect rect = UI::ResolveAnchoredRect(
                {0.0f, 0.0f, frame.width, frame.height},
                Math::Vec2(520.0f, 480.0f), UI::Anchor::Center,
                {16.0f, 16.0f, 16.0f, 16.0f});
            UI::PanelOptions panel;
            panel.position = Math::Vec2(rect.x, rect.y);
            panel.size = Math::Vec2(rect.width, rect.height);
            if (!ui.BeginPanel("TWO-PLAYER LAN", panel))
            {
                return;
            }

            UI::TextFieldOptions address;
            address.maximumCharacters = 15;
            address.placeholder = "127.0.0.1";
            address.hasError = !m_error.empty();
            (void)ui.TextField("SERVER IPV4", m_address, address);
            UI::TextFieldOptions port;
            port.maximumCharacters = 5;
            port.placeholder = "7777";
            port.hasError = !m_error.empty();
            (void)ui.TextField("PORT", m_port, port);
            if (ui.Button("HOST LAN") && m_host)
            {
                m_error.clear();
                (void)m_host(m_port, m_error);
            }
            if (ui.Button("JOIN LAN") && m_join)
            {
                m_error.clear();
                (void)m_join(m_address, m_port, m_error);
            }
            if (ui.Button("BACK") && m_back)
            {
                m_back();
            }

            // Each text field reserves a row plus its 8-pixel editor and 17-pixel label.
            UI::ScrollAreaOptions details;
            details.height = (std::max)(1.0f, rect.height - theme.defaultRowHeight -
                2.0f * theme.padding - theme.spacing -
                2.0f * (theme.defaultRowHeight + 25.0f + theme.spacing) -
                3.0f * (theme.defaultRowHeight + theme.spacing));
            if (ui.BeginScrollArea("LAN DETAILS", details))
            {
                const std::string status = m_status ? m_status() : std::string{};
                if (!m_error.empty())
                {
                    const Color errorColor(1.0f, 0.38f, 0.38f, 1.0f);
                    ui.WrappedLabel(m_error, {}, &errorColor);
                }
                if (!status.empty() && (m_error.empty() ||
                    status.find(m_error) == std::string::npos))
                {
                    ui.WrappedLabel(status);
                }
                ui.WrappedLabel("Host a match or join a host by its numeric IPv4 address.");
                ui.WrappedLabel("HOST LAN listens on all IPv4 interfaces.");
                ui.EndScrollArea();
            }
            ui.EndPanel();
        }

    private:
        std::function<bool(const std::string&, std::string&)> m_host;
        std::function<bool(const std::string&, const std::string&, std::string&)> m_join;
        std::function<void()> m_back;
        std::function<std::string()> m_status;
        std::string m_address = "127.0.0.1";
        std::string m_port = "7777";
        std::string m_error;
    };
}
