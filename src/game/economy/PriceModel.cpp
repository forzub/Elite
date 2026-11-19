#include "src/game/economy/PriceModel.h"

#include <algorithm>
#include <cmath>

namespace game::economy
{
double scarcityFactor(double stockT, double targetT, const PriceRules& rules)
{
    if (targetT <= 0.0)
        return 1.0;
    const double s = std::max(stockT, rules.minStockShare * targetT);
    return std::pow(targetT / s, rules.stockExponent);
}

double targetPriceCr(const GoodCell& cell, const PriceRules& rules)
{
    const double f = std::clamp(scarcityFactor(cell.stockT, cell.targetT, rules), rules.clampLo, rules.clampHi);
    return cell.anchorCr * f;
}

double anchorFromPrice(double priceCr, double stockT, double targetT, const PriceRules& rules)
{
    const double f = std::clamp(scarcityFactor(stockT, targetT, rules), rules.clampLo, rules.clampHi);
    return f > 0.0 ? priceCr / f : priceCr;
}

double stepCell(GoodCell& cell, const CellDrivers& d, const PriceRules& rules)
{
    if (!cell.traded || cell.anchorCr <= 0.0)
        return 0.0;
    const double flow = std::max(0.0, cell.flowTDay);
    const double demand = std::max(0.0, flow * (1.0 + rules.slowDemandAmp * d.slowDemand + rules.demandNoise * d.demandShock) * d.demandMult);
    const double supply = std::max(0.0, flow * (1.0 + rules.supplyNoise * d.supplyShock) * d.supplyMult);
    // the restock loop: orders (or sells off) the gap over the lead time; a supply cut also slows the orders
    const double normal = cell.normalStockT > 0.0 ? cell.normalStockT : cell.targetT;
    double restock = (normal - cell.stockT) / std::max(1.0, cell.leadDays);
    if (restock > 0.0)
        restock *= std::min(1.0, d.supplyMult);
    cell.stockT = std::max(0.0, cell.stockT + supply - demand + restock);

    const double target = targetPriceCr(cell, rules);
    const double before = cell.priceCr > 0.0 ? cell.priceCr : cell.anchorCr;
    double next = before + rules.smoothing * (target - before);
    next = std::clamp(next, before * (1.0 - rules.maxDailyChange), before * (1.0 + rules.maxDailyChange));
    next = std::clamp(next, cell.anchorCr * rules.clampLo, cell.anchorCr * rules.clampHi);
    cell.priceCr = next;
    return next / before - 1.0;
}

void eventDrivers(const std::string& type, const std::string& group, const std::string& goodId, bool eventGood, CellDrivers& d)
{
    // the CAUSE of every price move is a stock change: events act on supply / demand, never on the price directly
    if (type == "shortage")
    {
        if (eventGood)
            d.supplyMult *= 0.45;
    }
    else if (type == "surplus")
    {
        if (eventGood)
            d.supplyMult *= 1.6;
    }
    else if (type == "harvest_failure")
    {
        if (group == "food")
            d.supplyMult *= 0.5;
    }
    else if (type == "fuel_crunch")
    {
        if (group == "fuel")
            d.supplyMult *= 0.4;
    }
    else if (type == "epidemic" || type == "quarantine")
    {
        if (group == "medicine" || group == "medical")
            d.demandMult *= 1.8;
        else if (group == "food")
            d.demandMult *= 1.15;
    }
    else if (type == "blockade")
        d.supplyMult *= 0.55;
    else if (type == "piracy_wave")
        d.supplyMult *= 0.85;
    else if (type == "plant_accident")
    {
        if (group == "machinery" || group == "metals")
            d.supplyMult *= 0.6;
    }
    else if (type == "construction_boom")
    {
        if (group == "construction" || group == "metals")
            d.demandMult *= 1.6;
    }
    else if (type == "festival")
    {
        if (group == "luxury" || goodId == "food_fresh")
            d.demandMult *= 1.5;
    }
    else if (type == "dock_strike")
    {
        d.supplyMult *= 0.8;
        d.demandMult *= 0.9;
    }
}

CostBreakdown explainPrice(double baseCrPerT, bool producer, double transportCrPerT, double anchorCr, double priceCr, double spread,
                           const ProductionSplit& split)
{
    CostBreakdown b;
    const double production = std::max(0.0, baseCrPerT) * (producer ? 0.9 : 1.0);
    b.feedstockCr = production * split.feedstock;
    b.conversionCr = production * split.conversion;
    b.separationEnergyCr = production * split.separationEnergy;
    b.fabricationCr = production - b.feedstockCr - b.conversionCr - b.separationEnergyCr;
    b.transportCr = producer ? 0.0 : std::max(0.0, transportCrPerT);
    b.locationCr = anchorCr - production - b.transportCr;
    b.scarcityCr = priceCr - anchorCr;
    b.stationMarginCr = priceCr * std::max(0.0, spread) * 0.5;
    b.listCr = priceCr + b.stationMarginCr;
    return b;
}
} // namespace game::economy
