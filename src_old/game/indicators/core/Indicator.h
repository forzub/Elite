#pragma once
#include <string>
#include "game/ship/cockpit/ShipCockpitState.h"

class Indicator
{
public:
    virtual ~Indicator() = default;

    virtual void init() = 0;
    virtual void update(float value) = 0;
    virtual void apply(ShipCockpitState& state) = 0;

    virtual const std::string& id() const = 0;
};
