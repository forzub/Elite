#pragma once

#include <string>

#include "world/types/SignalSemanticState.h"
#include "src/scene/EntityID.h"
#include "src/world/types/SignalDisplayClass.h"
#include "src/world/coordinates/WorldPosition.h"

/*
    Snapshot-safe description of one received signal.

    IMPORTANT:
    This structure crosses the simulation -> snapshot -> client boundary.
    It must therefore contain values / stable identifiers only. Never store
    pointers or references to WorldSignal here: receivers evaluate temporary
    per-system signal vectors whose elements cease to exist at the end of the
    server simulation tick.
*/
struct SignalReceptionResult
{
    EntityId                        owner;

    // Presentation metadata copied while the source WorldSignal is alive.
    // Keeping it by value makes the reception self-contained and serializable.
    SignalDisplayClass              sourceDisplayClass = SignalDisplayClass::Other;
    std::string                     sourceLabel;

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
