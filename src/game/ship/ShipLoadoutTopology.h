#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/plant/ActuatorTopology.h"
#include "src/game/ship/plant/CanonicalActuatorLayouts.h"

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
                const auto& slots = plant::canonical::installationSlots(
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

// Installed module ids of the 24-nozzle Newtonian RCS bank of the loadout
// bound for `model` (its own ids, e.g. "C-120-A5A1.rcs_fore_upper_left_x");
// the canonical ids for loadout-less legacy/test descriptors. Views into the
// descriptor / canonical table: valid while the descriptor lives.
struct RcsNozzleIdView
{
    std::string_view nozzleId;
    std::string_view nodeId;
};

[[nodiscard]] inline std::array<RcsNozzleIdView, 24> rcsNozzleIds(
    const ShipDescriptor& descriptor,
    plant::PhysicalControlModelId model
)
{
    std::array<RcsNozzleIdView, 24> out {};
    const auto* loadout = descriptor.findLoadout(model);
    if (loadout && loadout->rcsNozzles.size() == out.size())
    {
        for (std::size_t i = 0; i < out.size(); ++i)
            out[i] = {loadout->rcsNozzles[i].nozzleId, loadout->rcsNozzles[i].nodeId};
        return out;
    }
    const auto& canonical = plant::canonical::newtonianRcsNozzles();
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = {canonical[i].nozzleId, canonical[i].nodeId};
    return out;
}

// A loadout must keep the STRUCTURE of its physical control model: the same
// slot count, ids, roles, force directions and freedoms, and (with RCS) the
// 24-nozzle bank's node/direction pattern. Mount coordinates are the ship's
// own (design passport) and are free. Throws on a structural divergence.
inline void requireLoadoutMatchesModelStructure(
    const ShipLoadoutDefinition& loadout
)
{
    const auto fail = [&](const char* what) {
        throw std::logic_error(
            "Loadout " + loadout.id +
            " diverges from its physical model: " + what);
    };
    const auto& model = plant::physicalControlSlots(loadout.legacyModelId);
    if (loadout.propulsionSlots.size() != model.size())
        fail("slot count");
    for (std::size_t i = 0; i < model.size(); ++i)
    {
        const auto& a = loadout.propulsionSlots[i];
        const auto& b = model[i];
        if (std::strcmp(a.id, b.id) != 0 ||
            a.role != b.role ||
            a.nominalForceDirectionBody != b.nominalForceDirectionBody ||
            a.freedom != b.freedom)
            fail("slot definition");
    }
    if (loadout.propulsionModuleIds.size() != model.size())
        fail("module binding count");

    if (loadout.rcsNozzles.empty())
        return;
    const auto& nozzles = plant::canonical::newtonianRcsNozzles();
    if (loadout.rcsNozzles.size() != nozzles.size())
        fail("RCS nozzle count");
    for (std::size_t i = 0; i < nozzles.size(); ++i)
    {
        const auto& a = loadout.rcsNozzles[i];
        const auto& b = nozzles[i];
        if (a.nodeId != b.nodeId ||
            a.forceDirectionBody != b.forceDirectionBody)
            fail("RCS nozzle definition");
    }
}

// Validates the loadout structure and registers its coordinates as the
// actuator topology every lever-arm consumer reads (plant/ActuatorTopology.h).
// The key is derived from the passport and loadout ids, so it is stable
// across processes (autopilot case files keep it as raw bytes).
inline void registerLoadoutTopology(ShipLoadoutDefinition& loadout)
{
    requireLoadoutMatchesModelStructure(loadout);
    plant::ActuatorTopology topology;
    topology.model = loadout.legacyModelId;
    topology.slots = loadout.propulsionSlots;
    if (loadout.rcsNozzles.size() == topology.rcs.size())
        for (std::size_t i = 0; i < topology.rcs.size(); ++i)
            topology.rcs[i] = {loadout.rcsNozzles[i].positionBody,
                               loadout.rcsNozzles[i].forceDirectionBody};
    else
        topology.rcs = plant::canonicalRcsNozzleBank();
    loadout.topologyKey = plant::actuatorTopologyKey(
        loadout.shipPassportId + "|" + loadout.id);
    plant::registerActuatorTopology(loadout.topologyKey, std::move(topology));
}

} // namespace game::ship
