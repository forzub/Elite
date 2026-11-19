#pragma once

#include <string>
#include <vector>

// The price of a deal at a hub market (docs/TRADE_DESIGN.md §2.3) and the fee stack on it (§5.1, §5.2) - PURE.
// The server (EconomyServer::trade) and the client's preview use the same formulas; the server's number is the one
// that is charged.
//
//   p_bid = price x (1 - spread/2),  p_ask = price x (1 + spread/2)
//   sell q t:  unit = p_bid x ( S / (S + kappa q/2) )^0.6
//   buy  q t:  unit = p_ask x ( S / max(S - kappa q/2, 0.05 T) )^0.6
//   the hub takes   q <= max(0, 1.5 T - S)        ("the warehouse is full")
//   the hub gives   q <= max(0, S - 0.05 T)       (the irreducible remainder)
// The deal moves the stock at once (the caller does it); the day's price moves only in the daily step - the second
// half of the reaction.
//
// Fees (MVP, step 2): the spread (inside bid / ask), cargo handling 12 Cr/t x the port index, VAT on a purchase (goods +
// handling; the goods part is reclaimable after the style's refund days minus 1 %) and on the handling of a sale, the
// turnover tax / tithe / artel levy on a sale's gross revenue. Docking (150 + 0.9 Cr/t dry mass) x the port index is
// computed here; charging it at docking is step 4.
namespace game::economy
{
struct FiscalStyle
{
    std::string id = "bureaucratic";
    std::string ru = "бюрократический";
    double vatShare = 0.20;
    int vatRefundDays = 30;
    double turnoverShare = 0.0;   // planned 3 %, theocratic tithe 10 %, clan artel levy 3 % - of a sale's gross revenue
    std::string turnoverRu;
    double portIndex = 1.0;       // multiplies docking and handling
};

struct TradeDealRules
{
    double kappa = 0.5;
    double stockExponent = 0.6;
    double minStockShare = 0.05;
    double hubBuyCapShare = 1.5;
    double handlingCrPerT = 12.0;
    double dockingBaseCr = 150.0;
    double dockingCrPerDryT = 0.9;
    double vatRefundFeeShare = 0.01;
};

// one market cell as the deal sees it
struct MarketSide
{
    double priceCr = 0.0;   // the day's mid price, Cr / t
    double spread = 0.1;
    double stockT = 0.0;
    double targetT = 0.0;
};

[[nodiscard]] double bidCr(const MarketSide& m);
[[nodiscard]] double askCr(const MarketSide& m);
[[nodiscard]] double hubTakesUpToT(const MarketSide& m, const TradeDealRules& r);   // the most the hub buys from the player
[[nodiscard]] double hubGivesUpToT(const MarketSide& m, const TradeDealRules& r);   // the most the hub sells to the player
// the deal's unit price for q tonnes (the impact of the deal itself)
[[nodiscard]] double sellUnitCr(const MarketSide& m, double tonnes, const TradeDealRules& r);
[[nodiscard]] double buyUnitCr(const MarketSide& m, double tonnes, const TradeDealRules& r);
[[nodiscard]] double dockingFeeCr(double dryMassT, const FiscalStyle& s, const TradeDealRules& r);

struct DealLine
{
    std::string label;
    double cr = 0.0;   // signed: + to the player, - from the player
};

struct DealQuote
{
    bool ok = false;
    std::string code;     // "" | "hub_full" | "hub_empty" | "zero"
    std::string reason;
    bool buy = false;
    double tonnes = 0.0;
    double listUnitCr = 0.0;   // bid (sell) / ask (buy) before the impact
    double unitCr = 0.0;       // the deal's unit price
    double impactShare = 0.0;  // unit / list - 1 (sell < 0, buy > 0)
    double goodsCr = 0.0;      // tonnes x unit
    double handlingCr = 0.0;
    double vatCr = 0.0;        // all VAT paid now
    double vatReclaimCr = 0.0; // the part refunded after vatRefundDays (minus the refund fee)
    double turnoverCr = 0.0;
    double totalCr = 0.0;      // signed: what the account changes by now (= the sum of lines)
    std::vector<DealLine> lines;
};

// the deal and its breakdown; the hub limits are checked (code / reason when not ok, the numbers still filled)
[[nodiscard]] DealQuote explainDeal(bool buy, const MarketSide& m, double tonnes, const FiscalStyle& style, const TradeDealRules& r);
} // namespace game::economy
