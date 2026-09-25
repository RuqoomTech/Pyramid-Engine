# Technology Stack

**Analysis Date:** 2026-09-05

## Overview

Pyramid Engine is a Windows-first, general-purpose C++17 game engine built around a Win32/WGL platform layer and an OpenGL renderer. The repository is versioned as `0.6.0-pre-alpha` (`CMakeLists.txt:3-6`). There are no package-manager dependencies (no vcpkg, Conan, FetchContent, or npm). The sole bundled third-party runtime library is `vendor/glad`. Everything else — foundation types, math, input, image codecs, model parsing, font rasterization, text layout, UI — is Pyramid-owned code organized as independently installable CMake packages.

## Languages & Runtimes

**Primary:**

- C++17 (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`) — all engine, library, example, test, and tool code. Declared in `CMakeLists.txt:20-22`. Enforced per-target with `-Wall -Wextra -Wpedantic` (GCC/Clang) or `/W4 /permissive-` (MSVC fallback) in `Engine/CMakeLists.txt:58-68`, `Libraries/PyramidFoundation/CMakeLists.txt:27-37`, `Libraries/PyramidImage/CMakeLists.txt:29-39`, `Tools/PyramidFontCompiler/CMakeLists.txt:8-18`.
- C (C11 ABI via MinGW/UCRT headers) — GLAD loader sources compiled as C alongside C++. Project declares `LANGUAGES C CXX` in `CMakeLists.txt:3`. Sources: `vendor/glad/src/glad.c`, `vendor/glad/src/glad_wgl.c` (built by `vendor/glad/CMakeLists.txt:1-4`).

**Shader language:**

- GLSL 3.30 (`#version 330 core`) — all engine and example shaders. Files: `Engine/Graphics/shaders/forward.vert`, `Engine/Graphics/shaders/forward.frag`, `Engine/Graphics/shaders/shadow.vert`, `Engine/Graphics/shaders/shadow.frag`, `Engine/Graphics/shaders/deferred_geometry.vert`, `Engine/Graphics/shaders/deferred_geometry.frag`, `Engine/Graphics/shaders/deferred_lighting.vert`, `Engine/Graphics/shaders/deferred_lighting.frag`, plus embedded UI shaders in `Engine/Graphics/source/UI/UIRenderer.cpp:24,48`. Runtime requires OpenGL 3.3 core or newer (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:682`); context creation tries 4.6 down to 3.3 (`Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:616-617`).

**Scripting / tooling languages:**

- PowerShell 5.1+ (Windows PowerShell) — build, bootstrap, smoke, and example wrappers in `scripts/bootstrap-msys2.ps1`, `scripts/build-mingw.ps1`, `scripts/bundle-mingw-runtime.ps1`, `scripts/configure-clean.ps1`, `scripts/run-smoke.ps1`, `scripts/run-example.ps1`.
- Python 3 (stdlib only: `struct`, `hashlib`, `pathlib`, `argparse`, `subprocess`, `runpy`, `tempfile`) — Ruqoom-owned reference-font asset pipeline. Scripts: `scripts/generate-pyramid-font.py` (Pyramid Sans TTF synthesis), `scripts/generate-pyramid-arabic-font.py` (Pyramid Arabic TTF synthesis), `scripts/regenerate-reference-fonts.py` (drives `Tools/PyramidFontCompiler` to bake deterministic 64-pixel SDF `.pfont` atlases; `--check` mode verifies byte-for-byte reproduction).
- CMake language (minimum 3.23) — entire build definition. Floor set in `CMakeLists.txt:1` and `CMakePresets.json:3-7`.
- Windows resource script (`.rc`) — executable metadata/branding: `assets/branding/pyramid-engine.rc` (compiled into `Tools/PyramidFontCompiler` via `Tools/PyramidFontCompiler/CMakeLists.txt:1-4`).

**Platform runtime:**

- Windows 10/11 x64 only. Non-Windows configure is a hard error unless `PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE=ON` (configure-only metadata validation, not a runnable build) — `CMakeLists.txt:14-18`, `docs/BUILDING.md:17`.
- MSYS2 UCRT64 with MinGW-w64 GCC (default) — `scripts/bootstrap-msys2.ps1:29-33` installs `mingw-w64-ucrt-x86_64-toolchain`, `mingw-w64-ucrt-x86_64-cmake`, `mingw-w64-ucrt-x86_64-ninja`.
- Clang targeting the same MinGW-w64/UCRT runtime (validated alternative) — `scripts/bootstrap-msys2.ps1:35-38` adds `mingw-w64-ucrt-x86_64-clang` + `mingw-w64-ucrt-x86_64-lld`; presets `clang-debug-tests` / `clang-release-tests` in `CMakePresets.json:66-83`. Visual Studio / MSVC is not required or used by CI.

## Build System

**Generator:** Ninja (all presets in `CMakePresets.json:12` set `"generator": "Ninja"`). `CMAKE_EXPORT_COMPILE_COMMANDS=ON` is set in the hidden `ninja-base` preset (`CMakePresets.json:13-14`).

**Configure presets** (`CMakePresets.json:8-84`):

| Preset | Inherits | Binary dir | Build type | Tests |
|---|---|---|---|---|
| `ninja-base` (hidden) | — | — | — | `OFF` |
| `gcc-base` (hidden) | `ninja-base` | — | — | — (`gcc`/`g++`) |
| `clang-base` (hidden) | `ninja-base` | — | — | — (`clang`/`clang++`) |
| `gcc-debug` | `gcc-base` | `build/gcc-debug` | Debug | `OFF` |
| `gcc-debug-tests` | `gcc-debug` | `build/gcc-debug-tests` | Debug | `ON` |
| `gcc-release-tests` | `gcc-debug-tests` | `build/gcc-release-tests` | Release | `ON` |
| `clang-debug-tests` | `clang-base` | `build/clang-debug-tests` | Debug | `ON` |
| `clang-release-tests` | `clang-debug-tests` | `build/clang-release-tests` | Release | `ON` |

Build presets (`CMakePresets.json:85-106`) and test presets (`CMakePresets.json:107-136`) mirror the configure presets one-to-one (e.g. `build-gcc-debug-tests` → `gcc-debug-tests`, `test-gcc-debug` → `gcc-debug-tests` with `outputOnFailure`).

**CMake options** (`CMakeLists.txt:8-27`, `CMake/PyramidMinGWRuntime.cmake:3-7`, documented in `docs/BUILDING.md:104-113`):

| Option | Default | Purpose |
|---|---|---|
| `PYRAMID_BUILD_TESTS` | `OFF` | Enables `CTest`, `Tests/` subtree, and per-library `test/` subtrees |
| `PYRAMID_BUILD_EXAMPLES` | `ON` | Builds `Examples/BasicGame`, `Examples/BasicRendering` (and `Examples/RTSReference` when examples or tests are on) |
| `PYRAMID_BUILD_TOOLS` | `ON` | Builds `Tools/PyramidFontCompiler` |
| `PYRAMID_WARNINGS_AS_ERRORS` | `OFF` | Promotes `-Wall -Wextra -Wpedantic` (`/W4` on MSVC) to errors |
| `PYRAMID_BUNDLE_MINGW_RUNTIME` | `ON` | Copies MinGW runtime DLLs beside build/install executables (see below) |
| `PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE` | `OFF` | Configure-only validation on Linux/macOS; never a runnable engine |
| `PYRAMID_VERSION_STATUS` | `"pre-alpha"` | Human-readable suffix in `PYRAMID_VERSION_STRING` |

**Output layout:** `CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_BINARY_DIR}/bin`, `CMAKE_LIBRARY_OUTPUT_DIRECTORY` / `CMAKE_ARCHIVE_OUTPUT_DIRECTORY=${CMAKE_BINARY_DIR}/lib` (`CMakeLists.txt:34-36`). Typical GCC Debug outputs per `docs/BUILDING.md:155-161`: `build/gcc-debug-tests/bin/BasicGame.exe`, `build/gcc-debug-tests/bin/BasicRenderingExample.exe`, `build/gcc-debug-tests/lib/libPyramidEngined.a` (note `DEBUG_POSTFIX d` on all library targets, e.g. `Engine/CMakeLists.txt:8-13`).

**Install / packaging:** GNUInstallDirs-based install in `CMakeLists.txt:70-152`. Installs public headers (`Engine/Core/include/`, `Engine/Graphics/include/`, `Engine/Platform/include/`, `vendor/glad/include/`, each `Libraries/*/include/`), the `PyramidEngine` + `glad` targets and all eight owned-library targets, per-package `*Config.cmake` / `*ConfigVersion.cmake` generated from `CMake/*Config.cmake.in` templates, and namespaced `Pyramid::` export sets (`PyramidEngineTargets`, one per library). `pyramid_configure_mingw_runtime()` (`CMake/PyramidMinGWRuntime.cmake:9-72`) creates the `PyramidMinGWRuntime` target that copies `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` (plus `libunwind.dll` / `libssp-0.dll` when present) from the compiler directory into the bin dir and into `install/bin`.

## Key Dependencies

No third-party package-manager dependencies exist. Verified by grep for `find_package` (only `OpenGL` + self-referential installed-package consumers), and absence of `FetchContent` / `ExternalProject` / `CPM` / `vcpkg` / `conan` manifests across the repo.

| Dependency | Version / source | Purpose | Where used |
|---|---|---|---|
| `vendor/glad` (bundled, in-tree) | GLAD-generated loader, headers in `vendor/glad/include/glad/glad.h`, `vendor/glad/include/glad/glad_wgl.h`, `vendor/glad/include/KHR/khrplatform.h` | Sole approved bundled third-party runtime: OpenGL + WGL function loading | Built by `vendor/glad/CMakeLists.txt`; linked `PUBLIC` by `Engine/CMakeLists.txt:25-36`; loaded in `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp:605,658` |
| Windows SDK / system libs (`OpenGL::GL`, `gdi32`, `user32`) | OS-provided (UCRT64 toolchain) | WGL context creation, window messaging, GDI device contexts, system OpenGL ICD entry points | `find_package(OpenGL REQUIRED)` + `target_link_libraries(... OpenGL::GL gdi32 user32)` in `Engine/CMakeLists.txt:38-46` |
| MinGW-w64 GCC runtime DLLs | Matches active compiler (`gcc`/`clang++` dir) | Allow direct `.exe` launch without editing `PATH` | Resolved in `CMake/PyramidMinGWRuntime.cmake:19-47`; copied by `scripts/bundle-mingw-runtime.ps1`; verified by `scripts/build-mingw.ps1:53-57` |
| MSYS2 UCRT64 packages | `mingw-w64-ucrt-x86_64-toolchain`, `-cmake`, `-ninja` (+ `-clang`, `-lld` for Clang) | Compiler, CMake ≥3.23, Ninja, linker | `scripts/bootstrap-msys2.ps1:29-38`; CI matrix in `.github/workflows/windows-ci.yml:26-66` |
| Python 3 stdlib | Any Python 3 (repo validated against local 3.11) | Reference-font TTF synthesis + `.pfont` regeneration driver | `scripts/generate-pyramid-font.py`, `scripts/generate-pyramid-arabic-font.py`, `scripts/regenerate-reference-fonts.py`; documented in `docs/BUILDING.md:275-292` |
| GitHub Actions + `msys2/setup-msys2@v2` + `actions/checkout@v4` + `actions/upload-artifact@v4` | Pinned actions in `.github/workflows/windows-ci.yml` | CI only (not a runtime dependency) | `.github/workflows/windows-ci.yml:70-77,212-219` |

**First-party (Pyramid-owned) packages** — these are the equivalent of internal dependencies; each is an independent CMake installable package with its own `*Config.cmake.in` in `CMake/`:

| Package / target | Directory | Depends on |
|---|---|---|
| `Pyramid::Foundation` | `Libraries/PyramidFoundation/` (`source/Prerequisites.cpp`, `source/Log.cpp`) | Nothing (leaf) |
| `Pyramid::Math` | `Libraries/PyramidMath/` | `Pyramid::Foundation` (checked per-library `CMakeLists.txt`) |
| `Pyramid::Input` | `Libraries/PyramidInput/` | `Pyramid::Foundation` |
| `Pyramid::Image` | `Libraries/PyramidImage/` (`source/BitReader.cpp`, `source/HuffmanDecoder.cpp`, `source/Inflate.cpp`, `source/JPEGLoader.cpp`, `source/PNGLoader.cpp`, `source/ZLib.cpp`) | `Pyramid::Foundation` |
| `Pyramid::Model` | `Libraries/PyramidModel/` | `Pyramid::Foundation`, `Pyramid::Math` |
| `Pyramid::Font` | `Libraries/PyramidFont/` | `Pyramid::Foundation` |
| `Pyramid::Text` | `Libraries/PyramidText/` | `Pyramid::Foundation`, `Pyramid::Font` |
| `Pyramid::UI` | `Libraries/PyramidUI/` | `Pyramid::Foundation`, `Pyramid::Text` |
| `Pyramid::Engine` (`PyramidEngine`) | `Engine/Core/` (`source/Game.cpp`), `Engine/Graphics/` (devices, buffers, shaders, textures, framebuffers, renderer passes, camera, scene, octree, `UIRenderer`, `ModelResourceImporter`), `Engine/Platform/` (`source/Windows/Win32OpenGLWindow.cpp`, `source/RuntimePath.cpp`, `source/SystemFont.cpp`) | All eight libraries + `glad` (PUBLIC) + `OpenGL::GL`/`gdi32`/`user32` (PRIVATE, WIN32) per `Engine/CMakeLists.txt:25-46` |
| `Pyramid::RTSReference` (game-side, not installed) | `Examples/RTSReference/` | Engine (test-only linkage in `Tests/CMakeLists.txt:64-67`) |

## Configuration

**Presets file:** `CMakePresets.json` (schema version 4, `cmakeMinimumRequired` 3.23). Use `cmake --preset gcc-debug-tests` → `cmake --build --preset build-gcc-debug-tests` → `ctest --preset test-gcc-debug` from MSYS2 UCRT64, or the PowerShell wrapper `scripts/build-mingw.ps1 -Compiler gcc -Configuration Debug` which additionally re-verifies the MinGW runtime bundle (`scripts/build-mingw.ps1:53-57`).

**Per-target compile config:** `PYRAMID_VERSION_MAJOR/MINOR/PATCH/STRING` injected as public compile definitions on `PyramidEngine` (`Engine/CMakeLists.txt:48-56`); `PYRAMID_SOURCE_DIR` injected as a private definition for development-shader lookup. Warning flags are per-target, never global (`Engine/CMakeLists.txt`, each `Libraries/*/CMakeLists.txt`, `Tools/PyramidFontCompiler/CMakeLists.txt`).

**CTest:** enabled only when `PYRAMID_BUILD_TESTS=ON` via `include(CTest)` / `enable_testing()` in `CMakeLists.txt:29-32`. Test executables are registered with `add_test(NAME <Suite>.<Case> ...)` in `Tests/CMakeLists.txt` and each `Libraries/*/test/CMakeLists.txt` (55–57 targets depending on platform; `Platform.SystemFonts` is `WIN32`-gated in `Tests/CMakeLists.txt:37-42`).

**External-consumer validation:** `Tests/Consumer/CMakeLists.txt` (`find_package(PyramidEngine)`), `Tests/LibrariesConsumer/CMakeLists.txt` (`PyramidFoundation`+`PyramidMath`+`PyramidInput`), `Tests/ImageConsumer/CMakeLists.txt`, `Tests/ModelConsumer/CMakeLists.txt`, `Tests/FontConsumer/CMakeLists.txt`, `Tests/UIConsumer/CMakeLists.txt` — each is configured and built independently against the install prefix in CI (`.github/workflows/windows-ci.yml:97-209`).

## Tooling

- **Compilers:** MinGW-w64 GCC (`gcc`/`g++`, default) and Clang (`clang`/`clang++`) from MSYS2 UCRT64. MSVC branches exist in target `CMakeLists.txt` files (`/W4 /permissive-`) but MSVC is not a supported workflow per `docs/BUILDING.md:15`.
- **Build drivers:** CMake ≥3.23, Ninja, `scripts/build-mingw.ps1` (PowerShell → UCRT64 CMake/CTest), `scripts/configure-clean.ps1` (clean reconfigure), `scripts/run-example.ps1` (build + launch `BasicGame`), `scripts/run-smoke.ps1` (headless process smoke test for `BasicGame` + `BasicRenderingExample`; not pixel validation).
- **Asset tools:** `Tools/PyramidFontCompiler` (built when `PYRAMID_BUILD_TOOLS=ON`; links only `Pyramid::Font` per `Tools/PyramidFontCompiler/CMakeLists.txt:6`) compiles reference TTFs into `.pfont` atlases; `scripts/regenerate-reference-fonts.py --check` enforces byte-for-byte reproducibility of `Examples/BasicGame/Assets/Fonts/PyramidSans*.{ttf,pfont}` and `PyramidArabic*.{ttf,pfont}`.
- **CI:** `.github/workflows/windows-ci.yml` — 4-job matrix (GCC/Clang × Debug/Release) on `windows-2022`, each doing configure → build → `ctest` → `cmake --install` → six independent consumer builds+runs → binary artifact upload. No Visual Studio step.
- **Test runner:** CTest via `ctest --preset test-gcc-debug` (or `test-gcc-release` / `test-clang-*`); enumerate with `ctest --test-dir build/gcc-debug-tests -N`.

---

*Stack analysis: 2026-09-05*
