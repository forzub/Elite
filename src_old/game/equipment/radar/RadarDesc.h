#pragma once
#include <memory>
#include "src/game/equipment/types/RadarVisualProfile.h"
#include "src/game/equipment/radar/IRadarEffectsConfig.h"
#include "src/game/equipment/types/RadarType.h"
#include "IRadarVisualConfig.h"

namespace game {

struct RadarDesc
{
    double powerConsumption;   // MW
    double heatGeneration;     // условное тепло
    double maxRange;           // базовая дальность (км или ваши единицы)
    double trackingSpeed;      // скорость накопления lock
    double jamResistance;      // устойчивость к помехам
    double scanInterval;        // частота обновления экрана
    
    // требования к платформе
    double requiredPowerCapacity;
    double requiredMountSize;
    double sweepSpeedDegPerSec;

    game::RadarType             type;                       // PPI, VerticalScreen, и т.д.
    game::RadarVisualProfile    visualProfile;              // CRT, LCD, Steampunk
   
    std::shared_ptr<IRadarVisualConfig> visual;
    std::shared_ptr<game::IRadarEffectsConfig> effects;
};

}