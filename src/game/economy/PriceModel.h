#pragma once

#include <string>

// The daily price correction of one market cell (hub x good), docs/ECONOMY_BANKING.md §4 (PROPOSAL 2026-11-19).
// PURE: the server supplies the drivers (event multipliers, hash noise) and calls stepCell once per world day.
//
//   stock'  = max(0, stock + supply - demand + restock)          restock = (normal stock - stock) / lead days
//   target price = anchor x (target / max(stock, minStockShare x target)) ^ stockExponent, clamped to [lo, hi] x anchor
//   price'  = price + smoothing x (target price - price), the move limited to +- maxDailyChange x price
//
// anchor = the cell's structural price (production + transport + location / risk, no scarcity), derived at
// start from the generated world state so the formula reproduces the initial price exactly.
namespace game::economy
{
struct PriceRules
{
    double stockExponent = 0.6;
    double smoothing = 0.35;
    double maxDailyChange = 0.08;
    double clampLo = 0.35;
    double clampHi = 4.0;
    double minStockShare = 0.05;
    double demandNoise = 0.12;     // daily demand wobble (share of the flow)
    double supplyNoise = 0.08;
    double slowDemandAmp = 0.10;   // weeks-long demand swing
    double slowDemandPeriodDays = 9.0;
    // TRADE_DESIGN §2.4 (step 10): dumping in a glut - storage and spoilage make holders cut the price:
    // dump = 1 - depth x clamp((S/T - from) / span, 0, 1) x (1 + shelfRef / shelf_days), >= floor
    double dumpFromRatio = 1.3;
    double dumpSpan = 1.2;
    double dumpDepth = 0.25;
    double dumpShelfRefDays = 30.0;
    double dumpFloor = 0.5;
    // a glut (S > glutRatio x T) may fall, a shortage (S < shortageRatio x T) may rise up to extremeDailyChange a day
    double glutRatio = 2.0;
    double shortageRatio = 0.3;
    double extremeDailyChange = 0.12;
};

struct GoodCell
{
    bool traded = false;
    bool producer = false;
    double anchorCr = 0.0;   // structural mid price, Cr / t
    double priceCr = 0.0;    // current mid price, Cr / t
    double stockT = 0.0;
    double targetT = 0.0;    // stock the hub wants (cover days x flow): the price reference
    // the stock the hub's supply chain actually sustains (<= target where the system imports or lacks the good):
    // the restock loop returns here, so a structural shortage keeps its price; 0 = targetT
    double normalStockT = 0.0;
    double flowTDay = 0.0;   // turnover, t / day
    double leadDays = 7.0;   // restock lead time
};

struct CellDrivers
{
    double supplyMult = 1.0;   // events (fuel crunch, blockade, ...)
    double demandMult = 1.0;
    double supplyShock = 0.0;  // [-1, 1] daily noise
    double shelfDays = 0.0;    // the good's shelf life (0 = keeps forever): spoilage deepens the dumping
    double demandShock = 0.0;
    double slowDemand = 0.0;   // [-1, 1] smooth weeks-long swing
};

[[nodiscard]] double scarcityFactor(double stockT, double targetT, const PriceRules& rules);
[[nodiscard]] double targetPriceCr(const GoodCell& cell, const PriceRules& rules);
// the structural price that makes targetPriceCr reproduce `priceCr` at `stockT`
[[nodiscard]] double anchorFromPrice(double priceCr, double stockT, double targetT, const PriceRules& rules);
// one world day; returns the relative price move
double stepCell(GoodCell& cell, const CellDrivers& drivers, const PriceRules& rules);

// event type (economy_rules.json events) -> supply / demand multipliers for a goods group
void eventDrivers(const std::string& eventType, const std::string& group, const std::string& goodId, bool eventGood, CellDrivers& d);

// The explained list price of fuel (and any good): what the player pays per tonne at a hub, decomposed.
// Sums exactly: production + transport + location + scarcity + stationMargin = list price (buy side).
struct CostBreakdown
{
    double feedstockCr = 0.0;
    double conversionCr = 0.0;
    double separationEnergyCr = 0.0;
    double fabricationCr = 0.0;
    double transportCr = 0.0;      // delivery from the nearest producer (C20 jump / leg cost per tonne)
    double locationCr = 0.0;       // remoteness, deep space, import / export position, legality risk (may be < 0)
    double scarcityCr = 0.0;       // current price - anchor (stock vs target)
    double stationMarginCr = 0.0;  // half the spread: the station's handling margin
    double listCr = 0.0;
};
struct ProductionSplit
{
    double feedstock = 0.55;
    double conversion = 0.0;
    double separationEnergy = 0.25;
    double fabrication = 0.20;
};
[[nodiscard]] CostBreakdown explainPrice(double baseCrPerT, bool producer, double transportCrPerT, double anchorCr, double priceCr,
                                         double spread, const ProductionSplit& split);
} // namespace game::economy
