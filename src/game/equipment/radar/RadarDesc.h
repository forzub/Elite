#pragma once
#include <memory>
#include <cstdint>
#include "src/game/equipment/types/RadarVisualProfile.h"
#include "src/game/equipment/radar/IRadarEffectsConfig.h"
#include "src/game/equipment/types/RadarType.h"
#include "IRadarVisualConfig.h"

namespace game {

enum class RadarBackendKind : std::uint8_t
{
    Legacy = 0,
    TestIdeal = 1
};

struct RadarDesc
{
    double powerConsumption;   // MW
    double heatGeneration;     // условное тепло
    double maxRange;           // базовая дальность (км или ваши единицы)
    double trackingSpeed;      // скорость накопления lock
    double jamResistance;      // устойчивость к помехам
    double scanInterval;        // physical sensor scan period for sensor backends

    // Sensor-backend contract. Legacy visual radar ignores these fields.
    RadarBackendKind backendKind = RadarBackendKind::Legacy;
    double processingLatencySeconds = 0.0;
    double trackHoldSeconds = 0.0;
    double positionUncertaintyMeters = 0.0;
    double velocityUncertaintyMps = 0.0;
    double ownPositionUncertaintyMeters = 0.0;
    double ownVelocityUncertaintyMps = 0.0;
    
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