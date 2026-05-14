# Architecture Research: Pyramid Engine Foundation Refactor

**Project:** Pyramid Engine Foundation  
**Research dimension:** Architecture  
**Researched:** 2026-05-14  
**Overall confidence:** HIGH for current-code risks and CMake/C++ principles; MEDIUM for DX9-specific prototype shape until a backend spike validates it.

## Executive Recommendation

Pyramid should refactor toward a layered engine with explicit runtime ownership and a narrow renderer backend contract, not toward a large plugin engine or feature expansion. The current code already has useful seams (`Window`, `IGraphicsDevice`, resource interfaces, `RenderSystem`), but the seams are leaky: `Game.cpp` selects `Win32OpenGLWindow` directly, high-level renderer files include OpenGL types, `OpenGLDevice::BindFramebuffer(IFramebuffer*)` is incomplete, render commands can retain raw stale pointers, and public render-pass declarations exceed implementation. The foundation milestone should make these boundaries true before adding features.

Recommended dependency direction:

```text
Examples / Apps
    -> Engine/Core runtime API
    -> Engine/Renderer public API where needed

Core runtime
    -> Platform interfaces
    -> Graphics interfaces
    -> Utils logging

Renderer frontend
    -> Graphics interfaces
    -> Scene/Math public data
    -> no OpenGL/DX headers

Graphics backend implementations
    -> Platform context services
    -> vendor/native API headers privately

Platform implementations
    -> native OS/window/context APIs privately

Math / Utils
    -> no graphics/platform dependency except narrowly justified logging/config support
```

Use this milestone to decide and document ownership and module layout, then implement only the minimum needed to prove it: clean OpenGL backend separation, honest public render-pass APIs, a validated graphics resource lifetime model, conditional platform/backend CMake, and examples rewritten against the clean API. DX9 should be a prototype seam only: enough to prove `GraphicsAPI::DirectX9` can select a backend and that the renderer frontend is not OpenGL-bound, not enough to chase feature parity.

## Grounding in Current Pyramid Codebase

The existing codebase map identifies these architectural facts and risks:

| Area | Current state | Refactor implication |
|------|---------------|----------------------|
| Runtime | `Pyramid::Game` owns `std::unique_ptr<Window>` and `std::unique_ptr<IGraphicsDevice>` and runs a single-threaded loop. | Keep a simple runtime owner, but replace direct `Win32OpenGLWindow` construction with factories/configuration. |
| Platform | Only `Win32OpenGLWindow` exists; it creates both Win32 window and WGL context. | Split window/event ownership from graphics context ownership or explicitly document a combined platform surface. |
| Graphics | `IGraphicsDevice` factory selects only OpenGL; `GraphicsAPI` lists DirectX/Vulkan values. | Make unsupported APIs impossible or explicitly return unsupported status; add DX9 as prototype after OpenGL seam cleanup. |
| Renderer | `RenderSystem` and passes use command buffers, render targets, cameras, scene data. | Keep renderer frontend, but remove OpenGL includes/native handle binding from renderer code. |
| Framebuffers | `BindFramebuffer(IFramebuffer*)` logs not implemented while other code binds GL framebuffer internals directly. | Complete framebuffer as first backend-seam proof. |
| Resources | GPU resources are `shared_ptr`, but `CommandBuffer` stores raw pointers/IDs without generation/lifetime validation. | Introduce handles or a validated registry before expanding command recording. |
| CMake | Single `PyramidEngine` target with module subdirectories; unconditional `glad`/`opengl32`; placeholder modules. | Stay target-based, but make backend/platform source/link options conditional and stop placeholder module ambiguity. |
| Examples/docs | Examples demonstrate current internal patterns; README/docs overstate capabilities. | Rewrite examples as API contracts and align docs after technical decisions. |

## Component Boundaries and Dependency Direction Options

### Recommended Boundary Model

Use one installable engine target for now, but enforce logical submodules with header locations, private implementation directories, and CMake source grouping. Do not split into many binary libraries in this milestone unless CMake refactor work proves a specific need; per-module libraries increase link/export complexity before the APIs stabilize.

| Component | Responsibility | May depend on | Must not depend on |
|-----------|----------------|---------------|--------------------|
| `Core` | Application lifecycle, runtime configuration, factory orchestration, frame loop. | `Platform` interfaces, `Graphics` interfaces, `Utils`. | Concrete Win32/OpenGL/DX headers in public headers. |
| `Platform` | Windows/events/input surface, native window handles, graphics context creation services. | `Core` primitives only if unavoidable, `Utils`. | Renderer, scene, concrete graphics resources. |
| `Graphics` frontend | API-neutral device/resource contracts, capabilities, resource descriptions. | `Core` primitives, `Math`, `Utils`, platform context interfaces. | Renderer passes, scene-management internals, native API headers in public frontend headers. |
| `Graphics/OpenGL` backend | OpenGL device/resources/state/framebuffer implementation. | Graphics frontend, platform GL context service, `glad` privately. | App/examples, renderer frontend ownership decisions. |
| `Graphics/DX9` backend prototype | Direct3D 9 device/resource skeleton proving backend seam. | Graphics frontend, platform Win32 native handle service, `d3d9` privately. | Full deferred/shadow parity during this milestone. |
| `Renderer` frontend | Frame orchestration, render pass model, command validation, render targets. | Graphics frontend, scene/camera/math. | `glad`, `windows.h`, OpenGL/DX concrete classes. |
| `Scene` | Renderable world data, transforms, lights, bounds, culling inputs. | Math, graphics resource interfaces where truly needed. | Backend implementation headers. |
| `Math` | Numeric primitives and SIMD/scalar utilities. | Standard library; optionally Utils logging privately. | Graphics, Platform. |
| `Utils` | Logging, image loading, compression helpers. | Standard library/Core primitives. | Graphics backend lifetimes. |

### Dependency Direction Options

| Option | Shape | Recommendation | Rationale |
|--------|-------|----------------|-----------|
| A. Keep single `PyramidEngine`, enforce logical boundaries | One binary target, module CMake files register sources; public/private headers separate contracts. | **Use now.** | Fits current code, minimizes build churn, lets architecture be corrected before target exports get complex. |
| B. Split into many static libraries (`PyramidCore`, `PyramidGraphics`, etc.) | CMake target graph mirrors module graph. | Defer. | Better dependency enforcement later, but exposes unresolved cycles now and expands CMake/install work. |
| C. Backend plugin modules | OpenGL/DX as loadable modules. | Reject for milestone. | Too much ABI/plugin complexity for a learning/small-game engine foundation refactor. |

**Decision needing research/ADR:** Whether the long-term engine wants separate CMake targets per stable module.  
**Implementation detail:** Move includes and source files without creating new binary targets unless the ADR chooses target split.

## Platform / Window / Context Ownership

Current `Win32OpenGLWindow` owns both OS window and WGL context. That is workable for a single OpenGL backend but blocks DX9 and cross-platform growth because different graphics APIs need different context/device creation rules.

### Ownership Options

| Option | Description | Pros | Cons | Recommendation |
|--------|-------------|------|------|----------------|
| A. Window owns graphics context | `Window` creates and presents API-specific context (`Win32OpenGLWindow`). | Simple; matches current code. | Couples platform to OpenGL; DX9 does not fit; multiplies window classes by API. | Do not keep as architecture. Accept only as temporary compatibility bridge. |
| B. Platform window owns native window/events; backend owns graphics context/device | `IWindow` exposes native handle service; `OpenGLDevice` creates WGL context, `DX9Device` creates D3D device. | Clean API-specific ownership; platform layer not tied to one graphics API. | Requires careful shutdown order and native-handle abstraction. | **Recommended.** |
| C. Separate `IGraphicsContext` created by platform factory | Platform creates `IWindow` + `IGraphicsContext`; graphics device consumes context. | Explicit context object; good for testing. | More abstractions; may be overbuilt for DX9/OpenGL only. | Consider if Option B becomes messy; not first choice. |
| D. Adopt SDL/GLFW ownership | External library owns window/context/input. | Faster cross-platform path. | Big dependency/API decision; may hide platform-learning goals. | Research separately before choosing. |

Recommended runtime ownership:

```text
Game / ApplicationRuntime
  owns Window
  owns GraphicsDevice
  owns optional Renderer or exposes device to app

Window
  owns native OS window and event pump
  exposes NativeWindowHandle or platform service object

GraphicsDevice
  owns API device/context and all API-global state
  presents via backend-specific swap/present using window handle/context
```

For OpenGL, moving WGL context ownership from `Win32OpenGLWindow` into `OpenGLDevice` is architecturally cleaner, but it may be implemented in stages: first introduce interfaces and factories, then migrate context creation. For DX9, the backend should own `IDirect3D9`/`IDirect3DDevice9` because device loss/reset and swap chain behavior are graphics-backend concerns.

### Platform Ownership ADR Topics

1. `ADR-Platform-Window-Context-Ownership`: choose Option B unless SDL/GLFW research overturns it.
2. `ADR-Platform-Factory-And-Native-Handles`: define how `Game` requests a platform window without including `Windows.h` in public APIs.
3. `ADR-Event-And-Input-Scope`: decide whether input remains out of scope or a minimal event pump/input snapshot is required for examples.

## Renderer / Backend Seam

### Recommended Seam

The renderer frontend should speak in terms of device interfaces, resource descriptions, render-pass contracts, and backend-neutral handles. Backend code should be the only place that knows about OpenGL names, D3D interfaces, WGL, or `glad`.

```text
Renderer::RenderSystem
  records validated RenderCommands
  owns RenderTarget descriptions/front-end objects
  calls IGraphicsDevice methods

IGraphicsDevice
  creates resources
  exposes capabilities and limits
  binds framebuffers/render targets through interface methods
  executes immediate operations requested by command buffer

OpenGLDevice / DX9Device
  translate descriptions/commands to native API calls
  own native resource destruction and state cache
```

### Backend Seam Work Items

| Work item | Decision or implementation? | Notes |
|-----------|-----------------------------|-------|
| Complete `IFramebuffer` / render-target binding contract | Decision + implementation | First proof that renderer no longer binds GL handles directly. |
| Move OpenGL concrete headers out of frontend includes | Implementation after boundary ADR | Public API should not require `glad` or OpenGL concrete resource classes. |
| Add `DeviceCapabilities` and backend-supported feature flags | Decision | Needed so examples and renderer passes do not assume deferred/shadow features exist on all backends. |
| Make unsupported passes explicit | Implementation | Remove/hide unimplemented `TransparentPass`, `PostProcessPass`, `UIRenderPass`, `DebugRenderPass`, and `RenderPassFactory` or implement them. |
| Define render pass API honesty policy | Decision | Public headers must link and behave, or be marked experimental/internal. |
| Add DX9 prototype backend factory path | Implementation after OpenGL cleanup | Should prove creation, clear, present, maybe a triangle/resource stub; not full parity. |

### OpenGL Cleanup Sequence

1. Inventory frontend files that include `glad`, `OpenGLFramebuffer`, `OpenGLStateManager`, or OpenGL resource concrete classes.
2. Finish `IGraphicsDevice::BindFramebuffer(IFramebuffer*)` and equivalent render-target binding/unbinding methods.
3. Move native GL calls from `RenderSystem`/passes into `OpenGLDevice` or OpenGL-specific resource implementations.
4. Keep `OpenGLStateManager` private to the OpenGL backend; renderer may request state, but not manipulate cache internals.
5. Add tests or smoke validation around framebuffer bind/resize/present paths before DX9 work.

### DX9 Prototype Seam

Keep DX9 scope deliberately small:

| Prototype capability | Required in foundation milestone? | Reason |
|----------------------|------------------------------------|--------|
| Backend factory selection via `GraphicsAPI::DirectX9` | Yes | Proves public API and CMake/backend source selection. |
| Create device from Win32 native handle | Yes | Proves platform/backend ownership model. |
| Clear and present | Yes | Minimal runtime validation. |
| Vertex/index buffer skeleton | Optional | Useful if low cost; do not block foundation. |
| Shader/material parity | No | DX9 shader model differences can derail scope. |
| Deferred/shadow/render-pass parity | No | Explicitly out of scope. |
| Device lost/reset robustness | Research flag | DX9-specific lifecycle risk; document but avoid full solution unless clear/present requires it. |

**Decision needing research/ADR:** exact minimum DX9 prototype acceptance criteria.  
**Implementation detail:** Direct3D headers/libraries must remain private/conditional in CMake.

## Graphics Resource Lifetime Model

### Current Risk

`CommandBuffer` stores raw pointers as `std::uintptr_t` and maintains raw pointer registries keyed by numeric IDs. This is not an ownership model. It allows queued commands to outlive resources and makes errors visible only during command execution or native API use. OpenGL destructors also assume a current valid context.

### Recommended Model

Use explicit resource handles with generation counters for command streams, backed by registries owned by `GraphicsDevice` or `RenderSystem`, and use RAII wrappers internally for native resources. Avoid storing raw pointers in commands. For MVP foundation, a simpler `weak_ptr` validation layer is acceptable if generation handles are too large, but the ADR should select a direction.

| Model | Pros | Cons | Recommendation |
|-------|------|------|----------------|
| Raw pointers in commands | Fast/simple. | UAF/stale command risk; no invalidation. | Reject. |
| `shared_ptr` captured by commands | Prevents UAF. | Commands extend GPU resource lifetime unexpectedly; may delete on wrong context/thread. | Avoid as default. |
| `weak_ptr` commands + validation | Easy migration from current `shared_ptr` factories. | Still pointer-centric; weak expiry checks at execute time. | Accept as transitional. |
| Handle `{index,generation,type}` + registry | Strong validation; debuggable; backend controls destruction. | More code; requires registry design. | **Recommended target.** |

Recommended lifetime rules:

1. `IGraphicsDevice` owns native API lifetime policy and must be shut down before the window/native context is destroyed.
2. Public resource objects are lightweight RAII/front handles; native deletion occurs through backend-owned destructors or deferred-destroy queues while the context/device is valid.
3. Command buffers store handles, not native pointers or concrete backend objects.
4. Command recording validates command arguments early where possible; execution returns status and logs unresolved handles as errors, not only warnings.
5. Shutdown order is explicit: renderer drains/destroys commands and render targets -> graphics device destroys resources/context -> platform window destroys native window.

### Resource Lifetime ADR Topics

1. `ADR-Graphics-Resource-Handles`: handle registry vs transitional weak-pointer validation.
2. `ADR-Graphics-Shutdown-And-Context-Ownership`: context-current requirements for native deletion.
3. `ADR-CommandBuffer-Validation`: record-time vs execute-time validation and error policy.

## CMake Organization

Official CMake documentation describes buildsystems as target graphs and emphasizes target usage requirements (`PUBLIC`, `PRIVATE`, `INTERFACE`) for include directories, compile definitions, and link dependencies. Pyramid should lean into that instead of global include/link settings.

### Recommended CMake Shape

Keep `PyramidEngine` as the primary target for this milestone, but make all platform/backend dependencies conditional and private:

```cmake
add_library(PyramidEngine)

target_compile_features(PyramidEngine PUBLIC cxx_std_17)

target_sources(PyramidEngine PRIVATE
    # core/frontend/platform-neutral sources
)

if(WIN32)
    target_sources(PyramidEngine PRIVATE
        Engine/Platform/source/Windows/Win32Window.cpp
    )
endif()

if(PYRAMID_ENABLE_OPENGL)
    target_sources(PyramidEngine PRIVATE
        # OpenGL backend sources
    )
    target_link_libraries(PyramidEngine PRIVATE glad OpenGL::GL)
endif()

if(WIN32 AND PYRAMID_ENABLE_DX9)
    target_sources(PyramidEngine PRIVATE
        # DX9 prototype backend sources
    )
    target_link_libraries(PyramidEngine PRIVATE d3d9)
endif()
```

### CMake Decisions

| Topic | Recommendation | Why |
|-------|----------------|-----|
| CMake minimum | Align root, presets, README, docs on the preset-required version or a deliberate lower version. | Current 3.16 vs preset 3.23 mismatch causes support confusion. |
| Target organization | Single target now; possible module targets later. | Keeps foundation refactor focused. |
| Backends | `PYRAMID_ENABLE_OPENGL` default ON; `PYRAMID_ENABLE_DX9` OFF/experimental. | Avoids forcing DX9 on all users/builds. |
| Native libraries | Use `find_package(OpenGL)` / imported targets where available; link native libs privately. | Avoids unconditional `opengl32` non-Windows failure. |
| Placeholder modules | Remove from build or mark as intentionally empty/internal. | Avoids false API expectations for Audio/Input/Physics/Scene/Renderer. |
| Tests | Add GoogleTest with CTest registration in a dedicated tests tree, while keeping legacy executable tests during migration. | Matches project requirement and phase gates. |

### CMake ADR Topics

1. `ADR-CMake-Target-Strategy`: single target now vs per-module libraries.
2. `ADR-CMake-Backend-Options`: backend options, defaults, and platform guards.
3. `ADR-CMake-Test-Organization`: GoogleTest/CTest layout and migration from utility executables.

## Example and Public API Contracts

Examples should become executable API contracts. Because compatibility is not important, rewrite them to demonstrate the desired architecture rather than preserving current internal patterns.

### Recommended Example Set

| Example | Contract it validates | Scope |
|---------|-----------------------|-------|
| `BasicGame` | `Game`/runtime lifecycle, window creation, update/render hooks, shutdown order. | Minimal app loop; no advanced renderer assumptions. |
| `BasicRendering` | Backend-neutral resource creation, clear/present, simple draw where supported. | Uses only public frontend APIs. |
| `RendererPasses` or rewritten `BasicGame` scene path | RenderSystem pass setup and scene/camera contract. | Only include passes that are implemented and supported by active backend. |
| Optional `BackendSmokeDX9` | DX9 prototype clear/present. | Build only when `PYRAMID_ENABLE_DX9=ON` on Windows. |

### API Honesty Rules

1. No public class/function may be declared unless it links and has a documented behavior.
2. Experimental APIs must be in an experimental namespace/header or behind a build option, not mixed with stable public declarations.
3. `GraphicsAPI` enum values must map to supported factory paths or return a clear unsupported result; do not silently list Vulkan/DirectX as if complete.
4. Examples may not include backend concrete headers unless the example is explicitly backend-specific and named that way.
5. Docs and README must describe current capabilities, not roadmap intent.

## Suggested Build / Refactor Order

1. **ADR and boundary freeze**
   - Decide platform/window/context ownership, target strategy, backend option policy, resource handle direction, API honesty rules.
   - Scope-control value: prevents renderer/platform/CMake rewrites from fighting each other.

2. **CMake and source-visibility cleanup**
   - Align CMake minimum/presets/docs, add backend/platform options, remove unconditional `opengl32`, make OpenGL/DX dependencies private/conditional.
   - Scope-control value: creates safe build gates before invasive code moves.

3. **Platform/runtime factory split**
   - Introduce window/platform factory and graphics-device creation config; remove direct `Win32OpenGLWindow` include from `Game.cpp` path where possible.
   - Scope-control value: prepares both OpenGL cleanup and DX9 prototype.

4. **OpenGL seam cleanup and framebuffer contract**
   - Complete framebuffer binding, move GL calls out of renderer frontend, keep `OpenGLStateManager` backend-private.
   - Scope-control value: required before claiming multi-backend readiness.

5. **Resource lifetime model**
   - Replace raw command pointers/IDs with selected handle or weak-validation model; define shutdown/destruction order tests.
   - Scope-control value: prevents hidden UAF bugs while refactoring renderer.

6. **API honesty and render-pass surface**
   - Remove/implement unbacked render-pass declarations; expose capability checks; make examples use only honest APIs.
   - Scope-control value: stops docs/examples from overpromising.

7. **DX9 prototype seam**
   - Conditional Windows-only backend factory path for clear/present; no parity work.
   - Scope-control value: validates architecture without expanding into a second full renderer.

8. **Example rewrite and docs truth pass**
   - Rewrite `BasicGame`/`BasicRendering`; update docs after code stabilizes.
   - Scope-control value: examples become validation targets and user-facing contracts.

## Decisions Needing Research vs Implementation Details

| Topic | Needs ADR/research? | Implementation detail? | Notes |
|-------|---------------------|------------------------|-------|
| SDL/GLFW vs custom platform layer | Yes | No | Current milestone can define an interface first; external library choice is separate research. |
| Window/context ownership | Yes | No | Locks shutdown order and backend design. |
| Single target vs per-module CMake libraries | Yes | No | Recommended single target now, but record rationale. |
| Exact file moves inside `Engine/Graphics` | No | Yes | Do after boundary decisions. |
| Handle registry vs weak-pointer validation | Yes | No | Affects command buffer and resource factories. |
| Render pass API honesty policy | Yes | Some | Decide policy, then remove/implement declarations. |
| DX9 prototype acceptance criteria | Yes | Some | Must prevent parity creep. |
| OpenGL framebuffer binding implementation | No | Yes | Current gap is known and should be fixed. |
| CMake `PUBLIC`/`PRIVATE` usage requirements | No | Yes | Official CMake docs support target usage requirement discipline. |

## Risks and Scope-Control Implications

| Risk | Impact | Mitigation |
|------|--------|------------|
| Turning foundation refactor into full renderer rewrite | Milestone stalls. | Define backend seam MVP: OpenGL cleanup + DX9 clear/present only. |
| DX9 device-loss complexity expands scope | Backend work consumes milestone. | Document as research flag; prototype only startup/clear/present unless required. |
| Platform abstraction over-designed before non-Windows support exists | Too many unused interfaces. | Use Windows implementation with API-neutral contracts; defer SDL/GLFW/non-Windows implementation. |
| Per-module CMake target split exposes cycles and install/export churn | Build refactor dominates architecture work. | Keep one target now; use logical boundaries and private deps. |
| Resource handle registry becomes large subsystem | Renderer cleanup delays. | Allow transitional `weak_ptr` validation if handle registry exceeds phase budget. |
| Examples continue using concrete backend internals | Multi-backend promise remains false. | Treat examples as gate: no backend headers in general examples. |
| Public headers keep unimplemented declarations | Link-time failures persist. | API honesty phase must remove or implement every public declaration. |

## Validation Gates

Each implementation phase should pass:

1. Configure/build with examples and tests enabled.
2. CTest/GoogleTest suite for touched utilities, command/resource validation, and non-graphics logic.
3. OpenGL example smoke run on Windows.
4. Backend-option configure checks: OpenGL enabled, DX9 disabled; and DX9 enabled when implemented.
5. Header/API check: examples include only public frontend headers unless intentionally backend-specific.
6. Docs/build consistency check for changed options, examples, and claims.

## Sources

- Pyramid planning context: `.planning/PROJECT.md` (foundation constraints, decisions, active requirements) — HIGH confidence.
- Pyramid codebase maps: `.planning/codebase/ARCHITECTURE.md`, `STRUCTURE.md`, `CONCERNS.md`, `CONVENTIONS.md` — HIGH confidence for current structure and risks.
- CMake buildsystem documentation, current online docs fetched 2026-05-14: targets, usage requirements, `PUBLIC`/`PRIVATE`/`INTERFACE`, object/static/shared libraries — HIGH confidence. https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html
- CMake tutorial index, current online docs fetched 2026-05-14: subdirectories, target commands, testing/CTest, install/export topics — HIGH confidence. https://cmake.org/cmake/help/latest/guide/tutorial/index.html
- ISO C++ Core Guidelines, fetched 2026-05-14: resource safety, RAII, encapsulating messy constructs, interface intent — HIGH confidence as design guidance. https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
