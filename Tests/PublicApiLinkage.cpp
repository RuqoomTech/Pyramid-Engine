#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/Texture/TextureResource.hpp>
#include <Pyramid/Graphics/Texture/TextureCache.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Geometry/MeshBounds.hpp>
#include <Pyramid/Graphics/Geometry/Mesh.hpp>
#include <Pyramid/Graphics/Geometry/MeshCache.hpp>
#include <Pyramid/Graphics/Shader/ShaderProgram.hpp>
#include <Pyramid/Graphics/Shader/ShaderCache.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>

#include <memory>
#include <string>
#include <string_view>

namespace
{
    class GameLinkageProbe final : public Pyramid::Game
    {
    public:
        using Pyramid::Game::SetRenderSystem;
    };

    using Pyramid::ITexture2D;
    using Pyramid::TextureFormat;
    using Pyramid::TextureSpecification;
    using Pyramid::SceneManagement::SceneManager;

    using CreateTextureFromSpec = std::shared_ptr<ITexture2D> (*)(const TextureSpecification&, const void*);
    using CreateTextureFromFile = std::shared_ptr<ITexture2D> (*)(const std::string&, bool, bool);
    using CreateTextureBySize = std::shared_ptr<ITexture2D> (*)(Pyramid::u32, Pyramid::u32, TextureFormat);

    volatile CreateTextureFromSpec g_createTextureFromSpec =
        static_cast<CreateTextureFromSpec>(&ITexture2D::Create);
    volatile CreateTextureFromFile g_createTextureFromFile =
        static_cast<CreateTextureFromFile>(&ITexture2D::Create);
    volatile CreateTextureBySize g_createTextureBySize =
        static_cast<CreateTextureBySize>(&ITexture2D::Create);

    volatile decltype(&ITexture2D::CreateRenderTarget) g_createRenderTarget = &ITexture2D::CreateRenderTarget;
    volatile decltype(&ITexture2D::CreateDepthTarget) g_createDepthTarget = &ITexture2D::CreateDepthTarget;
    volatile decltype(&ITexture2D::CreateFromColor) g_createFromColor = &ITexture2D::CreateFromColor;

    using TextureAssetIdFromString = Pyramid::TextureAssetId (*)(std::string_view);
    volatile TextureAssetIdFromString g_textureAssetIdFromString =
        static_cast<TextureAssetIdFromString>(&Pyramid::TextureAssetId::FromString);
    volatile decltype(&Pyramid::TextureAssetId::ToString) g_textureAssetIdToString =
        &Pyramid::TextureAssetId::ToString;
    using CalculateMemoryTextureId = Pyramid::TextureAssetId (*)(
        const Pyramid::TextureResourceSpecification&);
    using CalculateFileTextureId = Pyramid::TextureAssetId (*)(
        const Pyramid::TextureFileSpecification&);
    volatile CalculateMemoryTextureId g_calculateMemoryTextureId =
        static_cast<CalculateMemoryTextureId>(&Pyramid::TextureResource::CalculateContentId);
    volatile CalculateFileTextureId g_calculateFileTextureId =
        static_cast<CalculateFileTextureId>(&Pyramid::TextureResource::CalculateContentId);
    volatile decltype(&Pyramid::TextureResource::Create) g_createTextureResource =
        &Pyramid::TextureResource::Create;
    volatile decltype(&Pyramid::TextureResource::CreateFromFile) g_createFileTextureResource =
        &Pyramid::TextureResource::CreateFromFile;
    using GetOrCreateMemoryTexture = std::shared_ptr<Pyramid::TextureResource>
        (Pyramid::TextureCache::*)(const Pyramid::TextureResourceSpecification&);
    using GetOrCreateFileTexture = std::shared_ptr<Pyramid::TextureResource>
        (Pyramid::TextureCache::*)(const Pyramid::TextureFileSpecification&);
    volatile GetOrCreateMemoryTexture g_getOrCreateMemoryTexture =
        static_cast<GetOrCreateMemoryTexture>(&Pyramid::TextureCache::GetOrCreate);
    volatile GetOrCreateFileTexture g_getOrCreateFileTexture =
        static_cast<GetOrCreateFileTexture>(&Pyramid::TextureCache::GetOrCreate);
    using ReloadTexture = bool (Pyramid::TextureCache::*)(Pyramid::TextureAssetId);
    using ReloadTextureReplacement = bool (Pyramid::TextureCache::*)(
        Pyramid::TextureAssetId, const Pyramid::TextureFileSpecification&);
    volatile ReloadTexture g_reloadTexture =
        static_cast<ReloadTexture>(&Pyramid::TextureCache::Reload);
    volatile ReloadTextureReplacement g_reloadTextureReplacement =
        static_cast<ReloadTextureReplacement>(&Pyramid::TextureCache::Reload);
    volatile decltype(&Pyramid::TextureCache::Find) g_findCachedTexture =
        &Pyramid::TextureCache::Find;
    volatile decltype(&Pyramid::TextureCache::Evict) g_evictCachedTexture =
        &Pyramid::TextureCache::Evict;
    volatile decltype(&Pyramid::TextureCache::CollectUnused) g_collectUnusedTextures =
        &Pyramid::TextureCache::CollectUnused;
    volatile decltype(&Pyramid::TextureCache::Clear) g_clearTextureCache =
        &Pyramid::TextureCache::Clear;
    volatile decltype(&Pyramid::TextureCache::GetStats) g_getTextureCacheStats =
        &Pyramid::TextureCache::GetStats;

    volatile decltype(&SceneManager::LoadScene) g_loadScene = &SceneManager::LoadScene;
    volatile decltype(&SceneManager::SaveScene) g_saveScene = &SceneManager::SaveScene;
    volatile decltype(&SceneManager::GetObjectsInBox) g_getObjectsInBox = &SceneManager::GetObjectsInBox;
    volatile decltype(&SceneManager::QueryScene) g_queryScene = &SceneManager::QueryScene;
    volatile decltype(&SceneManager::GetNearestObject) g_getNearestObject =
        &SceneManager::GetNearestObject;
    volatile decltype(&SceneManager::GetKNearestObjects) g_getKNearestObjects =
        &SceneManager::GetKNearestObjects;
    volatile decltype(&Pyramid::SceneManagement::AABB::DistanceSquaredToPoint) g_aabbDistanceSquared =
        &Pyramid::SceneManagement::AABB::DistanceSquaredToPoint;
    volatile decltype(&Pyramid::SceneManagement::AABB::DistanceToPoint) g_aabbDistance =
        &Pyramid::SceneManagement::AABB::DistanceToPoint;
    volatile decltype(&Pyramid::SceneManagement::Octree::FindNearest) g_findOctreeNearest =
        &Pyramid::SceneManagement::Octree::FindNearest;
    volatile decltype(&Pyramid::SceneManagement::Octree::FindKNearest) g_findOctreeKNearest =
        &Pyramid::SceneManagement::Octree::FindKNearest;
    volatile decltype(&Pyramid::SceneManagement::Octree::QueryPoint) g_queryOctreePoint =
        &Pyramid::SceneManagement::Octree::QueryPoint;
    volatile decltype(&Pyramid::SceneManagement::Octree::QuerySphere) g_queryOctreeSphere =
        &Pyramid::SceneManagement::Octree::QuerySphere;
    volatile decltype(&Pyramid::SceneManagement::Octree::QueryBox) g_queryOctreeBox =
        &Pyramid::SceneManagement::Octree::QueryBox;
    volatile decltype(&Pyramid::SceneManagement::Octree::QueryRay) g_queryOctreeRay =
        &Pyramid::SceneManagement::Octree::QueryRay;
    volatile decltype(&SceneManager::UpdateVisibility) g_updateVisibility = &SceneManager::UpdateVisibility;
    volatile decltype(&SceneManager::RegisterEventCallback) g_registerEvent = &SceneManager::RegisterEventCallback;
    volatile decltype(&SceneManager::TriggerEvent) g_triggerEvent = &SceneManager::TriggerEvent;
    volatile decltype(&SceneManager::DrawDebugInfo) g_drawDebugInfo = &SceneManager::DrawDebugInfo;
    volatile decltype(&Pyramid::SceneManagement::Octree::UpdateIfMoved) g_updateOctreeIfMoved =
        &Pyramid::SceneManagement::Octree::UpdateIfMoved;
    volatile decltype(&Pyramid::SceneManagement::Octree::Synchronize) g_synchronizeOctree =
        &Pyramid::SceneManagement::Octree::Synchronize;
    volatile decltype(&Pyramid::SceneManagement::Octree::Configure) g_configureOctree =
        &Pyramid::SceneManagement::Octree::Configure;
    volatile decltype(&Pyramid::SceneManagement::Octree::GetConfiguration) g_getOctreeConfiguration =
        &Pyramid::SceneManagement::Octree::GetConfiguration;
    volatile decltype(&Pyramid::SceneManagement::Octree::Compact) g_compactOctree =
        &Pyramid::SceneManagement::Octree::Compact;
    volatile decltype(&Pyramid::SceneManagement::Octree::GetStats) g_getOctreeStats =
        &Pyramid::SceneManagement::Octree::GetStats;
    volatile decltype(&Pyramid::SceneManagement::Octree::GetLastCompactionStats) g_getLastOctreeCompaction =
        &Pyramid::SceneManagement::Octree::GetLastCompactionStats;
    volatile decltype(&Pyramid::Camera::SetViewportSize) g_setCameraViewport =
        &Pyramid::Camera::SetViewportSize;
    volatile decltype(&Pyramid::Camera::GetFrustumPlanes) g_getCameraFrustumPlanes =
        &Pyramid::Camera::GetFrustumPlanes;
    volatile decltype(&Pyramid::Camera::IsAABBVisible) g_isCameraAABBVisible =
        &Pyramid::Camera::IsAABBVisible;
    volatile decltype(&Pyramid::RenderObject::TryGetGeometryBounds) g_tryGetGeometryBounds =
        &Pyramid::RenderObject::TryGetGeometryBounds;
    volatile decltype(&Pyramid::RenderObject::GetLocalBounds) g_getRenderObjectLocalBounds =
        &Pyramid::RenderObject::GetLocalBounds;
    volatile decltype(&Pyramid::RenderObject::UseAutomaticBounds) g_useAutomaticBounds =
        &Pyramid::RenderObject::UseAutomaticBounds;
    volatile decltype(&Pyramid::RenderObject::GetWorldBounds) g_getRenderObjectWorldBounds =
        &Pyramid::RenderObject::GetWorldBounds;
    volatile decltype(&Pyramid::Geometry::CalculateLocalBounds) g_calculateGeometryBounds =
        &Pyramid::Geometry::CalculateLocalBounds;
    using MeshAssetIdFromString = Pyramid::MeshAssetId (*)(std::string_view);
    volatile MeshAssetIdFromString g_meshAssetIdFromString =
        static_cast<MeshAssetIdFromString>(&Pyramid::MeshAssetId::FromString);
    volatile decltype(&Pyramid::MeshAssetId::ToString) g_meshAssetIdToString =
        &Pyramid::MeshAssetId::ToString;
    volatile decltype(&Pyramid::Mesh::CalculateContentId) g_calculateMeshContentId =
        &Pyramid::Mesh::CalculateContentId;
    volatile decltype(&Pyramid::Mesh::Create) g_createMesh = &Pyramid::Mesh::Create;
    volatile decltype(&Pyramid::Mesh::IsValid) g_isMeshValid = &Pyramid::Mesh::IsValid;
    volatile decltype(&Pyramid::Mesh::GetLocalBounds) g_getMeshLocalBounds =
        &Pyramid::Mesh::GetLocalBounds;
    volatile decltype(&Pyramid::MeshCache::GetOrCreate) g_getOrCreateCachedMesh =
        &Pyramid::MeshCache::GetOrCreate;
    volatile decltype(&Pyramid::MeshCache::Find) g_findCachedMesh =
        &Pyramid::MeshCache::Find;
    volatile decltype(&Pyramid::MeshCache::Evict) g_evictCachedMesh =
        &Pyramid::MeshCache::Evict;
    volatile decltype(&Pyramid::MeshCache::CollectUnused) g_collectUnusedMeshes =
        &Pyramid::MeshCache::CollectUnused;
    volatile decltype(&Pyramid::MeshCache::Clear) g_clearMeshCache =
        &Pyramid::MeshCache::Clear;
    volatile decltype(&Pyramid::MeshCache::GetStats) g_getMeshCacheStats =
        &Pyramid::MeshCache::GetStats;
    using ShaderAssetIdFromString = Pyramid::ShaderAssetId (*)(std::string_view);
    volatile ShaderAssetIdFromString g_shaderAssetIdFromString =
        static_cast<ShaderAssetIdFromString>(&Pyramid::ShaderAssetId::FromString);
    volatile decltype(&Pyramid::ShaderAssetId::ToString) g_shaderAssetIdToString =
        &Pyramid::ShaderAssetId::ToString;
    volatile decltype(&Pyramid::ShaderProgram::CalculateContentId) g_calculateShaderContentId =
        &Pyramid::ShaderProgram::CalculateContentId;
    volatile decltype(&Pyramid::ShaderProgram::Create) g_createShaderProgram =
        &Pyramid::ShaderProgram::Create;
    volatile decltype(&Pyramid::ShaderProgram::IsValid) g_isShaderProgramValid =
        &Pyramid::ShaderProgram::IsValid;
    volatile decltype(&Pyramid::ShaderCache::GetOrCreate) g_getOrCreateCachedShader =
        &Pyramid::ShaderCache::GetOrCreate;
    volatile decltype(&Pyramid::ShaderCache::Recompile) g_recompileCachedShader =
        &Pyramid::ShaderCache::Recompile;
    volatile decltype(&Pyramid::ShaderCache::Find) g_findCachedShader =
        &Pyramid::ShaderCache::Find;
    volatile decltype(&Pyramid::ShaderCache::Evict) g_evictCachedShader =
        &Pyramid::ShaderCache::Evict;
    volatile decltype(&Pyramid::ShaderCache::CollectUnused) g_collectUnusedShaders =
        &Pyramid::ShaderCache::CollectUnused;
    volatile decltype(&Pyramid::ShaderCache::Clear) g_clearShaderCache =
        &Pyramid::ShaderCache::Clear;
    volatile decltype(&Pyramid::ShaderCache::GetStats) g_getShaderCacheStats =
        &Pyramid::ShaderCache::GetStats;
    volatile decltype(&Pyramid::Renderer::CommandBuffer::DrawMesh) g_drawMesh =
        &Pyramid::Renderer::CommandBuffer::DrawMesh;
    volatile decltype(&Pyramid::OpenGLFramebuffer::Resize) g_resizeFramebuffer =
        &Pyramid::OpenGLFramebuffer::Resize;
    volatile decltype(&Pyramid::Renderer::RenderTarget::Resize) g_resizeRenderTarget =
        &Pyramid::Renderer::RenderTarget::Resize;
    volatile decltype(&Pyramid::Renderer::RenderSystem::Resize) g_resizeRenderSystem =
        &Pyramid::Renderer::RenderSystem::Resize;
    volatile decltype(&GameLinkageProbe::SetRenderSystem) g_setRenderSystem =
        &GameLinkageProbe::SetRenderSystem;
    volatile decltype(&Pyramid::SceneNode::SetLocalPosition) g_setNodeLocalPosition =
        &Pyramid::SceneNode::SetLocalPosition;
    volatile decltype(&Pyramid::SceneNode::SetLocalRotation) g_setNodeLocalRotation =
        &Pyramid::SceneNode::SetLocalRotation;
    volatile decltype(&Pyramid::SceneNode::SetLocalScale) g_setNodeLocalScale =
        &Pyramid::SceneNode::SetLocalScale;
    volatile decltype(&Pyramid::SceneNode::TransformPointToWorld) g_transformPointToWorld =
        &Pyramid::SceneNode::TransformPointToWorld;
    volatile decltype(&Pyramid::SceneNode::TransformDirectionToWorld) g_transformDirectionToWorld =
        &Pyramid::SceneNode::TransformDirectionToWorld;
}

int main()
{
    return g_createTextureFromSpec &&
                   g_createTextureFromFile &&
                   g_createTextureBySize &&
                   g_createRenderTarget &&
                   g_createDepthTarget &&
                   g_createFromColor &&
                   g_textureAssetIdFromString &&
                   g_textureAssetIdToString &&
                   g_calculateMemoryTextureId &&
                   g_calculateFileTextureId &&
                   g_createTextureResource &&
                   g_createFileTextureResource &&
                   g_getOrCreateMemoryTexture &&
                   g_getOrCreateFileTexture &&
                   g_reloadTexture &&
                   g_reloadTextureReplacement &&
                   g_findCachedTexture &&
                   g_evictCachedTexture &&
                   g_collectUnusedTextures &&
                   g_clearTextureCache &&
                   g_getTextureCacheStats &&
                   g_loadScene &&
                   g_saveScene &&
                   g_getObjectsInBox &&
                   g_queryScene &&
                   g_getNearestObject &&
                   g_getKNearestObjects &&
                   g_aabbDistanceSquared &&
                   g_aabbDistance &&
                   g_findOctreeNearest &&
                   g_findOctreeKNearest &&
                   g_queryOctreePoint &&
                   g_queryOctreeSphere &&
                   g_queryOctreeBox &&
                   g_queryOctreeRay &&
                   g_updateVisibility &&
                   g_registerEvent &&
                   g_triggerEvent &&
                   g_drawDebugInfo &&
                   g_updateOctreeIfMoved &&
                   g_synchronizeOctree &&
                   g_configureOctree &&
                   g_getOctreeConfiguration &&
                   g_compactOctree &&
                   g_getOctreeStats &&
                   g_getLastOctreeCompaction &&
                   g_setCameraViewport &&
                   g_getCameraFrustumPlanes &&
                   g_isCameraAABBVisible &&
                   g_tryGetGeometryBounds &&
                   g_getRenderObjectLocalBounds &&
                   g_useAutomaticBounds &&
                   g_getRenderObjectWorldBounds &&
                   g_calculateGeometryBounds &&
                   g_meshAssetIdFromString &&
                   g_meshAssetIdToString &&
                   g_calculateMeshContentId &&
                   g_createMesh &&
                   g_isMeshValid &&
                   g_getMeshLocalBounds &&
                   g_getOrCreateCachedMesh &&
                   g_findCachedMesh &&
                   g_evictCachedMesh &&
                   g_collectUnusedMeshes &&
                   g_clearMeshCache &&
                   g_getMeshCacheStats &&
                   g_shaderAssetIdFromString &&
                   g_shaderAssetIdToString &&
                   g_calculateShaderContentId &&
                   g_createShaderProgram &&
                   g_isShaderProgramValid &&
                   g_getOrCreateCachedShader &&
                   g_recompileCachedShader &&
                   g_findCachedShader &&
                   g_evictCachedShader &&
                   g_collectUnusedShaders &&
                   g_clearShaderCache &&
                   g_getShaderCacheStats &&
                   g_drawMesh &&
                   g_resizeFramebuffer &&
                   g_resizeRenderTarget &&
                   g_resizeRenderSystem &&
                   g_setRenderSystem &&
                   g_setNodeLocalPosition &&
                   g_setNodeLocalRotation &&
                   g_setNodeLocalScale &&
                   g_transformPointToWorld &&
                   g_transformDirectionToWorld
               ? 0
               : 1;
}
