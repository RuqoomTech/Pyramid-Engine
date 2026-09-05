# Pitfalls Research: Agent-First Data-Driven Engine Platform

**Domain:** Agent-first C++17 game engine platform (SDK facades, data-first schemas, headless validate/run loop, machine-readable contracts on top of an existing engine)
**Researched:** 2026-09-05
**Confidence:** MEDIUM (cross-checked web findings) / HIGH (Pyramid codebase grounding from first-party files)
**Project:** Pyramid Engine — Windows-first C++17/OpenGL 3.3 engine becoming an agent-first platform

## Critical Pitfalls

### Pitfall 1: Promising SDK stability over shifting P0 internals

**What goes wrong:**
The Agent SDK facade is published while the engine underneath is still moving. Agents build games against observable behavior that later changes, and every game breaks. This is Hyrum's Law in its purest form: with enough agent users, *all* observable behavior becomes a dependency — sorted-vs-unsorted lists, exact error strings, timing, single-cascade shadow output, log-only compute dispatch. Pyramid has at least five live instances of this hazard today: `CommandBuffer::Dispatch()` records work that is only `PYRAMID_LOG_DEBUG`d and dropped, `DeferredLightingPass` binds only the first shadow cascade despite N cascades being created, `m_occlusionCullingEnabled` defaults to a flag with no algorithm, not all advertised `TextureFormat` values are mapped, and backend-neutral framebuffer binding is a TODO outside the OpenGL renderer (see `.planning/codebase/CONCERNS.md`).

**Why it happens:**
SDK work is visible and exciting; finishing P0 correctness items (compute-dispatch decision, shadow-map-array binding, occlusion decision, texture-format mapping, framebuffer binding) is grind. Teams ship the facade first and plan to "stabilize underneath later." Agents, unlike humans, cannot distinguish documented guarantees from accidental behavior — they copy whatever they observe, including bugs.

**How to avoid:**
1. Finish + freeze core *before* any SDK stability promise (this is already PROJECT.md Key Decision — enforce it, do not soften it).
2. For each P0 item, decide implement-or-remove, never leave a silent stub. The codebase already has the right explicit-failure pattern (`CreateDepthTarget` fails loudly, legacy `LoadScene`/`SaveScene` log rejection, v1 scenes rejected) — extend it to Dispatch and occlusion: either wire them or delete them from the command model/API before the SDK references them.
3. Define the SDK compatibility contract in writing on day one: what is stable, what is experimental, how versions move (frozen-surface + alias-window pattern, cf. Fundamental Engine `api-stability.md`, Iron Curtain mod-API-vs-engine-crates split).
4. Add a CI breaking-change gate: any public-header or schema change that alters observable behavior fails CI unless accompanied by a version bump + migration note (oasdiff-style discipline applied to C++ headers and JSON schemas).

**Warning signs:**
- SDK docs describe behavior that has a `// TODO` or log-only implementation behind it.
- An agent demo "works" but depends on single-cascade shadows, CPU command counts as performance numbers, or multisample resolve that was never verified on a real driver.
- Discussions about "we'll fix the internals after the SDK lands."

**Phase to address:**
Core-freeze phase (P0 verification). The SDK facade phase must *require* the freeze as an entry gate — no facade work starts until Dispatch/occlusion/shadow-array/format/binding decisions are closed and covered by `Tests/PublicApiLinkage.cpp`.

---

### Pitfall 2: Leaky facade — agents reach past the SDK into engine internals

**What goes wrong:**
The SDK is a thin wrapper, but agents discover and use the powerful internals behind it: mutating a cached `ShaderProgram` in place, creating parallel mesh uploads for byte-identical geometry, stuffing per-draw matrices into `Material` identity, treating `RenderObject` transforms as authoritative, calling Win32/WGL or OpenGL directly from game code. This silently defeats content-identity deduplication, orphans generational handles, multiplies GPU memory and compile cost, and forks the scene graph. Pyramid's architecture doc already names both halves of this anti-pattern ("mutating cached GPU resources in place" and "backend calls in UI / game semantics in engine").

**Why it happens:**
Facades that merely re-export internals don't constrain anyone. Agents are reward-driven explorers: if the internal header exists in the include path and solves the immediate problem, they will use it. C++ makes this worse — everything in the include tree is reachable unless physically separated.

**How to avoid:**
1. Make the agent-facing include set *physically* smaller than the engine include set. Ship the SDK as its own CMake package/target with only facade headers; engine internals are not in the agent's include path at all (same packaging discipline that already separates `Libraries/` from `Engine/` and keeps `RTSReference` out of the installed API).
2. Keep the immutability + handle-first rules as SDK law: shared geometry via `ResourceRegistry::Meshes()`, programs via `Shaders()` (never mutate), textures via `Textures()` (color space is identity, reload only transactionally), materials via `Materials()` (per-draw data in command-buffer uniforms). Every alias bind/remap/removal advances the generation; stale handles resolve to null, never to replacement content.
3. Enforce with tests agents cannot argue with: extend `Tests/PublicApiLinkage.cpp` to assert the SDK surface, add a "facade-only" consumer test (`Tests/` consumer pattern already exists) that builds a game including *only* SDK headers, and fail CI if an SDK example includes an engine-internal header.
4. Document the one blessed path per task in the facade guide (Capybara-style `SDK_FACADE.md`: "import the facade, do not open internals") and make every engine-internal header comment point back to the facade equivalent.

**Warning signs:**
- Agent-generated code `#include`s `Engine/Graphics/source/` or `OpenGL/*` paths, calls `gl*` directly, or constructs `Mesh`/`ShaderProgram`/`TextureResource` without going through `ResourceRegistry`/`ModelResourceImporter`.
- Duplicate GPU uploads for identical content appear in resource statistics; handle-stale tests start failing.
- `RenderObject` transforms edited directly instead of via `Entity` + components.

**Phase to address:**
Agent SDK facade phase. Entry dependency: core freeze (Pitfall 1). Verification: facade-only consumer test green; grep audit for banned includes in agent-path examples.

---

### Pitfall 3: Schema versioning without migration discipline

**What goes wrong:**
Scene v2 + versioned resource manifests exist and are good. Then v3 extensions land (cameras, environment, RTS gameplay components, editor metadata — all explicitly pending in `docs/ROADMAP.md` P3.6–P3.9) with a rename here, a new required field there, a tightened `additionalProperties: false` — and every agent-authored scene, manifest, and game template silently breaks. Strict readers turn additive evolution into a breaking change; renames without aliases orphan old content; missing upcasters force agents to hand-edit versioned files they don't understand.

**Why it happens:**
Schema authors think in terms of "the current format" while agents (and players' save files) live in *every* format ever shipped. JSON Schema makes it easy to write a strict schema and hard to notice you just broke backward compatibility. There is no official JSON Schema compatibility checker (json-schema-org GSoC 2026 proposal notes this gap explicitly vs. Protobuf `buf breaking` and Avro Schema Registry), so nothing catches the break unless the project builds the gate itself.

**How to avoid:**
1. Adopt explicit compatibility modes per format and write them down: scene/manifest/gameplay schemas default to BACKWARD (new loader reads all old documents); shared event-style formats use FULL (both directions). Classify every change before shipping: additive optional-with-default = safe; remove/rename/required-addition/type-narrow/add-`additionalProperties:false` = breaking, requires major version + upcaster.
2. Never use `additionalProperties: false` on agent-authored formats unless old readers are proven to reject-unknown-safely; prefer lenient readers (ignore unknown fields) + strict validators as a separate explicit step with explaining diagnostics.
3. Ship an upcaster per version jump (v2→v3 function, not documentation prose), keep every old document as a migration corpus (SaveCompat pattern: old saves become the test suite, each migrated through every declared version with schema validation + progress-preservation checks at each step), and add property-based round-trip tests (generate thousands of random v2 payloads, upcast, validate against v3).
4. Keep Pyramid's existing transactional discipline: parse-then-publish, hierarchy validation (cycles, duplicate/invalid IDs), missing/stale generation diagnostics without silent substitution. Extend `Tests/SceneSerializationTests.cpp` with v3-extension cases *when v3 is defined*, not after.

**Warning signs:**
- A schema PR adds a required field or renames a property with no version bump and no upcaster.
- `additionalProperties: false` appears in a schema that agents write to.
- Old example scenes stop loading after a "minor" format update; agents start hand-patching versioned JSON.

**Phase to address:**
Data-first game-definition phase (schemas + loader + headless validation). The contracts phase then publishes the versioning policy as machine-readable metadata. Verification: old-corpus migration test green; CI compatibility check on schema diffs.

---

### Pitfall 4: Headless/GPU-less fidelity gap — the null loop lies to agents

**What goes wrong:**
The headless validate/run loop becomes the agent's entire world, but it doesn't faithfully represent the real engine. Two failure modes, both observed in the wild: (a) the null path *skips work* — BeamNG's `-gfx null` never fires the frame-loop asset/collision registration so vehicles drive on heightmaps, Godot's dummy storage returns null for mesh queries and leaks RIDs at exit; (b) the visual path is *compared wrong* — pixel-exact image comparison fails on driver/antialiasing noise, or screenshot steps silently produce blank images under `--headless` instead of erroring. Pyramid is maximally exposed here: the runtime is Win32/WGL-only, there is no headless CI rendering today, `scripts/run-smoke.ps1` proves the process runs (not that pixels are right), render statistics are CPU command counts (not GPU timings), and multisample resolve is unverified across drivers.

**Why it happens:**
Teams build one "headless mode" and use it for both logic validation and visual validation. Logic wants determinism and no GPU; visuals want a real (or software-emulated) GPU and tolerant comparison. One mode cannot serve both. Additionally, engine frame loops entangle simulation registration with render frames (the BeamNG root cause), so nulling the renderer silently nulls simulation setup.

**How to avoid:**
1. Split the loop in two with different contracts, following the Godot-QA pattern that works in production: **logic mode** (fully headless, null device, fixed timestep, deterministic — asserts on domain events, scene state, diagnostics, exit codes) and **visual mode** (real rendering via software GL/Mesa or GPU nodes — xvfb + SwiftShader/OSMesa class solution — screenshot diff against baselines with 1–5% tolerance, baselines captured *in CI* not on dev machines since fonts/drivers differ).
2. Make headless-vs-GPU differences *explicit errors, never silent blanks*: a screenshot step under a null device must fail loudly (Godot-QA makes it an explicit error). Audit `Game::run` for render-frame-entangled setup (viewport restore, proxy sync, octree sync, asset registration) and ensure the null path still executes all non-GPU registration — or document precisely what null skips.
3. Fix the measurement lies before agents optimize against them: either add GPU timer queries behind `IGraphicsDevice` or rename current stats to CPU-submission-counts in both code and docs. Verify multisample create/resize/resolve on the declared OpenGL 3.3 minimum with on-hardware screenshots before tagging anything.
4. Minimum viable automated suite from day one of the headless phase: launch-to-menu test (<60s, catches missing refs and init ordering), load-every-scene test, save/load round-trip test, plus UI-screen screenshot tests (deterministic, low variance) before attempting full-scene visual diffs. CI blocks merges; advisory CI is ignored CI.

**Warning signs:**
- Agent says "validated headless" but no visual-mode run exists; or screenshots compared byte-exact and CI flickers on unrelated changes.
- Headless runs pass while GPU runs crash (registration skipped under null).
- Performance work cites command-buffer counts as frame cost.
- Leak/shutdown noise (the Godot GUT lesson: RID/ObjectDB noise, missing-import failures) turns green runs red, so the team disables the gate instead of fixing the harness (two-step job: headless import-warmup → headless test; `GODOT_DISABLE_LEAK_CHECKS`-equivalent discipline with exit codes reflecting *test* results).

**Phase to address:**
Headless validate/run loop phase — the highest-leverage phase in the roadmap (PROJECT.md already orders it before the agent surface). Verification: logic suite + visual suite both green in CI; documented null-device contract stating exactly what is and isn't exercised.

---

### Pitfall 5: Agents amplify every silent no-op and weak contract in the codebase

**What goes wrong:**
Agents don't just *hit* codebase flaws — they *scale* them. Measured failure classes: hallucinated APIs/packages (~5% commercial models, ~22% open models), silent edge-case errors (happy path green, boundaries wrong), scope creep (unasked "improvements"), self-confirming tests (the model tests its own assumptions), plausible-but-wrong logic, and pattern extrapolation (the agent faithfully extends whatever pattern dominates retrieved context — including decade-old antipatterns). The trigger unique to Pyramid: silent-default drift — the agent fills a value where the spec or API is silent (a reasonable-sounding default that compiles and passes weak tests but is semantically wrong), and nothing flags it because there was no error, only an absence. Pyramid's `Dispatch`-is-log-only, occlusion-flag-with-no-algorithm, and legacy `LoadScene` stubs are exactly the kind of absence agents pave over. Reviewers make it worse: studies show humans rate AI-generated PRs *more* neutrally/positively than human ones (clean formatting disarms review) while the code carries 1.5× more correctness issues and higher redundancy (1.87×) from reimplemented utilities.

**Why it happens:**
AGENTS.md already states the law: "agents amplify whatever the codebase is." A human hitting a silent no-op gets confused once; an agent hitting it generates a confident workaround, tests the workaround against its own assumptions, and bakes the misunderstanding into game templates every other agent will copy. Legacy brownfield patterns (JDBC-boilerplate-style: agents writing new code in the old style because retrieval surfaced old files) compound silently — a ratchet effect.

**How to avoid:**
1. Enforce the existing API discipline mercilessly and extend it to the agent surface: no required interface methods with silent no-op defaults; every public symbol implemented, removed, or documented as explicit failure; `Tests/PublicApiLinkage.cpp` coverage for every new SDK symbol. Close the three known silent stubs (Dispatch, occlusion, legacy scene overloads) before agents arrive.
2. Zero-invention-tolerance contracts for agent-authored data: every field an agent must fill has either a schema default, a frozen-artifact value, or an explicit "stop and ask" rule — never a gap the agent must guess across. Separate the three checkpoint types that fail differently: identity/source literals, physical-parameter numerics, and absence representations (None vs 0 vs "" vs sentinel) plus rule-precedence choices.
3. Grounding + gates: retrieval over the actual codebase/headers (audit that queries return *defining* files, not just callers), dependency allow-lists (new packages need explicit approval — `vendor/glad`-only policy already exists, extend the instinct to agent asset/codegen), unhappy-path acceptance criteria on every agent task, static analysis + build/type checks as hallucination catchers, and declared task boundaries checked after the run (scope-creep detector).
4. Treat agent-readability as an engineered property: characterization tests and captured tribal knowledge *before* scaling agent usage. The tenth agent run, after contracts are codified, is dramatically safer than the first — budget for that investment explicitly.

**Warning signs:**
- Agent code "works" but contains calls to functions that don't exist in the pinned headers (version-skew hallucination) or reimplements existing utilities (redundancy smell).
- Tests pass but only cover the happy path; edge inputs (empty, duplicate, timeout, malformed asset) untested.
- Diffs contain unasked refactors or dependency additions alongside the requested change.
- Debugging sessions reveal the agent "filled in" an unspecified value that was actually a semantic claim (wrong source tag, wrong physical constant, wrong absence encoding).

**Phase to address:**
Quality-baseline phase (warnings-as-errors, sanitizers, parser fuzzing, dead-code removal, linkage guarantees) *and* every agent-surface phase thereafter. This is the cross-cutting pitfall: the quality baseline reduces the flaw surface; the SDK/schema/contracts phases must each add agent-specific gates. Verification: hallucinated-API rate on a probe suite; unhappy-path coverage metric; scope-boundary check in review.

---

### Pitfall 6: Machine-readable contracts go stale within weeks

**What goes wrong:**
API reference, JSON schemas, and examples-as-contracts are written once, then implementation moves and the contracts lie. Agents trust the contracts absolutely (they have no tribal knowledge to contradict them), so stale contracts produce systematically wrong games: agents call removed parameters, emit old scene shapes, and copy examples that no longer run. The drift has four layers that move independently (Speakeasy's model): API behavior, spec file, SDK validation behavior, and customer/agent code. Strict client validation + spec lag produces the most confusing symptom: the engine would accept the content, but the SDK/Schema layer rejects it (or vice versa) — "200 OK yet SDK error."

**Why it happens:**
Hand-maintained specs parallel to code always diverge; it's a discipline problem, not a syntax problem. Examples rot fastest because nothing executes them. Without CI enforcement, "update the docs" loses to every feature deadline.

**How to avoid:**
1. Generate, don't duplicate: derive schemas/specs from the same source the runtime validates with (code-first: Zod→OpenAPI pattern generalized — the schema file is a *build artifact* of the loader/validator code, not a parallel document). Where generation isn't feasible, add the three CI gates: examples validated against schemas on every PR; spec-to-implementation contract tests (Dredd/Prism-style: the headless validator *is* the contract test runner); spec-diff breaking detection (oasdiff-style) requiring a version bump + migration note for breaking changes.
2. Examples-as-contracts means examples *execute*: every example in agent docs runs through the headless validate loop in CI and fails the build if it stops validating. No unexecuted snippets in agent-facing docs — ever.
3. Version + changelog the contracts themselves (`info.version`, `x-changelog`-style per-version change lists, `deprecated: true` flags with Sunset-style migration paths) so agents can detect "I was built against contract v2, runtime speaks v3" and follow the documented migration instead of guessing.
4. Keep the contract surface minimal: every additional endpoint/field/schema is a promise with maintenance cost. The core-value razor (PROJECT.md out-of-scope list) applies to contracts too.

**Warning signs:**
- An example copied verbatim from agent docs fails headless validation.
- Schema file and loader code changed in different PRs (or one changed without the other).
- `deprecated` markers exist in prose but not in machine-readable form; no changelog for schema versions.
- Agent bug reports cluster on "docs said X, validator says Y."

**Phase to address:**
Machine-readable contracts phase, with the enforcement habit (examples-execute-in-CI) established in the data-first phase and inherited by all later phases. Verification: contract-freshness dashboard (last-impl-change vs last-spec-change per surface); zero unexecuted examples; breaking-diff gate green.

---

### Pitfall 7: Data-first expressiveness trap (too weak → agents crack the engine; too strong → accidental untested language)

**What goes wrong:**
Two opposite failures with the same symptom (agents editing C++): the data schemas are too weak to express the RTS slice (unit behaviors, win conditions, terrain rules), so agents "escape" into engine code to get the job done — destroying the no-engine-changes core value; or the schemas grow conditionals, loops, and events until they are an untested, undebuggable scripting language that preempts the Baa decision and carries none of a language's tooling (debugger, hot-reload semantics, versioning). Both strand the project: the first violates the platform promise, the second builds a shadow language without admitting it.

**Why it happens:**
Data-driven design separates generic engine from domain data, but the boundary is a judgment call per feature. Under agent pressure ("just make the scenario work"), the expedient move is always to put the logic wherever it's easiest *this week*. Without an explicit expressiveness budget and escape policy, the schemas either starve or sprawl.

**How to avoid:**
1. Hold the PROJECT.md decision: data-first scripting *now*, Baa only after the admission gate (frozen ABI/FFI, ownership/failure semantics, one gameplay-only module, deterministic reload/rollback, editor integration). The gate is the anti-sprawl device — any schema feature that smells like a language construct (variables, loops, first-class functions) must justify why it isn't waiting for Baa.
2. Define the escape policy in writing: what agents may do when data can't express something (file a schema-extension request with a concrete scenario + diagnostics output; use the blessed converter/template extension point) vs. what they must never do (modify `Engine/`, add game semantics to `Pyramid::Engine`, fork a parallel upload/cache path).
3. Grow schemas by evidence from the RTS proof vehicle: each new schema capability must be motivated by a failing RTS-slice scenario, shipped with loader + validator + diagnostics + example + headless test together (never schema-without-loader or loader-without-diagnostics).
4. Keep the DDD pipeline shape (authoring → validation/build → loader/hot-reload → generic runtime) and always log *which data row caused which action* so agent-authored data bugs are traceable without reading engine code.

**Warning signs:**
- Agent PRs touch `Engine/` to implement game content that "couldn't be expressed in data."
- Schema files gain `if`/`loop`/`eval` constructs or embedded code strings without a corresponding language-design review.
- Baa-gate discussion reopens as "but we already have scripting in the schemas."

**Phase to address:**
Data-first game-definition phase (with the RTS slice as proof vehicle) and the proof phase itself. Verification: proof mini-game built with zero engine-code changes; schema-extension log showing evidence-driven additions only.

---

### Pitfall 8: Converter diagnostics agents can't act on (plus the silent-normalization time bomb)

**What goes wrong:**
External-format converters (OBJ/MTL today, more formats with the agent asset library tomorrow) accept broken input with either cryptic errors ("import failed") or — worse — silent "helpful" normalization: flipping V coordinates, rescaling units, guessing axis conventions, substituting fonts, swallowing duplicate materials. Agents respond to cryptic errors with retry loops that change random things; they never notice silent normalization until art looks wrong on GPU but right in validation. Pyramid already owns the right instincts (bounded OBJ/MTL import with `ModelImportLimits`, transactional publication with rollback, `flipY`-before-upload made explicit, color-space-as-identity, malformed/negative-index/non-finite/duplicate-material fixtures) — the pitfall is failing to extend that discipline to every *new* converter and to the curated asset catalog.

**Why it happens:**
Converter authors optimize for "it loads" rather than "it loads *correctly or explains why not*." Coordinate-system, unit-scale, UV-channel, and naming-convention mismatches (Maya Y-up vs Blender Z-up vs Unreal Z-up-X-forward vs Unity Y-up-Z-forward; cm-vs-meter; two-UV-channel requirements; `UCX_` collision naming) cluster across every engine's asset pipeline because DCC tools don't agree and exporters hide their choices. Silent normalization feels kind and is poison.

**How to avoid:**
1. Fail-fast validators with line-keyed, actionable diagnostics at conversion time (Khronos `gltf-validator`-style): every rejection names the file, line/material/attribute, the rule violated, the engine limit, and the fix. Hook the same checks into the authoring path so humans see failures before commit, not after.
2. Normalize *explicitly or not at all*: units, axis handedness, V-flip, color space, and sampler choices are declared pipeline steps recorded in provenance (source hash + converter version + flags = content key; config changes invalidate caches deterministically). Never silently flip/scale/substitute. Pyramid's `flipY`-explicit and color-space-in-identity precedents are the template.
3. Transactional publication everywhere: failed conversion leaves pre-existing cache entries intact (already true for `ModelResourceImporter` and texture reload — replicate for every new importer). Single-writer CI for processed artifacts with immutable outputs + manifest mapping logical IDs to artifact versions + provenance.
4. Every converter change ships with the five test types AGENTS.md already demands (positive/negative index, malformed input, limits, file resolution, transactional upload) plus very-large allocation-limit fixtures and fuzzer/asan coverage as the quality baseline lands. File-resolution rules must confine `map_Kd`/library paths to the asset root (no absolute paths, no `..` escape).

**Warning signs:**
- Converter PR without new malformed/limit fixtures.
- "Fixed" assets that render differently in validation vs. on GPU (normalization applied in one path only).
- Cache hits surviving a converter-flag change (pipeline version missing from the content key).
- Agent retry loops around an import error that doesn't name the offending line/rule/fix.

**Phase to address:**
Converters + agent-legible asset-library phase, building on the quality-baseline phase (fuzzing/sanitizers) and the data-first validation harness. Verification: converter diagnostic-actionability review (each error message links to a fix); cache-invalidation test; transactional-rollback test per importer.

---

### Pitfall 9: Treating agent-supplied content as trusted input

**What goes wrong:**
Once agents author scenes, manifests, MTL files, PNG/JPEG assets, font references, and UI strings, *all* of those become untrusted input to the engine — and to other agents' toolchains. Concrete vectors in Pyramid's existing attack surface: path traversal via OBJ `map_Kd`/material-library references; OOM/decode-loop via malformed PNG/zlib/JPEG (arithmetic/lossless/12-bit/four-component edge cases the bounded decoder explicitly rejects today); unbounded outlines/atlas exhaustion via corrupt SFNT tables (compound-glyph cycles); UTF-8 corruption via overlong sequences and lone surrogates flowing into hit-testing/caret/atlas lookup; tampered installed-host-font bytes via system-font extraction; and at the LLM layer, indirect prompt injection buried in any text the agent reads (asset metadata, docs, scene strings, web-fetched references) that hijacks subsequent tool calls. The 2026 coding-agent incidents (Novee/CSA: one GitHub issue reaching CI secrets across Claude Code, Gemini CLI, and Codex harnesses; `tac` bypasses; `/proc` secret exposure; `AGENTS.md` poisoning across multi-pass runs) show the harness — not the model — is where these break: permissions, sandboxing, and provenance checks failing at handoff points.

**Why it happens:**
Engines historically trust their asset tree (artists are insiders). Agent platforms invert this: the asset tree is adversary-reachable. Meanwhile agent harnesses run with broad tool permissions (read files, run builds, push commits) and load instruction files (`AGENTS.md`-equivalents) that a prior compromised pass can poison.

**How to avoid:**
1. Keep and extend the parsers-treat-bytes-as-hostile posture: bounded decoders, explicit rejection lists, allocation caps with very-large-input tests, cycle/depth protection for fonts, strict UTF-8 with fallback-glyph accounting (never corrupt), grapheme-boundary snapping and cluster-map preservation on every editing change. Never raise a limit without its test; never add a format (CFF/variable/color/WOFF, new image/model variants) without a new bounded parser + corpus tests.
2. Confine file resolution: normalize and jail all agent-supplied paths to the asset root; reject absolute paths and `..` escapes; validate `map_Kd`/library/font paths *before* cache/texture lookup. Audit installed-header/CMake exports for absolute-path leaks.
3. Harden the agent harness itself: least-privilege tool allow-lists, sandboxed code execution, separated workspaces per pass (never share a mutable workspace between an untrusted-content pass and a credentialed pass), provenance checks on instruction files, secrets out of the repo and out of logs, every workflow-written file treated as untrusted input. Apply the dual-LLM / code-then-execute instinct where agents process untrusted asset text: unprivileged parsing first, privileged actions only on validated structures.
4. Never bundle others' font bytes; never bypass the exact-family substitution-rejection check (`Tests/SystemFontTests.cpp` stays green); keep reference assets reproducible via `scripts/regenerate-reference-fonts.py`.

**Warning signs:**
- A new loader/converter reads a path from a file without jailing it.
- A parser change without new malformed/truncated/limit fixtures.
- Agent harness runs with full shell + credentials in one job; multi-pass agents share a workspace; instruction files writable by an earlier pass.
- Installed-font behavior changes without a coverage/capacity-gate review.

**Phase to address:**
Quality-baseline phase (fuzzing, sanitizers, allocation-limit fixtures) for parser hardening; headless-loop phase for sandboxing the validator; converters/asset-library phase for path-jailing; proof phase for harness privilege review. Verification: fixture + fuzz coverage per parser; path-traversal probe suite; harness permission audit before the proof game runs.

---

## Technical Debt Patterns

Shortcuts that seem reasonable building the agent platform but create long-term damage.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Ship SDK before P0 freeze ("stabilize later") | Agent demo sooner | Every agent game built on shifting behavior breaks; Hyrum's Law locks bugs in as features | Never — freeze is the SDK entry gate |
| Re-export internals as "the SDK" | Zero facade work | Agents depend on internals; any refactor is breaking; dedup/handle invariants collapse | Never — physically separate facade package |
| Strict `additionalProperties: false` everywhere | Catches typos early | All additive evolution becomes breaking; old agent content rejected | Only on closed internal structs, never on agent-authored formats without an upcaster |
| One "headless mode" for logic + visuals | One code path to maintain | False confidence (logic) + flickering CI (visuals) + silent blank screenshots | Never — split logic-headless from visual-regression modes |
| Pixel-exact image comparison | Simple to implement | CI flickers on driver/AA noise; team disables the gate | Never — use 1–5% tolerance + CI-captured baselines |
| Silent unit/axis/V-flip normalization in converters | "It just loads" | Time bomb: validation-vs-GPU divergence nobody can trace | Never — normalize explicitly with provenance or reject |
| Hand-maintained schemas/docs parallel to code | Fast first draft | Contracts lie within weeks; agents fail systematically | Only as a bootstrap behind a generation-or-gate plan with a dated expiry |
| Unexecuted doc examples | Docs ship faster | Examples rot; agents copy broken patterns | Never in agent-facing docs — every example executes in CI |
| Data-schema `if`/`eval` to "unblock" a scenario | Scenario ships this week | Accidental untested language; preempts Baa decision | Never — file a schema-extension request or wait for the Baa gate |
| Raising a parser limit to "fix" a rejection | One asset loads | OOM/DoS surface grows silently | Only with a matching very-large-input test + capacity-gate review |
| Advisory (non-blocking) validation CI | Fewer red builds | Agents and humans both ignore it; regressions ship | Never — validation gates block merges or they don't exist |
| Shared mutable workspace across agent passes | Simple orchestration | Poisoned instruction files / exfiltrated secrets (2026 multi-pass incidents) | Never — isolate passes, provenance-check instructions |

## Integration Gotchas

Agent-platform integrations and where they bite.

| Integration | Common Mistake | Correct Approach |
|-------------|---------------|------------------|
| SDK over `ResourceRegistry` caches | Mutating cached `ShaderProgram`/`TextureResource`/`Material` or parallel-uploading identical bytes | Immutable instances; changes only via transactional `Recompile`/`Reload`/`Replace`; shared acquisition via `Meshes()`/`Shaders()`/`Textures()`/`Materials()`; per-draw data in command-buffer uniforms |
| Scene v2 → v3 extension | Adding required/renamed fields as a "minor" change | Major version + upcaster + old-corpus migration tests + transactional parse-then-publish with missing/stale diagnostics |
| Null-device headless run | Assuming null exercises the same registration as GPU (BeamNG/Godot lessons) | Document the null contract explicitly; ensure non-GPU registration still runs; screenshot steps under null are loud errors |
| Software-GL visual CI (Mesa/SwiftShader/xvfb class) | Using dev-machine baselines; byte-exact compare | Capture baselines in CI; tolerance-based diff; separate import-warmup job from test job; exit codes reflect test results, not shutdown noise |
| Asset converters (OBJ/MTL + new formats) | Cryptic errors; silent normalization; cache key missing pipeline version | Line-keyed actionable diagnostics; explicit normalization with provenance; content key = source hash + converter version + flags; single-writer CI artifacts + manifest |
| System-font bytes (Win32 GDI) | Trusting host fonts; accepting silent substitution | Exact-family selection with substitution rejection; all parsing/rasterization/caching in Pyramid-owned code; coverage/capacity gates with owned SDF fallbacks |
| Runtime asset resolution | CWD-relative loads; absolute paths in installed interfaces | `ResolveRuntimePath()` (exe-adjacent first, CWD only as dev fallback); no source-tree absolutes in exports; MinGW DLLs bundled beside binaries |
| Agent harness tools/secrets | Broad permissions + shared workspace + writable instruction files | Least-privilege allow-lists; isolated per-pass workspaces; provenance-checked instructions; secrets out of repo/logs; treat workflow-written files as untrusted |
| Baa scripting (future) | Starting gameplay scripting in C++ strings inside schemas | Hold the admission gate: frozen ABI/FFI, ownership/failure semantics, one gameplay-only module, deterministic reload/rollback, editor integration — all green before any Baa work |

## Performance Traps

Patterns that survive the proof game but fail as agent content scales.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| No occlusion culling; frustum + octree carry all load | Overdraw-bound scenes submit everything frustum-visible; frame time climbs with scene density | Implement-or-remove decision before SDK; keep octree bounds tight + incremental `Synchronize()`; profile on real GPU, not command counts | First large RTS map with heavy occlusion (P3.7 terrain/large-map milestone) |
| Cache-bypass uploads (parallel geometry/shader/texture/material paths agents invent) | GPU memory climbs; residency stats show misses/conflicts; identical content compiled/uploaded repeatedly | Facade-only acquisition; content-identity discipline; monitor per-cache stats (hits/misses/bytes/evictions) in debug UI | As soon as multiple agent games share content or one game scales assets |
| Octree without per-frame sync + compaction | Stale placements after moves; empty-branch bloat after removals; traversal slows | Batch moves/removals → one `Synchronize()` (compacts once) per frame; transactional `Configure()`; watch `OctreeStats`/compaction metrics | Removal-heavy RTS battles; large moving-unit counts |
| Per-frame font rasterization / wrong sampler | Frame-time spikes; blurry small text (nearest) or soft large text without SDF path | Ship precompiled 64px SDF `.pfont`; reuse content-addressed cache; linear sampling for coverage, SDF mode + optical-weight bias for SDF; never reintroduce middleware rasterizers | Any UI-heavy agent game; first CJK/Arabic-heavy screen |
| CPU command counts treated as frame cost | "Optimization" that doesn't move GPU time; missed budgets | GPU timer queries behind device interface, or rename stats honestly; validate with real-GPU captures | First performance pass on the RTS slice |
| Unbounded log history / unclipped draw lists | Per-frame CPU/draw cost grows with session length or list size | Logger caps on; collapsing sections; clipped scroll areas with logical coordinates; stick-to-bottom console | Long agent validation runs; debug-UI-heavy games |

## Security Mistakes

Domain-specific issues for an engine whose asset tree becomes adversary-reachable.

| Mistake | Risk | Prevention |
|---------|------|------------|
| Trusting agent-authored asset paths (`map_Kd`, library refs, font paths) | Path traversal reads outside asset root | Jail + normalize all file resolution to asset root; reject absolute/`..`; validate before cache lookup; probe suite in CI |
| Raising parser limits without tests | OOM / long-loop DoS via crafted PNG/JPEG/OBJ/TTF | Bounded decoders stay bounded; every limit change needs a very-large-input test; fuzzing + ASan/UBSan per quality baseline |
| Accepting new font/image/model variants without a bounded parser | Unbounded outlines, atlas exhaustion, cache poisoning, decode OOM | Explicit rejection lists (CFF/variable/color/WOFF, 12-bit/hierarchical JPEG, etc.) until a new bounded parser + corpus tests land |
| Corrupting UTF-8 on hostile text instead of explicit fallback | Hit-test/caret/atlas misbehavior; downstream crashes | Strict decoding with diagnostics; fallback-glyph accounting; grapheme snapping; cluster-map preservation |
| Bundling third-party font bytes or bypassing substitution rejection | License exposure; tampered outlines | Ruqoom-owned sources only; reproducibility via `regenerate-reference-fonts.py`; exact-family checks green |
| Broad agent-harness permissions / shared workspaces | RCE, secret exfiltration, supply-chain push via prompt injection (2026 incidents) | Least privilege; isolated passes; instruction provenance; sandbox execution; secrets hygiene |
| Indirect prompt injection in asset/doc text agents read | Hijacked tool calls acting through the harness | Untrusted-text parsing before privileged action; structured outputs; unexpected-tool-call rejection; monitoring |
| Absolute paths / secrets leaking via installed interfaces or logs | Machine-specific breaks; credential exposure | Export audits; `ResolveRuntimePath` discipline; secrets never in repo/logs; no quoting secrets into planning docs |

## UX Pitfalls (Agent-DX: the agent is the user)

| Pitfall | Agent Impact | Better Approach |
|---------|-------------|-----------------|
| Cryptic validator errors ("invalid scene", "import failed") | Retry loops changing random fields; wasted iterations; learned helplessness | Line-keyed diagnostics: file + line + rule + limit + fix; "which data row caused this action" logging |
| Silent behavior differences between headless and GPU | Agent "validates" something that renders wrong; trust collapse in the loop | Explicit null-device contract; loud errors for GPU-only steps under null; paired logic + visual suites |
| Undocumented unspecified values (the silent-default trap) | Agent invents plausible-but-wrong values that pass weak tests | Zero-invention contracts: every fillable field has a default, a frozen value, or a stop-and-ask rule |
| Stale examples that don't run | Agents copy broken patterns at scale; every new agent repeats the failure | Examples execute in CI; freshness dashboard; no unexecuted snippets |
| Facade with ten ways to do one task | Agents pick the internal/powerful path and entrench it (Hyrum) | One blessed path per task in the facade guide; internals unreachable from the agent package |
| No deterministic iteration signal | Agent can't tell "my change fixed it" from noise (flickering visuals, leak-noise reds) | Deterministic logic mode; tolerance-based visuals; exit codes = test results; import-warmup separated |
| Schema errors reported without migration path | Agent hand-edits versioned files and corrupts them | Every breaking change ships an upcaster + migration note; old docs migrate automatically |

## "Looks Done But Isn't" Checklist

Verify these before claiming each agent-platform phase complete — each is a historically observed false-done.

- [ ] **SDK facade:** Looks done (headers + docs exist) — often missing facade-only consumer test proving agents *can't* reach internals. Verify: example builds against SDK package alone; banned-include grep clean; `PublicApiLinkage` covers new symbols.
- [ ] **P0 freeze:** Looks done (checklist ticked) — often missing the implement-or-remove proof for Dispatch/occlusion/shadow-array/formats/binding. Verify: no `// TODO`-behind-public-API remains; each decision has a test or a deletion.
- [ ] **Scene/schema vNext:** Looks done (new fields load) — often missing upcaster + old-corpus migration + `additionalProperties` review. Verify: every old example migrates automatically; strictness changes justified in the version note.
- [ ] **Headless loop:** Looks done (`--headless` runs green) — often missing the null-contract doc + visual-mode suite + CI baselines. Verify: GPU-only steps fail loudly under null; screenshot diffs use tolerance; baselines captured in CI.
- [ ] **Render-image regression:** Looks done (screenshots compared) — often missing tolerance + determinism (fixed timestep, leak-noise isolation). Verify: re-runs are stable; unrelated changes don't flicker; GPU timings (or honest CPU-count labeling) in place.
- [ ] **Contracts:** Look done (reference + schemas + examples exist) — often missing generation-or-gate + executed examples. Verify: examples run in CI; spec/impl change together; breaking-diff gate exists.
- [ ] **Converters:** Look done ("imports my test file") — often missing the five test types + very-large fixtures + path-jailing + transactional rollback. Verify: malformed/limit/traversal probes green; failed import preserves prior cache state.
- [ ] **Proof game:** Looks done (a game runs) — often missing the zero-engine-changes proof + agent-alone reproduction. Verify: `Engine/` untouched; fresh agent with only agent docs reproduces the build through the headless loop with no human help.
- [ ] **Quality baseline:** Looks done (CI green) — often missing warnings-as-errors + sanitizers + fuzzing + hardware verification on the OpenGL 3.3 minimum. Verify: P1 quality items closed, not just compiled; both examples visually verified on real GPU.
- [ ] **Harness security:** Looks done (agent runs) — often missing privilege audit + workspace isolation + instruction provenance. Verify: permission review + traversal/injection probes before the proof game.

## Recovery Strategies

When a pitfall fires despite prevention.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|--------------|----------------|
| SDK promised over shifting internals | HIGH | Freeze immediately; version the break honestly (major bump + migration guide); add upcaster/shim for the old observable behavior; gate all future facade changes on the freeze checklist. Do not silently "fix" behavior agents depend on. |
| Leaky facade entrenched in agent games | HIGH | Phase 1: bless one path and shim the rest with deprecation diagnostics. Phase 2: remove internals from the agent package after the alias window. Fix templates first (they replicate fastest). |
| Breaking schema change shipped | MEDIUM | Ship the missing upcaster + version bump retroactively; migrate the corpus; add the CI compatibility gate so it can't recur. Apologize in the changelog with a migration section. |
| Headless/GPU divergence discovered late | MEDIUM–HIGH | Split the suites (logic vs visual) immediately; write the null-contract doc from observed differences; re-baseline visuals in CI; re-validate the proof game on GPU. |
| Silent no-op baked into agent content | MEDIUM | Implement-or-remove the stub; add explicit-failure diagnostics; grep agent templates/examples for workarounds and fix them at the source; add the unhappy-path test that would have caught it. |
| Stale contracts | LOW–MEDIUM | Regenerate specs from code; re-execute all examples and fix-or-delete rotten ones; install the three gates (example validation, contract tests, breaking diff). Assign a contract owner per surface. |
| Data-schema sprawl toward shadow language | HIGH | Stop and adjudicate against the Baa gate: either admit the language (with design, versioning, debugger story) or roll features back into bounded schemas + extension-request process. |
| Cryptic-converter fallout (bad assets in tree) | LOW–MEDIUM | Re-validate the whole asset catalog with the improved validator; quarantine failures with actionable messages; add the missing fixtures; fix the cache-key to include pipeline version and reimport. |
| Harness compromise / injection incident | HIGH | Rotate secrets; isolate and audit affected passes/artifacts; separate workspaces; provenance-check instructions; add the probe suite; treat all workflow-written files as untrusted going forward. |

## Pitfall-to-Phase Mapping

Roadmap order follows the dependency chain: nothing agent-facing stands on unfrozen ground, and the loop agents iterate in comes before the surfaces they touch.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Stability promises on shifting internals (P1) | Core correctness freeze (P0 verification) — entry gate for all agent work | Implement-or-remove proof per P0 item; `PublicApiLinkage` green; no public TODOs |
| Agents amplifying silent no-ops / weak contracts (P5) | Quality baseline (warnings-as-errors, sanitizers, fuzzing, dead-code removal, linkage) | Probe suite: hallucination rate, unhappy-path coverage, scope-boundary checks |
| Headless/GPU fidelity gap (P4) | Headless validate/run loop (null device + harness + image regression) | Logic + visual suites green in CI; null-contract doc; CI baselines; tolerance diffs |
| Schema versioning breaks (P3) | Data-first game definition (schemas + loader + explaining diagnostics) | Old-corpus migration green; compatibility gate; transactional diagnostics |
| Leaky facade (P2) | Agent SDK facade (stable C++ API + templates + compatibility guarantees) | Facade-only consumer test; banned-include audit; compatibility policy published |
| Cryptic converters / silent normalization (P8) | Converters + asset library (diagnostics + versioned catalog + metadata) | Actionability review; five-test-types per importer; cache-key + rollback tests |
| Stale contracts (P6) | Machine-readable contracts (API ref + JSON schemas + examples-as-contracts) | Examples execute in CI; freshness dashboard; breaking-diff gate |
| Expressiveness trap / Baa sprawl (P7) | Data-first phase design + proof-phase enforcement (Baa gate held) | Zero-engine-changes proof; evidence-logged schema extensions only |
| Untrusted content / harness compromise (P9) | Cross-cutting: baseline (parsers) → headless (sandbox) → converters (jailing) → proof (privilege audit) | Fixture/fuzz coverage; traversal probes; harness permission audit |
| False-done validation (all) | Every phase exit via "Looks Done But Isn't" checklist | Checklist signed per phase; proof game reproduces agent-alone |

**Phase ordering rationale:** Freeze → Harden → Loop → Data → Facade → Converters → Contracts → Proof. The headless loop precedes the agent surface because the entire agent iteration cycle depends on windowless, GPU-less runs; contracts come after the surfaces they describe (so generation sources exist) but their *enforcement habits* (examples-execute, diagnostics quality) are established in the data-first phase; the proof game runs last because it validates the whole chain with the strictest criterion (zero engine changes, agent alone).

## Sources

- Hyrum's Law (official): https://www.hyrumslaw.com/ — HIGH-adjacent foundational statement, cross-checked via lawsofsoftwareengineering.com (2026-06-24). Confidence: MEDIUM (verified across two sources).
- Speakeasy "Building APIs (and SDKs) that never break" (API/spec/SDK/customer drift layers; strict-validation + evolution breakage; forward compatibility): https://www.speakeasy.com/blog/building-apis-and-sdks-that-never-break — Confidence: MEDIUM (detailed technical treatment, consistent with Hyrum sources).
- Iron Curtain mod-API stability (mod-facing YAML/Lua/WASM surface versioned separately from engine crates): https://iron-curtain-engine.github.io/iron-curtain-design-docs/modding/api-stability.html — Confidence: LOW (single design-doc source; pattern corroborates facade-separation recommendation).
- Fundamental Engine `api-stability.md` (frozen public surface + compatibility rules + alias window): https://github.com/zachshallbetter/fundamental-engine/blob/main/docs/canonical/api-stability.md — Confidence: LOW (single repo doc; cited as pattern example only).
- Capybara `SDK_FACADE.md` (facade-as-source-of-truth guidance): https://github.com/d-liya/capybara_2d_engine/blob/main/.agents/skills/capybara-game-developer/SDK_FACADE.md — Confidence: LOW (single repo doc; pattern example).
- JSON Schema Migration guide (BACKWARD/FORWARD/FULL, `additionalProperties: false` hazard, upcasters, property-based testing): https://jsonic.io/guides/json-schema-migration (2026-05-20) + Confluent Schema Registry evolution docs https://docs.confluent.io/platform/current/schema-registry/fundamentals/schema-evolution.html — Confidence: MEDIUM (two independent sources agree on compatibility taxonomy).
- json-schema-org compatibility-checker gap (GSoC 2026 proposal noting no official JSON Schema breaking-change tool vs `buf breaking`/Registry): https://github.com/json-schema-org/community/issues/984 — Confidence: MEDIUM (primary issue text; factual gap claim).
- SaveCompat (old saves as migration corpus with per-version validation + semantic diff): https://github.com/KanadeK/savecompat — Confidence: LOW (single repo; pattern example).
- BeamNG no-GPU mode thread (null-GFX skipping frame-loop collision/asset registration; headless-vs-no-GPU distinction): https://forum.beamng.tech/t/no-gpu-mode-headless-mode-no-collision-no-road/473 — Confidence: MEDIUM (vendor engineer confirms incompleteness; corroborated by Godot dummy-storage issues below).
- Godot headless rendering issues (`mesh_get_surface_count` null under dummy storage; headless-exit RID leaks; off-screen DisplayServer + SwiftShader/OSMesa proposal #1760): https://github.com/godotengine/godot/issues/86806, https://github.com/godotengine/godot-proposals/issues/1760 — Confidence: MEDIUM (primary issue reports + maintainer fix linkage).
- Godot-QA runner (logic-headless vs xvfb+software-GL visual split; CI baselines byte-stable; screenshot-under-headless is explicit error): https://github.com/youichi-uda/godot-qa — Confidence: MEDIUM (concrete tooling README with rationale; consistent with Vulkan CI render-validation guidance https://docs.vulkan.org/tutorial/latest/ML_Inference/Desktop_Applications/04_ci_render_validation.html on fragile pixel-exact compares).
- Godot GUT-on-CI practice (headless import-warmup → headless test split; leak-noise exit-code discipline): https://itch.io/blog/1088565/ci-tested-gut-on-godot-45 — Confidence: LOW (single practitioner report; consistent with Godot-QA split).
- Agent silent-default drift / zero-invention tolerance + Provenance Source Gate: https://newsletter.phillysaipharmacist.com/p/ai-agent-silent-default-drift-governance — Confidence: LOW (single practitioner account; cited for the absence-trigger failure mode, corroborated by hallucination literature).
- AI-generated-code failure classes (hallucinated APIs 5–22%, silent edge errors, scope creep, self-confirming tests, 1.5× correctness debt, 1.87× redundancy, reviewer-sentiment asymmetry): https://realitygraph.dev/why-ai-code-fails (2026-08-15), https://codex.danielvaughan.com/2026/06/23/silent-technical-debt-ai-generated-code-empirical-evidence-codex-cli-quality-defence-patterns/ — Confidence: MEDIUM (two sources with empirical claims in the same direction; exact percentages treated as indicative, not precise).
- Agents on brownfield/legacy (retrieval quality vs context size; pattern extrapolation extending old idioms; tribal-knowledge gaps; task-scope boundaries): https://tianpan.co/blog/2026/04/19/ai-coding-agents-brownfield-legacy-code — Confidence: MEDIUM (detailed practitioner analysis consistent with CodeRAG-Bench findings it cites).
- Hallucinated-tool silent crash (unvalidated tool names → silent no-op → confident continuation on false premises): https://dev.to/keerat_rashid/the-silent-crash-how-hallucinated-tool-breaks-ai-agents-34cc — Confidence: LOW (single article; mechanism corroborates no-silent-no-op recommendation).
- 2026 coding-agent harness incidents (Novee/CSA: CI secret exposure across Claude Code/Gemini CLI/Codex defaults; `tac`/`/proc`/counter side-channels; multi-pass `AGENTS.md` poisoning): https://labs.cloudsecurityalliance.org/research/csa-research-note-ai-coding-agent-cicd-secrets-20260808-csa/, https://cybersecuritynews.com/critical-flaws-in-ai-coding-agents/ — Confidence: MEDIUM (multiple outlets + advisories describe the same incident set).
- Prompt-injection fundamentals (trusted-instruction vs untrusted-data undifferentiated stream; OWASP #1; isolation/dual-LLM/code-then-execute patterns): https://owasp.org/www-community/attacks/PromptInjection, https://arxiv.org/html/2506.08837v1 (design patterns for securing LLM agents) — Confidence: MEDIUM (OWASP primary + peer-reviewed pattern paper agree).
- Contract-first/OpenAPI drift controls (code-first generation via zod-openapi; oasdiff breaking gates; Prism mock validation; examples-vs-schema CI; `deprecated`/`Sunset` handling): https://jsonic.io/guides/json-api-documentation (2026-05-20), https://www.datasops.com/blog/data-contracts-openapi, https://github.com/contractual-dev/contractual — Confidence: MEDIUM (three sources converge on generate + gate + test).
- Asset import pipeline practice (FBX-as-exchange vs glTF-canonical; explicit unit/axis/V normalization; content-keyed caching with pipeline version; single-writer CI; fail-fast validators): https://beefed.ai/en/automated-asset-import-pipeline, https://nastyrodent.com/asset-import-pipeline/ — Confidence: MEDIUM (two detailed pipeline guides agree; coordinate/UV/collision failure clusters corroborated across both).
- Data-driven design pipeline shape (authoring → validation/build → loader/hot-reload → generic runtime; "which data row caused this action" traceability): https://dev.to/methodox/data-driven-design-leveraging-lessons-from-game-development-in-everyday-software-5512 — Confidence: LOW (single overview; used only for pipeline vocabulary).
- Pyramid first-party grounding (HIGH confidence — files read directly): `.planning/PROJECT.md`, `docs/ROADMAP.md`, `AGENTS.md` (via environment), `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/CONCERNS.md` (2026-09-05 audits listing every P0 stub, fragile area, and coverage gap cited above).

---
*Pitfalls research for: agent-first C++17 game engine platform (Pyramid Engine)*
*Researched: 2026-09-05*
