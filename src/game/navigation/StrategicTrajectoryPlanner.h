#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{

/*
    Snapshot strategic trajectory for the local Hub domain.

    This planner deliberately does not predict sixty seconds of future world
    motion and does not command the ship.  It answers one question for one
    current snapshot: "from the ship's current Hub-relative state, what is a
    sensible collision-free geometric route to the docking approach axis?"

    The first leg preserves the current relative-velocity direction.  The last
    leg is a hard straight ingress along the docking-plane normal.  Large known
    obstacles are treated as conservative static spheres in this snapshot.
    A later receding-horizon layer may rebuild the snapshot periodically while
    retaining continuity with the previous plan.
*/
struct StrategicTrajectoryObstacle
{
    std::string id;
    glm::dvec3 centerMeters {0.0};
    double radiusMeters = 0.0;
};

struct StrategicTrajectoryRequest
{
    glm::dvec3 startPositionMeters {0.0};
    glm::dvec3 startVelocityMps {0.0};

    glm::dvec3 dockCenterMeters {0.0};
    // Unit normal pointing out of the dock entrance.
    glm::dvec3 dockOutward {0.0, 0.0, 1.0};

    double shipSafetyRadiusMeters = 0.0;
    double approachStandoffMeters = 300.0;
    double terminalDepthMeters = 20.0;
    double startLeadSeconds = 3.0;
    double minimumStartLeadMeters = 120.0;

    std::vector<StrategicTrajectoryObstacle> obstacles;
};

struct StrategicTrajectoryPlan
{
    bool valid = false;
    bool obstacleDetourUsed = false;
    std::string message;

    glm::dvec3 startPointMeters {0.0};
    glm::dvec3 startLeadPointMeters {0.0};
    glm::dvec3 approachPointMeters {0.0};
    glm::dvec3 terminalPointMeters {0.0};

    // Hub-local strategic centerline.  No time semantics are implied yet.
    std::vector<glm::dvec3> pointsMeters;
};

class StrategicTrajectoryPlanner
{
private:
    struct Node
    {
        glm::dvec3 position {0.0};
    };

    static constexpr double Epsilon = 1.0e-9;

    static bool finite(const glm::dvec3& v) noexcept
    {
        return std::isfinite(v.x) && std::isfinite(v.y) &&
               std::isfinite(v.z);
    }

    static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback)
    {
        const double length2 = glm::dot(value, value);
        if (!std::isfinite(length2) || length2 <= Epsilon)
            return fallback;
        return value / std::sqrt(length2);
    }

    static double segmentPointDistanceSquared(
        const glm::dvec3& a,
        const glm::dvec3& b,
        const glm::dvec3& p)
    {
        const glm::dvec3 ab = b - a;
        const double denom = glm::dot(ab, ab);
        if (denom <= Epsilon)
            return glm::dot(p - a, p - a);
        const double t = std::clamp(
            glm::dot(p - a, ab) / denom,
            0.0,
            1.0
        );
        const glm::dvec3 closest = a + ab * t;
        return glm::dot(p - closest, p - closest);
    }

    static bool segmentClear(
        const glm::dvec3& a,
        const glm::dvec3& b,
        const std::vector<StrategicTrajectoryObstacle>& obstacles,
        double shipSafetyRadiusMeters)
    {
        for (const auto& obstacle : obstacles)
        {
            const double radius = std::max(0.0, obstacle.radiusMeters) +
                std::max(0.0, shipSafetyRadiusMeters);
            if (radius <= 0.0)
                continue;

            if (segmentPointDistanceSquared(a, b, obstacle.centerMeters) <
                radius * radius)
            {
                return false;
            }
        }
        return true;
    }

    static glm::dvec3 leastParallelAxis(const glm::dvec3& direction)
    {
        const glm::dvec3 a = glm::abs(direction);
        if (a.x <= a.y && a.x <= a.z)
            return glm::dvec3(1.0, 0.0, 0.0);
        if (a.y <= a.z)
            return glm::dvec3(0.0, 1.0, 0.0);
        return glm::dvec3(0.0, 0.0, 1.0);
    }

    static std::vector<glm::dvec3> visibilityPath(
        const glm::dvec3& start,
        const glm::dvec3& goal,
        const std::vector<StrategicTrajectoryObstacle>& obstacles,
        double shipSafetyRadiusMeters)
    {
        if (segmentClear(start, goal, obstacles, shipSafetyRadiusMeters))
            return {start, goal};

        std::vector<Node> nodes;
        nodes.push_back({start});
        nodes.push_back({goal});

        const glm::dvec3 overall = normalizedOr(
            goal - start,
            glm::dvec3(1.0, 0.0, 0.0)
        );
        glm::dvec3 sideA = glm::cross(overall, leastParallelAxis(overall));
        sideA = normalizedOr(sideA, glm::dvec3(0.0, 1.0, 0.0));
        const glm::dvec3 sideB = normalizedOr(
            glm::cross(overall, sideA),
            glm::dvec3(0.0, 0.0, 1.0)
        );

        // Six deterministic shell samples per conservative sphere.  This is a
        // small local visibility graph, not a second physics solver.
        for (const auto& obstacle : obstacles)
        {
            const double radius = std::max(0.0, obstacle.radiusMeters) +
                std::max(0.0, shipSafetyRadiusMeters);
            if (radius <= 0.0)
                continue;

            const double shell = radius * 1.35 + 25.0;
            for (const glm::dvec3& axis : {
                    sideA, -sideA, sideB, -sideB, overall, -overall})
            {
                const glm::dvec3 p = obstacle.centerMeters + axis * shell;
                bool insideOther = false;
                for (const auto& other : obstacles)
                {
                    if (&other == &obstacle)
                        continue;
                    const double otherRadius =
                        std::max(0.0, other.radiusMeters) +
                        std::max(0.0, shipSafetyRadiusMeters);
                    if (glm::length(p - other.centerMeters) < otherRadius)
                    {
                        insideOther = true;
                        break;
                    }
                }
                if (!insideOther)
                    nodes.push_back({p});
            }
        }

        const std::size_t n = nodes.size();
        const double Inf = std::numeric_limits<double>::infinity();
        std::vector<double> distance(n, Inf);
        std::vector<std::size_t> previous(n, n);
        using QueueItem = std::pair<double, std::size_t>;
        std::priority_queue<
            QueueItem,
            std::vector<QueueItem>,
            std::greater<QueueItem>
        > queue;

        distance[0] = 0.0;
        queue.push({0.0, 0});
        while (!queue.empty())
        {
            const auto [cost, from] = queue.top();
            queue.pop();
            if (cost > distance[from] + 1.0e-9)
                continue;
            if (from == 1)
                break;

            for (std::size_t to = 0; to < n; ++to)
            {
                if (to == from)
                    continue;
                if (!segmentClear(
                        nodes[from].position,
                        nodes[to].position,
                        obstacles,
                        shipSafetyRadiusMeters))
                {
                    continue;
                }

                const double edge = glm::length(
                    nodes[to].position - nodes[from].position
                );
                const double next = cost + edge;
                if (next + 1.0e-9 < distance[to])
                {
                    distance[to] = next;
                    previous[to] = from;
                    queue.push({next, to});
                }
            }
        }

        if (!std::isfinite(distance[1]))
            return {};

        std::vector<glm::dvec3> reversed;
        for (std::size_t at = 1; at < n; at = previous[at])
        {
            reversed.push_back(nodes[at].position);
            if (at == 0)
                break;
            if (previous[at] >= n)
                return {};
        }
        std::reverse(reversed.begin(), reversed.end());
        return reversed;
    }

public:
    static StrategicTrajectoryPlan plan(
        const StrategicTrajectoryRequest& request)
    {
        StrategicTrajectoryPlan out;
        if (!finite(request.startPositionMeters) ||
            !finite(request.startVelocityMps) ||
            !finite(request.dockCenterMeters) ||
            !finite(request.dockOutward))
        {
            out.message = "non-finite strategic trajectory input";
            return out;
        }

        const glm::dvec3 outward = normalizedOr(
            request.dockOutward,
            glm::dvec3(0.0, 0.0, 1.0)
        );
        const glm::dvec3 inbound = -outward;
        out.startPointMeters = request.startPositionMeters;
        out.approachPointMeters = request.dockCenterMeters + outward *
            std::max(10.0, request.approachStandoffMeters);
        out.terminalPointMeters = request.dockCenterMeters - outward *
            std::max(0.0, request.terminalDepthMeters);

        glm::dvec3 startDirection = request.startVelocityMps;
        if (glm::length(startDirection) <= 0.25)
            startDirection = out.approachPointMeters - out.startPointMeters;
        startDirection = normalizedOr(startDirection, inbound);

        const double distanceToApproach = glm::length(
            out.approachPointMeters - out.startPointMeters
        );
        double leadDistance = std::max(
            request.minimumStartLeadMeters,
            glm::length(request.startVelocityMps) *
                std::max(0.0, request.startLeadSeconds)
        );
        leadDistance = std::min(
            leadDistance,
            std::max(20.0, distanceToApproach * 0.20)
        );
        out.startLeadPointMeters =
            out.startPointMeters + startDirection * leadDistance;

        // Preserve the current velocity tangent, but never commit to a long
        // straight prefix through a known obstacle. Shorten only its length;
        // its direction remains exactly the current relative velocity.
        while (leadDistance > 5.0 &&
               !segmentClear(
                   out.startPointMeters,
                   out.startLeadPointMeters,
                   request.obstacles,
                   request.shipSafetyRadiusMeters))
        {
            leadDistance *= 0.5;
            out.startLeadPointMeters =
                out.startPointMeters + startDirection * leadDistance;
        }
        if (!segmentClear(
                out.startPointMeters,
                out.startLeadPointMeters,
                request.obstacles,
                request.shipSafetyRadiusMeters))
        {
            out.message = "no safe initial strategic tangent";
            return out;
        }

        // The prefix makes the first tangent exactly the current relative
        // velocity direction.  The suffix is a hard straight docking ingress.
        out.pointsMeters.push_back(out.startPointMeters);
        if (glm::length(out.startLeadPointMeters - out.startPointMeters) > 1.0)
            out.pointsMeters.push_back(out.startLeadPointMeters);

        const auto middle = visibilityPath(
            out.startLeadPointMeters,
            out.approachPointMeters,
            request.obstacles,
            request.shipSafetyRadiusMeters
        );
        if (middle.empty())
        {
            out.message = "no static strategic path to docking approach";
            out.pointsMeters.clear();
            return out;
        }

        for (std::size_t i = 1; i < middle.size(); ++i)
            out.pointsMeters.push_back(middle[i]);
        out.obstacleDetourUsed = middle.size() > 2;

        if (out.pointsMeters.empty() ||
            glm::length(out.pointsMeters.back() - out.approachPointMeters) > 1.0e-6)
        {
            out.pointsMeters.push_back(out.approachPointMeters);
        }
        if (!segmentClear(
                out.approachPointMeters,
                out.terminalPointMeters,
                request.obstacles,
                request.shipSafetyRadiusMeters))
        {
            out.message = "docking ingress is blocked in strategic snapshot";
            out.pointsMeters.clear();
            return out;
        }
        out.pointsMeters.push_back(out.terminalPointMeters);

        // The approach point and terminal point must form the exact ingress
        // normal.  Keep this as an explicit invariant rather than hoping a
        // smoother happens to become perpendicular near the dock.
        const glm::dvec3 finalLeg = normalizedOr(
            out.terminalPointMeters - out.approachPointMeters,
            inbound
        );
        if (glm::dot(finalLeg, inbound) < 1.0 - 1.0e-9)
        {
            out.message = "strategic docking ingress is not normal to dock";
            out.pointsMeters.clear();
            return out;
        }

        out.valid = out.pointsMeters.size() >= 3;
        out.message = out.valid
            ? (out.obstacleDetourUsed
                ? "snapshot strategic docking path with static detour"
                : "snapshot strategic docking path")
            : "strategic trajectory has too few points";
        return out;
    }
};

} // namespace game::navigation
