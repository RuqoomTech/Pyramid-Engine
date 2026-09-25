# External Integrations

**Analysis Date:** 2026-09-05

## Summary

Pyramid Engine has **no networked external integrations**: no SaaS APIs, no databases, no auth providers, no webhooks, no telemetry, no package-registry fetches at build time. The only third-party code in the runtime is the in-tree `vendor/glad` OpenGL/WGL loader. All other "integrations" are OS-level system integrations on Windows (Win32 windowing, WGL/OpenGL driver, GDI font-byte access, clipboard, filesystem runtime paths, installed system fonts) plus a local Python asset-pipeline used at development time.

## APIs & External Services

**Networked APIs: None.**

- No HTTP/REST/gRPC clients, no cloud SDKs, no game-backend calls exist anywhere in `Engine/`, `Libraries/`, `Examples/`, `Tools/`, or `Tests/`. Grep for `find_package` shows only `OpenGL` (`Engine/CMakeLists.txt:39`) and self-referential installed-package consumers (`Tests/Consumer/CMakeLists.txt`, `Tests/LibrariesConsumer/CMakeLists.txt`, `Tests/ImageConsumer/CMakeLists.txt`, `Tests/ModelConsumer/CMakeLists.txt`, `Tests/FontConsumer/CMakeLists.txt`, `Tests/UIConsumer/CMakeLists.txt`).
- No `FetchContent`, `ExternalProject`, CPM, vcpkg manifest, or Conan file exists; builds are fully offline after the MSYS2 toolchain install.
- CI-only hosted services (not runtime): GitHub Actions runner `windows-2022`, `msys2/setup-msys2@v2`, `actions/checkout@v4`, `actions/upload-artifact@v4` — see `.github/workflows/windows-ci.yml:13-14,69-77,211-219`.

## Data Storage

**Databases: None (no SQL, no NoSQL, no ORM, no connection strings).**

- Persistence is file-based and Pyramid-owned:
  - Version-2 entity/component scene serialization — `Engine/Graphics/source/Scene/SceneSerializer.cpp`, header `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneSerializer.hpp` (covered by `Tests/SceneSerializationTests.cpp`).
  - Versioned resource manifests with deterministic manifest-key references — `Engine/Graphics/source/Resources/ResourceManifest.cpp`, `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceManifest.hpp`.
  - Versioned `.pfont` processed-font runtime format — produced by `Tools/PyramidFontCompiler/main.cpp`, consumed by `Libraries/PyramidFont/`; reference assets in `Examples/BasicGame/Assets/Fonts/` (`PyramidSans.ttf`, `PyramidSans-64-sdf.pfont`, `PyramidArabic.ttf`, `PyramidArabic-64-sdf.pfont`).
  - Legacy `SceneManager` JSON/XML/Binary methods remain explicitly unsupported (see `README.md` limitations).

**File storage: local filesystem only.**

- Runtime assets beside the executable are resolved through `Pyramid::Platform::ResolveRuntimePath` / `GetExecutableDirectory` (`Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp`, `Engine/Platform/source/RuntimePath.cpp:49-72` via `GetModuleFileNameW`, with a `/proc/self/exe` branch kept for configure-only validation). Direct `.exe` launches must never depend on the process working directory (enforced repo policy; see `docs/BUILDING.md:163-172`).
- Per-user processed-font SDF cache lives under a `Ruqoom/<AppName>` cache directory created by `CreateCacheDirectory` in `Engine/Platform/source/RuntimePath.cpp:33-46`.
- Checked-in example assets: `Examples/BasicGame/Assets/Fonts/`, `Examples/BasicGame/Assets/Models/` (resolved at runtime relative to the executable, not the source tree, in installed builds).

**Caching: in-process content-addressed caches only (no Redis/Memcached/CDN).**

- `ResourceRegistry::Meshes()/Shaders()/Textures()/Materials()` deduplicating caches — `Engine/Graphics/include/Pyramid/Graphics/Resources/ResourceRegistry.hpp`, `Engine/Graphics/source/Resources/ResourceRegistry.cpp`, with `MeshCache` (`Engine/Graphics/source/Geometry/MeshCache.cpp`), `ShaderCache` (`Engine/Graphics/source/Shader/ShaderCache.cpp`), `TextureCache` (`Engine/Graphics/source/Texture/TextureCache.cpp`), `MaterialCache` (`Engine/Graphics/source/Material/MaterialCache.cpp`).
- Content-addressed processed-font atlas cache in `Libraries/PyramidFont/`.

## Authentication & Identity

**Auth provider: None.**

- No login, OAuth, tokens, API keys, or secrets handling. There are no `.env` consumers, no credential files, and no secret directories in the build. (Per mapping policy, env/secret file contents were not inspected; only build references were checked — none exist.)
- Window/input focus is the only "identity-adjacent" OS concept: focus loss releases held controls (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp` focus-reset path; `Libraries/PyramidInput/` action-system consumption).

## Third-Party Libraries (bundled)

**`vendor/glad` — the sole approved bundled third-party runtime library** (repo policy in `AGENTS.md`; `README.md` repository-layout section).

- Contents: `vendor/glad/CMakeLists.txt` (builds `glad` static lib, alias `Pyramid::glad`, installs headers), `vendor/glad/include/glad/glad.h`, `vendor/glad/include/glad/glad_wgl.h`, `vendor/glad/include/glad/glad_glx.h` (unused on Windows; only `glad.c` + `glad_wgl.c` are compiled), `vendor/glad/include/KHR/khrplatform.h`, `vendor/glad/src/glad.c`, `vendor/glad/src/glad_wgl.c`, `vendor/glad/src/glad_glx.c` (present but not built).
- Linkage: `PyramidEngine` links `glad` PUBLIC (`Engine/CMakeLists.txt:25-36`) and re-exports its headers via install (`CMakeLists.txt:70-81`).
- Usage: `gladLoadWGL(m_hdc)` for WGL extensions then `gladLoadGL()` after core-context creation in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:605,658`.
- Policy: new runtime middleware and package-manager dependencies are prohibited by default; required non-platform functionality must become an independently maintained Pyramid/Ruqoom library instead.

**Deliberately NOT bundled** (all owned instead): image codecs (TGA/BMP subsets, custom PNG/DEFLATE, baseline/progressive JPEG in `Libraries/PyramidImage/source/` — `PNGLoader.cpp`, `JPEGLoader.cpp`, `Inflate.cpp`, `ZLib.cpp`, `HuffmanDecoder.cpp`, `BitReader.cpp`), OBJ/MTL parsing (`Libraries/PyramidModel/`), TrueType/SFNT parsing + CPU coverage/SDF rasterization (`Libraries/PyramidFont/`), text shaping/layout (`Libraries/PyramidText/`), UI (`Libraries/PyramidUI/`). No FreeType, HarfBuzz, stb, libpng, libjpeg, zlib, ICU, Qt, or C# runtime.

## System Integrations (OS / driver / hardware)

**Windowing + OpenGL context (Win32/WGL) — `Engine/Platform/`:**

- `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp` — window-class registration, `WM_*` message pump, keyboard/mouse polling with held/pressed/released states, pointer movement + wheel deltas, resize-event delivery, visibility/positioning, temporary-context → `wglCreateContextAttribsARB` core-profile negotiation (tries 4.6 → 3.3, forward-compatible, debug-bit in non-NDEBUG: `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:612-639`), `wglMakeCurrent` management, OpenGL version logging and 3.3-core minimum enforcement (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:682,969-1014`).
- Interface: `Engine/Platform/include/Pyramid/Platform/Window.hpp`, `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`.
- System libs linked PRIVATE on WIN32: `OpenGL::GL` (via `find_package(OpenGL REQUIRED)`), `gdi32`, `user32` — `Engine/CMakeLists.txt:38-46`.

**Graphics driver (OpenGL 3.3 core+) — `Engine/Graphics/`:**

- Device/loader: `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` (version-string query at `:328`), `Engine/Graphics/source/OpenGL/OpenGLDiagnostics.cpp` + `Engine/Graphics/source/OpenGL/OpenGLDiagnostics.hpp` (Debug driver callbacks, centralized error diagnostics), `Engine/Graphics/source/OpenGL/OpenGLStateManager.cpp`.
- Resources: `Engine/Graphics/source/OpenGL/Buffer/` (`OpenGLVertexBuffer.cpp`, `OpenGLIndexBuffer.cpp`, `OpenGLVertexArray.cpp`, `OpenGLUniformBuffer.cpp`, `OpenGLInstanceBuffer.cpp`, `OpenGLShaderStorageBuffer.cpp`), `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp`, `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp`, `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`.
- Shaders compiled at runtime are GLSL 3.30 (`Engine/Graphics/shaders/*.vert|*.frag`, embedded UI shaders in `Engine/Graphics/source/UI/UIRenderer.cpp:24,48`). Troubleshooting notes in `docs/BUILDING.md:259-265`: driver must support OpenGL 3.3 core; legacy fallback contexts are rejected.

**System fonts (read-only outline bytes) — `Engine/Platform/source/SystemFont.cpp`:**

- Uses GDI `GetFontData(deviceContext, 0, 0, …)` (`Engine/Platform/source/SystemFont.cpp:115,124`) plus `MultiByteToWideChar(CP_UTF8, …)` (`Engine/Platform/source/SystemFont.cpp:27-50`) to expose legally installed font bytes to the owned parser. Interface: `Engine/Platform/include/Pyramid/Platform/SystemFont.hpp`.
- No DirectWrite, no FreeType: parsing, shaping, rasterization, caching, and rendering stay Pyramid-owned (`Libraries/PyramidFont/`, `Libraries/PyramidText/`). `BasicGame` selects exact installed families per script on Windows (Segoe UI for Latin, Cairo-or-Tahoma for Arabic, Segoe UI Symbol for icons); each candidate must pass owned-parse + required-glyph-coverage + bounded SDF bake before entering the fallback family (see `README.md`). Covered by `Tests/SystemFontTests.cpp` (WIN32-gated in `Tests/CMakeLists.txt:37-42`).

**Clipboard (Win32, bounded, platform-neutral interface) — `Libraries/PyramidInput/` + Win32 backend:**

- Backend calls `OpenClipboard` / `SetClipboardData(CF_UNICODETEXT)` / `GetClipboardData(CF_UNICODETEXT)` / `IsClipboardFormatAvailable` in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:771-821`. UTF-16 surrogate pairs are combined in the backend; the public `Pyramid::Input` API stays backend-neutral with `CF_UNICODETEXT`-backed clipboard service and Unicode committed/composition text events. Covered by `Tests/ClipboardEncodingTests.cpp` and `Tests/InputTextEventTests.cpp`.

**Executable-relative runtime paths + per-user cache:**

- `GetModuleFileNameW`-based executable directory (`Engine/Platform/source/RuntimePath.cpp:56`) and `Ruqoom/<AppName>` cache directory creation (`Engine/Platform/source/RuntimePath.cpp:33-46`); interface `Engine/Platform/include/Pyramid/Platform/RuntimePath.hpp`. Used by the engine, examples, `UIRenderer` atlas loading, and `Tools/PyramidFontCompiler` outputs.

**MinGW compiler runtime (deployment integration):**

- `CMake/PyramidMinGWRuntime.cmake` (`pyramid_configure_mingw_runtime` / `pyramid_require_mingw_runtime`) + `scripts/bundle-mingw-runtime.ps1` copy `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` (and `libunwind.dll`/`libssp-0.dll` when emitted) beside every executable and into `install/bin`, so `.exe` files launch from Explorer/PowerShell without `PATH` edits. `PYRAMID_BUNDLE_MINGW_RUNTIME` defaults `ON`.

**Logging sink (OS debug output):**

- `Libraries/PyramidFoundation/source/Log.cpp:8` includes `<windows.h>` for the Windows debug-output sink behind the `PYRAMID_LOG_*` macros (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`). This is the only `<windows.h>` inclusion outside `Engine/Platform/`.

## Monitoring & Observability

- **Error tracking / APM / analytics: None** (no Sentry, no telemetry, no crash-reporter).
- **Logs:** owned `PYRAMID_LOG_*` diagnostics (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`, `Libraries/PyramidFoundation/source/Log.cpp`) + OpenGL driver debug callbacks in Debug builds (`Engine/Graphics/source/OpenGL/OpenGLDiagnostics.cpp`). Context/version info logged at context creation (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:674-675,981-1014`); device version string read in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp:328`.

## CI/CD & Deployment

- **Hosting / releases:** no store, no server, no container registry. Distribution is a local `cmake --install … --prefix install` tree plus CI binary artifacts (`build/<preset>/bin`, `build/<preset>/lib` uploaded via `actions/upload-artifact@v4` in `.github/workflows/windows-ci.yml:211-219`).
- **CI pipeline:** `.github/workflows/windows-ci.yml` (`Windows MinGW CI`, MSYS2 UCRT64, 4-matrix: GCC/Clang × Debug/Release). Each job: configure → build → CTest → `cmake --install` → six independent `find_package` consumer builds+runs (`Tests/Consumer`, `Tests/LibrariesConsumer`, `Tests/ImageConsumer`, `Tests/ModelConsumer`, `Tests/FontConsumer`, `Tests/UIConsumer`). No Visual Studio, no Linux/macOS jobs.
- **Local validation:** `scripts/build-mingw.ps1` (configure+build+bundle+test), `scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5` (process-liveness smoke test for `BasicGame` + `BasicRenderingExample`; explicitly not pixel validation).

## Environment Configuration

- **Required env vars: none.** Configure via CMake cache options/presets (see STACK.md), not environment. `C:\msys64\ucrt64\bin` is prepended to `PATH` transiently inside `scripts/build-mingw.ps1:19` only.
- **Secrets location: not applicable** — no secrets, tokens, or signing keys referenced by the build.
- **`.gitignore`-level note:** `.gitignore` covers `build/`, `install`-style outputs, `*.dll`, CMake byproducts; no secret files are referenced by build scripts.

## Webhooks & Callbacks

- **Incoming / outgoing webhooks: None.**
- **Driver/OS callbacks (in-process only, not webhooks):** OpenGL debug-message callback (`Engine/Graphics/source/OpenGL/OpenGLDiagnostics.cpp`), Win32 window procedure (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`), window-resize event delivery to `Game::onWindowResize` (`Engine/Core/source/Game.cpp`).

## Explicitly NOT Integrated

Do not plan work assuming any of these exist — each is called out in `README.md`, `AGENTS.md`, or `docs/BUILDING.md` as absent or prohibited:

- Audio, physics, editor, scripting runtimes (Baa is a long-term future gameplay-scripting experiment only; no ABI/FFI exists yet).
- DirectX / Vulkan (enum values reserved; only OpenGL implemented); compute dispatch is recorded but not executed.
- Linux / macOS (`CMakeLists.txt:14-18` rejects non-Windows configure by default).
- Databases, network services, auth providers, webhooks, telemetry, crash reporting.
- Package managers and middleware: vcpkg, Conan, FetchContent, FreeType, HarfBuzz, stb, libpng, libjpeg-turbo, zlib, ICU, Qt, C# runtimes.
- Occlusion culling (placeholder, disabled), `ITexture2D::CreateDepthTarget` (explicit failure; use framebuffer API), full Unicode Bidi/UAX#14/OpenType GSUB-GPOS/IME pre-edit (owned subset only), style sheets, controller navigation, raw relative mouse mode, persisted user bindings.

---

*Integration audit: 2026-09-05*
