#include "NavigationRuntimeControlBridge.h"

#include <cmath>

namespace game::navigation
{
namespace
{

bool finite(const NavigationRuntimeControlBridge::Vec3d& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool validIntent(
    const NavigationRuntimeControlBridge::Intent& intent
) noexcept
{
    return
        finite(intent.idealLinearAccelerationDemandMapMps2) &&
        finite(intent.idealAngularAccelerationDemandMapRadPerSec2) &&
        std::isfinite(intent.hazardUrgency01) &&
        intent.hazardUrgency01 >= 0.0 &&
        intent.hazardUrgency01 <= 1.0;
}

glm::dvec3 toGlm(
    const NavigationRuntimeControlBridge::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

} // namespace

NavigationRuntimeControlBridge::NavigationRuntimeControlBridge(
    const PilotSkillProfile& profile
) noexcept
    : executor_(profile)
{
}

bool NavigationRuntimeControlBridge::reset(
    double timeSeconds,
    const Intent& initialIntent
) noexcept
{
    if (!validIntent(initialIntent))
        return false;

    return executor_.reset(
        timeSeconds,
        toPilotCommand(initialIntent)
    );
}

NavigationRuntimeControlBridge::StepResult
NavigationRuntimeControlBridge::step(
    double timeSeconds,
    double deltaSeconds,
    const Intent& intent
) noexcept
{
    StepResult result;
    if (!validIntent(intent))
    {
        result.status = PilotExecutor::Status::InvalidInput;
        return result;
    }

    const auto pilot = executor_.step(
        timeSeconds,
        deltaSeconds,
        toPilotCommand(intent)
    );

    result.status = pilot.status;

    result.snapshot.intentRevision = intent.revision;
    result.snapshot.activeTargetRevision = pilot.activeTargetRevision;
    result.snapshot.idealLinearAccelerationDemandMapMps2 =
        intent.idealLinearAccelerationDemandMapMps2;
    result.snapshot.idealAngularAccelerationDemandMapRadPerSec2 =
        intent.idealAngularAccelerationDemandMapRadPerSec2;
    result.snapshot.executedLinearAccelerationDemandMapMps2 =
        fromPilotVec(
            pilot.executedLinearAccelerationDemandMapMetersPerSec2
        );
    result.snapshot.executedAngularAccelerationDemandMapRadPerSec2 =
        fromPilotVec(
            pilot.executedAngularAccelerationDemandMapRadPerSec2
        );
    result.snapshot.emergency = intent.emergency;
    result.snapshot.hazardUrgency01 = intent.hazardUrgency01;
    result.snapshot.reactionBlocked = pilot.reactionBlocked;
    result.snapshot.decisionSampled = pilot.decisionSampled;
    result.snapshot.queuedCommandApplied = pilot.queuedCommandApplied;
    result.snapshot.pendingCommandCount = pilot.pendingCommandCount;

    if (pilot.status != PilotExecutor::Status::Ok)
        return result;

    result.snapshot.valid = true;

    // Produce a clean autopilot-owned control sample. Manual key fields remain
    // neutral; downstream SharedShipPhysics/GameSimulation detect the explicit
    // navigation demand channel and apply the real capability limits.
    result.control = ShipControlState {};
    result.control.navigationAccelerationDemandValid = true;
    result.control.navigationLinearAccelerationDemandMapMps2 = toGlm(
        result.snapshot.executedLinearAccelerationDemandMapMps2
    );
    result.control.navigationAngularAccelerationDemandMapRadPerSec2 = toGlm(
        result.snapshot.executedAngularAccelerationDemandMapRadPerSec2
    );
    result.control.navigationIntentRevision = intent.revision;

    return result;
}

NavigationRuntimeControlBridge::PilotExecutor::Command
NavigationRuntimeControlBridge::toPilotCommand(
    const Intent& intent
) noexcept
{
    PilotExecutor::Command command;
    command.revision = intent.revision;
    command.linearAccelerationDemandMapMetersPerSec2 = {
        intent.idealLinearAccelerationDemandMapMps2.x,
        intent.idealLinearAccelerationDemandMapMps2.y,
        intent.idealLinearAccelerationDemandMapMps2.z
    };
    command.angularAccelerationDemandMapRadPerSec2 = {
        intent.idealAngularAccelerationDemandMapRadPerSec2.x,
        intent.idealAngularAccelerationDemandMapRadPerSec2.y,
        intent.idealAngularAccelerationDemandMapRadPerSec2.z
    };
    command.emergency = intent.emergency;
    command.hazardUrgency01 = intent.hazardUrgency01;
    return command;
}

NavigationRuntimeControlBridge::Vec3d
NavigationRuntimeControlBridge::fromPilotVec(
    const PilotExecutor::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

} // namespace game::navigation
