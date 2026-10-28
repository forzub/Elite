#pragma once

#include "src/game/catalog/CatalogTypes.h"
#include "src/game/equipment/rcs/presets/CobraArgonRcsTank.h"

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

inline constexpr std::string_view ArgonRcsTankId = "kb.rcs.argon-tank.at500";

// The RCS group's working-gas tank. Technical data:
// src/game/equipment/rcs/presets/CobraArgonRcsTank.h
[[nodiscard]] inline const ProductPassport& argonRcsTank()
{
    static const ProductPassport passport = []
    {
        const auto& tank = game::equipment::rcs::cobraArgonRcsTank();
        return ProductPassport {
            ArgonRcsTankId,
            ProductCategory::FluidStorage,
            "Cobra",
            "AT-500",
            "Cobra AT-500 RCS argon tank",
            "Cobrett",
            "Composite-overwrapped argon pressure vessel, 0.45 m3, "
            "300 bar design / 750 bar burst",
            "N1",
            "AT500-0001",
            "KB-TNK-N1-0001",
            "rev 0",
            PassportStatus::Provisional,
            tank.typeId,
            tank.dryMassKg + tank.inventoryKg,
            tank.lengthM, tank.diameterM, tank.diameterM,
            "Feeds all 24 RCS nozzles of the closed argon loop (500 kg "
            "liquid argon). Dry mass 180 kg. A damaged vessel holds less "
            "usable gas; a failed one leaves the RCS without gas.",
            {
                {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu.n1", {}}
            }
        };
    }();
    return passport;
}

} // namespace game::catalog::rcs
