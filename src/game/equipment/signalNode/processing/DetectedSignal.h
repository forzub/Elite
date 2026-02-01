#pragma once

#include "world/WorldSignal.h"
#include "src/world/types/SignalReceptionResult.h"

#include "render/VisualTuning.h"




struct DetectedSignal
{
    const WorldSignal* source = nullptr;


    std::string label;
    float confidence = 0.0f;   // 0..1
    float strength   = 0.0f;

    bool hasDistance = false;  // ❗ важно
    bool hasDirection = false;

    glm::vec3 direction;       // нормализованный вектор

    bool confirmed() const
    {
        return confidence >= 1.0f;
    }

    float stability = 0.0f; // 0..1 — устойчивость приёма
    bool  visible   = false;
    float flickerTimer = 0.0f;
    float visibility = 0.0f; // визуальная видимость (fade)

    float nameDecisionTimer     = 0.0f;
    float distanceDecisionTimer = 0.0f;

    // ----------------------debug----------------------
    // float waveCooldown = 0.0f;
    // static constexpr int MaxWaves = VisualTuning::MaxWaves; 
    // SignalWave waves[MaxWaves];

    float detectTimer = 0.0f;
    // ----------------------debug----------------------
};
