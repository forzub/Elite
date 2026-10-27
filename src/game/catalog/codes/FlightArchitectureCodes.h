#pragma once

#include <array>

#include "src/game/catalog/CatalogTypes.h"

// Flight/control generation codes and propulsion layout codes.
// A generation code (N1, A3) names the ship's flight/control architecture
// generation. A layout code (I, Y, X) names the main propulsion installation
// architecture. Neither is a flight-law switch nor an engine number.
namespace game::catalog::codes
{

inline constexpr CodeDescription NewtonianGeneration1 {
    "N1",
    "Newtonian Generation 1",
    "Older Newtonian flight/control architecture: mostly axial main thrust "
    "with limited vectoring, hull attitude independent of the velocity "
    "vector, active use of RCS, greater weight on pilot skill and "
    "autopilot planning."
};

inline constexpr CodeDescription AssistedGeneration3 {
    "A3",
    "Assisted Generation 3",
    "Modern Assisted flight/control architecture: developed thrust "
    "vectoring, automatic formation of the required forces and moments, "
    "easier handling, more complex equipment."
};

inline constexpr CodeDescription LayoutI {
    "I",
    "Axial main propulsion layout",
    "Main propulsion installed along the longitudinal axis. A propulsion "
    "installation architecture, not a generation or flight law."
};

inline constexpr std::array<const CodeDescription*, 2> flightGenerations {
    &NewtonianGeneration1,
    &AssistedGeneration3
};

} // namespace game::catalog::codes
