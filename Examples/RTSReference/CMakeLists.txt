add_library(PyramidRTSReference STATIC
    source/RTSInteractionController.cpp
    source/RTSUnitStore.cpp
    source/RTSSimulation.cpp
)
add_library(Pyramid::RTSReference ALIAS PyramidRTSReference)

target_link_libraries(PyramidRTSReference PUBLIC Pyramid::Engine)

target_include_directories(PyramidRTSReference
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(PyramidRTSReference PRIVATE -Wall -Wextra -Wpedantic)
    if(PYRAMID_WARNINGS_AS_ERRORS)
        target_compile_options(PyramidRTSReference PRIVATE -Werror)
    endif()
elseif(MSVC)
    target_compile_options(PyramidRTSReference PRIVATE /W4 /permissive-)
    if(PYRAMID_WARNINGS_AS_ERRORS)
        target_compile_options(PyramidRTSReference PRIVATE /WX)
    endif()
endif()

set_target_properties(PyramidRTSReference PROPERTIES FOLDER "Examples/Support")
