<p align="center">
  <img src="assets/branding/pyramid-engine-master-v1.png" width="144" alt="Pyramid Engine logo">
</p>

# Pyramid Engine

Pyramid Engine is a Windows-first, general-purpose C++17 game-engine project built around a Win32/WGL platform layer and an OpenGL renderer. A future Ruqoom RTS is the first proving game, not a constraint on the engine architecture.

**Current development version:** `0.6.0-pre-alpha`

## Eco ecosystem role

Pyramid Engine is an independently released Eco runtime consumer. It is not a
dependency of Baa, Nazm, Takween, Qalam, or ArbSh, and those tools do not replace
its current CMake/C++ reference build. Its first Baa integration is a future,
gameplay-only scripting experiment after the hosted ABI/FFI, runtime ownership,
debugger, and hot-reload contracts are explicitly admitted.

The engine's Arabic text and UI work may consume shared logical fixtures from
the planned `eco-arabic-text-corpus-v1`; this shares behavior tests rather than
forcing a C# or Qt runtime dependency into the engine.

The project is intended for engine development and experimentation. It is not yet a stable SDK or production-ready game engine.

## Status

| Area | Current state |
|---|---|
| Platform | Windows 10/11 x64 only |
| Graphics | OpenGL 3.3 core or newer |
| Toolchain | MSYS2 UCRT64, MinGW-w64 GCC, Ninja, and CMake 3.23+ |
| Optional compiler | Clang targeting the same MinGW-w64/UCRT runtime |
| Renderer | Forward, shadow, and deferred passes; several advanced paths remain partial |
| Scene | Stable-ID entities, hierarchical transforms, optional mesh-renderer/light components, scene serialization, and octree queries |
| Foundation | Standalone `Pyramid::Foundation` types, colors, assertions, and logging |
| Math | Standalone `Pyramid::Math` vectors, matrices, quaternions, geometry helpers, and SIMD utilities |
| Input | Standalone `Pyramid::Input` physical state, Unicode text events, clipboard abstraction, and named action mapping |
| Images | Standalone `Pyramid::Image` library with TGA/BMP subsets, custom non-interlaced PNG, and owned baseline/progressive JPEG decoding |
| Models | Standalone `Pyramid::Model` OBJ/MTL parser with bounded diagnostics, triangulation, indexing, generated normals, and normalized dependencies |
| Text | Standalone `Pyramid::Text` UTF conversion, grapheme-safe editing, owned Arabic contextual shaping, common bidirectional visual runs, fallback font families, international wrapping, and renderer-neutral glyph placement |
| Font | Standalone `Pyramid::Font` TrueType-outline parsing, coverage/SDF rasterization, content-addressed processed-font caching, deterministic `.pfont` assets, kerning, and bounded malformed-input rejection |
| UI | Standalone hybrid `Pyramid::UI` runtime with retained screens, typography roles, scalable SDF text, immediate debug tools, editable controls, modal input blocking, clipping, responsive layout, and renderer-independent draw lists |
| Tests | 57 Windows CTest targets, including standalone foundation, math, input, image, model, font, text, UI, runtime-path, and installed-system-font coverage plus focused engine/reference-game tests |
| CI | GCC and Clang, Debug and Release, package install, and external-consumer validation |

## Implemented

- Application lifecycle and frame loop through `Pyramid::Game`.
- Real Win32 keyboard and mouse input with held/pressed/released states, pointer movement, wheel deltas, and focus-safe reset behavior.
- Engine-generic named input actions with button/1D/2D values, prioritized contexts, control consumption, chords, and runtime rebinding.
- Unicode committed-text events remain separate from physical keys; the Win32 backend combines UTF-16 surrogate pairs and exposes a bounded `CF_UNICODETEXT` clipboard service through the platform-neutral input package.
- Reusable camera-controller framework with free-fly, target-orbit, and optional XZ-ground-plane strategy controllers. Controllers consume configurable named actions and never own physical bindings.
- A game-side RTS reference interaction layer with focus-safe edge scrolling, ray-based selection, and ground-plane command requests; RTS semantics remain outside `Pyramid::Engine`.
- Win32 window creation, resize-event delivery, resize-safe viewport updates, visibility, positioning, and WGL context management.
- OpenGL device, a game-owned `ResourceRegistry`, generation-checked typed resource handles, versioned resource manifests and entity-scene serialization, engine-owned mesh and material resources, stable resource identifiers, content-deduplicating mesh, shader-program, texture, and material caches, buffers, vertex arrays, shaders, textures, resize-safe framebuffers, and state caching.
- Forward, cascaded-shadow, deferred-geometry, and deferred-lighting passes.
- Perspective and orthographic cameras with normalized world-space frusta and point/sphere/AABB visibility tests.
- Bounds-aware point, sphere, box, ray, nearest-object, and K-nearest scene queries with octree/linear parity.
- An authoritative entity/component scene: every entity has a stable ID, name, visibility, and hierarchical transform; mesh-renderer and light components are optional. Renderer/light proxies are derived from entities for rendering, culling, and spatial queries.
- OpenGL driver debug callbacks and centralized error diagnostics in Debug builds.
- Foundation types/logging, math primitives, physical/action input, image decoding, CPU model importing, TrueType/font assets, text layout, and UI layout/interaction are independently testable owned libraries.
- Dependency-free OBJ/MTL import supports positive/negative indices, polygon triangulation, material groups, common MTL properties, generated smooth or hard-edge normals, vertex deduplication, bounds, file/memory input, and bounded malformed-input diagnostics.
- `ModelResourceImporter` transactionally publishes imported meshes, diffuse textures, and immutable materials through the existing caches using a configurable shader/material profile; repeated content is reused and failed imports roll back only resources introduced by that operation.
- Hybrid `Pyramid::UI` contexts reconcile immediate widgets into retained state, while `ScreenStack` owns persistent opaque, transparent, and modal game screens with deferred lifecycle-safe routing.
- `Pyramid::Text::TextBuffer` keeps logical Unicode scalar offsets on extended grapheme boundaries, so caret movement and deletion do not split combining sequences, emoji ZWJ sequences, Hangul clusters, or regional-indicator pairs.
- `Pyramid::Text` builds ordered fallback font families into one renderer-ready atlas and emits cluster, caret, selection, bidi-run, and glyph geometry for common Arabic/Latin mixed text. `Pyramid::UI` uses that mapping for visual RTL caret movement, hit testing, selection, wrapping, password masking, and retained text controls.
- `Pyramid::UI::Typography` defines heading, body, label, button, input, and caption roles independently from widget geometry. A 64-pixel SDF source atlas can therefore remain crisp across normal UI sizes and DPI scales while the renderer applies role-specific optical weight without duplicating font textures.
- `UIRenderer` uploads batched quads through the graphics device, renders coverage or derivative-smoothed signed-distance text with per-role weight control, supports the active processed/fallback-family atlas or embedded emergency atlas, explicitly restores the final DPI-scaled surface viewport after off-screen passes, applies scissor clipping, and restores the engine render-state baseline.
- `BasicGame` includes a retained main menu with Arabic and mixed-direction validation text, a modal profile-settings form with Unicode text editing, responsive gameplay HUD, modal pause menu, and an F1 runtime overlay. On Windows it selects exact installed families per script: Segoe UI for Latin, Cairo when installed and compatible or Tahoma for Arabic, and Segoe UI Symbol for icons. Each candidate must pass the owned parser, required-glyph coverage, and a bounded script-specific SDF bake before it enters the cached fallback family. Ruqoom-owned 64-pixel SDF fonts remain deterministic bundled fallbacks, and all runtime paths are independent of the terminal working directory.
- Installable CMake packages export `Pyramid::Foundation`, `Pyramid::Math`, `Pyramid::Input`, `Pyramid::Image`, `Pyramid::Model`, `Pyramid::Font`, `Pyramid::Text`, `Pyramid::UI`, and `Pyramid::Engine`.

## Important limitations

- DirectX and Vulkan enum values are reserved; only OpenGL is implemented.
- Linux and macOS builds are rejected explicitly.
- The renderer command buffer exposes graphics commands only; it has no compute dispatch API. Lower-level shader compute APIs are separate from the validated OpenGL 3.3 reference path.
- `SceneSerializer` version 2 persists stable entities, hierarchy, transforms, mesh-renderer components, light components, and the primary light. Cameras, environment settings, gameplay components, and editor metadata are not serialized yet; the legacy `SceneManager` JSON/XML/Binary methods remain unsupported.
- Occlusion culling remains a placeholder and is disabled by default.
- Direct and cached memory textures support the uncompressed RGB/RGBA, red-channel, floating-point, and depth/depth-stencil formats; [CPU upload layouts](docs/API.md#textures) are explicit. BC compressed formats remain reserved and unsupported, file codecs still decode RGB/RGBA 8-bit pixels, and framebuffer renderability is validated separately. Runtime sampling/readback and visual verification of the expanded format set remain pending.
- Audio and physics modules are not part of the current source tree. The owned international-text foundation is intentionally bounded: it does not yet implement complete Unicode Bidirectional Algorithm controls/isolates, full UAX #14 line breaking, OpenType GSUB/GPOS, general complex-script shaping, advanced Arabic ligatures/mark positioning, or native IME pre-edit/candidate presentation. Style sheets, controller navigation, raw relative mouse mode, collision-aware cameras, camera blending, and persisted user bindings are also not implemented yet. The reference edge-scrolling/selection/command layer is example support rather than an installed engine API. Model import currently supports OBJ/MTL and diffuse `map_Kd` material publication only; additional material maps, model formats, asset packaging, and scene-hierarchy instantiation remain later steps.

See [Roadmap and known issues](docs/ROADMAP.md) before building new systems on top of the engine.

## Toolchain setup

Pyramid does **not** require Visual Studio or the Microsoft C++ compiler. The supported default is MSYS2 UCRT64 with MinGW-w64 GCC and Ninja.

1. Install MSYS2 to its default location: `C:\msys64`.
2. From PowerShell in the repository, install the toolchain:

```powershell
./scripts/bootstrap-msys2.ps1 -Compiler gcc
```

3. Either open **MSYS2 UCRT64** or use the PowerShell build wrapper below.

## Build from PowerShell

```powershell
./scripts/build-mingw.ps1 -Compiler gcc -Configuration Debug
```

The build copies the required MinGW runtime DLLs beside every executable in `build/gcc-debug-tests/bin`, so the `.exe` files can be launched directly from Explorer or PowerShell.
Launch the retained game-UI example directly with `./scripts/run-example.ps1 -Example BasicGame`.

That configures, builds, and runs all tests. A Release build is:

```powershell
./scripts/build-mingw.ps1 -Compiler gcc -Configuration Release
```

Optional Clang validation:

```powershell
./scripts/bootstrap-msys2.ps1 -Compiler both
./scripts/build-mingw.ps1 -Compiler clang -Configuration Debug
```

## Build from MSYS2 UCRT64

```bash
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug
```

Release validation:

```bash
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
ctest --preset test-gcc-release
```

### Graphical smoke test

Run this from PowerShell after a successful build:

```powershell
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5
```

This catches early process failure. It does not replace visual inspection or render-image regression testing.

## Minimal application

```cpp
#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Input/InputActions.hpp>

class MyGame final : public Pyramid::Game
{
protected:
    void onCreate() override
    {
        Game::onCreate();
        if (!IsInitialized() || !GetResourceRegistry())
        {
            quit();
            return;
        }

        auto* gameplay = GetInputActions().CreateContext("gameplay");
        if (!gameplay ||
            !gameplay->AddAction("Quit", Pyramid::InputActionType::Button) ||
            !gameplay->AddBinding(
                "Quit",
                Pyramid::InputBinding::KeyBinding(Pyramid::Key::Escape)))
        {
            quit();
        }
    }

    void onUpdate(float deltaTime) override
    {
        Game::onUpdate(deltaTime);
        if (GetInputActions().WasActionPressed("gameplay", "Quit"))
            quit();
    }

    void onRender() override
    {
        auto* device = GetGraphicsDevice();
        if (!device)
            return;

        device->Clear(Pyramid::Color(0.08f, 0.10f, 0.14f, 1.0f));
        device->Present(true);
    }

    void onWindowResize(const Pyramid::WindowResizeEvent& event) override
    {
        Game::onWindowResize(event);
        if (!event.HasRenderableArea())
            return;

        // The engine updates the default viewport automatically. Register a
        // camera with SetActiveCamera() and a RenderSystem with SetRenderSystem()
        // to synchronize their window-sized resources. Standalone framebuffers
        // can call OpenGLFramebuffer::Resize() from this hook.
    }
};

int main()
{
    MyGame game;
    game.run();
}
```

A derived `onCreate()` must call `Game::onCreate()` before creating GPU resources.

## Install and consume

```bash
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
cmake --install build/gcc-release-tests --prefix install
```

External engine project:

```cmake
find_package(PyramidEngine CONFIG REQUIRED)
add_executable(MyGame main.cpp)
target_link_libraries(MyGame PRIVATE Pyramid::Engine)
```

Owned libraries can be consumed without the engine or OpenGL package:

```cmake
find_package(PyramidFoundation CONFIG REQUIRED)
find_package(PyramidMath CONFIG REQUIRED)
find_package(PyramidInput CONFIG REQUIRED)
add_executable(MyTool main.cpp)
target_link_libraries(MyTool PRIVATE Pyramid::Foundation Pyramid::Math Pyramid::Input)
```

Standalone image and model tooling use the same package model:

```cmake
find_package(PyramidImage CONFIG REQUIRED)
find_package(PyramidModel CONFIG REQUIRED)
find_package(PyramidFont CONFIG REQUIRED)
find_package(PyramidUI CONFIG REQUIRED)
add_executable(MyAssetTool main.cpp)
target_link_libraries(MyAssetTool PRIVATE Pyramid::Image Pyramid::Model Pyramid::Font Pyramid::UI)
```

The CI workflow validates engine, foundation/math/input, image, model, font, and UI package-consumer paths with GCC and Clang.

## Documentation

- [Documentation index](docs/README.md)
- [Building and testing](docs/BUILDING.md)
- [Architecture](docs/Architecture.md)
- [API overview](docs/API.md)
- [Examples](docs/EXAMPLES.md)
- [Development guide](docs/DEVELOPMENT.md)
- [Roadmap and known issues](docs/ROADMAP.md)
- [Changelog](CHANGELOG.md)

## Repository layout

```text
Libraries/
  PyramidFoundation/  Primitive types, colors, assertions, and logging
  PyramidMath/        Vectors, matrices, quaternions, and SIMD utilities
  PyramidInput/       Physical input state and named action mapping
  PyramidImage/       Owned image parsing, PNG/DEFLATE, and JPEG decoding
  PyramidModel/       Dependency-free OBJ/MTL parsing and imported-model data
  PyramidFont/        TrueType parsing, rasterization, atlases, and `.pfont` assets
  PyramidText/        UTF-8 decoding, kerning, wrapping, and glyph placement
  PyramidUI/          Retained/immediate widgets, screens, layout, and draw lists
Engine/
  Core/               Application lifecycle
  Graphics/           OpenGL resources, renderer, camera, scenes, and octree
  Platform/           Win32/WGL window and context implementation
Examples/          BasicGame, BasicRenderingExample, and game-side RTS reference support
Tests/             Focused tests and external package consumers
CMake/        Package configuration template
scripts/      MSYS2 setup, builds, clean configure, and smoke tests
docs/         Maintained documentation
vendor/glad/  Bundled OpenGL/WGL loader
```

## License

Pyramid Engine is licensed under the [MIT License](LICENSE).
