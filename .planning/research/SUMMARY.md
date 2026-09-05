# Project Research Summary

**Project:** Pyramid Engine — Agent-First Game Engine
**Domain:** Agent-first / data-driven C++17 game engine platform (Windows-first, OpenGL 3.3, self-owned stack)
**Researched:** 2026-09-05
**Confidence:** HIGH (stack contracts + baseline architecture) / MEDIUM-HIGH (headless + features) / MEDIUM (pitfall ecosystem evidence)

## Executive Summary

Pyramid is a Windows-first C++17/OpenGL 3.3 engine (0.6.0-pre-alpha baseline) becoming an **agent-first platform**: an external AI agent, given only agent-facing docs and a headless validate/run loop, builds a complete game from zero with no engine-code changes and no human help. Every mature engine surveyed (Godot, Unity, Unreal, Bevy, O3DE) converges on the same shape — text-authored scenes with stable IDs, machine-readable schemas, a headless validate/run loop with explaining diagnostics, scenario-as-contract playtests, bounded transactional converters, a small stable SDK facade, game templates, a curated asset catalog, and screenshot regression. Pyramid already owns the hard halves (deterministic v2 scene serialization with stable IDs, immutable content-addressed GPU resources with generational stale-to-null handles, transactional OBJ publication, owned PNG/JPEG/TrueType pipelines, fail-visibly test culture). The work is publishing those instincts as frozen, machine-checkable contracts plus the one missing loop: null-device headless validation.

The recommended approach is **contracts-first, headless-before-surface, data-first-before-language, freeze-before-facade**: (1) finish + freeze P0 core correctness before any SDK stability promise; (2) build the null-device + headless `pyramid-validate`/`pyramid-run` loop before any agent surface; (3) ship JSON Schema 2020-12 contracts + a Pyramid-owned bounded JSON parser/validator + data-first JSON gameplay DSL (no Lua/Baa embedding yet); (4) wrap a narrow versioned `Pyramid::Agent` facade over the frozen core; (5) add glTF-subset converters + pinned CC0 catalog; (6) generate machine-readable docs from the same source the runtime validates; (7) prove it with an agent-built RTS-slice mini-game produced through the agent path alone with `Engine/` read-only. Zero new runtime middleware throughout — `vendor/glad` stays the sole bundled runtime; Python is dev/CI-only.

The key risks are all closability risks for the agent loop, not feature risks: promising SDK stability over shifting P0 internals (Dispatch-is-log-only, single-cascade shadows, no-op occlusion flag, unmapped texture formats, framebuffer TODOs) locks bugs in as features via Hyrum's Law; a single "headless mode" used for both logic and visuals lies to agents (null skips registration, blank screenshots pass silently); hand-maintained schemas/docs rot within weeks and agents trust them absolutely; silent no-ops and unspecified defaults get amplified into confident wrong games at scale; untrusted agent-authored content (paths, malformed media, injected instruction text) reaches parsers and harnesses built when artists were insiders. Mitigation is structural, not advisory: freeze as an SDK entry gate, split logic-headless from visual-regression tiers with loud failures, generate-or-gate every contract in CI (examples execute or they don't ship), enforce no-silent-no-op + facade-only packaging + path-jailing + allocation limits with fixtures, and keep every validation gate blocking.

## Key Findings

### Recommended Stack

JSON (RFC 8259, UTF-8) is the single canonical encoding for every agent-authored artifact — game definitions, scene docs, manifest refs, gameplay/trigger docs, catalog index — because agents emit it reliably, every language parses it, it diffs/merges, and it extends Pyramid's deterministic-text philosophy with zero new runtime deps. TOML is flat-config-only, YAML is never (indentation/coercion footguns agents amplify), binary formats are never for authoring. Contracts are JSON Schema Draft 2020-12 (current standard, verified 2026-09-05) shipped as versioned files; the engine implements a bounded Pyramid-owned subset validator (`type/enum/required/properties/items/min-max/anyOf-oneOf/allOf/not/if-then-else/$ref/$defs`, `format` annotation-only) with full-spec Python `jsonschema` cross-check in CI, so the dependency ban holds without sacrificing spec honesty. Parsing is a new Pyramid-owned bounded JSON parser in `Libraries/` (depth/size/allocation limits, line/column + JSON-Pointer diagnostics) — the same class of work as the owned PNG/JPEG/OBJ/TrueType parsers. The iteration loop is two owned headless CLIs (`pyramid-validate` for schema+reference diagnostics with `--strict` and JSON reports; `pyramid-run` for fixed-timestep N-frame null-device simulation with scripted inputs and command-stream hashes) over a new `INullDevice`/null-GL backend — OSMesa is dead (removed Mesa 25.1, Linux-only replacement, no Windows story) and driver-dependent CI is rejected. Gameplay logic is a bounded JSON trigger/expression DSL evaluated fixed-timestep (topics/conditions/actions over components + counters), not embedded Lua — Lua/sol2 waits behind the Baa admission gate. Interchange is glTF 2.0.1 import-only subset via a bounded `Pyramid::Model` parser resolved through the existing transactional `ModelResourceImporter` + caches; Assimp/FBX-SDK/USD are rejected as middleware. Regression is owned WGL pbuffer/offscreen capture + `Pyramid::Image` PNG + threshold compare on GPU machines, command-stream hashes on GPU-less machines. Details: `STACK.md`.

**Core technologies:**
- JSON (RFC 8259/UTF-8, canonical, no version) — every game definition, scene, manifest, gameplay doc — agents emit it reliably, zero new runtime deps.
- JSON Schema Draft 2020-12 (`2020-12`, published 2022-06-16, still current) — machine-readable contracts for every agent-touched format; ships as files, no runtime dep.
- Pyramid-owned bounded JSON parser + schema-subset validator (new, `Libraries/`, C++17, v0.7.0) — in-engine parse+validate without middleware; depth/size/allocation limits + line/column/JSON-Pointer diagnostics.
- `pyramid-validate` + `pyramid-run` headless CLIs (new, 0.7.0) — the agent iteration loop: pure-validate (no GL) + null-device fixed-timestep run with deterministic reports.
- `INullDevice` / null GL backend (new, behind renderer seam, stubs GLAD surface) — real pipeline execution with no window/GPU; pixel truth stays in the separate GPU tier.
- Agent SDK facade (`Pyramid::Agent` stable C++17 headers, SemVer, linkage-tested) — small stable escape hatch over Entity/scene-v2, generational handles, action contexts; data-first remains primary.
- Data-first gameplay DSL (bounded JSON trigger/expression rules, fixed-timestep, schema-validated) — agent gameplay logic without compiling C++ or embedding a language runtime.
- glTF 2.0.1 import-only subset (spec 2.0.1/ISO 12113:2022, still current) — converter input alongside OBJ/MTL; runtime formats stay owned.
- Owned snapshot regression (WGL pbuffer + `Pyramid::Image` PNG + per-pixel/perceptual-hash threshold) — render regression with zero new middleware.

### Expected Features

The "user" is an external agent with only docs + headless loop. Table stakes are whatever lets it close build→validate→fix unattended; every item exists in ≥2 mature engines. Differentiators are where Pyramid's already-built invariants (determinism, immutability, transactional everything, zero-deps) become the published agent contract — leverage, not new build. Anti-features are the attractive traps: visual editor first, live MCP as primary path, runtime AI assets, Baa-now, binary scenes, silent no-op defaults, premature multiplayer/audio/backends. Details: `FEATURES.md`.

**Must have (table stakes):**
- Text scene definitions with stable IDs + frozen JSON Schemas for scene/manifest/gameplay — agents can only diff/patch/merge text; schemas are the pre-run check.
- Headless validate/run loop (null device, fixed timestep, exit-code + JUnit contract) — the critical-path iteration engine; nothing agent-facing works without it.
- Validation harness with explaining diagnostics (file + line + value + fix hint; content-errors vs engine-bugs separated) — agents fix what they can localize.
- Scenario-as-contract runner (JSON: scene + input replay + state/text asserts + screenshot diff) — behavioral proof; static checks are insufficient per GameBench 2026.
- Bounded transactional converters with diagnostics (glTF-first, OBJ done) — scripted CC0 ingestion with rollback, never silent substitution.
- Stable narrow Agent SDK facade v1 over the frozen P0 core — agents drown in full engine surface; stability promises require the freeze first.
- Game template + examples-as-contracts (minimal agent game compiling/running in CI) — copy-modify starting points that never go stale.
- Curated catalog subset + `catalog.json` (pinned CC0: Kenney + Quaternius; hashes, licenses, budgets) — agents can't browse art sites.
- Machine-readable docs (`llms.txt` + full reference from one source of truth, versioned with SDK) — cheap, multiplicative, do early.
- Canonical three-test shape in the proof game (launch + scene-load + save/roundtrip) — the 80/20 suite that catches init/reference/serialization breakage.

**Should have (competitive):**
- Deterministic fixed-timestep headless sim decoupled from rendering — ms-iteration, bit-reproducible, GPU-less; rare built-in from the slice up.
- Content-addressed immutable GPU resources + generational handles as the published agent contract — whole bug classes structurally impossible; already implemented, needs freezing + documenting.
- Data-first JSON gameplay scripting before a language — structured data agents emit reliably; unblocks now without hostage-taking Baa.
- Transactional everything (imports, reloads, scene ops) with rollback — high-volume trial-and-error without poisoning subsequent iterations.
- Owned zero-dependency pipelines as machine-checkable contracts — agent can read the whole pipeline, trust bounded behavior.
- Proof-by-construction: checked-in CI-rebuilt agent-made RTS scenario — the standing acceptance test for the whole platform.

**Defer (v2+):**
- Render-image regression at scale (beyond initial baselines) — after first visual escape; needs baselines + thresholds first.
- Full catalog mirror, glTF long-tail hardening, MCP thin wrapper, DSL grammar extensions — each gated on a concrete trigger (second genre, conversion gaps, post-freeze live-tool demand, v1 grammar outgrown).
- Baa scripting, full editor, audio subsystem, Linux port, DirectX/Vulkan — explicit PROJECT.md out-of-scope with sequencing reasons (gate, runtime-first, core-value razor, verified-Windows-slice-first).

### Architecture Approach

New layers wrap the proven baseline without restructuring it: Agent Surface (facade + templates + catalog) over Data-First Definition (loader + validator + converters) over Headless Validate/Run Loop (null device + headless host + two-tier harness), all sitting on the frozen baseline (Game loop, stable-ID Entity/scene v2, immutable generational `ResourceRegistry`, manifests, `ModelResourceImporter`, passes; Win32/WGL + OpenGL 3.3 + owned `Libraries/` + glad-only). Five patterns carry the design: facade-over-frozen-core (~15–30 functions, semver, physically separate package), Null-Object backend (`NullDevice : IGraphicsDevice` + `HeadlessWindow : Window`, no readback API), one-way authoritative data (JSON SOT → derived runtime, atomic dependency-ordered loads, never edit derived output), transactional load with explaining diagnostics (validate-all-before-publish, reuse `SceneSerializationDiagnostic` shape), examples-as-contracts + versioned schemas (every schema has a compiling example in CI; explicit version bumps + migration messages). Data flows one way: agent text → `PyramidValidate` (catalog → manifest → scene-v2 → gameplay) → diagnostics-or-N-frame-null-run → submit → GPU pixel tier in CI. Details: `ARCHITECTURE.md`.

**Major components:**
1. Contract layer (`docs/schemas/*.schema.json` + `docs/agent-sdk/*`, versioned, examples-as-contracts in CI) — machine-readable truth; agents read this, never engine headers.
2. `Libraries/PyramidGameDefinition/` (CPU-only, outside engine binary) — `game.json` + scene + manifest + gameplay loader + validator with `GameDefinitionDiagnostic` shape.
3. `NullDevice` (`Engine/Graphics/source/Null/`, implements `IGraphicsDevice`, counts draws, no pixels) + `HeadlessWindow` (`Engine/Platform/source/Headless/`, no Win32) — the headless execution substrate.
4. Headless host + `Tools/PyramidValidate` CLI (windowless `Game` driver, fixed-timestep N-frame tick, scripted input injection, stable text+JSON diagnostics, documented exit codes) — the agent compile-check.
5. Two-tier regression harness (Tier 1 null-logic always-on; Tier 2 GPU-gated offscreen-FBO-vs-PNG with tolerance) — logic proof vs pixel proof, never conflated.
6. `SDK/PyramidAgentSDK/` (stable facade `AgentGame/AgentScene/AgentAssets/AgentDiagnostics`, one-way dep on Engine, independently packaged) — the narrow versioned door to the same engine.
7. `Tools/PyramidAssetConverter/` (CPU structs → `ModelResourceImporter` → caches, transactional, provenance-recorded) + `assets/catalog/` (`catalog.json` + schema) — external bytes in, owned immutable resources out.
8. `Templates/AgentRTS/` (compiling day-zero starting point, CI-built through agent path alone) — the template that doubles as the compatibility guarantee.

### Critical Pitfalls

Nine researched; top five by blast radius. Full set with phases, verification, and recovery in `PITFALLS.md` (plus debt table, integration gotchas, performance traps, security matrix, "Looks Done But Isn't" checklist).

1. **Promising SDK stability over shifting P0 internals** — Hyrum's Law locks observed bugs (log-only Dispatch, single-cascade shadows, no-op occlusion flag, unmapped formats, framebuffer TODO) in as features. Avoid: finish + freeze core as a hard SDK entry gate; implement-or-remove every stub (extend the `CreateDepthTarget`/v1-rejection explicit-failure idiom); written compat contract + CI breaking-change gate (headers + schemas, version bump + migration note required).
2. **Headless/GPU-less fidelity gap (null loop lies)** — null path skips registration (BeamNG/Godot precedents) while pixel-exact compares flicker and blank screenshots pass silently. Avoid: split logic-headless (deterministic, asserts on events/state/exit codes) from visual-regression (real/software GL, CI-captured baselines, 1–5% tolerance); screenshot-under-null is a loud error; `NullDevice` has no readback; audit `Game::run` for render-entangled setup; fix CPU-count-as-perf lies.
3. **Agents amplify every silent no-op and weak contract** — hallucinated APIs (~5% commercial/~22% open), silent edge errors, scope creep, self-confirming tests, silent-default drift (agent invents plausible-but-wrong values), 1.5× correctness debt with disarming clean formatting. Avoid: merciless no-silent-no-op discipline + `PublicApiLinkage` for every SDK symbol; zero-invention contracts (every fillable field has default/frozen-value/stop-and-ask); grounding over defining files + allow-lists + unhappy-path criteria + scope-boundary checks; characterization tests before scaling agent use.
4. **Schema versioning without migration discipline** — v3 extensions (cameras, environment, RTS components, editor metadata) with a rename/required-field/`additionalProperties:false` silently break all agent content; no official JSON Schema compat checker exists. Avoid: written BACKWARD/FULL compat modes; lenient readers + strict-validator-as-separate-step; upcaster per major bump; old-corpus-as-migration-suite + property round-trips; parse-then-publish with missing/stale diagnostics preserved.
5. **Leaky facade (agents reach past the SDK)** — direct `gl*`/Win32, parallel uploads, mutated cached programs/textures/materials, `RenderObject` transforms as authoritative — defeats dedupe, orphans handles, forks the scene graph. Avoid: physically smaller agent include set (own CMake package, internals unreachable); immutability + handle-first as SDK law; facade-only consumer test + banned-include grep in CI; one blessed path per task in the facade guide.

## Implications for Roadmap

Suggested phase structure (7 phases + standing proof). Each phase is sized to deliver a closable agent capability with its own verification gate; order follows the dependency chain identified across all four research files — reordering creates rework.

### Phase 1: P0 Core Freeze + Quality Baseline
**Rationale:** Nothing agent-facing stands on unfrozen ground (PROJECT.md Key Decision; Pitfalls 1+5). SDK promises over log-only Dispatch / single-cascade shadows / no-op occlusion / unmapped formats / framebuffer TODO lock bugs in as features.
**Delivers:** Implement-or-remove proof per P0 item; no public TODO-behind-API remains; warnings-as-errors, sanitizer coverage, parser fuzzing, dead-code removal, `PublicApiLinkage` coverage; CPU-stats honestly labeled (or GPU timers behind `IGraphicsDevice`); multisample verified on OpenGL 3.3 minimum with on-hardware screenshots.
**Addresses:** SDK-stability precondition; "finish + freeze core" + "quality baseline" Active requirements.
**Avoids:** Pitfalls 1 (stability over shifting sand), 5 (silent no-op amplification), and the performance-trap of CPU-counts-as-cost.

### Phase 2: Headless Validate/Run Loop
**Rationale:** The entire agent iteration cycle depends on windowless GPU-less runs (PROJECT.md Key Decision: headless before agent surface). Unlocks testability for everything after; highest-leverage phase in the roadmap.
**Delivers:** `NullDevice : IGraphicsDevice` (no readback API) + `HeadlessWindow : Window` + headless `Game` path (fixed timestep, scripted `InputState`/action injection, `InputConsumptionMask` ordering preserved) + `pyramid-validate`/`pyramid-run` skeleton with stable text+JSON diagnostics and exit-code contract + two-tier harness skeleton (null-logic always-on; GPU-gated pixel tier with CI baselines + tolerance) + written null-contract doc + minimum viable suite (launch-to-menu <60s, load-every-scene, save/roundtrip, deterministic UI screenshots).
**Addresses:** Headless loop, validation harness, scenario-runner skeleton, render-regression scaffolding.
**Uses:** `INullDevice` seam + fixed-timestep + Godot-QA logic/visual split pattern from STACK.md; Null-Object + two-tier patterns from ARCHITECTURE.md.
**Implements:** `Engine/Graphics/source/Null/`, `Engine/Platform/source/Headless/`, `Tools/PyramidValidate` skeleton, `Tests/Regression/` skeleton.
**Avoids:** Pitfall 4 (fidelity gap); debt shortcuts "one headless mode" and "pixel-exact compare".

### Phase 3: Data-First Game Definition + Schemas
**Rationale:** Data-first is the primary agent path ("both interfaces, data-first first"; Baa gated). Needs Phase 2's harness for testing and publishes the versioning policy every later surface inherits. Contracts-first exposes real pain to guide surgery.
**Delivers:** `docs/schemas/` v0 (scene-v2 + manifest + gameplay + catalog-entry schemas, `$id`-versioned, LLM-readable descriptions/constraints/examples) + `Libraries/PyramidGameDefinition` loader + validator (dependency-ordered catalog → manifest → scene-v2 → gameplay; transactional parse-then-commit; `GameDefinitionDiagnostic` with line+key+rule+fix; missing-vs-stale generation separation; hierarchy validation) + bounded JSON trigger/expression DSL v1 (spawn/win-lose/timers/counters/input-bindings; evidence-budgeted, no `if`/`loop`/`eval` sprawl) + versioning policy (BACKWARD default, upcaster-per-major, old-corpus migration suite, compat CI gate) + `llms.txt` skeleton from the single source of truth.
**Addresses:** Schemas + strict loader + diagnostics; data-first scripting v1; machine-readable-docs skeleton; versioning discipline.
**Uses:** Owned JSON parser + subset validator + DSL from STACK.md.
**Implements:** `Libraries/PyramidGameDefinition/`, `docs/schemas/`, DSL evaluator.
**Avoids:** Pitfalls 3 (versioning breaks), 7 (expressiveness trap → shadow language), 6 (stale contracts — enforcement habits start here: schemas + code change together, examples execute).

### Phase 4: Agent SDK Facade + Game Template
**Rationale:** Facade methods are thin wrappers over load/validate/run flows proven in Phases 2–3; stability promises are only writable after the Phase 1 freeze. Entry-gated on Phase 1, depends on 2+3.
**Delivers:** `SDK/PyramidAgentSDK` (~15–30 functions: `LoadDefinition`, `Spawn/Query`, `Acquire`, `RunHeadless`, `Explain`) as its own CMake package with semver + written compat contract + `PublicApiLinkage` coverage + facade-only consumer test + banned-include CI audit + one blessed path per task in the facade guide + `Templates/AgentRTS` minimal template (scenario + scene + manifest + catalog refs) that passes `PyramidValidate` on day zero and blocks SDK/schema release if it regresses.
**Addresses:** Agent SDK facade v1; game template + examples-as-contracts seed.
**Uses:** Facade-over-frozen-core pattern; immutability + handle-first SDK law (`Meshes()`/`Shaders()`/`Textures()`/`Materials()`, no mutation, stale→null).
**Implements:** `SDK/PyramidAgentSDK/`, `Templates/AgentRTS/`.
**Avoids:** Pitfalls 1 (re-entry of freeze gate), 2 (leaky facade), 5 (hallucinated surface — narrow + tested).

### Phase 5: Converters + Curated Asset Catalog
**Rationale:** Reuses Phase 3 loader/diagnostic idioms; parallelizable with Phase 4 once Phase 3 lands. Pinned CC0 packs arrive as FBX/OBJ/glTF — without scripted diagnosed conversion they can't enter content-addressed caches.
**Delivers:** Owned glTF-2.0.1 subset importer in `Pyramid::Model` (positions/normals/UVs/indices + material subset; unknown extensions ignored with diagnostics) → `ModelResourceImporter` transactional publication (no parallel upload paths) + `Tools/PyramidAssetConverter` with fail-fast line-keyed actionable diagnostics, explicit-only normalization (units/axis/V-flip/color-space/sampler declared + provenance-recorded; content key = source hash + converter version + flags), path-jailing to asset root (no absolutes, no `..` escape), five-test-types per importer + very-large fixtures + fuzz/asan + rollback tests + `assets/catalog/` with `catalog.json` (pinned subset: one character kit, one environment kit, one UI pack, one audio-metadata pack; id/version/kind/hash/license/budgets) + catalog JSON schema.
**Addresses:** Converters with diagnostics; curated catalog subset.
**Uses:** glTF-subset + owned-pipeline strategy from STACK.md.
**Implements:** `Pyramid::Model` glTF subset, `Tools/PyramidAssetConverter/`, `assets/catalog/`.
**Avoids:** Pitfall 8 (cryptic/silent-normalization converters); Pitfall 9 vectors (traversal, OOM, cache poisoning).

### Phase 6: Machine-Readable Contracts (hardening)
**Rationale:** Comes after the surfaces it describes so generation sources exist (loader/validator/facade code), but enforcement habits were established in Phase 3. Small surface, multiplicative value — the last thing before the proof so the proof runs against frozen contracts.
**Delivers:** Generated-not-duplicated API reference + JSON Schemas (schema files as build artifacts of validator code where feasible) + three CI gates (examples-validated-against-schemas every PR; validator-as-contract-test-runner; spec-diff breaking detection requiring version bump + migration note) + versioned `llms.txt`/`llms-full.txt` + per-surface changelog with `deprecated` machine-readable flags + contract-freshness dashboard + zero unexecuted snippets in agent docs.
**Addresses:** Machine-readable contracts Active requirement; docs-discoverability table stakes.
**Avoids:** Pitfall 6 (stale contracts); Pitfall 3 recurrence (breaking-diff gate).

### Phase 7: Proof — Agent-Built RTS-Slice Mini-Game Through Agent Path Alone
**Rationale:** Integration test of the whole stack, not a content task; must run last with the strictest criterion. Validates the core value directly.
**Delivers:** Complete RTS-scenario mini-game (spawn rules, win/lose, progression, input bindings via DSL v1 + catalog pins + SDK version recorded alongside) built by a fresh agent with only agent docs, `Engine/` read-only in CI, zero engine-code changes; ships its three canonical tests (launch, scene-load, save/roundtrip) as scenario files that become permanent regression gates; paired logic-green + visual-green (GPU tier) required; harness privilege audit + traversal/injection probes green before the run.
**Addresses:** Proof Active requirement; standing acceptance test differentiator.
**Avoids:** All false-done modes via the "Looks Done But Isn't" checklist (facade-only proof, upcaster proof, null-contract proof, tolerance proof, executed-examples proof, five-test-types proof, zero-engine-changes proof, hardware-verification proof, harness-security proof).
**Uses:** Everything above; RTS slice as proof vehicle per PROJECT.md (rescues current roadmap instead of discarding it).

### Phase Ordering Rationale

- **Freeze → Harden → Loop → Data → Facade → Converters → Contracts → Proof** follows the dependency chain: schemas need no code but guide surgery; the null loop needs only existing `IGraphicsDevice`/`Window` interfaces and unlocks all later testing; the loader needs schemas + harness; the facade needs proven load/validate/run flows + frozen core; converters need loader idioms; contracts need implemented surfaces to generate from; proof needs everything with engine locked read-only.
- **Grouping keeps each phase's verification closable in isolation:** Phase 1 gates on implement-or-remove + sanitizers; Phase 2 on logic+visual suites + null-contract doc; Phase 3 on migration corpus + DSL-evidence log; Phase 4 on facade-only consumer test; Phase 5 on diagnostic-actionability + rollback + traversal probes; Phase 6 on freshness dashboard + zero-unexecuted-examples; Phase 7 on agent-alone reproduction.
- **Pitfall avoidance is sequenced, not bolted on:** the highest-blast-radius traps (frozen-ground, fidelity gap, amplification surface) are structually eliminated in Phases 1–2 before any surface agents touch exists; versioning/hardening traps are caught at creation (Phases 3–5) rather than audited later; staleness/false-done traps get CI gates (Phases 6–7) that persist beyond the milestone.

### Research Flags

Phases likely needing deeper research during planning (`/gsd-plan-phase --research-phase <N>` recommended):
- **Phase 2 (Headless loop):** MEDIUM — null-device seam design against Pyramid's actual `IGraphicsDevice`/`Game::run` entanglement (render-frame-skipped registration audit), fixed-timestep determinism, software-GL-vs-GPU-node choice for the visual tier on Windows CI, tolerance policy for PNG compares. Codebase-specific; survey patterns exist but the seam is bespoke.
- **Phase 3 (Data-first + DSL):** MEDIUM — DSL grammar boundary (what the RTS slice provably needs vs Baa-gated constructs), schema-subsetValidator parity testing against full-spec `jsonschema`, upcaster/versioning policy details. Needs evidence-driven scoping per planning session.
- **Phase 4 (SDK facade):** MEDIUM — exact facade function list (~15–30) from template use cases, semver/alias-window compat policy wording, facade-only packaging boundaries in CMake. Small surface but every addition is a promise.
- **Phase 5 (Converters + catalog):** MEDIUM — glTF subset scope (which material/extension edge cases the pinned catalog actually exercises), content-key/provenance design, CC0 pack pinning choices + license verification. Format-fuzzing depth question.

Phases with standard patterns (skip research-phase unless surprises emerge):
- **Phase 1 (Freeze + baseline):** well-established (warnings-as-errors, ASan/UBSan, fuzz harnesses, linkage tests); the P0 item list is already enumerated in CONCERNS.md — execution, not research.
- **Phase 6 (Contracts hardening):** standard generate-and-gate tooling (example validation, breaking-diff discipline, `llms.txt` conventions); apply established patterns.
- **Phase 7 (Proof game):** integration exercise of already-built surfaces; needs harness-privilege review (checklist) rather than new research.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Contract formats verified against primary specs (JSON Schema 2020-12 current status, glTF 2.0.1/ISO 12113, TOML/YAML spec comparison, OSMesa removal upstream); dependency-ban fit checked against repo rules; headless loop shape MEDIUM-HIGH (Godot/Unity/O3DE/Babylon patterns converge). |
| Features | HIGH (table stakes) / MEDIUM (differentiator payoffs) | Table stakes convergent across Godot/Unity/Unreal/Bevy/O3DE + godot-qa/GameBench/llms.txt standards; differentiator value reasoned from Pyramid's existing invariants + GameBench evidence, fewer shipped agent-first engines to copy. |
| Architecture | HIGH (baseline) / MEDIUM (ecosystem patterns) | Baseline read directly from `.planning/codebase/`, headers (`GraphicsDevice`, `SceneSerializer`, `ResourceManifest`, `Game`), STRUCTURE constraints; new-layer patterns (facade, null device, manifest-driven, headless-primacy) from surveyed engines, adapted to Pyramid seams. |
| Pitfalls | MEDIUM overall (HIGH on codebase grounding) | Every cited P0 stub and fragile area grounded in first-party files (CONCERNS.md, ARCHITECTURE.md, ROADMAP.md, PROJECT.md); ecosystem claims cross-checked (Hyrum/Speakeasy, BeamNG/Godot null issues, Godot-QA split, schema-evolution taxonomy, 2026 harness incidents) with per-source confidence in PITFALLS.md Sources. |

**Overall confidence:** MEDIUM-HIGH — direction and sequencing are well-supported; per-phase API/grammar/scope details need planning-time evidence (RTS-slice needs, catalog pins, facade function list).

### Gaps to Address

- **Facade function list + compat wording:** research proposes ~15–30 functions and frozen-surface + alias-window policy but the exact signatures need template use cases. Handle in Phase 4 planning: derive from `Templates/AgentRTS` needs, write the compat contract first, then code.
- **DSL grammar boundary:** v1 scope (spawn/win-lose/timers/counters/bindings) is proposed; the RTS proof vehicle may demand more. Handle in Phase 3 planning with an evidence log: each capability needs a failing scenario + loader + validator + diagnostics + example + test shipped together; language-smelling constructs wait for the Baa gate.
- **Visual-tier CI substrate on Windows:** tolerance policy + CI-baseline workflow are settled, but the GPU-vs-software-GL runner choice for Windows CI needs Phase 2 planning validation (no Windows software-GL story exists today; GPU-gated lane is the default assumption).
- **Catalog pins + converter subset edge:** which CC0 packs and which glTF corners matter is unknown until packs are pinned. Handle in Phase 5 planning: pin first, then scope the importer subset to what the pins exercise, with diagnostics for everything ignored.
- **GPU-timing vs CPU-count stats:** whether to add timer queries behind `IGraphicsDevice` or rename stats honestly is a Phase 1 decision needing a small spike during planning.
- **No official JSON Schema breaking-change checker:** the compat gate must be project-built (oasdiff-style discipline for schemas + headers). Handle in Phase 3/6 planning as a CI script task, not a dependency search.

## Sources

### Primary (HIGH confidence)
- json-schema.org Specification page + Draft 2020-12 docs (published 2022-06-16; "current version is 2020-12" verified 2026-09-05) — contract standard.
- Khronos glTF 2.0.1 spec registry (v2.0.1, 2021-10-11) + ISO/IEC 12113:2022 recognition — interchange target.
- Mesa OSMesa removal (upstream MR 33836, Mesa 25.1, 2025) + OSMesa docs — NOT-OSMesa rationale.
- Pyramid first-party ground truth (read directly): `.planning/PROJECT.md`, `docs/ROADMAP.md`, `AGENTS.md`, `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/STRUCTURE.md`, `.planning/codebase/CONCERNS.md`, `Engine/Graphics/include/Pyramid/Graphics/{GraphicsDevice,Scene/SceneSerializer,Resources/ResourceManifest}.hpp`, `Engine/Core/include/Pyramid/Core/Game.hpp`, `Tests/TestGraphicsDevice.hpp`, `docs/Architecture.md`.
- llms.txt spec v1.7.0 (llmstxt.org) — machine-readable docs standard.

### Secondary (MEDIUM confidence)
- Godot `--headless` + dedicated-server docs, headless-agent skill-kit loop, Godot CI export-test practice; Godot TSCN (`format=3`, `uid://`) + ESCN→SCN compile; Godot MCP ecosystem fragmentation (5+ competing servers; triforge0 64-tools/12-families); Godot headless issues (dummy-storage nulls, RID leaks, proposal #1760).
- Unity Graphics Test Framework (reference/actual/diff + tolerances) + GameCI test-runner patterns; Unreal Lyra/Cropout/starter templates + automation/Gauntlet; O3DE Atom screenshot + prefab/JSON-serializer notes.
- godot-qa runner (JSON scenarios, input replay, asserts, screenshot diff, exit 0/1/2, JUnit, logic-vs-xvfb-visual split, screenshot-under-headless is explicit error); Bugnet minimum-viable-suite guide (launch/scene-load/roundtrip); GameBench (Rosebud × Oxford, Jan 2026: scaffolding-strong, stateful-mechanics-weak; evidence-in-behaviour).
- Khronos glTF-Validator JSON reports; NVIDIA OpenUSD Exchange validator + `usdchecker` + `usd-convert-asset`; Assimp 40+-format scope (as interchange breadth reference, rejected as middleware); TOML v1.0.0 vs YAML 1.2 comparisons; Babylon NullEngine deterministic lockstep; Nebulite JSON rulesets + Genie Fusion JSON-schema+Lua layering (DSL-now-language-later precedent).
- Kenney (CC0, 270+ packs, GLB/PNG/WAV) + Quaternius (CC0, glTF/FBX/Blend, rigged/animated) + Free-Game-Dev-Assets catalog front-matter pattern; Fern/OpenAPI single-source-of-truth agent-docs guides; Hyrum's Law; Speakeasy API/SDK drift layers; Confluent Schema Registry evolution + jsonic migration guide (BACKWARD/FULL taxonomy); BeamNG null-GFX registration-skip thread; Vulkan CI render-validation tolerance guidance; AI-code failure-class studies (hallucination rates, redundancy, reviewer asymmetry); brownfield-agent analyses; OWASP prompt-injection + 2026 harness-incident advisories (Novee/CSA); contract-first drift controls (oasdiff/Prism/Dredd patterns); asset-pipeline guides (explicit normalization, content-keyed caching, single-writer CI).
- Flax `GPUDeviceNull`, Renegade headless-vs-graphics test split, droids-engine headless-primacy, Hyperscape manifest/data-manager architecture, Unreal ALIS external-data one-way pattern, SummerEngine/scena headless-pixel-trap docs.

### Tertiary (LOW confidence — pattern examples only, needs validation if relied upon)
- Iron Curtain `api-stability.md`, Fundamental Engine `api-stability.md`, Capybara `SDK_FACADE.md`, Dojo/Origo adapter layering, SaveCompat migration-corpus pattern, GUT-on-CI warmup-split practitioner report, silent-default-drift practitioner account, hallucinated-tool silent-crash article, DDD pipeline-shape overview.

---
*Research completed: 2026-09-05*
*Ready for roadmap: yes*
