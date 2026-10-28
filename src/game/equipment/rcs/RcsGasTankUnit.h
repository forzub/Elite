#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

#include "src/game/equipment/EquipmentDamage.h"

// RCS working-gas tank: one installed equipment TYPE (passport in
// src/game/catalog/rcs). The tank stores the working gas the RCS nozzles
// consume; its pressure, not a hidden budget, decides what they deliver.
// Gameplay law (cheap, readable to the player):
//  - a firing RCS draws the stored gas; the compressor refills the tank
//    only while the nozzles draw nothing;
//  - below the regulator pressure the nozzles weaken in proportion to the
//    pressure (a nearly empty tank gives weak thrusters, an empty one none
//    until it is refilled to the restart pressure);
//  - a damaged tank holds less usable gas (drains and refills faster); a
//    failed or destroyed tank gives the RCS no gas at all.
namespace game::equipment::rcs
{

struct RcsGasTankDescriptor
{
    std::string_view typeId {};
    std::string_view displayName {};
    std::string_view manufacturer {};
    std::string_view workingFluid {};
    // Physical passport data.
    double inventoryKg = 0.0;
    double liquidDensityKgPerM3 = 0.0;
    double internalVolumeM3 = 0.0;
    double designPressureBar = 0.0;
    double burstPressureBar = 0.0;
    double dryMassKg = 0.0;
    double lengthM = 0.0;
    double diameterM = 0.0;
    // Gameplay ratings of a healthy tank.
    // Seconds of full RCS draw from a full tank to empty.
    double fullDrawSeconds = 0.0;
    // Seconds the rated compressor needs from empty to full.
    double fullRecoverySeconds = 0.0;
    // An emptied tank restarts the nozzles at this fill.
    double restartFraction = 0.20;
    // Fill above which the regulator still gives the nozzles full thrust.
    double regulatorFullThrustFraction = 0.20;
    double compressorRatedElectricalMW = 0.0;
    game::equipment::EquipmentFailureProfile failure {0.95};
    game::equipment::EquipmentMaintenanceProfile maintenance {};
};

// Usable share of the rated capacity of an installed tank (0 = no gas).
[[nodiscard]] inline double rcsGasTankCapacity01(
    const RcsGasTankDescriptor& tank,
    const game::equipment::EquipmentDamageState& damage,
    bool installedAndAlive) noexcept
{
    if (!installedAndAlive)
        return 0.0;
    const auto function = game::equipment::evaluateEquipmentFunctionAtDamage01(
        game::equipment::totalAccumulatedDamage01(damage), tank.failure);
    return function.failed ? 0.0 : std::clamp(function.authority01, 0.0, 1.0);
}

// Nozzle thrust share the tank pressure allows (regulator law).
[[nodiscard]] inline double rcsGasThrustShare01(
    const RcsGasTankDescriptor& tank, double fill01, bool depleted) noexcept
{
    if (depleted)
        return 0.0;
    const double knee = std::clamp(tank.regulatorFullThrustFraction, 0.0, 1.0);
    const double fill = std::clamp(std::isfinite(fill01) ? fill01 : 0.0, 0.0, 1.0);
    return knee > 1.0e-9 ? std::min(1.0, fill / knee) : 1.0;
}

} // namespace game::equipment::rcs
