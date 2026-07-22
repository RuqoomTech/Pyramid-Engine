#pragma once

#include <Pyramid/Core/Prerequisites.hpp>
#include <Pyramid/Math/Math.hpp>
#include <Pyramid/Graphics/Scene.hpp>
#include <memory>
#include <vector>
#include <array>
#include <unordered_map>

namespace Pyramid
{
    namespace SceneManagement
    {

        /**
         * @brief Axis-aligned bounding box for spatial queries
         */
        struct AABB
        {
            Math::Vec3 min = Math::Vec3::Zero;
            Math::Vec3 max = Math::Vec3::Zero;

            AABB() = default;
            AABB(const Math::Vec3 &minPoint, const Math::Vec3 &maxPoint) : min(minPoint), max(maxPoint) {}

            // Utility functions
            Math::Vec3 GetCenter() const { return (min + max) * 0.5f; }
            Math::Vec3 GetSize() const { return max - min; }
            f32 GetVolume() const
            {
                Math::Vec3 size = GetSize();
                return size.x * size.y * size.z;
            }

            bool Contains(const Math::Vec3 &point) const
            {
                return point.x >= min.x && point.x <= max.x &&
                       point.y >= min.y && point.y <= max.y &&
                       point.z >= min.z && point.z <= max.z;
            }

            bool Contains(const AABB &other) const
            {
                return Contains(other.min) && Contains(other.max);
            }

            bool Intersects(const AABB &other) const
            {
                return min.x <= other.max.x && max.x >= other.min.x &&
                       min.y <= other.max.y && max.y >= other.min.y &&
                       min.z <= other.max.z && max.z >= other.min.z;
            }

            void Expand(const Math::Vec3 &point)
            {
                min.x = Math::Min(min.x, point.x);
                min.y = Math::Min(min.y, point.y);
                min.z = Math::Min(min.z, point.z);
                max.x = Math::Max(max.x, point.x);
                max.y = Math::Max(max.y, point.y);
                max.z = Math::Max(max.z, point.z);
            }

            // Additional methods implemented in source file
            bool IntersectsSphere(const Math::Vec3 &center, f32 radius) const;
            bool IntersectsRay(const Math::Vec3 &origin, const Math::Vec3 &direction, f32 &distance) const;
            void Expand(const AABB &other);
            AABB GetExpanded(f32 amount) const;
        };

        /**
         * @brief Octree node for spatial partitioning
         */
        class OctreeNode
        {
            friend class Octree;

        public:
            OctreeNode(const AABB &bounds, u32 depth, u32 maxDepth, u32 maxObjects);
            ~OctreeNode();

            // Object management
            void Insert(std::shared_ptr<RenderObject> object);
            void Remove(std::shared_ptr<RenderObject> object);
            void Clear();

            // Spatial queries
            void Query(const Math::Vec3 &point, std::vector<std::shared_ptr<RenderObject>> &results) const;
            void Query(const AABB &bounds, std::vector<std::shared_ptr<RenderObject>> &results) const;
            void Query(const Math::Vec3 &center, f32 radius, std::vector<std::shared_ptr<RenderObject>> &results) const;
            void QueryRay(const Math::Vec3 &origin, const Math::Vec3 &direction, f32 maxDistance,
                          std::vector<std::shared_ptr<RenderObject>> &results) const;

            // Frustum culling
            void QueryFrustum(const std::array<Math::Vec4, 6> &frustumPlanes,
                              std::vector<std::shared_ptr<RenderObject>> &results) const;

            // Maintenance
            void Update();
            void Rebuild();

            // Accessors
            const AABB &GetBounds() const { return m_bounds; }
            u32 GetDepth() const { return m_depth; }
            u32 GetObjectCount() const { return m_objects.size(); }
            u32 GetTotalObjectCount() const;
            bool IsLeaf() const { return m_children[0] == nullptr; }

            // Statistics
            u32 GetNodeCount() const;
            u32 GetMaxDepth() const;

        private:
            void Subdivide();
            u32 GetChildIndex(const Math::Vec3 &position) const;
            AABB GetChildBounds(u32 index) const;
            AABB CalculateObjectBounds(const std::shared_ptr<RenderObject> &object) const;

            // Node data
            AABB m_bounds;
            u32 m_depth;
            u32 m_maxDepth;
            u32 m_maxObjects;

            // Objects stored in this node
            std::vector<std::shared_ptr<RenderObject>> m_objects;

            // Child nodes (8 children for octree)
            std::array<std::unique_ptr<OctreeNode>, 8> m_children;

            // Flags
            bool m_needsRebuild = false;
        };

        /**
         * @brief Result of incrementally synchronizing scene objects with an octree
         */
        struct OctreeSyncStats
        {
            u32 insertedObjects = 0;
            u32 removedObjects = 0;
            u32 movedObjects = 0;
            u32 unchangedObjects = 0;

            bool HasChanges() const
            {
                return insertedObjects > 0 || removedObjects > 0 || movedObjects > 0;
            }
        };

        /**
         * @brief Main octree spatial partitioning structure
         */
        class Octree
        {
        public:
            Octree(const Math::Vec3 &center, const Math::Vec3 &size, u32 maxDepth = 8, u32 maxObjectsPerNode = 10);
            ~Octree();

            // Object management
            void Insert(std::shared_ptr<RenderObject> object);
            void Remove(std::shared_ptr<RenderObject> object);
            void Update(std::shared_ptr<RenderObject> object);
            bool UpdateIfMoved(std::shared_ptr<RenderObject> object);
            OctreeSyncStats Synchronize(const std::vector<std::shared_ptr<RenderObject>> &objects);
            void Clear();
            void Rebuild();

            // Spatial queries
            std::vector<std::shared_ptr<RenderObject>> QueryPoint(const Math::Vec3 &point) const;
            std::vector<std::shared_ptr<RenderObject>> QuerySphere(const Math::Vec3 &center, f32 radius) const;
            std::vector<std::shared_ptr<RenderObject>> QueryBox(const AABB &bounds) const;
            std::vector<std::shared_ptr<RenderObject>> QueryRay(const Math::Vec3 &origin,
                                                                const Math::Vec3 &direction,
                                                                f32 maxDistance = 1000.0f) const;

            // Frustum culling
            std::vector<std::shared_ptr<RenderObject>> QueryFrustum(const std::array<Math::Vec4, 6> &frustumPlanes) const;

            // Nearest neighbor queries
            std::shared_ptr<RenderObject> FindNearest(const Math::Vec3 &position) const;
            std::vector<std::shared_ptr<RenderObject>> FindKNearest(const Math::Vec3 &position, u32 k) const;

            // Configuration
            void SetMaxDepth(u32 maxDepth);
            void SetMaxObjectsPerNode(u32 maxObjects);
            void SetBounds(const Math::Vec3 &center, const Math::Vec3 &size);

            // Statistics and debugging
            struct OctreeStats
            {
                u32 totalNodes = 0;
                u32 leafNodes = 0;
                u32 totalObjects = 0;
                u32 maxDepth = 0;
                u32 averageObjectsPerLeaf = 0;
                f32 averageDepth = 0.0f;
                f32 memoryUsage = 0.0f; // In MB
            };

            OctreeStats GetStats() const;
            void DrawDebugVisualization() const;

            // Accessors
            const AABB &GetBounds() const;
            u32 GetMaxDepth() const { return m_maxDepth; }
            u32 GetMaxObjectsPerNode() const { return m_maxObjectsPerNode; }
            u32 GetTrackedObjectCount() const { return static_cast<u32>(m_objectBounds.size()); }
            bool Contains(const std::shared_ptr<RenderObject> &object) const;

        private:
            static bool BoundsEqual(const AABB &lhs, const AABB &rhs);

            void FindNearestRecursive(const Math::Vec3 &position,
                                      std::shared_ptr<RenderObject> &nearest,
                                      f32 &nearestDistance,
                                      const OctreeNode *node) const;

            // Root node
            std::unique_ptr<OctreeNode> m_root;

            // Configuration
            u32 m_maxDepth;
            u32 m_maxObjectsPerNode;

            // Object tracking for updates
            std::unordered_map<std::shared_ptr<RenderObject>, AABB> m_objectBounds;

        };

        /**
         * @brief Utility functions for spatial operations
         */
        namespace SpatialUtils
        {
            /**
             * @brief Calculate AABB for a render object
             */
            AABB CalculateAABB(const std::shared_ptr<RenderObject> &object);

            /**
             * @brief Calculate frustum planes from camera
             */
            std::array<Math::Vec4, 6> CalculateFrustumPlanes(const class Camera &camera);

            /**
             * @brief Test if AABB is inside frustum
             */
            bool AABBInFrustum(const AABB &aabb, const std::array<Math::Vec4, 6> &frustumPlanes);

            /**
             * @brief Calculate distance between point and AABB
             */
            f32 DistanceToAABB(const Math::Vec3 &point, const AABB &aabb);

            /**
             * @brief Ray-AABB intersection test
             */
            bool RayAABBIntersection(const Math::Vec3 &origin, const Math::Vec3 &direction,
                                     const AABB &aabb, f32 &distance);

            /**
             * @brief Sphere-AABB intersection test
             */
            bool SphereAABBIntersection(const Math::Vec3 &center, f32 radius, const AABB &aabb);
        }

    } // namespace Scene
} // namespace Pyramid
