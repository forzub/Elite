#pragma once

#include <climits>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// The server's contract board (docs/TRADE_DESIGN.md §3, §7.1, §9 row 5 - trade step 5). A member of EconomyServer (the
// only writer of money, cargo and contracts); this file holds the STATE (series, postings, taken contracts) and the
// pure model (publication days, reward, NPC competition roll) - no ledger here: the money moves in EconomyServer.
//
// Boards are SHARED (author 2026-11-19, §11 item 9): one posting list per hub for every player, first come first
// served; the NPC haulers compete by one deterministic daily roll per open posting (§3.7).
namespace game::economy
{
class EconomyModel;

struct ContractRules
{
    int constructionEpochDay = 9873;   // economy day (universe s / 86400) of construction day 0 (3126-01-01)
    struct Series
    {
        int periodDays = 14;
        int postLeadDays = 7;
        int boardLifeDays = 3;
        double slack = 2.0;
        int maxPostings = 1;
        double depositShareOfGoods = 0.0;
    } supply, crew, construction;
    int supplyPeriodSmall = 30;
    int smallStaffBelow = 20;
    std::vector<std::string> smallTypes;
    double maxPostingT = 40.0;
    double minPostingT = 0.5;
    std::string kitGood = "supply_kit";
    std::string kitRu = "набор снабжения";
    int maxPeoplePerPosting = 48;
    bool crewReturnLeg = true;
    double crewDepositPerPerson = 0.0;
    int p20Seats = 12;
    double p20LoadedT = 9.0;
    std::string p20Good = "crew_p20";
    std::string p20Ru = "вахта (модули P20)";
    double constructionPostingT = 40.0;
    double constructionMaxPostingValueCr = 60000.0;   // dear materials come in smaller lots
    int constructionOriginCandidates = 3;
    std::map<std::string, std::vector<std::string>> materials;   // project kind -> goods ("default")
    // travel / reward (§3.1)
    double sameSystemDays = 1.0, baseDays = 2.0, perJumpDays = 3.0, legs = 2.0;
    double c20JumpCr = 4700.0, c20LegCr = 1300.0, minRewardCr = 1500.0;
    double paxJumpCr = 600.0, paxLegCr = 150.0, crewMult = 1.3;
    double advanceShare = 0.2, penaltyShare = 0.5, abandonFeeShare = 0.1;
    double latePerDayCargo = 0.1, latePerDayPeople = 0.2;
    int lateMaxDays = 5;
    std::map<std::string, double> rewardChannelMult;
    // NPC competition (§3.7)
    double npcBaseRate = 0.5, busyArrivalsDay = 500.0, trafficMin = 0.1, attractivenessA0 = 0.7;
    std::map<std::string, double> npcChannelMult;
    double arrivalsPerCargoDock = 3.0, arrivalsPerRefuelDock = 1.0, arrivalsPerServiceDock = 0.5, arrivalsPerPad = 0.05,
           arrivalsMin = 2.0;
    int keepClosedDays = 1;
};
// false = not a contract_rules/1 document (the defaults stay)
bool loadContractRules(const nlohmann::json& j, ContractRules& out);

// A standing need: "hub M gets its supply every 14 days" (§3.2)
struct ContractSeries
{
    std::string id;              // "SER-<dest>-supply" | "SER-<dest>-crew-out" | "SER-<dest>-crew-back" | "SER-<project>-build"
    std::string kind;            // supply | crew_shift | construction
    std::string issuer;          // the actor that pays (the remote hub's / the project's operator)
    std::string issuerName;
    std::string originHub, destinationHub;
    std::string cause;           // "rotation:<hub>" | "supply:<hub>" | "project:<id>"
    std::string goodId;          // a trade good, the supply kit or the P20 crew modules
    std::string goodRu;
    double densityTM3 = 0.0;
    double tonnesPerPosting = 0.0;
    int peoplePerPosting = 0;
    int postings = 1;            // postings per publication day
    int periodDays = 14;         // strict
    int phaseDay = 0;            // hash(hub): the series of different hubs do not coincide
    int postLeadDays = 7;
    int boardLifeDays = 3;
    double slack = 2.0;
    double jumps = 0.0;
    double travelDays = 1.0;
    std::string channel = "open";
    // the money of one posting, fixed when the series is built (§3.1 formula; urgency 1 at the series' own slack)
    double goodsValueCr = 0.0, fairCr = 0.0, rewardCr = 0.0, advanceCr = 0.0, depositCr = 0.0, penaltyCr = 0.0;
    double lateSharePerDay = 0.1;
    double npcRatePerDay = 0.0;  // lambda of the origin board (traffic x attractiveness x channel)
    // construction: the project window in construction days (from 3126-01-01); halts (to < 0 = for good)
    bool project = false;
    int projectStart = 0, projectWorkDays = 1;
    std::vector<std::pair<int, int>> halts;
};

// ---- the pure model (unit-checked in EliteEconomyCheck)
// d_k = phase + k x period - lead: true when `day` is one of them
[[nodiscard]] bool isPublishDay(const ContractSeries& s, int day);
// the next publication day >= `day`
[[nodiscard]] int nextPublishDay(const ContractSeries& s, int day);
// the fair cargo reward (no urgency / risk premium): (4700 x jumps + 1300 x legs) x C20, at least min_cr
[[nodiscard]] double fairCargoRewardCr(const ContractRules& r, double jumps, int c20);
[[nodiscard]] double paxRewardCr(const ContractRules& r, double jumps, int people, bool crew);
[[nodiscard]] double trafficOf(const ContractRules& r, double arrivalsDay);
[[nodiscard]] double attractivenessOf(const ContractRules& r, double rewardCr, double fairCr);
// lambda per day of a posting on a board of that traffic
[[nodiscard]] double npcTakeRatePerDay(const ContractRules& r, double traffic, double attractiveness, const std::string& channel);
// the deterministic daily roll: hash(postingId, day) < 1 - exp(-rate)
[[nodiscard]] bool npcTakes(const std::string& postingId, int day, double ratePerDay);
// construction day -> building (the project's own rule: start, work days, halts; HUB_CONSTRUCTION.md "Time")
[[nodiscard]] bool projectBuilding(const ContractSeries& s, int constructionDay);

struct ContractPosting
{
    std::string id;              // "<series>/<day>/<n>"
    std::string seriesId, kind, issuer, issuerName, originHub, destinationHub, cause, goodId, goodRu, channel;
    double tonnes = 0.0;
    int people = 0;
    int c20 = 0;
    double densityTM3 = 0.0;
    double goodsValueCr = 0.0, fairCr = 0.0, rewardCr = 0.0, advanceCr = 0.0, depositCr = 0.0, penaltyCr = 0.0;
    double lateSharePerDay = 0.1;
    double npcRatePerDay = 0.0;
    int postedDay = 0, expiresDay = 0, deadlineDay = 0;
    std::string status = "open"; // open | taken | npc | expired
    std::uint64_t takenBy = 0;
    int closedDay = -1;
};

struct Contract
{
    ContractPosting p;
    std::uint64_t playerId = 0;
    std::uint64_t shipKey = 0;
    int takenDay = 0;
    int holdId = 0;              // the deposit's hold (0 = none)
    std::string holdBank;
    std::string issuerBank;
    std::string status = "active";   // active | delivered | late | failed | abandoned | cancelled
    int closedDay = -1;
    int lateDays = 0;
    double paidCr = 0.0;         // the payout at hand-in (reward - advance)
    double penaltyCr = 0.0;      // what was charged (late / failure / cancel fee + the advance back)
};

class ContractBoard
{
public:
    void setRules(const ContractRules& r) { m_rules = r; }
    [[nodiscard]] const ContractRules& rules() const { return m_rules; }
    void setSeries(std::vector<ContractSeries> s);
    [[nodiscard]] const std::vector<ContractSeries>& series() const { return m_series; }

    struct DayReport
    {
        int published = 0, npcTaken = 0, expired = 0, open = 0;
    };
    // the board's part of the daily step (§7.3 item 5): expiries, the NPC rolls, then the publications of `day`
    DayReport stepBoard(int day);

    [[nodiscard]] const ContractPosting* posting(const std::string& id) const;
    ContractPosting* posting(const std::string& id);
    [[nodiscard]] const std::vector<ContractPosting>& postings() const { return m_postings; }
    [[nodiscard]] std::vector<Contract>& contracts() { return m_contracts; }
    [[nodiscard]] const std::vector<Contract>& contracts() const { return m_contracts; }
    Contract* contract(const std::string& id);
    [[nodiscard]] std::uint64_t serial() const { return m_serial; }
    void touch() { ++m_serial; }
    // closed contracts beyond the newest `keep` per player are dropped
    void trimClosed(std::size_t keep);

    [[nodiscard]] nlohmann::json toJson() const;
    // parses into `out` (postings + contracts; the series and rules stay those of `out`); throws on malformed input
    static void fromJson(const nlohmann::json& j, std::vector<ContractPosting>& postings, std::vector<Contract>& contracts);
    void commit(std::vector<ContractPosting> postings, std::vector<Contract> contracts);

private:
    ContractRules m_rules;
    std::vector<ContractSeries> m_series;
    std::vector<ContractPosting> m_postings;
    std::vector<Contract> m_contracts;
    std::uint64_t m_serial = 1;
};

// Builds every series from the data (§3.1): the remote hubs' supply and crew rotation (hub_modules supply) and the
// construction projects (sites_3126 expansion / construction) that are building. `priceAt(hub, good)` = the origin
// market's mid price (< 0 = not traded there); `arrivalsOf(hub)` = arrivals a day (< 0 = unknown).
[[nodiscard]] std::vector<ContractSeries> buildContractSeries(const ContractRules& rules, const EconomyModel& model,
                                                              const nlohmann::json& sites,
                                                              const std::function<double(const std::string&, const std::string&)>& priceAt);
} // namespace game::economy
