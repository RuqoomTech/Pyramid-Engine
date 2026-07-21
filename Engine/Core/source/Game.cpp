#include "Pyramid/Core/Game.hpp"
#include "Pyramid/Platform/Windows/Win32OpenGLWindow.hpp"
#include <Pyramid/Util/Log.hpp>
#include <memory>
#include <chrono>
#include <algorithm>

namespace Pyramid
{

    Game::Game(GraphicsAPI api)
        : m_window(nullptr)
        , m_graphicsDevice(nullptr)
        , m_isRunning(false)
        , m_initialized(false)
    {
        PYRAMID_LOG_INFO("Initializing Pyramid Game Engine...");
        
        // Create window first
        if (api == GraphicsAPI::OpenGL)
        {
            m_window = std::make_unique<Win32OpenGLWindow>();
        }
        else
        {
            PYRAMID_LOG_ERROR("Unsupported graphics API: ", static_cast<int>(api));
            return;
        }

        if (!m_window)
        {
            PYRAMID_LOG_CRITICAL("Failed to create window");
            return;
        }

        // Initialize the window
        if (!m_window->Initialize("Pyramid Game Engine", 1280, 720))
        {
            PYRAMID_LOG_CRITICAL("Failed to initialize window");
            m_window.reset();
            return;
        }

        // Create graphics device, passing the window
        m_graphicsDevice = IGraphicsDevice::Create(api, m_window.get());
        if (!m_graphicsDevice)
        {
            PYRAMID_LOG_CRITICAL("Failed to create graphics device");
            return;
        }

        m_window->SetResizeCallback(
            [this](const WindowResizeEvent& event)
            {
                onWindowResize(event);
            });

        PYRAMID_LOG_INFO("Game engine initialized successfully");
        m_initialized = true;
    }

    Game::~Game()
    {
        PYRAMID_LOG_INFO("Shutting down Pyramid Game Engine...");

        if (m_window)
        {
            m_window->SetResizeCallback({});
        }
        
        // Explicitly shutdown graphics device before window destruction
        if (m_graphicsDevice)
        {
            m_graphicsDevice->Shutdown();
        }
        
        // Smart pointers will handle cleanup automatically
        // Order: m_graphicsDevice first, then m_window (based on declaration order)
        
        PYRAMID_LOG_INFO("Game engine shutdown complete");
    }

    void Game::onCreate()
    {
        PYRAMID_LOG_INFO("Calling Game::onCreate()");
        
        // Validate that construction was successful
        if (!m_window)
        {
            PYRAMID_LOG_CRITICAL("Window is null - constructor may have failed");
            m_isRunning = false;
            return;
        }
        
        if (!m_graphicsDevice)
        {
            PYRAMID_LOG_CRITICAL("Graphics device is null - constructor may have failed");
            m_isRunning = false;
            return;
        }
        
        // Initialize graphics device
        if (!m_graphicsDevice->Initialize())
        {
            PYRAMID_LOG_CRITICAL("Graphics device failed to initialize!");
            m_isRunning = false;
            m_initialized = false;  // Mark as not initialized
            return;
        }
        
        PYRAMID_LOG_INFO("Game onCreate completed successfully");
        m_isRunning = true;
    }

    void Game::onUpdate(float deltaTime)
    {
        (void)deltaTime;
        // Default implementation - base classes can override for game logic
        // Note: Screen clearing should be done in onRender(), not here
    }

    void Game::onRender()
    {
        // Default implementation - clear screen and present
        if (m_graphicsDevice)
        {
            m_graphicsDevice->Clear(Color(0.2f, 0.3f, 0.3f, 1.0f));
            m_graphicsDevice->Present(true);
        }
    }

    void Game::onWindowResize(const WindowResizeEvent& event)
    {
        if (event.state == WindowResizeState::Minimized)
        {
            PYRAMID_LOG_DEBUG("Window minimized");
            return;
        }

        const char* state =
            event.state == WindowResizeState::Maximized ? "maximized" : "restored";
        PYRAMID_LOG_DEBUG(
            "Window resized to ", event.width, "x", event.height, " (", state, ")");
    }

    void Game::run()
    {
        PYRAMID_LOG_INFO("Starting game loop...");
        
        // Check if the game was properly initialized
        if (!IsInitialized())
        {
            PYRAMID_LOG_ERROR("Game engine not properly initialized. Cannot start game loop.");
            return;
        }
        
        // Call onCreate which handles graphics device initialization
        onCreate();

        if (!m_isRunning)
        {
            PYRAMID_LOG_ERROR("Game::onCreate failed. Aborting run loop.");
            return;
        }

        // Initialize timing
        auto lastTime = std::chrono::high_resolution_clock::now();
        const float maxDeltaTime = 1.0f / 30.0f; // Cap at 30 FPS minimum (33ms max frame time)

        PYRAMID_LOG_INFO("Entering main game loop");

        while (m_isRunning)
        {
            // Process window messages
            if (!m_window || !m_window->ProcessMessages())
            {
                PYRAMID_LOG_INFO("Window close requested");
                m_isRunning = false;
                break;
            }

            // Calculate frame time
            auto currentTime = std::chrono::high_resolution_clock::now();
            float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
            lastTime = currentTime;

            // Clamp delta time to prevent large jumps (e.g., when debugging or system lag)
            deltaTime = (std::min)(deltaTime, maxDeltaTime);

            // Update game logic
            onUpdate(deltaTime);
            
            // Render frame
            onRender();
        }

        PYRAMID_LOG_INFO("Game loop ended");
    }

    void Game::quit()
    {
        PYRAMID_LOG_INFO("Game quit requested");
        m_isRunning = false;
    }

    bool Game::IsInitialized() const
    {
        return m_initialized && m_window && m_graphicsDevice;
    }

} // namespace Pyramid
