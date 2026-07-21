# Roadmap and known issues

This roadmap replaces separate implementation, blocker, and best-practices reports. It is based on the source snapshot audited on **July 21, 2026**. Priorities describe technical risk, not delivery dates.

## P0 — make the current engine trustworthy

### Build and release consistency

- Reconcile the CMake project version (`0.3.3`) with changelog entries through `0.6.0`.
- Guard Win32 sources and `opengl32` behind platform checks; fail early with a clear message on unsupported hosts.
- Remove or repair stale build files: `CMake/Dependencies.cmake`, `Game/CMakeLists.txt`, and the missing `Tools/AssetProcessor` path.
- Validate `cmake --install` from a clean Release build, including missing placeholder include directories.
- Add a Windows CI job for configure, build, CTest, and at least process-level example startup where a graphical runner is available.

### API completeness and linker safety

Implement or remove public declarations that currently have no definition, especially in `SceneManager`:

- `LoadScene` / `SaveScene` and serialization helpers;
- `GetObjectsInBox`;
- `UpdateVisibility`;
- `DrawDebugInfo`;
- `SceneUtils::CreateSpatialTestScene`;
- event callback registration/dispatch helpers;
- public texture convenience factories (`Create(width, height, ...)`, render/depth targets, and color textures);
- declared transparent, post-process, UI, debug, and render-pass factory classes.

Replace required no-op/default methods in `Window` and texture interfaces with explicit capability reporting or pure virtual operations.

### Rendering correctness

- Implement compute dispatch or remove `Dispatch` from the supported command set.
- Complete generic framebuffer binding in `OpenGLDevice::BindFramebuffer`.
- Remove fixed 1920×1080 sizing from `SetupDeferredPipeline`; use the active viewport/window and resize targets.
- Complete deferred shadow-map-array binding.
- Add state-transition tests for framebuffer, shader, texture, VAO, UBO, blending, depth, culling, and polygon modes.
- Verify multisampled render-target creation, resolve behavior, attachment ownership, and resizing.

### Image-loader correctness

- Complete the JPEG block-decoding TODO and remove generated test-pattern behavior from production paths.
- Register all maintained image tests with CTest or delete obsolete debug test files.
- Add corpus tests for valid, malformed, truncated, large, grayscale, alpha, interlaced/progressive, and unsupported files.
- Document exact supported subsets and reject unsupported files deterministically.
- Add fuzzing/sanitizer coverage for all parsers and decompressors.

## P1 — stabilize engine subsystems

### Scene management

- Implement scene transform propagation and dirty-state tracking.
- Connect scene/object counts to `SceneStats`.
- Define correct object bounds instead of deriving behavior from placeholder geometry where applicable.
- Implement or explicitly defer occlusion culling and LOD application.
- Add tests for octree insertion, removal, rebuild, ray/point/sphere/box/frustum queries, and moving objects.
- Separate persistence from runtime scene management when serialization is implemented.

### Resource and lifetime model

- Introduce stable resource handles or a registry with generation checks.
- Remove integer-ID and raw-pointer duplication from command APIs once one model is selected.
- Define ownership for render targets, attachment textures, and backend-native handles.
- Add shader/texture caching and deterministic teardown-order tests.
- Replace manual `ImageData` ownership with an RAII container while preserving an interoperable low-level view.

### Renderer quality

- Make frame statistics reflect executed draws, triangles, vertices, and GPU timings.
- Separate material data from pass-specific shader binding.
- Add resize-aware camera/projection and render-target updates.
- Validate forward, shadow, and deferred pipelines with render-image regression tests.
- Decide which OpenGL baseline is supported; align embedded and file-based GLSL versions with it.

### Testing and diagnostics

- Add unit tests for math, camera, scene, state cache, buffer layout, and command buffers.
- Enable compiler warnings at a strict but achievable level and treat new warnings as regressions.
- Add AddressSanitizer/UndefinedBehaviorSanitizer in a compatible toolchain job.
- Add structured GL error/debug callback handling in Debug builds.

## P2 — expand capabilities after stabilization

### Platform and backends

- Refactor platform selection so Win32/WGL is one implementation rather than a hard dependency.
- Add one additional window/context platform before claiming cross-platform support.
- Add another graphics backend only after the device/resource contracts are backend-neutral and tested.

### Engine systems

`Audio`, `Input`, and `Physics` currently contain only CMake placeholders. For each system, either remove it from the build until work begins or deliver a minimal vertical slice with tests and an example before documenting it as available.

Suggested order:

1. Input: keyboard/mouse events and action mapping, integrated with Win32.
2. Asset/resource management: paths, caching, lifetime, and reload behavior.
3. Physics integration: begin with a proven external library unless writing physics is a core research goal.
4. Audio integration: device lifecycle, buffers/sources, streaming, and spatialization.

### Tooling

- Asset conversion/validation pipeline.
- Shader compilation/reflection tooling.
- Frame capture and renderer debug UI.
- Scene/editor tooling only after serialization and resource identities are stable.

## Release readiness criteria

A first credible public pre-release should meet all of the following:

- version metadata, tag, and changelog agree;
- clean Windows Debug and Release builds pass in CI;
- all registered tests pass and image parsers have corpus coverage;
- both examples run without GL errors on the declared minimum driver;
- every documented public method is implemented or explicitly marked experimental;
- unsupported platform/backend choices fail clearly;
- install/export usage works from an external sample project;
- no P0 item remains open.
