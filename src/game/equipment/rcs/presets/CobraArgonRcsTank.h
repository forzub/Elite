#pragma once

#include "src/game/equipment/rcs/RcsGasTankUnit.h"

namespace game::equipment::rcs
{

// Cobra AT-500: the argon tank of the field-coupled RCS loop (closed loop:
// the gas returns through the cooler/compressor, nothing is vented). Authored
// GAME-UNIVERSE ratings.
//  - 500 kg liquid argon (1396 kg/m^3 near 87 K) = 0.358 m^3, in a 0.45 m^3
//    vessel (20 % ullage), 300 bar design / 750 bar burst (2.5 x).
//  - 150 s of full RCS draw (about 300 m/s of RCS delta-v at 2 m/s^2) and
//    200 s for the 2 MW compressor to refill it from empty: holding the
//    RCS key is not a way to fly; one autopilot route's turns and trims
//    (measured 26-51 s of full draw) leave a reserve above the regulator.
[[nodiscard]] inline const RcsGasTankDescriptor& cobraArgonRcsTank() noexcept
{
    static const RcsGasTankDescriptor tank = []
    {
        RcsGasTankDescriptor t;
        t.typeId = "cobra.rcs.argon-tank.at500.v1";
        t.displayName = "Cobra AT-500 RCS argon tank";
        t.manufacturer = "Cobra";
        t.workingFluid = "argon";
        t.inventoryKg = 500.0;
        t.liquidDensityKgPerM3 = 1396.0;
        t.internalVolumeM3 = 0.45;
        t.designPressureBar = 300.0;
        t.burstPressureBar = 750.0;
        t.dryMassKg = 180.0;
        t.lengthM = 1.6;
        t.diameterM = 0.65;
        t.fullDrawSeconds = 150.0;
        t.fullRecoverySeconds = 200.0;
        t.restartFraction = 0.20;
        t.regulatorFullThrustFraction = 0.20;
        t.compressorRatedElectricalMW = 2.0;
        t.failure.failureAtDamage01 = 0.95;
        t.maintenance.maintenanceWearThreshold01 = 0.20;
        return t;
    }();
    return tank;
}

} // namespace game::equipment::rcs
