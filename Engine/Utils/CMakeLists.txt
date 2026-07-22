target_sources(PyramidEngine
    PRIVATE
        source/Image.cpp
        source/Log.cpp
        source/BitReader.cpp
        source/HuffmanDecoder.cpp
        source/Inflate.cpp
        source/ZLib.cpp
        source/PNGLoader.cpp
        source/JPEGLoader.cpp
)

target_include_directories(PyramidEngine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
)

# Add test subdirectory if tests are enabled
if(PYRAMID_BUILD_TESTS)
    add_subdirectory(test)
endif()
