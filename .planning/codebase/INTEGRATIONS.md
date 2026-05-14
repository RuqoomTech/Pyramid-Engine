# External Integrations

**Analysis Date:** 2026-05-14

## APIs & External Services

**Operating System APIs:**
- Win32 windowing and message pump - Used for native windows, message processing, device contexts, and process entry points.
  - SDK/Client: Windows SDK headers through `<Windows.h>` / `<windows.h>`.
  - Implementation: `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, `Examples/BasicGame/source/Main.cpp`, and `Examples/BasicRendering/Main.cpp`.
  - Auth: Not applicable.
- WGL - Used to create and manage OpenGL contexts on Windows.
  - SDK/Client: `opengl32` plus GLAD WGL declarations from `vendor/glad/include/glad/glad_wgl.h`.
  - Implementation: `Win32OpenGLWindow::CreateOpenGLContext()` and `Win32OpenGLWindow::Present()` in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`.
  - Auth: Not applicable.

**Graphics APIs:**
- OpenGL - Active rendering backend for buffers, shaders, textures, framebuffers, render passes, and device diagnostics.
  - SDK/Client: `opengl32` system library and bundled GLAD loader.
  - Implementation: `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`, `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`, `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp`, `Engine/Graphics/source/OpenGL/Buffer/*.cpp`, and `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp`.
  - Auth: Not applicable.
- GLSL shader files - Used by renderer passes and example rendering code.
  - SDK/Client: OpenGL shader compiler at runtime.
  - Assets: `Engine/Graphics/shaders/forward.vert`, `forward.frag`, `shadow.vert`, `shadow.frag`, `deferred_geometry.vert`, `deferred_geometry.frag`, `deferred_lighting.vert`, and `deferred_lighting.frag`.
  - Resolution: `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp`.

**External Web Services:**
- Not detected - No HTTP clients, REST APIs, cloud SDKs, payment SDKs, telemetry SaaS, or hosted backend integrations are detected in source or build files.

## Data Storage

**Databases:**
- Not detected.
  - Connection: Not applicable.
  - Client: Not applicable.

**File Storage:**
- Local filesystem only.
  - Shader text files are loaded from candidate filesystem paths in `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp:13-56`.
  - Image files are loaded from local paths in `Engine/Utils/source/Image.cpp`, `Engine/Utils/source/PNGLoader.cpp`, and `Engine/Utils/source/JPEGLoader.cpp`.
  - Logs can be emitted to a local file; `scripts/run-smoke.ps1:65-76` checks for `pyramid_game.log`.

**Caching:**
- In-process caching only.
  - OpenGL device information is cached in `OpenGLDevice::GetDeviceInfo()` in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:289-313`.
  - OpenGL state caching/counting is centralized by `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp` and exposed through `IGraphicsDevice::GetStateChangeCount()` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`.
  - No Redis, Memcached, disk cache service, or asset cache service is detected.

## Authentication & Identity

**Auth Provider:**
- Not applicable - The engine has no user accounts, tokens, OAuth, API keys, or identity provider integration.
  - Implementation: Not detected.

## Monitoring & Observability

**Error Tracking:**
- None external.
- Internal diagnostics use engine logging macros from `Engine/Utils/include/Pyramid/Util/Log.hpp` and implementation in `Engine/Utils/source/Log.cpp`.
- OpenGL errors are captured manually in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` by `CaptureOpenGLError()`.

**Logs:**
- Thread-safe engine logger - Documented in `README.md:37-41` and implemented in `Engine/Utils/source/Log.cpp` / `Engine/Utils/include/Pyramid/Util/Log.hpp`.
- Console and file output are documented capabilities; smoke script expects `pyramid_game.log` in `scripts/run-smoke.ps1:65-76`.
- No external log collector, OpenTelemetry, Sentry, Crashpad, or analytics sink is detected.

**Profiling:**
- No external profiler integration detected.
- `PYRAMID_ENABLE_PROFILING` is documented in `docs/BuildGuide.md:237-245` but not wired into the active CMake files.

## CI/CD & Deployment

**Hosting:**
- Not detected - No deployment descriptors or release packaging scripts are present.
- Runtime target is local Windows desktop executables built into CMake output directories.

**CI Pipeline:**
- Not detected - No `.github/workflows`, Azure Pipelines, GitLab CI, or similar CI config is detected.
- Local CTest path exists through `CMakePresets.json:test-debug` and `Engine/Utils/test/CMakeLists.txt`.
- Local GUI smoke path exists through `scripts/run-smoke.ps1` and `docs/SmokeTests.md`.

**Build Automation:**
- `scripts/configure-clean.ps1` removes or falls back from `build/` and configures either `vs2022-debug` or `vs2022-debug-tests`.
- `scripts/run-smoke.ps1` resolves and launches `BasicGame` and `BasicRenderingExample` from build outputs.

## Environment Configuration

**Required env vars:**
- None detected for active code paths.
- `PYRAMID_SHADER_PATH` and `PYRAMID_ASSET_PATH` are documented in `docs/BuildGuide.md:246-254` as examples, but `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp` does not read them.

**Secrets location:**
- Not applicable - `.env` files are not detected and no credential files are detected during this map.
- Do not introduce secrets into committed build files or docs.

**Compile-time config:**
- `PYRAMID_SOURCE_DIR` is defined for `PyramidEngine` in `Engine/CMakeLists.txt:35-38` and used for shader path fallback in `Engine/Graphics/source/Renderer/ShaderPathResolver.cpp:20-23`.
- CMake options are configured in `CMakeLists.txt:8-12` and `CMakePresets.json:15-27`.

## Vendored Dependencies

### glad — OpenGL/WGL Loader

**Location:** `vendor/glad/`

**Files:**
- `vendor/glad/src/glad.c` - Core GL loader implementation.
- `vendor/glad/src/glad_wgl.c` - WGL loader implementation; included in the built `glad` target.
- `vendor/glad/src/glad_glx.c` - GLX loader source present in tree; not included by `vendor/glad/CMakeLists.txt`.
- `vendor/glad/include/glad/glad.h` - OpenGL declarations.
- `vendor/glad/include/glad/glad_wgl.h` - WGL declarations.
- `vendor/glad/include/glad/glad_glx.h` - GLX declarations present but unused by active Windows build.
- `vendor/glad/include/KHR/khrplatform.h` - Khronos platform types.

**Build Integration:**
- `vendor/glad/CMakeLists.txt` creates target `glad` from `src/glad.c` and `src/glad_wgl.c`.
- Root `CMakeLists.txt:23` adds `vendor/glad` before `Engine`.
- `Engine/CMakeLists.txt:29-33` links `PyramidEngine` privately against `glad` and `opengl32`.
- `Engine/CMakeLists.txt:23-26` adds `${PROJECT_SOURCE_DIR}/vendor` as a private include directory for `PyramidEngine`.

**Runtime Initialization:**
- `Win32OpenGLWindow::CreateOpenGLContext()` in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:234-240` calls `gladLoadWGL(m_hdc)` on a temporary context.
- It then attempts modern OpenGL contexts from 4.6 down to 3.3 in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:242-287`.
- It calls `gladLoadGL()` on the final context in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:273-280`.

**Caveats:**
- No upstream GLAD generator metadata, version file, or checksum is detected in `vendor/glad/`.
- `vendor/glad/src/glad_glx.c` is present but not compiled; the active project is Windows/WGL-oriented.
- Consumers should not add new direct GLAD usage outside OpenGL backend/platform code unless they also update CMake include paths.

## System Libraries

**OpenGL / opengl32:**
- Active link: `Engine/CMakeLists.txt:29-33` links `opengl32` directly.
- Unused helper: `CMake/Dependencies.cmake:1-3` contains `find_package(OpenGL REQUIRED)` and `target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)`, but this file is not included by the active CMake graph.
- Caveat: Prefer one link strategy. Current active strategy is direct `opengl32`; changing to `OpenGL::GL` requires including and correcting `CMake/Dependencies.cmake`.

**Windows SDK:**
- Required by Win32, WGL, and OpenGL context code in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`.
- Required by example entry points including `<windows.h>` in `Examples/BasicGame/source/Main.cpp` and `Examples/BasicRendering/Main.cpp`.

## Module Integration Patterns

**Engine Modules:**
- Use the single library target `PyramidEngine` declared in `Engine/CMakeLists.txt:1`.
- Register module source/header files with `target_sources(PyramidEngine ...)` for active compiled modules such as `Engine/Core/CMakeLists.txt`, `Engine/Graphics/CMakeLists.txt`, `Engine/Math/CMakeLists.txt`, `Engine/Platform/CMakeLists.txt`, and `Engine/Utils/CMakeLists.txt`.
- Add module include directories with `target_include_directories(PyramidEngine PUBLIC $<BUILD_INTERFACE:.../include> $<INSTALL_INTERFACE:include> PRIVATE .../source)`.
- Modules `Engine/Renderer`, `Engine/Input`, `Engine/Scene`, `Engine/Audio`, and `Engine/Physics` currently add include directories only and no sources.

**Examples:**
- `Examples/BasicGame/CMakeLists.txt` builds `BasicGame` as a `WIN32` executable from `source/Main.cpp`, `source/BasicGame.cpp`, and `include/BasicGame.hpp`; it links `PyramidEngine` privately.
- `Examples/BasicRendering/CMakeLists.txt` builds `BasicRenderingExample` as a `WIN32` executable from `Main.cpp` and `BasicRendering.cpp`; it links `PyramidEngine` privately and overrides runtime output to `${CMAKE_BINARY_DIR}/Examples/BasicRendering`.
- Root `CMakeLists.txt:26-29` adds examples only when `PYRAMID_BUILD_EXAMPLES=ON`.

**Tests:**
- `Engine/Utils/test/CMakeLists.txt` defines `add_utils_test(target_name source_file)` to create a C++ executable, link `PyramidEngine`, add `../include`, set folder `Tests`, and register CTest.
- Tests are only included when `PYRAMID_BUILD_TESTS=ON` through `Engine/Utils/CMakeLists.txt:25-28`.

**Game Directory Caveat:**
- `Game/CMakeLists.txt` creates `PyramidGame` and links `OpenGL3D`; `Game/Main.cpp` includes `<OpenGL3D/Game/OglGame.h>`. No active `OpenGL3D` target or include path is detected in the current CMake graph.
- Root `CMakeLists.txt` does not add `Game/`, so this stale integration does not affect default builds. Do not use `Game/` as a template without first migrating it to `PyramidEngine` APIs.

## Webhooks & Callbacks

**Incoming:**
- None detected.

**Outgoing:**
- None detected.

## Notable Dependency and Integration Caveats

- Keep `PYRAMID_BUILD_TOOLS=OFF` because root `CMakeLists.txt:31-33` references `Tools/AssetProcessor`, and no `Tools/` directory is detected.
- Use CMake 3.23+ when invoking presets from `CMakePresets.json`; direct root CMake only requires 3.16.
- The OpenGL backend is the only detected implemented graphics backend even though `GraphicsAPI` in `Engine/Core/include/Pyramid/Core/Prerequisites.hpp:18-27` lists DirectX and Vulkan enum values.
- Audio and physics integrations are planned/documented but not implemented as compiled source files; do not add external audio/physics SDKs without updating `Engine/Audio/CMakeLists.txt` or `Engine/Physics/CMakeLists.txt`.
- Image loading is self-contained; do not add `stb_image`, `libpng`, or `libjpeg` unless replacing or augmenting `Engine/Utils/source/Image.cpp`, `PNGLoader.cpp`, and `JPEGLoader.cpp` intentionally.
- Shader path resolution depends on working directory, relative path, and `PYRAMID_SOURCE_DIR`; it does not currently copy shader assets into build output.

---

*Integration audit: 2026-05-14*
