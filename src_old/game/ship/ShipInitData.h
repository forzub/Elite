#pragma once

#include <vector>
#include "ShipVisualIdentity.h"
#include "ShipRegistry.h"
#include "game/items/Item.h"

struct ShipInitData
{
    ShipVisualIdentity   visual;
    ShipRegistry         registry;

    std::vector<Item*>   initialInventory;
};
