#pragma once
#include <vector>
#include <glm/glm.hpp>

#include "scene/EntityID.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/ship/ShipTypeId.h"
#include "game/ship/core/ShipTransform.h"
#include "src/world/types/SignalReceptionResult.h"
#include "src/game/equipment/radar/RadarContact.h"
#include "src/game/damage/DamageEvent.h"
#include "src/game/simulation/ShipCoreStatus.h"

struct ShipSnapshot
{
    EntityId                                id;
    ShipRole                                role;
    ShipTypeId                              typeId;
    
    ShipTransform                           transform;
    std::vector<SignalReceptionResult>      receptions;
    std::vector<game::RadarContact>         radarContacts;
    std::vector<game::damage::DamageEvent>  damageEvents;
    game::ShipCoreStatus                    shipCoreStatus;
};