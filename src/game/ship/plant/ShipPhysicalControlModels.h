#pragma once

#include <vector>

#include "src/game/ship/plant/AssistedPhysicalControlModel.h"
#include "src/game/ship/plant/NewtonianPhysicalControlModel.h"

namespace game::ship::plant
{

[[nodiscard]] const std::vector<EngineSlotDefinition>& physicalControlSlots(
    PhysicalControlModelId model
);

[[nodiscard]] PhysicalControlSnapshot evaluatePhysicalControlModel(
    PhysicalControlModelId model,
    const ShipParams& nominalPhysics,
    const std::vector<EngineSlotState>& state
) noexcept;

} // namespace game::ship::plant
