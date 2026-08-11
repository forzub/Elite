#pragma once

#include <cstdint>

namespace game::navigation
{

// Local flight law inside the ship-owned travel frame. Neither law changes
// the large-scale kinematics of that frame; future J propulsion owns that.
enum class LocalFlightControlLaw : std::uint8_t
{
    Newtonian = 0,
    Assisted = 1
};

// Persistent attitude/autobrake action requested by the pilot. These actions
// are fulfilled through the normal angular/linear acceleration limits; they
// are not instantaneous teleports of orientation or velocity.
enum class VelocityAlignmentMode : std::uint8_t
{
    None = 0,
    ForwardToVelocity,
    BackwardToVelocity,
    BrakeToStop
};

inline const char* localFlightControlLawName(
    LocalFlightControlLaw law
) noexcept
{
    switch (law)
    {
        case LocalFlightControlLaw::Assisted:
            return "ASSISTED";

        case LocalFlightControlLaw::Newtonian:
        default:
            return "NEWTONIAN";
    }
}

} // namespace game::navigation
