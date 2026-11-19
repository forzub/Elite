#include "src/game/economy/EconomyModel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace game::economy
{
namespace
{
using nlohmann::json;

bool readJson(const std::string& name, json& out)
{
    for (const std::string prefix : {"assets/data/economy/", "src/assets/data/economy/"})
    {
        std::ifstream file(prefix + name, std::ios::binary);
        if (!file)
            continue;
        try
        {
            out = json::parse(file);
            return true;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[Economy] " << name << ": " << e.what() << '\n';
            return false;
        }
    }
    std::cerr << "[Economy] missing " << name << '\n';
    return false;
}

// splitmix64 + string hash: every random choice is a pure function of its key
std::uint64_t mix(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

std::uint64_t hashOf(const std::string& s, std::uint64_t salt = 0)
{
    std::uint64_t h = 1469598103934665603ull ^ mix(salt);
    for (const char c : s)
        h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    return mix(h);
}

// null-tolerant readers (the generated files write explicit nulls)
std::string str(const json& j, const char* key, const std::string& fallback = std::string())
{
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : fallback;
}

double num(const json& j, const char* key, double fallback = 0.0)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_number() ? it->get<double>() : fallback;
}

bool flag(const json& j, const char* key)
{
    const auto it = j.find(key);
    return it != j.end() && it->is_boolean() && it->get<bool>();
}

double unit(std::uint64_t h)
{
    return static_cast<double>(mix(h) >> 11) * (1.0 / 9007199254740992.0);
}

// smooth value noise over days in [-1, 1]
double dayNoise(std::uint64_t key, double day, double periodDays)
{
    const double x = day / periodDays;
    const double i = std::floor(x);
    const double f = x - i;
    const double s = f * f * (3.0 - 2.0 * f);
    const auto at = [&](double k) { return unit(key ^ mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(k) + (1ll << 40)))) * 2.0 - 1.0; };
    return at(i) * (1.0 - s) + at(i + 1.0) * s;
}

// price effect of an event on a goods group
double eventFactor(const std::string& type, const std::string& group, const std::string& goodId)
{
    if (type == "shortage") return 1.45;
    if (type == "surplus") return 0.70;
    if (type == "harvest_failure") return group == "food" ? 1.60 : 1.0;
    if (type == "fuel_crunch") return group == "fuel" ? 1.80 : 1.05;
    if (type == "epidemic" || type == "quarantine") return group == "medicine" ? 1.70 : (group == "food" ? 1.15 : 1.0);
    if (type == "blockade") return 1.30;
    if (type == "piracy_wave") return 1.15;
    if (type == "plant_accident") return group == "machinery" || group == "metals" ? 1.30 : 1.0;
    if (type == "construction_boom") return group == "construction" || group == "metals" ? 1.40 : 1.0;
    if (type == "festival") return group == "luxury" || goodId == "food_fresh" ? 1.30 : 1.0;
    if (type == "dock_strike") return 1.10;
    return 1.0;
}
} // namespace

const EconomyModel& EconomyModel::instance()
{
    static const EconomyModel model;
    return model;
}

EconomyModel::EconomyModel()
{
    load();
}

int EconomyModel::dayOf(double universeSeconds)
{
    return static_cast<int>(std::floor(universeSeconds / 86400.0));
}

void EconomyModel::load()
{
    json goods, hubs, market, rules, delay;
    if (!readJson("trade_goods.json", goods) || !readJson("hub_modules_3126.json", hubs) ||
        !readJson("market_state_3126.json", market) || !readJson("economy_rules.json", rules) ||
        !readJson("price_info_delay_3126.json", delay))
        return;

    std::unordered_map<std::string, std::string> groupRu;
    for (const auto& g : goods.value("groups", json::array()))
        groupRu[str(g, "id")] = str(g, "ru");
    std::unordered_map<std::string, std::size_t> goodIndex;
    for (const auto& g : goods.value("goods", json::array()))
    {
        TradeGood t;
        t.id = str(g, "id");
        t.ru = str(g, "ru", t.id);
        t.group = str(g, "group");
        t.groupRu = groupRu[t.group];
        t.baseCrPerTonne = num(g, "base_cr_t");
        t.densityTM3 = num(g, "density_t_m3", 0.0);
        goodIndex[t.id] = m_goods.size();
        m_goods.push_back(std::move(t));
    }

    std::unordered_map<std::string, std::string> moduleRu;
    if (hubs.contains("module_types") && hubs["module_types"].is_object())
        for (const auto& [type, info] : hubs["module_types"].items())
            moduleRu[type] = info.is_object() ? str(info, "ru", type) : type;
    for (const auto& h : hubs.value("hubs", json::array()))
    {
        HubEconomyInfo i;
        for (const auto& block : h.value("blocks", json::array()))
        {
            std::string line;
            for (const auto& module : block.value("modules", json::array()))
            {
                const std::string type = str(module, "type");
                const auto ru = moduleRu.find(type);
                const int count = static_cast<int>(num(module, "count", 1.0));
                line += (line.empty() ? "" : ", ") + (ru == moduleRu.end() ? type : ru->second) +
                        (count > 1 ? " ×" + std::to_string(count) : std::string());
                i.modules.push_back({str(block, "block"), str(block, "ru", str(block, "block")), type,
                                     ru == moduleRu.end() ? type : ru->second, std::max(1, count)});
            }
            if (!line.empty())
                i.composition.push_back(str(block, "ru", str(block, "block")) + ": " + line);
        }
        i.id = str(h, "id");
        i.name = str(h, "name", i.id);
        i.typeRu = str(h, "type_ru");
        i.ownerName = str(h, "owner_name");
        i.operatorName = str(h, "operator");
        i.bloc = str(h, "bloc");
        i.actor = str(h, "actor");
        i.market = str(h, "market");
        i.purpose = str(h, "purpose");
        i.description = str(h, "description_ru");
        i.systemName = str(h, "system_name");
        i.systemId = static_cast<int>(num(h, "system_id", -1.0));
        i.hostsBoard = flag(h, "hosts_board");
        i.hidden = flag(h, "hidden");
        i.typeId = str(h, "type");
        if (h.contains("supply") && h["supply"].is_object())
        {
            const json& sup = h["supply"];
            i.remote = flag(sup, "remote");
            i.supply.mode = str(sup, "supply_mode");
            i.supply.parent = str(sup, "supply_parent");
            i.supply.staff = static_cast<int>(num(sup, "staff", 0.0));
            i.supply.rotationDays = static_cast<int>(num(sup, "rotation_days", 0.0));
            i.supply.shiftSize = static_cast<int>(num(sup, "shift_size", 0.0));
            if (sup.contains("kit_30d_t") && sup["kit_30d_t"].is_object())
                for (const auto& [good, t] : sup["kit_30d_t"].items())
                    if (t.is_number())
                        i.supply.kit30dT.emplace_back(good, t.get<double>());
        }
        if (h.contains("totals") && h["totals"].is_object())
        {
            const json& t = h["totals"];
            i.totals.cargoDocks = static_cast<int>(num(t, "cargo_docks", 0.0));
            i.totals.refuelDocks = static_cast<int>(num(t, "refuel_docks", 0.0));
            i.totals.serviceDocks = static_cast<int>(num(t, "service_docks", 0.0));
            i.totals.pads = static_cast<int>(num(t, "pads", 0.0));
        }
        m_hubIndex[i.id] = m_hubs.size();
        m_hubs.push_back(std::move(i));
    }

    // market arrays follow the file's goods_order
    std::vector<int> order;
    for (const auto& id : market.value("goods_order", json::array()))
    {
        const auto it = goodIndex.find(id.get<std::string>());
        order.push_back(it == goodIndex.end() ? -1 : static_cast<int>(it->second));
    }
    for (const auto& h : market.value("hubs", json::array()))
    {
        HubMarketBase b;
        b.priceCr.assign(m_goods.size(), -1.0);
        b.stockT.assign(m_goods.size(), 0.0);
        b.spread.assign(m_goods.size(), 0.1);
        const auto& price = h.value("price_cr", json::array());
        const auto& stock = h.value("stock_t", json::array());
        const double spread = h.contains("spread") && h["spread"].is_number() ? h["spread"].get<double>() : 0.1;
        for (std::size_t k = 0; k < order.size() && k < price.size(); ++k)
        {
            const int g = order[k];
            if (g < 0 || !price[k].is_number())
                continue;
            b.priceCr[static_cast<std::size_t>(g)] = price[k].get<double>();
            if (k < stock.size() && stock[k].is_number())
                b.stockT[static_cast<std::size_t>(g)] = stock[k].get<double>();
            b.spread[static_cast<std::size_t>(g)] = spread;
        }
        m_markets[str(h, "hub")] = std::move(b);
    }

    if (rules.contains("events") && rules["events"].contains("types"))
        for (const auto& [id, e] : rules["events"]["types"].items())
        {
            EventType t;
            t.id = id;
            t.ru = str(e, "ru", id);
            t.p30 = num(e, "p_30d");
            const auto daysIt = e.find("days");
            const json days = daysIt != e.end() && daysIt->is_array() ? *daysIt : json::array();
            if (days.size() == 2 && days[0].is_number() && days[1].is_number())
            {
                t.minDays = std::max(1, days[0].get<int>());
                t.maxDays = std::max(t.minDays, days[1].get<int>());
            }
            m_eventTypes.push_back(std::move(t));
        }
    if (rules.contains("blocs") && rules["blocs"].contains("matrix"))
        for (const auto& [from, row] : rules["blocs"]["matrix"].items())
            for (const auto& [to, channel] : row.items())
                if (channel.is_string())
                    m_blocMatrix[from][to] = channel.get<std::string>();
    if (rules.contains("board") && rules["board"].contains("reward"))
    {
    }

    // bound to a local: `delay.value(...)` is a temporary and `.items()` only refers to it - in a
    // range-for (before C++23) the temporary died before the loop and the keys were read from freed
    // memory (intermittent startup crash in EconomyModel::load, 2026-11-11)
    const json delayHubs = delay.value("hubs", json::object());
    for (const auto& [id, n] : delayHubs.items())
        m_delayNodes[id] = {static_cast<int>(num(n, "node", -1.0)), num(n, "light_lag_s")};
    for (const auto& row : delay.value("bulk_h", json::array()))
    {
        std::vector<double> r;
        for (const auto& v : row)
            r.push_back(v.is_number() ? v.get<double>() : -1.0);
        m_bulkH.push_back(std::move(r));
    }

    m_loaded = !m_goods.empty() && !m_hubs.empty();
    std::cerr << "[Economy] " << m_goods.size() << " goods, " << m_hubs.size() << " hubs, " << m_markets.size()
              << " markets, " << m_eventTypes.size() << " event types" << '\n';
}

const HubEconomyInfo* EconomyModel::hub(const std::string& id) const
{
    const auto it = m_hubIndex.find(id);
    return it == m_hubIndex.end() ? nullptr : &m_hubs[it->second];
}

std::vector<const HubEconomyInfo*> EconomyModel::hubsOfSystem(int systemId) const
{
    std::vector<const HubEconomyInfo*> out;
    for (const auto& h : m_hubs)
        if (h.systemId == systemId && !h.hidden)
            out.push_back(&h);
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) { return a->name < b->name; });
    return out;
}

std::vector<ActiveEvent> EconomyModel::events(const std::string& hubId, int day) const
{
    std::vector<ActiveEvent> out;
    // an event starting in this or the previous 30-day window can still run
    const int window = static_cast<int>(std::floor(day / 30.0));
    for (const auto& t : m_eventTypes)
        for (int w = window - 1; w <= window; ++w)
        {
            const std::uint64_t key = hashOf(hubId, hashOf(t.id, static_cast<std::uint64_t>(w + 100000)));
            if (unit(key) >= t.p30)
                continue;
            const int start = w * 30 + static_cast<int>(unit(key ^ 0x51ull) * 30.0);
            const int length = t.minDays + static_cast<int>(unit(key ^ 0x73ull) * (t.maxDays - t.minDays + 1));
            if (day >= start && day < start + length)
                out.push_back({t.id, t.ru, start, start + length});
        }
    return out;
}

std::vector<MarketQuote> EconomyModel::market(const std::string& hubId, int day) const
{
    std::vector<MarketQuote> out(m_goods.size());
    const auto it = m_markets.find(hubId);
    const auto active = events(hubId, day);
    for (std::size_t g = 0; g < m_goods.size(); ++g)
    {
        auto& q = out[g];
        q.good = static_cast<int>(g);
        if (it == m_markets.end() || it->second.priceCr[g] < 0.0)
            continue;
        q.traded = true;
        const std::uint64_t key = hashOf(hubId, hashOf(m_goods[g].id));
        // slow drift (weeks) + short wobble (days)
        const double drift = 0.10 * dayNoise(key, day, 9.0) + 0.04 * dayNoise(key ^ 0x9ull, day, 2.0);
        double factor = 1.0 + drift;
        double stockFactor = 1.0 - 0.6 * drift;
        for (const auto& e : active)
        {
            double f = eventFactor(e.type, m_goods[g].group, m_goods[g].id);
            // shortage / surplus hit one good of the hub (seeded by the event)
            if (e.type == "shortage" || e.type == "surplus")
                f = (hashOf(hubId, static_cast<std::uint64_t>(e.startDay)) % m_goods.size()) == g ? f : 1.0;
            factor *= f;
            stockFactor /= f;
        }
        const double mid = it->second.priceCr[g] * factor;
        const double spread = it->second.spread[g];
        q.buyCr = mid * (1.0 + spread * 0.5);
        q.sellCr = mid * (1.0 - spread * 0.5);
        q.stockT = std::max(0.0, it->second.stockT[g] * stockFactor);
        q.drift = factor - 1.0;
    }
    return out;
}

double EconomyModel::infoDelayHours(const std::string& hubId, const std::string& observerHubId) const
{
    const auto a = m_delayNodes.find(hubId);
    const auto b = m_delayNodes.find(observerHubId);
    if (a == m_delayNodes.end() || b == m_delayNodes.end() || a->second.node < 0 || b->second.node < 0)
    {
        // inside one system the local relay reaches every hub within hours
        const auto* x = hub(hubId);
        const auto* y = hub(observerHubId);
        return x && y && x->systemId == y->systemId && x->systemId >= 0 ? 2.0 : -1.0;
    }
    const auto i = static_cast<std::size_t>(a->second.node);
    const auto j = static_cast<std::size_t>(b->second.node);
    if (i >= m_bulkH.size() || j >= m_bulkH[i].size() || m_bulkH[i][j] < 0.0)
        return -1.0;
    return a->second.lightLagS / 3600.0 + m_bulkH[i][j] + b->second.lightLagS / 3600.0;
}

std::string EconomyModel::channel(const std::string& fromHub, const std::string& toHub) const
{
    const auto* a = hub(fromHub);
    const auto* b = hub(toHub);
    if (!a || !b)
        return "licensed";
    if (a->bloc == b->bloc)
        return "open";
    for (const auto& [x, y] : {std::pair<std::string, std::string>{a->bloc, b->bloc}, {b->bloc, a->bloc}})
        if (const auto r = m_blocMatrix.find(x); r != m_blocMatrix.end())
            if (const auto c = r->second.find(y); c != r->second.end())
                return c->second;
    return "licensed";
}

} // namespace game::economy
