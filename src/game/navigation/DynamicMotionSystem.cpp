#include <algorithm>
#include <cmath>

#include "DynamicMotionSystem.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/navigation/LocalFlightControlStateMachine.h"

namespace game::navigation
{
namespace
{
inline double positiveOr(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

inline glm::dvec3 clampMagnitude(
    glm::dvec3 value,
    double maxLength
)
{
    if (maxLength <= 0.0)
        return glm::dvec3(0.0);

    const double length = glm::length(value);
    if (length > maxLength && length > 1.0e-12)
        value *= maxLength / length;

    return value;
}

// The caller-selected primary acceleration has priority. A combined command
// must still fit the ship linear-load envelope; only the secondary vector is
// reduced. Newtonian/main allocation passes main as primary, while Assisted
// course coupling may pass the automatic velocity stabilizer as primary.
inline glm::dvec3 clampSecondaryToTotalAccelerationEnvelope(
    const glm::dvec3& primary,
    glm::dvec3 secondary,
    double totalLimit
)
{
    if (totalLimit <= 0.0)
        return glm::dvec3(0.0);

    const double primaryLength = glm::length(primary);
    if (primaryLength >= totalLimit - 1.0e-12)
        return glm::dvec3(0.0);

    if (glm::length(primary + secondary) <= totalLimit + 1.0e-12)
        return secondary;

    // Solve |primary + s*secondary| = totalLimit for s in [0,1].
    const double a = glm::dot(secondary, secondary);
    if (a <= 1.0e-24)
        return glm::dvec3(0.0);

    const double b = 2.0 * glm::dot(primary, secondary);
    const double c = glm::dot(primary, primary) - totalLimit * totalLimit;
    const double discriminant = std::max(0.0, b * b - 4.0 * a * c);
    const double positiveRoot =
        (-b + std::sqrt(discriminant)) / (2.0 * a);
    const double fraction = std::clamp(positiveRoot, 0.0, 1.0);
    return secondary * fraction;
}

inline glm::dvec3 updateNewtonianMainGimbal(
    DynamicMotionState& motion,
    const ShipParams& params,
    double dt,
    const glm::dvec3& forward,
    const glm::dvec3& right,
    const glm::dvec3& up
)
{
    const double yawLimit =
        game::ship::mainGimbalYawLimitRad(params);
    const double pitchLimit =
        game::ship::mainGimbalPitchLimitRad(params);
    const double slew =
        game::ship::mainGimbalRateRadPerSec(params);

    double rightCommand =
        std::clamp(motion.mainGimbalRightCommand01, -1.0, 1.0);
    double upCommand =
        std::clamp(motion.mainGimbalUpCommand01, -1.0, 1.0);

    // Treat pitch/yaw travel as one elliptical gimbal envelope. With equal
    // axis limits this is a true cone, so diagonal commands cannot cheat a
    // 7-degree nozzle into ~10 degrees of resultant deflection.
    const double commandLength =
        std::sqrt(
            rightCommand * rightCommand +
            upCommand * upCommand
        );
    if (commandLength > 1.0)
    {
        rightCommand /= commandLength;
        upCommand /= commandLength;
    }

    const double targetYaw = rightCommand * yawLimit;
    const double targetPitch = upCommand * pitchLimit;
    const double maxStep = slew * std::max(0.0, dt);

    // Slew is a resultant nozzle-axis rate, not an independent free rate on
    // both servos. This prevents a diagonal pitch+yaw move from exceeding the
    // physical angular speed by sqrt(2).
    double deltaYaw =
        targetYaw - motion.mainGimbalYawRad;
    double deltaPitch =
        targetPitch - motion.mainGimbalPitchRad;
    const double deltaLength =
        std::sqrt(
            deltaYaw * deltaYaw +
            deltaPitch * deltaPitch
        );

    if (deltaLength <= maxStep || deltaLength <= 1.0e-15)
    {
        motion.mainGimbalYawRad = targetYaw;
        motion.mainGimbalPitchRad = targetPitch;
    }
    else if (maxStep > 0.0)
    {
        const double scale = maxStep / deltaLength;
        motion.mainGimbalYawRad += deltaYaw * scale;
        motion.mainGimbalPitchRad += deltaPitch * scale;
    }

    if (yawLimit <= 1.0e-12 && pitchLimit <= 1.0e-12)
        return forward;

    const glm::dvec3 direction =
        forward +
        right * std::tan(motion.mainGimbalYawRad) +
        up * std::tan(motion.mainGimbalPitchRad);

    const double length = glm::length(direction);
    return
        std::isfinite(length) && length > 1.0e-12
            ? direction / length
            : forward;
}

} // namespace

glm::dvec3 DynamicMotionSystem::limitRequestedPropulsionToControlledSpeed(
    const glm::dvec3& currentVelocity,
    const glm::dvec3& requestedAcceleration,
    double controlledSpeedLimit,
    double dt
)
{
    if (dt <= 0.0 || controlledSpeedLimit <= 0.0)
        return glm::dvec3(0.0);

    const glm::dvec3 requestedDeltaV = requestedAcceleration * dt;
    const double currentSpeed = glm::length(currentVelocity);
    const double allowedRadius = std::max(controlledSpeedLimit, currentSpeed);
    const glm::dvec3 candidateVelocity =
        currentVelocity + requestedDeltaV;

    if (glm::length(candidateVelocity) <= allowedRadius + 1.0e-12)
        return requestedAcceleration;

    // Clip only the ENGINE-produced delta-V to the current admissible speed
    // sphere. The physical velocity itself is never clamped. If an external
    // impulse has already pushed the craft above the normal control envelope,
    // propulsion may reduce that overspeed but may not make its magnitude
    // larger. This keeps collision/explosion impulses physically observable.
    const double a = glm::dot(requestedDeltaV, requestedDeltaV);
    if (a <= 1.0e-24)
        return glm::dvec3(0.0);

    const double b = 2.0 * glm::dot(currentVelocity, requestedDeltaV);
    const double c =
        glm::dot(currentVelocity, currentVelocity) -
        allowedRadius * allowedRadius;
    const double discriminant = std::max(0.0, b * b - 4.0 * a * c);
    const double root = (-b + std::sqrt(discriminant)) / (2.0 * a);
    const double fraction = std::clamp(root, 0.0, 1.0);

    return requestedAcceleration * fraction;
}

void DynamicMotionSystem::applySystemAccelerationDemand(
    DynamicMotionState& motion,
    const ShipParams& params,
    const glm::dvec3& linearAccelerationDemandSystemMps2,
    const glm::vec3& shipForward
)
{
    const bool finiteDemand =
        std::isfinite(linearAccelerationDemandSystemMps2.x) &&
        std::isfinite(linearAccelerationDemandSystemMps2.y) &&
        std::isfinite(linearAccelerationDemandSystemMps2.z);

    const glm::dvec3 shipForwardD(shipForward);
    if (!finiteDemand || glm::dot(shipForwardD, shipForwardD) <= 1.0e-18)
    {
        motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        return;
    }

    const glm::dvec3 forward =
        glm::normalize(glm::dvec3(shipForward));

    // Navigation/program acceleration demand owns the complete acceleration
    // sample. Never let a previous manual Assisted stabilizer demand leak into
    // the server/autopilot path.
    motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);

    const double forwardMainAuthority =
        game::ship::forwardMainAccelerationLimitMps2(params);
    const double reverseMainAuthority =
        game::ship::reverseMainAccelerationLimitMps2(params);
    const double manoeuvreAuthority =
        game::ship::manoeuvreAccelerationLimitMps2(params);

    // Installed propulsion is a ShipParams hardware fact, not a control-law
    // convention. Positive longitudinal demand may use a real rear/aft main
    // engine. Negative demand may use a real fore/reverse main engine only
    // when the vehicle profile says that actuator exists.
    const double requestedForward =
        glm::dot(linearAccelerationDemandSystemMps2, forward);

    const double mainLongitudinal =
        std::clamp(
            requestedForward,
            -reverseMainAuthority,
            forwardMainAuthority
        );

    motion.mainEngineAccelerationMps2 =
        forward * mainLongitudinal;

    const glm::dvec3 remainder =
        linearAccelerationDemandSystemMps2 -
        motion.mainEngineAccelerationMps2;

    motion.manoeuvreAccelerationMps2 =
        clampSecondaryToTotalAccelerationEnvelope(
            motion.mainEngineAccelerationMps2,
            clampMagnitude(remainder, manoeuvreAuthority),
            game::ship::mainAccelerationLimitMps2(params)
        );

    motion.engineAccelerationMps2 =
        motion.mainEngineAccelerationMps2 +
        motion.manoeuvreAccelerationMps2;

}


void DynamicMotionSystem::applyNavigationAssistedFlightModel(
    DynamicMotionState& motion,
    const KinematicFrame& frame,
    const ShipParams& params,
    float dt,
    const glm::dvec3& targetVelocitySystemMps,
    const glm::dvec3& executedAccelerationDemandSystemMps2,
    const glm::vec3& shipForward,
    const glm::vec3& shipRight,
    const glm::vec3& shipUp
)
{
    if (motion.localControlLaw != LocalFlightControlLaw::Assisted ||
        !frame.valid ||
        dt <= 0.0f ||
        !std::isfinite(targetVelocitySystemMps.x) ||
        !std::isfinite(targetVelocitySystemMps.y) ||
        !std::isfinite(targetVelocitySystemMps.z) ||
        !std::isfinite(executedAccelerationDemandSystemMps2.x) ||
        !std::isfinite(executedAccelerationDemandSystemMps2.y) ||
        !std::isfinite(executedAccelerationDemandSystemMps2.z))
    {
        motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        return;
    }

    const double maxSpeed =
        game::ship::controlledSpeedLimitMps(params);
    const glm::dvec3 hullForward =
        glm::normalize(glm::dvec3(shipForward));
    const glm::dvec3 forward =
        params.assistedWorkingThrustAxisEmergency
            ? glm::normalize(
                glm::dvec3(shipRight) *
                    params.assistedWorkingThrustAxisBody.x +
                glm::dvec3(shipUp) *
                    params.assistedWorkingThrustAxisBody.y -
                hullForward *
                    params.assistedWorkingThrustAxisBody.z
              )
            : hullForward;
    // A damaged ship MUST first rotate its hull until its reachable working
    // thrust axis points along the planned velocity course. Forcing the
    // ordinary nose-based lateral stabilizer during that transition spends
    // propulsion authority trying to "correct" a deliberate hull offset.
    // While attitude is acquiring the new propulsion heading, keep the
    // existing world velocity ballistic; attitude motors remain active.
    const glm::dvec3 presentVelocity =
        frame.localToWorldVector(motion.localVelocityMps);
    const double targetSpeed = glm::length(targetVelocitySystemMps);
    if (params.assistedWorkingThrustAxisEmergency &&
        glm::length(presentVelocity)>1.0 &&
        targetSpeed>1.0 &&
        glm::dot(forward,targetVelocitySystemMps/targetSpeed) <
            static_cast<double>(params.brakeAlignmentCosine))
    {
        motion.mainEngineAccelerationMps2 = {};
        motion.assistedStabilizationAccelerationMps2 = {};
        motion.manoeuvreAccelerationMps2 = {};
        motion.engineAccelerationMps2 = {};
        motion.desiredTacticalVelocityMps = presentVelocity;
        return;
    }

    const double targetForwardSpeedMps = std::max(
        0.0, glm::dot(targetVelocitySystemMps, forward)
    );
    const double responseGain =
        static_cast<double>(params.throttleAccel) > 0.0
            ? static_cast<double>(params.throttleAccel)
            : static_cast<double>(
                  params.fallbackThrottleResponsePerSecond
              );
    const double requestedForwardAcceleration = glm::dot(
        executedAccelerationDemandSystemMps2, forward
    );
    const double executableForwardAcceleration =
        std::isfinite(requestedForwardAcceleration)
            ? std::clamp(
                  requestedForwardAcceleration,
                  -game::ship::reverseMainAccelerationLimitMps2(params),
                  game::ship::forwardMainAccelerationLimitMps2(params)
              )
            : 0.0;
    // Assisted is a first-order speed controller: a = gain * (target - v).
    // Asking it for the reference speed alone guarantees a lag of a/gain
    // during every planned burn. Feed the already pilot-executed acceleration
    // into the setpoint; the real main engine still clips acceleration below.
    const double compensatedTargetSpeedMps =
        targetForwardSpeedMps +
        (responseGain > 0.0
            ? executableForwardAcceleration / responseGain
            : 0.0);
    (void)LocalFlightControlStateMachine::requestAssistedTargetSpeed(
        motion,
        compensatedTargetSpeedMps,
        maxSpeed
    );

    // Reuse the exact manual Assisted law. With zero keypad inputs this owns
    // the same nose-coupled velocity response, main-engine allocation and
    // automatic lateral stabilization accepted for player control.
    applyLocalFrameInput(
        motion,
        frame,
        params,
        dt,
        0.0f,
        false,
        0.0f,
        0.0f,
        0.0f,
        shipForward,
        shipRight,
        shipUp
    );

    // The ordinary Assisted law already damps measured lateral velocity
    // toward zero. Shift that physical equilibrium toward the requested
    // lateral velocity; leave the response and authority limit to the ship.
    // This is Assisted stabilization, never a keypad or manoeuvre RCS burn.
    const glm::dvec3 f = forward;
    const glm::dvec3 lateralTarget =
        targetVelocitySystemMps - f * targetForwardSpeedMps;
    const double lateralGain = std::max(
        0.0, static_cast<double>(params.strafeDamping)
    );
    const glm::dvec3 lateralFeedback = lateralTarget * lateralGain;

    const double assistedAuthority =
        game::ship::assistedLateralStabilizationAccelerationLimitMps2(params);
    const double totalLinearEnvelope =
        game::ship::mainAccelerationLimitMps2(params);

    // The trajectory budgets turn acceleration. Nose-coupled stabilization
    // alone has to accumulate a sideways velocity error before producing it.
    // Apply the pilot-executed lateral demand as feed-forward, subject to the
    // same physical stabilization and shared load limits as manual Assisted.
    const glm::dvec3 lateralFeedForward =
        executedAccelerationDemandSystemMps2 -
        f * requestedForwardAcceleration;

    motion.assistedStabilizationAccelerationMps2 =
        clampMagnitude(
            motion.assistedStabilizationAccelerationMps2 +
                lateralFeedForward + lateralFeedback,
            assistedAuthority
        );

    // Assisted doctrine gives velocity-to-nose correction priority. If the
    // stabilizer needs more of the shared load envelope, reduce main thrust;
    // never spill the correction into the precision RCS system.
    motion.mainEngineAccelerationMps2 =
        clampSecondaryToTotalAccelerationEnvelope(
            motion.assistedStabilizationAccelerationMps2,
            motion.mainEngineAccelerationMps2,
            totalLinearEnvelope
        );
    motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
    motion.engineAccelerationMps2 =
        motion.mainEngineAccelerationMps2 +
        motion.assistedStabilizationAccelerationMps2;
}

void DynamicMotionSystem::applyNavigationPrecisionVelocityTrim(
    DynamicMotionState& motion,
    const KinematicFrame& frame,
    const ShipParams& params,
    float dt,
    const glm::dvec3& targetVelocitySystemMps
)
{
    motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
    motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);

    if (!frame.valid ||
        dt <= 0.0f ||
        !std::isfinite(targetVelocitySystemMps.x) ||
        !std::isfinite(targetVelocitySystemMps.y) ||
        !std::isfinite(targetVelocitySystemMps.z))
    {
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        return;
    }

    const glm::dvec3 targetVelocityLocalMps =
        frame.worldToLocalVector(targetVelocitySystemMps);
    const glm::dvec3 velocityErrorLocalMps =
        targetVelocityLocalMps - motion.localVelocityMps;

    const double manoeuvreAuthority =
        game::ship::manoeuvreAccelerationLimitMps2(params);
    const glm::dvec3 requiredLocalAcceleration =
        velocityErrorLocalMps / static_cast<double>(dt);
    const glm::dvec3 commandedLocalAcceleration =
        clampMagnitude(
            requiredLocalAcceleration,
            manoeuvreAuthority
        );

    motion.manoeuvreAccelerationMps2 =
        frame.localToWorldVector(commandedLocalAcceleration);
    motion.engineAccelerationMps2 =
        motion.manoeuvreAccelerationMps2;
}

void DynamicMotionSystem::updateLocalFrameMotion(
    DynamicMotionState& motion,
    world::coordinates::WorldPosition& worldPosition,
    const KinematicFrame& frame,
    const ShipParams& params,
    double dt
)
{
    if (dt <= 0.0)
        return;

    // The main/Assisted and RCS channels contain executed acceleration after
    // the physical allocator. Its requests were limited BEFORE the motors
    // calculated their actual forces: never discard real motor delta-v here.
    // Legacy diagnostic motion without a physical plant retains its original
    // speed-envelope behavior. Newtonian RCS remains independent of the
    // main-engine speed envelope in both execution paths.
    const glm::dvec3 requestedMainLocalAcceleration =
        frame.worldToLocalVector(motion.mainEngineAccelerationMps2);

    glm::dvec3 actualManoeuvreWorldAcceleration =
        motion.manoeuvreAccelerationMps2;
    glm::dvec3 actualAssistedStabilizationWorldAcceleration =
        motion.assistedStabilizationAccelerationMps2;

    const double manoeuvreAuthority =
        game::ship::manoeuvreAccelerationLimitMps2(params);
    const double assistedStabilizationAuthority =
        game::ship::assistedLateralStabilizationAccelerationLimitMps2(params);

    actualAssistedStabilizationWorldAcceleration =
        clampMagnitude(
            actualAssistedStabilizationWorldAcceleration,
            assistedStabilizationAuthority
        );
    const double gasUsePerSecond = std::max(
        0.0,
        static_cast<double>(params.manoeuvreGasUsePerSecond)
    );
    const double gasRechargePerSecond = std::max(
        0.0,
        static_cast<double>(params.manoeuvreGasRechargePerSecond)
    );
    const double restartFraction = std::clamp(
        static_cast<double>(params.manoeuvreGasRestartFraction),
        0.0,
        1.0
    );

    motion.manoeuvreGasPressure01 = std::clamp(
        motion.manoeuvreGasPressure01,
        0.0,
        1.0
    );

    const double requestedManoeuvreMagnitude =
        glm::length(actualManoeuvreWorldAcceleration);
    const double normalizedManoeuvreUse = manoeuvreAuthority > 1.0e-12
        ? requestedManoeuvreMagnitude / manoeuvreAuthority
        : 0.0;

    if (motion.manoeuvreGasDepleted)
    {
        actualManoeuvreWorldAcceleration = glm::dvec3(0.0);
        motion.manoeuvreGasPressure01 = std::min(
            1.0,
            motion.manoeuvreGasPressure01 + gasRechargePerSecond * dt
        );

        if (motion.manoeuvreGasPressure01 >= restartFraction - 1.0e-12)
            motion.manoeuvreGasDepleted = false;
    }
    else
    {
        const double pressureDelta =
            gasRechargePerSecond -
            gasUsePerSecond * normalizedManoeuvreUse;

        motion.manoeuvreGasPressure01 = std::clamp(
            motion.manoeuvreGasPressure01 + pressureDelta * dt,
            0.0,
            1.0
        );

        if (normalizedManoeuvreUse > 1.0e-12 &&
            motion.manoeuvreGasPressure01 <= 1.0e-12)
        {
            motion.manoeuvreGasPressure01 = 0.0;
            motion.manoeuvreGasDepleted = true;
            actualManoeuvreWorldAcceleration = glm::dvec3(0.0);
        }
    }

    const glm::dvec3 actualManoeuvreLocalAcceleration =
        frame.worldToLocalVector(actualManoeuvreWorldAcceleration);

    if (motion.localControlLaw == LocalFlightControlLaw::Newtonian)
    {
        motion.newtonianManualManoeuvreDeltaVelocityMps +=
            actualManoeuvreLocalAcceleration * dt;

        const double ledgerMagnitude =
            glm::length(
                motion.newtonianManualManoeuvreDeltaVelocityMps
            );

        // Prevent tiny numerical sign flips from causing perpetual corrective
        // chatter around zero after a released manoeuvre.
        if (!motion.newtonianManualManoeuvreInputActive &&
            ledgerMagnitude <=
                std::max(
                    1.0e-6,
                    manoeuvreAuthority * dt * 1.0e-3
                ))
        {
            motion.newtonianManualManoeuvreDeltaVelocityMps =
                glm::dvec3(0.0);
        }
    }
    else
    {
        motion.newtonianManualManoeuvreDeltaVelocityMps =
            glm::dvec3(0.0);
        motion.newtonianManualManoeuvreInputActive = false;
    }

    const glm::dvec3 actualAssistedStabilizationLocalAcceleration =
        frame.worldToLocalVector(
            actualAssistedStabilizationWorldAcceleration
        );

    glm::dvec3 localAcceleration(0.0);
    if (motion.installedMainPlantExecuted)
    {
        // This is actual ΣF/m, including any asymmetry, gimbal reaction and
        // reactor throttling. Applying the speed envelope here again would
        // create an unreported impulse different from delivered motor force.
        localAcceleration =
            requestedMainLocalAcceleration +
            actualManoeuvreLocalAcceleration +
            actualAssistedStabilizationLocalAcceleration;
    }
    else if (motion.localControlLaw == LocalFlightControlLaw::Newtonian)
    {
        const glm::dvec3 controlledMainAcceleration =
            limitRequestedPropulsionToControlledSpeed(
                motion.localVelocityMps,
                requestedMainLocalAcceleration,
                game::ship::controlledSpeedLimitMps(params),
                dt
            );

        // Diagnostic/legacy path only. RCS is not a second main engine and
        // can still add small physical delta-v beyond maxCombatSpeed.
        localAcceleration =
            controlledMainAcceleration + actualManoeuvreLocalAcceleration;
    }
    else
    {
        localAcceleration = limitRequestedPropulsionToControlledSpeed(
            motion.localVelocityMps,
            requestedMainLocalAcceleration +
                actualManoeuvreLocalAcceleration +
                actualAssistedStabilizationLocalAcceleration,
            game::ship::controlledSpeedLimitMps(params),
            dt
        );
    }

    motion.engineAccelerationMps2 =
        frame.localToWorldVector(localAcceleration);

    motion.localVelocityMps += localAcceleration * dt;
    // One tick's installed-plant execution never authorizes the next tick.
    motion.installedMainPlantExecuted = false;

    if (motion.velocityAlignmentMode ==
            VelocityAlignmentMode::BrakeToStop &&
        glm::length(motion.localVelocityMps) <= static_cast<double>(params.stopSpeedEpsilonMps))
    {
        motion.localVelocityMps = glm::dvec3(0.0);
        motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        (void)LocalFlightControlStateMachine::completeVelocityAlignment(motion);
    }

    motion.localPositionMeters += motion.localVelocityMps * dt;

    const glm::dvec3 worldMeters =
        frame.localToWorldPosition(motion.localPositionMeters);

    motion.referenceVelocityMps =
        frame.localToWorldVelocity(
            motion.localPositionMeters,
            glm::dvec3(0.0)
        );

    motion.worldVelocityMps =
        frame.localToWorldVelocity(
            motion.localPositionMeters,
            motion.localVelocityMps
        );

    worldPosition =
        world::coordinates::makeWorldPositionFromMeters(worldMeters);
}

void DynamicMotionSystem::applyLocalFrameInput(
    DynamicMotionState& motion,
    const KinematicFrame& frame,
    const ShipParams& params,
    float dt,
    float targetSpeedRate,
    bool cruiseActive,
    float forwardInput,
    float liftInput,
    float strafeInput,
    const glm::vec3& shipForward,
    const glm::vec3& shipRight,
    const glm::vec3& shipUp
)
{
    const double dtD = std::max(0.0, static_cast<double>(dt));
    const double maxSpeed =
        game::ship::controlledSpeedLimitMps(params);
    const double forwardMainAccel =
        game::ship::forwardMainAccelerationLimitMps2(params);
    const double reverseMainAccel =
        game::ship::reverseMainAccelerationLimitMps2(params);
    const double manoeuvreAccel =
        game::ship::manoeuvreAccelerationLimitMps2(params);
    const double assistedLateralStabilizationAccel =
        game::ship::assistedLateralStabilizationAccelerationLimitMps2(params);

    const glm::dvec3 f = glm::normalize(glm::dvec3(shipForward));
    const glm::dvec3 r = glm::normalize(glm::dvec3(shipRight));
    const glm::dvec3 u = glm::normalize(glm::dvec3(shipUp));

    // Assisted normally treats the nose as the travel-forward direction. If
    // the aft/rear main bank is dead but the fore bank survives, the ship can
    // continue under full main power by treating -nose as its working travel
    // direction. This is a controller doctrine choice over real hardware, not
    // a change to the hull coordinate system.
    const bool assistedUsesForeMainAsPrimary =
        motion.localControlLaw == LocalFlightControlLaw::Assisted &&
        forwardMainAccel <= 1.0e-9 &&
        reverseMainAccel > 1.0e-9;
    const glm::dvec3 assistedTravelForward =
        assistedUsesForeMainAsPrimary
            ? -f
            : (params.assistedWorkingThrustAxisEmergency
                ? glm::normalize(
                    r*params.assistedWorkingThrustAxisBody.x +
                    u*params.assistedWorkingThrustAxisBody.y -
                    f*params.assistedWorkingThrustAxisBody.z
                  )
                : f);

    motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
    motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
    motion.assistedStabilizationAccelerationMps2 = glm::dvec3(0.0);

    if (cruiseActive)
    {
        // J/cruise owns a different propulsion layer. Local RCS is suppressed
        // by the input mapper and explicitly cleared here as a second guard.
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
        motion.newtonianManualManoeuvreDeltaVelocityMps = glm::dvec3(0.0);
        motion.newtonianManualManoeuvreInputActive = false;
        return;
    }

    const glm::dvec3 relativeWorldVelocity =
        frame.localToWorldVector(motion.localVelocityMps);

    motion.forwardSpeedMps = glm::dot(relativeWorldVelocity, f);
    motion.strafeSpeedMps = glm::dot(relativeWorldVelocity, r);
    motion.liftSpeedMps = glm::dot(relativeWorldVelocity, u);

    if (motion.localControlLaw == LocalFlightControlLaw::Newtonian)
    {
        // Newtonian owns real independent RCS clusters. Their body-axis force
        // remains a separate actuator from the aft main/TVC system.
        const bool manualManoeuvreInput =
            std::abs(static_cast<double>(forwardInput)) > 1.0e-6 ||
            std::abs(static_cast<double>(strafeInput)) > 1.0e-6 ||
            std::abs(static_cast<double>(liftInput)) > 1.0e-6;

        motion.newtonianManualManoeuvreInputActive =
            manualManoeuvreInput;

        if (manualManoeuvreInput)
        {
            motion.manoeuvreAccelerationMps2 =
                f * (static_cast<double>(forwardInput) * manoeuvreAccel) +
                r * (static_cast<double>(strafeInput) * manoeuvreAccel) +
                u * (static_cast<double>(liftInput) * manoeuvreAccel);
        }
        else if (dtD > 0.0 &&
                 glm::length(
                     motion.newtonianManualManoeuvreDeltaVelocityMps
                 ) > 1.0e-9)
        {
            // Release doctrine for manual Newtonian RCS:
            // cancel only the delta-v previously created by those RCS
            // thrusters. Do not chase total VREL and do not touch main-engine
            // or externally induced velocity.
            const glm::dvec3 requiredLocalAcceleration =
                -motion.newtonianManualManoeuvreDeltaVelocityMps / dtD;
            const glm::dvec3 commandedLocalAcceleration =
                clampMagnitude(
                    requiredLocalAcceleration,
                    manoeuvreAccel
                );
            motion.manoeuvreAccelerationMps2 =
                frame.localToWorldVector(
                    commandedLocalAcceleration
                );
        }
        // Newtonian main thrust follows the rate-limited physical nozzle axis.
        // RCS remains a separate small body-axis actuator; TVC is never folded
        // into manoeuvreAccelerationMps2.
        const glm::dvec3 mainThrustDirection =
            updateNewtonianMainGimbal(
                motion,
                params,
                dtD,
                f,
                r,
                u
            );

        // END: rotate tail-to-velocity via ShipController, then use bounded
        // main-engine deceleration. RCS remains an independent body-axis force.
        if (motion.velocityAlignmentMode ==
                VelocityAlignmentMode::BrakeToStop)
        {
            const double speed = glm::length(relativeWorldVelocity);
            if (speed <= static_cast<double>(params.stopSpeedEpsilonMps) || dtD <= 0.0)
            {
                motion.localVelocityMps = glm::dvec3(0.0);
                motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
                motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
                motion.engineAccelerationMps2 = glm::dvec3(0.0);
                (void)LocalFlightControlStateMachine::completeVelocityAlignment(motion);
                motion.targetForwardSpeedMps = 0.0;
                return;
            }

            const glm::dvec3 antiVelocity = -relativeWorldVelocity / speed;
            const glm::dvec3 velocityDirection =
                relativeWorldVelocity / speed;

            if (forwardMainAccel > 1.0e-9)
            {
                // Normal Newtonian doctrine: turn the nose anti-velocity so the
                // aft/rear main bank produces the braking acceleration.
                if (glm::dot(mainThrustDirection, antiVelocity) >=
                    static_cast<double>(params.brakeAlignmentCosine))
                {
                    const double brakeAccel =
                        std::min(forwardMainAccel, speed / dtD);
                    motion.mainEngineAccelerationMps2 =
                        mainThrustDirection * brakeAccel;
                }
            }
            else if (reverseMainAccel > 1.0e-9)
            {
                // Aft bank is dead. ShipController points the nose WITH the
                // velocity vector; the surviving fore/nose bank then thrusts
                // opposite the nose and supplies the required braking dv.
                if (glm::dot(mainThrustDirection, velocityDirection) >=
                    static_cast<double>(params.brakeAlignmentCosine))
                {
                    const double brakeAccel =
                        std::min(reverseMainAccel, speed / dtD);
                    motion.mainEngineAccelerationMps2 =
                        -mainThrustDirection * brakeAccel;
                }
            }

            motion.engineAccelerationMps2 =
                motion.mainEngineAccelerationMps2 +
                motion.manoeuvreAccelerationMps2;
            motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
            return;
        }

        // Newtonian '+' addresses the current PRIMARY longitudinal main bank.
        // The aft/rear bank remains primary while alive. If it is lost but the
        // fore/nose bank survives, '+' deliberately drives the ship in the
        // opposite hull direction (-nose). '-' remains a no-op: it is not a
        // synthetic bidirectional throttle and does not invent reverse hardware.
        const double mainThrustCommand = std::clamp(
            static_cast<double>(targetSpeedRate),
            0.0,
            1.0
        );

        if (forwardMainAccel > 1.0e-9)
        {
            motion.mainEngineAccelerationMps2 =
                mainThrustDirection *
                (mainThrustCommand * forwardMainAccel);
        }
        else if (reverseMainAccel > 1.0e-9)
        {
            motion.mainEngineAccelerationMps2 =
                -mainThrustDirection *
                (mainThrustCommand * reverseMainAccel);
        }
        motion.engineAccelerationMps2 =
            motion.mainEngineAccelerationMps2 +
            motion.manoeuvreAccelerationMps2;

        motion.desiredTacticalVelocityMps = relativeWorldVelocity;
        motion.targetForwardSpeedMps = glm::length(motion.localVelocityMps);
        return;
    }

    // ---------------- Assisted / Elite-style law ----------------
    // Assisted has no dedicated ordinary RCS. Manual body-axis translation is
    // allocated to the same vectoring-main authority used by automatic
    // stabilization. The input semantics survive; the fake manoeuvre actuator
    // does not.
    motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);

    // Free-rotation mode and its bounded return-to-velocity alignment own NO
    // longitudinal cruise servo. Only keypad body translation is admitted.
    // Attitude input still reaches the installed main-engine torque allocator.
    if (motion.assistedFreeRotation ||
        motion.velocityAlignmentMode == VelocityAlignmentMode::ForwardToVelocity)
    {
        motion.targetForwardSpeedMps=glm::dot(relativeWorldVelocity,f);
        motion.assistedTargetSpeedHold=false;
        motion.assistedThrottleTrimWasActive=false;
        motion.desiredTacticalVelocityMps=relativeWorldVelocity;
        motion.mainEngineAccelerationMps2=clampMagnitude(
            f*(static_cast<double>(forwardInput)*assistedLateralStabilizationAccel)+
            r*(static_cast<double>(strafeInput)*assistedLateralStabilizationAccel)+
            u*(static_cast<double>(liftInput)*assistedLateralStabilizationAccel),
            assistedLateralStabilizationAccel);
        motion.manoeuvreAccelerationMps2=glm::dvec3(0.0);
        motion.assistedStabilizationAccelerationMps2=glm::dvec3(0.0);
        motion.engineAccelerationMps2=motion.mainEngineAccelerationMps2;
        return;
    }

    // With the fore/reverse bank destroyed, Assisted cannot pretend it has
    // an instantaneous reverse main. END/BrakeToStop uses the existing
    // physical flip-and-burn attitude controller: coast while it turns the
    // hull anti-velocity, then burn only when the aft thrust line is aligned.
    // Actual angular motion remains engine-controlled; velocity stays
    // ballistic throughout the turn.
    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop &&
        reverseMainAccel <= 1.0e-9 && forwardMainAccel > 1.0e-9)
    {
        const double speed = glm::length(relativeWorldVelocity);
        motion.desiredTacticalVelocityMps = glm::dvec3(0.0);
        if (speed <= static_cast<double>(params.stopSpeedEpsilonMps))
        {
            motion.localVelocityMps = glm::dvec3(0.0);
            (void)LocalFlightControlStateMachine::
                completeVelocityAlignment(motion);
            motion.targetForwardSpeedMps = 0.0;
            motion.engineAccelerationMps2 = glm::dvec3(0.0);
            return;
        }
        if (dtD > 0.0)
        {
            const glm::dvec3 opposite = -relativeWorldVelocity/speed;
            if (glm::dot(assistedTravelForward,opposite) >=
                static_cast<double>(params.brakeAlignmentCosine))
            {
                motion.mainEngineAccelerationMps2 =
                    assistedTravelForward *
                    std::min(forwardMainAccel,speed/dtD);
            }
        }
        motion.engineAccelerationMps2=motion.mainEngineAccelerationMps2;
        return;
    }

    const double targetSpeedChangeRate =
        std::max(
            static_cast<double>(
                params.assistedMinimumTargetSpeedChangeRateMps2
            ),
            maxSpeed *
                static_cast<double>(
                    params.assistedTargetSpeedChangeRateFractionPerSecond
                )
        );
    (void)LocalFlightControlStateMachine::
        updateAssistedLongitudinalTarget(
            motion,
            static_cast<double>(targetSpeedRate),
            targetSpeedChangeRate,
            dtD,
            glm::dot(relativeWorldVelocity, assistedTravelForward),
            maxSpeed
        );

    const glm::dvec3 desiredWorldVelocity =
        assistedTravelForward * motion.targetForwardSpeedMps;
    motion.desiredTacticalVelocityMps = desiredWorldVelocity;

    glm::dvec3 velocityError =
        desiredWorldVelocity - relativeWorldVelocity;

    auto yieldAxisToManualTranslation =
        [&](const glm::dvec3& axis, float input)
    {
        if (std::abs(static_cast<double>(input)) > 1.0e-6)
            velocityError -= axis * glm::dot(velocityError, axis);
    };

    yieldAxisToManualTranslation(f, forwardInput);
    yieldAxisToManualTranslation(r, strafeInput);
    yieldAxisToManualTranslation(u, liftInput);

    const double longitudinalResponse =
        positiveOr(
            static_cast<double>(params.throttleAccel),
            static_cast<double>(
                params.fallbackThrottleResponsePerSecond
            )
        );
    const double lateralResponse =
        positiveOr(
            static_cast<double>(params.strafeDamping),
            longitudinalResponse
        );
    const bool brakingToStop =
        motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop;

    // END must converge exactly. Ordinary Assisted flight instead uses the
    // authored longitudinal/lateral response gains and remains acceleration
    // bounded by the real vehicle envelope.
    const double longitudinalGain =
        brakingToStop && dtD > 0.0
            ? 1.0 / dtD
            : longitudinalResponse;
    const double lateralGain =
        brakingToStop && dtD > 0.0
            ? 1.0 / dtD
            : lateralResponse;

    // After aft-engine loss, "longitudinal" is the attainable CoM-aligned
    // working-thrust direction, not the hull's nose. Otherwise the ordinary
    // Assisted stabilizer immediately fights the intended safe off-axis burn.
    const double longitudinalVelocityError =
        glm::dot(velocityError, assistedTravelForward);
    const glm::dvec3 lateralVelocityError =
        velocityError -
            assistedTravelForward * longitudinalVelocityError;

    const double requestedLongitudinalAcceleration =
        longitudinalVelocityError * longitudinalGain;
    const double requestedMainLongitudinalAcceleration =
        std::clamp(
            requestedLongitudinalAcceleration,
            -reverseMainAccel,
            forwardMainAccel
        );

    const double totalLinearEnvelope =
        game::ship::mainAccelerationLimitMps2(params);

    // Assisted doctrine is direction-coupled: after the pilot turns the hull,
    // removing the old sideways VREL has priority over immediately rebuilding
    // longitudinal speed. The previous main-first allocator could consume the
    // complete load envelope along the new nose and leave zero authority for
    // lateral cancellation, making FA-on feel Newtonian for several seconds.
    //
    // Manual Assisted translation and automatic stabilization share the
    // vectoring-main budget. There is no hidden gas-limited RCS actuator in
    // this physical family.
    const glm::dvec3 manualAssistedVectoringAcceleration =
        clampMagnitude(
            f * (
                static_cast<double>(forwardInput) *
                assistedLateralStabilizationAccel
            ) +
            r * (
                static_cast<double>(strafeInput) *
                assistedLateralStabilizationAccel
            ) +
            u * (
                static_cast<double>(liftInput) *
                assistedLateralStabilizationAccel
            ),
            assistedLateralStabilizationAccel
        );

    const glm::dvec3 automaticAssistedStabilizationAcceleration =
        clampSecondaryToTotalAccelerationEnvelope(
            manualAssistedVectoringAcceleration,
            clampMagnitude(
                lateralVelocityError * lateralGain,
                assistedLateralStabilizationAccel
            ),
            totalLinearEnvelope
        );

    // The envelope helper returns only the admitted SECONDARY vector. Manual
    // translation is the primary vectoring-main request and must remain in the
    // executed actuator command; dropping it here was the exact reason
    // Assisted strafe/lift became zero when ghost-RCS was removed.
    motion.assistedStabilizationAccelerationMps2 =
        manualAssistedVectoringAcceleration +
        automaticAssistedStabilizationAcceleration;

    const glm::dvec3 requestedMainAcceleration =
        assistedTravelForward * requestedMainLongitudinalAcceleration;
    motion.mainEngineAccelerationMps2 =
        clampSecondaryToTotalAccelerationEnvelope(
            motion.assistedStabilizationAccelerationMps2,
            requestedMainAcceleration,
            totalLinearEnvelope
        );

    // Assisted has no independent manoeuvre/RCS thrusters. If the installed
    // fore/aft engines cannot supply a longitudinal correction, that part
    // of the request remains unserved; it must not turn into a second,
    // physically unallocated acceleration in the motion integrator.
    // Keypad translation and velocity stabilization already share the
    // real vectoring-main engine allocation above.
    motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
    motion.engineAccelerationMps2 =
        motion.mainEngineAccelerationMps2 +
        motion.assistedStabilizationAccelerationMps2;

    if (motion.velocityAlignmentMode == VelocityAlignmentMode::BrakeToStop &&
        glm::length(motion.localVelocityMps) <= static_cast<double>(params.stopSpeedEpsilonMps))
    {
        motion.localVelocityMps = glm::dvec3(0.0);
        motion.mainEngineAccelerationMps2 = glm::dvec3(0.0);
        motion.manoeuvreAccelerationMps2 = glm::dvec3(0.0);
        motion.engineAccelerationMps2 = glm::dvec3(0.0);
        (void)LocalFlightControlStateMachine::completeVelocityAlignment(motion);
    }
}

} // namespace game::navigation
