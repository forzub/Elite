#pragma once
#include <memory>
#include "src/game/equipment/types/RadarVisualProfile.h"


class RadarWidgetBase;

class RadarWidgetFactory
{
public:
    static std::unique_ptr<RadarWidgetBase>
    create(game::RadarVisualProfile profile);
};