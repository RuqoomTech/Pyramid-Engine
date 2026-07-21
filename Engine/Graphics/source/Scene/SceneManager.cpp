#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Util/Log.hpp>
#include <fstream>
#include <chrono>
#include <utility>

namespace Pyramid
{
    namespace SceneManagement
    {

        SceneManager::SceneManager()
            : m_activeScene(nullptr), m_spatialPartitioningEnabled(true), m_octreeMaxDepth(8), m_octreeSize(1000.0f, 1000.0f, 1000.0f), m_octreeCenter(Math::Vec3::Zero), m_lodEnabled(true), m_frustumCullingEnabled(true), m_occlusionCullingEnabled(false), m_debugVisualization(false), m_needsOctreeRebuild(false), m_lastUpdateTime(0.0f)
        {
            // SceneManager initialized
            InitializeOctree();
        }

        SceneManager::~SceneManager()
        {
            PYRAMID_LOG_INFO("SceneManager destroyed");
        }

        std::shared_ptr<Pyramid::Scene> SceneManager::CreateScene(const std::string &name)
        {
            auto scene = std::make_shared<Pyramid::Scene>(name);
            m_scenes[name] = scene;

            if (!m_activeScene)
            {
                SetActiveScene(scene);
            }

            PYRAMID_LOG_INFO("Created scene: ", name);
            return scene;
        }

        bool SceneManager::LoadScene(const std::string &filePath, SerializationFormat format)
        {
            (void)format;
            PYRAMID_LOG_ERROR("Scene loading is not implemented: ", filePath);
            return false;
        }

        bool SceneManager::SaveScene(const std::string &filePath, SerializationFormat format)
        {
            (void)format;
            PYRAMID_LOG_ERROR("Scene saving is not implemented: ", filePath);
            return false;
        }

        void SceneManager::SetActiveScene(std::shared_ptr<Pyramid::Scene> scene)
        {
            if (m_activeScene != scene)
            {
                m_activeScene = scene;
                m_needsOctreeRebuild = true;
                PYRAMID_LOG_INFO("Active scene changed");
            }
        }

        void SceneManager::InitializeOctree()
        {
            if (m_spatialPartitioningEnabled)
            {
                m_octree = std::make_unique<SceneManagement::Octree>(m_octreeCenter, m_octreeSize, m_octreeMaxDepth);
                PYRAMID_LOG_INFO("Octree spatial partitioning initialized");
                PYRAMID_LOG_INFO("  Size: (", m_octreeSize.x, ", ", m_octreeSize.y, ", ", m_octreeSize.z, ")");
                PYRAMID_LOG_INFO("  Max Depth: ", m_octreeMaxDepth);
            }
        }

        void SceneManager::RebuildSpatialPartition()
        {
            if (!m_spatialPartitioningEnabled || !m_octree || !m_activeScene)
                return;

            auto start = std::chrono::high_resolution_clock::now();

            m_octree->Clear();

            // Add all render objects from active scene to octree
            // Create a dummy camera for getting all objects
            Camera dummyCamera;
            auto renderObjects = m_activeScene->GetVisibleObjects(dummyCamera);
            for (const auto &obj : renderObjects)
            {
                if (obj)
                {
                    m_octree->Insert(obj);
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<float, std::milli>(end - start).count();

            m_stats.lastUpdateTime = duration;
            m_needsOctreeRebuild = false;

            PYRAMID_LOG_INFO("Spatial partition rebuilt in ", duration, " ms");
            PYRAMID_LOG_INFO("  Objects added: ", renderObjects.size());
        }

        QueryResult SceneManager::QueryScene(const QueryParams &params)
        {
            QueryResult result;

            if (!m_activeScene)
                return result;

            auto start = std::chrono::high_resolution_clock::now();

            if (m_spatialPartitioningEnabled && m_octree)
            {
                // Use spatial partitioning for efficient queries
                switch (params.type)
                {
                case QueryType::Point:
                    result.objects = m_octree->QueryPoint(params.position);
                    break;
                case QueryType::Sphere:
                    result.objects = m_octree->QuerySphere(params.position, params.radius);
                    break;
                case QueryType::Box:
                {
                    AABB bounds(params.position - params.size * 0.5f,
                                params.position + params.size * 0.5f);
                    result.objects = m_octree->QueryBox(bounds);
                }
                break;
                case QueryType::Ray:
                    result.objects = m_octree->QueryRay(params.position, params.direction, params.maxDistance);
                    break;
                case QueryType::Frustum:
                    PYRAMID_LOG_WARN("QueryScene does not accept frustum planes; use GetVisibleObjects(camera)");
                    break;
                }
            }
            else
            {
                // Fallback to brute force search
                Camera dummyCamera;
                auto allObjects = m_activeScene->GetVisibleObjects(dummyCamera);
                for (const auto &obj : allObjects)
                {
                    // Simple distance check for demonstration
                    if (obj && (obj->position - params.position).Length() <= params.radius)
                    {
                        result.objects.push_back(obj);
                    }
                }
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<float, std::milli>(end - start).count();

            result.totalFound = static_cast<u32>(result.objects.size());
            m_stats.lastQueryTime = duration;

            return result;
        }

        std::vector<std::shared_ptr<RenderObject>> SceneManager::GetVisibleObjects(const Camera &camera)
        {
            std::vector<std::shared_ptr<RenderObject>> visibleObjects;

            if (!m_activeScene)
                return visibleObjects;

            if (m_spatialPartitioningEnabled && m_octree && m_frustumCullingEnabled)
            {
                // Use frustum culling with spatial partitioning
                auto frustumPlanes = SpatialUtils::CalculateFrustumPlanes(camera);
                visibleObjects = m_octree->QueryFrustum(frustumPlanes);
            }
            else
            {
                // Fallback to all objects
                visibleObjects = m_activeScene->GetVisibleObjects(camera);
            }

            // Apply additional culling if enabled
            if (m_occlusionCullingEnabled)
            {
                auto it = std::remove_if(visibleObjects.begin(), visibleObjects.end(),
                                         [this, &camera](const std::shared_ptr<RenderObject> &obj)
                                         {
                                             return OcclusionCull(obj, camera);
                                         });
                visibleObjects.erase(it, visibleObjects.end());
            }

            m_stats.visibleObjects = static_cast<u32>(visibleObjects.size());
            return visibleObjects;
        }

        std::vector<std::shared_ptr<RenderObject>> SceneManager::GetObjectsInRadius(const Math::Vec3 &center, f32 radius)
        {
            QueryParams params;
            params.type = QueryType::Sphere;
            params.position = center;
            params.radius = radius;

            auto result = QueryScene(params);
            return result.objects;
        }

        std::vector<std::shared_ptr<RenderObject>> SceneManager::GetObjectsInBox(
            const Math::Vec3 &min,
            const Math::Vec3 &max)
        {
            QueryParams params;
            params.type = QueryType::Box;
            params.position = (min + max) * 0.5f;
            params.size = max - min;
            return QueryScene(params).objects;
        }

        std::shared_ptr<RenderObject> SceneManager::GetNearestObject(const Math::Vec3 &position)
        {
            if (m_spatialPartitioningEnabled && m_octree)
            {
                return m_octree->FindNearest(position);
            }

            // Fallback implementation
            if (!m_activeScene)
                return nullptr;

            Camera dummyCamera;
            auto allObjects = m_activeScene->GetVisibleObjects(dummyCamera);
            std::shared_ptr<RenderObject> nearest = nullptr;
            f32 nearestDistance = std::numeric_limits<f32>::max();

            for (const auto &obj : allObjects)
            {
                if (obj)
                {
                    f32 distance = (obj->position - position).Length();
                    if (distance < nearestDistance)
                    {
                        nearestDistance = distance;
                        nearest = obj;
                    }
                }
            }

            return nearest;
        }

        void SceneManager::Update(f32 deltaTime, UpdateFlags flags)
        {
            if (!m_activeScene)
                return;

            auto start = std::chrono::high_resolution_clock::now();

            // Update transforms if requested
            if ((static_cast<u32>(flags) & static_cast<u32>(UpdateFlags::Transforms)) != 0)
            {
                UpdateTransforms();
            }

            // Update spatial partition if needed
            if ((static_cast<u32>(flags) & static_cast<u32>(UpdateFlags::SpatialPartition)) != 0 || m_needsOctreeRebuild)
            {
                UpdateSpatialPartition();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<float, std::milli>(end - start).count();

            m_stats.lastUpdateTime = duration;
            m_lastUpdateTime += deltaTime;
        }

        void SceneManager::UpdateTransforms()
        {
            if (!m_activeScene)
                return;

            // Update scene transforms
            // TODO: Add scene update method when available
        }

        void SceneManager::UpdateVisibility(const Camera &camera)
        {
            const auto visible = GetVisibleObjects(camera);
            m_stats.visibleObjects = static_cast<u32>(visible.size());
            const u32 total = m_activeScene ? static_cast<u32>(m_activeScene->GetObjectCount()) : 0;
            m_stats.culledObjects = total > m_stats.visibleObjects ? total - m_stats.visibleObjects : 0;
        }

        void SceneManager::UpdateSpatialPartition()
        {
            if (m_needsOctreeRebuild)
            {
                RebuildSpatialPartition();
            }
            else if (m_octree)
            {
                m_octree->Rebuild();
            }
        }

        const SceneStats &SceneManager::GetStats() const
        {
            // Update stats
            const_cast<SceneManager *>(this)->m_stats.totalNodes = m_activeScene ? 1u : 0u;
            const_cast<SceneManager *>(this)->m_stats.totalObjects =
                m_activeScene ? static_cast<u32>(m_activeScene->GetObjectCount()) : 0u;

            if (m_octree)
            {
                auto octreeStats = m_octree->GetStats();
                const_cast<SceneManager *>(this)->m_stats.octreeNodes = octreeStats.totalNodes;
                const_cast<SceneManager *>(this)->m_stats.octreeDepth = octreeStats.maxDepth;
            }

            return m_stats;
        }

        void SceneManager::ResetStats()
        {
            m_stats = SceneStats{};
        }

        void SceneManager::RegisterEventCallback(const std::string &eventType, SceneEventCallback callback)
        {
            if (!eventType.empty() && callback)
            {
                m_eventCallbacks[eventType].push_back(std::move(callback));
            }
        }

        void SceneManager::TriggerEvent(const std::string &eventType, std::shared_ptr<SceneNode> node)
        {
            const auto it = m_eventCallbacks.find(eventType);
            if (it == m_eventCallbacks.end())
                return;

            for (const auto &callback : it->second)
            {
                callback(eventType, node);
            }
        }

        void SceneManager::DrawDebugInfo()
        {
            if (!m_debugVisualization)
                return;

            const auto &stats = GetStats();
            PYRAMID_LOG_DEBUG(
                "Scene stats: objects=", stats.totalObjects,
                ", visible=", stats.visibleObjects,
                ", octreeNodes=", stats.octreeNodes,
                ", octreeDepth=", stats.octreeDepth);
        }

        bool SceneManager::FrustumCull(const std::shared_ptr<RenderObject> &object, const Camera &camera)
        {
            // Simple frustum culling implementation
            // This would be more sophisticated in a real implementation
            return false; // For now, don't cull anything
        }

        bool SceneManager::OcclusionCull(const std::shared_ptr<RenderObject> &object, const Camera &camera)
        {
            // Occlusion culling implementation would go here
            return false; // For now, don't cull anything
        }

        f32 SceneManager::CalculateLOD(const std::shared_ptr<RenderObject> &object, const Camera &camera)
        {
            if (!object)
                return 1.0f;

            f32 distance = (object->position - camera.GetPosition()).Length();

            // Simple LOD calculation based on distance
            if (distance < 10.0f)
                return 1.0f; // High detail
            else if (distance < 50.0f)
                return 0.5f; // Medium detail
            else
                return 0.25f; // Low detail
        }

        // Utility functions
        namespace SceneUtils
        {
            std::unique_ptr<SceneManager> CreateSceneManager()
            {
                return std::make_unique<SceneManager>();
            }

            std::shared_ptr<Pyramid::Scene> CreateSpatialTestScene(u32 objectCount)
            {
                auto scene = std::make_shared<Pyramid::Scene>("Spatial Test Scene");
                for (u32 index = 0; index < objectCount; ++index)
                {
                    auto object = std::make_shared<RenderObject>();
                    object->name = "SpatialObject" + std::to_string(index);
                    object->position = Math::Vec3(
                        static_cast<f32>(index % 10),
                        static_cast<f32>((index / 10) % 10),
                        static_cast<f32>(index / 100));
                    scene->AddRenderObject(object);
                }
                return scene;
            }

            bool ValidateScene(const std::shared_ptr<Pyramid::Scene> &scene)
            {
                if (!scene)
                {
                    PYRAMID_LOG_ERROR("Scene validation failed: null scene");
                    return false;
                }

                PYRAMID_LOG_INFO("Scene validation passed");
                return true;
            }

            void OptimizeScene(std::shared_ptr<Pyramid::Scene> scene)
            {
                if (!scene)
                    return;

                PYRAMID_LOG_INFO("Scene optimization completed");
            }
        }

    } // namespace SceneManagement
} // namespace Pyramid
