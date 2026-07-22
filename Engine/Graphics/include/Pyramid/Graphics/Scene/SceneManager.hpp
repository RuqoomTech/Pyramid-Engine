#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

namespace Pyramid
{
    // Forward declarations
    class Camera;

    namespace SceneManagement
    {
        class Octree;
    }

    namespace SceneManagement
    {

        /**
         * @brief Scene query types for spatial partitioning
         */
        enum class QueryType
        {
            Point,   // Point-in-space queries
            Sphere,  // Sphere intersection queries
            Box,     // Axis-aligned bounding box queries
            Frustum, // Camera frustum queries
            Ray      // Ray intersection queries
        };

        /**
         * @brief Query parameters for spatial queries
         */
        struct QueryParams
        {
            QueryType type = QueryType::Point;
            Math::Vec3 position = Math::Vec3::Zero;
            Math::Vec3 direction = Math::Vec3::Forward;
            Math::Vec3 size = Math::Vec3::One;
            f32 radius = 1.0f;
            f32 maxDistance = 1000.0f;
        };

        /**
         * @brief Query results containing found objects
         */
        struct QueryResult
        {
            std::vector<std::shared_ptr<RenderObject>> objects;
            std::vector<std::shared_ptr<SceneNode>> nodes;
            std::vector<f32> distances;
            u32 totalChecked = 0;
            u32 totalFound = 0;
        };

        /**
         * @brief Scene statistics for performance monitoring
         */
        struct SceneStats
        {
            u32 totalNodes = 0;
            u32 totalObjects = 0;
            u32 visibleObjects = 0;
            u32 culledObjects = 0;
            u32 octreeNodes = 0;
            u32 octreeDepth = 0;
            u32 spatialObjectsInserted = 0;
            u32 spatialObjectsRemoved = 0;
            u32 spatialObjectsMoved = 0;
            u32 spatialObjectsUnchanged = 0;
            f32 lastQueryTime = 0.0f;
            f32 lastUpdateTime = 0.0f;
        };

        /**
         * @brief Scene update flags for optimization
         */
        enum class UpdateFlags : u32
        {
            None = 0,
            Transforms = 1 << 0,
            Visibility = 1 << 1,
            Lighting = 1 << 2,
            Materials = 1 << 3,
            SpatialPartition = 1 << 4,
            All = 0xFFFFFFFF
        };

        /**
         * @brief Scene serialization format
         */
        enum class SerializationFormat
        {
            Binary, // Fast binary format
            JSON,   // Human-readable JSON
            XML     // Structured XML format
        };

        /**
         * @brief Scene manager with spatial partitioning and explicit persistence capability checks
         */
        class SceneManager
        {
        public:
            SceneManager();
            ~SceneManager();

            // Scene management
            std::shared_ptr<Pyramid::Scene> CreateScene(const std::string &name);
            bool LoadScene(const std::string &filePath, SerializationFormat format = SerializationFormat::JSON);
            bool SaveScene(const std::string &filePath, SerializationFormat format = SerializationFormat::JSON);
            void SetActiveScene(std::shared_ptr<Pyramid::Scene> scene);
            std::shared_ptr<Pyramid::Scene> GetActiveScene() const { return m_activeScene; }

            // Spatial partitioning
            void EnableSpatialPartitioning(bool enable) { m_spatialPartitioningEnabled = enable; }
            bool IsSpatialPartitioningEnabled() const { return m_spatialPartitioningEnabled; }
            void SetOctreeDepth(u32 depth) { m_octreeMaxDepth = depth; }
            void SetOctreeSize(const Math::Vec3 &size) { m_octreeSize = size; }
            void RebuildSpatialPartition();
            // Incrementally inserts, removes, and relocates changed render objects.
            void UpdateSpatialPartition();

            // Scene queries
            QueryResult QueryScene(const QueryParams &params);
            std::vector<std::shared_ptr<RenderObject>> GetVisibleObjects(const Camera &camera);
            std::vector<std::shared_ptr<RenderObject>> GetObjectsInRadius(const Math::Vec3 &center, f32 radius);
            std::vector<std::shared_ptr<RenderObject>> GetObjectsInBox(const Math::Vec3 &min, const Math::Vec3 &max);
            std::shared_ptr<RenderObject> GetNearestObject(const Math::Vec3 &position);

            // Scene updates
            void Update(f32 deltaTime, UpdateFlags flags = UpdateFlags::All);
            void UpdateTransforms();
            void UpdateVisibility(const Camera &camera);

            // Performance monitoring
            const SceneStats &GetStats() const;
            void ResetStats();

            // Event system
            using SceneEventCallback = std::function<void(const std::string &, std::shared_ptr<SceneNode>)>;
            void RegisterEventCallback(const std::string &eventType, SceneEventCallback callback);
            void TriggerEvent(const std::string &eventType, std::shared_ptr<SceneNode> node);

            // Advanced features
            void SetLODEnabled(bool enabled) { m_lodEnabled = enabled; }
            void SetFrustumCullingEnabled(bool enabled) { m_frustumCullingEnabled = enabled; }
            void SetOcclusionCullingEnabled(bool enabled) { m_occlusionCullingEnabled = enabled; }

            // Debug visualization
            void SetDebugVisualization(bool enabled) { m_debugVisualization = enabled; }
            void DrawDebugInfo();

        private:
            // Internal scene management
            void InitializeOctree();

            // Culling algorithms
            bool FrustumCull(const std::shared_ptr<RenderObject> &object, const Camera &camera);
            bool OcclusionCull(const std::shared_ptr<RenderObject> &object, const Camera &camera);
            f32 CalculateLOD(const std::shared_ptr<RenderObject> &object, const Camera &camera);

            // Scene data
            std::shared_ptr<Pyramid::Scene> m_activeScene;
            std::unordered_map<std::string, std::shared_ptr<Pyramid::Scene>> m_scenes;

            // Spatial partitioning
            std::unique_ptr<SceneManagement::Octree> m_octree;
            bool m_spatialPartitioningEnabled = true;
            u32 m_octreeMaxDepth = 8;
            Math::Vec3 m_octreeSize = Math::Vec3(1000.0f, 1000.0f, 1000.0f);
            Math::Vec3 m_octreeCenter = Math::Vec3::Zero;

            // Performance settings
            bool m_lodEnabled = true;
            bool m_frustumCullingEnabled = true;
            bool m_occlusionCullingEnabled = false;
            bool m_debugVisualization = false;

            // Statistics
            SceneStats m_stats;

            // Event system
            std::unordered_map<std::string, std::vector<SceneEventCallback>> m_eventCallbacks;

            // Update tracking
            bool m_needsOctreeRebuild = false;
            f32 m_lastUpdateTime = 0.0f;
        };

        /**
         * @brief Scene manager factory and utilities
         */
        namespace SceneUtils
        {
            /**
             * @brief Create a scene manager with default settings
             */
            std::unique_ptr<SceneManager> CreateSceneManager();

            /**
             * @brief Create a test scene with spatial partitioning demonstration
             */
            std::shared_ptr<Pyramid::Scene> CreateSpatialTestScene(u32 objectCount = 1000);

            /**
             * @brief Validate scene integrity
             */
            bool ValidateScene(const std::shared_ptr<Pyramid::Scene> &scene);

            /**
             * @brief Optimize scene for rendering performance
             */
            void OptimizeScene(std::shared_ptr<Pyramid::Scene> scene);
        }

    } // namespace SceneManagement
} // namespace Pyramid
