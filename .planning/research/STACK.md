# Stack Research: Agent-First Platform Layer

**Domain:** Data-first game definitions + headless validate/run loop + agent-facing SDK over existing C++17/OpenGL 3.3 engine
**Researched:** 2026-09-05
**Confidence:** HIGH (contract formats) / MEDIUM-HIGH (headless pattern)

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| JSON (RFC 8259 / ECMA-404, UTF-8) | Canonical encoding, no version | Canonical bytes for every data-first game definition: `game.json`, scene docs, manifest refs, gameplay/trigger docs, asset-catalog index | Agents emit JSON reliably; every language parses it; the engine already has deterministic v2 scene serialization with diagnostics — JSON extends that philosophy with zero new runtime deps. Human-editable, diffable, schema-validatable. This is the single canonical format — not one of several options. |
| JSON Schema Draft 2020-12 | `https://json-schema.org/draft/2020-12/schema` (published 2022-06-16, still current as of 2026) | Machine-readable contracts for every file format the agent touches; headless validator input; docs source | Current standard (verified on json-schema.org spec page: "The current version is 2020-12"). Self-describing meta-schema, `$id`/`$defs`/`$dynamicRef`/`unevaluatedProperties` cover versioned component schemas. Ships as plain files — no runtime dependency. Validators exist in every agent-side language; engine side implements a bounded Pyramid-owned subset (see below). |
| Pyramid-owned bounded JSON parser + Schema-subset validator (new code in `Libraries/`, C++17, no third-party) | New, versioned with engine (start at 0.7.0) | Parse + validate all game-definition JSON inside the engine binary and headless tools without adding middleware | Dependency ban (`vendor/glad` is the sole approved bundled runtime; no package-manager deps) rules out nlohmann/json, RapidJSON, yaml-cpp, toml++. The engine already owns PNG/JPEG codecs, OBJ/MTL parsing, TrueType parsing — a bounded JSON parser (objects/arrays/strings/numbers/booleans/null, depth + size + allocation limits, malformed-input diagnostics with line/column + JSON-Pointer path) is the same class of work and matches the fail-visibly test culture. Schema subset = `type/enum/required/properties/patternProperties/additionalProperties/items/prefixItems/min-max/anyOf-oneOf-allOf-not/if-then-else/$ref/$defs` — enough for component schemas; `format` stays annotation-only per 2020-12 format-annotation vocabulary. |
| `pyramid-validate` + `pyramid-run` headless CLI tools (Pyramid-owned, C++17, same engine binary path) | New, 0.7.0 | The agent iteration loop: `validate` = schema + reference + diagnostics pass with zero window/GPU; `run` = fixed-timestep headless simulation with scripted inputs and deterministic report | Industry pattern converges here: Godot `--headless` + `--check-only --script` + dummy audio driver for CI validation; Babylon `NullEngine` (deterministic lockstep, fixed timestep) for logic tests; O3DE/Unity screenshot-comparison suites for render regression. Pyramid needs the same two tiers: (1) pure-validate (no GL at all), (2) null-device run (GL calls stubbed, state tracked). Windowless + GPU-less runs are the prerequisite the PROJECT.md key decisions already call out ("headless before agent surface"). |
| Pyramid-owned `INullDevice` / null GL backend (interface behind renderer, stubs GLAD calls, tracks framebuffer/viewport state) | New, 0.7.0 | Lets `pyramid-run` execute the real scene/resource/simulation pipeline with no window and no GPU | OSMesa is dead as a strategy (removed upstream in Mesa 25.1, 2025; Linux-only EGL-surfaceless replacement; no Windows story) and Windows CI has no system software-GL guarantee. A null backend inverts the dependency: the engine defines the seam, GLAD stays the only GL loader, and validation never depends on a driver. Real-GPU image regression stays optional via existing WGL path on GPU machines (pbuffer/offscreen + owned PNG snapshots compared with threshold). |
| Agent SDK facade: Pyramid-owned stable C++17 headers (`Pyramid::Agent` / `Pyramid::Game`), SemVer + compat guarantees | New, 0.7.0+ | Simplified stable surface over Entity/scene v2, ResourceRegistry handles, action contexts, camera profiles, UIRenderer publication | Agents need a small stable target, not the full engine headers. Facade wraps existing systems (stable entity IDs, generational handles→null-on-stale, transactional imports, `InputConsumptionMask` ordering) with zero new runtime deps. Versioned (`AGENT_SDK_VERSION`, deprecation window, linkage tests in `Tests/PublicApiLinkage.cpp`). Data-first remains the primary agent path; the SDK is the escape hatch for logic the data DSL cannot express — per PROJECT.md "both interfaces, data-first first". |
| Data-first gameplay DSL: Pyramid-owned bounded JSON trigger/expression rules (no embedded language runtime) | New, 0.7.0 | Agent-authored gameplay logic (spawn conditions, win/lose, RTS orders, timers, counters) without compiling C++ or embedding Lua | PROJECT.md gates Baa behind an admission checklist and defers audio until agent games need it — embedding Lua 5.4/5.5 (or sol2/LuaBridge) now would violate the middleware ban and import GC/reentrancy/hot-reload risk for unproven gain. Precedent exists for JSON rulesets sharing the native pipeline (Nebulite: JSON rulesets + native rulesets on one runtime; Genie Fusion: JSON schemas validate engine files + Lua only as later layer). A bounded DSL (topics/conditions/actions over entity components + global counters, fixed-timestep evaluated, fully schema-validated) is sufficient for the RTS proof scenario and keeps every agent artifact validatable headlessly. |
| glTF 2.0.1 (ISO/IEC 12113:2022) — import-only interchange subset | Spec 2.0.1 (2021-10-11); ISO standard 2022; still current 2026 | External-format converter input (static meshes + material subset + images) alongside existing OBJ/MTL | Khronos/ISO standard, universal exporter support — the only sane interchange target. Import-only: bounded `Pyramid::Model` glTF-JSON subset parser (positions/normals/UVs/indices, one material subset, `KHR_materials_*` ignored with diagnostics), resolved through existing `ModelResourceImporter` + mesh/texture caches transactionally. Runtime formats stay owned (versioned manifests, `.pfont`); glTF never becomes a runtime dependency. |
| Owned snapshot regression: WGL pbuffer/offscreen capture + `Pyramid::Image` PNG encode + threshold compare (per-pixel + perceptual-hash gate) | Existing pieces (Image, framebuffers) + new harness | Automated render-image regression on GPU machines; null-device run asserts non-blank/command-stream hashes on GPU-less machines | Follows Unity Graphics Test Framework / O3DE Atom screenshot pattern (reference images + tolerance levels + actual/diff extraction) but implemented with owned codecs — no RenderDoc/Apitrace dependency. Reference images versioned beside tests; failures print actionable diff stats (pixel-diff %, worst region), never silent pass. |

### Supporting Libraries

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Python 3 stdlib only (`json`, `jsonschema` if preinstalled — else vendored check script, `hashlib`, `pathlib`) | System Python 3.10+ (dev/CI only, never runtime) | Cross-check schemas offline, converter scripts, asset-catalog generation, examples-as-contracts CI gate | Dev-time contract verification: validate every `schemas/*.schema.json` + example game definitions in CI with the reference `jsonschema` implementation to catch divergence between the Pyramid-owned C++ subset validator and the full spec. Converters stay stdlib-only so contributors need no `pip install`. |
| Existing owned libraries (Foundation, Math, Input, Image, Model, Font, Text, UI) | As shipped in 0.6.0 baseline | Reuse, do not duplicate: diagnostics, math, codecs, model parsing, text layout, widget/draw-list generation | Every new platform piece builds on these. New JSON parser lives in Foundation-adjacent owned code; game-definition loader consumes Model/Image/Font/Text; headless tools link the same libraries the engine links. |
| `vendor/glad` (sole bundled runtime) | As vendored | Only GL loader; null backend stubs its call surface | No change. Null device implements the same seam the WGL path implements — GLAD never initialised in validate/null-run paths. |

### Development Tools

| Tool | Purpose | Notes |
|------|---------|-------|
| `pyramid-validate` CLI (`--schema-dir --game --strict`, machine-readable `--report json` + human diagnostics) | Agent + CI entry point for "is this game definition legal and explain why not" | Exit non-zero on any error; diagnostics carry file + JSON-Pointer path + schema rule + fix hint. `--strict` promotes warnings (unknown fields, deprecated components) to errors for CI. Deterministic output ordering (mirrors scene v2 determinism). |
| `pyramid-run` CLI (`--frames N --timestep ms --input-script --snapshot-at --report`) | Deterministic headless playthrough: fixed timestep, scripted inputs, resource-load + simulation + null-render, summary report | Fixed timestep only (no wall-clock in reports); seeded RNG surfaced in report; scripted `InputState`/action-context injection reuses the generic action system. Snapshot hooks emit command-stream hashes every K frames so GPU-less runs still catch divergence. |
| JSON Schema files under version control (`schemas/<name>.v<major>.schema.json` with `$id`, `$schema: 2020-12`) | Single source of truth for every agent-authored format | Each schema has `$id` with version, `title`/`description` written for LLM consumption (constraints, defaults, examples inline), and a co-located `examples/` directory that doubles as contract tests. Breaking changes bump major + keep old schema for migration diagnostics. |
| Asset catalog (`assets/catalog.json` + content hashes, machine-readable metadata) | Curated versioned library agents browse without filesystem archaeology | Entries: id, version, kind, content-hash, license/provenance, schema-validated metadata, thumbnail/snapshot reference. Hashes reuse the content-addressed resource identity the engine already has. |

## Installation

```bash
# Runtime: nothing new. C++17 + OpenGL 3.3 + vendor/glad (already vendored).
# Supported toolchain stays MSYS2 UCRT64 MinGW-w64 GCC (Clang validated).
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug

# Dev-only contract cross-check (CI / contributor machines with Python 3):
python scripts/validate-contracts.py --schema-dir schemas --examples examples-as-contracts
```

No `npm install`, no `vcpkg install`, no `pip install`, no Conan. That is the point.

## Alternatives Considered

| Recommended | Alternative | When to Use Alternative |
|-------------|-------------|-------------------------|
| JSON for all game definitions | TOML v1.0.0 (Jan 2021) | Only for flat human-edited tool config (≤3 nesting levels) if a future tool needs hand config; never for scene/entity/component data (heterogeneous arrays, deep hierarchies, and null-vs-missing semantics fit JSON; TOML forbids heterogeneous arrays and null, gets verbose past 3 levels). |
| JSON for all game definitions | YAML 1.2 | Never for agent-authored data: 86-page spec, indentation-significant (silent mis-nesting), historic type-coercion footguns (`NO`→false, `3.10`→3.1, octal surprises) — agents amplify exactly these failure modes. Ecosystem mandate (K8s/CI) does not apply here. |
| Pyramid-owned JSON subset validator | Full-spec validators (nlohmann/json-schema-validator, RapidJSON Schema, Valijson) | Only if the dependency ban is explicitly lifted by discussion: they buy full 2020-12 coverage (`$dynamicRef` edge cases, `unevaluated*` full semantics) at the cost of a foreign runtime dependency inside the engine binary. Mitigation until then: bounded owned subset in-engine + full-spec Python cross-check in CI. |
| Null-device backend owned seam | OSMesa / Mesa software rasterizer | Never on this project's horizon: OSMesa was removed upstream in Mesa 25.1 (2025, conda-forge/mesa MR 33836), the sanctioned replacement (EGL surfaceless/llvmpipe) is Linux-only, and there is no Windows software-GL story that keeps the "no new middleware" constraint. |
| Null-device + optional WGL pbuffer snapshots | EGL headless / ANGLE / SwiftShader | Only when/if a Linux port (post-0.7.0, per roadmap sequence) needs GPU-less real rendering in CI — revisit then, still as an optional harness, never as engine runtime. |
| Bounded JSON gameplay DSL now | Embedded Lua 5.4.x / 5.5 (current: 5.5 Dec 2025; 5.4.8 widely bound via sol2/LuaBridge3) | Only after the Baa admission gate passes or Lua-becomes-necessary is proven by agent games the DSL cannot express. Lua is ~200KB and embeddable, but it is still new runtime middleware (plus a binding layer: sol2 or LuaBridge) prohibited by default, and it imports GC-pause, sandboxing, and determinism questions the headless loop would have to solve anyway. Data-first scripting now, language later — per PROJECT.md. |
| glTF 2.0.1 import subset | Assimp (multi-format importer) | Never by default: Assimp is exactly the kind of large runtime middleware the ban exists for ( importer surface, CVE history, nondeterministic version drift). Owned bounded parsers (OBJ/MTL exists; glTF-subset added) with limits + fixtures + transactional upload. |
| glTF 2.0.1 import subset | FBX / USD as interchange | Only on evidence: FBX SDK is proprietary and platform-tied; USD is a heavyweight dependency tree. Neither is needed for the agent asset library's curated catalog. |

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| nlohmann/json, RapidJSON, simdjson (runtime JSON) | New runtime middleware; violates the sole-`vendor/glad` rule; version drift inside the engine binary | Pyramid-owned bounded JSON parser in `Libraries/` with allocation limits + diagnostics |
| yaml-cpp / libyaml, toml++ / tomllib-in-engine | Same ban; YAML additionally brings coercion/indentation failure modes agents will trigger at scale | JSON for all game data; Python stdlib `tomllib` only in dev scripts if ever needed |
| sol2, LuaBridge, any Lua embedding now | New runtime + binding middleware before the language gate; GC, sandboxing, determinism surface area | Bounded JSON trigger/expression DSL evaluated fixed-timestep in-engine; Baa/Lua revisited post-gate |
| Assimp, FBX SDK, USD | Heavyweight importer middleware; contradicts owned-pipeline strategy (bounded OBJ/MTL, owned PNG/JPEG, owned `.pfont`) | Owned glTF-2.0.1 subset importer in `Pyramid::Model` → existing caches |
| OSMesa, Mesa llvmpipe-on-Windows, ANGLE-as-requirement | Removed/dead-end on Windows (OSMesa gone since Mesa 25.1); new platform dependency for zero engine benefit | `INullDevice` seam + WGL pbuffer snapshots where a GPU exists |
| vcpkg / Conan / npm-style package deps | Prohibited by default; breaks reproducible self-owned stack | Vendored-only (`glad`); stdlib-only dev scripts |
| New binary formats for game definitions (MessagePack, CBOR, custom binary) | Opaque to agents and humans; undiffable; unvalidatable without tooling; contradicts "examples-as-contracts" | Human-readable JSON + schemas; owned binary stays where it belongs (`.pfont` atlases, GPU resources) |
| SQLite / embedded DB for game definitions or catalog | Query engine inside the definition path; agents cannot read/write it with text tools; versioning/merging pain | Flat JSON files + `catalog.json` index with content hashes |
| C# / .NET scripting, Python-in-engine | Runtime weight (MBs), interop layers, platform drag — all for a problem the data DSL + SDK facade already cover | Stable C++17 facade + JSON DSL; Python stays dev-only stdlib scripts |

## Stack Patterns by Variant

**If the agent authors a new mini-game (the proof vehicle):**
- Use `game.json` + scene docs + manifest refs + catalog asset IDs only, validated by `pyramid-validate`, played by `pyramid-run --frames`.
- Because the entire loop stays in text + schemas + deterministic reports — no compiler, no window, no GPU required.

**If gameplay logic exceeds the DSL (complex RTS AI, custom systems):**
- Use the Agent SDK facade (stable C++ headers, game-side code in examples/games, never `Pyramid::Engine` internals).
- Because the facade is versioned and linkage-tested while engine internals keep evolving toward the freeze.

**If a contributor adds a new component type:**
- Add its JSON Schema (`schemas/`, versioned `$id`) + owned C++ validator cases + examples-as-contracts fixtures (valid + each invalid class) + `pyramid-validate` diagnostics text.
- Because contracts-first is the project's stated strategy ("machine-readable contracts are fast; structural surgery guided by exposed pain").

**If render regression is needed without a GPU (CI):**
- Use null-device run with command-stream hashes + logic assertions; reserve pixel snapshots for GPU machines.
- Because pixel comparison without a rasterizer is meaningless, but pipeline divergence is still detectable deterministically.

## Version Compatibility

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| Schemas `$schema: 2020-12` | Python `jsonschema` ≥ 4.x (dev cross-check), any 2020-12 agent-side validator | Flag `format` as annotation-only; do not rely on `$dynamicRef` beyond one level so the owned subset validator stays honest |
| `game.json` v1 + scene v2 | Engine 0.7.0 prereleases | Scene v2 serialization is the shipped baseline; game-definition versioning is additive (new optional fields, `minVersion` gating like glTF's `asset.minVersion` pattern) |
| glTF import subset | glTF 2.0 / 2.0.1 assets (`asset.version: "2.0"`), backward/forward-compatible per Khronos minor-version rules | Unknown extensions/chunks ignored with diagnostics, never hard errors — mirrors the spec's forward-compat requirement |
| TOML (if ever used for tool config) | TOML v1.0.0 | Parsers: Python 3.11+ stdlib `tomllib`; no engine parser |
| MSYS2 UCRT64 MinGW-w64 GCC / Clang | C++17, CMake presets (`gcc-debug-tests`, `gcc-release-tests`), OpenGL 3.3 / GLSL 3.30 | Unchanged baseline; headless tools build under the same presets and run under `ctest` |

## Sources

- json-schema.org Specification page + Draft 2020-12 docs (published 2022-06-16; "current version is 2020-12" verified 2026-09-05) — HIGH confidence
- Khronos glTF 2.0.1 spec registry (v2.0.1, 2021-10-11) + ISO/IEC 12113:2022 recognition — HIGH confidence
- TOML v1.0.0 (Jan 2021) vs YAML 1.2 comparisons (spec size ~30 vs ~86 pages; coercion/indentation analysis, 2026 guides) — HIGH confidence for format choice
- Mesa OSMesa removal (upstream MR 33836, Mesa 25.1, 2025; conda-forge feedstock discussion) + OSMesa docs — HIGH confidence for NOT-OSMesa
- Godot `--headless` + dedicated-server export docs, Godot headless-agent skill kit loop (patch → dry-run → smoke → logic tests → export), Godot CI export-test PR — MEDIUM-HIGH confidence for validate/run loop shape
- Unity Graphics Test Framework docs (reference/actual/diff image workflow, tolerance levels) + O3DE Atom screenshot tests — MEDIUM-HIGH confidence for snapshot-regression pattern
- Babylon NullEngine typings (deterministic lockstep options) — MEDIUM confidence, pattern reference only
- Lua 5.5 (Dec 2025) / 5.4.8 binding matrix (LuaBridge3), Lua-5.4-scripting opinion pieces, Nebulite JSON-ruleset engine, Genie Fusion JSON-schema+LUA SDK layering — MEDIUM confidence for DSL-now-language-later

---
*Stack research for: Pyramid Engine agent-first platform layer (data-first definitions, headless loop, Agent SDK, contracts, converters, asset library)*
*Researched: 2026-09-05*
