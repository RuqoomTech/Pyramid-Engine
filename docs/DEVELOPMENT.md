# Development guide

## Workflow

1. Create a focused branch.
2. Configure a test preset.
3. Build and run CTest before changing behavior.
4. Make one coherent subsystem change.
5. Add or update tests, including linkage coverage for public APIs.
6. Run the graphical smoke test and visually inspect renderer changes.
7. Update existing documentation and the changelog.
8. Record exact validation commands in the pull request.

Recommended local validation:

```powershell
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5
```

## Conventions

- C++17 with extensions disabled.
- Four-space indentation and braces on new lines.
- `PascalCase` for types and most public methods.
- `camelCase` for locals and parameters.
- `m_` prefix for fields.
- RAII and smart pointers for ownership.
- `PYRAMID_LOG_*` and assertion macros for diagnostics.
- No required interface operation may silently default to a no-op.
- Platform-specific APIs belong behind platform implementation headers.

The primary toolchain enables GCC/Clang `-Wall -Wextra -Wpedantic`. Warnings are not errors by default because existing warnings still need cleanup. New code should not add warnings.

## Public API discipline

For every public method, choose one behavior:

1. implement it;
2. remove it before release;
3. fail explicitly with a documented capability limitation.

Do not leave declarations without definitions. Add the symbol to `Tests/PublicApiLinkage.cpp` when a factory or subsystem method is especially vulnerable to linker regressions.

## Tests

Utility tests live under `Engine/Utils/test` and are registered by `add_utils_test`. A test must:

- return non-zero on failure;
- avoid hidden skips that report success;
- use standards-valid embedded fixtures;
- clean up temporary files;
- print enough context to diagnose a failure.

`API.PublicApiLinkage` verifies selected exported symbols. `Platform.WindowResizeEvents` verifies the platform-neutral callback contract without requiring a Win32 window. `Graphics.CameraViewportResize` verifies perspective/orthographic resize behavior and zero-area rejection. `Graphics.FramebufferResize` verifies structural attachment rules, multisample consistency, and zero-area resize preservation without requiring a graphics context. Windows CI validates GCC and Clang in Debug and Release, plus installation and an external `find_package` consumer.

Renderer changes require manual visual validation until image-regression tests exist.

Debug builds request an OpenGL debug context and attach a synchronous driver callback when supported. Driver warnings and errors are routed through `PYRAMID_LOG_*`; notification-level messages are suppressed. Release builds do not enable the callback.

Window resize callbacks run synchronously on the game thread while `ProcessMessages()` dispatches native events. Before the virtual hook runs, `Game` updates the default viewport, synchronizes the camera registered through `SetActiveCamera()`, resizes the `RenderSystem` registered through `SetRenderSystem()`, and marks zero-sized/minimized surfaces non-renderable. Keep custom handlers lightweight, reject `!event.HasRenderableArea()`, resize only standalone targets there, and avoid retaining references to the callback argument.

## CMake and package changes

After changing targets, include directories, or installation:

```bash
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
ctest --preset test-gcc-release
cmake --install build/gcc-release-tests --prefix install
cmake -S Tests/Consumer -B build/consumer -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_PREFIX_PATH="$PWD/install"
cmake --build build/consumer --parallel
```

Keep build-tree paths out of installed target interfaces. Public headers should be installed as files, not exposed as absolute `INTERFACE_SOURCES`.

## Documentation

Maintain this fixed set:

```text
README.md
docs/README.md
docs/BUILDING.md
docs/ARCHITECTURE.md
docs/API.md
docs/EXAMPLES.md
docs/DEVELOPMENT.md
docs/ROADMAP.md
CHANGELOG.md
```

Do not add separate blocker reports, status snapshots, duplicated setup guides, or speculative API references.

## Pull requests

Include:

- purpose and affected modules;
- public API or ownership changes;
- exact Debug/Release/test commands;
- screenshots or video for visual changes;
- known limitations and follow-up work;
- documentation and changelog updates.

## Release hygiene

Before tagging:

1. synchronize CMake version, status, tag, and changelog;
2. pass clean GCC and Clang Debug/Release CI;
3. run all registered tests;
4. manually run and inspect both examples;
5. install into an empty prefix and build the external consumer;
6. verify every documented public method is implemented or explicitly unsupported;
7. review the roadmap and remove completed P0 items.
