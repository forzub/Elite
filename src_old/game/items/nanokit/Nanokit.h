#pragma once

#include <string>
#include "game/items/Item.h"

struct NanoKit : public Item
{
    int                 charges;
    std::string         label;    // чисто UI / лор

    bool usesCargoSpace() const override { return true; }
    int cargoMass() const override { return 1; }
    int cargoVolume() const override { return 1; }
};