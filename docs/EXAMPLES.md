# Examples

The repository contains two Windows graphical applications. Both use GLSL 3.30 and require an OpenGL 3.3-or-newer core context.

## BasicGame

Location: `Examples/BasicGame`

Demonstrates:

- deriving from `Pyramid::Game`;
- base lifecycle initialization;
- shader compilation;
- vertex/index buffer and vertex-array setup;
- texture loading;
- frame update and rendering;
- logging configuration.

Build:

```powershell
cmake --preset gcc-debug
cmake --build --preset build-gcc-debug
```

Run:

```powershell
./build/gcc-debug/bin/BasicGame.exe
```


## BasicRenderingExample

Location: `Examples/BasicRendering`

This is the lower-level reference rendering path. It demonstrates:

- inline GLSL 3.30 shaders;
- cube geometry with position, normal, texture-coordinate, and color attributes;
- scene and material uniform buffers;
- perspective camera setup;
- indexed rendering and basic lighting.

Run:

```powershell
./build/gcc-debug/bin/BasicRenderingExample.exe
```

## Smoke validation

```powershell
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug -DurationSeconds 5
```

A successful timed process run proves that startup did not immediately fail. After graphics changes, also verify:

- both windows open;
- shader compilation reports no errors;
- geometry is visible;
- resizing does not crash;
- closing exits cleanly;
- the OpenGL debug/error log remains clean.

The project does not yet include automated pixel-comparison tests.
