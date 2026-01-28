#include "WorldSignalWaves.h"
#include "render/VisualTuning.h"

#include <iostream>

void updateWorldSignalWaves(
    WorldLabelVisualState& visual,
    float dt,
    bool allowSpawn
)
{
    // ============================================================
    // 1. Update existing waves
    // ============================================================
    // Каждая волна — это независимый визуальный объект.
    // Если волна "жива", она расширяется со временем.
    // Когда достигает максимального радиуса — умирает.
    //
    // ВАЖНО:
    // - волны не зависят от текущего сигнала
    // - они доживают свой цикл сами
    // ============================================================
    
    for (int i = 0; i < WorldLabelVisualState::MaxWaves; ++i)
    {
        SignalWave& wave = visual.waves[i];

        // Пустой слот — ничего не делаем
        if (!wave.alive)
            continue;

        // Расширяем волну
        wave.radius += VisualTuning::Waves::waveSpeed * dt;

        // Если волна достигла предельного радиуса —
        // считаем её завершённой и освобождаем слот
        if (wave.radius >= VisualTuning::Waves::maxWaveRadius)
        {
            wave.alive  = false;
            wave.radius = VisualTuning::Waves::startWaveRadius;
        }
    }

    // ============================================================
    // 2. Spawn new waves (if allowed)
    // ============================================================
    // Рождение новых волн — НЕ автоматическое.
    // Оно разрешается внешней логикой (allowSpawn),
    // которая уже решила, что сейчас "уместно" шуметь.
    // ============================================================

    // Если спавн волн запрещён — выходим.
    // Уже существующие волны при этом продолжают жить.

    
    // Накапливаем время с момента последнего спавна
    visual.waveSpawnTimer += dt;
    
    
    if (visual.waveSpawnTimer < VisualTuning::Waves::waveSpawnInterval)
        return;


    if (!allowSpawn)
        return;


    // Если интервал ещё не прошёл — рано создавать новую волну

    // ПЫТАЕМСЯ заспавнить
    bool spawned = false;


    // ============================================================
    // 3. Allocate new wave
    // ============================================================
    // Ищем первый свободный слот и активируем новую волну.
    // Если свободных слотов нет — волна просто не появится.
    // Это сознательное ограничение, а не ошибка.
    // ============================================================
    for (int i = 0; i < WorldLabelVisualState::MaxWaves; ++i)
    {
        SignalWave& wave = visual.waves[i];

        if (!wave.alive)
        {
            wave.alive  = true;
            spawned = true;
            wave.radius = VisualTuning::Waves::startWaveRadius;

            // Временный лог — полезен для отладки ритма волн
            // std::cout << "[Wave] spawn idx=" << i << std::endl;
            
            break;
        }
    }

    // СБРАСЫВАЕМ ТАЙМЕР ТОЛЬКО ЕСЛИ СПАВН УДАЛСЯ
    if (spawned)
    {
        visual.waveSpawnTimer = 0.0f;
    }
}



