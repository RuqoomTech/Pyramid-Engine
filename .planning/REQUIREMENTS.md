# Requirements: Pyramid Engine — Agent-First Platform

**Defined:** 2026-09-05
**Core Value:** An external AI agent, given only the agent-facing docs and a headless validate/run loop, can build a complete working game from zero — no engine-code changes, no human help.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases. The "user" below is the external AI agent working through the agent path.

### P0 Core Freeze

- [x] **FRZ-01**: Agent builds never hit a log-only compute-dispatch stub — dispatch is implemented or removed from the command model
- [ ] **FRZ-02**: Deferred shadow-map-array binding is complete (or explicitly removed with documented rationale)
- [x] **FRZ-03**: Occlusion culling uses a supported technique or the setting is removed (no placeholder flag)
- [x] **FRZ-04**: Every advertised texture format is mapped, or unsupported enum values are removed
- [ ] **FRZ-05**: Backend-neutral framebuffer binding is complete, including depth-texture creation through the texture interface

### Quality Baseline

- [ ] **QLT-01**: CI builds warning-clean with warnings-as-errors on all supported toolchains
- [ ] **QLT-02**: AddressSanitizer/UndefinedBehaviorSanitizer coverage runs in a compatible-toolchain CI job
- [ ] **QLT-03**: Parser fuzzing, malformed/truncated fixtures, and allocation-limit coverage for image, model, and font parsers
- [ ] **QLT-04**: Public-API linkage tests cover every new agent-facing symbol; dead code on the agent path is removed

### Headless Loop

- [ ] **HDL-01**: Agent can validate game definitions via a headless CLI with no window and no GPU, following the exit-code contract (0 = pass, 1 = fail, 2 = bad args)
- [ ] **HDL-02**: Null graphics device and headless window implement the existing device/window interfaces; fixed-timestep simulation runs decoupled from rendering with reproducible results
- [ ] **HDL-03**: Validation harness emits structured reports (JSON + JUnit XML) citing file, line, offending value, and how to fix, distinguishing content errors from engine bugs
- [ ] **HDL-04**: Agent can run a game headless with scripted inputs and get deterministic state outcomes

### Scenario Runner

- [ ] **SCN-01**: Agent can author scenario files (scene to load, scripted steps, state/text asserts) that the headless runner executes natively
- [ ] **SCN-02**: Scenario failures cite the failing step with expected vs actual values and exit nonzero
- [ ] **SCN-03**: Proof game ships the three canonical scenario tests: launch, scene-load, and save/load roundtrip

### Data-First Definitions

- [ ] **DAT-01**: JSON Schemas are published for scene v2, resource manifests, and gameplay definitions
- [ ] **DAT-02**: Strict loader rejects invalid definitions with explaining diagnostics (file, line, value, fix)
- [ ] **DAT-03**: Data-first gameplay grammar v1 (spawn rules, win/lose conditions, progression, input bindings) ships with schema, validation, and scenario coverage
- [ ] **DAT-04**: Schema versioning policy is documented and breaking schema changes are gated in CI

### Agent SDK

- [ ] **SDK-01**: Narrow versioned C++ Agent SDK facade exposes only the frozen-core surface agents are told about
- [ ] **SDK-02**: SDK compatibility guarantees are documented (versioning and alias policy)
- [ ] **SDK-03**: Every SDK call either performs its documented effect or returns an explicit failure — no silent no-op defaults
- [ ] **SDK-04**: One minimal agent-game template (scenario + scene + manifest + catalog refs) is validated by the headless loop on every commit

### Converters and Catalog

- [ ] **CNV-01**: glTF-subset converter is bounded and transactional with rollback, emitting line-keyed diagnostics on failure
- [ ] **CNV-02**: Converter performs no silent unit/axis/V-flip handling; content identity includes source hash plus converter version and flags
- [ ] **CAT-01**: Pinned CC0 catalog subset ships with machine-readable catalog metadata (content hashes, licenses, formats, poly/texture budgets)
- [ ] **CAT-02**: Catalog entries convert through transactional publication into the content-addressed caches

### Machine-Readable Contracts

- [ ] **DOC-01**: `llms.txt` index plus full machine-readable API/format reference are generated from a single source of truth and versioned with the SDK
- [ ] **DOC-02**: Examples-as-contracts — every schema and SDK call is covered by a CI-built template or sample
- [ ] **DOC-03**: Contract-drift gates in CI detect breaking spec, schema, or SDK changes

### Render Regression

- [ ] **REG-01**: Screenshot-diff regression with tolerance against committed baselines covers deterministic scenes
- [ ] **REG-02**: Baseline updates are intentional operations, never silent overwrites; renderer changes still get human visual inspection

### Proof Game

- [ ] **PRF-01**: An agent-built RTS scenario mini-game is produced through the agent path alone — no engine-code changes, no human help
- [ ] **PRF-02**: The proof records the scenario files, catalog pins, and SDK version that produced it
- [ ] **PRF-03**: The proof game rebuilds and passes in CI as the standing acceptance test for the whole platform

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Agent Loop Extensions

- **HDL-05**: Thin MCP/transport wrapper over the stable CLI and validator for live tool use
- **REG-03**: Render-image regression at scale (more baselines, tighter thresholds)
- **SCN-04**: Extended scenario asserts (performance budgets, GPU-timing thresholds)

### Content and Scripting

- **CNV-03**: glTF converter hardening (wider format long tail, PBR edge cases)
- **CAT-03**: Full curated catalog beyond the pinned subset
- **DAT-05**: Data-first scripting grammar extensions (progression systems, AI behaviors)
- **DAT-06**: Baa gameplay scripting (gated behind the admission checklist)

### Platform

- **PLT-01**: Audio subsystem (admitted only when agent-made games require sound)
- **PLT-02**: Linux port (after the verified Windows slice)
- **PLT-03**: DirectX/Vulkan renderers
- **PLT-04**: Full editor built on the validated runtime model

## Out of Scope

Explicitly excluded. Documented to prevent scope creep.

| Feature | Reason |
|---------|--------|
| Full visual editor as the agent interface | Agents operate on text/APIs, not GUIs; editor follows the validated runtime model |
| Live MCP/editor-drive server as primary path | Fragmented ecosystem, flaky in CI, invisible to review; file-based loop first |
| AI-generated assets at runtime | Nondeterministic, unlicensable, untestable against content-addressed caches |
| Baa-first scripting | Holds the vision hostage to the language project; gated behind admission checklist |
| Binary-only scene formats | Agent-hostile; text authoring with optional compiled cache instead |
| Silent no-op SDK defaults | Agents amplify them into hallucinated-success loops; banned by API discipline |
| Realtime multiplayer | Milestone-sized subsystem; no demonstrated agent-game need |
| Hand-authored C++ as primary consumer path | Agent path is the design target; human ergonomics follow from the same contracts |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| FRZ-01 | Phase 1 | Complete |
| FRZ-02 | Phase 1 | Pending |
| FRZ-03 | Phase 1 | Complete |
| FRZ-04 | Phase 1 | Complete |
| FRZ-05 | Phase 1 | Pending |
| QLT-01 | Phase 2 | Pending |
| QLT-02 | Phase 2 | Pending |
| QLT-03 | Phase 2 | Pending |
| QLT-04 | Phase 2 | Pending |
| HDL-01 | Phase 3 | Pending |
| HDL-02 | Phase 3 | Pending |
| HDL-03 | Phase 3 | Pending |
| HDL-04 | Phase 3 | Pending |
| SCN-01 | Phase 4 | Pending |
| SCN-02 | Phase 4 | Pending |
| SCN-03 | Phase 4 | Pending |
| DAT-01 | Phase 5 | Pending |
| DAT-02 | Phase 5 | Pending |
| DAT-03 | Phase 5 | Pending |
| DAT-04 | Phase 5 | Pending |
| SDK-01 | Phase 6 | Pending |
| SDK-02 | Phase 6 | Pending |
| SDK-03 | Phase 6 | Pending |
| SDK-04 | Phase 6 | Pending |
| CNV-01 | Phase 7 | Pending |
| CNV-02 | Phase 7 | Pending |
| CAT-01 | Phase 7 | Pending |
| CAT-02 | Phase 7 | Pending |
| DOC-01 | Phase 8 | Pending |
| DOC-02 | Phase 8 | Pending |
| DOC-03 | Phase 8 | Pending |
| REG-01 | Phase 4 | Pending |
| REG-02 | Phase 4 | Pending |
| PRF-01 | Phase 9 | Pending |
| PRF-02 | Phase 9 | Pending |
| PRF-03 | Phase 9 | Pending |

**Coverage:**

- v1 requirements: 36 total
- Mapped to phases: 36
- Unmapped: 0

---
*Requirements defined: 2026-09-05*
*Last updated: 2026-09-05 after initial definition*
