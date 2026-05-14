#include "CRTEffects.h"
#include <glad/gl.h>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include "src/game/equipment/radar/config/PPICRTEffectsConfig.h"

CRTEffect::CRTEffect()
{
    // Инициализация таймеров для автозапуска (для теста)
    m_data.verticalRoll.minInterval = 0.0f;
    m_data.verticalRoll.timer = -1.0f;
}

void CRTEffect::update(float dt)
{
     
    // Фазы обновляются ВСЕГДА для анимации постоянных эффектов
    m_data.crtPhase += dt * m_data.flicker.frequency;
    m_data.breathingPhase += dt * m_data.breathing.speed;
    m_data.humBarPhase += dt * m_data.constantHumBars.speed;
    
    // Нормализация фаз
    while (m_data.crtPhase > 2.0f * 3.14159f) 
        m_data.crtPhase -= 2.0f * 3.14159f;
    while (m_data.breathingPhase > 2.0f * 3.14159f) 
        m_data.breathingPhase -= 2.0f * 3.14159f;
    while (m_data.humBarPhase > 1000.0f)      
        m_data.humBarPhase -= 1000.0f;
    
    // Обновляем глюки
    updateGlitchState(m_data.tearing, dt);
    updateGlitchState(m_data.episodicHumBars, dt);
    updateGlitchState(m_data.ghosting, dt);
    updateGlitchState(m_data.verticalRoll, dt);
}

void CRTEffect::updatePhases(float dt)
{
    m_data.crtPhase += dt * m_data.flicker.frequency;
    m_data.breathingPhase += dt * m_data.breathing.speed;
    m_data.humBarPhase += dt * m_data.constantHumBars.speed;
    
    // Нормализация фаз
    while (m_data.crtPhase > 2.0f * 3.14159f) 
        m_data.crtPhase -= 2.0f * 3.14159f;
    while (m_data.breathingPhase > 2.0f * 3.14159f) 
        m_data.breathingPhase -= 2.0f * 3.14159f;
    while (m_data.humBarPhase > 1000.0f)      
        m_data.humBarPhase -= 1000.0f;
}

void CRTEffect::updateGlitchState(game::PPICRTEffectsConfig::GlitchState& glitch, float dt)
{
    static float lastPrint = 0.0f;
    
    if (!glitch.active) {
        if (glitch.intensity > 0.0f) {
            printf("⚠️ Странно: active=false но intensity>0\n");
        }
        glitch.intensity = 0.0f;
        return;
    }
    
    // Печатаем состояние раз в секунду
    if (glitch.active && glitch.intensity > 0.0f) {
        if (glitch.elapsed - lastPrint > 1.0f) {
            printf("📊 Глюк активен: intensity=%.2f, elapsed=%.2f/%.2f\n", 
                   glitch.intensity, glitch.elapsed, glitch.duration);
            lastPrint = glitch.elapsed;
        }
    }

    
    if (!glitch.active) {
        if (glitch.intensity > 0.0f) {
            printf("⚠️ Почему intensity > 0 при active=false?\n");
        }
        glitch.intensity = 0.0f;
        return;
    }
    
    // Если сейчас активен
    if (glitch.intensity > 0.0f) {
        glitch.elapsed += dt;
        if (glitch.elapsed >= glitch.duration) {
            printf("🔄 Глюк выключился: duration=%.2f, elapsed=%.2f\n", 
                   glitch.duration, glitch.elapsed);
            glitch.intensity = 0.0f;
            glitch.timer = glitch.minInterval + 
                (rand() / (float)RAND_MAX) * 
                (glitch.maxInterval - glitch.minInterval);
            printf("   Пауза: timer=%.2f\n", glitch.timer);
        }
    } 
    // Если не активен, ждем таймер
    else {
        glitch.timer -= dt;
        if (glitch.timer <= 0.0f) {
            glitch.duration = glitch.minDuration + 
                (rand() / (float)RAND_MAX) * 
                (glitch.maxDuration - glitch.minDuration);
            
            glitch.intensity = glitch.minIntensity + 
                (rand() / (float)RAND_MAX) * 
                (glitch.maxIntensity - glitch.minIntensity);
            
            glitch.elapsed = 0.0f;
            
            printf("🎯 Глюк включился: duration=%.2f, intensity=%.2f\n", 
                   glitch.duration, glitch.intensity);
        }
    }
}

void CRTEffect::applyToShader(unsigned int shaderProgram, float time,
                              float centerX, float centerY, 
                              float radiusX, float radiusY)
{
    // Проверим, что все uniform-ы устанавливаются
    GLint loc;
    
    loc = glGetUniformLocation(shaderProgram, "flickerIntensity");
    if (loc >= 0) glUniform1f(loc, m_data.flicker.intensity);
    
    loc = glGetUniformLocation(shaderProgram, "breathingIntensity");
    if (loc >= 0) glUniform1f(loc, m_data.breathing.intensity);
    
    loc = glGetUniformLocation(shaderProgram, "constantHumBarIntensity");
    if (loc >= 0) glUniform1f(loc, m_data.constantHumBars.intensity);
    
    loc = glGetUniformLocation(shaderProgram, "constantHumBarPhase");
    if (loc >= 0) glUniform1f(loc, m_data.humBarPhase);
    
    loc = glGetUniformLocation(shaderProgram, "constantHumBarWidth");
    if (loc >= 0) glUniform1f(loc, m_data.constantHumBars.width);



    // Постоянные эффекты
    glUniform1f(glGetUniformLocation(shaderProgram, "flickerIntensity"), 
                m_data.flicker.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "breathingIntensity"), 
                m_data.breathing.intensity);
    // Постоянные эффекты с новыми параметрами
    glUniform1f(glGetUniformLocation(shaderProgram, "flickerFrequency"), 
                m_data.flicker.frequency);
    glUniform1f(glGetUniformLocation(shaderProgram, "breathingSpeed"), 
                m_data.breathing.speed);
    
    // Постоянные полосы
    glUniform1f(glGetUniformLocation(shaderProgram, "constantHumBarIntensity"), 
                m_data.constantHumBars.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "constantHumBarPhase"), 
                m_data.humBarPhase);
    glUniform1f(glGetUniformLocation(shaderProgram, "constantHumBarWidth"), 
                m_data.constantHumBars.width);
    
    // Tearing
    glUniform1f(glGetUniformLocation(shaderProgram, "tearIntensity"), 
                m_data.tearing.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "tearStripCount"), 
                m_data.tearing.tearingParams.stripCount);
    glUniform1f(glGetUniformLocation(shaderProgram, "tearStripHeight"), 
                m_data.tearing.tearingParams.stripHeight);
    glUniform1f(glGetUniformLocation(shaderProgram, "tearMaxShift"), 
                m_data.tearing.tearingParams.maxShift);
    glUniform1f(glGetUniformLocation(shaderProgram, "tearChaosSpeed"), 
                m_data.tearing.tearingParams.chaosSpeed);
    
    // Ghosting
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostIntensity"), 
                m_data.ghosting.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostPrimaryOffset"), 
                m_data.ghosting.ghostingParams.primaryOffset);
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostSecondaryOffset"), 
                m_data.ghosting.ghostingParams.secondaryOffset);
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostTertiaryOffset"), 
                m_data.ghosting.ghostingParams.tertiaryOffset);
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostPrimaryIntensity"), 
                m_data.ghosting.ghostingParams.primaryIntensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostSecondaryIntensity"), 
                m_data.ghosting.ghostingParams.secondaryIntensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "ghostTertiaryIntensity"), 
                m_data.ghosting.ghostingParams.tertiaryIntensity);
    
    // Vertical Roll
    glUniform1f(glGetUniformLocation(shaderProgram, "rollIntensity"), 
                m_data.verticalRoll.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "rollSpeed"), 
                m_data.verticalRoll.rollParams.speed);
    
    // Episodic Hum Bars
    glUniform1f(glGetUniformLocation(shaderProgram, "humBarIntensity"), 
                m_data.episodicHumBars.intensity);
    glUniform1f(glGetUniformLocation(shaderProgram, "humBarSpeed"), 
                m_data.episodicHumBars.humBarsParams.speed);
    glUniform1f(glGetUniformLocation(shaderProgram, "humBarWidth"), 
                m_data.episodicHumBars.humBarsParams.width);

    static float lastGhostPrint = 0.0f;
    if (time - lastGhostPrint > 2.0f) {
        printf("👻 Ghosting: active=%s, intensity=%.2f\n", 
            m_data.ghosting.active ? "yes" : "no",
            m_data.ghosting.intensity);
        lastGhostPrint = time;
    }

    static float lastPrint = 0.0f;
    if (time - lastPrint > 2.0f) {
        printf("CRT Effect active: flicker=%.2f, bars=%.2f, tearing=%s, roll=%s\n",
               m_data.flicker.intensity,
               m_data.constantHumBars.intensity,
               m_data.tearing.active ? "yes" : "no",
               m_data.verticalRoll.active ? "yes" : "no");
        lastPrint = time;
    }
}

void CRTEffect::resetGlitches()
{
    m_data.tearing.active = false;
    m_data.episodicHumBars.active = false;
    m_data.ghosting.active = false;
    m_data.verticalRoll.active = false;
    
    m_data.tearing.intensity = 0.0f;
    m_data.episodicHumBars.intensity = 0.0f;
    m_data.ghosting.intensity = 0.0f;
    m_data.verticalRoll.intensity = 0.0f;
}

// Методы настройки параметров
void CRTEffect::setTearingParams(float minInterval, float maxInterval, 
                                  float minDuration, float maxDuration,
                                  float minIntensity, float maxIntensity,
                                  float stripCount, float stripHeight, 
                                  float maxShift, float chaosSpeed)
{
    m_data.tearing.minInterval = minInterval;
    m_data.tearing.maxInterval = maxInterval;
    m_data.tearing.minDuration = minDuration;
    m_data.tearing.maxDuration = maxDuration;
    m_data.tearing.minIntensity = minIntensity;
    m_data.tearing.maxIntensity = maxIntensity;
    
    m_data.tearing.tearingParams.stripCount = stripCount;
    m_data.tearing.tearingParams.stripHeight = stripHeight;
    m_data.tearing.tearingParams.maxShift = maxShift;
    m_data.tearing.tearingParams.chaosSpeed = chaosSpeed;
}

void CRTEffect::setEpisodicHumBarsParams(float minInterval, float maxInterval,
                                          float minDuration, float maxDuration,
                                          float minIntensity, float maxIntensity,
                                          float speed, float width)
{
    m_data.episodicHumBars.minInterval = minInterval;
    m_data.episodicHumBars.maxInterval = maxInterval;
    m_data.episodicHumBars.minDuration = minDuration;
    m_data.episodicHumBars.maxDuration = maxDuration;
    m_data.episodicHumBars.minIntensity = minIntensity;
    m_data.episodicHumBars.maxIntensity = maxIntensity;
    
    m_data.episodicHumBars.humBarsParams.speed = speed;
    m_data.episodicHumBars.humBarsParams.width = width;
}

void CRTEffect::setGhostingParams(float minInterval, float maxInterval,
                                   float minDuration, float maxDuration,
                                   float minIntensity, float maxIntensity,
                                   float primaryOffset, float secondaryOffset, float tertiaryOffset,
                                   float primaryIntensity, float secondaryIntensity, float tertiaryIntensity)
{
    m_data.ghosting.minInterval = minInterval;
    m_data.ghosting.maxInterval = maxInterval;
    m_data.ghosting.minDuration = minDuration;
    m_data.ghosting.maxDuration = maxDuration;
    m_data.ghosting.minIntensity = minIntensity;
    m_data.ghosting.maxIntensity = maxIntensity;
    
    m_data.ghosting.ghostingParams.primaryOffset = primaryOffset;
    m_data.ghosting.ghostingParams.secondaryOffset = secondaryOffset;
    m_data.ghosting.ghostingParams.tertiaryOffset = tertiaryOffset;
    m_data.ghosting.ghostingParams.primaryIntensity = primaryIntensity;
    m_data.ghosting.ghostingParams.secondaryIntensity = secondaryIntensity;
    m_data.ghosting.ghostingParams.tertiaryIntensity = tertiaryIntensity;
}

void CRTEffect::setVerticalRollParams(float minInterval, float maxInterval,
                                       float minDuration, float maxDuration,
                                       float minIntensity, float maxIntensity,
                                       float speed)
{
    m_data.verticalRoll.minInterval = minInterval;
    m_data.verticalRoll.maxInterval = maxInterval;
    m_data.verticalRoll.minDuration = minDuration;
    m_data.verticalRoll.maxDuration = maxDuration;
    m_data.verticalRoll.minIntensity = minIntensity;
    m_data.verticalRoll.maxIntensity = maxIntensity;
    
    m_data.verticalRoll.rollParams.speed = speed;
}


void CRTEffect::loadConfig(std::shared_ptr<game::IRadarEffectsConfig> config)
{
    auto crtConfig = std::dynamic_pointer_cast<game::PPICRTEffectsConfig>(config);
    if (!crtConfig) return;
    
    m_data.flicker              = crtConfig->flicker;
    m_data.breathing            = crtConfig->breathing;
    m_data.constantHumBars      = crtConfig->constantHumBars;
    m_data.tearing              = crtConfig->tearing;
    m_data.episodicHumBars      = crtConfig->episodicHumBars;
    m_data.ghosting             = crtConfig->ghosting;
    m_data.verticalRoll         = crtConfig->verticalRoll;

    std::cout << "m_data.ghosting minInterval" << m_data.ghosting.minInterval << "\n";
    std::cout << "m_data.ghosting maxInterval" << m_data.ghosting.maxInterval << "\n";
}


void CRTEffect::enableGhosting(bool enable) 
    { 
      
        m_data.ghosting.active = enable; 
         
    }


