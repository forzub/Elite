#pragma once

// Cobra's FIELD-COUPLED fixed RCS nozzles recirculate argon in a closed
// high-pressure distribution loop. This is a GAME-UNIVERSE working medium,
// NOT an open-cycle chemical rocket expelling 500 kg per engagement.
// Liquid inventory stays aboard; the float resource describes the fraction
// currently available in the active pressure manifold. The balance waits in
// the cooler/return buffer. Repressurization consumes genuine ER-5 power.
// Ambient interplanetary space is NOT an automatic refuelling source.
// Future station or atmospheric collection must explicitly add mass.
namespace game::ship::plant::rcsWorkingFluid
{
inline constexpr double ArgonInventoryKg = 500.0;
// Approximate liquid argon density near its normal boiling point (87K).
inline constexpr double ArgonLiquidDensityKgPerM3 = 1396.0;
inline constexpr double CompressorRatedElectricalMW = 2.0;
inline constexpr double LiquidTankVolumeM3 =
    ArgonInventoryKg / ArgonLiquidDensityKgPerM3;
}
