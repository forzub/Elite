#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/game/economy/BankingModel.h"
#include "src/game/economy/PriceModel.h"
#include <set>
#include <deque>
#include <map>
#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/TradeQuoteModel.h"
#include "src/game/simulation/ClientTradeSnapshot.h"
#include "src/game/simulation/ClientWalletSnapshot.h"

// Server-held economy (docs/ECONOMY_BANKING.md, PROPOSAL 2026-11-19): national banks and exchange rates,
// the players' ledgers, the market stock / price state of every hub, and the ONE daily world correction.
//
// Owned by GameServer; the only writer of money. Everything that charges or pays goes through here:
//   quoteFuel / chargeFuel   - the refuel service (parallel agent) asks the price and charges the client
//   chargeService            - tow, minimum repair, docking, fees
//   credit                   - income (contracts, sales)
// advanceTo(day) runs the daily step for each world day passed (at most 3 per call, catch-up spread over
// ticks); the step is deterministic: hash noise keyed by (hub, good, day), events from EconomyModel.
namespace game::economy
{
enum class FuelKind : int
{
    JumpCharge = 0,    // the J-core charge, tonnes (trade good "jump_charge")
    ReactorFuel = 1,   // D / He-3 pellets (trade good "reactor_fuel")
    ReactionMass = 2,  // RCS argon top-up (trade good "tech_gases"; the Cobra recirculates it - leaks / damage only)
};
[[nodiscard]] const char* fuelGoodId(FuelKind kind);

struct FuelQuote
{
    bool available = false;
    std::string reason;          // when !available
    std::string hubId;           // the hub whose market priced it (a system reference hub for an unknown hub)
    std::string hubName;
    bool systemReference = false;
    std::string polity;
    std::string bankId;          // the bank that is paid
    std::string currency;
    double fxPerCr = 1.0;
    double stationFactor = 1.0;  // the fuel station's markup (fuel_stations.json priceFactor), already in listCrPerT
    double listCrPerT = 0.0;     // what the station asks (buy side), Cr / t
    double listLocalPerT = 0.0;
    double chargedLocalPerT = 0.0;   // 0 while fuel is free of charge
    bool freeOfCharge = true;
    double stockT = 0.0;
    CostBreakdown breakdown;     // Cr / t
};

// <path>.tmp + atomic replace; false (and the old file kept) on any failure
bool writeFileAtomically(const std::string& path, const std::string& text);

struct FuelCharge
{
    bool ok = false;
    double tonnesRequested = 0.0;
    double tonnesGranted = 0.0;  // < requested: only the lifeline amount could be paid
    double listCr = 0.0;         // list value of what was granted
    double chargedCr = 0.0;
    double lifelineCr = 0.0;
    std::string reason;
    FuelQuote quote;
};

class EconomyServer
{
public:
    EconomyServer() = default;
    ~EconomyServer();   // uninstalls itself from game::economy::service
    EconomyServer(const EconomyServer&) = delete;
    EconomyServer& operator=(const EconomyServer&) = delete;

    // loads national_banks.json + market_state_3126.json (+ EconomyModel); false = economy off (no data)
    bool load();
    [[nodiscard]] bool loaded() const { return m_loaded; }

    // the first world day; restores ELITE_ECONOMY_STATE_FILE when it exists
    void start(int day);
    // run the daily correction for every day up to `day` (<= 3 per call)
    void advanceTo(int day);
    [[nodiscard]] int day() const { return m_day; }

    // a ledger for a player (home bank = the polity of `homeSystemId`), idempotent
    void ensurePlayer(std::uint64_t playerId, int homeSystemId);

    // ---- the refuel-service interface (see docs/ECONOMY_BANKING.md §6)
    [[nodiscard]] FuelQuote quoteFuel(const std::string& hubId, int systemId, FuelKind kind) const;
    // playerId 0 = an AI ship: nothing is charged (no AI wallets yet), the depot stock still drops
    FuelCharge chargeFuel(std::uint64_t playerId, const std::string& hubId, int systemId, FuelKind kind, double tonnes);
    // the unused part of a prepaid refuel (the job was cut short): the money back, the fuel back into the stock
    void refundFuel(std::uint64_t playerId, const std::string& hubId, int systemId, FuelKind kind, double tonnes, double crPerT);
    // the fuel left the depot (stock drops; while free, the player's free list value grows) - no charge here
    void recordFuelDelivery(std::uint64_t playerId, const std::string& hubId, int systemId, FuelKind kind, double tonnes);
    PaymentResult chargeService(std::uint64_t playerId, const std::string& hubId, int systemId, Purpose purpose, double amountCr,
                                const std::string& memo);
    void credit(std::uint64_t playerId, const std::string& hubId, int systemId, double amountCr, const std::string& memo);
    // an administrative fine to the authority of `hubId` (its polity's national bank): never refused - what cannot be
    // paid becomes a fine debt (BankingModel payFine). playerId 0 (AI): nothing booked.
    PaymentResult chargeFine(std::uint64_t playerId, const std::string& hubId, int systemId, double amountCr, const std::string& memo);
    [[nodiscard]] double towCostCr() const { return m_banks.rules.towCostCr; }
    [[nodiscard]] double minRepairCostCr() const { return m_banks.rules.minRepairCr; }

    [[nodiscard]] bool fuelFreeOfCharge() const { return m_fuelFree; }
    void setFuelFreeOfCharge(bool free) { m_fuelFree = free; }


    // ---- trade (docs/TRADE_DESIGN.md §2.3, §7.2, §7.4; steps 1-2)
    struct TradeOrder
    {
        bool buy = true;
        std::uint64_t playerId = 0;
        std::uint64_t shipKey = 0;       // the ship's persistent instance id (the manifest key)
        std::string hubId;               // the hub the ship is docked at (the caller checked the berth)
        int systemId = -1;
        std::string goodId;
        double tonnes = 0.0;
        double limitUnitCr = 0.0;        // buy: not dearer than; sell: not cheaper than (0 = none)
        int quoteDay = -1;               // the day the client priced it (-1 = any)
        std::uint32_t requestId = 0;
        CargoCapacity capacity;
    };
    // all or nothing: the stock, the manifest and the money move together or nothing changes; the result is also kept
    // for the session (tradeSnapshot().results)
    game::simulation::ClientTradeResult trade(const TradeOrder& order);
    // a refusal decided outside the economy (not docked, ...) recorded as the command's result
    void recordTradeRefusal(std::uint64_t playerId, std::uint32_t requestId, const std::string& code, const std::string& message);
    // DEV: put cargo aboard without paying (cost basis 0) / empty the hold; "" = done, else the reason
    std::string devAddCargo(std::uint64_t playerId, std::uint64_t shipKey, const std::string& goodId, double tonnes,
                            const CargoCapacity& capacity, const std::string& hubId);
    void devClearCargo(std::uint64_t shipKey);
    [[nodiscard]] const CargoRegistry& cargo() const { return m_cargo; }

    // A ship was lost (destroyed / disabled). IDEMPOTENT by eventId: the first call takes every lot off the ship's
    // manifest into lost-cargo records (state aboard_wreck, legal known_owner, no claim) and saves at once; later calls
    // with the same eventId do nothing (returns false). The cargo never "resurrects" with the ship after a restart and
    // cannot be both in a manifest and lost (docs/CARGO_SALVAGE_DESIGN.md: the minimal pre-MVP part).
    struct ShipLoss
    {
        std::string eventId;           // "loss:<shipKey>:<server tick>"
        std::uint64_t shipKey = 0;
        int systemId = -1;
        double worldM[3] {0.0, 0.0, 0.0};
        double universeS = 0.0;
        std::string cause;
    };
    struct LostCargo
    {
        std::string recordId;          // "L<serial>"
        std::string eventId;
        std::uint64_t shipKey = 0, playerId = 0;
        CargoLot lot;
        std::string state = "aboard_wreck";   // CargoUnitState (design §2.2): aboard_wreck | adrift | held | recovered | destroyed | disposed
        std::string legal = "known_owner";    // LegalStatus
        std::string rightsHolder;             // who the goods are owed to now (the owner until a claim is paid)
        std::string claim = "none";           // ClaimState
        int systemId = -1;
        double worldM[3] {0.0, 0.0, 0.0};
        double universeS = 0.0;
        int day = 0;
        std::string cause;
    };
    bool recordShipLoss(const ShipLoss& loss);
    [[nodiscard]] const std::vector<LostCargo>& lostCargo() const { return m_lostCargo; }
    [[nodiscard]] bool lossRecorded(const std::string& eventId) const { return m_lossEvents.count(eventId) != 0; }
    [[nodiscard]] FiscalStyle fiscalStyleOf(const std::string& hubId, int systemId) const;
    // the import duty rate (share of the goods value) for goods bought at `originHub` and sold at `hubId` (TRADE_DESIGN
    // §5.2, step 4): 0 inside one polity or for an unknown origin; else the selling hub's style rate x the customs
    // channel multiplier between the two polities' banks
    [[nodiscard]] double importDutyRate(const std::string& originHub, const std::string& hubId, int systemId) const;
    [[nodiscard]] double customsFeeMult(Channel c) const { return m_customsFeeMult[static_cast<int>(c)]; }

    // ---- port fees (TRADE_DESIGN §5.2, step 4): charged by the server at the dispatcher's docked / released events.
    // IDEMPOTENT per call: `callId` names one docking ("call:<ship>:<dock>:<ms>"); the docking fee is booked once per
    // callId, the berth fee once at its release - a repeated event is reported `duplicate` and changes nothing. Never
    // refused: what cannot be paid becomes a 0 % debt at the port's bank (payOrDebt) - a fee never strands a ship.
    struct PortCharge
    {
        bool booked = false;       // a ledger entry was made (paid or debt)
        bool duplicate = false;    // this callId's event was already booked
        double feeCr = 0.0;
        double paidCr = 0.0;
        double debtCr = 0.0;
        double hours = 0.0;        // berth: the hours at the berth
        std::string styleId;
        double portIndex = 1.0;
    };
    PortCharge chargeDocking(std::uint64_t playerId, const std::string& callId, const std::string& hubId, int systemId, double dryMassT,
                             const std::string& hubName = std::string());
    PortCharge chargeBerth(std::uint64_t playerId, const std::string& callId, const std::string& hubId, int systemId, double berthHours,
                           const std::string& hubName = std::string());
    [[nodiscard]] const TradeDealRules& dealRules() const { return m_dealRules; }
    // the hub's OWN market cell (no system fallback for trade); false = this hub does not trade that good
    [[nodiscard]] bool marketSide(const std::string& hubId, const std::string& goodId, MarketSide& out) const;
    // the session projection: the market of `hubId` (server prices), the manifest, the latest results; the caller adds
    // the hull figures (capacity, mass, jump range)
    [[nodiscard]] game::simulation::ClientTradeSnapshot tradeSnapshot(std::uint64_t playerId, std::uint64_t shipKey,
                                                                      const std::string& hubId, int systemId, bool docked) const;
    // ---- replication
    [[nodiscard]] game::simulation::ClientWalletSnapshot walletSnapshot(std::uint64_t playerId, const std::string& hubId,
                                                                        int systemId) const;

    // ---- lookup
    [[nodiscard]] std::string polityOf(const std::string& hubId, int systemId) const;
    [[nodiscard]] const BankDirectory& banks() const { return m_banks; }
    [[nodiscard]] const ClientLedger* ledger(std::uint64_t playerId) const;
    [[nodiscard]] ClientLedger* ledger(std::uint64_t playerId);

    // ---- persistence (JSON); the world has no save yet - opt in with ELITE_ECONOMY_STATE_FILE=<path>
    [[nodiscard]] std::string saveState() const;
    bool restoreState(const std::string& json);

private:
    struct HubMarket
    {
        std::string hubId;
        std::string name;
        std::string polity;
        std::string market;
        int systemId = -1;
        double spread = 0.1;
        std::vector<GoodCell> cells;        // by EconomyModel::goods() index
        std::vector<double> transportCrT;   // delivery cost from the nearest producer (fuel goods)
    };
    void stepDay(int day);
    [[nodiscard]] int goodIndex(const std::string& id) const;
    [[nodiscard]] const HubMarket* marketFor(const std::string& hubId, int systemId, int good, bool& systemReference) const;
    [[nodiscard]] std::string bankOf(const std::string& hubId, int systemId) const;
    [[nodiscard]] std::string hubDisplayName(const std::string& hubId) const;

    bool m_loaded = false;
    bool m_fuelFree = true;
    int m_day = -1;
    BankDirectory m_banks;
    PriceRules m_priceRules;
    std::unordered_map<std::string, ProductionSplit> m_splits;
    std::vector<HubMarket> m_markets;
    std::unordered_map<std::string, std::size_t> m_marketIndex;
    std::unordered_map<std::string, std::string> m_hubPolity;      // every economy hub (incl. no market)
    std::unordered_map<int, std::string> m_systemPolity;           // the system's dominant polity
    std::unordered_map<std::uint64_t, ClientLedger> m_ledgers;
    std::string m_stateFile;
    double m_aiFreeFuelListCr = 0.0;
    // trade
    struct VatRefund
    {
        std::uint64_t playerId = 0;
        std::string bankId;
        double amountCr = 0.0;
        int dueDay = 0;
    };
    void pushResult(std::uint64_t playerId, game::simulation::ClientTradeResult r);
    void payDueVatRefunds(int day);
    CargoRegistry m_cargo;
    std::set<std::string> m_lossEvents;
    std::vector<LostCargo> m_lostCargo;
    std::uint64_t m_nextLostCargo = 1;
    TradeDealRules m_dealRules;
    std::unordered_map<std::string, FiscalStyle> m_styles;
    std::unordered_map<std::string, std::string> m_regimeStyle;
    std::vector<VatRefund> m_vatRefunds;
    double m_customsFeeMult[5] = {1.0, 1.5, 2.5, 4.0, 4.0};   // by Channel
    std::map<std::string, int> m_portCalls;                  // callId -> 1 docking booked | 2 berth booked
    std::deque<std::string> m_portCallOrder;                 // the oldest calls are forgotten beyond 512
    std::unordered_map<std::uint64_t, std::vector<game::simulation::ClientTradeResult>> m_results;
    std::uint64_t m_tradeSerial = 0;
};
} // namespace game::economy
