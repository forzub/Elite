#pragma once

#include <array>

#include "src/game/catalog/CatalogTypes.h"

// Operational (economic/service) classes of ship models. A model may hold
// several, written "FRN-U/INT-A": a list of classes, never a flight mode.
namespace game::catalog::codes
{

inline constexpr CodeDescription FrontierUtility {
    "FRN-U",
    "Frontier Utility",
    "Universal frontier ship for remote systems, colonies and stations with "
    "limited infrastructure: people and small cargo, technical and supply "
    "work, long autonomous operation, repairable with modest means. A role, "
    "not a size class."
};

inline constexpr CodeDescription InterSystemArmedTransport {
    "INT-A",
    "Inter-system Armed Transport",
    "Light armed transport of cargo and people between systems: higher "
    "autonomy, operation on dangerous routes, defensive armament and more "
    "developed power and control systems."
};

inline constexpr std::array<const CodeDescription*, 2> operationalClasses {
    &FrontierUtility,
    &InterSystemArmedTransport
};

} // namespace game::catalog::codes
