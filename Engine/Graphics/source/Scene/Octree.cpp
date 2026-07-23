#include <Pyramid/Graphics/Scene/Octree.hpp>
#include <Pyramid/Graphics/Camera.hpp>
#include <Pyramid/Util/Log.hpp>
#include <algorithm>
#include <limits>
#include <unordered_set>
#include <queue>
#include <cmath>
#include <exception>

namespace Pyramid
{
    namespace SceneManagement
    {
        namespace
        {
            bool IsFiniteVector(const Math::Vec3 &value)
            {
                return std::isfinite(value.x) &&
                       std::isfinite(value.y) &&
                       std::isfinite(value.z);
            }

            AABB ConfigurationBounds(const OctreeConfiguration &configuration)
            {
                const Math::Vec3 halfSize = configuration.size * 0.5f;
                return AABB(configuration.center - halfSize, configuration.center + halfSize);
            }

            Math::Vec3 SanitizeConstructionSize(const Math::Vec3 &size)
            {
                const auto sanitizeAxis = [](f32 value)
                {
                    if (!std::isfinite(value) || Math::Abs(value) <= 0.0001f)
                    {
                        return 1.0f;
                    }
                    return Math::Abs(value);
                };

                return Math::Vec3(
                    sanitizeAxis(size.x),
                    sanitizeAxis(size.y),
                    sanitizeAxis(size.z));
            }
        }

        // AABB Implementation
        f32 AABB::DistanceSquaredToPoint(const Math::Vec3 &point) const
        {
            f32 distanceSquared = 0.0f;
            for (u32 axis = 0; axis < 3; ++axis)
            {
                if (point[axis] < min[axis])
                {
                    const f32 delta = min[axis] - point[axis];
                    distanceSquared += delta * delta;
                }
                else if (point[axis] > max[axis])
                {
                    const f32 delta = point[axis] - max[axis];
                    distanceSquared += delta * delta;
                }
            }
            return distanceSquared;
        }

        f32 AABB::DistanceToPoint(const Math::Vec3 &point) const
        {
            return std::sqrt(DistanceSquaredToPoint(point));
        }

        bool AABB::IntersectsSphere(const Math::Vec3 &center, f32 radius) const
        {
            if (radius < 0.0f)
            {
                return false;
            }

            f32 sqDist = 0.0f;

            // Calculate squared distance from sphere center to AABB
            for (int i = 0; i < 3; ++i)
            {
                f32 v = center[i];
                f32 minVal = min[i];
                f32 maxVal = max[i];

                if (v < minVal)
                    sqDist += (minVal - v) * (minVal - v);
                if (v > maxVal)
                    sqDist += (v - maxVal) * (v - maxVal);
            }

            return sqDist <= radius * radius;
        }

        bool AABB::IntersectsRay(const Math::Vec3 &origin, const Math::Vec3 &direction, f32 &distance) const
        {
            if (direction.LengthSquared() <= 1e-12f)
            {
                return false;
            }

            f32 tmin = 0.0f;
            f32 tmax = std::numeric_limits<f32>::max();

            for (int i = 0; i < 3; ++i)
            {
                if (Math::Abs(direction[i]) < 1e-6f)
                {
                    // Ray is parallel to slab
                    if (origin[i] < min[i] || origin[i] > max[i])
                        return false;
                }
                else
                {
                    f32 ood = 1.0f / direction[i];
                    f32 t1 = (min[i] - origin[i]) * ood;
                    f32 t2 = (max[i] - origin[i]) * ood;

                    if (t1 > t2)
                        std::swap(t1, t2);

                    tmin = Math::Max(tmin, t1);
                    tmax = Math::Min(tmax, t2);

                    if (tmin > tmax)
                        return false;
                }
            }

            distance = tmin;
            return true;
        }

        void AABB::Expand(const AABB &other)
        {
            min.x = Math::Min(min.x, other.min.x);
            min.y = Math::Min(min.y, other.min.y);
            min.z = Math::Min(min.z, other.min.z);
            max.x = Math::Max(max.x, other.max.x);
            max.y = Math::Max(max.y, other.max.y);
            max.z = Math::Max(max.z, other.max.z);
        }

        AABB AABB::GetExpanded(f32 amount) const
        {
            Math::Vec3 expansion(amount, amount, amount);
            return AABB(min - expansion, max + expansion);
        }

        // OctreeNode Implementation
        OctreeNode::OctreeNode(const AABB &bounds, u32 depth, u32 maxDepth, u32 maxObjects)
            : m_bounds(bounds), m_depth(depth), m_maxDepth(maxDepth), m_maxObjects(maxObjects), m_needsRebuild(false)
        {
            // Initialize children to nullptr
            for (auto &child : m_children)
            {
                child = nullptr;
            }
        }

        OctreeNode::~OctreeNode() = default;

        void OctreeNode::Insert(std::shared_ptr<RenderObject> object)
        {
            if (!object)
            {
                return;
            }

            if (m_depth >= m_maxDepth || (IsLeaf() && m_objects.size() < m_maxObjects))
            {
                m_objects.push_back(object);
                return;
            }

            if (IsLeaf())
            {
                Subdivide();

                auto existingObjects = std::move(m_objects);
                m_objects.clear();
                for (const auto &existingObject : existingObjects)
                {
                    Insert(existingObject);
                }
            }

            const AABB objectBounds = CalculateObjectBounds(object);
            for (const auto &child : m_children)
            {
                if (child && child->m_bounds.Contains(objectBounds))
                {
                    child->Insert(object);
                    return;
                }
            }

            // Objects spanning child boundaries stay in this node so child-node
            // rejection can never hide geometry that extends into the frustum.
            m_objects.push_back(object);
        }

        void OctreeNode::Remove(std::shared_ptr<RenderObject> object)
        {
            if (!object)
            {
                return;
            }

            m_objects.erase(
                std::remove(m_objects.begin(), m_objects.end(), object),
                m_objects.end());

            for (auto &child : m_children)
            {
                if (child)
                {
                    child->Remove(object);
                }
            }
        }

        void OctreeNode::Clear()
        {
            m_objects.clear();
            for (auto &child : m_children)
            {
                child.reset();
            }
        }

        void OctreeNode::Query(const Math::Vec3 &point, std::vector<std::shared_ptr<RenderObject>> &results) const
        {
            // The root may retain objects outside its configured bounds. Child
            // nodes are safe to prune because insertion descends only when an
            // object's complete AABB fits inside the child.
            if (m_depth > 0 && !m_bounds.Contains(point))
            {
                return;
            }

            for (const auto &object : m_objects)
            {
                if (object && CalculateObjectBounds(object).Contains(point))
                {
                    results.push_back(object);
                }
            }

            for (const auto &child : m_children)
            {
                if (child)
                {
                    child->Query(point, results);
                }
            }
        }

        void OctreeNode::Query(const AABB &bounds, std::vector<std::shared_ptr<RenderObject>> &results) const
        {
            if (m_depth > 0 && !m_bounds.Intersects(bounds))
            {
                return;
            }

            for (const auto &object : m_objects)
            {
                if (object && CalculateObjectBounds(object).Intersects(bounds))
                {
                    results.push_back(object);
                }
            }

            for (const auto &child : m_children)
            {
                if (child)
                {
                    child->Query(bounds, results);
                }
            }
        }

        void OctreeNode::Query(const Math::Vec3 &center, f32 radius, std::vector<std::shared_ptr<RenderObject>> &results) const
        {
            if (radius < 0.0f || (m_depth > 0 && !m_bounds.IntersectsSphere(center, radius)))
            {
                return;
            }

            for (const auto &object : m_objects)
            {
                if (object && CalculateObjectBounds(object).IntersectsSphere(center, radius))
                {
                    results.push_back(object);
                }
            }

            for (const auto &child : m_children)
            {
                if (child)
                {
                    child->Query(center, radius, results);
                }
            }
        }

        void OctreeNode::QueryRay(
            const Math::Vec3 &origin,
            const Math::Vec3 &direction,
            f32 maxDistance,
            std::vector<std::shared_ptr<RenderObject>> &results) const
        {
            f32 nodeDistance = 0.0f;
            if (m_depth > 0 &&
                (!m_bounds.IntersectsRay(origin, direction, nodeDistance) || nodeDistance > maxDistance))
            {
                return;
            }

            for (const auto &object : m_objects)
            {
                if (!object)
                    continue;

                f32 objectDistance = 0.0f;
                if (CalculateObjectBounds(object).IntersectsRay(origin, direction, objectDistance) &&
                    objectDistance <= maxDistance)
                {
                    results.push_back(object);
                }
            }

            for (const auto &child : m_children)
            {
                if (child)
                    child->QueryRay(origin, direction, maxDistance, results);
            }
        }

        void OctreeNode::Update()
        {
            if (m_needsRebuild)
                Rebuild();

            for (auto &child : m_children)
            {
                if (child)
                    child->Update();
            }
        }

        void OctreeNode::Rebuild()
        {
            std::vector<std::shared_ptr<RenderObject>> objects;
            objects.reserve(GetTotalObjectCount());

            const auto collect = [&](const auto &self, const OctreeNode *node) -> void
            {
                objects.insert(objects.end(), node->m_objects.begin(), node->m_objects.end());
                for (const auto &child : node->m_children)
                {
                    if (child)
                        self(self, child.get());
                }
            };

            collect(collect, this);
            Clear();
            for (const auto &object : objects)
                Insert(object);
            m_needsRebuild = false;
        }

        OctreeCompactionStats OctreeNode::Compact()
        {
            OctreeCompactionStats stats;
            if (IsLeaf())
            {
                return stats;
            }

            u32 descendantObjects = 0;
            for (auto &child : m_children)
            {
                if (!child)
                {
                    continue;
                }

                const auto childStats = child->Compact();
                stats.collapsedNodes += childStats.collapsedNodes;
                stats.promotedObjects += childStats.promotedObjects;
                descendantObjects += child->GetTotalObjectCount();
            }

            const u32 totalObjects = static_cast<u32>(m_objects.size()) + descendantObjects;
            if (descendantObjects == 0 || totalObjects <= m_maxObjects)
            {
                for (auto &child : m_children)
                {
                    if (!child)
                    {
                        continue;
                    }

                    stats.collapsedNodes += child->GetNodeCount();
                    child->CollectObjects(m_objects);
                    child.reset();
                }

                stats.promotedObjects += descendantObjects;
            }

            return stats;
        }

        u32 OctreeNode::GetMaxDepth() const
        {
            u32 depth = m_depth;
            for (const auto &child : m_children)
            {
                if (child)
                    depth = Math::Max(depth, child->GetMaxDepth());
            }
            return depth;
        }

        AABB OctreeNode::CalculateObjectBounds(const std::shared_ptr<RenderObject> &object) const
        {
            if (!object)
            {
                return AABB();
            }

            Math::Vec3 boundsMin;
            Math::Vec3 boundsMax;
            object->GetWorldBounds(boundsMin, boundsMax);
            return AABB(boundsMin, boundsMax);
        }

        void OctreeNode::CollectObjects(std::vector<std::shared_ptr<RenderObject>> &objects) const
        {
            objects.insert(objects.end(), m_objects.begin(), m_objects.end());
            for (const auto &child : m_children)
            {
                if (child)
                {
                    child->CollectObjects(objects);
                }
            }
        }

        u32 OctreeNode::GetTotalObjectCount() const
        {
            u32 count = static_cast<u32>(m_objects.size());

            if (!IsLeaf())
            {
                for (const auto &child : m_children)
                {
                    if (child)
                    {
                        count += child->GetTotalObjectCount();
                    }
                }
            }

            return count;
        }

        u32 OctreeNode::GetNodeCount() const
        {
            u32 count = 1; // This node

            if (!IsLeaf())
            {
                for (const auto &child : m_children)
                {
                    if (child)
                    {
                        count += child->GetNodeCount();
                    }
                }
            }

            return count;
        }

        void OctreeNode::QueryFrustum(const std::array<Math::Vec4, 6> &frustumPlanes,
                                      std::vector<std::shared_ptr<RenderObject>> &results) const
        {
            // The root may retain objects that lie outside its configured bounds.
            // Child nodes are safe to reject because insertion only descends when
            // the complete object AABB fits inside that child.
            if (m_depth > 0 && !SpatialUtils::AABBInFrustum(m_bounds, frustumPlanes))
            {
                return;
            }

            for (const auto &object : m_objects)
            {
                if (!object || !object->visible)
                {
                    continue;
                }

                if (SpatialUtils::AABBInFrustum(CalculateObjectBounds(object), frustumPlanes))
                {
                    results.push_back(object);
                }
            }

            for (const auto &child : m_children)
            {
                if (child)
                {
                    child->QueryFrustum(frustumPlanes, results);
                }
            }
        }

        void OctreeNode::Subdivide()
        {
            if (m_depth >= m_maxDepth)
                return;

            // Create 8 child nodes
            for (u32 i = 0; i < 8; ++i)
            {
                AABB childBounds = GetChildBounds(i);
                m_children[i] = std::make_unique<OctreeNode>(childBounds, m_depth + 1, m_maxDepth, m_maxObjects);
            }
        }

        u32 OctreeNode::GetChildIndex(const Math::Vec3 &position) const
        {
            Math::Vec3 center = m_bounds.GetCenter();
            u32 index = 0;

            if (position.x >= center.x)
                index |= 1;
            if (position.y >= center.y)
                index |= 2;
            if (position.z >= center.z)
                index |= 4;

            return index;
        }

        AABB OctreeNode::GetChildBounds(u32 index) const
        {
            Math::Vec3 center = m_bounds.GetCenter();

            Math::Vec3 childMin = center;
            Math::Vec3 childMax = center;

            if (index & 1)
            {
                childMin.x = center.x;
                childMax.x = m_bounds.max.x;
            }
            else
            {
                childMin.x = m_bounds.min.x;
                childMax.x = center.x;
            }

            if (index & 2)
            {
                childMin.y = center.y;
                childMax.y = m_bounds.max.y;
            }
            else
            {
                childMin.y = m_bounds.min.y;
                childMax.y = center.y;
            }

            if (index & 4)
            {
                childMin.z = center.z;
                childMax.z = m_bounds.max.z;
            }
            else
            {
                childMin.z = m_bounds.min.z;
                childMax.z = center.z;
            }

            return AABB(childMin, childMax);
        }

        // Octree Implementation
        Octree::Octree(const Math::Vec3 &center, const Math::Vec3 &size, u32 maxDepth, u32 maxObjectsPerNode)
            : m_maxDepth(maxDepth), m_maxObjectsPerNode(Math::Max(1u, maxObjectsPerNode))
        {
            OctreeConfiguration configuration;
            configuration.center = IsFiniteVector(center) ? center : Math::Vec3::Zero;
            configuration.size = SanitizeConstructionSize(size);
            configuration.maxDepth = maxDepth;
            configuration.maxObjectsPerNode = m_maxObjectsPerNode;

            if (!IsFiniteVector(center) ||
                configuration.size.x != size.x ||
                configuration.size.y != size.y ||
                configuration.size.z != size.z ||
                maxObjectsPerNode == 0)
            {
                PYRAMID_LOG_WARN(
                    "Invalid octree construction values were normalized to center=",
                    configuration.center.x, ",", configuration.center.y, ",", configuration.center.z,
                    " size=", configuration.size.x, ",", configuration.size.y, ",", configuration.size.z,
                    " maxObjectsPerNode=", configuration.maxObjectsPerNode);
            }

            m_root = std::make_unique<OctreeNode>(
                ConfigurationBounds(configuration),
                0,
                configuration.maxDepth,
                configuration.maxObjectsPerNode);

            PYRAMID_LOG_INFO(
                "Octree created with max depth: ",
                configuration.maxDepth,
                ", max objects per node: ",
                configuration.maxObjectsPerNode);
        }

        Octree::~Octree() = default;

        void Octree::Insert(std::shared_ptr<RenderObject> object)
        {
            if (!m_root || !object)
            {
                return;
            }

            if (m_objectBounds.find(object) != m_objectBounds.end())
            {
                UpdateIfMoved(object);
                return;
            }

            m_root->Insert(object);
            m_objectBounds.emplace(object, SpatialUtils::CalculateAABB(object));
        }

        void Octree::Remove(std::shared_ptr<RenderObject> object)
        {
            if (!object || !m_root || m_objectBounds.erase(object) == 0)
            {
                m_lastCompactionStats = {};
                return;
            }

            m_root->Remove(object);
            Compact();
        }

        void Octree::Update(std::shared_ptr<RenderObject> object)
        {
            if (!object || !m_root)
            {
                m_lastCompactionStats = {};
                return;
            }

            const bool wasTracked = m_objectBounds.find(object) != m_objectBounds.end();
            if (wasTracked)
            {
                m_root->Remove(object);
            }

            m_root->Insert(object);
            m_objectBounds[object] = SpatialUtils::CalculateAABB(object);
            if (wasTracked)
            {
                Compact();
            }
            else
            {
                m_lastCompactionStats = {};
            }
        }

        bool Octree::UpdateIfMoved(std::shared_ptr<RenderObject> object)
        {
            if (!object || !m_root)
            {
                m_lastCompactionStats = {};
                return false;
            }

            const AABB currentBounds = SpatialUtils::CalculateAABB(object);
            const auto tracked = m_objectBounds.find(object);
            if (tracked == m_objectBounds.end())
            {
                m_root->Insert(object);
                m_objectBounds.emplace(object, currentBounds);
                m_lastCompactionStats = {};
                return true;
            }

            if (BoundsEqual(tracked->second, currentBounds))
            {
                m_lastCompactionStats = {};
                return false;
            }

            m_root->Remove(object);
            m_root->Insert(object);
            tracked->second = currentBounds;
            Compact();
            return true;
        }

        OctreeSyncStats Octree::Synchronize(const std::vector<std::shared_ptr<RenderObject>> &objects)
        {
            OctreeSyncStats stats;
            if (!m_root)
            {
                m_lastCompactionStats = {};
                return stats;
            }

            std::unordered_set<std::shared_ptr<RenderObject>> liveObjects;
            liveObjects.reserve(objects.size());

            for (const auto &object : objects)
            {
                if (!object || !liveObjects.insert(object).second)
                {
                    continue;
                }

                const AABB currentBounds = SpatialUtils::CalculateAABB(object);
                const auto tracked = m_objectBounds.find(object);
                if (tracked == m_objectBounds.end())
                {
                    m_root->Insert(object);
                    m_objectBounds.emplace(object, currentBounds);
                    ++stats.insertedObjects;
                }
                else if (!BoundsEqual(tracked->second, currentBounds))
                {
                    m_root->Remove(object);
                    m_root->Insert(object);
                    tracked->second = currentBounds;
                    ++stats.movedObjects;
                }
                else
                {
                    ++stats.unchangedObjects;
                }
            }

            std::vector<std::shared_ptr<RenderObject>> removedObjects;
            removedObjects.reserve(m_objectBounds.size());
            for (const auto &entry : m_objectBounds)
            {
                if (liveObjects.find(entry.first) == liveObjects.end())
                {
                    removedObjects.push_back(entry.first);
                }
            }

            for (const auto &object : removedObjects)
            {
                m_root->Remove(object);
                m_objectBounds.erase(object);
                ++stats.removedObjects;
            }

            if (stats.movedObjects > 0 || stats.removedObjects > 0)
            {
                stats.compaction = Compact();
            }
            else
            {
                m_lastCompactionStats = {};
            }

            return stats;
        }

        void Octree::Clear()
        {
            if (m_root)
            {
                m_root->Clear();
            }
            m_objectBounds.clear();
            m_lastCompactionStats = {};
        }

        std::vector<std::shared_ptr<RenderObject>> Octree::QueryPoint(const Math::Vec3 &point) const
        {
            std::vector<std::shared_ptr<RenderObject>> results;
            if (m_root)
            {
                m_root->Query(point, results);
            }

            std::unordered_set<std::shared_ptr<RenderObject>> seen;
            results.erase(
                std::remove_if(results.begin(), results.end(), [&seen](const auto &object)
                {
                    return !object || !seen.insert(object).second;
                }),
                results.end());
            return results;
        }

        std::vector<std::shared_ptr<RenderObject>> Octree::QuerySphere(const Math::Vec3 &center, f32 radius) const
        {
            std::vector<std::shared_ptr<RenderObject>> results;
            if (m_root && radius >= 0.0f)
            {
                m_root->Query(center, radius, results);
            }

            std::unordered_set<std::shared_ptr<RenderObject>> seen;
            results.erase(
                std::remove_if(results.begin(), results.end(), [&seen](const auto &object)
                {
                    return !object || !seen.insert(object).second;
                }),
                results.end());
            return results;
        }

        std::vector<std::shared_ptr<RenderObject>> Octree::QueryBox(const AABB &bounds) const
        {
            std::vector<std::shared_ptr<RenderObject>> results;
            if (m_root)
            {
                m_root->Query(bounds, results);
            }

            std::unordered_set<std::shared_ptr<RenderObject>> seen;
            results.erase(
                std::remove_if(results.begin(), results.end(), [&seen](const auto &object)
                {
                    return !object || !seen.insert(object).second;
                }),
                results.end());
            return results;
        }

        std::vector<std::shared_ptr<RenderObject>> Octree::QueryRay(const Math::Vec3 &origin,
                                                                    const Math::Vec3 &direction,
                                                                    f32 maxDistance) const
        {
            std::vector<std::shared_ptr<RenderObject>> results;
            const f32 directionLengthSquared = direction.LengthSquared();
            if (!m_root || maxDistance < 0.0f || directionLengthSquared <= 1e-12f)
            {
                return results;
            }

            const Math::Vec3 normalizedDirection = direction / Math::Fast::Sqrt(directionLengthSquared);
            m_root->QueryRay(origin, normalizedDirection, maxDistance, results);

            std::unordered_set<std::shared_ptr<RenderObject>> seen;
            results.erase(
                std::remove_if(results.begin(), results.end(), [&seen](const auto &object)
                {
                    return !object || !seen.insert(object).second;
                }),
                results.end());

            std::sort(results.begin(), results.end(), [&](const auto &lhs, const auto &rhs)
            {
                f32 lhsDistance = 0.0f;
                f32 rhsDistance = 0.0f;
                SpatialUtils::CalculateAABB(lhs).IntersectsRay(origin, normalizedDirection, lhsDistance);
                SpatialUtils::CalculateAABB(rhs).IntersectsRay(origin, normalizedDirection, rhsDistance);
                return lhsDistance < rhsDistance;
            });
            return results;
        }

        std::vector<std::shared_ptr<RenderObject>> Octree::QueryFrustum(const std::array<Math::Vec4, 6> &frustumPlanes) const
        {
            std::vector<std::shared_ptr<RenderObject>> results;
            if (m_root)
            {
                m_root->QueryFrustum(frustumPlanes, results);
            }
            return results;
        }

        std::shared_ptr<RenderObject> Octree::FindNearest(const Math::Vec3 &position) const
        {
            const auto nearestObjects = FindKNearest(position, 1);
            return nearestObjects.empty() ? nullptr : nearestObjects.front();
        }

        std::vector<std::shared_ptr<RenderObject>> Octree::FindKNearest(
            const Math::Vec3 &position,
            u32 k) const
        {
            if (!m_root || k == 0)
            {
                return {};
            }

            struct Candidate
            {
                f32 distanceSquared = 0.0f;
                std::shared_ptr<RenderObject> object;
            };

            struct FarthestFirst
            {
                bool operator()(const Candidate &lhs, const Candidate &rhs) const
                {
                    return lhs.distanceSquared < rhs.distanceSquared;
                }
            };

            std::priority_queue<Candidate, std::vector<Candidate>, FarthestFirst> nearest;
            std::unordered_set<const RenderObject *> visited;

            const auto considerObject = [&](const std::shared_ptr<RenderObject> &object)
            {
                if (!object || !visited.insert(object.get()).second)
                {
                    return;
                }

                const f32 distanceSquared =
                    SpatialUtils::CalculateAABB(object).DistanceSquaredToPoint(position);
                if (nearest.size() < k)
                {
                    nearest.push({distanceSquared, object});
                }
                else if (distanceSquared < nearest.top().distanceSquared)
                {
                    nearest.pop();
                    nearest.push({distanceSquared, object});
                }
            };

            const auto visitNode = [&](const auto &self, const OctreeNode *node) -> void
            {
                if (!node)
                {
                    return;
                }

                // Node-local objects must always be tested. In particular, the
                // root can retain objects outside its configured bounds, while
                // objects spanning child boundaries remain in their parent.
                for (const auto &object : node->m_objects)
                {
                    considerObject(object);
                }

                if (node->IsLeaf())
                {
                    return;
                }

                std::vector<std::pair<f32, const OctreeNode *>> orderedChildren;
                orderedChildren.reserve(node->m_children.size());
                for (const auto &child : node->m_children)
                {
                    if (child)
                    {
                        orderedChildren.emplace_back(
                            child->m_bounds.DistanceSquaredToPoint(position),
                            child.get());
                    }
                }

                std::sort(orderedChildren.begin(), orderedChildren.end(),
                          [](const auto &lhs, const auto &rhs)
                          {
                              return lhs.first < rhs.first;
                          });

                for (const auto &[minimumDistanceSquared, child] : orderedChildren)
                {
                    const f32 rejectionDistanceSquared =
                        nearest.size() < k
                            ? std::numeric_limits<f32>::max()
                            : nearest.top().distanceSquared;
                    if (minimumDistanceSquared > rejectionDistanceSquared)
                    {
                        break;
                    }

                    self(self, child);
                }
            };

            visitNode(visitNode, m_root.get());

            std::vector<Candidate> ordered;
            ordered.reserve(nearest.size());
            while (!nearest.empty())
            {
                ordered.push_back(nearest.top());
                nearest.pop();
            }
            std::sort(ordered.begin(), ordered.end(),
                      [](const Candidate &lhs, const Candidate &rhs)
                      {
                          return lhs.distanceSquared < rhs.distanceSquared;
                      });

            std::vector<std::shared_ptr<RenderObject>> results;
            results.reserve(ordered.size());
            for (const auto &candidate : ordered)
            {
                results.push_back(candidate.object);
            }
            return results;
        }

        bool Octree::Configure(const OctreeConfiguration &configuration)
        {
            OctreeConfiguration normalized = configuration;
            normalized.maxObjectsPerNode = Math::Max(1u, normalized.maxObjectsPerNode);

            if (!IsValidConfiguration(normalized))
            {
                PYRAMID_LOG_ERROR(
                    "Rejected invalid octree configuration: center and size must be finite, ",
                    "and every size component must be greater than zero");
                return false;
            }

            const AABB requestedBounds = ConfigurationBounds(normalized);
            if (m_root &&
                BoundsEqual(m_root->GetBounds(), requestedBounds) &&
                m_maxDepth == normalized.maxDepth &&
                m_maxObjectsPerNode == normalized.maxObjectsPerNode)
            {
                return true;
            }

            return RebuildWithConfiguration(normalized);
        }

        OctreeConfiguration Octree::GetConfiguration() const
        {
            OctreeConfiguration configuration;
            if (m_root)
            {
                configuration.center = m_root->GetBounds().GetCenter();
                configuration.size = m_root->GetBounds().GetSize();
            }
            configuration.maxDepth = m_maxDepth;
            configuration.maxObjectsPerNode = m_maxObjectsPerNode;
            return configuration;
        }

        void Octree::SetMaxDepth(u32 maxDepth)
        {
            auto configuration = GetConfiguration();
            configuration.maxDepth = maxDepth;
            Configure(configuration);
        }

        void Octree::SetMaxObjectsPerNode(u32 maxObjects)
        {
            auto configuration = GetConfiguration();
            configuration.maxObjectsPerNode = maxObjects;
            Configure(configuration);
        }

        void Octree::SetBounds(const Math::Vec3 &center, const Math::Vec3 &size)
        {
            auto configuration = GetConfiguration();
            configuration.center = center;
            configuration.size = size;
            Configure(configuration);
        }

        void Octree::DrawDebugVisualization() const
        {
            const auto stats = GetStats();
            PYRAMID_LOG_DEBUG(
                "Octree stats: nodes=", stats.totalNodes,
                ", leaves=", stats.leafNodes,
                ", emptyLeaves=", stats.emptyLeafNodes,
                ", objects=", stats.totalObjects,
                ", tracked=", stats.trackedObjects,
                ", maxDepth=", stats.maxDepth,
                ", memoryMB=", stats.memoryUsage,
                ", lastCollapsedNodes=", m_lastCompactionStats.collapsedNodes);
        }

        void Octree::Rebuild()
        {
            RebuildWithConfiguration(GetConfiguration());
        }

        OctreeCompactionStats Octree::Compact()
        {
            m_lastCompactionStats = m_root ? m_root->Compact() : OctreeCompactionStats{};
            return m_lastCompactionStats;
        }

        bool Octree::RebuildWithConfiguration(const OctreeConfiguration &configuration)
        {
            if (!IsValidConfiguration(configuration))
            {
                return false;
            }

            try
            {
                auto replacementRoot = std::make_unique<OctreeNode>(
                    ConfigurationBounds(configuration),
                    0,
                    configuration.maxDepth,
                    configuration.maxObjectsPerNode);

                std::unordered_map<std::shared_ptr<RenderObject>, AABB> replacementBounds;
                replacementBounds.reserve(m_objectBounds.size());

                for (const auto &entry : m_objectBounds)
                {
                    const auto &object = entry.first;
                    if (!object)
                    {
                        continue;
                    }

                    replacementRoot->Insert(object);
                    replacementBounds.emplace(object, SpatialUtils::CalculateAABB(object));
                }

                m_root.swap(replacementRoot);
                m_objectBounds.swap(replacementBounds);
                m_maxDepth = configuration.maxDepth;
                m_maxObjectsPerNode = configuration.maxObjectsPerNode;
                m_lastCompactionStats = {};
                return true;
            }
            catch (const std::exception &error)
            {
                PYRAMID_LOG_ERROR("Octree reconfiguration failed; previous tree preserved: ", error.what());
            }
            catch (...)
            {
                PYRAMID_LOG_ERROR("Octree reconfiguration failed; previous tree preserved");
            }

            return false;
        }

        bool Octree::IsValidConfiguration(const OctreeConfiguration &configuration)
        {
            return IsFiniteVector(configuration.center) &&
                   IsFiniteVector(configuration.size) &&
                   configuration.size.x > 0.0f &&
                   configuration.size.y > 0.0f &&
                   configuration.size.z > 0.0f &&
                   configuration.maxObjectsPerNode > 0;
        }

        Octree::OctreeStats Octree::GetStats() const
        {
            OctreeStats stats;
            stats.configuredMaxDepth = m_maxDepth;
            stats.trackedObjects = static_cast<u32>(m_objectBounds.size());
            if (!m_root)
            {
                return stats;
            }

            u64 leafDepthTotal = 0;
            u64 leafObjectTotal = 0;
            size_t estimatedBytes = m_objectBounds.size() *
                (sizeof(std::shared_ptr<RenderObject>) + sizeof(AABB) + sizeof(void *) * 2);

            const auto accumulate = [&](const auto &self, const OctreeNode *node) -> void
            {
                ++stats.totalNodes;
                stats.maxDepth = Math::Max(stats.maxDepth, node->m_depth);
                stats.maxObjectsInNode = Math::Max(
                    stats.maxObjectsInNode,
                    static_cast<u32>(node->m_objects.size()));
                stats.totalObjects += static_cast<u32>(node->m_objects.size());
                estimatedBytes += sizeof(OctreeNode) +
                                  node->m_objects.capacity() * sizeof(std::shared_ptr<RenderObject>);

                if (node->IsLeaf())
                {
                    ++stats.leafNodes;
                    leafDepthTotal += node->m_depth;
                    leafObjectTotal += node->m_objects.size();
                    if (node->m_objects.empty())
                    {
                        ++stats.emptyLeafNodes;
                    }
                    else
                    {
                        ++stats.occupiedLeafNodes;
                    }
                    return;
                }

                ++stats.internalNodes;
                stats.objectsInInternalNodes += static_cast<u32>(node->m_objects.size());
                for (const auto &child : node->m_children)
                {
                    if (child)
                    {
                        self(self, child.get());
                    }
                }
            };

            accumulate(accumulate, m_root.get());

            if (stats.leafNodes > 0)
            {
                stats.averageObjectsPerLeaf = static_cast<f32>(leafObjectTotal) /
                                              static_cast<f32>(stats.leafNodes);
                stats.averageDepth = static_cast<f32>(leafDepthTotal) /
                                     static_cast<f32>(stats.leafNodes);
                stats.emptyLeafRatio = static_cast<f32>(stats.emptyLeafNodes) /
                                       static_cast<f32>(stats.leafNodes);

                const u64 totalLeafCapacity = static_cast<u64>(stats.leafNodes) *
                                              static_cast<u64>(Math::Max(1u, m_maxObjectsPerNode));
                stats.leafUtilization = totalLeafCapacity > 0
                    ? static_cast<f32>(leafObjectTotal) / static_cast<f32>(totalLeafCapacity)
                    : 0.0f;
            }

            stats.memoryUsage = static_cast<f32>(estimatedBytes) / (1024.0f * 1024.0f);
            return stats;
        }

        const AABB &Octree::GetBounds() const
        {
            return m_root->GetBounds();
        }

        bool Octree::Contains(const std::shared_ptr<RenderObject> &object) const
        {
            return object && m_objectBounds.find(object) != m_objectBounds.end();
        }

        bool Octree::BoundsEqual(const AABB &lhs, const AABB &rhs)
        {
            constexpr f32 tolerance = 0.0001f;
            const auto nearlyEqual = [](f32 a, f32 b)
            {
                return Math::Abs(a - b) <= tolerance;
            };

            return nearlyEqual(lhs.min.x, rhs.min.x) &&
                   nearlyEqual(lhs.min.y, rhs.min.y) &&
                   nearlyEqual(lhs.min.z, rhs.min.z) &&
                   nearlyEqual(lhs.max.x, rhs.max.x) &&
                   nearlyEqual(lhs.max.y, rhs.max.y) &&
                   nearlyEqual(lhs.max.z, rhs.max.z);
        }

        // Spatial utility functions
        namespace SpatialUtils
        {
            AABB CalculateAABB(const std::shared_ptr<RenderObject> &object)
            {
                if (!object)
                {
                    return AABB();
                }

                Math::Vec3 boundsMin;
                Math::Vec3 boundsMax;
                object->GetWorldBounds(boundsMin, boundsMax);
                return AABB(boundsMin, boundsMax);
            }

            std::array<Math::Vec4, 6> CalculateFrustumPlanes(const Camera &camera)
            {
                return camera.GetFrustumPlanes();
            }

            bool AABBInFrustum(const AABB &aabb, const std::array<Math::Vec4, 6> &frustumPlanes)
            {
                constexpr f32 tolerance = 0.0001f;
                for (const auto &plane : frustumPlanes)
                {
                    const Math::Vec3 positiveVertex(
                        plane.x >= 0.0f ? aabb.max.x : aabb.min.x,
                        plane.y >= 0.0f ? aabb.max.y : aabb.min.y,
                        plane.z >= 0.0f ? aabb.max.z : aabb.min.z);

                    const f32 distance = plane.x * positiveVertex.x +
                                         plane.y * positiveVertex.y +
                                         plane.z * positiveVertex.z + plane.w;
                    if (distance < -tolerance)
                    {
                        return false;
                    }
                }
                return true;
            }

            f32 DistanceToAABB(const Math::Vec3 &point, const AABB &aabb)
            {
                f32 sqDist = 0.0f;

                for (int i = 0; i < 3; ++i)
                {
                    f32 v = point[i];
                    if (v < aabb.min[i])
                        sqDist += (aabb.min[i] - v) * (aabb.min[i] - v);
                    if (v > aabb.max[i])
                        sqDist += (v - aabb.max[i]) * (v - aabb.max[i]);
                }

                return Math::Fast::Sqrt(sqDist);
            }


            bool RayAABBIntersection(
                const Math::Vec3 &origin,
                const Math::Vec3 &direction,
                const AABB &aabb,
                f32 &distance)
            {
                return aabb.IntersectsRay(origin, direction, distance);
            }

            bool SphereAABBIntersection(
                const Math::Vec3 &center,
                f32 radius,
                const AABB &aabb)
            {
                return aabb.IntersectsSphere(center, radius);
            }
        }

    } // namespace Scene
} // namespace Pyramid
