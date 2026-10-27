#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"

namespace game::ship
{
// Topology of the loadout bound for `model`. Descriptors WITHOUT loadouts
// (legacy/test fixtures) fall back to the fixed legacy tables; production
// hulls always carry their loadouts.
[[nodiscard]] inline const std::vector<plant::EngineSlotDefinition>&
propulsionTopology(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model
)
{
    if (const auto* loadout = descriptor.findLoadout(model))
        return loadout->propulsionSlots;
    return plant::physicalControlSlots(model);
}

[[nodiscard]] inline const std::vector<ShipEngineReactorGroupDefinition>&
engineReactorGroups(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model
)
{
    if (const auto* loadout = descriptor.findLoadout(model))
        return loadout->engineReactorGroups;

    // Legacy fallback: the fixed installation skeleton, without module IDs.
    static const std::array<std::vector<ShipEngineReactorGroupDefinition>,6>
        legacy = [] {
            std::array<std::vector<ShipEngineReactorGroupDefinition>,6> out;
            for (std::size_t k = 0; k < out.size(); ++k)
            {
                const auto& slots = presets::cobra::installationSlots(
                    static_cast<plant::PhysicalControlModelId>(k));
                for (const auto& slot : slots.engineReactors)
                {
                    ShipEngineReactorGroupDefinition group;
                    group.slotId = slot.slotId;
                    group.normalizedPositionBody = slot.normalizedPositionBody;
                    for (const char* engine : slot.poweredEngineSlots)
                        group.poweredEngineSlotIds.emplace_back(engine);
                    out[k].push_back(std::move(group));
                }
            }
            return out;
        }();
    return legacy[static_cast<std::size_t>(model)];
}

// Pure plant/capability code still reads the fixed physicalControlSlots()
// and RCS nozzle tables by model. Until those consumers take the loadout,
// a loadout must describe EXACTLY that topology, otherwise the server
// would allocate against one geometry and project capability from another.
inline void requireLoadoutMatchesLegacyTopology(
    const ShipLoadoutDefinition& loadout
)
{
    const auto fail = [&](const char* what) {
        throw std::logic_error(
            "Loadout " + loadout.id +
            " diverges from legacy topology: " + what);
    };
    const auto& legacy = plant::physicalControlSlots(loadout.legacyModelId);
    if (loadout.propulsionSlots.size() != legacy.size())
        fail("slot count");
    for (std::size_t i = 0; i < legacy.size(); ++i)
    {
        const auto& a = loadout.propulsionSlots[i];
        const auto& b = legacy[i];
        if (std::strcmp(a.id, b.id) != 0 ||
            a.role != b.role ||
            a.normalizedPositionBody != b.normalizedPositionBody ||
            a.nominalForceDirectionBody != b.nominalForceDirectionBody ||
            a.freedom != b.freedom)
            fail("slot definition");
    }
    if (loadout.propulsionModuleIds.size() != legacy.size())
        fail("module binding count");

    if (loadout.rcsNozzles.empty())
        return;
    const auto& nozzles = presets::cobra::newtonianRcsNozzles();
    if (loadout.rcsNozzles.size() != nozzles.size())
        fail("RCS nozzle count");
    for (std::size_t i = 0; i < nozzles.size(); ++i)
    {
        const auto& a = loadout.rcsNozzles[i];
        const auto& b = nozzles[i];
        if (a.nozzleId != b.nozzleId ||
            a.nodeId != b.nodeId ||
            a.positionBody != b.positionBody ||
            a.forceDirectionBody != b.forceDirectionBody)
            fail("RCS nozzle definition");
    }
}
} // namespace game::ship
