#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"

// One installable hardware configuration of a hull (Cobra "Assisted-I" etc.).
// The hull descriptor owns its loadouts; runtime binds ONE of them. Topology,
// installed module identities and generator wiring are data of the selected
// loadout, never a lookup keyed by ship type.

struct ShipEngineReactorGroupDefinition
{
    std::string slotId;
    glm::dvec3 normalizedPositionBody {0.0};
    // Propulsion slot IDs (ShipLoadoutDefinition::propulsionSlots[].id)
    // powered by this generator.
    std::vector<std::string> poweredEngineSlotIds;
    std::string reactorModuleId;
};

struct ShipRcsNozzleMountDefinition
{
    std::string nozzleId;
    std::string nodeId;
    glm::dvec3 positionBody {0.0};
    glm::dvec3 forceDirectionBody {0.0};
};

struct ShipLoadoutDefinition
{
    std::string id;
    // Temporary legacy loadout index: wire, CSV and diagnostics still carry
    // PhysicalControlModelId. It selects a loadout; it is not topology.
    game::ship::plant::PhysicalControlModelId legacyModelId =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    game::navigation::LocalFlightControlLaw flightLaw =
        game::navigation::LocalFlightControlLaw::Assisted;

    // Main propulsion and aggregate RCS-node slots, in canonical slot order.
    std::vector<game::ship::plant::EngineSlotDefinition> propulsionSlots;
    // Installed module bound to each propulsion slot, same order and size.
    std::vector<std::string> propulsionModuleIds;
    std::vector<ShipEngineReactorGroupDefinition> engineReactorGroups;
    // Individually installed fixed-direction RCS nozzles; empty if none.
    std::vector<ShipRcsNozzleMountDefinition> rcsNozzles;
};
