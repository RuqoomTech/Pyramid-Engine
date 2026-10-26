# Roadmap and known issues

Priorities are based on technical risk. They are not delivery dates.

## Product direction locked July 25, 2026

Pyramid is a general-purpose engine whose first proving game will be a Ruqoom RTS. The authoritative runtime model is a stable-ID entity/component scene with hierarchical transforms. The first playable milestone remains Windows-only; Linux follows after it. A full editor is planned. Baa is the long-term scripting language and eventual engine implementation language, but the C++ engine remains the behavioral reference until the Baa toolchain/runtime can reproduce it. New engine/editor/gameplay subsystems should be owned by the Pyramid ecosystem. GLAD is the sole approved bundled third-party runtime library; the former JPEG middleware dependency has been removed.

## Stabilization completed through July 29, 2026

The current `0.6.0-pre-alpha` baseline includes:

- synchronized CMake version and pre-alpha status;
- explicit Windows-only configure guard;
- removal of stale build files and empty Input/Audio/Physics module placeholders;
- OpenGL 3.3/GLSL 3.30 minimum for the reference examples;
- strict required window operations with Win32 implementations;
- relocatable install/export package and external-consumer test;
- Windows Debug/Release CI for build, CTest, install, and package consumption;
- 57 registered Windows tests, including standalone foundation, math, input, image, model, font, international text, UI, executable-relative runtime paths, exact installed-system-font selection/coverage/cache validation, pre-action UI input consumption, command-buffer statistics, OpenGL UI batching and clipping, platform input, action mapping, reusable camera controllers, game-side RTS interaction, engine-owned mesh resources, transactional OBJ-to-mesh/texture/material publication, immutable geometry bounds, real PNG/JPEG decoding, transactional texture loading, OpenGL diagnostics, window events, camera resize/frustum behavior, framebuffer resize, scene-hierarchy transforms, spatial queries, resource caches, and scene serialization;
- extracted primitive types/logging, math, physical/action input, image codecs, CPU model importing, TrueType/font assets, text, and UI into independently installable `Pyramid::Foundation`, `Pyramid::Math`, `Pyramid::Input`, `Pyramid::Image`, `Pyramid::Model`, `Pyramid::Font`, `Pyramid::Text`, and `Pyramid::UI` packages;
- completed the previously stubbed `Mat4` determinant and inverse operations with pivoted elimination;
- corrected standards-invalid PNG, zlib, and JPEG test fixtures;
- public texture convenience definitions and explicit depth-target failure;
- definitions for scene events, box queries, visibility statistics, spatial test scenes, and octree operations;
- removal of public render-pass classes that had no implementations;
- whole-object non-Windows static linkage validation with no unresolved symbols;
- a missing `<cstring>` dependency fixed in the image loader;
- Debug-context negotiation and OpenGL driver callback diagnostics;
- platform-neutral resize events delivered from Win32 `WM_SIZE` through `Game::onWindowResize()`;
- real Win32 keyboard/mouse polling with per-frame transitions, pointer/wheel deltas, explicit pointer-sample validity, mouse capture, and focus-safe release;
- game-side RTS edge scrolling, selectable-filtered ray picking, stable selection, and ground-plane command requests without engine-level RTS concepts;
- automatic default-viewport updates, per-pass main-surface restoration, final UI surface ownership, active-camera projection synchronization, and minimized-window render suspension;
- transactional framebuffer recreation, unified render-target ownership, and render-system resize propagation;
- an authoritative stable-ID entity/component scene with cycle-safe hierarchy operations, recursive world-transform invalidation, mesh-renderer/light components, and generated renderer proxies;
- normalized camera-frustum extraction, bounds-aware scene visibility, octree frustum queries, incremental moving-object synchronization, and exact bounds-aware point/sphere/box/ray queries.

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
- [x] Remove fixed deferred target dimensions and propagate resize events into framebuffer/render-pass targets.
- Complete deferred shadow-map-array binding.
- Verify multisampled targets and resolve behavior across supported drivers; attachment ownership and resize preservation are now centralized.
- [x] Add OpenGL debug-callback handling.
- [ ] Add comprehensive render-state transition tests; UI viewport/scissor/state restoration now has focused coverage.

### Texture and image correctness

- [x] Bind immutable cached textures through the OpenGL device without rejecting their wrappers; verify material/command-buffer paths and visible imported textures.
- [x] Synchronize cached texture bindings for upload, reload, mutation, framebuffer attachments, deletion, and reused native names, with focused regressions and native-driver binding checks.
- [x] Implement all uncompressed texture formats with explicit CPU upload layouts, typed updates, immutable-cache validation, and stable enum codes; compressed BC codes remain explicitly unsupported.
- [x] Implement depth/depth-stencil texture creation through the texture interface.
- Validate the expanded formats with native-driver sampling/readback and example visual checks; visual checks are deferred at the user's request. Fake-GL, cache, public-linkage, and installed-package checks cover the implemented contracts.
- Revisit BC compression when representative assets justify it, with capability checks and a defined block-upload contract before advertising support.
- [x] Apply sRGB intent, border color, mip filters, and safe RGB unpack alignment.
- [x] Extract `Pyramid::Image` and replace libjpeg-turbo with an owned bounded baseline/progressive Huffman JPEG decoder supporting grayscale, common chroma subsampling, and restart markers.
- Sampler correctness execution plan:
  1. [x] Commit verified texture binding/format work (`d52fe2e`); GCC Debug/Release 57/57 and installed consumer already passed.
  2. [x] Implement mutable filter, wrap, border-color, and anisotropy setters in the existing OpenGL adapter. Validate before native calls; retain legacy known aliases; preserve binding-cache coherence and attempt sampler restoration after driver errors.
  3. [x] Apply requested anisotropy consistently during direct/cache creation with current-context core/ARB/EXT support checks, supported maximum clamping, and explicit 1x fallback. Keep cache identities based on requests and expose cached native textures read-only.
  4. [x] Add fake-GL and cache regressions, public-symbol linkage checks, then run GCC Debug/Release and installed-package validation. Commit after checks pass. Visual/native-driver acceptance stays deferred.
- Expand parser fuzzing, very-large allocation-limit fixtures, and PNG interlace coverage; JPEG now has malformed/truncated, progressive-color, grayscale, sampling, odd-size, and restart-marker corpus tests.
- Add parser fuzzing and sanitizer coverage.

### Scene correctness

- [x] Prevent retained entity facades from reviving after scene clearing or explicit reuse of a destroyed entity ID; reject stale facade mutations.
- [x] Implement hierarchy-wide transform dirty propagation and cycle-safe reparenting.
- [x] Extract normalized camera frustum planes and use transformed object bounds for scene/octree visibility.
- Replace placeholder occlusion culling with a supported technique or remove the setting.
- [x] Introduce an engine-owned mesh resource with validated vertex/index ownership, layout, topology, draw count, and immutable local bounds.
- [x] Add stable mesh identifiers and a graphics-device-bound resource cache that shares exact geometry across aliases; manual `RenderObject` bounds overrides and the unit-cube fallback remain supported.
- [x] Integrate dependency-free OBJ primitives transactionally with the mesh cache while preserving material-slot metadata.
- [x] Add focused octree synchronization tests for moving, inserted, removed, and unchanged objects.
- [x] Add focused octree tests for point, ray, exact box/sphere intersections, root-overflow objects, and linear/octree parity.
- [x] Use full object bounds and branch pruning for nearest-object and K-nearest queries.
- [x] Add focused octree tests for transactional bounds, depth, and capacity changes.
- [x] Add automatic octree branch compaction and structural health metrics.
- [x] Replace flat render-object/`SceneNode` authoring with stable entities, mandatory transforms, optional mesh-renderer/light components, and generated renderer proxies.
- [x] Add version-2 entity/component serialization using resource-manifest keys, hierarchy validation, transactional parsing, and missing/stale diagnostics.
- Add cameras, environment settings, RTS gameplay components, and editor metadata to later scene-format versions.

## P1 — stable OpenGL core

Target outcome: a trustworthy rendering SDK rather than a larger feature list.

- [x] Stable resource ownership and teardown order through a game-owned `ResourceRegistry`; mesh, shader-program, immutable sampled-texture, and material resources provide deterministic identity, explicit ownership, dependency-safe collection, and ordered command application.
- Resize-safe camera and render targets.
- [x] Shader-program caching with stable source identifiers and transactional replacement.
- [x] Texture caching with stable identifiers, explicit color-space identity, transactional file reload, eviction, and residency statistics.
- Accurate frame statistics and GPU timings.
- Automated render-image regression tests.
- Warning cleanup followed by warnings-as-errors in CI.
- AddressSanitizer/UndefinedBehaviorSanitizer coverage in a compatible toolchain.
- RAII image data container while retaining a low-level view.

## P2 — scene and asset foundation

- [x] Stable typed resource handles with persistent generation checks, stale-reference rejection, registry resolution, and handle-backed scene renderables.
- [x] Versioned resource manifests with deterministic typed-handle serialization, transactional validation, and missing/stale diagnostics.
- [x] Add an engine-owned immutable material resource for shader/texture references, typed uniforms, and fixed render state.
- [x] Add exact-content material caching with stable aliases, transactional replacement, eviction, and residency statistics.
- [x] Add owned bounded OBJ/MTL parsing and transactional imported-primitive publication through the mesh cache.
- [x] Convert imported MTL metadata and diffuse image paths into immutable texture/material resources through a configurable shader/material profile.
- Shader preprocessing, dependency tracking, and reload.
- Scene serialization with versioning and validation, built on resource manifests.
- Asset packaging and path abstraction independent of the source checkout.
- [x] Runtime debug UI and frame/resource/input inspection foundation.
- [x] Add owned UTF-8 decoding, wrapping/alignment, collapsing sections, clipped scrolling, bounded log history, and a runtime diagnostics console.

## P3 — first playable RTS runtime

Target outcome: one playable two-player Windows LAN/direct-IP Ruqoom RTS vertical slice that validates the engine rather than adding more infrastructure in isolation. Internet services, matchmaking, host migration, and seamless reconnect follow a demonstrated need.

1. [x] real Win32 keyboard/mouse polling and focus-safe per-frame state;
2. [x] engine-generic configurable action mapping with prioritized contexts, control consumption, chords, runtime rebinding, and an RTS camera reference profile;
3. [x] reusable, action-driven free-fly, target-orbit, and optional strategy camera controllers with validated settings and home-pose reset;
4. [x] RTS reference-game input features built above the generic engine: edge scrolling, selection, and command input;
5. [x] owned OBJ/MTL model import feeding existing mesh, diffuse-texture, and immutable material resources, with scene-hierarchy instantiation still pending;
6. RTS components such as selectable, team/owner, movement target, health, and basic unit state;
7. fixed-step simulation and a command queue, with per-frame input edges consumed once and explicit overload behavior;
8. two-player LAN/direct-IP sessions, a bounded native transport, protocol/content compatibility checks, host-authoritative gameplay, validated ownership, snapshot presentation, and explicit disconnect behavior;
9. terrain/large-map rendering and scalable visibility/spatial updates;
10. [x] debug statistics and inspection UI as the first editor foundation;
11. versioned scene extensions for camera/environment, game-owned persistence for gameplay components, and editor-owned metadata;
12. measured 1080p/60 FPS acceptance with 500 total units on declared reference hardware, a separate 2,000-unit stress workload, and a verified Windows `0.7.0-pre-alpha` vertical slice, then Linux platform work. These are proposed performance targets, not demonstrated results.

A full editor follows the validated runtime model. Baa integration starts as gameplay scripting only after its runtime, ABI/FFI, debugger, and hot-reload semantics are defined; engine reimplementation is a later compatibility project, not a blind source translation.

### Baa scripting admission gate

Pyramid Engine is registered as an Eco runtime consumer, not as a dependency of
the hosted language toolchain. C++ and CMake remain the behavioral/build
references until the following consumer-side gate passes:

- [ ] Baa freezes a narrow hosted ABI/FFI for a gameplay module and an approved
      engine-facing API surface.
- [ ] Allocation, strings, errors, threads, module loading, debugger hooks, and
      hot-reload state migration have explicit ownership and failure behavior.
- [ ] One gameplay-only Baa module builds and loads beside the current C++
      example without moving graphics, platform, or engine-core ownership.
- [ ] Reload, rollback, crash containment, version mismatch, and Windows
      packaging are deterministic and tested.
- [ ] Qalam and Baa-LSP edit/debug the module without becoming the owner of
      Pyramid scene or asset-authoring UI.
- [ ] Takween orchestration waits until it can describe the mixed native+Baa
      graph without prematurely replacing the validated CMake build.

Arabic behavior shared with Qalam and ArbSh begins as the data-only
`eco-arabic-text-corpus-v1` test corpus. Pyramid's renderer, fonts, shaping
subset, and visual acceptance remain independently owned and validated.

### UI and editor continuation

- [x] Runtime TrueType import, CPU rasterization, and owned versioned `.pfont` asset format.
- [x] Strict UTF-8 decoding, malformed-sequence diagnostics, tab expansion, word/character wrapping, and horizontal alignment for the embedded debug atlas.
- [x] Owned extended-grapheme boundaries, common Latin/Arabic/numeric bidirectional visual runs, core Arabic contextual presentation-form shaping, ordered fallback font families, international word/CJK break opportunities, and cluster-aware UI hit testing/caret/selection.
- [x] Readable owned Arabic reference outlines, explicit joining-edge geometry, deterministic 64-pixel SDF reference atlases, derivative-smoothed UI rendering, semantic typography roles, content-addressed runtime font caching, and exact script-specific Win32 installed-font sources with coverage/capacity gates processed entirely by Pyramid-owned code.
- Complete Unicode bidi controls/isolates, full UAX #14 line breaking, OpenType GSUB/GPOS, general complex-script shaping, advanced Arabic ligatures/mark positioning, and Win32 IME pre-edit/candidate presentation. Committed Unicode editing and clipboard integration are complete; IME remains the next isolated text-input step.
- [x] Retained opaque/transparent/modal screen routing, deferred lifecycle operations, responsive anchors/docking, safe events, transition state, nine-slice geometry, and reference main-menu/HUD/pause composition.
- Style classes, visual transition application, controller navigation, and reusable list/tree widgets.
- Editor widgets including tree views, property grids, splitters, menus, dialogs, docking, and scene/resource inspectors.

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
