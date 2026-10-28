#pragma once

#include "src/game/equipment/rcs/presets/CobraArgonRcsTank.h"

// Cobra's FIELD-COUPLED fixed RCS nozzles recirculate argon in a closed
// high-pressure distribution loop. This is a GAME-UNIVERSE working medium,
// NOT an open-cycle chemical rocket expelling 500 kg per engagement.
// Liquid inventory stays aboard; the float resource describes the fraction
// currently available in the active pressure manifold. The balance waits in
// the cooler/return buffer. Repressurization consumes genuine ER-5 power.
// Ambient interplanetary space is NOT an automatic refuelling source.
// Future station or atmospheric collection must explicitly add mass.
// The numbers belong to the installed tank's passport (Cobra AT-500).
namespace game::ship::plant::rcsWorkingFluid
{
inline const double ArgonInventoryKg =
    game::equipment::rcs::cobraArgonRcsTank().inventoryKg;
inline const double ArgonLiquidDensityKgPerM3 =
    game::equipment::rcs::cobraArgonRcsTank().liquidDensityKgPerM3;
inline const double CompressorRatedElectricalMW =
    game::equipment::rcs::cobraArgonRcsTank().compressorRatedElectricalMW;
inline const double LiquidTankVolumeM3 =
    ArgonInventoryKg / ArgonLiquidDensityKgPerM3;
}
