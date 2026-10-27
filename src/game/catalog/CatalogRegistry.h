#pragma once

#include <array>
#include <string>
#include <string_view>

#include "src/game/catalog/power/KobraReactorPassports.h"
#include "src/game/catalog/propulsion/KobraMainDrivePassports.h"
#include "src/game/catalog/rcs/KobraRcsPassports.h"
#include "src/game/catalog/ships/CobrettPassports.h"

// Lookup over all category catalogs. Each category keeps its own files; this
// registry only enumerates them.
namespace game::catalog
{

[[nodiscard]] inline const ProductPassport* findProduct(std::string_view id)
{
    static const std::array<const ProductPassport*, 6> products {
        &propulsion::kobraN1AxialMainDrive(),
        &propulsion::cr1VectorMainDrive(),
        &power::er5PropulsionReactor(),
        &rcs::fieldCoupledRcsNozzle(),
        &ships::cobrettIN1().product,
        &ships::cobrettIA3().product
    };
    for (const auto* product : products)
        if (product->stableId == id)
            return product;
    return nullptr;
}

[[nodiscard]] inline const ships::ShipPassport* findShipPassport(
    std::string_view id)
{
    if (id == ships::CobrettIN1Id)
        return &ships::cobrettIN1();
    if (id == ships::CobrettIA3Id)
        return &ships::cobrettIA3();
    return nullptr;
}

// Every position that names a module must name a real installed module
// whose technical preset matches the passport's technicalTypeId. Returns an
// empty string when consistent, otherwise the first problem.
template <typename ModuleLookup>
[[nodiscard]] inline std::string validateShipPassportInstallation(
    const ships::ShipPassport& ship,
    ModuleLookup technicalTypeOfModule)
{
    for (const auto& position : ship.assemblies)
    {
        if (position.moduleId.empty())
            continue;
        const auto technical = technicalTypeOfModule(position.moduleId);
        if (!technical)
            return position.code + ": module " + position.moduleId +
                " is not installed";
        if (position.productStableId.empty())
            continue;
        const auto* product = findProduct(position.productStableId);
        if (!product)
            return position.code + ": unknown product " +
                position.productStableId;
        if (!product->technicalTypeId.empty() &&
            product->technicalTypeId != *technical)
            return position.code + ": module " + position.moduleId +
                " carries " + std::string(*technical) + ", passport expects " +
                std::string(product->technicalTypeId);
    }
    return {};
}

} // namespace game::catalog
