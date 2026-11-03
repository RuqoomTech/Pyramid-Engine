add_library(PyramidRTSProtocol STATIC
    source/RTSProtocol.cpp
)
add_library(Pyramid::RTSProtocol ALIAS PyramidRTSProtocol)
target_compile_features(PyramidRTSProtocol PUBLIC cxx_std_17)
target_link_libraries(PyramidRTSProtocol PUBLIC Pyramid::Foundation Pyramid::Math)
target_include_directories(PyramidRTSProtocol
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

add_library(PyramidRTSReference STATIC
    source/RTSInteractionController.cpp
    source/RTSUnitStore.cpp
    source/RTSSimulation.cpp
    source/RTSHostSession.cpp
    source/RTSConnection.cpp
)
add_library(Pyramid::RTSReference ALIAS PyramidRTSReference)

target_link_libraries(PyramidRTSReference PUBLIC Pyramid::Engine Pyramid::RTSProtocol)

target_include_directories(PyramidRTSReference
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

foreach(rtsTarget IN ITEMS PyramidRTSProtocol PyramidRTSReference)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${rtsTarget} PRIVATE -Wall -Wextra -Wpedantic)
        if(PYRAMID_WARNINGS_AS_ERRORS)
            target_compile_options(${rtsTarget} PRIVATE -Werror)
        endif()
    elseif(MSVC)
        target_compile_options(${rtsTarget} PRIVATE /W4 /permissive-)
        if(PYRAMID_WARNINGS_AS_ERRORS)
            target_compile_options(${rtsTarget} PRIVATE /WX)
        endif()
    endif()
    set_target_properties(${rtsTarget} PROPERTIES FOLDER "Examples/Support")
endforeach()
