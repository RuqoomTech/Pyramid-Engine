#include <Pyramid/Graphics/Scene.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Graphics/Geometry/Vertex.hpp>
#include <Pyramid/Graphics/Buffer/VertexArray.hpp>
#include <algorithm>
#include <vector>
#include <cmath>
#include <limits>

namespace Pyramid
{

    // RenderObject Implementation
    Math::Mat4 RenderObject::GetTransformMatrix() const
    {
        const Math::Quat normalizedRotation = Math::IsZero(rotation.LengthSquared())
                                                  ? Math::Quat::Identity
                                                  : rotation.Normalized();
        const Math::Mat4 scaleMatrix = Math::Mat4::CreateScale(scale);
        const Math::Mat4 rotationMatrix = normalizedRotation.ToMatrix4();
        const Math::Mat4 translationMatrix = Math::Mat4::CreateTranslation(position);

        return translationMatrix * rotationMatrix * scaleMatrix;
    }

    void RenderObject::SetLocalBounds(const Math::Vec3 &minPoint, const Math::Vec3 &maxPoint)
    {
        localBoundsMin = Math::Vec3(
            Math::Min(minPoint.x, maxPoint.x),
            Math::Min(minPoint.y, maxPoint.y),
            Math::Min(minPoint.z, maxPoint.z));
        localBoundsMax = Math::Vec3(
            Math::Max(minPoint.x, maxPoint.x),
            Math::Max(minPoint.y, maxPoint.y),
            Math::Max(minPoint.z, maxPoint.z));
        boundsMode = RenderBoundsMode::Manual;
    }

    bool RenderObject::TryGetGeometryBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
    {
        return vertexArray && vertexArray->TryGetLocalBounds(minPoint, maxPoint);
    }

    bool RenderObject::GetLocalBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
    {
        const bool usedGeometry =
            boundsMode == RenderBoundsMode::Automatic && TryGetGeometryBounds(minPoint, maxPoint);
        if (!usedGeometry)
        {
            minPoint = localBoundsMin;
            maxPoint = localBoundsMax;
        }

        const Math::Vec3 canonicalMin(
            Math::Min(minPoint.x, maxPoint.x),
            Math::Min(minPoint.y, maxPoint.y),
            Math::Min(minPoint.z, maxPoint.z));
        const Math::Vec3 canonicalMax(
            Math::Max(minPoint.x, maxPoint.x),
            Math::Max(minPoint.y, maxPoint.y),
            Math::Max(minPoint.z, maxPoint.z));
        minPoint = canonicalMin;
        maxPoint = canonicalMax;
        return usedGeometry;
    }

    void RenderObject::GetWorldBounds(Math::Vec3 &minPoint, Math::Vec3 &maxPoint) const
    {
        Math::Vec3 localMin;
        Math::Vec3 localMax;
        GetLocalBounds(localMin, localMax);

        const Math::Mat4 transform = GetTransformMatrix();
        const f32 maximum = std::numeric_limits<f32>::max();
        minPoint = Math::Vec3(maximum);
        maxPoint = Math::Vec3(-maximum);

        for (u32 corner = 0; corner < 8; ++corner)
        {
            const Math::Vec3 localCorner(
                (corner & 1u) != 0u ? localMax.x : localMin.x,
                (corner & 2u) != 0u ? localMax.y : localMin.y,
                (corner & 4u) != 0u ? localMax.z : localMin.z);
            const Math::Vec3 worldCorner = (transform * Math::Vec4(localCorner, 1.0f)).ToVec3();

            minPoint.x = Math::Min(minPoint.x, worldCorner.x);
            minPoint.y = Math::Min(minPoint.y, worldCorner.y);
            minPoint.z = Math::Min(minPoint.z, worldCorner.z);
            maxPoint.x = Math::Max(maxPoint.x, worldCorner.x);
            maxPoint.y = Math::Max(maxPoint.y, worldCorner.y);
            maxPoint.z = Math::Max(maxPoint.z, worldCorner.z);
        }
    }

    namespace
    {
        Math::Quat NormalizeRotation(const Math::Quat &rotation)
        {
            if (Math::IsZero(rotation.LengthSquared()))
            {
                return Math::Quat::Identity;
            }

            return rotation.Normalized();
        }

        Math::Vec3 ExtractWorldScale(const Math::Mat4 &transform)
        {
            const auto axisLength = [](f32 x, f32 y, f32 z)
            {
                return std::sqrt(x * x + y * y + z * z);
            };

            return Math::Vec3(
                axisLength(transform.m[0], transform.m[1], transform.m[2]),
                axisLength(transform.m[4], transform.m[5], transform.m[6]),
                axisLength(transform.m[8], transform.m[9], transform.m[10]));
        }
    }

    // SceneNode Implementation
    SceneNode::SceneNode(const std::string &name)
        : m_name(name)
    {
    }

    SceneNode::~SceneNode()
    {
        for (const auto &child : m_children)
        {
            if (!child)
            {
                continue;
            }

            child->m_parent.reset();
            child->MarkWorldTransformDirty();
        }

        m_children.clear();
    }

    bool SceneNode::HasAncestor(const SceneNode *node) const
    {
        if (!node)
        {
            return false;
        }

        auto ancestor = m_parent.lock();
        while (ancestor)
        {
            if (ancestor.get() == node)
            {
                return true;
            }

            ancestor = ancestor->m_parent.lock();
        }

        return false;
    }

    void SceneNode::AddChild(std::shared_ptr<SceneNode> child)
    {
        if (!child || child.get() == this || HasAncestor(child.get()))
        {
            return;
        }

        auto self = weak_from_this().lock();
        if (!self)
        {
            return;
        }

        if (auto currentParent = child->m_parent.lock())
        {
            if (currentParent.get() == this)
            {
                return;
            }

            currentParent->RemoveChild(child);
        }

        child->m_parent = self;
        m_children.push_back(child);
        child->MarkWorldTransformDirty();
    }

    void SceneNode::RemoveChild(std::shared_ptr<SceneNode> child)
    {
        if (!child)
        {
            return;
        }

        const auto oldSize = m_children.size();
        m_children.erase(
            std::remove(m_children.begin(), m_children.end(), child),
            m_children.end());

        if (m_children.size() == oldSize)
        {
            return;
        }

        if (auto currentParent = child->m_parent.lock(); currentParent.get() == this)
        {
            child->m_parent.reset();
        }

        child->MarkWorldTransformDirty();
    }

    void SceneNode::SetParent(std::shared_ptr<SceneNode> parent)
    {
        auto self = weak_from_this().lock();
        if (!self)
        {
            return;
        }

        if (parent)
        {
            parent->AddChild(self);
            return;
        }

        if (auto oldParent = m_parent.lock())
        {
            oldParent->RemoveChild(self);
        }
    }

    void SceneNode::SetLocalTransform(
        const Math::Vec3 &position,
        const Math::Quat &rotation,
        const Math::Vec3 &scale)
    {
        m_localPosition = position;
        m_localRotation = NormalizeRotation(rotation);
        m_localScale = scale;
        MarkLocalTransformDirty();
    }

    void SceneNode::SetLocalPosition(const Math::Vec3 &position)
    {
        m_localPosition = position;
        MarkLocalTransformDirty();
    }

    void SceneNode::SetLocalRotation(const Math::Quat &rotation)
    {
        m_localRotation = NormalizeRotation(rotation);
        MarkLocalTransformDirty();
    }

    void SceneNode::SetLocalScale(const Math::Vec3 &scale)
    {
        m_localScale = scale;
        MarkLocalTransformDirty();
    }

    Math::Vec3 SceneNode::GetWorldPosition() const
    {
        const Math::Mat4 &worldTransform = GetWorldTransform();
        return Math::Vec3(worldTransform.m[12], worldTransform.m[13], worldTransform.m[14]);
    }

    Math::Quat SceneNode::GetWorldRotation() const
    {
        GetWorldTransform();
        return m_worldRotation;
    }

    Math::Vec3 SceneNode::GetWorldScale() const
    {
        GetWorldTransform();
        return m_worldScale;
    }

    const Math::Mat4 &SceneNode::GetLocalTransform() const
    {
        if (m_transformDirty)
        {
            UpdateLocalTransform();
        }
        return m_localTransform;
    }

    const Math::Mat4 &SceneNode::GetWorldTransform() const
    {
        if (m_worldTransformDirty)
        {
            UpdateWorldTransform();
        }
        return m_worldTransform;
    }

    Math::Vec3 SceneNode::TransformPointToWorld(const Math::Vec3 &point) const
    {
        return (GetWorldTransform() * Math::Vec4(point, 1.0f)).ToVec3();
    }

    Math::Vec3 SceneNode::TransformDirectionToWorld(const Math::Vec3 &direction) const
    {
        return (GetWorldTransform() * Math::Vec4(direction, 0.0f)).ToVec3();
    }

    void SceneNode::MarkLocalTransformDirty()
    {
        m_transformDirty = true;
        MarkWorldTransformDirty();
    }

    void SceneNode::MarkWorldTransformDirty()
    {
        m_worldTransformDirty = true;

        for (const auto &child : m_children)
        {
            if (child)
            {
                child->MarkWorldTransformDirty();
            }
        }
    }

    void SceneNode::UpdateLocalTransform() const
    {
        const Math::Mat4 scaleMatrix = Math::Mat4::CreateScale(m_localScale);
        const Math::Mat4 rotationMatrix = m_localRotation.ToMatrix4();
        const Math::Mat4 translationMatrix = Math::Mat4::CreateTranslation(m_localPosition);

        m_localTransform = translationMatrix * rotationMatrix * scaleMatrix;
        m_transformDirty = false;
    }

    void SceneNode::UpdateWorldTransform() const
    {
        if (auto parent = m_parent.lock())
        {
            const Math::Mat4 &parentTransform = parent->GetWorldTransform();
            m_worldTransform = parentTransform * GetLocalTransform();
            m_worldRotation = NormalizeRotation(parent->m_worldRotation * m_localRotation);
        }
        else
        {
            m_worldTransform = GetLocalTransform();
            m_worldRotation = m_localRotation;
        }

        m_worldScale = ExtractWorldScale(m_worldTransform);
        m_worldTransformDirty = false;
    }

    // Scene Implementation
    Scene::Scene(const std::string &name)
        : m_name(name)
    {
        m_rootNode = std::make_shared<SceneNode>("Root");
    }

    std::shared_ptr<SceneNode> Scene::CreateNode(const std::string &name)
    {
        auto node = std::make_shared<SceneNode>(name);
        m_rootNode->AddChild(node);
        return node;
    }

    void Scene::RemoveNode(std::shared_ptr<SceneNode> node)
    {
        if (node && node != m_rootNode)
        {
            if (auto parent = node->m_parent.lock())
            {
                parent->RemoveChild(node);
            }
        }
    }

    std::shared_ptr<SceneNode> Scene::FindNode(const std::string &name) const
    {
        // Simple recursive search - could be optimized with a hash map
        std::function<std::shared_ptr<SceneNode>(std::shared_ptr<SceneNode>)> search;
        search = [&](std::shared_ptr<SceneNode> node) -> std::shared_ptr<SceneNode>
        {
            if (node->GetName() == name)
            {
                return node;
            }
            for (auto &child : node->m_children)
            {
                if (auto found = search(child))
                {
                    return found;
                }
            }
            return nullptr;
        };

        return search(m_rootNode);
    }

    void Scene::AddRenderObject(std::shared_ptr<RenderObject> object)
    {
        if (object)
        {
            m_renderObjects.push_back(object);
        }
    }

    void Scene::RemoveRenderObject(std::shared_ptr<RenderObject> object)
    {
        auto it = std::find(m_renderObjects.begin(), m_renderObjects.end(), object);
        if (it != m_renderObjects.end())
        {
            m_renderObjects.erase(it);
        }
    }

    void Scene::AddLight(std::shared_ptr<Light> light)
    {
        if (light)
        {
            m_lights.push_back(light);
        }
    }

    void Scene::RemoveLight(std::shared_ptr<Light> light)
    {
        auto it = std::find(m_lights.begin(), m_lights.end(), light);
        if (it != m_lights.end())
        {
            m_lights.erase(it);
        }
    }

    std::vector<std::shared_ptr<RenderObject>> Scene::GetVisibleObjects(const Camera &camera) const
    {
        std::vector<std::shared_ptr<RenderObject>> visibleObjects;

        for (const auto &object : m_renderObjects)
        {
            if (!object || !object->visible)
            {
                continue;
            }

            Math::Vec3 boundsMin;
            Math::Vec3 boundsMax;
            object->GetWorldBounds(boundsMin, boundsMax);
            if (camera.IsAABBVisible(boundsMin, boundsMax))
            {
                visibleObjects.push_back(object);
            }
        }

        return visibleObjects;
    }

    std::vector<std::shared_ptr<Light>> Scene::GetVisibleLights(const Camera &camera) const
    {
        (void)camera;
        std::vector<std::shared_ptr<Light>> visibleLights;

        for (const auto &light : m_lights)
        {
            if (light && light->enabled)
            {
                // For now, include all lights - could add light culling later
                visibleLights.push_back(light);
            }
        }

        return visibleLights;
    }

    void Scene::Clear()
    {
        m_renderObjects.clear();
        m_lights.clear();
        m_primaryLight.reset();
        m_rootNode = std::make_shared<SceneNode>("Root");
    }

    // SceneUtils Implementation
    namespace SceneUtils
    {
        std::shared_ptr<Scene> CreateTestScene()
        {
            auto scene = std::make_shared<Scene>("Test Scene");

            // Create a simple directional light
            auto light = CreateDirectionalLight(Math::Vec3(0.5f, -1.0f, 0.5f));
            scene->AddLight(light);
            scene->SetPrimaryLight(light);

            // Set up basic environment
            auto &env = scene->GetEnvironment();
            env.skyColor = Math::Vec3(0.5f, 0.7f, 1.0f);
            env.ambientColor = Math::Vec3(0.1f, 0.1f, 0.1f);

            return scene;
        }

        std::shared_ptr<RenderObject> CreateCube(f32 size)
        {
            auto object = std::make_shared<RenderObject>();
            object->name = "Cube";

            // Create cube geometry
            f32 halfSize = size * 0.5f;
            object->SetLocalBounds(Math::Vec3(-halfSize), Math::Vec3(halfSize));
            
            // Cube vertices (position + color)
            std::vector<Vertex> vertices = {
                // Front face (red tint)
                {-halfSize, -halfSize,  halfSize, 1.0f, 0.8f, 0.8f, 1.0f},
                { halfSize, -halfSize,  halfSize, 1.0f, 0.8f, 0.8f, 1.0f},
                { halfSize,  halfSize,  halfSize, 1.0f, 0.8f, 0.8f, 1.0f},
                {-halfSize,  halfSize,  halfSize, 1.0f, 0.8f, 0.8f, 1.0f},
                
                // Back face (green tint)
                {-halfSize, -halfSize, -halfSize, 0.8f, 1.0f, 0.8f, 1.0f},
                { halfSize, -halfSize, -halfSize, 0.8f, 1.0f, 0.8f, 1.0f},
                { halfSize,  halfSize, -halfSize, 0.8f, 1.0f, 0.8f, 1.0f},
                {-halfSize,  halfSize, -halfSize, 0.8f, 1.0f, 0.8f, 1.0f},
                
                // Left face (blue tint)
                {-halfSize, -halfSize, -halfSize, 0.8f, 0.8f, 1.0f, 1.0f},
                {-halfSize, -halfSize,  halfSize, 0.8f, 0.8f, 1.0f, 1.0f},
                {-halfSize,  halfSize,  halfSize, 0.8f, 0.8f, 1.0f, 1.0f},
                {-halfSize,  halfSize, -halfSize, 0.8f, 0.8f, 1.0f, 1.0f},
                
                // Right face (yellow tint)
                { halfSize, -halfSize, -halfSize, 1.0f, 1.0f, 0.8f, 1.0f},
                { halfSize, -halfSize,  halfSize, 1.0f, 1.0f, 0.8f, 1.0f},
                { halfSize,  halfSize,  halfSize, 1.0f, 1.0f, 0.8f, 1.0f},
                { halfSize,  halfSize, -halfSize, 1.0f, 1.0f, 0.8f, 1.0f},
                
                // Top face (magenta tint)
                {-halfSize,  halfSize, -halfSize, 1.0f, 0.8f, 1.0f, 1.0f},
                { halfSize,  halfSize, -halfSize, 1.0f, 0.8f, 1.0f, 1.0f},
                { halfSize,  halfSize,  halfSize, 1.0f, 0.8f, 1.0f, 1.0f},
                {-halfSize,  halfSize,  halfSize, 1.0f, 0.8f, 1.0f, 1.0f},
                
                // Bottom face (cyan tint)
                {-halfSize, -halfSize, -halfSize, 0.8f, 1.0f, 1.0f, 1.0f},
                { halfSize, -halfSize, -halfSize, 0.8f, 1.0f, 1.0f, 1.0f},
                { halfSize, -halfSize,  halfSize, 0.8f, 1.0f, 1.0f, 1.0f},
                {-halfSize, -halfSize,  halfSize, 0.8f, 1.0f, 1.0f, 1.0f}
            };
            
            // Cube indices
            std::vector<u32> indices = {
                // Front face
                0, 1, 2,  2, 3, 0,
                // Back face
                4, 6, 5,  6, 4, 7,
                // Left face
                8, 9, 10,  10, 11, 8,
                // Right face
                12, 14, 13,  14, 12, 15,
                // Top face
                16, 17, 18,  18, 19, 16,
                // Bottom face
                20, 22, 21,  22, 20, 23
            };

            // Create vertex array and buffers (would need graphics device access)
            // For now, store the geometry data in the object for later use
            // TODO: This requires access to graphics device to create actual buffers

            return object;
        }

        std::shared_ptr<RenderObject> CreateSphere(f32 radius, u32 segments)
        {
            auto object = std::make_shared<RenderObject>();
            object->name = "Sphere";
            object->SetLocalBounds(Math::Vec3(-radius), Math::Vec3(radius));

            // Create sphere geometry using UV sphere algorithm
            segments = (std::max)(segments, 8u); // Minimum 8 segments
            u32 rings = segments / 2;
            
            std::vector<Vertex> vertices;
            std::vector<u32> indices;
            
            // Generate vertices
            for (u32 ring = 0; ring <= rings; ++ring)
            {
                f32 phi = Math::PI * static_cast<f32>(ring) / static_cast<f32>(rings);
                f32 y = radius * std::cos(phi);
                f32 ringRadius = radius * std::sin(phi);
                
                for (u32 segment = 0; segment <= segments; ++segment)
                {
                    f32 theta = 2.0f * Math::PI * static_cast<f32>(segment) / static_cast<f32>(segments);
                    f32 x = ringRadius * std::cos(theta);
                    f32 z = ringRadius * std::sin(theta);
                    
                    // Color based on position (creates a nice gradient effect)
                    f32 r = 0.5f + 0.5f * std::sin(theta);
                    f32 g = 0.5f + 0.5f * std::cos(phi);
                    f32 b = 0.5f + 0.5f * std::sin(phi + theta);
                    
                    vertices.emplace_back(x, y, z, r, g, b, 1.0f);
                }
            }
            
            // Generate indices
            for (u32 ring = 0; ring < rings; ++ring)
            {
                for (u32 segment = 0; segment < segments; ++segment)
                {
                    u32 current = ring * (segments + 1) + segment;
                    u32 next = current + segments + 1;
                    
                    // First triangle
                    indices.push_back(current);
                    indices.push_back(next);
                    indices.push_back(current + 1);
                    
                    // Second triangle
                    indices.push_back(current + 1);
                    indices.push_back(next);
                    indices.push_back(next + 1);
                }
            }
            
            // Store geometry data for later buffer creation
            // TODO: This requires access to graphics device to create actual buffers

            return object;
        }

        std::shared_ptr<RenderObject> CreatePlane(f32 width, f32 height)
        {
            auto object = std::make_shared<RenderObject>();
            object->name = "Plane";

            // Create plane geometry (XZ plane, facing up)
            f32 halfWidth = width * 0.5f;
            f32 halfHeight = height * 0.5f;
            object->SetLocalBounds(
                Math::Vec3(-halfWidth, 0.0f, -halfHeight),
                Math::Vec3(halfWidth, 0.0f, halfHeight));
            
            std::vector<Vertex> vertices = {
                // Plane vertices (white color)
                {-halfWidth, 0.0f, -halfHeight, 1.0f, 1.0f, 1.0f, 1.0f}, // Bottom-left
                { halfWidth, 0.0f, -halfHeight, 1.0f, 1.0f, 1.0f, 1.0f}, // Bottom-right
                { halfWidth, 0.0f,  halfHeight, 1.0f, 1.0f, 1.0f, 1.0f}, // Top-right
                {-halfWidth, 0.0f,  halfHeight, 1.0f, 1.0f, 1.0f, 1.0f}  // Top-left
            };
            
            std::vector<u32> indices = {
                // Two triangles to form a quad
                0, 1, 2,  // First triangle
                2, 3, 0   // Second triangle
            };
            
            // Store geometry data for later buffer creation
            // TODO: This requires access to graphics device to create actual buffers

            return object;
        }

        std::shared_ptr<Light> CreateDirectionalLight(const Math::Vec3 &direction,
                                                      const Math::Vec3 &color,
                                                      f32 intensity)
        {
            auto light = std::make_shared<Light>();
            light->type = LightType::Directional;
            light->direction = direction.Normalized();
            light->color = color;
            light->intensity = intensity;
            light->name = "Directional Light";

            return light;
        }
    }

} // namespace Pyramid
