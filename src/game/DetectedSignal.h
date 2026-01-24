#pragma once

#include "WorldSignal.h"

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
};
