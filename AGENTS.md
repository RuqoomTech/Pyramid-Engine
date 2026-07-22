# Repository guidelines

## Scope

- `Engine/` builds the C++17 `PyramidEngine` library.
- Active modules are Core, Graphics, Math, Platform, and Utils.
- `Examples/BasicGame` and `Examples/BasicRendering` are the graphical references.
- `Tests/PublicApiLinkage.cpp` protects selected public symbols; `Tests/WindowResizeEventTests.cpp` protects resize callback semantics; `Tests/CameraViewportTests.cpp` protects projection resizing; `Tests/FramebufferResizeTests.cpp` protects framebuffer specification and zero-area resize behavior; `Tests/TextureLoadingTests.cpp` protects transactional file loading and OpenGL upload state; `Tests/SceneTransformTests.cpp` protects hierarchy composition, invalidation, reparenting, cycle rejection, and parent-destruction behavior; `Tests/CameraFrustumTests.cpp` protects camera orientation, frustum extraction, transformed bounds, and linear/octree scene visibility.
- `Tests/Consumer` validates the installed CMake package.
- `vendor/glad` is a bundled public dependency. libjpeg-turbo is an external open-source dependency resolved through CMake `FindJPEG`.
- The supported Windows toolchain is MSYS2 UCRT64 with MinGW-w64 GCC; Clang is also validated. Visual Studio is not required.
- Input, audio, physics, editor, scripting, DirectX, Vulkan, Linux, and macOS are not supported.

## Build and test

```powershell
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5
```

Release validation:

```powershell
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
ctest --preset test-gcc-release
```

## Style

- C++17, four spaces, braces on new lines.
- Types/public methods use `PascalCase`; locals/parameters use `camelCase`; fields use `m_`.
- Prefer RAII, explicit ownership, and `PYRAMID_LOG_*` diagnostics.
- Do not add required interface methods with silent no-op defaults.
- Do not expose source-tree absolute paths through installed target interfaces.

## Public APIs

Every public declaration must be implemented, removed, or documented as an explicit failure. Add linkage-sensitive symbols to `Tests/PublicApiLinkage.cpp`.

Do not describe placeholder algorithms as complete. Frustum culling is implemented; occlusion culling is not. Current examples and engine shaders target GLSL 3.30; the runtime requires OpenGL 3.3 core or newer.

## Tests

Tests must fail visibly, use valid fixtures, avoid false-success skips, clean temporary files, and print actionable context. Renderer changes require visual inspection because process smoke testing is not pixel validation.

## Documentation

Update the maintained compact set instead of adding overlapping guides or status files. Planned work belongs in `docs/ROADMAP.md`; historical changes belong in `CHANGELOG.md`.

## Pull requests

Include affected modules, ownership/API decisions, exact validation commands, visible rendering evidence, known limitations, and documentation/changelog updates.
