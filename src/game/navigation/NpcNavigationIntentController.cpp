#include "NpcNavigationIntentController.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

glm::dvec3 finiteOrZero(const glm::dvec3& value) noexcept
{
    if (!std::isfinite(value.x) ||
        !std::isfinite(value.y) ||
        !std::isfinite(value.z))
    {
        return glm::dvec3(0.0);
    }
    return value;
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const glm::dvec3 finiteValue = finiteOrZero(value);
    const double length = glm::length(finiteValue);
    if (length <= 1.0e-12)
        return fallback;
    return finiteValue / length;
}

} // namespace

NavigationRuntimeControlBridge::Intent
NpcNavigationIntentController::buildIntent(
    const NpcNavigationKinematicState& state,
    const NpcNavigationGoal& goal
) noexcept
{
    NavigationRuntimeControlBridge::Intent intent;
    intent.revision = goal.revision;
    intent.emergency = goal.emergency;
    intent.hazardUrgency01 = std::clamp(goal.hazardUrgency01, 0.0, 1.0);

    const glm::dvec3 relativeWorldVelocity =
        finiteOrZero(state.relativeWorldVelocityMps);

    const glm::dvec3 forward = normalizedOr(
        state.forwardMap,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        state.rightMap,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        state.upMap,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    glm::dvec3 desiredRelativeWorldVelocity(0.0);
    if (goal.mode == NpcNavigationGoalMode::MaintainForwardCruise)
    {
        desiredRelativeWorldVelocity =
            forward * std::max(0.0, goal.desiredForwardSpeedMps);
    }

    const glm::dvec3 linearDemand =
        (desiredRelativeWorldVelocity - relativeWorldVelocity) *
        std::max(0.0, goal.velocityResponsePerSecond);

    const double angularDamping =
        std::max(0.0, goal.angularDampingPerSecond);

    const glm::dvec3 angularDemand =
        right * (-state.pitchRateRadPerSec * angularDamping) +
        up * (-state.yawRateRadPerSec * angularDamping) +
        forward * (-state.rollRateRadPerSec * angularDamping);

    intent.idealLinearAccelerationDemandMapMps2 = {
        linearDemand.x,
        linearDemand.y,
        linearDemand.z
    };
    intent.idealAngularAccelerationDemandMapRadPerSec2 = {
        angularDemand.x,
        angularDemand.y,
        angularDemand.z
    };
    return intent;
}

} // namespace game::navigation
