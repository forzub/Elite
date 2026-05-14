#pragma once
#include <glm/glm.hpp>
#include "src/game/equipment/radar/IRadarEffectsConfig.h"

namespace game {

// ============================================================================
// СТРУКТУРЫ ДЛЯ НАСТРОЙКИ LCD ЭФФЕКТОВ
// ============================================================================


// Главная структура всех LCD эффектов
struct PPILCDEffectsConfig : public IRadarEffectsConfig {
    // ------------------------------------------------------------------------
    // ПОСТОЯННЫЕ ЭФФЕКТЫ LCD
    // ------------------------------------------------------------------------
    
    struct {
        float intensity = 0.9f;        // Мерцание подсветки
        float frequency = 50.0f;       // 120 Гц
    } backlightFlicker;
    
    struct {
        float intensity = 0.9f;         // Задержка пикселей
    } pixelResponse;
    
    // ------------------------------------------------------------------------
    // ЭПИЗОДИЧЕСКИЕ ГЛЮКИ LCD
    // ------------------------------------------------------------------------
    
    struct GlitchState {
        // Управление от сервера
        bool active = false;
        
        // Текущее состояние
        float timer = 0.0f;
        float intensity = 0.0f;
        
        // Параметры случайности
        float minInterval = 3.0f;
        float maxInterval = 8.0f;
        float minDuration = 0.5f;
        float maxDuration = 2.0f;
        float minIntensity = 0.3f;
        float maxIntensity = 1.0f;
        
        // Специфические параметры для Cable Lines
        struct CableLineParams {
            float minPosition = 0.2f;
            float maxPosition = 0.8f;
            float width = 0.02f;
            glm::vec3 color = {1.0f, 0.0f, 0.0f};
        } cableLineParams;
        
        // Специфические параметры для Image Retention
        struct RetentionParams {
            float decaySpeed = 0.1f;
        } retentionParams;
        
        // Специфические параметры для Digital Noise
        struct DigitalNoiseParams {
            float amount = 10.2f;
        } noiseParams;
        
        // Специфические параметры для Tearing
        struct TearingParams {
            float maxOffset = 20.0f;
        } tearingParams;
        
        // Специфические параметры для Interference
        struct InterferenceParams {
            float speed = 5.0f;
            float amplitude = 0.6f;
        } interferenceParams;

        // ===== НОВЫЙ ГЛЮК: FREEZE (застывание) =====
        struct FreezeParams {
            int mode = 0;                    // 0=полный, 1=частичный, 2=полосы, 3=мерцающий
            float holdStrength = 0.8f;       // Насколько сильно держит предыдущий кадр
            float stripCount = 3.0f;         // Количество полос для режима 2
            float flickerSpeed = 5.0f;       // Скорость мерцания для режима 3
        } freezeParams;
        
        // Внутренние переменные
        float duration = 0.0f;
        float elapsed = 0.0f;
        
        // Для freeze нужно хранить предыдущий кадр
        unsigned int frozenFrame = 0;
    };

    GlitchState cableLines;
    GlitchState imageRetention;
    GlitchState digitalNoise;
    GlitchState lcdTearing;
    GlitchState interference;
    GlitchState freeze;           // ← НОВЫЙ ГЛЮК
    
    // Фазы
    float phase = 0.0f;
};

} // namespace game