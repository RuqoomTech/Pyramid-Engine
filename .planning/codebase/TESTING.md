# Testing Patterns

**Analysis Date:** 2026-05-14

## Test Framework

**Runner:**
- CTest with standalone C++ executable tests.
- Config: root `CMakeLists.txt` enables testing only when `PYRAMID_BUILD_TESTS=ON`; `Engine/Utils/CMakeLists.txt` adds the test subdirectory; `Engine/Utils/test/CMakeLists.txt` registers each executable with `add_test(NAME Utils.<target> COMMAND $<TARGET_FILE:<target>>)`.
- Presets: `CMakePresets.json` defines `vs2022-debug-tests`, `build-debug-tests-clean`, and `test-debug`.

**Assertion Library:**
- No external unit-test assertion framework is used in current executable tests.
- Tests use boolean helper functions, explicit `if` checks, `std::cout` error messages, and process exit code `0`/`1`, as in `Engine/Utils/test/TestPNGComponents.cpp` and `Engine/Utils/test/TestJPEGSimple.cpp`.
- `docs/Contributing.md` contains planned GoogleTest examples, but GoogleTest is not detected in the active CMake files.

**Run Commands:**
```bash
cmake -B build -S . -DPYRAMID_BUILD_TESTS=ON -DPYRAMID_BUILD_EXAMPLES=ON  # Configure with tests and examples
cmake --build build --config Debug                                           # Build all configured targets
ctest --test-dir build -C Debug --output-on-failure                          # Run registered CTest tests
cmake --preset vs2022-debug-tests                                            # Configure with preset
cmake --build --preset build-debug-tests-clean                               # Clean build preset with tests
ctest --preset test-debug                                                    # Run CTest preset
```

## Test File Organization

**Location:**
- Current automated tests are under `Engine/Utils/test/` and focus on image/codec utility components.
- No active `Tests/Unit` or `Tests/Integration` tree is present; root `CMakeLists.txt` contains commented-out references to those directories.
- Graphics/runtime behavior is validated by examples and smoke scripts rather than unit tests: `Examples/BasicGame`, `Examples/BasicRendering`, `scripts/run-smoke.ps1`, and `docs/SmokeTests.md`.

**Naming:**
- Use `Test<Feature>.cpp` for test sources and target names, for example `TestPNGComponents`, `TestJPEGSimple`, `TestJPEGHuffman`, `TestJPEGDequantizer`, `TestJPEGIDCT`, `TestJPEGColorConverter`, and `TestJPEGIntegration` in `Engine/Utils/test/CMakeLists.txt`.
- CTest names are prefixed with `Utils.` by `add_utils_test`, for example `Utils.TestJPEGIDCT`.

**Structure:**
```
Engine/Utils/test/
├── CMakeLists.txt              # add_utils_test helper and CTest registration
├── TestPNGComponents.cpp       # BitReader/Huffman/Inflate/ZLib/Image extension checks
├── TestJPEGSimple.cpp          # Minimal marker parsing and extension checks
├── TestJPEGHuffman.cpp         # JPEG Huffman decoder checks
├── TestJPEGHuffmanDebug.cpp    # Debug-focused Huffman checks
├── TestJPEGDequantizer.cpp     # Quantization/dequantization checks
├── TestJPEGIDCT.cpp            # IDCT checks
├── TestJPEGColorConverter.cpp  # YCbCr/RGB conversion checks
└── TestJPEGIntegration.cpp     # JPEG loader + Image class integration checks
```

## Test Structure

**Suite Organization:**
```cpp
#include "Pyramid/Util/Image.hpp"
#include <iostream>

using namespace Pyramid::Util;

bool TestFeatureBehavior()
{
    std::cout << "=== Testing Feature Behavior ===" << std::endl;

    if (!/* condition */)
    {
        std::cout << "ERROR: clear failure reason" << std::endl;
        return false;
    }

    std::cout << "SUCCESS: Feature behavior test passed!" << std::endl;
    return true;
}

int main()
{
    bool allTestsPassed = true;
    allTestsPassed &= TestFeatureBehavior();
    return allTestsPassed ? 0 : 1;
}
```

**Patterns:**
- Keep each test case as a `bool Test...()` helper and aggregate results in `main`, matching `Engine/Utils/test/TestPNGComponents.cpp`.
- Print a section header before each helper and a summary at the end.
- On failure, print expected and actual values before returning `false`, as in `Engine/Utils/test/TestJPEGSimple.cpp`.
- Free allocated `ImageData` buffers on all paths before returning; prefer `Image::Free(result.Data)` when using the image API.
- Use deterministic in-memory byte arrays for parser/codec fixtures, as in `Engine/Utils/test/TestJPEGSimple.cpp` and `Engine/Utils/test/TestJPEGIntegration.cpp`.

## Mocking

**Framework:** Not detected.

**Patterns:**
```cpp
// Current tests avoid mocks and use direct in-memory fixtures.
std::vector<uint8_t> minimalJPEG = {
    0xFF, 0xD8,       // SOI
    0xFF, 0xC0,       // SOF0
    0x00, 0x0B,
    0x08,
    0x00, 0x08,
    0x00, 0x08,
    0x01,
    0x01,
    0x11,
    0x00,
    0xFF, 0xD9        // EOI
};
ImageData result = JPEGLoader::LoadFromMemory(minimalJPEG.data(), minimalJPEG.size());
```

**What to Mock:**
- Not applicable for current tests. For future code that depends on platform or graphics APIs, prefer small interface seams such as `IGraphicsDevice` from `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` and lightweight fake implementations over external mocking frameworks unless a test framework is added.

**What NOT to Mock:**
- Do not mock small deterministic utilities such as `BitReader`, `HuffmanDecoder`, `JPEGIDCT`, or `JPEGColorConverter`; test them directly with known inputs in `Engine/Utils/test/`.
- Do not mock CMake/CTest registration; add the executable target and verify it appears in `ctest -N`.

## Fixtures and Factories

**Test Data:**
```cpp
uint8_t deflateData[] = {
    0x01,
    0x05, 0x00,
    0xFA, 0xFF,
    'H', 'e', 'l', 'l', 'o'
};

std::vector<uint8_t> jpegData = {
    0xFF, 0xD8,
    0xFF, 0xC0,
    0x00, 0x0B,
    0x08,
    0x00, 0x20,
    0x00, 0x20,
    0x01,
    0x01,
    0x11,
    0x00,
    0xFF, 0xD9
};
```

**Location:**
- Fixtures are embedded directly in the test `.cpp` files under `Engine/Utils/test/`.
- Optional external files are probed by name in the current working directory (`test.jpg`, `test.jpeg`, `sample.jpg`) by `Engine/Utils/test/TestJPEGIntegration.cpp`; tests pass without these files by falling back to in-memory fixtures or informational skips.
- No shared fixture directory is detected.

## Coverage

**Requirements:** None enforced.

**View Coverage:**
```bash
# No coverage target is configured. Use build + ctest as the current quality gate.
ctest --test-dir build -C Debug --output-on-failure
```

## Test Types

**Unit Tests:**
- Scope: utility components in `Engine/Utils/source/` and `Engine/Utils/include/Pyramid/Util/`.
- Approach: standalone executable targets that directly call component APIs and validate deterministic values.
- Covered areas include bit reading, Huffman decoding, Inflate/ZLib, PNG loader infrastructure, JPEG marker parsing, JPEG Huffman decoding, JPEG dequantization, IDCT, color conversion, and JPEG/Image integration.

**Integration Tests:**
- Scope: current integration coverage is limited to utility loader composition, especially `Engine/Utils/test/TestJPEGIntegration.cpp` checking `JPEGLoader` and `Image::Load` together.
- Example applications (`Examples/BasicGame`, `Examples/BasicRendering`) act as manual/runtime integration tests for core, platform, graphics, renderer, scene, and math systems.
- `docs/SmokeTests.md` documents a smoke runner that launches `BasicGame` and `BasicRenderingExample` via `scripts/run-smoke.ps1` and checks startup/runtime behavior.

**E2E Tests:**
- No full automated E2E framework is detected.
- GUI/rendering validation is local smoke/manual validation using built example executables and `scripts/run-smoke.ps1`.

## Current Test Coverage and Gaps

**Covered:**
- Utility image/codec components under `Engine/Utils/` have executable tests in `Engine/Utils/test/`.
- CTest registration exists for eight utility test targets through `Engine/Utils/test/CMakeLists.txt`.

**Gaps:**
- No active tests are detected for `Engine/Core`, `Engine/Math`, `Engine/Graphics`, `Engine/Renderer`, `Engine/Platform`, `Engine/Input`, `Engine/Scene`, `Engine/Audio`, or `Engine/Physics`.
- Root `CMakeLists.txt` leaves `Tests/Unit` and `Tests/Integration` commented out, so there is no repository-wide unit/integration test hierarchy.
- No coverage thresholds, sanitizer presets, static analysis gates, or formatting checks are configured.
- Some tests contain debug output and permissive behavior that can hide failures, including the skipped ZLib failure path in `Engine/Utils/test/TestPNGComponents.cpp` and the registered debug target `Engine/Utils/test/TestJPEGHuffmanDebug.cpp`.
- Graphics and renderer behavior depends on examples/smoke tests and hardware availability; CI-style headless rendering tests are not detected.

## Adding New Tests

**Executable utility test:**
1. Add `Engine/Utils/test/Test<Feature>.cpp`.
2. Use `bool Test...()` helpers and return `allTestsPassed ? 0 : 1` from `main`.
3. Register it in `Engine/Utils/test/CMakeLists.txt` with `add_utils_test(Test<Feature> Test<Feature>.cpp)`.
4. Configure with `PYRAMID_BUILD_TESTS=ON` and run `ctest --test-dir build -C Debug --output-on-failure`.

**New module test:**
- If testing a module outside Utils, create a module-local `test/` directory following the `Engine/Utils/test/CMakeLists.txt` pattern or introduce a top-level `Tests/` tree by activating the commented `Tests/Unit` / `Tests/Integration` structure in root `CMakeLists.txt`.
- Link test executables against `PyramidEngine` and add public/private include directories exactly as `Engine/Utils/test/CMakeLists.txt` does.

**Runtime/rendering test:**
- Build with examples enabled and validate `Examples/BasicGame` and `Examples/BasicRendering`.
- For local smoke validation, use the documented command in `docs/SmokeTests.md`:
```powershell
powershell -ExecutionPolicy Bypass -File scripts/run-smoke.ps1 -BuildDir build-clean -Config Debug -DurationSeconds 5
```

## Common Patterns

**Async Testing:**
```cpp
// No async test framework is present. For runtime loops, prefer bounded smoke tests
// such as scripts/run-smoke.ps1 rather than unbounded waits inside C++ tests.
```

**Error Testing:**
```cpp
ImageData result = Image::Load("nonexistent_unique_12345.jpg");
if (result.Data != nullptr)
{
    std::cout << "ERROR: Non-existent .jpg file was loaded" << std::endl;
    Image::Free(result.Data);
    return false;
}
```

**Manual/Smoke Testing:**
```powershell
cmake --preset vs2022-debug-tests
cmake --build --preset build-debug-tests-clean
ctest --preset test-debug
powershell -ExecutionPolicy Bypass -File scripts/run-smoke.ps1 -BuildDir build-clean -Config Debug -DurationSeconds 5
```

---

*Testing analysis: 2026-05-14*
