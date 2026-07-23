# Development guide

## Workflow

1. Create a focused branch.
2. Configure a test preset.
3. Build and run CTest before changing behavior.
4. Make one coherent subsystem change.
5. Add or update tests, including linkage coverage for public APIs.
6. Run the graphical smoke test and visually inspect renderer changes.
7. Update existing documentation and the changelog.
8. Record exact validation commands in the pull request.

Recommended local validation:

```powershell
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5
```

## Conventions

- C++17 with extensions disabled.
- Four-space indentation and braces on new lines.
- `PascalCase` for types and most public methods.
- `camelCase` for locals and parameters.
- `m_` prefix for fields.
- RAII and smart pointers for ownership.
- `PYRAMID_LOG_*` and assertion macros for diagnostics.
- No required interface operation may silently default to a no-op.
- Platform-specific APIs belong behind platform implementation headers.

The primary toolchain enables GCC/Clang `-Wall -Wextra -Wpedantic`. Warnings are not errors by default because existing warnings still need cleanup. New code should not add warnings.

## Public API discipline

For every public method, choose one behavior:

1. implement it;
2. remove it before release;
3. fail explicitly with a documented capability limitation.

Do not leave declarations without definitions. Add the symbol to `Tests/PublicApiLinkage.cpp` when a factory or subsystem method is especially vulnerable to linker regressions.

## Tests

Utility tests live under `Engine/Utils/test` and are registered by `add_utils_test`. A test must:

- return non-zero on failure;
- avoid hidden skips that report success;
- use standards-valid embedded fixtures;
- clean up temporary files;
- print enough context to diagnose a failure.

`API.PublicApiLinkage` verifies selected exported symbols. `Platform.WindowResizeEvents` verifies the platform-neutral callback contract without requiring a Win32 window. `Graphics.CameraViewportResize` verifies perspective/orthographic resize behavior and zero-area rejection. `Graphics.CameraFrustum` verifies camera orientation, normalized plane extraction, point/sphere/AABB classification, transformed object bounds, and linear/octree scene visibility. `Graphics.OctreeUpdates` verifies that scene snapshots incrementally relocate, insert, and remove objects without rebuilding stable entries. `Graphics.OctreeQueries` verifies full-bound point/sphere/box/ray semantics, root-overflow handling, nearest-first ray ordering, and parity between spatial and linear paths. `Graphics.NearestQueries` verifies point-to-AABB distance, nearest/K-nearest ordering and limits, branch-prunable spatial results, and octree/linear parity. `Graphics.OctreeConfiguration` verifies atomic bounds/depth/capacity changes, object preservation, invalid-input rejection, constructor normalization, and root-overflow behavior. `Graphics.OctreeCompaction` verifies bottom-up branch collapse, movement/removal integration, synchronization compaction reporting, and health metrics. `Graphics.RenderObjectBounds` verifies automatic/manual transformed bounds, `Graphics.MeshResource` verifies validated resource construction, immutable metadata, indexed/non-indexed draw submission, topology handling, and invalid input rejection, and `Graphics.MeshCache` verifies deterministic identifiers, exact-content sharing across aliases, conflict rejection, upload counts, residency, collection, and eviction lifetime. `Graphics.ShaderCache` verifies deterministic source identifiers, compile-once reuse, stage validation, forwarding, identifier conflicts, transactional replacement, failed-recompile preservation, existing-content reuse, residency, and collection. `Graphics.TextureCache` verifies deterministic texture identifiers, exact memory/file deduplication, explicit color-space identity, alias conflicts, transactional file reload, failed-reload preservation, replacement reuse, residency estimates, and collection. `Graphics.MaterialResource` verifies deterministic material identity, order-independent hashing, shader/texture ownership, validation, fixed-state application, typed uniforms, and command-buffer ordering. `Graphics.MaterialCache` verifies exact-content deduplication, stable aliases, identifier conflicts, transactional replacement, failure preservation, resident replacement reuse, external-owner lifetime, residency statistics, collection, and eviction. `Graphics.FramebufferResize` verifies structural attachment rules, multisample consistency, and zero-area resize preservation without requiring a graphics context. `Graphics.TextureLoading` uses a fake OpenGL backend to verify file upload state and transactional reload behavior. `Graphics.SceneTransforms` verifies hierarchy composition and cache invalidation without a graphics context. JPEG tests must use standards-valid encoded fixtures rather than marker-only pseudo-images. Windows CI validates GCC and Clang in Debug and Release, plus installation and an external `find_package` consumer.

Renderer changes require manual visual validation until image-regression tests exist.

Mesh changes must preserve immutable layout/count/topology/bounds/identifier metadata, keep raw GPU buffers out of `RenderObject`, reject invalid index and vertex ranges before upload, and keep indexed/non-indexed command submission equivalent. Cache changes must deduplicate by exact content, preserve deterministic identifiers, reject one alias mapped to different resident content, retain strong ownership until explicit eviction/collection, and never invalidate external resource owners during cache removal or reload. Texture identity must include decoded pixels, sampler/mip state, and explicit color space; file reload must publish only after a complete replacement exists. Material identity must include referenced shader/texture content IDs, slots, typed uniform values, and fixed state while excluding debug names; dynamic per-draw values must stay outside the material resource. Material-cache replacement must validate or resolve a complete immutable replacement before publishing a stable alias, preserve old external owners, and reject replacement through content-derived identifiers.

Scene hierarchy changes must preserve four invariants: no cycles, no duplicate direct children, every parent/local edit invalidates the complete descendant subtree, and reparenting preserves local TRS. Add tests before changing those semantics.

Frustum planes must remain normalized and inward-facing in the order left, right, bottom, top, near, far. Scene visibility must test transformed object bounds rather than center points. An octree object may descend only when its complete AABB fits inside one child; spanning objects stay in the parent. Spatial synchronization must compare complete world AABBs, tolerate insignificant floating-point drift, and deduplicate scene snapshots. Configuration changes must validate first, rebuild into temporary ownership, preserve every tracked object, and swap live state only after successful construction. Compaction must run bottom-up, promote every surviving descendant before releasing children, never alter the tracked-object map, and remain a no-op when the structure is already minimal. Spatial queries must test complete world AABBs, keep root-retained objects queryable outside configured bounds, return unique results, and preserve identical semantics when the octree is disabled.

Debug builds request an OpenGL debug context and attach a synchronous driver callback when supported. Driver warnings and errors are routed through `PYRAMID_LOG_*`; notification-level messages are suppressed. Release builds do not enable the callback.

Window resize callbacks run synchronously on the game thread while `ProcessMessages()` dispatches native events. Before the virtual hook runs, `Game` updates the default viewport, synchronizes the camera registered through `SetActiveCamera()`, resizes the `RenderSystem` registered through `SetRenderSystem()`, and marks zero-sized/minimized surfaces non-renderable. Keep custom handlers lightweight, reject `!event.HasRenderableArea()`, resize only standalone targets there, and avoid retaining references to the callback argument.

## CMake and package changes

After changing targets, include directories, or installation:

```bash
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
ctest --preset test-gcc-release
cmake --install build/gcc-release-tests --prefix install
cmake -S Tests/Consumer -B build/consumer -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=g++ \
  -DCMAKE_PREFIX_PATH="$PWD/install"
cmake --build build/consumer --parallel
```

Keep build-tree paths out of installed target interfaces. Public headers should be installed as files, not exposed as absolute `INTERFACE_SOURCES`. Public static-library dependencies such as JPEG must be resolved in `PyramidEngineConfig.cmake.in` and installed by CI/bootstrap tooling.

## Documentation

Maintain this fixed set:

```text
README.md
docs/README.md
docs/BUILDING.md
docs/ARCHITECTURE.md
docs/API.md
docs/EXAMPLES.md
docs/DEVELOPMENT.md
docs/ROADMAP.md
CHANGELOG.md
```

Do not add separate blocker reports, status snapshots, duplicated setup guides, or speculative API references.

## Pull requests

Include:

- purpose and affected modules;
- public API or ownership changes;
- exact Debug/Release/test commands;
- screenshots or video for visual changes;
- known limitations and follow-up work;
- documentation and changelog updates.

## Release hygiene

Before tagging:

1. synchronize CMake version, status, tag, and changelog;
2. pass clean GCC and Clang Debug/Release CI;
3. run all registered tests;
4. manually run and inspect both examples;
5. install into an empty prefix and build the external consumer;
6. verify every documented public method is implemented or explicitly unsupported;
7. review the roadmap and remove completed P0 items.
