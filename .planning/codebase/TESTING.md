# Testing Patterns

**Analysis Date:** 2026-09-05

## Test Framework

**Runner:**
- No GoogleTest / Catch2 / doctest. Every test is a standalone C++17 executable with `int main()` returning `0` on success, `1` (`EXIT_FAILURE`) on failure.
- CTest is the runner: `include(CTest)` + `enable_testing()` only when `PYRAMID_BUILD_TESTS=ON` (`CMakeLists.txt:29-32`). Each executable is registered with one `add_test(NAME <Suite>.<Case> COMMAND $<TARGET_FILE:...>)` (`Tests/CMakeLists.txt`).
- Config: `Tests/CMakeLists.txt` (engine/platform tests), `Libraries/*/test/CMakeLists.txt` (library tests), `Tests/*/CMakeLists.txt` (install-consumer tests). There is no `jest.config` / `vitest.config` / `pytest.ini` equivalent.

**Assertion Library:**
- None. Hand-written helpers per file: `Fail()`, `Require()`, `Expect()`, `NearlyEqual()`. Tests print to `std::cerr` and either `return EXIT_FAILURE` or `std::exit(EXIT_FAILURE)`.

**Run Commands:**
```powershell
cmake --preset gcc-debug-tests
cmake --build --preset build-gcc-debug-tests
ctest --preset test-gcc-debug
./scripts/run-smoke.ps1 -BuildDir build/gcc-debug-tests -DurationSeconds 5
```
- Release validation:
```powershell
cmake --preset gcc-release-tests
cmake --build --preset build-gcc-release-tests
ctest --preset test-gcc-release
```
- Clang equivalents: `cmake --preset clang-debug-tests`, `cmake --build --preset build-clang-debug-tests`, `ctest --preset test-clang-debug` (and `-release-tests` variants). Preset definitions in `CMakePresets.json:48-136`; all test presets set `outputOnFailure: true`.
- Smoke is process-liveness, NOT pixel validation: `scripts/run-smoke.ps1` launches `build/<preset>/bin/BasicGame.exe` and `BasicRenderingExample.exe`, waits N seconds, passes if still running or exited 0. Renderer changes additionally require human visual inspection (see below).

## Test File Organization

**Location:**
- Engine/platform tests: one file per executable directly under `Tests/*.cpp` + shared fake `Tests/TestGraphicsDevice.hpp`. Examples: `Tests/EntitySceneTests.cpp`, `Tests/SceneSerializationTests.cpp`, `Tests/ResourceHandleTests.cpp`, `Tests/InputActionTests.cpp`, `Tests/RTSInteractionTests.cpp`.
- Library tests: `Libraries/<Lib>/test/*.cpp`. Full inventory: `Libraries/PyramidFoundation/test/FoundationTests.cpp`, `Libraries/PyramidMath/test/MathTests.cpp`, `Libraries/PyramidImage/test/TestPNGLoader.cpp`, `TestPNGComponents.cpp`, `TestJPEGVariants.cpp`, `TestJPEGSimple.cpp`, `TestJPEGRobustness.cpp`, `TestJPEGParser.cpp`, `TestJPEGIntegration.cpp`, `Libraries/PyramidModel/test/ObjImportTests.cpp`, `ObjFileTests.cpp`, `ObjFailureTests.cpp`, `Libraries/PyramidFont/test/FontTests.cpp`, `ReferenceTypographyTests.cpp`, `Libraries/PyramidText/test/TextTests.cpp`, `TextBufferTests.cpp`, `InternationalTextTests.cpp`, `Libraries/PyramidUI/test/UITests.cpp`, `TextEditingTests.cpp`, `ScalableTypographyTests.cpp`, `GameUITests.cpp`.
- Install/consumer tests (validate the *installed* packages link and run): `Tests/Consumer/main.cpp`, `Tests/LibrariesConsumer/main.cpp`, `Tests/ImageConsumer/main.cpp`, `Tests/ModelConsumer/main.cpp`, `Tests/FontConsumer/main.cpp`, `Tests/UIConsumer/main.cpp`, each with its own `CMakeLists.txt`.
- Fixtures: `Tests/Fixtures/JPEGFixtures.hpp` + `Tests/Fixtures/JPEG/*.jpg` (baseline/progressive/restart/gray corpus); inline TGA writer structs in `Tests/TextureCacheTests.cpp` and `Tests/ModelMaterialResourceImporterTests.cpp`.

**Naming:**
- CTest names use `<Area>.<Case>` dotted suites: `API.PublicApiLinkage`, `Graphics.EntityScene`, `Graphics.SceneSerialization`, `Graphics.CameraFrustum`, `Graphics.OctreeUpdates`, `Graphics.ResourceHandles`, `Platform.InputState`, `Input.ActionMapping`, `Examples.RTSInteraction`, `Foundation.Core` (`Tests/CMakeLists.txt`, `Libraries/PyramidFoundation/test/CMakeLists.txt`).
- Executable target names match file names: `add_executable(EntitySceneTests EntitySceneTests.cpp)` + `set_target_properties(... FOLDER "Tests/Graphics")` (`Tests/CMakeLists.txt:79-82`). `FOLDER` groups are `Tests/API`, `Tests/Graphics`, `Tests/Platform`, `Tests/Input`, `Tests/Examples`, `Tests/Libraries`.

**Structure:**
```
Tests/
  <Name>Tests.cpp            # one CTest executable each
  TestGraphicsDevice.hpp     # shared GL-free fake device
  Fixtures/JPEGFixtures.hpp  # binary corpus headers
  Fixtures/JPEG/*.jpg
  Consumer/main.cpp           # installed-engine smoke
  LibrariesConsumer/main.cpp  # installed-foundation/math/input smoke
  ImageConsumer/ ModelConsumer/ FontConsumer/ UIConsumer/
Libraries/<Lib>/test/
  <Topic>Tests.cpp
```

## Test Structure

**Suite Organization:**
- There are no suites/fixtures classes. Each `main()` is a linear script of arrange → act → check, failing fast. Canonical skeleton (from `Tests/EntitySceneTests.cpp:1-46`):
```cpp
#include <Pyramid/Graphics/Scene.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << "EntitySceneTests failure: " << message << '\n';
        return EXIT_FAILURE;
    }

    bool NearlyEqual(float left, float right, float epsilon = 0.0001f)
    {
        return std::fabs(left - right) <= epsilon;
    }
}

int main()
{
    using namespace Pyramid;
    Scene scene("Entity Scene");
    Entity root = scene.CreateEntityWithId(EntityId(0x10), "Root");
    if (!root || scene.GetEntityCount() != 3)
    {
        return Fail("stable entity creation failed");
    }
    // ... more sequential checks, each returning Fail(...) on mismatch ...
    return 0;
}
```
- `Require()` variant (used in `Tests/InputActionTests.cpp:9-16`, `Tests/RTSInteractionTests.cpp:12-19`) exits immediately instead of returning:
```cpp
void Require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "InputAction test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}
```
- `Expect()` variant (used in `Libraries/PyramidFoundation/test/FoundationTests.cpp:9-17`) accumulates `bool passed` across checks and returns `passed ? 0 : 1` at the end — use it only when every check is independent and you want full output in one run.

**Patterns:**
- Setup pattern: construct the object directly in `main()` with valid arguments (`Scene scene("Entity Scene")`, `InputState input; input.SetFocused(true);`, `ResourceRegistry registry(device);`). No SetUp()/TearDown() methods exist.
- Float comparison pattern: always use a file-local `NearlyEqual` with explicit epsilon (`0.0001f` for scene math in `Tests/EntitySceneTests.cpp:15-24`, `0.001f` for RTS interaction in `Tests/RTSInteractionTests.cpp:21-34`). Never use `==` on `f32`/`Vec3`.
- Assertion pattern: one behavior per `if`, message names the invariant (`"cycle, self-parent, or hierarchy enumeration validation failed"`, `"renderer/light proxy synchronization failed"`, `"duplicate context should fail"`). Prefix the `std::cerr` line with the test file name so CTest `outputOnFailure` output is attributable.
- Failure-visibility rule (from `AGENTS.md`): tests must fail visibly, print actionable context, and never skip to green. Every check above follows it — no `return 0` fallthrough on untested paths, no empty `catch`, no `SKIP`.

## Mocking

**Framework:** None. Hand-written fakes + factory injection.

**Patterns:**
- Fake the whole `IGraphicsDevice` with `Pyramid::Tests::TestGraphicsDevice` (`Tests/TestGraphicsDevice.hpp:166-374`): counts creations (`vertexBufferCreations`, `shaderCreations`, `textureCreations`), records draws (`drawCalls`, `lastDrawCount`, `lastTopology`), tracks viewport/state, and exposes injectable factories:
```cpp
Tests::TestGraphicsDevice device;
device.shaderFactory = []() { return std::make_shared<TestShader>(); };
ResourceRegistry registry(device);
```
(`Tests/SceneSerializationTests.cpp:84-86`)
- Fake individual interfaces inline per file when only one is needed: `TestShader final : public IShader` with no-op `Bind/Unbind` and `return true` compiles (`Tests/SceneSerializationTests.cpp:20-42`, `Tests/ResourceHandleTests.cpp:20-42`); `TestTexture final : public ITexture2D` returning canned sizes (`Tests/ResourceHandleTests.cpp:44-65`, `Tests/TextureCacheTests.cpp:21-46`); `TestVertexBuffer/TestIndexBuffer/TestVertexArray` with real CPU-side bounds math (`Tests/TestGraphicsDevice.hpp:18-164`).
- Inject allocation faults with counters, not exceptions: `device.failVertexBufferCreationAt = N` makes the Nth `CreateVertexBuffer()` return `nullptr`; tests then assert the importer fails cleanly (`Tests/ModelResourceImporterTests.cpp:133`, `Tests/TestGraphicsDevice.hpp:210-219`, `Tests/UIRendererTests.cpp:136`).
- Follow `Tests/TestGraphicsDevice.hpp` as the reference fake. When a new `IGraphicsDevice` method is added, extend the fake and `Tests/PublicApiLinkage.cpp` together (see Test Inventory below).

**What to Mock:**
- Mock the GPU boundary only: `IGraphicsDevice`, `IShader`, `ITexture2D`, `IVertexBuffer`, `IIndexBuffer`, `IVertexArray`. This keeps resource/cache/registry/serializer/octree tests hermetic and runnable on headless CI.

**What NOT to Mock:**
- Do not mock `Scene`, `Entity`, `ResourceRegistry`, `ResourceManifest`, `MeshCache`, `ShaderCache`, `TextureCache`, `MaterialCache`, `InputState`, `InputActionSystem`, `Octree`, or `Pyramid::Model::ObjImporter` — exercise the real implementations against the fakes above. Do not mock `Pyramid::Math`, `Pyramid::Util::Logger`, or fixed-width aliases; `Tests/Consumer/main.cpp` and `Tests/LibrariesConsumer/main.cpp` assert their real behavior.

## Fixtures and Factories

**Test Data:**
- Build mesh specs with a file-local factory so vertex lifetime is explicit:
```cpp
MeshSpecification MakeMeshSpecification(
    const std::array<Vertex, 3>& vertices,
    const std::array<u32, 3>& indices,
    MeshAssetId assetId)
{
    MeshSpecification specification;
    specification.vertexData = vertices.data();
    specification.vertexDataSize = sizeof(vertices);
    specification.vertexCount = static_cast<u32>(vertices.size());
    specification.layout = {
        {ShaderDataType::Float3, "Position"},
        {ShaderDataType::Float4, "Color"}};
    specification.indexData = indices.data();
    specification.indexCount = static_cast<u32>(indices.size());
    specification.assetId = assetId;
    return specification;
}
```
(`Tests/SceneSerializationTests.cpp:61-77`, duplicated in `Tests/ResourceHandleTests.cpp:73-...`)
- Generate binary image fixtures in-code with packed structs (`#pragma pack(push, 1) struct TgaHeader {...}` in `Tests/TextureCacheTests.cpp:48-60`) plus `WriteTga()` / `MakeModel()` helpers (`Tests/ModelMaterialResourceImporterTests.cpp:86-...`).
- Reuse the shared JPEG corpus via `Tests/Fixtures/JPEGFixtures.hpp` (`BaselineRGBJPEG`, `ProgressiveGrayJPEG` byte arrays + sizes) for parser tests (`Libraries/PyramidImage/test/TestJPEGRobustness.cpp:11-15`).
- Reference the checked-in font asset for UI renderer tests through the compile definition, never a hard-coded absolute path:
```cpp
target_compile_definitions(UIRendererTests PRIVATE
    PYRAMID_UI_TEST_FONT="${PROJECT_SOURCE_DIR}/Examples/BasicGame/Assets/Fonts/PyramidSans-64-sdf.pfont")
```
(`Tests/CMakeLists.txt:177-179`)

**Location:**
- Small builders live file-local in an anonymous namespace at the top of each `*Tests.cpp`. Only genuinely shared artifacts live in `Tests/Fixtures/` or `Tests/TestGraphicsDevice.hpp`. Do not create a global fixtures library.

## Coverage

**Requirements:** No numeric coverage gate is enforced. Coverage is enforced by inventory: every module listed below must keep its named CTest green, and parser/codec changes must add malformed/truncated/limit/corpus cases (per `AGENTS.md`).

**View Coverage:**
```powershell
ctest --preset test-gcc-debug               # full suite, outputOnFailure: true
ctest --preset test-gcc-debug -R Graphics    # filter by suite prefix
ctest --preset test-gcc-debug -R "Resource|Scene|Octree"
```

## Test Types

**Unit Tests — scope and approach:**
- Validate pure logic with real objects, no device: `Foundation.Core` (type widths, color ABI, log-level parsing, bounded logger history in `Libraries/PyramidFoundation/test/FoundationTests.cpp`), `Platform.InputState` (`Tests/InputStateTests.cpp`), `Platform.ClipboardEncoding` (malformed surrogates, output limits in `Tests/ClipboardEncodingTests.cpp`), camera/frustum math (`Tests/CameraFrustumTests.cpp`, `Tests/CameraViewportTests.cpp`), mesh validation incl. NaN/Inf rejection (`Tests/MeshResourceTests.cpp:214`, `Tests/RenderObjectBoundsTests.cpp:127`, `Tests/OctreeConfigurationTests.cpp:110`).

**Integration Tests — scope and approach:**
- Exercise caches/registries/serializers against `TestGraphicsDevice`: `Graphics.MeshCache/TextureCache/ShaderCache/MaterialCache`, `Graphics.ResourceRegistry/ResourceHandles/ResourceManifest`, `Graphics.SceneSerialization` (deterministic version-2 round trips, exact manifest references, hierarchy validation, missing/stale diagnostics), `Graphics.ModelResourceImport/ModelMaterialResourceImport` (transactional upload, malformed-texture preserves prior resources), `Graphics.OctreeUpdates/OctreeQueries/OctreeConfiguration/OctreeCompaction`, `Graphics.FramebufferResize`, `Graphics.UIRenderer` (32-bit buffer-limit rejection, zero-surface rejection), `Graphics.CommandBufferStats`, `Input.ActionMapping/TextEvents`, `Platform.RuntimePaths/WindowResizeEvents`, `Examples.RTSInteraction` (game-side, links `Pyramid::RTSReference` per `Tests/CMakeLists.txt:64-67`).

**E2E / Consumer Tests — installed-package validation:**
- `Tests/Consumer/main.cpp` links the installed `Pyramid::Engine` and smoke-tests math + mesh/shader/texture/material IDs + material cache + manifest + scene + input actions + free-fly camera in one binary; exits non-zero unless every invariant holds (`Tests/Consumer/main.cpp:76-85`).
- `Tests/LibrariesConsumer/main.cpp` validates installed Foundation/Math/Input; `Tests/ImageConsumer`, `Tests/ModelConsumer`, `Tests/FontConsumer`, `Tests/UIConsumer` do the same for their packages.
- Process smoke via `scripts/run-smoke.ps1` (see Run Commands). It proves the examples start and stay up; it does not compare pixels.

**Visual inspection rule:** Renderer changes require human visual inspection of `Examples/BasicGame` and `Examples/BasicRendering` because process smoke testing is not pixel validation (per `AGENTS.md`). PRs touching renderer/shader/framebuffer/UI-renderer code must state the visual evidence.

## Common Patterns

**Temp-file handling (mandatory cleanup):**
```cpp
const std::filesystem::path root = "pyramid_step32_model_fixtures";
std::filesystem::remove_all(root);
// ... write fixtures, run import, assert ...
std::filesystem::remove_all(root);   // on EVERY return path, including failures
```
(`Tests/ModelMaterialResourceImporterTests.cpp:212-487` — every `return Fail(...)` is preceded by `remove_all`). Single files use `std::remove(filePath.c_str())` (`Tests/TextureCacheTests.cpp:180-218`, `Tests/TextureLoadingTests.cpp:160`). Scoped RAII guards are also accepted (`TempDirectory` in `Tests/SystemFontTests.cpp:31-44`, `CurrentPathGuard` in `Tests/RuntimePathTests.cpp:24-35`). Never leave fixture directories behind.

**Error Testing:**
```cpp
// OBJ/model pattern — assert error presence AND message content:
auto result = ObjImporter::Import(request);
if (!result.HasErrors() || result.IsValid() ||
    !ContainsDiagnostic(result, "material library was not provided"))
{
    return Fail("missing declared material library was not rejected");
}
// Manifest pattern:
if (ResourceManifest::Deserialize(malformed, rejected, diagnostics) || ...)
    return Fail("manifest serialization is malformed");
// Handle pattern: stale handles resolve to null, never to a replacement.
```
(`Libraries/PyramidModel/test/ObjFailureTests.cpp:40-45`, `Tests/ResourceManifestTests.cpp:103-109`)

**Parser robustness (required for parser/codec work):**
- Truncation loops: feed every N-byte prefix and require rejection + non-empty `GetLastError()` (`Libraries/PyramidImage/test/TestJPEGRobustness.cpp:25-45`).
- Malformed fixtures: `malformed` surrogate pairs (`Tests/ClipboardEncodingTests.cpp:34-38`), `malformed.tga` (`Tests/ModelMaterialResourceImporterTests.cpp:220-224`), zero/out-of-range/NaN OBJ indices and vertices (`Libraries/PyramidModel/test/ObjFailureTests.cpp:55-69`).
- Allocation limits and representative standards-valid corpus cases (baseline/progressive/restart JPEGs in `Tests/Fixtures/JPEG/`).

**Platform gating:** Guard Windows-only tests with `if(WIN32)` in CMake (`SystemFontTests` in `Tests/CMakeLists.txt:37-42`). MinGW runtime DLLs are wired via `PyramidMinGWRuntime` dependency loops at the end of `Tests/CMakeLists.txt:193-201` and each library `test/CMakeLists.txt` — copy that block when adding a new test directory.

---

*Testing analysis: 2026-09-05*
