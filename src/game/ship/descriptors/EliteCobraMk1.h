#pragma once
#include "game/ship/ShipDescriptor.h"
#include "game/ship/Ship.h"
#include "game/ship/cockpit/CockpitContours.h"

struct EliteCobraMk1{

    static const ShipDescriptor& EliteCobraMk1Descriptor();  // Статический метод
    CockpitGeometry createCockpitGeometry();
    
};