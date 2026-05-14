#include "ObstaclePathPlanner.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

#include <glm/common.hpp>
#include <glm/gtx/norm.hpp>

namespace world::navigation
{

static glm::vec3 safeNormalizeOr(
    const glm::vec3& v,
    const glm::vec3& fallback
)
{
    if (glm::length2(v) < 0.000001f)
        return fallback;

    return glm::normalize(v);
}

static glm::vec3 toLocal(
    const NavigationObstacle& o,
    const glm::vec3& p
)
{
    return glm::transpose(o.rotation) * (p - o.center);
}

static glm::vec3 toWorld(
    const NavigationObstacle& o,
    const glm::vec3& p
)
{
    return o.center + o.rotation * p;
}

static bool pointInsideBox(
    const glm::vec3& p,
    const glm::vec3& h
)
{
    return std::abs(p.x) <= h.x &&
           std::abs(p.y) <= h.y &&
           std::abs(p.z) <= h.z;
}

static glm::vec3 escapePointFromBox(
    const glm::vec3& point,
    const NavigationObstacle& o,
    float clearance
)
{
    if (o.shape != NavigationObstacleShape::Box)
        return point;

    const glm::vec3 local =
        toLocal(o, point);

    const glm::vec3 h =
        o.halfExtents + glm::vec3(clearance);

    if (!pointInsideBox(local, h))
        return point;

    glm::vec3 escaped = local;

    const glm::vec3 d(
        h.x - std::abs(local.x),
        h.y - std::abs(local.y),
        h.z - std::abs(local.z)
    );

    if (d.x <= d.y && d.x <= d.z)
    {
        escaped.x = (local.x >= 0.0f ? 1.0f : -1.0f) * (h.x + clearance);
    }
    else if (d.y <= d.x && d.y <= d.z)
    {
        escaped.y = (local.y >= 0.0f ? 1.0f : -1.0f) * (h.y + clearance);
    }
    else
    {
        escaped.z = (local.z >= 0.0f ? 1.0f : -1.0f) * (h.z + clearance);
    }

    return toWorld(o, escaped);
}

static float distancePointToSegmentSq(
    const glm::vec3& p,
    const glm::vec3& a,
    const glm::vec3& b
)
{
    const glm::vec3 ab = b - a;
    const float ab2 = glm::length2(ab);

    if (ab2 < 0.000001f)
        return glm::length2(p - a);

    float t = glm::dot(p - a, ab) / ab2;
    t = glm::clamp(t, 0.0f, 1.0f);

    return glm::length2(p - (a + ab * t));
}

static bool segmentIntersectsAabb(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec3& h
)
{
    const glm::vec3 d = b - a;

    float tMin = 0.0f;
    float tMax = 1.0f;

    for (int axis = 0; axis < 3; ++axis)
    {
        const float start = a[axis];
        const float dir = d[axis];
        const float minV = -h[axis];
        const float maxV =  h[axis];

        if (std::abs(dir) < 0.000001f)
        {
            if (start < minV || start > maxV)
                return false;
        }
        else
        {
            const float inv = 1.0f / dir;
            float t1 = (minV - start) * inv;
            float t2 = (maxV - start) * inv;

            if (t1 > t2)
                std::swap(t1, t2);

            tMin = glm::max(tMin, t1);
            tMax = glm::min(tMax, t2);

            if (tMin > tMax)
                return false;
        }
    }

    return true;
}

static bool segmentBlockedByObstacle(
    const glm::vec3& a,
    const glm::vec3& b,
    const NavigationObstacle& o,
    float clearance
)
{
    if (o.shape == NavigationObstacleShape::Box)
    {
        const glm::vec3 la = toLocal(o, a);
        const glm::vec3 lb = toLocal(o, b);

        const glm::vec3 h =
            o.halfExtents + glm::vec3(clearance);

        return segmentIntersectsAabb(la, lb, h);
    }

    const float r =
        o.radius + clearance;

    return distancePointToSegmentSq(
        o.center,
        a,
        b
    ) <= r * r;
}

static bool segmentBlocked(
    const glm::vec3& a,
    const glm::vec3& b,
    const std::vector<NavigationObstacle>& obstacles,
    float clearance
)
{
    for (const auto& o : obstacles)
    {
        if (segmentBlockedByObstacle(a, b, o, clearance))
            return true;
    }

    return false;
}

static void addBoxNodes(
    std::vector<glm::vec3>& nodes,
    const NavigationObstacle& o,
    float clearance
)
{
    const glm::vec3 h =
        o.halfExtents + glm::vec3(clearance * 2.0f);

    for (int sx : {-1, 1})
    for (int sy : {-1, 1})
    for (int sz : {-1, 1})
    {
        nodes.push_back(
            toWorld(
                o,
                glm::vec3(
                    float(sx) * h.x,
                    float(sy) * h.y,
                    float(sz) * h.z
                )
            )
        );
    }

    nodes.push_back(toWorld(o, glm::vec3( h.x, 0, 0)));
    nodes.push_back(toWorld(o, glm::vec3(-h.x, 0, 0)));
    nodes.push_back(toWorld(o, glm::vec3(0,  h.y, 0)));
    nodes.push_back(toWorld(o, glm::vec3(0, -h.y, 0)));
    nodes.push_back(toWorld(o, glm::vec3(0, 0,  h.z)));
    nodes.push_back(toWorld(o, glm::vec3(0, 0, -h.z)));
}

static void addSphereNodes(
    std::vector<glm::vec3>& nodes,
    const glm::vec3& start,
    const glm::vec3& target,
    const NavigationObstacle& o,
    const ObstaclePathPlannerParams& params
)
{
    const glm::vec3 dir =
        safeNormalizeOr(target - start, glm::vec3(0, 0, -1));

    const glm::vec3 up =
        std::abs(dir.y) > 0.85f
            ? glm::vec3(1, 0, 0)
            : glm::vec3(0, 1, 0);

    const glm::vec3 right =
        safeNormalizeOr(glm::cross(dir, up), glm::vec3(1, 0, 0));

    const glm::vec3 realUp =
        safeNormalizeOr(glm::cross(right, dir), glm::vec3(0, 1, 0));

    const float r =
        o.radius + params.clearance * 2.0f;

    const int samples =
        glm::max(params.radialSamples, 8);

    for (int i = 0; i < samples; ++i)
    {
        const float a =
            6.28318530718f * float(i) / float(samples);

        nodes.push_back(
            o.center +
            std::cos(a) * right * r +
            std::sin(a) * realUp * r
        );
    }
}

static std::vector<int> reconstruct(
    int target,
    const std::vector<int>& parent
)
{
    std::vector<int> p;

    for (int cur = target; cur >= 0; cur = parent[cur])
        p.push_back(cur);

    std::reverse(p.begin(), p.end());
    return p;
}

std::vector<SmallCraftWaypoint> buildObstacleAwarePath(
    const glm::vec3& start,
    const glm::vec3& target,
    const std::vector<NavigationObstacle>& obstacles,
    const ObstaclePathPlannerParams& params,
    SmallCraftWaypointKind finalKind
)
{
    glm::vec3 safeStart = start;
    glm::vec3 safeTarget = target;

    for (const auto& o : obstacles)
    {
        safeStart =
            escapePointFromBox(
                safeStart,
                o,
                params.clearance
            );

        safeTarget =
            escapePointFromBox(
                safeTarget,
                o,
                params.clearance
            );
    }

    std::vector<glm::vec3> nodes;
    nodes.push_back(safeStart);
    nodes.push_back(safeTarget);

    for (const auto& o : obstacles)
    {
        if (o.shape == NavigationObstacleShape::Box)
            addBoxNodes(nodes, o, params.clearance);
        else
            addSphereNodes(nodes, safeStart, safeTarget, o, params);
    }

    if (!segmentBlocked(safeStart, safeTarget, obstacles, params.clearance))
    {
        std::vector<SmallCraftWaypoint> direct;

        if (glm::length2(safeStart - start) > 0.01f)
        {
            direct.push_back({
                safeStart,
                SmallCraftWaypointKind::Transit
            });
        }

        direct.push_back({
            safeTarget,
            finalKind
        });

        return direct;
    }

    const int n =
        static_cast<int>(nodes.size());

    const int startIndex = 0;
    const int targetIndex = 1;

    std::vector<float> dist(
        nodes.size(),
        std::numeric_limits<float>::max()
    );

    std::vector<int> parent(
        nodes.size(),
        -1
    );

    struct Q
    {
        int index = 0;
        float cost = 0.0f;

        bool operator<(const Q& other) const
        {
            return cost > other.cost;
        }
    };

    std::priority_queue<Q> q;
    dist[startIndex] = 0.0f;
    q.push({ startIndex, 0.0f });

    while (!q.empty())
    {
        const Q cur = q.top();
        q.pop();

        if (cur.cost > dist[cur.index])
            continue;

        if (cur.index == targetIndex)
            break;

        for (int next = 0; next < n; ++next)
        {
            if (next == cur.index)
                continue;

            if (segmentBlocked(
                    nodes[cur.index],
                    nodes[next],
                    obstacles,
                    params.clearance))
            {
                continue;
            }

            const float edge =
                glm::length(nodes[next] - nodes[cur.index]);

            const float nextCost =
                cur.cost + edge;

            if (nextCost >= dist[next])
                continue;

            dist[next] = nextCost;
            parent[next] = cur.index;

            q.push({ next, nextCost });
        }
    }

    if (parent[targetIndex] < 0)
        return {};

    const auto path =
        reconstruct(targetIndex, parent);

    std::vector<SmallCraftWaypoint> out;

    if (glm::length2(safeStart - start) > 0.01f)
    {
        out.push_back({
            safeStart,
            SmallCraftWaypointKind::Transit
        });
    }

    for (size_t i = 1; i < path.size(); ++i)
    {
        const bool last =
            i + 1 == path.size();

        const glm::vec3 p =
            nodes[path[i]];

        out.push_back({
            last ? safeTarget : p,
            last ? finalKind : SmallCraftWaypointKind::Transit
        });
    }

    return out;
}



bool debugSegmentBlockedByNavigationObstacles(
    const glm::vec3& a,
    const glm::vec3& b,
    const std::vector<NavigationObstacle>& obstacles,
    float clearance
)
{
    return segmentBlocked(
        a,
        b,
        obstacles,
        clearance
    );
}



} // namespace world::navigation    