---
phase: 01
slug: p0-core-freeze
# status lifecycle: draft (seeded by plan-phase) → validated (set by validate-phase §6)
# audit-milestone §5.5 distinguishes NOT-VALIDATED (draft) from PARTIAL (validated + nyquist_compliant: false) (#2117)
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-09-11
---

# Phase 01 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CTest (CMake presets) |
| **Config file** | `CMakePresets.json` (`gcc-debug-tests` / `gcc-release-tests`) |
| **Quick run command** | `cmake --build --preset build-gcc-debug-tests` then focused `ctest -R <TestName>` |
| **Full suite command** | `ctest --preset test-gcc-debug` |
| **Estimated runtime** | ~unknown — measure during first wave, record here |

---

## Sampling Rate

- **After every task commit:** Run quick build plus the focused suite for touched areas
- **After every plan wave:** Run `ctest --preset test-gcc-debug`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** record measured full-suite time here once known

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Threat Ref | Secure Behavior | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|------------|-----------------|-----------|-------------------|-------------|--------|
| 01-01-T1 | 01-01 | 1 | FRZ-01 | T-01-01/02 | Removal fails at compile time, never silent drop | build + linkage + unit | `cmake --build --preset build-gcc-debug-tests` then `ctest --preset test-gcc-debug -R "PublicApi\|CommandBuffer"` | Tests/PublicApiLinkage.cpp | ⬜ pending |
| 01-01-T2 | 01-01 | 1 | FRZ-03 | T-01-03 | Visibility parity on frustum plus octree | build + unit | `ctest --preset test-gcc-debug -R "Scene\|Frustum\|Octree\|Culling\|PublicApi"` | Engine/Graphics/source/Scene/SceneManager.cpp | ⬜ pending |
| 01-01-T3 | 01-01 | 1 | FRZ-01/03 | T-01-04 | Rationale documented; absence proven | full suite | `ctest --preset test-gcc-debug` | docs/ROADMAP.md, CHANGELOG.md | ⬜ pending |
| 01-02-T1 | 01-02 | 1 | FRZ-04 | T-02-01/03 | RGBA16F type-correct upload; malformed fails explicitly | unit | `ctest --preset test-gcc-debug -R "Texture"` | Tests/TextureLoadingTests.cpp | ⬜ pending |
| 01-02-T2 | 01-02 | 1 | FRZ-04 | — | One-way BC7 decision explicitly recorded | decision gate | reviewer selects prune or keep-explicit-failure | — | ⬜ pending |
| 01-02-T3 | 01-02 | 1 | FRZ-04 | T-02-02/04 | Full table mapped; S3TC gated; BC7 resolved | unit | `ctest --preset test-gcc-debug -R "Texture\|Resource"` | Tests/TextureCacheTests.cpp | ⬜ pending |
| 01-03-T1 | 01-03 | 1 | FRZ-02 | T-03-02 | One-layer array binds with correct target plus uploads | unit | `ctest --preset test-gcc-debug -R "Shadow\|Framebuffer\|Lighting"` | Tests/ShadowArrayTests.cpp | ⬜ pending |
| 01-03-T2 | 01-03 | 1 | FRZ-02 | T-03-01/03/04 | N-cascade array; restore preserved; empty explicit | unit | `ctest --preset test-gcc-debug -R "Shadow\|Framebuffer\|Render"` | Tests/ShadowArrayTests.cpp | ⬜ pending |
| 01-03-T3 | 01-03 | 1 | FRZ-02 | — | Human confirms cascade pixels | manual visual | screenshots from Examples/BasicRendering on GPU | — | ⬜ pending |
| 01-04-T1 | 01-04 | 2 | FRZ-05 | T-04-01 | Neutral bind real with symmetric errors | unit | `ctest --preset test-gcc-debug -R "Framebuffer\|PublicApi"` | Engine/Graphics/include/Pyramid/Graphics/Framebuffer.hpp | ⬜ pending |
| 01-04-T2 | 01-04 | 2 | FRZ-05 | T-04-02/03/04 | Routed restores; depth targets transactional | unit | `ctest --preset test-gcc-debug -R "Framebuffer\|Texture\|Resource\|Render"` | Engine/Graphics/source/Texture.cpp | ⬜ pending |
| 01-04-T3 | 01-04 | 2 | FRZ-05 | — | Human confirms routing pixels plus lifecycle | manual visual | screenshots from both examples on GPU | — | ⬜ pending |

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements: `Tests/` suites, `CMakePresets.json`, `Tests/PublicApiLinkage.cpp`, `scripts/run-smoke.ps1`. No new framework or fixture scaffolding expected; confirm during planning.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Multi-cascade shadow rendering looks correct (no popping, all cascades bound) | FRZ-02 | Smoke tests are not pixel validation (AGENTS.md) | Run `Examples/BasicRendering` on real GPU, capture screenshots, human-inspect cascade transitions |
| On-hardware runtime behavior (resize/minimize/restore/close, shutdown) | D-04 | Cannot be proven headless in CI | Execute `docs/ROADMAP.md` P0 Windows runtime verification checklist on OpenGL 3.3 minimum hardware |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency recorded above
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
