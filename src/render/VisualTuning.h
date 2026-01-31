#pragma once

namespace VisualTuning
{
    

    // // =====================================================
    // // ПАРАМЕТРЫ ПРИЕМНИКА СИГНАЛА
    // // =====================================================

    
    constexpr float receiverBaseNoise = 0.05f;      // шумовой порог приемника корабля
    constexpr float snrStableThreshold = 3.0f;      // SNR и устойчивость сигнал в ~3 раза сильнее шума — классическое инженерное правило
    constexpr float snrDecodeThreshold = 6.0f;      // SNR, при котором сигнал считается полностью устойчивым


    constexpr float fadeInTime        = 0.25f;
    constexpr float fadeOutTime       = 0.5f;


    // // =====================================================
    // // ПАРАМЕТРЫ отображения сигнала
    // // =====================================================

    constexpr float fadeInSpeed        = 1.0f;  // изменение значения visibility  в секунду для FadingIn
    constexpr float fadeOutSpeed       = 2.0f;  // изменение значения visibility  в секунду для FadingOut

    constexpr float minVisibleTime    = 2.4f;   // время отображения одного состояния метки

    constexpr float noiseCheckMin     = 0.3f;
    constexpr float noiseCheckMax     = 1.5f;

    constexpr float noiseWindowMin    = 0.3f;
    constexpr float noiseWindowMax    = 0.8f;


    // // =====================================================
    // // ПАРАМЕТРЫ отображения сигнала - волна
    // // =====================================================
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
        inline constexpr float minWaveAlpha  = 0.02f;

        // --- НОВОЕ: базовый цвет волн ---
        inline constexpr glm::vec3 color =
        {
            0.55f, 0.85f, 1.0f
        };

    }

    // // =====================================================
    // // ПАРАМЕТРЫ отображения сигнала - edge marker
    // // =====================================================

    namespace wl_edge{
        constexpr float ARROW_LENGTH        = 9.0f;
        constexpr float ARROW_WIDTH         = 8.0f;
        constexpr float DISTANCE            = 6.0f;
    }

}