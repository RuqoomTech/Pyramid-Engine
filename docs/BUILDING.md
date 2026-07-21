# Building and testing

## Supported environment

Pyramid Engine currently supports one build environment:

- Windows 10 or 11, x64
- Visual Studio 2022 with **Desktop development with C++**
- Windows SDK
- CMake 3.23 or newer
- OpenGL 3.3 core or newer

A normal configure on Linux or macOS fails immediately with a clear unsupported-platform message. The internal `PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE` option exists only for configure-time metadata validation; it does not make the engine buildable on those systems.

## Presets

| Configure preset | Build directory | Purpose |
|---|---|---|
| `vs2022-debug` | `build/debug` | Debug engine and examples |
| `vs2022-debug-tests` | `build/debug-tests` | Debug engine, examples, and tests |
| `vs2022-release-tests` | `build/release-tests` | Release engine, examples, and tests |

### Debug

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

### Debug with tests

```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests
ctest --preset test-debug
```

### Release with tests

```powershell
cmake --preset vs2022-release-tests
cmake --build --preset build-release-tests
ctest --preset test-release
```

Clean configuration helper:

```powershell
./scripts/configure-clean.ps1 -Preset vs2022-debug-tests
```

## CMake options

| Option | Default | Purpose |
|---|---:|---|
| `PYRAMID_BUILD_EXAMPLES` | `ON` | Build both graphical examples |
| `PYRAMID_BUILD_TESTS` | `OFF` | Enable CTest and all maintained tests |
| `PYRAMID_WARNINGS_AS_ERRORS` | `OFF` | Promote MSVC warnings to errors |
| `PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE` | `OFF` | Configure-only validation outside Windows |

Manual configuration:

```powershell
cmake -S . -B build/manual -G "Visual Studio 17 2022" -A x64 `
  -DPYRAMID_BUILD_EXAMPLES=ON `
  -DPYRAMID_BUILD_TESTS=ON
cmake --build build/manual --config Debug --parallel
ctest --test-dir build/manual -C Debug --output-on-failure
```

## Tests

CTest registers 11 executables:

- `Utils.TestPNGComponents`
- `Utils.TestPNGLoader`
- `Utils.TestJPEGSimple`
- `Utils.TestJPEGParser`
- `Utils.TestJPEGHuffman`
- `Utils.TestJPEGHuffmanDebug`
- `Utils.TestJPEGDequantizer`
- `Utils.TestJPEGIDCT`
- `Utils.TestJPEGColorConverter`
- `Utils.TestJPEGIntegration`
- `API.PublicApiLinkage`

The API test takes addresses of public factory and scene-management methods so missing definitions fail at link time.

## Graphical smoke test

```powershell
./scripts/run-smoke.ps1 -BuildDir build/debug -Config Debug -DurationSeconds 5
```

The script starts `BasicGame` and `BasicRenderingExample`, then fails if either exits abnormally during the requested interval. Visually inspect both windows after renderer changes.

Typical Debug outputs:

```text
build/debug/bin/Debug/BasicGame.exe
build/debug/bin/Debug/BasicRenderingExample.exe
build/debug/lib/Debug/PyramidEngined.lib
```

## Installable package

Build and install:

```powershell
cmake --preset vs2022-release-tests
cmake --build --preset build-release-tests
cmake --install build/release-tests --config Release --prefix install
```

The installation contains:

- public Pyramid headers;
- GLAD headers;
- `PyramidEngine` and `glad` libraries;
- `PyramidEngineConfig.cmake` and version metadata;
- exported targets under the `Pyramid::` namespace.

Consumer:

```cmake
find_package(PyramidEngine CONFIG REQUIRED)
target_link_libraries(MyTarget PRIVATE Pyramid::Engine)
```

`Tests/Consumer` is the reference external consumer and is built by Windows CI after installation.

## CI

`.github/workflows/windows-ci.yml` validates Debug and Release independently:

1. configure;
2. build engine, examples, and tests;
3. run CTest;
4. install into a clean prefix;
5. configure and build the external consumer;
6. run the consumer executable;
7. upload available binaries.

The hosted runner does not validate rendered pixels or interactive behavior.

## Troubleshooting

### Unsupported platform message

Use Windows. The configure-only override is not a supported build mode.

### OpenGL context creation fails

Update the GPU driver and confirm OpenGL 3.3 core support. The window layer attempts versions from 4.6 down to 3.3 and rejects legacy fallback contexts.

### Shader compilation fails

The bundled engine shaders and examples use GLSL 3.30. Inspect the shader log and the reported OpenGL/GLSL versions.

### Executable cannot locate engine shaders

Development builds resolve checked-in shaders using the compile-time source root. Copying only an executable away from the repository can break this lookup.

### Build files are locked

Close Visual Studio and running examples, then rerun `scripts/configure-clean.ps1`.
