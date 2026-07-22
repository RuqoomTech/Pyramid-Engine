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

add_executable(FramebufferResizeTests FramebufferResizeTests.cpp)
target_link_libraries(FramebufferResizeTests PRIVATE Pyramid::Engine)
set_target_properties(FramebufferResizeTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.FramebufferResize COMMAND $<TARGET_FILE:FramebufferResizeTests>)

add_executable(TextureLoadingTests TextureLoadingTests.cpp)
target_link_libraries(TextureLoadingTests PRIVATE Pyramid::Engine)
set_target_properties(TextureLoadingTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.TextureLoading COMMAND $<TARGET_FILE:TextureLoadingTests>)

add_executable(SceneTransformTests SceneTransformTests.cpp)
target_link_libraries(SceneTransformTests PRIVATE Pyramid::Engine)
set_target_properties(SceneTransformTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.SceneTransforms COMMAND $<TARGET_FILE:SceneTransformTests>)

add_executable(CameraFrustumTests CameraFrustumTests.cpp)
target_link_libraries(CameraFrustumTests PRIVATE Pyramid::Engine)
set_target_properties(CameraFrustumTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.CameraFrustum COMMAND $<TARGET_FILE:CameraFrustumTests>)
