# Changelog

All notable changes to Pyramid Engine are documented here. The project is pre-alpha; version numbers describe development milestones rather than API-stability guarantees.

## [Unreleased]

### Next priorities

- Windows runtime verification for Debug and Release.
- Resize-safe deferred rendering.
- Real JPEG block decoding.
- Texture-format and depth-target completion.
- Scene transform and culling correctness.

## [0.6.0-pre-alpha] - 2026-07-21

### Build and packaging

- Synchronized project metadata at `0.6.0-pre-alpha`.
- Raised the supported CMake workflow to 3.23+.
- Added an explicit Windows-only platform guard.
- Removed stale `Game`, dependency, tool, and empty subsystem build entries.
- Added MSVC `/W4` and `/permissive-` defaults.
- Added relocatable CMake package exports for `Pyramid::Engine` and GLAD.
- Added an external `find_package` consumer.
- Added Windows Debug/Release CI for build, tests, install, and consumer validation.

### API reliability

- Converted optional window no-ops into required operations and implemented them for Win32.
- Added definitions for texture size/render-target/color factories.
- Made unsupported depth texture creation fail explicitly.
- Added scene persistence failure paths instead of unresolved symbols.
- Implemented scene box queries, visibility statistics, events, debug statistics, and spatial test-scene creation.
- Completed declared octree operations and spatial helper definitions.
- Removed public transparent, post-process, UI, debug, and factory pass declarations that had no implementations.
- Added `API.PublicApiLinkage` to catch selected missing definitions.

### Rendering and examples

- Standardized bundled examples on GLSL 3.30.
- Required an OpenGL 3.3-or-newer core context and removed legacy-context fallback.
- Added Win32 size tracking and guarded swap-interval use.

### Tests and correctness

- Registered `TestPNGLoader` and `TestJPEGParser` with CTest.
- Corrected invalid PNG CRC/zlib fixture data.
- Corrected the zlib Adler-32 fixture.
- Corrected the JPEG DHT segment length fixture.
- Fixed floating-point absolute-value usage in the IDCT test.
- Fixed a missing `<cstring>` include in image loading.
- Verified 11 maintained tests with zero failures in the non-graphical audit environment.
- Verified all non-Windows engine objects link together under whole-archive linkage.

### Documentation

- Updated all maintained documentation for the new version, OpenGL minimum, test set, package workflow, removed placeholders, and remaining limitations.

## Historical milestones

### 0.6 scene-management milestone — 2025-07

Introduced scene management, octree structures, AABB helpers, spatial-query APIs, and scene statistics. Several algorithms were initially placeholders and remain tracked in the roadmap.

### 0.4 image-processing milestone — 2025-07

Introduced custom TGA, BMP, PNG, zlib/DEFLATE, and JPEG helper components. PNG is operational for the tested subset; JPEG image reconstruction is still incomplete.

### 0.3.9 logging milestone — 2025-06

Moved logging into Utils and added severity filtering, file rotation, structured fields, assertions, and thread synchronization.

### 0.3.8 graphics-resource milestone — 2025-05

Added buffer layouts, shaders, uniforms, textures, assertions, and expanded example rendering.

### 0.3.3 foundation milestone — 2025-01

Established the initial CMake project, Win32/OpenGL application loop, graphics device, math, and examples.
