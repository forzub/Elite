#pragma once

#include <vector>

#include <glm/glm.hpp>

namespace world::navigation
{

enum class SmallCraftWaypointKind
{
    Transit,
    FlyBy,
    WorkPoint
};

struct SmallCraftWaypoint
{
    glm::vec3 position {0.0f};
    SmallCraftWaypointKind kind = SmallCraftWaypointKind::Transit;
};

struct SmallCraftNavigationState
{
    std::vector<SmallCraftWaypoint> waypoints;
    int currentWaypoint = 0;

    bool hasPath() const
    {
        return currentWaypoint >= 0 &&
               currentWaypoint < static_cast<int>(waypoints.size());
    }

    void clear()
    {
        waypoints.clear();
        currentWaypoint = 0;
    }

    void setPath(std::vector<SmallCraftWaypoint> path)
    {
        waypoints = std::move(path);
        currentWaypoint = 0;
    }
};

float computeStoppingDistance(
    float speed,
    float maxAcceleration
);

float computeAllowedStopSpeed(
    float distanceToTarget,
    float maxAcceleration
);

glm::vec3 steerVelocityTowards(
    const glm::vec3& currentVelocity,
    const glm::vec3& currentPosition,
    const glm::vec3& targetPosition,
    float maxSpeed,
    float maxAcceleration,
    bool mustStopAtTarget,
    float dt
);

bool updateSmallCraftNavigation(
    glm::vec3& position,
    glm::vec3& velocity,
    SmallCraftNavigationState& nav,
    float maxSpeed,
    float maxAcceleration,
    float arrivalRadius,
    float dt
);

} // namespace world::navigation