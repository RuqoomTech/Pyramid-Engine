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

add_executable(OctreeUpdateTests OctreeUpdateTests.cpp)
target_link_libraries(OctreeUpdateTests PRIVATE Pyramid::Engine)
set_target_properties(OctreeUpdateTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.OctreeUpdates COMMAND $<TARGET_FILE:OctreeUpdateTests>)

add_executable(OctreeQueryTests OctreeQueryTests.cpp)
target_link_libraries(OctreeQueryTests PRIVATE Pyramid::Engine)
set_target_properties(OctreeQueryTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.OctreeQueries COMMAND $<TARGET_FILE:OctreeQueryTests>)

add_executable(NearestQueryTests NearestQueryTests.cpp)
target_link_libraries(NearestQueryTests PRIVATE Pyramid::Engine)
set_target_properties(NearestQueryTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.NearestQueries COMMAND $<TARGET_FILE:NearestQueryTests>)

add_executable(OctreeConfigurationTests OctreeConfigurationTests.cpp)
target_link_libraries(OctreeConfigurationTests PRIVATE Pyramid::Engine)
set_target_properties(OctreeConfigurationTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.OctreeConfiguration COMMAND $<TARGET_FILE:OctreeConfigurationTests>)

add_executable(OctreeCompactionTests OctreeCompactionTests.cpp)
target_link_libraries(OctreeCompactionTests PRIVATE Pyramid::Engine)
set_target_properties(OctreeCompactionTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.OctreeCompaction COMMAND $<TARGET_FILE:OctreeCompactionTests>)

add_executable(RenderObjectBoundsTests RenderObjectBoundsTests.cpp)
target_link_libraries(RenderObjectBoundsTests PRIVATE Pyramid::Engine)
set_target_properties(RenderObjectBoundsTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.RenderObjectBounds COMMAND $<TARGET_FILE:RenderObjectBoundsTests>)

add_executable(MeshResourceTests MeshResourceTests.cpp)
target_link_libraries(MeshResourceTests PRIVATE Pyramid::Engine)
set_target_properties(MeshResourceTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.MeshResource COMMAND $<TARGET_FILE:MeshResourceTests>)

add_executable(MeshCacheTests MeshCacheTests.cpp)
target_link_libraries(MeshCacheTests PRIVATE Pyramid::Engine)
set_target_properties(MeshCacheTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.MeshCache COMMAND $<TARGET_FILE:MeshCacheTests>)

add_executable(ShaderCacheTests ShaderCacheTests.cpp)
target_link_libraries(ShaderCacheTests PRIVATE Pyramid::Engine)
set_target_properties(ShaderCacheTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ShaderCache COMMAND $<TARGET_FILE:ShaderCacheTests>)

add_executable(TextureCacheTests TextureCacheTests.cpp)
target_link_libraries(TextureCacheTests PRIVATE Pyramid::Engine)
set_target_properties(TextureCacheTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.TextureCache COMMAND $<TARGET_FILE:TextureCacheTests>)

add_executable(MaterialResourceTests MaterialResourceTests.cpp)
target_link_libraries(MaterialResourceTests PRIVATE Pyramid::Engine)
set_target_properties(MaterialResourceTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.MaterialResource COMMAND $<TARGET_FILE:MaterialResourceTests>)
