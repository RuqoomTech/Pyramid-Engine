# Feature Landscape: Pyramid Engine Foundation Refactor

**Domain:** Brownfield C++17/OpenGL game engine foundation milestone  
**Researched:** 2026-05-14  
**Scope:** Foundation capabilities that make the engine honest, testable, backend-ready, cross-platform-ready, and safe to extend. This is not a gameplay/product feature list.

## Table Stakes

Capabilities expected before the refactor milestone can be considered trustworthy. These should convert directly into requirement IDs.

| Capability ID | Capability | Why Expected | Complexity | Dependencies | Acceptance Shape |
|---|---|---|---|---|---|
| FND-CAP-001 | ADR-backed foundation decisions | Platform ownership, renderer/backend seams, CMake shape, test policy, and docs policy affect many later changes; guessing causes rewrites. | Medium | None | Decision records exist and are referenced by implementation phases. |
| FND-CAP-002 | Foundation-only scope guard | The current engine already has many tempting feature gaps; milestone success depends on preventing audio/physics/editor/gameplay expansion. | Low | FND-CAP-001 | Explicit non-goals are documented and used to reject unrelated work. |
| FND-CAP-003 | Public API honesty pass | Public declarations must not fail at link time or imply missing capabilities such as unimplemented render passes or fake JPEG support. | Medium | FND-CAP-001 | Missing APIs are implemented, hidden, removed, or marked experimental. |
| FND-CAP-004 | Build system truth and target hygiene | A foundation engine must configure, build, test, and install consistently across documented options. | Medium | FND-CAP-001 | CMake minimum, presets, options, targets, examples, tests, and docs agree. |
| FND-CAP-005 | GoogleTest adoption through CTest | The milestone needs maintainable assertions, fixtures, and module tests beyond ad hoc executable checks. | Medium | FND-CAP-004 | GoogleTest is integrated without breaking CTest; at least one non-Utils module test exists. |
| FND-CAP-006 | Phase validation gate | Every implementation phase needs a repeatable gate: configure, build, CTest/GoogleTest, example build, smoke validation where applicable, and docs consistency. | Medium | FND-CAP-004, FND-CAP-005 | A documented command sequence/preset verifies the phase and is run before phase completion. |
| FND-CAP-007 | Renderer/backend seam cleanup | High-level renderer code should not bind OpenGL-specific objects directly if DX9 or later backends are planned. | High | FND-CAP-001, FND-CAP-003 | Renderer code depends on graphics interfaces; OpenGL details stay in backend implementation. |
| FND-CAP-008 | Framebuffer abstraction completion | Framebuffer binding is a current backend-abstraction hole and blocks honest render target/pass design. | High | FND-CAP-007 | `IFramebuffer` binding works through `IGraphicsDevice`; render targets do not need direct GL handles. |
| FND-CAP-009 | Render pass API reconciliation | Declared-but-missing passes create link-time failures and false capability claims. | Medium | FND-CAP-003, FND-CAP-007 | Forward/shadow/deferred/current passes are documented as supported; unfinished transparent/post/UI/debug/factory APIs are implemented or removed. |
| FND-CAP-010 | Command/resource lifetime contract | Queued render commands need stable resource validation to avoid stale pointer/use-after-free risks. | High | FND-CAP-007 | Commands use handles, weak ownership, or a validated registry with failure reporting before GL/DX calls. |
| FND-CAP-011 | OpenGL backend containment | OpenGL remains the current backend, but backend headers/state/glad dependencies should not leak into examples or API-independent renderer layers. | High | FND-CAP-007, FND-CAP-008 | Examples create resources through `IGraphicsDevice`; backend-specific includes are private or clearly experimental. |
| FND-CAP-012 | Direct3D 9 prototype seam definition | DX9 is useful to prove backend separation, but full renderer parity is outside this milestone. | Medium | FND-CAP-007, FND-CAP-011 | API factory and build structure can select a DX9 prototype stub/seam without promising feature parity. |
| FND-CAP-013 | Platform ownership seam | Cross-platform aspirations require a clear split between core lifecycle, native window/context, input/event pumping, and graphics-device creation. | High | FND-CAP-001, FND-CAP-004 | `Game` no longer hardcodes Win32/WGL ownership decisions beyond a platform factory or equivalent seam. |
| FND-CAP-014 | Platform-conditional CMake | Windows-only sources and `opengl32` linking must be conditional before non-Windows builds are credible. | Medium | FND-CAP-004, FND-CAP-013 | Platform libraries/sources are guarded; unsupported platforms fail clearly or build non-runtime targets. |
| FND-CAP-015 | Example API cleanup | Examples are the executable contract for small-engine users and must demonstrate the intended clean API, not old internal patterns. | Medium | FND-CAP-003, FND-CAP-007, FND-CAP-013 | BasicGame/BasicRendering build and run using public, backend-neutral engine APIs. |
| FND-CAP-016 | Example smoke validation alignment | Smoke scripts, target names, docs, and CMake outputs must agree or examples cannot serve as validation gates. | Low | FND-CAP-004, FND-CAP-015 | Smoke script resolves actual target paths/names and documents bounded execution expectations. |
| FND-CAP-017 | Asset ownership cleanup | Image buffers should not require manual `Image::Free()` in ordinary safe paths. | Medium | FND-CAP-003 | `ImageData` owns memory via RAII or an equivalent safe wrapper; callers cannot double-free by default. |
| FND-CAP-018 | Trusted-input asset guardrails | Foundation needs reasonable protection from malformed development assets without becoming a security-hardening project. | Medium | FND-CAP-017, FND-CAP-005 | Dimension limits, checked size calculations, truncated/corrupt fixture tests, and clear failure returns exist. |
| FND-CAP-019 | JPEG honesty | Current JPEG behavior cannot be claimed as complete if it emits synthetic/test-pattern pixels. | High | FND-CAP-003, FND-CAP-005, FND-CAP-017 | Either baseline JPEG decoding is genuinely implemented and tested, or JPEG support is disabled/marked unsupported. |
| FND-CAP-020 | Minimal tests outside image utilities | Foundation confidence requires tests for math/core/renderer seams, not only PNG/JPEG components. | Medium | FND-CAP-005, FND-CAP-007 | At least core lifecycle-independent, math/SIMD fallback, command-buffer/resource-validation, and asset guardrail tests are registered. |
| FND-CAP-021 | Docs truth pass | README, build guide, API docs, critical-issues docs, and examples docs must match actual capabilities. | Medium | FND-CAP-003 through FND-CAP-020 | Docs no longer claim cross-platform runtime, complete JPEG, culling, or render passes unless implemented. |
| FND-CAP-022 | Contributor workflow documentation | Future contributors need clear rules for adding modules, tests, examples, and backend/platform code. | Low | FND-CAP-004, FND-CAP-005, FND-CAP-021 | Contributor docs describe module layout, CMake registration, test expectations, and validation commands. |

## Differentiators / Optional Enhancements

Valuable foundation improvements, but not required unless they directly support table-stakes work.

| Capability ID | Capability | Value Proposition | Complexity | Dependencies | Recommendation |
|---|---|---|---|---|---|
| FND-OPT-001 | Headless or software-assisted rendering checks | Would make renderer changes easier to validate in CI without a desktop/GPU. | High | FND-CAP-007, FND-CAP-008, FND-CAP-020 | Defer unless a phase specifically targets rendering verification. |
| FND-OPT-002 | CI matrix beyond Windows/MSVC | Catches portability regressions earlier for cross-platform aspirations. | Medium | FND-CAP-014 | Add after platform-conditional CMake is clean; start with configure/build-only non-Windows jobs. |
| FND-OPT-003 | Sanitizer/static-analysis presets | Useful for memory and undefined-behavior risks in custom image parsing and renderer ownership. | Medium | FND-CAP-004, FND-CAP-018 | Add as optional local/CI presets after build hygiene stabilizes. |
| FND-OPT-004 | Device capability reporting | Querying limits such as texture units and backend features helps backend neutrality. | Medium | FND-CAP-007, FND-CAP-011 | Useful if DX9 seam exposes capability differences; keep minimal. |
| FND-OPT-005 | Scalar/SIMD equivalence test suite | Improves confidence for cross-platform and non-x86 behavior. | Medium | FND-CAP-005, FND-CAP-020 | Include a minimal version in table-stakes tests; deeper coverage is optional. |
| FND-OPT-006 | Backend plugin-style target split | Cleaner long-term than one monolithic engine target, but likely too much churn for this milestone. | High | FND-CAP-004, FND-CAP-007, FND-CAP-012 | Document direction, but avoid implementing unless CMake refactor already requires it. |
| FND-OPT-007 | Fixture asset directory and golden samples | Better image/render regression tests than embedded bytes alone. | Medium | FND-CAP-005, FND-CAP-018, FND-CAP-019 | Add only for assets needed by JPEG/PNG guardrails or example validation. |
| FND-OPT-008 | Structured logging configuration paths | Avoids surprise log files in working directories and helps packaged examples. | Low | FND-CAP-021 | Nice follow-up; not blocking unless smoke tests depend on logs. |

## Anti-Features / Explicit Non-Goals

| Anti-Feature | Why Avoid | What to Do Instead |
|---|---|---|
| Full Direct3D 9 renderer parity | Would turn a foundation seam into a second renderer project. | Define and optionally stub/prototype the backend seam only. |
| Vulkan/Metal/D3D11+ backend work | Expands backend scope before OpenGL abstraction and DX9 seam prove the design. | Keep interfaces backend-neutral and record future-backend constraints in ADRs. |
| Full hostile-input asset hardening/fuzzing campaign | The milestone treats assets as trusted development inputs with guardrails; full security work would derail scope. | Add bounds, checked arithmetic, corrupt/truncated fixtures, and honest docs. |
| Building a real game or content pipeline | Examples validate APIs; a game would hide foundation problems under app-specific code. | Rewrite/add minimal examples only when they prove the public API. |
| Adding broad audio/input/physics systems | Placeholder modules are tempting, but not needed to make the existing engine trustworthy. | Leave as documented future modules unless needed for platform ownership decisions. |
| Preserving current internal API compatibility | Existing internals include leaky backend dependencies and misleading public surfaces. | Break APIs when doing so makes boundaries honest and documented. |
| Implementing advanced renderer features before seam cleanup | Transparent/UI/debug/post passes are not foundation blockers unless public APIs already promise them. | Reconcile public declarations first; defer feature-rich rendering. |
| Occlusion culling or advanced scene optimization | Current culling/stats are placeholders; advanced optimization is premature. | Either make current stats/culling honest/minimal or mark as unsupported. |
| Package manager migration as a primary goal | Dependency tooling can become a side project. | Keep glad/vendor treatment minimal; only change what backend/platform seams require. |

## Capability Dependencies and Suggested Order

```text
FND-CAP-001 ADR decisions
  -> FND-CAP-002 scope guard
  -> FND-CAP-004 build truth
      -> FND-CAP-005 GoogleTest/CTest
          -> FND-CAP-006 phase gate
          -> FND-CAP-020 non-Utils tests
      -> FND-CAP-014 platform-conditional CMake
  -> FND-CAP-003 API honesty
      -> FND-CAP-009 render pass reconciliation
      -> FND-CAP-017 asset ownership
          -> FND-CAP-018 asset guardrails
          -> FND-CAP-019 JPEG honesty
  -> FND-CAP-007 renderer/backend seam
      -> FND-CAP-008 framebuffer abstraction
      -> FND-CAP-010 command/resource lifetime
      -> FND-CAP-011 OpenGL containment
          -> FND-CAP-012 DX9 prototype seam
  -> FND-CAP-013 platform ownership seam
      -> FND-CAP-014 platform-conditional CMake

FND-CAP-015 example API cleanup depends on API/platform/renderer decisions.
FND-CAP-016 smoke validation alignment depends on build truth and examples.
FND-CAP-021 docs truth pass should happen after implementation truth is known.
FND-CAP-022 contributor workflow docs should close the milestone.
```

## MVP Recommendation for the Foundation Milestone

Prioritize these as the minimum viable foundation:

1. **Decision and scope layer:** FND-CAP-001, FND-CAP-002.
2. **Build/test gate:** FND-CAP-004, FND-CAP-005, FND-CAP-006.
3. **API honesty:** FND-CAP-003, FND-CAP-009, FND-CAP-019.
4. **Renderer seam:** FND-CAP-007, FND-CAP-008, FND-CAP-010, FND-CAP-011, FND-CAP-012.
5. **Platform seam:** FND-CAP-013, FND-CAP-014.
6. **Asset safety:** FND-CAP-017, FND-CAP-018.
7. **Validation surface:** FND-CAP-015, FND-CAP-016, FND-CAP-020.
8. **Docs closeout:** FND-CAP-021, FND-CAP-022.

Defer optional differentiators unless they become necessary for a table-stakes acceptance gate.

## Complexity Summary

| Complexity | Capabilities | Roadmap Implication |
|---|---|---|
| Low | FND-CAP-002, FND-CAP-016, FND-CAP-022 | Good early/late glue tasks; should not dominate phases. |
| Medium | FND-CAP-001, FND-CAP-003, FND-CAP-004, FND-CAP-005, FND-CAP-006, FND-CAP-009, FND-CAP-012, FND-CAP-014, FND-CAP-015, FND-CAP-017, FND-CAP-018, FND-CAP-020, FND-CAP-021 | Most should be phase-sized requirements with clear acceptance criteria. |
| High | FND-CAP-007, FND-CAP-008, FND-CAP-010, FND-CAP-011, FND-CAP-013, FND-CAP-019 | Require deeper design, sequencing, and regression validation. |

## Sources and Confidence

| Source | Relevance | Confidence |
|---|---|---|
| `.planning/PROJECT.md` | Milestone goals, active requirements, constraints, out-of-scope boundaries. | HIGH |
| `.planning/codebase/ARCHITECTURE.md` | Current module boundaries, backend/platform coupling, public API patterns. | HIGH |
| `.planning/codebase/STRUCTURE.md` | Directory layout, CMake/module organization, examples, docs locations. | HIGH |
| `.planning/codebase/CONCERNS.md` | Known renderer, asset, platform, test, and docs risks. | HIGH |
| `.planning/codebase/TESTING.md` | Existing CTest/executable pattern and GoogleTest gap. | HIGH |

## Research Gaps

- Exact GoogleTest acquisition method should be decided during stack/build research, not in this feature list.
- Exact platform/windowing choice for cross-platform support needs dedicated platform research.
- Exact DX9 prototype API shape needs renderer/backend architecture research.
- JPEG completion complexity depends on how much of the existing decoder is salvageable.
