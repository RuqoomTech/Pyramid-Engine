# Examples

The repository contains two Windows graphical applications. Both use GLSL 3.30 and require an OpenGL 3.3-or-newer core context.

## BasicGame

Location: `Examples/BasicGame`

Demonstrates:

- deriving from `Pyramid::Game`;
- base lifecycle initialization;
- stable shader-program compilation and reuse through `ShaderCache`;
- engine-owned indexed mesh creation with validated layout, topology, draw count, and local bounds;
- content-derived mesh-cache reuse: the cube and floor request identical geometry but perform one GPU upload;
- texture loading;
- frame update and rendering;
- automatic default-viewport updates and active-camera projection resizing;
- platform-neutral resize-event logging through `Game::onWindowResize()`;
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

- inline GLSL 3.30 shaders compiled through a stable `ShaderCache` asset ID;
- an engine-owned cube mesh with position, normal, texture-coordinate, and color attributes;
- caller-defined stable mesh and shader asset identifiers resolved through their graphics-device-bound caches;
- scene and material uniform buffers;
- perspective camera setup registered through `Game::SetActiveCamera()`;
- topology-aware indexed rendering and basic lighting.

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
- resize, maximize, minimize, and restore messages appear in the log;
- the cube keeps the correct proportions after resizing;
- minimizing suspends rendering until a renderable client area is restored;
- resizing does not crash;
- closing exits cleanly;
- the OpenGL debug/error log remains clean.

The project does not yet include automated pixel-comparison tests.
