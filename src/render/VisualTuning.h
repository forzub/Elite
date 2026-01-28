#pragma once

namespace VisualTuning
{
    // // --------------------------------------------------
    // // Visibility (общая логика появления / исчезновения)
    // // --------------------------------------------------
    // constexpr float FadeInSpeed  = 10.0f;  // ↑ быстрее появляется
    // constexpr float FadeOutSpeed = 10.2f;  // ↑ быстрее гаснет

    // // --------------------------------------------------
    // // Direction-only signals (волны)
    // // --------------------------------------------------
    // constexpr int   MaxWaves        = 3;
    // constexpr float WaveSpawnPeriod = 0.6f;   // сек между рождением волн
    // constexpr float WaveSpeed       = 25.0f;  // px/sec
    // constexpr float WaveMaxRadius   = 60.0f;  // px

    // // --------------------------------------------------
    // // --- Фликер: даже при высокой stability сигнал иногда пропадает - пропадание сигнала
    // // --------------------------------------------------
    // constexpr float minInterval = 0.2f; // быстро
    // constexpr float maxInterval = 1.5f; // медленно

    // // --------------------------------------------------
    // // сколько секунд нужно для полной стабилизации
    // // --------------------------------------------------
    // constexpr float StableLockTime = 30.0f; // секунд
    // constexpr float WeakLockTime   = 90.0f; // секунд (в слабой зоне)

    // // --------------------------------------------------
    // // вычисляем коэффициенты роста
    // // --------------------------------------------------
    // constexpr float StableGain = 1.0f / StableLockTime;
    // constexpr float WeakGain   = 1.0f / WeakLockTime;


    // // --------------------------------------------------
    // // Alpha / fading
    // // --------------------------------------------------
    // constexpr float MinWaveAlpha = 0.05f;

    // // --------------------------------------------------
    // // Text
    // // --------------------------------------------------
    // constexpr float LabelYOffset = 16.0f;
    
    // // ниже порога — сигнала нет вообще
    // constexpr float DetectThreshold = 0.01f;

    // // Минимальная мощность сигнала для попытки декодировать ИМЯ источника
    // // 0.30 ≈ начало "зоны нестабильного приёма имени"
    // static constexpr float NameMinPower = 0.30f;

    // // =====================================================
    // // DISTANCE DECODE TUNING
    // // =====================================================

    // // Минимальная мощность сигнала для попытки принять ДИСТАНЦИЮ
    // // Ниже этого значения дистанция никогда не принимается
    // static constexpr float DistanceMinPower = 0.40f;

    // // Дополнительный "запас уверенности":
    // // дистанция принимается медленнее и теряется быстрее
    // static constexpr float DistanceProbabilityBias = 0.7f;

    // // =====================================================
    // // SIGNAL DECISION TIMING
    // // =====================================================

    // как часто можно ПЕРЕСМАТРИВАТЬ имя (сек)
    // static constexpr float NameDecisionInterval = 0.6f;

    // как часто можно ПЕРЕСМАТРИВАТЬ дистанцию (сек)
    // static constexpr float DistanceDecisionInterval = 1.2f;

    // constexpr float DetectInterval = 0.5f; // сек

    constexpr float receiverBaseNoise = 0.05f;      // шумовой порог приемника корабля
    constexpr float snrStableThreshold = 3.0f;      // SNR и устойчивость сигнал в ~3 раза сильнее шума — классическое инженерное правило
    constexpr float snrDecodeThreshold = 6.0f;      // SNR, при котором сигнал считается полностью устойчивым


    constexpr float fadeInTime        = 0.25f;
    constexpr float fadeOutTime       = 0.5f;

    constexpr float fadeInSpeed        = 1.0f;  // изменение значения visibility  в секунду для FadingIn
    constexpr float fadeOutSpeed       = 2.0f;  // изменение значения visibility  в секунду для FadingOut

    constexpr float minVisibleTime    = 2.4f;   // время отображения одного состояния метки

    constexpr float noiseCheckMin     = 0.3f;
    constexpr float noiseCheckMax     = 1.5f;

    constexpr float noiseWindowMin    = 0.3f;
    constexpr float noiseWindowMax    = 0.8f;

    namespace Waves{
        
        // --- wave render Presence ---
        constexpr float minVisibleTime = 0.4f;
        constexpr float minHiddenTime  = 0.4f;
    
        constexpr float fadeInTime  = 0.3f;
        constexpr float fadeOutTime = 0.25f;
        constexpr float minAlpha    = 0.05f;

    
        // --- Waves ---
        constexpr float waveSpawnInterval = 0.8f;
        constexpr float waveSpeed         = 25.0f;   // units/sec - не трогать, иначе будут пропуски
        constexpr float startWaveRadius   = 5.0f;
        constexpr float maxWaveRadius     = 60.0f;

        // --- НОВОЕ: визуальный масштаб ---
        inline constexpr float pixelsPerUnit = 15.0f;
        inline constexpr float lineThickness = 0.025f;
        inline constexpr float minWaveAlpha      = 0.02f;

        // --- НОВОЕ: базовый цвет волн ---
        inline constexpr glm::vec3 color =
        {
            0.55f, 0.85f, 1.0f
        };

    }

}