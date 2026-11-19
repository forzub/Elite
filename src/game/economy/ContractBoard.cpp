#include "src/game/economy/ContractBoard.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/EconomyModel.h"

namespace game::economy
{
namespace
{
using nlohmann::json;

std::uint64_t mix(std::uint64_t x)
{
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdull;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ull;
    x ^= x >> 33;
    return x;
}

std::uint64_t hashOf(const std::string& s, std::uint64_t salt)
{
    std::uint64_t h = 1469598103934665603ull ^ mix(salt + 0x9e3779b97f4a7c15ull);
    for (const unsigned char c : s)
        h = (h ^ c) * 1099511628211ull;
    return mix(h);
}

double unit(std::uint64_t h)
{
    return static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
}

double num(const json& j, const char* key, double fallback)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_number() ? it->get<double>() : fallback;
}

std::string str(const json& j, const char* key, const std::string& fallback = std::string())
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : fallback;
}

void loadSeriesRules(const json& j, ContractRules::Series& s)
{
    if (!j.is_object())
        return;
    s.periodDays = std::max(1, static_cast<int>(num(j, "period_days", s.periodDays)));
    s.postLeadDays = std::max(0, static_cast<int>(num(j, "post_lead_days", s.postLeadDays)));
    s.boardLifeDays = std::max(1, static_cast<int>(num(j, "board_life_days", s.boardLifeDays)));
    s.slack = num(j, "deadline_slack", s.slack);
    s.maxPostings = std::max(1, static_cast<int>(num(j, "max_postings", s.maxPostings)));
    s.depositShareOfGoods = num(j, "deposit_share_of_goods", s.depositShareOfGoods);
}

double mapValue(const std::map<std::string, double>& m, const std::string& key, double fallback)
{
    const auto it = m.find(key);
    return it == m.end() ? fallback : it->second;
}

int positiveMod(int a, int m)
{
    const int r = a % m;
    return r < 0 ? r + m : r;
}
} // namespace

bool loadContractRules(const json& j, ContractRules& r)
{
    if (!j.is_object() || str(j, "schema") != "contract_rules/1")
        return false;
    r.constructionEpochDay = static_cast<int>(num(j, "construction_epoch_day", r.constructionEpochDay));
    const json supply = j.value("supply", json::object());
    loadSeriesRules(supply, r.supply);
    r.supplyPeriodSmall = std::max(1, static_cast<int>(num(supply, "period_days_small", r.supplyPeriodSmall)));
    r.smallStaffBelow = static_cast<int>(num(supply, "small_staff_below", r.smallStaffBelow));
    r.smallTypes.clear();
    for (const auto& t : supply.value("small_types", json::array()))
        if (t.is_string())
            r.smallTypes.push_back(t.get<std::string>());
    r.maxPostingT = num(supply, "max_posting_t", r.maxPostingT);
    r.minPostingT = num(supply, "min_posting_t", r.minPostingT);
    r.kitGood = str(supply, "kit_good", r.kitGood);
    r.kitRu = str(supply, "kit_ru", r.kitRu);
    const json crew = j.value("crew_shift", json::object());
    loadSeriesRules(crew, r.crew);
    r.maxPeoplePerPosting = std::max(1, static_cast<int>(num(crew, "max_people_per_posting", r.maxPeoplePerPosting)));
    r.crewReturnLeg = crew.value("return_leg", r.crewReturnLeg);
    r.crewDepositPerPerson = num(crew, "deposit_cr_per_person", r.crewDepositPerPerson);
    r.p20Seats = std::max(1, static_cast<int>(num(crew, "p20_seats", r.p20Seats)));
    r.p20LoadedT = num(crew, "p20_loaded_t", r.p20LoadedT);
    r.p20Good = str(crew, "p20_good", r.p20Good);
    r.p20Ru = str(crew, "p20_ru", r.p20Ru);
    const json build = j.value("construction", json::object());
    loadSeriesRules(build, r.construction);
    r.constructionPostingT = num(build, "posting_t", r.constructionPostingT);
    r.constructionMaxPostingValueCr = num(build, "max_posting_value_cr", r.constructionMaxPostingValueCr);
    r.constructionOriginCandidates = std::max(1, static_cast<int>(num(build, "origin_candidates", r.constructionOriginCandidates)));
    r.materials.clear();
    const json materials = build.value("materials", json::object());
    for (const auto& [kind, goods] : materials.items())
        for (const auto& g : goods)
            if (g.is_string())
                r.materials[kind].push_back(g.get<std::string>());
    const json travel = j.value("travel", json::object());
    r.sameSystemDays = num(travel, "same_system_days", r.sameSystemDays);
    r.baseDays = num(travel, "base_days", r.baseDays);
    r.perJumpDays = num(travel, "per_jump_days", r.perJumpDays);
    r.legs = num(travel, "legs", r.legs);
    const json reward = j.value("reward", json::object());
    r.c20JumpCr = num(reward, "c20_jump_cr", r.c20JumpCr);
    r.c20LegCr = num(reward, "c20_leg_cr", r.c20LegCr);
    r.minRewardCr = num(reward, "min_cr", r.minRewardCr);
    r.paxJumpCr = num(reward, "pax_jump_cr", r.paxJumpCr);
    r.paxLegCr = num(reward, "pax_leg_cr", r.paxLegCr);
    r.crewMult = num(reward, "crew_mult", r.crewMult);
    r.advanceShare = num(reward, "advance_share", r.advanceShare);
    r.penaltyShare = num(reward, "penalty_share", r.penaltyShare);
    r.abandonFeeShare = num(reward, "abandon_fee_share", r.abandonFeeShare);
    r.latePerDayCargo = num(reward, "late_per_day_cargo", r.latePerDayCargo);
    r.latePerDayPeople = num(reward, "late_per_day_people", r.latePerDayPeople);
    r.lateMaxDays = static_cast<int>(num(reward, "late_max_days", r.lateMaxDays));
    const json rcm = reward.value("channel_mult", json::object());
    for (const auto& [k, v] : rcm.items())
        if (v.is_number())
            r.rewardChannelMult[k] = v.get<double>();
    const json npc = j.value("npc", json::object());
    r.npcBaseRate = num(npc, "base_rate_per_day", r.npcBaseRate);
    r.busyArrivalsDay = num(npc, "busy_arrivals_day", r.busyArrivalsDay);
    r.trafficMin = num(npc, "traffic_min", r.trafficMin);
    r.attractivenessA0 = num(npc, "attractiveness_a0", r.attractivenessA0);
    const json ncm = npc.value("channel_mult", json::object());
    for (const auto& [k, v] : ncm.items())
        if (v.is_number())
            r.npcChannelMult[k] = v.get<double>();
    const json fb = npc.value("arrivals_fallback", json::object());
    r.arrivalsPerCargoDock = num(fb, "per_cargo_dock", r.arrivalsPerCargoDock);
    r.arrivalsPerRefuelDock = num(fb, "per_refuel_dock", r.arrivalsPerRefuelDock);
    r.arrivalsPerServiceDock = num(fb, "per_service_dock", r.arrivalsPerServiceDock);
    r.arrivalsPerPad = num(fb, "per_pad", r.arrivalsPerPad);
    r.arrivalsMin = num(fb, "min", r.arrivalsMin);
    r.keepClosedDays = static_cast<int>(num(npc, "keep_closed_days", r.keepClosedDays));
    return true;
}

// ---------------------------------------------------------------- the pure model
bool isPublishDay(const ContractSeries& s, int day)
{
    const int period = std::max(1, s.periodDays);
    return positiveMod(day + s.postLeadDays - s.phaseDay, period) == 0;
}

int nextPublishDay(const ContractSeries& s, int day)
{
    const int period = std::max(1, s.periodDays);
    return day + positiveMod(period - positiveMod(day + s.postLeadDays - s.phaseDay, period), period);
}

double fairCargoRewardCr(const ContractRules& r, double jumps, int c20)
{
    return std::max(r.minRewardCr, (r.c20JumpCr * jumps + r.c20LegCr * r.legs) * std::max(1, c20));
}

double paxRewardCr(const ContractRules& r, double jumps, int people, bool crew)
{
    return (r.paxJumpCr * jumps + r.paxLegCr * r.legs) * std::max(0, people) * (crew ? r.crewMult : 1.0);
}

double trafficOf(const ContractRules& r, double arrivalsDay)
{
    return std::clamp(arrivalsDay / std::max(1.0, r.busyArrivalsDay), r.trafficMin, 1.0);
}

double attractivenessOf(const ContractRules& r, double rewardCr, double fairCr)
{
    if (!(fairCr > 0.0))
        return 1.0;
    const double a0 = std::min(0.99, r.attractivenessA0);
    return std::clamp((rewardCr / fairCr - a0) / (1.0 - a0), 0.0, 2.0);
}

double npcTakeRatePerDay(const ContractRules& r, double traffic, double attractiveness, const std::string& channel)
{
    return r.npcBaseRate * traffic * attractiveness * mapValue(r.npcChannelMult, channel, 1.0);
}

bool npcTakes(const std::string& postingId, int day, double ratePerDay)
{
    if (!(ratePerDay > 0.0))
        return false;
    const double p = 1.0 - std::exp(-ratePerDay);
    return unit(hashOf(postingId, 0x6e70630000000000ull ^ static_cast<std::uint64_t>(static_cast<std::uint32_t>(day)))) < p;
}

bool projectBuilding(const ContractSeries& s, int cd)
{
    if (!s.project)
        return true;
    if (cd < s.projectStart)
        return false;
    int halted = 0;
    for (const auto& [from, to] : s.halts)
    {
        if (to == INT_MIN)   // halted for good from `from`
        {
            if (cd >= from)
                return false;
            continue;
        }
        if (cd >= from)
        {
            if (cd < to)
                return false;
            halted += to - from;
        }
    }
    return static_cast<double>(cd - s.projectStart - halted) / std::max(1, s.projectWorkDays) < 1.0;
}

// ---------------------------------------------------------------- the board
void ContractBoard::setSeries(std::vector<ContractSeries> s)
{
    m_series = std::move(s);
    ++m_serial;
}

const ContractPosting* ContractBoard::posting(const std::string& id) const
{
    for (const auto& p : m_postings)
        if (p.id == id)
            return &p;
    return nullptr;
}

ContractPosting* ContractBoard::posting(const std::string& id)
{
    for (auto& p : m_postings)
        if (p.id == id)
            return &p;
    return nullptr;
}

Contract* ContractBoard::contract(const std::string& id)
{
    for (auto& c : m_contracts)
        if (c.p.id == id)
            return &c;
    return nullptr;
}

ContractBoard::DayReport ContractBoard::stepBoard(int day)
{
    DayReport rep;
    // 1. the open postings of earlier days: expiry first, else the NPC haulers' roll (§3.7)
    for (auto& p : m_postings)
    {
        if (p.status != "open" || p.postedDay >= day)
            continue;
        if (day >= p.expiresDay)
        {
            p.status = "expired";
            p.closedDay = day;
            ++rep.expired;
        }
        else if (npcTakes(p.id, day, p.npcRatePerDay))
        {
            p.status = "npc";
            p.closedDay = day;
            ++rep.npcTaken;
        }
    }
    // 2. closed postings leave the board after keep_closed_days (shown grey until then: "NPC взял")
    m_postings.erase(std::remove_if(m_postings.begin(), m_postings.end(),
                                    [&](const ContractPosting& p) {
                                        return p.status != "open" && p.closedDay >= 0 && p.closedDay < day - m_rules.keepClosedDays;
                                    }),
                     m_postings.end());
    // 3. today's publications: strictly on d_k (a replayed day publishes nothing twice: ids are deterministic)
    const int cday = day - m_rules.constructionEpochDay;
    for (const auto& s : m_series)
    {
        if (!isPublishDay(s, day) || !projectBuilding(s, cday))
            continue;
        for (int n = 0; n < std::max(1, s.postings); ++n)
        {
            ContractPosting p;
            p.id = s.id + "/" + std::to_string(day) + "/" + std::to_string(n);
            if (posting(p.id) || contract(p.id))
                continue;
            p.seriesId = s.id;
            p.kind = s.kind;
            p.issuer = s.issuer;
            p.issuerName = s.issuerName;
            p.originHub = s.originHub;
            p.destinationHub = s.destinationHub;
            p.cause = s.cause;
            p.goodId = s.goodId;
            p.goodRu = s.goodRu;
            p.channel = s.channel;
            p.tonnes = s.tonnesPerPosting;
            p.people = s.peoplePerPosting;
            p.densityTM3 = s.densityTM3;
            p.c20 = c20For(p.tonnes, p.densityTM3);
            p.goodsValueCr = s.goodsValueCr;
            p.fairCr = s.fairCr;
            p.rewardCr = s.rewardCr;
            p.advanceCr = s.advanceCr;
            p.depositCr = s.depositCr;
            p.penaltyCr = s.penaltyCr;
            p.lateSharePerDay = s.lateSharePerDay;
            p.npcRatePerDay = s.npcRatePerDay;
            p.postedDay = day;
            p.expiresDay = day + std::max(1, s.boardLifeDays);
            p.deadlineDay = p.expiresDay + static_cast<int>(std::ceil(s.travelDays * s.slack));
            m_postings.push_back(std::move(p));
            ++rep.published;
        }
    }
    for (const auto& p : m_postings)
        rep.open += p.status == "open";
    ++m_serial;
    return rep;
}

void ContractBoard::trimClosed(std::size_t keep)
{
    std::map<std::uint64_t, std::size_t> closed;
    for (auto it = m_contracts.rbegin(); it != m_contracts.rend(); ++it)
        if (it->status != "active")
            ++closed[it->playerId];
    for (auto it = m_contracts.begin(); it != m_contracts.end();)
    {
        if (it->status != "active" && closed[it->playerId] > keep)
        {
            --closed[it->playerId];
            it = m_contracts.erase(it);
        }
        else
            ++it;
    }
}

namespace
{
json postingToJson(const ContractPosting& p)
{
    return {{"id", p.id}, {"series", p.seriesId}, {"kind", p.kind}, {"issuer", p.issuer}, {"issuer_name", p.issuerName},
            {"origin", p.originHub}, {"dest", p.destinationHub}, {"cause", p.cause}, {"good", p.goodId}, {"good_ru", p.goodRu},
            {"channel", p.channel}, {"t", p.tonnes}, {"people", p.people}, {"c20", p.c20}, {"density", p.densityTM3},
            {"goods_cr", p.goodsValueCr}, {"fair_cr", p.fairCr}, {"reward_cr", p.rewardCr}, {"advance_cr", p.advanceCr},
            {"deposit_cr", p.depositCr}, {"penalty_cr", p.penaltyCr}, {"late_share", p.lateSharePerDay}, {"npc_rate", p.npcRatePerDay},
            {"posted", p.postedDay}, {"expires", p.expiresDay}, {"deadline", p.deadlineDay}, {"status", p.status},
            {"taken_by", p.takenBy}, {"closed", p.closedDay}};
}

ContractPosting postingFromJson(const json& j)
{
    ContractPosting p;
    p.id = j.at("id").get<std::string>();
    p.seriesId = j.value("series", std::string());
    p.kind = j.value("kind", std::string());
    p.issuer = j.value("issuer", std::string());
    p.issuerName = j.value("issuer_name", std::string());
    p.originHub = j.value("origin", std::string());
    p.destinationHub = j.value("dest", std::string());
    p.cause = j.value("cause", std::string());
    p.goodId = j.value("good", std::string());
    p.goodRu = j.value("good_ru", std::string());
    p.channel = j.value("channel", std::string("open"));
    p.tonnes = j.value("t", 0.0);
    p.people = j.value("people", 0);
    p.c20 = j.value("c20", 0);
    p.densityTM3 = j.value("density", 0.0);
    p.goodsValueCr = j.value("goods_cr", 0.0);
    p.fairCr = j.value("fair_cr", 0.0);
    p.rewardCr = j.value("reward_cr", 0.0);
    p.advanceCr = j.value("advance_cr", 0.0);
    p.depositCr = j.value("deposit_cr", 0.0);
    p.penaltyCr = j.value("penalty_cr", 0.0);
    p.lateSharePerDay = j.value("late_share", 0.1);
    p.npcRatePerDay = j.value("npc_rate", 0.0);
    p.postedDay = j.value("posted", 0);
    p.expiresDay = j.value("expires", 0);
    p.deadlineDay = j.value("deadline", 0);
    p.status = j.value("status", std::string("open"));
    p.takenBy = j.value("taken_by", std::uint64_t{0});
    p.closedDay = j.value("closed", -1);
    return p;
}
} // namespace

json ContractBoard::toJson() const
{
    json out;
    out["postings"] = json::array();
    for (const auto& p : m_postings)
        out["postings"].push_back(postingToJson(p));
    out["contracts"] = json::array();
    for (const auto& c : m_contracts)
        out["contracts"].push_back({{"posting", postingToJson(c.p)}, {"player", c.playerId}, {"ship", c.shipKey}, {"taken", c.takenDay},
                                    {"hold", c.holdId}, {"hold_bank", c.holdBank}, {"issuer_bank", c.issuerBank},
                                    {"status", c.status}, {"closed", c.closedDay}, {"late", c.lateDays}, {"paid_cr", c.paidCr},
                                    {"penalty_cr", c.penaltyCr}});
    return out;
}

void ContractBoard::fromJson(const json& j, std::vector<ContractPosting>& postings, std::vector<Contract>& contracts)
{
    postings.clear();
    contracts.clear();
    if (!j.is_object())
        return;
    for (const auto& p : j.value("postings", json::array()))
        postings.push_back(postingFromJson(p));
    for (const auto& x : j.value("contracts", json::array()))
    {
        Contract c;
        c.p = postingFromJson(x.at("posting"));
        c.playerId = x.value("player", std::uint64_t{0});
        c.shipKey = x.value("ship", std::uint64_t{0});
        c.takenDay = x.value("taken", 0);
        c.holdId = x.value("hold", 0);
        c.holdBank = x.value("hold_bank", std::string());
        c.issuerBank = x.value("issuer_bank", std::string());
        c.status = x.value("status", std::string("active"));
        c.closedDay = x.value("closed", -1);
        c.lateDays = x.value("late", 0);
        c.paidCr = x.value("paid_cr", 0.0);
        c.penaltyCr = x.value("penalty_cr", 0.0);
        contracts.push_back(std::move(c));
    }
}

void ContractBoard::commit(std::vector<ContractPosting> postings, std::vector<Contract> contracts)
{
    m_postings.swap(postings);
    m_contracts.swap(contracts);
    ++m_serial;
}

// ---------------------------------------------------------------- the series from the data
std::vector<ContractSeries> buildContractSeries(const ContractRules& r, const EconomyModel& model, const json& sites,
                                                const std::function<double(const std::string&, const std::string&)>& priceAt)
{
    std::vector<ContractSeries> out;
    const auto& goods = model.goods();
    const auto goodOf = [&](const std::string& id) -> const TradeGood* {
        for (const auto& g : goods)
            if (g.id == id)
                return &g;
        return nullptr;
    };
    // arrivals a day: the site catalog, else a docks proxy (hub_modules totals)
    std::unordered_map<std::string, double> arrivals;
    std::unordered_map<std::string, std::string> parentBody;
    const json siteList = sites.is_object() ? sites.value("sites", json::array()) : json::array();
    for (const auto& s : siteList)
    {
        if (!s.is_object())
            continue;
        if (s.contains("arrivals_day") && s["arrivals_day"].is_number())
            arrivals[str(s, "id")] = s["arrivals_day"].get<double>();
        parentBody[str(s, "id")] = str(s, "parent_body_id");
    }
    const auto arrivalsOf = [&](const HubEconomyInfo& h) {
        if (const auto it = arrivals.find(h.id); it != arrivals.end())
            return it->second;
        return std::max(r.arrivalsMin, r.arrivalsPerCargoDock * h.totals.cargoDocks + r.arrivalsPerRefuelDock * h.totals.refuelDocks +
                                           r.arrivalsPerServiceDock * h.totals.serviceDocks + r.arrivalsPerPad * h.totals.pads);
    };
    const auto route = [&](const HubEconomyInfo& from, const HubEconomyInfo& to, ContractSeries& s) {
        s.channel = model.channel(from.id, to.id);
        if (from.systemId == to.systemId && from.systemId >= 0)
        {
            s.jumps = 0.0;
            s.travelDays = r.sameSystemDays;
        }
        else
        {
            const double delayH = model.infoDelayHours(to.id, from.id);
            s.jumps = std::max(1.0, std::round(delayH > 0.0 ? delayH / 24.0 : 1.0));
            s.travelDays = r.baseDays + r.perJumpDays * s.jumps;
        }
    };
    const auto money = [&](ContractSeries& s, double fair, const HubEconomyInfo& origin) {
        s.fairCr = std::round(fair);
        s.rewardCr = std::round(fair * mapValue(r.rewardChannelMult, s.channel, 1.0) / 10.0) * 10.0;
        s.advanceCr = std::round(s.rewardCr * r.advanceShare);
        s.penaltyCr = std::round(s.rewardCr * r.penaltyShare);
        s.npcRatePerDay = npcTakeRatePerDay(r, trafficOf(r, arrivalsOf(origin)), attractivenessOf(r, s.rewardCr, s.fairCr), s.channel);
    };
    const auto priceOr = [&](const std::string& hub, const std::string& good) {
        const double p = priceAt ? priceAt(hub, good) : -1.0;
        if (p > 0.0)
            return p;
        const auto* g = goodOf(good);
        return g ? g->baseCrPerTonne : 0.0;
    };

    // 1. the remote hubs' standing needs: supply every 14 (30) days, crew rotation cohorts (§3.1, §3.2)
    for (const auto& h : model.hubs())
    {
        if (h.supply.mode != "board" || h.supply.parent.empty() || h.hidden)
            continue;
        const auto* parent = model.hub(h.supply.parent);
        if (!parent || !parent->hostsBoard)
            continue;
        const std::string issuer = h.actor.empty() ? h.operatorName : h.actor;
        const std::string issuerName = h.ownerName.empty() ? h.name : h.ownerName;
        const int phaseSalt = static_cast<int>(hashOf(h.id, 0x5e71e5ull) % 1000003ull);
        const bool small = h.supply.staff < r.smallStaffBelow ||
                           std::find(r.smallTypes.begin(), r.smallTypes.end(), h.typeId) != r.smallTypes.end();
        double kitT = 0.0, volume = 0.0, value = 0.0;
        for (const auto& [good, t] : h.supply.kit30dT)
        {
            const auto* g = goodOf(good);
            kitT += t;
            volume += t / (g && g->densityTM3 > 0.0 ? g->densityTM3 : 1.0);
            value += t * priceOr(parent->id, good);
        }
        if (kitT > 0.0)
        {
            ContractSeries s;
            s.kind = "supply";
            s.id = "SER-" + h.id + "-supply";
            s.issuer = issuer;
            s.issuerName = issuerName;
            s.originHub = parent->id;
            s.destinationHub = h.id;
            s.cause = "supply:" + h.id;
            s.goodId = r.kitGood;
            s.goodRu = r.kitRu;
            s.periodDays = small ? r.supplyPeriodSmall : r.supply.periodDays;
            s.phaseDay = phaseSalt % s.periodDays;
            s.postLeadDays = std::min(r.supply.postLeadDays, s.periodDays - 1);
            s.boardLifeDays = r.supply.boardLifeDays;
            s.slack = r.supply.slack;
            const double perPeriodT = kitT * s.periodDays / 30.0;
            if (perPeriodT >= r.minPostingT)
            {
                s.postings = std::min(r.supply.maxPostings, std::max(1, static_cast<int>(std::ceil(perPeriodT / r.maxPostingT - 1.0e-9))));
                s.tonnesPerPosting = std::min(r.maxPostingT, std::round(perPeriodT / s.postings * 100.0) / 100.0);
                s.densityTM3 = volume > 0.0 ? kitT / volume : 1.0;
                route(*parent, h, s);
                s.goodsValueCr = std::round(value / kitT * s.tonnesPerPosting);
                s.depositCr = std::round(s.goodsValueCr * r.supply.depositShareOfGoods);
                s.lateSharePerDay = r.latePerDayCargo;
                money(s, fairCargoRewardCr(r, s.jumps, c20For(s.tonnesPerPosting, s.densityTM3)), *parent);
                out.push_back(std::move(s));
            }
        }
        if (h.supply.rotationDays > 0 && h.supply.shiftSize > 0)
        {
            const int people = static_cast<int>(std::round(static_cast<double>(h.supply.shiftSize) * r.crew.periodDays / h.supply.rotationDays));
            if (people > 0)
                for (int leg = 0; leg < (r.crewReturnLeg && h.hostsBoard ? 2 : 1); ++leg)
                {
                    const auto& from = leg == 0 ? *parent : h;
                    const auto& to = leg == 0 ? h : *parent;
                    ContractSeries s;
                    s.kind = "crew_shift";
                    s.id = "SER-" + h.id + (leg == 0 ? "-crew-out" : "-crew-back");
                    s.issuer = issuer;
                    s.issuerName = issuerName;
                    s.originHub = from.id;
                    s.destinationHub = to.id;
                    s.cause = "rotation:" + h.id;
                    s.goodId = r.p20Good;
                    s.goodRu = r.p20Ru;
                    s.periodDays = r.crew.periodDays;
                    s.phaseDay = (phaseSalt + 3 + leg) % s.periodDays;   // the shift goes out, the relieved come back a day later
                    s.postLeadDays = std::min(r.crew.postLeadDays, s.periodDays - 1);
                    s.boardLifeDays = r.crew.boardLifeDays;
                    s.slack = r.crew.slack;
                    s.postings = std::min(r.crew.maxPostings,
                                          std::max(1, (people + r.maxPeoplePerPosting - 1) / r.maxPeoplePerPosting));
                    s.peoplePerPosting = std::min(r.maxPeoplePerPosting, (people + s.postings - 1) / s.postings);
                    const int modules = (s.peoplePerPosting + r.p20Seats - 1) / r.p20Seats;
                    s.tonnesPerPosting = modules * r.p20LoadedT;
                    s.densityTM3 = r.p20LoadedT / C20VolumeM3;   // one loaded P20 = one C20 place
                    route(from, to, s);
                    s.depositCr = std::round(s.peoplePerPosting * r.crewDepositPerPerson);
                    s.lateSharePerDay = r.latePerDayPeople;
                    money(s, paxRewardCr(r, s.jumps, s.peoplePerPosting, true), from);
                    out.push_back(std::move(s));
                }
        }
    }

    // 2. construction (HUB_CONSTRUCTION.md): every project of a site; it publishes only while building. A project buys its
    // materials at the busiest boarded exchanges around the same body (else the system) that trade them - up to
    // origin_candidates suppliers, each its own weekly series (a lot open to independents; the bulk goes by the
    // builder's own transport).
    std::map<std::pair<int, std::string>, std::vector<const HubEconomyInfo*>> candidates;
    const auto parentOf = [&](const std::string& id) {
        const auto it = parentBody.find(id);
        return it == parentBody.end() ? std::string() : it->second;
    };
    const auto originsFor = [&](int systemId, const std::string& good) -> const std::vector<const HubEconomyInfo*>& {
        auto [it, fresh] = candidates.try_emplace({systemId, good});
        if (fresh)
        {
            for (const auto* h : model.hubsOfSystem(systemId))
                if (h->hostsBoard && !h->hidden && priceAt && priceAt(h->id, good) > 0.0)
                    it->second.push_back(h);
            std::stable_sort(it->second.begin(), it->second.end(),
                             [&](const HubEconomyInfo* a, const HubEconomyInfo* b) { return arrivalsOf(*a) > arrivalsOf(*b); });
        }
        return it->second;
    };
    for (const auto& site : siteList)
    {
        if (!site.is_object())
            continue;
        const std::string siteId = str(site, "id");
        const auto* dest = model.hub(siteId);
        if (!dest || dest->hidden)
            continue;
        std::vector<json> projects;
        if (site.contains("expansion") && site["expansion"].is_array())
            for (const auto& p : site["expansion"])
                projects.push_back(p);
        if (site.contains("construction") && site["construction"].is_object() && site["construction"].contains("project") &&
            site["construction"]["project"].is_object())
            projects.push_back(site["construction"]["project"]);
        const std::string op = str(site, "operator", dest->operatorName);
        for (const auto& p : projects)
        {
            const std::string pid = str(p, "id");
            if (pid.empty())
                continue;
            const std::string kind = str(p, "kind", "default");
            auto mit = r.materials.find(kind);
            if (mit == r.materials.end())
                mit = r.materials.find("default");
            if (mit == r.materials.end() || mit->second.empty())
                continue;
            const std::uint64_t key = hashOf(pid, 0xb0117ull);
            const auto& mats = mit->second;
            const std::string body = parentOf(dest->id);
            // the suppliers: per material the exchanges of the same body first, then the rest of the system
            std::vector<std::pair<const HubEconomyInfo*, std::string>> suppliers;
            for (int pass = 0; pass < 2; ++pass)
                for (std::size_t k = 0; k < mats.size(); ++k)
                {
                    const std::string& m = mats[(key + k) % mats.size()];
                    for (const auto* h : originsFor(dest->systemId, m))
                    {
                        if (static_cast<int>(suppliers.size()) >= r.constructionOriginCandidates)
                            break;
                        const bool sameBody = !body.empty() && parentOf(h->id) == body;
                        if (h->id == dest->id || (pass == 0) != sameBody)
                            continue;
                        if (std::none_of(suppliers.begin(), suppliers.end(), [&](const auto& x) { return x.first == h; }))
                            suppliers.emplace_back(h, m);
                    }
                }
            for (const auto& [origin, material] : suppliers)
            {
            const auto* g = goodOf(material);
            if (!g)
                continue;
            const std::uint64_t okey = hashOf(pid + "@" + origin->id, 0xb0118ull);
            ContractSeries s;
            s.kind = "construction";
            s.id = "SER-" + pid + "-build@" + origin->id;
            s.issuer = op;
            s.issuerName = dest->operatorName.empty() ? dest->name : (dest->ownerName.empty() ? dest->operatorName : dest->ownerName);
            s.originHub = origin->id;
            s.destinationHub = dest->id;
            s.cause = "project:" + pid + " " + str(p, "title");
            s.goodId = g->id;
            s.goodRu = g->ru;
            s.densityTM3 = g->densityTM3;
            s.periodDays = r.construction.periodDays;
            s.phaseDay = static_cast<int>(okey % static_cast<std::uint64_t>(s.periodDays));
            s.postLeadDays = std::min(r.construction.postLeadDays, s.periodDays - 1);
            s.boardLifeDays = r.construction.boardLifeDays;
            s.slack = r.construction.slack;
            s.postings = r.construction.maxPostings;
            const double unitCr = priceOr(origin->id, g->id);
            s.tonnesPerPosting = unitCr > 0.0 ? std::min(r.constructionPostingT, std::max(1.0, std::floor(r.constructionMaxPostingValueCr / unitCr)))
                                              : r.constructionPostingT;
            route(*origin, *dest, s);
            s.goodsValueCr = std::round(unitCr * s.tonnesPerPosting);
            s.depositCr = std::round(s.goodsValueCr * r.construction.depositShareOfGoods);
            s.lateSharePerDay = r.latePerDayCargo;
            money(s, fairCargoRewardCr(r, s.jumps, c20For(s.tonnesPerPosting, s.densityTM3)), *origin);
            s.project = true;
            s.projectStart = static_cast<int>(num(p, "start_day", 0.0));
            s.projectWorkDays = std::max(1, static_cast<int>(num(p, "work_days", 1.0)));
            for (const auto& h : p.value("halts", json::array()))
                if (h.is_array() && h.size() >= 2 && h[0].is_number())
                    s.halts.emplace_back(h[0].get<int>(), h[1].is_number() ? h[1].get<int>() : INT_MIN);
            out.push_back(std::move(s));
            }
        }
    }
    return out;
}
} // namespace game::economy
