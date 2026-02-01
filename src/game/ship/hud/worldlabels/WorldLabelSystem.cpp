#include "game/ship/hud/worldlabels/WorldLabelSystem.h"
#include "render/VisualTuning.h"

#include <glm/common.hpp>
#include <cstdlib>   // rand
#include <iostream>

//
// Утилита: случайное число в диапазоне [0..1]
// Используется для нерегулярного появления сигнала
//
static float rand01()
{
    return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

//
// Обновляет ВИЗУАЛЬНОЕ состояние метки сигнала.
// ❗ НЕ физика, ❗ НЕ рендер.
//
// Отвечает за:
// - окна видимости
// - плавное появление / исчезновение
// - визуальную инерцию
//
// Параметры:
// dt                      — шаг времени
// signalPhysicallyPresent — есть ли сигнал по физике (Noise или Decoded)
// snr                     — отношение сигнал/шум (влияет на шанс появления)
// visual                  — текущее визуальное состояние метки
//
void WorldLabelSystem::updateVisualState(
    float dt,
    SignalSemanticState semanticState,
    float snr,
    WorldLabelVisualState& visual
)
{
    // Таймер текущего визуального состояния (presence)
    visual.presenceTimer += dt;
    

    switch (visual.presence)
    {
    // ------------------------------------------------------------
    // СОСТОЯНИЕ: метка полностью невидима
    // ------------------------------------------------------------
    case SignalPresence::Absent:
    {
        

        // Гарантируем, что альфа = 0
        visual.visibility = 0.0f;

        // NONE → ничего не делаем
        if (semanticState == SignalSemanticState::None)
            break;

        float chance = 1.0f;

        // Если сигнал физически присутствует,
        // мы МОЖЕМ попробовать показать его игроку
        if (semanticState == SignalSemanticState::Noise && visual.presenceTimer >= VisualTuning::minVisibleTime)
        {
            // Вероятность появления зависит от качества сигнала.
            // Чем выше SNR — тем чаще появляются "окна".
            chance = glm::clamp(
                snr / VisualTuning::snrDecodeThreshold,
                0.05f,   // даже самый слабый сигнал иногда проявляется
                1.0f
            );

            // Случайное решение: открыть окно или нет
            float randomsize = rand01();
            if (randomsize < chance)
            {
                visual.presence = SignalPresence::FadingIn;
                visual.presenceTimer = 0.0f;
            }

            // std::cout
            //     << "[Absent] " 
            //     << " chance=" << chance
            //     << " randomsize=" << randomsize
            //     << std::endl;
        }
        break;
    }

    // ------------------------------------------------------------
    // СОСТОЯНИЕ: плавное появление метки (fade-in)
    // ------------------------------------------------------------
    case SignalPresence::FadingIn:
    {
        // Нормализованный прогресс появления
        float t = visual.presenceTimer / VisualTuning::fadeInTime;

        // Плавно увеличиваем видимость
        visual.visibility += dt * VisualTuning::fadeInSpeed;

        // если сигнал исчез ВО ВРЕМЯ появления
        if (semanticState == SignalSemanticState::None)
        {


            visual.presence = SignalPresence::FadingOut;
            visual.presenceTimer = 0.0f;
            break;
        }

        // Как только дошли до полной видимости —
        // переходим в состояние Present
        if (visual.visibility >= 1.0f)
        {
           visual.visibility = 1.0f;
            visual.presence = SignalPresence::Present;
            visual.presenceTimer = 0.0f;
        }
        break;
    }

    // ------------------------------------------------------------
    // СОСТОЯНИЕ: метка полностью видима
    // ------------------------------------------------------------
    case SignalPresence::Present:
    {
        // В этом состоянии метка всегда полностью видима
        visual.visibility = 1.0f;

        // Минимальное время, которое метка обязана быть видимой,
        // чтобы не было "моргания"
        // if (visual.presenceTimer < VisualTuning::minVisibleTime) //-------debug-----
        if (semanticState == SignalSemanticState::Decoded)
            break;
        
        // NONE → сразу уходим
        if (semanticState == SignalSemanticState::None)
        {
            visual.presence = SignalPresence::FadingOut;
            visual.presenceTimer = 0.0f;
            break;
        }

        // Если физически сигнал пропал —
        // начинаем плавное исчезновение
        float chance = 1.0f;

        if (semanticState == SignalSemanticState::Noise && visual.presenceTimer >= VisualTuning::minVisibleTime)
        {
            // Вероятность появления зависит от качества сигнала.
            // Чем выше SNR — тем чаще появляются "окна".
            chance = glm::clamp(
                snr / VisualTuning::snrDecodeThreshold,
                0.05f,   // даже самый слабый сигнал иногда проявляется
                1.0f
            );

            
            // Случайное решение: открыть окно или нет
            float randomsize = rand01();
            if (randomsize < chance)
            {
                visual.presenceTimer = 0.0f;
            }else{
                visual.presence = SignalPresence::FadingOut;
                visual.presenceTimer = 0.0f;
            }

            // std::cout
            //     << "[Present] " 
            //     << " chance=" << chance
            //     << " randomsize=" << randomsize
            //     << std::endl;
        }
        break;
    }

    // ------------------------------------------------------------
    // СОСТОЯНИЕ: плавное исчезновение метки (fade-out)
    // ------------------------------------------------------------
    case SignalPresence::FadingOut:
    {

        // Нормализованный прогресс исчезновения
        float t = visual.presenceTimer / VisualTuning::fadeOutTime;

        // Плавно уменьшаем видимость
        visual.visibility -= dt * VisualTuning::fadeOutSpeed;

        // Когда полностью исчезли —
        // возвращаемся в состояние Absent
        // if (t >= 1.0f)                                           //-------debug-----
        if (semanticState == SignalSemanticState::Decoded)
        {
            visual.presence = SignalPresence::FadingIn;
            visual.presenceTimer = 0.0f;

            break;
        }

        if (visual.visibility <= 0.0f)
        {
            visual.visibility = 0.0f;
            visual.presence = SignalPresence::Absent;
            visual.presenceTimer = 0.0f;
        }
        break;
    }
    }
}
