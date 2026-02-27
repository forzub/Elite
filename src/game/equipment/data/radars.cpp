// double powerConsumption;
    // Потребляемая мощность (MW).
    // Если доступная мощность < этого значения — радар не работает.

// double heatGeneration;     
    // Количество тепла, генерируемого при работе.
    // Должно учитываться ThermalSystem.
    // Не влияет на detection напрямую.

// double maxRange;           
    // Максимальная физическая дальность обнаружения.
    // Используется в RadarModule.
    // Это authoritative предел.


// === ПОВЕДЕНИЕ ОБНАРУЖЕНИЯ ===

// double trackingSpeed;      
    // Скорость накопления lock (если реализован захват цели).
    // Сейчас, вероятно, не используется.

// double jamResistance;      
    // Устойчивость к помехам.
    // Используется, если есть система глушения.
    // Сейчас может быть не задействована.

// double scanInterval;       
    // Интервал между сканированиями (сек).
    // Ограничивает частоту обновления detection.
    // В коде сейчас закомментирован.


// === ВИЗУАЛ ===

// game::RadarVisualProfile visualProfile;
    // Тип отображения радара (CRT, цифровой и т.д.).

// double sweepSpeedDegPerSec;
    // Скорость вращения визуального луча (градусы/сек).
    // Используется только в UI.


// === ТРЕБОВАНИЯ К ПЛАТФОРМЕ ===

// double requiredPowerCapacity;
    // Минимальная мощность реактора для установки радара.
    // Проверяется при конфигурации корабля.

// double requiredMountSize;
    // Требуемый размер слота/крепления.
    // Используется при установке оборудования.



#include "radars.h"
#include "src/game/equipment/types/RadarVisualProfile.h"
#include "src/game/equipment/radar/CRTVisualConfig.h"

namespace game {

const RadarDesc CRT_RADAR = []()
{
    auto crt_cfg = std::make_shared<CRTVisualConfig>();
    // --- Background ---
    crt_cfg->m_bgColor = {0.0f, 0.15f, 0.0f, 0.7f};
    crt_cfg->m_axisColor = {0.0f, 1.0f, 0.0f, 0.8f};
    // --- Sweep ---
    crt_cfg->m_sweepColor = {0.0f, 1.0f, 0.0f, 0.05f};
    crt_cfg->m_sweepLineWidth = 2.0f;
    crt_cfg->m_trailFadePower = 1.5f;
    // --- Contacts ---
    crt_cfg->m_contactColor = {0.0f, 1.0f, 0.0f, 0.80f};
    crt_cfg->m_contactHalfWidth = 5.0f;
    crt_cfg->m_contactBoxSize = 4.0f;
    // --- Layout tuning ---
    crt_cfg->m_radiusScale = 0.85f;
    crt_cfg->m_verticalScale = 0.3f;
    // --- textures ---
    crt_cfg->backgroundTexturePath = "assets/img/radar_mk1_900x300_bg.png";
    crt_cfg->overlayTexturePath = "assets/img/radar_mk1_900x300_glass.png";

    return RadarDesc{
        8.0,                        // MW
        0.4,                        // условное тепло
        100.0,                      // базовая дальность (м)
        0.0,                        // скорость накопления lock
        0.0,                        // устойчивость к помехам
        0.0,                        // частота обновления экрана (?)
        RadarVisualProfile::CRT,
        50.0,                       // требования к платформе requiredPowerCapacity
        1.0,                        // требования к платформе requiredMountSize
        720.0,                      // частота обновления экрана
        crt_cfg
    };
}();


const RadarDesc YACHT_RADAR = {
    2.0,    // power
    0.2,    // heat
    10.0,   // maxRange
    0.0,    // trackingSpeed (нет захвата)
    0.2,     // jamResistance
    0.0,
    RadarVisualProfile::CRT,
    50.0,   
    1.0,
    30.0,   // sweepSpeedDegPerSec

    nullptr
};

const RadarDesc CIVIL_RADAR = {
    3.0,
    0.3,
    20.0,
    0.0,
    0.3,
    0.0,
    RadarVisualProfile::CRT,
    50.0,   
    1.0,
    30.0,   // sweepSpeedDegPerSec

    nullptr

};

const RadarDesc FIGHTER_RADAR = {
    8.0,
    1.5,
    40000.0,
    1.0,   // быстрый lock
    0.6,
    0.0,
    RadarVisualProfile::FighterHUD,
    50.0,   
    1.0,
    30.0,   // sweepSpeedDegPerSec

    nullptr

};

const RadarDesc COMBAT_RADAR = {
    6.0,
    1.0,
    50.0,
    0.8,
    0.5,
    0.0,
    RadarVisualProfile::VerticalScreen,
    50.0,   
    1.0,
    30.0,   // sweepSpeedDegPerSec

    nullptr

};

const RadarDesc CARRIER_RADAR = {
    20.0,
    4.0,
    100.0,
    0.6,
    0.8,
    0.0,
    RadarVisualProfile::HolographicSphere,
    50.0,   
    1.0,
    30.0,   // sweepSpeedDegPerSec

    nullptr

};

}