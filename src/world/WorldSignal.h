#pragma once

#include <string>
#include <glm/vec3.hpp>
#include "src/world/coordinates/WorldPosition.h"

#include "src/world/types/SignalDisplayClass.h"
#include "src/world/types/SignalType.h"
#include "src/world/types/SignalAddress.h"


#include "src/game/signals/SignalPattern.h"
#include "src/scene/EntityID.h"



struct WorldSignal
{
    SignalType              type;
    SignalDisplayClass      displayClass;
    SignalAddress           address;
    world::coordinates::WorldPosition worldPosition;

    // Legacy mirror. Не источник истины.
    glm::vec3 position {0.0f};
    float                   power;      // базовая мощность сигнала
    float                   maxRange;   // максимальная дальность
    bool                    enabled;
    std::string             label;
    EntityId                owner;

    
};




