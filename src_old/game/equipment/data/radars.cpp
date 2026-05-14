#include "radars.h"
#include "src/game/equipment/types/RadarVisualProfile.h"
#include "src/game/equipment/radar/config/PPIVisualConfig.h"
#include "src/game/equipment/radar/config/PPILCDEffectsConfig.h"
#include "src/game/equipment/radar/config/PPICRTEffectsConfig.h"

namespace game {


const RadarDesc PPI_CRT_RADAR = []()
{
    auto ppi_cfg = std::make_shared<PPIVisualConfig>();
    // --- Background ---
    ppi_cfg->background.color = {0.0f, 0.15f, 0.0f, 0.9f};
    ppi_cfg->background.perspective = 0.30f;
    
    // --- Sweep ---
    ppi_cfg->sweep.color = {0.0f, 1.0f, 0.0f, 0.1f};
    ppi_cfg->sweep.lineWidth = 2.0f;
    ppi_cfg->sweep.trailLengthDeg = 35.0f;
    ppi_cfg->sweep.trailSteps = 30;
    ppi_cfg->sweep.trailFadePower = 1.5f;
    
    // --- Contacts ---
    ppi_cfg->contact.color = {0.0f, 1.0f, 0.0f, 0.8f};
    ppi_cfg->contact.halfWidth = 5.0f;
    ppi_cfg->contact.boxSize = 4.0f;
    ppi_cfg->contact.radiusScale = 0.85f;
    ppi_cfg->contact.verticalScale = 0.3f;
    
    // Настройки сетки (гратикулы)
    ppi_cfg->graticule.intensity = 0.9f;
    ppi_cfg->graticule.color = {0.0f, 1.0f, 0.0f, 1.0f};
    ppi_cfg->graticule.lineWidth = 2.0f;
    ppi_cfg->graticule.numRings = 4;
    ppi_cfg->graticule.numRays = 8;

    // --- textures ---
    ppi_cfg->backgroundTexturePath = "assets/img/radar_mk1_900x300_bg.png";
    ppi_cfg->overlayTexturePath = "assets/img/radar_mk1_900x300_glass.png";


    auto crt_effects = std::make_shared<PPICRTEffectsConfig>();
    
    // ПОСТОЯННЫЕ ЭФФЕКТЫ
    crt_effects->flicker.intensity = 0.6f;
    crt_effects->flicker.frequency = 50.0f;
    
    crt_effects->breathing.intensity = 0.5f;
    crt_effects->breathing.speed = 1.2f;
    
    crt_effects->constantHumBars.intensity = 0.5f;
    crt_effects->constantHumBars.speed = 50.0f;
    crt_effects->constantHumBars.width = 0.15f;

    // ЭПИЗОДИЧЕСКИЕ ГЛЮКИ
    
    // Параметры случайности
    crt_effects->tearing.minInterval = 2.0f;
    crt_effects->tearing.maxInterval = 5.0f;
    crt_effects->tearing.minDuration = 1.0f;
    crt_effects->tearing.maxDuration = 3.0f;
    crt_effects->tearing.minIntensity = 0.3f;
    crt_effects->tearing.maxIntensity = 1.0f;

    // Специфические параметры для Tearing
    crt_effects->tearing.tearingParams.stripCount = 6.0f;
    crt_effects->tearing.tearingParams.stripHeight = 0.04f;
    crt_effects->tearing.tearingParams.maxShift = 20.0f;
    crt_effects->tearing.tearingParams.chaosSpeed = 100.0f;


    // Параметры случайности
    crt_effects->episodicHumBars.minInterval = 2.0f;
    crt_effects->episodicHumBars.maxInterval = 5.0f;
    crt_effects->episodicHumBars.minDuration = 1.0f;
    crt_effects->episodicHumBars.maxDuration = 3.0f;
    crt_effects->episodicHumBars.minIntensity = 0.3f;
    crt_effects->episodicHumBars.maxIntensity = 1.0f;

    // Специфические параметры для HumBars
    crt_effects->episodicHumBars.humBarsParams.speed = 0.3f;
    crt_effects->episodicHumBars.humBarsParams.width = 0.03f;



    // Параметры случайности
    crt_effects->ghosting.active = true;
    crt_effects->ghosting.minInterval = 0.0f;
    crt_effects->ghosting.maxInterval = 0.0f;
    crt_effects->ghosting.minDuration = 5.0f;
    crt_effects->ghosting.maxDuration = 10.0f;
    crt_effects->ghosting.minIntensity = 0.3f;
    crt_effects->ghosting.maxIntensity = 1.0f;

    // Специфические параметры для Ghosting
    crt_effects->ghosting.ghostingParams.primaryOffset = 13.0f;
    crt_effects->ghosting.ghostingParams.secondaryOffset = 55.0f;
    crt_effects->ghosting.ghostingParams.tertiaryOffset = 105.0f;
    crt_effects->ghosting.ghostingParams.primaryIntensity = 0.7f;
    crt_effects->ghosting.ghostingParams.secondaryIntensity = 0.5f;
    crt_effects->ghosting.ghostingParams.tertiaryIntensity = 0.1f;


    // Параметры случайности
    crt_effects->verticalRoll.minInterval = 2.0f;
    crt_effects->verticalRoll.maxInterval = 5.0f;
    crt_effects->verticalRoll.minDuration = 1.0f;
    crt_effects->verticalRoll.maxDuration = 3.0f;
    crt_effects->verticalRoll.minIntensity = 0.3f;
    crt_effects->verticalRoll.maxIntensity = 1.0f;
    // Специфические параметры для VerticalRoll
    crt_effects->verticalRoll.rollParams.speed = 100.0f;
    
       
    RadarDesc desc;

    desc.powerConsumption = 8.0;                    // Потребление (МВт)	0.3% от реактора
    desc.heatGeneration = 0.4;                      // Тепловыделение (МВт)	5% от потребления (КПД 95%)
    desc.maxRange = 1000.0;                         // Дальность (м)	Для FRN-U
    desc.trackingSpeed = 0.0;                       // Не используется
    desc.jamResistance = 0.0;                       // Не используется
    desc.scanInterval = 0.0;                        // Не используется        
    
    // требования к платформе
    desc.requiredPowerCapacity = 50.0;
    desc.requiredMountSize = 1.0;
    desc.sweepSpeedDegPerSec = 720.0;

    desc.type = RadarType::PPI;                       
    desc.visualProfile = RadarVisualProfile::CRT;
    desc.visual = ppi_cfg;
    desc.effects = crt_effects;

    return desc;
}();




const RadarDesc PPI_LCD_RADAR = []()
{
    auto ppi_cfg = std::make_shared<PPIVisualConfig>();
    // --- Background ---
    ppi_cfg->background.color = {0.0f, 0.08f, 0.15f, 0.7f};
    ppi_cfg->background.perspective = 0.30f;
    
    // --- Sweep ---
    ppi_cfg->sweep.color = {0.0f, 0.3f, 0.3f, 0.5f};
    ppi_cfg->sweep.lineWidth = 2.0f;
    ppi_cfg->sweep.trailLengthDeg = 5.0f;
    ppi_cfg->sweep.trailSteps = 5;
    ppi_cfg->sweep.trailFadePower = 2.5f;
    
    // --- Contacts ---
    ppi_cfg->contact.color = {0.0f, 0.5f, 0.8f, 0.8f};
    ppi_cfg->contact.halfWidth = 5.0f;
    ppi_cfg->contact.boxSize = 4.0f;
    ppi_cfg->contact.radiusScale = 0.85f;
    ppi_cfg->contact.verticalScale = 0.3f;
    
    // Настройки сетки (гратикулы)
    ppi_cfg->graticule.intensity = 0.9f;
    ppi_cfg->graticule.color = {0.0f, 0.5f, 0.8f, 1.0f};
    ppi_cfg->graticule.lineWidth = 2.0f;
    ppi_cfg->graticule.numRings = 4;
    ppi_cfg->graticule.numRays = 8;

    // --- textures ---
    ppi_cfg->backgroundTexturePath = "assets/img/radar_mk1_900x300_bg.png";
    ppi_cfg->overlayTexturePath = "assets/img/radar_mk1_900x300_glass.png";

    auto lcd_effects = std::make_shared<PPILCDEffectsConfig>();
    
    // ПОСТОЯННЫЕ ЭФФЕКТЫ LCD
    lcd_effects->backlightFlicker.intensity = 0.9f;
    lcd_effects->backlightFlicker.frequency = 50.0f;

    lcd_effects->pixelResponse.intensity = 0.9f;

    // ЭПИЗОДИЧЕСКИЕ ГЛЮКИ LCD

    // Параметры случайности
    lcd_effects->cableLines.minInterval = 3.0f;
    lcd_effects->cableLines.maxInterval = 8.0f;
    lcd_effects->cableLines.minDuration = 0.5f;
    lcd_effects->cableLines.maxDuration = 2.0f;
    lcd_effects->cableLines.minIntensity = 0.3f;
    lcd_effects->cableLines.maxIntensity = 1.0f;
    // Специфические параметры для Cable Lines
    lcd_effects->cableLines.cableLineParams.minPosition = 0.2f;
    lcd_effects->cableLines.cableLineParams.maxPosition = 0.8f;
    lcd_effects->cableLines.cableLineParams.width = 0.02f;
    lcd_effects->cableLines.cableLineParams.color = {1.0f, 0.0f, 0.0f};

    // Параметры случайности
    lcd_effects->imageRetention.minInterval = 3.0f;
    lcd_effects->imageRetention.maxInterval = 8.0f;
    lcd_effects->imageRetention.minDuration = 0.5f;
    lcd_effects->imageRetention.maxDuration = 2.0f;
    lcd_effects->imageRetention.minIntensity = 0.3f;
    lcd_effects->imageRetention.maxIntensity = 1.0f;
    // Специфические параметры для Image Retention
    lcd_effects->imageRetention.retentionParams.decaySpeed = 0.1f;

    // Параметры случайности
    lcd_effects->digitalNoise.minInterval = 3.0f;
    lcd_effects->digitalNoise.maxInterval = 8.0f;
    lcd_effects->digitalNoise.minDuration = 0.5f;
    lcd_effects->digitalNoise.maxDuration = 2.0f;
    lcd_effects->digitalNoise.minIntensity = 0.3f;
    lcd_effects->digitalNoise.maxIntensity = 1.0f;
    // Специфические параметры для Digital Noise
    lcd_effects->digitalNoise.noiseParams.amount = 5.2f;

    // Параметры случайности
    lcd_effects->lcdTearing.active = false;
    lcd_effects->lcdTearing.minInterval = 0.0f;
    lcd_effects->lcdTearing.maxInterval = 0.0f;
    lcd_effects->lcdTearing.minDuration = 0.5f;
    lcd_effects->lcdTearing.maxDuration = 2.0f;
    lcd_effects->lcdTearing.minIntensity = 0.3f;
    lcd_effects->lcdTearing.maxIntensity = 1.0f;
    // Специфические параметры для Tearing
    lcd_effects->lcdTearing.tearingParams.maxOffset = 20.0f;

    // Параметры случайности
    lcd_effects->interference.minInterval = 3.0f;
    lcd_effects->interference.maxInterval = 8.0f;
    lcd_effects->interference.minDuration = 0.5f;
    lcd_effects->interference.maxDuration = 2.0f;
    lcd_effects->interference.minIntensity = 0.3f;
    lcd_effects->interference.maxIntensity = 1.0f;
    // Специфические параметры для Interference
    lcd_effects->interference.interferenceParams.speed = 5.0f;
    lcd_effects->interference.interferenceParams.amplitude = 0.6f;

    // Параметры случайности
    lcd_effects->freeze.minInterval = 3.0f;
    lcd_effects->freeze.maxInterval = 8.0f;
    lcd_effects->freeze.minDuration = 0.5f;
    lcd_effects->freeze.maxDuration = 2.0f;
    lcd_effects->freeze.minIntensity = 0.3f;
    lcd_effects->freeze.maxIntensity = 1.0f;
    // ===== НОВЫЙ ГЛЮК: FREEZE (застывание) =====
    lcd_effects->freeze.freezeParams.mode = 0;
    lcd_effects->freeze.freezeParams.holdStrength = 0.8f;
    lcd_effects->freeze.freezeParams.stripCount = 3.0f;
    lcd_effects->freeze.freezeParams.flickerSpeed = 5.0f;


    RadarDesc desc;

    desc.powerConsumption = 8.0;                    // Потребление (МВт)	0.3% от реактора
    desc.heatGeneration = 0.4;                      // Тепловыделение (МВт)	40% от потребления (КПД 60%)
    desc.maxRange = 1000.0;                         // Дальность (м)	Для FRN-U
    desc.trackingSpeed = 0.0;                       // Не используется
    desc.jamResistance = 0.0;                       // Не используется
    desc.scanInterval = 0.0;                        // Не используется
    
    // требования к платформе
    desc.requiredPowerCapacity = 50.0;
    desc.requiredMountSize = 1.0;
    desc.sweepSpeedDegPerSec = 1400.0;

    desc.type = RadarType::PPI;                       
    desc.visualProfile = RadarVisualProfile::LCD;
    desc.visual = ppi_cfg;
    desc.effects = lcd_effects; 

    return desc;
}();





// const RadarDesc PPI_CRT_RADAR = []()
// {
//     auto ppi_cfg = std::make_shared<PPIVisualConfig>();
//     // --- Background ---
//     ppi_cfg->m_bgColor = {0.0f, 0.15f, 0.0f, 0.7f};
//     ppi_cfg->m_axisColor = {0.0f, 1.0f, 0.0f, 0.8f};
//     // --- Sweep ---
//     ppi_cfg->m_sweepColor = {0.0f, 1.0f, 0.0f, 0.1f};
//     ppi_cfg->m_sweepLineWidth = 2.0f;
//     ppi_cfg->m_trailFadePower = 1.5f;
//     // --- Contacts ---
//     ppi_cfg->m_contactColor = {0.0f, 1.0f, 0.0f, 0.80f};
//     ppi_cfg->m_contactHalfWidth = 5.0f;
//     ppi_cfg->m_contactBoxSize = 3.0f;
//     // --- Layout tuning ---
//     ppi_cfg->m_radiusScale = 0.85f;
//     ppi_cfg->m_verticalScale = 0.3f;
//     // --- textures ---
//     ppi_cfg->backgroundTexturePath = "assets/img/radar_mk1_900x300_bg.png";
//     ppi_cfg->overlayTexturePath = "assets/img/radar_mk1_900x300_glass.png";

//     return RadarDesc{
//         8.0,                        // MW
//         0.4,                        // условное тепло
//         100.0,                      // базовая дальность (м)
//         0.0,                        // скорость накопления lock
//         0.0,                        // устойчивость к помехам
//         0.0,                        // частота обновления экрана (?)
//         RadarVisualProfile::PPI_CRT,    // тип визуала - PPI (CRT)
//         50.0,                       // требования к платформе requiredPowerCapacity
//         1.0,                        // требования к платформе requiredMountSize
//         720.0,                      // частота обновления экрана
//         ppi_cfg
//     };
// }();


// const RadarDesc YACHT_RADAR = {
//     2.0,    // power
//     0.2,    // heat
//     10.0,   // maxRange
//     0.0,    // trackingSpeed
//     0.2,    // jamResistance
//     0.0,
//     RadarVisualProfile::HolographicSphere,  // LCD тип
//     50.0,   
//     1.0,
//     30.0,   // sweepSpeedDegPerSec
//     nullptr
// };

// const RadarDesc CIVIL_RADAR = {
//     3.0,
//     0.3,
//     20.0,
//     0.0,
//     0.3,
//     0.0,
//     RadarVisualProfile::PPI,  // CRT тип
//     50.0,   
//     1.0,
//     30.0,
//     nullptr
// };

// const RadarDesc FIGHTER_RADAR = {
//     8.0,
//     1.5,
//     40000.0,
//     1.0,
//     0.6,
//     0.0,
//     RadarVisualProfile::FighterHUD,  // LCD тип
//     50.0,   
//     1.0,
//     30.0,
//     nullptr
// };

// const RadarDesc COMBAT_RADAR = {
//     6.0,
//     1.0,
//     50.0,
//     0.8,
//     0.5,
//     0.0,
//     RadarVisualProfile::VerticalScreen,  // CRT тип
//     50.0,   
//     1.0,
//     30.0,
//     nullptr
// };

// const RadarDesc CARRIER_RADAR = {
//     20.0,
//     4.0,
//     100.0,
//     0.6,
//     0.8,
//     0.0,
//     RadarVisualProfile::HolographicSphere,  // LCD тип
//     50.0,   
//     1.0,
//     30.0,
//     nullptr
// };

}