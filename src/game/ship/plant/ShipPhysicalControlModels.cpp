#include "src/game/ship/plant/ShipPhysicalControlModels.h"

namespace game::ship::plant
{

const std::vector<EngineSlotDefinition>& physicalControlSlots(
    PhysicalControlModelId model
)
{
    if (familyOf(model) == PhysicalControlFamily::Assisted)
        return AssistedPhysicalControlModel::slots(layoutOf(model));

    return NewtonianPhysicalControlModel::slots(layoutOf(model));
}

PhysicalControlSnapshot evaluatePhysicalControlModel(
    PhysicalControlModelId model,
    const ShipParams& nominalPhysics,
    const std::vector<EngineSlotState>& state
) noexcept
{
    if (familyOf(model) == PhysicalControlFamily::Assisted)
    {
        return AssistedPhysicalControlModel::evaluate(
            layoutOf(model),
            nominalPhysics,
            state
        );
    }

    return NewtonianPhysicalControlModel::evaluate(
        layoutOf(model),
        nominalPhysics,
        state
    );
}

} // namespace game::ship::plant
