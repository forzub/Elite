#pragma once

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/ShipTypeId.h"

class ShipDescriptorRegistry
{
public:
    static const ShipDescriptor& get(ShipTypeId id);
};
