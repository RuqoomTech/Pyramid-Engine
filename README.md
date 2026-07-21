# Pyramid Engine

Pyramid is an experimental C++17 game-engine codebase focused on a native Win32/OpenGL renderer, engine-owned math and image-loading utilities, and a small scene/rendering framework.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)](#current-status)
[![Renderer](https://img.shields.io/badge/renderer-OpenGL-green.svg)](#current-status)

> **Maturity:** pre-alpha. The repository is suitable for engine development and experimentation, not production games.

## Current status

This documentation was verified against the source tree on **July 21, 2026**.

| Area | Current state |
|---|---|
| Runtime platform | Windows only: Win32 windowing and WGL context creation |
| Graphics backend | OpenGL only; `IGraphicsDevice::Create` rejects the other declared APIs |
| Language and build | C++17, CMake, Visual Studio 2022 x64 presets |
| OpenGL level | The Win32 path attempts core contexts from 4.6 down to 3.3, then can retain its temporary legacy context if modern creation fails; bundled examples use GLSL 4.60 and require OpenGL 4.6 |
| Project version | `CMakeLists.txt` declares 0.3.3; `CHANGELOG.md` contains milestones through 0.6.0. These must be reconciled before a release |
| Automated tests | Eight utility test executables are registered with CTest when `PYRAMID_BUILD_TESTS=ON` |
| Continuous integration | Not included in this snapshot |

### Implemented

- Core `Game` lifecycle and variable-timestep loop.
- Win32 window and OpenGL context creation.
- OpenGL device abstraction, buffers, vertex arrays, shaders, textures, framebuffer utilities, and cached render state.
- Command-buffer recording plus forward, shadow, and deferred render-pass implementations.
- Camera, scene objects, lights, scene graph support, octree spatial queries, and frustum-query integration.
- Vector, matrix, quaternion, geometry, and SIMD helper code.
- In-tree TGA/BMP loaders, a custom PNG path, and experimental JPEG parser/decoder components.
- Configurable console/file logging with rotation, structured fields, and assertions.
- `BasicGame` and `BasicRenderingExample` sample applications.

### Partial or unavailable

- `Audio`, `Input`, and `Physics` are placeholder modules with no public implementation.
- DirectX and Vulkan values exist in `GraphicsAPI`, but no backend is implemented.
- Compute dispatch is recorded but not sent to OpenGL.
- Generic `IFramebuffer` binding is incomplete; handle-based binding is the working path.
- Several public declarations have no definitions, including parts of `SceneManager`, optional render-pass classes, and texture convenience factories.
- Scene transform updates, occlusion culling, and some mesh-generation paths are placeholders.
- JPEG loading does not decode real image blocks yet; it currently returns a generated test pattern after parsing.
- Linux and macOS are not build targets in the current CMake configuration.

See [Roadmap and known issues](docs/ROADMAP.md) for prioritized work.

## Build

### Requirements

- Windows 10 or 11, x64
- Visual Studio 2022 with **Desktop development with C++** and a Windows SDK
- CMake 3.23 or newer for the included presets
- A GPU/driver supporting OpenGL 4.6 to run the bundled examples

### Configure and build

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

Build and run the registered tests:

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests-clean
ctest --preset test-debug
```

Run the graphical smoke test after building:

```powershell
./scripts/run-smoke.ps1 -BuildDir build -Config Debug -DurationSeconds 5
```

For output paths, CMake options, installation, and troubleshooting, read [Building and testing](docs/BUILDING.md).

## Minimal application

```cpp
#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Graphics/GraphicsDevice.hpp>

class MyGame final : public Pyramid::Game
{
protected:
    void onCreate() override
    {
        Game::onCreate();
        if (!IsInitialized())
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
};

int main()
{
    MyGame game;
    game.run();
}
```

Derived `onCreate()` implementations must call `Game::onCreate()` before creating GPU resources. A custom `onRender()` must present the frame itself, or use `Pyramid::Renderer::RenderSystem::EndFrame()`.

## Documentation

- [Documentation index](docs/README.md)
- [Building and testing](docs/BUILDING.md)
- [Architecture](docs/ARCHITECTURE.md)
- [API overview](docs/API.md)
- [Examples](docs/EXAMPLES.md)
- [Development guide](docs/DEVELOPMENT.md)
- [Roadmap and known issues](docs/ROADMAP.md)
- [Changelog](CHANGELOG.md)

## Repository layout

```text
Engine/                 PyramidEngine library
  Core/                 Game lifecycle and common types
  Graphics/             OpenGL backend, rendering, camera, and scenes
  Math/                 Math and SIMD utilities
  Platform/             Win32/WGL platform implementation
  Utils/                Logging and image loading
  Audio|Input|Physics/  Reserved placeholder modules
Examples/               Runnable sample applications
scripts/                Configure and smoke-test helpers
docs/                   Maintained project documentation
vendor/glad/             Bundled OpenGL/WGL loader
```

## License

Pyramid Engine is licensed under the [MIT License](LICENSE).
