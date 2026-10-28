#include "src/game/ship/control/ShipControlExecutionGateway.h"

#include <algorithm>
#include <cmath>

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"
#include "src/game/ship/plant/ShipEngineControlMatrix.h"

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

    motion.newtonianRcsKeepsDeltaV = control.rcsKeepsTranslationDeltaV;
    motion.newtonianRcsPerAxisAuthority =
        control.rcsTranslationPerAxisAuthority;
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
    float dt,
    const std::vector<game::equipment::engine::MountedEngine>* mountedEngines,
    game::ship::plant::AssistedManeuverPrograms* maneuverPrograms,
    const std::vector<MainEnginePowerGroup>* powerGroups
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
    // A previous fixed-step execution must not authorize a future tick,
    // even if this command exits early or the plant rejects the request.
    motion.installedMainPlantExecuted = false;
    motion.requestedMainEngineAccelerationMps2 =
        motion.mainEngineAccelerationMps2;
    motion.requestedManoeuvreAccelerationMps2 =
        motion.manoeuvreAccelerationMps2;
    motion.requestedAssistedStabilizationAccelerationMps2 =
        motion.assistedStabilizationAccelerationMps2;
    motion.assistedWorkingThrustAxisEmergency =
        capabilities.assistedWorkingThrustAxisEmergency;
    motion.assistedWorkingThrustAxisBody =
        capabilities.assistedWorkingThrustAxisBody;
    motion.mainEngineDeliveredThrust01.fill(0.0);
    motion.mainEngineReportedSlotCount = 0;
    if (dt <= 0.0f ||
        motion.mode != game::navigation::MotionMode::HubTactical ||
        !motion.travelFrame.valid)
    {
        // Never display a previous burn after changing motion mode.
        motion.engineTelemetry.reset(control.controlTick);
        return path;
    }

    // Preserve the command-to-plant boundary. DynamicMotionSystem above
    // supplied a REQUEST; integration below may consume ONLY the force
    // that a valid physical execution actually delivered. No allocator
    // failure may leave the unexecuted request in a motion field.
    const auto rejectUnexecutedPropulsion = [&]()
    {
        motion.mainEngineAccelerationMps2={};
        motion.assistedStabilizationAccelerationMps2={};
        if (game::ship::plant::familyOf(model) ==
            game::ship::plant::PhysicalControlFamily::Assisted)
            motion.manoeuvreAccelerationMps2={};
        motion.engineAccelerationMps2=motion.manoeuvreAccelerationMps2;
        motion.mainPropulsionTorqueBodyNm={};
        motion.mainPropulsionAngularAccelerationBodyRadPerSec2={};
        motion.mainEngineDeliveredThrust01.fill(0.0);
        motion.mainEngineReportedSlotCount=0;
        motion.mainTrimLoad01=0.0;
        motion.mainTrimUsableScale01=0.0;
    };

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

    if (mountedEngines)
    {
        // Both flight families execute their INSTALLED main engines through
        // one physical matrix. Assisted adds its keyed lateral/stabilization
        // demand; Newtonian independently retains its actual RCS subsystem.
        // Assisted angular control is also engine-executed, never free.
        const bool assisted =
            game::ship::plant::familyOf(model) ==
                game::ship::plant::PhysicalControlFamily::Assisted;
        const glm::dvec3 requestedAllWorld =
            requestedMainWorld +
            (assisted ? motion.assistedStabilizationAccelerationMps2 :
                        glm::dvec3(0.0));

        // The ordinary combat-speed envelope constrains the REQUEST, not
        // the wrench after the mounted engine(s) execute it. Use exactly
        // the same envelope as the legacy motion path, in travel-frame
        // coordinates (velocity and acceleration must share that frame).
        // Newtonian RCS is independent and is never clipped here.
        const auto& frame = motion.travelFrame;
        const glm::dvec3 boundedRequestLocal =
            game::navigation::DynamicMotionSystem::
                limitRequestedPropulsionToControlledSpeed(
                    motion.localVelocityMps,
                    frame.worldToLocalVector(requestedAllWorld),
                    game::ship::controlledSpeedLimitMps(capabilities),
                    static_cast<double>(dt)
                );
        const glm::dvec3 boundedRequestWorld =
            frame.localToWorldVector(boundedRequestLocal);

        game::ship::plant::EngineMatrixDemand demand;
        demand.accelerationBodyMps2 = {
            glm::dot(boundedRequestWorld,right),
            glm::dot(boundedRequestWorld,up),
            -glm::dot(boundedRequestWorld,forward)
        };
        demand.angularAccelerationBodyRadPerSec2 = assisted
            ? motion.requestedMainAngularAccelerationBodyRadPerSec2
            : glm::dvec3(0.0); // Newtonian pilot attitude owns RCS.
        // TVC is an immediate field-vector command in this game universe.
        // Independent nozzles reach the requested cone angle this very tick.
        demand.tick = control.controlTick;
        // Nozzle torque the installed attitude actuators will cancel this
        // tick: half of the ship's real angular authority, the rest stays
        // for attitude control. Zero when those actuators are lost.
        demand.compensableAngularAccelerationRadPerSec2 =
            0.5 * game::ship::angularAccelerationLimitRadPerSec2(
                capabilities);
        // Assisted: cached signed maneuver patterns from the installed
        // geometry/capabilities. No per-tick 96-pass optimization.
        // Newtonian remains on its independent main/TVC + RCS doctrine.
        auto actual =
            assisted && maneuverPrograms
                ? maneuverPrograms->execute(
                    model,*mountedEngines,capabilities,demand
                  )
                : game::ship::plant::executeEngineControlMatrix(
                    model,*mountedEngines,capabilities,demand
                  );
        motion.engineTelemetry.reset(control.controlTick);
        if (!actual.valid)
        {
            rejectUnexecutedPropulsion();
            return path;
        }

        // Scale already-resolved physical nozzle forces by the electrical
        // budget of their INSTALLED reactor, then recompute the real rigid
        // body wrench. This is an execution constraint, not HUD bookkeeping.
        // Both the Assisted cached programs and Newtonian matrix use it.
        if (powerGroups && powerGroups->empty())
        {
            // An explicit installed electrical budget with no reactor
            // groups is unavailable power, NOT unlimited power. A nullptr
            // is reserved for isolated diagnostic/legacy callers.
            rejectUnexecutedPropulsion();
            return path;
        }
        if (powerGroups)
        {
            // Assisted wrench programs balance forces and torques ACROSS
            // generator groups. Scaling each group independently breaks that
            // balance and injects unintended yaw/pitch after a key release.
            // Use one common feasible fraction for ALL Assisted motors.
            double commonFactor=1.0;
            if(assisted)
            {
                for(const auto& group:*powerGroups)
                {
                    double demandMW=0.0;
                    for(const auto i:group.engineIndices)
                        if(i<actual.count)
                            demandMW+=std::max(
                                0.0,actual.engines[i].electricalMW);
                    if(demandMW>0.0)
                        commonFactor=std::min(commonFactor,
                            std::clamp(std::max(0.0,group.availableMW)/
                                demandMW,0.0,1.0));
                }
                for(std::size_t i=0;i<actual.count;++i)
                {
                    auto& e=actual.engines[i];
                    e.deliveredThrust01*=commonFactor;
                    e.forceBodyNewtons*=commonFactor;
                    e.electricalMW*=commonFactor;
                    e.heatMW*=commonFactor;
                }
            }
            else
            {
                // Newtonian does not use Assisted's paired-force program.
                for(const auto& group:*powerGroups)
                {
                    double demandMW=0.0;
                    for(const auto i:group.engineIndices)
                        if(i<actual.count)
                            demandMW+=std::max(
                                0.0,actual.engines[i].electricalMW);
                    const double factor=demandMW>0.0
                        ? std::clamp(std::max(0.0,group.availableMW)/
                            demandMW,0.0,1.0):1.0;
                    if(factor>=1.0)continue;
                    for(const auto i:group.engineIndices)
                    {
                        if(i>=actual.count)continue;
                        auto& e=actual.engines[i];
                        e.deliveredThrust01*=factor;
                        e.forceBodyNewtons*=factor;
                        e.electricalMW*=factor;
                        e.heatMW*=factor;
                    }
                }
            }
            const auto& topology =
                game::ship::plant::physicalControlSlots(model);
            glm::dvec3 force(0.0),torque(0.0);
            glm::dvec3 extents=capabilities.physicalActuatorHalfExtentsMeters;
            if(extents.x<=0.0 || extents.y<=0.0 || extents.z<=0.0)
                extents=glm::dvec3(std::max(1.0f,capabilities.turnRadius));
            for(std::size_t i=0;i<actual.count && i<topology.size();++i)
            {
                const auto& f=actual.engines[i].forceBodyNewtons;
                force+=f;
                torque+=glm::cross(
                    topology[i].normalizedPositionBody*extents-
                    capabilities.centerOfMassLocalMeters,f);
            }
            actual.forceBodyNewtons=force;
            actual.torqueBodyNm=torque;
            actual.accelerationBodyMps2=
                force/std::max(1.0,capabilities.massKg);
            actual.angularAccelerationBodyRadPerSec2={
                torque.x/std::max(1.0,capabilities.pitchInertiaKgM2),
                torque.y/std::max(1.0,capabilities.yawInertiaKgM2),
                -torque.z/std::max(1.0,capabilities.rollInertiaKgM2)};
        }

        auto& telemetry = motion.engineTelemetry;
        telemetry.valid = true;
        telemetry.count = static_cast<std::uint8_t>(actual.count);
        motion.mainEngineDeliveredThrust01.fill(0.0);
        motion.mainEngineReportedSlotCount=actual.count;
        for (std::size_t i=0;i<actual.count;++i)
        {
            const auto& e=actual.engines[i];
            telemetry.thrust01[i] = e.deliveredThrust01;
            telemetry.available01[i] = e.availableThrust01;
            telemetry.nozzleYawRad[i] = e.nozzleYawRad;
            telemetry.nozzlePitchRad[i] = e.nozzlePitchRad;
            telemetry.forceBodyNewtons[i] = e.forceBodyNewtons;
            telemetry.electricalMW[i] = e.electricalMW;
            telemetry.wasteHeatMW[i] = e.heatMW;
            motion.mainEngineDeliveredThrust01[i] = e.deliveredThrust01;
        }
        motion.mainEngineAccelerationMps2 =
            right*actual.accelerationBodyMps2.x +
            up*actual.accelerationBodyMps2.y -
            forward*actual.accelerationBodyMps2.z;
        motion.assistedStabilizationAccelerationMps2 = {};
        motion.engineAccelerationMps2 =
            motion.mainEngineAccelerationMps2 +
            motion.manoeuvreAccelerationMps2;
        motion.mainPropulsionTorqueBodyNm=actual.torqueBodyNm;
        motion.mainPropulsionAngularAccelerationBodyRadPerSec2 =
            actual.angularAccelerationBodyRadPerSec2;
        motion.mainTrimLoad01=0.0;
        motion.mainTrimUsableScale01=1.0;
        // Only installed motors (not slot approximations) can certify the
        // actual force/mass consumed by the fixed-step integrator.
        motion.installedMainPlantExecuted = true;
        transform.pitchRate += static_cast<float>(
            actual.angularAccelerationBodyRadPerSec2.x * dt
        );
        transform.yawRate += static_cast<float>(
            actual.angularAccelerationBodyRadPerSec2.y * dt
        );
        transform.rollRate += static_cast<float>(
            actual.angularAccelerationBodyRadPerSec2.z * dt
        );
        return path;
    }

    // Diagnostic layouts with no installed motors keep their historic
    // isolated slot oracle; they do not publish invented motor data.
    motion.engineTelemetry.reset(control.controlTick);
    const auto trim =
        game::ship::plant::evaluateMainPropulsionFeedbackTrim(
            model,
            slotState,
            capabilities,
            requestedMainBody,
            attitudeIntent01,
            remainingYawTrimRad,
            remainingPitchTrimRad,
            control.controlTick,
            mountedEngines
        );

    if (!trim.valid || !trim.reaction.valid)
    {
        rejectUnexecutedPropulsion();
        return path;
    }

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
