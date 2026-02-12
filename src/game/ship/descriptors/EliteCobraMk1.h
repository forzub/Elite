#pragma once
#include "game/ship/ShipDescriptor.h"
#include "game/ship/Ship.h"
#include "game/ship/cockpit/CockpitContours.h"
#include "game/ship/cockpit/CockpitIndicators.h"


struct CockpitLayout {
    std::vector<CockpitIndicatorLayout> indicators;
};

struct EliteCobraMk1{

    static const ShipDescriptor& EliteCobraMk1Descriptor(ShipRole role);  // Статический метод
    // static CockpitLayout cockpitLayout(const ShipDescriptor& desc);
    static CockpitGeometry createCockpitGeometry(const ShipDescriptor& desc);
    
};