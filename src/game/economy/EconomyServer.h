#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/game/economy/BankingModel.h"
#include "src/game/economy/PriceModel.h"
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
    double listCrPerT = 0.0;     // what the station asks (buy side), Cr / t
    double listLocalPerT = 0.0;
    double chargedLocalPerT = 0.0;   // 0 while fuel is free of charge
    bool freeOfCharge = true;
    double stockT = 0.0;
    CostBreakdown breakdown;     // Cr / t
};

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
};
} // namespace game::economy
