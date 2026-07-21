# Repository guidelines

## Project structure

- `Engine/` builds the `PyramidEngine` C++17 library.
- Active modules use `include/` and `source/` directories: Core, Graphics, Math, Platform, and Utils.
- Audio, Input, and Physics are placeholders and must not be described as implemented.
- `Engine/Utils/test/` contains utility test executables.
- `Examples/BasicGame` and `Examples/BasicRendering` are the current runnable examples.
- `docs/` contains the maintained compact documentation set.
- `vendor/glad/` is the bundled OpenGL/WGL loader.
- Generated `build*` directories stay uncommitted.

## Build and test

Use the Windows/Visual Studio presets from the repository root:

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

With tests:

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests-clean
ctest --preset test-debug
```

For graphics/runtime changes:

```powershell
./scripts/run-smoke.ps1 -BuildDir build -Config Debug -DurationSeconds 5
```

The current source is Win32/OpenGL-specific. Do not present Linux, macOS, DirectX, or Vulkan as supported.

## Style

- C++17, four-space indentation, braces on new lines.
- Types and most public methods: `PascalCase`.
- Locals and parameters: `camelCase`.
- Fields: `m_` prefix.
- Prefer RAII and explicit ownership.
- Use `PYRAMID_LOG_*`, `PYRAMID_ASSERT`, and `PYRAMID_CORE_ASSERT` for diagnostics.
- Avoid adding required interface methods with silent no-op defaults.

## Tests

Add focused `Test<Feature>.cpp` executables and register them with CTest. Tests must return non-zero on failure and print actionable context. Renderer changes also require visual inspection; the smoke script only detects early process failure.

## Documentation

Update an existing maintained document rather than adding overlapping guides or status reports. Verify paths, target names, namespaces, signatures, platform requirements, and implementation status against source. Planned behavior belongs in `docs/ROADMAP.md`, not `docs/API.md`.

## Pull requests

Use concise imperative commit subjects. Include the affected modules, design/lifetime decisions, exact build and test commands, visible-output evidence for rendering changes, known limitations, and documentation/changelog updates.
