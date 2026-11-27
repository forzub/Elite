#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/game/ship/refit/RepairRefit.h"

namespace game::simulation
{
// Repair & Refit publication of an owned ship (F7). Ownership, registration
// and the last server-computed preview (RepairRefitPreview on the
// authoritative ShipCore) with the result of the last applied operation.
// Module states themselves travel in the ordinary ObjectGraphSnapshot.
struct ShipRefitSnapshot
{
    // ON THE WIRE: owned by the RECEIVING session's player (GameServer::projectShipRowsForSession, from
    // ShipOwnershipRegistry); in the server's canonical snapshot: the ship has a player owner. No owner id rides:
    // ownerActor was a shared actor of every player (F7 merged their fleets) - author 2026-11-27
    bool owned = false;
    std::string registrationId;
    std::string shipName;
    std::uint32_t previewSerial = 0;    // bumps on every new preview
    bool previewValid = false;
    game::ship::refit::RefitPreview preview;
    std::uint32_t applySerial = 0;      // bumps on every apply
    std::string applyResult;            // empty = applied, otherwise why not
    // per-instance configuration: "moduleId=productId" in the order applied - the client resolves the same descriptor
    // (refit::configuredDescriptor) instead of the bare catalog passport
    std::vector<std::string> productOverrides;
};
} // namespace game::simulation
