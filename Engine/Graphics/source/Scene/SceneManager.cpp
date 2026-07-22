#include <Pyramid/Graphics/Scene/SceneManager.hpp>
#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Util/Log.hpp>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <utility>
#include <vector>

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

            // Spatial storage must not depend on the current camera. Hidden objects
            // remain indexed; rendering visibility queries filter them separately.
            const auto &renderObjects = m_activeScene->GetRenderObjects();
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
            {
                return result;
            }

            const auto start = std::chrono::high_resolution_clock::now();

            if (m_spatialPartitioningEnabled && m_octree && m_needsOctreeRebuild)
            {
                RebuildSpatialPartition();
            }

            if (m_spatialPartitioningEnabled && m_octree)
            {
                switch (params.type)
                {
                case QueryType::Point:
                    result.objects = m_octree->QueryPoint(params.position);
                    break;
                case QueryType::Sphere:
                    result.objects = m_octree->QuerySphere(params.position, params.radius);
                    break;
                case QueryType::Box:
                    result.objects = m_octree->QueryBox(AABB(
                        params.position - params.size * 0.5f,
                        params.position + params.size * 0.5f));
                    break;
                case QueryType::Ray:
                    result.objects = m_octree->QueryRay(
                        params.position,
                        params.direction,
                        params.maxDistance);
                    break;
                case QueryType::Frustum:
                    PYRAMID_LOG_WARN("QueryScene does not accept frustum planes; use GetVisibleObjects(camera)");
                    break;
                }
            }
            else
            {
                const auto &allObjects = m_activeScene->GetRenderObjects();
                result.totalChecked = static_cast<u32>(allObjects.size());

                const AABB queryBox(
                    params.position - params.size * 0.5f,
                    params.position + params.size * 0.5f);
                const f32 directionLengthSquared = params.direction.LengthSquared();
                const bool validRay = params.maxDistance >= 0.0f && directionLengthSquared > 1e-12f;
                const Math::Vec3 rayDirection = validRay ? params.direction.Normalized() : Math::Vec3::Zero;
                std::vector<std::pair<f32, std::shared_ptr<RenderObject>>> rayHits;

                for (const auto &object : allObjects)
                {
                    if (!object)
                    {
                        continue;
                    }

                    const AABB objectBounds = SpatialUtils::CalculateAABB(object);
                    switch (params.type)
                    {
                    case QueryType::Point:
                        if (objectBounds.Contains(params.position))
                        {
                            result.objects.push_back(object);
                        }
                        break;
                    case QueryType::Sphere:
                        if (objectBounds.IntersectsSphere(params.position, params.radius))
                        {
                            result.objects.push_back(object);
                        }
                        break;
                    case QueryType::Box:
                        if (objectBounds.Intersects(queryBox))
                        {
                            result.objects.push_back(object);
                        }
                        break;
                    case QueryType::Ray:
                    {
                        f32 distance = 0.0f;
                        if (validRay &&
                            objectBounds.IntersectsRay(params.position, rayDirection, distance) &&
                            distance <= params.maxDistance)
                        {
                            rayHits.emplace_back(distance, object);
                        }
                        break;
                    }
                    case QueryType::Frustum:
                        break;
                    }
                }

                if (params.type == QueryType::Ray)
                {
                    std::sort(rayHits.begin(), rayHits.end(), [](const auto &lhs, const auto &rhs)
                    {
                        return lhs.first < rhs.first;
                    });
                    result.objects.reserve(rayHits.size());
                    result.distances.reserve(rayHits.size());
                    for (const auto &hit : rayHits)
                    {
                        result.distances.push_back(hit.first);
                        result.objects.push_back(hit.second);
                    }
                }
                else if (params.type == QueryType::Frustum)
                {
                    PYRAMID_LOG_WARN("QueryScene does not accept frustum planes; use GetVisibleObjects(camera)");
                }
            }

            if (params.type == QueryType::Ray && result.distances.empty() && !result.objects.empty())
            {
                const f32 directionLengthSquared = params.direction.LengthSquared();
                if (params.maxDistance >= 0.0f && directionLengthSquared > 1e-12f)
                {
                    const Math::Vec3 rayDirection = params.direction.Normalized();
                    result.distances.reserve(result.objects.size());
                    for (const auto &object : result.objects)
                    {
                        f32 distance = 0.0f;
                        SpatialUtils::CalculateAABB(object).IntersectsRay(
                            params.position,
                            rayDirection,
                            distance);
                        result.distances.push_back(distance);
                    }
                }
            }

            const auto end = std::chrono::high_resolution_clock::now();
            result.totalFound = static_cast<u32>(result.objects.size());
            m_stats.lastQueryTime =
                std::chrono::duration<float, std::milli>(end - start).count();

            return result;
        }

        std::vector<std::shared_ptr<RenderObject>> SceneManager::GetVisibleObjects(const Camera &camera)
        {
            std::vector<std::shared_ptr<RenderObject>> visibleObjects;

            if (!m_activeScene)
                return visibleObjects;

            if (m_spatialPartitioningEnabled && m_octree && m_needsOctreeRebuild)
            {
                RebuildSpatialPartition();
            }

            if (m_frustumCullingEnabled)
            {
                if (m_spatialPartitioningEnabled && m_octree)
                {
                    visibleObjects = m_octree->QueryFrustum(camera.GetFrustumPlanes());
                }
                else
                {
                    visibleObjects = m_activeScene->GetVisibleObjects(camera);
                }
            }
            else
            {
                for (const auto &object : m_activeScene->GetRenderObjects())
                {
                    if (object && object->visible)
                    {
                        visibleObjects.push_back(object);
                    }
                }
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
            const auto nearestObjects = GetKNearestObjects(position, 1);
            return nearestObjects.empty() ? nullptr : nearestObjects.front();
        }

        std::vector<std::shared_ptr<RenderObject>> SceneManager::GetKNearestObjects(
            const Math::Vec3 &position,
            u32 count)
        {
            if (!m_activeScene || count == 0)
            {
                return {};
            }

            if (m_spatialPartitioningEnabled && m_octree)
            {
                if (m_needsOctreeRebuild)
                {
                    RebuildSpatialPartition();
                }
                return m_octree->FindKNearest(position, count);
            }

            std::vector<std::pair<f32, std::shared_ptr<RenderObject>>> candidates;
            candidates.reserve(m_activeScene->GetRenderObjects().size());
            for (const auto &object : m_activeScene->GetRenderObjects())
            {
                if (object)
                {
                    candidates.emplace_back(
                        SpatialUtils::CalculateAABB(object).DistanceSquaredToPoint(position),
                        object);
                }
            }

            std::sort(candidates.begin(), candidates.end(),
                      [](const auto &lhs, const auto &rhs)
                      {
                          return lhs.first < rhs.first;
                      });
            if (candidates.size() > count)
            {
                candidates.resize(count);
            }

            std::vector<std::shared_ptr<RenderObject>> results;
            results.reserve(candidates.size());
            for (const auto &candidate : candidates)
            {
                results.push_back(candidate.second);
            }
            return results;
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
            m_stats.spatialObjectsInserted = 0;
            m_stats.spatialObjectsRemoved = 0;
            m_stats.spatialObjectsMoved = 0;
            m_stats.spatialObjectsUnchanged = 0;

            if (!m_spatialPartitioningEnabled || !m_octree || !m_activeScene)
            {
                return;
            }

            if (m_needsOctreeRebuild)
            {
                RebuildSpatialPartition();
                m_stats.spatialObjectsInserted = static_cast<u32>(m_activeScene->GetObjectCount());
                return;
            }

            const auto syncStats = m_octree->Synchronize(m_activeScene->GetRenderObjects());
            m_stats.spatialObjectsInserted = syncStats.insertedObjects;
            m_stats.spatialObjectsRemoved = syncStats.removedObjects;
            m_stats.spatialObjectsMoved = syncStats.movedObjects;
            m_stats.spatialObjectsUnchanged = syncStats.unchangedObjects;

            if (syncStats.HasChanges())
            {
                PYRAMID_LOG_DEBUG(
                    "Spatial partition synchronized: inserted=", syncStats.insertedObjects,
                    ", removed=", syncStats.removedObjects,
                    ", moved=", syncStats.movedObjects);
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
                ", octreeDepth=", stats.octreeDepth,
                ", spatialMoved=", stats.spatialObjectsMoved,
                ", spatialInserted=", stats.spatialObjectsInserted,
                ", spatialRemoved=", stats.spatialObjectsRemoved);
        }

        bool SceneManager::FrustumCull(const std::shared_ptr<RenderObject> &object, const Camera &camera)
        {
            if (!object || !object->visible)
            {
                return true;
            }

            Math::Vec3 boundsMin;
            Math::Vec3 boundsMax;
            object->GetWorldBounds(boundsMin, boundsMax);
            return !camera.IsAABBVisible(boundsMin, boundsMax);
        }

        bool SceneManager::OcclusionCull(const std::shared_ptr<RenderObject> &object, const Camera &camera)
        {
            (void)object;
            (void)camera;
            // Occlusion culling is not implemented yet.
            return false;
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
