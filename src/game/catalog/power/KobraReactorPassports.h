#pragma once

#include "src/game/catalog/CatalogTypes.h"

// Propulsion generators. Technical data: src/game/equipment/reactor/presets.
namespace game::catalog::power
{

inline constexpr std::string_view ER5PropulsionReactorId =
    "kb.reactor.er5-propulsion";

// ER-5 dedicated propulsion generator. Today BOTH Cobrett models mount it;
// N1/A3-specific generators are not authored yet (provisional).
[[nodiscard]] inline const ProductPassport& er5PropulsionReactor()
{
    static const ProductPassport passport {
        ER5PropulsionReactorId,
        ProductCategory::Reactor,
        "Cobretty Corporation",
        "ER-5",
        "Cobretty Cobra ER-5 Propulsion Reactor",
        "Cobrett",
        "ER-5 propulsion reactor",
        "",
        "ER5-PRP-0001",
        "KB-RCT-ER5-0001",
        "rev 0",
        PassportStatus::Provisional,
        "elite.reactor.cobra.cobretty.er5.v1",
        0.0, 0.0, 0.0, 0.0,
        "Shared by N1 and A3 until generation-specific units exist.",
        {
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu.n1",
             {"36A1A1"}},
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu-inta.a3",
             {"37A1A1", "37A2A1"}}
        }
    };
    return passport;
}

} // namespace game::catalog::power
