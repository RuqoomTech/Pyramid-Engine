# Domain Pitfalls: Pyramid Engine Foundation Refactor

**Domain:** Brownfield C++17 game engine foundation refactor  
**Researched:** 2026-05-14  
**Scope rule:** Foundation-only. Every fix must improve architecture honesty, build/test reliability, public API truth, backend/platform seams, resource lifetime safety, asset guardrails, or documentation alignment. Defer gameplay systems, renderer feature parity, production asset security, and broad subsystem expansion.

## Critical Pitfalls

Mistakes that can cause rewrites, unsafe extension points, or a milestone that never finishes.

### Pitfall 1: Turning a Foundation Refactor into a Feature Rewrite

**What goes wrong:** The milestone expands from boundaries and reliability into full renderer parity, a complete DX9 backend, real game content, audio/input/physics implementations, advanced culling, tooling, or production-grade asset hardening.  
**Why it happens:** Brownfield engines expose many tempting gaps at once, and Pyramid already has placeholder modules and public claims that invite "while we are here" fixes.  
**Consequences:** Architecture decisions are delayed, validation gates remain weak, and new features land on top of dishonest APIs and leaky seams.

**Warning signs:**
- Requirements mention new user-facing gameplay/rendering capabilities rather than seam cleanup or API honesty.
- A phase includes full Direct3D 9 parity instead of a prototype seam.
- Tasks add source files under `Engine/Audio`, `Engine/Input`, `Engine/Physics`, or separate `Engine/Renderer`/`Engine/Scene` modules without first resolving ownership.
- Reviews spend more time on visuals than on build, tests, docs, boundaries, and public contracts.

**Prevention strategy:**
- Start with ADR-style decisions for platform ownership, backend seams, CMake organization, test strategy, and docs policy.
- Add a phase exit question: "Does this change make the foundation more honest/testable/extensible, or is it product feature work?"
- Treat DX9 as seam validation only; one minimal backend object path is enough for this milestone.
- Use explicit anti-scope language in every phase: no full game, no full renderer parity, no broad subsystem implementation.

**Roadmap phase to address:** Phase 1: Architecture Decisions and Scope Guardrails. Re-check at every phase transition.  
**Review/verification detection:** Diff should map to active foundation requirements in `.planning/PROJECT.md`; PR summary must list which foundation risk it reduces. Reject work that cannot name the boundary, contract, gate, or honesty issue it fixes.

### Pitfall 2: Preserving Public APIs That Lie

**What goes wrong:** Public headers, docs, or examples continue to advertise render passes, scene stats, primitive factories, JPEG support, cross-platform support, or module capabilities that are incomplete or fail at link/runtime.  
**Why it happens:** Existing public declarations feel safer to keep than to break, and docs often lag behind code during refactors.  
**Consequences:** Users can compile against APIs that fail to link, examples teach bad patterns, and roadmap work targets stale blockers instead of current defects.

**Warning signs:**
- Public declarations exist without matching source definitions or CMake registration.
- README/docs claim complete JPEG, cross-platform readiness, advanced passes, or culling while code contains placeholders.
- Examples include backend headers or use APIs that are not intended as stable public contracts.
- "We will implement it later" is used to justify keeping an advertised API.

**Prevention strategy:**
- Prefer removal, internalization, or explicit experimental naming over placeholder public APIs.
- For every public header touched, verify implementation, linkage, example usage, and docs claims together.
- Add a public API truth checklist: header compiles, links, works in an example/test, and is documented with limitations.
- Rewrite examples freely so they demonstrate the new clean API, not compatibility with old internals.

**Roadmap phase to address:** Phase 2: Public API Honesty and Example Alignment; revisit in docs phase.  
**Review/verification detection:** Build all examples and tests; run link checks through normal targets; search touched docs for overclaims; require reviewers to verify that each public type/function has an implementation or is clearly hidden/experimental.

### Pitfall 3: Cleaning Renderer Code While Leaving OpenGL Leaks in High-Level Layers

**What goes wrong:** Renderer and scene-level files continue to include `glad/glad.h`, OpenGL framebuffer classes, or native GL handles directly while claiming to support multi-backend architecture.  
**Why it happens:** Direct GL calls are the fastest way to fix immediate rendering problems, especially around framebuffer and render target behavior.  
**Consequences:** DX9 or any future backend must duplicate high-level renderer logic, backend selection remains cosmetic, and platform/backend work becomes a rewrite.

**Warning signs:**
- New `gl*` calls outside OpenGL backend files.
- `RenderSystem`, render passes, examples, or scene code downcast to OpenGL concrete types.
- `IFramebuffer` remains incomplete while render targets bind native framebuffer handles directly.
- Examples need `vendor/glad/include` or OpenGL implementation headers.

**Prevention strategy:**
- Complete the minimal framebuffer/device abstraction before adding renderer features.
- Route render target binding, clear, viewport, texture, shader, buffer, and presentation operations through `IGraphicsDevice` or backend-owned implementation files.
- Keep GL state caching inside `OpenGLStateManager`; do not bypass it from high-level renderer code.
- Add a boundary rule: high-level renderer files may depend on interfaces and data structures, not OpenGL headers or native handles.

**Roadmap phase to address:** Phase 3: Renderer Backend Seam Cleanup.  
**Review/verification detection:** Grep for `glad`, `gl[A-Z_]`, `OpenGL*`, `GLuint`, `HGLRC`, and framebuffer concrete includes outside backend/platform directories; verify examples link only to `PyramidEngine` public API; inspect `IFramebuffer` paths for backend-neutral use.

### Pitfall 4: Designing Command Buffers Without a Resource Lifetime Contract

**What goes wrong:** Recorded commands keep raw pointers, numeric IDs, or stale resource references that can outlive shaders, buffers, textures, framebuffers, or uniform objects.  
**Why it happens:** Command recording often starts as a thin wrapper over immediate GL calls, then later becomes a deferred API without ownership rules.  
**Consequences:** Use-after-free, stale bindings, non-deterministic crashes, silent draw failures, and backend-specific cleanup bugs.

**Warning signs:**
- Commands store `std::uintptr_t`, raw pointers, or unversioned IDs.
- Missing resources are logged only during `Execute()` and command execution continues as if safe.
- Resource destruction can happen from destructors that assume a current GL context.
- There is no documented rule for whether command buffers own, borrow, weak-reference, or validate resources.

**Prevention strategy:**
- Choose one contract: generation-counted handles backed by a registry, or validated `weak_ptr`/`shared_ptr` semantics owned by `RenderSystem`.
- Validate commands at record time or `End()` and return an explicit status from execute paths.
- Centralize graphics resource destruction through the graphics device/render context owner where practical.
- Add focused tests or fakes for stale resource, missing resource, double execute, reset, and shutdown-order cases.

**Roadmap phase to address:** Phase 4: Command and Resource Lifetime Safety.  
**Review/verification detection:** Unit/integration tests must intentionally destroy or invalidate resources before execution and assert safe failure; code review should reject new raw pointer command payloads without documented lifetime ownership.

### Pitfall 5: Treating Cross-Platform Readiness as a CMake Toggle Only

**What goes wrong:** CMake conditionals are added, but `Game.cpp`, window/context creation, GLAD source selection, OpenGL linking, SIMD assumptions, and public platform headers remain Windows/WGL-specific.  
**Why it happens:** Non-Windows support often looks like a build-system problem until platform ownership and context lifecycle are examined.  
**Consequences:** Non-Windows builds fail at configure/link time, platform abstractions leak `Windows.h`, and future SDL/GLFW/custom platform decisions become constrained by accidental dependencies.

**Warning signs:**
- `opengl32` remains linked unconditionally.
- `Game.cpp` directly includes `Win32OpenGLWindow`.
- Public engine headers expose Win32 types.
- `vendor/glad` still builds only WGL loader code.
- SIMD code lacks scalar or non-x86 guarded paths.

**Prevention strategy:**
- Decide platform ownership first: platform factory + context abstraction, not direct concrete construction in core.
- Use platform-conditional source registration and `find_package(OpenGL)`/imported targets instead of hardcoded Windows linkage.
- Keep public `Window` and graphics context contracts native-handle-free unless wrapped in opaque types.
- Add non-Windows compile-readiness as a staged goal after CMake boundaries exist; do not promise runtime support until validated.

**Roadmap phase to address:** Phase 5: Platform and Build Portability Seams.  
**Review/verification detection:** Configure on at least one non-MSVC/non-Windows or compile-only CI job once conditionals exist; grep public headers for `Windows.h`, `HWND`, `HDC`, `HGLRC`; verify `Game` creates windows through a factory.

### Pitfall 6: Refactoring CMake Without Aligning Tests, Docs, and Target Reality

**What goes wrong:** Files move, targets rename, or minimum CMake versions change while presets, docs, smoke scripts, install rules, and test registration remain inconsistent.  
**Why it happens:** Build refactors are often verified by one local Visual Studio build and not by the documented commands users actually run.  
**Consequences:** CI misses tests, smoke scripts launch the wrong executable name, docs tell users impossible commands, and installed headers expose modules with no implementation.

**Warning signs:**
- Root `cmake_minimum_required` differs from preset requirements.
- `PYRAMID_BUILD_TOOLS` points at missing directories.
- Test source files exist but are not in CMake.
- Docs mention build options not defined in CMake.
- Smoke scripts hardcode executable names that differ from target names.

**Prevention strategy:**
- Pick one supported CMake minimum and update root, presets, README, and build guide together.
- Make target names authoritative; scripts should resolve or list all valid configured output names.
- Register every active test in CTest and check `ctest -N` in review.
- Keep placeholder modules include-only until ownership is decided; do not install docs that imply compiled systems exist.

**Roadmap phase to address:** Phase 6: CMake/Test Infrastructure Alignment.  
**Review/verification detection:** Run documented configure/build/test commands and smoke script; compare `ctest -N` against expected tests; verify docs and presets mention the same CMake version, options, and target names.

### Pitfall 7: Adopting GoogleTest Without Changing the Quality Gate

**What goes wrong:** GoogleTest is added as a dependency, but graphics, math, platform seams, command buffers, and asset guardrails remain untested; old permissive executable tests continue hiding failures.  
**Why it happens:** Framework adoption can be mistaken for coverage improvement.  
**Consequences:** Refactors still rely on manual examples, renderer regressions appear late, and CI confidence remains low.

**Warning signs:**
- A new test framework target exists but no new assertions cover foundation risks.
- Existing debug or informational tests remain registered as normal pass/fail tests.
- Tests only cover image components while renderer/platform/math changes proceed.
- Smoke tests are documented but not part of phase verification.

**Prevention strategy:**
- Define required test categories before adding the dependency: API link tests, command lifetime tests, framebuffer/device seam tests, math SIMD/scalar equivalence, image corrupt-boundary tests, and example smoke validation.
- Keep CTest as the runner and make GoogleTest integrate through CTest.
- Convert or quarantine permissive debug tests; failures must fail the process.
- Require every phase to list build, CTest, and smoke/manual verification commands run.

**Roadmap phase to address:** Phase 6: CMake/Test Infrastructure Alignment, then expand per implementation phase.  
**Review/verification detection:** `ctest --output-on-failure` must include new GoogleTest cases; review test names against changed modules; inspect tests for real assertions rather than log-only checks.

### Pitfall 8: Fixing JPEG by Accidentally Starting an Asset Security Project

**What goes wrong:** The milestone either leaves JPEG fake/partial while claiming support, or expands into exhaustive hostile-input hardening, fuzzing infrastructure, and production-grade codec security.  
**Why it happens:** Image parsing bugs are visible and important, but the project scope explicitly allows only trusted-input guardrails for now.  
**Consequences:** Either public API honesty remains false, or asset work consumes the foundation milestone.

**Warning signs:**
- JPEG decode still returns synthetic gradients/test patterns under successful load paths.
- README says complete JPEG while tests only validate markers/components.
- Tasks require broad fuzzing infrastructure or full arbitrary-file security posture.
- Image loaders still allocate from unchecked width/height/pixel-count calculations.

**Prevention strategy:**
- Choose one honest path: complete baseline JPEG decoding enough for claimed support, or remove/mark JPEG support as experimental/unsupported.
- Add trusted-input guardrails: maximum dimensions/pixel count, checked multiplication, truncated/corrupt fixture coverage, and RAII-owned image buffers.
- Do not accept hostile-input security requirements unless they are explicitly moved into a future milestone.
- Keep tests deterministic and small; use fixtures that prove no synthetic success path remains.

**Roadmap phase to address:** Phase 7: Asset API Honesty and Trusted-Input Guardrails.  
**Review/verification detection:** Load known JPEG fixtures and verify expected pixels, not just non-null output; run corrupt/truncated image tests; review docs for exact supported/unsupported format language.

## Moderate Pitfalls

### Pitfall 9: Splitting Modules Before Defining Ownership

**What goes wrong:** Code is moved into `Engine/Renderer`, `Engine/Scene`, `Engine/Input`, `Engine/Audio`, or `Engine/Physics` because those directories exist, not because dependency direction has been decided.  
**Prevention:** Record module ownership first; keep renderer and scene work under existing `Engine/Graphics/...` paths until a deliberate extraction plan exists.  
**Warning signs:** Duplicate scene/render types, include-only modules gaining random source files, circular includes, examples needing new private include paths.  
**Roadmap phase:** Phase 1 decisions; any extraction only in a later dedicated phase.  
**Review/verification detection:** Check new files against the architecture decision; reject duplicate concepts and unregistered source files.

### Pitfall 10: Keeping Shutdown Order Implicit

**What goes wrong:** Window, graphics device, GL context, framebuffer, and resource destructors rely on object destruction order rather than explicit shutdown ownership.  
**Prevention:** Document and enforce that graphics resources are destroyed before the context/window; prefer device-owned cleanup queues for backend resources.  
**Warning signs:** Destructors call `glDelete*` with no context-current guarantee; partial window initialization failures rely only on final destructor cleanup.  
**Roadmap phase:** Phase 4 resource lifetime and Phase 5 platform seam work.  
**Review/verification detection:** Add shutdown-order tests/failure-injection where practical; inspect destructors and `Game::~Game`/`Shutdown` paths for explicit ordering.

### Pitfall 11: Letting Examples Become the Architecture

**What goes wrong:** Example-specific shortcuts, inline shaders, direct backend headers, or one-off setup patterns become de facto engine APIs.  
**Prevention:** Treat examples as validation clients of public APIs only; rewrite them after the public API is cleaned.  
**Warning signs:** Examples include concrete backend headers, duplicate engine initialization logic, or require manual asset/shader path assumptions not documented as engine behavior.  
**Roadmap phase:** Phase 2 public API honesty and final docs/examples phase.  
**Review/verification detection:** Examples should link to `PyramidEngine` and include public `Pyramid/...` headers only; smoke tests should validate examples after API changes.

### Pitfall 12: Making Placeholder Scene Features Look Real

**What goes wrong:** Scene stats, transform updates, primitive factories, frustum culling, and occlusion paths remain placeholder but are used to validate renderer behavior or documented as complete.  
**Prevention:** Either implement minimal truthful basics needed by examples/tests or explicitly mark unsupported. Do not build advanced culling in this milestone unless needed for API honesty.  
**Warning signs:** Stats always return zero, culling always returns false, generated primitives have no GPU buffers, docs mention advanced scene management as complete.  
**Roadmap phase:** Phase 2 API honesty; limited implementation only if examples/tests require it.  
**Review/verification detection:** Add small tests for stats/culling/primitive contracts or remove claims; verify examples do not depend on placeholder success.

### Pitfall 13: Global State Sprawl

**What goes wrong:** More singleton managers are added for convenience, compounding `OpenGLStateManager` and `Logger` global-state constraints.  
**Prevention:** Prefer explicit ownership by `Game`, `IGraphicsDevice`, `RenderSystem`, or platform services; add singleton state only with an ADR and teardown contract.  
**Warning signs:** New `GetInstance()` services, hidden mutable render state, hard-to-reset tests, order-dependent failures.  
**Roadmap phase:** Phase 1 architecture decisions and all implementation phases.  
**Review/verification detection:** Review new services for lifetime, reset, thread/context ownership, and test isolation.

## Minor Pitfalls

### Pitfall 14: Over-Documenting Future Systems

**What goes wrong:** Docs describe planned audio, physics, input, Vulkan/DX, or production asset behavior as if it exists.  
**Prevention:** Use capability tables with statuses: implemented, experimental, planned, out of scope.  
**Warning signs:** User guide examples call APIs with no compiled implementation.  
**Roadmap phase:** Final docs truth pass.  
**Review/verification detection:** Docs claims must map to code, tests, or explicit limitations.

### Pitfall 15: Hiding Build Assumptions in Local Paths

**What goes wrong:** Shader/assets/log paths work only from one working directory or one developer machine.  
**Prevention:** Centralize path resolution, document it, and validate from build-tree smoke tests.  
**Warning signs:** Runtime searches current directory only, docs mention env vars not read by code, logs appear in unexpected directories.  
**Roadmap phase:** CMake/test alignment and docs phase.  
**Review/verification detection:** Smoke tests from a clean build directory; inspect docs against `ShaderPathResolver` and logger behavior.

## Phase-Specific Warnings

| Phase Topic | Likely Pitfall | Mitigation | Review/Verification |
|-------------|----------------|------------|---------------------|
| Architecture decisions | Scope sprawl or vague ADRs | Decision matrices with explicit non-goals | Every implementation task maps to an ADR or active foundation requirement |
| Public API honesty | Keeping declarations that fail later | Remove, hide, implement minimally, or mark experimental | Build/link examples/tests and inspect public headers vs source/CMake |
| Renderer seam cleanup | GL leakage remains in renderer | Finish framebuffer/device abstractions first | Grep backend symbols outside backend/platform files |
| Command/resource safety | Deferred commands outlive resources | Handles/generation registry or validated weak/shared ownership | Tests for stale resource and explicit failure status |
| Platform readiness | CMake-only portability | Platform factory, conditional sources, opaque public interfaces | Non-Windows configure/compile check when available; public header scan |
| CMake/test infrastructure | Framework without coverage | GoogleTest through CTest plus required risk tests | `ctest -N`, `ctest --output-on-failure`, smoke script |
| Asset guardrails | Fake JPEG or security overreach | Honest JPEG support/unsupported status plus trusted-input checks | Pixel fixture tests and corrupt/truncated fixture tests |
| Docs truth pass | Future claims presented as reality | Capability status tables and exact build/run commands | Run documented commands; map claims to tests/examples/code |

## Review Checklist for Foundation-Only Control

- Does the change directly improve architecture boundaries, API honesty, build/test gates, platform/backend seams, resource lifetime, trusted-input guardrails, or docs accuracy?
- Does it avoid full DX9 parity, full cross-platform runtime claims, broad subsystem implementation, and production-grade hostile-input hardening?
- Are public headers backed by source files, CMake target registration, tests/examples, and truthful docs?
- Are OpenGL/Win32/native handles contained behind backend/platform layers?
- Are resource lifetimes explicit rather than implied by raw pointers or destructor order?
- Do documented commands, presets, scripts, target names, and CMake options agree?
- Does verification include build, CTest/GoogleTest where applicable, examples/smoke validation, and docs/build consistency?

## Sources

- `.planning/PROJECT.md` - milestone scope, active requirements, out-of-scope boundaries, and key decisions. Confidence: HIGH.
- `.planning/codebase/CONCERNS.md` - concrete Pyramid risks: GL leakage, command lifetime, incomplete framebuffer abstraction, stale docs, JPEG behavior, Windows-only wiring, and test gaps. Confidence: HIGH.
- `.planning/codebase/TESTING.md` - current CTest/executable test pattern, GoogleTest gap, smoke validation, and missing coverage. Confidence: HIGH.
- `.planning/codebase/STACK.md` - active C++17/CMake/OpenGL/WGL/GLAD stack and build option mismatches. Confidence: HIGH.
- `.planning/codebase/ARCHITECTURE.md` - current component boundaries, data flow, anti-patterns, and architectural constraints. Confidence: HIGH.
