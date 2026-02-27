#pragma once
#include <memory>
#include "src/game/equipment/types/RadarVisualProfile.h"
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
    game::RadarVisualProfile visualProfile;
    // требования к платформе
    double requiredPowerCapacity;
    double requiredMountSize;
    double sweepSpeedDegPerSec;
   
    std::shared_ptr<IRadarVisualConfig> visual;
};

}