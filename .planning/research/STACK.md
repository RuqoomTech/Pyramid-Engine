# Technology Stack Research: Pyramid Engine Foundation Refactor

**Project:** Pyramid Engine foundation milestone  
**Research dimension:** Stack/tooling choices  
**Researched:** 2026-05-14  
**Overall confidence:** HIGH for CMake/GoogleTest/Direct3D9/Doxygen basics; MEDIUM for SDL3 adoption cost until a spike validates the current Win32/WGL migration path.

## Executive Recommendation

Use a conservative foundation stack: **C++17, CMake 3.28+, CTest, GoogleTest 1.17.0 via pinned FetchContent, GitHub Actions on Windows/MSVC first, Doxygen for API docs, existing GLAD/OpenGL retained, SDL3 introduced behind a platform seam rather than directly replacing Win32 immediately, and native Windows SDK Direct3D 9 headers/libraries for the prototype backend seam**.

The refactor should not chase a package-manager migration, renderer rewrite, SDL GPU adoption, Vulkan, D3D11/12, Sphinx, or a full DX9 renderer. The foundation milestone needs honest boundaries and validation gates, not new engine features.

## Recommended Stack

### Core Language and Build

| Technology | Recommended Version / Status | Purpose | Current-Code Fit | Migration Cost | Confidence | Why |
|---|---:|---|---|---|---|---|
| C++ | Keep C++17 | Engine/library/tests/examples | Already configured and matches GoogleTest 1.17.0 minimum | Low | HIGH | Keeps scope stable while enabling modern RAII, `std::filesystem`, `std::optional`, and GoogleTest current release support. Do not upgrade language standard during the foundation milestone. |
| CMake | Raise project minimum to 3.28+ | Build graph, presets, dependency integration | Existing presets already require 3.23; root currently says 3.16 | Low/Med | HIGH | Aligns root, presets, docs, and CI on one modern baseline. 3.28 supports current FetchContent ergonomics like `EXCLUDE_FROM_ALL`; avoids old/manual dependency patterns. |
| CMakePresets.json | Keep and expand | Reproducible local/CI workflows | Already present | Low | HIGH | Official CMake docs position presets as project-wide shared configure/build/test settings; use them as the source of truth for phase gates. |
| Ninja Multi-Config | Add optional preset | Fast local/CI builds outside Visual Studio | Not currently default | Low | MEDIUM | Useful for future Linux/macOS and faster CI. Keep VS2022 preset as the Windows default for now. |

### Platform / Window / Context Ownership

| Technology | Recommended Version / Status | Purpose | Current-Code Fit | Migration Cost | Confidence | Why |
|---|---:|---|---|---|---|---|
| SDL3 | Target SDL 3.4.8 or later stable; add behind `PYRAMID_PLATFORM_SDL3` | Cross-platform windowing, event/input ownership, OpenGL context creation | Replaces direct Win32/WGL ownership gradually | Med/High | MEDIUM | SDL3 is a better game-engine foundation than GLFW because it covers windows, input/controllers, future audio/device needs, and current cross-platform context APIs. SDL docs verify OpenGL context creation via `SDL_CreateWindow(... SDL_WINDOW_OPENGL)`, `SDL_GL_CreateContext`, `SDL_GL_MakeCurrent`, and `SDL_GL_SwapWindow`. |
| Existing Win32/WGL layer | Keep as legacy/reference adapter during transition | Preserve current working Windows/OpenGL runtime | Native current fit | Low | HIGH | Do not big-bang replace it. First introduce `IPlatform`, `IWindow`, and `IGraphicsContext` ownership seams; then port examples to the seam. |
| GLFW 3.4 | Do **not** choose as primary platform layer | Alternative window/context library | Would fit OpenGL only | Med | HIGH | GLFW is excellent for OpenGL/Vulkan windows and has runtime platform selection/null backend, but Pyramid wants a game-engine foundation with broader input/controller/audio direction. SDL3 reduces future platform subsystem churn. |

**Prescriptive direction:**

1. Phase 1 creates interfaces and factories; no SDL dependency required yet.
2. Phase 2 keeps `Win32OpenGLWindow` compiling only on Windows and moves `Windows.h` out of public/core headers.
3. Phase 3 adds an SDL3 experimental platform adapter and migrates examples only after the seam is stable.

### Graphics Backend / Context / Loader

| Technology | Recommended Version / Status | Purpose | Current-Code Fit | Migration Cost | Confidence | Why |
|---|---:|---|---|---|---|---|
| OpenGL + GLAD | Keep existing vendored GLAD for OpenGL backend only | Current renderer backend | Already implemented | Low | HIGH | The milestone should clean leaks through `IGraphicsDevice`, not replace the active backend. Keep GLAD private to backend/platform code. |
| CMake `OpenGL::GL` | Replace direct `opengl32` linking where practical | Cross-platform OpenGL imported target | Current code links `opengl32` directly | Low | HIGH | Current unconditional `opengl32` is a known portability blocker. Use platform guards and imported targets instead. |
| Native Direct3D 9 | Windows SDK `d3d9.h`, link `D3d9.lib`, runtime `D3d9.dll` | Prototype backend seam | New backend, Windows-only | Medium | HIGH | Microsoft docs verify `Direct3DCreate9(D3D_SDK_VERSION)` creates `IDirect3D9`; requirements are `d3d9.h`, `D3d9.lib`, `D3d9.dll`. This is enough for a seam/prototype without new third-party dependencies. |
| `Microsoft::WRL::ComPtr` or tiny local COM RAII wrapper | Windows SDK COM lifetime ownership | DX9 prototype resources | New utility | Low | MEDIUM | Use RAII for COM pointers. Prefer SDK-provided WRL if available in CI; otherwise a tiny local `ComPtr` wrapper is acceptable. Do not introduce ATL just for COM ownership. |

**DX9 scope gate:** Implement only backend discovery/device creation/clear/present or one trivial triangle if needed to prove the seam. No full render-pass parity, shader abstraction overhaul, D3DX, Effects framework, or asset pipeline work.

### Testing

| Technology | Recommended Version / Status | Purpose | Current-Code Fit | Migration Cost | Confidence | Why |
|---|---:|---|---|---|---|---|
| GoogleTest | 1.17.0 latest verified release; pin exact tag/commit | Unit and integration test framework | Planned in docs, not wired | Medium | HIGH | GoogleTest 1.17.0 requires C++17, which matches Pyramid. Official quickstart shows FetchContent, `GTest::gtest_main`, `include(GoogleTest)`, and `gtest_discover_tests`. |
| CTest | Current CMake runner | Test orchestration and CI output | Already present | Low | HIGH | Keep CTest as the runner. GoogleTest should feed CTest, not replace it. |
| `gtest_discover_tests()` | Use for normal host tests | Per-test CTest registration | New helper | Low | HIGH | CMake docs prefer discovery for robust parameterized-test handling and no reconfigure needed when test lists change. Use `gtest_add_tests()` only for cross-compiling or tests that cannot run at discovery time. |
| Lightweight fakes | Prefer hand-written fakes over broad mocking | Platform/graphics seam tests | New pattern | Low/Med | MEDIUM | Keep tests understandable. Use gMock only where interface behavior matters; avoid over-mocking deterministic math/image utilities. |

**Test organization recommendation:**

```text
Tests/
  Unit/Core/
  Unit/Math/
  Unit/Utils/
  Unit/GraphicsContracts/
  Integration/Platform/
Engine/*/test/        # acceptable for module-local tests during migration
```

Keep existing `Engine/Utils/test` compiling while migrating tests into GoogleTest. Do not delete working executable tests until equivalent GoogleTest coverage exists.

### CI and Quality Gates

| Tool / Gate | Recommendation | Current-Code Fit | Migration Cost | Confidence | Why |
|---|---|---|---|---|---|
| GitHub Actions | Add/repair Windows-first workflow | No reliable current CI in integrations map; concerns mention one Windows preset but workflow was not detected in map | Low/Med | MEDIUM | Windows/MSVC is the only validated runtime. Start where the code works. |
| Configure gate | `cmake --preset vs2022-debug-tests` | Already exists | Low | HIGH | Fails fast on CMake drift. |
| Build gate | `cmake --build --preset build-debug-tests-clean --target PyramidEngine BasicGame BasicRenderingExample` | Mostly existing | Low | HIGH | Ensures engine and examples compile with tests enabled. |
| Test gate | `ctest --preset test-debug --output-on-failure` with `noTestsAction=error` | Existing preset can be tightened | Low | HIGH | Prevents silent no-test CI passes. |
| Smoke gate | Keep PowerShell smoke script as manual/local or scheduled Windows gate | Existing script | Low/Med | MEDIUM | GUI/GPU smoke can be flaky on hosted runners. Keep it as a documented phase gate locally; CI can run it later if reliable. |
| Format | Add `.clang-format`; initially check only changed/foundation files | New | Low | MEDIUM | Style consistency matters, but full-repo reformat would create noisy brownfield churn. |
| Static analysis | Add `clang-tidy` preset later as non-blocking | New | Medium | LOW/MEDIUM | Valuable but noisy on brownfield C++ and Windows/OpenGL code. Do not make it blocking before baseline cleanup. |
| Sanitizers | Defer broad sanitizer gates | New | Medium/High | MEDIUM | Useful for asset parsing and memory safety, but Windows/MSVC ASan plus graphics code requires careful setup. Add after tests are framework-based. |

### Documentation Tooling

| Tool | Recommended Version / Status | Purpose | Current-Code Fit | Migration Cost | Confidence | Why |
|---|---:|---|---|---|---|---|
| Markdown docs | Keep | Human docs, roadmap, guides | Already used | Low | HIGH | Lowest friction and already in repo. Docs phase should be a truth pass, not a tooling migration. |
| Doxygen | Use current Doxygen 1.18.x if available in CI/local | C++ API reference from headers + Markdown pages | Not currently wired as a gate | Low/Med | HIGH | Doxygen officially supports C++ comment blocks and Markdown pages; ideal for public API honesty checks. |
| CMake doc target | Add `docs-api` / `docs-check` target | Build docs consistently | New | Low | HIGH | Makes docs validation a phase gate without requiring a hosted docs site. |
| MkDocs / Sphinx+Breathe | Do **not** adopt now | Alternative docs sites | New toolchain | Med/High | HIGH | Adds Python/site complexity before API docs are honest. Reconsider only after docs truth pass and Doxygen output are stable. |

## Decision Matrices

### Cross-Platform Windowing / Context Layer

| Criterion | SDL3 | GLFW 3.4 | Custom Win32/X11/Cocoa |
|---|---|---|---|
| OpenGL context support | Strong; `SDL_GL_*` APIs verified | Strong; primary use case | Must write/maintain per OS |
| Input/controller future | Stronger fit for game engine | Basic keyboard/mouse/gamepad | Expensive |
| Current code disruption | Medium/high if direct swap; medium if behind seam | Medium | Very high over time |
| Scope risk | Medium if SDL subsystems sprawl | Low/medium | High |
| Recommendation | **Choose behind seam** | Keep as rejected alternative | Do not choose |

**Decision:** Use SDL3 as the preferred future platform adapter, but do not let SDL types escape engine public APIs. Keep Win32/WGL adapter until SDL3 proves parity for examples.

### Dependency Management

| Option | Recommendation | Why |
|---|---|---|
| CMake FetchContent with pinned release archive/commit | **Use for GoogleTest and optional SDL3 experiment** | Minimal ecosystem shift; official CMake and GoogleTest docs support it; good for CI repeatability when pinned. |
| vcpkg manifest | Do not adopt in this milestone | Good ecosystem, but adds package-manager policy, bootstrap, triplets, binary cache, and contributor docs beyond foundation scope. |
| Conan | Do not adopt | Even more process/tooling overhead for the current dependency set. |
| Vendoring everything | Keep only existing GLAD; avoid more vendoring by default | Vendoring SDL/GoogleTest increases repo maintenance unless offline builds become a hard requirement. |

### Documentation

| Option | Recommendation | Why |
|---|---|---|
| Markdown + Doxygen | **Use** | Fits C++ API honesty and current docs. |
| Sphinx+Breathe | Defer | More powerful but too much Python tooling for a foundation-only pass. |
| MkDocs | Defer | Good website generator, but does not solve C++ API extraction. |
| README-only | Reject | Cannot enforce public API truth or header documentation consistency. |

## CMake Organization Requirements

Use target-based CMake and keep module boundaries visible without prematurely splitting the engine into many libraries.

Recommended structure:

```cmake
option(PYRAMID_BUILD_TESTS "Build tests" OFF)
option(PYRAMID_BUILD_EXAMPLES "Build examples" ON)
option(PYRAMID_PLATFORM_WIN32 "Enable Win32 platform adapter" ON) # guarded by WIN32
option(PYRAMID_PLATFORM_SDL3 "Enable SDL3 platform adapter" OFF)
option(PYRAMID_RENDER_OPENGL "Enable OpenGL backend" ON)
option(PYRAMID_RENDER_D3D9 "Enable Direct3D 9 prototype backend" OFF) # WIN32 only

find_package(OpenGL REQUIRED) # only when OpenGL backend enabled
target_link_libraries(PyramidEngine PRIVATE OpenGL::GL)
```

Rules:

- Keep `PyramidEngine` as the main library target for this milestone unless target splitting becomes necessary to enforce dependencies.
- Use module-local `CMakeLists.txt` with `target_sources(PyramidEngine ...)` as today, but make sources conditional by platform/backend.
- Add small internal interface/helper targets only when they prevent dependency leaks, for example `PyramidWarnings`, `PyramidOptions`, or `PyramidTestSupport`.
- Public headers must not include `Windows.h`, GLAD, SDL, or `d3d9.h` unless the header is explicitly a backend/platform implementation header outside the stable public API.
- Treat renderer backend selection as a factory/configuration concern, not as public enum claims for unimplemented APIs.

## What NOT To Use

| Do Not Use | Why Not |
|---|---|
| SDL_Renderer or SDL GPU API for Pyramid rendering | It bypasses the engine's renderer/backend seam and becomes a renderer rewrite. SDL3 should own platform/window/input, not Pyramid's rendering abstraction. |
| D3DX9 / Effects framework | Deprecated legacy helper stack; unnecessary for a prototype seam and would obscure backend ownership. |
| Direct3D 11/12 or Vulkan in this milestone | Better modern APIs, but they do not answer the explicit DX9 prototype seam and would explode renderer scope. |
| vcpkg/Conan migration | Valuable later, but too much build-process churn for two immediate dependencies. |
| Full package/install/export refactor | Defer until public API honesty and target structure are stable. |
| Full-repo clang-tidy as blocking gate | Brownfield noise will block useful foundation work. Baseline first, then tighten. |
| Full CI graphics correctness tests on hosted runners | GPU/window availability is variable. Keep compile/tests blocking; smoke locally or scheduled until reliable. |
| Sphinx+Breathe/MkDocs site stack | Docs truth matters more than site polish. Doxygen + Markdown is enough for this milestone. |

## Installation / Integration Sketch

```cmake
# GoogleTest: tests only, pinned exact release/commit.
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  # Add URL_HASH during implementation.
  EXCLUDE_FROM_ALL
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(PyramidMathTests Tests/Unit/Math/Vec3Tests.cpp)
target_link_libraries(PyramidMathTests PRIVATE PyramidEngine GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(PyramidMathTests DISCOVERY_MODE PRE_TEST)
```

```cmake
# DX9 prototype: Windows only.
if(WIN32 AND PYRAMID_RENDER_D3D9)
  target_sources(PyramidEngine PRIVATE
    Engine/Graphics/source/D3D9/D3D9Device.cpp
    Engine/Graphics/source/D3D9/D3D9SwapChain.cpp
  )
  target_link_libraries(PyramidEngine PRIVATE d3d9)
  target_compile_definitions(PyramidEngine PRIVATE PYRAMID_HAS_D3D9=1)
endif()
```

## Roadmap Implications

1. **Build/test foundation first:** align CMake minimum/presets, add GoogleTest, keep existing tests, and make CTest fail on no tests.
2. **Seams before replacement:** introduce platform/context/backend interfaces before SDL3 or DX9 implementation work.
3. **Renderer honesty before backend parity:** remove GL leaks and unimplemented public render-pass declarations before adding real DX9 work.
4. **CI gates grow gradually:** Windows build + CTest blocking first; docs and smoke gates after they are deterministic.
5. **Docs phase uses Doxygen + Markdown:** document actual APIs/limitations and fail docs only after the truth pass removes stale claims.

## Sources

- SDL3 Context7 docs: `SDL_GL_CreateContext`, `SDL_GL_SetAttribute`, `SDL_GL_MakeCurrent`, `SDL_GL_SwapWindow` available since SDL 3.2.0. Confidence: HIGH.
- SDL GitHub releases: SDL 3.4.8 latest release on 2026-05-02. Confidence: HIGH.
- GLFW official docs/download: GLFW 3.4 released 2024-02-23; docs verify window/context creation, runtime platform selection, null platform, thread-safety limits. Confidence: HIGH.
- GoogleTest official quickstart and releases: GoogleTest 1.17.0 latest release on 2026-04-30; requires C++17; CMake integration via FetchContent, `GTest::gtest_main`, and `gtest_discover_tests`. Confidence: HIGH.
- CMake official docs: CMakePresets, FetchContent, and GoogleTest modules. Confidence: HIGH.
- Microsoft Learn Direct3D9 docs: `Direct3DCreate9`, `d3d9.h`, `D3d9.lib`, `D3d9.dll`. Confidence: HIGH.
- Doxygen official docs: C++ comment blocks and Markdown pages supported; fetched docs generated by Doxygen 1.18.0. Confidence: HIGH.
- Pyramid codebase maps: `.planning/codebase/STACK.md`, `INTEGRATIONS.md`, `CONCERNS.md`, `TESTING.md`. Confidence: HIGH for current-code fit.
