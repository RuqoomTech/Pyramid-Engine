# Pyramid Engine Foundation

## What This Is

Pyramid Engine is a brownfield C++17 game engine project currently centered on a Windows/OpenGL runtime, a `Pyramid::Game` lifecycle, graphics device abstractions, renderer passes, scene/math utilities, custom image loading, and example applications. The next project milestone is a foundation refactor: make the engine architecture honest, testable, cross-platform-ready, and safe to extend before broader feature work continues.

The long-term identity is balanced: a serious learning engine for graphics/platform architecture, a practical small-game engine, and a renderer/backend experimentation lab. The immediate work should favor correctness of boundaries, build reliability, tests, and documentation over compatibility with current internals.

## Core Value

Pyramid Engine must become a trustworthy foundation where examples, tests, docs, and public APIs accurately reflect what the engine can do and where future renderer/platform backends can be added without architectural guesswork.

## Requirements

### Validated

<!-- Shipped and confirmed present in the existing brownfield codebase. -->

- [validated] Engine builds as a C++17 CMake project around a `PyramidEngine` library target.
- [validated] Current runtime supports Windows desktop execution through Win32/WGL and OpenGL.
- [validated] `Pyramid::Game` provides a runnable lifecycle with create, update, render, and frame loop hooks.
- [validated] Graphics code exposes an `IGraphicsDevice` abstraction with an OpenGL implementation.
- [validated] Renderer code contains command buffers, render targets, render passes, scene integration, cameras, and lighting structures.
- [validated] Math utilities provide vectors, matrices, quaternions, SIMD helpers, and common numeric support.
- [validated] Utility image loading supports TGA, BMP, PNG, and partial JPEG-related components.
- [validated] BasicGame and BasicRendering examples exist as runtime validation targets.
- [validated] Current tests are executable/CTest-style utility tests focused mainly on PNG/JPEG/image components.
- [validated] A codebase map exists under `.planning/codebase/` and captures current stack, architecture, conventions, testing, integrations, and concerns.

### Active

<!-- Current scope. Building toward these. -->

- [ ] Establish explicit architecture decisions for module boundaries, dependency direction, platform ownership, renderer/backend seams, CMake structure, test framework adoption, and documentation policy.
- [ ] Control scope sprawl by keeping this milestone focused on foundation work only.
- [ ] Adopt GoogleTest and make build, CTest/GoogleTest, example smoke validation, and docs/build consistency the expected phase gate.
- [ ] Make public engine APIs honest: no public declarations that fail at link time, overpromise capabilities, or hide placeholder implementations as complete features.
- [ ] Improve renderer seams by addressing render pass API honesty, graphics backend separation, framebuffer abstraction, and command/resource lifetime risks.
- [ ] Prepare for a multi-backend renderer direction with OpenGL cleaned up and a Direct3D 9 prototype seam planned for legacy support, without requiring full renderer parity in this milestone.
- [ ] Prepare cross-platform structure now by researching and defining platform/window/context/input ownership before implementation.
- [ ] Research and select a CMake organization strategy before large build refactors.
- [ ] Fully implement JPEG decoding or otherwise make JPEG behavior genuinely complete and tested before claiming support.
- [ ] Apply trusted-input asset guardrails including ownership cleanup, reasonable dimension checks, checked size calculations, and corrupt/truncated fixture coverage where practical.
- [ ] Implement minimal viable basics for unfinished modules only where needed to support examples, tests, or honest public contracts.
- [ ] Rewrite examples freely when needed so they demonstrate the new clean API instead of preserving current internal patterns.
- [ ] Produce a dedicated docs phase covering truth pass, contributor guide, and user guide.

### Out of Scope

<!-- Explicit boundaries. Includes reasoning to prevent re-adding. -->

- Full production-grade hostile-input asset hardening - this milestone treats assets as trusted development inputs with sensible guardrails, not as arbitrary untrusted uploads.
- Full Direct3D 9 renderer parity - DX9 is a prototype seam for legacy support and backend validation, not a complete replacement backend in this foundation milestone.
- Full game project or content production - examples may be rewritten or added for validation, but building a real game is not the goal of this milestone.
- Broad feature expansion unrelated to foundation quality - new features are allowed only when they clarify architecture, validate a seam, or make incomplete public surfaces honest.
- Compatibility preservation for current internal APIs - breaking changes are acceptable when they make architecture, testing, or public behavior more correct.

## Context

Pyramid Engine already contains substantial graphics/rendering code, scene structures, math utilities, image codecs, examples, docs, and CMake infrastructure. The codebase map identifies important risks: OpenGL-specific details leak through high-level renderer code, incomplete framebuffer abstraction, command/resource lifetime hazards, render pass declarations without implementations, placeholder scene statistics/culling, manual image loader ownership, partial/fake JPEG decode behavior, Windows-only platform wiring, CMake/documentation mismatches, and limited test coverage outside image utilities.

The user wants a full project initialization discussion and has selected a foundation refactor direction. All core quality outcomes matter: stable foundation, trusted changes, usable APIs, and clean builds. The first phase should focus on research decisions, recorded as ADR-style documents, to avoid starting implementation with the wrong architecture. Scope sprawl is the highest early risk.

Research should be foundation-oriented: cross-platform/windowing choices, platform ownership, renderer backend architecture, Direct3D 9 seam strategy, CMake organization, GoogleTest integration, and documentation structure. Research output should use decision matrices rather than oversized implementation specs; implementation details belong in later phase plans.

## Constraints

- **Milestone scope**: Foundation only - stop when architecture decisions, build/test gates, API honesty, platform seams, renderer seams, asset safety, and docs are addressed.
- **Compatibility**: Existing internals and public APIs may break if that makes the architecture correct.
- **Backend direction**: Multi-backend is desired; OpenGL remains current, Direct3D 9 is targeted as a prototype seam for legacy support.
- **Cross-platform direction**: Cross-platform readiness is required, but platform/windowing approach must be researched before locking SDL, GLFW, or custom platform layers.
- **Testing**: GoogleTest is the preferred framework; every phase should aim for full gate validation.
- **Asset safety**: Trusted-input guardrails are sufficient for now; do not turn this milestone into a full security hardening project.
- **Documentation**: A dedicated docs phase must align README, build guide, critical issues, API docs, contributor guidance, and user-facing build/run instructions with actual capabilities.

## Key Decisions

<!-- Decisions that constrain future work. Add throughout project lifecycle. -->

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Use a foundation refactor milestone | The current engine has architecture drift, reliability gaps, API confusion, and build friction. | Pending |
| Treat stable foundation, trusted changes, usable API, and clean build as co-equal priorities | Selecting only one would leave the engine unsafe to extend. | Pending |
| Keep milestone scope foundation-only | Scope sprawl is the earliest risk to control. | Pending |
| Record major decisions as ADR-style docs | Platform, backend, build, tests, and docs choices need durable rationale. | Pending |
| Research architecture, platform ownership, and CMake structure before locking implementation | Current seams affect every later phase and should not be guessed. | Pending |
| Adopt GoogleTest as the planned test framework | The user prefers a real framework over continuing only standalone ad hoc tests. | Pending |
| Use full validation gates for phases | Build, tests, examples/smoke, and docs/build consistency should prevent regressions. | Pending |
| Make public APIs honest | Public declarations and docs must not imply missing or placeholder capabilities. | Pending |
| Prioritize render pass API honesty first in renderer cleanup | Current public render pass declarations exceed implementation. | Pending |
| Pursue multi-backend architecture with DX9 as a prototype seam | DX9 supports the legacy-support goal while proving backend separation. | Pending |
| Keep DX9 scope to prototype seam in this milestone | Full parity would expand scope beyond foundation work. | Pending |
| Treat cross-platform work and DX9 backend work as separate tracks | Cross-platform platform ownership and legacy backend validation have different risks. | Pending |
| Fully implement JPEG support before claiming it | Current JPEG behavior is not honest enough for production-facing docs or APIs. | Pending |
| Use trusted-input asset guardrails, not hostile-input hardening | This keeps asset safety useful without derailing foundation scope. | Pending |
| Rewrite examples freely if needed | Examples should demonstrate clean APIs, not preserve outdated internal usage. | Pending |
| Implement minimal viable basics for unfinished modules where needed | Stubbed modules should become honest without creating unrelated subsystem projects. | Pending |
| Create a dedicated docs phase covering truth pass, contributor guide, and user guide | Documentation needs focused alignment after technical foundation decisions. | Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? -> Move to Out of Scope with reason
2. Requirements validated? -> Move to Validated with phase reference
3. New requirements emerged? -> Add to Active
4. Decisions to log? -> Add to Key Decisions
5. "What This Is" still accurate? -> Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check - still the right priority?
3. Audit Out of Scope - reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-05-14 after initialization discussion*
