#include "src/game/fuel/FuelStationCatalog.h"
#include <filesystem>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "src/game/economy/EconomyServer.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cmath>
#include <map>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "src/game/economy/EconomyModel.h"
#include "src/game/economy/EconomyService.h"

namespace game::economy
{

// Atomic replace (audit 2026-11-19): the accounts were rewritten in place (trunc) - a crash mid-write left a file the
// next start read as "unreadable, fresh state", silently zeroing every account. Write <file>.tmp, flush, then replace.
thread_local unsigned long g_lastWriteError = 0;   // the last Win32 error of writeFileAtomically (diagnostics)

bool writeFileAtomically(const std::string& path, const std::string& text)
{
    const std::string tmp = path + ".tmp";
#ifdef _WIN32
    // the data reaches the disk (FlushFileBuffers) before the replace: a power loss cannot leave an empty file
    const std::wstring from = std::filesystem::path(tmp).wstring(), to = std::filesystem::path(path).wstring();
    HANDLE h = CreateFileW(from.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE && (g_lastWriteError = GetLastError(), true))
        return false;
    DWORD written = 0;
    const bool ok = WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size() &&
                    FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok)
    {
        DeleteFileW(from.c_str());
        return false;
    }
    const bool moved = MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!moved)
        g_lastWriteError = GetLastError();
    return moved;
#else
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
            return false;
        f << text;
        f.flush();
        if (!f)
            return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
#endif
}

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
            std::cerr << "[EconomyServer] " << name << ": " << e.what() << '\n';
            return false;
        }
    }
    std::cerr << "[EconomyServer] missing " << name << " - banking / daily prices off" << '\n';
    return false;
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

std::uint64_t mix(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

std::uint64_t hashOf(const std::string& s, std::uint64_t salt)
{
    std::uint64_t h = 1469598103934665603ull ^ mix(salt);
    for (const char c : s)
        h = (h ^ static_cast<unsigned char>(c)) * 1099511628211ull;
    return mix(h);
}

double unit(std::uint64_t h)
{
    return static_cast<double>(mix(h) >> 11) * (1.0 / 9007199254740992.0);
}

double signedUnit(std::uint64_t h)
{
    return unit(h) * 2.0 - 1.0;
}

// ~N(0, 1) from four uniforms (Irwin-Hall), deterministic
double gaussian(std::uint64_t h)
{
    double s = 0.0;
    for (std::uint64_t k = 0; k < 4; ++k)
        s += unit(h ^ (0x51ull * (k + 1)));
    return (s - 2.0) * std::sqrt(3.0);
}

// smooth value noise over days in [-1, 1]
double dayNoise(std::uint64_t key, double day, double periodDays)
{
    const double x = day / periodDays;
    const double i = std::floor(x);
    const double f = x - i;
    const double s = f * f * (3.0 - 2.0 * f);
    const auto at = [&](double k) { return signedUnit(key ^ mix(static_cast<std::uint64_t>(static_cast<std::int64_t>(k) + (1ll << 40)))); };
    return at(i) * (1.0 - s) + at(i + 1.0) * s;
}

std::string grouped(double v, int decimals = 0)
{
    char b[64];
    std::snprintf(b, sizeof b, decimals > 0 ? "%.*f" : "%.*f", decimals, std::fabs(v));
    std::string s = b;
    const auto dot = s.find('.');
    std::string head = s.substr(0, dot), tail = dot == std::string::npos ? "" : s.substr(dot);
    std::string out;
    for (std::size_t i = 0; i < head.size(); ++i)
    {
        if (i > 0 && (head.size() - i) % 3 == 0)
            out += ' ';
        out += head[i];
    }
    return (v < 0.0 ? "-" : "") + out + tail;
}

const char* loanKindRu(LoanKind k)
{
    switch (k)
    {
    case LoanKind::Term: return "кредит";
    case LoanKind::Lifeline: return "спасательный кредит";
    case LoanKind::Restructured: return "реструктуризация";
    }
    return "";
}
} // namespace

const char* fuelGoodId(FuelKind kind)
{
    switch (kind)
    {
    case FuelKind::JumpCharge: return "jump_charge";
    case FuelKind::ReactorFuel: return "reactor_fuel";
    case FuelKind::ReactionMass: return "tech_gases";
    }
    return "jump_charge";
}

namespace
{
double steadyS()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool saveTiming()
{
    // DEV: ELITE_DEV_ECONOMY_SAVE_TIMING=1 logs the cost of every state save (server thread + writer thread)
    static const bool on = [] {
        const char* v = std::getenv("ELITE_DEV_ECONOMY_SAVE_TIMING");
        return v && *v == '1';
    }();
    return on;
}
constexpr double kSaveIntervalS = 2.0;   // a crash loses at most this (+ one tick + one write)
} // namespace

// ONE background writer: the newest state replaces a pending one (latest wins); it finishes the serialization (the
// board JSON from the copied postings / contracts, dump) and replaces the file atomically by writeFileAtomically (a crash
// mid-write leaves the previous complete file). The destructor drains the pending state.
struct EconomyServer::StateParts
{
    struct Cell
    {
        bool traded = false;
        double stockT = 0.0, priceCr = 0.0;
    };
    struct Market
    {
        std::string hubId;
        std::vector<Cell> cells;
    };
    nlohmann::json head;
    std::vector<Market> markets;
    std::vector<ContractPosting> postings;
    std::vector<Contract> contracts;
};

struct EconomyServer::StateWriter
{
    std::string path;
    std::mutex mutex;
    std::condition_variable wake;
    StateParts pending;
    bool hasPending = false;
    bool stop = false;
    std::thread thread;

    explicit StateWriter(std::string p) : path(std::move(p)), thread([this] { run(); }) {}
    ~StateWriter()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            stop = true;
        }
        wake.notify_one();
        thread.join();
    }
    void submit(StateParts parts)
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            pending = std::move(parts);
            hasPending = true;
        }
        wake.notify_one();
    }
    // a finished state whose write failed (audit ECO-5: it was dropped after ~3 s of retries - the confirmed deal lost
    // with the next crash, nothing re-queued it): retried every 2 s until written or superseded by a newer state
    std::string unsaved;
    void run()
    {
        for (;;)
        {
            StateParts parts;
            std::string retryText;
            {
                std::unique_lock<std::mutex> lock(mutex);
                if (unsaved.empty())
                    wake.wait(lock, [this] { return hasPending || stop; });
                else
                    wake.wait_for(lock, std::chrono::seconds(2), [this] { return hasPending || stop; });
                if (hasPending)
                {
                    parts = std::move(pending);
                    pending = StateParts{};
                    hasPending = false;
                    unsaved.clear();   // superseded
                }
                else if (!unsaved.empty())
                    retryText = unsaved;   // the failed one again (also on stop: one last attempt)
                else if (stop)
                    return;   // stop with nothing left to write
                else
                    continue;
            }
            const double t0 = steadyS();
            const std::string text = retryText.empty() ? finishState(std::move(parts)) : std::move(retryText);
            const double t1 = steadyS();
            // the rename over the old file fails while another process holds it open (a reader, an antivirus, a backup
            // on Windows): retry briefly; still failing - keep this state queued unless a newer one arrived meanwhile
            // (TRADE step 17: found by the urgent-save check - a reader made the save silently drop the deal)
            bool ok = false;
            for (int attempt = 0; attempt < 40 && !ok; ++attempt)   // ~3 s: 10 x 20 ms, then 30 x 100 ms
            {
                ok = writeFileAtomically(path, text);
                if (!ok)
                {
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        if (hasPending)
                            break;   // a newer state is waiting: write that one instead
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(attempt < 10 ? 20 : 100));
                }
            }
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (!ok && !hasPending && !stop)
                    unsaved = text;   // kept: retried every 2 s
                else if (ok)
                    unsaved.clear();
            }
            if (!ok)
                std::cerr << "[EconomyServer] state NOT saved to " << path << " (the previous file is kept; Win32 error " << g_lastWriteError
                          << ") - " << (stop ? "giving up at shutdown" : "kept, retried every 2 s") << '\n';
            if (saveTiming())
                std::cerr << "[EconomySave] writer: " << text.size() << " B, markets + board + dump " << 1000.0 * (t1 - t0) << " ms, write "
                          << 1000.0 * (steadyS() - t1) << " ms (background)" << '\n';
        }
    }
};

EconomyServer::EconomyServer() = default;

EconomyServer::~EconomyServer()
{
    service::uninstall(this);
    const bool dirty = m_stateDirty;
    flushStateIfDue(true);   // the last dirty state reaches the disk on a clean exit
    m_writer.reset();        // joins the writer after its pending write
    if (dirty)
        std::cerr << "[EconomyServer] the last state written on shutdown: " << m_stateFile << '\n';
}

// ---------------------------------------------------------------- load
bool EconomyServer::load()
{
    json doc, market;
    const auto& model = EconomyModel::instance();
    if (!model.loaded() || !readJson("national_banks.json", doc) || !readJson("market_state_3126.json", market))
        return false;

    for (const auto& b : doc.value("banks", json::array()))
    {
        NationalBank nb;
        nb.id = str(b, "id");
        nb.polityName = str(b, "polity_name", nb.id);
        nb.name = str(b, "bank", nb.id);
        nb.currencyName = str(b, "currency");
        nb.code = str(b, "code", "CR");
        nb.bloc = str(b, "bloc");
        nb.systemId = static_cast<int>(num(b, "system_id", -1.0));
        nb.quality = num(b, "quality", 0.5);
        nb.inflation = num(b, "inflation", 0.02);
        nb.policyRate = num(b, "policy_rate", 0.04);
        nb.depositRate = num(b, "deposit_rate", 0.03);
        nb.loanRate = num(b, "loan_rate", 0.07);
        nb.overdraftRate = num(b, "overdraft_rate", 0.13);
        nb.lifelineRate = num(b, "lifeline_rate", 0.05);
        nb.reserve = b.value("reserve", false);
        nb.fxAnchorPerCr = num(b, "fx_anchor_per_cr", 1.0);
        nb.fxSigmaDay = num(b, "fx_sigma_day", 0.0);
        nb.fxHalfLifeDays = num(b, "fx_half_life_days", 730.0);
        nb.fxBand = num(b, "fx_band", 0.0);
        nb.regime = str(b, "regime");
        m_banks.add(std::move(nb));
    }
    const json noBank = doc.value("no_bank", json::object());
    for (const auto& [polity, info] : noBank.items())
        m_banks.setLifelineBank(polity, str(info, "lifeline_bank"));
    const json matrix = doc.value("bloc_matrix", json::object());
    for (const auto& [from, row] : matrix.items())
        for (const auto& [to, c] : row.items())
            if (c.is_string())
                m_banks.setChannel(from, to, channelFromString(c.get<std::string>()));

    const json r = doc.value("rules", json::object());
    auto& rules = m_banks.rules;
    rules.graceDays = static_cast<int>(num(r, "grace_days", rules.graceDays));
    rules.restructureAfterDays = static_cast<int>(num(r, "restructure_after_days", rules.restructureAfterDays));
    rules.lateFeeShare = num(r, "late_fee_share", rules.lateFeeShare);
    rules.restructureTermDays = static_cast<int>(num(r, "restructure_term_days", rules.restructureTermDays));
    rules.garnishShare = num(r, "garnish_share", rules.garnishShare);
    rules.garnishProtectedCr = num(r, "garnish_protected_cr", rules.garnishProtectedCr);
    rules.inDuplumCap = num(r, "in_duplum_cap", rules.inDuplumCap);
    rules.lifelineLimitCr90d = num(r, "lifeline_limit_cr_90d", rules.lifelineLimitCr90d);
    rules.lifelineFreeDays = static_cast<int>(num(r, "lifeline_free_days", rules.lifelineFreeDays));
    rules.lifelineFuelMaxT = num(r, "lifeline_fuel_max_t", rules.lifelineFuelMaxT);
    rules.towCostCr = num(r, "tow_cost_cr", rules.towCostCr);
    rules.minRepairCr = num(r, "min_repair_cr", rules.minRepairCr);
    rules.fxFeeShare = num(r, "fx_fee_share", rules.fxFeeShare);
    rules.cashExchangeShare = num(r, "cash_exchange_share", rules.cashExchangeShare);
    if (r.contains("transfer_fee") && r["transfer_fee"].is_object())
        for (const auto& [ch, fee] : r["transfer_fee"].items())
        {
            auto& t = rules.transfer[static_cast<int>(channelFromString(ch))];
            t.allowed = fee.is_object();
            t.share = t.allowed ? num(fee, "share", 0.0) : 0.0;
            t.minCr = t.allowed ? num(fee, "min_cr", 0.0) : 0.0;
        }
    if (r.contains("player_start") && r["player_start"].is_object())
    {
        rules.startAccountCr = num(r["player_start"], "account_cr", rules.startAccountCr);
        rules.startCashCr = num(r["player_start"], "cash_cr", rules.startCashCr);
        rules.startOverdraftCr = num(r["player_start"], "overdraft_cr", rules.startOverdraftCr);
    }

    const json p = doc.value("prices", json::object());
    m_fuelFree = p.value("fuel_free_of_charge", true);
    if (const char* env = std::getenv("ELITE_FUEL_FREE"); env && *env)
        m_fuelFree = std::atoi(env) != 0;
    m_priceRules.stockExponent = num(p, "stock_exponent", m_priceRules.stockExponent);
    m_priceRules.smoothing = num(p, "smoothing_per_day", m_priceRules.smoothing);
    m_priceRules.maxDailyChange = num(p, "max_daily_change", m_priceRules.maxDailyChange);
    if (p.contains("clamp") && p["clamp"].is_array() && p["clamp"].size() == 2)
    {
        m_priceRules.clampLo = p["clamp"][0].get<double>();
        m_priceRules.clampHi = p["clamp"][1].get<double>();
    }
    m_priceRules.minStockShare = num(p, "min_stock_share", m_priceRules.minStockShare);
    m_priceRules.demandNoise = num(p, "demand_noise", m_priceRules.demandNoise);
    m_priceRules.supplyNoise = num(p, "supply_noise", m_priceRules.supplyNoise);
    m_priceRules.slowDemandAmp = num(p, "slow_demand_amp", m_priceRules.slowDemandAmp);
    m_priceRules.slowDemandPeriodDays = num(p, "slow_demand_period_days", m_priceRules.slowDemandPeriodDays);
    {
        // TRADE step 10: dumping, glut / shortage step limits, NPC flows (trade_fees.json "npc_market")
        json fees;
        if (readJson("trade_fees.json", fees))
        {
            const json n = fees.value("npc_market", json::object());
            m_priceRules.dumpFromRatio = num(n, "dump_from_ratio", m_priceRules.dumpFromRatio);
            m_priceRules.dumpSpan = num(n, "dump_span", m_priceRules.dumpSpan);
            m_priceRules.dumpDepth = num(n, "dump_depth", m_priceRules.dumpDepth);
            m_priceRules.dumpShelfRefDays = num(n, "dump_shelf_ref_days", m_priceRules.dumpShelfRefDays);
            m_priceRules.dumpFloor = num(n, "dump_floor", m_priceRules.dumpFloor);
            m_priceRules.glutRatio = num(n, "glut_ratio", m_priceRules.glutRatio);
            m_priceRules.shortageRatio = num(n, "shortage_ratio", m_priceRules.shortageRatio);
            m_priceRules.extremeDailyChange = num(n, "extreme_daily_change", m_priceRules.extremeDailyChange);
            m_npcRules.k = num(n, "npc_k", m_npcRules.k);
            m_npcRules.minMargin = num(n, "npc_min_margin", m_npcRules.minMargin);
            m_npcRules.deliveryCrT = num(n, "npc_delivery_cr_t", m_npcRules.deliveryCrT);
            m_npcRules.lagMin = static_cast<int>(num(n, "npc_lag_min_days", m_npcRules.lagMin));
            m_npcRules.lagMax = std::max(m_npcRules.lagMin, static_cast<int>(num(n, "npc_lag_max_days", m_npcRules.lagMax)));
            m_npcRules.travelMin = static_cast<int>(num(n, "npc_travel_min_days", m_npcRules.travelMin));
            m_npcRules.travelMax = std::max(m_npcRules.travelMin, static_cast<int>(num(n, "npc_travel_max_days", m_npcRules.travelMax)));
            m_npcRules.maxShareOfFlow = num(n, "npc_max_share_of_flow", m_npcRules.maxShareOfFlow);
        }
    }
    const json lead = p.value("restock_lead_days", json::object());
    const double c20Jump = num(p, "c20_jump_cr", 4700.0), c20Leg = num(p, "c20_leg_cr", 1300.0), c20T = num(p, "c20_payload_t", 20.0);
    const json splits = p.value("production_split", json::object());   // a local: see ledgerFromJson
    for (const auto& [good, s] : splits.items())
        m_splits[good] = {num(s, "feedstock", 0.55), num(s, "conversion", 0.0), num(s, "separation_energy", 0.25), num(s, "fabrication", 0.2)};

    // hub -> polity (the hub's actor; a no-state zone keeps its zone id so it maps to its lifeline bank)
    std::unordered_map<int, std::unordered_map<std::string, int>> votes;
    for (const auto& h : model.hubs())
    {
        std::string polity = h.actor;
        if (polity.empty() && h.bloc.rfind("zone:", 0) == 0)
            polity = h.bloc.substr(5);
        m_hubPolity[h.id] = polity;
        if (!polity.empty() && h.systemId >= 0)
            ++votes[h.systemId][polity];
    }
    for (const auto& [system, v] : votes)
    {
        const auto best = std::max_element(v.begin(), v.end(), [](const auto& a, const auto& b) {
            return a.second != b.second ? a.second < b.second : a.first > b.first;
        });
        m_systemPolity[system] = best->first;
    }

    // market state: initial stock / target / flow; anchor = structural price (formula reproduces the price)
    const auto& goods = model.goods();
    std::vector<int> order;
    for (const auto& id : market.value("goods_order", json::array()))
        order.push_back(goodIndex(id.get<std::string>()));
    for (const auto& h : market.value("hubs", json::array()))
    {
        HubMarket m;
        m.hubId = str(h, "hub");
        const auto* info = model.hub(m.hubId);
        m.name = info ? info->name : m.hubId;
        m.systemId = info ? info->systemId : -1;
        m.polity = m_hubPolity.count(m.hubId) ? m_hubPolity[m.hubId] : std::string();
        m.market = str(h, "market", "local");
        m.spread = num(h, "spread", 0.1);
        m.cells.assign(goods.size(), GoodCell{});
        m.transportCrT.assign(goods.size(), 0.0);
        std::vector<std::string> producers;
        for (const auto& pr : h.value("producer", json::array()))
            if (pr.is_string())
                producers.push_back(pr.get<std::string>());
        const auto& price = h.value("price_cr", json::array());
        const auto& stock = h.value("stock_t", json::array());
        const auto& target = h.value("target_t", json::array());
        const auto& flow = h.value("flow_t_day", json::array());
        const double marketLead = num(lead, m.market.c_str(), 10.0);
        for (std::size_t k = 0; k < order.size() && k < price.size(); ++k)
        {
            const int g = order[k];
            if (g < 0 || !price[k].is_number())
                continue;
            auto& c = m.cells[static_cast<std::size_t>(g)];
            c.traded = true;
            c.priceCr = price[k].get<double>();
            c.stockT = k < stock.size() && stock[k].is_number() ? stock[k].get<double>() : 0.0;
            c.targetT = k < target.size() && target[k].is_number() ? target[k].get<double>() : c.stockT;
            c.normalStockT = c.stockT;   // the generated stock encodes the hub's structural scarcity
            c.flowTDay = k < flow.size() && flow[k].is_number() ? flow[k].get<double>() : c.targetT / 30.0;
            c.producer = std::find(producers.begin(), producers.end(), goods[static_cast<std::size_t>(g)].id) != producers.end();
            c.leadDays = c.producer ? num(lead, "producer", 3.0) : marketLead;
            c.anchorCr = anchorFromPrice(c.priceCr, c.stockT, c.targetT, m_priceRules);
        }
        m_marketIndex[m.hubId] = m_markets.size();
        m_markets.push_back(std::move(m));
    }
    // delivery cost per tonne from the nearest producer: same system = 2 legs, else 1 jump + 2 legs (C20 rates)
    std::unordered_map<int, std::vector<bool>> systemProduces;
    for (const auto& m : m_markets)
    {
        auto& v = systemProduces[m.systemId];
        v.resize(goods.size(), false);
        for (std::size_t g = 0; g < goods.size(); ++g)
            v[g] = v[g] || m.cells[g].producer;
    }
    for (auto& m : m_markets)
        for (std::size_t g = 0; g < goods.size(); ++g)
            if (m.cells[g].traded && !m.cells[g].producer)
                m.transportCrT[g] = (systemProduces[m.systemId][g] ? 2.0 * c20Leg : c20Jump + 2.0 * c20Leg) / c20T;


    // trade: the deal rules and the fiscal styles (trade_fees.json, hand-authored; defaults when missing)
    json fees;
    if (readJson("trade_fees.json", fees))
    {
        const json deal = fees.value("deal", json::object());
        m_dealRules.kappa = num(deal, "kappa", m_dealRules.kappa);
        m_dealRules.stockExponent = num(deal, "stock_exponent", m_dealRules.stockExponent);
        m_dealRules.minStockShare = num(deal, "min_stock_share", m_dealRules.minStockShare);
        m_dealRules.hubBuyCapShare = num(deal, "hub_buy_cap_share", m_dealRules.hubBuyCapShare);
        m_dealRules.handlingCrPerT = num(deal, "handling_cr_per_t", m_dealRules.handlingCrPerT);
        m_bulkHandlingShare = num(deal, "bulk_handling_share", m_bulkHandlingShare);
        if (deal.contains("bulk_groups"))
            m_bulkGroups = deal["bulk_groups"].get<std::vector<std::string>>();
        m_dealRules.dockingBaseCr = num(deal, "docking_base_cr", m_dealRules.dockingBaseCr);
        m_dealRules.dockingCrPerDryT = num(deal, "docking_cr_per_dry_t", m_dealRules.dockingCrPerDryT);
        m_dealRules.vatRefundFeeShare = num(deal, "vat_refund_fee_share", m_dealRules.vatRefundFeeShare);
        m_dealRules.berthIncludedHours = num(deal, "berth_included_hours", m_dealRules.berthIncludedHours);
        m_dealRules.berthCrPerHour = num(deal, "berth_cr_per_hour", m_dealRules.berthCrPerHour);
        m_dealRules.berthCrPerDayCap = num(deal, "berth_cr_per_day_cap", m_dealRules.berthCrPerDayCap);
        const json tax = fees.value("income_tax", json::object());
        m_taxRules.periodDays = std::max(1, static_cast<int>(num(tax, "period_days", m_taxRules.periodDays)));
        m_taxRules.declareDays = std::clamp(static_cast<int>(num(tax, "declare_days", m_taxRules.declareDays)), 0, m_taxRules.periodDays - 1);
        m_taxRules.latePenaltyShare = num(tax, "late_penalty_share", m_taxRules.latePenaltyShare);
        m_taxRules.withholdingShare = num(tax, "withholding_share", m_taxRules.withholdingShare);
        const json styles = fees.value("styles", json::object());
        for (const auto& [id, s] : styles.items())
        {
            FiscalStyle st;
            st.id = id;
            st.ru = str(s, "ru", id);
            st.vatShare = num(s, "vat", 0.0);
            st.vatRefundDays = static_cast<int>(num(s, "vat_refund_days", 0.0));
            st.turnoverShare = num(s, "turnover", 0.0);
            st.turnoverRu = str(s, "turnover_ru");
            st.portIndex = num(s, "port_index", 1.0);
            st.dutyShare = num(s, "duty", st.dutyShare);
            st.incomeTaxShare = num(s, "income_tax", 0.0);
            st.taxAdminFeeShare = num(s, "tax_admin_fee", 0.0);
            m_styles[id] = st;
        }
        const json cons = fees.value("construction", json::object());
        m_buildBaseCostCr = num(cons, "base_cost_cr", m_buildBaseCostCr);
        m_buildBaseDays = std::max(1, static_cast<int>(num(cons, "base_work_days", m_buildBaseDays)));
        m_buildRetention = num(cons, "retention_share", m_buildRetention);
        m_buildAbandonDays = static_cast<int>(num(cons, "abandon_after_unpaid_days", m_buildAbandonDays));
        m_buildHaltPDay = num(cons, "halt_p_day", m_buildHaltPDay);
        m_buildIncomeYear = num(cons, "income_share_year", m_buildIncomeYear);
        m_buildMaintYear = num(cons, "maintenance_share_year", m_buildMaintYear);
        if (cons.contains("milestones"))
            m_buildMilestones = cons["milestones"].get<std::vector<double>>();
        const json kindFactor = cons.value("kind_factor", json::object());
        for (const auto& [k, v] : kindFactor.items())
            m_buildKindFactor[k] = v.get<double>();
        const json mineJ = fees.value("mining", json::object());
        m_mineApplicationCr = num(mineJ, "application_cr", m_mineApplicationCr);
        m_mineSurveyCr = num(mineJ, "survey_cr", m_mineSurveyCr);
        m_mineRentBaseCr = num(mineJ, "rent_base_cr", m_mineRentBaseCr);
        m_mineRentPerRadiusCr = num(mineJ, "rent_per_radius_cr", m_mineRentPerRadiusCr);
        m_mineBaseTDay = num(mineJ, "base_t_day", m_mineBaseTDay);
        m_mineStaff = static_cast<int>(num(mineJ, "staff", m_mineStaff));
        m_mineWageMonthCr = num(mineJ, "wage_month_cr", m_mineWageMonthCr);
        m_mineRoyalty = num(mineJ, "royalty_share", m_mineRoyalty);
        m_mineLapseDays = static_cast<int>(num(mineJ, "lapse_after_unpaid_days", m_mineLapseDays));
        m_mineRecoverable = num(mineJ, "recoverable_share", m_mineRecoverable);
        m_mineMaintYear = num(mineJ, "maintenance_share_year", m_mineMaintYear);
        const json botsJ = fees.value("bots", json::object());
        m_botCandidatesPerHub = static_cast<int>(num(botsJ, "candidates_per_hub", m_botCandidatesPerHub));
        m_botBaseWageCr = num(botsJ, "base_wage_cr", m_botBaseWageCr);
        m_botLegDays = std::max(1, static_cast<int>(num(botsJ, "leg_days", m_botLegDays)));
        m_botReportDelayDays = std::max(0, static_cast<int>(num(botsJ, "report_delay_days", m_botReportDelayDays)));
        const json rescue = fees.value("rescue", json::object());
        m_sosPerHubDay = num(rescue, "sos_per_hub_day", m_sosPerHubDay);
        m_sosOpenDays = std::max(1, static_cast<int>(num(rescue, "open_days", m_sosOpenDays)));
        m_sosCapsuleCr = num(rescue, "reward_capsule_cr", m_sosCapsuleCr);
        m_sosDerelictCr = num(rescue, "reward_derelict_cr", m_sosDerelictCr);
        m_sosMedevacCr = num(rescue, "reward_medevac_cr", m_sosMedevacCr);
        const json emp = fees.value("employment", json::object());
        m_courierWageCr = num(emp, "courier_wage_cr", m_courierWageCr);
        m_militaryWageCr = num(emp, "military_wage_cr", m_militaryWageCr);
        m_militaryTermDays = static_cast<int>(num(emp, "military_term_days", m_militaryTermDays));
        if (emp.contains("courier_ships"))
            m_courierShips = emp["courier_ships"].get<std::vector<std::string>>();
        if (emp.contains("military_ships"))
            m_militaryShips = emp["military_ships"].get<std::vector<std::string>>();
        const json arrest = fees.value("arrest", json::object());
        m_arrestAfterDays = std::max(1, static_cast<int>(num(arrest, "after_days", m_arrestAfterDays)));
        m_auctionShare = std::clamp(num(arrest, "auction_share", m_auctionShare), 0.0, 1.0);
        const json ins = fees.value("insurance", json::object());
        m_cargoPremiumShare = num(ins, "cargo_premium_share", m_cargoPremiumShare);
        m_deductibleShare = num(ins, "deductible_share", m_deductibleShare);
        m_insuranceDays = std::max(1, static_cast<int>(num(ins, "days_valid", m_insuranceDays)));
        const json lic = fees.value("licences", json::object());
        m_licenceDays = std::max(1, static_cast<int>(num(lic, "days_valid", m_licenceDays)));
        const json licTypes = lic.value("types", json::object());   // named: items() of a temporary dangles
        for (const auto& [id, t] : licTypes.items())
        {
            LicenceType lt;
            lt.id = id;
            lt.ru = str(t, "ru", id);
            lt.feeCr = num(t, "fee_cr", 0.0);
            if (t.contains("goods"))
                lt.goods = t["goods"].get<std::vector<std::string>>();
            m_licenceTypes.push_back(std::move(lt));
        }
        for (const auto& c : lic.value("pilot_categories", json::array()))
            m_pilotCategories.push_back({str(c, "id"), str(c, "ru"), str(c, "control"), str(c, "note"), c.value("start", false)});
        const json sm = fees.value("smuggling", json::object());
        {
            auto& S = m_smuggling;
            const json rm = sm.value("regime_mult", json::object());
            const json bm = sm.value("black_mult", json::object());
            S.strictMult = num(rm, "strict", S.strictMult);
            S.plannedMult = num(rm, "planned", S.plannedMult);
            S.hiddenInLegal = num(sm, "hidden_in_legal", S.hiddenInLegal);
            S.attentionPerSeizure = num(sm, "attention_per_seizure", S.attentionPerSeizure);
            S.attentionCap = num(sm, "attention_cap", S.attentionCap);
            S.fineMult = num(sm, "fine_mult", S.fineMult);
            S.inspectionFeeCr = num(sm, "inspection_fee_cr", S.inspectionFeeCr);
            S.blackMultRestricted = num(bm, "restricted", S.blackMultRestricted);
            S.blackMultBanned = num(bm, "banned", S.blackMultBanned);
            S.blackCapT = num(sm, "black_cap_t", S.blackCapT);
        }
        const json mr = fees.value("market_regimes", json::object());
        {
            const json corridor = mr.value("corridor", json::object());
            const json planned = mr.value("planned", json::object());
            const json mob = mr.value("mobilization", json::object());
            auto& R = m_regimeRules;
            if (corridor.contains("basket_groups"))
                R.corridorGroups = corridor["basket_groups"].get<std::vector<std::string>>();
            R.corridorMin = num(corridor, "price_min", R.corridorMin);
            R.corridorMax = num(corridor, "price_max", R.corridorMax);
            R.rationBelowStockShare = num(corridor, "ration_below_stock_share", R.rationBelowStockShare);
            R.rationMinT = num(corridor, "ration_min_t", R.rationMinT);
            R.rationStockShare = num(corridor, "ration_stock_share", R.rationStockShare);
            R.stateMin = num(planned, "state_price_min", R.stateMin);
            R.stateMax = num(planned, "state_price_max", R.stateMax);
            R.planDays = std::max(1, static_cast<int>(num(planned, "plan_days", R.planDays)));
            R.stateNeedShare = num(planned, "state_need_share", R.stateNeedShare);
            if (mob.contains("groups"))
                R.mobilizationGroups = mob["groups"].get<std::vector<std::string>>();
        }
        const json regimeMarket = fees.value("regime_market", json::object());   // named: items() of a temporary dangles
        for (const auto& [regime, market] : regimeMarket.items())
            if (market.is_string())
                m_regimeMarket[regime] = market.get<std::string>();
        const json regimes = fees.value("regime_style", json::object());
        for (const auto& [regime, style] : regimes.items())
            if (style.is_string())
                m_regimeStyle[regime] = style.get<std::string>();
    }
    // the customs channel multipliers of the import duty (economy_rules.json customs.by_channel[].fee_mult - one source
    // with the customs layer of step 9). "closed" carries 0 there (no admission at all); until step 9 refuses closed
    // imports at the border, goods that got through anyway pay the black-channel rate, never less than an open border.
    json econRules;
    if (readJson("economy_rules.json", econRules))
    {
        const json byChannel = econRules.value("customs", json::object()).value("by_channel", json::object());
        for (const auto& [name, c] : byChannel.items())
        {
            const int k = static_cast<int>(channelFromString(name));
            if (k >= 0 && k < 5 && c.is_object())
            {
                m_customsFeeMult[k] = num(c, "fee_mult", m_customsFeeMult[k]);
                m_smuggling.inspectP[k] = num(c, "inspect_p", m_smuggling.inspectP[k]);   // the customs inspection (step 9)
            }
        }
    }
    if (!(m_customsFeeMult[static_cast<int>(Channel::Closed)] > 0.0))
        m_customsFeeMult[static_cast<int>(Channel::Closed)] = m_customsFeeMult[static_cast<int>(Channel::Black)];

    // contracts (TRADE_DESIGN §3, step 5): the rules (contract_rules.json) and the series from hub_modules supply and the
    // construction projects of the site catalog
    {
        json contractRules, sites;
        ContractRules rules;
        if (readJson("contract_rules.json", contractRules))
            loadContractRules(contractRules, rules);
        for (const std::string path : {"assets/data/galaxy_details/world_sites/sites_3126.json",
                                       "src/assets/data/galaxy_details/world_sites/sites_3126.json"})
        {
            std::ifstream f(path, std::ios::binary);
            if (!f)
                continue;
            try
            {
                sites = json::parse(f);
            }
            catch (const std::exception& e)
            {
                std::cerr << "[ContractBoard] " << path << ": " << e.what() << '\n';
            }
            break;
        }
        m_board.setRules(rules);
        m_board.setSeries(buildContractSeries(rules, model, sites, [this](const std::string& hub, const std::string& good) {
            MarketSide side;
            return marketSide(hub, good, side) ? side.priceCr : -1.0;
        }));
        std::size_t supply = 0, crew = 0, build = 0;
        for (const auto& s : m_board.series())
            (s.kind == "supply" ? supply : s.kind == "crew_shift" ? crew : build) += 1;
        std::cerr << "[ContractBoard] " << m_board.series().size() << " series: " << supply << " supply, " << crew << " crew shift, " << build
                  << " construction (sites " << (sites.is_object() ? "loaded" : "MISSING") << ")" << '\n';
    }
    m_loaded = !m_banks.banks().empty() && !m_markets.empty();
    std::cerr << "[EconomyServer] " << m_banks.banks().size() << " national banks, " << m_markets.size() << " markets, fuel "
              << (m_fuelFree ? "FREE of charge (list prices computed)" : "charged") << '\n';
    return m_loaded;
}

int EconomyServer::goodIndex(const std::string& id) const
{
    const auto& goods = EconomyModel::instance().goods();
    for (std::size_t g = 0; g < goods.size(); ++g)
        if (goods[g].id == id)
            return static_cast<int>(g);
    return -1;
}

// ---------------------------------------------------------------- time
void EconomyServer::start(int day)
{
    if (!m_loaded)
        return;
    m_day = day;
    if (const char* path = std::getenv("ELITE_ECONOMY_STATE_FILE"); path && *path)
    {
        m_stateFile = path;
        std::ifstream f(m_stateFile, std::ios::binary);
        if (f)
        {
            std::stringstream ss;
            ss << f.rdbuf();
            f.close();
            if (restoreState(ss.str()))
                std::cerr << "[EconomyServer] state restored from " << m_stateFile << " (day " << m_day << ")" << '\n';
            else
            {
                // never write over a file we could not read: set it aside (it may be repairable by hand)
                const std::string bad = m_stateFile + ".bad-" + std::to_string(static_cast<long long>(std::time(nullptr)));
                std::error_code ec;
                std::filesystem::rename(m_stateFile, bad, ec);
                if (ec)
                {
                    std::cerr << "[EconomyServer] " << m_stateFile << ": UNREADABLE and could not be set aside (" << ec.message()
                              << ") - saving DISABLED to protect it" << '\n';
                    m_stateFile.clear();
                }
                else
                    std::cerr << "[EconomyServer] " << m_stateFile << ": UNREADABLE - kept as " << bad << "; fresh state" << '\n';
            }
        }
    }
    // a fresh world (no saved boards): the boards as they stand today - the publications of the last board-life days
    // with their NPC rolls, as if the server had been running (otherwise every board is empty until the next day)
    if (m_board.postings().empty() && m_board.contracts().empty())
    {
        int life = 1;
        for (const auto& s : m_board.series())
            life = std::max(life, s.boardLifeDays);
        ContractBoard::DayReport rep;
        for (int d = m_day - life; d <= m_day; ++d)
            rep = m_board.stepBoard(d);
        std::cerr << "[ContractBoard] fresh boards for day " << m_day << " (the last " << life << " days replayed): " << rep.open
                  << " open postings" << '\n';
    }
}

void EconomyServer::advanceTo(int day)
{
    if (!m_loaded)
        return;
    flushStateIfDue();   // each server tick: the throttled state save
    if (m_day < 0)
    {
        m_day = day;
        return;
    }
    if (day < m_day)
    {
        // a timeline rewind (DEV clock): the economy does not run backwards - re-anchor the day
        std::cerr << "[EconomyDay] clock went back " << (m_day - day) << " days - re-anchored at day " << day << '\n';
        m_day = day;
        for (auto& [id, l] : m_ledgers)
            l.lastDay = std::min(l.lastDay, day);
        return;
    }
    if (day - m_day > 400)
    {
        std::cerr << "[EconomyDay] clock jumped " << (day - m_day) << " days - stepping only the last day" << '\n';
        m_day = day - 1;
    }
    for (int steps = 0; m_day < day && steps < 3; ++steps)
        stepDay(++m_day);
}

void EconomyServer::stepDay(int day)
{
    const auto& model = EconomyModel::instance();
    const auto& goods = model.goods();

    // exchange rates
    double fxMax = 0.0;
    std::string fxMaxCode;
    for (const auto& b : m_banks.banks())
    {
        const double before = m_banks.fx(b.id);
        const double after = stepFx(b, before, gaussian(hashOf(b.id, static_cast<std::uint64_t>(day) * 7919ull)));
        m_banks.setFx(b.id, after);
        if (std::fabs(after / before - 1.0) > std::fabs(fxMax))
        {
            fxMax = after / before - 1.0;
            fxMaxCode = b.code;
        }
    }

    // markets
    const int jc = goodIndex("jump_charge");
    double sumAbs = 0.0, maxMove = 0.0;
    std::size_t cells = 0, eventCount = 0;
    std::string maxWhere;
    std::vector<double> jumpPrices;
    for (std::size_t mi = 0; mi < m_markets.size(); ++mi)
    {
        auto& m = m_markets[mi];
        const bool corridor = marketRegimeOf(polityOf(m.hubId, m.systemId)) == "corridor";
        const auto events = model.events(m.hubId, day);
        eventCount += events.size();
        for (std::size_t g = 0; g < goods.size(); ++g)
        {
            auto& c = m.cells[g];
            if (!c.traded)
                continue;
            CellDrivers d;
            for (const auto& e : events)
            {
                const bool eventGood = hashOf(m.hubId, static_cast<std::uint64_t>(e.startDay) + 0x5eedull) % goods.size() == g;
                eventDrivers(e.type, goods[g].group, goods[g].id, eventGood, d);
            }
            const std::uint64_t key = hashOf(m.hubId, hashOf(goods[g].id, 0));
            d.supplyShock = signedUnit(key ^ mix(static_cast<std::uint64_t>(day) * 2 + 1));
            d.demandShock = signedUnit(key ^ mix(static_cast<std::uint64_t>(day) * 2 + 2));
            d.slowDemand = dayNoise(key, day, m_priceRules.slowDemandPeriodDays);
            d.shelfDays = goods[g].shelfDays;
            double move = stepCell(c, d, m_priceRules);
            if (corridor && c.anchorCr > 0.0 &&
                std::find(m_regimeRules.corridorGroups.begin(), m_regimeRules.corridorGroups.end(), goods[g].group) !=
                    m_regimeRules.corridorGroups.end())
            {
                // a managed corridor (§2.5): the basket's price stays in anchor x [min, max]; a shortage becomes rationing
                const double before = c.priceCr / (1.0 + move);
                c.priceCr = std::clamp(c.priceCr, c.anchorCr * m_regimeRules.corridorMin, c.anchorCr * m_regimeRules.corridorMax);
                move = before > 0.0 ? c.priceCr / before - 1.0 : move;
            }
            sumAbs += std::fabs(move);
            ++cells;
            if (std::fabs(move) > std::fabs(maxMove))
            {
                maxMove = move;
                maxWhere = goods[g].id + " @ " + m.hubId;
            }
            if (static_cast<int>(g) == jc)
                jumpPrices.push_back(c.priceCr * (1.0 + 0.5 * m.spread));
        }
    }

    // NPC speculators between the hubs of a system (TRADE_DESIGN §2.6, step 10)
    stepNpcFlows(day);

    // input VAT refunds that came due (trade, TRADE_DESIGN §5.2)
    payDueVatRefunds(day);

    // the contract boards (§7.3 items 5-6): expiries, NPC haulers, today's publications; then the contracts' deadlines
    const auto board = m_board.stepBoard(day);
    stepContracts(day);
    std::size_t activeContracts = 0;
    for (const auto& c : m_board.contracts())
        activeContracts += c.status == "active";
    std::cerr << "[ContractBoard] day " << day << ": published " << board.published << ", taken by NPC " << board.npcTaken << ", expired "
              << board.expired << ", open " << board.open << "; player contracts active " << activeContracts << '\n';
    ++m_tradeSerial;

    // ledgers
    for (auto& [id, l] : m_ledgers)
    {
        const Standing before = l.standing;
        const auto rep = stepLedgerDay(l, m_banks, day);
        if (rep.standingChanged || rep.restructured || day % 30 == 0)
            std::cerr << "[EconomyDay] player " << id << ": net " << grouped(netWorthCr(l, m_banks)) << " Cr, debt "
                      << grouped(totalDebtCr(l, m_banks)) << " Cr, standing " << standingName(before) << " -> "
                      << standingName(l.standing) << ", interest +" << grouped(rep.interestEarnedCr, 2) << " / -"
                      << grouped(rep.interestChargedCr, 2) << " Cr" << '\n';
    }

    stepTaxes(day);
    payWages(day);
    stepBots(day);
    stepConstruction(day);
    stepMines(day);

    std::sort(jumpPrices.begin(), jumpPrices.end());
    char line[512];
    std::snprintf(line, sizeof line,
                  "[EconomyDay] day %d: %zu markets / %zu cells, mean |dp| %.2f %%, max %+.1f %% (%s), events %zu; "
                  "J charge list median %s Cr/t [%s .. %s]%s; fx max %+.2f %% (%s); ledgers %zu",
                  day, m_markets.size(), cells, cells ? 100.0 * sumAbs / static_cast<double>(cells) : 0.0, 100.0 * maxMove,
                  maxWhere.c_str(), eventCount, jumpPrices.empty() ? "-" : grouped(jumpPrices[jumpPrices.size() / 2]).c_str(),
                  jumpPrices.empty() ? "-" : grouped(jumpPrices.front()).c_str(), jumpPrices.empty() ? "-" : grouped(jumpPrices.back()).c_str(),
                  m_fuelFree ? " (charged 0: fuel free)" : "", 100.0 * fxMax, fxMaxCode.c_str(), m_ledgers.size());
    std::cerr << line << '\n';

    saveNow("the daily step", false);
}

// ---------------------------------------------------------------- players
// ---------------------------------------------------------------- NPC flows (TRADE_DESIGN §2.6, step 10)
// Not ships: flows. A speculator at hub n sees hub h's price from a bulletin a few days old; when h's bid beats n's ask
// plus the haul by more than the minimum margin, it buys at n now and the goods land at h after the trip. Everybody
// sees the same old bulletin, so they all ship at once into a market that has already turned - the cobweb of §2.4,
// damped by the restock loop and the dumping. The stock moves; nobody pays (no AI wallets, ECONOMY_BANKING §10).
void EconomyServer::stepNpcFlows(int day)
{
    const auto t0 = std::chrono::steady_clock::now();
    const auto& goods = EconomyModel::instance().goods();
    // arrivals (one pass, no erase in the middle)
    std::size_t keep = 0;
    for (std::size_t k = 0; k < m_npcShipments.size(); ++k)
    {
        const auto& sh = m_npcShipments[k];
        if (sh.arriveDay <= day)
            m_markets[sh.market].cells[static_cast<std::size_t>(sh.good)].stockT += sh.tonnes;
        else
            m_npcShipments[keep++] = sh;
    }
    m_npcShipments.resize(keep);
    // today's bulletins
    if (m_npcRingFirstDay < 0 || day < m_npcRingFirstDay)
        m_npcRingFirstDay = day;
    for (auto& m : m_markets)
    {
        if (m.priceRing.size() != m.cells.size())
            m.priceRing.assign(m.cells.size(), std::array<float, 8>{});
        for (std::size_t g = 0; g < m.cells.size(); ++g)
            m.priceRing[g][static_cast<std::size_t>(day) % 8] = static_cast<float>(m.cells[g].priceCr);
    }
    if (m_systemMarkets.empty())
        for (std::size_t i = 0; i < m_markets.size(); ++i)
            m_systemMarkets[m_markets[i].systemId].push_back(i);
    // per market once: which goods the state holds (planned / mobilization) - those do not move by speculation
    if (m_npcStateGood.size() != m_markets.size())
    {
        m_npcStateGood.assign(m_markets.size(), std::vector<char>(goods.size(), 0));
        for (std::size_t i = 0; i < m_markets.size(); ++i)
        {
            const std::string regime = marketRegimeOf(polityOf(m_markets[i].hubId, m_markets[i].systemId));
            for (std::size_t g = 0; g < goods.size(); ++g)
                m_npcStateGood[i][g] = regime == "planned" ||
                                       (regime == "mobilization" && std::find(m_regimeRules.mobilizationGroups.begin(),
                                                                              m_regimeRules.mobilizationGroups.end(), goods[g].group) !=
                                                                        m_regimeRules.mobilizationGroups.end());
        }
    }
    const auto& R = m_npcRules;
    std::size_t shipped = 0;
    double tonnes = 0.0;
    for (const auto& [system, list] : m_systemMarkets)
    {
        if (list.size() < 2)
            continue;
        for (const std::size_t ni : list)
            for (const std::size_t hi : list)
            {
                if (ni == hi)
                    continue;
                auto& n = m_markets[ni];
                const auto& h = m_markets[hi];
                const std::uint64_t pair = mix((static_cast<std::uint64_t>(ni) << 32) ^ hi ^ 0x4e9cull);
                const int lag = R.lagMin + static_cast<int>(pair % static_cast<std::uint64_t>(R.lagMax - R.lagMin + 1));
                if (day - lag < m_npcRingFirstDay)
                    continue;   // no bulletin that old yet
                const std::size_t slot = static_cast<std::size_t>(day - lag) % 8;
                const auto& stateN = m_npcStateGood[ni];
                const auto& stateH = m_npcStateGood[hi];
                for (std::size_t g = 0; g < n.cells.size(); ++g)
                {
                    auto& cn = n.cells[g];
                    const auto& ch = h.cells[g];
                    if (!cn.traded || !ch.traded || cn.priceCr <= 0.0 || stateN[g] || stateH[g])
                        continue;
                    const double bidH = h.priceRing[g][slot] * (1.0 - 0.5 * h.spread);
                    const double askN = cn.priceCr * (1.0 + 0.5 * n.spread);
                    const double excess = (bidH - askN - R.deliveryCrT) / askN - R.minMargin;
                    if (excess <= 0.0)
                        continue;
                    const double available = std::max(0.0, cn.stockT - 0.5 * cn.targetT);
                    const double q = std::min({R.k * ch.flowTDay * excess, R.maxShareOfFlow * ch.flowTDay, available});
                    if (!(q > 1.0e-3))
                        continue;
                    cn.stockT -= q;
                    const int travel = R.travelMin + static_cast<int>((pair >> 8) % static_cast<std::uint64_t>(R.travelMax - R.travelMin + 1));
                    m_npcShipments.push_back({hi, static_cast<int>(g), q, day + travel});
                    ++shipped;
                    tonnes += q;
                }
            }
    }
    m_npcTonnesShipped += tonnes;
    if (day % 10 == 0)
        std::cerr << "[NpcFlows] day " << day << ": " << shipped << " shipments today (" << grouped(tonnes) << " t), "
                  << m_npcShipments.size() << " in flight, step " << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count()
                  << " ms" << '\n';
}

// ---------------------------------------------------------------- construction (TRADE_DESIGN §6.2, step 16)
namespace
{
const char* buildKindRu(const std::string& kind)
{
    return kind == "minehead" ? "добычная установка" : kind == "hall" ? "цех (зал)" : kind == "yard" ? "верфь-двор" : kind == "slab" ? "жилой корпус" : kind == "hab" ? "жилой модуль"
         : kind == "tower" ? "башня" : kind == "dock" ? "причал" : "модуль";
}
} // namespace

std::vector<EconomyServer::BuildOffer> EconomyServer::buildOffersAt(const std::string& hubId, std::uint64_t playerId) const
{
    std::vector<BuildOffer> out;
    std::vector<PlannedProject> targets;
    if (const auto it = m_buildable.find(hubId); it != m_buildable.end())
        targets.push_back(it->second);
    // step 16 part 2: the rig of the player's leased site here
    for (const auto& c : m_concessions)
        if (playerId != 0 && c.owner == playerId && c.site.hubId == hubId && c.status == "leased")
            targets.push_back({"site:" + c.site.id, "minehead"});
    static const char* const Firms[] = {"«Орбиталь-Строй»", "«Кольцо и Ферма»", "«Тяжмонтаж»", "«Хабитат-Сервис»", "«Звёздный подряд»", "«Стапель»"};
    const int week = std::max(0, m_day) / 7;
    for (const auto& target : targets)
    {
        bool taken = false;
        for (const auto& p : m_buildProjects)
            taken = taken || (p.offer.projectId == target.projectId && p.status != "abandoned");
        if (taken)
            continue;
        const bool rig = target.projectId.rfind("site:", 0) == 0;
        const std::string tag = rig ? "#rig" : "#build";
        const double kf = m_buildKindFactor.count(target.kind) ? m_buildKindFactor.at(target.kind) : 1.5;
        for (int k = 0; k < 3; ++k)
        {
            const std::uint64_t h = hashOf(hubId + tag + std::to_string(k) + (rig ? target.projectId : std::string()),
                                           static_cast<std::uint64_t>(week) + 0xb11dull);
            BuildOffer o;
            o.id = hubId + tag + std::to_string(k) + "@" + std::to_string(week) + (rig ? ":" + target.projectId.substr(5) : std::string());
            o.hubId = hubId;
            o.projectId = target.projectId;
            o.kind = target.kind;
            o.contractorRu = Firms[(hashOf(hubId + "#firms", static_cast<std::uint64_t>(week)) % 6 + 2 * static_cast<std::uint64_t>(k)) % 6];   // 3 different firms
            o.priceCr = std::round(m_buildBaseCostCr * kf * (1.15 + 0.15 * unit(h ^ 1)) / 1000.0) * 1000.0;
            o.workDays = std::max(1, static_cast<int>(std::lround(m_buildBaseDays * (0.9 + 0.5 * unit(h ^ 2)))));
            o.reliability = 0.6 + 0.35 * unit(h ^ 3);
            out.push_back(o);
        }
    }
    return out;
}

double EconomyServer::buildProgress(const BuildProject& p, int day) const
{
    int worked = day - p.startDay;
    for (const auto& [from, to] : p.halts)
        if (day > from)
            worked -= (to < 0 ? day : std::min(day, to)) - from;
    return std::clamp(static_cast<double>(worked) / std::max(1, p.offer.workDays), 0.0, 1.0);
}

game::simulation::ClientTradeResult EconomyServer::signConstruction(std::uint64_t playerId, const std::string& offerId, const std::string& hubId,
                                                                    int systemId, std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    ClientLedger* l = ledger(playerId);
    if (!l)
        return refuse("no_account", "нет счёта");
    const BuildOffer* o = nullptr;
    const auto offers = buildOffersAt(hubId, playerId);
    for (const auto& x : offers)
        if (x.id == offerId)
            o = &x;
    if (!o)
        return refuse("build_unknown", "предложение подрядчика уже не действует");
    // the first milestone (s1, 15 %) at signing; the milestones sum to 95 %, the last 5 % (the retention) at the acceptance
    const double payNow = std::round(o->priceCr * m_buildMilestones[0] * 100.0) / 100.0;
    const PaymentResult p = pay(*l, m_banks, bankOf(hubId, systemId), payNow, Purpose::Other, m_day, false,
                                "стройка: первая веха (" + std::string(buildKindRu(o->kind)) + ", " + o->contractorRu + ")");
    if (!p.ok)
        return refuse("no_money", "не хватает денег на первую веху: " + grouped(payNow) + " Cr");
    BuildProject bp;
    bp.offer = *o;
    bp.owner = playerId;
    bp.startDay = m_day;
    bp.paidMilestones = 1;
    bp.paidCr = payNow;
    bp.retentionCr = 0.0;
    m_buildProjects.push_back(bp);
    ++m_buildRevision;
    logEvent(*l, m_day, "договор подряда: " + std::string(buildKindRu(o->kind)) + " у " + hubDisplayName(hubId) + ", " + o->contractorRu + ", " +
                            grouped(o->priceCr) + " Cr, " + std::to_string(o->workDays) + " сут");
    r.ok = true;
    r.code = "build_signed";
    r.totalCr = -payNow;
    r.message = "договор: " + std::string(buildKindRu(o->kind)) + " у " + hubDisplayName(hubId) + " за " + grouped(o->priceCr) + " Cr";
    pushResult(playerId, r);
    saveNow("construction signed");
    std::cerr << "[Build] player " << playerId << " signs " << o->id << " (" << o->projectId << ", " << o->kind << ") " << o->priceCr << " Cr, "
              << o->workDays << " days, r " << o->reliability << '\n';
    return r;
}

void EconomyServer::stepConstruction(int day)
{
    for (auto& p : m_buildProjects)
    {
        ClientLedger* l = ledger(p.owner);
        if (!l || p.status == "abandoned")
            continue;
        const std::string& hub = p.offer.hubId;
        const auto* info = EconomyModel::instance().hub(hub);
        const int system = info ? info->systemId : -1;
        if (p.status == "done" && p.offer.projectId.rfind("site:", 0) == 0)
            continue;   // a mine rig: stepMines runs it
        if (p.status == "done")
        {
            // the module is let to the hub operator: income, maintenance (taxable)
            const double net = p.offer.priceCr * (m_buildIncomeYear - m_buildMaintYear) / 360.0;
            receive(*l, m_banks, bankOf(hub, system), net, day, "доход модуля у " + hubDisplayName(hub));
            if (TaxBook* t = taxBook(p.owner))
                addRevenue(*t, net);
            continue;
        }
        const double prog = buildProgress(p, day);
        // the next milestone due by progress (s2 at 25 %, s3 at 50 %, s4 at 75 %)
        const int due = std::min(4, 1 + static_cast<int>(prog * 4.0 + 1e-9));
        bool funding = false;
        while (p.paidMilestones < due && p.paidMilestones < static_cast<int>(m_buildMilestones.size()))
        {
            const double amount = std::round(p.offer.priceCr * m_buildMilestones[static_cast<std::size_t>(p.paidMilestones)] * 100.0) / 100.0;
            const double payNow = amount;
            const PaymentResult r = pay(*l, m_banks, bankOf(hub, system), payNow, Purpose::Other, day, false,
                                        "стройка: веха s" + std::to_string(p.paidMilestones + 1));
            if (!r.ok)
            {
                funding = true;
                break;
            }
            p.paidCr += payNow;
            ++p.paidMilestones;
            ++m_buildRevision;
            logEvent(*l, day, "стройка у " + hubDisplayName(hub) + ": оплачена веха s" + std::to_string(p.paidMilestones) + " (" + grouped(payNow) + " Cr)");
        }
        const bool openHalt = !p.halts.empty() && p.halts.back().second < 0;
        if (funding && !openHalt)
        {
            p.halts.push_back({day, -1});   // halted: funding
            p.unpaidSince = day;
            p.status = "halted";
            ++m_buildRevision;
            logEvent(*l, day, "стройка у " + hubDisplayName(hub) + " остановлена: нет оплаты вехи");
        }
        else if (!funding && openHalt && p.unpaidSince >= 0)
        {
            p.halts.back().second = day;   // paid: the work resumes
            p.unpaidSince = -1;
            p.status = "building";
            ++m_buildRevision;
        }
        if (p.unpaidSince >= 0 && day - p.unpaidSince >= m_buildAbandonDays)
        {
            p.status = "abandoned";
            ++m_buildRevision;
            logEvent(*l, day, "подрядчик ушёл со стройки у " + hubDisplayName(hub) + ": 180 сут без оплаты");
            continue;
        }
        // a random halt (supply, accident, politics) x (1.5 - reliability), 1..5 days
        if (p.status == "building" && unit(hashOf(p.offer.id, static_cast<std::uint64_t>(day))) < m_buildHaltPDay * (1.5 - p.offer.reliability))
        {
            const int len = 1 + static_cast<int>(unit(hashOf(p.offer.id, static_cast<std::uint64_t>(day) + 7)) * 5.0);
            p.halts.push_back({day, day + len});
            ++m_buildRevision;
            logEvent(*l, day, "стройка у " + hubDisplayName(hub) + ": простой " + std::to_string(len) + " сут (снабжение / авария)");
        }
        if (buildProgress(p, day) >= 1.0 && p.paidMilestones >= static_cast<int>(m_buildMilestones.size()))
        {
            p.status = "done";
            ++m_buildRevision;
            // the retention (price - the milestones) at the acceptance (minimal set: no 180-day guarantee period)
            p.retentionCr = std::max(0.0, std::round((p.offer.priceCr - p.paidCr) * 100.0) / 100.0);
            if (!p.retentionPaid && p.retentionCr > 0.0)
            {
                const PaymentResult r = pay(*l, m_banks, bankOf(hub, system), p.retentionCr, Purpose::Other, day, false, "стройка: удержание при приёмке");
                p.retentionPaid = r.ok;
                if (!r.ok)
                    std::cerr << "[Build] " << p.offer.id << " retention " << p.retentionCr << " NOT paid: " << r.reason << '\n';
                if (r.ok)
                    p.paidCr += p.retentionCr;
            }
            logEvent(*l, day, "стройка у " + hubDisplayName(hub) + (p.offer.projectId.rfind("site:", 0) == 0 ? " завершена: установка готова к добыче"
                                                                                                            : " завершена: модуль сдан оператору хаба"));
            std::cerr << "[Build] " << p.offer.id << " DONE, paid " << p.paidCr << " Cr, retention " << p.retentionCr << (p.retentionPaid ? " paid" : " held") << '\n';
        }
    }
}

// ---------------------------------------------------------------- mining concessions (TRADE_DESIGN §6.1, step 16 part 2)
namespace
{
std::uint64_t mineStockKey(const std::string& siteId)
{
    return 0x6D00000000000000ull | (hashOf(siteId, 0x5170ull) & 0x00FFFFFFFFFFFFFFull);   // the mine's stockpile manifest
}
} // namespace

std::vector<EconomyServer::SiteOffer> EconomyServer::siteOffersAt(const std::string& hubId) const
{
    std::vector<SiteOffer> out;
    const auto* info = EconomyModel::instance().hub(hubId);
    if (!info || (info->typeId != "mining_hub" && info->typeId != "belt_hub") || (!m_routableHubs.empty() && !m_routableHubs.count(hubId)))   // a hub one can fly to
        return out;
    const int week = std::max(0, m_day) / 7;
    for (int k = 0; k < 3; ++k)
    {
        const std::uint64_t h = hashOf(hubId + "#site" + std::to_string(k), static_cast<std::uint64_t>(week) + 0x5173ull);
        SiteOffer o;
        o.id = hubId + "#site" + std::to_string(k) + "@" + std::to_string(week);
        bool leased = false;
        for (const auto& c : m_concessions)
            leased = leased || (c.site.id == o.id && c.status != "lapsed");
        if (leased)
            continue;
        o.hubId = hubId;
        o.nameRu = "астероид " + std::to_string(1000 + (h >> 8) % 9000);
        o.goodId = unit(h ^ 5) < 0.2 ? "ore_rare" : "ore_common";
        const double u = unit(h ^ 6);
        o.radiusM = std::round(50.0 + 1950.0 * u * u);
        o.grade = std::round((0.5 + 2.5 * unit(h ^ 7)) * 100.0) / 100.0;
        o.reserveT = std::round(4.0 / 3.0 * 3.14159265358979 * o.radiusM * o.radiusM * o.radiusM * 2.4 * m_mineRecoverable);
        o.rentYearCr = std::round(m_mineRentBaseCr + m_mineRentPerRadiusCr * o.radiusM * o.grade);
        o.leaseCostCr = m_mineApplicationCr + m_mineSurveyCr;
        out.push_back(o);
    }
    return out;
}

game::simulation::ClientTradeResult EconomyServer::leaseSite(std::uint64_t playerId, const std::string& offerId, const std::string& hubId, int systemId,
                                                             std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    ClientLedger* l = ledger(playerId);
    if (!l)
        return refuse("no_account", "нет счёта");
    const auto offers = siteOffersAt(hubId);
    const SiteOffer* o = nullptr;
    for (const auto& x : offers)
        if (x.id == offerId)
            o = &x;
    if (!o)
        return refuse("site_unknown", "участок уже не предлагается");
    const PaymentResult p = pay(*l, m_banks, bankOf(hubId, systemId), o->leaseCostCr, Purpose::Fee, m_day, false,
                                "концессия: заявка и разведка (" + o->nameRu + ")");
    if (!p.ok)
        return refuse("no_money", "не хватает денег на заявку и разведку: " + grouped(o->leaseCostCr) + " Cr");
    if (TaxBook* t = taxBook(playerId))
        addExpense(*t, o->leaseCostCr);
    Concession c;
    c.site = *o;
    c.owner = playerId;
    c.startDay = m_day;
    c.reserveLeftT = o->reserveT;
    m_concessions.push_back(c);
    ++m_buildRevision;
    logEvent(*l, m_day, "концессия: " + o->nameRu + " у " + hubDisplayName(hubId) + ", радиус " + grouped(o->radiusM) + " м, запас " + grouped(o->reserveT) +
                            " т, аренда " + grouped(o->rentYearCr) + " Cr/год; дальше - договор на добычную установку (Стройка)");
    r.ok = true;
    r.code = "site_leased";
    r.totalCr = -o->leaseCostCr;
    r.message = "концессия: " + o->nameRu + " у " + hubDisplayName(hubId);
    pushResult(playerId, r);
    saveNow("concession");
    std::cerr << "[Mine] player " << playerId << " leases " << o->id << " (" << o->nameRu << ", r " << o->radiusM << " m, grade " << o->grade << ", "
              << o->goodId << ", reserve " << o->reserveT << " t, rent " << o->rentYearCr << " Cr/yr)" << '\n';
    return r;
}

void EconomyServer::setMineSale(std::uint64_t playerId, const std::string& siteId, bool sellLocal)
{
    for (auto& c : m_concessions)
        if (c.owner == playerId && c.site.id == siteId && c.sellLocal != sellLocal)
        {
            c.sellLocal = sellLocal;
            ++m_buildRevision;
            saveNow("mine sale");
            std::cerr << "[Mine] " << siteId << ": " << (sellLocal ? "sold at the hub daily" : "kept in the stockpile") << '\n';
        }
}

double EconomyServer::mineStockT(const std::string& siteId, const std::string& goodId) const
{
    const CargoManifest* m = m_cargo.find(mineStockKey(siteId));
    return m ? m->ownTonnesOf(goodId) : 0.0;
}

game::simulation::ClientTradeResult EconomyServer::loadFromMine(std::uint64_t playerId, const std::string& siteId, std::uint64_t shipKey,
                                                                const CargoCapacity& capacity, const std::string& hubId, double tonnes,
                                                                std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    const Concession* c = nullptr;
    for (const auto& x : m_concessions)
        if (x.owner == playerId && x.site.id == siteId)
            c = &x;
    if (!c)
        return refuse("mine_unknown", "это не ваш рудник");
    if (c->site.hubId != hubId)
        return refuse("mine_far", "склад рудника у " + hubDisplayName(c->site.hubId) + ": подлетите туда");
    const std::uint64_t key = mineStockKey(siteId);
    const double stock = mineStockT(siteId, c->site.goodId);
    const CargoManifest* aboard = m_cargo.find(shipKey);
    const double room = std::max(0.0, capacity.netT - (aboard ? aboard->netT() : 0.0));
    const double t = std::floor(std::min({tonnes > 0.0 ? tonnes : stock, stock, room}) * 100.0) / 100.0;
    if (t < 0.01)
        return refuse(stock < 0.01 ? "mine_empty" : "no_room", stock < 0.01 ? "на складе рудника пусто" : "трюм полон");
    const int g = goodIndex(c->site.goodId);
    const double density = g >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(g)].densityTM3 : 2.4;
    double basis = 0.0;
    if (!m_cargo.unload(key, c->site.goodId, t, density, basis))
        return refuse("mine_empty", "на складе рудника пусто");
    CargoLot lot;
    lot.lotId = siteId + "#haul@" + std::to_string(m_day);
    lot.goodId = c->site.goodId;
    lot.tonnes = t;
    lot.owner = "player:" + std::to_string(playerId);
    lot.costBasisCr = basis;
    lot.originHub = hubId;
    lot.loadedDay = m_day;
    (void)m_cargo.load(shipKey, playerId, lot, density);
    r.ok = true;
    r.code = "mine_loaded";
    r.message = "погружено со склада рудника: " + grouped(t, 2) + " т";
    pushResult(playerId, r);
    saveNow("mine haul");
    std::cerr << "[Mine] player " << playerId << " loads " << t << " t of " << c->site.goodId << " from " << siteId << " into " << shipKey << '\n';
    return r;
}

void EconomyServer::stepMines(int day)
{
    for (auto& c : m_concessions)
    {
        if (c.status == "lapsed" || c.status == "depleted")
            continue;
        ClientLedger* l = ledger(c.owner);
        if (!l)
            continue;
        const std::string& hub = c.site.hubId;
        const auto* info = EconomyModel::instance().hub(hub);
        const int system = info ? info->systemId : -1;
        const std::string bank = bankOf(hub, system);
        // the rent (a daily share of the year); unpaid for 90 days -> the state takes the site back
        const double rent = std::round(c.site.rentYearCr / 360.0 * 100.0) / 100.0;
        if (pay(*l, m_banks, bank, rent, Purpose::Fee, day, false, "аренда участка: " + c.site.nameRu).ok)
        {
            c.unpaidDays = 0;
            if (TaxBook* t = taxBook(c.owner))
                addExpense(*t, rent);
        }
        else if (++c.unpaidDays >= m_mineLapseDays)
        {
            c.status = "lapsed";
            ++m_buildRevision;
            logEvent(*l, day, "концессия " + c.site.nameRu + " отозвана: " + std::to_string(m_mineLapseDays) + " сут без арендной платы");
            continue;
        }
        if (c.status == "leased")
        {
            for (const auto& p : m_buildProjects)
                if (p.offer.projectId == "site:" + c.site.id && p.status == "done")
                {
                    c.status = "mining";
                    c.rigPriceCr = p.offer.priceCr;
                    ++m_buildRevision;
                    logEvent(*l, day, "рудник " + c.site.nameRu + ": установка принята, добыча начата");
                }
            if (c.status != "mining")
                continue;
        }
        // the shift's wages and the rig's upkeep; unpaid -> no output today
        const double wage = std::round(m_mineStaff * m_mineWageMonthCr / 30.0 * 100.0) / 100.0;
        const double upkeep = std::round(c.rigPriceCr * m_mineMaintYear / 360.0 * 100.0) / 100.0;
        if (!pay(*l, m_banks, bank, wage + upkeep, Purpose::Fee, day, false, "рудник " + c.site.nameRu + ": смена и обслуживание").ok)
        {
            c.lastOutputT = 0.0;
            continue;
        }
        if (TaxBook* t = taxBook(c.owner))
            addExpense(*t, wage + upkeep);
        // the output: the grade, then the tail of the reserve
        const double depletion = std::clamp(c.reserveLeftT / std::max(1.0, 0.05 * c.site.reserveT), 0.0, 1.0);
        const double t = std::min(c.reserveLeftT, m_mineBaseTDay * (0.5 + c.site.grade / 3.0) * depletion);
        c.reserveLeftT -= t;
        c.minedT += t;
        c.lastOutputT = t;
        if (c.reserveLeftT < 1.0)
        {
            c.status = "depleted";
            ++m_buildRevision;
            logEvent(*l, day, "рудник " + c.site.nameRu + " выработан");
        }
        // the stockpile -> the hub's market (the ordinary deal; the price falls with the stock), the royalty to the state
        const std::uint64_t key = mineStockKey(c.site.id);
        const int g = goodIndex(c.site.goodId);
        const double density = g >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(g)].densityTM3 : 2.4;
        CargoLot lot;
        lot.lotId = c.site.id + "@" + std::to_string(day);
        lot.goodId = c.site.goodId;
        lot.tonnes = t;
        lot.owner = "player:" + std::to_string(c.owner);
        lot.originHub = hub;
        lot.loadedDay = day;
        if (t > 0.01)
            (void)m_cargo.load(key, c.owner, lot, density);
        if (!c.sellLocal)
            continue;   // kept for the owner to haul
        const CargoManifest* m = m_cargo.find(key);
        TradeOrder o;
        o.buy = false;
        o.playerId = c.owner;
        o.shipKey = key;
        o.hubId = hub;
        o.systemId = system;
        o.goodId = c.site.goodId;
        o.tonnes = m ? m->ownTonnesOf(c.site.goodId) : 0.0;
        o.capacity = {1.0e9, 0};
        if (o.tonnes < 0.01)
            continue;
        const auto res = trade(o);
        if (!res.ok)
            continue;   // no buyer today: the ore waits in the stockpile
        const double royalty = std::round(res.totalCr * m_mineRoyalty * 100.0) / 100.0;
        (void)pay(*l, m_banks, bank, royalty, Purpose::Tax, day, false, "роялти: " + c.site.nameRu);
        c.soldT += o.tonnes;
        c.revenueCr += res.totalCr - royalty;
    }
}

// ---------------------------------------------------------------- bot pilots (TRADE_DESIGN §6.4, step 15)
namespace
{
// a beta-like trait: most are middling, a tail of idiots and a few aces
double traitOf(std::uint64_t h)
{
    const double a = unit(h), b = unit(h ^ 0x9e3779b97f4a7c15ull);
    return std::clamp(0.5 * (a + b) * 1.1 - 0.05, 0.0, 1.0);
}
} // namespace

std::vector<EconomyServer::BotCandidate> EconomyServer::botCandidatesAt(const std::string& hubId, int systemId) const
{
    std::vector<BotCandidate> out;
    if (hubId.empty() || m_marketIndex.find(hubId) == m_marketIndex.end())
        return out;
    (void)systemId;
    static const char* const First[] = {"Ари", "Бо", "Вик", "Гао", "Дара", "Ено", "Жан", "Зоя", "Иво", "Кай", "Лин", "Мира", "Нур", "Ода", "Пэй", "Рин"};
    static const char* const Last[] = {"Ковач", "Сато", "Мендес", "Олсен", "Рао", "Дюбуа", "Ким", "Новак", "Фаррелл", "Ибрагим", "Хольм", "Чен"};
    const int week = std::max(0, m_day) / 7;
    for (int k = 0; k < m_botCandidatesPerHub; ++k)
    {
        const std::uint64_t h = hashOf(hubId + "#bot" + std::to_string(k), static_cast<std::uint64_t>(week) + 0xb07ull);
        BotCandidate c;
        c.id = hubId + "#bot" + std::to_string(k) + "@" + std::to_string(week);
        if (m_hiredCandidates.count(c.id))
            continue;
        c.hubId = hubId;
        c.name = std::string(First[h % 16]) + " " + Last[(h >> 8) % 12];
        c.traits.skill = traitOf(h ^ 1);
        c.traits.discipline = traitOf(h ^ 2);
        c.traits.punctuality = traitOf(h ^ 3);
        c.traits.thrift = traitOf(h ^ 4);
        c.traits.honesty = traitOf(h ^ 5);
        c.traits.nerve = traitOf(h ^ 6);
        // the CV: the skill embellished by a dishonest candidate (TRADE_DESIGN §6.4: "резюме может быть приукрашено")
        c.skillShown = std::clamp(c.traits.skill + (1.0 - c.traits.honesty) * 0.35 * unit(h ^ 7), 0.0, 1.0);
        c.yearsShown = 1 + static_cast<int>(c.skillShown * 14.0);
        c.wageCr = std::round(m_botBaseWageCr * (0.6 + 0.8 * c.traits.skill));
        out.push_back(c);
    }
    return out;
}

void EconomyServer::botReport(Bot& b, int day, const std::string& text)
{
    b.reports.push_back({day + m_botReportDelayDays, text});
    if (b.reports.size() > 12)
        b.reports.erase(b.reports.begin());
}

game::simulation::ClientTradeResult EconomyServer::hireBot(std::uint64_t playerId, const std::string& candidateId, const std::string& hubId,
                                                           int systemId, std::uint64_t shipKey, const std::string& shipRu,
                                                           const CargoCapacity& capacity, double dryMassT, std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    if (!ledger(playerId))
        return refuse("no_account", "нет счёта");
    const BotCandidate* c = nullptr;
    const auto list = botCandidatesAt(hubId, systemId);
    for (const auto& x : list)
        if (x.id == candidateId)
            c = &x;
    if (!c)
        return refuse("bot_unknown", "кандидат уже не в списке агентства");
    if (shipKey == 0 || !(capacity.netT > 0.0))
        return refuse("bot_no_ship", "нужен свой корабль с трюмом, которым вы сейчас не управляете");
    for (const auto& b : m_bots)
        if (b.shipKey == shipKey && b.state != "quit")
            return refuse("bot_ship_busy", "на этом корабле уже есть пилот-бот");
    Bot b;
    b.id = "bot:" + c->id;
    b.name = c->name;
    b.traits = c->traits;
    b.skillShown = c->skillShown;
    b.wageCr = c->wageCr;
    b.owner = playerId;
    b.shipKey = shipKey;
    b.shipRu = shipRu;
    b.capacity = capacity;
    b.dryMassT = dryMassT;
    b.systemId = systemId;
    b.hubA = hubId;
    m_hiredCandidates.insert(c->id);
    botReport(b, m_day - m_botReportDelayDays, "нанят(а) на корабль " + shipRu + ", зарплата " + grouped(b.wageCr) + " Cr / 30 сут");
    m_bots.push_back(b);
    r.ok = true;
    r.code = "bot_hired";
    r.message = "нанят пилот " + b.name + " (" + shipRu + ")";
    pushResult(playerId, r);
    saveNow("bot hired");
    std::cerr << "[Bots] player " << playerId << " hires " << b.id << " " << b.name << " (skill " << b.traits.skill << ", shown "
              << b.skillShown << ", honesty " << b.traits.honesty << ") on ship " << shipKey << '\n';
    return r;
}

game::simulation::ClientTradeResult EconomyServer::orderBotLoop(std::uint64_t playerId, const std::string& botId, const std::string& hubId,
                                                                int systemId, std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    Bot* bot = nullptr;
    for (auto& b : m_bots)
        if (b.id == botId && b.owner == playerId && b.state != "quit")
            bot = &b;
    if (!bot)
        return refuse("bot_unknown", "нет такого пилота");
    const std::string from = bot->state == "idle" && !hubId.empty() ? hubId : bot->hubA;
    const auto itA = m_marketIndex.find(from);
    if (itA == m_marketIndex.end())
        return refuse("bot_no_market", "у хаба корабля бота нет рынка");
    // the best margin: buy at A (ask), sell at B (bid), the same system
    std::string bestHub, bestGood;
    double bestMargin = 0.0;
    const auto& goods = EconomyModel::instance().goods();
    for (const auto& m : m_markets)
    {
        if (m.hubId == from || m.systemId != systemId)
            continue;
        for (std::size_t g = 0; g < goods.size(); ++g)
        {
            MarketSide a, b;
            if (!marketSide(from, goods[g].id, a) || !marketSide(m.hubId, goods[g].id, b))
                continue;
            if (legalityFor(playerId, static_cast<int>(g), from, systemId) != "legal" || legalityFor(playerId, static_cast<int>(g), m.hubId, systemId) != "legal")
                continue;
            // the NET margin of a 20 t trip priced like the server will charge it: handling, VAT (minus what comes back),
            // the import duty and the turnover tax, plus the two dockings of the round trip (step-15 balance note: the list
            // margin picked loops the fees ate)
            const double q = 20.0;
            const DealQuote buyQ = explainDeal(true, a, q, fiscalStyleOf(from, systemId), m_dealRules, 0.0);
            const DealQuote sellQ = explainDeal(false, b, q, fiscalStyleOf(m.hubId, systemId), m_dealRules, importDutyRate(from, m.hubId, systemId));
            if (!buyQ.ok || !sellQ.ok)
                continue;
            const double cost = -buyQ.totalCr - buyQ.vatReclaimCr * (1.0 - m_dealRules.vatRefundFeeShare) +
                                2.0 * dockingFeeCr(260.0, fiscalStyleOf(from, systemId), m_dealRules);
            const double margin = (sellQ.totalCr - cost) / std::max(1.0, cost);
            if (margin > bestMargin)
            {
                bestMargin = margin;
                bestHub = m.hubId;
                bestGood = goods[g].id;
            }
        }
    }
    if (bestHub.empty())
        return refuse("bot_no_loop", "в системе нет выгодной петли от этого хаба");
    bot->hubA = from;
    bot->hubB = bestHub;
    bot->goodId = bestGood;
    bot->systemId = systemId;
    bot->state = "at_a";
    botReport(*bot, m_day - m_botReportDelayDays, "приказ: петля " + hubDisplayName(from) + " <-> " + hubDisplayName(bestHub) + ", товар " + bestGood +
                                                      " (маржа " + std::to_string(static_cast<int>(bestMargin * 100.0)) + " %)");
    r.ok = true;
    r.code = "bot_order";
    r.message = bot->name + ": петля " + hubDisplayName(from) + " <-> " + hubDisplayName(bestHub) + ", " + bestGood;
    pushResult(playerId, r);
    saveNow("bot order");
    std::cerr << "[Bots] " << bot->id << " loop " << from << " <-> " << bestHub << " " << bestGood << " margin " << bestMargin << '\n';
    return r;
}

void EconomyServer::fireBot(std::uint64_t playerId, const std::string& botId)
{
    for (auto& b : m_bots)
        if (b.id == botId && b.owner == playerId && b.state != "quit")
        {
            b.state = "quit";
            botReport(b, m_day - m_botReportDelayDays, "уволен владельцем");
            saveNow("bot fired");
        }
}

void EconomyServer::stepBots(int day)
{
    for (auto& b : m_bots)
    {
        if (b.state == "quit")
            continue;
        ClientLedger* l = ledger(b.owner);
        if (!l)
            continue;
        // the wage: daily; 30 days unpaid -> the bot leaves
        const PaymentResult paid = pay(*l, m_banks, l->homeBank, std::round(b.wageCr / 30.0 * 100.0) / 100.0, Purpose::Fee, day, false,
                                       "зарплата пилота " + b.name);
        b.unpaidDays = paid.ok ? 0 : b.unpaidDays + 1;
        if (paid.ok)
            if (TaxBook* t = taxBook(b.owner))
                addExpense(*t, b.wageCr / 30.0);
        if (b.unpaidDays >= 30)
        {
            b.state = "quit";
            botReport(b, day, "ушёл: 30 суток без зарплаты");
            continue;
        }
        const std::uint64_t h = hashOf(b.id, static_cast<std::uint64_t>(day) * 977ull);
        const auto& t = b.traits;
        const auto good = goodIndex(b.goodId);
        const double density = good >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(good)].densityTM3 : 1.0;
        if (b.state == "at_a" || b.state == "at_b")
        {
            const bool atA = b.state == "at_a";
            const std::string hub = atA ? b.hubA : b.hubB;
            (void)chargeDocking(b.owner, b.id + "@" + std::to_string(day) + (atA ? "a" : "b"), hub, b.systemId, b.dryMassT);
            TradeOrder o;
            o.playerId = b.owner;
            o.shipKey = b.shipKey;
            o.hubId = hub;
            o.systemId = b.systemId;
            o.goodId = b.goodId;
            o.capacity = b.capacity;
            o.requestId = 0;
            if (atA)
            {
                // buy what fits (half again if the money / the hub limit refuses)
                const CargoManifest* m = m_cargo.find(b.shipKey);
                o.buy = true;
                o.tonnes = std::max(0.0, b.capacity.netT - (m ? m->netT() : 0.0)) * 0.9;
                // what the owner's money pays for (ask + VAT + handling ~ x1.35), leaving a reserve for fees and wages
                if (MarketSide side; marketSide(hub, b.goodId, side))
                {
                    const double money = std::max(0.0, spendableCr(*l, m_banks, bankOf(hub, b.systemId)) - 2000.0);
                    o.tonnes = std::min(o.tonnes, money / std::max(1.0, askCr(side) * 1.35));
                }
                game::simulation::ClientTradeResult res;
                for (int k = 0; k < 4 && o.tonnes > 0.5; ++k, o.tonnes *= 0.5)
                {
                    res = trade(o);
                    if (res.ok)
                        break;
                }
                // the trip's cost = the lots' cost basis (the reclaimable input VAT excluded - it comes back later)
                b.tripCostCr = 0.0;
                if (const CargoManifest* after = m_cargo.find(b.shipKey))
                    for (const auto& lot : after->lots)
                        if (lot.goodId == b.goodId && lot.contractId.empty())
                            b.tripCostCr += lot.costBasisCr;
                botReport(b, day, res.ok ? "погрузка у " + hubDisplayName(hub) + ": " + res.message : "не смог купить у " + hubDisplayName(hub) + ": " + res.message);
            }
            else
            {
                // theft on the trip (honesty)
                if (unit(h ^ 0x7e) < 0.02 * (1.0 - t.honesty) * (1.0 - t.honesty))
                    if (const CargoManifest* m = m_cargo.find(b.shipKey))
                    {
                        const double aboard = m->ownTonnesOf(b.goodId);
                        const double stolen = aboard * (0.05 + 0.25 * unit(h ^ 0x7f));
                        double basis = 0.0;
                        if (stolen > 0.01)
                            (void)m_cargo.unload(b.shipKey, b.goodId, stolen, density, basis);
                        // the owner sees a shortfall, not a theft
                        botReport(b, day, "недостача груза: " + grouped(stolen, 2) + " т");
                    }
                const CargoManifest* m = m_cargo.find(b.shipKey);
                o.buy = false;
                o.tonnes = m ? m->ownTonnesOf(b.goodId) : 0.0;
                const auto res = o.tonnes > 0.01 ? trade(o) : game::simulation::ClientTradeResult{};
                if (res.ok)
                {
                    const double tripProfit = res.totalCr - b.tripCostCr;
                    b.profitCr += tripProfit;
                    ++b.trips;
                    botReport(b, day, "продажа у " + hubDisplayName(hub) + ": " + res.message + "; рейс " + (tripProfit >= 0 ? "+" : "") + grouped(tripProfit) + " Cr");
                }
                else
                    botReport(b, day, "не смог продать у " + hubDisplayName(hub) + (res.message.empty() ? std::string() : ": " + res.message));
            }
            // the leg: base days + a route error (skill) + a late arrival (punctuality)
            int legs = m_botLegDays;
            if (unit(h ^ 0x11) < 0.02 + 0.25 * (1.0 - t.skill) * (1.0 - t.skill))
            {
                legs += 1 + static_cast<int>(unit(h ^ 0x12) * 2.0);
                botReport(b, day, "ошиблся маршрутом: +" + std::to_string(legs - m_botLegDays) + " сут");
            }
            if (unit(h ^ 0x13) < 0.5 * (1.0 - t.punctuality))
                legs += 1;
            b.legDaysLeft = legs;
            b.state = atA ? "leg_to_b" : "leg_to_a";
            continue;
        }
        if (b.state == "leg_to_b" || b.state == "leg_to_a")
        {
            if (--b.legDaysLeft <= 0)
                b.state = b.state == "leg_to_b" ? "at_b" : "at_a";
        }
    }
}

// ---------------------------------------------------------------- rescue (TRADE_DESIGN §4.2, step 12)
std::vector<EconomyServer::SosCall> EconomyServer::sosCallsIn(int systemId) const
{
    std::vector<SosCall> out;
    if (systemId < 0 || m_day < 0)
        return out;
    for (const auto* h : EconomyModel::instance().hubsOfSystem(systemId))
    {
        if (!m_routableHubs.empty() && !m_routableHubs.count(h->id))
            continue;   // only a hub a route can reach (an economy-only plant / surface city has no world position)
        for (int d = m_day - m_sosOpenDays + 1; d <= m_day; ++d)
        {
            const std::uint64_t key = hashOf(h->id, static_cast<std::uint64_t>(d) * 31ull + 0x5050ull);
            if (unit(key) >= m_sosPerHubDay)
                continue;
            SosCall c;
            c.id = "sos:" + h->id + "@" + std::to_string(d);
            c.hubId = h->id;
            c.systemId = systemId;
            c.day = d;
            c.expiresDay = d + m_sosOpenDays;
            const double kind = unit(key ^ 0xabcdefull);
            c.kind = kind < 0.60 ? "capsule" : kind < 0.85 ? "derelict" : "medevac";
            c.kindRu = c.kind == "capsule" ? "спасательная капсула" : c.kind == "derelict" ? "корабль без хода, экипаж жив" : "медэвакуация";
            c.rewardCr = c.kind == "capsule" ? m_sosCapsuleCr : c.kind == "derelict" ? m_sosDerelictCr : m_sosMedevacCr;
            out.push_back(c);
        }
    }
    return out;
}

std::string EconomyServer::sosStatus(const SosCall& c) const
{
    if (const auto it = m_sosState.find(c.id); it != m_sosState.end())
        return it->second.second;
    return m_day >= c.expiresDay ? "expired" : "open";
}

game::simulation::ClientTradeResult EconomyServer::takeSos(std::uint64_t playerId, const std::string& sosId, std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    ClientLedger* l = ledger(playerId);
    if (!l)
        return refuse("no_account", "нет счёта");
    if (m_activeSos.count(playerId))
        return refuse("sos_busy", "у вас уже есть принятый вызов");
    // the call: "sos:<hub>@<day>"
    const auto at = sosId.rfind('@');
    if (sosId.rfind("sos:", 0) != 0 || at == std::string::npos)
        return refuse("sos_unknown", "нет такого вызова");
    const std::string hubId = sosId.substr(4, at - 4);
    const auto* hub = EconomyModel::instance().hub(hubId);
    if (!hub)
        return refuse("sos_unknown", "нет такого вызова");
    const SosCall* call = nullptr;
    const auto calls = sosCallsIn(hub->systemId);
    for (const auto& c : calls)
        if (c.id == sosId)
            call = &c;
    if (!call)
        return refuse("sos_unknown", "вызов уже закрыт");
    if (sosStatus(*call) != "open")
        return refuse("sos_closed", "вызов уже " + sosStatus(*call));
    if (!hasLicence(playerId, polityOf(hubId, hub->systemId), "sar"))
        return refuse("licence", "нужна лицензия МЧС этой политии (F5 -> Лицензии)");
    m_sosState[sosId] = {playerId, "taken"};
    m_activeSos[playerId] = sosId;
    logEvent(*l, m_day, "принят вызов SOS: " + call->kindRu + " у " + hubDisplayName(hubId));
    r.ok = true;
    r.code = "sos_taken";
    r.message = "вызов принят: " + call->kindRu + " у " + hubDisplayName(hubId) + " — летите туда";
    pushResult(playerId, r);
    saveNow("sos taken");
    std::cerr << "[Rescue] player " << playerId << " takes " << sosId << " (" << call->kind << ", " << call->rewardCr << " Cr)" << '\n';
    return r;
}

bool EconomyServer::completeSosAt(std::uint64_t playerId, const std::string& hubId, int systemId)
{
    const auto it = m_activeSos.find(playerId);
    if (it == m_activeSos.end() || hubId.empty())
        return false;
    const std::string id = it->second;
    if (id.rfind("sos:" + hubId + "@", 0) != 0)
        return false;
    ClientLedger* l = ledger(playerId);
    double reward = 0.0;
    std::string kindRu;
    for (const auto& c : sosCallsIn(systemId))
        if (c.id == id)
        {
            reward = c.rewardCr;
            kindRu = c.kindRu;
        }
    if (reward <= 0.0)   // the call's day left the window while flying: paid by its kind from the id's hash anyway
        reward = m_sosCapsuleCr;
    if (l)
    {
        const auto* bank = m_banks.bankForPolity(polityOf(hubId, systemId));
        receive(*l, m_banks, bank ? bank->id : l->homeBank, reward, m_day, "МЧС: " + (kindRu.empty() ? std::string("спасение") : kindRu) + " у " + hubDisplayName(hubId));
        if (TaxBook* t = taxBook(playerId))
            addRevenue(*t, reward);
    }
    m_sosState[id] = {playerId, "done"};
    m_activeSos.erase(it);
    game::simulation::ClientTradeResult r;
    r.ok = true;
    r.code = "sos_done";
    r.totalCr = reward;
    r.message = "спасение выполнено у " + hubDisplayName(hubId) + ": +" + grouped(reward) + " Cr";
    pushResult(playerId, r);
    saveNow("sos done");
    std::cerr << "[Rescue] player " << playerId << " completed " << id << ": +" << reward << " Cr" << '\n';
    return true;
}

// ---------------------------------------------------------------- the employment service (F5, TRADE_DESIGN §11b)
std::vector<EconomyServer::Vacancy> EconomyServer::vacanciesAt(const std::string& hubId, int systemId) const
{
    std::vector<Vacancy> out;
    if (hubId.empty())
        return out;
    const std::string polity = polityOf(hubId, systemId);
    const auto* bank = m_banks.bankForPolity(polity);
    const std::string where = hubDisplayName(hubId);
    const int week = std::max(0, m_day) / 7;
    const std::uint64_t seed = hashOf(hubId, static_cast<std::uint64_t>(week) + 0x10bull);
    static const char* const Companies[] = {"курьерская служба", "транспортная компания", "почтово-грузовая линия", "логистика"};
    for (int k = 0; k < 2 && !m_courierShips.empty(); ++k)
    {
        Vacancy v;
        v.id = hubId + "#courier" + std::to_string(k) + "@" + std::to_string(week);
        v.kind = "courier";
        v.employerRu = std::string(Companies[(seed >> (8 * k)) % 4]) + " «" + where + "»";
        v.roleRu = "пилот-курьер на зарплате";
        v.bankId = bank ? bank->id : std::string();
        v.passportId = m_courierShips[(seed >> (16 + 8 * k)) % m_courierShips.size()];
        v.requirementRu = "права: малый / средний грузовик — курьер";
        v.wageCr = m_courierWageCr;
        out.push_back(v);
    }
    if (bank && !m_militaryShips.empty())
    {
        Vacancy v;
        v.id = hubId + "#military@" + std::to_string(week);
        v.kind = "military";
        v.employerRu = "силы обороны: " + bank->polityName;
        v.roleRu = "пилот патрульного корабля";
        v.bankId = bank->id;
        v.passportId = m_militaryShips[(seed >> 40) % m_militaryShips.size()];
        v.unitRu = "патрульная эскадрилья «" + where + "»";
        v.rankRu = "пилот (рядовой)";
        v.requirementRu = "гражданство: " + bank->polityName;
        v.wageCr = m_militaryWageCr;
        v.termDays = m_militaryTermDays;
        out.push_back(v);
    }
    return out;
}

std::string EconomyServer::vacancyRefusal(std::uint64_t playerId, const Vacancy& v, const std::string& hubId, int systemId) const
{
    const ClientLedger* l = ledger(playerId);
    if (!l)
        return "нет учётной записи";
    if (m_employment.count(playerId))
        return "вы уже работаете — сначала увольтесь";
    if (v.kind == "courier")
    {
        bool category = false;
        for (const auto& c : m_pilotCategories)
            category = category || (c.id == "cargo_courier" && c.start);
        if (!category)
            return "нет категории прав «курьер»";
    }
    if (v.kind == "military")
    {
        const auto* home = m_banks.bank(l->homeBank);
        const auto* here = m_banks.bankForPolity(polityOf(hubId, systemId));
        if (!home || !here || home->id != here->id)
            return "служба только для граждан: " + (here ? here->polityName : std::string("?"));
    }
    return {};
}

void EconomyServer::startEmployment(std::uint64_t playerId, const Vacancy& v)
{
    m_employment[playerId] = {v, m_day};
    if (ClientLedger* l = ledger(playerId))
        logEvent(*l, m_day, "принят: " + v.roleRu + ", " + v.employerRu + ", " + grouped(v.wageCr) + " Cr / 30 сут");
    saveNow("employment");
}

void EconomyServer::endEmployment(std::uint64_t playerId, const std::string& why)
{
    const auto it = m_employment.find(playerId);
    if (it == m_employment.end())
        return;
    if (ClientLedger* l = ledger(playerId))
        logEvent(*l, m_day, "уволен: " + it->second.job.employerRu + " (" + why + ")");
    m_employment.erase(it);
    saveNow("employment");
}

const EconomyServer::Employment* EconomyServer::employment(std::uint64_t playerId) const
{
    const auto it = m_employment.find(playerId);
    return it == m_employment.end() ? nullptr : &it->second;
}

void EconomyServer::payWages(int day)
{
    for (const auto& [player, e] : m_employment)
        if (ClientLedger* l = ledger(player); l && day > e.sinceDay)
        {
            const double wage = std::round(e.job.wageCr / 30.0 * 100.0) / 100.0;
            receive(*l, m_banks, e.job.bankId.empty() ? l->homeBank : e.job.bankId, wage, day, "зарплата: " + e.job.employerRu);
            if (TaxBook* t = taxBook(player))
                addRevenue(*t, wage);
        }
}

// ---------------------------------------------------------------- ship arrest (ECONOMY_BANKING 2026-11-20, TRADE_DESIGN §11d)
EconomyServer::ArrestClaim EconomyServer::arrestClaim(std::uint64_t playerId, const std::string& hubId, int systemId) const
{
    ArrestClaim c;
    const ClientLedger* l = ledger(playerId);
    const auto* bank = m_banks.bankForPolity(polityOf(hubId, systemId));
    if (!l || !bank)
        return c;
    c.bankId = bank->id;
    for (const auto& loan : l->loans)
        if (loan.bankId == bank->id)
        {
            c.overdueDays = std::max(c.overdueDays, loan.overdueDays);
            // a restructured debt is never "overdue" (the 60-day restructuring resets it): it counts from its start
            // while less than half of it was repaid - ~5 months after the first missed payment
            if (loan.kind == LoanKind::Restructured && loan.outstanding * 2 >= loan.principal)
                c.overdueDays = std::max(c.overdueDays, m_day - loan.startDay);
        }
    if (const Account* a = findAccount(*l, bank->id))
        c.overdueDays = std::max(c.overdueDays, a->overdueDays);
    c.debtCr = debtAtBankCr(*l, m_banks, bank->id);
    c.due = c.overdueDays >= m_arrestAfterDays && c.debtCr > 0.0;
    return c;
}

void EconomyServer::settleArrest(std::uint64_t playerId, const std::string& bankId, const std::string& hubId, double proceedsCr,
                                 const std::vector<std::string>& seized)
{
    ClientLedger* l = ledger(playerId);
    if (!l)
        return;
    const std::string where = hubDisplayName(hubId);
    const double before = debtAtBankCr(*l, m_banks, bankId);
    double left = 0.0;
    if (proceedsCr > 0.0)
        left = repayFromProceeds(*l, m_banks, bankId, proceedsCr, m_day, "арест в порту " + where + ", торги");
    if (left > 0.005)
        receive(*l, m_banks, bankId, left, m_day, "остаток выручки от торгов после погашения долга");
    const double after = debtAtBankCr(*l, m_banks, bankId);
    std::string list;
    for (const auto& s : seized)
        list += (list.empty() ? "" : "; ") + s;
    logEvent(*l, m_day, seized.empty() ? "арест в порту " + where + ": изымать нечего (орудие труда защищено), долг " + grouped(after) + " Cr"
                                       : "арест в порту " + where + " (обменный): " + list + "; выручка " + grouped(proceedsCr) +
                                             " Cr, долг " + grouped(before) + " -> " + grouped(after) + " Cr");
    game::simulation::ClientTradeResult r;
    r.ok = false;
    r.code = "arrest";
    r.totalCr = proceedsCr;
    r.message = seized.empty() ? "арест: изымать нечего, долг " + grouped(after) + " Cr"
                               : "арест: заменено " + std::to_string(seized.size()) + " модул., выручка " + grouped(proceedsCr) + " Cr, долг " +
                                     grouped(after) + " Cr";
    pushResult(playerId, r);
    saveNow("ship arrest");
    std::cerr << "[Arrest] player " << playerId << " at " << hubId << ": " << seized.size() << " module(s) exchanged, proceeds "
              << grouped(proceedsCr) << " Cr, debt " << grouped(before) << " -> " << grouped(after) << " Cr, left to the client "
              << grouped(left) << " Cr" << '\n';
}

// ---------------------------------------------------------------- cargo insurance (TRADE_DESIGN §3.6, step 13)
EconomyServer::InsuranceQuote EconomyServer::quoteCargoInsurance(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId,
                                                                  int systemId) const
{
    InsuranceQuote q;
    const CargoManifest* m = m_cargo.find(shipKey);
    if (!m || m->lots.empty())
    {
        q.why = "трюм пуст";
        return q;
    }
    for (const auto& lot : m->lots)
    {
        if (!lot.contractId.empty())
            continue;   // the issuer's goods: the issuer insures them
        const int g = goodIndex(lot.goodId);
        const std::string legal = legalityFor(playerId, g, hubId, systemId);
        if (legal == "banned" || legal == "restricted")
            continue;   // contraband is not insurable
        double unit = g >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(g)].baseCrPerTonne : 0.0;
        if (MarketSide side; g >= 0 && marketSide(hubId, lot.goodId, side))   // the price the market trades at (regime applied)
            unit = side.fixedPriceCr > 0.0 ? side.fixedPriceCr : side.priceCr;
        q.valueCr += unit * lot.tonnes;
        q.lotIds.push_back(lot.lotId);
    }
    if (q.lotIds.empty())
        q.why = "нечего страховать: груз контракта или контрабанда";
    q.premiumCr = std::round(m_cargoPremiumShare * q.valueCr * 100.0) / 100.0;   // x (1 + 2 x risk): risk 0 for now
    return q;
}

game::simulation::ClientTradeResult EconomyServer::insureCargo(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId,
                                                               int systemId, std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    ClientLedger* l = ledger(playerId);
    if (!l)
        return refuse("no_account", "нет счёта");
    if (hubId.empty())
        return refuse("not_docked", "страховку оформляют у причала");
    const InsuranceQuote q = quoteCargoInsurance(playerId, shipKey, hubId, systemId);
    if (!q.why.empty())
        return refuse("not_insurable", q.why);
    const std::string bank = bankOf(hubId, systemId);
    const PaymentResult p = pay(*l, m_banks, bank, q.premiumCr, Purpose::Fee, m_day, false, "страховка груза");
    if (!p.ok)
        return refuse("no_money", "не хватает денег на премию " + grouped(q.premiumCr) + " Cr");
    if (TaxBook* t = taxBook(playerId))
        addExpense(*t, q.premiumCr);
    // one policy per ship: a new one replaces the old
    m_cargoPolicies.erase(std::remove_if(m_cargoPolicies.begin(), m_cargoPolicies.end(), [&](const CargoPolicy& c) { return c.shipKey == shipKey; }),
                          m_cargoPolicies.end());
    CargoPolicy pol;
    pol.playerId = playerId;
    pol.shipKey = shipKey;
    pol.bankId = bank;
    pol.insuredCr = q.valueCr;
    pol.endDay = m_day + m_insuranceDays;
    // per insured lot: its tonnes and value now (the same unit prices as the quote)
    if (const CargoManifest* m = m_cargo.find(shipKey))
        for (const auto& lot : m->lots)
            if (std::find(q.lotIds.begin(), q.lotIds.end(), lot.lotId) != q.lotIds.end())
            {
                const int g = goodIndex(lot.goodId);
                double unit = g >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(g)].baseCrPerTonne : 0.0;
                if (MarketSide side; g >= 0 && marketSide(hubId, lot.goodId, side))   // the price the market trades at (regime applied)
                    unit = side.fixedPriceCr > 0.0 ? side.fixedPriceCr : side.priceCr;
                pol.lotTonnes[lot.lotId] = lot.tonnes;
                pol.lotValueCr[lot.lotId] = unit * lot.tonnes;
            }
    m_cargoPolicies.push_back(pol);
    logEvent(*l, m_day, "страховка груза на " + grouped(q.valueCr) + " Cr до дня " + std::to_string(pol.endDay) + ": -" + grouped(q.premiumCr) + " Cr");
    r.ok = true;
    r.code = "insured";
    r.totalCr = -q.premiumCr;
    r.message = "груз застрахован на " + grouped(q.valueCr) + " Cr до дня " + std::to_string(pol.endDay);
    pushResult(playerId, r);
    saveNow("cargo insurance");
    std::cerr << "[Insurance] player " << playerId << ": cargo of ship " << shipKey << " insured for " << grouped(q.valueCr) << " Cr, premium "
              << grouped(q.premiumCr) << " Cr, until day " << pol.endDay << '\n';
    return r;
}

// ---------------------------------------------------------------- smuggling (TRADE_DESIGN §2.8, step 9)
std::string EconomyServer::familyAt(const std::string& hubId, int systemId) const
{
    const auto* bank = m_banks.bank(polityOf(hubId, systemId));
    return EconomyModel::instance().regimeFamily(bank && !bank->regime.empty() ? bank->regime : std::string("no_state"));
}

std::string EconomyServer::legalityAt(int good, const std::string& hubId, int systemId) const
{
    return EconomyModel::instance().legalityOf(good, familyAt(hubId, systemId));
}

double EconomyServer::blackBidCrPerT(std::uint64_t playerId, int good, const std::string& hubId, int systemId) const
{
    if (good < 0)
        return 0.0;
    const std::string legality = legalityFor(playerId, good, hubId, systemId);
    if (legality != "banned" && legality != "restricted")
        return 0.0;   // legal / licensed: the open market
    // the shadow price: the free cell price (under a state price it keeps moving), else the good's base price
    double shadow = EconomyModel::instance().goods()[static_cast<std::size_t>(good)].baseCrPerTonne;
    if (const auto it = m_marketIndex.find(hubId); it != m_marketIndex.end())
        if (const auto& c = m_markets[it->second].cells[static_cast<std::size_t>(good)]; c.traded && c.priceCr > 0.0)
            shadow = c.priceCr;
    return shadow * (legality == "banned" ? m_smuggling.blackMultBanned : m_smuggling.blackMultRestricted);
}

std::string EconomyServer::licenceForGood(const std::string& goodId) const
{
    for (const auto& t : m_licenceTypes)
        if (std::find(t.goods.begin(), t.goods.end(), goodId) != t.goods.end())
            return t.id;
    return {};
}

bool EconomyServer::hasLicence(std::uint64_t playerId, const std::string& polity, const std::string& type) const
{
    const auto it = m_licences.find(playerId);
    if (it == m_licences.end())
        return false;
    for (const auto& l : it->second)
        if (l.type == type && l.polity == polity && l.expiresDay > m_day)
            return true;
    return false;
}

std::string EconomyServer::legalityFor(std::uint64_t playerId, int good, const std::string& hubId, int systemId) const
{
    const std::string raw = legalityAt(good, hubId, systemId);
    if (raw != "restricted" || good < 0)
        return raw;
    const std::string type = licenceForGood(EconomyModel::instance().goods()[static_cast<std::size_t>(good)].id);
    if (type.empty())
        return "licensed";   // no licence type covers it: treated as open (data gap, logged by the checks)
    return hasLicence(playerId, polityOf(hubId, systemId), type) ? "licensed" : "restricted";
}

game::simulation::ClientTradeResult EconomyServer::buyLicence(std::uint64_t playerId, const std::string& type, const std::string& hubId,
                                                               int systemId, std::uint32_t requestId)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.day = m_day;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(playerId, requestId, code, message);
        r.code = code;
        r.message = message;
        return r;
    };
    ClientLedger* l = ledger(playerId);
    const LicenceType* lt = nullptr;
    for (const auto& t : m_licenceTypes)
        if (t.id == type)
            lt = &t;
    if (!l || !lt)
        return refuse("no_licence_type", "нет такой лицензии: " + type);
    if (hubId.empty())
        return refuse("not_docked", "лицензию оформляют у причала хаба политии");
    const std::string polity = polityOf(hubId, systemId);
    const double fee = std::round(lt->feeCr * fiscalStyleOf(hubId, systemId).portIndex * 100.0) / 100.0;
    const auto* bank = m_banks.bankForPolity(polity);
    const PaymentResult p = pay(*l, m_banks, bankOf(hubId, systemId), fee, Purpose::Fee, m_day, false,
                                "лицензия «" + lt->ru + "» (" + (bank ? bank->polityName : polity) + ")");
    if (!p.ok)
        return refuse("no_money", "не хватает денег на лицензию: " + grouped(fee) + " Cr (" + p.reason + ")");
    if (TaxBook* t = taxBook(playerId))
        addExpense(*t, fee);
    auto& held = m_licences[playerId];
    HeldLicence* existing = nullptr;
    for (auto& h : held)
        if (h.type == type && h.polity == polity)
            existing = &h;
    if (!existing)
    {
        held.push_back({type, polity, m_day});
        existing = &held.back();
    }
    existing->expiresDay = std::max(existing->expiresDay, m_day) + m_licenceDays;
    logEvent(*l, m_day, "лицензия «" + lt->ru + "»: -" + grouped(fee) + " Cr, действует до дня " + std::to_string(existing->expiresDay));
    r.ok = true;
    r.code = "licence";
    r.totalCr = -fee;
    r.message = "лицензия «" + lt->ru + "» до дня " + std::to_string(existing->expiresDay);
    pushResult(playerId, r);
    saveNow("licence");
    std::cerr << "[Licence] player " << playerId << ": " << type << " from " << polity << " for " << grouped(fee) << " Cr, until day "
              << existing->expiresDay << '\n';
    return r;
}

game::simulation::ClientTradeResult EconomyServer::sellBlack(const TradeOrder& o)
{
    game::simulation::ClientTradeResult r;
    r.requestId = o.requestId;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(o.playerId, o.requestId, code, message);
        r.code = code;
        r.message = message;
        r.day = m_day;
        return r;
    };
    ClientLedger* l = ledger(o.playerId);
    const int g = goodIndex(o.goodId);
    if (!l || g < 0)
        return refuse("no_good", "неизвестный товар " + o.goodId);
    const auto& good = EconomyModel::instance().goods()[static_cast<std::size_t>(g)];
    const double price = blackBidCrPerT(o.playerId, g, o.hubId, o.systemId);
    if (!(price > 0.0))
        return refuse("legal_here", "«" + good.ru + "» здесь не запрещён: продавайте на рынке");
    const CargoManifest* m = m_cargo.find(o.shipKey);
    const double aboard = m ? m->ownTonnesOf(o.goodId) : 0.0;
    const double tonnes = o.tonnes > 0.0 ? std::min(o.tonnes, aboard) : aboard;
    if (!(tonnes > 1.0e-6))
        return refuse("not_aboard", "на борту нет своего «" + good.ru + "»");
    auto& sold = m_blackSold[o.hubId];
    if (sold.first != m_day)
        sold = {m_day, 0.0};
    if (sold.second + tonnes > m_smuggling.blackCapT + 1.0e-9)
        return refuse("black_full", "скупщик сегодня берёт не больше " + grouped(std::max(0.0, m_smuggling.blackCapT - sold.second), 1) + " т");
    double basis = 0.0;
    (void)m_cargo.unload(o.shipKey, o.goodId, tonnes, good.densityTM3, basis);
    m_cargoChanged.insert(o.shipKey);
    sold.second += tonnes;
    const double total = std::round(tonnes * price * 100.0) / 100.0;
    char memo[160];
    std::snprintf(memo, sizeof memo, "из-под полы: %.2f т «%s»", tonnes, good.ru.c_str());
    receive(*l, m_banks, bankOf(o.hubId, o.systemId), total, m_day, memo);   // not declared: no tax book line
    logEvent(*l, m_day, std::string(memo) + ": +" + grouped(total) + " Cr");
    r.ok = true;
    r.code = "black_sold";
    r.totalCr = total;
    r.message = std::string("продано из-под полы ") + grouped(tonnes, 2) + " т по " + grouped(price) + " Cr/т";
    pushResult(o.playerId, r);
    saveNow("black market");
    std::cerr << "[Smuggling] player " << o.playerId << ": BLACK SALE " << tonnes << " t " << o.goodId << " at " << o.hubId << " x "
              << grouped(price) << " = " << grouped(total) << " Cr" << '\n';
    return r;
}

EconomyServer::CustomsResult EconomyServer::customsInspection(std::uint64_t playerId, std::uint64_t shipKey, const std::string& callId,
                                                              const std::string& hubId, int systemId)
{
    CustomsResult res;
    ClientLedger* l = ledger(playerId);
    if (!l || !m_loaded)
        return res;
    const auto& S = m_smuggling;
    const std::string polity = polityOf(hubId, systemId);
    const auto* hubBank = m_banks.bankForPolity(polity);
    const Channel ch = hubBank ? m_banks.channel(l->homeBank, hubBank->id) : Channel::Open;
    double p = S.inspectP[static_cast<int>(ch)];
    if (familyAt(hubId, systemId) == "strict")
        p *= S.strictMult;
    if (marketRegimeOf(polity) == "planned")
        p *= S.plannedMult;
    if (m_devCustomsP >= 0.0)
        p = m_devCustomsP;
    res.probability = std::clamp(p, 0.0, 1.0);
    const std::uint64_t seed = hashOf(hubId + "|" + callId, shipKey ^ (static_cast<std::uint64_t>(m_day) << 20));
    if (unit(seed) >= res.probability)
        return res;
    res.inspected = true;
    const std::string where = hubDisplayName(hubId);
    res.feeCr = S.inspectionFeeCr;
    if (res.feeCr > 0.0)
    {
        (void)payOrDebt(*l, m_banks, bankOf(hubId, systemId), res.feeCr, Purpose::Fee, m_day, "досмотр на таможне " + where, "сбор в долг");
        if (TaxBook* t = taxBook(playerId))
            addExpense(*t, res.feeCr);
    }
    const CargoManifest* m = m_cargo.find(shipKey);
    std::vector<std::string> contraband;   // goods
    bool anyLegal = false;
    if (m)
        for (const auto& lot : m->lots)
        {
            const std::string legal = legalityFor(playerId, goodIndex(lot.goodId), hubId, systemId);
            const bool banned = legal == "banned" || legal == "restricted";   // contraband (no licence for a restricted good)
            if (banned && std::find(contraband.begin(), contraband.end(), lot.goodId) == contraband.end())
                contraband.push_back(lot.goodId);
            anyLegal = anyLegal || !banned;
        }
    if (contraband.empty())
    {
        logEvent(*l, m_day, "досмотр на таможне " + where + ": нарушений нет (сбор " + grouped(res.feeCr) + " Cr)");
        std::cerr << "[Smuggling] player " << playerId << ": customs at " << hubId << " (p " << res.probability << "): clean" << '\n';
        return res;
    }
    // found? hidden in legal cargo is harder to find; every seizure by this polity in a year makes them look harder
    int recent = 0;
    for (const auto& [pol, day] : m_seizures[playerId])
        recent += pol == polity && m_day - day <= 365;
    const double attention = std::min(S.attentionCap, 1.0 + S.attentionPerSeizure * recent);
    double pFind = std::min(1.0, (anyLegal ? S.hiddenInLegal : 1.0) * attention);
    if (m_devCustomsP >= 1.0)
        pFind = 1.0;   // DEV: a forced inspection always finds
    if (unit(seed ^ 0x5eedfeedull) >= pFind)
    {
        logEvent(*l, m_day, "досмотр на таможне " + where + ": ничего не нашли");
        std::cerr << "[Smuggling] player " << playerId << ": customs at " << hubId << " (p " << res.probability << "): contraband NOT found (p_find "
                  << pFind << ")" << '\n';
        return res;
    }
    res.found = true;
    std::vector<std::string> failed;
    for (const auto& goodId : contraband)
    {
        const int g = goodIndex(goodId);
        const auto& good = EconomyModel::instance().goods()[static_cast<std::size_t>(g)];
        double unitCr = good.baseCrPerTonne;
        if (const auto it = m_marketIndex.find(hubId); it != m_marketIndex.end())
            if (const auto& c = m_markets[it->second].cells[static_cast<std::size_t>(g)]; c.traded && c.priceCr > 0.0)
                unitCr = c.priceCr;
        double own = 0.0;
        if (const CargoManifest* now = m_cargo.find(shipKey))
        {
            own = now->ownTonnesOf(goodId);
            for (const auto& lot : now->lots)   // a contract's lot of it: the issuer's cargo goes too, the contract fails
                if (lot.goodId == goodId && !lot.contractId.empty() &&
                    std::find(failed.begin(), failed.end(), lot.contractId) == failed.end())
                {
                    failed.push_back(lot.contractId);
                    res.seizedT += lot.tonnes;
                    res.valueCr += lot.tonnes * unitCr;
                }
        }
        double basis = 0.0;
        if (own > 0.0)   // (the manifest may be gone after this - nothing above is used again)
            (void)m_cargo.unload(shipKey, goodId, own, good.densityTM3, basis);
        res.seizedT += own;
        res.valueCr += own * unitCr;
    }
    for (const auto& id : failed)
        if (Contract* c = m_board.contract(id); c && c->status == "active")
            failContract(*c, "failed", "груз изъят таможней");
    m_cargoChanged.insert(shipKey);
    res.fineCr = std::round(S.fineMult * res.valueCr * 100.0) / 100.0;
    (void)payFine(*l, m_banks, bankOf(hubId, systemId), res.fineCr, m_day, "контрабанда, " + where);
    m_seizures[playerId].push_back({polity, m_day});
    logEvent(*l, m_day, "таможня " + where + ": изъято " + grouped(res.seizedT, 2) + " т контрабанды (" + grouped(res.valueCr) + " Cr), штраф " +
                            grouped(res.fineCr) + " Cr");
    game::simulation::ClientTradeResult r;
    r.ok = false;
    r.code = "customs_seized";
    r.totalCr = -res.fineCr;
    r.message = "таможня: изъято " + grouped(res.seizedT, 2) + " т, штраф " + grouped(res.fineCr) + " Cr";
    pushResult(playerId, r);
    saveNow("customs");
    std::cerr << "[Smuggling] player " << playerId << ": customs at " << hubId << " (p " << res.probability << ", attention " << attention
              << "): SEIZED " << res.seizedT << " t worth " << grouped(res.valueCr) << " Cr, fine " << grouped(res.fineCr) << " Cr, "
              << failed.size() << " contract(s) failed; net worth " << grouped(netWorthCr(*l, m_banks)) << " Cr" << '\n';
    return res;
}

// ---------------------------------------------------------------- market regimes (TRADE_DESIGN §2.5, step 8)
std::string EconomyServer::marketRegimeOf(const std::string& polity) const
{
    const auto* bank = m_banks.bank(polity);   // the polity's own bank carries its regime; none -> no state
    const std::string regime = bank && !bank->regime.empty() ? bank->regime : std::string("no_state");
    const auto it = m_regimeMarket.find(regime);
    return it == m_regimeMarket.end() ? std::string("free") : it->second;
}

MarketSide EconomyServer::regimeSide(const HubMarket& m, int good, std::uint64_t playerId, std::string* note) const
{
    const auto& c = m.cells[static_cast<std::size_t>(good)];
    MarketSide side{c.priceCr, m.spread, c.stockT, c.targetT};
    const std::string polity = polityOf(m.hubId, m.systemId);
    const std::string regime = marketRegimeOf(polity);
    const std::string& group = EconomyModel::instance().goods()[static_cast<std::size_t>(good)].group;
    if (std::find(m_bulkGroups.begin(), m_bulkGroups.end(), group) != m_bulkGroups.end())
        side.handlingMult = m_bulkHandlingShare;
    const auto in = [&](const std::vector<std::string>& v) { return std::find(v.begin(), v.end(), group) != v.end(); };
    const auto& R = m_regimeRules;
    if (regime == "planned" || (regime == "mobilization" && in(R.mobilizationGroups)))
    {
        // the state price: fixed for a plan period; the hub sells only what is above the state's need (Kornai)
        const int plan = std::max(0, m_day) / R.planDays;
        const double h = static_cast<double>(hashOf(polity + "|" + std::to_string(good), static_cast<std::uint64_t>(plan) + 0x91a7ull) % 10000ull) / 10000.0;
        side.fixedPriceCr = c.anchorCr * (R.stateMin + (R.stateMax - R.stateMin) * h);
        side.giveCapT = std::max(0.0, c.stockT - R.stateNeedShare * c.targetT);
        if (note)
            *note = side.giveCapT > 0.0 ? std::string("госцена") : std::string("нет в продаже: госпотребность");
        return side;
    }
    if (regime == "corridor" && in(R.corridorGroups) && c.anchorCr > 0.0)   // the corridor holds from the first day, not only after a daily step
        side.priceCr = std::clamp(side.priceCr, c.anchorCr * R.corridorMin, c.anchorCr * R.corridorMax);
    if (regime == "corridor" && in(R.corridorGroups) && c.stockT < R.rationBelowStockShare * c.targetT)
    {
        // a shortage under a held price: rationing - a buyer's day quota
        const double ration = std::max(R.rationMinT, R.rationStockShare * c.stockT);
        double used = 0.0;
        if (playerId != 0)
            if (const auto it = m_rationUsed.find(std::to_string(playerId) + "|" + m.hubId + "|" + std::to_string(good));
                it != m_rationUsed.end() && it->second.first == m_day)
                used = it->second.second;
        side.giveCapT = std::max(0.0, ration - used);
        if (note)
        {
            char b[96];
            std::snprintf(b, sizeof b, "по карточкам: %.1f т на сегодня", side.giveCapT);
            *note = b;
        }
        return side;
    }
    if (regime == "corridor" && in(R.corridorGroups) && note)
        *note = "цена в коридоре";
    return side;
}

void EconomyServer::recordRation(const HubMarket& m, int good, std::uint64_t playerId, double tonnes)
{
    if (playerId == 0)
        return;
    auto& r = m_rationUsed[std::to_string(playerId) + "|" + m.hubId + "|" + std::to_string(good)];
    if (r.first != m_day)
        r = {m_day, 0.0};
    r.second += tonnes;
}

// ---------------------------------------------------------------- the income tax (TaxModel, TRADE_DESIGN step 7)
TaxBook* EconomyServer::taxBook(std::uint64_t playerId)
{
    if (playerId == 0 || !m_ledgers.count(playerId))
        return nullptr;   // AI: no books
    return &m_taxBooks[playerId];
}

FiscalStyle EconomyServer::residenceStyle(const ClientLedger& l) const
{
    // the residence polity = the home bank's (a bank id is its polity id); its regime -> the fiscal style
    const auto* bank = m_banks.bank(l.homeBank);
    const std::string regime = bank && !bank->regime.empty() ? bank->regime : std::string("no_state");
    if (const auto it = m_regimeStyle.find(regime); it != m_regimeStyle.end())
        if (const auto st = m_styles.find(it->second); st != m_styles.end())
            return st->second;
    return FiscalStyle{};
}

void EconomyServer::receiveContractMoney(std::uint64_t playerId, ClientLedger& l, const std::string& issuerBank, double amountCr,
                                         const std::string& memo)
{
    if (!(amountCr > 0.0))
        return;
    double withheld = 0.0;
    bool credited = false;
    if (!issuerBank.empty() && issuerBank != l.homeBank && m_taxRules.withholdingShare > 0.0)
    {
        // withheld at source by the issuer's country; credited at home only through a treaty (an open / licensed channel)
        withheld = std::round(amountCr * m_taxRules.withholdingShare * 100.0) / 100.0;
        const Channel ch = m_banks.channel(l.homeBank, issuerBank);
        credited = ch == Channel::Open || ch == Channel::Licensed;
    }
    receive(l, m_banks, issuerBank, amountCr - withheld, m_day, memo);
    if (withheld > 0.0)
    {
        const auto* b = m_banks.bank(issuerBank);
        logEvent(l, m_day, "налог у источника -" + grouped(withheld) + " Cr (" + (b ? b->polityName : issuerBank) + ", " +
                               (credited ? "зачтётся дома" : "дома не зачитывается: нет соглашения") + ")");
    }
    if (TaxBook* t = taxBook(playerId))
    {
        addRevenue(*t, amountCr);
        addWithheld(*t, withheld, credited);
    }
}

void EconomyServer::stepTaxes(int day)
{
    for (auto& [id, l] : m_ledgers)
    {
        TaxBook* t = taxBook(id);
        if (!t)
            continue;
        const FiscalStyle rs = residenceStyle(l);
        const TaxAssessment a = rollTaxPeriod(*t, day, rs.incomeTaxShare, rs.taxAdminFeeShare, m_taxRules);
        if (a.closed)
        {
            const int from = a.period * m_taxRules.periodDays;
            logEvent(l, day, "налоговый период " + std::to_string(from) + "-" + std::to_string(from + m_taxRules.periodDays - 1) +
                                 ": прибыль " + grouped(a.profitCr) + " Cr, налог " + grouped(a.taxCr + a.feeCr) + " Cr к уплате до дня " +
                                 std::to_string(a.dueDay));
            std::cerr << "[Tax] player " << id << ": period " << from << ".." << (from + m_taxRules.periodDays - 1) << " closed: profit "
                      << grouped(a.profitCr) << " Cr x " << rs.incomeTaxShare << " (" << rs.ru << ") -> tax " << grouped(a.taxCr)
                      << " + fee " << grouped(a.feeCr) << " Cr, due day " << a.dueDay << '\n';
            saveNow("tax");
        }
        if (!taxDue(*t, day))
            continue;
        const double amount = t->assessedTaxCr + t->assessedFeeCr;
        const std::string memo = "налог на доход за период с дня " + std::to_string(t->assessedPeriod * m_taxRules.periodDays) +
                                 (t->assessedFeeCr > 0.0 ? " (с платой за администрирование " + grouped(t->assessedFeeCr) + " Cr)" : std::string());
        const PaymentResult p = payOrDebt(l, m_banks, l.homeBank, amount, Purpose::Tax, day, memo, "налог в долг");
        const double debt = p.ok ? p.lifelineCr : amount;
        const double penalty = settleTax(*t, amount - debt, debt, m_taxRules);
        if (penalty > 0.0)
            (void)payOrDebt(l, m_banks, l.homeBank, penalty, Purpose::Fine, day,
                            "пени по налогу (" + std::to_string(static_cast<int>(std::lround(m_taxRules.latePenaltyShare * 100.0))) +
                                " % неуплаченного)", "пени в долг");
        logEvent(l, day, "налог на доход " + grouped(amount) + " Cr: " + (debt > 0.0 ? "в долг" + std::string(penalty > 0.0 ? " + пени " + grouped(penalty) + " Cr" : "") : "уплачен"));
        std::cerr << "[Tax] player " << id << ": day " << day << " DUE " << grouped(amount) << " Cr -> paid " << grouped(amount - debt)
                  << ", debt " << grouped(debt) << ", penalty " << grouped(penalty) << "; net worth " << grouped(netWorthCr(l, m_banks))
                  << " Cr" << '\n';
        saveNow("tax");
    }
}

void EconomyServer::ensurePlayer(std::uint64_t playerId, int homeSystemId)
{
    if (!m_loaded || playerId == 0 || m_ledgers.count(playerId))
        return;
    const auto it = m_systemPolity.find(homeSystemId);
    const auto* bank = m_banks.bankForPolity(it == m_systemPolity.end() ? std::string() : it->second);
    m_ledgers.emplace(playerId, openLedger(m_banks, playerId, bank ? bank->id : std::string(), std::max(0, m_day)));
    std::cerr << "[EconomyServer] player " << playerId << ": home bank " << (bank ? bank->name : "?") << ", net "
              << grouped(netWorthCr(m_ledgers[playerId], m_banks)) << " Cr" << '\n';
}

const ClientLedger* EconomyServer::ledger(std::uint64_t playerId) const
{
    const auto it = m_ledgers.find(playerId);
    return it == m_ledgers.end() ? nullptr : &it->second;
}

ClientLedger* EconomyServer::ledger(std::uint64_t playerId)
{
    const auto it = m_ledgers.find(playerId);
    return it == m_ledgers.end() ? nullptr : &it->second;
}

std::string EconomyServer::polityOf(const std::string& hubId, int systemId) const
{
    if (const auto it = m_hubPolity.find(hubId); it != m_hubPolity.end() && !it->second.empty())
        return it->second;
    if (systemId < 0)
        if (const auto* h = EconomyModel::instance().hub(hubId))
            systemId = h->systemId;
    const auto it = m_systemPolity.find(systemId);
    return it == m_systemPolity.end() ? std::string() : it->second;
}

std::string EconomyServer::bankOf(const std::string& hubId, int systemId) const
{
    const auto* b = m_banks.bankForPolity(polityOf(hubId, systemId));
    return b ? b->id : std::string();
}

// ---------------------------------------------------------------- quotes and charges
const EconomyServer::HubMarket* EconomyServer::marketFor(const std::string& hubId, int systemId, int good, bool& systemReference) const
{
    systemReference = false;
    if (good < 0)
        return nullptr;
    const auto g = static_cast<std::size_t>(good);
    if (const auto it = m_marketIndex.find(hubId); it != m_marketIndex.end() && m_markets[it->second].cells[g].traded)
        return &m_markets[it->second];
    // a hub without its own market data (runtime hubs like earth_orbital_hub): the system's main depot
    if (systemId < 0)
        if (const auto* h = EconomyModel::instance().hub(hubId))
            systemId = h->systemId;
    const HubMarket* best = nullptr;
    for (const auto& m : m_markets)
        if (m.systemId == systemId && m.cells[g].traded && (!best || m.cells[g].flowTDay > best->cells[g].flowTDay))
            best = &m;
    systemReference = best != nullptr;
    return best;
}

FuelQuote EconomyServer::quoteFuel(const std::string& hubId, int systemId, FuelKind kind) const
{
    FuelQuote q;
    q.freeOfCharge = m_fuelFree;
    if (!m_loaded)
    {
        q.reason = "экономика не загружена";
        return q;
    }
    const int g = goodIndex(fuelGoodId(kind));
    bool sysRef = false;
    const HubMarket* m = marketFor(hubId, systemId, g, sysRef);
    if (!m)
    {
        q.reason = "здесь и в системе это топливо не продаётся";
        return q;
    }
    const auto& good = EconomyModel::instance().goods()[static_cast<std::size_t>(g)];
    const auto& c = m->cells[static_cast<std::size_t>(g)];
    q.available = true;
    q.hubId = m->hubId;
    q.hubName = m->name;
    q.systemReference = sysRef;
    q.polity = polityOf(hubId, systemId);
    q.bankId = bankOf(hubId, systemId);
    if (const auto* b = m_banks.bank(q.bankId))
        q.currency = b->code;
    q.fxPerCr = m_banks.fx(q.bankId);
    const auto split = m_splits.count(good.id) ? m_splits.at(good.id) : m_splits.count("default") ? m_splits.at("default") : ProductionSplit{};
    q.breakdown = explainPrice(good.baseCrPerTonne, c.producer, m->transportCrT[static_cast<std::size_t>(g)], c.anchorCr, c.priceCr,
                               m->spread, split);
    // the station's own markup (an illegal pump, a remote tender) is part of THE price - the refuel service, the
    // wallet and the finance tab all read this one number (audit 2026-11-19: it was applied only in the simulation)
    if (const auto* station = game::fuel::FuelStationCatalog::instance().find(hubId))
        q.stationFactor = station->priceFactor > 0.0 ? station->priceFactor : 1.0;
    q.listCrPerT = q.breakdown.listCr * q.stationFactor;
    q.listLocalPerT = q.listCrPerT * q.fxPerCr;
    q.chargedLocalPerT = m_fuelFree ? 0.0 : q.listLocalPerT;
    q.stockT = c.stockT;
    return q;
}

FuelCharge EconomyServer::chargeFuel(std::uint64_t playerId, const std::string& hubId, int systemId, FuelKind kind, double tonnes)
{
    FuelCharge r;
    r.tonnesRequested = tonnes;
    r.quote = quoteFuel(hubId, systemId, kind);
    if (!r.quote.available || !(tonnes > 0.0))
    {
        r.reason = r.quote.available ? "нулевой объём" : r.quote.reason;
        return r;
    }
    ClientLedger* l = ledger(playerId);
    const char* what = kind == FuelKind::JumpCharge ? "заряд прыжка" : kind == FuelKind::ReactorFuel ? "реакторное топливо" : "рабочее тело";
    char memo[160];
    double granted = tonnes;
    if (m_fuelFree || !l)
    {
        r.ok = true;
        r.listCr = granted * r.quote.listCrPerT;
        if (l)
        {
            l->freeFuelListCr += r.listCr;
            std::snprintf(memo, sizeof memo, "заправка: %s %.2f т бесплатно (по прейскуранту %s Cr)", what, granted, grouped(r.listCr).c_str());
            logEvent(*l, m_day, memo);
        }
        else
            m_aiFreeFuelListCr += r.listCr;
    }
    else
    {
        std::snprintf(memo, sizeof memo, "заправка: %s %.2f т", what, granted);
        PaymentResult p = pay(*l, m_banks, r.quote.bankId, granted * r.quote.listCrPerT, Purpose::Fuel, m_day, false, memo);
        if (!p.ok)
        {
            // cannot pay it all: at least the lifeline amount, on credit if needed
            granted = std::min(tonnes, m_banks.rules.lifelineFuelMaxT);
            std::snprintf(memo, sizeof memo, "заправка (минимум до станции): %s %.2f т", what, granted);
            p = pay(*l, m_banks, r.quote.bankId, granted * r.quote.listCrPerT, Purpose::Fuel, m_day, true, memo);
        }
        if (!p.ok)
        {
            r.reason = p.reason;
            return r;
        }
        r.ok = true;
        r.listCr = granted * r.quote.listCrPerT;
        r.chargedCr = r.listCr;
        if (TaxBook* t = taxBook(playerId))
            addExpense(*t, r.chargedCr);
        r.lifelineCr = p.lifelineCr;
    }
    r.tonnesGranted = granted;
    // the fuel physically leaves the depot even when it is free: its stock (and tomorrow's price) feels it
    if (const auto it = m_marketIndex.find(r.quote.hubId); it != m_marketIndex.end())
    {
        auto& c = m_markets[it->second].cells[static_cast<std::size_t>(goodIndex(fuelGoodId(kind)))];
        c.stockT = std::max(0.0, c.stockT - granted);
    }
    std::cerr << "[Economy] fuel " << fuelGoodId(kind) << " " << granted << " t at " << r.quote.hubName << " for "
              << (playerId ? "player " + std::to_string(playerId) : std::string("AI")) << ": list " << grouped(r.listCr) << " Cr, charged "
              << grouped(r.chargedCr) << " Cr" << (r.lifelineCr > 0.0 ? " (lifeline credit)" : "") << '\n';
    return r;
}

void EconomyServer::recordFuelDelivery(std::uint64_t playerId, const std::string& hubId, int systemId, FuelKind kind, double tonnes)
{
    if (!m_loaded || !(tonnes > 0.0))
        return;
    const FuelQuote q = quoteFuel(hubId, systemId, kind);
    if (!q.available)
        return;
    const double list = tonnes * q.listCrPerT;
    if (m_fuelFree)
    {
        if (ClientLedger* l = ledger(playerId))
        {
            l->freeFuelListCr += list;
            char memo[160];
            std::snprintf(memo, sizeof memo, "заправка: %s %.2f т бесплатно (по прейскуранту %s Cr)", fuelGoodId(kind), tonnes,
                          grouped(list).c_str());
            logEvent(*l, m_day, memo);
        }
        else
            m_aiFreeFuelListCr += list;
    }
    if (const auto it = m_marketIndex.find(q.hubId); it != m_marketIndex.end())
    {
        auto& c = m_markets[it->second].cells[static_cast<std::size_t>(goodIndex(fuelGoodId(kind)))];
        c.stockT = std::max(0.0, c.stockT - tonnes);
    }
}

void EconomyServer::refundFuel(std::uint64_t playerId, const std::string& hubId, int systemId, FuelKind kind, double tonnes,
                               double crPerT)
{
    if (!m_loaded || !(tonnes > 0.0))
        return;
    const FuelQuote q = quoteFuel(hubId, systemId, kind);
    if (q.available)
        if (const auto it = m_marketIndex.find(q.hubId); it != m_marketIndex.end())
            m_markets[it->second].cells[static_cast<std::size_t>(goodIndex(fuelGoodId(kind)))].stockT += tonnes;
    if (crPerT > 0.0 && playerId != 0)
    {
        char memo[160];
        std::snprintf(memo, sizeof memo, "возврат за незалитое топливо: %.2f т", tonnes);
        credit(playerId, hubId, systemId, tonnes * crPerT, memo);
    }
}

PaymentResult EconomyServer::chargeService(std::uint64_t playerId, const std::string& hubId, int systemId, Purpose purpose,
                                           double amountCr, const std::string& memo)
{
    ClientLedger* l = ledger(playerId);
    if (!l)
    {
        PaymentResult r;
        r.ok = playerId == 0;   // AI: no wallets yet
        r.reason = r.ok ? "" : "нет счёта";
        return r;
    }
    const PaymentResult p = pay(*l, m_banks, bankOf(hubId, systemId), amountCr, purpose, m_day, true, memo);
    if (TaxBook* t = taxBook(playerId); t && p.ok && purpose != Purpose::Fine && purpose != Purpose::Other)
        addExpense(*t, amountCr);
    return p;
}

PaymentResult EconomyServer::chargeFine(std::uint64_t playerId, const std::string& hubId, int systemId, double amountCr,
                                        const std::string& memo)
{
    ClientLedger* l = ledger(playerId);
    if (!l)
    {
        PaymentResult r;
        r.ok = playerId == 0;
        r.reason = r.ok ? "" : "нет счёта";
        return r;
    }
    return payFine(*l, m_banks, bankOf(hubId, systemId), amountCr, m_day, memo);
}

void EconomyServer::credit(std::uint64_t playerId, const std::string& hubId, int systemId, double amountCr, const std::string& memo)
{
    if (ClientLedger* l = ledger(playerId))
        receive(*l, m_banks, bankOf(hubId, systemId), amountCr, m_day, memo);
}

// ---------------------------------------------------------------- port fees (TRADE_DESIGN step 4)
namespace
{
// marks the call's event booked; false = it already was (the idempotency of a repeated docked / released event)
bool markPortCall(std::map<std::string, int>& calls, std::deque<std::string>& order, const std::string& callId, int bit)
{
    auto [it, fresh] = calls.try_emplace(callId, 0);
    if (fresh)
    {
        order.push_back(callId);
        while (order.size() > 512)
        {
            calls.erase(order.front());
            order.pop_front();
        }
        it = calls.find(callId);
    }
    if (it == calls.end() || (it->second & bit))
        return false;
    it->second |= bit;
    return true;
}
} // namespace

std::string EconomyServer::hubDisplayName(const std::string& hubId) const
{
    if (const auto it = m_marketIndex.find(hubId); it != m_marketIndex.end() && !m_markets[it->second].name.empty())
        return m_markets[it->second].name;
    if (const auto* h = EconomyModel::instance().hub(hubId); h && !h->name.empty())
        return h->name;
    return hubId;
}

EconomyServer::PortCharge EconomyServer::chargeDocking(std::uint64_t playerId, const std::string& callId, const std::string& hubId,
                                                       int systemId, double dryMassT, const std::string& hubName)
{
    PortCharge c;
    const FiscalStyle style = fiscalStyleOf(hubId, systemId);
    c.styleId = style.id;
    c.portIndex = style.portIndex;
    c.feeCr = dockingFeeCr(dryMassT, style, m_dealRules);
    ClientLedger* l = ledger(playerId);
    if (!m_loaded || !l || !(c.feeCr > 0.0))
        return c;   // AI ships (no wallets yet) / no economy: nothing booked
    if (!markPortCall(m_portCalls, m_portCallOrder, callId, 1))
    {
        c.duplicate = true;
        return c;
    }
    char memo[200];
    std::snprintf(memo, sizeof memo, "стыковка %s (%.0f т сухой массы, индекс порта %.1f)", (hubName.empty() ? hubDisplayName(hubId) : hubName).c_str(),
                  dryMassT, style.portIndex);
    const PaymentResult p = payOrDebt(*l, m_banks, bankOf(hubId, systemId), c.feeCr, Purpose::Docking, m_day, memo, "сбор в долг");
    if (TaxBook* t = taxBook(playerId); t && p.ok)
        addExpense(*t, c.feeCr);
    c.booked = p.ok;
    c.debtCr = p.ok ? p.lifelineCr : 0.0;
    c.paidCr = p.ok ? c.feeCr - c.debtCr : 0.0;
    std::cerr << "[PortFee] player " << playerId << " call " << callId << ": DOCKING at " << hubId << " (" << style.id << ", index "
              << style.portIndex << ", dry " << dryMassT << " t) " << grouped(c.feeCr, 2) << " Cr: paid " << grouped(c.paidCr, 2) << ", debt "
              << grouped(c.debtCr, 2) << "; net worth " << grouped(netWorthCr(*l, m_banks)) << " Cr" << '\n';
    ++m_tradeSerial;
    saveNow("the docking fee");
    return c;
}

EconomyServer::PortCharge EconomyServer::chargeBerth(std::uint64_t playerId, const std::string& callId, const std::string& hubId,
                                                     int systemId, double berthHours, const std::string& hubName)
{
    PortCharge c;
    const FiscalStyle style = fiscalStyleOf(hubId, systemId);
    c.styleId = style.id;
    c.portIndex = style.portIndex;
    c.hours = std::max(0.0, berthHours);
    c.feeCr = berthFeeCr(c.hours, style, m_dealRules);
    ClientLedger* l = ledger(playerId);
    if (!m_loaded || !l)
        return c;
    if (!markPortCall(m_portCalls, m_portCallOrder, callId, 2))
    {
        c.duplicate = true;
        return c;
    }
    if (!(c.feeCr > 0.005))
    {
        std::cerr << "[PortFee] player " << playerId << " call " << callId << ": BERTH at " << hubId << " " << grouped(c.hours, 2)
                  << " h (within the " << m_dealRules.berthIncludedHours << " h the docking fee covers): 0 Cr" << '\n';
        return c;
    }
    char memo[200];
    std::snprintf(memo, sizeof memo, "стоянка %s: %.1f ч (сверх %.0f ч по %.0f Cr/ч)", (hubName.empty() ? hubDisplayName(hubId) : hubName).c_str(),
                  c.hours, m_dealRules.berthIncludedHours, m_dealRules.berthCrPerHour * style.portIndex);
    const PaymentResult p = payOrDebt(*l, m_banks, bankOf(hubId, systemId), c.feeCr, Purpose::Parking, m_day, memo, "сбор в долг");
    if (TaxBook* t = taxBook(playerId); t && p.ok)
        addExpense(*t, c.feeCr);
    c.booked = p.ok;
    c.debtCr = p.ok ? p.lifelineCr : 0.0;
    c.paidCr = p.ok ? c.feeCr - c.debtCr : 0.0;
    std::cerr << "[PortFee] player " << playerId << " call " << callId << ": BERTH at " << hubId << " " << grouped(c.hours, 2) << " h -> "
              << grouped(c.feeCr, 2) << " Cr: paid " << grouped(c.paidCr, 2) << ", debt " << grouped(c.debtCr, 2) << "; net worth "
              << grouped(netWorthCr(*l, m_banks)) << " Cr" << '\n';
    ++m_tradeSerial;
    saveNow("the berth fee");
    return c;
}

void EconomyServer::openPortCall(std::uint64_t shipKey, const std::string& callId, const std::string& hubId,
                                 const std::string& dockModuleId, std::uint64_t playerId)
{
    if (!m_loaded || shipKey == 0 || playerId == 0)
        return;
    const auto it = m_openPortCalls.find(shipKey);
    if (it != m_openPortCalls.end() && it->second.callId == callId)
        return;   // the same call (a repeated docked event)
    m_openPortCalls[shipKey] = {callId, hubId, dockModuleId, playerId, m_universeS};
    saveNow("the port call opened", false);
}

const EconomyServer::OpenPortCall* EconomyServer::openPortCall(std::uint64_t shipKey) const
{
    const auto it = m_openPortCalls.find(shipKey);
    return it == m_openPortCalls.end() ? nullptr : &it->second;
}

double EconomyServer::closePortCall(std::uint64_t shipKey, double timeScale)
{
    const auto it = m_openPortCalls.find(shipKey);
    if (it == m_openPortCalls.end())
        return -1.0;
    const double hours = std::max(0.0, m_universeS - it->second.sinceUniverseS) * std::max(0.0, timeScale) / 3600.0;
    m_openPortCalls.erase(it);
    saveNow("the port call closed");
    return hours;
}

double EconomyServer::importDutyRate(const std::string& originHub, const std::string& hubId, int systemId) const
{
    if (originHub.empty() || originHub == hubId)
        return 0.0;
    const std::string from = polityOf(originHub, -1);
    const std::string to = polityOf(hubId, systemId);
    if (from.empty() || to.empty() || from == to)
        return 0.0;
    const auto* a = m_banks.bankForPolity(from);
    const auto* b = m_banks.bankForPolity(to);
    const Channel ch = a && b ? m_banks.channel(a->id, b->id) : Channel::Licensed;
    return fiscalStyleOf(hubId, systemId).dutyShare * customsFeeMult(ch);
}

// ---------------------------------------------------------------- trade (TRADE_DESIGN steps 1-2)
FiscalStyle EconomyServer::fiscalStyleOf(const std::string& hubId, int systemId) const
{
    const std::string polity = polityOf(hubId, systemId);
    const auto* bank = m_banks.bank(polity);   // the polity's OWN bank carries its regime; a zone without one: no state
    const std::string regime = bank && !bank->regime.empty() ? bank->regime : std::string("no_state");
    if (const auto it = m_regimeStyle.find(regime); it != m_regimeStyle.end())
        if (const auto st = m_styles.find(it->second); st != m_styles.end())
            return st->second;
    return FiscalStyle{};
}

bool EconomyServer::marketSide(const std::string& hubId, const std::string& goodId, MarketSide& out) const
{
    const int g = goodIndex(goodId);
    const auto it = m_marketIndex.find(hubId);
    if (g < 0 || it == m_marketIndex.end())
        return false;
    const auto& m = m_markets[it->second];
    const auto& c = m.cells[static_cast<std::size_t>(g)];
    if (!c.traded)
        return false;
    out = regimeSide(m, g, 0, nullptr);
    return true;
}

void EconomyServer::pushResult(std::uint64_t playerId, game::simulation::ClientTradeResult r)
{
    r.day = m_day;
    auto& v = m_results[playerId];
    v.push_back(std::move(r));
    if (v.size() > 4)
        v.erase(v.begin(), v.end() - 4);
    ++m_tradeSerial;
}

void EconomyServer::recordOperation(std::uint64_t playerId, const std::string& code, const std::string& message, double totalCr)
{
    game::simulation::ClientTradeResult r;
    r.ok = true;
    r.code = code;
    r.message = message;
    r.totalCr = totalCr;
    pushResult(playerId, r);
    if (ClientLedger* l = ledger(playerId))
        logEvent(*l, m_day, message);
    saveNow(code.c_str());
}

void EconomyServer::recordTradeRefusal(std::uint64_t playerId, std::uint32_t requestId, const std::string& code, const std::string& message)
{
    game::simulation::ClientTradeResult r;
    r.requestId = requestId;
    r.code = code;
    r.message = message;
    std::cerr << "[Trade] player " << playerId << " request " << requestId << ": REFUSED (" << code << ") " << message << '\n';
    pushResult(playerId, std::move(r));
}

game::simulation::ClientTradeResult EconomyServer::trade(const TradeOrder& o)
{
    game::simulation::ClientTradeResult r;
    r.requestId = o.requestId;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(o.playerId, o.requestId, code, message);
        r.code = code;
        r.message = message;
        r.day = m_day;
        return r;
    };
    if (!m_loaded)
        return refuse("off", "экономика не загружена");
    ClientLedger* l = ledger(o.playerId);
    if (!l)
        return refuse("no_account", "нет счёта");
    if (o.black)
        return sellBlack(o);
    const int g = goodIndex(o.goodId);
    if (g >= 0)
    {
        const std::string legal = legalityFor(o.playerId, g, o.hubId, o.systemId);
        const std::string& ru = EconomyModel::instance().goods()[static_cast<std::size_t>(g)].ru;
        if (legal == "banned")
            return refuse("contraband", "«" + ru + "» здесь запрещён: только из-под полы (F8 ГРУЗ -> чёрный рынок)");
        if (legal == "restricted")
        {
            std::string licRu = licenceForGood(o.goodId);
            for (const auto& t : m_licenceTypes)
                if (t.id == licRu)
                    licRu = t.ru;
            return refuse("licence", "«" + ru + "» здесь только по лицензии «" + licRu + "» (F5 -> Лицензии)");
        }
    }
    if (g < 0 && !o.buy)
        if (const CargoManifest* held = m_cargo.find(o.shipKey))
            for (const auto& lot : held->lots)
                if (lot.goodId == o.goodId && !lot.contractId.empty())
                    return refuse("contract_cargo", "груз контракта " + lot.contractId + " принадлежит заказчику: продать его нельзя");
    if (g < 0)
        return refuse("no_good", "неизвестный товар " + o.goodId);
    const auto& good = EconomyModel::instance().goods()[static_cast<std::size_t>(g)];
    // the issuer's goods are never the player's to sell (TRADE_DESIGN §3.4: that would be appropriation)
    if (!o.buy)
        if (const CargoManifest* held = m_cargo.find(o.shipKey))
        {
            double contractT = 0.0;
            for (const auto& lot : held->lots)
                if (lot.goodId == o.goodId && !lot.contractId.empty())
                    contractT += lot.tonnes;
            if (contractT > 0.0 && held->ownTonnesOf(o.goodId) + 1.0e-6 < o.tonnes)
                return refuse("contract_cargo", "груз контракта (" + grouped(contractT, 2) + " т «" + good.ru +
                                                    "») принадлежит заказчику: продать его нельзя");
        }
    const auto mit = m_marketIndex.find(o.hubId);
    if (mit == m_marketIndex.end() || !m_markets[mit->second].cells[static_cast<std::size_t>(g)].traded)
        return refuse("no_market", "на этом хабе нет рынка товара «" + good.ru + "»");
    if (o.quoteDay >= 0 && o.quoteDay != m_day)
        return refuse("requote", "сутки сменились (котировка дня " + std::to_string(o.quoteDay) + ", сейчас " + std::to_string(m_day) +
                                     "): цена пересчитана, повторите");
    auto& market = m_markets[mit->second];
    auto& cell = market.cells[static_cast<std::size_t>(g)];
    std::string regimeNote;
    const MarketSide side = regimeSide(market, g, o.playerId, &regimeNote);
    if (o.buy && side.giveCapT >= 0.0 && side.giveCapT + 1.0e-6 < o.tonnes)
        return refuse("regime", regimeNote.empty() ? "нет в продаже" : regimeNote);
    const FiscalStyle style = fiscalStyleOf(o.hubId, o.systemId);
    // a sale pays the import duty on the tonnes bought in another polity (the lots unload() takes, oldest first)
    double dutyShare = 0.0;
    if (!o.buy)
        if (const CargoManifest* held = m_cargo.find(o.shipKey))
            dutyShare = CargoRegistry::fifoDutyShare(*held, o.goodId, o.tonnes,
                                                     [&](const CargoLot& lot) { return importDutyRate(lot.originHub, o.hubId, o.systemId); });
    const DealQuote q = explainDeal(o.buy, side, o.tonnes, style, m_dealRules, dutyShare);
    if (!q.ok)
        return refuse(q.code, q.reason);
    if (o.limitUnitCr > 0.0 && (o.buy ? q.unitCr > o.limitUnitCr * (1.0 + 1.0e-9) : q.unitCr < o.limitUnitCr * (1.0 - 1.0e-9)))
        return refuse("price_moved", "цена изменилась: " + grouped(q.unitCr) + " Cr/т вместо " + grouped(o.limitUnitCr) + ", повторить?");
    const std::string bank = bankOf(o.hubId, o.systemId);
    char memo[200];
    double basisOut = 0.0;
    if (o.buy)
    {
        CargoLot lot;
        lot.goodId = o.goodId;
        lot.tonnes = o.tonnes;
        lot.owner = "player:" + std::to_string(o.playerId);
        lot.originHub = o.hubId;
        lot.loadedDay = m_day;
        const CargoManifest& m = m_cargo.manifest(o.shipKey, o.playerId);
        const std::string fit = CargoRegistry::fits(m, o.capacity, o.tonnes, CargoRegistry::extraC20(m, lot, good.densityTM3));
        if (!fit.empty())
            return refuse("no_space", fit);
        std::snprintf(memo, sizeof memo, "покупка %.2f т «%s» на %s", o.tonnes, good.ru.c_str(), market.name.c_str());
        const PaymentResult p = pay(*l, m_banks, bank, -q.totalCr, Purpose::Trade, m_day, false, memo);
        if (!p.ok)
            return refuse("no_money", "недостаточно средств: нужно " + grouped(-q.totalCr) + " Cr (" + p.reason + ")");
        cell.stockT = std::max(0.0, cell.stockT - o.tonnes);
        recordRation(market, g, o.playerId, o.tonnes);
        if (q.vatReclaimCr > 0.0 && style.vatRefundDays > 0)
            m_vatRefunds.push_back({o.playerId, bank, q.vatReclaimCr, m_day + style.vatRefundDays});
        lot.costBasisCr = -q.totalCr - (style.vatRefundDays > 0 ? q.vatReclaimCr * (1.0 - m_dealRules.vatRefundFeeShare) : 0.0);
        if (TaxBook* t = taxBook(o.playerId))
            addExpense(*t, lot.costBasisCr);   // net of the input VAT that comes back
        m_cargo.load(o.shipKey, o.playerId, lot, good.densityTM3);
        r.message = std::string("куплено ") + grouped(o.tonnes, 2) + " т по " + grouped(q.unitCr) + " Cr/т";
    }
    else
    {
        const CargoManifest* m = m_cargo.find(o.shipKey);
        const double aboard = m ? m->ownTonnesOf(o.goodId) : 0.0;
        if (aboard + 1.0e-6 < o.tonnes)
            return refuse("not_aboard", "на борту только " + grouped(aboard, 2) + " т «" + good.ru + "»");
        std::snprintf(memo, sizeof memo, "продажа %.2f т «%s» на %s", o.tonnes, good.ru.c_str(), market.name.c_str());
        if (q.totalCr < 0.0)   // the fees exceed the revenue (cheap bulk): the difference is paid first, all or nothing
        {
            const PaymentResult p = pay(*l, m_banks, bank, -q.totalCr, Purpose::Trade, m_day, false, memo);
            if (!p.ok)
                return refuse("no_money", "сборы больше выручки, а денег нет: нужно " + grouped(-q.totalCr) + " Cr");
        }
        (void)m_cargo.unload(o.shipKey, o.goodId, o.tonnes, good.densityTM3, basisOut);
        if (q.totalCr > 0.0)
            receive(*l, m_banks, bank, q.totalCr, m_day, memo);
        if (TaxBook* t = taxBook(o.playerId))
            q.totalCr > 0.0 ? addRevenue(*t, q.totalCr) : addExpense(*t, -q.totalCr);   // the proceeds after the deal's fees
        cell.stockT += o.tonnes;
        r.message = std::string("продано ") + grouped(o.tonnes, 2) + " т по " + grouped(q.unitCr) + " Cr/т; к себестоимости " +
                    (q.totalCr - basisOut >= 0.0 ? "+" : "") + grouped(q.totalCr - basisOut) + " Cr";
    }
    r.ok = true;
    r.totalCr = q.totalCr;
    for (const auto& line : q.lines)
        r.lines.push_back(line.label + "|" + (line.cr >= 0.0 ? "+" : "") + grouped(line.cr));
    std::snprintf(memo, sizeof memo, "%s %.2f т «%s» на %s: %s%s Cr", o.buy ? "покупка" : "продажа", o.tonnes, good.ru.c_str(),
                  market.name.c_str(), q.totalCr >= 0.0 ? "+" : "", grouped(q.totalCr).c_str());
    logEvent(*l, m_day, memo);
    if (q.dutyCr > 0.0)   // the duty is inside the deal's total; its own ledger line names it (F8 ФИНАНСЫ)
    {
        std::snprintf(memo, sizeof memo, "пошлина -%s Cr (%.1f %% ввоз «%s» на %s)", grouped(q.dutyCr).c_str(), q.dutyShare * 100.0,
                      good.ru.c_str(), market.name.c_str());
        logEvent(*l, m_day, memo);
    }
    std::cerr << "[Trade] player " << o.playerId << " request " << o.requestId << ": " << (o.buy ? "BUY " : "SELL ") << o.tonnes << " t "
              << o.goodId << " at " << o.hubId << " (" << style.id << "): list " << q.listUnitCr << " -> deal " << q.unitCr
              << " Cr/t (impact " << 100.0 * q.impactShare << " %), goods " << q.goodsCr << ", handling " << q.handlingCr << ", VAT "
              << q.vatCr << ", duty " << q.dutyCr << " (" << 100.0 * q.dutyShare << " %), turnover " << q.turnoverCr << ", account " << (q.totalCr >= 0.0 ? "+" : "") << q.totalCr
              << " Cr; stock " << (o.buy ? cell.stockT + o.tonnes : cell.stockT - o.tonnes) << " -> " << cell.stockT << " t; net worth "
              << netWorthCr(*l, m_banks) << " Cr" << '\n';
    pushResult(o.playerId, r);
    // a deal moves money and goods: it must not wait for the daily save (a crash would lose it - TRADE_DESIGN §7.6; the
    // JSONL money journal of step 17 replaces this full write)
    saveNow("the deal");
    r.day = m_day;
    return r;
}

std::string EconomyServer::devAddCargo(std::uint64_t playerId, std::uint64_t shipKey, const std::string& goodId, double tonnes,
                                       const CargoCapacity& capacity, const std::string& hubId)
{
    const int g = goodIndex(goodId);
    if (g < 0 || !(tonnes > 0.0))
        return "неизвестный товар или нулевая масса";
    const auto& good = EconomyModel::instance().goods()[static_cast<std::size_t>(g)];
    CargoLot lot;
    lot.goodId = goodId;
    lot.tonnes = tonnes;
    lot.owner = "player:" + std::to_string(playerId);
    lot.originHub = hubId.empty() ? std::string("dev") : hubId;
    lot.loadedDay = m_day;
    const CargoManifest& m = m_cargo.manifest(shipKey, playerId);
    const std::string fit = CargoRegistry::fits(m, capacity, tonnes, CargoRegistry::extraC20(m, lot, good.densityTM3));
    if (!fit.empty())
        return fit;
    m_cargo.load(shipKey, playerId, lot, good.densityTM3);
    ++m_tradeSerial;
    saveNow("DEV cargo add");
    return {};
}

double EconomyServer::devSpendAll(std::uint64_t playerId, const std::string& hubId, int systemId)
{
    ClientLedger* l = ledger(playerId);
    if (!l)
        return 0.0;
    const std::string bank = bankOf(hubId, systemId);
    const double all = spendableCr(*l, m_banks, bank);
    const PaymentResult r = pay(*l, m_banks, bank, all, Purpose::Trade, m_day, false, "DEV: всё потрачено");
    saveNow("DEV spend all");
    return r.ok ? all : 0.0;
}

void EconomyServer::devClearCargo(std::uint64_t shipKey)
{
    m_cargo.clear(shipKey);
    ++m_tradeSerial;
    saveNow("DEV cargo clear");
}

void EconomyServer::payDueVatRefunds(int day)
{
    for (auto it = m_vatRefunds.begin(); it != m_vatRefunds.end();)
    {
        if (it->dueDay > day)
        {
            ++it;
            continue;
        }
        if (ClientLedger* l = ledger(it->playerId))
        {
            const double back = it->amountCr * (1.0 - m_dealRules.vatRefundFeeShare);
            receive(*l, m_banks, it->bankId, back, day, "возврат входного НДС (минус 1 % сбор)");
            std::cerr << "[Trade] player " << it->playerId << ": input VAT refund +" << grouped(back, 2) << " Cr" << '\n';
        }
        it = m_vatRefunds.erase(it);
        ++m_tradeSerial;
    }
}

game::simulation::ClientTradeSnapshot EconomyServer::tradeSnapshot(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId,
                                                                   int systemId, bool docked) const
{
    game::simulation::ClientTradeSnapshot s;
    if (!m_loaded)
        return s;
    s.valid = true;
    s.day = m_day;
    s.serial = m_cargo.serial() * 1000003ull + m_tradeSerial;
    s.hubId = hubId;
    s.docked = docked;
    s.kappa = m_dealRules.kappa;
    s.stockExponent = m_dealRules.stockExponent;
    s.minStockShare = m_dealRules.minStockShare;
    s.hubBuyCapShare = m_dealRules.hubBuyCapShare;
    if (const ClientLedger* l = ledger(playerId))
        s.spendableCr = spendableCr(*l, m_banks, bankOf(hubId, systemId));
    if (const auto it = m_marketIndex.find(hubId); it != m_marketIndex.end())
    {
        const auto& m = m_markets[it->second];
        const FiscalStyle style = fiscalStyleOf(hubId, systemId);
        s.hubName = m.name;
        s.spread = m.spread;
        s.fiscalStyleRu = style.ru;
        s.vatShare = style.vatShare;
        s.vatRefundDays = style.vatRefundDays;
        s.turnoverShare = style.turnoverShare;
        s.turnoverRu = style.turnoverRu;
        s.handlingCrPerT = m_dealRules.handlingCrPerT * style.portIndex;
        for (std::size_t g = 0; g < m.cells.size(); ++g)
        {
            const auto& c = m.cells[g];
            if (!c.traded)
                continue;
            std::string note;
            const MarketSide side = regimeSide(m, static_cast<int>(g), playerId, &note);
            s.market.push_back({static_cast<int>(g), true, side.priceCr, bidCr(side), askCr(side), c.stockT, c.targetT,
                                hubTakesUpToT(side, m_dealRules), hubGivesUpToT(side, m_dealRules),
                                c.anchorCr > 0.0 ? side.priceCr / c.anchorCr - 1.0 : 0.0, side.fixedPriceCr, side.giveCapT, note});
        }
    }
    else
        s.marketNote = hubId.empty() ? "рынок виден у хаба (в его зоне)" : "у этого хаба нет своего рынка";
    if (const CargoManifest* m = m_cargo.find(shipKey))
    {
        for (const auto& lot : m->lots)
            s.cargo.push_back({lot.lotId, goodIndex(lot.goodId), lot.tonnes, lot.c20, lot.tonnes > 0.0 ? lot.costBasisCr / lot.tonnes : 0.0,
                               lot.originHub, lot.loadedDay, lot.owner, lot.contractId,
                               hubId.empty() ? 0.0 : importDutyRate(lot.originHub, hubId, systemId),
                               hubId.empty() ? std::string("legal") : legalityFor(playerId, goodIndex(lot.goodId), hubId, systemId),
                               hubId.empty() ? 0.0 : blackBidCrPerT(playerId, goodIndex(lot.goodId), hubId, systemId)});
        s.cargoNetT = m->netT();
        s.cargoGrossT = m->grossT();
        s.c20Used = m->c20();
    }
    for (const auto& v : m_vatRefunds)
        if (v.playerId == playerId)
            s.vatReclaimCr += v.amountCr;
    for (const auto& pol : m_cargoPolicies)
        if (pol.shipKey == shipKey && pol.endDay >= m_day)
        {
            s.insuredCr = pol.insuredCr;
            s.insuredUntilDay = pol.endDay;
        }
    if (!hubId.empty())
    {
        const InsuranceQuote q = quoteCargoInsurance(playerId, shipKey, hubId, systemId);
        s.insuranceQuoteCr = q.why.empty() ? q.premiumCr : 0.0;
        s.insuranceValueCr = q.why.empty() ? q.valueCr : 0.0;
    }
    if (const auto it = m_results.find(playerId); it != m_results.end())
        s.results = it->second;
    // the contract board of this hub (shared) and the player's contracts (trade step 5)
    s.dayNow = std::clamp(m_clockDays, static_cast<double>(m_day), static_cast<double>(m_day) + 0.999);
    s.boardHubId = hubId;
    if (!hubId.empty())
        for (const auto& p : m_board.postings())
            if (p.originHub == hubId)
                s.board.push_back(contractRow(p));
    for (const auto& c : m_board.contracts())
    {
        if (c.playerId != playerId)
            continue;
        auto row = contractRow(c.p);
        row.status = c.status;
        row.closedDay = c.closedDay;
        row.paidCr = c.paidCr;
        row.penaltyPaidCr = c.penaltyCr;
        row.lateDays = c.status == "active" ? std::max(0, m_day - c.p.deadlineDay) : c.lateDays;
        row.aboard = m_cargo.contractTonnesOf(shipKey, c.p.id) > 0.0;
        s.contracts.push_back(std::move(row));
    }
    return s;
}

game::simulation::ClientContractRow EconomyServer::contractRow(const ContractPosting& p) const
{
    game::simulation::ClientContractRow r;
    r.id = p.id;
    r.kind = p.kind;
    r.originHub = p.originHub;
    r.originName = hubDisplayName(p.originHub);
    r.destinationHub = p.destinationHub;
    r.destinationName = hubDisplayName(p.destinationHub);
    r.issuerName = p.issuerName;
    r.goodId = p.goodId;
    r.goodRu = p.goodRu;
    r.cause = p.cause;
    r.tonnes = p.tonnes;
    r.people = p.people;
    r.c20 = p.c20;
    r.rewardCr = p.rewardCr;
    r.advanceCr = p.advanceCr;
    r.depositCr = p.depositCr;
    r.penaltyCr = p.penaltyCr;
    r.lateSharePerDay = p.lateSharePerDay;
    r.postedDay = p.postedDay;
    r.expiresDay = p.expiresDay;
    r.deadlineDay = p.deadlineDay;
    r.status = p.status;
    r.closedDay = p.closedDay;
    return r;
}

// ---------------------------------------------------------------- replication
game::simulation::ClientWalletSnapshot EconomyServer::walletSnapshot(std::uint64_t playerId, const std::string& hubId, int systemId) const
{
    game::simulation::ClientWalletSnapshot w;
    const ClientLedger* l = ledger(playerId);
    if (!l)
        return w;
    w.valid = true;
    w.day = m_day;
    w.homeBank = l->homeBank;
    w.standing = standingName(l->standing);
    w.standingRu = standingRu(l->standing);
    const auto codeOf = [&](const std::string& bank) {
        const auto* b = m_banks.bank(bank);
        return b ? b->code : std::string("CR");
    };
    for (const auto& [bank, minor] : l->cash)
    {
        game::simulation::ClientMoneyLine c;
        c.bankId = bank;
        c.label = "наличные";
        c.currency = codeOf(bank);
        c.amount = toUnits(minor);
        c.amountCr = c.amount / m_banks.fx(bank);
        w.cash.push_back(c);
    }
    for (const auto& a : l->accounts)
    {
        const auto* b = m_banks.bank(a.bankId);
        game::simulation::ClientMoneyLine c;
        c.bankId = a.bankId;
        c.label = b ? b->name : a.bankId;
        c.currency = codeOf(a.bankId);
        c.amount = toUnits(a.balance);
        c.amountCr = c.amount / m_banks.fx(a.bankId);
        c.limit = toUnits(a.overdraftLimit);
        c.annualRate = b ? (a.balance >= 0 ? b->depositRate : b->overdraftRate) : 0.0;
        c.state = a.overdueDays > 0 ? "сверх лимита " + std::to_string(a.overdueDays) + " сут" : a.frozen ? "овердрафт заморожен" : "";
        w.accounts.push_back(c);
    }
    for (const auto& loan : l->loans)
    {
        const auto* b = m_banks.bank(loan.bankId);
        game::simulation::ClientMoneyLine c;
        c.bankId = loan.bankId;
        c.label = std::string(loanKindRu(loan.kind)) + (b ? " (" + b->name + ")" : "");
        c.currency = codeOf(loan.bankId);
        c.amount = toUnits(loan.outstanding);
        c.amountCr = c.amount / m_banks.fx(loan.bankId);
        c.limit = toUnits(loan.installment);
        c.annualRate = loan.annualRate;
        if (loan.hardship)
            c.state = "нет дохода: проценты заморожены";
        else if (loan.kind == LoanKind::Lifeline && m_day < loan.startDay + m_banks.rules.lifelineFreeDays)
            c.state = "без процентов до дня " + std::to_string(loan.startDay + m_banks.rules.lifelineFreeDays);
        else if (loan.overdueDays > 0)
            c.state = "просрочка " + std::to_string(loan.overdueDays) + " сут";
        w.loans.push_back(c);
    }
    w.netWorthCr = netWorthCr(*l, m_banks);
    w.lifelineLeftCr = lifelineLeftCr(*l, m_banks, m_day);
    w.freeFuelListCr = l->freeFuelListCr;
    w.heldCr = heldCr(*l, m_banks);
    // the garnish of income by a restructured debt and the protected minimum (TRADE_DESIGN §1.3)
    for (const auto& loan : l->loans)
        if (loan.kind == LoanKind::Restructured)
            w.garnishDebtCr += toUnits(loan.outstanding) / m_banks.fx(loan.bankId);
    w.lastWithheldCr = l->lastWithheldCr;
    w.lastWithheldDay = l->lastWithheldDay;
    w.protectedCr = m_banks.rules.garnishProtectedCr;
    // licences (step 11): held, offered here, the pilot licence categories
    {
        const std::string here = polityOf(hubId, systemId);
        const auto* hereBank = m_banks.bank(here);
        w.licencePolity = hereBank ? hereBank->polityName : here;
        const auto typeRu = [&](const std::string& id) {
            for (const auto& t : m_licenceTypes)
                if (t.id == id)
                    return t.ru;
            return id;
        };
        if (const auto it = m_licences.find(playerId); it != m_licences.end())
            for (const auto& h : it->second)
            {
                const auto* b = m_banks.bank(h.polity);
                w.licences.push_back({h.type, typeRu(h.type), b ? b->polityName : h.polity, h.expiresDay, h.expiresDay > m_day});
            }
        const double portIndex = fiscalStyleOf(hubId, systemId).portIndex;
        for (const auto& t : m_licenceTypes)
        {
            std::string goodsRu;
            for (const auto& gid : t.goods)
                if (const int g = goodIndex(gid); g >= 0)
                    goodsRu += (goodsRu.empty() ? "" : ", ") + EconomyModel::instance().goods()[static_cast<std::size_t>(g)].ru;
            w.licenceOffers.push_back({t.id, t.ru, std::round(t.feeCr * portIndex * 100.0) / 100.0, goodsRu, hasLicence(playerId, here, t.id)});
        }
        for (const auto& c : m_pilotCategories)
            w.pilotCategories.push_back({c.ru, c.control, c.start, c.note});
    }
    // mining concessions (step 16 part 2)
    for (const auto& o : siteOffersAt(hubId))
    {
        const int g = goodIndex(o.goodId);
        w.siteOffers.push_back({o.id, o.nameRu, g >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(g)].ru : o.goodId, o.radiusM, o.grade,
                                o.reserveT, o.rentYearCr, o.leaseCostCr});
    }
    for (const auto& c : m_concessions)
        if (c.owner == playerId)
        {
            const int g = goodIndex(c.site.goodId);
            game::simulation::ClientMine cm;
            cm.id = c.site.id;
            cm.nameRu = c.site.nameRu;
            cm.hubName = hubDisplayName(c.site.hubId);
            cm.goodRu = g >= 0 ? EconomyModel::instance().goods()[static_cast<std::size_t>(g)].ru : c.site.goodId;
            cm.statusRu = c.status == "leased" ? "арендован: нужна добычная установка" : c.status == "mining" ? "добыча" : c.status == "depleted" ? "выработан" : "отозван";
            cm.reserveT = c.site.reserveT;
            cm.reserveLeftT = c.reserveLeftT;
            cm.lastOutputT = c.lastOutputT;
            cm.minedT = c.minedT;
            cm.soldT = c.soldT;
            cm.revenueCr = c.revenueCr;
            cm.rentYearCr = c.site.rentYearCr;
            cm.stockT = mineStockT(c.site.id, c.site.goodId);
            cm.sellLocal = c.sellLocal;
            cm.here = c.site.hubId == hubId;
            w.mines.push_back(cm);
        }
    // construction (step 16)
    for (const auto& o : buildOffersAt(hubId, playerId))
        w.buildOffers.push_back({o.id, std::string(buildKindRu(o.kind)) + " (" + o.projectId + ")", o.contractorRu, o.priceCr, o.workDays, o.reliability});
    for (const auto& p : m_buildProjects)
        if (p.owner == playerId)
        {
            const double prog = buildProgress(p, m_day);
            game::simulation::ClientBuildProject cp;
            cp.id = p.offer.id;
            cp.hubName = hubDisplayName(p.offer.hubId);
            cp.projectRu = buildKindRu(p.offer.kind);
            cp.contractorRu = p.offer.contractorRu;
            cp.statusRu = p.status == "building" ? "строится" : p.status == "halted" ? "остановлена: нет оплаты" : p.status == "done" ? "сдан, приносит доход" : "брошена";
            cp.progress = prog;
            cp.stage = std::min(4, 1 + static_cast<int>(prog * 4.0));
            cp.priceCr = p.offer.priceCr;
            cp.paidCr = p.paidCr;
            cp.incomeCrPerDay = p.status == "done" && p.offer.projectId.rfind("site:", 0) != 0 ? p.offer.priceCr * (m_buildIncomeYear - m_buildMaintYear) / 360.0 : 0.0;
            w.buildProjects.push_back(cp);
        }
    // bot pilots (step 15): this hub's agency and the owner's bots (reports already delivered)
    for (const auto& c : botCandidatesAt(hubId, systemId))
        w.botCandidates.push_back({c.id, c.name, c.skillShown, c.yearsShown, c.wageCr});
    for (const auto& b : m_bots)
        if (b.owner == playerId)
        {
            game::simulation::ClientBot cb;
            cb.id = b.id;
            cb.name = b.name;
            cb.skillShown = b.skillShown;
            cb.wageCr = b.wageCr;
            cb.shipKey = b.shipKey;
            cb.shipRu = b.shipRu;
            cb.stateRu = b.state == "idle" ? "ждёт приказа" : b.state == "at_a" || b.state == "at_b" ? "у хаба: сделка"
                         : b.state == "quit" ? "ушёл" : "в пути, " + std::to_string(b.legDaysLeft) + " сут";
            cb.loopRu = b.hubB.empty() ? std::string() : hubDisplayName(b.hubA) + " <-> " + hubDisplayName(b.hubB) + ", " + b.goodId;
            cb.trips = b.trips;
            cb.profitCr = b.profitCr;
            cb.unpaidDays = b.unpaidDays;
            for (const auto& [visible, text] : b.reports)
                if (visible <= m_day)
                    cb.reports.push_back("день " + std::to_string(visible) + ": " + text);
            w.bots.push_back(cb);
        }
    // rescue (F5 МЧС)
    for (const auto& c : sosCallsIn(systemId))
    {
        const auto* h = EconomyModel::instance().hub(c.hubId);
        w.sos.push_back({c.id, c.kindRu, c.hubId, h ? h->name : c.hubId, c.rewardCr, c.expiresDay, sosStatus(c),
                         m_sosState.count(c.id) && m_sosState.at(c.id).first == playerId});
    }
    if (const auto it = m_activeSos.find(playerId); it != m_activeSos.end())
        w.activeSosId = it->second;
    w.sarLicence = hasLicence(playerId, polityOf(hubId, systemId), "sar");
    // the employment service (F5)
    for (const auto& v : vacanciesAt(hubId, systemId))
    {
        game::simulation::ClientVacancy cv;
        cv.id = v.id;
        cv.kind = v.kind;
        cv.employerRu = v.employerRu;
        cv.roleRu = v.roleRu;
        cv.shipRu = v.passportId;
        cv.requirementRu = v.requirementRu;
        cv.wageCr = v.wageCr;
        cv.termDays = v.termDays;
        cv.whyNotRu = vacancyRefusal(playerId, v, hubId, systemId);
        cv.eligible = cv.whyNotRu.empty();
        w.vacancies.push_back(cv);
    }
    if (const Employment* e = employment(playerId))
    {
        w.employed = true;
        w.jobKind = e->job.kind;
        w.jobEmployerRu = e->job.employerRu;
        w.jobRoleRu = e->job.roleRu;
        w.jobShipRu = e->job.passportId;
        w.jobUnitRu = e->job.unitRu;
        w.jobRankRu = e->job.rankRu;
        w.jobWageCr = e->job.wageCr;
        w.jobSinceDay = e->sinceDay;
        w.jobTermEndDay = e->job.termDays > 0 ? e->sinceDay + e->job.termDays : -1;
    }
    // the income tax of the residence polity (TaxModel, TRADE_DESIGN step 7)
    {
        const FiscalStyle rs = residenceStyle(*l);
        const auto* home = m_banks.bank(l->homeBank);
        w.taxResidence = home ? home->polityName : l->homeBank;
        w.taxStyleRu = rs.ru;
        w.taxRate = rs.incomeTaxShare;
        w.taxAdminFeeShare = rs.taxAdminFeeShare;
        w.taxPeriodDays = m_taxRules.periodDays;
        w.taxDeclareDays = m_taxRules.declareDays;
        w.taxWithholdingShare = m_taxRules.withholdingShare;
        if (const auto it = m_taxBooks.find(playerId); it != m_taxBooks.end())
        {
            const TaxBook& t = it->second;
            const int period = t.period >= 0 ? t.period : taxPeriodOf(m_day, m_taxRules);
            w.taxPeriodStartDay = period * m_taxRules.periodDays;
            w.taxRevenueCr = t.revenueCr;
            w.taxExpensesCr = t.expensesCr;
            w.taxWithheldCreditCr = t.withheldCreditCr;
            w.taxWithheldLostCr = t.withheldLostCr;
            w.taxEstimateCr = estimatedTaxCr(t, rs.incomeTaxShare, rs.taxAdminFeeShare);
            w.taxAssessedPeriodStartDay = t.assessedPeriod >= 0 ? t.assessedPeriod * m_taxRules.periodDays : -1;
            w.taxAssessedProfitCr = t.assessedProfitCr;
            w.taxAssessedCr = t.assessedTaxCr;
            w.taxAssessedFeeCr = t.assessedFeeCr;
            w.taxDueDay = t.dueDay;
            w.taxSettled = t.settled;
            w.taxPaidCr = t.paidCr;
            w.taxDebtCr = t.debtCr;
            w.taxPenaltyCr = t.penaltyCr;
        }
    }

    w.localPolity = polityOf(hubId, systemId);
    if (const auto* b = m_banks.bankForPolity(w.localPolity))
    {
        w.localBank = b->name;
        w.localCurrency = b->code;
        w.localFxPerCr = m_banks.fx(b->id);
        if (m_banks.bank(w.localPolity))
            w.localPolity = b->polityName;
    }

    const FuelQuote q = quoteFuel(hubId, systemId, FuelKind::JumpCharge);
    w.fuelFree = m_fuelFree;
    if (q.available)
    {
        w.fuelQuoteValid = true;
        w.fuelHubName = q.hubName + (q.systemReference ? " (цена системы)" : "");
        w.jumpChargeListPerT = q.listLocalPerT;
        w.jumpChargeChargedPerT = q.chargedLocalPerT;
        w.jumpChargeListCrPerT = q.listCrPerT;
        w.jumpChargeStockT = q.stockT;
        const auto& b = q.breakdown;
        const auto row = [&](const char* label, double v) { w.fuelBreakdown.push_back(std::string(label) + " " + grouped(v) + " Cr/т"); };
        row("сырьё (изотопное сырьё)", b.feedstockCr);
        if (b.conversionCr > 0.5)
            row("конверсия", b.conversionCr);
        row("разделение (энергия)", b.separationEnergyCr);
        row("изготовление заряда", b.fabricationCr);
        row("доставка от завода", b.transportCr);
        row(b.locationCr >= 0.0 ? "надбавка места (удалённость, риск)" : "скидка места (экспорт, свой завод)", b.locationCr);
        row(b.scarcityCr >= 0.0 ? "дефицит запаса" : "избыток запаса", b.scarcityCr);
        row("маржа станции", b.stationMarginCr);
    }
    const FuelQuote rf = quoteFuel(hubId, systemId, FuelKind::ReactorFuel);
    if (rf.available)
        w.reactorFuelListPerKg = rf.listLocalPerT / 1000.0;

    const std::size_t from = l->events.size() > 6 ? l->events.size() - 6 : 0;
    for (std::size_t i = from; i < l->events.size(); ++i)
        w.notices.push_back("день " + std::to_string(l->events[i].day) + ": " + l->events[i].text);
    return w;
}

// ---------------------------------------------------------------- persistence
namespace
{
json ledgerToJson(const ClientLedger& l)
{
    json j;
    j["owner"] = l.ownerId;
    j["home"] = l.homeBank;
    j["standing"] = static_cast<int>(l.standing);
    j["next_loan"] = l.nextLoanId;
    j["last_day"] = l.lastDay;
    j["free_fuel_list_cr"] = l.freeFuelListCr;
    j["last_withheld"] = {l.lastWithheldDay, l.lastWithheldCr};   // economy_state/5
    json cash = json::object();
    for (const auto& [b, m] : l.cash)
        cash[b] = m;
    j["cash"] = cash;
    for (const auto& a : l.accounts)
        j["accounts"].push_back({{"bank", a.bankId}, {"balance", a.balance}, {"overdraft", a.overdraftLimit}, {"frozen", a.frozen},
                                 {"overdue", a.overdueDays}, {"carry", a.carry}, {"held", a.held}});
    j["next_hold"] = l.nextHoldId;
    for (const auto& h : l.holds)
        j["holds"].push_back({{"id", h.id}, {"bank", h.bankId}, {"amount", h.amount}, {"day", h.day}, {"memo", h.memo}});
    for (const auto& x : l.loans)
        j["loans"].push_back({{"id", x.id}, {"bank", x.bankId}, {"kind", static_cast<int>(x.kind)}, {"principal", x.principal},
                              {"outstanding", x.outstanding}, {"interest", x.interestCharged}, {"arrears", x.arrears},
                              {"installment", x.installment}, {"rate", x.annualRate}, {"start", x.startDay}, {"term", x.termDays},
                              {"due", x.nextDueDay}, {"overdue", x.overdueDays}, {"late_fee", x.lateFeeCharged},
                              {"hardship", x.hardship}, {"carry", x.carry}});
    for (int i = 0; i < 30; ++i)
        j["income"].push_back({l.incomeDay[i], l.incomeCr[i]});
    for (const auto& [d, cr] : l.lifelineDraws)
        j["lifeline"].push_back({d, cr});
    for (const auto& e : l.events)
        j["events"].push_back({e.day, e.text});
    return j;
}

ClientLedger ledgerFromJson(const json& j)
{
    ClientLedger l;
    l.ownerId = j.value("owner", std::uint64_t{0});
    l.homeBank = j.value("home", std::string());
    l.standing = static_cast<Standing>(j.value("standing", 0));
    l.nextLoanId = j.value("next_loan", 1);
    l.lastDay = j.value("last_day", -1);
    l.freeFuelListCr = j.value("free_fuel_list_cr", 0.0);
    if (const json w = j.value("last_withheld", json::array()); w.is_array() && w.size() == 2)
    {
        l.lastWithheldDay = w[0].get<int>();
        l.lastWithheldCr = w[1].get<double>();
    }
    // bound to locals: `.items()` of a temporary dies before the loop (the EconomyModel::load crash, 2026-11-11)
    const json cash = j.value("cash", json::object());
    for (const auto& [b, m] : cash.items())
        l.cash[b] = m.get<Minor>();
    for (const auto& a : j.value("accounts", json::array()))
    {
        Account x;
        x.bankId = a.value("bank", std::string());
        x.balance = a.value("balance", Minor{0});
        x.overdraftLimit = a.value("overdraft", Minor{0});
        x.frozen = a.value("frozen", false);
        x.overdueDays = a.value("overdue", 0);
        x.carry = a.value("carry", 0.0);
        x.held = a.value("held", Minor{0});
        l.accounts.push_back(x);
    }
    for (const auto& a : j.value("loans", json::array()))
    {
        Loan x;
        x.id = a.value("id", 0);
        x.bankId = a.value("bank", std::string());
        x.kind = static_cast<LoanKind>(a.value("kind", 0));
        x.principal = a.value("principal", Minor{0});
        x.outstanding = a.value("outstanding", Minor{0});
        x.interestCharged = a.value("interest", Minor{0});
        x.arrears = a.value("arrears", Minor{0});
        x.installment = a.value("installment", Minor{0});
        x.annualRate = a.value("rate", 0.0);
        x.startDay = a.value("start", 0);
        x.termDays = a.value("term", 360);
        x.nextDueDay = a.value("due", 0);
        x.overdueDays = a.value("overdue", 0);
        x.lateFeeCharged = a.value("late_fee", false);
        x.hardship = a.value("hardship", false);
        x.carry = a.value("carry", 0.0);
        l.loans.push_back(x);
    }
    const json income = j.value("income", json::array());
    for (std::size_t i = 0; i < 30 && i < income.size(); ++i)
    {
        l.incomeDay[i] = income[i][0].get<int>();
        l.incomeCr[i] = income[i][1].get<double>();
    }
    for (const auto& d : j.value("lifeline", json::array()))
        l.lifelineDraws.emplace_back(d[0].get<int>(), d[1].get<double>());
    for (const auto& e : j.value("events", json::array()))
        l.events.push_back({e[0].get<int>(), e[1].get<std::string>()});
    l.nextHoldId = j.value("next_hold", 1);
    for (const auto& h : j.value("holds", json::array()))
        l.holds.push_back({h.value("id", 0), h.value("bank", std::string()), h.value("amount", Minor{0}), h.value("day", 0),
                           h.value("memo", std::string())});
    return l;
}
} // namespace

std::string EconomyServer::saveState() const
{
    return finishState(saveStateParts());
}

std::string EconomyServer::finishState(StateParts parts)
{
    json markets = json::object();
    for (const auto& m : parts.markets)
    {
        json stock = json::array(), price = json::array();
        for (const auto& c : m.cells)
        {
            stock.push_back(c.traded ? json(c.stockT) : json());
            price.push_back(c.traded ? json(c.priceCr) : json());
        }
        markets[m.hubId] = {{"stock", stock}, {"price", price}};
    }
    parts.head["markets"] = std::move(markets);
    parts.head["contracts"] = ContractBoard::toJson(parts.postings, parts.contracts);
    return parts.head.dump();
}

// the server-thread part: everything but the board as a JSON tree, the board's postings / contracts copied
EconomyServer::StateParts EconomyServer::saveStateParts() const
{
    StateParts parts;
    json& j = parts.head;
    j["schema"] = "economy_state/14";   // 2: cargo manifests, input VAT refunds, holds (TRADE_DESIGN §7.6); 3: contracts (step 5);
                                       // 4: port calls (open + booked); 5: ledger last_withheld (the protected minimum);
                                       // 6: tax books (the income tax, step 7); 7: seizures (customs attention, step 9); 8: licences (step 11); 9: cargo policies (step 13); 10: employment; 11: SOS; 12: bots; 13: construction; 14: mining concessions
    j["day"] = m_day;
    // (fuel_free is a RULE - economy_rules / ELITE_FUEL_FREE - not world state: not saved; it was written and never read)
    json fx = json::object();
    for (const auto& b : m_banks.banks())
        fx[b.id] = m_banks.fx(b.id);
    j["fx"] = fx;
    // "markets" (finishState): a flat copy of the stock / price of every cell
    parts.markets.reserve(m_markets.size());
    for (const auto& m : m_markets)
    {
        auto& out = parts.markets.emplace_back();
        out.hubId = m.hubId;
        out.cells.reserve(m.cells.size());
        for (const auto& c : m.cells)
            out.cells.push_back({c.traded, c.stockT, c.priceCr});
    }
    json goods = json::array();
    for (const auto& g : EconomyModel::instance().goods())
        goods.push_back(g.id);
    j["goods_order"] = goods;
    j["ledgers"] = json::array();
    // by owner id: an unordered_map iterates differently after a restore, and the file must be deterministic
    std::vector<std::uint64_t> owners;
    for (const auto& [id, l] : m_ledgers)
        owners.push_back(id);
    std::sort(owners.begin(), owners.end());
    for (const auto id : owners)
        j["ledgers"].push_back(ledgerToJson(m_ledgers.at(id)));
    j["cargo"] = m_cargo.toJson();
    j["loss_events"] = json::array();
    for (const auto& e : m_lossEvents)
        j["loss_events"].push_back(e);
    j["next_lost_cargo"] = m_nextLostCargo;
    j["lost_cargo"] = json::array();
    for (const auto& r : m_lostCargo)
        j["lost_cargo"].push_back({{"id", r.recordId}, {"event", r.eventId}, {"ship", r.shipKey}, {"player", r.playerId},
                                   {"lot", CargoRegistry::lotToJson(r.lot)}, {"state", r.state}, {"legal", r.legal},
                                   {"rights", r.rightsHolder}, {"claim", r.claim}, {"system", r.systemId},
                                   {"pos", {r.worldM[0], r.worldM[1], r.worldM[2]}}, {"t", r.universeS}, {"day", r.day},
                                   {"cause", r.cause}, {"insurer", r.insurer}, {"payout", r.payoutCr}});
    j["build"] = json::array();
    for (const auto& p : m_buildProjects)
    {
        json halts = json::array();
        for (const auto& [a, b] : p.halts)
            halts.push_back({a, b});
        j["build"].push_back({{"id", p.offer.id}, {"hub", p.offer.hubId}, {"project", p.offer.projectId}, {"kind", p.offer.kind},
                              {"contractor", p.offer.contractorRu}, {"price", p.offer.priceCr}, {"days", p.offer.workDays}, {"r", p.offer.reliability},
                              {"owner", p.owner}, {"start", p.startDay}, {"paid_m", p.paidMilestones}, {"paid", p.paidCr},
                              {"retention", p.retentionCr}, {"halts", halts}, {"unpaid", p.unpaidSince}, {"status", p.status},
                              {"retention_paid", p.retentionPaid}});
    }
    j["mines"] = json::array();
    for (const auto& c : m_concessions)
        j["mines"].push_back({{"id", c.site.id}, {"hub", c.site.hubId}, {"name", c.site.nameRu}, {"good", c.site.goodId}, {"radius", c.site.radiusM},
                              {"grade", c.site.grade}, {"reserve", c.site.reserveT}, {"rent", c.site.rentYearCr}, {"lease", c.site.leaseCostCr},
                              {"owner", c.owner}, {"start", c.startDay}, {"status", c.status}, {"left", c.reserveLeftT}, {"mined", c.minedT},
                              {"sold", c.soldT}, {"revenue", c.revenueCr}, {"last", c.lastOutputT}, {"rig", c.rigPriceCr}, {"unpaid", c.unpaidDays}, {"sell_local", c.sellLocal}});
    j["bots"] = json::array();
    for (const auto& b : m_bots)
    {
        json reports = json::array();
        for (const auto& [d, text] : b.reports)
            reports.push_back({d, text});
        j["bots"].push_back({{"id", b.id}, {"name", b.name}, {"traits", {b.traits.skill, b.traits.discipline, b.traits.punctuality, b.traits.thrift,
                                                                            b.traits.honesty, b.traits.nerve}},
                             {"shown", b.skillShown}, {"wage", b.wageCr}, {"owner", b.owner}, {"ship", b.shipKey}, {"ship_ru", b.shipRu},
                             {"cap", {b.capacity.netT, b.capacity.c20Slots}}, {"dry", b.dryMassT}, {"state", b.state}, {"a", b.hubA},
                             {"b", b.hubB}, {"good", b.goodId}, {"system", b.systemId}, {"leg", b.legDaysLeft}, {"trips", b.trips},
                             {"unpaid", b.unpaidDays}, {"profit", b.profitCr}, {"cost", b.tripCostCr}, {"reports", reports}});
    }
    j["bot_hired"] = json::array();
    for (const auto& id : m_hiredCandidates)
        j["bot_hired"].push_back(id);
    j["sos"] = json::array();
    for (const auto& [id, st] : m_sosState)
        j["sos"].push_back({id, st.first, st.second});
    j["sos_active"] = json::array();
    {
        std::vector<std::uint64_t> ids;
        for (const auto& [p, id] : m_activeSos)
            ids.push_back(p);
        std::sort(ids.begin(), ids.end());
        for (const auto p : ids)
            j["sos_active"].push_back({p, m_activeSos.at(p)});
    }
    j["employment"] = json::array();
    {
        std::vector<std::uint64_t> ids;
        for (const auto& [id, e] : m_employment)
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        for (const auto id : ids)
        {
            const auto& e = m_employment.at(id);
            j["employment"].push_back({{"player", id}, {"id", e.job.id}, {"kind", e.job.kind}, {"employer", e.job.employerRu},
                                       {"role", e.job.roleRu}, {"bank", e.job.bankId}, {"ship", e.job.passportId}, {"unit", e.job.unitRu},
                                       {"rank", e.job.rankRu}, {"wage", e.job.wageCr}, {"term", e.job.termDays}, {"since", e.sinceDay}});
        }
    }
    j["cargo_policies"] = json::array();
    for (const auto& pol : m_cargoPolicies)
    {
        json lots = json::array();
        for (const auto& [id, v] : pol.lotValueCr)
            lots.push_back({id, v, pol.lotTonnes.count(id) ? pol.lotTonnes.at(id) : 0.0});
        j["cargo_policies"].push_back({{"player", pol.playerId}, {"ship", pol.shipKey}, {"bank", pol.bankId}, {"cr", pol.insuredCr},
                                       {"end", pol.endDay}, {"lots", lots}});
    }
    j["licences"] = json::array();
    {
        std::vector<std::uint64_t> ids;
        for (const auto& [id, v] : m_licences)
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        for (const auto id : ids)
            for (const auto& h : m_licences.at(id))
                j["licences"].push_back({id, h.type, h.polity, h.expiresDay});
    }
    j["seizures"] = json::array();
    {
        std::vector<std::uint64_t> ids;
        for (const auto& [id, v] : m_seizures)
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        for (const auto id : ids)
            for (const auto& [pol, day] : m_seizures.at(id))
                j["seizures"].push_back({id, pol, day});
    }
    j["taxes"] = json::array();
    {
        std::vector<std::uint64_t> ids;
        for (const auto& [id, t] : m_taxBooks)
            ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        for (const auto id : ids)
        {
            const TaxBook& t = m_taxBooks.at(id);
            j["taxes"].push_back({{"player", id}, {"period", t.period}, {"rev", t.revenueCr}, {"exp", t.expensesCr},
                                  {"wh_credit", t.withheldCreditCr}, {"wh_lost", t.withheldLostCr}, {"a_period", t.assessedPeriod},
                                  {"a_profit", t.assessedProfitCr}, {"a_tax", t.assessedTaxCr}, {"a_fee", t.assessedFeeCr},
                                  {"due", t.dueDay}, {"settled", t.settled}, {"paid", t.paidCr}, {"debt", t.debtCr},
                                  {"penalty", t.penaltyCr}, {"paid_total", t.paidTotalCr}});
        }
    }
    j["vat_refunds"] = json::array();
    for (const auto& v : m_vatRefunds)
        j["vat_refunds"].push_back({{"player", v.playerId}, {"bank", v.bankId}, {"cr", v.amountCr}, {"due", v.dueDay}});
    // "contracts" (finishState): the shared boards and the taken contracts (their holds live in the ledgers)
    parts.postings = m_board.postings();
    parts.contracts = m_board.contracts();
    // 4: the open port calls (shipKey order) and the booked call events (idempotency across a restart)
    json open = json::array();
    for (const auto& [ship, c] : m_openPortCalls)
        open.push_back({{"ship", ship}, {"call", c.callId}, {"hub", c.hubId}, {"dock", c.dockModuleId}, {"player", c.playerId},
                        {"since", c.sinceUniverseS}});
    json booked = json::array();
    for (const auto& id : m_portCallOrder)
        if (const auto it = m_portCalls.find(id); it != m_portCalls.end())
            booked.push_back({id, it->second});
    j["port_calls"] = {{"open", open}, {"booked", booked}};
    return parts;
}

bool EconomyServer::recordShipLoss(const ShipLoss& loss)
{
    if (!m_loaded || loss.eventId.empty() || m_lossEvents.count(loss.eventId))
        return false;   // idempotent: this loss was resolved already
    m_lossEvents.insert(loss.eventId);
    m_openPortCalls.erase(loss.shipKey);   // a ship lost at the berth: no berth fee (as before: no release event)
    std::uint64_t player = 0;
    const std::vector<CargoLot> lots = m_cargo.takeAll(loss.shipKey, player);
    // cargo insurance (step 13): the insured lots that were lost pay (1 - deductible) of their insured value; each paid
    // lot is the insurer's from now on (ECO-1)
    std::map<std::string, std::pair<std::string, double>> paidLots;   // lot id -> (insurer bank, payout)
    for (auto it = m_cargoPolicies.begin(); it != m_cargoPolicies.end();)
    {
        if (it->shipKey != loss.shipKey)
        {
            ++it;
            continue;
        }
        double payout = 0.0;
        if (it->endDay >= m_day)
            for (const auto& lot : lots)
                if (const auto v = it->lotValueCr.find(lot.lotId); v != it->lotValueCr.end())
                {
                    const double insuredT = it->lotTonnes.count(lot.lotId) ? it->lotTonnes.at(lot.lotId) : lot.tonnes;
                    const double lotPay = v->second * std::min(1.0, insuredT > 0.0 ? lot.tonnes / insuredT : 0.0);
                    payout += lotPay;
                    paidLots[lot.lotId] = {it->bankId, std::round(lotPay * (1.0 - m_deductibleShare) * 100.0) / 100.0};
                }
        payout = std::round(payout * (1.0 - m_deductibleShare) * 100.0) / 100.0;
        if (ClientLedger* pl = ledger(it->playerId); pl && payout > 0.0)
        {
            receive(*pl, m_banks, it->bankId, payout, m_day, "страховая выплата за груз (" + loss.cause + ")");
            std::cerr << "[Insurance] player " << it->playerId << ": cargo payout " << grouped(payout) << " Cr (" << loss.cause << ")" << '\n';
        }
        it = m_cargoPolicies.erase(it);
    }
    double tonnes = 0.0;
    for (const auto& lot : lots)
    {
        LostCargo r;
        r.recordId = "W" + std::to_string(m_nextLostCargo++);   // W: a wreck record (the lots are L<n> per manifest - ECO-6 id clash)
        r.eventId = loss.eventId;
        r.shipKey = loss.shipKey;
        r.playerId = player;
        r.lot = lot;
        r.rightsHolder = lot.owner;
        if (const auto paid = paidLots.find(lot.lotId); paid != paidLots.end() && paid->second.second > 0.0)
        {
            r.insurer = paid->second.first;
            r.payoutCr = paid->second.second;
            r.claim = "paid";
            r.rightsHolder = "insurer:" + r.insurer;   // subrogated
        }
        r.systemId = loss.systemId;
        for (int k = 0; k < 3; ++k)
            r.worldM[k] = loss.worldM[k];
        r.universeS = loss.universeS;
        r.day = m_day;
        r.cause = loss.cause;
        tonnes += lot.tonnes;
        m_lostCargo.push_back(std::move(r));
    }
    if (ClientLedger* l = player ? ledger(player) : nullptr)
        if (!lots.empty())
        {
            char memo[200];
            std::snprintf(memo, sizeof memo, "корабль потерян (%s): груз %.1f т остался в обломках", loss.cause.c_str(), tonnes);
            logEvent(*l, m_day, memo);
        }
    std::cerr << "[Economy] ship loss " << loss.eventId << " (" << loss.cause << "): " << lots.size() << " lot(s), " << tonnes
              << " t -> lost-cargo records (aboard_wreck), system " << loss.systemId << '\n';
    ++m_tradeSerial;
    saveNow("the ship loss");
    return true;
}

bool EconomyServer::restoreState(const std::string& text)
{
    json j;
    try
    {
        j = json::parse(text);
    }
    catch (const std::exception&)
    {
        return false;
    }
    const std::string schema = j.value("schema", std::string());
    if (schema != "economy_state/1" && schema != "economy_state/2" && schema != "economy_state/3" &&
        schema != "economy_state/4" && schema != "economy_state/5" && schema != "economy_state/6" &&
        schema != "economy_state/7" && schema != "economy_state/8" &&
        schema != "economy_state/9" && schema != "economy_state/10" &&
        schema != "economy_state/11" && schema != "economy_state/12" &&
        schema != "economy_state/13" && schema != "economy_state/14")   // /1: no trade registries; /2: no contracts;
                                                                       // /3: no port calls; /4: no last_withheld; /5: no taxes
        return false;
    // Parse EVERYTHING into temporaries first; the live state changes only after the whole file parsed (audit
    // 2026-11-19: an exception half-way left fx / markets restored, the ledgers cleared, and the caller started a
    // "fresh state" that the next save wrote over the original file - every account lost).
    try
    {
        const int day = j.value("day", m_day);
        std::vector<std::pair<std::string, double>> fxs;
        const json fxJson = j.value("fx", json::object());   // a named object: items() of a temporary would dangle
        for (const auto& [bank, v] : fxJson.items())
            fxs.emplace_back(bank, v.get<double>());
        struct CellValue
        {
            std::size_t market = 0, good = 0;
            double stockT = 0.0, priceCr = 0.0;
        };
        std::vector<CellValue> cellValues;
        std::vector<int> order;
        for (const auto& id : j.value("goods_order", json::array()))
            order.push_back(goodIndex(id.get<std::string>()));
        const json markets = j.value("markets", json::object());
        for (const auto& [hub, m] : markets.items())
        {
            const auto it = m_marketIndex.find(hub);
            if (it == m_marketIndex.end())
                continue;
            const auto& cells = m_markets[it->second].cells;
            const auto& stock = m.at("stock");
            const auto& price = m.at("price");
            for (std::size_t k = 0; k < order.size() && k < stock.size() && k < price.size(); ++k)
            {
                const int g = order[k];
                if (g < 0 || !cells[static_cast<std::size_t>(g)].traded || !stock[k].is_number() || !price[k].is_number())
                    continue;
                cellValues.push_back({it->second, static_cast<std::size_t>(g), stock[k].get<double>(), price[k].get<double>()});
            }
        }
        decltype(m_ledgers) ledgers;
        for (const auto& l : j.value("ledgers", json::array()))
        {
            ClientLedger x = ledgerFromJson(l);
            ledgers[x.ownerId] = std::move(x);
        }
        std::set<std::string> lossEvents;
        for (const auto& e : j.value("loss_events", json::array()))
            lossEvents.insert(e.get<std::string>());
        std::vector<LostCargo> lostCargo;
        for (const auto& x : j.value("lost_cargo", json::array()))
        {
            LostCargo r;
            r.recordId = x.value("id", std::string());
            r.eventId = x.value("event", std::string());
            r.shipKey = x.value("ship", std::uint64_t{0});
            r.playerId = x.value("player", std::uint64_t{0});
            r.lot = CargoRegistry::lotFromJson(x.value("lot", json::object()));
            r.state = x.value("state", std::string("aboard_wreck"));
            r.legal = x.value("legal", std::string("known_owner"));
            r.rightsHolder = x.value("rights", std::string());
            r.claim = x.value("claim", std::string("none"));
            r.systemId = x.value("system", -1);
            const json pos = x.value("pos", json::array());
            for (std::size_t k = 0; k < 3 && k < pos.size(); ++k)
                r.worldM[k] = pos[k].get<double>();
            r.universeS = x.value("t", 0.0);
            r.day = x.value("day", 0);
            r.insurer = x.value("insurer", std::string());
            r.payoutCr = x.value("payout", 0.0);
            r.cause = x.value("cause", std::string());
            lostCargo.push_back(std::move(r));
        }
        const std::uint64_t nextLostCargo = j.value("next_lost_cargo", std::uint64_t{1});
        CargoRegistry cargo;   // a dry run of the cargo parse (throws here, not in the commit)
        cargo.fromJson(j.value("cargo", json::array()));
        decltype(m_concessions) concessions;   // economy_state/14
        for (const auto& x : j.value("mines", json::array()))
        {
            Concession c;
            c.site.id = x.value("id", std::string());
            c.site.hubId = x.value("hub", std::string());
            c.site.nameRu = x.value("name", std::string());
            c.site.goodId = x.value("good", std::string("ore_common"));
            c.site.radiusM = x.value("radius", 0.0);
            c.site.grade = x.value("grade", 1.0);
            c.site.reserveT = x.value("reserve", 0.0);
            c.site.rentYearCr = x.value("rent", 0.0);
            c.site.leaseCostCr = x.value("lease", 0.0);
            c.owner = x.value("owner", std::uint64_t{0});
            c.startDay = x.value("start", 0);
            c.status = x.value("status", std::string("leased"));
            c.reserveLeftT = x.value("left", 0.0);
            c.minedT = x.value("mined", 0.0);
            c.soldT = x.value("sold", 0.0);
            c.revenueCr = x.value("revenue", 0.0);
            c.lastOutputT = x.value("last", 0.0);
            c.rigPriceCr = x.value("rig", 0.0);
            c.unpaidDays = x.value("unpaid", 0);
            c.sellLocal = x.value("sell_local", true);
            concessions.push_back(std::move(c));
        }
        decltype(m_buildProjects) buildProjects;   // economy_state/13
        for (const auto& x : j.value("build", json::array()))
        {
            BuildProject p;
            p.offer.id = x.value("id", std::string());
            p.offer.hubId = x.value("hub", std::string());
            p.offer.projectId = x.value("project", std::string());
            p.offer.kind = x.value("kind", std::string());
            p.offer.contractorRu = x.value("contractor", std::string());
            p.offer.priceCr = x.value("price", 0.0);
            p.offer.workDays = x.value("days", 1);
            p.offer.reliability = x.value("r", 0.8);
            p.owner = x.value("owner", std::uint64_t{0});
            p.startDay = x.value("start", 0);
            p.paidMilestones = x.value("paid_m", 0);
            p.paidCr = x.value("paid", 0.0);
            p.retentionCr = x.value("retention", 0.0);
            for (const auto& h : x.value("halts", json::array()))
                p.halts.push_back({h.at(0).get<int>(), h.at(1).get<int>()});
            p.unpaidSince = x.value("unpaid", -1);
            p.status = x.value("status", std::string("building"));
            p.retentionPaid = x.value("retention_paid", false);
            buildProjects.push_back(std::move(p));
        }
        decltype(m_bots) bots;   // economy_state/12
        for (const auto& x : j.value("bots", json::array()))
        {
            Bot b;
            b.id = x.value("id", std::string());
            b.name = x.value("name", std::string());
            const json tr = x.value("traits", json::array());
            if (tr.size() == 6)
                b.traits = {tr[0].get<double>(), tr[1].get<double>(), tr[2].get<double>(), tr[3].get<double>(), tr[4].get<double>(), tr[5].get<double>()};
            b.skillShown = x.value("shown", 0.0);
            b.wageCr = x.value("wage", 0.0);
            b.owner = x.value("owner", std::uint64_t{0});
            b.shipKey = x.value("ship", std::uint64_t{0});
            b.shipRu = x.value("ship_ru", std::string());
            const json cap = x.value("cap", json::array());
            if (cap.size() == 2)
                b.capacity = {cap[0].get<double>(), cap[1].get<int>()};
            b.dryMassT = x.value("dry", 0.0);
            b.state = x.value("state", std::string("idle"));
            b.hubA = x.value("a", std::string());
            b.hubB = x.value("b", std::string());
            b.goodId = x.value("good", std::string());
            b.systemId = x.value("system", -1);
            b.legDaysLeft = x.value("leg", 0);
            b.trips = x.value("trips", 0);
            b.unpaidDays = x.value("unpaid", 0);
            b.profitCr = x.value("profit", 0.0);
            b.tripCostCr = x.value("cost", 0.0);
            for (const auto& r : x.value("reports", json::array()))
                b.reports.push_back({r.at(0).get<int>(), r.at(1).get<std::string>()});
            bots.push_back(std::move(b));
        }
        decltype(m_hiredCandidates) hiredCandidates;
        for (const auto& id : j.value("bot_hired", json::array()))
            hiredCandidates.insert(id.get<std::string>());
        decltype(m_sosState) sosState;   // economy_state/11
        for (const auto& x : j.value("sos", json::array()))
            sosState[x.at(0).get<std::string>()] = {x.at(1).get<std::uint64_t>(), x.at(2).get<std::string>()};
        decltype(m_activeSos) activeSos;
        for (const auto& x : j.value("sos_active", json::array()))
            activeSos[x.at(0).get<std::uint64_t>()] = x.at(1).get<std::string>();
        decltype(m_employment) employment;   // economy_state/10
        for (const auto& x : j.value("employment", json::array()))
        {
            Employment e;
            e.job.id = x.value("id", std::string());
            e.job.kind = x.value("kind", std::string());
            e.job.employerRu = x.value("employer", std::string());
            e.job.roleRu = x.value("role", std::string());
            e.job.bankId = x.value("bank", std::string());
            e.job.passportId = x.value("ship", std::string());
            e.job.unitRu = x.value("unit", std::string());
            e.job.rankRu = x.value("rank", std::string());
            e.job.wageCr = x.value("wage", 0.0);
            e.job.termDays = x.value("term", 0);
            e.sinceDay = x.value("since", 0);
            employment[x.at("player").get<std::uint64_t>()] = e;
        }
        decltype(m_cargoPolicies) policies;   // economy_state/9
        for (const auto& x : j.value("cargo_policies", json::array()))
        {
            CargoPolicy pol;
            pol.playerId = x.value("player", std::uint64_t{0});
            pol.shipKey = x.value("ship", std::uint64_t{0});
            pol.bankId = x.value("bank", std::string());
            pol.insuredCr = x.value("cr", 0.0);
            pol.endDay = x.value("end", 0);
            for (const auto& lot : x.value("lots", json::array()))
            {
                pol.lotValueCr[lot.at(0).get<std::string>()] = lot.at(1).get<double>();
                pol.lotTonnes[lot.at(0).get<std::string>()] = lot.at(2).get<double>();
            }
            policies.push_back(std::move(pol));
        }
        decltype(m_licences) licences;   // economy_state/8
        for (const auto& x : j.value("licences", json::array()))
            licences[x.at(0).get<std::uint64_t>()].push_back({x.at(1).get<std::string>(), x.at(2).get<std::string>(), x.at(3).get<int>()});
        decltype(m_seizures) seizures;   // economy_state/7
        for (const auto& x : j.value("seizures", json::array()))
            seizures[x.at(0).get<std::uint64_t>()].push_back({x.at(1).get<std::string>(), x.at(2).get<int>()});
        decltype(m_taxBooks) taxBooks;   // economy_state/6 (older files: none - the books open on the next day)
        for (const auto& x : j.value("taxes", json::array()))
        {
            TaxBook t;
            t.period = x.value("period", -1);
            t.revenueCr = x.value("rev", 0.0);
            t.expensesCr = x.value("exp", 0.0);
            t.withheldCreditCr = x.value("wh_credit", 0.0);
            t.withheldLostCr = x.value("wh_lost", 0.0);
            t.assessedPeriod = x.value("a_period", -1);
            t.assessedProfitCr = x.value("a_profit", 0.0);
            t.assessedTaxCr = x.value("a_tax", 0.0);
            t.assessedFeeCr = x.value("a_fee", 0.0);
            t.dueDay = x.value("due", -1);
            t.settled = x.value("settled", true);
            t.paidCr = x.value("paid", 0.0);
            t.debtCr = x.value("debt", 0.0);
            t.penaltyCr = x.value("penalty", 0.0);
            t.paidTotalCr = x.value("paid_total", 0.0);
            taxBooks[x.at("player").get<std::uint64_t>()] = t;
        }
        decltype(m_vatRefunds) vatRefunds;
        for (const auto& v : j.value("vat_refunds", json::array()))
            vatRefunds.push_back({v.value("player", std::uint64_t{0}), v.value("bank", std::string()), v.value("cr", 0.0), v.value("due", 0)});

        std::vector<ContractPosting> postings;
        std::vector<Contract> contracts;
        ContractBoard::fromJson(j.value("contracts", json::object()), postings, contracts);

        decltype(m_openPortCalls) openCalls;
        decltype(m_portCalls) bookedCalls;
        decltype(m_portCallOrder) bookedOrder;
        const json portCalls = j.value("port_calls", json::object());
        for (const auto& c : portCalls.value("open", json::array()))
            openCalls[c.at("ship").get<std::uint64_t>()] = {c.value("call", std::string()), c.value("hub", std::string()),
                                                            c.value("dock", std::string()), c.value("player", std::uint64_t{0}),
                                                            c.value("since", 0.0)};
        for (const auto& b : portCalls.value("booked", json::array()))
            if (bookedCalls.emplace(b.at(0).get<std::string>(), b.at(1).get<int>()).second)
                bookedOrder.push_back(b.at(0).get<std::string>());

        // ---- commit (no parsing below this line)
        m_day = day;
        for (const auto& [bank, v] : fxs)
            m_banks.setFx(bank, v);
        for (const auto& c : cellValues)
        {
            m_markets[c.market].cells[c.good].stockT = c.stockT;
            m_markets[c.market].cells[c.good].priceCr = c.priceCr;
        }
        m_ledgers.swap(ledgers);
        m_cargo.fromJson(j.value("cargo", json::array()));   // already parsed once above (cannot throw now); keeps the registry serial
        m_vatRefunds.swap(vatRefunds);
        m_taxBooks.swap(taxBooks);
        m_seizures.swap(seizures);
        m_licences.swap(licences);
        m_cargoPolicies.swap(policies);
        m_employment.swap(employment);
        m_sosState.swap(sosState);
        m_bots.swap(bots);
        m_buildProjects.swap(buildProjects);
        m_concessions.swap(concessions);
        ++m_buildRevision;
        m_hiredCandidates.swap(hiredCandidates);
        m_activeSos.swap(activeSos);
        m_lossEvents.swap(lossEvents);
        m_lostCargo.swap(lostCargo);
        m_nextLostCargo = nextLostCargo;
        m_board.commit(std::move(postings), std::move(contracts));
        m_openPortCalls.swap(openCalls);
        m_portCalls.swap(bookedCalls);
        m_portCallOrder.swap(bookedOrder);
        ++m_tradeSerial;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[EconomyServer] state: " << e.what() << '\n';
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- contracts (TRADE_DESIGN §3, step 5)
void EconomyServer::saveNow(const char* what, bool urgent)
{
    if (m_stateFile.empty())
        return;
    m_stateDirty = true;
    m_dirtyWhat = what;
    m_urgentSave = m_urgentSave || urgent;
}

void EconomyServer::flushStateIfDue(bool force)
{
    if (!m_stateDirty || m_stateFile.empty())
        return;
    const double now = steadyS();
    if (!force && !m_urgentSave && now - m_lastSaveS < kSaveIntervalS)
        return;
    m_stateDirty = false;
    m_urgentSave = false;
    m_lastSaveS = now;
    StateParts parts = saveStateParts();
    const double serialized = steadyS();
    if (!m_writer)
        m_writer = std::make_unique<StateWriter>(m_stateFile);
    m_writer->submit(std::move(parts));
    if (saveTiming())
        std::cerr << "[EconomySave] " << m_dirtyWhat << (force ? " (final)" : "") << ": state tree + board / market copy "
                  << 1000.0 * (serialized - now) << " ms, hand-off " << 1000.0 * (steadyS() - serialized)
                  << " ms on the server thread" << '\n';
}

bool EconomyServer::takeCargoChange(std::uint64_t shipKey)
{
    return m_cargoChanged.erase(shipKey) != 0;
}

game::simulation::ClientTradeResult EconomyServer::takeContract(const ContractOrder& o)
{
    game::simulation::ClientTradeResult r;
    r.requestId = o.requestId;
    const auto refuse = [&](const std::string& code, const std::string& message) {
        recordTradeRefusal(o.playerId, o.requestId, code, message);
        r.code = code;
        r.message = message;
        r.day = m_day;
        return r;
    };
    if (!m_loaded)
        return refuse("off", "экономика не загружена");
    ClientLedger* l = ledger(o.playerId);
    if (!l)
        return refuse("no_account", "нет счёта");
    ContractPosting* p = m_board.posting(o.contractId);
    if (!p)
        return refuse("no_contract", "заявки " + o.contractId + " нет на доске");
    if (p->status != "open")
        return refuse("taken", p->status == "npc" ? "заявку уже забрал другой перевозчик" : p->status == "expired" ? "заявка снята с доски"
                                                                                                              : "заявка уже взята");
    if (o.hubId.empty() || o.hubId != p->originHub)
        return refuse("not_docked", "взять заявку можно только у причала хаба отправления " + hubDisplayName(p->originHub));
    // the hold: the cargo (a P20 crew lot takes one C20 place per module)
    CargoLot lot;
    lot.goodId = p->goodId;
    lot.tonnes = p->tonnes;
    lot.people = p->people;
    lot.owner = "issuer:" + p->issuer;
    lot.contractId = p->id;
    lot.originHub = p->originHub;
    lot.loadedDay = m_day;
    const CargoManifest& m = m_cargo.manifest(o.shipKey, o.playerId);
    const std::string fit = CargoRegistry::fits(m, o.capacity, p->tonnes, c20For(p->tonnes, p->densityTM3));
    if (!fit.empty())
        return refuse("no_space", fit);
    // the deposit: the client's own money blocked on an account (at the origin's bank if there is one, else home)
    Contract c;
    c.p = *p;
    c.playerId = o.playerId;
    c.shipKey = o.shipKey;
    c.takenDay = m_day;
    c.issuerBank = bankOf(p->destinationHub, -1);
    if (c.issuerBank.empty())
        c.issuerBank = bankOf(o.hubId, o.systemId);
    if (p->depositCr > 0.0)
    {
        const std::string local = bankOf(o.hubId, o.systemId);
        c.holdBank = findAccount(*l, local) ? local : l->homeBank;
        // (std::string, not a fixed buffer: a cut in the middle of a UTF-8 letter made the state file unwritable)
        const std::string memo = "залог по контракту " + p->id + " (" + hubDisplayName(p->originHub) + " -> " +
                                 hubDisplayName(p->destinationHub) + ")";
        c.holdId = placeHold(*l, m_banks, c.holdBank, p->depositCr, m_day, memo);
        if (c.holdId == 0)
            return refuse("no_deposit", "нечем обеспечить залог " + grouped(p->depositCr) + " Cr: нужны свободные деньги на счёте (не овердрафт)");
    }
    if (p->advanceCr > 0.0)
        receiveContractMoney(o.playerId, *l, c.issuerBank, p->advanceCr, "аванс по контракту " + p->id);
    m_cargo.load(o.shipKey, o.playerId, lot, p->densityTM3);
    p->status = "taken";
    p->takenBy = o.playerId;
    p->closedDay = m_day;
    m_board.contracts().push_back(c);
    m_board.touch();
    logEvent(*l, m_day, "контракт " + p->id + " взят: " + hubDisplayName(p->originHub) + " -> " + hubDisplayName(p->destinationHub) +
                            ", награда " + grouped(p->rewardCr) + " Cr, аванс " + grouped(p->advanceCr) + ", залог " + grouped(p->depositCr) +
                            ", срок день " + std::to_string(p->deadlineDay));
    std::cerr << "[Contract] player " << o.playerId << " TAKE " << p->id << " (" << p->kind << ", " << p->tonnes << " t " << p->goodId
              << (p->people > 0 ? ", " + std::to_string(p->people) + " people" : std::string()) << ") " << p->originHub << " -> "
              << p->destinationHub << ": reward " << p->rewardCr << ", advance " << p->advanceCr << " paid, deposit " << p->depositCr
              << " held (hold " << c.holdId << " at " << c.holdBank << "), deadline day " << p->deadlineDay << " (now " << m_day
              << "); net worth " << netWorthCr(*l, m_banks) << " Cr, held " << heldCr(*l, m_banks) << " Cr" << '\n';
    r.ok = true;
    r.code = "contract_taken";
    r.totalCr = p->advanceCr;
    r.message = "контракт взят: аванс +" + grouped(p->advanceCr) + " Cr, залог " + grouped(p->depositCr) + " Cr заблокирован, груз на борту";
    pushResult(o.playerId, r);
    saveNow("the contract was taken");
    r.day = m_day;
    return r;
}

int EconomyServer::deliverContracts(std::uint64_t playerId, std::uint64_t shipKey, const std::string& hubId, int systemId)
{
    (void)systemId;
    if (!m_loaded || hubId.empty())
        return 0;
    ClientLedger* l = ledger(playerId);
    if (!l)
        return 0;
    const auto& rules = m_board.rules();
    int handedIn = 0;
    for (auto& c : m_board.contracts())
    {
        if (c.playerId != playerId || c.status != "active" || c.p.destinationHub != hubId)
            continue;
        if (!(m_cargo.contractTonnesOf(shipKey, c.p.id) > 0.0))
            continue;   // its cargo is on another ship (or gone): nothing to hand in here
        (void)m_cargo.takeContract(shipKey, c.p.id);
        m_cargoChanged.insert(shipKey);
        c.lateDays = std::max(0, m_day - c.p.deadlineDay);
        c.closedDay = m_day;
        c.paidCr = std::max(0.0, c.p.rewardCr - c.p.advanceCr);
        if (c.paidCr > 0.0)
            receiveContractMoney(playerId, *l, c.issuerBank, c.paidCr,
                    "контракт " + c.p.id + " сдан на " + hubDisplayName(hubId) + (c.lateDays > 0 ? " с опозданием" : " в срок"));
        if (c.holdId != 0)
            releaseHold(*l, c.holdId);
        double debt = 0.0;
        if (c.lateDays > 0)
        {
            // late: reward x (1 - share x d) - the cut is a penalty to the issuer, never refused (a debt when unpaid)
            c.status = "late";
            c.penaltyCr = std::round(c.p.rewardCr * std::min(1.0, c.p.lateSharePerDay * c.lateDays));
            const std::string memo = "неустойка за опоздание " + std::to_string(c.lateDays) + " сут по контракту " + c.p.id;
            const PaymentResult p = payOrDebt(*l, m_banks, c.issuerBank, c.penaltyCr, Purpose::ContractPenalty, m_day, memo, "неустойка в долг");
            debt = p.ok ? p.lifelineCr : 0.0;
            if (TaxBook* t = taxBook(playerId))
                addExpense(*t, c.penaltyCr);
        }
        else
            c.status = "delivered";
        logEvent(*l, m_day, "контракт " + c.p.id + " сдан" + (c.lateDays > 0 ? " с опозданием" : " в срок") + ": +" + grouped(c.paidCr) +
                                " Cr (награда " + grouped(c.p.rewardCr) + " - аванс " + grouped(c.p.advanceCr) + ")" +
                                (c.lateDays > 0 ? ", неустойка -" + grouped(c.penaltyCr) + " Cr" : std::string()) + ", залог " +
                                grouped(c.p.depositCr) + " Cr разблокирован");
        std::cerr << "[Contract] player " << playerId << " HAND-IN " << c.p.id << " at " << hubId << " (" << hubDisplayName(hubId) << "), day "
                  << m_day << " deadline " << c.p.deadlineDay << ": " << c.status << ", payout +" << c.paidCr << " Cr (reward " << c.p.rewardCr
                  << " - advance " << c.p.advanceCr << "), late penalty " << c.penaltyCr << " (debt " << debt << "), deposit "
                  << c.p.depositCr << " released (hold " << c.holdId << "); net worth " << netWorthCr(*l, m_banks) << " Cr, held "
                  << heldCr(*l, m_banks) << " Cr" << '\n';
        c.holdId = 0;
        ++handedIn;
    }
    if (handedIn > 0)
    {
        (void)rules;
        m_board.trimClosed(8);
        m_board.touch();
        ++m_tradeSerial;
        saveNow("the contract hand-in");
    }
    return handedIn;
}

void EconomyServer::failContract(Contract& c, const std::string& status, const std::string& why)
{
    ClientLedger* l = ledger(c.playerId);
    const auto lots = m_cargo.takeContract(c.shipKey, c.p.id);   // the issuer's goods are reclaimed / written off
    if (!lots.empty())
        m_cargoChanged.insert(c.shipKey);
    c.status = status;
    c.closedDay = m_day;
    c.lateDays = std::max(0, m_day - c.p.deadlineDay);
    c.penaltyCr = c.p.penaltyCr + c.p.advanceCr;   // the penalty + the advance back
    double debt = 0.0;
    if (l)
    {
        const std::string memo = "контракт " + c.p.id + " сорван (" + why + "): неустойка " + grouped(c.p.penaltyCr) + " + возврат аванса " +
                                 grouped(c.p.advanceCr);
        const PaymentResult p = payOrDebt(*l, m_banks, c.issuerBank, c.penaltyCr, Purpose::ContractPenalty, m_day, memo, "неустойка в долг");
        debt = p.ok ? p.lifelineCr : 0.0;
        if (TaxBook* t = taxBook(c.playerId))
            addExpense(*t, c.penaltyCr + (c.holdId != 0 ? c.p.depositCr : 0.0));   // the penalty + the forfeited deposit
        if (c.holdId != 0)
            captureHold(*l, m_banks, c.holdId, m_day, "по контракту " + c.p.id);
    }
    std::cerr << "[Contract] player " << c.playerId << " " << (status == "abandoned" ? "ABANDONED" : "FAILED") << " " << c.p.id << " ("
              << why << "), day " << m_day << " deadline " << c.p.deadlineDay << ": penalty " << c.p.penaltyCr << " + advance back "
              << c.p.advanceCr << " (debt " << debt << "), deposit " << c.p.depositCr << " forfeited (hold " << c.holdId << "), "
              << lots.size() << " lot(s) off ship " << c.shipKey << (l ? "; net worth " + grouped(netWorthCr(*l, m_banks)) + " Cr" : std::string())
              << '\n';
    c.holdId = 0;
    m_board.touch();
    ++m_tradeSerial;
}

game::simulation::ClientTradeResult EconomyServer::abandonContract(const ContractOrder& o)
{
    game::simulation::ClientTradeResult r;
    r.requestId = o.requestId;
    Contract* c = m_board.contract(o.contractId);
    if (!c || c->playerId != o.playerId || c->status != "active")
    {
        recordTradeRefusal(o.playerId, o.requestId, "no_contract", "нет такого действующего контракта " + o.contractId);
        r.code = "no_contract";
        return r;
    }
    ClientLedger* l = ledger(o.playerId);
    if (l && !o.hubId.empty() && o.hubId == c->p.originHub && m_day <= c->p.deadlineDay)
    {
        // a cancel at the origin (the cargo goes back to the issuer's warehouse): a fee + the advance back, deposit released
        (void)m_cargo.takeContract(o.shipKey, c->p.id);
        m_cargoChanged.insert(o.shipKey);
        c->status = "cancelled";
        c->closedDay = m_day;
        c->penaltyCr = std::round(c->p.rewardCr * m_board.rules().abandonFeeShare) + c->p.advanceCr;
        const PaymentResult p = payOrDebt(*l, m_banks, c->issuerBank, c->penaltyCr, Purpose::ContractPenalty, m_day,
                                          "отказ от контракта " + c->p.id + " (сбор + возврат аванса)", "неустойка в долг");
        if (TaxBook* t = taxBook(o.playerId))
            addExpense(*t, c->penaltyCr);
        if (c->holdId != 0)
            releaseHold(*l, c->holdId);
        c->holdId = 0;
        if (ContractPosting* posting = m_board.posting(c->p.id); posting && m_day < posting->expiresDay)
        {
            posting->status = "open";   // back on the board for the others
            posting->takenBy = 0;
            posting->closedDay = -1;
        }
        std::cerr << "[Contract] player " << o.playerId << " CANCELLED " << c->p.id << " at the origin: fee + advance back " << c->penaltyCr
                  << " (debt " << (p.ok ? p.lifelineCr : 0.0) << "), deposit released" << '\n';
        r.ok = true;
        r.code = "contract_cancelled";
        r.totalCr = -c->penaltyCr;
        r.message = "отказ от контракта: -" + grouped(c->penaltyCr) + " Cr (сбор и возврат аванса), залог разблокирован";
    }
    else
    {
        failContract(*c, "abandoned", "отказ в пути");
        r.ok = true;
        r.code = "contract_abandoned";
        r.totalCr = -c->penaltyCr;
        r.message = "контракт брошен: неустойка и возврат аванса -" + grouped(c->penaltyCr) + " Cr, залог " + grouped(c->p.depositCr) + " Cr удержан";
    }
    m_board.touch();
    pushResult(o.playerId, r);
    saveNow("the contract was abandoned");
    r.day = m_day;
    return r;
}

void EconomyServer::stepContracts(int day)
{
    const int lateMax = m_board.rules().lateMaxDays;
    bool changed = false;
    for (auto& c : m_board.contracts())
        if (c.status == "active" && day > c.p.deadlineDay + lateMax)
        {
            failContract(c, "failed", "срок + " + std::to_string(lateMax) + " сут истёк");
            changed = true;
        }
    if (changed)
        m_board.trimClosed(8);
}
} // namespace game::economy
