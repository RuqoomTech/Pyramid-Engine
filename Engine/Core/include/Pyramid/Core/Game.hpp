#pragma once
#include <Pyramid/Graphics/GraphicsDevice.hpp>
#include <Pyramid/Platform/Window.hpp> // Added for std::unique_ptr<Window>
#include <Pyramid/Input/InputActions.hpp>
#include <Pyramid/UI/UI.hpp>
#include <memory>
#include <vector>

namespace Pyramid {

class Camera;
class ResourceRegistry;
namespace Renderer { class RenderSystem; }

/**
 * @file Game.hpp
 * @brief Main game engine class that manages the graphics device and game loop
 */

/**
 * @class Game
 * @brief Core game engine class that manages the game loop and graphics device
 *
 * This class serves as the main entry point for the game engine functionality.
 * It manages the graphics device and main game loop.
 * Inherit from this class to create your own game implementation.
 */
class Game
{
public:
    /**
     * @brief Constructor - Initializes the game engine components
     * @param api The graphics API to use (defaults to OpenGL)
     */
    explicit Game(GraphicsAPI api = GraphicsAPI::OpenGL);

    /**
     * @brief Destructor - Cleans up engine resources
     */
    virtual ~Game();

    /**
     * @brief Starts the main game loop
     * This function blocks until the game is quit
     */
    void run();

    /**
     * @brief Signals the game loop to stop
     * Call this to gracefully shut down the game
     */
    void quit();

    /**
     * @brief Check if the game engine was initialized successfully
     * @return true if the game engine is ready to run, false otherwise
     */
    bool IsInitialized() const;

protected:
    /**
     * @brief Called when the game engine is first created
     * Override this to initialize your game resources
     * @note Always call the base implementation first: Game::onCreate()
     */
    virtual void onCreate();

    /**
     * @brief Called every frame to update game logic
     * Override this to implement your game update logic
     * @param deltaTime Time elapsed since last update in seconds (clamped to prevent large jumps)
     * @note This should contain game logic only, not rendering operations
     */
    virtual void onUpdate(float deltaTime);

    /**
     * Uncapped monotonic elapsed seconds sampled for the current update frame.
     * Zero before run() samples a frame. Simulation consumers own pause/catch-up
     * policy; onUpdate's float argument remains capped at 1/30 second.
     */
    [[nodiscard]] f64 GetFrameElapsedSeconds() const;

    /**
     * @brief Called every frame to render the game
     * Override this to implement your game rendering
     * @note The base implementation clears the screen and presents the frame
     */
    virtual void onRender();

    /**
     * @brief Called when the native window client area changes size or state.
     * @param event New client dimensions and restored/minimized/maximized state.
     *
     * The callback runs during window-message processing on the game thread.
     * A minimized event can contain a zero-sized client area.
     */
    virtual void onWindowResize(const WindowResizeEvent& event);

    /**
     * @brief Gets the graphics device instance
     * @return IGraphicsDevice* Pointer to the graphics device
     */
    IGraphicsDevice* GetGraphicsDevice() const { return m_graphicsDevice.get(); }

    /**
     * @brief Get the engine-owned graphics resource registry.
     *
     * The registry is created with the graphics device and destroyed before
     * device shutdown, guaranteeing dependency-safe cache teardown.
     */
    ResourceRegistry* GetResourceRegistry() const { return m_resourceRegistry.get(); }

    /**
     * @brief Current keyboard and mouse state for the game window.
     *
     * Pressed/released flags and mouse/wheel deltas are valid for the current
     * update frame. Held states remain active until release or focus loss.
     */
    const InputState& GetInput() const;

    /** Native Unicode clipboard associated with the game window. */
    Clipboard* GetClipboard() const;

    /**
     * @brief Engine-generic named action and input-context system.
     *
     * Configure contexts and bindings during onCreate(). Game evaluates them
     * once per frame after native input messages and before onUpdate().
     */
    InputActionSystem& GetInputActions() { return m_inputActions; }
    const InputActionSystem& GetInputActions() const { return m_inputActions; }

    /**
     * @brief Register the camera whose projection follows the window client size.
     * @param camera Non-owning camera pointer, or nullptr to detach it.
     *
     * The camera is immediately synchronized to the current renderable client
     * area and remains synchronized across restore/maximize resize events.
     */
    void SetActiveCamera(Camera* camera);

    /**
     * @brief Get the camera currently synchronized with the render surface.
     */
    Camera* GetActiveCamera() const { return m_activeCamera; }

    /**
     * @brief Register the render system whose window-sized targets follow the client area.
     * @param renderSystem Non-owning pointer, or nullptr to detach it.
     *
     * The render system is immediately synchronized to the current renderable
     * client area and receives subsequent restore/maximize resize events.
     */
    void SetRenderSystem(Renderer::RenderSystem* renderSystem);

    Renderer::RenderSystem* GetRenderSystem() const { return m_renderSystem; }

    /**
     * Register a non-owning UI context for pre-action input capture. The context
     * must outlive its registration and should be unregistered before destruction.
     */
    void RegisterUIContext(UI::Context* context);
    void UnregisterUIContext(UI::Context* context);

    /** Physical controls reserved by registered UI contexts for this frame. */
    const InputConsumptionMask& GetUIInputConsumption() const
    {
        return m_uiInputConsumption;
    }

    /**
     * @brief Whether the window currently has a non-zero renderable client area.
     */
    bool IsRenderSurfaceAvailable() const { return m_renderSurfaceAvailable; }

private:
    void HandleWindowResize(const WindowResizeEvent& event);

    std::unique_ptr<Window> m_window;
    std::unique_ptr<IGraphicsDevice> m_graphicsDevice;
    std::unique_ptr<ResourceRegistry> m_resourceRegistry;
    InputActionSystem m_inputActions;
    InputConsumptionMask m_uiInputConsumption;
    std::vector<UI::Context*> m_uiContexts;
    Camera* m_activeCamera;
    Renderer::RenderSystem* m_renderSystem;
    bool m_isRunning;
    bool m_initialized; // Track if initialization was successful
    bool m_renderSurfaceAvailable;
    f64 m_frameElapsedSeconds = 0.0;
};

} // namespace Pyramid
