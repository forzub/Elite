#pragma once

#include <cstdint>
#include <string>

#include "src/game/ship/refit/RepairRefit.h"

namespace game::simulation
{
// Repair & Refit publication of an owned ship (F7). Ownership, registration
// and the last server-computed preview (RepairRefitPreview on the
// authoritative ShipCore) with the result of the last applied operation.
// Module states themselves travel in the ordinary ObjectGraphSnapshot.
struct ShipRefitSnapshot
{
    bool owned = false;                 // the ship has a real owner (player / organisation)
    std::uint32_t ownerActor = 0;
    std::string registrationId;
    std::string shipName;
    std::uint32_t previewSerial = 0;    // bumps on every new preview
    bool previewValid = false;
    game::ship::refit::RefitPreview preview;
    std::uint32_t applySerial = 0;      // bumps on every apply
    std::string applyResult;            // empty = applied, otherwise why not
};
} // namespace game::simulation
