# Examples

The repository contains two graphical examples. There are no checked-in tutorial series or physics/input/audio demos.

## BasicGame

Target: `BasicGame`
Source: `Examples/BasicGame/`

Demonstrates the primary engine path:

- deriving from `Pyramid::Game`;
- initializing `RenderSystem`;
- creating a perspective camera;
- compiling an inline GLSL 4.60 shader;
- creating indexed cube geometry;
- adding a render object and directional light to a scene;
- updating camera/object state and rendering with the default shadow/forward pipeline.

Debug builds use the executable name `BasicGamed.exe` because the target defines `DEBUG_POSTFIX d`.

## BasicRenderingExample

Target: `BasicRenderingExample`
Source: `Examples/BasicRendering/`

Demonstrates lower-level rendering setup with direct resource creation, shaders, geometry, camera state, and draw submission. Its embedded shaders also use GLSL 4.60.

## Build

```powershell
cmake --preset vs2022-debug
cmake --build --preset build-debug
```

Typical Debug executables:

```text
build/bin/Debug/BasicGamed.exe
build/Examples/BasicRendering/Debug/BasicRenderingExample.exe
```

Run both through the smoke helper:

```powershell
./scripts/run-smoke.ps1 -BuildDir build -Config Debug -DurationSeconds 5
```

## Validation checklist

A smoke run only confirms that each process starts and does not fail immediately. For graphics changes, also verify:

1. The window creates a modern OpenGL context without a fallback warning.
2. Shaders compile and link without errors in the log.
3. Geometry is visible with the expected camera and depth behavior.
4. Resizing, closing, and repeated startup/shutdown do not trigger GL or lifetime errors.
5. `pyramid_game.log` contains no unexpected warnings or errors.

## Adding an example

1. Create `Examples/<Name>/CMakeLists.txt` and source files.
2. Link the executable to `PyramidEngine`.
3. Add the subdirectory under the `PYRAMID_BUILD_EXAMPLES` block in the root `CMakeLists.txt`.
4. Keep shaders/assets inside the example or use a documented resolver path.
5. Add a focused description here and extend `scripts/run-smoke.ps1` only when the example is stable enough for routine startup validation.
