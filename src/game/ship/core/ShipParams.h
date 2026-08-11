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
    float maxCombatSpeed;   // тактическая
    float maxCruiseSpeed;   // маршевый (игровая)
    float throttleAccel;
    
    // --- стабилизация ---
    float autoLevelStrength; // 0 = выкл
    
    // манёвровые двигатели
    float strafeAccel;      // ускорение манёвровых
    float strafeDamping;    // затухание манёвровых
    float maxStrafeSpeed;   // ограничение

    // Common local-flight acceleration envelope. For crewed craft this is
    // normally the lower of crew tolerance and structural limit. Uncrewed
    // drones may use a higher descriptor value, but still run the same motion
    // code and remain constrained by their structure/equipment profile.
    float maxGs          = 5.0f;
    // Characteristic lever arm used to translate angular rates/acceleration
    // into a local load envelope. Crewed craft normally tune maxGs for crew;
    // uncrewed craft can use their structural/equipment limit instead.
    float turnRadius     = 20.0f;
    
    
};

struct ShipPhysicsProfile
{
    ShipParams params;
    
};
