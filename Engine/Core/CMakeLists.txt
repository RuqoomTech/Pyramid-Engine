target_sources(PyramidEngine
    PRIVATE
        source/Game.cpp
        source/Prerequisites.cpp
    PRIVATE
        include/Pyramid/Core/Prerequisites.hpp
        include/Pyramid/Core/Game.hpp
)

target_include_directories(PyramidEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)
