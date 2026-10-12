#pragma once

#include <vector>

#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::plant
{

class NewtonianPhysicalControlModel final
{
public:
    [[nodiscard]] static const std::vector<EngineSlotDefinition>& slots(
        PropulsionLayout layout
    );

    [[nodiscard]] static PhysicalControlSnapshot evaluate(
        PropulsionLayout layout,
        const ShipParams& nominalPhysics,
        const std::vector<EngineSlotState>& state
    ) noexcept;
};

} // namespace game::ship::plant
