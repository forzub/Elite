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

    auto reaction =
        game::ship::plant::evaluateMainPropulsionReaction(
            model,
            slotState,
            capabilities,
            mainAccelerationBody
        );

    if (!reaction.valid)
        return path;

    // Assisted damage trim.
    //
    // A healthy symmetric bank has zero parasitic reaction and therefore this
    // path is exactly inert. With asymmetric damage, Assisted spends part of
    // its remaining automatic vectoring/attitude authority cancelling the
    // reaction torque. That compensation is not free: some propulsive margin
    // is reserved for trim, reducing usable main acceleration before the
    // remaining reaction is applied to the rigid body.
    //
    // This is deliberately a cinematic allocator rather than a detailed
    // per-nozzle solver. The important causal contract is:
    //   damage -> parasitic torque -> trim effort -> lost usable thrust;
    // if trim authority saturates, residual torque remains visible.
    if (game::ship::plant::familyOf(model) ==
        game::ship::plant::PhysicalControlFamily::Assisted)
    {
        const double trimAuthority =
            std::max(
                0.0,
                static_cast<double>(capabilities.angularAccel)
            );

        if (trimAuthority > 1.0e-9)
        {
            const glm::dvec3 rawAlpha =
                reaction.angularAccelerationBodyRadPerSec2;

            const double trimLoad01 =
                std::clamp(
                    std::max({
                        std::abs(rawAlpha.x),
                        std::abs(rawAlpha.y),
                        std::abs(rawAlpha.z)
                    }) / trimAuthority,
                    0.0,
                    1.0
                );

            // At full trim load reserve 20% of currently requested main-force
            // margin for balancing. Healthy trimLoad=0 => exact old behavior.
            const double usableMainScale =
                1.0 - 0.20 * trimLoad01;

            if (usableMainScale < 1.0 - 1.0e-12)
            {
                motion.mainEngineAccelerationMps2 *= usableMainScale;

                const glm::dvec3 scaledMainAccelerationWorld =
                    motion.mainEngineAccelerationMps2;
                const glm::dvec3 scaledMainAccelerationBody(
                    glm::dot(scaledMainAccelerationWorld, right),
                    glm::dot(scaledMainAccelerationWorld, up),
                    -glm::dot(scaledMainAccelerationWorld, forward)
                );

                reaction =
                    game::ship::plant::evaluateMainPropulsionReaction(
                        model,
                        slotState,
                        capabilities,
                        scaledMainAccelerationBody
                    );
            }

            if (reaction.valid)
            {
                glm::dvec3 residualAlpha =
                    reaction.angularAccelerationBodyRadPerSec2;

                auto trimAxis =
                    [trimAuthority](double value) noexcept
                    {
                        return
                            value -
                            std::clamp(
                                value,
                                -trimAuthority,
                                trimAuthority
                            );
                    };

                residualAlpha.x = trimAxis(residualAlpha.x);
                residualAlpha.y = trimAxis(residualAlpha.y);
                residualAlpha.z = trimAxis(residualAlpha.z);

                reaction.angularAccelerationBodyRadPerSec2 =
                    residualAlpha;

                reaction.torqueBodyNm = {
                    residualAlpha.x *
                        std::max(
                            1.0,
                            static_cast<double>(
                                capabilities.pitchInertiaKgM2
                            )
                        ),
                    residualAlpha.y *
                        std::max(
                            1.0,
                            static_cast<double>(
                                capabilities.yawInertiaKgM2
                            )
                        ),
                    -residualAlpha.z *
                        std::max(
                            1.0,
                            static_cast<double>(
                                capabilities.rollInertiaKgM2
                            )
                        )
                };
            }
        }
    }

    motion.mainPropulsionTorqueBodyNm =
        reaction.torqueBodyNm;
    motion.mainPropulsionAngularAccelerationBodyRadPerSec2 =
        reaction.angularAccelerationBodyRadPerSec2;

    // This is physical force application, not a controller request. Newtonian
    // keeps the residual reaction directly visible. Assisted only reaches this
    // point with torque left over after its automatic trim authority saturates.
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
