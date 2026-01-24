#include "game/SignalReceiver.h"
#include <glm/glm.hpp>
#include <cstdlib>

 

static float rand01()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}


static bool isDirectionOnly(SignalType t)
{
    return (t == SignalType::SOSAntic ||
            t == SignalType::Unknown);
}



void SignalReceiver::update(
    float dt,
    const glm::vec3& receiverPos,
    const std::vector<WorldSignal>& worldSignals
)
{
    for (const WorldSignal& sig : worldSignals)
    {
        if (!sig.enabled)
            continue;

        float dist = glm::length(sig.position - receiverPos);

        // вне физического радиуса — сигнала нет
        if (dist > sig.maxRange)
            continue;

        // ----------------------------------------------------
        // найти или создать DetectedSignal
        // ----------------------------------------------------
        DetectedSignal* ds = nullptr;
        for (auto& d : m_detected)
        {
            if (d.source == &sig)
            {
                ds = &d;
                break;
            }
        }

        if (!ds)
        {
            m_detected.push_back({});
            ds = &m_detected.back();
            ds->source     = &sig;
            ds->stability  = 0.0f;
            ds->visible    = false;
            ds->confidence = 0.0f;
        }

        // ----------------------------------------------------
        // направление — ВСЕГДА детерминированное
        // ----------------------------------------------------
        ds->direction = glm::normalize(sig.position - receiverPos);
        ds->hasDirection = true;

        // ----------------------------------------------------
        // зоны приёма (задел под помехи)
        // ----------------------------------------------------
        float stableRange = sig.maxRange * 0.4f;
        float weakRange   = sig.maxRange * 0.8f;

        // древние / неопределённые сигналы стабилизируются хуже
        if (isDirectionOnly(sig.type))
        {
            stableRange *= 0.1f; // 10% от нормальной stable-зоны
        }

        // TODO: SignalEnvironment
        // stableRange *= (1.0f - env.noise - env.interference);
        // weakRange   *= (1.0f - env.noise - env.interference);

        // ----------------------------------------------------
        // устойчивость и видимость
        // ----------------------------------------------------
        if (dist < stableRange)
        {
            // В стабильной зоне — всегда стабилен
            ds->stability = 1.0f;
            ds->visible   = true;
        }
        else if (dist < weakRange)
        {
            // В зоне неуверенного приёма

            // --- ВАЖНО: ограничение максимальной стабильности ---
            float maxStability = isDirectionOnly(sig.type) ? 0.6f : 1.0f;

            ds->stability = glm::min(
                ds->stability + dt * 0.1f,
                maxStability
            );

            // --- Фликер: даже при высокой stability сигнал иногда пропадает ---
            // скорость мерцания (секунды между проверками)
            float minInterval = 0.3f; // быстро
            float maxInterval = 1.5f; // медленно

            // чем выше stability — тем реже меняется состояние
            float interval = glm::mix(
                maxInterval,
                minInterval,
                ds->stability
            );

            ds->flickerTimer -= dt;

            if (ds->flickerTimer <= 0.0f)
            {
                ds->flickerTimer = interval;

                float flickerChance = glm::max(0.2f, 1.0f - ds->stability);
                ds->visible = (rand01() > flickerChance);
            }
        }
        else
        {
            // За пределами слабой зоны — сигнала нет
            ds->stability = 0.0f;
            ds->visible   = false;
        }

        // ----------------------------------------------------
        // сила сигнала (для информации / HUD)
        // ----------------------------------------------------
        float strength = computeSignalStrength(sig, receiverPos);

        // амплитудный шум (НЕ влияет на direction)
        float noise = 0.8f + rand01() * 0.4f;
        ds->strength = strength * noise;

        // ----------------------------------------------------
        // confidence — UX-метрика (не блокирующая механику)
        // ----------------------------------------------------
        ds->confidence = glm::clamp(
            ds->confidence + dt * ds->stability,
            0.0f,
            1.0f
        );

        // ----------------------------------------------------
        // distance — политика по типу
        // ----------------------------------------------------
        if (isDirectionOnly(sig.type))
        {
            ds->hasDistance = false;
        }
        else
        {
            ds->hasDistance = (ds->stability > 0.8f);
        }
    }
}





const std::vector<DetectedSignal>& SignalReceiver::detected() const
{
    return m_detected;
}



float SignalReceiver::computeSignalStrength(
    const WorldSignal& sig,
    const glm::vec3& receiverPos
) const
{
    float dist = glm::length(sig.position - receiverPos);
    if (dist <= 0.0f)
        return sig.power;

    // простое затухание 1/r^2
    return sig.power / (dist * dist);
}


