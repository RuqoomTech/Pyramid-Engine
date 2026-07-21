# Building and testing

## Supported build

The checked-in build is a **Windows x64 / Visual Studio 2022** configuration. The engine links `opengl32` directly and compiles a Win32 platform source file unconditionally, so Linux and macOS are not supported by this snapshot.

The root `CMakeLists.txt` accepts CMake 3.16, but `CMakePresets.json` requires **CMake 3.23 or newer**. Use 3.23+ for a consistent workflow.

### Prerequisites

- Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK
- CMake 3.23+
- Git when cloning from a repository
- OpenGL 4.6-capable GPU/driver for the bundled examples

The window code attempts core contexts from OpenGL 4.6 down to 3.3. If modern context creation is unavailable, it can retain the temporary legacy context used during WGL bootstrap. Both examples embed `#version 460 core` shaders, so they require a real OpenGL/GLSL 4.6 context.

## Preset workflow

Configure and build the default Debug configuration:

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

Clean and rebuild:

```powershell
cmake --build --preset build-debug-clean
```

The configure helper removes the build directory first and falls back to `build-clean` when files are locked:

```powershell
./scripts/configure-clean.ps1 -Preset vs2022-debug
```

## Tests

Configure the test preset, build, and run CTest:

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests-clean
ctest --preset test-debug
```

The following utility targets are registered with CTest:

- `TestPNGComponents`
- `TestJPEGSimple`
- `TestJPEGHuffman`
- `TestJPEGHuffmanDebug`
- `TestJPEGDequantizer`
- `TestJPEGIDCT`
- `TestJPEGColorConverter`
- `TestJPEGIntegration`

Additional test source files exist under `Engine/Utils/test/` but are not registered in its `CMakeLists.txt`; they are not part of the default test run.

## Graphical smoke test

After building the examples:

```powershell
./scripts/run-smoke.ps1 -BuildDir build -Config Debug -DurationSeconds 5
```

The script locates and launches:

- `BasicGame` (`BasicGamed.exe` in Debug because the target has a debug postfix)
- `BasicRenderingExample`

A process that remains alive for the requested duration passes the smoke check. This verifies startup stability, not rendering correctness. Inspect the windows and logs when changing graphics code.

## CMake options

| Option | Default | Effect |
|---|---:|---|
| `PYRAMID_BUILD_EXAMPLES` | `ON` | Builds `BasicGame` and `BasicRenderingExample` |
| `PYRAMID_BUILD_TESTS` | `OFF` | Enables CTest and utility test targets |
| `PYRAMID_BUILD_TOOLS` | `OFF` | Reserved; enabling it currently references a missing `Tools/AssetProcessor` directory |

Manual configuration remains available:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DPYRAMID_BUILD_EXAMPLES=ON `
  -DPYRAMID_BUILD_TESTS=OFF `
  -DPYRAMID_BUILD_TOOLS=OFF
cmake --build build --config Debug
```

## Output layout

With the Visual Studio generator, typical Debug outputs are:

```text
build/lib/Debug/PyramidEngined.lib
build/bin/Debug/BasicGamed.exe
build/Examples/BasicRendering/Debug/BasicRenderingExample.exe
```

Exact library placement can vary by generator because archive output is configured under `build/lib` while multi-configuration generators append the configuration name.

## Install

```powershell
cmake --build build --config Release
cmake --install build --config Release --prefix install
```

The install rules copy public headers and the `PyramidEngine` library. Several placeholder module include directories do not currently exist, so install behavior should be validated before publishing an SDK.

## Common failures

### CMake cannot find the compiler

Open a **Developer PowerShell for VS 2022**, or repair the Visual Studio C++ workload and Windows SDK.

### `opengl32` or Win32 headers fail on a non-Windows host

This is expected. The current platform and link configuration is Windows-only.

### The example opens but shader compilation fails

Confirm that the driver exposes OpenGL/GLSL 4.6. The embedded example shaders use GLSL 4.60 even though some engine shader files use GLSL 3.30.

### The executable cannot find engine shaders

The engine receives `PYRAMID_SOURCE_DIR` at compile time and resolves bundled shader paths from the source tree. Moving only the executable may break this development-time lookup.

### The build directory is locked

Close running examples and Visual Studio processes that hold generated files, then rerun `scripts/configure-clean.ps1`. The helper can configure `build-clean` as a fallback.
