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

    
};
