add_library(PyramidEngine)
add_library(Pyramid::Engine ALIAS PyramidEngine)

add_subdirectory(Core)
add_subdirectory(Graphics)
add_subdirectory(Platform)

set_target_properties(PyramidEngine PROPERTIES
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    DEBUG_POSTFIX d
    EXPORT_NAME Engine
)

target_include_directories(PyramidEngine
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine/Core/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine/Graphics/include>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Engine/Platform/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${PROJECT_SOURCE_DIR}/vendor"
)

target_link_libraries(PyramidEngine
    PUBLIC
        glad
        Pyramid::Foundation
        Pyramid::Math
        Pyramid::Input
        Pyramid::Image
        Pyramid::Model
        Pyramid::Font
        Pyramid::Text
        Pyramid::UI
)

if(WIN32)
    find_package(OpenGL REQUIRED)
    target_link_libraries(PyramidEngine
        PRIVATE
            OpenGL::GL
            gdi32
            user32
            ws2_32
    )
endif()

target_compile_definitions(PyramidEngine
    PUBLIC
        PYRAMID_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
        PYRAMID_VERSION_MINOR=${PROJECT_VERSION_MINOR}
        PYRAMID_VERSION_PATCH=${PROJECT_VERSION_PATCH}
        PYRAMID_VERSION_STRING="${PYRAMID_VERSION_STRING}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(PyramidEngine PRIVATE -Wall -Wextra -Wpedantic)
    if(PYRAMID_WARNINGS_AS_ERRORS)
        target_compile_options(PyramidEngine PRIVATE -Werror)
    endif()
elseif(MSVC)
    target_compile_options(PyramidEngine PRIVATE /W4 /permissive-)
    if(PYRAMID_WARNINGS_AS_ERRORS)
        target_compile_options(PyramidEngine PRIVATE /WX)
    endif()
endif()

set_target_properties(PyramidEngine PROPERTIES FOLDER "Engine")
