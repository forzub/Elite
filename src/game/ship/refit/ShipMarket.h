#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/ship/refit/ShipMarketRules.h"

// Ship market seam (F6 Shipyard). A seller offers a CONCRETE ship instance:
// model (passport id), age, operating hours, wear, damage, the installed
// equipment and a price from the data-driven depreciation rules
// (ship_market_rules.json) applied to the passport list price. The DEV
// inventory below is deterministic mock stock until real seller inventories
// (economy layer, ShipInstance registry) feed the same struct.
namespace game::ship::refit
{
struct ShipOfferEquipment
{
    std::string category;
    std::string productId;
    std::string productName;
    int quantity = 0;
};

struct ShipOffer
{
    std::string offerId;
    std::string passportId;
    std::string sellerId;        // manufacturer / used_dealer / cheap_used / salvage
    std::string sellerNameRu;
    bool dev = true;             // mock inventory until the economy feeds real stock
    double ageYears = 0.0;
    double operatingHours = 0.0;
    double wear01 = 0.0;
    double damage01 = 0.0;
    std::string condition;       // new / good / worn / wreck
    double priceCr = 0.0;
    double newPriceCr = 0.0;
    double scrapFloorCr = 0.0;
    std::vector<ShipOfferEquipment> equipment;
};

// Market value of an instance: new -> first sale loss -> age / hours decay
// -> wear / damage, never below scrap + reusable components.
[[nodiscard]] inline double shipMarketValueCr(const game::catalog::ships::ShipDesign& s, double ageYears, double hours,
                                              double wear01, double damage01, double& scrapFloorCr)
{
    const auto& r = ShipMarketRules::instance();
    scrapFloorCr = s.dryMassTonnes * r.scrapCrPerTonne + s.pricePartsCr * r.salvageComponentShare01 * (1.0 - damage01);
    if (ageYears <= 0.0 && hours <= 0.0 && wear01 <= 0.0 && damage01 <= 0.0)
        return s.priceListCr;
    double v = s.priceListCr * (1.0 - r.firstSaleLoss01);
    v *= std::pow(0.5, ageYears / std::max(0.1, r.ageHalflifeYears));
    v *= std::pow(0.5, hours / std::max(1.0, r.hoursHalflife));
    v *= std::clamp(1.0 - r.wearValueLoss01 * wear01, 0.0, 1.0);
    v *= std::clamp(1.0 - damage01, 0.0, 1.0);
    return std::max(scrapFloorCr, v);
}

namespace market_detail
{
[[nodiscard]] inline double unitHash(const std::string& key)
{
    std::uint64_t h = 1469598103934665603ull;
    for (const char c : key)
        h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    return static_cast<double>(h % 1000003ull) / 1000003.0;
}
} // namespace market_detail

// DEV: one deterministic offer per seller category for a model.
[[nodiscard]] inline std::vector<ShipOffer> devShipOffers(const game::catalog::ships::ShipDesign& s)
{
    std::vector<ShipOffer> out;
    const auto& catalog = game::catalog::ships::ShipDesignCatalog::instance();
    for (const auto& seller : ShipMarketRules::instance().sellers)
    {
        ShipOffer o;
        o.offerId = s.id + "#" + seller.id;
        o.passportId = s.id;
        o.sellerId = seller.id;
        o.sellerNameRu = seller.nameRu;
        const auto pick = [&](const char* what, double lo, double hi) {
            return lo + (hi - lo) * market_detail::unitHash(o.offerId + what);
        };
        o.ageYears = std::min(pick("age", seller.ageMinYears, seller.ageMaxYears), static_cast<double>(std::max(1, s.serviceLifeYears)));
        o.operatingHours = o.ageYears * pick("hours", 1500.0, 4500.0);
        o.wear01 = pick("wear", seller.wearMin01, seller.wearMax01);
        o.damage01 = pick("damage", seller.damageMin01, seller.damageMax01);
        o.condition = o.ageYears <= 0.0 ? "new" : o.damage01 > 0.3 ? "wreck" : o.wear01 > 0.3 ? "worn" : "good";
        o.newPriceCr = s.priceListCr;
        // the seller's margin moves the price, never below scrap + reusable parts
        const double value = shipMarketValueCr(s, o.ageYears, o.operatingHours, o.wear01, o.damage01, o.scrapFloorCr);
        o.priceCr = o.ageYears <= 0.0 ? value * seller.priceX : std::max(o.scrapFloorCr, value * seller.priceX);
        for (const auto& item : s.installed)
        {
            const auto* p = catalog.findProduct(item.productId);
            o.equipment.push_back({item.category, item.productId, p ? p->name : item.productId, item.quantity});
        }
        out.push_back(std::move(o));
    }
    return out;
}
} // namespace game::ship::refit
