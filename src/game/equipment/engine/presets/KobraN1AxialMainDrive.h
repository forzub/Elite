#pragma once

#include <cstdlib>

#include "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h"

namespace game::equipment::engine::presets
{
// KOBERSBERG-BRAUNAUER — N1-generation axial main drive (Cobrett-I N1).
// Older, simple, serviceable Newtonian main propulsion unit: strong mostly
// axial thrust and only a narrow nozzle deflection for gentle thrust-vector
// correction. Lateral trims belong to the separate translational RCS; large
// thrust-direction changes come from rotating the hull.
//
// PROVISIONAL: thrust, electrical input, thermal and wear figures are taken
// over unchanged from the current CR-1 until N1 hardware is authored on its
// own. Only the vectoring envelope is an N1 property today.
[[nodiscard]] inline EngineUnitDescriptor KobraN1AxialMainDrive() noexcept
{
    EngineUnitDescriptor e = CobrettyCR1VectorMainDrive();
    e.engineTypeId = "elite.engine.kobra.n1-axial.v1";
    e.engineDisplayName = "KOBRA N1 Axial Main Drive";
    e.manufacturerName = "Kobersberg-Braunauer Raumfahrzeugwerke GmbH";
    // Product identity: src/game/catalog/propulsion/KobraMainDrivePassports.h

    // Narrow mechanical nozzle travel and slew. +/-7 deg selected by the
    // 2026-10-28 sweep: the smallest travel at which every route (healthy
    // I/Y/X, narrow-zone fixture) reaches the safe-tube accuracy plateau;
    // below 4 deg Y/X lose their margin, at 0 deg they leave the corridor.
    e.nominalGimbalRangeRad = 0.122173048;   // 7 deg
    e.nominalGimbalRateRadPerSec = 0.139626340; // 8 deg/s
    // DEV sweep hook (tools/navigation_runtime/newtonian/gimbal_sweep.sh).
    if (const char* deg = std::getenv("ELITE_DEV_N1_GIMBAL_DEG"))
        e.nominalGimbalRangeRad =
            std::atof(deg) * 3.14159265358979323846 / 180.0;
    return e;
}
} // namespace game::equipment::engine::presets
