#pragma once

#include <string>
#include <glm/vec3.hpp>


#include "src/world/types/SignalDisplayClass.h"
#include "src/world/types/SignalType.h"

#include "src/game/signals/SignalPattern.h"





struct Ship;


struct WorldSignal
{
    SignalType              type;
    SignalDisplayClass      displayClass;
    glm::vec3               position;
    float                   power;      // базовая мощность сигнала
    float                   maxRange;   // максимальная дальность
    bool                    enabled;
    std::string             label;
    const Ship*             owner = nullptr;
    
};




