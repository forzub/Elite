#pragma once

#include "src/game/catalog/CatalogTypes.h"

// Main propulsion units. Technical data: src/game/equipment/engine/presets.
namespace game::catalog::propulsion
{

inline constexpr std::string_view KobraN1AxialMainDriveId =
    "kb.engine.n1-axial";
inline constexpr std::string_view CR1VectorMainDriveId =
    "kb.engine.cr1-vector";

// Older N1-generation Newtonian main drive: strong mostly axial thrust, a
// narrow nozzle deflection for gentle thrust-vector correction, simple and
// serviceable. RCS does lateral trims; large direction changes rotate the
// hull. Thrust/power/thermal figures are provisional copies of CR-1; the
// +/-7 deg nozzle travel is a provisional legacy value.
[[nodiscard]] inline const ProductPassport& kobraN1AxialMainDrive()
{
    static const ProductPassport passport {
        KobraN1AxialMainDriveId,
        ProductCategory::MainPropulsion,
        "Kobersberg-Braunauer Raumfahrzeugwerke GmbH",
        "KB-N1-AX",
        "KOBRA N1 Axial Main Drive",
        "Cobrett",
        "N1 axial main drive",
        "N1",
        "KB-N1AX-0001",
        "KB-ENG-N1-0001",
        "rev 0",
        PassportStatus::Provisional,
        "elite.engine.kobra.n1-axial.v1",
        0.0, 0.0, 0.0, 0.0,
        "Provisional: thrust, power, thermal and wear copied from CR-1; "
        "nozzle travel +/-7 deg is a legacy value pending flight selection.",
        {
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu.n1",
             {"36A1A2"}}
        }
    };
    return passport;
}

// Modern A3-generation Assisted wide-vector main drive (current CR-1):
// field-steered nozzle with a large thrust-vector envelope (~75 deg) that
// produces substantial cross-axis thrust for Assisted flight.
[[nodiscard]] inline const ProductPassport& cr1VectorMainDrive()
{
    static const ProductPassport passport {
        CR1VectorMainDriveId,
        ProductCategory::MainPropulsion,
        "Cobretty Corporation",
        "CR-1",
        "Cobretty Cobra CR-1 Vector Main Drive",
        "Cobrett",
        "CR-1 wide-vector main drive",
        "A3",
        "CR1-VEC-0001",
        "KB-ENG-A3-0001",
        "rev 0",
        PassportStatus::Provisional,
        "elite.engine.cobretty.cr1-vector.v1",
        0.0, 0.0, 0.0, 0.0,
        "Manufacturer name is the legacy preset value pending lore review.",
        {
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu-inta.a3",
             {"37A1A2", "37A1A3", "37A2A2", "37A2A3"}}
        }
    };
    return passport;
}

} // namespace game::catalog::propulsion
