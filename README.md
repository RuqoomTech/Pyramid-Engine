# Pyramid Engine

Pyramid Engine is a Windows-first, C++17 game-engine project built around a Win32/WGL platform layer and an OpenGL renderer.

**Current development version:** `0.6.0-pre-alpha`

The project is suitable for engine development and experimentation. It is not yet a stable SDK or production-ready game engine.

## Status

| Area | Current state |
|---|---|
| Platform | Windows 10/11 x64 only |
| Graphics | OpenGL 3.3 core or newer |
| Toolchain | Visual Studio 2022 and CMake 3.23+ |
| Renderer | Forward, shadow, and deferred passes; several advanced paths remain partial |
| Scene | Scene graph, render objects, lights, scene manager, and octree queries |
| Math | Vectors, matrices, quaternions, geometry helpers, and SIMD-oriented utilities |
| Images | TGA/BMP subsets, non-interlaced PNG, and experimental JPEG components |
| Tests | 11 CTest targets: 10 utility tests and one public-API linkage test |
| CI | Windows Debug and Release build, test, install, and external-consumer validation |

## Implemented

- Application lifecycle and frame loop through `Pyramid::Game`.
- Win32 window creation, resizing, visibility, positioning, and WGL context management.
- OpenGL device, buffers, vertex arrays, shaders, textures, framebuffers, and state caching.
- Forward, cascaded-shadow, deferred-geometry, and deferred-lighting passes.
- Perspective and orthographic cameras.
- Scene objects, lights, hierarchy nodes, scene management, octree storage, and spatial queries.
- Logging, assertions, image loading, compression utilities, and math primitives.
- Installable CMake package exported as `Pyramid::Engine`.

## Important limitations

- DirectX and Vulkan enum values are reserved; only OpenGL is implemented.
- Linux and macOS builds are rejected explicitly.
- Compute dispatch is recorded by the command buffer but is not executed by OpenGL.
- Scene persistence methods fail explicitly because serialization is not implemented.
- Occlusion culling and frustum-plane extraction remain placeholder algorithms.
- `ITexture2D::CreateDepthTarget` fails explicitly; use the framebuffer API for depth attachments.
- JPEG parsing and helper stages exist, but the loader still generates a test pattern instead of decoding real image blocks.
- Input, audio, and physics modules are not part of the current source tree.

See [Roadmap and known issues](docs/ROADMAP.md) before building new systems on top of the engine.

## Build

### Requirements

- Windows 10 or 11, x64
- Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK
- CMake 3.23 or newer
- GPU and driver supporting OpenGL 3.3 core or newer

### Debug build

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

### Tests

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests
ctest --preset test-debug
```

Release validation is also available:

```powershell
cmake --preset vs2022-release-tests
cmake --build --preset build-release-tests
ctest --preset test-release
```

### Graphical smoke test

```powershell
./scripts/run-smoke.ps1 -BuildDir build/debug -Config Debug -DurationSeconds 5
```

This catches early process failure. It does not replace visual inspection or render-image regression testing.

## Minimal application

```cpp
#include <Pyramid/Core/Game.hpp>

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

A derived `onCreate()` must call `Game::onCreate()` before creating GPU resources.

## Install and consume

```powershell
cmake --preset vs2022-release-tests
cmake --build --preset build-release-tests
cmake --install build/release-tests --config Release --prefix install
```

External CMake project:

```cmake
find_package(PyramidEngine CONFIG REQUIRED)
add_executable(MyGame main.cpp)
target_link_libraries(MyGame PRIVATE Pyramid::Engine)
```

The CI workflow validates this package-consumer path.

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
Engine/
  Core/       Application lifecycle and common types
  Graphics/   OpenGL resources, renderer, camera, scenes, and octree
  Math/       Math and SIMD-oriented utilities
  Platform/   Win32/WGL implementation
  Utils/      Logging, compression, and image loading
Examples/     BasicGame and BasicRenderingExample
Tests/        Public API linkage and external package consumer
CMake/        Package configuration template
scripts/      Clean configure and graphical smoke helpers
docs/         Maintained documentation
vendor/glad/  Bundled OpenGL/WGL loader
```

## License

Pyramid Engine is licensed under the [MIT License](LICENSE).
