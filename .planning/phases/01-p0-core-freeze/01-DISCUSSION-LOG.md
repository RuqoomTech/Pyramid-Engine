# Phase 1: P0 Core Freeze - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-09-11
**Phase:** 1-P0 Core Freeze
**Areas discussed:** Compute dispatch fate, Occlusion culling fate, Texture/depth/framebuffer, Hardware verification scope

---

## Compute dispatch fate

| Option | Description | Selected |
|--------|-------------|----------|
| Remove Dispatch (Recommended) | GL 3.3 has no compute shaders; drop Dispatch + CompileCompute/DispatchCompute until a real backend exists | |
| Keep reserved + explicit failure | Keep the API surface but fail loudly like depth-target creation does | |
| You decide | Researcher/planner decide implement-vs-remove | ✓ |

**User's choice:** You decide
**Notes:** Bounding follow-up (does anything on the agent path need GPU compute?) also delegated. Recorded as agent discretion bounded by GL 3.3-has-no-compute fact and the no-silent-no-op rule.

---

## Occlusion culling fate

| Option | Description | Selected |
|--------|-------------|----------|
| Remove setting (Recommended) | Flag with no algorithm misrepresents support; frustum + octree carry the load until a real technique earns its own phase | |
| Implement technique | Software Hi-Z or GPU occlusion queries with Tests coverage | |
| You decide | Researcher/planner decide implement-vs-remove | ✓ |

**User's choice:** You decide
**Notes:** No bounding follow-up asked — user had delegated twice already; recorded as discretion bounded by existing frustum + octree coverage and explicit-failure-on-removal.

---

## Texture/depth/framebuffer

| Option | Description | Selected |
|--------|-------------|----------|
| Prune enums (Recommended) | Delete unsupported TextureFormat values so unmapped formats are a compile-time signal, not runtime failure | |
| Map everything | Implement mappings for every advertised format through OpenGLTexture | |
| You decide | Researcher/planner decide map-vs-prune | ✓ |

**User's choice:** You decide
**Notes:** Depth-texture and neutral-BindFramebuffer sub-questions folded into CONTEXT.md bounding facts rather than asked separately, given the delegation pattern.

---

## Hardware verification scope

| Option | Description | Selected |
|--------|-------------|----------|
| Code-only, HW at gate (Recommended) | Phase 1 stays code + CI-testable; real-GPU checks gate the pre-release tag, not the phase | |
| Full HW verification in phase | Both examples on real GPU, resize/minimize/restore/close checks, OpenGL error + screenshot capture all inside Phase 1 | |
| You decide | Researcher/planner decide verification scope | ✓ |

**User's choice:** You decide
**Notes:** Pre-release exit criteria from docs/ROADMAP.md recorded as bounding facts in CONTEXT.md.

---

## the agent's Discretion

All four areas: Compute dispatch fate, Occlusion culling fate, Texture/depth/framebuffer, Hardware verification scope.

## Deferred Ideas

None — discussion stayed within phase scope.
