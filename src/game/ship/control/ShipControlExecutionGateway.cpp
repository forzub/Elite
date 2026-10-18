#include "src/game/ship/control/ShipControlExecutionGateway.h"

#include <algorithm>
#include <cmath>

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"

namespace game::ship::control
{
namespace
{

bool hasManualTranslationOverride(
    const ShipControlState& control
) noexcept
{
    return
        control.cruiseActive ||
        control.jumpActive ||
        control.assistedMaxSpeedCommand ||
        control.velocityAlignmentCommand !=
            game::navigation::VelocityAlignmentMode::None ||
        std::abs(control.targetSpeedRate) > 0.001f ||
        std::abs(control.forwardInput) > 0.001f ||
        std::abs(control.liftInput) > 0.001f ||
        std::abs(control.strafeInput) > 0.001f ||
        std::abs(control.mainGimbalRightInput) > 0.001f ||
        std::abs(control.mainGimbalUpInput) > 0.001f;
}

} // namespace

ShipControlExecutionGateway::TranslationPath
ShipControlExecutionGateway::selectTranslationPath(
    const ShipControlState& control,
    game::navigation::LocalFlightControlLaw law
) noexcept
{
    if (hasManualTranslationOverride(control))
        return TranslationPath::ManualFlight;

    if (control.navigationVelocityTargetValid &&
        control.navigationAccelerationDemandValid &&
        control.navigationPrecisionTranslationOnly)
    {
        return TranslationPath::NavigationPrecision;
    }

    if (control.navigationVelocityTargetValid &&
        control.navigationAccelerationDemandValid &&
        law == game::navigation::LocalFlightControlLaw::Assisted)
    {
        return TranslationPath::NavigationAssisted;
    }

    if (control.navigationAccelerationDemandValid)
        return TranslationPath::NavigationAccelerationDemand;

    return TranslationPath::ManualFlight;
}

void ShipControlExecutionGateway::evaluateAttitudeControl(
    ShipTransform& transform,
    const ShipParams& capabilities,
    const ShipControlState& control,
    const WorldParams& world,
    float dt
)
{
    SharedShipPhysics::evaluateControl(
        transform,
        capabilities,
        control,
        world,
        dt
    );
}

void ShipControlExecutionGateway::integrateAttitude(
    ShipTransform& transform,
    const ShipParams& capabilities,
    const ShipControlState& control,
    const WorldParams& world,
    float dt
)
{
    SharedShipPhysics::integrate(
        transform,
        capabilities,
        control,
        world,
        dt
    );
}

void ShipControlExecutionGateway::propagateOrientation(
    ShipTransform& transform,
    float dt
)
{
    SharedShipPhysics::propagateOrientation(
        transform,
        dt
    );
}

ShipControlExecutionGateway::TranslationPath
ShipControlExecutionGateway::evaluateTranslationControl(
    ShipTransform& transform,
    const ShipParams& capabilities,
    const ShipControlState& control,
    float dt
)
{
    auto& motion = transform.motion;
    motion.mainPropulsionTorqueBodyNm = glm::dvec3(0.0);
    motion.mainPropulsionAngularAccelerationBodyRadPerSec2 =
        glm::dvec3(0.0);
    motion.mainTrimLoad01 = 0.0;
    motion.mainTrimUsableScale01 = 1.0;

    const TranslationPath path =
        selectTranslationPath(control, motion.localControlLaw);

    if (motion.mode != game::navigation::MotionMode::HubTactical ||
        !motion.travelFrame.valid)
    {
        return path;
    }

    switch (path)
    {
        case TranslationPath::NavigationPrecision:
            game::navigation::DynamicMotionSystem::
                applyNavigationPrecisionVelocityTrim(
                    motion,
                    motion.travelFrame,
                    capabilities,
                    dt,
                    control.navigationTargetVelocitySystemMps
                );
            break;

        case TranslationPath::NavigationAssisted:
            game::navigation::DynamicMotionSystem::
                applyNavigationAssistedFlightModel(
                    motion,
                    motion.travelFrame,
                    capabilities,
                    dt,
                    control.navigationTargetVelocitySystemMps,
                    control.navigationLinearAccelerationDemandSystemMps2,
                    transform.forward(),
                    transform.right(),
                    transform.up()
                );
            break;

        case TranslationPath::NavigationAccelerationDemand:
            game::navigation::DynamicMotionSystem::
                applySystemAccelerationDemand(
                    motion,
                    capabilities,
                    control.navigationLinearAccelerationDemandSystemMps2,
                    transform.forward()
                );
            break;

        case TranslationPath::ManualFlight:
        default:
            game::navigation::DynamicMotionSystem::applyLocalFrameInput(
                motion,
                motion.travelFrame,
                capabilities,
                dt,
                control.targetSpeedRate,
                control.cruiseActive,
                control.forwardInput,
                control.liftInput,
                control.strafeInput,
                transform.forward(),
                transform.right(),
                transform.up()
            );
            break;
    }

    return path;
}

ShipControlExecutionGateway::TranslationPath
ShipControlExecutionGateway::evaluateTranslationControl(
    ShipTransform& transform,
    const ShipParams& capabilities,
    const ShipControlState& control,
    game::ship::plant::PhysicalControlModelId model,
    const std::vector<game::ship::plant::EngineSlotState>& slotState,
    float dt
)
{
    const TranslationPath path =
        evaluateTranslationControl(
            transform,
            capabilities,
            control,
            dt
        );

    auto& motion = transform.motion;
    motion.mainEngineDeliveredThrust01.fill(0.0);
    motion.mainEngineReportedSlotCount = 0;
    if (dt <= 0.0f ||
        motion.mode != game::navigation::MotionMode::HubTactical ||
        !motion.travelFrame.valid)
    {
        return path;
    }

    const glm::dvec3 requestedMainWorld =
        motion.mainEngineAccelerationMps2;

    const glm::dvec3 right(transform.right());
    const glm::dvec3 up(transform.up());
    const glm::dvec3 forward(transform.forward());

    // Canonical body basis is +X right, +Y up, +Z aft. ShipTransform::forward
    // points along body -Z.
    const glm::dvec3 requestedMainBody(
        glm::dot(requestedMainWorld, right),
        glm::dot(requestedMainWorld, up),
        -glm::dot(requestedMainWorld, forward)
    );

    const bool explicitMainGimbalCommand =
        std::abs(control.mainGimbalRightInput) > 0.001f ||
        std::abs(control.mainGimbalUpInput) > 0.001f;

    const bool newtonian =
        game::ship::plant::familyOf(model) ==
        game::ship::plant::PhysicalControlFamily::Newtonian;

    // Do not fight an explicit pilot/autopilot TVC command. Otherwise the
    // feedback trim may use only the travel that remains inside the physical
    // nozzle envelope. Assisted has no persistent manual gimbal state.
    const double usedYawRad =
        newtonian ? std::abs(motion.mainGimbalYawRad) : 0.0;
    const double usedPitchRad =
        newtonian ? std::abs(motion.mainGimbalPitchRad) : 0.0;

    const double remainingYawTrimRad =
        explicitMainGimbalCommand
            ? 0.0
            : std::max(
                  0.0,
                  static_cast<double>(
                      capabilities.mainGimbalYawLimitRad
                  ) - usedYawRad
              );
    const double remainingPitchTrimRad =
        explicitMainGimbalCommand
            ? 0.0
            : std::max(
                  0.0,
                  static_cast<double>(
                      capabilities.mainGimbalPitchLimitRad
                  ) - usedPitchRad
              );

    const glm::dvec3 attitudeIntent01(
        static_cast<double>(control.pitchInput),
        static_cast<double>(control.yawInput),
        static_cast<double>(control.rollInput)
    );

    const auto trim =
        game::ship::plant::evaluateMainPropulsionFeedbackTrim(
            model,
            slotState,
            capabilities,
            requestedMainBody,
            attitudeIntent01,
            remainingYawTrimRad,
            remainingPitchTrimRad,
            control.controlTick
        );

    if (!trim.valid || !trim.reaction.valid)
        return path;

    // The allocator can only remove/redirect real main-engine force. Rebuild
    // the world vector from its body-space result and keep all independent
    // manoeuvre/stabilization forces intact.
    motion.mainEngineAccelerationMps2 =
        right * trim.accelerationBodyMps2.x +
        up * trim.accelerationBodyMps2.y -
        forward * trim.accelerationBodyMps2.z;

    motion.engineAccelerationMps2 =
        motion.mainEngineAccelerationMps2 +
        motion.manoeuvreAccelerationMps2 +
        motion.assistedStabilizationAccelerationMps2;

    motion.mainTrimLoad01 = trim.trimLoad01;
    motion.mainTrimUsableScale01 = trim.thrustScale01;
    motion.mainEngineDeliveredThrust01 = trim.deliveredThrust01;
    motion.mainEngineReportedSlotCount = trim.reportedSlotCount;

    motion.mainPropulsionTorqueBodyNm =
        trim.reaction.torqueBodyNm;
    motion.mainPropulsionAngularAccelerationBodyRadPerSec2 =
        trim.reaction.angularAccelerationBodyRadPerSec2;

    // Whatever the cheap main-engine feedback cannot remove remains real hull
    // reaction. Existing Assisted/Newtonian attitude feedback then sees that
    // motion on the next control sample instead of a hidden synthetic cancel.
    transform.pitchRate += static_cast<float>(
        trim.reaction.angularAccelerationBodyRadPerSec2.x *
        static_cast<double>(dt)
    );
    transform.yawRate += static_cast<float>(
        trim.reaction.angularAccelerationBodyRadPerSec2.y *
        static_cast<double>(dt)
    );
    transform.rollRate += static_cast<float>(
        trim.reaction.angularAccelerationBodyRadPerSec2.z *
        static_cast<double>(dt)
    );

    return path;
}

bool ShipControlExecutionGateway::propagateHubTacticalTranslation(
    ShipTransform& transform,
    const ShipParams& capabilities,
    double dt
)
{
    auto& motion = transform.motion;
    if (motion.mode != game::navigation::MotionMode::HubTactical ||
        !motion.travelFrame.valid)
    {
        return false;
    }

    game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
        motion,
        transform.worldPosition,
        motion.travelFrame,
        capabilities,
        dt
    );

    return true;
}

} // namespace game::ship::control
