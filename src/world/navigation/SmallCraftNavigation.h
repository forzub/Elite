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
    // Local navigation-space position.
    // Usually owner-local / repair-local.
    // NOT global world position.
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

// Updates navigation in local float space.
// This function must not be used for global world-coordinate movement.
// Convert WorldPosition -> local space before calling it.
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