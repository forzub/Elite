#pragma once

#include "src/game/catalog/CatalogTypes.h"

// Jump-system products installed on catalog ships (FE-30 passports: Cobrett 36A5 / 37A5).
// Design data: src/assets/data/ships/ship_catalog_3126.json (um.core.z05, std.tank.jumpcharge).
// No runtime preset yet (the jump is not implemented): technicalTypeId stays empty.
namespace game::catalog::jump
{

inline constexpr std::string_view JumpCoreZ05Id = "um.core.z05";
inline constexpr std::string_view JumpChargeTankId = "std.tank.jumpcharge";

[[nodiscard]] inline const ProductPassport& jumpCoreZ05()
{
    static const ProductPassport passport {
        JumpCoreZ05Id,
        ProductCategory::JumpDrive,
        "mfr.um_core_fab",
        "Я-0.5",
        "Ядро «Нулевая точка» Я-0.5",
        "Нулевая точка",
        "Я-0.5",
        "",
        "",
        "",
        "rev 0",
        PassportStatus::Provisional,
        {},
        6000.0,
        0.0, 0.0, 0.0,
        "Sealed jump core rated for 500 t. No runtime function yet.",
        {
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu.n1", {}},
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu-inta.a3", {}}
        }
    };
    return passport;
}

[[nodiscard]] inline const ProductPassport& jumpChargeTank()
{
    static const ProductPassport passport {
        JumpChargeTankId,
        ProductCategory::FluidStorage,
        "mfr.agora_modular",
        "БЗ-1",
        "Бак заряда прыжка (1 т)",
        "",
        "БЗ-1",
        "",
        "",
        "",
        "rev 0",
        PassportStatus::Provisional,
        {},
        350.0,
        0.0, 0.0, 0.0,
        "Jump-charge tank, 1 t of charge. No runtime function yet.",
        {
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu.n1", {}},
            {CompatibilityKind::InstalledOn, "kb.ship.cobrett-i.frnu-inta.a3", {}}
        }
    };
    return passport;
}

} // namespace game::catalog::jump
