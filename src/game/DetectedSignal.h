#pragma once

#include "WorldSignal.h"
// ----------------------debug----------------------
#include "render/SignalWave.h"
#include "render/VisualTuning.h"

// ----------------------debug----------------------

struct DetectedSignal
{
    const WorldSignal* source = nullptr;

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

    // ----------------------debug----------------------
    float waveCooldown = 0.0f;
    static constexpr int MaxWaves = VisualTuning::MaxWaves; 
    SignalWave waves[MaxWaves];
    // ----------------------debug----------------------
};
