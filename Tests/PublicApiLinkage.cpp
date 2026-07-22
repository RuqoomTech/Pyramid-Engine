#include <Pyramid/Core/Game.hpp>
#include <Pyramid/Graphics/Texture.hpp>
#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/OpenGL/OpenGLFramebuffer.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>

#include <memory>
#include <string>

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

    volatile decltype(&SceneManager::LoadScene) g_loadScene = &SceneManager::LoadScene;
    volatile decltype(&SceneManager::SaveScene) g_saveScene = &SceneManager::SaveScene;
    volatile decltype(&SceneManager::GetObjectsInBox) g_getObjectsInBox = &SceneManager::GetObjectsInBox;
    volatile decltype(&SceneManager::UpdateVisibility) g_updateVisibility = &SceneManager::UpdateVisibility;
    volatile decltype(&SceneManager::RegisterEventCallback) g_registerEvent = &SceneManager::RegisterEventCallback;
    volatile decltype(&SceneManager::TriggerEvent) g_triggerEvent = &SceneManager::TriggerEvent;
    volatile decltype(&SceneManager::DrawDebugInfo) g_drawDebugInfo = &SceneManager::DrawDebugInfo;
    volatile decltype(&Pyramid::SceneManagement::Octree::UpdateIfMoved) g_updateOctreeIfMoved =
        &Pyramid::SceneManagement::Octree::UpdateIfMoved;
    volatile decltype(&Pyramid::SceneManagement::Octree::Synchronize) g_synchronizeOctree =
        &Pyramid::SceneManagement::Octree::Synchronize;
    volatile decltype(&Pyramid::Camera::SetViewportSize) g_setCameraViewport =
        &Pyramid::Camera::SetViewportSize;
    volatile decltype(&Pyramid::Camera::GetFrustumPlanes) g_getCameraFrustumPlanes =
        &Pyramid::Camera::GetFrustumPlanes;
    volatile decltype(&Pyramid::Camera::IsAABBVisible) g_isCameraAABBVisible =
        &Pyramid::Camera::IsAABBVisible;
    volatile decltype(&Pyramid::RenderObject::GetWorldBounds) g_getRenderObjectWorldBounds =
        &Pyramid::RenderObject::GetWorldBounds;
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
                   g_loadScene &&
                   g_saveScene &&
                   g_getObjectsInBox &&
                   g_updateVisibility &&
                   g_registerEvent &&
                   g_triggerEvent &&
                   g_drawDebugInfo &&
                   g_updateOctreeIfMoved &&
                   g_synchronizeOctree &&
                   g_setCameraViewport &&
                   g_getCameraFrustumPlanes &&
                   g_isCameraAABBVisible &&
                   g_getRenderObjectWorldBounds &&
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
