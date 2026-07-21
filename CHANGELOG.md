# Changelog

All notable changes to the Pyramid Game Engine will be documented in this file.

> **Versioning note:** the active CMake project metadata is `0.3.3`, while this historical file contains later milestones through `0.6.0`. Treat those entries as repository history, not verified release metadata, until the version, tags, and changelog are reconciled.

## [Unreleased]

### Documentation
- Consolidated the documentation into a compact, maintained set.
- Corrected platform, backend, OpenGL, build-target, test, and module-status claims against the source tree.
- Replaced duplicate architecture/setup documents and speculative API references with source-backed architecture, API, development, examples, and roadmap guides.
- Removed broken internal links and documentation for nonexistent examples and systems.
- Documented the current version mismatch between CMake metadata and historical changelog milestones.

## [0.6.0] - 2025-07-13

### Added
- **Scene Management Core Architecture** - Production-ready scene management system
  - **SceneManager Class**: Comprehensive scene lifecycle management and organization
    - Scene creation, loading, and switching capabilities
    - Active scene management with automatic resource cleanup
    - Event system with callback-based scene notifications
    - Performance monitoring with real-time statistics
  - **Octree Spatial Partitioning**: Hierarchical spatial optimization system
    - Configurable depth (default 8 levels) and objects per node
    - O(log n) object insertion and query complexity
    - Automatic subdivision and node management
    - Memory-efficient smart pointer-based resource management
  - **Advanced Spatial Query System**: Multiple query types for game logic
    - Point queries: Find objects at specific locations
    - Sphere queries: Radius-based object discovery (explosions, AI awareness)
    - Box queries: Rectangular region object detection (triggers, areas)
    - Ray queries: Line-based intersection testing (line of sight, projectiles)
    - Frustum queries: Camera-based visibility culling for rendering
  - **AABB Implementation**: Axis-aligned bounding box system
    - Complete intersection testing (point, sphere, ray, AABB)
    - Expansion operations for dynamic bounds
    - Volume and geometric utility calculations
  - **Performance Monitoring**: Real-time statistics and profiling
    - Query execution time tracking
    - Object count and visibility statistics
    - Octree node count and depth monitoring
    - Memory usage tracking and optimization hints
  - **Integration Features**: Seamless integration with existing systems
    - Camera system integration for frustum culling
    - Scene class compatibility and extension
    - Smart pointer-based memory management with RAII
    - Thread-safe design considerations
  - **Utility Functions**: Helper classes and factory methods
    - SceneUtils namespace with factory functions
    - SpatialUtils namespace with geometric operations
    - Scene validation and optimization utilities
    - Test scene generation for development

### Technical Details
- **Namespace**: `Pyramid::SceneManagement` to avoid conflicts with existing Scene class
- **Files Added**:
  - `Engine/Graphics/include/Pyramid/Graphics/Scene/SceneManager.hpp`
  - `Engine/Graphics/include/Pyramid/Graphics/Scene/Octree.hpp`
  - `Engine/Graphics/source/Scene/SceneManager.cpp`
  - `Engine/Graphics/source/Scene/Octree.cpp`
- **Performance**: 10-100x speedup for spatial queries in large scenes (O(log n) vs O(n))
- **Memory**: ~64 bytes per octree node, ~100KB for 1000 objects
- **Build Integration**: Full CMake integration with Graphics module

### Documentation
- **API Documentation**: Complete API reference with usage examples
- **Architecture Documentation**: Updated with Scene Management system details
- **Usage Examples**: Comprehensive examples for game development scenarios
- **Performance Guidelines**: Optimization recommendations and best practices

## [0.4.0] - 2025-07-12

### Added
- **Complete Custom Image Processing Library** - Zero external dependencies
  - **TGA Support**: Full implementation for uncompressed RGB/RGBA formats
    - Proper orientation handling (top-left vs bottom-left)
    - BGR to RGB conversion
    - Alpha channel support
  - **BMP Support**: Windows Bitmap format implementation
    - Row padding handling
    - BGR to RGB conversion
    - Multiple bit depths (24/32-bit)
  - **PNG Support**: Complete PNG implementation with custom DEFLATE decompression
    - All PNG filter types: None, Sub, Up, Average, Paeth
    - Multiple color types: RGB, RGBA, Grayscale, Indexed
    - Custom DEFLATE implementation (RFC 1951 compliant)
    - Custom ZLIB wrapper with Adler-32 checksums
    - Custom Huffman decoder for dynamic and fixed trees
    - Comprehensive chunk parsing and validation
  - **JPEG Support**: Complete baseline DCT implementation
    - JPEG marker parsing (SOI, SOF0, DQT, DHT, SOS, EOI, APP0)
    - Custom Huffman decoder for DC/AC coefficients
    - DC prediction with component-specific predictors
    - Dequantization with quantization table support
    - Inverse DCT (IDCT) implementation with proper mathematical precision
    - YCbCr to RGB color space conversion (both fast and high-quality modes)
    - Support for grayscale, interleaved, and planar formats
  - **Production Ready Features**:
    - Comprehensive error handling and validation
    - Extensive test coverage for all components
    - Memory management with proper cleanup
    - Performance optimizations
    - Real-world integration with BasicGame example

### Enhanced
- **BasicGame Example**: Updated to showcase multi-format image loading
  - Texture cycling between TGA, BMP, PNG, and JPEG formats
  - Real-time demonstration of image loader capabilities
  - Automatic format detection and loading

### Technical Achievements
- **15+ Custom Components**: Built from scratch without external dependencies
- **5000+ Lines of Code**: High-quality, well-tested image processing code
- **Zero Dependencies**: Completely self-contained implementation
- **RFC Compliance**: DEFLATE implementation follows RFC 1951 specification
- **Mathematical Precision**: IDCT implementation with proper DCT mathematics

## [0.3.9] - 2025-06-30

### Added
- **Enhanced Logging System** - Complete rewrite and migration from Core to Utils module
  - Thread-safe logging with mutex protection and deadlock prevention
  - Multiple log levels: Trace, Debug, Info, Warn, Error, Critical
  - File output with automatic rotation and configurable size limits (default 10MB, 5 files)
  - Configurable console and file logging levels (runtime adjustable)
  - Source location tracking with file, function, and line number information
  - Structured logging support with key-value pairs for analytics
  - Multiple logging interfaces:
    - C-style variadic logging: `PYRAMID_LOG_INFO("Message: ", value)`
    - Stream-style logging: `PYRAMID_INFO_STREAM() << "Message: " << value`
    - Structured logging: `PYRAMID_LOG_STRUCTURED(level, message, fields)`
  - Enhanced assertion macros with automatic logging integration
  - Performance optimizations with early exit filtering and local buffers
  - Configurable timestamp formats with millisecond precision
  - Color-coded console output for different log levels
  - Debug-only logging macros for development builds

### Changed
- **BREAKING**: Logging system moved from `Engine/Core/` to `Engine/Utils/`
  - Old include: `#include <Pyramid/Core/Log.hpp>` (no longer exists)
  - New include: `#include <Pyramid/Util/Log.hpp>`
  - All logging functionality now in `Pyramid::Util` namespace
- All engine components updated to use new Utils-based logging system
- Logging macros maintain same interface but now provide enhanced functionality
- BasicGame example updated to demonstrate new logging configuration options

### Removed
- Old Core-based logging system completely removed
- Backward compatibility layer removed after successful migration
- Legacy `Pyramid::Log` namespace no longer available

### Fixed
- Thread safety issues in logging system resolved
- Deadlock prevention in multi-threaded logging scenarios
- Race conditions in log level checking eliminated
- Memory management improved with local buffer usage

### Migration Guide
For users upgrading from previous versions:
1. Update include statements: `#include <Pyramid/Core/Log.hpp>` → `#include <Pyramid/Util/Log.hpp>`
2. Existing logging calls continue to work without changes
3. Optionally configure enhanced features:
   ```cpp
   Pyramid::Util::LoggerConfig config;
   config.enableFile = true;
   config.logFilePath = "your_game.log";
   PYRAMID_CONFIGURE_LOGGER(config);
   ```
4. Consider using new structured logging for analytics and debugging

## [0.3.8] - 2025-05-07

### Added
- `IGraphicsDevice::CreateShader()` method and `OpenGLDevice` implementation for shader program creation.
- `BufferLayout` system (`BufferLayout.hpp`, `BufferElement`, `ShaderDataType`) for defining vertex attribute layouts.
- `IVertexArray::AddVertexBuffer` now accepts a `BufferLayout` to configure vertex attributes.
- Protected `Game::GetGraphicsDevice()` accessor for derived game classes.
- Implemented uniform setting methods (`SetUniformInt`, `SetUniformFloat*`, `SetUniformMat*`) in `IShader` and `OpenGLShader`.
- `OpenGLShader` now caches uniform locations.
- Added basic Texture System:
    - `ITexture`, `ITexture2D` interfaces (`Texture.hpp`).
    - `OpenGLTexture2D` implementation using `stb_image` for loading (`OpenGLTexture.hpp`, `OpenGLTexture.cpp`).
    - `TextureSpecification` struct and related enums.
    - Static `ITexture2D::Create` factory methods (`Texture.cpp`).
    - `IGraphicsDevice::CreateTexture2D` methods added and implemented for OpenGL.
- Added basic Logging system (`Log.hpp` with `PYRAMID_LOG_*` macros).
- Added basic Assertion system (`Log.hpp` with `PYRAMID_ASSERT` / `PYRAMID_CORE_ASSERT` macros).
- `BasicGame` example updated to load and render a textured triangle, modulated by the dynamic uniform tint.

### Changed
- Integrated logging macros into `Game.cpp`, `OpenGLShader.cpp`, `OpenGLTexture.cpp`, `BasicGame.cpp`.
- Integrated assertion macros into `Game.cpp`.
- Refactored `OpenGLDevice` to accept a `Window*` dependency via constructor, decoupling it from direct `Win32OpenGLWindow` instantiation.
- `Game` class now owns the `Window` instance (`std::unique_ptr<Window> m_window`).
- `Game` constructor now creates the `Window` and passes it to `IGraphicsDevice::Create`.
- `OpenGLVertexArray.cpp` updated to use `BufferLayout` for `glVertexAttribPointer` setup.

### Fixed
- `Game::run()` loop now correctly processes window messages (`m_window->ProcessMessages()`), resolving issues with window not appearing or becoming unresponsive.
- `Game::onCreate()` now explicitly initializes the graphics device (`m_graphicsDevice->Initialize()`) and checks for success before starting the game loop.
- `Win32OpenGLWindow::Initialize` now correctly uses the provided `title` parameter instead of a hardcoded one.
- Removed unnecessary `#include <windows.h>` from `Engine/Core/source/Game.cpp`.
- Made graphics device creation in `Game::Game()` constructor consistently use the `IGraphicsDevice::Create()` factory.

## [0.3.7] - 2025-02-16

### Added

- Vertex Array Objects (VAO) support
- Index buffer support
- Updated BasicGame example to use VAOs
- Vertex attribute management
- DrawIndexed and SetViewport methods

### Changed

- Refactored rendering pipeline to use VAOs
- Enhanced OpenGL buffer management
- Updated example to demonstrate vertex arrays and indexed drawing

## [0.3.6] - 2025-01-31

### Fixed

- Fixed window creation and OpenGL initialization issues
- Added proper window message processing in the game loop
- Fixed window title encoding for Unicode support
- Added GLAD initialization in OpenGLDevice
- Enabled depth testing by default
- Fixed window closing behavior

### Changed

- Moved graphics device initialization to Game constructor
- Improved window message handling in Win32OpenGLWindow
- Updated BasicGame example to use base class functionality

## [0.3.5] - 2025-01-31

### Added

- Initial project setup
- Core engine architecture
- Graphics abstraction layer
- Window management system
- Basic game loop
- OpenGL 3.3+ support
- GLAD integration
- CMake build system
- Basic game example
- Documentation

### Changed

- Reorganized project structure into modular components:
- Core: Core engine functionality
- Graphics: Graphics abstraction layer
- Platform: Platform-specific code
- Math: Math library
- Utils: Utility functions
- Renderer: Rendering system
- Input: Input handling
- Scene: Scene management
- Audio: Audio system
- Physics: Physics system
- Renamed OglGame to Game for better abstraction
- Renamed OglWindow to Window for better abstraction
- Updated documentation to reflect new structure

### Removed

- Direct OpenGL dependencies from public headers
- Platform-specific code from core components

## [0.3.4] - 2025-01-31

### Changed

- Major reorganization of engine structure:
  - Moved all core functionality to `Engine/` directory
  - Separated graphics, platform, and core components
  - Created proper module hierarchy
  - Improved code organization and maintainability
- Renamed files for better clarity and consistency:
  - OglGame -> Game
  - OglWindow -> Window
  - Added proper namespacing under Pyramid::
- Enhanced graphics abstraction:
  - Improved shader system
  - Better vertex buffer management
  - Cleaner graphics device interface

### Added

- New directory structure:
  - Engine/Core: Core engine functionality
  - Engine/Graphics: Graphics abstraction layer
  - Engine/Platform: Platform-specific code
  - Examples/BasicGame: Example game implementation
  - vendor/glad: OpenGL loader
- Comprehensive documentation for new structure
- Better separation of concerns between modules

### Removed

- Old OpenGL3D directory structure
- Redundant graphics engine implementation
- Deprecated file naming conventions

## [0.3.3] - 2025-01-31

### Added

- Proper window message processing
- Window state management (minimize, maximize, close)
- OpenGL context management improvements

### Fixed

- Window closing immediately after opening
- OpenGL context initialization issues
- Window procedure message handling
- Memory leaks in OpenGL context management

## [0.3.2] - 2025-01-31

### Added

- Proper vsync support
- Unicode support for window creation

### Changed

- Fixed window management system
- Better error handling in graphics initialization

### Fixed

- Unicode string handling in window creation
- Proper cleanup of OpenGL resources
- Vsync implementation
- Window class registration

## [0.3.1] - 2025-01-30

### Added

- Basic geometry rendering support
- Vertex buffer abstraction
- Shader system abstraction
- Test rectangle rendering with colored vertices
- Improved file organization for graphics components

### Changed

- Reorganized graphics-related files into more logical structure
- Enhanced OpenGL device implementation with proper resource management
- Improved version detection for OpenGL

## [0.3.0] - 2025-01-30

### Added

- Graphics API abstraction layer (IGraphicsDevice interface)
- OpenGL implementation of graphics device
- Support for multiple OpenGL versions (3.3 to 4.6)
- Core engine types and platform detection in Prerequisites.h
- Graphics API selection in game initialization

### Changed

- Refactored OglGame to use new graphics abstraction
- Improved error handling and initialization
- Better organization of graphics-related code
- Updated window management to work with graphics abstraction

### Prepared

- Framework for future DirectX 9/10/11/12 support
- Version detection and fallback system
- Graphics API feature management

## [0.2.3] - 2025-01-30

### Added

- Comprehensive Doxygen-style documentation for all major classes
- New `docs/` directory with detailed documentation:
  - Architecture.md: Engine architecture documentation
  - GettingStarted.md: User guide and tutorials
- Better code comments and explanations

### Changed

- Window title changed to "Pyramid Game Engine"
- Window class name updated to "PyramidGameEngine"
- Improved code formatting and style
- Enhanced error handling documentation

### Fixed

- Float truncation warnings in OglGame
- Unused variable warning in Win32OpenGLWindow
- Code style consistency

## [0.2.2] - 2025-01-30

### Added

- Source file grouping for better IDE organization
- Proper folder organization in Visual Studio
- More comprehensive .gitignore rules

### Changed

- Improved CMake configuration
- Better organization of source and header files in CMake
- Enhanced build type configuration
- Optimized installation rules using GNUInstallDirs
- Cleaned up project structure

### Removed

- Unused CMake utility files
- Redundant CMake configurations

## [0.2.1] - 2025-01-30

### Added

- Comprehensive README.md with build instructions and project information
- Detailed project structure documentation
- Build verification and testing

### Fixed

- Unicode string handling in Windows API calls
- Window creation and management in Win32 implementation
- Build system configuration for proper library linking

### Changed

- Improved code documentation and comments
- Enhanced error handling in window creation

## [0.2.0] - 2025-01-30

### Added

- Proper OpenGL and Windows system library linking
- Dependencies management through CMake/Dependencies.cmake
- Installation rules for headers and binaries
- Multi-processor compilation support for MSVC
- Better source organization in IDEs using source_group
- Support for both Debug and Release configurations

### Changed

- Updated to modern CMake practices with better target management
- Improved Windows API integration with proper Unicode support
- Better organized output directories for binaries and libraries
- Enhanced compiler options and warning levels
- Separated OpenGL3D and Game components into distinct targets

### Fixed

- Unicode/ANSI string conversion issues in Windows API calls
- OpenGL context creation and management
- CMake configuration issues with library linking
- Window creation in Win32 implementation

## [0.1.0] - 2025-01-30

### Added

- Initial project structure
- Basic CMake build system
- Comprehensive .gitignore file
- OpenGL3D library foundation
- Basic window management system
- Game executable target

### Changed

- Modernized C++ standard to C++17
- Improved project organization
- Better handling of platform-specific settings

### Fixed

- Initial build system configuration
- Project structure organization
- Basic dependency management
