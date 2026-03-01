#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "src/game/equipment/radar/IRadarVisualConfig.h"


// ============================================================================
// СТРУКТУРЫ ДЛЯ НАСТРОЕК ВНЕШНЕГО ВИДА РАДАРА
// ============================================================================

// Настройки фона радара
struct RadarBackgroundConfig {
    glm::vec4 color = {0.0f, 0.15f, 0.0f, 0.7f};
    float perspective = 0.30f;
};

// Настройки сетки (гратикулы)
struct RadarGraticuleConfig {
    float intensity = 0.9f;
    glm::vec4 color = {0.0f, 1.0f, 0.0f, 1.0f};
    float lineWidth = 2.0f;
    int numRings = 4;
    int numRays = 8;
};

// Настройки свипера (вращающегося луча)
struct RadarSweepConfig {
    glm::vec4 color = {0.0f, 1.0f, 0.0f, 0.1f};
    float lineWidth = 2.0f;
    float trailLengthDeg = 35.0f;
    int trailSteps = 30;
    float trailFadePower = 1.5f;
};

// Настройки меток (контактов)
struct RadarContactConfig {
    glm::vec4 color = {0.0f, 1.0f, 0.0f, 0.8f};
    float halfWidth = 5.0f;
    float boxSize = 4.0f;
    float radiusScale = 0.85f;
    float verticalScale = 0.3f;
};


struct PPIVisualConfig : public IRadarVisualConfig
{
    RadarBackgroundConfig   background;
    RadarGraticuleConfig    graticule;
    RadarSweepConfig        sweep;
    RadarContactConfig      contact;
    
    // текстуры
    const char* backgroundTexturePath;
    const char* overlayTexturePath;
};


    