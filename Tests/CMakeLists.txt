add_executable(PublicApiLinkage PublicApiLinkage.cpp)
target_link_libraries(PublicApiLinkage PRIVATE Pyramid::Engine)
set_target_properties(PublicApiLinkage PROPERTIES FOLDER "Tests/API")
add_test(NAME API.PublicApiLinkage COMMAND $<TARGET_FILE:PublicApiLinkage>)

add_executable(OpenGLDiagnosticsTests OpenGLDiagnosticsTests.cpp)
target_link_libraries(OpenGLDiagnosticsTests PRIVATE Pyramid::Engine)
target_include_directories(OpenGLDiagnosticsTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Graphics/source")
set_target_properties(OpenGLDiagnosticsTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.OpenGLDiagnostics COMMAND $<TARGET_FILE:OpenGLDiagnosticsTests>)

add_executable(WindowResizeEventTests WindowResizeEventTests.cpp)
target_include_directories(WindowResizeEventTests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/Engine/Core/include"
        "${PROJECT_SOURCE_DIR}/Engine/Platform/include"
)
set_target_properties(WindowResizeEventTests PROPERTIES FOLDER "Tests/Platform")
add_test(NAME Platform.WindowResizeEvents COMMAND $<TARGET_FILE:WindowResizeEventTests>)

add_executable(CameraViewportTests CameraViewportTests.cpp)
target_link_libraries(CameraViewportTests PRIVATE Pyramid::Engine)
set_target_properties(CameraViewportTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.CameraViewportResize COMMAND $<TARGET_FILE:CameraViewportTests>)
