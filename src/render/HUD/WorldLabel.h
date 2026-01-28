#pragma once

#include <glm/glm.hpp>
#include <string>

#include "world/types/SignalDisplayClass.h"
#include "world/types/SignalSemanticState.h"



enum class SignalPresence
{
    Absent,
    FadingIn,
    Present,
    FadingOut
};

struct SignalWave
{
    float radius = 0.0f;
    bool  alive  = false;
};

struct WorldLabelData
{
    // --- Пространство
    glm::vec3 worldPos{0.0f};
    float distance = 0.0f;                 // валидно только для Decoded

    // --- Семантика
    SignalSemanticState semanticState = SignalSemanticState::None;
    SignalDisplayClass displayClass;

    // --- Идентификация
    std::string         displayName     = "undefined";
    bool                hasDistance     = false;              // derived

    // --- Числовые параметры
    float receivedPower = 0.0f;
    float signalToNoiseRatio = 0.0f;
    float stability = 0.0f;

    // --- Производные флаги
    bool isOccluded = false;
};



struct WorldLabelVisualState
{
    SignalPresence presence = SignalPresence::Absent;

    float presenceTimer = 0.0f;
    float visibility = 0.0f;

    float noisePhase = 0.0f;

    // --- добавляем ЛОГИЧЕСКИЕ таймеры
    float nextCheckTimer = 0.0f;   // когда снова попробовать показать сигнал
    float windowTimer = 0.0f;      // сколько длится текущее окно
    bool  windowActive = false;


    // --- Waves ---
    static constexpr int MaxWaves = 3;
    SignalWave waves[MaxWaves];
    float waveSpawnTimer = 0.0f;
};



struct WorldLabel
{
    WorldLabelData data;
    WorldLabelVisualState visual;

    glm::vec2 screenPos;
    bool onScreen;
};
