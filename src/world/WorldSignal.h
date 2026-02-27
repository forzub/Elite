#pragma once

#include <string>
#include <glm/vec3.hpp>


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
    glm::vec3               position;
    float                   power;      // базовая мощность сигнала
    float                   maxRange;   // максимальная дальность
    bool                    enabled;
    std::string             label;
    EntityId                owner;

    
};




