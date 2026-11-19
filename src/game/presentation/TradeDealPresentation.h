#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/game/economy/TradeQuoteModel.h"
#include "src/game/simulation/ClientTradeSnapshot.h"

// F8 MARKET deal window (docs/TRADE_DESIGN.md §8.1, step 3 of §9) - PURE page model.
//
// The preview is computed with the SAME TradeQuoteModel::explainDeal the server's EconomyServer::trade charges with,
// from the inputs the server replicated (session.trade: the cell's mid price, spread, stock, target, the deal rules,
// the fiscal terms, the manifest, the hull capacity and the money pay() can cover). With an unchanged market the
// preview equals the charge to the bit; Confirm sends the quoted unit price as the limit, so a market that moved in
// between is refused as "price_moved" and re-quoted, never charged silently.
namespace game::presentation
{
// the window's own state (SpaceState owns it; nothing here is authoritative)
struct TradeDealRequest
{
    int good = -1;                 // EconomyModel::goods() index; -1 = no row selected
    bool buy = true;
    double tonnes = -1.0;          // < 0 = the default (one C20 box, within the limits)
    bool unitsC20 = false;         // the slider counts C20 boxes instead of tonnes
    std::uint32_t sentRequest = 0; // the last Confirm's request id (0 = none)
    double sentUnitCr = 0.0;       // the unit price it was sent with (limitUnitCr)
    double sentTonnes = 0.0;
    bool sentBuy = true;
};

struct TradeDealLineView
{
    std::string kind;              // "goods" | "handling" | "vat" | "turnover"
    double cr = 0.0;               // signed (+ to the player), exactly explainDeal's line
    long long shownCr = 0;         // whole Cr shown; the shown lines add up to shownTotalCr exactly
};

enum class TradeDealAnswer
{
    None,
    Waiting,       // sent, no result with that request id yet
    Ok,
    Refused,
    PriceMoved     // "price_moved" / "requote": the market moved - the preview is already the new quote
};

struct TradeDealView
{
    bool valid = false;
    int good = -1;
    std::string goodId;
    bool buy = true;
    bool unitsC20 = false;
    bool docked = false;
    double tonnes = 0.0;           // the quantity priced (clamped to [0, maxT])
    double boxT = 20.0;            // net tonnes of this good one C20 takes
    int c20 = 0;                   // the boxes `tonnes` takes on its own
    double aboardT = 0.0;          // own tonnes of this good aboard

    // the limits (TRADE_DESIGN §8.1): the smallest one decides maxT
    double holdLimitT = 0.0;       // buy: C20 places + payload (the server's CargoRegistry::fits)
    double hubLimitT = 0.0;        // buy: hubGivesUpToT; sell: hubTakesUpToT
    double moneyLimitT = 0.0;      // buy: what spendableCr pays for (incl. overdraft, fees)
    double maxT = 0.0;
    std::string limitKind;         // "hold_c20" | "payload" | "hub" | "money" | "aboard"
    int c20Free = 0;
    double spendableCr = 0.0;

    game::economy::DealQuote quote;   // explainDeal(buy, side, tonnes, style, rules) - what the server will charge
    std::vector<TradeDealLineView> lines;
    long long shownTotalCr = 0;
    double vatRefundCr = 0.0;      // buy: what comes back after the refund days (minus the refund fee)
    int vatRefundDays = 0;

    bool canConfirm = false;
    std::string blockKind;         // why not: "not_docked" | "zero" | "waiting" | "quote" (quote.reason) | "money"

    TradeDealAnswer answer = TradeDealAnswer::None;
    game::simulation::ClientTradeResult result;   // the server's result for sentRequest
    double sentUnitCr = 0.0;
    std::uint32_t sentRequest = 0;
};

// the deal rules / the cell / the fiscal terms as the server replicated them
[[nodiscard]] game::economy::TradeDealRules dealRulesOf(const game::simulation::ClientTradeSnapshot& s);
[[nodiscard]] game::economy::FiscalStyle fiscalStyleOf(const game::simulation::ClientTradeSnapshot& s);
[[nodiscard]] game::economy::MarketSide marketSideOf(const game::simulation::ClientTradeSnapshot& s,
                                                     const game::simulation::ClientMarketRow& row);

[[nodiscard]] TradeDealView buildTradeDeal(const game::simulation::ClientTradeSnapshot& snap, const TradeDealRequest& req);

// whole-Cr display of signed amounts that add up exactly to the rounded total (largest remainder)
[[nodiscard]] std::vector<long long> roundLinesToTotal(const std::vector<double>& cr, long long& totalOut);

// the next quantity for a step (+-1 / +-10 tonnes, or boxes in C20 units), and for a slider fraction 0..1
[[nodiscard]] double tradeDealStep(const TradeDealView& v, int steps);
[[nodiscard]] double tradeDealAtFraction(const TradeDealView& v, double fraction);
} // namespace game::presentation
