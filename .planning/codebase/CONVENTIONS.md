# Coding Conventions

**Analysis Date:** 2026-09-05

## Naming Patterns

**Files:**
- Use `PascalCase` for public headers and matching sources: `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`, `Engine/Graphics/source/Scene.cpp`, `Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Core/source/Game.cpp`
- Use `PascalCase` for test executables mirroring the unit under test: `Tests/EntitySceneTests.cpp`, `Tests/SceneSerializationTests.cpp`, `Tests/ResourceHandleTests.cpp`, `Tests/CameraFrustumTests.cpp`, `Tests/ModelResourceImporterTests.cpp`
- Use `PascalCase.hpp` for shared test fakes: `Tests/TestGraphicsDevice.hpp`
- Use `*Tests.cpp` / `Test*.cpp` suffix for every test translation unit; one executable per file (see `Tests/CMakeLists.txt`)
- Library tests live under `Libraries/<Name>/test/`: `Libraries/PyramidFoundation/test/FoundationTests.cpp`, `Libraries/PyramidModel/test/ObjFailureTests.cpp`, `Libraries/PyramidImage/test/TestJPEGRobustness.cpp`

**Types and methods:**
- Use `PascalCase` for all types and public methods. Examples from `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`:
```cpp
class EntityId final { bool IsValid() const; u64 GetValue() const; };
class Entity final { bool IsValid() const; bool SetParent(Entity parent); Math::Vec3 GetWorldPosition() const; };
struct MeshRendererComponent { bool visible = true; };
enum class LightType : u8 { Directional, Point, Spot, Area };
```
- Use `PascalCase` for interface methods even when overriding: `void Bind() override {}`, `bool Initialize() override` in `Tests/TestGraphicsDevice.hpp` and `Engine/Platform/include/Pyramid/Platform/Window.hpp`
- Use `GetX` / `SetX` / `IsX` / `HasX` / `TryX` prefixes consistently: `GetGraphicsDevice()`, `IsInitialized()`, `IsRenderSurfaceAvailable()`, `HasMeshRenderer()`, `TryParse()`, `TryGetLocalBounds()` (`Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`)
- Use `Create` / `Acquire` / `Calculate` / `Resolve` for factories: `Mesh::Create()`, `ResourceRegistry::AcquireMesh()`, `Mesh::CalculateContentId()`, `Platform::ResolveRuntimePath()`, asset-id factories `MeshAssetId::FromString()`, `EntityId::TryParse()`

**Variables and fields:**
- Use `camelCase` for locals and parameters: `vertexDataSize`, `drawCount`, `renderSurfaceAvailable`, `gameplayContext` (`Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`, `Tests/Consumer/main.cpp`)
- Prefix all non-static member fields with `m_`: `m_window`, `m_graphicsDevice`, `m_resourceRegistry`, `m_activeCamera`, `m_lastError`, `m_vertexCount` (`Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Graphics/include/Pyramid/Graphics/OpenGL/OpenGLDevice.hpp`)
- Prefix file-local constants with `k`: `kFnvPrime`, `kPrimaryOffset`, `kSecondaryOffset` (`Engine/Graphics/source/Geometry/Mesh.cpp`)
- Suffix stats/counters descriptively, initialize inline: `u32 drawCalls = 0; u64 cacheHits = 0; u32 residentMeshes = 0;` (`Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Geometry/MeshCache.hpp`)
- Use fixed-width aliases from `Libraries/PyramidFoundation/include/Pyramid/Core/Prerequisites.hpp` everywhere, never raw `int`/`unsigned` for engine data: `u8`, `u16`, `u32`, `u64`, `i32`, `f32`, `f64`

**Namespaces and aliases:**
- Put all engine code in `namespace Pyramid`, nested `Pyramid::Tests`, `Pyramid::Math`, `Pyramid::Util`, `Pyramid::Renderer`, `Pyramid::Examples::RTSReference`
- Reference link targets as `Pyramid::Engine`, `Pyramid::Foundation`, `Pyramid::Math`, `Pyramid::Input`, `Pyramid::Image`, `Pyramid::Model`, `Pyramid::Font`, `Pyramid::Text`, `Pyramid::UI` in every `target_link_libraries()` call

## Code Style

**Formatting:**
- No formatter config is checked in (no `.clang-format`, `.eslintrc`, `biome.json`). Enforce style manually in review.
- Use C++17 only: `set(CMAKE_CXX_STANDARD 17)`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF` (`CMakeLists.txt`)
- Use 4 spaces, no tabs. Continuation indent is 4 spaces.
- Place opening braces on new lines for namespaces, types, functions, and control flow:
```cpp
namespace Pyramid
{
    bool Mesh::IsValid() const
    {
        if (!m_texture)
        {
            return false;
        }
        return true;
    }
}
```
- This brace style is mandatory in `Engine/`, `Libraries/`, `Tests/`, and `Examples/` — match `Engine/Core/source/Game.cpp`, `Engine/Graphics/source/Geometry/Mesh.cpp`, `Tests/EntitySceneTests.cpp`
- Always use `#pragma once` as the first line of headers, never `#ifndef` guards (`Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`, `Tests/TestGraphicsDevice.hpp`)
- Mark single-argument constructors `explicit`: `explicit Game(GraphicsAPI api = GraphicsAPI::OpenGL);`, `explicit EntityId(u64 value)`, `explicit TestTexture(TextureSpecification specification)` (`Engine/Core/include/Pyramid/Core/Game.hpp`)
- Mark owning resource types `final` and delete copy/move when identity matters: `class Mesh final { Mesh(const Mesh&) = delete; ... ~Mesh() = default; }` (`Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`)

**Linting / warnings:**
- No clang-tidy or ESLint gate. The compiler warning set is the linter.
- GCC/Clang builds must compile clean under `-Wall -Wextra -Wpedantic`; MSVC under `/W4 /permissive-` (`Engine/CMakeLists.txt`)
- Opt in to warnings-as-errors with `-DPYRAMID_WARNINGS_AS_ERRORS=ON` (`-Werror` / `/WX`). Fix the warning, never suppress it inline without a comment explaining why.
- Keep `PYRAMID_BUILD_TESTS=ON` builds warning-clean; `Tests/` executables inherit the same flags via the preset chain in `CMakePresets.json`

**Include order:**
- In a `.cpp` file, include its own header first using the quoted build-interface path, then remaining `Pyramid/` headers, then C++ standard headers:
```cpp
#include "Pyramid/Core/Game.hpp"
#include "Pyramid/Platform/Windows/Win32OpenGLWindow.hpp"
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Util/Log.hpp>
#include <memory>
#include <chrono>
```
(`Engine/Core/source/Game.cpp`)
- In headers, include only what the declaration needs (`<cstddef>`, `<memory>`, `<string>`, `<string_view>`, `<vector>` in `Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`); put I/O and algorithm includes in the `.cpp`
- Never add `vendor/` includes to public headers; `vendor` is a `PRIVATE` include directory (`Engine/CMakeLists.txt`). Only `vendor/glad` is approved.

## Import Organization

**Order:**
1. Matching header for the translation unit (`#include "Pyramid/Core/Game.hpp"`, `#include <Pyramid/Graphics/Geometry/Mesh.hpp>`)
2. Other `Pyramid/` headers grouped by layer (Core/Graphics/Platform, then Libraries: `Pyramid/Util/Log.hpp`, `Pyramid/Math/Math.hpp`)
3. Third-party (`glad`, OpenGL) — only in `.cpp` files or private headers
4. C/C++ standard library (`<memory>`, `<string>`, `<vector>`, `<filesystem>`, `<limits>`)

**Path Aliases:**
- Include with the installed logical path in all cases: `#include <Pyramid/Graphics/Scene.hpp>`, `#include <Pyramid/Util/Log.hpp>`, `#include <Pyramid/Model/ObjImporter.hpp>` — never relative `../` includes across modules
- Test-only helpers use a quoted local include: `#include "TestGraphicsDevice.hpp"` (`Tests/SceneSerializationTests.cpp`, `Tests/ResourceHandleTests.cpp`, `Tests/TextureCacheTests.cpp`)
- Link with namespaced imported targets, never raw library file names: `target_link_libraries(EntitySceneTests PRIVATE Pyramid::Engine)`, `target_link_libraries(InputStateTests PRIVATE Pyramid::Input)` (`Tests/CMakeLists.txt`)

## Error Handling

**Patterns — use all of these, do not invent new ones:**
- Return `bool` for fallible operations and expose the reason via `GetLastError()` / `std::string& error` / `std::string* error`. Never throw from engine code. The only `catch` blocks in `Engine/` are `catch (const std::bad_alloc&)` → return `nullptr`/`false` and `catch (const std::exception&)` at the Octree serialization boundary (`Engine/Graphics/source/Geometry/Mesh.cpp`, `Engine/Graphics/source/Texture/TextureResource.cpp`, `Engine/Graphics/source/Shader/ShaderProgram.cpp`, `Engine/Graphics/source/Material/Material.cpp`, `Engine/Graphics/source/Scene/Octree.cpp`)
```cpp
static std::shared_ptr<Mesh> Create(IGraphicsDevice& device, const MeshSpecification& specification);
static MeshAssetId CalculateContentId(const MeshSpecification& specification); // invalid id == malformed
bool IsValid() const;
std::string GetLastError() const override;
```
(`Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`, `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`)
- Use the `Validate → return invalid/null/false + SetError` idiom for content creation. Example from `Engine/Graphics/source/Texture/TextureResource.cpp`:
```cpp
// Validate(...) fills std::string& error, logs PYRAMID_LOG_ERROR on failure,
// stores m_lastError, returns nullptr. Callers check for null, never exceptions.
```
- Use optional-output-pointer form for platform/clipboard/font APIs: `bool SetText(std::u32string_view text, std::string* error = nullptr) override;` (`Engine/Platform/include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp`, `Engine/Platform/include/Pyramid/Platform/SystemFont.hpp`)
- Mark every fallible query `[[nodiscard]]`: `[[nodiscard]] bool IsBound() const`, `[[nodiscard]] bool Initialize(...)`, `[[nodiscard]] std::shared_ptr<ITexture2D> ResolveTexture(...)` (`Engine/Graphics/include/Pyramid/Graphics/CameraController.hpp`, `Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`). Callers must check the result — `if (!child.SetParent(root)) return Fail(...)` is the test-side mirror of this rule.
- Model/import failures use diagnostics vectors, not bare bools: check `result.HasErrors()`, `result.IsValid()`, `result.GetWarningCount()`, and search `diagnostic.message` for the expected substring (`Libraries/PyramidModel/test/ObjFailureTests.cpp`)
- Serialization failures use result structs with error counts: `malformedError.IsSuccess()`, `malformedError.GetErrorCount()` must be asserted together (`Tests/ModelMaterialResourceImporterTests.cpp`)
- On failure always do both: store the error string AND emit `PYRAMID_LOG_ERROR(...)` with the same message (`Engine/Graphics/source/Texture/TextureResource.cpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`)
- Handle handles as non-owning generational references: every alias bind/remap/removal advances generation; stale handles resolve to null, never to a replacement (`Tests/ResourceHandleTests.cpp`). Direct cache mutation must invalidate existing handles.

## Logging

**Framework:** `Pyramid::Util::Logger` via macros in `Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp`. Do not use `printf`, `std::cout` (except test drivers), or platform `OutputDebugString` directly.

**Patterns:**
- Use `PYRAMID_LOG_TRACE / _DEBUG / _INFO / _WARN / _ERROR / _CRITICAL` with variadic stream-style concatenation (commas, no format strings):
```cpp
PYRAMID_LOG_INFO("Initializing Pyramid Game Engine...");
PYRAMID_LOG_ERROR("Unsupported graphics API: ", static_cast<int>(api));
PYRAMID_LOG_CRITICAL("Failed to create graphics device");
PYRAMID_LOG_DEBUG("Window minimized");
```
(`Engine/Core/source/Game.cpp`, `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp`)
- Every macro captures `SourceLocation{__FILE__, PYRAMID_FUNCTION_NAME, __LINE__}` automatically — do not prepend `__FILE__` yourself (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp:208-213`)
- Use the `_STREAM` variant only when building a message conditionally across statements: `PYRAMID_LOG_STREAM(level)`, `PYRAMID_ERROR_STREAM()` (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp:216-222`)
- Use `PYRAMID_LOG_DEBUG_ONLY(...)` / `PYRAMID_LOG_TRACE_ONLY(...)` for hot-path diagnostics so release builds skip evaluation (`Libraries/PyramidFoundation/include/Pyramid/Util/Log.hpp:230-240`)
- Log lifecycle at INFO (init/shutdown/loop enter-exit), recoverable anomalies at WARN, blocking failures at ERROR/CRITICAL. Follow `Engine/Core/source/Game.cpp` and `Engine/Platform/source/Windows/Win32OpenGLWindow.cpp` as the reference cadence.
- Tests must NOT depend on log output for pass/fail; assert on return values, `GetLastError()`, and diagnostics vectors instead.

## Comments

**When to Comment:**
- Every public header declaration gets a Doxygen comment. Follow the `Game.hpp` / `Mesh.hpp` / `Entity.hpp` pattern:
```cpp
/**
 * @brief Engine-owned immutable geometry resource.
 * ...
 */
class Mesh final { ... };
/**
 * @brief Calculate the deterministic fingerprint for a valid specification.
 * @return Invalid identifier when the specification is malformed.
 */
```
- Document ownership, lifetime, and thread affinity in the comment when it matters: "The registry is created with the graphics device and destroyed before device shutdown", "The callback runs during window-message processing on the game thread", "The caller only needs to keep the pointed-to memory alive for the duration of Mesh::Create()" (`Engine/Core/include/Pyramid/Core/Game.hpp`, `Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`)
- Use `//` for implementation notes sparingly; prefer self-describing code. `Engine/Core/source/Game.cpp` uses short `// Create window first` section markers — that density is the ceiling.

**JSDoc/TSDoc equivalent (Doxygen):**
- Use `@file`, `@class`, `@brief`, `@param`, `@return`, `@note` tags. Document every `@param` including units (`deltaTime ... in seconds`) and edge cases (`A minimized event can contain a zero-sized client area`) (`Engine/Core/include/Pyramid/Core/Game.hpp:15-90`)

## Function Design

**Size:** Keep functions focused on one validation-or-action step. `Mesh.cpp` splits hashing (`StableHasher128::AddByte/AddBytes/AddU32/AddString/Finish`) from validation (`IsTopologyCountValid`) from creation (`Mesh::Create`). Follow that decomposition; do not add god-functions that validate, allocate, upload, and register.

**Parameters:** Pass specs by `const&` (`const MeshSpecification&`, `const WindowResizeEvent&`), devices by `&` (`IGraphicsDevice& device`), optional strings by pointer (`std::string* error = nullptr`), required diagnostics by reference (`std::string& error`). Use `std::string_view` for read-only names (`MeshAssetId::FromString(std::string_view)`).

**Return Values:** Prefer (in order): `bool` + error output for actions, nullable `shared_ptr` for created resources, invalid-handle/id sentinel for content addressing (`CalculateContentId` returns invalid id when malformed), `[[nodiscard]]` value types for pure queries (`GetDrawCount()`, `GetWorldPosition()`).

## Module Design

**Exports:** Each module is one CMake library with a `Pyramid::` alias (`PyramidEngine`/`Pyramid::Engine`, plus `Pyramid::Foundation`, `Pyramid::Math`, `Pyramid::Image`, `Pyramid::Input`, `Pyramid::Model`, `Pyramid::Font`, `Pyramid::Text`, `Pyramid::UI`). Depend via `target_link_libraries(... Pyramid::X)`, never via relative paths or global `include_directories`. Public headers install from `Engine/*/include/` and `Libraries/*/include/`; keep private sources under `source/` and expose them only through explicit `target_include_directories(... PRIVATE .../source)` exceptions (see `Tests/CMakeLists.txt:8` for `Engine/Graphics/source`).

**Barrel Files:** Use aggregate headers (`Pyramid/Graphics/Scene.hpp`, `Pyramid/Platform/Input.hpp`) for test and consumer convenience, but implementation headers stay granular (`Scene/Entity.hpp`, `Scene/SceneSerializer.hpp`, `Geometry/Mesh.hpp`). Tests include both levels: `#include <Pyramid/Graphics/Scene.hpp>` plus the specific header under test.

**Ownership rules (must-follow, from `AGENTS.md`):**
- CPU parsing stays out of the engine binary: OBJ/MTL parsing in `Pyramid::Model` (`Libraries/PyramidModel/`); image codecs in `Pyramid::Image`; math in `Pyramid::Math`; logging/types in `Pyramid::Foundation`. `Pyramid::Model` must not include graphics, Win32, OpenGL, or GLAD headers.
- Publish parsed models only through `ModelResourceImporter` + the existing mesh cache (`Engine/Graphics/include/Pyramid/Graphics/Model/ModelResourceImporter.hpp`); never build a parallel upload path.
- Pass scene geometry as `Mesh` (`Engine/Graphics/include/Pyramid/Graphics/Geometry/Mesh.hpp`); do not reintroduce raw vertex-array fields on `RenderObject`.
- Share reusable geometry/shaders/textures/materials through `ResourceRegistry::Meshes()/Shaders()/Textures()/Materials()`; never compile identical stage source twice, mutate a cached `ShaderProgram` in place, treat a cached `TextureResource` as mutable, or store per-draw matrices in material identity. Color space is part of texture identity; file changes go through transactional reload.
- Keep platform input backend-neutral at the public boundary: native messages feed `InputState`, `Game` evaluates generic `InputActionSystem` contexts before updates, focus loss releases held controls. Never hard-code game action names (e.g. RTS verbs) inside `Pyramid::Input`. Camera controllers consume configurable named action references and distinguish per-frame delta input from time-scaled rate input; physical bindings live in examples/games.
- Keep `Pyramid::UI` renderer-, OpenGL-, Win32-, scene-, and game-independent (layout, widget state, hit testing, focus, pointer capture, draw-list generation only). Publish UI through `UIRenderer` (`Engine/Graphics/include/Pyramid/Graphics/UI/UIRenderer.hpp`), never with OpenGL calls inside `Pyramid::UI`.
- Keep `Pyramid::Font` independent of UI/graphics/Win32/FreeType/HarfBuzz (TrueType/SFNT parsing, outlines, CPU coverage/SDF rasterization, content-addressed atlas caching, versioned `.pfont` format). Keep `Pyramid::Text` renderer-neutral (decoding, kerning, wrapping, alignment, metrics, glyph-run placement, grapheme-snapped editing, logical-to-visual cluster maps). Never split a cluster during wrapping/editing.
- Author scenes with `Entity` + components (`Engine/Graphics/include/Pyramid/Graphics/Scene/Entity.hpp`); do not reintroduce `SceneNode` or make `RenderObject` transforms authoritative. Renderer/light proxies are generated from the scene.
- Resolve executable-relative assets only through `Pyramid::Platform::ResolveRuntimePath`; never depend on the process working directory (`Tests/RuntimePathTests.cpp`).
- Do not add required interface methods with silent no-op defaults — new interface methods are pure virtual (`= 0`) as in `Engine/Platform/include/Pyramid/Platform/Window.hpp` and `Engine/Graphics/include/Pyramid/Graphics/Texture.hpp:80-88`.
- Do not expose source-tree absolute paths through installed interfaces (only the private `PYRAMID_SOURCE_DIR` compile definition may hold one: `Engine/CMakeLists.txt:55`).
- Prefer RAII and explicit ownership: `std::unique_ptr<Window> m_window`, `std::unique_ptr<IGraphicsDevice> m_graphicsDevice`, `std::unique_ptr<ResourceRegistry> m_resourceRegistry` (`Engine/Core/include/Pyramid/Core/Game.hpp:172-174`); GPU resources as `std::shared_ptr` created by static `Create()` factories. Long-lived scene references use typed non-owning registry handles, not raw pointers.

---

*Convention analysis: 2026-09-05*
