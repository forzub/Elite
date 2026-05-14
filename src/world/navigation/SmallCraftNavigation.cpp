#include "SmallCraftNavigation.h"

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtx/norm.hpp>

namespace world::navigation
{

float computeStoppingDistance(
    float speed,
    float maxAcceleration
)
{
    if (maxAcceleration <= 0.001f)
        return 999999.0f;

    return (speed * speed) / (2.0f * maxAcceleration);
}

float computeAllowedStopSpeed(
    float distanceToTarget,
    float maxAcceleration
)
{
    if (maxAcceleration <= 0.001f)
        return 0.0f;

    if (distanceToTarget <= 0.0f)
        return 0.0f;

    return std::sqrt(2.0f * maxAcceleration * distanceToTarget);
}

glm::vec3 steerVelocityTowards(
    const glm::vec3& currentVelocity,
    const glm::vec3& currentPosition,
    const glm::vec3& targetPosition,
    float maxSpeed,
    float maxAcceleration,
    bool mustStopAtTarget,
    float dt
)
{
    if (dt <= 0.0f)
        return currentVelocity;

    const glm::vec3 toTarget = targetPosition - currentPosition;
    const float dist2 = glm::length2(toTarget);

    if (dist2 < 0.000001f)
        return glm::vec3(0.0f);

    const float dist = std::sqrt(dist2);
    const glm::vec3 dir = toTarget / dist;

    float desiredSpeed = maxSpeed;

    if (mustStopAtTarget)
    {
        // Ключевая часть:
        // если до точки осталось мало, скорость ограничивается тормозным путём.
        const float allowedStopSpeed =
            computeAllowedStopSpeed(dist, maxAcceleration);

        desiredSpeed = glm::min(maxSpeed, allowedStopSpeed);
    }

    const glm::vec3 desiredVelocity = dir * desiredSpeed;

    glm::vec3 delta = desiredVelocity - currentVelocity;

    const float maxDelta = maxAcceleration * dt;
    const float deltaLen2 = glm::length2(delta);

    if (deltaLen2 > maxDelta * maxDelta)
        delta = glm::normalize(delta) * maxDelta;

    glm::vec3 newVelocity = currentVelocity + delta;

    const float speed2 = glm::length2(newVelocity);
    if (speed2 > maxSpeed * maxSpeed)
        newVelocity = glm::normalize(newVelocity) * maxSpeed;

    return newVelocity;
}

bool updateSmallCraftNavigation(
    glm::vec3& position,
    glm::vec3& velocity,
    SmallCraftNavigationState& nav,
    float maxSpeed,
    float maxAcceleration,
    float arrivalRadius,
    float dt
)
{
    if (!nav.hasPath())
        return true;

    const SmallCraftWaypoint& wp =
        nav.waypoints[nav.currentWaypoint];

    const bool mustStop =
        wp.kind == SmallCraftWaypointKind::WorkPoint;

    const glm::vec3 oldPosition = position;

    velocity = steerVelocityTowards(
        velocity,
        position,
        wp.position,
        maxSpeed,
        maxAcceleration,
        mustStop,
        dt
    );

    position += velocity * dt;

    const glm::vec3 toTargetBefore = wp.position - oldPosition;
    const glm::vec3 toTargetAfter  = wp.position - position;

    const float dist2 = glm::length2(toTargetAfter);

    const bool passedTarget =
        glm::dot(toTargetBefore, toTargetAfter) <= 0.0f;

    if (mustStop)
{
    const float effectiveArrivalRadius =
        glm::max(arrivalRadius, 0.75f);

    const bool closeEnough =
        dist2 <= effectiveArrivalRadius * effectiveArrivalRadius;

    const bool passedCloseEnough =
        passedTarget &&
        glm::length2(toTargetBefore) <=
            effectiveArrivalRadius * effectiveArrivalRadius * 4.0f;

    /*
        WorkPoint:
        не требуем микроскопически точной остановки.
        Если дрон вошёл в радиус допуска или проскочил рядом —
        считаем точку достигнутой.
    */
    if (closeEnough || passedCloseEnough)
    {
        position = wp.position;
        velocity = glm::vec3(0.0f);

        ++nav.currentWaypoint;

        if (!nav.hasPath())
        {
            velocity = glm::vec3(0.0f);
            return true;
        }

        return false;
    }

    return false;
}

    // Transit:
    // можно проходить по радиусу или пересечением.
    if (dist2 <= arrivalRadius * arrivalRadius || passedTarget)
    {
        ++nav.currentWaypoint;

        if (!nav.hasPath())
        {
            velocity = glm::vec3(0.0f);
            return true;
        }
    }

    return false;
}

} // namespace world::navigation