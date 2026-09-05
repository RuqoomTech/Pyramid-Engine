# Feature Research

**Domain:** Agent-first / data-driven C++17 game engine platform (Pyramid Engine)
**Researched:** 2026-09-05
**Confidence:** HIGH for table stakes (convergent across Godot/Unity/Unreal/Bevy/O3DE + 2026 agent-tooling standards); MEDIUM for differentiator payoffs (fewer shipped agent-first engines to copy, reasoning from GameBench evidence + MCP ecosystem fragmentation).

## Feature Landscape

### Table Stakes (Users Expect These)

The "user" here is an external AI agent given only agent-facing docs and a headless loop. Missing any of these, the agent cannot close its build→validate→fix loop and the platform feels incomplete. Every item below exists in at least two mature engines.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Text-based scene definition format with stable IDs | Agents (and VCS) can only diff, patch, and merge text. Godot TSCN (`format=3` + `uid://` stable IDs), Unity YAML scenes, Bevy BSN all converge here; binary-only scenes are agent-hostile. | MEDIUM | Pyramid already has deterministic v2 entity/component serialization with stable IDs — the gap is publishing a frozen schema + JSON Schema files and a strict loader, not inventorizing a format. |
| JSON Schemas for every file format (scene, manifest, gameplay) | Machine-readable contracts are how agents validate before running. OpenAPI/JSON-Schema is the established discovery layer (Fern auto-generates `llms.txt` + specs from one source of truth; `agents.json` builds on OpenAPI). | LOW | One schema source generates docs, validation code, and examples. Pyramid manifests + scene v2 are the first two schemas. |
| Headless validate/run loop (no window, no GPU) | The entire agent iteration loop depends on it — Godot `--headless --import`, Unity `-batchmode -nographics`, Unreal automation, `godot --headless` GUT/GdUnit CI runs. No headless = no unattended agent. | HIGH | Pyramid needs a null display/audio/renderer device plus fixed-timestep sim decoupled from rendering. This is the critical-path feature everything else hangs off. |
| Validation harness with explaining diagnostics + nonzero exit codes | Agents fix what they can localize. Established pattern: structured reports (glTF-Validator JSON report, NVIDIA USD Asset Validator rules, `usdchecker`), JUnit XML for CI, exit `0`=pass / `1`=fail / `2`=bad-args (godot-qa convention). Pyramid's culture already demands actionable, fail-visibly diagnostics. | MEDIUM | Diagnostics must cite file + line + offending value + how to fix, and distinguish content errors from engine bugs. Reuse the scene-serialization diagnostic style already in the codebase. |
| Scenario-as-contract playtest files (JSON scene + scripted steps + asserts) | godot-qa proves the pattern: a JSON scenario (scene to load, input replay, node/text asserts, screenshot diff) that the agent writes during development and CI replays forever. GameBench (Oxford/Rosebud, Jan 2026) shows LLMs are strong on scaffolding but weak on stateful mechanics over time — static checks are insufficient; behavioral replay is the check that matters. | MEDIUM | Pyramid's headless runner should execute scenario files natively. The RTS proof scenario doubles as the first regression gate. |
| External-format converters with diagnostics (bounded, transactional) | No engine owns every authoring tool. Assimp loads 40+ formats; glTF→USD/FBX→USD CLI paths are all headless-scriptable; O3DE asset processor and NVIDIA's converter validate at each pipeline hand-off gate. Agents ingest CC0 packs in FBX/OBJ/glTF — conversion must be scriptable and loud on failure. | MEDIUM | Pyramid already does transactional OBJ→mesh/texture/material publication with rollback — extend the pattern (bounded, fuzzed, allocation-limited) to glTF-first, keep ownership rules (CPU parsing in `Pyramid::Model`, no graphics dependency). |
| Simplified stable SDK facade over the full engine | Agents drown in full engine surface. The convergent pattern is a small versioned tool/API surface instead of raw internals (cf. triforge0 godot-mcp v1.0: 64 tools in 12 families with extension SDK; Unreal Lyra/Cropout as opinionated starting points). Stability promises require a frozen core first. | HIGH | Pyramid Agent SDK = narrow C++ facade + game templates with semver/compat guarantees. Must wait for P0 core freeze (compute-dispatch, shadow-map-array, occlusion, texture-format decisions). |
| Game templates + examples-as-contracts | Every engine onboards via templates (UE Lyra, Cropout, starter templates; Unity/GDevelop RTS templates). For agents, templates are copy-modify starting points that must compile and run — verified by CI, never stale prose. | LOW | Pyramid's `Examples/BasicGame` + RTS reference are the seeds; add one minimal agent-game template (scenario + scene + manifest + catalog refs) validated by the headless loop on every commit. |
| Curated asset catalog with machine-readable metadata | Agents cannot browse art sites; they need a versioned catalog with license/format/tags metadata per entry (the Free-Game-Dev-Assets catalog front-matter pattern: `id/name/license/formats/tags/verified`). Kenney (CC0, 270+ packs, GLB/PNG/WAV) and Quaternius (CC0, glTF/FBX/OBJ/Blend, rigged + animated) are the proven CC0 sources with consistent styles. | LOW | Start with a small pinned subset (one character kit, one environment kit, one UI pack, one audio pack) + `catalog.json` (content hashes, licenses, format, poly/texture budgets). Full-catalog mirroring is explicitly deferred. |
| Render-image regression (screenshot diff with tolerance) | Logic tests miss visual breakage; screenshot-comparison with 1–5% tolerance against committed baselines is standard practice (godot-qa visual mode via xvfb+Mesa; CI-to-CI byte-stable baselines). Pyramid roadmap already calls for automated render-image regression. | MEDIUM | Software-GL/xvfb-class path for CI; tight thresholds only on UI/deterministic scenes; deliberate visual changes update baselines intentionally. Renderer changes still require human visual inspection (smoke tests are not pixel validation). |
| Machine-readable API docs (`llms.txt` + full reference) | 2026 standard pair: `/llms.txt` index + `llms-full.txt` concatenated reference; single source of truth (OpenAPI/Fern Definition) generating human docs, Markdown, and schemas together so nothing drifts. Agents directed to machine-readable sources beat training-data guessing. | LOW | Cheap, high-leverage, do early. Every doc page gets a `.md` URL; version the docs with the SDK. |
| Minimum viable test-suite shape (launch + scene-load + roundtrip) | Industry 80/20: launch test (reach menu headless), scene-load test (every level loads), save/load roundtrip test — three tests catch init/reference/serialization regressions that dominate game breakage. Maps directly onto Pyramid's deterministic serialization + transactional imports. | LOW | The agent's proof game ships these three as its scenario files; they become the platform's canonical example of "done." |

### Differentiators (Competitive Advantage)

Nobody ships a complete agent-first native engine yet (Rosebud et al. are browser/codegen platforms; Godot MCP is 5+ fragmented competing servers, no standard). These align with Pyramid's core value — *zero-to-playable with no engine changes, no human help* — and with instincts the codebase already has.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Deterministic fixed-timestep headless sim decoupled from rendering | Agents iterate in milliseconds without a GPU and get bit-reproducible results; determinism turns "it works on my machine" into a checkable property. Most engines bolt headless on late (Godot stall bugs, driver-dependent CI). Built-in from the slice up is rare. | HIGH | Requires sim/render separation discipline; pays off in every downstream feature (scenarios, regression, proof game). |
| Content-addressed immutable GPU resources + generational handles as the agent contract | Agents alias, rebind, and hot-reload constantly; Pyramid's immutable mesh/shader/texture/material caches with stale-handles-to-null make whole bug classes (use-after-free, silent replacement, duplicate uploads) structurally impossible. No mainstream engine exposes this as the authoring contract. | MEDIUM | Already implemented — the differentiator is *documenting and freezing it as agent-facing contract* rather than building it. |
| Data-first scripting (JSON gameplay definitions) before a language | Agents emit structured data far more reliably than code; data-first gameplay defs unblock the agent path now while Baa stays gated behind its admission checklist. No hostage-taking on the language project. | MEDIUM | Schema + loader + validation + scenario coverage; keep the grammar small (spawn rules, win/lose, progression, input bindings). |
| Transactional everything (imports, reloads, scene ops) with rollback | Agent loops are high-volume trial-and-error; partial application poisons subsequent iterations. Pyramid's transactional OBJ publication is the seed — generalizing "apply-or-rollback with diagnostics" across asset reload and scene mutation is a genuine platform edge. | MEDIUM | Builds directly on existing cache/registry patterns; test with fault-injection fixtures. |
| Owned zero-dependency pipelines as machine-checkable contracts | No package-manager deps, `vendor/glad` only, owned PNG/JPEG/OBJ/TrueType decoders with malformed-input + fuzz + allocation-limit tests. An agent can read the entire pipeline (no opaque middleware) and trust bounded behavior — irreproducible in Assimp-sized dependency stacks. | LOW (leverage existing) | Mostly documentation + schema publication over completed work; keep fuzzing as a standing gate. |
| Proof-by-construction: the agent-built mini-game as a standing feature | A checked-in, CI-rebuilt agent-made RTS scenario that any external agent is expected to reproduce is stronger than any doc claim — it is the acceptance test for the whole platform, permanently. GameBench's "evidence in behaviour, not code" points exactly here. | MEDIUM | The P3 RTS slice is the proof vehicle; the scenario files + catalog pins + SDK version that produced it are recorded alongside. |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Full visual editor as the agent interface | "Agents need what humans use" | Agents operate on text/APIs, not GUIs; the editor is explicitly sequenced *after* the validated runtime model. Building it first burns the critical path and produces an interface agents can't drive. | Data-first schemas + headless loop + SDK facade; editor follows the validated model. |
| Live MCP/editor-drive server as the primary agent path | Godot/Blender MCP demos look magical | Ecosystem is fragmented (5+ competing Godot MCP servers, client-specific, version-dependent); stateful editor-driving is flaky in CI and invisible to review. File-based headless loop is deterministic, diffable, committable. | Ship the file-based loop first; a thin MCP wrapper over the *same* CLI/validator later is cheap once the contracts are stable. |
| AI-generated assets at runtime | "Infinite content" | Nondeterministic, unlicensable, untestable against content-addressed caches; violates the owned-pipeline + CC0-catalog strategy and the no-middleware constraint. | Curated pinned CC0 catalog (Kenney/Quaternius) with hashes + budgets; procedural placement via data-first gameplay defs. |
| Full scripting language now (Baa early) | "Agents need code, not data" | Holds the vision hostage to the language project; premature language surface becomes compat debt before the core is frozen. PROJECT.md gates Baa explicitly. | Data-first JSON gameplay scripting now; Baa after the admission checklist passes. |
| Binary-only / editor-internal scene formats | Faster loads, smaller files | Agent-hostile (unpatchable, unmergable) and contradicts deterministic-text diagnostics culture. | Text authoring format (schema-validated) with optional compiled binary cache at import/run time (Godot ESCN→SCN pattern). |
| Silent no-op defaults in the SDK | "Smoother onboarding" | Agents amplify whatever the codebase is: silent no-ops become hallucinated-success loops where the agent believes a feature works. Explicitly banned by Pyramid API discipline. | Every SDK call either does the documented thing or returns an explicit failure with diagnostics. |
| Realtime multiplayer / audio subsystem / new render backends now | "A real engine has these" | Each is a milestone-sized subsystem; PROJECT.md applies the core-value razor (only when agent-made games require it). Audio/DX/Vulkan/Linux all have explicit sequencing. | Data-first hooks where cheap (e.g. audio asset metadata reserved), subsystems admitted only on demonstrated agent-game need. |
| Hand-authored C++ as the primary consumer path | Familiar engine ergonomics | Optimizing for human C++ ergonomics first re-centers the project away from the agent path; human ergonomics should fall out of the same contracts. | Agent path is the design target; human docs/examples consume the identical schemas + facade. |

## Feature Dependencies

```
[Headless validate/run loop]
    └──requires──> [Null device + fixed-timestep sim]
[Scenario-as-contract playtests]
    └──requires──> [Headless validate/run loop]
                       └──requires──> [Text scene format + stable IDs] (exists: v2 serialization)
[Agent SDK facade]
    └──requires──> [P0 core freeze (correctness decisions)]
    └──requires──> [JSON Schemas for scene/manifest/gameplay]
                       └──requires──> [Loader + explaining diagnostics]
[Game template + proof mini-game]
    └──requires──> [Agent SDK facade]
    └──requires──> [Scenario playtests]
    └──requires──> [Curated catalog subset]
                       └──requires──> [Converters with diagnostics]
[Render-image regression]
    └──requires──> [Headless validate/run loop]
    └──enhances──> [Scenario playtests]
[Machine-readable docs (llms.txt + schemas)]
    ──enhances──> [every feature above] (cross-cutting; do first, it's cheap)
[Data-first gameplay scripting]
    └──requires──> [JSON Schemas + loader + validation]
    ──conflicts──> [Baa-first scripting] (sequencing conflict: data now, language after gate)
[Live MCP server] ──conflicts──> [current-phase focus] (defer until contracts stable; thin wrapper later)
```

### Dependency Notes

- **Headless loop requires null device + fixed timestep:** windowless GPU-less runs with reproducible sim are the foundation; nothing agent-facing works without it (Key Decision: headless before agent surface).
- **Scenarios require the headless loop + text scenes:** replay/assert/diff only makes sense against deterministic text-authored content executed without a display.
- **SDK facade requires P0 freeze + schemas:** stability promises on shifting sand (open compute/shadow/occlusion/texture-format items) would be broken on arrival; schemas are the machine-checkable half of the promise.
- **Proof game requires SDK + scenarios + catalog:** the mini-game is the integration test of the whole stack, not a standalone content task.
- **Catalog requires converters:** pinned CC0 packs arrive as FBX/OBJ/glTF; without scripted diagnosed conversion they can't enter content-addressed caches.
- **Docs enhance everything:** `llms.txt`/schemas are LOW cost with multiplicative value — publish early and regenerate from the single source of truth on every change.
- **Data-first vs Baa conflict is sequencing, not substance:** building both now splits the contract surface agents must learn; data-first now keeps one stable target.

## MVP Definition

### Launch With (v1 — agent path viable)

Minimum to validate the core value: an external agent builds a complete mini-game with docs + headless loop alone.

- [ ] JSON Schemas for scene v2 + resource manifests + gameplay defs, with strict loader and explaining diagnostics — the machine-readable contract everything else references
- [ ] Headless validate/run loop (null device, fixed timestep, exit-code contract, JUnit output) — the agent's iteration engine
- [ ] Scenario-as-contract runner (load scene, replay inputs, assert state/text, fail loudly) — behavioral proof, not just static validity
- [ ] Agent SDK facade v1 (narrow, versioned) over the frozen P0 core — the only API agents are told about
- [ ] One game template + curated catalog subset (pinned, hashed, licensed) wired through converters — the copy-modify starting point
- [ ] `llms.txt` + full machine-readable API/format reference generated from the single source of truth — discoverability
- [ ] Proof: agent-built RTS scenario mini-game produced through the agent path alone, with its three canonical tests (launch, scene-load, save/roundtrip)

### Add After Validation (v1.x)

- [ ] Render-image regression at scale (more baselines, tighter thresholds) — trigger: first visual regression escapes scenario asserts
- [ ] Full curated catalog (more kits, budgets, metadata) — trigger: proof game + one more genre need assets beyond the subset
- [ ] glTF-first converter hardening (more of Assimp's long tail, PBR edge cases) — trigger: catalog packs hit conversion gaps
- [ ] Thin MCP/transport wrapper over the stable CLI + validator — trigger: external agents request live tool use *after* contracts are frozen
- [ ] Data-first scripting grammar extensions (progression, AI behaviors) — trigger: second agent game outgrows the v1 grammar

### Future Consideration (v2+)

- [ ] Baa gameplay scripting — why defer: gated behind admission checklist; data-first must prove the loop first
- [ ] Full editor built on the validated runtime model — why defer: follows the runtime, not before it (explicit sequence)
- [ ] Audio subsystem — why defer: core-value razor, only when agent-made games require sound
- [ ] Linux port, DirectX/Vulkan backends — why defer: explicit roadmap sequence after verified Windows slice

## Feature Prioritization Matrix

| Feature | User Value (agent) | Implementation Cost | Priority |
|---------|-------------------|---------------------|----------|
| JSON Schemas + strict loader + diagnostics | HIGH | LOW | P1 |
| Headless validate/run loop | HIGH | HIGH | P1 |
| Scenario-as-contract runner | HIGH | MEDIUM | P1 |
| Machine-readable docs (llms.txt + reference) | HIGH | LOW | P1 |
| Agent SDK facade v1 | HIGH | HIGH | P1 |
| Game template + examples-as-contracts | HIGH | LOW | P1 |
| Converters with diagnostics (glTF-first) | HIGH | MEDIUM | P1 |
| Curated catalog subset + catalog.json | MEDIUM | LOW | P1 |
| Proof mini-game (RTS scenario) | HIGH | MEDIUM | P1 |
| Render-image regression | MEDIUM | MEDIUM | P2 |
| Data-first scripting grammar v1 | HIGH | MEDIUM | P1 (part of schemas row scope) |
| Thin MCP wrapper | LOW (now) | MEDIUM | P3 |
| Full catalog mirror | LOW (now) | MEDIUM | P3 |
| Baa scripting | LOW (now) | HIGH | P3 (v2+) |
| Full editor | LOW (now) | HIGH | P3 (v2+) |
| Audio / new backends / Linux | LOW (now) | HIGH | P3 (v2+) |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | Godot 4.x | Unity / Unreal | Pyramid Approach |
|---------|-----------|----------------|------------------|
| Text scene format + stable IDs | TSCN `format=3` + `uid://`; defaults omitted | Unity YAML scenes; UE DataAssets/uproject text | v2 serialization exists; publish frozen schema + JSON Schema, keep text-authoritative / binary-cache-at-import |
| Headless validate/run | `--headless --import`, GUT/GdUnit headless, godot-qa scenarios; known long-run stalls | GameCI test runner (edit/playmode); UE automation + Gauntlet | Null device + fixed-timestep sim first; scenario runner native; exit-code + JUnit contract |
| Converters + diagnostics | `.godot/imported` pipeline; ESCN compile step | O3DE asset processor; NVIDIA USD validator + `usdchecker`; Assimp 40+ formats | Transactional bounded owned converters (OBJ done → glTF next), rollback + explaining diagnostics, fault-injection tests |
| Asset catalog | Godot Asset Library (mixed licenses) | Fab / Asset Store (commercial) | Pinned CC0 subset (Kenney + Quaternius) + versioned `catalog.json` with hashes, licenses, budgets |
| Agent interface | Fragmented: 5+ competing MCP servers, client-specific | Rosebud-style codegen platforms (browser, not native) | File-based headless loop + schemas + SDK facade; MCP only as thin late wrapper |
| Behavioral proof | godot-qa scenario JSON replayed in CI; GameBench adherence scoring | Playmode tests; Gauntlet automation | Scenario files are first-class contracts; agent-built RTS scenario is the standing acceptance test |
| Machine-readable docs | Docs site + LSP; no standard agent index | API refs; no standard agent index | `llms.txt` + `llms-full.txt` + JSON Schemas from single source of truth, versioned with SDK |

## Sources

- Godot TSCN file format docs (stable `format=3`, `uid://` IDs, ESCN→SCN import compile): docs.godotengine.org; Bevy Scene/BSN docs (docs.rs) — text scene conventions
- godot-qa (headless CI playtest runner: JSON scenarios, input replay, node/text asserts, screenshot diff, exit 0/1/2, JUnit): github.com/youichi-uda/godot-qa
- Bugnet automated-regression-testing guide (launch / scene-load / save-load roundtrip minimum viable suite; Unity GameCI + GdUnit4 headless patterns): bugnet.io/blog
- GameBench (Rosebud × Oxford FLAIR, Jan 2026: LLMs strong on scaffolding, weak on stateful mechanics; evidence-in-behaviour): evals.rosebud.ai
- Godot MCP ecosystem (ee0pdt/Godot-MCP 608★, IvanMurzak/Godot-MCP, triforge0 standard-server v1.0 64-tools/12-families, bradypp) + Blender MCP + StraySpark 2026 MCP overview — fragmentation evidence for file-first sequencing
- Khronos glTF-Validator (JSON validation reports), NVIDIA OpenUSD Exchange Asset Validator + `usdchecker`, usd-convert-asset (agent skills shipped alongside converters), Assimp 40+ formats — converter/diagnostics patterns
- Kenney (CC0, 270+ packs, GLB/PNG/WAV) + Quaternius (CC0, glTF/FBX/Blend, rigged/animated) + Free-Game-Dev-Assets catalog front-matter pattern (`license/formats/tags/verified`) — catalog model
- llms.txt spec v1.7.0 (llmstxt.org), Fern agent-docs guides (single source of truth → docs + Markdown + specs), Cloudflare docs-for-agents, `agents.json` (wild-card-ai, OpenAPI-based) — machine-readable contract standards
- Unreal Lyra / Cropout / starter templates; GDevelop/Unity RTS template anatomy — template + examples-as-contracts model
- Pyramid repo context: `.planning/PROJECT.md` (core value, constraints, key decisions), `docs/ROADMAP.md` (0.6.0-pre-alpha baseline, P0 list, sequencing)

---
*Feature research for: agent-first / data-driven C++17 game engine platform*
*Researched: 2026-09-05*
