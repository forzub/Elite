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
bool writeFileAtomically(const std::string& path, const std::string& text)
{
    const std::string tmp = path + ".tmp";
#ifdef _WIN32
    // the data reaches the disk (FlushFileBuffers) before the replace: a power loss cannot leave an empty file
    const std::wstring from = std::filesystem::path(tmp).wstring(), to = std::filesystem::path(path).wstring();
    HANDLE h = CreateFileW(from.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
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
    return MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
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
    void run()
    {
        for (;;)
        {
            StateParts parts;
            {
                std::unique_lock<std::mutex> lock(mutex);
                wake.wait(lock, [this] { return hasPending || stop; });
                if (!hasPending)
                    return;   // stop with nothing left to write
                parts = std::move(pending);
                pending = StateParts{};
                hasPending = false;
            }
            const double t0 = steadyS();
            const std::string text = finishState(std::move(parts));
            const double t1 = steadyS();
            const bool ok = writeFileAtomically(path, text);
            if (!ok)
                std::cerr << "[EconomyServer] state NOT saved to " << path << " (the previous file is kept)" << '\n';
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
                m_customsFeeMult[k] = num(c, "fee_mult", m_customsFeeMult[k]);
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

    saveNow("the daily step");
}

// ---------------------------------------------------------------- players
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
    saveNow("the port call opened");
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
    const int g = goodIndex(o.goodId);
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
                               hubId.empty() ? 0.0 : importDutyRate(lot.originHub, hubId, systemId)});
        s.cargoNetT = m->netT();
        s.cargoGrossT = m->grossT();
        s.c20Used = m->c20();
    }
    for (const auto& v : m_vatRefunds)
        if (v.playerId == playerId)
            s.vatReclaimCr += v.amountCr;
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
    j["schema"] = "economy_state/6";   // 2: cargo manifests, input VAT refunds, holds (TRADE_DESIGN §7.6); 3: contracts (step 5);
                                       // 4: port calls (open + booked); 5: ledger last_withheld (the protected minimum);
                                       // 6: tax books (the income tax, step 7)
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
                                   {"cause", r.cause}});
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
    double tonnes = 0.0;
    for (const auto& lot : lots)
    {
        LostCargo r;
        r.recordId = "L" + std::to_string(m_nextLostCargo++);
        r.eventId = loss.eventId;
        r.shipKey = loss.shipKey;
        r.playerId = player;
        r.lot = lot;
        r.rightsHolder = lot.owner;
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
        schema != "economy_state/4" && schema != "economy_state/5" && schema != "economy_state/6")   // /1: no trade registries; /2: no contracts;
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
            r.cause = x.value("cause", std::string());
            lostCargo.push_back(std::move(r));
        }
        const std::uint64_t nextLostCargo = j.value("next_lost_cargo", std::uint64_t{1});
        CargoRegistry cargo;   // a dry run of the cargo parse (throws here, not in the commit)
        cargo.fromJson(j.value("cargo", json::array()));
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
void EconomyServer::saveNow(const char* what)
{
    if (m_stateFile.empty())
        return;
    m_stateDirty = true;
    m_dirtyWhat = what;
}

void EconomyServer::flushStateIfDue(bool force)
{
    if (!m_stateDirty || m_stateFile.empty())
        return;
    const double now = steadyS();
    if (!force && now - m_lastSaveS < kSaveIntervalS)
        return;
    m_stateDirty = false;
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
