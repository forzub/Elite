#pragma once

// Cobra Mk1 hull: its six installable loadouts, assembled 1:1 from the
// existing Cobra topology presets. No coordinate, module ID, slot order or
// generator wiring is changed here; this only gives the data an owner.

#include <stdexcept>
#include <string>
#include <vector>

#include "src/game/ship/ShipLoadout.h"
#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"
#include "src/game/ship/presets/cobra/CobraPropulsionPresets.h"

namespace game::ship::presets::cobra
{
[[nodiscard]] inline std::vector<ShipLoadoutDefinition> cobraLoadouts()
{
    using Model = PhysicalControlModelId;

    std::vector<ShipLoadoutDefinition> loadouts;
    for (const Model model : {
             Model::AssistedI, Model::AssistedY, Model::AssistedX,
             Model::NewtonianI, Model::NewtonianY, Model::NewtonianX})
    {
        const bool assisted =
            plant::familyOf(model) == plant::PhysicalControlFamily::Assisted;

        ShipLoadoutDefinition loadout;
        loadout.id = plant::physicalControlModelName(model);
        loadout.legacyModelId = model;
        loadout.flightLaw = plant::nativeFlightLaw(model);
        loadout.propulsionSlots = propulsionSlots(model);
        loadout.propulsionModuleIds = assisted
            ? installedAssistedEngineModuleIds(model)
            : installedNewtonianEngineModuleIds(model);

        const auto reactorIds = assisted
            ? installedAssistedReactorModuleIds(model)
            : installedNewtonianReactorModuleIds(model);
        const auto& groups = installationSlots(model).engineReactors;
        if (reactorIds.size() != groups.size())
            throw std::logic_error(
                "Invalid Cobra loadout reactor bindings: " + loadout.id);
        for (std::size_t i = 0; i < groups.size(); ++i)
        {
            ShipEngineReactorGroupDefinition group;
            group.slotId = groups[i].slotId;
            group.normalizedPositionBody = groups[i].normalizedPositionBody;
            for (const char* engine : groups[i].poweredEngineSlots)
                group.poweredEngineSlotIds.emplace_back(engine);
            group.reactorModuleId = reactorIds[i];
            loadout.engineReactorGroups.push_back(std::move(group));
        }

        if (!assisted)
        {
            for (const auto& nozzle : newtonianRcsNozzles())
            {
                ShipRcsNozzleMountDefinition mount;
                mount.nozzleId = std::string(nozzle.nozzleId);
                mount.nodeId = std::string(nozzle.nodeId);
                mount.positionBody = nozzle.positionBody;
                mount.forceDirectionBody = nozzle.forceDirectionBody;
                loadout.rcsNozzles.push_back(std::move(mount));
            }
        }

        // Two canonical ship models share the I layout but are DIFFERENT
        // products/generations (game::catalog::ships::CobrettPassports.h).
        if (model == Model::NewtonianI)
            loadout.shipPassportId = "kb.ship.cobrett-i.frnu.n1";
        else if (model == Model::AssistedI)
            loadout.shipPassportId = "kb.ship.cobrett-i.frnu-inta.a3";

        loadouts.push_back(std::move(loadout));
    }
    return loadouts;
}
} // namespace game::ship::presets::cobra
