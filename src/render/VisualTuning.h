#pragma once

namespace VisualTuning
{
    // --------------------------------------------------
    // Visibility (общая логика появления / исчезновения)
    // --------------------------------------------------
    constexpr float FadeInSpeed  = 10.0f;  // ↑ быстрее появляется
    constexpr float FadeOutSpeed = 10.2f;  // ↑ быстрее гаснет

    // --------------------------------------------------
    // Direction-only signals (волны)
    // --------------------------------------------------
    constexpr int   MaxWaves        = 3;
    constexpr float WaveSpawnPeriod = 0.6f;   // сек между рождением волн
    constexpr float WaveSpeed       = 25.0f;  // px/sec
    constexpr float WaveMaxRadius   = 60.0f;  // px

    // --------------------------------------------------
    // Alpha / fading
    // --------------------------------------------------
    constexpr float MinWaveAlpha = 0.05f;

    // --------------------------------------------------
    // Text
    // --------------------------------------------------
    constexpr float LabelYOffset = 16.0f;
}