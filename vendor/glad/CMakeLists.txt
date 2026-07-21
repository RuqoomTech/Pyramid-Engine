add_library(glad
    src/glad.c
    src/glad_wgl.c
)
add_library(Pyramid::glad ALIAS glad)

target_include_directories(glad
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(glad PROPERTIES
    FOLDER "vendor"
    EXPORT_NAME glad
)
