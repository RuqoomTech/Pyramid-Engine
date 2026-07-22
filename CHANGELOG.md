# Changelog

All notable changes to Pyramid Engine are documented here. The project is pre-alpha; version numbers describe development milestones rather than API-stability guarantees.

## [Unreleased]

### Incremental octree synchronization

- Added `Octree::Synchronize()` to incrementally insert new objects, remove stale objects, and relocate only objects whose world-space AABBs changed.
- Added `Octree::UpdateIfMoved()`, tracked-object inspection, duplicate-insertion protection, and floating-point tolerance for stable bounds.
- Replaced `SceneManager::UpdateSpatialPartition()` full-tree rebuilds with incremental synchronization after the initial scene build.
- Added per-update spatial insertion, removal, movement, and unchanged-object statistics.
- Hardened octree removal so duplicate legacy entries cannot survive an update.
- Added `Graphics.OctreeUpdates` coverage for movement across branches, bounds-only changes, additions, removals, duplicate snapshots, and stable-scene no-op updates.

### Camera frustum and scene visibility

- Corrected camera orientation conventions so OpenGL forward is local negative Z and view matrices use inverse camera rotation.
- Added robust `LookAt()` handling for zero-length and collinear-up inputs.
- Extracted and normalized six inward-facing world-space frustum planes from the view-projection matrix.
- Added public frustum-plane access plus accurate point, sphere, and AABB visibility tests.
- Added explicit local bounds to `RenderObject` and transformed all eight corners into world-space AABBs.
- Replaced center-point scene visibility with bounds-aware culling.
- Implemented octree node/object frustum rejection, safe storage for objects spanning child boundaries, hidden-object filtering, and camera-independent spatial rebuilds.
- Made scene-manager frustum enable/disable behavior consistent across octree and linear paths.
- Added `Graphics.CameraFrustum` coverage for perspective, orthographic, translated, rotated, scene, scene-manager, and octree classifications.

### Scene transform hierarchy

- Added hierarchy-wide world-transform invalidation for every local transform and parent change.
- Added cycle rejection, duplicate-child prevention, safe detach/reparent behavior, unmanaged-node guards, and cleanup for externally retained children when a parent is destroyed.
- Normalized local and composed world rotations before matrix use.
- Cached world rotation and effective basis scale alongside the world matrix.
- Added parent/children accessors plus point and direction conversion to world space.
- Added `Graphics.SceneTransforms` coverage for multi-generation TRS composition, cache invalidation, reparenting, detachment, cycle rejection, and destruction behavior.

### Image and texture loading

- Replaced JPEG test-pattern generation with real baseline and progressive JPEG decoding through libjpeg-turbo.
- Normalized JPEG output to tightly packed RGB pixels and added invalid-data, size-overflow, allocation, and scanline failure handling.
- Removed the unused custom JPEG entropy/IDCT/color-conversion pipeline and its misleading public headers/tests.
- Made file-backed OpenGL texture replacement transactional so failed reloads preserve the previous valid GPU object.
- Added explicit texture load state/error reporting, RGB/RGBA format validation, sRGB internal formats, complete mip-filter mapping, border-color parameters, and unpack-alignment restoration.
- Added validated baseline/progressive JPEG fixtures and `Graphics.TextureLoading` coverage for upload state, failed reload preservation, and data-size checks.
- Added libjpeg-turbo to MSYS2 bootstrap, CI, installed package dependencies, and external-consumer resolution.

### Framebuffer resize safety

- Made `OpenGLFramebuffer::Resize()` transactional so failed replacement creation preserves the last valid framebuffer and attachments.
- Added zero-sized extent guards, structural specification validation, duplicate attachment detection, and consistent multisample validation.
- Centralized framebuffer resource cleanup and fixed leaked depth/stencil attachment objects during invalidation.
- Applied each attachment's declared filtering and wrapping parameters during texture creation.
- Replaced the renderer's duplicate raw `RenderTarget` framebuffer implementation with the shared `OpenGLFramebuffer` lifecycle.
- Added `RenderTarget::Resize()`, `RenderPass::Resize()`, and `RenderSystem::Resize()` propagation.
- Added `Game::SetRenderSystem()` so registered window-sized render targets follow valid window resize events automatically.
- Updated deferred G-buffer resizing to preserve the existing buffer if replacement fails.
- Added framebuffer specification and zero-area resize coverage.

### Resize-safe rendering

- Propagated renderable window resize events to the default OpenGL viewport.
- Added `Camera::SetViewportSize()` for perspective and orthographic projection updates.
- Added `Game::SetActiveCamera()` so one non-owning camera follows the window client size automatically.
- Suspended rendering and presentation while the window is minimized or has a zero-sized client area.
- Registered both examples with the active-camera resize path.
- Added camera aspect, projection invalidation, orthographic resize, and zero-area tests.

### Window events

- Added a platform-neutral `WindowResizeEvent` with restored, minimized, and maximized states.
- Added replaceable resize callbacks to the `Window` base interface.
- Translated Win32 `WM_SIZE` messages into deduplicated engine resize events.
- Added the overridable `Game::onWindowResize()` hook and detached it safely during shutdown.
- Added focused callback, replacement, detach, state, and renderable-area tests.

### Rendering diagnostics

- Added a centralized OpenGL diagnostics module for draining and reporting all pending errors.
- Enabled synchronous driver debug callbacks in Debug builds when the current context supports them.
- Requested a Win32 OpenGL debug context in non-Release builds.
- Added readable source, type, severity, and error-name mapping plus focused unit coverage.
- Suppressed high-volume notification messages while preserving warnings and errors.

### Next priorities

- Windows runtime verification for Debug and Release.
- Texture-format and depth-target completion.
- Geometry-derived local bounds during mesh import.

## [0.6.0-pre-alpha] - 2026-07-21

### Build and packaging

- Synchronized project metadata at `0.6.0-pre-alpha`.
- Raised the supported CMake workflow to 3.23+.
- Added an explicit Windows-only platform guard.
- Removed stale `Game`, dependency, tool, and empty subsystem build entries.
- Replaced the Visual Studio presets with Ninja presets for MSYS2 UCRT64 GCC and Clang.
- Added GCC/Clang `-Wall -Wextra -Wpedantic` defaults.
- Added PowerShell helpers for installing and invoking the open-source MinGW toolchain.
- Added relocatable CMake package exports for `Pyramid::Engine` and GLAD.
- Added an external `find_package` consumer.
- Added MSYS2 UCRT64 CI for GCC and Clang Debug/Release builds, tests, install, and consumer validation.

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

Introduced custom TGA, BMP, PNG, zlib/DEFLATE, and JPEG helper components. PNG is operational for the tested subset; the original incomplete JPEG reconstruction path was later replaced by libjpeg-turbo.

### 0.3.9 logging milestone — 2025-06

Moved logging into Utils and added severity filtering, file rotation, structured fields, assertions, and thread synchronization.

### 0.3.8 graphics-resource milestone — 2025-05

Added buffer layouts, shaders, uniforms, textures, assertions, and expanded example rendering.

### 0.3.3 foundation milestone — 2025-01

Established the initial CMake project, Win32/OpenGL application loop, graphics device, math, and examples.
