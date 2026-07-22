# Pyramid Engine

Pyramid Engine is a Windows-first, C++17 game-engine project built around a Win32/WGL platform layer and an OpenGL renderer.

**Current development version:** `0.6.0-pre-alpha`

The project is intended for engine development and experimentation. It is not yet a stable SDK or production-ready game engine.

## Status

| Area | Current state |
|---|---|
| Platform | Windows 10/11 x64 only |
| Graphics | OpenGL 3.3 core or newer |
| Toolchain | MSYS2 UCRT64, MinGW-w64 GCC, Ninja, and CMake 3.23+ |
| Optional compiler | Clang targeting the same MinGW-w64/UCRT runtime |
| Renderer | Forward, shadow, and deferred passes; several advanced paths remain partial |
| Scene | Scene graph, render objects, lights, scene manager, and octree queries |
| Math | Vectors, matrices, quaternions, geometry helpers, and SIMD-oriented utilities |
| Images | TGA/BMP subsets, custom non-interlaced PNG, and libjpeg-turbo JPEG decoding |
| Tests | 16 CTest targets: 5 image/utility tests plus API linkage and focused graphics/platform coverage |
| CI | GCC and Clang, Debug and Release, package install, and external-consumer validation |

## Implemented

- Application lifecycle and frame loop through `Pyramid::Game`.
- Win32 window creation, resize-event delivery, resize-safe viewport updates, visibility, positioning, and WGL context management.
- OpenGL device, buffers, vertex arrays, shaders, textures, resize-safe framebuffers, and state caching.
- Forward, cascaded-shadow, deferred-geometry, and deferred-lighting passes.
- Perspective and orthographic cameras with normalized world-space frusta and point/sphere/AABB visibility tests.
- Bounds-aware point, sphere, box, ray, nearest-object, and K-nearest scene queries with octree/linear parity.
- Scene objects, lights, cycle-safe hierarchy nodes, transformed object bounds, scene management, incrementally synchronized octree storage, bounds-accurate point/sphere/box/ray queries, and frustum-aware visibility queries.
- OpenGL driver debug callbacks and centralized error diagnostics in Debug builds.
- Logging, assertions, image loading, compression utilities, and math primitives.
- Installable CMake package exported as `Pyramid::Engine`.

## Important limitations

- DirectX and Vulkan enum values are reserved; only OpenGL is implemented.
- Linux and macOS builds are rejected explicitly.
- Compute dispatch is recorded by the command buffer but is not executed by OpenGL.
- Scene persistence methods fail explicitly because serialization is not implemented.
- Occlusion culling remains a placeholder and is disabled by default.
- `ITexture2D::CreateDepthTarget` fails explicitly; use the framebuffer API for depth attachments.
- JPEG decoding requires the open-source libjpeg-turbo package installed by the MSYS2 bootstrap script.
- Input, audio, and physics modules are not part of the current source tree.

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

External CMake project:

```cmake
find_package(PyramidEngine CONFIG REQUIRED)
add_executable(MyGame main.cpp)
target_link_libraries(MyGame PRIVATE Pyramid::Engine)
```

The CI workflow validates this package-consumer path with both GCC and Clang.

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
Engine/
  Core/       Application lifecycle and common types
  Graphics/   OpenGL resources, renderer, camera, scenes, and octree
  Math/       Math and SIMD-oriented utilities
  Platform/   Win32/WGL implementation
  Utils/      Logging, compression, and image loading
Examples/     BasicGame and BasicRenderingExample
Tests/        Public API linkage and external package consumer
CMake/        Package configuration template
scripts/      MSYS2 setup, builds, clean configure, and smoke tests
docs/         Maintained documentation
vendor/glad/  Bundled OpenGL/WGL loader
```

## License

Pyramid Engine is licensed under the [MIT License](LICENSE).
