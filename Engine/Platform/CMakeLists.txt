target_sources(PyramidEngine
    PRIVATE
        source/Windows/Win32OpenGLWindow.cpp
    PRIVATE
        include/Pyramid/Platform/Window.hpp
        include/Pyramid/Platform/Windows/Win32OpenGLWindow.hpp
)

target_include_directories(PyramidEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)
