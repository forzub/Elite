// GameSimulation: Repair & Refit (F7) previews and DEV applies on the
// authoritative ShipCore; published per owned ship in ShipSnapshot::refit.

#include "src/game/simulation/GameSimulationInternal.h"

namespace
{
// Both ships belong to the same real owner (or are the same ship).
bool sameOwner(const Ship& a, const Ship& b)
{
    const auto owner = a.core().registry().ownerActor;
    return owner != 0 && owner != ActorIds::Unknown() && owner == b.core().registry().ownerActor;
}
} // namespace

void GameSimulation::requestRefitPreview(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation)
{
    const Ship* commander = getShip(commandingShip);
    const EntityId target = subject.value == 0 ? commandingShip : subject;
    const Ship* ship = getShip(target);
    if (!commander || !ship || (target.value != commandingShip.value && !sameOwner(*commander, *ship)))
        return;
    auto& pub = m_refitPublications[target.value];
    pub.preview = game::ship::refit::previewRefit(ship->core(), operation);
    pub.previewValid = true;
    ++pub.previewSerial;
    std::cerr << "[Refit] preview ship=" << target.value << " module=" << operation.moduleId << " kind="
              << static_cast<int>(operation.kind) << " ok=" << pub.preview.ok << " deltas=" << pub.preview.deltas.size()
              << (pub.preview.error.empty() ? std::string() : " error=" + pub.preview.error) << '\n';
}

void GameSimulation::applyRefit(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation)
{
    const Ship* commander = getShip(commandingShip);
    const EntityId target = subject.value == 0 ? commandingShip : subject;
    Ship* ship = getShip(target);
    if (!commander || !ship || (target.value != commandingShip.value && !sameOwner(*commander, *ship)))
        return;
    auto& pub = m_refitPublications[target.value];
    pub.applyResult = game::ship::refit::applyRefitOperation(ship->core(), operation);
    ++pub.applySerial;
    // the shown preview is now history: recompute it on the new live state
    pub.preview = game::ship::refit::previewRefit(ship->core(), operation);
    ++pub.previewSerial;
    std::cerr << "[Refit] apply ship=" << target.value << " module=" << operation.moduleId << " result="
              << (pub.applyResult.empty() ? std::string("APPLIED") : pub.applyResult) << '\n';
}

game::simulation::ShipRefitSnapshot GameSimulation::refitSnapshotFor(EntityId shipId) const
{
    game::simulation::ShipRefitSnapshot out;
    const Ship* ship = getShip(shipId);
    if (!ship)
        return out;
    const auto& registry = ship->core().registry();
    out.owned = registry.ownerActor != 0 && registry.ownerActor != ActorIds::Unknown();
    if (!out.owned)
        return out;
    if (const auto it = m_refitPublications.find(shipId.value); it != m_refitPublications.end())
        out = it->second;
    out.owned = true;
    out.ownerActor = registry.ownerActor;
    out.registrationId = registry.registrationId;
    out.shipName = ship->core().visualIdentity().shipName.empty() ? ship->core().descriptor().identity.shipName
                                                                   : ship->core().visualIdentity().shipName;
    return out;
}
