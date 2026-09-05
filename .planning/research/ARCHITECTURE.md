# Architecture Research: Agent-First Platform Layers for Pyramid Engine

**Domain:** Agent-first / data-driven C++17 game engine platform (on top of existing Pyramid 0.6.0-pre-alpha baseline)
**Researched:** 2026-09-05
**Confidence:** HIGH (existing engine structure — read from codebase map + headers); MEDIUM (ecosystem patterns — web survey of manifest-driven, headless, facade approaches)

## Standard Architecture

### System Overview

New agent-first layers (top three boxes) integrate with — but do not restructure — the proven baseline (bottom three boxes). Scene v2 + manifests are the foundation; everything new builds outward from them.

```
┌─────────────────────────────────────────────────────────────────┐
│                    AGENT SURFACE (new)                           │
│  ┌──────────────────┐  ┌──────────────────┐  ┌────────────────┐ │
│  │ Agent SDK facade │  │ Game templates   │  │ Asset catalog  │ │
│  │ PyramidAgentSDK  │  │ + examples-as-   │  │ (versioned,    │ │
│  │ (stable C++ API) │  │ contracts        │  │ machine-read.) │ │
│  └────────┬─────────┘  └────────┬─────────┘  └───────┬────────┘ │
│           │                     │                    │          │
├───────────┴─────────────────────┴────────────────────┴──────────┤
│                 DATA-FIRST DEFINITION (new)                      │
│  ┌────────────────────┐  ┌──────────────────────────────────┐   │
│  │ GameDefinition     │  │ Converters (external → owned,    │   │
│  │ loader + validator │  │ with explaining diagnostics)     │   │
│  │ schemas (JSON)     │  │ glTF/OBJ/PNG → owned pipeline    │   │
│  └─────────┬──────────┘  └────────────────┬─────────────────┘   │
│            │  transactional, deterministic │                     │
├────────────┴──────────────────────────────┴─────────────────────┤
│                HEADLESS VALIDATE / RUN LOOP (new)                │
│  ┌────────────────┐  ┌──────────────────┐  ┌──────────────────┐  │
│  │ NullDevice     │  │ Headless host    │  │ Regression       │  │
│  │ (IGraphics-    │  │ (windowless Game │  │ harness (logic + │  │
│  │  Device null)  │  │  driver, N-frame │  │  pixel tiers)    │  │
│  │                │  │  tick, exit code)│  │                  │  │
│  └───────┬────────┘  └────────┬─────────┘  └────────┬─────────┘  │
│          │                    │                     │            │
├──────────┴────────────────────┴─────────────────────┴────────────┤
│              EXISTING ENGINE BASELINE (stable, frozen)           │
│  Game loop │ Scene (stable-ID Entity+components, v2 serializer)  │
│  ResourceRegistry (immutable content-addressed, generational     │
│   handles) │ ResourceManifest │ ModelResourceImporter │ Passes   │
├─────────────────────────────────────────────────────────────────┤
│  Win32/WGL Platform │ OpenGL 3.3 backend │ Owned Libraries       │
│  (Foundation/Math/Input/Image/Model/Font/Text/UI) │ glad only   │
└─────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| Contract layer (schemas + API ref) | Machine-readable truth for every file format and SDK call; versioned; agent reads this, never engine headers directly | `docs/schemas/*.schema.json` + `docs/agent-sdk/*.md` generated/checked against code; examples-as-contracts checked in CI |
| GameDefinition loader | Parse `game.json` + scene files + manifest + gameplay data into validated in-memory definition; dependency-ordered, atomic (all-or-nothing) | New `Libraries/PyramidGameDefinition/` (CPU-only, no graphics/Win32); builds on `SceneSerializer` + `ResourceManifest` transactional parse pattern |
| GameDefinition validator | Explaining diagnostics: line-keyed errors, missing/stale asset separation, fix suggestions; non-zero exit on failure | Reuse `SceneSerializationDiagnostic` / `ResourceManifestDiagnostic` enum+line+key+message shape; new `GameDefinitionDiagnostic` |
| NullGraphicsDevice | Production `IGraphicsDevice` that accepts all calls, creates CPU-side stand-ins, records draws, produces no pixels | `Engine/Graphics/source/Null/NullDevice.cpp` mirroring `OpenGLDevice` interface; Flax `GPUDeviceNull` precedent; distinct from `Tests/TestGraphicsDevice` (test double) |
| Headless host | Windowless `Game` driver: no Win32 window, fixed-timestep N-frame tick, headless input/event injection, deterministic exit code | `Engine/Platform` headless `Window` impl (or `Game` headless-run path) + `Tools/PyramidValidate/main.cpp` CLI; droids-engine "headless is primary" precedent |
| Regression harness | Two-tier proof: (1) null-logic tier (always runs, GPU-less, asserts no-missing/no-stale/N-frames-no-crash), (2) GPU pixel tier (windowed/offscreen FBO capture vs reference PNG, tolerance-based) | Tier 1 in `Tools/PyramidValidate`; Tier 2 as GPU-gated CTest + reference images in `Tests/Regression/`; Renegade headless-vs-graphics-proofs split precedent |
| Agent SDK facade | Small stable simplified C++ API over the engine: create game, load definition, spawn/query entities, acquire assets, run headless; semver compatibility guarantee | New independently-packaged `SDK/PyramidAgentSDK/` (or `Libraries/PyramidAgentSDK/`) depending on `Pyramid::Engine`, never inside `Engine/`; Dojo/Origo adapter-layer precedent |
| Converters | External formats → owned pipeline with diagnostics + rollback (never silent substitution) | `Tools/PyramidAssetConverter/` over `Pyramid::Model`/`Pyramid::Image`; reuse `ModelResourceImporter` transactional publication |
| Asset catalog | Curated versioned asset set with machine-readable metadata (id, license, poly count, bounds, manifest key) | `assets/catalog/` + `catalog.json` + JSON schema; Hyperscape `manifests/` + provider precedent |
| Game templates | Minimal compilable agent starting points (RTS scenario template) that pass `PyramidValidate` on day zero | `Templates/AgentRTS/` + CI job that builds template through agent path alone |

## Recommended Project Structure

```
Pyramid-Engine/
├── Engine/
│   ├── Graphics/source/Null/        # NEW: NullDevice.cpp (+ header in include/.../Null/)
│   └── Platform/source/Headless/    # NEW: HeadlessWindow.cpp (Window impl, no Win32)
├── Libraries/
│   └── PyramidGameDefinition/       # NEW: CPU-only definition layer (outside engine binary)
│       ├── include/Pyramid/GameDefinition/GameDefinition.hpp
│       ├── include/Pyramid/GameDefinition/GameDefinitionLoader.hpp
│       ├── include/Pyramid/GameDefinition/GameDefinitionValidator.hpp
│       └── source/*.cpp
├── SDK/
│   └── PyramidAgentSDK/             # NEW: stable facade, independently packaged
│       ├── include/Pyramid/Agent/AgentGame.hpp
│       ├── include/Pyramid/Agent/AgentScene.hpp
│       ├── include/Pyramid/Agent/AgentAssets.hpp
│       ├── include/Pyramid/Agent/AgentDiagnostics.hpp
│       └── source/*.cpp
├── Tools/
│   ├── PyramidValidate/             # NEW: headless validate/run CLI (agent iteration loop)
│   └── PyramidAssetConverter/       # NEW: external-format converters
├── docs/
│   ├── schemas/                     # NEW: JSON schemas for every file format
│   └── agent-sdk/                   # NEW: machine-readable API reference
├── Templates/
│   └── AgentRTS/                    # NEW: agent starting point, examples-as-contract
├── assets/catalog/                  # NEW: curated versioned asset library + catalog.json
└── Tests/Regression/                # NEW: null-logic tests (always) + GPU pixel tests (gated)
```

### Structure Rationale

- **`Engine/Graphics/source/Null/`:** Null device implements the existing `IGraphicsDevice` interface so `Game`, `RenderSystem`, and `ResourceRegistry` run unmodified. Stays in the engine binary because it *is* a graphics backend (like `OpenGL/`). This is the Flax `GPUDeviceNull` pattern, not a test mock.
- **`Engine/Platform/source/Headless/`:** Headless window implements the existing `Window` interface with no Win32 calls, so `Game::run` message-pump/input/resize logic is exercised without a window server. Keeps the public `Window` boundary backend-neutral per existing constraint.
- **`Libraries/PyramidGameDefinition/`:** Definition parsing/validation is CPU-only (JSON → validated structs → `Scene`/`Manifest` text). Must stay outside the engine binary per ownership rule (like `Pyramid::Model`: no graphics/Win32/GLAD dependency). GPU publication goes through the existing `ModelResourceImporter` + registry caches, never a parallel uploader.
- **`SDK/PyramidAgentSDK/`:** Facade lives outside `Engine/` so engine internals can evolve behind a semver-pinned surface. Depends on `Pyramid::Engine` one-way (Origo `GodotAdapter` / Dojo SDK adapter precedent). Independently packaged with its own `*Config.cmake.in` like the eight owned libraries.
- **`Tools/PyramidValidate/`:** Offline CLI like `Tools/PyramidFontCompiler` — links definition library + engine, never ships in the runtime. This is the agent's compile-check: fast, deterministic, GPU-less by default.
- **`docs/schemas/ + Templates/AgentRTS/`:** Contracts-first: schemas and compiling templates land before structural surgery, so roadmap pain is exposed by evidence (per PROJECT.md key decision).

## Architectural Patterns

### Pattern 1: Facade over a frozen core (Agent SDK)

**What:** A small (~15–30 function) stable C++ surface (`AgentGame::LoadDefinition`, `AgentScene::Spawn/Query`, `AgentAssets::Acquire`, `AgentDiagnostics::Explain`) that forwards to the full engine API. Engine internals stay frozen; only the facade carries a compatibility promise.
**When to use:** Always for agent-facing work. Agents never include `Engine/Graphics/*` directly; humans may.
**Trade-offs:** Adds a thin maintenance layer, but decouples agent-proof stability (PROJECT.md "finish + freeze core before SDK stability promises") from engine iteration. Must resist facade bloat — every addition needs a template/contract use case.

**Example:**
```cpp
// Agent path (stable): data + facade only, no engine headers.
Pyramid::Agent::AgentGame game;
auto report = game.LoadDefinition("game.json");  // validates + publishes
if (!report.Succeeded()) {
    for (const auto& d : report.diagnostics) { PYRAMID_LOG_ERROR(d.message); }
    return 1;
}
auto player = game.Scene().Spawn("units/knight", {0.0f, 0.0f, 0.0f});
game.RunHeadless({.frames = 600});  // null device, deterministic
```

### Pattern 2: Null Object backend (headless run loop)

**What:** `NullDevice : IGraphicsDevice` accepts every call, allocates CPU stand-ins, counts draws/state changes, returns success, presents nowhere. `HeadlessWindow : Window` synthesizes messages/input without Win32. Together they let `Game::run` execute logic, scene sync, octree, UI layout, and resource publication with zero GPU/window.
**When to use:** The agent iteration loop and all CI logic tests. Pixel truth comes from a separate GPU tier.
**Trade-offs:** Catches logic/definition errors fast and deterministically, but proves nothing about pixels — must be paired with an explicit GPU regression tier or teams fool themselves (Godot `--headless` screenshot trap).

**Example:**
```cpp
// Factory sketch — same seam as IGraphicsDevice::Create, new backend id.
enum class GraphicsAPI { OpenGL, Null };  // Null = headless validation
std::unique_ptr<IGraphicsDevice> device = IGraphicsDevice::Create(GraphicsAPI::Null, nullptr);
// RenderSystem::Render(scene, camera) executes against NullDevice:
// draws recorded + counted, framebuffer/viewport binds validated, no GL calls.
```

### Pattern 3: One-way authoritative data (data-first definitions)

**What:** JSON definition files are the single source of truth; runtime objects (`Scene`, registry contents, proxies) are derived and never edited back into source. Pipeline is SYNC → GENERATE → PROPAGATE in one direction only (Unreal external-data precedent: JSON authoritative, cooked assets derived).
**When to use:** All agent-authored content: scenes, manifests, gameplay tuning, catalog entries.
**Trade-offs:** Requires disciplined "never hand-edit derived output" rule and atomic multi-file loads (Hyperscape `DataManager`: validate ALL files before loading ANY; dependency-ordered: catalog → items/materials → manifests → scenes → gameplay). Pays off in agent-editability: text diffs, no binary cooking, no editor required.

### Pattern 4: Transactional load with explaining diagnostics

**What:** Every load path (definition, manifest, scene v2, converter, importer) validates fully before publishing anything, and on failure leaves prior state untouched while returning line-keyed, fix-oriented diagnostics (Pyramid already does this in `SceneSerializer`, `ResourceManifest::Deserialize`, `ModelResourceImporter` — extend the idiom, don't invent a new one).
**When to use:** All agent-touchable ingestion. Agents amplify whatever the codebase is — silent substitution or partial publication becomes un-debuggable agent loops.
**Trade-offs:** Slightly more code per loader (two-phase parse-then-commit), but it is what makes the headless loop closable: agent runs validate → reads diagnostics → fixes data → re-runs, with no human triage.

### Pattern 5: Examples-as-contracts + versioned schemas

**What:** Every schema has a minimal compiling example that CI builds and runs through `PyramidValidate`; every SDK function appears in a template. Schema version bumps are explicit (`"version": 2`) and old versions reject with a migration message (scene v2 precedent: v1 flat scenes intentionally rejected).
**When to use:** From day one of the contract layer — before the SDK freezes.
**Trade-offs:** CI cost per example, but it is the only compatibility guarantee agents can rely on: docs rot, green contract tests don't.

## Data Flow

### Agent authoring loop (primary flow — must close without humans)

```
[Agent writes game.json + scene + manifest + catalog refs]
     ↓
[Tools/PyramidValidate definition.json]  (headless CLI, NullDevice, no GPU/window)
     ↓  transactional load: catalog → manifest → scene v2 → gameplay
[Diagnostics: line + key + code + fix hint, exit != 0 on failure]
     ↓ fail → agent fixes data, re-runs (tight loop, seconds)
     ↓ pass
[N-frame headless run: scene proxies → octree sync → passes vs NullDevice]
     ↓  draw counts + missing/stale report + crash/timeout detection
[PASS → agent submits; GPU pixel tier runs in CI (offscreen FBO → PNG compare)]
```

### Runtime load flow (definition → playable)

```
GameDefinition text files
     ↓ (GameDefinitionLoader: schema check → reference-integrity check)
Validated definition (in-memory structs, asset keys + generations)
     ↓ (ResourceManifest::Restore vs ResourceRegistry: exact generation check)
GPU resources published (immutable Mesh/Texture/Material via existing caches;
 converters/model importer transactional; stale → null, never substitution)
     ↓ (SceneSerializer::Deserialize: stable IDs, hierarchy validation)
Authoritative Scene (Entity + components)
     ↓ (SynchronizeRenderProxies / SynchronizeLightProxies)
Renderer proxies → SceneManager/octree → RenderSystem passes → device
     ↓                                      ↓
Headless: NullDevice (counts)        Windowed: OpenGLDevice (pixels)
```

### SDK flow (same engine, narrower door)

```
Agent C++ (includes only Pyramid/Agent/*.hpp)
     ↓  facade calls (stable, versioned)
PyramidAgentSDK (validates args → loads definitions → forwards to Engine)
     ↓  existing engine API (unstable-internally, frozen-by-policy)
Game / Scene / ResourceRegistry / RenderSystem / Platform
```

State management across these flows follows existing rules unchanged: per-frame `InputState` snapshot, UI `InputConsumptionMask` before action evaluation, dirty-flagged hierarchy transforms, graphics-thread-owned registry teardown (materials → textures → shaders → meshes → device → window).

### Key Data Flows

1. **Definition → diagnostics (agent feedback):** file bytes → schema validator → loader → typed diagnostics with source line + key + remediation; never throw, never partial-publish, always non-zero exit + machine-parseable output.
2. **Definition → registry → scene (publication):** manifest generations checked before scene deserialize; missing assets vs stale generations reported separately (existing `SceneSerializer`/`Manifest::Restore` distinction preserved at the definition layer).
3. **Scene → proxies → passes (execution):** entity world matrices/visibility → `RenderObject`/`Light` proxies → octree/culling → command buffers → Null (logic proof) or OpenGL (pixel proof). No second transform authority, no parallel upload path.
4. **Catalog → game (reuse):** catalog.json metadata → converter/importer → content-addressed cache (dedupe by bytes) → manifest key → scene reference. Color space stays part of texture identity; shared geometry via `ResourceRegistry::Meshes()`.

## Scaling Considerations

This platform scales with *content volume and agent-loop throughput*, not request users.

| Scale | Architecture Adjustments |
|-------|--------------------------|
| 1 template game (proof) | Single `game.json` + handful of scenes; null-logic tier only; pixel tier = 2–3 reference screenshots; monolith validate CLI is fine |
| 10s of agent games / 100s of assets | Catalog indexing + content-addressed dedupe pay off; add definition-file watcher / incremental re-validate; shard regression PNGs per scene; first bottleneck is full re-validate time — fix with per-file caching keyed on content hash |
| 100s of games / 1000s of assets | Split converters into per-format workers; parallelize GPU pixel tier across CI agents; manifest/restore needs generation-tombstone GC policy; second bottleneck is reference-image churn — fix with perceptual-tolerance policy (Chebyshev/RMSE thresholds, not byte equality) + explicit re-baselining workflow |

### Scaling Priorities

1. **First bottleneck:** validate-loop latency (agent iterates dozens of times per game). Keep null tier < 5s for template-scale games: lazy-load nothing, parse once, cache by content hash, emit only failing diagnostics by default.
2. **Second bottleneck:** pixel-reference maintenance (every shader/resize change invalidates PNGs). Mitigate with small fixed-size offscreen captures, tolerance-based compare, and a `--rebaseline` workflow that requires human/agent-explicit intent — never silent auto-update.

## Anti-Patterns

### Anti-Pattern 1: Headless screenshots (null device producing pixels)

**What people do:** Point the null/headless path at pixel assertions, or trust `--headless` viewport reads as framing truth.
**Why it's wrong:** Null by definition renders nothing; placeholder viewport sizes corrupt layout measurement; green CI hides blind rendering (Godot/SummerEngine precedent).
**Do this instead:** Enforce the two-tier contract in code: `NullDevice` has no readback API (or one that always fails visibly); pixel assertions live only in the GPU-gated tier with a strict GPU-backend constructor that aborts when software fallback engages (scena `with_headless_gpu` strict vs prefer-gpu precedent).

### Anti-Pattern 2: SDK leaking engine internals

**What people do:** Expose `RenderObject`, `IGraphicsDevice`, Win32 handles, or raw GL ids through the agent facade "for power users"; or let agents include engine headers for expediency.
**Why it's wrong:** Freezes internals prematurely, couples agents to one backend, breaks the independent-testability ownership rule, and re-creates the parallel-API drift droids-engine warns against.
**Do this instead:** Facade exposes data handles + definition paths + diagnostics only; any escape hatch returns an opaque token, never a raw pointer. Contract tests assert the facade header set includes no `Engine/Graphics/*` or Win32 headers.

### Anti-Pattern 3: Game semantics in the engine / engine surgery for the proof game

**What people do:** Add RTS unit/selection/command components to `Pyramid::Engine` to make the proof slice easier, or fork resource ownership for "agent resources."
**Why it's wrong:** Violates the standing ownership rule (RTS stays in game/reference layers; resources stay immutable + handle-first) and poisons the generic platform with one game's vocabulary.
**Do this instead:** Proof-game components live in `Templates/AgentRTS/` + `Examples/RTSReference/`; SDK gains generic spawn/query/attach-data primitives, not RTS types.

### Anti-Pattern 4: Parallel upload / mutable cache shortcuts in converters

**What people do:** Converters that create their own VAOs/textures instead of publishing through `ResourceRegistry` caches + `ModelResourceImporter`.
**Why it's wrong:** Breaks content-identity dedupe, generation safety, and manifest round-trips — the exact invariants the definition layer depends on.
**Do this instead:** Converters output CPU `ImportedModel`/image bytes; GPU publication goes through the one existing transactional path (per STRUCTURE.md "Where to Add New Code").

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| CI GPU runners (pixel tier) | Gated CTest lane with offscreen FBO capture + tolerance compare | No new middleware; software rasterizer (SwiftShader-style) acceptable only as explicitly-labeled fallback, never as pixel proof |
| JSON schema validators | Schemas checked in; validation implemented in owned C++ (no npm/package dependency) | Dependency prohibition stands — do not pull in a JSON-schema runtime library; hand-rolled bounded validation matching existing parser style |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Agent SDK ↔ Engine | C++ facade calls, one-way dependency (SDK → Engine) | SDK never reaches into `Engine/*/source/`; engine never includes SDK headers; versioned independently |
| Definition loader ↔ Scene/Manifest | Reuse `SceneSerializer` + `ResourceManifest` text formats as the persistence sub-layer | Do not invent a competing scene encoding; GameDefinition wraps/extends v2 + manifest keys |
| Validate CLI ↔ Engine | Links engine + definition lib; drives `Game` headless path | CLI output is the agent contract: stable text + JSON diagnostics modes, documented exit codes |
| Converters ↔ Registry | CPU structs → `ModelResourceImporter` → caches | Transactional + rollback-only-failed-aliases, same as existing importer |
| Templates ↔ SDK + Schemas | Template builds against installed SDK + validates against schemas in CI | A template that fails `PyramidValidate` blocks SDK/schema release |

## Build Order Implications (for roadmap phasing)

Dependencies dictate this sequence — each step unlocks the next, and reordering creates rework:

1. **Contracts first (schemas + examples-as-contracts skeleton).** No code dependencies; exposes real pain to guide surgery. Outputs: `docs/schemas/` v0 + one minimal template that initially validates by hand.
2. **NullDevice + headless host + `PyramidValidate` skeleton.** Depends only on existing `IGraphicsDevice`/`Window` interfaces. Unlocks the entire iteration loop; everything after is testable headlessly. Do this before any agent surface work (PROJECT.md key decision).
3. **GameDefinition loader + validator (over scene v2 + manifests).** Depends on (1) schemas and (2) headless harness for testing. Builds the data-first half of "both interfaces, data-first first."
4. **Agent SDK facade + game templates.** Depends on (2)+(3): facade methods are thin wrappers over load/validate/run flows proven in the harness. Freeze engine P0 correctness items before promising SDK stability.
5. **Converters + asset catalog.** Depends on (3) loader/diagnostics idioms (converters reuse the same diagnostic shape). Parallelizable with (4) once (3) lands.
6. **Proof: agent-built mini-game through agent path alone.** Depends on all above; must run with engine-code changes forbidden by CI (read-only engine checkout for the proof agent).

## Sources

- Pyramid baseline: `.planning/codebase/ARCHITECTURE.md`, `.planning/codebase/STRUCTURE.md`, `docs/Architecture.md`, `Engine/Graphics/include/Pyramid/Graphics/{GraphicsDevice,Scene/SceneSerializer,Resources/ResourceManifest}.hpp`, `Engine/Core/include/Pyramid/Core/Game.hpp`, `Tests/TestGraphicsDevice.hpp` — HIGH confidence (read directly).
- Manifest-driven data layers (atomic multi-file load, dependency order, load-time validation, providers): Hyperscape manifest architecture; O3DE prefab template/instance/link + JSON serializer notes — MEDIUM.
- Agent-first headless primacy (stdin/stdout harness, headless-as-primary, same primitives for human/agent): droids-engine — MEDIUM.
- Null-device pattern (`GPUDeviceNull` implementing full device interface, skipping render): FlaxEngine source — MEDIUM.
- Headless-vs-graphics test split (headless logic proofs vs GPU proofs; blueprint-assertion without instantiation): Renegade engine test strategy — MEDIUM.
- Headless-has-no-pixels trap + strict-vs-fallback GPU constructors: SummerEngine headless docs, scena headless-rendering docs — MEDIUM.
- One-way authoritative text data (JSON SOT, generation/propagation/sync separation): Unreal ALIS external-data architecture — MEDIUM.
- Offscreen framebuffer CI validation with tolerance tiers: Vulkan CI render-validation docs — LOW (pattern only, OpenGL adaptation needed).

---
*Architecture research for: agent-first / data-driven engine platform layers on Pyramid Engine*
*Researched: 2026-09-05*
