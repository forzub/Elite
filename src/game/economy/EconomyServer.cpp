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

EconomyServer::~EconomyServer()
{
    service::uninstall(this);
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
            m_styles[id] = st;
        }
        const json regimes = fees.value("regime_style", json::object());
        for (const auto& [regime, style] : regimes.items())
            if (style.is_string())
                m_regimeStyle[regime] = style.get<std::string>();
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
}

void EconomyServer::advanceTo(int day)
{
    if (!m_loaded)
        return;
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
            const double move = stepCell(c, d, m_priceRules);
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

    if (!m_stateFile.empty() && !writeFileAtomically(m_stateFile, saveState()))
        std::cerr << "[EconomyServer] state NOT saved to " << m_stateFile << " (the previous file is kept)" << '\n';
}

// ---------------------------------------------------------------- players
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
    return pay(*l, m_banks, bankOf(hubId, systemId), amountCr, purpose, m_day, true, memo);
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
    out = {c.priceCr, m.spread, c.stockT, c.targetT};
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
    if (g < 0)
        return refuse("no_good", "неизвестный товар " + o.goodId);
    const auto& good = EconomyModel::instance().goods()[static_cast<std::size_t>(g)];
    const auto mit = m_marketIndex.find(o.hubId);
    if (mit == m_marketIndex.end() || !m_markets[mit->second].cells[static_cast<std::size_t>(g)].traded)
        return refuse("no_market", "на этом хабе нет рынка товара «" + good.ru + "»");
    if (o.quoteDay >= 0 && o.quoteDay != m_day)
        return refuse("requote", "сутки сменились (котировка дня " + std::to_string(o.quoteDay) + ", сейчас " + std::to_string(m_day) +
                                     "): цена пересчитана, повторите");
    auto& market = m_markets[mit->second];
    auto& cell = market.cells[static_cast<std::size_t>(g)];
    const MarketSide side{cell.priceCr, market.spread, cell.stockT, cell.targetT};
    const FiscalStyle style = fiscalStyleOf(o.hubId, o.systemId);
    const DealQuote q = explainDeal(o.buy, side, o.tonnes, style, m_dealRules);
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
        if (q.vatReclaimCr > 0.0 && style.vatRefundDays > 0)
            m_vatRefunds.push_back({o.playerId, bank, q.vatReclaimCr, m_day + style.vatRefundDays});
        lot.costBasisCr = -q.totalCr - (style.vatRefundDays > 0 ? q.vatReclaimCr * (1.0 - m_dealRules.vatRefundFeeShare) : 0.0);
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
    std::cerr << "[Trade] player " << o.playerId << " request " << o.requestId << ": " << (o.buy ? "BUY " : "SELL ") << o.tonnes << " t "
              << o.goodId << " at " << o.hubId << " (" << style.id << "): list " << q.listUnitCr << " -> deal " << q.unitCr
              << " Cr/t (impact " << 100.0 * q.impactShare << " %), goods " << q.goodsCr << ", handling " << q.handlingCr << ", VAT "
              << q.vatCr << ", turnover " << q.turnoverCr << ", account " << (q.totalCr >= 0.0 ? "+" : "") << q.totalCr
              << " Cr; stock " << (o.buy ? cell.stockT + o.tonnes : cell.stockT - o.tonnes) << " -> " << cell.stockT << " t; net worth "
              << netWorthCr(*l, m_banks) << " Cr" << '\n';
    pushResult(o.playerId, r);
    // a deal moves money and goods: it must not wait for the daily save (a crash would lose it - TRADE_DESIGN §7.6; the
    // JSONL money journal of step 17 replaces this full write)
    if (!m_stateFile.empty() && !writeFileAtomically(m_stateFile, saveState()))
        std::cerr << "[EconomyServer] state NOT saved after the deal" << '\n';
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
    if (!m_stateFile.empty())
        (void)writeFileAtomically(m_stateFile, saveState());
    return {};
}

void EconomyServer::devClearCargo(std::uint64_t shipKey)
{
    m_cargo.clear(shipKey);
    ++m_tradeSerial;
    if (!m_stateFile.empty())
        (void)writeFileAtomically(m_stateFile, saveState());
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
            const MarketSide side{c.priceCr, m.spread, c.stockT, c.targetT};
            s.market.push_back({static_cast<int>(g), true, c.priceCr, bidCr(side), askCr(side), c.stockT, c.targetT,
                                hubTakesUpToT(side, m_dealRules), hubGivesUpToT(side, m_dealRules),
                                c.anchorCr > 0.0 ? c.priceCr / c.anchorCr - 1.0 : 0.0});
        }
    }
    else
        s.marketNote = hubId.empty() ? "рынок виден у хаба (в его зоне)" : "у этого хаба нет своего рынка";
    if (const CargoManifest* m = m_cargo.find(shipKey))
    {
        for (const auto& lot : m->lots)
            s.cargo.push_back({lot.lotId, goodIndex(lot.goodId), lot.tonnes, lot.c20, lot.tonnes > 0.0 ? lot.costBasisCr / lot.tonnes : 0.0,
                               lot.originHub, lot.loadedDay, lot.owner, lot.contractId});
        s.cargoNetT = m->netT();
        s.cargoGrossT = m->grossT();
        s.c20Used = m->c20();
    }
    for (const auto& v : m_vatRefunds)
        if (v.playerId == playerId)
            s.vatReclaimCr += v.amountCr;
    if (const auto it = m_results.find(playerId); it != m_results.end())
        s.results = it->second;
    return s;
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
    json j;
    j["schema"] = "economy_state/2";   // 2: cargo manifests, input VAT refunds, holds (TRADE_DESIGN §7.6)
    j["day"] = m_day;
    // (fuel_free is a RULE - economy_rules / ELITE_FUEL_FREE - not world state: not saved; it was written and never read)
    json fx = json::object();
    for (const auto& b : m_banks.banks())
        fx[b.id] = m_banks.fx(b.id);
    j["fx"] = fx;
    json markets = json::object();
    for (const auto& m : m_markets)
    {
        json stock = json::array(), price = json::array();
        for (const auto& c : m.cells)
        {
            stock.push_back(c.traded ? json(c.stockT) : json());
            price.push_back(c.traded ? json(c.priceCr) : json());
        }
        markets[m.hubId] = {{"stock", stock}, {"price", price}};
    }
    j["markets"] = markets;
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
    j["vat_refunds"] = json::array();
    for (const auto& v : m_vatRefunds)
        j["vat_refunds"].push_back({{"player", v.playerId}, {"bank", v.bankId}, {"cr", v.amountCr}, {"due", v.dueDay}});
    return j.dump();
}

bool EconomyServer::recordShipLoss(const ShipLoss& loss)
{
    if (!m_loaded || loss.eventId.empty() || m_lossEvents.count(loss.eventId))
        return false;   // idempotent: this loss was resolved already
    m_lossEvents.insert(loss.eventId);
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
    if (!m_stateFile.empty() && !writeFileAtomically(m_stateFile, saveState()))
        std::cerr << "[EconomyServer] state NOT saved to " << m_stateFile << " (the previous file is kept)" << '\n';
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
    if (schema != "economy_state/1" && schema != "economy_state/2")   // /1: no trade registries yet (empty)
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
        decltype(m_vatRefunds) vatRefunds;
        for (const auto& v : j.value("vat_refunds", json::array()))
            vatRefunds.push_back({v.value("player", std::uint64_t{0}), v.value("bank", std::string()), v.value("cr", 0.0), v.value("due", 0)});

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
        m_lossEvents.swap(lossEvents);
        m_lostCargo.swap(lostCargo);
        m_nextLostCargo = nextLostCargo;
        ++m_tradeSerial;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[EconomyServer] state: " << e.what() << '\n';
        return false;
    }
    return true;
}
} // namespace game::economy
