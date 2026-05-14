#pragma once
#include <glm/glm.hpp>
#include "src/game/equipment/radar/IRadarEffectsConfig.h"

namespace game {

// Главная структура всех CRT эффектов
struct PPICRTEffectsConfig : public IRadarEffectsConfig {
    // ------------------------------------------------------------------------
    // ПОСТОЯННЫЕ ЭФФЕКТЫ
    // ------------------------------------------------------------------------
    
    struct {
        float intensity = 0.6f;
        float frequency = 50.0f;
    } flicker;
    
    struct {
        float intensity = 0.5f;
        float speed = 1.2f;
    } breathing;
    
    struct {
        float intensity = 0.5f;
        float speed = 50.0f;
        float width = 0.15f;
    } constantHumBars;
    
    // ------------------------------------------------------------------------
    // ЭПИЗОДИЧЕСКИЕ ГЛЮКИ
    // ------------------------------------------------------------------------
    
    struct GlitchState {
        // Управление от сервера
        bool active = false;
        
        // Текущее состояние
        float timer = 0.0f;
        float intensity = 0.0f;
        
        // Параметры случайности
        float minInterval = 1.0f;
        float maxInterval = 2.0f;
        float minDuration = 5.0f;
        float maxDuration = 10.0f;
        float minIntensity = 0.3f;
        float maxIntensity = 1.0f;
        
        // Специфические параметры для Tearing
        struct TearingParams {
            float stripCount = 6.0f;
            float stripHeight = 0.04f;
            float maxShift = 20.0f;
            float chaosSpeed = 100.0f;
        } tearingParams;
        
        // Специфические параметры для HumBars
        struct HumBarsParams {
            float speed = 0.3f;
            float width = 0.03f;
        } humBarsParams;
        
        // Специфические параметры для Ghosting
        struct GhostingParams {
            float primaryOffset = 13.0f;
            float secondaryOffset = 25.0f;
            float tertiaryOffset = 45.0f;
            float primaryIntensity = 0.7f;
            float secondaryIntensity = 0.5f;
            float tertiaryIntensity = 0.1f;
        } ghostingParams;
        
        // Специфические параметры для VerticalRoll
        struct VerticalRollParams {
            float speed = 100.0f;
        } rollParams;
        
        // Внутренние переменные
        float duration = 0.0f;
        float elapsed = 0.0f;
    };

    GlitchState tearing;
    GlitchState episodicHumBars;
    GlitchState ghosting;
    GlitchState verticalRoll;
    
    // Фазы для анимации
    float crtPhase = 0.0f;
    float breathingPhase = 0.0f;
    float humBarPhase = 0.0f;
};

} // namespace game