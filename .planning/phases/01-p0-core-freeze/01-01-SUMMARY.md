---
phase: 01-p0-core-freeze
plan: 01
subsystem: graphics
tags: [opengl, command-buffer, shader, scene-visibility, contract-honesty]

requires: []
provides:
  - Compile-time removal of the compute-dispatch surface (FRZ-01)
  - Compile-time removal of the occlusion-culling placeholder (FRZ-03)
  - P0 rationale entries in docs/ROADMAP.md and CHANGELOG.md
affects: [01-02, 01-03, 01-04, texture-format-mapping, shadow-map-array, framebuffer-binding]

actuals:
  tokens: 8000
  tasks: 3
  commits: 4

tech-stack:
  added: []
  patterns: [removal-hygiene-delete-declaration-definition-tests-together, explicit-failure-over-silent-drop, content-hash-version-bump-on-schema-change]

key-files:
  created: []
  modified:
    - Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp
    - Engine/Graphics/source/Renderer/CommandBuffer.cpp
    - Engine/Graphics/include/Pyramid/Graphics/Shader/Shader.hpp
    - Engine/Graphics/include/Pyramid/Graphics/OpenGL/Shader/OpenGLShader.hpp
    - Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp
    - Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderProgram.hpp
    - Engine/Graphics/source/Shader/ShaderProgram.cpp
    - Engine/Graphics/source/Material/Material.cpp
    - Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp
    - Engine/Graphics/source/Scene/SceneManager.cpp
    - Tests/ShaderCacheTests.cpp
    - Tests/MaterialCacheTests.cpp
    - Tests/MaterialResourceTests.cpp
    - Tests/ModelMaterialResourceImporterTests.cpp
    - Tests/ResourceHandleTests.cpp
    - Tests/ResourceRegistryTests.cpp
    - Tests/SceneSerializationTests.cpp
    - Tests/UIRendererTests.cpp
    - docs/ROADMAP.md
    - CHANGELOG.md

key-decisions:
  - "Full ShaderProgram compute-path removal (computeSource field, ShaderProgramType::Compute, IsCompute, Material guard) rather than narrow method deletion, so compute is inexpressible at compile time"
  - "Kept generic SSBO bindings with a documented rationale comment: stage-agnostic storage, not a compute-dispatch capability"
  - "Bumped shader content-hash schema to Content.v2 and re-pinned the graphics content ID after removing the compute field from the hash stream"

patterns-established:
  - "Removal hygiene: delete the declaration, definition, command-model value, and every test fake together; the linkage suite plus absence searches are the verification"

requirements-completed: [FRZ-01, FRZ-03]

coverage:
  - id: D1
    description: "Compute-dispatch surface removed end to end; misuse fails at compile time"
    requirement: "FRZ-01"
    verification:
      - kind: unit
        ref: "ctest --preset test-gcc-debug -R PublicApi|CommandBuffer"
        status: pass
      - kind: other
        ref: "repo-wide absence search for DispatchCompute|CompileCompute|RenderCommandType::Dispatch across Engine/Tests/Examples (zero matches)"
        status: pass
    human_judgment: false
  - id: D2
    description: "Occlusion-culling placeholder removed; visibility is frustum plus octree only"
    requirement: "FRZ-03"
    verification:
      - kind: unit
        ref: "ctest --preset test-gcc-debug -R Scene|Frustum|Octree|Culling|PublicApi"
        status: pass
      - kind: other
        ref: "repo-wide absence search for OcclusionCull|SetOcclusionCullingEnabled|occlusionCullingEnabled across Engine/Tests/Examples (zero matches)"
        status: pass
    human_judgment: false
  - id: D3
    description: "Removal rationale recorded in docs/ROADMAP.md P0 and CHANGELOG.md; full debug suite green"
    verification:
      - kind: integration
        ref: "ctest --preset test-gcc-debug (57/57 passed)"
        status: pass
    human_judgment: false

duration: 35min
completed: 2026-09-12
status: complete
---

# Phase 01 Plan 01: Remove Compute-Dispatch and Occlusion-Culling Stubs Summary

**Compute dispatch and occlusion culling deleted outright across the command model, shader stack, and scene manager, with rationale docs and a green 57-test suite.**

## Performance

- **Duration:** 35 min
- **Started:** 2026-09-12T01:06:10Z
- **Completed:** 2026-09-12T01:41:18Z
- **Tasks:** 3
- **Files modified:** 20 (8 engine, 8 tests, 2 docs) plus this SUMMARY

## Accomplishments

- Compute dispatch is inexpressible in code: `RenderCommandType::Dispatch`, `CommandBuffer::Dispatch`, `IShader::CompileCompute`/`DispatchCompute`, the `OpenGLShader` implementations, and the full `ShaderProgram` compute path are gone; any agent attempt fails at compile time (FRZ-01, per D-01 REMOVE).
- Occlusion culling is inexpressible in code: `SetOcclusionCullingEnabled`, `OcclusionCull`, and `m_occlusionCullingEnabled` are gone; visibility is observably frustum plus octree only (FRZ-03, per D-02 REMOVE).
- Rationale recorded where agents look: one P0 line each in `docs/ROADMAP.md` plus `CHANGELOG.md` entries naming every deleted symbol.
- Full debug build with zero errors and full `ctest --preset test-gcc-debug` green (57/57), including the linkage suite and all frustum/octree/scene suites.

## Task Commits

Each task was committed atomically:

1. **Task 1: Remove compute-dispatch surface end to end** - `3062d96` (feat)
2. **Task 2: Remove occlusion-culling placeholder end to end** - `6191b4f` (feat)
3. **Task 3: Record removal rationale and prove contract honesty** - `b5039fc` (docs)

## Files Created/Modified

- `Engine/Graphics/include/Pyramid/Graphics/Renderer/RenderSystem.hpp` - Dispatch enum value, dispatch union member, and Dispatch declaration deleted
- `Engine/Graphics/source/Renderer/CommandBuffer.cpp` - Dispatch record method and Execute log-and-drop arm deleted; default loud-WARN arm kept
- `Engine/Graphics/include/Pyramid/Graphics/Shader/Shader.hpp` - CompileCompute/DispatchCompute pure virtuals deleted; SSBO keep-rationale comment added
- `Engine/Graphics/include/Pyramid/Graphics/OpenGL/Shader/OpenGLShader.hpp` - compute override declarations deleted (Rule 3)
- `Engine/Graphics/source/OpenGL/Shader/OpenGLShader.cpp` - CompileCompute/DispatchCompute definitions deleted
- `Engine/Graphics/include/Pyramid/Graphics/Shader/ShaderProgram.hpp` - compute overrides, computeSource field, ShaderProgramType enum, GetType/IsCompute, m_type deleted (Rule 3)
- `Engine/Graphics/source/Shader/ShaderProgram.cpp` - compute validation/compilation/hash branches deleted; content-hash schema bumped to Content.v2 (Rule 2/3)
- `Engine/Graphics/source/Material/Material.cpp` - dead IsCompute material guard deleted (Rule 3)
- `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp` - occlusion setter, OcclusionCull declaration, and member deleted
- `Engine/Graphics/source/Scene/SceneManager.cpp` - occlusion filter block, stub definition, and constructor initializer deleted
- `Tests/ShaderCacheTests.cpp` - FakeShader compute overrides removed; compute-path test replaced with empty-spec rejection; cache stats recounted (misses 3, created 4, residents 4, aliases >= 6, collected 4); v2 content ID re-pinned to `169d157c7b97692700583a44c06f8179`
- `Tests/MaterialCacheTests.cpp`, `Tests/MaterialResourceTests.cpp`, `Tests/ModelMaterialResourceImporterTests.cpp`, `Tests/ResourceHandleTests.cpp`, `Tests/ResourceRegistryTests.cpp`, `Tests/SceneSerializationTests.cpp`, `Tests/UIRendererTests.cpp` - FakeShader compute overrides removed (Rule 3, required to compile)
- `docs/ROADMAP.md` - P0 rationale lines for both removals, both marked [x]
- `CHANGELOG.md` - new `P0 core freeze` section naming all deleted symbols (single Wave-1 owner per plan)

## Decisions Made

- Full rather than narrow compute removal: deleting only the two `IShader` methods would have left `computeSource`, `ShaderProgramType::Compute`, and `IsCompute()` as settable-but-dead surface (a keep-reserved-with-explicit-failure fallback the researcher ranked below removal). Full deletion makes compute inexpressible at compile time, matching the plan's done-criteria.
- Kept generic SSBO bindings (`BindShaderStorageBuffer`, `SetShaderStorageBlockBinding`, `CreateShaderStorageBuffer`, `IShaderStorageBuffer` subsystem): audit found them stage-agnostic with no compute-exclusive call sites, so per the plan they stay, with a rationale comment on the `IShader` declarations.
- Bumped the shader content-hash schema (`Pyramid.ShaderProgram.Content.v1` to `v2`) because removing `computeSource` from the hash stream changes every content ID; re-pinned the graphics content ID by running the suite (`169d157c7b97692700583a44c06f8179`).
- `Tests/PublicApiLinkage.cpp` needed no edit: it never pinned any removed symbol (verified by full read), so the plan's conditional pin-removal was a verified no-op.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Extended removal to ShaderProgram, OpenGLShader header, Material guard, and 8 test fakes**
- **Found during:** Task 1 (Remove compute-dispatch surface end to end)
- **Issue:** Plan file list omitted `OpenGLShader.hpp`, `ShaderProgram.hpp/.cpp`, `Material.cpp`, and the per-file `IShader` test fakes, but deleting the `IShader` pure virtuals breaks every override (`override` on a non-existent base method is a hard error) and leaves `CompileSpecification` calling a deleted method. The plan's own acceptance criteria (zero occurrences of `DispatchCompute`/`CompileCompute` in Engine/Tests/Examples) are unsatisfiable without these files.
- **Fix:** Removed compute overrides, the `ShaderProgram` compute specification path, the `Material` compute guard, and all fake overrides; repurposed the mixed compute+graphics rejection test as an empty-spec rejection test.
- **Files modified:** `OpenGLShader.hpp`, `ShaderProgram.hpp/.cpp`, `Material.cpp`, all 8 test files listed above
- **Verification:** Full engine+tests build with zero errors; absence searches return zero; `Graphics.ShaderCache`, `Graphics.MaterialResource`, `Graphics.MaterialCache`, and all resource suites pass
- **Committed in:** `3062d96` (Task 1 commit)

**2. [Rule 2 - Missing Critical] Bumped shader content-hash schema version and re-pinned the stability hash**
- **Found during:** Task 1
- **Issue:** Removing `computeSource` from `HashSpecification` silently changes every shader content ID; keeping the `Content.v1` tag would misrepresent a schema break, and the pinned stability hash in `ShaderCacheTests` no longer matched.
- **Fix:** Bumped the schema tag to `Content.v2` and re-pinned the graphics content ID from a real run (`169d157c7b97692700583a44c06f8179`); removed a self-introduced `unused variable hasGeometry` warning in the same function.
- **Files modified:** `Engine/Graphics/source/Shader/ShaderProgram.cpp`, `Tests/ShaderCacheTests.cpp`
- **Verification:** `Graphics.ShaderCache` passes with the new pin; no new compiler warnings from touched files
- **Committed in:** `3062d96` (Task 1 commit)

**3. [Rule 3 - Blocking] Recounted ShaderCache statistics after deleting the compute-program block**
- **Found during:** Task 1
- **Issue:** Deleting the compute `GetOrCreate` block changed deterministic cache counters asserted by the test (misses, creations, residents, aliases, collected).
- **Fix:** Recounted from `ShaderCache.cpp` alias logic: misses 4→3, created 5→4, residents 5→4, resident-asset floor 7→6, collected 5→4.
- **Files modified:** `Tests/ShaderCacheTests.cpp`
- **Verification:** `Graphics.ShaderCache` passes
- **Committed in:** `3062d96` (Task 1 commit)

---

**Total deviations:** 3 auto-fixed (1 missing critical, 2 blocking)
**Impact on plan:** All three were required for correctness and compilation; no scope creep. No new engine capabilities added. No architectural changes (Rule 4 never triggered).

## Issues Encountered

- `CHANGELOG.md` and `docs/ROADMAP.md` are tracked as binary (CRLF/encoding), so the Task 3 commit stat shows `Bin ... bytes` with 0 insertions/deletions; committed content was verified line-by-line via `git show HEAD:<path>`.
- `Tests/PublicApiLinkage.cpp` required no changes (no removed symbol was pinned) — plan-conditional step verified as a no-op by full-file read, not skipped.
- Pre-existing warnings in `DeferredLightingPass.cpp` (unused `cmd` parameters) are out of scope and were left untouched.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- Ready for plan 01-02 (FRZ-04 texture formats), 01-03 (FRZ-02 shadow-map-array), then 01-04 (FRZ-05 framebuffer + depth targets).
- Plans 01-02 and 01-03 must not edit `CHANGELOG.md` — their entries consolidate through plan 01-04 per this plan's Task 3.
- Renderer changes in later plans still require human visual inspection (smoke tests are not pixel validation per AGENTS.md).

---
*Phase: 01-p0-core-freeze*
*Completed: 2026-09-12*

## Self-Check: PASSED

- All 20 modified files verified present on disk; all 3 task commits (`3062d96`, `6191b4f`, `b5039fc`) verified in git log.
- All plan acceptance criteria re-verified: absence searches zero in Engine/Tests/Examples; full suite 57/57 green.
