add_executable(PublicApiLinkage PublicApiLinkage.cpp)
target_link_libraries(PublicApiLinkage PRIVATE Pyramid::Engine)
set_target_properties(PublicApiLinkage PROPERTIES FOLDER "Tests/API")
add_test(NAME API.PublicApiLinkage COMMAND $<TARGET_FILE:PublicApiLinkage>)

add_executable(OpenGLDiagnosticsTests OpenGLDiagnosticsTests.cpp)
target_link_libraries(OpenGLDiagnosticsTests PRIVATE Pyramid::Engine)
target_include_directories(OpenGLDiagnosticsTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Graphics/source")
set_target_properties(OpenGLDiagnosticsTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.OpenGLDiagnostics COMMAND $<TARGET_FILE:OpenGLDiagnosticsTests>)

add_executable(InputStateTests InputStateTests.cpp)
target_link_libraries(InputStateTests PRIVATE Pyramid::Input)
set_target_properties(InputStateTests PROPERTIES FOLDER "Tests/Platform")
add_test(NAME Platform.InputState COMMAND $<TARGET_FILE:InputStateTests>)

add_executable(InputActionTests InputActionTests.cpp)
target_link_libraries(InputActionTests PRIVATE Pyramid::Input)
set_target_properties(InputActionTests PROPERTIES FOLDER "Tests/Input")
add_test(NAME Input.ActionMapping COMMAND $<TARGET_FILE:InputActionTests>)

add_executable(InputTextEventTests InputTextEventTests.cpp)
target_link_libraries(InputTextEventTests PRIVATE Pyramid::Input)
set_target_properties(InputTextEventTests PROPERTIES FOLDER "Tests/Input")
add_test(NAME Input.TextEvents COMMAND $<TARGET_FILE:InputTextEventTests>)

add_executable(ClipboardEncodingTests ClipboardEncodingTests.cpp)
target_link_libraries(ClipboardEncodingTests PRIVATE Pyramid::Input)
set_target_properties(ClipboardEncodingTests PROPERTIES FOLDER "Tests/Platform")
add_test(NAME Platform.ClipboardEncoding COMMAND $<TARGET_FILE:ClipboardEncodingTests>)

add_executable(RuntimePathTests RuntimePathTests.cpp)
target_link_libraries(RuntimePathTests PRIVATE Pyramid::Engine)
set_target_properties(RuntimePathTests PROPERTIES FOLDER "Tests/Platform")
add_test(NAME Platform.RuntimePaths COMMAND $<TARGET_FILE:RuntimePathTests>)

if(WIN32)
    add_executable(SystemFontTests SystemFontTests.cpp)
    target_link_libraries(SystemFontTests PRIVATE Pyramid::Engine)
    set_target_properties(SystemFontTests PROPERTIES FOLDER "Tests/Platform")
    add_test(NAME Platform.SystemFonts COMMAND $<TARGET_FILE:SystemFontTests>)
endif()

add_executable(WindowResizeEventTests WindowResizeEventTests.cpp)
target_link_libraries(WindowResizeEventTests PRIVATE Pyramid::Input)
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

add_executable(CameraControllerTests CameraControllerTests.cpp)
target_link_libraries(CameraControllerTests PRIVATE Pyramid::Engine)
set_target_properties(CameraControllerTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.CameraControllers COMMAND $<TARGET_FILE:CameraControllerTests>)

add_executable(RTSInteractionTests RTSInteractionTests.cpp)
target_link_libraries(RTSInteractionTests PRIVATE Pyramid::RTSReference)
set_target_properties(RTSInteractionTests PROPERTIES FOLDER "Tests/Examples")
add_test(NAME Examples.RTSInteraction COMMAND $<TARGET_FILE:RTSInteractionTests>)

add_executable(FramebufferResizeTests FramebufferResizeTests.cpp)
target_link_libraries(FramebufferResizeTests PRIVATE Pyramid::Engine)
set_target_properties(FramebufferResizeTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.FramebufferResize COMMAND $<TARGET_FILE:FramebufferResizeTests>)

add_executable(TextureLoadingTests TextureLoadingTests.cpp)
target_link_libraries(TextureLoadingTests PRIVATE Pyramid::Engine)
set_target_properties(TextureLoadingTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.TextureLoading COMMAND $<TARGET_FILE:TextureLoadingTests>)

add_executable(ShadowArrayTests ShadowArrayTests.cpp)
target_link_libraries(ShadowArrayTests PRIVATE Pyramid::Engine)
set_target_properties(ShadowArrayTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ShadowArray COMMAND $<TARGET_FILE:ShadowArrayTests>)

add_executable(EntitySceneTests EntitySceneTests.cpp)
target_link_libraries(EntitySceneTests PRIVATE Pyramid::Engine)
set_target_properties(EntitySceneTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.EntityScene COMMAND $<TARGET_FILE:EntitySceneTests>)

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

add_executable(MaterialCacheTests MaterialCacheTests.cpp)
target_link_libraries(MaterialCacheTests PRIVATE Pyramid::Engine)
set_target_properties(MaterialCacheTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.MaterialCache COMMAND $<TARGET_FILE:MaterialCacheTests>)

add_executable(ResourceRegistryTests ResourceRegistryTests.cpp)
target_link_libraries(ResourceRegistryTests PRIVATE Pyramid::Engine)
set_target_properties(ResourceRegistryTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ResourceRegistry COMMAND $<TARGET_FILE:ResourceRegistryTests>)

add_executable(ResourceHandleTests ResourceHandleTests.cpp)
target_link_libraries(ResourceHandleTests PRIVATE Pyramid::Engine)
set_target_properties(ResourceHandleTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ResourceHandles COMMAND $<TARGET_FILE:ResourceHandleTests>)

add_executable(ResourceManifestTests ResourceManifestTests.cpp)
target_link_libraries(ResourceManifestTests PRIVATE Pyramid::Engine)
set_target_properties(ResourceManifestTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ResourceManifest COMMAND $<TARGET_FILE:ResourceManifestTests>)

add_executable(SceneSerializationTests SceneSerializationTests.cpp)
target_link_libraries(SceneSerializationTests PRIVATE Pyramid::Engine)
set_target_properties(SceneSerializationTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.SceneSerialization COMMAND $<TARGET_FILE:SceneSerializationTests>)

add_executable(ModelResourceImporterTests ModelResourceImporterTests.cpp)
target_link_libraries(ModelResourceImporterTests PRIVATE Pyramid::Engine)
set_target_properties(ModelResourceImporterTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ModelResourceImport COMMAND $<TARGET_FILE:ModelResourceImporterTests>)


add_executable(UIRendererTests UIRendererTests.cpp)
target_link_libraries(UIRendererTests PRIVATE Pyramid::Engine)
target_compile_definitions(UIRendererTests PRIVATE
    PYRAMID_UI_TEST_FONT="${PROJECT_SOURCE_DIR}/Examples/BasicGame/Assets/Fonts/PyramidSans-64-sdf.pfont"
)
set_target_properties(UIRendererTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.UIRenderer COMMAND $<TARGET_FILE:UIRendererTests>)

add_executable(CommandBufferStatsTests CommandBufferStatsTests.cpp)
target_link_libraries(CommandBufferStatsTests PRIVATE Pyramid::Engine)
set_target_properties(CommandBufferStatsTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.CommandBufferStats COMMAND $<TARGET_FILE:CommandBufferStatsTests>)

add_executable(ModelMaterialResourceImporterTests ModelMaterialResourceImporterTests.cpp)
target_link_libraries(ModelMaterialResourceImporterTests PRIVATE Pyramid::Engine)
set_target_properties(ModelMaterialResourceImporterTests PROPERTIES FOLDER "Tests/Graphics")
add_test(NAME Graphics.ModelMaterialResourceImport COMMAND $<TARGET_FILE:ModelMaterialResourceImporterTests>)

if(TARGET PyramidMinGWRuntime)
    get_property(_pyramid_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_pyramid_test_target IN LISTS _pyramid_test_targets)
        get_target_property(_pyramid_test_type "${_pyramid_test_target}" TYPE)
        if(_pyramid_test_type STREQUAL "EXECUTABLE")
            add_dependencies("${_pyramid_test_target}" PyramidMinGWRuntime)
        endif()
    endforeach()
endif()
