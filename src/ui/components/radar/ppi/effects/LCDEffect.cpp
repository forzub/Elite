#include "LCDEffects.h"
#include <glad/gl.h>
#include <cstdlib>
#include <iostream>
#include "src/game/equipment/radar/config/PPILCDEffectsConfig.h"

LCDEffect::LCDEffect()
{
    // Инициализация при необходимости
}

void LCDEffect::update(float dt)
{
   
    m_data.phase += dt;
    
    // Обновляем все глюки
    updateGlitchState(m_data.cableLines, dt);
    updateGlitchState(m_data.imageRetention, dt);
    updateGlitchState(m_data.digitalNoise, dt);
    updateGlitchState(m_data.lcdTearing, dt);
    updateGlitchState(m_data.interference, dt);
}

void LCDEffect::updateGlitchState(game::PPILCDEffectsConfig::GlitchState& glitch, float dt)
{
    if (!glitch.active) {
        glitch.intensity = 0.0f;
        return;
    }
    
    // Если таймер еще не установлен - запускаем глюк сразу
    if (glitch.timer <= 0.0f && glitch.intensity == 0.0f) {
        glitch.duration = glitch.minDuration + 
            (rand() / (float)RAND_MAX) * 
            (glitch.maxDuration - glitch.minDuration);
        
        glitch.intensity = glitch.minIntensity + 
            (rand() / (float)RAND_MAX) * 
            (glitch.maxIntensity - glitch.minIntensity);
        
        glitch.elapsed = 0.0f;
        printf("LCD Glitch STARTED! intensity=%.2f\n", glitch.intensity);
    }
    
    // Обновление активного глюка
    if (glitch.intensity > 0.0f) {
        glitch.elapsed += dt;
        if (glitch.elapsed >= glitch.duration) {
            glitch.intensity = 0.0f;
            glitch.timer = glitch.minInterval + 
                (rand() / (float)RAND_MAX) * 
                (glitch.maxInterval - glitch.minInterval);
            printf("LCD Glitch ENDED\n");
        }
    } else {
        glitch.timer -= dt;
    }
}

void LCDEffect::applyToShader(unsigned int shaderProgram, float time,
                              float centerX, float centerY, 
                              float radiusX, float radiusY)
{   
    // Постоянные эффекты
    glUniform1f(glGetUniformLocation(shaderProgram, "backlightFlickerIntensity"), 
                m_data.backlightFlicker.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "pixelResponseIntensity"), 
                m_data.pixelResponse.intensity);
    
    // Cable Lines - используем настройки из структуры
    glUniform1f(glGetUniformLocation(shaderProgram, "cableLineIntensity"), 
                m_data.cableLines.intensity);
    
    // Параметры Cable Lines из структуры
    glUniform1f(glGetUniformLocation(shaderProgram, "cableLineWidth"), 
                m_data.cableLines.cableLineParams.width);
    glUniform3f(glGetUniformLocation(shaderProgram, "cableLineColor"), 
                m_data.cableLines.cableLineParams.color.r,
                m_data.cableLines.cableLineParams.color.g,
                m_data.cableLines.cableLineParams.color.b);
    
    // ===== TEARING =====
    glUniform1f(glGetUniformLocation(shaderProgram, "lcdTearingIntensity"), 
                m_data.lcdTearing.intensity);
    
    // Генерируем позиции разрывов (это остается, т.к. это анимация)
    float t = time * 0.1;
    glUniform1f(glGetUniformLocation(shaderProgram, "tearingLine1"), 0.3 + 0.1 * sin(t));
    glUniform1f(glGetUniformLocation(shaderProgram, "tearingLine2"), 0.6 + 0.1 * cos(t * 1.3));
    glUniform1f(glGetUniformLocation(shaderProgram, "tearingLine3"), 0.8 + 0.1 * sin(t * 0.7));
    
    // Используем maxOffset из структуры
    glUniform1f(glGetUniformLocation(shaderProgram, "tearingShift1"), 
                m_data.lcdTearing.tearingParams.maxOffset * 1.5f);
    glUniform1f(glGetUniformLocation(shaderProgram, "tearingShift2"), 
                m_data.lcdTearing.tearingParams.maxOffset * 0.75f);
    glUniform1f(glGetUniformLocation(shaderProgram, "tearingShift3"), 
                m_data.lcdTearing.tearingParams.maxOffset * 2.25f);
    
    // Interference
    glUniform1f(glGetUniformLocation(shaderProgram, "interferenceIntensity"), 
                m_data.interference.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "interferenceSpeed"), 
                m_data.interference.interferenceParams.speed);
    glUniform1f(glGetUniformLocation(shaderProgram, "interferenceAmplitude"), 
                m_data.interference.interferenceParams.amplitude);
    
    // Digital Noise
    glUniform1f(glGetUniformLocation(shaderProgram, "noiseIntensity"), 
                m_data.digitalNoise.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "noiseAmount"), 
                m_data.digitalNoise.noiseParams.amount);
    
    // Image Retention
    glUniform1f(glGetUniformLocation(shaderProgram, "retentionIntensity"), 
                m_data.imageRetention.intensity);
    
    // Freeze
    glUniform1f(glGetUniformLocation(shaderProgram, "freezeIntensity"), 
                m_data.freeze.intensity);
    glUniform1i(glGetUniformLocation(shaderProgram, "freezeMode"), 
                m_data.freeze.freezeParams.mode);
    glUniform1f(glGetUniformLocation(shaderProgram, "freezeHoldStrength"), 
                m_data.freeze.freezeParams.holdStrength);
    glUniform1f(glGetUniformLocation(shaderProgram, "freezeStripCount"), 
                m_data.freeze.freezeParams.stripCount);
    glUniform1f(glGetUniformLocation(shaderProgram, "freezeFlickerSpeed"), 
                m_data.freeze.freezeParams.flickerSpeed);
}

// void LCDEffect::applyToShader(unsigned int shaderProgram, float time,
//                               float centerX, float centerY, 
//                               float radiusX, float radiusY)
// {   
//     // Постоянные эффекты
//     glUniform1f(glGetUniformLocation(shaderProgram, "backlightFlickerIntensity"), 
//                 m_data.backlightFlicker.intensity);
//     glUniform1f(glGetUniformLocation(shaderProgram, "pixelResponseIntensity"), 
//                 m_data.pixelResponse.intensity);
    
//     // Cable Lines
//     glUniform1f(glGetUniformLocation(shaderProgram, "cableLineIntensity"), 
//                 m_data.cableLines.intensity);
    
//     // ===== TEARING =====
//     glUniform1f(glGetUniformLocation(shaderProgram, "lcdTearingIntensity"), 
//                 m_data.lcdTearing.intensity);
    
//     // Генерируем позиции разрывов
//     float t = time * 0.1;
//     glUniform1f(glGetUniformLocation(shaderProgram, "tearingLine1"), 0.3 + 0.1 * sin(t));
//     glUniform1f(glGetUniformLocation(shaderProgram, "tearingLine2"), 0.6 + 0.1 * cos(t * 1.3));
//     glUniform1f(glGetUniformLocation(shaderProgram, "tearingLine3"), 0.8 + 0.1 * sin(t * 0.7));
    
//     glUniform1f(glGetUniformLocation(shaderProgram, "tearingShift1"), 30.0 * m_data.lcdTearing.intensity);
//     glUniform1f(glGetUniformLocation(shaderProgram, "tearingShift2"), 15.0 * m_data.lcdTearing.intensity);
//     glUniform1f(glGetUniformLocation(shaderProgram, "tearingShift3"), 45.0 * m_data.lcdTearing.intensity);
    
    
//     glUniform1f(glGetUniformLocation(shaderProgram, "interferenceIntensity"), 
//                 m_data.interference.intensity);
//     glUniform1f(glGetUniformLocation(shaderProgram, "interferenceSpeed"), 
//                 m_data.interference.interferenceParams.speed);
//     glUniform1f(glGetUniformLocation(shaderProgram, "interferenceAmplitude"), 
//                 m_data.interference.interferenceParams.amplitude);
    
//     // Digital Noise
//     glUniform1f(glGetUniformLocation(shaderProgram, "noiseIntensity"), 
//                 m_data.digitalNoise.intensity);
//     glUniform1f(glGetUniformLocation(shaderProgram, "noiseAmount"), 
//                 m_data.digitalNoise.noiseParams.amount);
    
//     // Image Retention
//     glUniform1f(glGetUniformLocation(shaderProgram, "retentionIntensity"), 
//                 m_data.imageRetention.intensity);
    
//     // Freeze
//     glUniform1f(glGetUniformLocation(shaderProgram, "freezeIntensity"), 
//                 m_data.freeze.intensity);
//     glUniform1i(glGetUniformLocation(shaderProgram, "freezeMode"), 
//                 m_data.freeze.freezeParams.mode);
//     glUniform1f(glGetUniformLocation(shaderProgram, "freezeHoldStrength"), 
//                 m_data.freeze.freezeParams.holdStrength);
//     glUniform1f(glGetUniformLocation(shaderProgram, "freezeStripCount"), 
//                 m_data.freeze.freezeParams.stripCount);
//     glUniform1f(glGetUniformLocation(shaderProgram, "freezeFlickerSpeed"), 
//                 m_data.freeze.freezeParams.flickerSpeed);
// }

void LCDEffect::resetGlitches()
{
    m_data.cableLines.active = false;
    m_data.imageRetention.active = false;
    m_data.digitalNoise.active = false;
    m_data.lcdTearing.active = false;
    m_data.interference.active = false;
    
    m_data.cableLines.intensity = 0.0f;
    m_data.imageRetention.intensity = 0.0f;
    m_data.digitalNoise.intensity = 0.0f;
    m_data.lcdTearing.intensity = 0.0f;
    m_data.interference.intensity = 0.0f;
}


void LCDEffect::loadConfig(std::shared_ptr<game::IRadarEffectsConfig> config)
{
    auto lcdConfig = std::dynamic_pointer_cast<game::PPILCDEffectsConfig>(config);
    if (!lcdConfig) return;
    
    m_data.backlightFlicker     = lcdConfig->backlightFlicker;
    m_data.pixelResponse        = lcdConfig->pixelResponse;
    m_data.cableLines           = lcdConfig->cableLines;
    m_data.imageRetention       = lcdConfig->imageRetention;
    m_data.digitalNoise         = lcdConfig->digitalNoise;
    m_data.lcdTearing           = lcdConfig->lcdTearing;
    m_data.interference         = lcdConfig->interference;
    m_data.freeze               = lcdConfig->freeze;
}