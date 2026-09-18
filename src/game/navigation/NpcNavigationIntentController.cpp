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

NavigationSystemControlIntent
NpcNavigationIntentController::buildIntent(
    const NpcNavigationKinematicState& state,
    const NpcNavigationGoal& goal
) noexcept
{
    NavigationSystemControlIntent intent;
    intent.revision = goal.revision;
    intent.emergency = goal.emergency;
    intent.hazardUrgency01 = std::clamp(goal.hazardUrgency01, 0.0, 1.0);

    const glm::dvec3 relativeSystemVelocity =
        finiteOrZero(state.relativeSystemVelocityMps);

    const glm::dvec3 forward = normalizedOr(
        state.forwardSystem,
        glm::dvec3(0.0, 0.0, -1.0)
    );
    const glm::dvec3 right = normalizedOr(
        state.rightSystem,
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 up = normalizedOr(
        state.upSystem,
        glm::dvec3(0.0, 1.0, 0.0)
    );

    glm::dvec3 desiredRelativeSystemVelocity(0.0);
    if (goal.mode == NpcNavigationGoalMode::MaintainForwardCruise)
    {
        desiredRelativeSystemVelocity =
            forward * std::max(0.0, goal.desiredForwardSpeedMps);
    }

    const glm::dvec3 linearDemand =
        (desiredRelativeSystemVelocity - relativeSystemVelocity) *
        std::max(0.0, goal.velocityResponsePerSecond);

    const double angularDamping =
        std::max(0.0, goal.angularDampingPerSecond);

    const glm::dvec3 angularDemand =
        right * (-state.pitchRateRadPerSec * angularDamping) +
        up * (-state.yawRateRadPerSec * angularDamping) +
        forward * (-state.rollRateRadPerSec * angularDamping);

    intent.idealLinearAccelerationSystemMps2 = {
        linearDemand.x,
        linearDemand.y,
        linearDemand.z
    };
    intent.idealAngularAccelerationSystemRadPerSec2 = {
        angularDemand.x,
        angularDemand.y,
        angularDemand.z
    };
    return intent;
}

} // namespace game::navigation
