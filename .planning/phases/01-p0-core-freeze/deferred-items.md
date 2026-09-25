# Deferred Items — Phase 01 P0 Core Freeze

## Deferred Items

- `OpenGLFramebuffer::SetDebugLabel` calls `glObjectLabel` without a
  `GLAD_GL_KHR_debug` version/pointer guard
  status: open
  **What:** `Engine/Graphics/source/OpenGL/OpenGLFramebuffer.cpp:1199` calls
  `glObjectLabel(GL_FRAMEBUFFER, ...)` unconditionally. `glObjectLabel` is a
  `GL_KHR_debug` / core-4.3 entry point, so on the locked OpenGL 3.3 baseline
  GLAD resolves it to `NULL` and the call is a null-function-pointer
  dereference — the exact failure mode RESEARCH §Common Pitfalls 1 warns about.
  Discovered while extending `Tests/FramebufferResizeTests.cpp` to drive
  `DeferredGeometryPass` under the fake-GL harness: the fake did not stub
  `glObjectLabel` and the process died with an access violation until the stub
  was added. The harness stub is in place; the engine-side guard is not.
  **Why deferred:** pre-existing and unrelated to plan 01-04's framebuffer
  binding migration and depth-target work (SCOPE BOUNDARY). Fixing it means
  adding a debug-extension guard to `OpenGLFramebuffer`, which is a separate
  change with its own test requirement (force the pointer null and assert the
  no-op).
  **Suggested owner:** a follow-up P0 hardening plan, or the Plan 01-04
  verifier if it judges the crash risk in-scope for the freeze.
