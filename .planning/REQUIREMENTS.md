# Requirements: Pyramid Engine Foundation

**Defined:** 2026-05-14
**Core Value:** Pyramid Engine must become a trustworthy foundation where examples, tests, docs, and public APIs accurately reflect what the engine can do and where future renderer/platform backends can be added without architectural guesswork.

## v1 Requirements

Requirements for the foundation milestone. Each maps to exactly one roadmap phase.

### Decisions and Scope

- [ ] **DEC-01**: Project has ADR-style decision records for module boundaries, dependency direction, platform ownership, renderer/backend seams, CMake structure, test policy, and docs policy.
- [ ] **DEC-02**: Project has an explicit foundation-only scope guard that documents non-goals and rejects unrelated feature expansion during this milestone.

### Build and Testing

- [ ] **BLD-01**: Build system truth is restored so CMake minimum version, presets, options, targets, examples, tests, install behavior, and documentation agree.
- [ ] **BLD-02**: GoogleTest is integrated through CTest without breaking existing executable tests.
- [ ] **BLD-03**: Every implementation phase has a repeatable validation gate covering configure, build, CTest/GoogleTest, example build, smoke validation where applicable, and docs/build consistency.
- [ ] **BLD-04**: At least one registered non-Utils test suite exists for foundation-critical code outside current image utility coverage.

### Public API Honesty

- [ ] **API-01**: Public engine APIs are audited so declarations that fail to link, overpromise missing behavior, or expose placeholder systems are implemented, hidden, removed, or clearly marked experimental.
- [ ] **API-02**: Render pass public APIs match implemented behavior; unfinished transparent, post-process, UI, debug, or factory APIs do not appear as supported features unless implemented.

### Renderer and Backend Seams

- [ ] **REN-01**: High-level renderer code depends on graphics interfaces and backend-neutral descriptions rather than OpenGL concrete types or native handles.
- [ ] **REN-02**: Framebuffer binding and render-target operations route through `IGraphicsDevice`/`IFramebuffer` instead of bypassing the graphics device abstraction.
- [ ] **REN-03**: Render command recording and execution have an explicit resource lifetime contract using handles, weak validation, or an equivalent validated registry with clear failure reporting.
- [ ] **REN-04**: OpenGL-specific headers, GLAD dependencies, state management, and backend implementation details are contained in private backend/platform code.
- [ ] **REN-05**: A Direct3D 9 prototype seam is defined and implemented only far enough to validate backend selection, device creation, clear/present behavior, and unsupported-feature reporting.

### Platform Runtime

- [ ] **PLAT-01**: Core runtime no longer hardcodes Win32/WGL startup decisions; platform/window/context/event ownership is routed through explicit interfaces, factories, or equivalent seams.
- [ ] **PLAT-02**: Platform-specific sources, graphics backend links, and unsupported-platform behavior are handled conditionally in CMake.

### Assets

- [ ] **AST-01**: Image loading ownership is RAII-based or equivalently safe so normal callers do not manually free raw image buffers.
- [ ] **AST-02**: Trusted-input asset guardrails exist for image dimensions, checked size calculations, truncated/corrupt fixtures, and clear failure behavior.
- [ ] **AST-03**: JPEG support is honest: baseline JPEG decoding is genuinely implemented and tested, or support is disabled/marked unsupported until complete.

### Examples and Smoke Validation

- [ ] **EX-01**: BasicGame and BasicRendering build and run using clean public, backend-neutral engine APIs rather than backend/private implementation details.
- [ ] **EX-02**: Smoke validation scripts, target names, documented commands, and CMake output paths agree.

### Documentation and Contributor Workflow

- [ ] **DOC-01**: README, build guide, API docs, critical-issues docs, and example docs accurately describe implemented, experimental, planned, and out-of-scope capabilities.
- [ ] **DOC-02**: Contributor documentation explains module layout, CMake registration, test expectations, backend/platform rules, validation gates, and how to add foundation-safe changes.

## v2 Requirements

Deferred to future releases. Tracked but not in the current roadmap.

### Renderer and Backend Expansion

- **REN-V2-01**: Direct3D 9 backend reaches practical renderer parity with the OpenGL path.
- **REN-V2-02**: Vulkan, Direct3D 11/12, Metal, or another modern backend is evaluated after the DX9 prototype seam proves the architecture.
- **REN-V2-03**: Headless or software-assisted rendering tests validate renderer behavior in CI without a desktop GPU dependency.

### Platform Expansion

- **PLAT-V2-01**: SDL3, GLFW, or a custom platform adapter provides a non-Windows runtime implementation after platform interfaces are stable.
- **PLAT-V2-02**: CI includes non-Windows configure/build jobs after platform-conditional CMake is reliable.

### Quality Expansion

- **QUAL-V2-01**: Sanitizer/static-analysis presets are added after build and test hygiene stabilizes.
- **QUAL-V2-02**: Full scalar/SIMD equivalence coverage is expanded beyond the minimum foundation tests.
- **QUAL-V2-03**: Fixture asset directories and golden samples are added where useful for broader regression testing.

### Engine Systems

- **SYS-V2-01**: Audio, input, physics, scene, and renderer modules gain broader real implementations beyond minimal viable basics.
- **SYS-V2-02**: Advanced scene culling, occlusion, UI rendering, post-processing, and debug rendering are added after public API honesty and backend seams are complete.

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Full Direct3D 9 renderer parity | The milestone uses DX9 only to prove the backend seam and legacy support direction. |
| Vulkan, Metal, Direct3D 11, or Direct3D 12 backend implementation | Additional backends would expand scope before the renderer abstraction is proven. |
| Full hostile-input asset parser hardening or fuzzing campaign | Assets are treated as trusted development inputs with guardrails in this milestone. |
| Building a real game or substantial game content | Examples validate engine APIs; a real game would distract from foundation quality. |
| Broad audio/input/physics implementation | Stubbed modules should become honest or minimally viable only when needed for foundation validation. |
| Preserving current internal API compatibility | Breaking changes are acceptable when they improve architecture, tests, or truthfulness. |
| Advanced renderer features before seam cleanup | Feature-rich rendering is deferred until public APIs, backend seams, and validation gates are trustworthy. |
| Package-manager migration as a primary goal | Dependency tooling should not become a side project unless required by selected architecture decisions. |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| DEC-01 | TBD | Pending |
| DEC-02 | TBD | Pending |
| BLD-01 | TBD | Pending |
| BLD-02 | TBD | Pending |
| BLD-03 | TBD | Pending |
| BLD-04 | TBD | Pending |
| API-01 | TBD | Pending |
| API-02 | TBD | Pending |
| REN-01 | TBD | Pending |
| REN-02 | TBD | Pending |
| REN-03 | TBD | Pending |
| REN-04 | TBD | Pending |
| REN-05 | TBD | Pending |
| PLAT-01 | TBD | Pending |
| PLAT-02 | TBD | Pending |
| AST-01 | TBD | Pending |
| AST-02 | TBD | Pending |
| AST-03 | TBD | Pending |
| EX-01 | TBD | Pending |
| EX-02 | TBD | Pending |
| DOC-01 | TBD | Pending |
| DOC-02 | TBD | Pending |

**Coverage:**
- v1 requirements: 22 total
- Mapped to phases: 0
- Unmapped: 22 pending roadmap

---
*Requirements defined: 2026-05-14*
*Last updated: 2026-05-14 after initial definition*
