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

} // namespace

NavigationRuntimeControlBridge::Intent
NpcNavigationIntentController::buildIntent(
    const Ship& ship,
    const NpcNavigationGoal& goal
) noexcept
{
    NavigationRuntimeControlBridge::Intent intent;
    intent.revision = goal.revision;
    intent.emergency = goal.emergency;
    intent.hazardUrgency01 = std::clamp(goal.hazardUrgency01, 0.0, 1.0);

    const auto& tr = ship.core().transform();

    glm::dvec3 relativeWorldVelocity(0.0);
    if (tr.motion.travelFrame.valid)
    {
        relativeWorldVelocity =
            tr.motion.travelFrame.localToWorldVector(
                tr.motion.localVelocityMps
            );
    }
    else
    {
        relativeWorldVelocity = tr.motion.worldVelocityMps;
    }
    relativeWorldVelocity = finiteOrZero(relativeWorldVelocity);

    glm::dvec3 desiredRelativeWorldVelocity(0.0);
    if (goal.mode == NpcNavigationGoalMode::MaintainForwardCruise)
    {
        glm::dvec3 forward(tr.forward());
        const double forwardLength = glm::length(forward);
        if (forwardLength > 1.0e-12)
            forward /= forwardLength;
        else
            forward = glm::dvec3(0.0, 0.0, -1.0);

        desiredRelativeWorldVelocity =
            forward * std::max(0.0, goal.desiredForwardSpeedMps);
    }

    const glm::dvec3 linearDemand =
        (desiredRelativeWorldVelocity - relativeWorldVelocity) *
        std::max(0.0, goal.velocityResponsePerSecond);

    const glm::dvec3 right(tr.right());
    const glm::dvec3 up(tr.up());
    const glm::dvec3 forward(tr.forward());

    const double angularDamping =
        std::max(0.0, goal.angularDampingPerSecond);
    const glm::dvec3 angularDemand =
        right * (-static_cast<double>(tr.pitchRate) * angularDamping) +
        up * (-static_cast<double>(tr.yawRate) * angularDamping) +
        forward * (-static_cast<double>(tr.rollRate) * angularDamping);

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
