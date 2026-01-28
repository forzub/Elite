#pragma once

#include <string>
#include <glm/vec3.hpp>

#include "world/types/SignalSemanticState.h"
#include "world/types/SignalDisplayClass.h"


enum class SignalType
{
    Planets,              // навигационный, стабильный
    StationClass,         // станции
    SOSModern,             // точный, но ограниченный
    SOSAntic,              // древний, направление только
    Beacon,                // стабильный маяк
    CivilianTransponder,
    MilitaryTransponder,
    PirateTransponder,
    Unknown                // аномалия, направление
};




struct WorldSignal
{
    SignalType          type;
    SignalDisplayClass  displayClass;
    glm::vec3           position;

    float               power;      // базовая мощность сигнала
    float               maxRange;   // максимальная дальность
   
    bool                enabled;
    std::string         label;
};



struct SignalReceptionResult
{
    const WorldSignal* source;   // <-- ВАЖНО
    
    // --- Геометрия
    glm::vec3 sourceWorldPos;
    float distance;                  // физическая дистанция

    // --- Сырой сигнал
    float emittedPower;              // мощность источника
    float receivedPower;             // после затухания, помех и экранирования

    // --- Шумы и помехи
    float noiseFloor;                // уровень шума приёмника
    float interferencePower;         // суммарные активные помехи

    // --- Экранирование
    float occlusionFactor;           // 0..1 (0 — полностью экранирован)

    // --- Итоговые метрики
    float signalToNoiseRatio;        // SNR
    float stability;                 // 0..1 (временная устойчивость)

    // --- Семантика
    SignalSemanticState semanticState;
};
