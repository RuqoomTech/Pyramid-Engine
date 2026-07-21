cmake_minimum_required(VERSION 3.23)

project(Pyramid VERSION 0.6.0 LANGUAGES C CXX)

set(PYRAMID_VERSION_STATUS "pre-alpha" CACHE STRING "Human-readable release status")
set(PYRAMID_VERSION_STRING "${PROJECT_VERSION}-${PYRAMID_VERSION_STATUS}")

option(
    PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE
    "Allow configure-only validation on unsupported hosts"
    OFF
)

if(NOT WIN32 AND NOT PYRAMID_ALLOW_UNSUPPORTED_HOST_CONFIGURE)
    message(FATAL_ERROR
        "Pyramid Engine currently supports Windows only. "
        "Use Windows 10/11 with Visual Studio 2022 and a Windows SDK.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PYRAMID_BUILD_TESTS "Build test projects" OFF)
option(PYRAMID_BUILD_EXAMPLES "Build example projects" ON)
option(PYRAMID_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

if(PYRAMID_BUILD_TESTS)
    include(CTest)
    enable_testing()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

add_subdirectory(vendor/glad)
add_subdirectory(Engine)

if(PYRAMID_BUILD_EXAMPLES)
    add_subdirectory(Examples/BasicGame)
    add_subdirectory(Examples/BasicRendering)
endif()

if(PYRAMID_BUILD_TESTS)
    add_subdirectory(Tests)
endif()

include(GNUInstallDirs)

install(
    DIRECTORY
        Engine/Core/include/
        Engine/Graphics/include/
        Engine/Platform/include/
        Engine/Math/include/
        Engine/Utils/include/
        vendor/glad/include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING
        PATTERN "*.h"
        PATTERN "*.hpp"
)

install(
    TARGETS PyramidEngine glad
    EXPORT PyramidEngineTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(
    EXPORT PyramidEngineTargets
    FILE PyramidEngineTargets.cmake
    NAMESPACE Pyramid::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PyramidEngine"
)


include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/CMake/PyramidEngineConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/PyramidEngineConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PyramidEngine"
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/PyramidEngineConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${PROJECT_BINARY_DIR}/PyramidEngineConfig.cmake"
        "${PROJECT_BINARY_DIR}/PyramidEngineConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/PyramidEngine"
)

message(STATUS "Pyramid Engine version: ${PYRAMID_VERSION_STRING}")
