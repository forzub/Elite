#pragma once

#include "src/game/catalog/CatalogTypes.h"

// Manoeuvre thrusters. Technical data:
// src/game/equipment/engine/ManoeuvreEnginePreset.h
namespace game::catalog::rcs
{

inline constexpr std::string_view FieldCoupledRcsNozzleId =
    "kb.rcs.field-coupled-nozzle";

[[nodiscard]] inline const ProductPassport& fieldCoupledRcsNozzle()
{
    static const ProductPassport passport {
        FieldCoupledRcsNozzleId,
        ProductCategory::ManoeuvreThruster,
        "Cobra",
        "RCS-FC",
        "Cobra electric manoeuvre nozzle",
        "Cobrett",
        "Fixed-direction field-coupled RCS nozzle",
        "N1",
        "RCSFC-0001",
        "KB-RCS-N1-0001",
        "rev 0",
        PassportStatus::Provisional,
        "cobra.rcs.electric.fixed.v1",
        0.0, 0.0, 0.0, 0.0,
        "24 units on Cobrett-I N1 (8 mount nodes x 3 axes).",
        {
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu.n1", {}}
        }
    };
    return passport;
}

} // namespace game::catalog::rcs
