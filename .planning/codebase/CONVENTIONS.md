# Coding Conventions

**Analysis Date:** 2026-05-14

## Naming Patterns

**Files:**
- Use `PascalCase` for C++ public headers and source files: `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Math/source/Vec3.cpp`.
- Keep public engine headers under `Engine/<Module>/include/Pyramid/<Module>/...` and implementations under `Engine/<Module>/source/...`; mirror subdirectories for implementation-specific code such as `Engine/Graphics/source/OpenGL/Buffer/OpenGLVertexBuffer.cpp`.
- Use `Test<Feature>.cpp` for standalone test executables in `Engine/Utils/test/`, for example `Engine/Utils/test/TestJPEGIDCT.cpp`.
- Example applications use `Main.cpp` plus app-specific class files, for example `Examples/BasicGame/source/Main.cpp` and `Examples/BasicGame/include/BasicGame.hpp`.

**Functions:**
- Use `PascalCase` for public API methods and free functions: `Initialize`, `Shutdown`, `CreateVertexBuffer`, `SetViewport` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`.
- Existing lifecycle overrides in examples use lowercase callback names inherited from the app framework: `onCreate`, `onUpdate`, `onRender` in `Examples/BasicGame/source/BasicGame.cpp`.
- Test helper functions use `PascalCase` with a `Test` prefix: `TestBitReader`, `TestJPEGLoaderIntegration`, `TestMinimalJPEG` in `Engine/Utils/test/TestPNGComponents.cpp` and `Engine/Utils/test/TestJPEGIntegration.cpp`.

**Variables:**
- Use `camelCase` for parameters and locals: `deltaTime`, `vertexBuffer`, `indexBuffer`, `allTestsPassed` in `Examples/BasicGame/source/BasicGame.cpp` and `Engine/Utils/test/TestPNGComponents.cpp`.
- Use fixed-width engine aliases (`u32`, `f32`, `i32`) from `Engine/Core/include/Pyramid/Core/Prerequisites.hpp` for public engine numeric APIs.
- Constants commonly use `PascalCase` in docs and `k`-prefixed `camel/Pascal` for internal constants: `kMaxTextureSlots` in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`, `kForwardVertexShader` in `Examples/BasicGame/source/BasicGame.cpp`.

**Types:**
- Use `PascalCase` for classes, structs, and enum classes: `IGraphicsDevice`, `Color`, `GraphicsAPI`, `LogLevel` in `Engine/Core/include/Pyramid/Core/Prerequisites.hpp` and `Engine/Utils/include/Pyramid/Util/Log.hpp`.
- Prefix pure interface types with `I` when they represent backend-neutral contracts: `IGraphicsDevice`, `ITexture2D`, `IUniformBuffer` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`.
- Use `m_` + camel/Pascal member names for private fields. Current code contains both `m_initialized` and `m_Root`; prefer the lower-camel variant used by `Engine/Utils/include/Pyramid/Util/Log.hpp` (`m_mutex`, `m_config`, `m_logFile`).

## Code Style

**Formatting:**
- No `.clang-format`, `.clang-tidy`, `.editorconfig`, or formatter config is present at repository root; match nearby file style manually.
- Use C++17 as required by `CMakeLists.txt` (`CMAKE_CXX_STANDARD 17`) and documented in `docs/Contributing.md`.
- Indent with 4 spaces. Put braces on their own lines for classes, functions, namespaces, conditionals, and loops, as in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`.
- Prefer `#pragma once` in headers, shown throughout `Engine/Core/include/Pyramid/Core/Prerequisites.hpp`, `Engine/Math/include/Pyramid/Math/Vec3.hpp`, and `Engine/Utils/include/Pyramid/Util/Log.hpp`.
- Keep line lengths readable; several existing graphics APIs have long signatures in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, but new code should wrap parameters like `Examples/BasicGame/source/BasicGame.cpp` does for `std::make_unique<Pyramid::Camera>`.

**Linting:**
- No automated lint target or static analysis config is detected. Quality gates are compile success, executable tests, `ctest`, and smoke/example validation.
- Keep new code warning-clean for MSVC 2022, GCC, and Clang per `docs/Contributing.md`.

## Header and Source Organization

**Headers:**
- Place public headers in `Engine/<Module>/include/Pyramid/<Module>/...`; include them with `<Pyramid/...>` from engine/example code, as in `Examples/BasicGame/source/BasicGame.cpp`.
- Start headers with `#pragma once`, include required engine prerequisites early, and avoid unnecessary concrete implementation includes by using forward declarations where possible, as `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` does for `Window`, `IShader`, and buffer interfaces.
- Document public APIs with Doxygen-style `@brief`, `@param`, and `@return` blocks. `Engine/Math/include/Pyramid/Math/Vec3.hpp` and `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp` are the clearest examples.
- Order class declarations as public interface first, then protected/private implementation details, matching the guideline in `docs/Contributing.md`.

**Sources:**
- Include the matching header first, followed by engine dependencies, vendor/system headers, and standard library headers. Examples: `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` and `Engine/Utils/source/Log.cpp`.
- Keep module CMake source registration in the module `CMakeLists.txt`, for example `Engine/Utils/CMakeLists.txt` and `Engine/Math/CMakeLists.txt`.
- Use anonymous namespaces for file-local helpers and constants, as in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp` and `Examples/BasicGame/source/BasicGame.cpp`.

## Import Organization

**Order:**
1. Matching local header (`#include "BasicGame.hpp"` in `Examples/BasicGame/source/BasicGame.cpp`).
2. Public engine headers using `<Pyramid/...>`.
3. Vendor/platform headers (`<glad/glad.h>`, `<windows.h>`).
4. Standard library headers (`<vector>`, `<string>`, `<iostream>`, `<filesystem>`).

**Path Aliases:**
- CMake exposes module public include directories with `$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>` in files such as `Engine/Utils/CMakeLists.txt`; use `<Pyramid/...>` includes for public headers.
- Some utility tests use quoted public includes such as `#include "Pyramid/Util/Image.hpp"` in `Engine/Utils/test/TestPNGComponents.cpp`; prefer the project-wide `<Pyramid/...>` style for new engine and example code.

## Error Handling

**Patterns:**
- Public initialization and resource creation usually report failure with `bool` or `nullptr`, then set/log diagnostic state: `OpenGLDevice::Initialize` and `OpenGLDevice::CreateUniformBuffer` in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`.
- Use `PYRAMID_LOG_ERROR` or `PYRAMID_LOG_CRITICAL` before returning failure for engine/runtime errors, as in `Examples/BasicGame/source/BasicGame.cpp`.
- For subsystem parser/loader errors, keep a last-error string and expose it through `GetLastError`, as used by tests in `Engine/Utils/test/TestJPEGSimple.cpp` and `Engine/Utils/test/TestPNGComponents.cpp`.
- Use `PYRAMID_ASSERT` for client/game invariants and `PYRAMID_CORE_ASSERT` for core engine invariants. These macros are defined in `Engine/Utils/include/Pyramid/Util/Log.hpp` and compile out under `NDEBUG`.

## RAII and Memory Management

**Ownership:**
- Prefer RAII destructors for native resources. `OpenGLDevice::~OpenGLDevice` calls `Shutdown` in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`; logger cleanup is handled by `Logger::~Logger` in `Engine/Utils/source/Log.cpp`.
- Prefer `std::unique_ptr` for exclusive ownership and `std::shared_ptr` for shared engine objects. Examples: `m_renderSystem = std::make_unique<Pyramid::Renderer::RenderSystem>()`, `m_scene = std::make_shared<Pyramid::Scene>()` in `Examples/BasicGame/source/BasicGame.cpp`.
- Return `std::shared_ptr` from graphics factory methods where the interface shares GPU resources: `CreateVertexBuffer`, `CreateTexture2D`, `CreateUniformBuffer` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`.
- Raw pointers are acceptable for non-owning references or C-style buffers. Example: `Window*` in `OpenGLDevice::OpenGLDevice(Window *window)` is checked for null in `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`.
- Existing image loaders return raw `unsigned char*` buffers in `ImageData`; free them with `Image::Free` when possible, as in `Engine/Utils/test/TestJPEGSimple.cpp`. Some tests use `delete[]` directly in `Engine/Utils/test/TestJPEGIntegration.cpp`; prefer `Image::Free` for new tests using `ImageData`.

## Logging

**Framework:** `Pyramid::Util::Logger` macros from `Engine/Utils/include/Pyramid/Util/Log.hpp`.

**Patterns:**
- Use `PYRAMID_LOG_TRACE`, `PYRAMID_LOG_DEBUG`, `PYRAMID_LOG_INFO`, `PYRAMID_LOG_WARN`, `PYRAMID_LOG_ERROR`, and `PYRAMID_LOG_CRITICAL` for engine/application diagnostics.
- Use variadic stream-style macro arguments, not string formatting libraries: `PYRAMID_LOG_INFO("Successfully loaded PNG image: ", result.Width, "x", result.Height)` in `Engine/Utils/source/PNGLoader.cpp`.
- Use `PYRAMID_LOG_DEBUG_ONLY` / `PYRAMID_LOG_TRACE_ONLY` for expensive debug-only messages; they compile to no-ops under `NDEBUG` in `Engine/Utils/include/Pyramid/Util/Log.hpp`.
- In standalone tests, write clear status and failure details to `std::cout` and return non-zero from `main`, as in `Engine/Utils/test/TestPNGComponents.cpp`.

## Comments

**When to Comment:**
- Comment public API behavior and non-obvious algorithms. `Engine/Math/include/Pyramid/Math/Vec3.hpp` documents vector operations; `Engine/Utils/test/TestPNGComponents.cpp` comments binary test data layout.
- Use comments to explain graphics/shader intent and test fixture structure, as in `Examples/BasicGame/source/BasicGame.cpp` and `Engine/Utils/test/TestJPEGIntegration.cpp`.
- Avoid stale process comments such as `// Added` in new code; several existing files contain this deviation (`Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`, `Engine/Graphics/source/OpenGL/OpenGLDevice.cpp`).

**JSDoc/TSDoc:**
- Not applicable; this is a C++ codebase. Use Doxygen-style C++ documentation for public headers.

## Function Design

**Size:**
- Prefer small focused methods for resource setup, validation, and actions. `Examples/BasicGame/source/BasicGame.cpp` separates `onCreate`, `CreateColoredCube`, `SetupScene`, and camera update behavior.
- Split complex parsing or rendering logic into helper classes/modules rather than expanding a single file. Utility image loading is separated across `Engine/Utils/source/JPEGHuffmanDecoder.cpp`, `Engine/Utils/source/JPEGDequantizer.cpp`, `Engine/Utils/source/JPEGIDCT.cpp`, and `Engine/Utils/source/JPEGColorConverter.cpp`.

**Parameters:**
- Pass non-trivial read-only objects by `const&`: `const Color &color`, `const TextureSpecification &specification`, and `const std::string &filepath` in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`.
- Pass optional raw data as `const void*` or typed pointer plus size when interfacing with graphics/image APIs; document ownership expectations.

**Return Values:**
- Return `bool` for operations that can fail without producing a resource, `nullptr` for failed factory/resource creation, and concrete values for math operations.
- For tests, aggregate boolean helper results into `allTestsPassed` and return `0` or `1` from `main`, as in `Engine/Utils/test/TestPNGComponents.cpp`.

## Module Design

**Exports:**
- Export public API through headers in `Engine/<Module>/include/Pyramid/<Module>/...`; keep backend implementations in `source` directories and module-specific CMake target sources.
- Use nested namespaces matching the module when applicable: `Pyramid::Util` in `Engine/Utils/include/Pyramid/Util/Log.hpp`, `Pyramid::Math` in `Engine/Math/include/Pyramid/Math/Vec3.hpp`.
- Keep factory helpers close to the interface they construct. Example: texture factory methods are declared in `Engine/Graphics/include/Pyramid/Graphics/Texture.hpp` and implemented in `Engine/Graphics/source/Texture.cpp`.

**Barrel Files:**
- Module-level convenience headers are used where helpful, such as `Engine/Math/include/Pyramid/Math/Math.hpp`.
- Do not add broad barrel headers by default; prefer targeted includes to control compile dependencies.

## Quality Deviations to Avoid in New Code

- Do not add tests that intentionally pass by skipping failed assertions. `Engine/Utils/test/TestPNGComponents.cpp` currently returns success when `ZLib::Decompress` fails due to checksum differences.
- Do not leave debug-only test executables in the default test list unless they assert stable behavior. `Engine/Utils/test/TestJPEGHuffmanDebug.cpp` is registered by `Engine/Utils/test/CMakeLists.txt`.
- Do not add `// Added` comments or historical notes; document purpose instead. Existing examples appear in `Engine/Graphics/include/Pyramid/Graphics/GraphicsDevice.hpp`.
- Do not allocate raw arrays in new code unless required by an external ABI; prefer `std::vector`, `std::array`, `std::unique_ptr<T[]>`, or existing engine image ownership helpers.

---

*Convention analysis: 2026-05-14*
