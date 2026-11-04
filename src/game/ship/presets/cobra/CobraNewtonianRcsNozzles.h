#pragma once

// Cobra alias of the canonical Newtonian RCS bank (8 mount nodes x 3 fixed
// outward nozzles, plant::canonical::newtonianRcsNozzles). Cobra's nozzle
// positions ARE the canonical ones; other hulls carry their own passport
// positions in their loadout (ShipLoadoutDefinition::rcsNozzles).
#include <array>
#include <string_view>
#include "src/game/ship/plant/CanonicalActuatorLayouts.h"

namespace game::ship::presets::cobra
{
using plant::canonical::RcsNozzleMount;
using plant::canonical::newtonianRcsNozzles;
} // namespace game::ship::presets::cobra
