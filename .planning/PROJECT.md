# Pyramid Engine — Agent-First Game Engine

## What This Is

Pyramid is a Windows-first C++17 game engine becoming an **agent-first platform**: AI agents build complete games from zero to playable through data-first game definitions and a stable Agent SDK, validated by a headless run loop — without modifying engine code and without human help. For human developers it remains a general-purpose engine whose first proving game is a Ruqoom RTS; the RTS slice doubles as the proof vehicle for the agent path.

## Core Value

An external AI agent, given only the agent-facing docs and a headless validate/run loop, can build a complete working game from zero — no engine-code changes, no human help.

## Requirements

### Validated

Shipped in the `0.6.0-pre-alpha` baseline (see `docs/ROADMAP.md`, `.planning/codebase/`).

- ✓ Win32/WGL window, message pump, and OpenGL 3.3 core renderer with forward/deferred/shadow passes — existing
- ✓ Stable-ID entity/component scene with cycle-safe hierarchy, generated renderer/light proxies, v2 serialization — existing
- ✓ Immutable content-addressed GPU resources (mesh, shader, texture, material) with generational handles and versioned manifests — existing
- ✓ Real Win32 keyboard/mouse polling, engine-generic action mapping, reusable camera controllers — existing
- ✓ Independently-packaged owned libraries: Foundation, Math, Input, Image, Model, Font, Text, UI — existing
- ✓ Owned asset pipelines: bounded OBJ/MTL import, PNG/JPEG codecs, TrueType → versioned `.pfont`, international text layout — existing
- ✓ Runtime debug UI, diagnostics console, and game-side RTS interaction reference — existing

### Active

- [ ] P0 core correctness finished and frozen (compute-dispatch decision, shadow-map-array binding, occlusion-culling decision, texture-format mapping)
- [ ] Quality baseline: warnings-as-errors, sanitizer coverage, parser fuzzing, dead-code removal, public-API linkage guarantees
- [ ] Headless validate/run loop: null device, validation harness, automated render-image regression
- [ ] Data-first game definition: schemas for scene/manifest/gameplay formats, loader, headless validation with explaining diagnostics
- [ ] Agent SDK facade: simplified stable C++ API over the engine plus game templates, with compatibility guarantees
- [ ] Converters plus agent-legible asset library: external-format converters with diagnostics, curated versioned catalog with machine-readable metadata
- [ ] Machine-readable contracts: API reference, JSON schemas for every file format, examples-as-contracts
- [ ] Proof: an agent-built complete mini-game (RTS scenario) produced through the agent path alone

### Out of Scope

- Linux port — after the verified Windows `0.7.0` slice, per roadmap sequence
- Full editor — follows the validated runtime model, not before it
- Baa gameplay scripting — gated behind the admission checklist; data-first scripting now, Baa after the gate passes
- Audio subsystem — only when agent-made games require sound (core-value razor)
- DirectX/Vulkan renderers — OpenGL 3.3 core baseline stands
- Hand-authored C++ as the primary consumer path — the agent path is the design target; human ergonomics follow from the same contracts

## Context

- Brownfield: `0.6.0-pre-alpha` baseline per `docs/ROADMAP.md` stabilization list (July 2026); fresh codebase map in `.planning/codebase/` (2026-09-05).
- First proving game is a Ruqoom RTS; the P3 RTS slice (components, terrain, scene extensions) becomes the proof vehicle rather than the end goal.
- `vendor/glad` is the sole approved bundled third-party runtime library; no package-manager dependencies.
- Supported toolchain is MSYS2 UCRT64 with MinGW-w64 GCC; Clang is validated. Windows-only until the slice is verified.
- The engine already exhibits agent-friendly instincts (deterministic serialization with diagnostics, transactional imports with rollback, stale-handles-to-null, fail-visibly test culture) — the project extends that philosophy into a deliberate platform.

## Constraints

- **Tech stack**: C++17, OpenGL 3.3 core / GLSL 3.30, Windows-only until the vertical slice is verified — shifting these needs explicit discussion
- **Ownership**: `Engine/` owns only Core, Graphics, Win32/WGL Platform; foundational types, math, input, codecs, model parsing, font, text, UI stay in `Libraries/`; game semantics stay out of `Pyramid::Engine` — why: independent testing and packaging
- **Dependencies**: new runtime middleware and package-manager dependencies prohibited by default — why: reproducible, self-owned stack
- **API discipline**: no required interface methods with silent no-op defaults; every public symbol implemented, removed, or documented as explicit failure — why: agents amplify whatever the codebase is

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Both interfaces (data-first + Agent SDK), data-first first | Scene v2 + manifests already exist; data needs no toolchain in the agent loop | — Pending |
| Contracts first, restructure on evidence | Machine-readable contracts are fast; structural surgery guided by exposed pain | — Pending |
| Headless validate/run loop before agent surface | The entire agent iteration loop depends on windowless, GPU-less runs | — Pending |
| Finish + freeze core before SDK stability promises | Agents cannot build on shifting sand (open P0 correctness items) | — Pending |
| Data-first scripting now, Baa after the admission gate | Don't hold the vision hostage to the language project | — Pending |
| RTS slice as proof vehicle | Rescues the current roadmap instead of discarding it | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-09-05 after initialization*
