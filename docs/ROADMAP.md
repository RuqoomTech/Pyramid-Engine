# Roadmap and known issues

Priorities are based on technical risk. They are not delivery dates.

## Stabilization completed on July 21, 2026

The current `0.6.0-pre-alpha` baseline includes:

- synchronized CMake version and pre-alpha status;
- explicit Windows-only configure guard;
- removal of stale build files and empty Input/Audio/Physics module placeholders;
- OpenGL 3.3/GLSL 3.30 minimum for the reference examples;
- strict required window operations with Win32 implementations;
- relocatable install/export package and external-consumer test;
- Windows Debug/Release CI for build, CTest, install, and package consumption;
- 14 registered tests, including PNG/JPEG parsers, OpenGL diagnostics, window events, and camera-resize coverage;
- corrected standards-invalid PNG, zlib, and JPEG test fixtures;
- public texture convenience definitions and explicit depth-target failure;
- definitions for scene events, box queries, visibility statistics, spatial test scenes, and octree operations;
- removal of public render-pass classes that had no implementations;
- whole-object non-Windows static linkage validation with no unresolved symbols;
- a missing `<cstring>` dependency fixed in the image loader;
- Debug-context negotiation and OpenGL driver callback diagnostics;
- platform-neutral resize events delivered from Win32 `WM_SIZE` through `Game::onWindowResize()`;
- automatic default-viewport updates, active-camera projection synchronization, and minimized-window render suspension.

## P0 — verify and finish the current vertical slice

### Windows runtime verification

- Run clean Debug and Release CI on the actual repository host.
- Launch both examples on at least one supported GPU/driver.
- Verify resize callback delivery, minimize/restore, visibility changes, close handling, and shutdown on Windows hardware.
- Capture OpenGL errors and screenshots for the reference rendering path.
- Tag the first verified pre-release only after these checks pass.

### Rendering correctness

- Implement compute dispatch or remove it from the command model.
- Complete backend-neutral framebuffer binding.
- [x] Add platform-neutral window resize events.
- [x] Propagate renderable resize events into the default viewport and active camera.
- Remove fixed deferred target dimensions and propagate resize events into framebuffer/render-pass targets.
- Complete deferred shadow-map-array binding.
- Verify multisampled targets, resolve behavior, and attachment ownership.
- [x] Add OpenGL debug-callback handling.
- [ ] Add render-state transition tests.

### Texture and image correctness

- Map all advertised texture formats or remove unsupported enum values.
- Implement depth texture creation through the texture interface.
- Apply sRGB intent, border color, anisotropy, mip filters, and `FlipY` consistently.
- Replace JPEG test-pattern generation with real baseline block decoding.
- Add malformed/truncated/large/progressive/interlaced image corpus tests.
- Add parser fuzzing and sanitizer coverage.

### Scene correctness

- Implement hierarchy-wide transform dirty propagation.
- Extract real camera frustum planes.
- Replace placeholder occlusion culling with a supported technique or remove the setting.
- Derive spatial bounds from geometry instead of approximate position/scale boxes.
- Add focused octree tests for moving objects, rays, boxes, frusta, and configuration changes.
- Implement scene persistence only after a stable resource identity model exists.

## P1 — stable OpenGL core

Target outcome: a trustworthy rendering SDK rather than a larger feature list.

- Stable resource ownership and teardown order.
- Resize-safe camera and render targets.
- Shader and texture caching.
- Accurate frame statistics and GPU timings.
- Automated render-image regression tests.
- Warning cleanup followed by warnings-as-errors in CI.
- AddressSanitizer/UndefinedBehaviorSanitizer coverage in a compatible toolchain.
- RAII image data container while retaining a low-level view.

## P2 — scene and asset foundation

- Stable resource handles with generation checks.
- Mesh/material asset import and caching.
- Shader preprocessing, dependency tracking, and reload.
- Scene serialization with versioning and validation.
- Asset packaging and path abstraction independent of the source checkout.
- Debug UI and frame inspection tools.

## P3 — additional engine systems

Add systems only as complete vertical slices with tests and an example:

1. input and action mapping integrated with Win32;
2. audio device, buffers/sources, streaming, and spatialization;
3. physics integration using a proven library unless physics research is a project goal;
4. a second platform implementation;
5. another graphics backend after the contracts are backend-neutral.

## Pre-release exit criteria

A credible tagged pre-release requires:

- version, status, tag, and changelog agreement;
- green Windows Debug and Release CI;
- all registered tests passing;
- both examples visually verified on the declared OpenGL minimum;
- clean install and external-consumer build;
- no unresolved public symbol;
- explicit behavior for every unsupported capability;
- no remaining P0 issue that can corrupt data, crash normal usage, or misrepresent support.
