# Repository guidelines

## Scope

- `Engine/` builds the C++17 `PyramidEngine` library.
- Active modules are Core, Graphics, Math, Platform, and Utils.
- `Examples/BasicGame` and `Examples/BasicRendering` are the graphical references.
- `Tests/PublicApiLinkage.cpp` protects selected public symbols.
- `Tests/Consumer` validates the installed CMake package.
- `vendor/glad` is a bundled public dependency.
- Input, audio, physics, editor, scripting, DirectX, Vulkan, Linux, and macOS are not supported.

## Build and test

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests
ctest --preset test-debug
./scripts/run-smoke.ps1 -BuildDir build/debug-tests -Config Debug -DurationSeconds 5
```

Release validation:

```powershell
cmake --preset vs2022-release-tests
cmake --build --preset build-release-tests
ctest --preset test-release
```

## Style

- C++17, four spaces, braces on new lines.
- Types/public methods use `PascalCase`; locals/parameters use `camelCase`; fields use `m_`.
- Prefer RAII, explicit ownership, and `PYRAMID_LOG_*` diagnostics.
- Do not add required interface methods with silent no-op defaults.
- Do not expose source-tree absolute paths through installed target interfaces.

## Public APIs

Every public declaration must be implemented, removed, or documented as an explicit failure. Add linkage-sensitive symbols to `Tests/PublicApiLinkage.cpp`.

Do not describe placeholder algorithms as complete. Current examples and engine shaders target GLSL 3.30; the runtime requires OpenGL 3.3 core or newer.

## Tests

Tests must fail visibly, use valid fixtures, avoid false-success skips, clean temporary files, and print actionable context. Renderer changes require visual inspection because process smoke testing is not pixel validation.

## Documentation

Update the maintained compact set instead of adding overlapping guides or status files. Planned work belongs in `docs/ROADMAP.md`; historical changes belong in `CHANGELOG.md`.

## Pull requests

Include affected modules, ownership/API decisions, exact validation commands, visible rendering evidence, known limitations, and documentation/changelog updates.
