#pragma once


struct ShipParams
{
    // --- угловая динамика ---
    float maxPitchRate;
    float maxYawRate;
    float maxRollRate;
    
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
    double pitchInertiaKgM2      = 1.0;
    double yawInertiaKgM2        = 1.0;
    double rollInertiaKgM2       = 1.0;
    
    
};

struct ShipPhysicsProfile
{
    ShipParams params;
    
};
