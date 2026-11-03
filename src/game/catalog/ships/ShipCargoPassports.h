#pragma once

// Authoritative cargo capacity of the ship classes (one source for the ship
// descriptors, the dock traffic and tools/colonization_lab/canon/ship_economics.json).
//
// Standard cargo container C20: payload up to 20 t, inner volume 23.3 m3,
// tare 1.7 t. A hull carries a whole number of container places; its
// structural / thrust payload limit may be reached before the places are full.
namespace catalog::ships
{

struct ShipCargoPassport
{
    double dryMassKg = 0.0;
    int cargoTonnes = 0;        // NET payload limit with every C20 place loaded
    int cargoVolumeM3 = 0;
    int containerSlots = 0;     // C20 places
};

inline constexpr int C20PayloadTonnes = 20;
inline constexpr double C20VolumeM3 = 23.3;
inline constexpr double C20TareTonnes = 1.7;

// Mass the hull really carries: net payload + tare of every loaded C20 place.
// The jump charge, burn and traffic kinematics use this, never the net value.
[[nodiscard]] constexpr double grossCargoTonnes(const ShipCargoPassport& p) noexcept
{
    return p.cargoTonnes + p.containerSlots * C20TareTonnes;
}

// Cobrett-I N1: six C20 places in the rear hold (6 x 23.3 = 140 m3); typical mixed
// cargo is volume-limited (~0.4 t/m3 -> ~56 t), dense cargo reaches 120 t.
inline constexpr ShipCargoPassport CobrettIN1Cargo {260000.0, 120, 140, 6};

// OCS-4 "Lastochka" (orbital only: 0.4 g, no landing gear or heat shield): 3.3 MN gives
// the passport 4 m/s2 at 830 t, i.e. 450 t GROSS cargo. With all 24 C20 places loaded
// that is 409 t net + 40.8 t tare (dense containers); the rest of the 2600 m3 bay takes
// loose and bulk cargo under the same 450 t gross limit.
inline constexpr ShipCargoPassport Ocs4Cargo {380000.0, 409, 2600, 24};

} // namespace catalog::ships
