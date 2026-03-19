#pragma once


#include "src/game/damage/HitComponent.h"
#include "game/ship/ShipDescriptor.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace game::ship
{

class ShipHitBuilder
{
public:

    static void build(
        game::damage::HitComponent& hitComponent,
        const ShipDescriptor& descriptor
    );

    void loadVolumes(
        game::damage::HitComponent& component,
        const json& data
    );

private:

    static void buildRadiatorPanels(
        game::damage::HitComponent& hitComponent,
        const ShipDescriptor& descriptor);
};

}