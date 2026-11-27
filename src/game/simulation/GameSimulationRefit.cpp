// GameSimulation: Repair & Refit (F7) previews and DEV applies on the
// authoritative ShipCore; published per owned ship in ShipSnapshot::refit.

#include "src/game/simulation/GameSimulationInternal.h"

bool GameSimulation::isPlayerOwnedShip(const Ship& ship) const noexcept
{
    return m_playerOwnedShipInstances.count(ship.core().registry().instanceId) != 0;
}

void GameSimulation::requestRefitPreview(EntityId commandingShip, EntityId subject, const game::ship::refit::RefitOperation& operation)
{
    const Ship* commander = getShip(commandingShip);
    const EntityId target = subject.value == 0 ? commandingShip : subject;
    const Ship* ship = getShip(target);
    if (!commander || !ship)   // authority: GameServer::refitAuthority, before this call
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
    if (!commander || !ship)   // authority: GameServer::refitAuthority, before the charge and this call
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

void GameSimulation::refuseRefit(EntityId commandingShip, EntityId subject, const std::string& why)
{
    const EntityId target = subject.value == 0 ? commandingShip : subject;
    auto& pub = m_refitPublications[target.value];
    pub.applyResult = why;
    ++pub.applySerial;
    std::cerr << "[Refit] apply ship=" << target.value << " REFUSED: " << why << '\n';
}

game::simulation::ShipRefitSnapshot GameSimulation::refitSnapshotFor(EntityId shipId) const
{
    game::simulation::ShipRefitSnapshot out;
    const Ship* ship = getShip(shipId);
    if (!ship)
        return out;
    const auto& registry = ship->core().registry();
    // the configuration is VISIBLE (the client resolves the configured descriptor of every ship): always published
    out.productOverrides = ship->core().productOverrides();
    // the rest is the owner's: a player-owned ship (ShipOwnershipRegistry). In the canonical snapshot "owned" means
    // "has a player owner"; GameServer::projectShipRowsForSession turns it into "owned by THIS viewer's player"
    if (!isPlayerOwnedShip(*ship))
        return out;
    if (const auto it = m_refitPublications.find(shipId.value); it != m_refitPublications.end())
        out = it->second;
    out.owned = true;
    out.registrationId = registry.registrationId;
    out.productOverrides = ship->core().productOverrides();
    out.shipName = ship->core().visualIdentity().shipName.empty() ? ship->core().descriptor().identity.shipName
                                                                   : ship->core().visualIdentity().shipName;
    return out;
}
