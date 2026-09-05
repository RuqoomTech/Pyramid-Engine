# Roadmap: Pyramid Engine — Agent-First Platform

## Overview

From an unfrozen 0.6.0-pre-alpha baseline to a proven agent-first platform: first freeze P0 core correctness and lock in a quality baseline (nothing stands on shifting sand), then build the headless validate/run loop that unlocks all later testing, layer scenario-as-contract and render-regression verification on top of it, publish data-first game definitions with schemas as the primary agent path, wrap a narrow versioned Agent SDK facade over the frozen core, add converters plus a curated asset catalog, harden machine-readable contracts with CI drift gates, and finally prove the whole stack with an agent-built RTS mini-game produced through the agent path alone — the standing acceptance test.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: P0 Core Freeze** - Finish or explicitly remove every P0 correctness stub
- [ ] **Phase 2: Quality Baseline** - Warnings-as-errors, sanitizers, fuzzing, linkage guarantees
- [ ] **Phase 3: Headless Validate/Run Loop** - Null device, headless CLI, structured diagnostics, deterministic runs
- [ ] **Phase 4: Scenario Runner + Render Regression** - Scenario-as-contract execution plus screenshot-diff gates
- [ ] **Phase 5: Data-First Game Definition + Schemas** - Versioned schemas, strict loader, gameplay grammar v1
- [ ] **Phase 6: Agent SDK Facade + Game Template** - Narrow versioned C++ facade with a CI-validated template
- [ ] **Phase 7: Converters + Curated Asset Catalog** - Transactional glTF ingestion plus pinned CC0 catalog
- [ ] **Phase 8: Machine-Readable Contracts Hardening** - Generated docs, examples-as-contracts, drift gates
- [ ] **Phase 9: Proof — Agent-Built RTS Mini-Game** - Full agent-path build with zero engine-code changes

## Phase Details

### Phase 1: P0 Core Freeze
**Goal**: Agents build on frozen ground — every P0 correctness gap is implemented or explicitly removed
**Mode:** mvp
**Depends on**: Nothing (first phase)
**Requirements**: FRZ-01, FRZ-02, FRZ-03, FRZ-04, FRZ-05
**Success Criteria** (what must be TRUE):
  1. Agent dispatching compute work through the documented command model gets real execution or an explicit removal notice — never a silent log-only stub (FRZ-01)
  2. Agent using deferred shadow-map-array bindings and backend-neutral framebuffer depth targets gets complete documented behavior through the texture interface (FRZ-02, FRZ-05)
  3. Agent enabling occlusion culling gets a supported technique with observable effect, or the setting is gone with documented rationale (FRZ-03)
  4. Agent using any advertised texture format gets a working mapping — no unmapped enum value reaches agent-visible API (FRZ-04)
**Plans**: TBD

Plans:
- [ ] 01-01: TBD during plan-phase

### Phase 2: Quality Baseline
**Goal**: The agent path is guarded by warning-clean builds, sanitizers, fuzzing, and linkage guarantees
**Mode:** mvp
**Depends on**: Phase 1
**Requirements**: QLT-01, QLT-02, QLT-03, QLT-04
**Success Criteria** (what must be TRUE):
  1. Agent-facing CI builds are warning-clean with warnings-as-errors on MinGW GCC and Clang (QLT-01)
  2. Agent can trust memory safety basics: an AddressSanitizer/UndefinedBehaviorSanitizer CI job runs green over engine, libraries, and parsers (QLT-02)
  3. Agent feeding malformed, truncated, or oversized image, model, or font bytes gets an explicit failure diagnostic — never a crash, hang, or silent acceptance (QLT-03)
  4. Every new agent-facing symbol is covered by public-API linkage tests, and dead code on the agent path is removed with the linkage suite green (QLT-04)
**Plans**: TBD

Plans:
- [ ] 02-01: TBD during plan-phase

### Phase 3: Headless Validate/Run Loop
**Goal**: Agents iterate build-to-validate-to-fix with no window and no GPU
**Mode:** mvp
**Depends on**: Phase 2
**Requirements**: HDL-01, HDL-02, HDL-03, HDL-04
**Success Criteria** (what must be TRUE):
  1. Agent runs a headless validate CLI over game definitions with no window and no GPU and gets the exit-code contract (0 = pass, 1 = fail, 2 = bad args) (HDL-01)
  2. Agent runs fixed-timestep simulation decoupled from rendering on the null device/headless window and gets reproducible identical results across runs (HDL-02)
  3. Agent receives structured JSON + JUnit reports citing file, line, offending value, and how to fix, with content errors distinguished from engine bugs (HDL-03)
  4. Agent runs a game headless with scripted inputs and observes deterministic state outcomes (HDL-04)
**Plans**: TBD

Plans:
- [ ] 03-01: TBD during plan-phase

### Phase 4: Scenario Runner + Render Regression
**Goal**: Agents prove behavior with executable scenarios and guarded render baselines
**Mode:** mvp
**Depends on**: Phase 3
**Requirements**: SCN-01, SCN-02, SCN-03, REG-01, REG-02
**Success Criteria** (what must be TRUE):
  1. Agent authors a scenario file (scene to load, scripted steps, state/text asserts) and the headless runner executes it natively (SCN-01)
  2. Agent sees a scenario failure cite the failing step with expected vs actual values and a nonzero exit (SCN-02)
  3. Agent can diff deterministic scenes against committed baselines with tolerance, and baseline updates are intentional operations — never silent overwrites (REG-01, REG-02)
  4. The three canonical scenario tests exist and pass headless: launch, scene-load, and save/load roundtrip (SCN-03)
**Plans**: TBD

Plans:
- [ ] 04-01: TBD during plan-phase

### Phase 5: Data-First Game Definition + Schemas
**Goal**: Agents author complete games as validated data — the primary agent path
**Mode:** mvp
**Depends on**: Phase 4
**Requirements**: DAT-01, DAT-02, DAT-03, DAT-04
**Success Criteria** (what must be TRUE):
  1. Agent reads published JSON Schemas for scene v2, resource manifests, and gameplay definitions and validates definitions offline before running (DAT-01)
  2. Agent submitting invalid definitions gets strict-loader rejection citing file, line, offending value, and fix (DAT-02)
  3. Agent authors spawn rules, win/lose conditions, progression, and input bindings in gameplay grammar v1 and sees them schema-validated with scenario coverage (DAT-03)
  4. Agent relying on a schema version gets a documented versioning policy, and breaking schema changes are gated in CI (DAT-04)
**Plans**: TBD

Plans:
- [ ] 05-01: TBD during plan-phase

### Phase 6: Agent SDK Facade + Game Template
**Goal**: Agents escape data-only work through a narrow stable SDK with a copy-modify starting point
**Mode:** mvp
**Depends on**: Phase 5 (entry-gated on Phase 1 freeze)
**Requirements**: SDK-01, SDK-02, SDK-03, SDK-04
**Success Criteria** (what must be TRUE):
  1. Agent includes only the narrow versioned C++ facade surface and builds a game without reaching past it into frozen-core internals (SDK-01)
  2. Agent reads documented compatibility guarantees (versioning and alias policy) and can rely on them across releases (SDK-02)
  3. Agent calling any SDK function gets its documented effect or an explicit failure — never a silent no-op default (SDK-03)
  4. Agent copies the one minimal game template (scenario + scene + manifest + catalog refs) and sees it validated by the headless loop on every commit (SDK-04)
**Plans**: TBD

Plans:
- [ ] 06-01: TBD during plan-phase

### Phase 7: Converters + Curated Asset Catalog
**Goal**: Agents ingest outside art and browse a pinned catalog that feeds content-addressed caches
**Mode:** mvp
**Depends on**: Phase 5 (parallelizable with Phase 6)
**Requirements**: CNV-01, CNV-02, CAT-01, CAT-02
**Success Criteria** (what must be TRUE):
  1. Agent converts glTF-subset assets through a bounded transactional converter with rollback and gets line-keyed diagnostics on failure (CNV-01)
  2. Agent sees no silent unit/axis/V-flip handling — normalization is declared and content identity includes source hash plus converter version and flags (CNV-02)
  3. Agent browses the pinned CC0 catalog subset with machine-readable metadata (content hashes, licenses, formats, poly/texture budgets) (CAT-01)
  4. Agent publishes catalog entries through transactional publication into the content-addressed caches (CAT-02)
**Plans**: TBD

Plans:
- [ ] 07-01: TBD during plan-phase

### Phase 8: Machine-Readable Contracts Hardening
**Goal**: Agents read docs that cannot go stale — generated from one source of truth, gated in CI
**Mode:** mvp
**Depends on**: Phase 7
**Requirements**: DOC-01, DOC-02, DOC-03
**Success Criteria** (what must be TRUE):
  1. Agent reads the `llms.txt` index plus full machine-readable API/format reference generated from a single source of truth and versioned with the SDK (DOC-01)
  2. Agent finds every schema and SDK call covered by a CI-built template or sample — nothing documented is unexecuted (DOC-02)
  3. Agent attempts touching a breaking spec, schema, or SDK change and sees contract-drift CI gates fire (DOC-03)
**Plans**: TBD

Plans:
- [ ] 08-01: TBD during plan-phase

### Phase 9: Proof — Agent-Built RTS Mini-Game
**Goal**: A fresh agent builds a complete RTS scenario mini-game through the agent path alone
**Mode:** mvp
**Depends on**: Phase 8
**Requirements**: PRF-01, PRF-02, PRF-03
**Success Criteria** (what must be TRUE):
  1. A fresh agent with only agent-facing docs produces a working RTS scenario mini-game with zero engine-code changes and zero human help (PRF-01)
  2. The proof records the scenario files, catalog pins, and SDK version that produced it (PRF-02)
  3. The proof game rebuilds and passes in CI as the standing acceptance test for the whole platform (PRF-03)
**Plans**: TBD

Plans:
- [ ] 09-01: TBD during plan-phase

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 (Phases 6 and 7 are parallelizable after Phase 5)

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. P0 Core Freeze | 0/0 | Not started | - |
| 2. Quality Baseline | 0/0 | Not started | - |
| 3. Headless Validate/Run Loop | 0/0 | Not started | - |
| 4. Scenario Runner + Render Regression | 0/0 | Not started | - |
| 5. Data-First Game Definition + Schemas | 0/0 | Not started | - |
| 6. Agent SDK Facade + Game Template | 0/0 | Not started | - |
| 7. Converters + Curated Asset Catalog | 0/0 | Not started | - |
| 8. Machine-Readable Contracts Hardening | 0/0 | Not started | - |
| 9. Proof — Agent-Built RTS Mini-Game | 0/0 | Not started | - |
