#pragma once

#include "world/types/SignalSemanticState.h"
#include "src/scene/EntityID.h"
#include "src/world/WorldSignal.h"
#include "src/world/coordinates/WorldPosition.h"

struct SignalReceptionResult
{
    const WorldSignal*              source;   // <-- ВАЖНО
    EntityId                        owner;
    // --- Геометрия
    world::coordinates::WorldPosition sourceWorldPosition;

    // Legacy local/source mirror for old HUD code.
    glm::vec3 sourceWorldPos {0.0f};
    
    float                           distance;                  // физическая дистанция

    // --- Сырой сигнал
    float                           emittedPower;              // мощность источника
    float                           receivedPower;             // после затухания, помех и экранирования

    // --- Шумы и помехи
    float                           noiseFloor;                // уровень шума приёмника
    float                           interferencePower;         // суммарные активные помехи

    // --- Экранирование
    float                           occlusionFactor;           // 0..1 (0 — полностью экранирован)

    // --- Итоговые метрики
    float                           signalToNoiseRatio;        // SNR
    float                           stability;                 // 0..1 (временная устойчивость)

    // --- Семантика
    SignalSemanticState             semanticState;
};