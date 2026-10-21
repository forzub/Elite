#pragma once

#include <glm/glm.hpp>

struct ShipParams
{
    // --- угловая динамика ---
    float maxPitchRate;
    float maxYawRate;
    float maxRollRate;
    
    // Aggregate rigid-body angular actuator authority. For the current
    // Cobra/navigation model this represents bounded main-nozzle vectoring plus
    // attitude-control authority; it is not free instantaneous reorientation.
    float angularAccel;     // насколько быстро РАЗГОНЯЕМСЯ
    float angularDamping;   // насколько быстро ГАСИМСЯ
   
    // --- линейное движение ---
    // Maximum speed that the ship may create with its own ordinary local
    // propulsion. This is a CONTROL/SAFETY envelope, not a hard physical
    // velocity clamp: collisions, explosions and other external impulses may
    // push the craft beyond it. Engines may then brake back inside the envelope.
    float maxCombatSpeed;
    float maxCruiseSpeed;   // маршевый (игровая)
    float throttleAccel;

    // Low-level local-flight/controller policy that affects deterministic
    // motion integration. These are generic vehicle/control inputs; the motion
    // system must not keep private behavior constants.
    float stopSpeedEpsilonMps = 0.05f;
    float brakeAlignmentCosine = 0.995f;
    float assistedMinimumTargetSpeedChangeRateMps2 = 50.0f;
    float assistedTargetSpeedChangeRateFractionPerSecond = 0.60f;
    float fallbackThrottleResponsePerSecond = 1.0f;

    // Installed ordinary longitudinal main propulsion.
    //
    // "Forward" authority is produced by aft/rear engines and accelerates
    // along the ship's +travel/forward axis. "Reverse" authority is produced
    // by fore/nose engines and accelerates opposite the ship forward axis.
    // Availability is the current effective hardware state; descriptor/runtime
    // adapters may clear it after damage. Acceleration values are installed
    // actuator ratings before the shared crew/structure load envelope is
    // applied. Zero keeps the legacy "use load envelope" compatibility path.
    bool forwardMainEngineAvailable = true;
    bool reverseMainEngineAvailable = false;
    float forwardMainEngineAccelerationMps2 = 0.0f;
    float reverseMainEngineAccelerationMps2 = 0.0f;

    // Installed main-nozzle thrust-vector-control (TVC) envelope.
    //
    // These are physical actuator limits, not autopilot tuning constants.
    // Zero means a fixed-axis main engine.  The current Newtonian proving
    // craft uses a deliberately modest real-world-like gimbal envelope.
    float mainGimbalPitchLimitRad = 0.0f;
    float mainGimbalYawLimitRad = 0.0f;
    float mainGimbalRateRadPerSec = 0.0f;
    
    // --- стабилизация ---
    float autoLevelStrength; // 0 = выкл
    
    // Манёвровые/RCS. Legacy strafe* fields are retained because the current
    // navigation envelope still consumes them; direct keypad thrusters use the
    // dedicated authority/resource values below.
    float strafeAccel;      // legacy/navigation lateral acceleration
    float strafeDamping;    // assisted lateral stabilization response
    float maxStrafeSpeed;   // legacy assisted translation envelope

    // Physical keypad manoeuvre thrusters. The same six body-axis jets are
    // available in both local control laws. Newtonian may accumulate their
    // delta-v without a controlled-speed cap; Assisted keeps the normal local
    // speed envelope and stabilization.
    float manoeuvreThrusterAccel = 0.0f;

    // Normalized high-pressure manoeuvre-gas accumulator. Full manual RCS use
    // drains faster than the pump can replenish it. A depleted accumulator is
    // latched off until pressure reaches the restart fraction, preventing a
    // held key from degenerating into frame-rate-scale chatter.
    float manoeuvreGasUsePerSecond = 0.0f;
    float manoeuvreGasRechargePerSecond = 0.0f;
    float manoeuvreGasRestartFraction = 0.20f;

    // Common local-flight acceleration envelope. For crewed craft this is
    // normally the lower of crew tolerance and structural limit. Uncrewed
    // drones may use a higher descriptor value, but still run the same motion
    // code and remain constrained by their structure/equipment profile.
    float maxGs          = 5.0f;

    // Optional linear-only acceleration envelope. Zero keeps legacy behavior
    // and falls back to maxGs. This lets a ship tune longitudinal dv/dt
    // without silently changing its angular/load envelope.
    float maxLinearGs    = 0.0f;

    // Characteristic lever arm used to translate angular rates/acceleration
    // into a local load envelope. Crewed craft normally tune maxGs for crew;
    // uncrewed craft can use their structural/equipment limit instead.
    float turnRadius     = 20.0f;

    // Baseline rigid-body mass properties used by collision impulse response.
    // Runtime cargo/damage may later provide effective values, but external
    // impulses must always update physical linear/angular velocity rather than
    // being folded into control-law limits. Principal axes are ship-local:
    // pitch = right/X, yaw = up/Y, roll = forward/Z.
    double massKg                = 1.0;

    // Explicit center of mass in canonical ship-local coordinates:
    // +X right, +Y up, -Z forward, +Z aft. Existing ships keep the previous
    // implicit behavior by defaulting to the local origin.
    glm::dvec3 centerOfMassLocalMeters {0.0};

    double pitchInertiaKgM2      = 1.0;
    double yawInertiaKgM2        = 1.0;
    double rollInertiaKgM2       = 1.0;

    // Appended for aggregate-initialization compatibility. Distinguishes an
    // explicitly authored "no manoeuvre thrusters" vehicle from legacy
    // descriptors that still use strafeAccel as the old RCS fallback.
    bool manoeuvreThrusterAuthorityExplicit = false;

    // Physical half-extents used to map normalized propulsion-slot positions
    // into metres: X=half width, Y=half height, Z=half length. Runtime
    // capability projection derives this from ShipDescriptor::logicalDimensions
    // when available. Zero keeps a conservative legacy fallback.
    glm::dvec3 physicalActuatorHalfExtentsMeters {0.0};

    // Damage-aware Assisted propulsion heading in canonical body coordinates
    // (+X right, +Y up, -Z nose). Ordinary healthy ships keep -Z.
    // Valid only when a real surviving aft drive can place its thrust line
    // through the live CoM within its physical nozzle cone.
    bool assistedWorkingThrustAxisEmergency = false;
    glm::dvec3 assistedWorkingThrustAxisBody {0.0, 0.0, -1.0};
    double assistedWorkingThrustAxisAngleRad = 0.0;
    // Exact installed aft actuator chosen for a validated emergency ray.
    // -1 means ordinary balanced bank operation (no forced single motor).
    int assistedWorkingThrustMotorIndex = -1;
    
    
};

struct ShipPhysicsProfile
{
    ShipParams params;
    
};
