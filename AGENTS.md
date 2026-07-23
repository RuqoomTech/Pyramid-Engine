# Repository guidelines

## Scope

- `Engine/` builds the C++17 `PyramidEngine` library.
- Active modules are Core, Graphics, Math, Platform, and Utils.
- `Examples/BasicGame` and `Examples/BasicRendering` are the graphical references.
- `Tests/PublicApiLinkage.cpp` protects selected public symbols; `Tests/WindowResizeEventTests.cpp` protects resize callback semantics; `Tests/CameraViewportTests.cpp` protects projection resizing; `Tests/FramebufferResizeTests.cpp` protects framebuffer specification and zero-area resize behavior; `Tests/TextureLoadingTests.cpp` protects transactional file loading and OpenGL upload state; `Tests/SceneTransformTests.cpp` protects hierarchy composition, invalidation, reparenting, cycle rejection, and parent-destruction behavior; `Tests/CameraFrustumTests.cpp` protects camera orientation, frustum extraction, transformed bounds, and linear/octree scene visibility; `Tests/OctreeUpdateTests.cpp` protects incremental movement, insertion, removal, and stable-scene synchronization; `Tests/OctreeQueryTests.cpp` protects bounds-accurate point, sphere, box, and nearest-first ray queries plus linear/octree parity; `Tests/NearestQueryTests.cpp` protects bounds-distance nearest/K-nearest ordering, limits, root-overflow behavior, and octree/linear parity. `Tests/OctreeConfigurationTests.cpp` protects atomic bounds/depth/capacity changes, tracked-object preservation, invalid configuration rejection, and constructor normalization. `Tests/OctreeCompactionTests.cpp` protects automatic branch collapse after removals and movement, explicit no-op compaction, synchronization compaction statistics, object preservation, and health-metric consistency. `Tests/RenderObjectBoundsTests.cpp` protects mesh-derived/manual transformed bounds. `Tests/MeshResourceTests.cpp` protects mesh validation, immutable metadata, topology-aware indexed/non-indexed submission, and `RenderObject` integration. `Tests/MeshCacheTests.cpp` protects deterministic identifiers, exact-content deduplication, alias conflicts, upload counts, strong residency, unused collection, explicit eviction, and external-owner lifetime. `Tests/ShaderCacheTests.cpp` protects deterministic source identifiers, compile-once reuse, stage validation, immutable forwarding, alias conflicts, transactional recompilation, failure preservation, replacement reuse, and cache lifetime.
- `Tests/Consumer` validates the installed CMake package.
- `vendor/glad` is a bundled public dependency. libjpeg-turbo is an external open-source dependency resolved through CMake `FindJPEG`.
- The supported Windows toolchain is MSYS2 UCRT64 with MinGW-w64 GCC; Clang is also validated. Visual Studio is not required.
- Input, audio, physics, editor, scripting, DirectX, Vulkan, Linux, and macOS are not supported.

## Build and test

```powershell
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5
```

Release validation:

```powershell
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
ctest --preset test-gcc-release
```

## Style

- C++17, four spaces, braces on new lines.
- Types/public methods use `PascalCase`; locals/parameters use `camelCase`; fields use `m_`.
- Prefer RAII, explicit ownership, and `PYRAMID_LOG_*` diagnostics. Geometry passed to scenes must use `Mesh`; do not reintroduce raw vertex-array fields on `RenderObject`. Shared reusable geometry should be acquired through a graphics-device-bound `MeshCache`; do not create parallel uploads for byte-identical mesh specifications. Shared shader programs should be acquired through `ShaderCache`; do not mutate cached `ShaderProgram` instances directly or compile identical stage source repeatedly.
- Do not add required interface methods with silent no-op defaults.
- Do not expose source-tree absolute paths through installed target interfaces.

## Public APIs

Every public declaration must be implemented, removed, or documented as an explicit failure. Add linkage-sensitive symbols to `Tests/PublicApiLinkage.cpp`.

Do not describe placeholder algorithms as complete. Frustum culling is implemented; occlusion culling is not. Current examples and engine shaders target GLSL 3.30; the runtime requires OpenGL 3.3 core or newer.

## Tests

Tests must fail visibly, use valid fixtures, avoid false-success skips, clean temporary files, and print actionable context. Renderer changes require visual inspection because process smoke testing is not pixel validation.

## Documentation

Update the maintained compact set instead of adding overlapping guides or status files. Planned work belongs in `docs/ROADMAP.md`; historical changes belong in `CHANGELOG.md`.

## Pull requests

Include affected modules, ownership/API decisions, exact validation commands, visible rendering evidence, known limitations, and documentation/changelog updates.
