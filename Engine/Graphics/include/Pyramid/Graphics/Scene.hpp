#pragma once

#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Graphics/Renderer/RenderSystem.hpp>
#include <Pyramid/Graphics/Material/Material.hpp>
#include <vector>
#include <memory>
#include <string>

namespace Pyramid
{
    // Forward declarations
    class Mesh;
    class ITexture2D;

    enum class RenderBoundsMode
    {
        Automatic,
        Manual
    };

    /**
     * @brief Renderable object in the scene
     */
    struct RenderObject
    {
        // Transform
        Math::Vec3 position = Math::Vec3::Zero;
        Math::Quat rotation = Math::Quat::Identity;
        Math::Vec3 scale = Math::Vec3::One;

        // Rendering data
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;

        // Fallback/manual local-space bounds. Automatic mode uses immutable mesh bounds.
        Math::Vec3 localBoundsMin = Math::Vec3(-0.5f);
        Math::Vec3 localBoundsMax = Math::Vec3(0.5f);
        RenderBoundsMode boundsMode = RenderBoundsMode::Automatic;

        // Metadata
        std::string name;
        bool visible = true;
        bool castShadows = true;
        bool receiveShadows = true;

        // Utility functions
        Math::Mat4 GetTransformMatrix() const;
        Math::Vec3 GetWorldPosition() const { return position; }
        void SetWorldPosition(const Math::Vec3 &pos) { position = pos; }
        void SetLocalBounds(const Math::Vec3 &minPoint, const Math::Vec3 &maxPoint);
        void UseAutomaticBounds() { boundsMode = RenderBoundsMode::Automatic; }
        RenderBoundsMode GetBoundsMode() const { return boundsMode; }
        bool TryGetGeometryBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const;
        bool GetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const;
        void GetWorldBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const;
    };

    /**
     * @brief Light types for scene lighting
     */
    enum class LightType
    {
        Directional,
        Point,
        Spot,
        Area
    };

    /**
     * @brief Light object in the scene
     */
    struct Light
    {
        LightType type = LightType::Directional;

        // Transform
        Math::Vec3 position = Math::Vec3::Zero;
        Math::Vec3 direction = Math::Vec3(0.0f, -1.0f, 0.0f);

        // Light properties
        Math::Vec3 color = Math::Vec3::One;
        f32 intensity = 1.0f;
        f32 range = 10.0f;          // For point/spot lights
        f32 innerConeAngle = 30.0f; // For spot lights (degrees)
        f32 outerConeAngle = 45.0f; // For spot lights (degrees)

        // Shadow properties
        bool castShadows = true;
        f32 shadowBias = 0.005f;
        u32 shadowMapSize = 1024;

        // Metadata
        std::string name;
        bool enabled = true;
    };

    /**
     * @brief Environment settings for the scene
     */
    struct Environment
    {
        // Sky/Background
        Math::Vec3 skyColor = Math::Vec3(0.5f, 0.7f, 1.0f);
        std::shared_ptr<ITexture2D> skyboxTexture;
        std::shared_ptr<ITexture2D> environmentMap; // HDR environment map for IBL

        // Ambient lighting
        Math::Vec3 ambientColor = Math::Vec3(0.1f, 0.1f, 0.1f);
        f32 ambientIntensity = 1.0f;

        // Fog
        bool fogEnabled = false;
        Math::Vec3 fogColor = Math::Vec3(0.7f, 0.7f, 0.7f);
        f32 fogDensity = 0.01f;
        f32 fogStart = 10.0f;
        f32 fogEnd = 100.0f;

        // Post-processing
        f32 exposure = 1.0f;
        f32 gamma = 2.2f;
    };

    /**
     * @brief Scene graph node for hierarchical transforms
     */
    class SceneNode : public std::enable_shared_from_this<SceneNode>
    {
    public:
        SceneNode(const std::string &name = "Node");
        ~SceneNode();

        // Hierarchy management. Reparenting preserves the local transform.
        void AddChild(std::shared_ptr<SceneNode> child);
        void RemoveChild(std::shared_ptr<SceneNode> child);
        void SetParent(std::shared_ptr<SceneNode> parent);

        std::shared_ptr<SceneNode> GetParent() const { return m_parent.lock(); }
        const std::vector<std::shared_ptr<SceneNode>> &GetChildren() const { return m_children; }
        size_t GetChildCount() const { return m_children.size(); }
        bool HasParent() const { return !m_parent.expired(); }

        // Transform operations
        void SetLocalTransform(const Math::Vec3 &position, const Math::Quat &rotation, const Math::Vec3 &scale);
        void SetLocalPosition(const Math::Vec3 &position);
        void SetLocalRotation(const Math::Quat &rotation);
        void SetLocalScale(const Math::Vec3 &scale);

        // Accessors
        const Math::Vec3 &GetLocalPosition() const { return m_localPosition; }
        const Math::Quat &GetLocalRotation() const { return m_localRotation; }
        const Math::Vec3 &GetLocalScale() const { return m_localScale; }

        Math::Vec3 GetWorldPosition() const;
        Math::Quat GetWorldRotation() const;
        Math::Vec3 GetWorldScale() const;

        const Math::Mat4 &GetLocalTransform() const;
        const Math::Mat4 &GetWorldTransform() const;
        Math::Vec3 TransformPointToWorld(const Math::Vec3 &point) const;
        Math::Vec3 TransformDirectionToWorld(const Math::Vec3 &direction) const;

        // Metadata
        const std::string &GetName() const { return m_name; }
        void SetName(const std::string &name) { m_name = name; }

        bool IsVisible() const { return m_visible; }
        void SetVisible(bool visible) { m_visible = visible; }

        // Render object attachment
        void SetRenderObject(std::shared_ptr<RenderObject> object) { m_renderObject = object; }
        std::shared_ptr<RenderObject> GetRenderObject() const { return m_renderObject; }

    private:
        bool HasAncestor(const SceneNode *node) const;
        void MarkLocalTransformDirty();
        void MarkWorldTransformDirty();
        void UpdateLocalTransform() const;
        void UpdateWorldTransform() const;

        std::string m_name;
        bool m_visible = true;

        // Hierarchy
        std::weak_ptr<SceneNode> m_parent;
        std::vector<std::shared_ptr<SceneNode>> m_children;

        // Transform
        Math::Vec3 m_localPosition = Math::Vec3::Zero;
        Math::Quat m_localRotation = Math::Quat::Identity;
        Math::Vec3 m_localScale = Math::Vec3::One;

        mutable Math::Mat4 m_localTransform = Math::Mat4::Identity;
        mutable Math::Mat4 m_worldTransform = Math::Mat4::Identity;
        mutable Math::Quat m_worldRotation = Math::Quat::Identity;
        mutable Math::Vec3 m_worldScale = Math::Vec3::One;
        mutable bool m_transformDirty = true;
        mutable bool m_worldTransformDirty = true;

        // Attached render object
        std::shared_ptr<RenderObject> m_renderObject;

        // Friend classes for access to private members
        friend class Scene;
    };

    /**
     * @brief Main scene class containing all renderable objects and lights
     */
    class Scene
    {
    public:
        Scene(const std::string &name = "Scene");
        ~Scene() = default;

        // Scene graph management
        std::shared_ptr<SceneNode> CreateNode(const std::string &name = "Node");
        void RemoveNode(std::shared_ptr<SceneNode> node);
        std::shared_ptr<SceneNode> FindNode(const std::string &name) const;
        std::shared_ptr<SceneNode> GetRootNode() const { return m_rootNode; }

        // Object management
        void AddRenderObject(std::shared_ptr<RenderObject> object);
        void RemoveRenderObject(std::shared_ptr<RenderObject> object);
        const std::vector<std::shared_ptr<RenderObject>> &GetRenderObjects() const { return m_renderObjects; }

        // Light management
        void AddLight(std::shared_ptr<Light> light);
        void RemoveLight(std::shared_ptr<Light> light);
        const std::vector<std::shared_ptr<Light>> &GetLights() const { return m_lights; }

        // Primary directional light (sun)
        void SetPrimaryLight(std::shared_ptr<Light> light) { m_primaryLight = light; }
        std::shared_ptr<Light> GetPrimaryLight() const { return m_primaryLight; }

        // Environment
        Environment &GetEnvironment() { return m_environment; }
        const Environment &GetEnvironment() const { return m_environment; }

        // Culling and optimization
        std::vector<std::shared_ptr<RenderObject>> GetVisibleObjects(const class Camera &camera) const;
        std::vector<std::shared_ptr<Light>> GetVisibleLights(const class Camera &camera) const;

        // Utility functions
        void Clear();
        size_t GetObjectCount() const { return m_renderObjects.size(); }
        size_t GetLightCount() const { return m_lights.size(); }

        // Metadata
        const std::string &GetName() const { return m_name; }
        void SetName(const std::string &name) { m_name = name; }

    private:
        std::string m_name;

        // Scene graph
        std::shared_ptr<SceneNode> m_rootNode;

        // Render objects and lights
        std::vector<std::shared_ptr<RenderObject>> m_renderObjects;
        std::vector<std::shared_ptr<Light>> m_lights;
        std::shared_ptr<Light> m_primaryLight; // Main directional light

        // Environment settings
        Environment m_environment;
    };

    /**
     * @brief Utility functions for scene creation
     */
    namespace SceneUtils
    {
        /**
         * @brief Create a simple test scene with basic objects
         * @return Shared pointer to created scene
         */
        std::shared_ptr<Scene> CreateTestScene();

        /**
         * @brief Create a cube render object
         * @param size Cube size
         * @return Shared pointer to render object
         */
        std::shared_ptr<RenderObject> CreateCube(f32 size = 1.0f);

        /**
         * @brief Create a sphere render object
         * @param radius Sphere radius
         * @param segments Number of segments
         * @return Shared pointer to render object
         */
        std::shared_ptr<RenderObject> CreateSphere(f32 radius = 1.0f, u32 segments = 32);

        /**
         * @brief Create a plane render object
         * @param width Plane width
         * @param height Plane height
         * @return Shared pointer to render object
         */
        std::shared_ptr<RenderObject> CreatePlane(f32 width = 1.0f, f32 height = 1.0f);

        /**
         * @brief Create a directional light (sun)
         * @param direction Light direction
         * @param color Light color
         * @param intensity Light intensity
         * @return Shared pointer to light
         */
        std::shared_ptr<Light> CreateDirectionalLight(const Math::Vec3 &direction,
                                                      const Math::Vec3 &color = Math::Vec3::One,
                                                      f32 intensity = 1.0f);
    }

} // namespace Pyramid
