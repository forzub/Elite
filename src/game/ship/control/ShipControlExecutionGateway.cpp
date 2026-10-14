#include "src/game/ship/control/ShipControlExecutionGateway.h"

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
    if (dt <= 0.0f ||
        motion.mode != game::navigation::MotionMode::HubTactical ||
        !motion.travelFrame.valid)
    {
        return path;
    }

    const glm::dvec3 mainAccelerationWorld =
        motion.mainEngineAccelerationMps2;

    const glm::dvec3 right(transform.right());
    const glm::dvec3 up(transform.up());
    const glm::dvec3 forward(transform.forward());

    // Canonical body basis is +X right, +Y up, +Z aft. ShipTransform::forward
    // points along body -Z.
    const glm::dvec3 mainAccelerationBody(
        glm::dot(mainAccelerationWorld, right),
        glm::dot(mainAccelerationWorld, up),
        -glm::dot(mainAccelerationWorld, forward)
    );

    const auto reaction =
        game::ship::plant::evaluateMainPropulsionReaction(
            model,
            slotState,
            capabilities,
            mainAccelerationBody
        );

    if (!reaction.valid)
        return path;

    motion.mainPropulsionTorqueBodyNm =
        reaction.torqueBodyNm;
    motion.mainPropulsionAngularAccelerationBodyRadPerSec2 =
        reaction.angularAccelerationBodyRadPerSec2;

    // This is physical force application, not a controller request. Do not run
    // it through normal controlled-rate clamps: an asymmetric damaged main
    // bank may overpower the surviving attitude-control authority.
    transform.pitchRate += static_cast<float>(
        reaction.angularAccelerationBodyRadPerSec2.x *
        static_cast<double>(dt)
    );
    transform.yawRate += static_cast<float>(
        reaction.angularAccelerationBodyRadPerSec2.y *
        static_cast<double>(dt)
    );
    transform.rollRate += static_cast<float>(
        reaction.angularAccelerationBodyRadPerSec2.z *
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
