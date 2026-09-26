#include "NavigationRuntimeControlBridge.h"

#include <cmath>

namespace game::navigation
{
namespace
{

bool finite(const glm::dvec3& value) noexcept
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
        finite(intent.idealLinearAccelerationSystemMps2) &&
        finite(intent.idealAngularAccelerationSystemRadPerSec2) &&
        std::isfinite(intent.hazardUrgency01) &&
        intent.hazardUrgency01 >= 0.0 &&
        intent.hazardUrgency01 <= 1.0;
}

bool validActuatorCommand(
    const NavigationRuntimeControlBridge::ProgramActuatorCommand& actuator
) noexcept
{
    const auto unit = [](double value) noexcept
    {
        return std::isfinite(value) &&
            value >= 0.0 &&
            value <= 1.0;
    };

    return
        actuator.valid &&
        unit(actuator.rearMainThrottle01) &&
        unit(actuator.foreMainThrottle01) &&
        finite(actuator.manoeuvreAccelerationSystemMps2) &&
        finite(actuator.linearFeedbackAccelerationSystemMps2);
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
    result.snapshot.idealLinearAccelerationDemandSystemMps2 =
        intent.idealLinearAccelerationSystemMps2;
    result.snapshot.idealAngularAccelerationDemandSystemRadPerSec2 =
        intent.idealAngularAccelerationSystemRadPerSec2;
    result.snapshot.executedLinearAccelerationDemandSystemMps2 =
        fromPilotVec(
            pilot.executedLinearAccelerationDemandMetersPerSec2
        );
    result.snapshot.executedAngularAccelerationDemandSystemRadPerSec2 =
        fromPilotVec(
            pilot.executedAngularAccelerationDemandRadPerSec2
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
    result.control.navigationLinearAccelerationDemandSystemMps2 = result.snapshot.executedLinearAccelerationDemandSystemMps2;
    result.control.navigationAngularAccelerationDemandSystemRadPerSec2 = result.snapshot.executedAngularAccelerationDemandSystemRadPerSec2;
    result.control.navigationIntentRevision = intent.revision;

    return result;
}

NavigationRuntimeControlBridge::StepResult
NavigationRuntimeControlBridge::stepProgram(
    double timeSeconds,
    double deltaSeconds,
    const Intent& intent,
    const ProgramActuatorCommand& actuator
) noexcept
{
    StepResult result = step(
        timeSeconds,
        deltaSeconds,
        intent
    );

    if (result.status != PilotExecutor::Status::Ok ||
        !result.snapshot.valid ||
        !validActuatorCommand(actuator))
    {
        if (!validActuatorCommand(actuator))
        {
            result.status = PilotExecutor::Status::InvalidInput;
            result.control = ShipControlState {};
            result.snapshot.valid = false;
        }
        return result;
    }

    result.control.navigationActuatorProgramValid = true;
    result.control.navigationRearMainThrottle01 =
        actuator.rearMainThrottle01;
    result.control.navigationForeMainThrottle01 =
        actuator.foreMainThrottle01;
    result.control.navigationManoeuvreAccelerationSystemMps2 =
        actuator.manoeuvreAccelerationSystemMps2;
    result.control.navigationLinearFeedbackAccelerationSystemMps2 =
        actuator.linearFeedbackAccelerationSystemMps2;
    return result;
}

NavigationRuntimeControlBridge::PilotExecutor::Command
NavigationRuntimeControlBridge::toPilotCommand(
    const Intent& intent
) noexcept
{
    PilotExecutor::Command command;
    command.revision = intent.revision;
    command.targetRevision = intent.targetRevision;
    command.linearAccelerationDemandMetersPerSec2 = {
        intent.idealLinearAccelerationSystemMps2.x,
        intent.idealLinearAccelerationSystemMps2.y,
        intent.idealLinearAccelerationSystemMps2.z
    };
    command.angularAccelerationDemandRadPerSec2 = {
        intent.idealAngularAccelerationSystemRadPerSec2.x,
        intent.idealAngularAccelerationSystemRadPerSec2.y,
        intent.idealAngularAccelerationSystemRadPerSec2.z
    };
    command.emergency = intent.emergency;
    command.hazardUrgency01 = intent.hazardUrgency01;
    return command;
}

glm::dvec3
NavigationRuntimeControlBridge::fromPilotVec(
    const PilotExecutor::Vec3d& value
) noexcept
{
    return {value.x, value.y, value.z};
}

} // namespace game::navigation
