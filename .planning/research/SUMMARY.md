# Project Research Summary

**Project:** Pyramid Engine Foundation
**Domain:** Brownfield C++17/OpenGL game engine foundation refactor
**Researched:** 2026-05-14
**Confidence:** HIGH overall; MEDIUM where SDL3 migration and DX9 prototype details require implementation spikes.

## Executive Summary

Pyramid Engine is a brownfield C++17 game engine currently centered on Windows, Win32/WGL, OpenGL, a `Pyramid::Game` lifecycle, renderer/frontend abstractions, custom image loading, and example apps. Experts would not expand this engine by adding renderer features first; they would make ownership, public API claims, build/test gates, backend seams, resource lifetimes, and docs truthful before building more systems on top.

The recommended approach is conservative and foundation-first: keep C++17 and the current OpenGL backend, modernize CMake/presets/testing, introduce platform and graphics seams before replacing implementations, contain OpenGL/Win32 details behind private backend/platform code, and use a tiny Direct3D 9 prototype only to validate multi-backend architecture. SDL3 is the preferred future cross-platform platform adapter, but should be introduced only after interfaces/factories are stable and without leaking SDL types into public engine APIs.

The main risks are scope sprawl, public APIs that lie, OpenGL leakage in renderer code, unsafe command/resource lifetimes, and build/test/docs drift. Mitigate them with Phase 1 ADRs, explicit non-goals, a public API truth checklist, backend boundary scans, GoogleTest through CTest, phase validation gates, and a final docs truth pass that maps claims to code/tests/examples.

## Key Findings

### Recommended Stack

Use a minimal, durable stack that improves reliability without turning this milestone into tooling migration. Keep the existing engine target and OpenGL runtime working while adding test, build, docs, platform, and backend seams incrementally.

**Core technologies:**
- **C++17:** keep the current language standard; it matches the codebase and GoogleTest 1.17.0 requirements.
- **CMake 3.28+ with CMakePresets:** align root CMake, presets, docs, and CI around one modern baseline; use target-based `PUBLIC`/`PRIVATE` discipline.
- **CTest + GoogleTest 1.17.0 via pinned FetchContent:** keep CTest as runner and register GoogleTest suites with `gtest_discover_tests()`.
- **GitHub Actions, Windows/MSVC first:** make configure/build/CTest blocking before attempting broad CI matrices or hosted graphics smoke tests.
- **OpenGL + vendored GLAD:** retain current backend, but keep GLAD and OpenGL types private to backend/platform implementation.
- **CMake `OpenGL::GL`:** replace hardcoded `opengl32` where practical and guard platform/backend links conditionally.
- **SDL3 3.4.8+ later, behind `PYRAMID_PLATFORM_SDL3`:** preferred future platform adapter, but only after platform interfaces exist.
- **Native Direct3D 9:** use Windows SDK `d3d9.h`/`D3d9.lib` for a Windows-only prototype seam; no D3DX or full renderer parity.
- **Doxygen + Markdown:** validate public C++ API/docs honesty without adding a Python docs site stack.

### Table-Stakes Foundation Capabilities

The foundation milestone should convert these directly into roadmap requirements and acceptance checks.

**Must have:**
- **ADR-backed decisions:** module boundaries, dependency direction, platform ownership, renderer/backend seams, CMake shape, test policy, docs policy.
- **Foundation-only scope guard:** no full game, full DX9 parity, broad audio/input/physics, production asset security, or renderer feature expansion.
- **Build/test truth:** CMake minimum/presets/options/targets/docs agree; GoogleTest integrates through CTest; every phase has repeatable validation commands.
- **Public API honesty:** no declarations that fail to link or imply incomplete render passes, fake JPEG support, cross-platform runtime, or placeholder modules.
- **Renderer/backend seam cleanup:** renderer frontend uses interfaces and resource descriptions, not OpenGL concrete types or native handles.
- **Framebuffer abstraction completion:** `IFramebuffer` and render-target binding route through `IGraphicsDevice`.
- **Command/resource lifetime contract:** commands use handles or validated weak ownership, not raw pointers/unversioned IDs.
- **OpenGL containment and DX9 prototype seam:** OpenGL is current backend; DX9 proves factory/device/clear/present only.
- **Platform ownership seam:** `Game` must stop hardcoding `Win32OpenGLWindow`; window/events/native handles and graphics device/context ownership need explicit contracts.
- **Asset honesty and guardrails:** image ownership should be RAII; JPEG must be real and tested or marked unsupported; trusted-input bounds/checks should exist.
- **Examples as contracts:** BasicGame/BasicRendering should demonstrate public, backend-neutral APIs.
- **Docs truth pass:** README, build guide, API docs, critical issues, contributor docs, and examples docs must match actual capabilities.

**Should have / optional after table stakes:**
- Headless/software-assisted rendering checks if renderer validation remains manual-heavy.
- CI matrix beyond Windows/MSVC after platform-conditional CMake is clean.
- Sanitizer/static-analysis presets after test/build hygiene stabilizes.
- Minimal device capability reporting if DX9 exposes meaningful differences.
- Fixture assets/golden samples only where asset guardrails or smoke validation need them.

**Defer:**
- Full DX9 renderer parity, Vulkan/Metal/D3D11+, SDL Renderer/SDL GPU, package-manager migration, broad fuzzing/security campaign, plugin-style backend modules, full module target split, Sphinx/MkDocs site stack, and advanced scene/culling/rendering features.

### Architecture Decision Themes and ADR Topics

The architecture should remain one primary `PyramidEngine` target for now, with logical module boundaries enforced by headers, private implementation directories, conditional CMake sources, and dependency scans. The desired dependency direction is Apps -> Core/Renderer public APIs -> Platform/Graphics interfaces -> backend/private native APIs; Math and Utils should remain mostly independent.

**Major components:**
1. **Core runtime:** owns application lifecycle, runtime configuration, factories, frame loop, and explicit shutdown ordering.
2. **Platform layer:** owns OS window/events/native handles, without leaking `Windows.h` from public APIs.
3. **Graphics frontend:** owns API-neutral devices, resources, capabilities, and descriptions.
4. **Graphics backends:** OpenGL and DX9 private implementations that translate frontend contracts to native API calls.
5. **Renderer frontend:** orchestrates passes, render targets, and command validation through graphics interfaces only.
6. **Scene/Math/Utils:** provide data, numeric support, logging, image loading, and guardrails without backend coupling.

**Phase 1 ADR topics / open decisions:**
- `ADR-Platform-Window-Context-Ownership`: choose who owns native windows, GL contexts, D3D devices, presentation, and shutdown ordering.
- `ADR-Platform-Factory-And-Native-Handles`: define how `Game` requests a window/device without public `Windows.h`, `HWND`, `HDC`, or `HGLRC` leakage.
- `ADR-Event-And-Input-Scope`: decide whether minimal events/input are needed for examples or remain out of scope.
- `ADR-CMake-Target-Strategy`: keep one target now vs. split stable modules later; recommendation is one target now.
- `ADR-CMake-Backend-Options`: define `PYRAMID_RENDER_OPENGL`, `PYRAMID_RENDER_D3D9`, platform guards, defaults, and unsupported-platform behavior.
- `ADR-CMake-Test-Organization`: define GoogleTest/CTest layout and legacy executable-test migration.
- `ADR-Graphics-Resource-Handles`: choose generation-counted handles vs. transitional `weak_ptr` validation.
- `ADR-Graphics-Shutdown-And-Context-Ownership`: define context-current requirements and resource destruction order.
- `ADR-CommandBuffer-Validation`: define record-time vs. execute-time validation and failure policy.
- `ADR-Render-Pass-API-Honesty`: remove, hide, implement, or mark experimental every advertised render-pass/public renderer API.
- `ADR-DX9-Prototype-Acceptance`: lock DX9 to backend selection, device creation, clear/present, and optional trivial draw only.
- `ADR-Docs-And-Capability-Status`: require docs/examples to use implemented/experimental/planned/out-of-scope status language.

### Main Pitfalls and Prevention Strategies

1. **Foundation refactor becomes a feature rewrite** — start with ADRs, enforce non-goals, and require every phase task to name the foundation risk it reduces.
2. **Public APIs keep lying** — apply a truth checklist: public header compiles, links, works in test/example, and has accurate docs/limitations.
3. **OpenGL leaks remain in high-level renderer layers** — complete framebuffer/device abstractions first and grep for GL/native symbols outside backend/platform code.
4. **Command buffers lack resource lifetime rules** — reject raw pointer command payloads; use handles or validated weak ownership with focused stale-resource tests.
5. **Cross-platform readiness is treated as CMake-only** — define platform/window/context ownership and opaque public contracts before promising non-Windows runtime.
6. **CMake, tests, docs, and smoke scripts drift** — update minimum versions, presets, target names, docs, `ctest -N`, and smoke commands together.
7. **GoogleTest is adopted without better coverage** — require tests for API link behavior, framebuffer/device seams, resource lifetime, math equivalence, and asset guardrails.
8. **JPEG work becomes either fake support or a security project** — choose honest support vs. unsupported status, add RAII/bounds/corrupt fixture tests, and avoid broad hostile-input hardening.

## Implications for Roadmap

### Phase 1: Architecture Decisions and Scope Guardrails
**Rationale:** All later implementation depends on ownership, boundaries, CMake shape, backend policy, test gates, and honesty rules.  
**Delivers:** ADRs listed above, explicit non-goals, foundation-only review checklist, phase validation policy.  
**Addresses:** FND-CAP-001, FND-CAP-002.  
**Avoids:** Scope sprawl, vague ownership, premature file/module moves, global-state growth.

### Phase 2: Build and Test Foundation
**Rationale:** Invasive refactors need repeatable configure/build/test gates before code movement begins.  
**Delivers:** aligned CMake minimum/presets/docs, `PYRAMID_BUILD_TESTS`, pinned GoogleTest, CTest registration, no-tests failure, initial non-Utils tests, Windows CI baseline.  
**Addresses:** FND-CAP-004, FND-CAP-005, FND-CAP-006, FND-CAP-020.  
**Avoids:** CMake/test/docs drift and framework-without-coverage.

### Phase 3: Public API Honesty and Example Contract Baseline
**Rationale:** Roadmap work should not build against APIs that fail to link or advertise placeholder behavior.  
**Delivers:** public declaration audit, render-pass/API status policy applied, JPEG claim decision started or scoped, examples adjusted away from private backend internals where possible.  
**Addresses:** FND-CAP-003, FND-CAP-009, FND-CAP-015, early FND-CAP-019.  
**Avoids:** lying APIs, examples becoming architecture, placeholder scene/render claims.

### Phase 4: Platform/Runtime Ownership Seam
**Rationale:** `Game` currently hardcodes Win32/WGL; renderer and DX9 work need a clean window/device creation path first.  
**Delivers:** platform/window factory, opaque native-handle service or equivalent, explicit shutdown ordering, conditional Win32 source registration, `Windows.h` removal from public/core paths.  
**Addresses:** FND-CAP-013, FND-CAP-014.  
**Avoids:** CMake-only portability, implicit shutdown order, public Win32 leakage.

### Phase 5: Renderer Backend Seam and OpenGL Containment
**Rationale:** Multi-backend readiness is false until renderer/frontend code stops binding GL-specific resources directly.  
**Delivers:** completed `IFramebuffer` binding, render-target operations through `IGraphicsDevice`, GLAD/OpenGL headers private to backend, backend capability/status reporting as needed.  
**Addresses:** FND-CAP-007, FND-CAP-008, FND-CAP-011.  
**Avoids:** renderer cleanup that still leaves GL leakage.

### Phase 6: Command and Resource Lifetime Safety
**Rationale:** Backend/refactor work can expose UAF/stale-command bugs unless command ownership is explicit.  
**Delivers:** selected handle/weak-validation model, explicit execute failure status, destruction/shutdown tests, no raw pointer command payloads.  
**Addresses:** FND-CAP-010.  
**Avoids:** stale resources, context-invalid destruction, non-deterministic renderer failures.

### Phase 7: DX9 Prototype Seam
**Rationale:** DX9 should validate the architecture only after OpenGL and platform seams are real.  
**Delivers:** Windows-only conditional backend factory path, native D3D9 device creation, clear/present smoke path, optional trivial triangle if cheap, clear unsupported feature reporting.  
**Addresses:** FND-CAP-012.  
**Avoids:** full renderer parity and DX9 device-loss scope creep.

### Phase 8: Asset API Honesty and Trusted-Input Guardrails
**Rationale:** Image loading is a public utility surface with ownership and honesty risks independent of renderer seams.  
**Delivers:** RAII image buffers, checked dimensions/size calculations, corrupt/truncated fixtures, JPEG genuinely implemented/tested or marked unsupported.  
**Addresses:** FND-CAP-017, FND-CAP-018, FND-CAP-019.  
**Avoids:** fake JPEG support and broad security overreach.

### Phase 9: Examples, Smoke Validation, and Docs Truth Pass
**Rationale:** User-facing contracts should close the milestone after implementation truth is known.  
**Delivers:** BasicGame/BasicRendering rewritten as clean public API contracts, smoke script alignment, Doxygen/Markdown docs, contributor workflow, capability status tables.  
**Addresses:** FND-CAP-015, FND-CAP-016, FND-CAP-021, FND-CAP-022.  
**Avoids:** stale docs, hidden local-path assumptions, future systems documented as real.

### Phase Ordering Rationale

- Decisions precede implementation because platform ownership, backend seams, CMake strategy, resource lifetime, and API honesty affect almost every file touched later.
- Build/test gates come before invasive refactors so each boundary change can be validated with CTest, examples, and documented commands.
- Platform/runtime and renderer seams precede DX9 because DX9 should prove abstraction, not force a second renderer rewrite.
- Asset guardrails are separated from backend work to keep JPEG/image honesty focused and avoid a security-hardening detour.
- Docs and examples close the milestone because they must describe actual outcomes, not planned intent.

### Research Flags

Phases likely needing deeper research or spikes during planning:
- **Phase 1:** ADRs must resolve ownership and acceptance criteria; decisions are high leverage.
- **Phase 4:** SDL3/custom platform migration path and native-handle abstraction need validation against current Win32/WGL code.
- **Phase 6:** handle registry vs. weak-pointer validation affects renderer internals and shutdown safety.
- **Phase 7:** DX9 device creation/reset behavior should be spike-limited before implementation tasks are finalized.
- **Phase 8:** JPEG completion complexity depends on existing decoder salvageability.

Phases with standard patterns that likely do not need separate research-phase work:
- **Phase 2:** CMake/CTest/GoogleTest/Windows CI patterns are well documented.
- **Phase 3:** API audit/link/example truth checks are process-heavy but technically straightforward.
- **Phase 5:** OpenGL containment pattern is clear; implementation needs careful code audit more than research.
- **Phase 9:** Doxygen + Markdown docs truth pass is standard once code status is known.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH / MEDIUM | HIGH for C++17, CMake, CTest, GoogleTest, OpenGL, Doxygen, native DX9 basics; MEDIUM for SDL3 migration cost until a seam spike validates it. |
| Features | HIGH | Capabilities are grounded in `.planning/PROJECT.md` and codebase maps; scope/non-goals are explicit. |
| Architecture | HIGH / MEDIUM | HIGH for current risks and dependency direction; MEDIUM for final DX9 prototype shape and resource-handle implementation cost. |
| Pitfalls | HIGH | Pitfalls are directly tied to known brownfield risks in renderer, platform, tests, docs, image loading, and CMake. |

**Overall confidence:** HIGH for roadmap structure; MEDIUM for estimates on SDL3, DX9, and JPEG implementation effort.

### Gaps to Address

- **SDL3 adoption path:** validate after platform interfaces exist; do not start with SDL dependency replacement.
- **DX9 prototype acceptance:** lock to factory/device/clear/present and decide how much device-loss handling is required for a smoke path.
- **Resource lifetime model:** decide handles vs. transitional weak validation before command-buffer refactor tasks are written.
- **JPEG scope:** inspect existing decoder and choose complete baseline support or unsupported/experimental status.
- **Non-Windows readiness:** first make sources/links conditional, then add configure/build-only checks before claiming runtime support.
- **API inventory:** perform a public header/source/CMake/docs audit to identify exact declarations that must be removed, hidden, or implemented.

## Sources

### Primary (HIGH confidence)
- `.planning/PROJECT.md` — milestone identity, active requirements, constraints, out-of-scope boundaries, and pending key decisions.
- `.planning/research/STACK.md` — stack/tooling recommendations, versions, decision matrices, and roadmap implications.
- `.planning/research/FEATURES.md` — table-stakes capabilities, optional differentiators, anti-features, dependency order, and MVP recommendation.
- `.planning/research/ARCHITECTURE.md` — component boundaries, dependency direction, ownership options, ADR topics, build order, and validation gates.
- `.planning/research/PITFALLS.md` — critical/moderate/minor pitfalls, phase warnings, and review checklist.
- `.planning/codebase/*` references cited by research outputs — current codebase structure, architecture, testing gaps, stack, integrations, and concerns.

### External official sources cited by research outputs
- CMake official documentation — presets, buildsystem targets, FetchContent, CTest, and GoogleTest module behavior.
- GoogleTest official quickstart/releases — C++17 requirement and CMake integration for version 1.17.0.
- SDL3 documentation/releases — OpenGL context APIs and SDL 3.4.8 availability.
- GLFW official docs — rejected alternative baseline for platform/windowing comparison.
- Microsoft Learn Direct3D 9 docs — `Direct3DCreate9`, `d3d9.h`, `D3d9.lib`, `D3d9.dll` requirements.
- Doxygen official docs — C++ API comments and Markdown page support.
- ISO C++ Core Guidelines — RAII, resource safety, and interface/encapsulation guidance.

---
*Research completed: 2026-05-14*
*Ready for roadmap: yes*
