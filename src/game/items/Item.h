#pragma once
#include <string>

struct Item
{
    virtual ~Item() = default;

    // влияет ли на груз (габариты и масса)
    virtual bool usesCargoSpace() const { return false; }

    // если да
    virtual int cargoMass()   const { return 0; }
    virtual int cargoVolume() const { return 0; }
};
