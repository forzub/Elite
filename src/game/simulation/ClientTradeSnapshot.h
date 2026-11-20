#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Per-session projection of the trade layer (docs/TRADE_DESIGN.md §7.5): the market of the hub the ship is at as
// the SERVER prices it (EconomyServer - one price of record: what the client shows is what is charged), the ship's
// cargo manifest, the fiscal terms here and the latest command results. The client only shows it and sends requests
// (ClientEconomyCommand); it never changes a stock, a price or a manifest itself.
namespace game::simulation
{
struct ClientMarketRow
{
    int good = -1;               // EconomyModel::goods() index (the same data file on both sides)
    bool traded = false;
    double priceCr = 0.0;        // the day's mid price
    double bidCr = 0.0;          // the player sells at (before the deal's own impact)
    double askCr = 0.0;          // the player buys at
    double stockT = 0.0;
    double targetT = 0.0;
    double hubTakesUpToT = 0.0;  // the most the hub buys now ("склад полон" beyond)
    double hubGivesUpToT = 0.0;  // the most the hub sells now
    double drift = 0.0;          // price vs the cell's structural anchor
    // the market regime (TRADE_DESIGN §2.5, step 8): the state price (0 = none), the cap on what the hub sells to THIS
    // player (< 0 = none), a short note ("госцена", "по карточкам: 1.2 т на сегодня", "нет в продаже: госпотребность")
    double fixedPriceCr = 0.0;
    double giveCapT = -1.0;
    std::string regimeNote;
};

struct ClientCargoLine
{
    std::string lotId;
    int good = -1;
    double tonnes = 0.0;
    int c20 = 0;
    double costBasisCrPerT = 0.0;
    std::string originHub;
    int loadedDay = 0;
    std::string owner;
    std::string contractId;
    double dutyShare = 0.0;      // the import duty rate for selling this lot HERE (share of the goods value; 0 = this polity)
};

struct ClientTradeResult
{
    std::uint32_t requestId = 0;
    bool ok = false;
    std::string code;                 // "" ok | "not_docked" | "no_market" | "no_money" | "hub_full" | "hub_empty" | "no_space" | ...
    std::string message;
    std::vector<std::string> lines;   // the breakdown "label|+-Cr"
    double totalCr = 0.0;             // signed: what the account changed by
    int day = 0;
};

// A contract posting of the hub's board or one of the player's contracts (TRADE_DESIGN §3, §8.2 - trade step 5): the
// server's ContractBoard as replicated; times in economy days (the client shows dayNow - x as days / hours)
struct ClientContractRow
{
    std::string id;
    std::string kind;                 // supply | crew_shift | construction
    std::string originHub, originName;
    std::string destinationHub, destinationName;
    std::string issuerName;
    std::string goodId;               // a trade good, "supply_kit" or "crew_p20"
    std::string goodRu;
    std::string cause;
    double tonnes = 0.0;
    int people = 0;
    int c20 = 0;
    double rewardCr = 0.0;
    double advanceCr = 0.0;
    double depositCr = 0.0;
    double penaltyCr = 0.0;
    double lateSharePerDay = 0.0;
    int postedDay = 0;
    int expiresDay = 0;               // off the board at the start of this day
    int deadlineDay = 0;              // delivered by the end of this day = on time
    std::string status;               // board: open | taken | npc | expired; mine: active | delivered | late | failed | abandoned | cancelled
    int closedDay = -1;
    double paidCr = 0.0;              // mine: the payout at hand-in
    double penaltyPaidCr = 0.0;       // mine: the penalty / fee / advance back charged
    int lateDays = 0;
    bool aboard = false;              // mine: its cargo is in this ship's manifest
};

struct ClientTradeSnapshot
{
    bool valid = false;
    int day = 0;
    std::uint64_t serial = 0;         // changes with every trade / cargo change

    // the market (the hub the ship is at; empty with marketNote outside a hub with a market)
    std::string hubId;
    std::string hubName;
    bool docked = false;              // at a berth: deals allowed
    std::string marketNote;
    double spread = 0.0;
    std::string fiscalStyleRu;
    double vatShare = 0.0;
    int vatRefundDays = 0;
    double turnoverShare = 0.0;
    std::string turnoverRu;
    double handlingCrPerT = 0.0;      // incl. the port index
    double kappa = 0.5;
    // the rest of the deal rules (TradeDealRules) and the money the server's pay() can cover here: the deal window
    // (trade step 3) prices a deal with the SAME TradeQuoteModel::explainDeal the server charges with
    double stockExponent = 0.6;
    double minStockShare = 0.05;
    double hubBuyCapShare = 1.5;
    double spendableCr = 0.0;         // cash + accounts incl. overdraft, after FX / transfer fees, payable to this hub's bank
    std::vector<ClientMarketRow> market;

    // the manifest of the controlled ship
    std::vector<ClientCargoLine> cargo;
    double cargoNetT = 0.0;
    double cargoGrossT = 0.0;         // net + 1.7 t tare per C20
    int c20Used = 0;
    int c20Slots = 0;
    double netCapacityT = 0.0;
    double shipMassT = 0.0;           // the simulation's live mass (follows the manifest)
    double jumpRangeLy = 0.0;         // one full J charge at this mass
    double jumpRangeEmptyLy = 0.0;    // the same with an empty hold
    double vatReclaimCr = 0.0;        // input VAT waiting for its refund

    std::vector<ClientTradeResult> results;   // newest last, at most 4

    // the contract board of the hub the ship is at (shared by every player) and this player's contracts (trade step 5)
    double dayNow = 0.0;              // the economy day with its fraction (timers)
    std::string boardHubId;
    std::vector<ClientContractRow> board;
    std::vector<ClientContractRow> contracts;
};
} // namespace game::simulation
