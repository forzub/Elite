#pragma once
#include <memory>
#include "src/game/equipment/types/RadarVisualProfile.h"
#include "src/game/equipment/types/RadarType.h"

class RadarWidgetBase;

class RadarWidgetFactory
{
public:
    static std::unique_ptr<RadarWidgetBase>
    create(game::RadarType type, game::RadarVisualProfile visualProfile);
};