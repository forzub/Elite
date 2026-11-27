#pragma once

#include <functional>
#include "src/game/economy/WorldSaveStore.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/game/economy/BankingModel.h"
#include "src/game/economy/TaxModel.h"
#include "src/game/economy/PriceModel.h"
#include <set>
#include <deque>
#include <map>
#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/ContractBoard.h"
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
    EconomyServer();
    ~EconomyServer();   // uninstalls itself from game::economy::service; writes a dirty state and joins the writer
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
        bool black = false;              // a sale to the hub's black market (contraband here, TRADE_DESIGN §2.8, step 9)
    };
    // all or nothing: the stock, the manifest and the money move together or nothing changes; the result is also kept
    // for the session (tradeSnapshot().results)
    game::simulation::ClientTradeResult trade(const TradeOrder& order);
    // a refusal decided outside the economy (not docked, ...) recorded as the command's result
    void recordTradeRefusal(std::uint64_t playerId, std::uint32_t requestId, const std::string& code, const std::string& message);
    // a completed non-market operation the client should see in its results (a ship purchase)
    void recordOperation(std::uint64_t playerId, const std::string& code, const std::string& message, double totalCr);
    // DEV: put cargo aboard without paying (cost basis 0) / empty the hold; "" = done, else the reason
    std::string devAddCargo(std::uint64_t playerId, std::uint64_t shipKey, const std::string& goodId, double tonnes,
                            const CargoCapacity& capacity, const std::string& hubId);
    void devClearCargo(std::uint64_t shipKey);
    // DEV: the player pays all spendable money (cash + accounts + the arranged overdraft to its limit) to the bank of
    // hubId - a broke pilot for the debt / protected-minimum checks; returns the Cr paid
    double devSpendAll(std::uint64_t playerId, const std::string& hubId, int systemId);
    [[nodiscard]] const CargoRegistry& cargo() const { return m_cargo; }

    // A ship was lost (destroyed / disabled). IDEMPOTENT by eventId: the first call takes every lot off the ship's
    // manifest into lost-cargo records (state aboard_wreck, legal known_owner, no claim) and saves (within ~2 s); later calls
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
        // the claim settled at the loss (audit ECO-1): the insurer that paid, the amount - the goods are the insurer's
        // from then on (subrogation, CARGO_SALVAGE_DESIGN §8): a recovery must not give the owner the goods AND the payout
        std::string insurer;
        double payoutCr = 0.0;
    };
    bool recordShipLoss(const ShipLoss& loss);
    [[nodiscard]] const std::vector<LostCargo>& lostCargo() const { return m_lostCargo; }
    [[nodiscard]] bool lossRecorded(const std::string& eventId) const { return m_lossEvents.count(eventId) != 0; }
    [[nodiscard]] FiscalStyle fiscalStyleOf(const std::string& hubId, int systemId) const;
    // TRADE step 9: "legal" | "restricted" | "banned" of a good at a hub (its polity's regime family); until licences
    // (step 11) only "banned" is contraband - "restricted" trades openly with a note
    [[nodiscard]] std::string legalityAt(int good, const std::string& hubId, int systemId) const;
    // the black market's price for a good the player holds (0 = not contraband here for this player)
    [[nodiscard]] double blackBidCrPerT(std::uint64_t playerId, int good, const std::string& hubId, int systemId) const;
    // ---- licences (TRADE_DESIGN §4, step 11)
    // "legal" | "licensed" (restricted, the licence held) | "restricted" (no licence: contraband) | "banned"
    [[nodiscard]] std::string legalityFor(std::uint64_t playerId, int good, const std::string& hubId, int systemId) const;
    [[nodiscard]] std::string licenceForGood(const std::string& goodId) const;   // "" = none lists it
    [[nodiscard]] bool hasLicence(std::uint64_t playerId, const std::string& polity, const std::string& type) const;
    // ---- construction (TRADE_DESIGN §6.2, step 16): a contract on a PLANNED expansion project of a city hub
    struct PlannedProject
    {
        std::string projectId, kind;
    };
    // the server passes the hubs' planned (not started) projects it can draw (HubExpansion), one per hub
    void setBuildableProjects(std::map<std::string, PlannedProject> byHub) { m_buildable = std::move(byHub); }
    [[nodiscard]] std::size_t buildableCount() const { return m_buildable.size(); }
    struct BuildOffer
    {
        std::string id, hubId, projectId, kind, contractorRu;
        double priceCr = 0.0, reliability = 0.0;
        int workDays = 0;
    };
    struct BuildProject
    {
        BuildOffer offer;
        std::uint64_t owner = 0;
        int startDay = 0;
        int paidMilestones = 0;      // 0..4
        double paidCr = 0.0;
        double retentionCr = 0.0;
        std::vector<std::pair<int, int>> halts;   // economy days [from, to); to < 0 = open
        int unpaidSince = -1;
        std::string status = "building";   // building / halted / done / abandoned
        bool retentionPaid = false;
    };
    [[nodiscard]] std::vector<BuildOffer> buildOffersAt(const std::string& hubId, std::uint64_t playerId = 0) const;
    // ---- mining concessions (TRADE_DESIGN §6.1, step 16 part 2): a small asteroid at a mining / belt hub
    struct SiteOffer
    {
        std::string id, hubId, nameRu, goodId;
        double radiusM = 0.0, grade = 1.0, reserveT = 0.0, rentYearCr = 0.0, leaseCostCr = 0.0;
    };
    struct Concession
    {
        SiteOffer site;
        std::uint64_t owner = 0;
        int startDay = 0;
        std::string status = "leased";   // leased (the rig not built yet) / mining / depleted / lapsed
        double reserveLeftT = 0.0, minedT = 0.0, soldT = 0.0, revenueCr = 0.0, lastOutputT = 0.0, rigPriceCr = 0.0;
        int unpaidDays = 0;
        bool sellLocal = true;   // false: the ore stays in the stockpile for the owner to haul (part 3)
    };
    // part 3: the owner's choice - sell at the mine's hub daily or keep the ore; haul it from the stockpile into a hold
    void setMineSale(std::uint64_t playerId, const std::string& siteId, bool sellLocal);
    [[nodiscard]] double mineStockT(const std::string& siteId, const std::string& goodId) const;
    game::simulation::ClientTradeResult loadFromMine(std::uint64_t playerId, const std::string& siteId, std::uint64_t shipKey, const CargoCapacity& capacity,
                                                     const std::string& hubId, double tonnes, std::uint32_t requestId);
    [[nodiscard]] std::vector<SiteOffer> siteOffersAt(const std::string& hubId) const;
    game::simulation::ClientTradeResult leaseSite(std::uint64_t playerId, const std::string& offerId, const std::string& hubId, int systemId,
                                                  std::uint32_t requestId);
    [[nodiscard]] const std::vector<Concession>& concessions() const { return m_concessions; }
    game::simulation::ClientTradeResult signConstruction(std::uint64_t playerId, const std::string& offerId, const std::string& hubId, int systemId,
                                                         std::uint32_t requestId);
    [[nodiscard]] const std::vector<BuildProject>& buildProjects() const { return m_buildProjects; }
    [[nodiscard]] double buildProgress(const BuildProject& p, int day) const;
    [[nodiscard]] std::uint64_t buildRevision() const { return m_buildRevision; }
    // ---- bot pilots (TRADE_DESIGN §6.4, step 15): hired onto an owned ship, a trade loop run abstractly once a day
    struct BotTraits
    {
        double skill = 0.5, discipline = 0.5, punctuality = 0.5, thrift = 0.5, honesty = 0.5, nerve = 0.5;
    };
    struct BotCandidate
    {
        std::string id, name, hubId;
        BotTraits traits;
        double skillShown = 0.0;
        int yearsShown = 0;
        double wageCr = 0.0;
    };
    struct Bot
    {
        std::string id, name;
        BotTraits traits;
        double skillShown = 0.0, wageCr = 0.0;
        std::uint64_t owner = 0, shipKey = 0;
        std::string shipRu;
        CargoCapacity capacity;
        double dryMassT = 0.0;
        std::string state = "idle";   // idle / at_a / leg_to_b / at_b / leg_to_a / quit
        std::string hubA, hubB, goodId;
        int systemId = -1, legDaysLeft = 0, trips = 0, unpaidDays = 0;
        double profitCr = 0.0, tripCostCr = 0.0;
        std::vector<std::pair<int, std::string>> reports;   // (the day the owner learns it, text)
    };
    [[nodiscard]] std::vector<BotCandidate> botCandidatesAt(const std::string& hubId, int systemId) const;
    game::simulation::ClientTradeResult hireBot(std::uint64_t playerId, const std::string& candidateId, const std::string& hubId, int systemId,
                                                std::uint64_t shipKey, const std::string& shipRu, const CargoCapacity& capacity, double dryMassT,
                                                std::uint32_t requestId);
    // the trade loop from the bot ship's hub to the other market hub of the system with the best margin
    game::simulation::ClientTradeResult orderBotLoop(std::uint64_t playerId, const std::string& botId, const std::string& hubId, int systemId,
                                                     std::uint32_t requestId);
    void fireBot(std::uint64_t playerId, const std::string& botId);
    [[nodiscard]] const std::vector<Bot>& bots() const { return m_bots; }
    // ---- rescue (TRADE_DESIGN §4.2, step 12): SOS calls near busy hubs; a licensee takes one and reaches the hub
    struct SosCall
    {
        std::string id, kind, kindRu, hubId;
        int systemId = -1;
        int day = 0;
        int expiresDay = 0;
        double rewardCr = 0.0;
    };
    [[nodiscard]] std::vector<SosCall> sosCallsIn(int systemId) const;
    [[nodiscard]] std::string sosStatus(const SosCall& c) const;   // open / taken / done / expired
    game::simulation::ClientTradeResult takeSos(std::uint64_t playerId, const std::string& sosId, std::uint32_t requestId);
    // the pilot's ship is at `hubId`: completes the pilot's active call there; true = completed now
    bool completeSosAt(std::uint64_t playerId, const std::string& hubId, int systemId);
    void setRoutableHubs(std::set<std::string> hubs) { m_routableHubs = std::move(hubs); }
    [[nodiscard]] const std::string& stateFilePath() const { return m_stateFile; }
    // THE WORLD SAVE (stage 8, WorldSaveStore.h): other owners' parts committed in the SAME generation as the economy.
    // The provider runs on the server thread at the flush (the same tick as the economy's own state); cleared, the
    // last parts it gave are written again (the shutdown flush after the owner is gone).
    void setCompanionProvider(std::function<std::vector<WorldSavePart>()> provider) { m_companionProvider = std::move(provider); }
    // a part loaded with the economy's generation (nullptr: none) - the owner restores from it
    [[nodiscard]] const std::string* loadedCompanion(const std::string& name) const;
    // mark a companion's change: saved with the next flush (urgent = the next tick)
    void companionChanged(const char* what, bool urgent) { saveNow(what, urgent); }
    // write now (the owner's shutdown): the provider is still alive
    void flushFinal() { flushStateIfDue(true); }
    [[nodiscard]] std::size_t routableHubCount() const { return m_routableHubs.size(); }
    // ---- the employment service (F5, TRADE_DESIGN §11b)
    struct Vacancy
    {
        std::string id, kind, employerRu, roleRu, bankId, passportId, unitRu, rankRu, requirementRu;
        double wageCr = 0.0;
        int termDays = 0;
    };
    struct Employment
    {
        Vacancy job;
        int sinceDay = 0;
    };
    [[nodiscard]] std::vector<Vacancy> vacanciesAt(const std::string& hubId, int systemId) const;
    // "" = may take it, otherwise why not (licence category, citizenship, already employed)
    [[nodiscard]] std::string vacancyRefusal(std::uint64_t playerId, const Vacancy& v, const std::string& hubId, int systemId) const;
    void startEmployment(std::uint64_t playerId, const Vacancy& v);
    void endEmployment(std::uint64_t playerId, const std::string& why);
    [[nodiscard]] const Employment* employment(std::uint64_t playerId) const;
    // ---- ship arrest (ECONOMY_BANKING 2026-11-20, TRADE_DESIGN §11d): a debt overdue >= arrestAfterDays at the bank of
    // the polity whose hub the ship docks at -> an exchange seizure (expensive modules swapped for the cheapest working
    // ones by the caller, the difference sold at auctionShare); the proceeds repay that bank, the rest is the client's
    struct ArrestClaim
    {
        bool due = false;
        std::string bankId;
        double debtCr = 0.0;
        int overdueDays = 0;
    };
    [[nodiscard]] ArrestClaim arrestClaim(std::uint64_t playerId, const std::string& hubId, int systemId) const;
    [[nodiscard]] double auctionShare() const { return m_auctionShare; }
    void settleArrest(std::uint64_t playerId, const std::string& bankId, const std::string& hubId, double proceedsCr,
                      const std::vector<std::string>& seized);
    // ---- cargo insurance (TRADE_DESIGN §3.6, step 13)
    struct InsuranceQuote
    {
        double valueCr = 0.0;
        double premiumCr = 0.0;
        std::vector<std::string> lotIds;
        std::string why;               // "" = insurable
    };
    [[nodiscard]] InsuranceQuote quoteCargoInsurance(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId, int systemId) const;
    game::simulation::ClientTradeResult insureCargo(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId, int systemId,
                                                    std::uint32_t requestId);
    game::simulation::ClientTradeResult buyLicence(std::uint64_t playerId, const std::string& type, const std::string& hubId, int systemId,
                                                   std::uint32_t requestId);
    struct CustomsResult
    {
        bool inspected = false;
        bool found = false;
        double probability = 0.0;
        double seizedT = 0.0;
        double valueCr = 0.0;
        double fineCr = 0.0;
        double feeCr = 0.0;
    };
    // the customs inspection at docking (once per port call): a deterministic roll by hub / ship / call / day
    CustomsResult customsInspection(std::uint64_t playerId, std::uint64_t shipKey, const std::string& callId, const std::string& hubId,
                                    int systemId);
    void setDevCustomsProbability(double p) { m_devCustomsP = p; }
    // NPC flows (step 10): shipments in flight now and the tonnes shipped since the start (checks)
    [[nodiscard]] std::size_t npcShipmentsInFlight() const { return m_npcShipments.size(); }
    [[nodiscard]] double npcTonnesShipped() const { return m_npcTonnesShipped; }
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
    // The open port call of a player ship (persisted, economy_state/4): opened at the docking fee, closed at the berth
    // fee. Its start is the UNIVERSE clock (anchored to real time, so it runs on across a server restart - the server
    // timeline clock restarts at 0): after a restart the call is resumed at the same berth (no second docking fee) or
    // closed when the ship is seen away from it (the berth fee up to then).
    struct OpenPortCall
    {
        std::string callId;
        std::string hubId;
        std::string dockModuleId;
        std::uint64_t playerId = 0;
        double sinceUniverseS = 0.0;
    };
    void openPortCall(std::uint64_t shipKey, const std::string& callId, const std::string& hubId, const std::string& dockModuleId,
                      std::uint64_t playerId);
    [[nodiscard]] const OpenPortCall* openPortCall(std::uint64_t shipKey) const;
    // forgets the ship's open call; the hours at the berth by the universe clock x `timeScale` (< 0: no open call)
    double closePortCall(std::uint64_t shipKey, double timeScale);
    // the universe clock (s); set by the server each tick and before start()
    void setUniverseTime(double seconds) { m_universeS = seconds; }
    [[nodiscard]] const TradeDealRules& dealRules() const { return m_dealRules; }
    // the hub's OWN market cell (no system fallback for trade); false = this hub does not trade that good
    [[nodiscard]] bool marketSide(const std::string& hubId, const std::string& goodId, MarketSide& out) const;
    // the session projection: the market of `hubId` (server prices), the manifest, the latest results; the caller adds
    // the hull figures (capacity, mass, jump range)
    [[nodiscard]] game::simulation::ClientTradeSnapshot tradeSnapshot(std::uint64_t playerId, std::uint64_t shipKey,
                                                                      const std::string& hubId, int systemId, bool docked) const;
    // ---- contracts (TRADE_DESIGN §3, §9 row 5 - trade step 5): the shared boards of every hub, the players' contracts.
    // Take: docked at the posting's origin; the deposit is held on the account (BankingModel hold; no free money = no
    // contract), the advance paid, the cargo loaded as a contract lot (owner = the issuer, never sellable). Hand-in is
    // automatic when the ship docks at the destination with the lot aboard (on time: reward - advance, deposit released;
    // late d <= late_max_days: the same minus a late penalty, never refused - a debt when unpaid). Past the deadline +
    // late_max_days, or abandoned away from the origin: failure (advance back + penalty as ContractPenalty, never
    // refused; the deposit forfeited; the cargo leaves the manifest). Abandoned at the origin before the deadline: a
    // cancel fee, the deposit released, the posting back on the board.
    struct ContractOrder
    {
        std::uint64_t playerId = 0;
        std::uint64_t shipKey = 0;
        std::string hubId;               // the hub the ship is docked at ("" = not docked)
        int systemId = -1;
        std::string contractId;
        std::uint32_t requestId = 0;
        CargoCapacity capacity;
    };
    game::simulation::ClientTradeResult takeContract(const ContractOrder& order);
    game::simulation::ClientTradeResult abandonContract(const ContractOrder& order);
    // the ship docked at `hubId` (the dispatcher's docked event): hands in every active contract of the player due there
    // whose cargo is aboard this ship; returns how many were handed in
    int deliverContracts(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId, int systemId);
    // a contract failure took cargo off this ship (outside its system pass): true once, the caller re-applies the mass
    bool takeCargoChange(std::uint64_t shipKey);
    [[nodiscard]] const ContractBoard& contracts() const { return m_board; }
    // the economy day with its fraction (the board's timers); set by the server each tick
    void setClockDays(double days) { m_clockDays = days; }

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
        std::vector<std::array<float, 8>> priceRing;   // the last 8 days' prices by good (day % 8): the NPCs' bulletins
    };
    void stepDay(int day);
    // ---- NPC flows (TRADE_DESIGN §2.6, step 10): speculators between the hubs of a system, on bulletins a few days old
    struct NpcRules
    {
        double k = 0.5;              // t / day per t / day of the destination's turnover, per unit of excess margin
        double minMargin = 0.15;     // below this margin over the ask nobody bothers
        double deliveryCrT = 40.0;   // in-system haul, Cr / t
        int lagMin = 1, lagMax = 3;  // the bulletin's age in days (in-system beacons)
        int travelMin = 1, travelMax = 3;
        double maxShareOfFlow = 2.0; // one route's daily shipment <= this x the destination's daily turnover
    };
    struct NpcShipment
    {
        std::size_t market = 0;
        int good = 0;
        double tonnes = 0.0;
        int arriveDay = 0;
    };
    NpcRules m_npcRules;
    std::vector<NpcShipment> m_npcShipments;               // in flight (not saved: a restart drops them - minimal set)
    std::unordered_map<int, std::vector<std::size_t>> m_systemMarkets;
    int m_npcRingFirstDay = -1;
    double m_npcTonnesShipped = 0.0;
    std::vector<std::vector<char>> m_npcStateGood;
    struct LicenceType
    {
        std::string id, ru;
        double feeCr = 0.0;
        std::vector<std::string> goods;
    };
    struct HeldLicence
    {
        std::string type, polity;
        int expiresDay = 0;
    };
    struct PilotCategory
    {
        std::string id, ru, control, note;
        bool start = false;
    };
    std::vector<LicenceType> m_licenceTypes;
    std::vector<PilotCategory> m_pilotCategories;
    int m_licenceDays = 360;
    std::unordered_map<std::uint64_t, std::vector<HeldLicence>> m_licences;
    struct CargoPolicy
    {
        std::uint64_t playerId = 0;
        std::uint64_t shipKey = 0;
        std::string bankId;
        std::map<std::string, double> lotValueCr;   // lot id -> insured value per tonne x tonnes at the time
        std::map<std::string, double> lotTonnes;
        double insuredCr = 0.0;
        int endDay = 0;
    };
    std::vector<CargoPolicy> m_cargoPolicies;
    double m_cargoPremiumShare = 0.004, m_deductibleShare = 0.10;
    int m_insuranceDays = 30;
    int m_arrestAfterDays = 90;
    double m_courierWageCr = 4000.0, m_militaryWageCr = 2500.0;
    double m_sosPerHubDay = 0.15, m_sosCapsuleCr = 4000.0, m_sosDerelictCr = 6000.0, m_sosMedevacCr = 10000.0;
    int m_sosOpenDays = 2;
    std::map<std::string, std::pair<std::uint64_t, std::string>> m_sosState;   // call id -> (player, "taken" | "done")
    std::unordered_map<std::uint64_t, std::string> m_activeSos;               // player -> call id
    std::set<std::string> m_routableHubs;   // the world hubs a route can reach (set by the server; empty = all)
    std::vector<Bot> m_bots;
    std::map<std::string, PlannedProject> m_buildable;
    std::vector<BuildProject> m_buildProjects;
    std::uint64_t m_buildRevision = 0;
    double m_buildBaseCostCr = 3050000.0, m_buildRetention = 0.05, m_buildHaltPDay = 0.01, m_buildIncomeYear = 0.20, m_buildMaintYear = 0.02;
    int m_buildBaseDays = 30, m_buildAbandonDays = 180;
    std::vector<double> m_buildMilestones {0.15, 0.30, 0.30, 0.20};
    std::map<std::string, double> m_buildKindFactor;
    void stepConstruction(int day);
    std::vector<Concession> m_concessions;
    double m_bulkHandlingShare = 0.3;                          // the bulk terminal tariff share (ore, water / gas)
    std::vector<std::string> m_bulkGroups {"ore", "water_gas"};
    double m_mineApplicationCr = 50000.0, m_mineSurveyCr = 200000.0, m_mineRentBaseCr = 2000.0, m_mineRentPerRadiusCr = 6.0, m_mineBaseTDay = 150.0,
           m_mineWageMonthCr = 3000.0, m_mineRoyalty = 0.07, m_mineRecoverable = 0.3, m_mineMaintYear = 0.02;
    int m_mineStaff = 12, m_mineLapseDays = 90;
    void stepMines(int day);
    std::set<std::string> m_hiredCandidates;
    int m_botCandidatesPerHub = 3, m_botLegDays = 1, m_botReportDelayDays = 1;
    double m_botBaseWageCr = 4000.0;
    void stepBots(int day);
    void botReport(Bot& b, int day, const std::string& text);
    int m_militaryTermDays = 720;
    std::vector<std::string> m_courierShips, m_militaryShips;
    std::unordered_map<std::uint64_t, Employment> m_employment;
    void payWages(int day);
    double m_auctionShare = 0.6;   // market x good: held by the state (no speculation)
    void stepNpcFlows(int day);
    // ---- smuggling (TRADE_DESIGN §2.8, step 9)
    struct SmugglingRules
    {
        double strictMult = 1.5, plannedMult = 1.5, hiddenInLegal = 0.7, attentionPerSeizure = 0.25, attentionCap = 2.0;
        double fineMult = 2.0, inspectionFeeCr = 600.0, blackMultRestricted = 1.3, blackMultBanned = 2.5, blackCapT = 50.0;
        double inspectP[5] = {0.05, 0.25, 0.7, 1.0, 1.0};   // by Channel (economy_rules customs.by_channel inspect_p)
    };
    SmugglingRules m_smuggling;
    double m_devCustomsP = -1.0;                                                  // DEV: forced inspection probability
    std::unordered_map<std::uint64_t, std::vector<std::pair<std::string, int>>> m_seizures;   // player -> (polity, day)
    std::map<std::string, std::pair<int, double>> m_blackSold;                    // hub -> (day, tonnes bought by its fence)
    [[nodiscard]] std::string familyAt(const std::string& hubId, int systemId) const;
    game::simulation::ClientTradeResult sellBlack(const TradeOrder& o);
    // the market regime (TRADE_DESIGN §2.5, step 8): "free" | "corridor" | "planned" | "mobilization" | "no_state"
    [[nodiscard]] std::string marketRegimeOf(const std::string& polity) const;
    // the side a deal / the snapshot sees: the cell + the regime (the state price, the cap; for a player the day's
    // ration left); `note` = the short line for the market row
    [[nodiscard]] MarketSide regimeSide(const HubMarket& m, int good, std::uint64_t playerId, std::string* note) const;
    void recordRation(const HubMarket& m, int good, std::uint64_t playerId, double tonnes);
    struct MarketRegimeRules
    {
        std::vector<std::string> corridorGroups {"food", "water_gas", "medical"};
        double corridorMin = 0.9, corridorMax = 1.15;
        double rationBelowStockShare = 0.5, rationMinT = 1.0, rationStockShare = 0.02;
        double stateMin = 0.8, stateMax = 1.0;
        int planDays = 360;
        double stateNeedShare = 0.7;
        std::vector<std::string> mobilizationGroups {"fuel", "weapons", "metals", "machinery"};
    };
    MarketRegimeRules m_regimeRules;
    std::unordered_map<std::string, std::string> m_regimeMarket;           // government regime -> market regime
    std::map<std::string, std::pair<int, double>> m_rationUsed;            // player|hub|good -> (day, tonnes bought)
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
    // the income tax (TaxModel, TRADE_DESIGN step 7): one book per player, the residence = the ledger's home bank
    std::unordered_map<std::uint64_t, TaxBook> m_taxBooks;
    TaxRules m_taxRules;
    TaxBook* taxBook(std::uint64_t playerId);
    [[nodiscard]] FiscalStyle residenceStyle(const ClientLedger& l) const;
    // a contract's money from its issuer: the tax withheld at source when the issuer is of another polity
    void receiveContractMoney(std::uint64_t playerId, ClientLedger& l, const std::string& issuerBank, double amountCr,
                              const std::string& memo);
    void stepTaxes(int day);
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
    // the last 64 results of a player by request id (audit 2: a re-delivered request is not run again AND gets its
    // original answer - it was dropped silently, the client waited)
    std::unordered_map<std::uint64_t, std::deque<game::simulation::ClientTradeResult>> m_replyCache;

public:
    // a request id already answered: its result published again; false = not known (run it)
    bool replayResult(std::uint64_t playerId, std::uint32_t requestId);

private:
    void stepContracts(int day);
    // failure (deadline passed / abandoned away from the origin): advance back + penalty (never refused), deposit forfeited
    void failContract(Contract& c, const std::string& status, const std::string& why);
    // State saves (perf 2026-11-19: ~1.3 MB since the contract board): saveNow only marks the state dirty;
    // flushStateIfDue (each advanceTo = each server tick) serializes it on the server thread at most once per
    // kSaveIntervalS and hands the text to ONE background writer (latest wins, atomic .tmp + replace); the destructor
    // writes the last dirty state and joins the writer. A crash loses at most ~2 s, never leaves a torn file.
    // urgent (money moved: a deal, a fee, a contract, a tax...) = written at the next tick, not after the 2 s throttle
    // (TRADE step 17: a crash right after a deal keeps the deal); the daily step stays throttled
    void saveNow(const char* what, bool urgent = true);
    bool m_urgentSave = false;
    void flushStateIfDue(bool force = false);
    // saveState = finishState(saveStateParts()): the server thread builds the small JSON tree (ledgers, cargo, ...) and
    // copies the market cells and the board's postings / contracts; the markets / board JSON + dump run in the writer
    struct StateParts;   // EconomyServer.cpp
    [[nodiscard]] StateParts saveStateParts() const;
    [[nodiscard]] static std::string finishState(StateParts parts);
    struct StateWriter;
    std::unique_ptr<StateWriter> m_writer;
    std::function<std::vector<WorldSavePart>()> m_companionProvider;
    std::vector<WorldSavePart> m_lastCompanions;     // the provider's last parts (written again once it is cleared)
    std::vector<WorldSavePart> m_loadedCompanions;   // the companions of the loaded generation
    bool m_stateDirty = false;
    const char* m_dirtyWhat = "";
    double m_lastSaveS = -1e9;   // steady clock, s
    [[nodiscard]] game::simulation::ClientContractRow contractRow(const ContractPosting& p) const;
    ContractBoard m_board;
    std::set<std::uint64_t> m_cargoChanged;
    double m_clockDays = 0.0;
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
    std::map<std::uint64_t, OpenPortCall> m_openPortCalls;   // shipKey -> the call at the berth now
    double m_universeS = 0.0;
    std::unordered_map<std::uint64_t, std::vector<game::simulation::ClientTradeResult>> m_results;
    std::uint64_t m_tradeSerial = 0;
};
} // namespace game::economy
