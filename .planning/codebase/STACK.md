# Technology Stack

**Analysis Date:** 2026-05-14

## Languages

**Primary:**
- C++17 - Engine library, examples, and executable tests. The standard is set globally in `CMakeLists.txt:5-6` with `CMAKE_CXX_STANDARD 17` and `CMAKE_CXX_STANDARD_REQUIRED ON`.
- C - Vendored GLAD OpenGL/WGL loader implementation in `vendor/glad/src/glad.c` and `vendor/glad/src/glad_wgl.c`; `vendor/glad/src/glad_glx.c` is present but not built by `vendor/glad/CMakeLists.txt`.
- GLSL - Runtime shader assets in `Engine/Graphics/shaders/*.vert` and `Engine/Graphics/shaders/*.frag`; `Examples/BasicRendering/README.md` documents GLSL 4.60 usage.

**Secondary:**
- PowerShell - Local automation scripts in `scripts/configure-clean.ps1` and `scripts/run-smoke.ps1`.
- CMake language - Build graph in root `CMakeLists.txt`, module `CMakeLists.txt` files under `Engine/*/`, examples under `Examples/*/`, and `vendor/glad/CMakeLists.txt`.
- Markdown - Project and API documentation under `README.md`, `docs/`, and `.planning/codebase/`.

## Runtime

**Environment:**
- Windows desktop runtime - The active platform implementation is `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, which uses Win32 `HWND`, `HDC`, `HGLRC`, WGL context creation, `PeekMessage`, and `SwapBuffers`.
- x64 CPU with SSE2 minimum, AVX recommended - Documented in `README.md:50-53` and `docs/BuildGuide.md:7-19`; runtime CPU feature probes live in `Engine/Math/include/Pyramid/Math/MathSIMD.hpp` and `Engine/Math/source/MathSIMD.cpp`.
- GPU with OpenGL 3.3+ minimum - Documented in `README.md:52-53`; `Win32OpenGLWindow::CreateOpenGLContext()` in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:242-247` attempts OpenGL 4.6 down to 3.3.

**Package Manager:**
- None detected - There is no vcpkg manifest, Conan recipe, package-lock, or other package manager lockfile in the repository root.
- Vendored dependency strategy - `vendor/glad/` is committed and built from source.
- Lockfile: Not applicable.

## Build System

**CMake:**
- Minimum from root: CMake 3.16.0 (`CMakeLists.txt:1`).
- Preset file requires: CMake 3.23 (`CMakePresets.json:3-7`). Use CMake 3.23+ when using presets.
- Project version: `Pyramid` version `0.3.3` in `CMakeLists.txt:2`.
- Output directories: runtime artifacts go to `${CMAKE_BINARY_DIR}/bin`; libraries and archives go to `${CMAKE_BINARY_DIR}/lib` (`CMakeLists.txt:17-20`).
- Install rules copy module public headers from `Engine/Core/include`, `Engine/Graphics/include`, `Engine/Platform/include`, `Engine/Math/include`, `Engine/Utils/include`, `Engine/Renderer/include`, `Engine/Input/include`, `Engine/Scene/include`, `Engine/Audio/include`, and `Engine/Physics/include` (`CMakeLists.txt:41-46`).

**Build Options:**
- `PYRAMID_BUILD_TESTS` default `OFF` - Enables CTest at root and adds `Engine/Utils/test` through `Engine/Utils/CMakeLists.txt:25-28`.
- `PYRAMID_BUILD_EXAMPLES` default `ON` - Adds `Examples/BasicGame` and `Examples/BasicRendering` in `CMakeLists.txt:26-29`.
- `PYRAMID_BUILD_TOOLS` default `OFF` - Attempts `add_subdirectory(Tools/AssetProcessor)` in `CMakeLists.txt:31-33`; no `Tools/` directory is detected, so keep this option OFF.
- `PYRAMID_ENABLE_SIMD`, `PYRAMID_ENABLE_LOGGING`, and `PYRAMID_ENABLE_PROFILING` are documented in `docs/BuildGuide.md:237-245` but are not defined in root `CMakeLists.txt`; do not rely on them until implemented.

**Presets:**
- `vs2022-debug` - Visual Studio 17 2022 x64, examples ON, tests OFF, tools OFF (`CMakePresets.json:10-19`).
- `vs2022-debug-tests` - Inherits `vs2022-debug` and enables tests (`CMakePresets.json:22-27`).
- `build-debug`, `build-debug-clean`, and `build-debug-tests-clean` compile Debug configurations (`CMakePresets.json:30-47`).
- `test-debug` runs CTest for the `vs2022-debug-tests` configure preset (`CMakePresets.json:49-58`).

## Frameworks

**Core:**
- Custom C++ game engine framework - `Engine/Core/source/Game.cpp` implements `Pyramid::Game`, creates a `Win32OpenGLWindow`, creates an `IGraphicsDevice`, and runs the update/render loop.
- CMake target `PyramidEngine` - Created by `Engine/CMakeLists.txt:1`; all engine modules add sources and includes to this one library target.

**Graphics:**
- OpenGL/WGL - Active backend in `Engine/Graphics/source/OpenGL/` and `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`.
- GLAD - Generated loader target `glad` from `vendor/glad/CMakeLists.txt`; linked privately into `PyramidEngine` in `Engine/CMakeLists.txt:29-33`.
- GLSL shader pipeline - Runtime shader files in `Engine/Graphics/shaders/`; shader lookup is handled by `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp` using current path, source path, and `Engine/Graphics/shaders` fallback.

**Testing:**
- CTest - `enable_testing()` is called only when `PYRAMID_BUILD_TESTS=ON` in `CMakeLists.txt:13-15`; tests are registered with `add_test()` in `Engine/Utils/test/CMakeLists.txt:15`.
- Standalone C++ executables - `TestPNGComponents`, `TestJPEGSimple`, `TestJPEGHuffman`, `TestJPEGHuffmanDebug`, `TestJPEGDequantizer`, `TestJPEGIDCT`, `TestJPEGColorConverter`, and `TestJPEGIntegration` are defined in `Engine/Utils/test/CMakeLists.txt:18-25`.
- GUI smoke tests - `scripts/run-smoke.ps1` launches `BasicGame` and `BasicRenderingExample` and verifies they stay alive or exit successfully.

**Build/Dev:**
- Visual Studio 2022 / MSVC v143 - Required by `README.md:56-59` and `docs/BuildGuide.md:21-39`.
- Windows 10/11 SDK - Required for Win32, WGL, and `opengl32` usage (`docs/BuildGuide.md:25-33`).
- Ninja - Optional documented generator in `docs/BuildGuide.md:54-57` and `docs/BuildGuide.md:181-193`.

## Key Dependencies

**Critical:**
- `opengl32` / system OpenGL - Linked directly in `Engine/CMakeLists.txt:29-33`; required by Win32 OpenGL context creation and all OpenGL calls.
- GLAD - Bundled in `vendor/glad/`; provides `<glad/glad.h>` and `<glad/glad_wgl.h>` used by `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, and OpenGL buffer/texture/shader headers.
- Win32 API - Required by `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, `Examples/BasicGame/source/Main.cpp`, and `Examples/BasicRendering/Main.cpp`.

**Infrastructure:**
- C++ standard library - Smart pointers, filesystem, chrono, streams, vectors, and strings are used across `Engine/Core/source/Game.cpp`, `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp`, and `Engine/Utils/source/*.cpp`.
- MSVC intrinsics - `<intrin.h>` is used in `Engine/Math/include/Pyramid/Math/MathSIMD.hpp`, `Engine/Math/source/MathSIMD.cpp`, `Engine/Math/include/Pyramid/Math/MathPerformance.hpp`, and `Engine/Utils/include/Pyramid/Util/Log.hpp`.
- Custom image codecs - TGA/BMP in `Engine/Utils/source/Image.cpp`, PNG in `Engine/Utils/source/PNGLoader.cpp` plus `Inflate.cpp`/`ZLib.cpp`, and JPEG in `Engine/Utils/source/JPEGLoader.cpp` plus JPEG component files. No `stb_image`, `libpng`, or `libjpeg` dependency is detected.

## Engine/Runtime Technologies

**Core/Game Loop:**
- `Pyramid::Game` owns a `Window` and `IGraphicsDevice`, initializes them in `Engine/Core/source/Game.cpp:11-53`, and runs a single-threaded loop in `Engine/Core/source/Game.cpp:120-172`.
- Frame time uses `std::chrono::high_resolution_clock` and clamps to 1/30s max delta in `Engine/Core/source/Game.cpp:140-162`.

**Graphics/Renderer:**
- Graphics abstraction starts at `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` with `IGraphicsDevice` methods for buffers, shaders, textures, UBOs, instance buffers, SSBOs, state changes, and device diagnostics.
- OpenGL concrete implementation lives in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` and related files under `Engine/Graphics/source/OpenGL/Buffer/`, `Engine/Graphics/source/OpenGL/Shader/`, `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp`, and `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`.
- Render system passes are compiled from `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `ShadowMapPass.cpp`, `DeferredGeometryPass.cpp`, and `DeferredLightingPass.cpp`.
- Scene and culling technology lives in `Engine/Graphics/source/Scene/SceneManager.cpp` and `Engine/Graphics/source/Scene/Octree.cpp`.

**Platform/Input:**
- Windowing is Win32-only in active code: `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`.
- Input module currently contributes include directories only through `Engine/Input/CMakeLists.txt`; no input source files are registered.

**Audio/Physics:**
- `Engine/Audio/CMakeLists.txt` and `Engine/Physics/CMakeLists.txt` only add include directories. API documentation in `docs/API_Reference/Audio/AudioSystem.md` and `docs/API_Reference/Physics/PhysicsSystem.md` describes planned systems, not detected compiled implementations.

**Math:**
- Vector, matrix, quaternion, and SIMD sources are compiled from `Engine/Math/source/Vec2.cpp`, `Vec3.cpp`, `Vec4.cpp`, `Mat3.cpp`, `Mat4.cpp`, `Quat.cpp`, and `MathSIMD.cpp`.
- Public math aggregation header is `Engine/Math/include/Pyramid/Math/Math.hpp`.

**Image Handling:**
- `Pyramid::Util::Image` dispatches by file extension in `Engine/Utils/source/Image.cpp:77-104`.
- Supported formats detected in code: `.tga`, `.bmp`, `.png`, `.jpg`, `.jpeg`.
- Texture loading integrates images with graphics through `IGraphicsDevice::CreateTexture2D(const std::string&)` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp:113-120` and `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:214-217`.

## Configuration

**Environment:**
- `.env` files: Not detected.
- Runtime environment variables: `PYRAMID_SHADER_PATH` and `PYRAMID_ASSET_PATH` are documented examples in `docs/BuildGuide.md:246-254`, but current shader resolution code in `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp` does not read environment variables.
- Compile-time path: `PYRAMID_SOURCE_DIR` is defined privately for `PyramidEngine` in `Engine/CMakeLists.txt:35-38` and used by `ShaderPathResolver` in `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp:20-23`.

**Build:**
- Root build config: `CMakeLists.txt`.
- CMake presets: `CMakePresets.json`.
- Dependency helper: `CMake/Dependencies.cmake` exists and calls `find_package(OpenGL REQUIRED)`, but it is not included by the active root or engine `CMakeLists.txt` files.
- Clean configure script: `scripts/configure-clean.ps1`.
- Generated outputs ignored by `.gitignore`: `build/`, `cmake-build*/`, `out/`, `Debug/`, `Release/`, `bin/`, `lib/`, CMake cache/files, Visual Studio artifacts, and compiled binaries.

## Include/Link Strategy

- Add module sources to the single `PyramidEngine` target with `target_sources()` as shown in `Engine/Core/CMakeLists.txt`, `Engine/Graphics/CMakeLists.txt`, `Engine/Math/CMakeLists.txt`, `Engine/Platform/CMakeLists.txt`, and `Engine/Utils/CMakeLists.txt`.
- Add each module public include root with `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>` and `$<INSTALL_INTERFACE:include>`; keep module `source` directories private.
- Include engine headers with `#include <Pyramid/...>` or `#include "Pyramid/..."`, matching paths under each module's `include/Pyramid/` directory.
- Link examples against `PyramidEngine`, as in `Examples/BasicGame/CMakeLists.txt:7` and `Examples/BasicRendering/CMakeLists.txt:18`.
- Link tests against `PyramidEngine` using the helper in `Engine/Utils/test/CMakeLists.txt:1-16`.
- Keep `glad` private to `PyramidEngine`; only add direct `vendor/glad/include` includes to consumers that directly include GLAD headers, as `Examples/BasicGame/CMakeLists.txt:12-13` currently does.

## Platform Requirements

**Development:**
- Windows 10/11 64-bit.
- Visual Studio 2022 with Desktop development with C++, MSVC v143, Windows 10/11 SDK, and CMake tools.
- CMake 3.16+ for direct commands; CMake 3.23+ for `CMakePresets.json`.
- OpenGL-capable GPU and current drivers.

**Production:**
- Windows desktop executable deployment is the only detected active target.
- Requires a working OpenGL/WGL implementation and Windows system `opengl32`.
- Example smoke validation expects `BasicGame` and `BasicRenderingExample` executables from the build tree.

---

*Stack analysis: 2026-05-14*
