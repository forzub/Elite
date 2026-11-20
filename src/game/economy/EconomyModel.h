#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Live economy, first runtime slice (data: src/assets/data/economy, generated
// by tools/colonization_lab/economy_live.py - PROPOSAL).
//
// The model is DETERMINISTIC: the market of a hub on day N and its bulletin
// board are pure functions of the data and N (hash-seeded slow price drift,
// events in 30-day windows with the rule probabilities, board entries per
// day). Any observer computes the same state for any day - a remote hub's
// "last known prices" are simply its market on day (now - network delay).
// Player trades do not exist yet; once they do, the state moves to the
// server and this model becomes its baseline.
namespace game::economy
{
struct TradeGood
{
    std::string id;
    std::string ru;
    std::string group;
    std::string groupRu;
    double baseCrPerTonne = 0.0;
    double densityTM3 = 0.0;   // t / m3 (a C20 holds 23.3 m3: light goods fill it by volume)
    // regime family (open / strict / faith / military) -> "legal" | "restricted" | "banned" (TRADE_DESIGN §2.8)
    std::map<std::string, std::string> legality;
    double shelfDays = 0.0;    // shelf life (0 = keeps): the dumping of a glut (TRADE_DESIGN §2.4)
};

struct HubEconomyInfo
{
    std::string id;
    std::string name;
    std::string typeRu;
    std::string ownerName;
    std::string operatorName;
    std::string bloc;
    std::string actor;         // stable 3126 actor / unit id (canon/naming.json resolves its names)
    std::string market;        // full / supply / ... / none
    std::string purpose;
    std::string description;
    std::string systemName;
    int systemId = -1;
    bool hostsBoard = false;
    bool remote = false;
    bool hidden = false;
    std::string typeId;        // hub_modules "type" (mining_hub, science_hub, beacon_relay, ...)
    // the standing need of a remote hub (hub_modules supply; TRADE_DESIGN §3.1 - the contract series of step 5)
    struct Supply
    {
        std::string mode;      // "board" (published on the parent's board) | "own_ship" | "" (none)
        std::string parent;    // the hub that supplies it
        int staff = 0;
        int rotationDays = 0;
        int shiftSize = 0;
        std::vector<std::pair<std::string, double>> kit30dT;   // good id -> tonnes per 30 days
    } supply;
    struct Totals
    {
        int cargoDocks = 0, refuelDocks = 0, serviceDocks = 0, pads = 0;
    } totals;
    // Composition (hub_modules blocks): one line per block, e.g.
    // "Пассажирский блок: челночный док x141, отель x21".
    std::vector<std::string> composition;
    // The same composition per module (block order kept). This is the TARGET
    // composition of the world model: hub_modules carries no build status -
    // what really operates comes from the runtime hub (the map decides).
    struct Module
    {
        std::string block;     // admin / passenger / cargo / service / ...
        std::string blockRu;
        std::string type;      // module_types key (admin_station, shuttle_dock, ...)
        std::string ru;
        int count = 1;
    };
    std::vector<Module> modules;
};

struct MarketQuote
{
    int good = -1;             // index into goods()
    bool traded = false;
    double buyCr = 0.0;        // the player pays (Cr / t)
    double sellCr = 0.0;       // the player receives
    double stockT = 0.0;
    double drift = 0.0;        // vs the base price: +0.12 = 12 % above
};

struct ActiveEvent
{
    std::string type;
    std::string ru;
    int startDay = 0;
    int endDay = 0;            // exclusive
};


class EconomyModel
{
public:
    static const EconomyModel& instance();

    [[nodiscard]] bool loaded() const { return m_loaded; }
    [[nodiscard]] const std::vector<TradeGood>& goods() const { return m_goods; }
    // the regime family of a government regime (trade_goods.json regime_family; unknown / no state -> "open")
    [[nodiscard]] std::string regimeFamily(const std::string& regime) const;
    // "legal" | "restricted" | "banned" of a good in a regime family (unknown -> "legal")
    [[nodiscard]] std::string legalityOf(int good, const std::string& family) const;
    [[nodiscard]] const std::vector<HubEconomyInfo>& hubs() const { return m_hubs; }
    [[nodiscard]] const HubEconomyInfo* hub(const std::string& id) const;
    [[nodiscard]] std::vector<const HubEconomyInfo*> hubsOfSystem(int systemId) const;

    [[nodiscard]] std::vector<MarketQuote> market(const std::string& hubId, int day) const;
    [[nodiscard]] std::vector<ActiveEvent> events(const std::string& hubId, int day) const;

    // Hours a price bulletin of `hubId` needs to reach `observerHubId` over the
    // beacon network (bulk queue); < 0 = no connection.
    [[nodiscard]] double infoDelayHours(const std::string& hubId, const std::string& observerHubId) const;
    // Bloc channel between two hubs' blocs.
    [[nodiscard]] std::string channel(const std::string& fromHub, const std::string& toHub) const;

    // Universe seconds -> economy day.
    [[nodiscard]] static int dayOf(double universeSeconds);

private:
    EconomyModel();
    void load();

    struct HubMarketBase
    {
        std::vector<double> priceCr;   // < 0 = not traded
        std::vector<double> stockT;
        std::vector<double> spread;
    };
    struct EventType
    {
        std::string id;
        std::string ru;
        double p30 = 0.0;
        int minDays = 1;
        int maxDays = 7;
    };
    struct DelayNode
    {
        int node = -1;
        double lightLagS = 0.0;
    };

    bool m_loaded = false;
    std::vector<TradeGood> m_goods;
    std::map<std::string, std::string> m_regimeFamily;
    std::vector<HubEconomyInfo> m_hubs;
    std::unordered_map<std::string, std::size_t> m_hubIndex;
    std::unordered_map<std::string, HubMarketBase> m_markets;
    std::vector<EventType> m_eventTypes;
    std::unordered_map<std::string, DelayNode> m_delayNodes;
    std::vector<std::vector<double>> m_bulkH;   // node x node, < 0 = none
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> m_blocMatrix;
};
} // namespace game::economy
