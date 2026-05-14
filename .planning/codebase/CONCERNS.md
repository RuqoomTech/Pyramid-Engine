# Codebase Concerns

**Analysis Date:** Thu May 14 2026

## Tech Debt

**Rendering command/resource lifetime model:**
- Issue: `CommandBuffer` stores raw pointers as `std::uintptr_t` in a union and also keeps raw pointer registries keyed by numeric IDs. No ownership, generation, or invalidation contract protects queued commands from resources destroyed before `Execute()`.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp`
- Impact: Use-after-free or stale binding is possible when render targets, shaders, textures, uniform buffers, or vertex arrays are released while commands remain queued.
- Fix approach: Prefer handles with generation counters or `std::weak_ptr`/validated resource registries owned by `RenderSystem`; reject unresolved resources before issuing GL calls.

**Incomplete framebuffer abstraction split:**
- Issue: `OpenGLDevice::BindFramebuffer(IFramebuffer*)` only records `"Framebuffer binding not yet implemented"`, while other code binds native framebuffer handles directly.
- Files: `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`
- Impact: Engine code bypasses `IGraphicsDevice`, making render paths OpenGL-specific and preventing backend abstraction from being reliable.
- Fix approach: Complete `IFramebuffer` binding in `OpenGLDevice`, then route `RenderTarget`/`OpenGLFramebuffer` binding through the device API rather than direct GL/state-manager calls.

**Render pass declarations exceed implementation:**
- Issue: `TransparentPass`, `PostProcessPass`, `UIRenderPass`, `DebugRenderPass`, and `RenderPassFactory` are declared in the public header but have no source definitions detected.
- Files: `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderPasses.hpp`, `Engine/Graphics/CMakeLists.txt`
- Impact: Users can include and call APIs that fail at link time; documentation and API surface imply capabilities that are not present.
- Fix approach: Either implement source files and add them to `Engine/Graphics/CMakeLists.txt`, or remove/hide unfinished public APIs behind feature flags.

**Primitive factory functions do not upload geometry:**
- Issue: `CreateCube()`, `CreateSphere()`, and `CreatePlane()` generate local `vertices`/`indices` but return `RenderObject` without vertex/index buffers because they lack graphics-device access.
- Files: `Engine/Graphics/source/Scene.cpp`
- Impact: Created primitives appear valid by name/scale but contain no GPU-ready geometry, causing silent rendering gaps.
- Fix approach: Accept `IGraphicsDevice&` or a mesh-builder service, store CPU mesh data explicitly, and create buffers through the existing buffer/vertex-array APIs.

**Scene statistics and culling are placeholders:**
- Issue: `SceneManager::UpdateTransforms()` is empty, `GetStats()` forces `totalNodes` and `totalObjects` to zero, and frustum/occlusion culling always returns false.
- Files: `Engine/Graphics/source/Scene/SceneManager.cpp`
- Impact: Scene update metrics are misleading, visibility culling does not reduce draw work, and callers cannot trust reported scene complexity.
- Fix approach: Add scene graph traversal/count APIs, update transforms from dirty nodes, and implement frustum culling before adding occlusion culling.

**Image loader ownership is manual:**
- Issue: `ImageData` exposes raw `unsigned char*` with a separate `Image::Free()` requirement; TGA/BMP/JPEG paths allocate with `new[]` directly.
- Files: `Engine/Utils/include/Pyramid/Util/Image.hpp`, `Engine/Utils/source/Image.cpp`, `Engine/Utils/source/JPEGLoader.cpp`
- Impact: Texture loading callers can leak or double-free image buffers; exception or early-return paths require careful manual cleanup.
- Fix approach: Replace raw image buffers with RAII (`std::vector<unsigned char>` or `std::unique_ptr<unsigned char[]>`) and make `ImageData` own its buffer.

## Known Bugs

**JPEG loader returns a generated test pattern instead of decoded pixels:**
- Symptoms: Loading a `.jpg`/`.jpeg` can succeed but fills output with a gradient/block test pattern.
- Files: `Engine/Utils/source/JPEGLoader.cpp`
- Trigger: Any code path using `Util::Image::Load()` for JPEG textures reaches `JPEGLoader::DecodeImageData()`.
- Workaround: Use PNG/TGA/BMP for visual assets until full JPEG MCU/block decoding is implemented.

**Computed image sizes can overflow or accept invalid dimensions:**
- Symptoms: Malformed BMP/TGA headers with negative/huge dimensions can produce incorrect `uint32_t imageSize` calculations and unsafe allocations/reads.
- Files: `Engine/Utils/source/Image.cpp`
- Trigger: Loading untrusted or corrupted `.bmp`/`.tga` files through `Util::Image::Load()`.
- Workaround: Treat asset files as trusted input; add dimension bounds and checked multiplication before production use.

**Smoke-test executable names may diverge from CMake target names:**
- Symptoms: `scripts/run-smoke.ps1` looks for `BasicRenderingExample.exe`, while docs and target layout also reference `BasicRendering` variants.
- Files: `scripts/run-smoke.ps1`, `docs/SmokeTests.md`, `Examples/BasicRendering/CMakeLists.txt`, `README.md`
- Trigger: Running smoke tests after a build that emits a different target filename/path.
- Workaround: Resolve executable paths from CMake targets or keep script candidates synchronized with `Examples/BasicRendering/CMakeLists.txt`.

## Security Considerations

**Untrusted asset parsing lacks hard limits:**
- Risk: Custom BMP/TGA/PNG/JPEG parsing performs heap allocation from file-controlled dimensions and compressed data without a central maximum image size policy.
- Files: `Engine/Utils/source/Image.cpp`, `Engine/Utils/source/PNGLoader.cpp`, `Engine/Utils/source/JPEGLoader.cpp`, `Engine/Utils/source/Inflate.cpp`, `Engine/Utils/source/ZLib.cpp`
- Current mitigation: Format checks reject unsupported bit depth/compression in some paths.
- Recommendations: Add maximum width/height/pixel count, checked arithmetic, compressed-size limits, and fuzz/regression tests for malformed images.

**Raw GL resource handles and direct deletion require context correctness:**
- Risk: Destructors call `glDelete*` directly and assume a valid/current OpenGL context.
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`, `Engine/Graphics/source/OpenGL/OpenGLTexture.cpp`
- Current mitigation: RAII destructors release handles when objects are destroyed.
- Recommendations: Centralize graphics-resource destruction on the render thread/context owner, and guard deletion paths for shutdown order.

**Logging writes runtime files in the current working directory:**
- Risk: Examples/tests may create `pyramid_game.log` relative to the process working directory, which can be unexpected in packaged or restricted environments.
- Files: `Engine/Utils/source/Log.cpp`, `scripts/run-smoke.ps1`
- Current mitigation: Logging uses engine macros and smoke tests check for startup markers.
- Recommendations: Route logs through configurable app-data/build output paths and document permissions expectations.

## Performance Bottlenecks

**Visibility culling is not active:**
- Problem: Frustum and occlusion culling currently return false for every object, so all objects remain candidates.
- Files: `Engine/Graphics/source/Scene/SceneManager.cpp`
- Cause: Culling functions are placeholder implementations.
- Improvement path: Implement camera frustum tests against object bounds, then add occlusion only after correctness tests exist.

**Framebuffer implementation is very large and multi-purpose:**
- Problem: `OpenGLFramebuffer.cpp` is over 1000 lines and owns validation, creation, resize, attachment management, pixel readback, blitting, and utility helpers.
- Files: `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`
- Cause: Several framebuffer responsibilities are implemented in one translation unit.
- Improvement path: Split attachment creation/validation/readback helpers into focused files and add unit-level tests around resize and attachment formats.

**Render command execution logs unresolved resources at runtime:**
- Problem: Missing command resources are detected only during `Execute()` and continue through the command stream.
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`
- Cause: Commands are accepted during recording without validation against a stable resource registry.
- Improvement path: Validate command arguments during recording or at `End()`, and return an execution status from `CommandBuffer::Execute()`.

## Fragile Areas

**Windows-only platform layer:**
- Files: `Engine/Platform/CMakeLists.txt`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`, `Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, `Engine/Core/source/Game.cpp`, `Engine/CMakeLists.txt`
- Why fragile: The platform target always compiles Win32/WGL sources and links `opengl32`; `Game.cpp` includes the Windows OpenGL window directly.
- Safe modification: Introduce platform-conditional CMake sources and a platform factory; keep public `Window` APIs independent of `Windows.h`.
- Test coverage: No Linux/macOS CI matrix detected; `.github/workflows/ci.yml` runs only `windows-latest`.

**x86 SIMD assumptions:**
- Files: `Engine/Math/source/MathSIMD.cpp`, `Engine/Math/include/Pyramid/Math/MathSIMD.hpp`, `Engine/Math/include/Pyramid/Math/MathPerformance.hpp`
- Why fragile: SIMD headers use MSVC intrinsics or `<x86intrin.h>`/CPUID; non-x86 architectures need separate code paths.
- Safe modification: Guard x86 implementations with architecture checks and provide scalar/default implementations for ARM and unsupported compilers.
- Test coverage: No detected math/SIMD test executables in `Engine/Utils/test/` or CMake test registration.

**OpenGL-specific code leaks through high-level layers:**
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/CMakeLists.txt`
- Why fragile: High-level renderer files include `glad/glad.h` and OpenGL framebuffer classes directly, coupling rendering architecture to OpenGL.
- Safe modification: Move GL-specific operations behind `IGraphicsDevice`/framebuffer interfaces before adding new renderer features.
- Test coverage: Rendering validation is smoke-test based; no headless renderer tests are registered.

**Win32 initialization cleanup on partial failure:**
- Files: `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`
- Why fragile: `Initialize()` can return false after creating `HWND`/`HDC`/temporary GL state; destructor eventually cleans up, but repeated initialize/failure paths are hard to reason about.
- Safe modification: Use scoped cleanup for each initialization stage and reset handles immediately on failed setup.
- Test coverage: No platform-window unit tests or failure-injection tests detected.

## Scaling Limits

**Texture slot limit is hardcoded to 32:**
- Current capacity: `kMaxTextureSlots = 32` in `OpenGLDevice`.
- Limit: Hardware with fewer texture units or shader paths requiring more explicit slots can fail or log errors only at bind time.
- Scaling path: Query `GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS`/per-stage limits during device initialization and expose validated limits in `DeviceInfo`.

**Scene/object counts are unavailable:**
- Current capacity: `SceneManager::GetStats()` reports zero total nodes/objects.
- Limit: Tools and runtime decisions cannot scale scene updates based on real object counts.
- Scaling path: Add counted scene graph APIs and update stats from authoritative containers in `Scene`/`SceneManager`.

**CI exercises only one OS and one preset:**
- Current capacity: `.github/workflows/ci.yml` uses `windows-latest`, `vs2022-debug-tests`, and `ctest --preset test-debug`.
- Limit: Portability regressions for non-Windows generators, release builds, and non-MSVC compilers are not detected.
- Scaling path: Add Ninja/Clang or GCC matrix entries once platform CMake conditionals exist.

## Dependencies at Risk

**Bundled glad/WGL-only OpenGL loader:**
- Risk: `vendor/glad/CMakeLists.txt` builds `glad.c` and `glad_wgl.c` only.
- Impact: The dependency is tied to Windows WGL and does not support GLX/EGL/Cocoa context paths despite `glad_glx.h` existing in `vendor/glad/include/glad/`.
- Migration plan: Generate/load platform-specific glad sources conditionally or move to a cross-platform window/context library.

**Direct `opengl32` link dependency:**
- Risk: `Engine/CMakeLists.txt` links `opengl32` unconditionally.
- Impact: Non-Windows builds fail at configure/link time.
- Migration plan: Use CMake `find_package(OpenGL)` and platform-specific imported targets guarded by `WIN32`/`APPLE`/`UNIX`.

**CMake minimum mismatch between root and presets:**
- Risk: Root `CMakeLists.txt` allows 3.16, but `CMakePresets.json` requires CMake 3.23.
- Impact: Users following manual commands can configure with older CMake while preset users need newer tooling; docs list mixed requirements.
- Migration plan: Align `cmake_minimum_required()`, `CMakePresets.json`, `README.md`, and `docs/BuildGuide.md` on one supported minimum.

## Missing Critical Features

**Full JPEG decoding:**
- Problem: JPEG Huffman/dequantizer/IDCT components exist, but image assembly returns a synthetic test pattern.
- Blocks: Production use of `.jpg`/`.jpeg` textures and README claim of complete JPEG baseline DCT support.

**Audio/Input/Physics/Scene/Renderer module implementation:**
- Problem: Several module directories contain only `CMakeLists.txt` include-directory setup and no source/header files.
- Blocks: Public roadmap/API expectations for audio, input, physics, separate scene module, and separate renderer module.

**Headless/automated rendering verification:**
- Problem: GUI smoke tests exist, but CI uses `ctest` only; render correctness depends on local desktop/GPU execution.
- Blocks: Reliable validation of graphics changes in automated environments.

## Test Coverage Gaps

**Graphics/rendering code lacks unit or integration tests:**
- What's not tested: `CommandBuffer`, render passes, framebuffers, OpenGL device binding, shaders, textures, scene culling, and primitive creation.
- Files: `Engine/Graphics/source/Renderer/RenderSystem.cpp`, `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `Engine/Graphics/source/Scene.cpp`, `Engine/Graphics/source/Scene/SceneManager.cpp`
- Risk: Renderer regressions appear only in examples or manual smoke runs.
- Priority: High

**Several existing test files are not registered in CMake:**
- What's not tested: `TestPNGLoader.cpp` and `TestJPEGParser.cpp` are present but not added via `add_utils_test()`.
- Files: `Engine/Utils/test/CMakeLists.txt`, `Engine/Utils/test/TestPNGLoader.cpp`, `Engine/Utils/test/TestJPEGParser.cpp`
- Risk: PNG loader and JPEG parser regressions can be missed by CI.
- Priority: Medium

**Utility image tests do not cover malicious/corrupt boundaries:**
- What's not tested: Oversized dimensions, truncated rows, invalid chunk lengths, unsupported compression paths, and integer overflow cases.
- Files: `Engine/Utils/source/Image.cpp`, `Engine/Utils/source/PNGLoader.cpp`, `Engine/Utils/source/JPEGLoader.cpp`, `Engine/Utils/test/`
- Risk: Asset loading bugs and memory safety issues can reach runtime.
- Priority: High

**Math/SIMD has no detected CTest coverage:**
- What's not tested: Scalar/SIMD equivalence, CPU feature detection, alignment assumptions, and fallback behavior.
- Files: `Engine/Math/source/MathSIMD.cpp`, `Engine/Math/include/Pyramid/Math/MathSIMD.hpp`, `Engine/Math/include/Pyramid/Math/MathPerformance.hpp`
- Risk: Platform/compiler-specific math errors can go unnoticed.
- Priority: Medium

## Documentation Mismatches or Stale Areas

**Critical issues document is stale against current renderer code:**
- Issue: `docs/CRITICAL_ISSUES_AND_BLOCKERS.md` states binding APIs and render passes are missing, while current code includes binding APIs and several pass source files.
- Files: `docs/CRITICAL_ISSUES_AND_BLOCKERS.md`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Graphics/source/Renderer/ForwardRenderPass.cpp`, `Engine/Graphics/source/Renderer/DeferredGeometryPass.cpp`, `Engine/Graphics/source/Renderer/DeferredLightingPass.cpp`, `Engine/Graphics/source/Renderer/ShadowMapPass.cpp`
- Impact: Planners may prioritize already-completed work and miss current blockers such as incomplete framebuffer abstraction and missing factory/pass definitions.
- Fix approach: Refresh status tables and blockers from current code before using the document for roadmap decisions.

**README overstates asset and platform completeness:**
- Issue: `README.md` claims complete JPEG baseline support, cross-platform readiness, advanced render passes, and frustum culling; code contains JPEG test-pattern output, Windows-only platform binding, and placeholder culling.
- Files: `README.md`, `Engine/Utils/source/JPEGLoader.cpp`, `Engine/Platform/CMakeLists.txt`, `Engine/Core/source/Game.cpp`, `Engine/Graphics/source/Scene/SceneManager.cpp`
- Impact: New contributors and users get inaccurate expectations of production readiness.
- Fix approach: Mark incomplete systems as experimental and link to current limitations in `docs/CRITICAL_ISSUES_AND_BLOCKERS.md`.

---

*Concerns audit: Thu May 14 2026*
