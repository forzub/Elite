// Unit checks of the pure economy parts (docs/ECONOMY_BANKING.md): interest, payments, lifeline, the insolvency
// path (no death spiral), in duplum, the daily price step, FX; plus a data-driven server run (365 days) when the
// economy data is reachable from the working directory (run from the repository root).
//
//   EliteEconomyCheck            -> "ECONOMY CHECK: N passed, 0 failed", exit code = failures
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <climits>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

#include <nlohmann/json.hpp>
#include "src/game/economy/BankingModel.h"
#include "src/game/economy/EconomyServer.h"
#include "src/game/economy/PriceModel.h"
#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/ContractBoard.h"
#include "src/game/economy/TradeQuoteModel.h"
#include "src/game/economy/TaxModel.h"
#include "src/game/economy/EconomyModel.h"
#include "src/game/presentation/TradeDealPresentation.h"

using namespace game::economy;

namespace
{
int g_pass = 0, g_fail = 0;

void check(bool ok, const std::string& what)
{
    if (ok)
        ++g_pass;
    else
        ++g_fail;
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
}

bool near(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

// two-bank test world: Sol (reserve, CR) + a foreign bank in another bloc
BankDirectory testBanks()
{
    BankDirectory d;
    NationalBank sol;
    sol.id = "sol";
    sol.name = "Sol Treasury";
    sol.code = "CR";
    sol.bloc = "sol";
    sol.reserve = true;
    sol.policyRate = 0.032;
    sol.depositRate = 0.0245;
    sol.loanRate = 0.062;
    sol.overdraftRate = 0.122;
    sol.lifelineRate = 0.042;
    d.add(sol);
    NationalBank h;
    h.id = "harmony";
    h.name = "Harmony";
    h.code = "HRL";
    h.bloc = "pact";
    h.policyRate = 0.0405;
    h.depositRate = 0.033;
    h.loanRate = 0.0705;
    h.overdraftRate = 0.1305;
    h.lifelineRate = 0.0505;
    h.fxAnchorPerCr = 4.383;
    h.fxSigmaDay = 0.0043;
    h.fxHalfLifeDays = 730;
    h.fxBand = 0.14;
    d.add(h);
    NationalBank c;
    c.id = "closedland";
    c.name = "Closed";
    c.code = "CLD";
    c.bloc = "hermit";
    c.fxAnchorPerCr = 10.0;
    d.add(c);
    d.setChannel("sol", "pact", Channel::Licensed);
    d.setChannel("pact", "sol", Channel::Licensed);
    d.setChannel("sol", "hermit", Channel::Closed);
    d.setChannel("hermit", "sol", Channel::Closed);
    return d;
}

std::uint64_t g_rng = 0x1234567ull;
double uniform()
{
    g_rng = g_rng * 6364136223846793005ull + 1442695040888963407ull;
    return static_cast<double>(g_rng >> 11) * (1.0 / 9007199254740992.0);
}
double gauss()
{
    double s = 0.0;
    for (int k = 0; k < 4; ++k)
        s += uniform();
    return (s - 2.0) * std::sqrt(3.0);
}

void testInterestAndAnnuity()
{
    check(annuityInstallment(toMinor(100000.0), 0.0, 10) == toMinor(10000.0), "annuity at 0 % = principal / n");
    const double r = 0.06 * 30.0 / 365.0;
    const double expect = 100000.0 * r / (1.0 - std::pow(1.0 + r, -12));
    check(near(toUnits(annuityInstallment(toMinor(100000.0), 0.06, 12)), expect, 0.02), "annuity 6 %, 12 periods = closed form");

    BankDirectory d = testBanks();
    d.rules.startAccountCr = 10000.0;
    d.rules.startCashCr = 0.0;
    d.rules.startOverdraftCr = 4000.0;
    ClientLedger l = openLedger(d, 1, "sol", 0);
    for (int day = 0; day < 365; ++day)
        stepLedgerDay(l, d, day);
    const double bal = toUnits(findAccount(l, "sol")->balance);
    check(near(bal, 10000.0 * std::pow(1.0 + 0.0245 / 365.0, 365.0), 0.5), "deposit 2.45 % for a year compounds daily (" + std::to_string(bal) + ")");

    ClientLedger o = openLedger(d, 2, "sol", 0);
    findAccount(o, "sol")->balance = toMinor(-4000.0);
    for (int day = 0; day < 30; ++day)
        stepLedgerDay(o, d, day);
    const double ob = toUnits(findAccount(o, "sol")->balance);
    check(near(ob, -4000.0 * std::pow(1.0 + 0.122 / 365.0, 30.0), 0.05), "overdraft 12.2 % charged daily (" + std::to_string(ob) + ")");
}

void testPayments()
{
    BankDirectory d = testBanks();
    d.rules.startAccountCr = 1000.0;
    d.rules.startCashCr = 100.0;
    d.rules.startOverdraftCr = 0.0;
    ClientLedger l = openLedger(d, 1, "sol", 0);
    auto r = pay(l, d, "sol", 150.0, Purpose::Trade, 0, false, "test");
    check(r.ok && l.cash["sol"] == 0 && findAccount(l, "sol")->balance == toMinor(950.0), "home payment: cash first, then the account");

    // a payment in Harmony from the Sol account: licensed channel 0.5 % (min 10 Cr) + FX 1 %
    r = pay(l, d, "harmony", 100.0, Purpose::Docking, 1, false, "dock");
    check(r.ok && near(r.feesCr, 10.0, 0.01) && near(toUnits(findAccount(l, "sol")->balance), 840.0, 0.02),
          "foreign payment: transfer fee min 10 Cr (fees " + std::to_string(r.feesCr) + ")");

    // all or nothing
    const auto before = findAccount(l, "sol")->balance;
    r = pay(l, d, "sol", 5000.0, Purpose::Trade, 2, true, "too much");
    check(!r.ok && findAccount(l, "sol")->balance == before && l.loans.empty(), "a failed payment changes nothing (trade gets no lifeline)");

    // closed channel: the Sol account cannot pay a hermit bank; lifeline still works for fuel
    r = pay(l, d, "closedland", 10.0, Purpose::Trade, 2, false, "closed");
    check(!r.ok, "closed bloc channel refuses a transfer");
    r = pay(l, d, "closedland", 1000.0, Purpose::Fuel, 2, true, "fuel");
    check(r.ok && near(r.lifelineCr, 1000.0, 0.01) && l.loans.size() == 1 && l.loans[0].kind == LoanKind::Lifeline,
          "fuel behind a closed channel: lifeline credit at the local bank");

    // income in a foreign bank goes home with an FX fee
    receive(l, d, "harmony", 1000.0, 3, "contract");
    check(near(toUnits(findAccount(l, "sol")->balance), 840.0 + 990.0, 0.05), "foreign income lands home minus 1 % FX");
}

void testLifeline()
{
    BankDirectory d = testBanks();
    d.rules.startAccountCr = 0.0;
    d.rules.startCashCr = 0.0;
    d.rules.startOverdraftCr = 0.0;
    ClientLedger l = openLedger(d, 1, "sol", 0);
    auto r = pay(l, d, "sol", 7400.0, Purpose::Fuel, 0, true, "fuel");
    check(r.ok && near(r.lifelineCr, 7400.0, 0.01), "broke client still gets fuel (lifeline)");
    r = pay(l, d, "sol", 20000.0, Purpose::Repair, 1, true, "repair");
    check(!r.ok, "lifeline 90-day limit holds for repair (25 000 Cr)");
    r = pay(l, d, "sol", 14000.0, Purpose::Tow, 1, true, "tow");
    check(r.ok, "a tow is never refused, even beyond the lifeline limit");
    check(near(lifelineLeftCr(l, d, 95), 25000.0, 0.01), "the lifeline window renews after 90 days");
    // interest-free for 90 days
    const Minor out0 = l.loans[0].outstanding;
    for (int day = 0; day < 89; ++day)
        stepLedgerDay(l, d, day);
    check(l.loans[0].outstanding == out0, "lifeline credit carries no interest for 90 days");
}

void testNoDeathSpiral()
{
    BankDirectory d = testBanks();
    d.rules.startAccountCr = 0.0;
    d.rules.startCashCr = 0.0;
    d.rules.startOverdraftCr = 4000.0;
    ClientLedger l = openLedger(d, 1, "sol", 0);
    // the client spends the whole overdraft and then earns nothing for ten years
    check(pay(l, d, "sol", 4000.0, Purpose::Trade, 0, false, "spend").ok, "overdraft can be spent to its limit");
    bool sawGrace = false, sawDelinquent = false, sawRestructured = false, fuelEveryQuarter = true;
    double maxDebt = 0.0;
    for (int day = 1; day <= 3650; ++day)
    {
        stepLedgerDay(l, d, day);
        sawGrace = sawGrace || l.standing == Standing::Grace;
        sawDelinquent = sawDelinquent || l.standing == Standing::Delinquent;
        sawRestructured = sawRestructured || l.standing == Standing::Restructured;
        maxDebt = std::max(maxDebt, totalDebtCr(l, d) - std::min(0.0, toUnits(findAccount(l, "sol")->balance)));
        if (day % 91 == 0)
            fuelEveryQuarter = fuelEveryQuarter && pay(l, d, "sol", 3000.0, Purpose::Fuel, day, true, "fuel").ok;
    }
    check(sawGrace && sawDelinquent && sawRestructured, "no income: good -> grace -> delinquent -> restructured");
    check(fuelEveryQuarter, "fuel is available every quarter for ten years without income");
    // 40 quarters x 3 000 lifeline fuel + the 4 000 overdraft, interest at most in duplum (x2)
    const double bound = 2.0 * (4000.0 + 40.0 * 3000.0);
    check(maxDebt <= bound, "debt bounded by in duplum (max " + std::to_string(maxDebt) + " <= " + std::to_string(bound) + ")");
    bool hardship = false;
    for (const auto& loan : l.loans)
        hardship = hardship || (loan.kind == LoanKind::Restructured && loan.hardship);
    check(hardship, "restructured debt with no income: hardship freeze (no interest)");
    for (const auto& [b, m] : l.cash)
        check(m >= 0, "cash never negative");

    // recovery: 3 000 Cr income every 30 days repays it (25 % garnish) and the standing returns to good
    int goodDay = -1;
    for (int day = 3651; day <= 3651 + 365 * 40; ++day)
    {
        if (day % 30 == 0)
            receive(l, d, "sol", 3000.0, day, "contract");
        stepLedgerDay(l, d, day);
        if (l.standing == Standing::Good)
        {
            goodDay = day;
            break;
        }
    }
    check(goodDay > 0, "steady income repays the restructured debt (good again after " + std::to_string(goodDay - 3651) + " days)");
}

void testInDuplum()
{
    BankDirectory d = testBanks();
    d.rules.startAccountCr = 0.0;
    d.rules.startCashCr = 0.0;
    d.rules.startOverdraftCr = 0.0;
    d.rules.restructureAfterDays = 1000000;   // keep the term loan unpaid and overdue forever
    ClientLedger l = openLedger(d, 1, "sol", 0);
    Loan* loan = takeLoan(l, d, "sol", 10000.0, 360, 0.10, 0);
    check(loan != nullptr, "a term loan is granted in good standing");
    findAccount(l, "sol")->balance = 0;   // the money is spent
    for (int day = 1; day <= 365 * 30; ++day)
        stepLedgerDay(l, d, day);
    const Loan& x = l.loans.front();
    check(x.interestCharged <= x.principal, "in duplum: interest + fees never exceed the principal");
    check(toUnits(x.outstanding) <= 20000.0 + 0.01, "an unpaid 10 000 Cr loan never exceeds 20 000 Cr");
}

void testPriceStep()
{
    PriceRules rules;
    GoodCell c;
    c.traded = true;
    c.flowTDay = 11.4;
    c.targetT = 343.0;
    c.stockT = 343.0;
    c.anchorCr = 7254.0;
    c.priceCr = 7254.0;
    c.leadDays = 7.0;
    double worst = 0.0, maxMove = 0.0;
    for (int day = 0; day < 365; ++day)
    {
        CellDrivers d;
        d.supplyShock = uniform() * 2.0 - 1.0;
        d.demandShock = uniform() * 2.0 - 1.0;
        d.slowDemand = std::sin(day / 9.0);
        maxMove = std::max(maxMove, std::fabs(stepCell(c, d, rules)));
        worst = std::max(worst, std::fabs(c.priceCr / c.anchorCr - 1.0));
    }
    check(worst < 0.15, "calm market stays within 15 % of its anchor for a year (worst " + std::to_string(worst * 100.0) + " %)");

    // a fuel crunch: supply x0.4 for 20 days
    double peak = 0.0;
    for (int day = 0; day < 20; ++day)
    {
        CellDrivers d;
        eventDrivers("fuel_crunch", "fuel", "jump_charge", false, d);
        const double move = stepCell(c, d, rules);
        maxMove = std::max(maxMove, std::fabs(move));
        peak = std::max(peak, c.priceCr / c.anchorCr);
    }
    check(peak > 1.1, "fuel crunch raises the price through the stock (peak x" + std::to_string(peak) + ")");
    check(maxMove <= rules.maxDailyChange + 1e-9, "no daily move beyond 8 %");
    for (int day = 0; day < 90; ++day)
        stepCell(c, CellDrivers{}, rules);
    check(std::fabs(c.priceCr / c.anchorCr - 1.0) < 0.02, "after the crunch the price returns to the anchor (damped)");

    // anchor round trip and the clamp
    const double a = anchorFromPrice(9000.0, 100.0, 343.0, rules);
    GoodCell e = c;
    e.anchorCr = a;
    e.stockT = 100.0;
    check(near(targetPriceCr(e, rules), 9000.0, 1e-6), "anchorFromPrice reproduces the initial price");
    e.stockT = 0.0;
    check(targetPriceCr(e, rules) <= a * rules.clampHi + 1e-6, "empty stock: price clamped at x4 the anchor");

    // the breakdown sums to the list price
    const auto b = explainPrice(5080.0, false, 365.0, 7000.0, 7400.0, 0.18, {0.41, 0.05, 0.36, 0.18});
    const double sum = b.feedstockCr + b.conversionCr + b.separationEnergyCr + b.fabricationCr + b.transportCr + b.locationCr + b.scarcityCr +
                       b.stationMarginCr;
    check(near(sum, b.listCr, 1e-6) && near(b.listCr, 7400.0 * 1.09, 1e-6), "cost breakdown sums exactly to the list price");
}

void testFx()
{
    BankDirectory d = testBanks();
    const NationalBank& h = *d.bank("harmony");
    double fx = h.fxAnchorPerCr, lo = fx, hi = fx, sumLog = 0.0;
    for (int day = 0; day < 3650; ++day)
    {
        fx = stepFx(h, fx, gauss());
        lo = std::min(lo, fx);
        hi = std::max(hi, fx);
        sumLog += std::log(fx / h.fxAnchorPerCr);
    }
    check(lo >= h.fxAnchorPerCr / (1.0 + h.fxBand) - 1e-9 && hi <= h.fxAnchorPerCr * (1.0 + h.fxBand) + 1e-9,
          "FX stays inside the managed band for ten years");
    check(std::fabs(sumLog / 3650.0) < 0.08, "FX reverts to its anchor (mean log deviation " + std::to_string(sumLog / 3650.0) + ")");
    check(stepFx(*d.bank("sol"), 1.0, 3.0) == 1.0, "the Cr (reserve) never moves");
}

// ---- trade layer, steps 1-2 (docs/TRADE_DESIGN.md §2.3, §5, §7.2)
void testTradeModel()
{
    check(near(c20NetTonnes(0.65), 15.145, 1e-9) && near(c20NetTonnes(7.8), 20.0, 1e-9),
          "a C20 takes 23.3 m3 x density, at most 20 t (grain 15.1 t, steel 20 t)");
    check(c20For(15.145, 0.65) == 1 && c20For(15.2, 0.65) == 2 && c20For(0.0, 0.65) == 0, "C20 boxes a lot needs");

    CargoManifest m;
    m.lots.push_back({"L1", "food_staples", 15.145, 1});
    m.lots.push_back({"L2", "steel", 30.0, 2});
    check(near(m.netT(), 45.145, 1e-9) && m.c20() == 3 && near(m.grossT(), 45.145 + 3 * 1.7, 1e-9),
          "gross cargo mass = net + 1.7 t tare per C20");

    TradeDealRules r;
    FiscalStyle sol;   // bureaucratic: VAT 20 %, port index 1
    const MarketSide small{1000.0, 0.10, 200.0, 200.0};
    const double impact = sellUnitCr(small, 120.0, r) / bidCr(small) - 1.0;
    check(near(impact, std::pow(200.0 / 230.0, 0.6) - 1.0, 1e-12) && impact < -0.075 && impact > -0.085,
          "selling 120 t into a 200 t stock: the deal price -8 % (" + std::to_string(impact * 100.0) + " %)");
    const MarketSide big{1000.0, 0.10, 2000.0, 2000.0};
    const double impactBig = sellUnitCr(big, 120.0, r) / bidCr(big) - 1.0;
    check(impactBig > -0.01 && impactBig < -0.008, "the same 120 t into 2 000 t: -0.9 % (" + std::to_string(impactBig * 100.0) + " %)");

    // the hub limits: the warehouse is full above 1.5 T, the irreducible remainder 5 % T
    const MarketSide m200{1000.0, 0.10, 200.0, 200.0};
    const auto full = explainDeal(false, m200, 101.0, sol, r);
    check(!full.ok && full.code == "hub_full" && explainDeal(false, m200, 100.0, sol, r).ok, "the hub takes at most 1.5 T - S (100 t here)");
    const auto empty = explainDeal(true, m200, 191.0, sol, r);
    check(!empty.ok && empty.code == "hub_empty" && explainDeal(true, m200, 190.0, sol, r).ok, "the hub gives at most S - 0.05 T (190 t)");

    // the breakdown sums to the total; a buy, then a sell of the same goods the same day at the same hub always loses -
    // even counting the input VAT that comes back later (minus the refund fee)
    bool sums = true, alwaysLoses = true;
    double worst = -1e18;
    for (const double spread : {0.06, 0.10, 0.18, 0.25})
        for (const double stock : {50.0, 200.0, 2000.0, 40000.0})
            for (const double q : {1.0, 15.145, 60.0, 120.0})
                for (const double vat : {0.0, 0.08, 0.20})
                {
                    FiscalStyle st;
                    st.vatShare = vat;
                    st.portIndex = vat > 0.1 ? 1.0 : 0.7;
                    MarketSide side{900.0, spread, stock, stock};
                    const auto b = explainDeal(true, side, std::min(q, hubGivesUpToT(side, r)), st, r);
                    if (!b.ok)
                        continue;
                    side.stockT -= b.tonnes;
                    const auto s = explainDeal(false, side, b.tonnes, st, r);
                    double lineSum = 0.0;
                    for (const auto& l : b.lines)
                        lineSum += l.cr;
                    sums = sums && near(lineSum, b.totalCr, 1e-6);
                    const double roundTrip = b.totalCr + s.totalCr + b.vatReclaimCr * (1.0 - r.vatRefundFeeShare);
                    alwaysLoses = alwaysLoses && s.ok && roundTrip < 0.0;
                    worst = std::max(worst, roundTrip);
                }
    check(sums, "the deal breakdown sums to what the account changes by");
    check(alwaysLoses, "buy -> sell the same day at the same hub always loses (best case " + std::to_string(worst) + " Cr)");

    check(near(dockingFeeCr(260.0, sol, r), 384.0, 1e-9), "docking a Cobrett-I (260 t dry) at a bureaucratic port = 384 Cr");
    const auto sell = explainDeal(false, MarketSide{1214.0 / 0.97, 0.06, 312.0, 312.0}, 10.5, sol, r);
    check(near(sell.handlingCr, 126.0, 1e-9) && near(sell.vatCr, 25.2, 1e-9), "selling 10.5 t: handling 126 Cr, VAT on it 25 Cr (§8.1)");

    // holds (escrow): blocked money is the client's but not spendable
    BankDirectory d = testBanks();
    ClientLedger l = openLedger(d, 1, "sol", 0);
    l.cash.clear();
    const int hold = placeHold(l, d, "sol", 15000.0, 0, "залог");
    check(hold > 0 && near(heldCr(l, d), 15000.0, 0.01) && near(netWorthCr(l, d), 20000.0, 0.01), "a hold blocks 15 000 of 20 000 Cr");
    check(!pay(l, d, "sol", 10000.0, Purpose::Trade, 0, false, "x").ok, "held money cannot be spent (5 000 free + 4 000 overdraft)");
    check(placeHold(l, d, "sol", 6000.0, 0, "x") == 0, "a hold never draws the overdraft");
    check(releaseHold(l, hold) && pay(l, d, "sol", 10000.0, Purpose::Trade, 0, false, "x").ok, "released: the money is spendable again");
}

void testTradeServer()
{
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  trade server run (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    const CargoCapacity cobrett{120.0, 6};
    const auto worth = [&](std::uint64_t p) { return netWorthCr(*s.ledger(p), s.banks()); };
    const auto stockOf = [&](const char* hub, const char* good) {
        MarketSide m;
        return s.marketSide(hub, good, m) ? m.stockT : -1.0;
    };
    check(s.fiscalStyleOf("earth_orbital_hub", 0).id == "bureaucratic" && near(s.fiscalStyleOf("earth_orbital_hub", 0).vatShare, 0.20, 1e-9),
          "Sol is bureaucratic: VAT 20 %");

    EconomyServer::TradeOrder o;
    o.playerId = 1;
    o.shipKey = 501;
    o.hubId = "earth_orbital_hub";
    o.systemId = 0;
    o.goodId = "food_staples";
    o.tonnes = c20NetTonnes(0.65);   // 1 C20 of grain
    o.capacity = cobrett;
    o.requestId = 1;
    const double w0 = worth(1), s0 = stockOf("earth_orbital_hub", "food_staples");
    const auto bought = s.trade(o);
    const auto* m = s.cargo().find(501);
    check(bought.ok && bought.totalCr < -9000.0 && near(worth(1) - w0, bought.totalCr, 0.05) &&
              near(s0 - stockOf("earth_orbital_hub", "food_staples"), o.tonnes, 1e-9),
          "buy 1 C20 of grain: the account -" + std::to_string(-bought.totalCr) + " Cr, the hub stock -15.1 t at once");
    check(m && m->lots.size() == 1 && m->c20() == 1 && near(m->netT(), 15.145, 1e-9) && near(m->grossT(), 16.845, 1e-9),
          "the manifest: 15.1 t net, 1 C20, 16.8 t gross");

    // the same day, the same hub: sold back at a loss even after the VAT refund
    o.buy = false;
    o.requestId = 2;
    const auto sold = s.trade(o);
    const double vatBack = 0.20 * 0.99 * (-bought.totalCr / 1.2 - o.tonnes * 12.0);
    check(sold.ok && s.cargo().find(501)->lots.empty() && worth(1) + vatBack < w0,
          "sold back the same day: " + std::to_string(worth(1) + vatBack - w0) + " Cr including the VAT refund to come");

    // all or nothing: refusals change nothing
    const auto unchanged = [&](const std::string& what, const game::simulation::ClientTradeResult& r, const std::string& code) {
        check(!r.ok && r.code == code, what + " -> " + r.code + " (" + r.message + ")");
    };
    o.requestId = 3;
    unchanged("selling grain that is not aboard", s.trade(o), "not_aboard");
    o.buy = true;
    o.tonnes = 7 * c20NetTonnes(0.65);
    const double w1 = worth(1), s1 = stockOf("earth_orbital_hub", "food_staples");
    unchanged("7 C20 into 6 places", s.trade(o), "no_space");
    o.tonnes = 1.0;
    o.quoteDay = 999;
    unchanged("a quote of yesterday", s.trade(o), "requote");
    o.quoteDay = -1;
    o.limitUnitCr = 100.0;
    unchanged("not dearer than 100 Cr/t", s.trade(o), "price_moved");
    o.limitUnitCr = 0.0;
    s.ensurePlayer(2, 0);
    (void)s.chargeService(2, "earth_orbital_hub", 0, Purpose::Other, worth(2) + 3000.0, "test: spent down");
    o.playerId = 2;
    o.shipKey = 502;
    o.goodId = "medical_equipment";   // legal everywhere (computing needs the export licence since step 11)
    o.tonnes = 10.0;
    const double w2 = worth(2);
    unchanged("a broke player buying 10 t of medical equipment", s.trade(o), "no_money");
    check(near(worth(1), w1, 1e-9) && near(worth(2), w2, 1e-9) && near(stockOf("earth_orbital_hub", "food_staples"), s1, 1e-9) &&
              s.cargo().find(502) && s.cargo().find(502)->lots.empty(),
          "refused deals moved no money, no stock, no cargo");

    // buy here, sell at another Sol hub
    o.playerId = 1;
    o.shipKey = 501;
    o.goodId = "food_staples";
    o.tonnes = c20NetTonnes(0.65);
    o.requestId = 10;
    check(s.trade(o).ok, "bought 1 C20 of grain again");
    o.buy = false;
    o.hubId = "earth_hub.tikhiy";
    o.requestId = 11;
    const double t0 = stockOf("earth_hub.tikhiy", "food_staples");
    const auto there = s.trade(o);
    check(there.ok && near(stockOf("earth_hub.tikhiy", "food_staples") - t0, o.tonnes, 1e-9), "sold at Тихий: its stock +15.1 t");
    const auto snap = s.tradeSnapshot(1, 501, "earth_hub.tikhiy", 0, true);
    check(snap.valid && !snap.market.empty() && snap.results.size() == 4 && snap.results.back().requestId == 11 && snap.vatReclaimCr > 0.0,
          "the trade snapshot: the server market, the last 4 results, the VAT to come back");

    // DEV cargo + save / restore round trip (economy_state/2)
    check(s.devAddCargo(1, 501, "steel", 40.0, cobrett, "earth_orbital_hub").empty() && s.cargo().find(501)->c20() == 2,
          "DEV cargo: 40 t of steel = 2 C20");
    check(!s.devAddCargo(1, 501, "steel", 81.0, cobrett, "earth_orbital_hub").empty(), "DEV cargo still obeys the 120 t payload limit");
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    check(t.restoreState(saved) && t.saveState() == saved && t.cargo().find(501) && near(t.cargo().find(501)->grossT(), 43.4, 1e-9),
          "save / restore: the manifest and the VAT refunds round-trip exactly");

    // a lost ship: its cargo leaves the manifest ONCE into lost-cargo records; no resurrection after a restore
    {
        const std::size_t lostBefore = t.lostCargo().size();
        const double grossBefore = t.cargo().find(501)->grossT();
        EconomyServer::ShipLoss loss;
        loss.eventId = "loss:501:42";
        loss.shipKey = 501;
        loss.systemId = 0;
        loss.cause = "crushed";
        const bool first = t.recordShipLoss(loss);
        const bool again = t.recordShipLoss(loss);
        const std::size_t added = t.lostCargo().size() - lostBefore;
        double lostT = 0.0;
        for (const auto& r : t.lostCargo())
            if (r.eventId == loss.eventId)
                lostT += r.lot.tonnes;
        check(first && !again && added >= 1 && !t.cargo().find(501) && near(lostT, 40.0, 1e-9) &&
                  t.lostCargo().back().state == "aboard_wreck" && t.lossRecorded(loss.eventId),
              "a ship loss moves the cargo off the manifest once (" + std::to_string(added) + " record(s), " +
                  std::to_string(lostT) + " t; gross before " + std::to_string(grossBefore) + " t)");
        const std::string lostSaved = t.saveState();
        EconomyServer u;
        u.load();
        check(u.restoreState(lostSaved) && !u.cargo().find(501) && u.lostCargo().size() == t.lostCargo().size() &&
                  u.lossRecorded(loss.eventId) && !u.recordShipLoss(loss) && u.saveState() == lostSaved,
              "after a restore the lost cargo stays lost (no manifest), the event stays resolved");
    }

    // the input VAT comes back after 30 days minus 1 %
    const double before = worth(1);
    for (int day = 1001; day <= 1031; ++day)
        s.advanceTo(day);
    check(s.tradeSnapshot(1, 501, "", 0, false).vatReclaimCr == 0.0 && worth(1) > before + 0.5 * vatBack,
          "the input VAT was refunded after 30 days (+" + std::to_string(worth(1) - before) + " Cr)");
}

void testServer()
{
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  server run (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    s.setFuelFreeOfCharge(true);   // the data now charges fuel (author 2026-11-19 later); this part tests the free switch
    const auto* l = s.ledger(1);
    check(l && l->homeBank == "sol_authority", "a player starting in Sol banks with the Sol Authority");

    const auto unknown = s.quoteFuel("dev_unlisted_dock", 0, FuelKind::JumpCharge);
    check(unknown.available && unknown.systemReference && unknown.polity == "sol_authority",
          "a runtime hub without market data gets the system's J-charge price (" + unknown.hubName + ")");
    auto q = s.quoteFuel("earth_orbital_hub", 0, FuelKind::JumpCharge);
    check(q.available && !q.systemReference && q.listCrPerT > 1000.0, "earth_orbital_hub sells J charge from its own market");
    check(q.chargedLocalPerT == 0.0 && q.freeOfCharge, "fuel free of charge: charged 0, list price still computed (" +
                                                           std::to_string(q.listCrPerT) + " Cr/t)");
    const auto& b = q.breakdown;
    check(near(b.feedstockCr + b.conversionCr + b.separationEnergyCr + b.fabricationCr + b.transportCr + b.locationCr + b.scarcityCr +
                   b.stationMarginCr,
               q.listCrPerT, 1e-6),
          "server fuel breakdown sums to the list price");

    const double before = netWorthCr(*s.ledger(1), s.banks());
    auto f = s.chargeFuel(1, "earth_orbital_hub", 0, FuelKind::JumpCharge, 3.0);
    check(f.ok && f.tonnesGranted == 3.0 && f.chargedCr == 0.0 && near(netWorthCr(*s.ledger(1), s.banks()), before, 1e-6) &&
              s.ledger(1)->freeFuelListCr > 0.0,
          "free refuel: 3 t granted, nothing charged, list value recorded");
    s.setFuelFreeOfCharge(false);
    f = s.chargeFuel(1, "earth_orbital_hub", 0, FuelKind::JumpCharge, 1.0);
    check(f.ok && f.chargedCr > 1000.0 && netWorthCr(*s.ledger(1), s.banks()) < before - 1000.0, "charged refuel debits the wallet");
    s.setFuelFreeOfCharge(true);

    const double q0 = q.listCrPerT;
    for (int day = 1001; day <= 1365; ++day)
        s.advanceTo(day);
    check(s.day() == 1365, "365 daily corrections ran (3 per call max, one call per day here)");
    const auto q1 = s.quoteFuel("earth_orbital_hub", 0, FuelKind::JumpCharge);
    check(q1.listCrPerT > 0.6 * q0 && q1.listCrPerT < 1.6 * q0,
          "J-charge price after a year stays plausible (" + std::to_string(q0) + " -> " + std::to_string(q1.listCrPerT) + " Cr/t)");

    const auto w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    check(w.valid && w.localCurrency == "CR" && w.fuelQuoteValid && !w.accounts.empty() && !w.fuelBreakdown.empty(), "wallet snapshot");

    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    check(t.restoreState(saved) && t.saveState() == saved, "state save / restore round trip is exact");
}

// audit 2026-11-19: ONE fuel price (the station's markup inside the economy's quote) and an atomic state file
void testFuelPriceAndAtomicSave()
{
    EconomyServer s;
    s.load();
    const auto ill = s.quoteFuel("ill.004.sump", 4, FuelKind::JumpCharge);
    check(ill.available && near(ill.stationFactor, 3.0, 1e-9) && near(ill.listCrPerT, 3.0 * ill.breakdown.listCr, 1e-6),
          "an illegal pump's markup (x3) is inside the economy's quote (" + std::to_string(ill.listCrPerT) + " Cr/t)");
    const auto legal = s.quoteFuel("earth_orbital_hub", 0, FuelKind::JumpCharge);
    check(legal.available && near(legal.stationFactor, 1.0, 1e-9) && near(legal.listCrPerT, legal.breakdown.listCr, 1e-6),
          "a legal hub's quote carries no markup");

    // a prepaid refuel cut short: the unused tonnes come back - the money and the stock
    s.setFuelFreeOfCharge(false);
    s.start(1000);
    s.ensurePlayer(7, 0);
    const double worth0 = netWorthCr(*s.ledger(7), s.banks());
    const double stock0 = s.quoteFuel("earth_orbital_hub", 0, FuelKind::JumpCharge).stockT;
    const auto bought = s.chargeFuel(7, "earth_orbital_hub", 0, FuelKind::JumpCharge, 0.5);
    s.refundFuel(7, "earth_orbital_hub", 0, FuelKind::JumpCharge, 0.2, bought.chargedCr / bought.tonnesGranted);
    const double worth1 = netWorthCr(*s.ledger(7), s.banks());
    const double stock1 = s.quoteFuel("earth_orbital_hub", 0, FuelKind::JumpCharge).stockT;
    check(bought.ok && bought.chargedCr > 0.0 && near(worth0 - worth1, bought.chargedCr * 0.3 / 0.5, 1.0) && near(stock0 - stock1, 0.3, 1e-6),
          "prepaid 0.5 t, 0.2 t refunded: paid for 0.3 t, the stock down 0.3 t");

    // a file that fails half-way through the parse changes NOTHING (audit 2026-11-19: fx / markets were restored,
    // the ledgers cleared, then "fresh state" overwrote the file)
    {
        const std::string before = s.saveState();
        nlohmann::json bad = nlohmann::json::parse(before);
        bad["day"] = 777;
        bad["markets"] = {{"earth_orbital_hub", {{"price", nlohmann::json::array()}}}};   // "stock" missing: at() throws
        check(!s.restoreState(bad.dump()) && s.saveState() == before, "a state that fails half-way is rejected and changes nothing");
    }
    namespace fs = std::filesystem;
    const std::string path = "economy_check_state.json";
    fs::remove(path);
    fs::remove_all(path + ".tmp");
    check(writeFileAtomically(path, "A") && writeFileAtomically(path, "BB"), "atomic write: create, then replace");
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    check(text == "BB" && !fs::exists(path + ".tmp"), "atomic write: the new content, no temp file left");
    fs::create_directory(path + ".tmp");   // the temp file cannot be created: the write must fail and keep the old file
    const bool failed = !writeFileAtomically(path, "CCC");
    std::ifstream in2(path, std::ios::binary);
    std::string kept((std::istreambuf_iterator<char>(in2)), std::istreambuf_iterator<char>());
    in2.close();
    check(failed && kept == "BB", "atomic write: a failed write keeps the previous file intact");
    fs::remove_all(path + ".tmp");
    fs::remove(path);

    // an unreadable state file at start is set aside as <file>.bad-<time> and never written over
    {
        const std::string statePath = "economy_check_bad_state.json";
        for (const auto& e : fs::directory_iterator("."))
            if (e.path().filename().string().rfind(statePath + ".bad-", 0) == 0)
                fs::remove(e.path());
        {
            std::ofstream f(statePath, std::ios::binary);
            f << "{\"schema\":\"economy_state/2\",\"markets\":{\"earth_orbital_hub\":{}}}";
        }
#ifdef _WIN32
        _putenv_s("ELITE_ECONOMY_STATE_FILE", statePath.c_str());
#else
        setenv("ELITE_ECONOMY_STATE_FILE", statePath.c_str(), 1);
#endif
        EconomyServer r;
        r.load();
        r.start(1000);
        bool setAside = false;
        std::string keptText;
        for (const auto& e : fs::directory_iterator("."))
            if (e.path().filename().string().rfind(statePath + ".bad-", 0) == 0)
            {
                setAside = true;
                std::ifstream k(e.path(), std::ios::binary);
                keptText.assign((std::istreambuf_iterator<char>(k)), std::istreambuf_iterator<char>());
                k.close();
                fs::remove(e.path());
            }
        check(setAside && keptText.find("economy_state/2") != std::string::npos && !fs::exists(statePath),
              "an unreadable state file is kept as .bad-<time>, not overwritten");
#ifdef _WIN32
        _putenv_s("ELITE_ECONOMY_STATE_FILE", "");
#else
        unsetenv("ELITE_ECONOMY_STATE_FILE");
#endif
        fs::remove(statePath);
    }
}
// ---------------------------------------------------------------- the open port call survives a restart (economy_state/4)
void testPortCallPersistence()
{
    using nlohmann::json;
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  port call persistence (economy data not found from this working directory)\n");
        return;
    }
    const double t0 = 3.2e10;   // the universe clock (s): anchored to real time, it runs on while the server is down
    s.setUniverseTime(t0);
    s.start(1000);
    s.ensurePlayer(1, 0);
    const std::uint64_t ship = 777;
    const std::string call = "call:118:hub_hex_dock_a:52000";
    s.openPortCall(ship, call, "earth_orbital_hub", "hub_hex_dock_a", 1);
    const auto d = s.chargeDocking(1, call, "earth_orbital_hub", 0, 260.0);
    const std::string saved = s.saveState();

    // a restart 10 h later (universe clock): the call is restored with its start, the docking event is a duplicate
    EconomyServer t;
    t.load();
    t.setUniverseTime(t0 + 10.0 * 3600.0);
    t.start(1000);
    const bool restored = t.restoreState(saved);
    const auto* c = t.openPortCall(ship);
    check(d.booked && restored && t.saveState() == saved && c && c->callId == call && c->hubId == "earth_orbital_hub" &&
              c->dockModuleId == "hub_hex_dock_a" && c->playerId == 1 && near(c->sinceUniverseS, t0, 1e-6),
          "economy_state/4: the open port call (call id, berth, player, universe-clock start) survives save / restore byte-exact");
    const double w0 = netWorthCr(*t.ledger(1), t.banks());
    const auto again = t.chargeDocking(1, call, "earth_orbital_hub", 0, 260.0);
    const double hours = t.closePortCall(ship, 1.0);
    const auto b = t.chargeBerth(1, call, "earth_orbital_hub", 0, hours);
    check(again.duplicate && !again.booked && near(hours, 10.0, 1e-9) && b.booked && near(b.feeCr, 150.0, 1e-9) &&
              near(w0 - netWorthCr(*t.ledger(1), t.banks()), 150.0, 0.011) && !t.openPortCall(ship) && t.closePortCall(ship, 1.0) < 0.0,
          "after a restart: no second docking fee; leaving the berth books 10 h (universe clock) -> 150 Cr once, the call closed");

    // an economy_state/3 file (no port calls) still loads; a broken port_calls block is rejected whole
    json old = json::parse(saved);
    old["schema"] = "economy_state/3";
    old.erase("port_calls");
    EconomyServer u;
    u.load();
    u.start(1000);
    const bool oldOk = u.restoreState(old.dump());
    const std::string before = u.saveState();
    json bad = json::parse(saved);
    bad["port_calls"]["open"][0].erase("ship");
    check(oldOk && !u.openPortCall(ship) && !u.restoreState(bad.dump()) && u.saveState() == before,
          "an economy_state/3 file loads (no open calls); a broken port_calls block is rejected and changes nothing");

    // the throttled background save: a second change within 2 s waits, the destructor writes it and joins the writer
    {
        const std::string path = "economy_check_writer_state.json";
        std::filesystem::remove(path);
#ifdef _WIN32
        _putenv_s("ELITE_ECONOMY_STATE_FILE", path.c_str());
#else
        setenv("ELITE_ECONOMY_STATE_FILE", path.c_str(), 1);
#endif
        std::string expect;
        bool firstWritten = false;
        {
            EconomyServer w;
            w.load();
            w.setUniverseTime(t0);
            w.start(1000);
            w.ensurePlayer(1, 0);
            w.openPortCall(ship, call, "earth_orbital_hub", "hub_hex_dock_a", 1);
            w.advanceTo(1000);   // the first save is due at once (handed to the writer)
            w.openPortCall(ship + 1, call + "b", "earth_orbital_hub", "hub_hex_dock_b", 1);
            w.advanceTo(1000);   // within 2 s: only marked dirty
            expect = w.saveState();
            for (int k = 0; k < 200 && !firstWritten; ++k)
            {
                firstWritten = std::filesystem::exists(path);
                if (!firstWritten)
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
        std::ifstream f(path, std::ios::binary);
        const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();
#ifdef _WIN32
        _putenv_s("ELITE_ECONOMY_STATE_FILE", "");
#else
        unsetenv("ELITE_ECONOMY_STATE_FILE");
#endif
        check(firstWritten && text == expect && !std::filesystem::exists(path + ".tmp"),
              "state save: the writer thread wrote the first change; the throttled second one was written on shutdown (" +
                  std::to_string(text.size()) + " B, == saveState(), no .tmp left)");
        std::filesystem::remove(path);
    }

    // TRADE step 17: money moved = written at the next tick, not after the 2 s throttle (a crash keeps the deal)
    {
        const std::string path = "economy_check_urgent_state.json";
        std::filesystem::remove(path);
#ifdef _WIN32
        _putenv_s("ELITE_ECONOMY_STATE_FILE", path.c_str());
#else
        setenv("ELITE_ECONOMY_STATE_FILE", path.c_str(), 1);
#endif
        bool dealOnDisk = false;
        std::size_t lastSize = 0, expectSize = 0;
        double waitedMs = 0.0;
        {
            EconomyServer w;
            w.load();
            w.setUniverseTime(t0);
            w.start(1000);
            w.ensurePlayer(1, 0);
            w.openPortCall(ship, call, "earth_orbital_hub", "hub_hex_dock_a", 1);
            w.advanceTo(1000);   // the first save (throttle window starts now)
            const CargoCapacity cobrett{120.0, 6};
            (void)w.devAddCargo(1, ship, "steel", 20.0, cobrett, "earth_orbital_hub");   // money / cargo moved: urgent
            w.advanceTo(1000);   // the next tick: handed to the writer at once
            const std::string expect = w.saveState();
            const auto t = std::chrono::steady_clock::now();
            while (!dealOnDisk && std::chrono::steady_clock::now() - t < std::chrono::milliseconds(1900))
            {
                std::ifstream f(path, std::ios::binary);
                const std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                dealOnDisk = text == expect;
                lastSize = text.size();
                expectSize = expect.size();
                if (!dealOnDisk)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));   // a polite reader: a reader holding the file blocks the rename (retried by the writer)
            }
            waitedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
        }
#ifdef _WIN32
        _putenv_s("ELITE_ECONOMY_STATE_FILE", "");
#else
        unsetenv("ELITE_ECONOMY_STATE_FILE");
#endif
        check(dealOnDisk && waitedMs < 1900.0, "an urgent save (cargo / money moved) is on disk " + std::to_string(waitedMs) +
                                                   " ms later - inside the 2 s throttle window (file " + std::to_string(lastSize) + " B, expected " + std::to_string(expectSize) + " B)");
        std::filesystem::remove(path);
    }
}
// ---------------------------------------------------------------- contracts (TRADE_DESIGN §3, step 5)
void testContracts()
{
    // the pure model: strict publication days, the NPC half-life
    {
        ContractSeries ser;
        ser.periodDays = 14;
        ser.phaseDay = 5;
        ser.postLeadDays = 7;
        std::vector<int> days;
        for (int d = 1000; d < 1200; ++d)
            if (isPublishDay(ser, d))
                days.push_back(d);
        bool every14 = days.size() >= 14;
        for (std::size_t k = 1; k < days.size(); ++k)
            every14 = every14 && days[k] - days[k - 1] == 14;
        check(every14 && (days[0] + 7 - 5) % 14 == 0 && nextPublishDay(ser, 1000) == days[0] && nextPublishDay(ser, days[0]) == days[0] &&
                  nextPublishDay(ser, days[0] + 1) == days[1],
              "a series publishes strictly on d_k = phase + k x 14 - lead (" + std::to_string(days.size()) + " days in 200, first " +
                  std::to_string(days[0]) + ")");
        const ContractRules r;
        const double busy = npcTakeRatePerDay(r, trafficOf(r, 5515.0), attractivenessOf(r, 2600.0, 2600.0), "open");
        const double frontier = npcTakeRatePerDay(r, trafficOf(r, 2.0), attractivenessOf(r, 2600.0, 2600.0), "open");
        // many seeds: 40 000 postings rolled day after day at the busy rate
        const int n = 40000;
        std::vector<int> takenOn(n, 0);
        for (int i = 0; i < n; ++i)
            for (int d = 1; d <= 60 && takenOn[i] == 0; ++d)
                if (npcTakes("seed-" + std::to_string(i), 5000 + d, busy))
                    takenOn[i] = d;
        int after1 = 0, after2 = 0;
        for (const int t : takenOn)
        {
            after1 += t == 0 || t > 1;
            after2 += t == 0 || t > 2;
        }
        const double s1 = static_cast<double>(after1) / n, s2 = static_cast<double>(after2) / n;
        const double hl1 = std::log(2.0) / -std::log(s1), hl2 = 2.0 * std::log(2.0) / -std::log(s2);
        check(near(busy, 0.5, 1e-9) && near(hl1, 1.386, 0.06) && near(hl2, 1.386, 0.06) && near(std::log(2.0) / frontier, 13.86, 0.05),
              "NPC haulers: a fair posting at a busy hub (5 515 arrivals/day) lambda 0.5/day -> half-life " + std::to_string(hl1) + " d (1 day) / " +
                  std::to_string(hl2) + " d (2 days) over 40 000 seeds (~1.39); a frontier post (2 arrivals) 13.9 d");
        ContractSeries p;
        p.project = true;
        p.projectStart = 100;
        p.projectWorkDays = 50;
        p.halts = {{120, 130}, {200, INT_MIN}};
        check(!projectBuilding(p, 99) && projectBuilding(p, 100) && !projectBuilding(p, 125) && projectBuilding(p, 130) &&
                  projectBuilding(p, 159) && !projectBuilding(p, 160) && !projectBuilding(p, 205),
              "a construction series runs only while its project is building (start, work days, a halt pauses it)");
    }

    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  contracts (economy data not found from this working directory)\n");
        return;
    }
    const int day0 = s.contracts().rules().constructionEpochDay + 322;   // 3126-11-19
    s.start(day0);
    std::size_t openAtStart = 0;
    for (const auto& p : s.contracts().postings())
        openAtStart += p.status == "open" && p.postedDay <= day0 && p.expiresDay > day0;
    check(openAtStart > 50, "a fresh server starts with today's boards (" + std::to_string(openAtStart) +
                                " open postings: the last board-life days replayed), not empty until the next day");
    const auto stepTo = [&](int d) {
        while (s.day() < d)
            s.advanceTo(d);
    };
    std::size_t supply = 0, crew = 0, build = 0;
    const ContractSeries* europa = nullptr;
    for (const auto& ser : s.contracts().series())
    {
        (ser.kind == "supply" ? supply : ser.kind == "crew_shift" ? crew : build) += 1;
        if (ser.id == "SER-site.000.sol.europa-supply")
            europa = &ser;
    }
    check(supply > 100 && crew > 100 && build > 100 && europa && europa->originHub == "earth_hub.tikhiy" && europa->periodDays == 14 &&
              near(europa->tonnesPerPosting, 40.006 * 14.0 / 30.0, 0.01),
          "series from the data: " + std::to_string(supply) + " supply, " + std::to_string(crew) + " crew shift, " + std::to_string(build) +
              " construction; Europa (Конамара): supply 18.67 t every 14 days from Тихий");

    // every day: the postings of the day are exactly the series due that day (and nothing else)
    std::map<std::string, const ContractSeries*> byId;
    for (const auto& ser : s.contracts().series())
        byId[ser.id] = &ser;
    bool exact = true;
    std::vector<int> europaDays;
    int busyRolled = 0, busyTaken = 0;
    std::string why;
    for (int d = day0 + 1; d <= day0 + 120; ++d)
    {
        stepTo(d);
        int expected = 0, got = 0;
        for (const auto& ser : s.contracts().series())
            if (isPublishDay(ser, d) && projectBuilding(ser, d - s.contracts().rules().constructionEpochDay))
                expected += std::max(1, ser.postings);
        for (const auto& p : s.contracts().postings())
        {
            if (p.postedDay == d)
            {
                ++got;
                const auto it = byId.find(p.seriesId);
                if (it == byId.end() || !isPublishDay(*it->second, d))
                {
                    exact = false;
                    why = p.id;
                }
                if (p.seriesId == "SER-site.000.sol.europa-supply")
                    europaDays.push_back(d);
            }
            // the busy-hub NPC roll as it happened on the real board: the postings rolled for the first time today
            if (p.postedDay == d - 1 && near(p.npcRatePerDay, 0.5, 1e-9))
            {
                ++busyRolled;
                busyTaken += p.status == "npc" && p.closedDay == d;
            }
        }
        if (got != expected)
        {
            exact = false;
            why = "day " + std::to_string(d) + ": " + std::to_string(got) + " vs " + std::to_string(expected);
        }
    }
    bool europa14 = europaDays.size() >= 8;
    for (std::size_t k = 1; k < europaDays.size(); ++k)
        europa14 = europa14 && europaDays[k] - europaDays[k - 1] == 14;
    check(exact && europa14, "120 days of the server board: every posting appears exactly on its series' d_k (" + why +
                                 "); Europa supply on " + std::to_string(europaDays.size()) + " days, 14 apart");
    const double pTaken = busyRolled ? static_cast<double>(busyTaken) / busyRolled : 0.0;
    const double hlBoard = std::log(2.0) / -std::log(1.0 - pTaken);
    check(busyRolled > 300 && near(hlBoard, 1.386, 0.25),
          "the real board: " + std::to_string(busyTaken) + " of " + std::to_string(busyRolled) +
              " fair postings at busy hubs taken by NPC on their first day -> half-life " + std::to_string(hlBoard) + " d (~1.4)");

    // ---- take -> deposit held, advance paid, cargo aboard as the issuer's lot
    const auto worth = [&](std::uint64_t p) { return netWorthCr(*s.ledger(p), s.banks()); };
    const auto held = [&](std::uint64_t p) { return heldCr(*s.ledger(p), s.banks()); };
    const auto debt = [&](std::uint64_t p) { return totalDebtCr(*s.ledger(p), s.banks()); };
    const auto pick = [&](const std::string& kind, double minDeposit) -> const ContractPosting* {
        for (const auto& p : s.contracts().postings())
            if (p.status == "open" && p.kind == kind && p.depositCr >= minDeposit && p.postedDay == s.day() && p.depositCr < 12000.0 &&
                p.originHub != p.destinationHub)
                return &p;
        return nullptr;
    };
    CargoCapacity cap;
    cap.netT = 400.0;
    cap.c20Slots = 20;
    const auto order = [&](std::uint64_t player, std::uint64_t ship, const ContractPosting& p) {
        EconomyServer::ContractOrder o;
        o.playerId = player;
        o.shipKey = ship;
        o.hubId = p.originHub;
        o.systemId = -1;
        o.contractId = p.id;
        o.requestId = 7;
        o.capacity = cap;
        return o;
    };
    // a day with a construction posting with a deposit
    const ContractPosting* found = nullptr;
    for (int k = 0; k < 14 && !(found = pick("construction", 1.0)); ++k)
        stepTo(s.day() + 1);
    if (!found)
    {
        check(false, "no construction posting with a deposit in 14 days");
        return;
    }
    const ContractPosting posted = *found;
    s.ensurePlayer(1, 0);
    double w0 = worth(1);
    EconomyServer::ContractOrder wrongHub = order(1, 501, posted);
    wrongHub.hubId = posted.destinationHub;
    const auto refusedAway = s.takeContract(wrongHub);
    const auto taken = s.takeContract(order(1, 501, posted));
    const CargoManifest* m = s.cargo().find(501);
    bool lotOk = m && m->lots.size() == 1 && m->lots[0].contractId == posted.id && m->lots[0].owner.rfind("issuer:", 0) == 0 &&
                 near(m->lots[0].tonnes, posted.tonnes, 1e-9);
    check(!refusedAway.ok && refusedAway.code == "not_docked" && taken.ok && near(held(1), posted.depositCr, 0.02) &&
              near(worth(1) - w0, posted.advanceCr, 0.02) && lotOk && s.contracts().posting(posted.id)->status == "taken",
          "take " + posted.id + " (" + posted.goodId + " " + std::to_string(posted.tonnes) + " t " + posted.originHub + " -> " +
              posted.destinationHub + "): only at the origin's berth; deposit " + std::to_string(posted.depositCr) + " held (" +
              std::to_string(held(1)) + "), advance +" + std::to_string(posted.advanceCr) + ", the issuer's lot aboard");
    s.ensurePlayer(2, 0);
    const auto again = s.takeContract(order(2, 502, posted));
    check(!again.ok && again.code == "taken", "a shared board: the posting is gone for everyone else (first come, first served)");

    // ---- contract cargo cannot be sold
    EconomyServer::TradeOrder sell;
    sell.buy = false;
    sell.playerId = 1;
    sell.shipKey = 501;
    sell.hubId = posted.destinationHub;
    sell.goodId = posted.goodId;
    sell.tonnes = 5.0;
    sell.capacity = cap;
    const auto sold = s.trade(sell);
    check(!sold.ok && sold.code == "contract_cargo" && near(s.cargo().find(501)->netT(), posted.tonnes, 1e-9),
          "selling the contract's " + posted.goodId + " is refused (\"" + sold.message + "\"), the lot stays aboard");

    // ---- deliver on time: reward - advance, deposit released
    w0 = worth(1);
    const int handed = s.deliverContracts(1, 501, posted.destinationHub, -1);
    const Contract* done = nullptr;
    for (const auto& c : s.contracts().contracts())
        if (c.p.id == posted.id)
            done = &c;
    check(handed == 1 && done && done->status == "delivered" && near(worth(1) - w0, posted.rewardCr - posted.advanceCr, 0.05) &&
              near(held(1), 0.0, 1e-9) && s.cargo().find(501)->lots.empty() && s.takeCargoChange(501),
          "hand-in on docking at the destination in time: +" + std::to_string(worth(1) - w0) + " Cr (reward " + std::to_string(posted.rewardCr) +
              " - advance), deposit released, the lot left the manifest");

    // ---- late: reward x (1 - 0.1 x d), the cut is a penalty (never refused)
    const ContractPosting* late = nullptr;
    for (int k = 0; k < 14 && !(late = pick("supply", 1.0)); ++k)
        stepTo(s.day() + 1);
    if (!late)
    {
        check(false, "no supply posting with a deposit in 14 days");
        return;
    }
    const ContractPosting latePosted = *late;
    s.ensurePlayer(3, 0);
    check(s.takeContract(order(3, 503, latePosted)).ok, "take supply " + latePosted.id + " (" + std::to_string(latePosted.tonnes) + " t kit)");
    stepTo(latePosted.deadlineDay + 2);
    w0 = worth(3);
    (void)s.deliverContracts(3, 503, latePosted.destinationHub, -1);
    const Contract* lateC = nullptr;
    for (const auto& c : s.contracts().contracts())
        if (c.p.id == latePosted.id)
            lateC = &c;
    const double lateCut = std::round(latePosted.rewardCr * 0.1 * 2.0);
    check(lateC && lateC->status == "late" && lateC->lateDays == 2 && near(lateC->penaltyCr, lateCut, 1e-9) &&
              near(worth(3) - w0, latePosted.rewardCr - latePosted.advanceCr - lateCut, 0.05) && near(held(3), 0.0, 1e-9),
          "2 days late: payout " + std::to_string(worth(3) - w0) + " = reward - advance - 20 % (" + std::to_string(lateCut) +
              " Cr penalty), deposit released");

    // ---- failure: deadline + 5 days -> advance back + penalty (a broke pilot: debt, never refused), deposit forfeited
    const ContractPosting* failP = nullptr;
    for (int k = 0; k < 14 && !(failP = pick("supply", 1.0)); ++k)
        stepTo(s.day() + 1);
    if (!failP)
    {
        check(false, "no supply posting to fail");
        return;
    }
    const ContractPosting failPosted = *failP;
    s.ensurePlayer(4, 0);
    check(s.takeContract(order(4, 504, failPosted)).ok, "take " + failPosted.id + " to fail it");
    // spend everything free (the deposit stays blocked): the penalty cannot be paid
    (void)s.chargeService(4, failPosted.originHub, -1, Purpose::Other,
                          s.tradeSnapshot(4, 504, failPosted.originHub, -1, true).spendableCr - 0.5, "test: broke");
    const double balance0 = worth(4), debt0 = debt(4), held0 = held(4);
    stepTo(failPosted.deadlineDay + 6);
    const Contract* failC = nullptr;
    for (const auto& c : s.contracts().contracts())
        if (c.p.id == failPosted.id)
            failC = &c;
    const double owed = failPosted.penaltyCr + failPosted.advanceCr;
    const bool lotGone = !s.cargo().find(504) || s.cargo().find(504)->lots.empty();
    check(failC && failC->status == "failed" && near(held0, failPosted.depositCr, 0.02) && near(held(4), 0.0, 1e-9) &&
              near(debt(4) - debt0, owed, 0.5) && lotGone && s.takeCargoChange(504) &&
              near(balance0 - worth(4), failPosted.depositCr + owed, 1.0 + 0.002 * (failPosted.depositCr + owed)),
          "failed (deadline + 5 days): deposit " + std::to_string(failPosted.depositCr) + " forfeited, penalty + advance back " +
              std::to_string(owed) + " booked as debt (" + std::to_string(debt(4) - debt0) + ") - not refused; the lot seized");

    // ---- abandon away from the origin = failure; at the origin = a cancel fee, the deposit back, the posting reopened
    const ContractPosting* abP = nullptr;
    for (int k = 0; k < 14 && !(abP = pick("construction", 1.0)); ++k)
        stepTo(s.day() + 1);
    if (!abP)
    {
        check(false, "no posting to abandon");
        return;
    }
    const ContractPosting abPosted = *abP;
    s.ensurePlayer(5, 0);
    check(s.takeContract(order(5, 505, abPosted)).ok, "take " + abPosted.id + " to abandon it");
    EconomyServer::ContractOrder away = order(5, 505, abPosted);
    away.hubId.clear();   // in flight
    w0 = worth(5);
    const auto ab = s.abandonContract(away);
    check(ab.ok && ab.code == "contract_abandoned" && near(held(5), 0.0, 1e-9) &&
              near(w0 - worth(5), abPosted.depositCr + abPosted.penaltyCr + abPosted.advanceCr, 0.5),
          "abandoned in flight: deposit " + std::to_string(abPosted.depositCr) + " forfeited + penalty " + std::to_string(abPosted.penaltyCr) +
              " + advance back (" + std::to_string(w0 - worth(5)) + " Cr)");
    const ContractPosting* cP = nullptr;
    for (const auto& p : s.contracts().postings())
        if (!cP && p.status == "open" && p.depositCr > 0.0 && p.postedDay == s.day() && p.depositCr < 12000.0)
            cP = &p;
    if (cP)
    {
        const ContractPosting cPosted = *cP;
        s.ensurePlayer(6, 0);
        const double w6 = worth(6);
        (void)s.takeContract(order(6, 506, cPosted));
        const auto cancel = s.abandonContract(order(6, 506, cPosted));
        const double fee = std::round(cPosted.rewardCr * 0.1);
        check(cancel.ok && cancel.code == "contract_cancelled" && near(held(6), 0.0, 1e-9) && near(w6 - worth(6), fee, 0.05) &&
                  s.contracts().posting(cPosted.id)->status == "open",
              "abandoned at the origin before loading out: a 10 % fee (" + std::to_string(w6 - worth(6)) + " Cr net), deposit released, back on the board");
    }

    // ---- save / restore: exact round trip (the boards, the contracts, the holds in the ledgers)
    s.ensurePlayer(7, 0);
    const ContractPosting* keepP = nullptr;
    for (const auto& p : s.contracts().postings())
        if (!keepP && p.status == "open" && p.depositCr > 0.0 && p.depositCr < 12000.0)
            keepP = &p;
    if (keepP)
        (void)s.takeContract(order(7, 507, *keepP));
    const std::string saved = s.saveState();
    EconomyServer t;
    (void)t.load();
    t.start(day0);
    const bool restored = t.restoreState(saved);
    std::size_t active = 0;
    for (const auto& c : t.contracts().contracts())
        active += c.status == "active";
    check(restored && t.saveState() == saved && active >= 1 && t.contracts().postings().size() == s.contracts().postings().size() &&
              near(heldCr(*t.ledger(7), t.banks()), heldCr(*s.ledger(7), s.banks()), 1e-9),
          "save / restore round trip is exact (" + std::to_string(saved.size()) + " bytes, " + std::to_string(t.contracts().postings().size()) +
              " postings, " + std::to_string(active) + " active contract(s), the deposit hold kept)");
    // a malformed contracts block leaves the live state untouched (temporaries, then commit)
    nlohmann::json bad = nlohmann::json::parse(saved);
    bad["contracts"]["contracts"][0]["posting"] = 42;
    const bool rejected = !t.restoreState(bad.dump());
    check(rejected && t.saveState() == saved, "a broken contracts block is rejected whole: the live board / ledgers unchanged");
}

} // namespace

// trade step 3: the F8 deal window prices a deal with the server's TradeQuoteModel on the replicated inputs - its
// breakdown sums to its total, the total is what EconomyServer::trade charges, its limits are the server's refusals
void testTradeDealWindow()
{
    using game::presentation::buildTradeDeal;
    using game::presentation::TradeDealRequest;
    {
        long long total = 0;
        const auto shown = game::presentation::roundLinesToTotal({-9049.62, -181.74, -1846.27}, total);
        check(total == -11078 && shown[0] + shown[1] + shown[2] == total, "whole-Cr lines add up to the rounded total (largest remainder)");
    }
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  deal window (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    s.ensurePlayer(2, 0);
    s.credit(1, "earth_orbital_hub", 0, 200000.0, "test: a trader with money");
    const CargoCapacity cobrett{120.0, 6};
    const auto worth = [&](std::uint64_t p) { return netWorthCr(*s.ledger(p), s.banks()); };
    const auto snapshot = [&](std::uint64_t player, std::uint64_t ship) {
        auto snap = s.tradeSnapshot(player, ship, "earth_orbital_hub", 0, true);
        snap.c20Slots = cobrett.c20Slots;   // the hull figures GameServer::tradeForSession adds
        snap.netCapacityT = cobrett.netT;
        return snap;
    };
    int grain = -1;
    const auto& goods = EconomyModel::instance().goods();
    for (std::size_t k = 0; k < goods.size(); ++k)
        if (goods[k].id == "food_staples")
            grain = static_cast<int>(k);
    const auto order = [&](const game::presentation::TradeDealView& v, std::uint64_t player, std::uint64_t ship, int day) {
        EconomyServer::TradeOrder o;
        o.buy = v.buy;
        o.playerId = player;
        o.shipKey = ship;
        o.hubId = "earth_orbital_hub";
        o.systemId = 0;
        o.goodId = v.goodId;
        o.tonnes = v.tonnes;
        o.limitUnitCr = v.quote.unitCr;   // what the window sends: the exact quoted unit price
        o.quoteDay = day;
        o.capacity = cobrett;
        o.requestId = 100;
        return o;
    };
    const auto sums = [](const game::presentation::TradeDealView& v) {
        double sum = 0.0;
        long long shown = 0;
        for (const auto& l : v.lines)
        {
            sum += l.cr;
            shown += l.shownCr;
        }
        return std::fabs(sum - v.quote.totalCr) < 1e-9 && shown == v.shownTotalCr && v.shownTotalCr == std::llround(v.quote.totalCr);
    };

    // buy 2 C20 of grain: the preview = the charge
    TradeDealRequest req;
    req.good = grain;
    req.buy = true;
    req.tonnes = 2.0 * c20NetTonnes(0.65);
    auto snap = snapshot(1, 601);
    auto v = buildTradeDeal(snap, req);
    check(v.valid && v.canConfirm && v.lines.size() == 3 && sums(v),
          "deal window (buy 2 C20 grain): goods + handling + VAT = total " + std::to_string(v.quote.totalCr) + " Cr, shown lines add up");
    check(near(v.holdLimitT, 6.0 * c20NetTonnes(0.65), 1e-4) && v.limitKind == "hold_c20" && near(v.maxT, v.holdLimitT, 1e-9),
          "empty Cobrett: the slider stops at 6 C20 of grain (" + std::to_string(v.maxT) + " t)");
    double w0 = worth(1);
    auto r = s.trade(order(v, 1, 601, snap.day));
    check(r.ok && r.totalCr == v.quote.totalCr && near(worth(1) - w0, v.quote.totalCr, 0.02) && r.lines.size() == v.lines.size(),
          "the server charged exactly the window's total (" + std::to_string(r.totalCr) + " vs " + std::to_string(v.quote.totalCr) + ")");

    // after the deal: the limits follow the manifest (today's lot merges); the max fits, a hair more does not
    snap = snapshot(1, 601);
    req.tonnes = 1.0e9;
    v = buildTradeDeal(snap, req);
    check(near(v.holdLimitT, 4.0 * c20NetTonnes(0.65), 1e-4) && near(v.aboardT, 2.0 * c20NetTonnes(0.65), 1e-9),
          "2 C20 aboard: 4 C20 left (" + std::to_string(v.holdLimitT) + " t)");
    auto o = order(v, 1, 601, snap.day);
    o.tonnes += 0.01;
    o.limitUnitCr = 0.0;
    const auto over = s.trade(o);
    check(!over.ok && over.code == "no_space", "the server refuses the window's max + 0.01 t (" + over.code + ")");

    // money: a nearly broke player - the window's money limit is what pay() covers (incl. overdraft, fees)
    (void)s.chargeService(2, "earth_orbital_hub", 0, Purpose::Other, snapshot(2, 602).spendableCr - 3000.0, "test: spent down");
    auto snap2 = snapshot(2, 602);
    TradeDealRequest broke = req;
    broke.tonnes = 1.0e9;
    auto v2 = buildTradeDeal(snap2, broke);
    check(near(snap2.spendableCr, 3000.0, 0.02) && v2.limitKind == "money" && v2.moneyLimitT > 0.5 && -v2.quote.totalCr <= snap2.spendableCr,
          "3 000 Cr spendable: the slider stops at " + std::to_string(v2.maxT) + " t (" + std::to_string(-v2.quote.totalCr) + " Cr)");
    o = order(v2, 2, 602, snap2.day);
    o.tonnes += 0.5;
    o.limitUnitCr = 0.0;
    const auto tooMuch = s.trade(o);
    w0 = worth(2);
    r = s.trade(order(v2, 2, 602, snap2.day));
    check(!tooMuch.ok && tooMuch.code == "no_money" && r.ok && r.totalCr == v2.quote.totalCr && near(worth(2) - w0, r.totalCr, 0.02),
          "the server takes the window's money max and refuses 0.5 t more (" + tooMuch.code + ")");

    // the market moved between the quote and Confirm: "price_moved", the re-quote then goes through at the new price
    snap = snapshot(1, 601);
    req.tonnes = c20NetTonnes(0.65);
    v = buildTradeDeal(snap, req);
    {
        s.ensurePlayer(3, 0);
        EconomyServer::TradeOrder other = order(v, 3, 603, snap.day);
        other.tonnes = 3.0;
        other.limitUnitCr = 0.0;
        (void)s.trade(other);   // someone else buys first: less stock, dearer
    }
    const auto moved = s.trade(order(v, 1, 601, snap.day));
    snap = snapshot(1, 601);
    req.sentRequest = 100;
    req.sentUnitCr = v.quote.unitCr;
    const auto again = buildTradeDeal(snap, req);
    check(!moved.ok && moved.code == "price_moved" && again.answer == game::presentation::TradeDealAnswer::PriceMoved &&
              again.quote.unitCr > v.quote.unitCr,
          "a moved market: price_moved, the window shows " + std::to_string(again.quote.unitCr) + " instead of " +
              std::to_string(v.quote.unitCr));
    r = s.trade(order(again, 1, 601, snap.day));
    check(r.ok && r.totalCr == again.quote.totalCr, "the one-key re-quote is charged exactly its new total");

    // sell everything aboard: the window's sell total = the server's credit
    snap = snapshot(1, 601);
    TradeDealRequest sell;
    sell.good = grain;
    sell.buy = false;
    sell.tonnes = 1.0e9;
    v = buildTradeDeal(snap, sell);
    w0 = worth(1);
    r = s.trade(order(v, 1, 601, snap.day));
    check(v.limitKind == "aboard" && near(v.tonnes, 3.0 * c20NetTonnes(0.65), 1e-9) && sums(v) && r.ok && r.totalCr == v.quote.totalCr &&
              near(worth(1) - w0, r.totalCr, 0.02) && s.cargo().find(601)->lots.empty(),
          "sell all 3 C20 (" + v.limitKind + ", " + std::to_string(v.tonnes) + " t, " + (r.ok ? "ok" : r.code) + ", server " +
              std::to_string(r.totalCr) + "): revenue - unloading - VAT (+ turnover) = " + std::to_string(v.quote.totalCr) + " Cr, credited exactly");
}

// ---- trade step 4: port fees (docking + berth) and the import duty (docs/TRADE_DESIGN.md §5.2, §9 row 4)
void testPortFeesAndDuty()
{
    using game::presentation::buildTradeDeal;
    using game::presentation::TradeDealRequest;
    // the pure formulas
    {
        FiscalStyle bureau;   // port index 1.0
        FiscalStyle corp;
        corp.portIndex = 1.4;
        const TradeDealRules r;
        check(near(dockingFeeCr(260.0, bureau, r), 384.0, 1e-9) && near(dockingFeeCr(260.0, corp, r), 384.0 * 1.4, 1e-9),
              "docking fee of a Cobrett (260 t dry): 150 + 0.9 x 260 = 384 Cr x the port index (corporate 537.6)");
        check(near(berthFeeCr(0.5, bureau, r), 0.0, 1e-12) && near(berthFeeCr(4.0, bureau, r), 0.0, 1e-12) &&
                  near(berthFeeCr(10.0, bureau, r), 150.0, 1e-9) && near(berthFeeCr(28.0, bureau, r), 400.0, 1e-9) &&
                  near(berthFeeCr(64.0, bureau, r), 400.0 * 2 + 300.0, 1e-9) && near(berthFeeCr(10.0, corp, r), 210.0, 1e-9),
              "berth fee: 4 h included, then 25 Cr/h, <= 400 Cr per 24 h, x the port index");
    }
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  port fees / duty (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    const auto worth = [&](std::uint64_t p) { return netWorthCr(*s.ledger(p), s.banks()); };
    const auto debt = [&](std::uint64_t p) { return totalDebtCr(*s.ledger(p), s.banks()); };
    const auto lastEvent = [&](std::uint64_t p) { return s.ledger(p)->events.empty() ? std::string() : s.ledger(p)->events.back().text; };

    // one Cobrett call at Earth orbit (Sol: bureaucratic, index 1.0): 384 Cr, once
    double w0 = worth(1);
    const auto d1 = s.chargeDocking(1, "call:7:dock_a:1000", "earth_orbital_hub", 0, 260.0);
    const auto d1again = s.chargeDocking(1, "call:7:dock_a:1000", "earth_orbital_hub", 0, 260.0);
    check(d1.booked && near(d1.feeCr, 384.0, 1e-9) && near(d1.paidCr, 384.0, 1e-9) && d1again.duplicate && !d1again.booked &&
              near(w0 - worth(1), 384.0, 0.011) && lastEvent(1).find("стыковка") != std::string::npos,
          "docking at Earth orbit: -384 Cr once, the repeated event books nothing (ledger: \"" + lastEvent(1) + "\")");
    const auto b1 = s.chargeBerth(1, "call:7:dock_a:1000", "earth_orbital_hub", 0, 3.5);
    const auto b1again = s.chargeBerth(1, "call:7:dock_a:1000", "earth_orbital_hub", 0, 30.0);
    check(near(b1.feeCr, 0.0, 1e-12) && !b1.booked && b1again.duplicate && near(w0 - worth(1), 384.0, 0.011),
          "3.5 h at the berth: within the included 4 h, 0 Cr; a repeated release books nothing - the call cost 384 Cr in all");
    w0 = worth(1);
    const auto d2 = s.chargeDocking(1, "call:7:dock_a:9000", "earth_orbital_hub", 0, 260.0);
    const auto b2 = s.chargeBerth(1, "call:7:dock_a:9000", "earth_orbital_hub", 0, 10.0);
    check(d2.booked && b2.booked && near(b2.feeCr, 150.0, 1e-9) && near(w0 - worth(1), 534.0, 0.011) &&
              lastEvent(1).find("стоянка") != std::string::npos,
          "a 10 h call: docking 384 + berth 6 h x 25 = 150 -> 534 Cr (ledger: \"" + lastEvent(1) + "\")");

    // unpaid -> debt, never a refusal (the fee must not strand the ship)
    s.ensurePlayer(2, 0);
    (void)s.chargeService(2, "earth_orbital_hub", 0, Purpose::Other, s.tradeSnapshot(2, 0, "earth_orbital_hub", 0, true).spendableCr - 0.5,
                          "test: broke");
    const double debt0 = debt(2);
    const double spend0 = s.tradeSnapshot(2, 0, "earth_orbital_hub", 0, true).spendableCr;
    const auto d3 = s.chargeDocking(2, "call:8:dock_a:1", "earth_orbital_hub", 0, 260.0);
    const auto b3 = s.chargeBerth(2, "call:8:dock_a:1", "earth_orbital_hub", 0, 12.0);
    check(spend0 < 1.0 && d3.booked && near(d3.debtCr, 384.0, 1e-9) && b3.booked && near(b3.debtCr, 200.0, 1e-9) &&
              near(debt(2) - debt0, 584.0, 0.02) && lastEvent(2).find("сбор в долг") != std::string::npos,
          "a broke pilot: docking 384 + berth 200 become a 0 % debt (" + std::to_string(debt(2) - debt0) + " Cr), not a refusal");

    // the import duty x the customs channel
    check(near(s.importDutyRate("earth_orbital_hub", "earth_hub.tikhiy", 0), 0.0, 1e-12) &&
              near(s.importDutyRate("dev", "earth_orbital_hub", 0), 0.0, 1e-12),
          "no duty inside one polity (Earth orbit -> Тихий) or for an unknown origin");
    const auto& hubs = EconomyModel::instance().hubs();
    const std::string sol = s.polityOf("earth_orbital_hub", 0);
    const auto* solBank = s.banks().bankForPolity(sol);
    std::string foreign;
    int foreignSystem = -1;
    Channel foreignChannel = Channel::Open;
    int checkedRates = 0, wrongRates = 0;
    bool channels[5] = {};
    for (const auto& h : hubs)
    {
        MarketSide m;
        if (!s.marketSide(h.id, "food_staples", m))
            continue;
        const std::string pol = s.polityOf(h.id, h.systemId);
        if (pol.empty() || pol == sol)
            continue;
        const auto* bank = s.banks().bankForPolity(pol);
        if (!bank || !solBank)
            continue;
        const Channel ch = s.banks().channel(solBank->id, bank->id);
        const double expect = s.fiscalStyleOf(h.id, h.systemId).dutyShare * s.customsFeeMult(ch);
        ++checkedRates;
        wrongRates += near(s.importDutyRate("earth_orbital_hub", h.id, h.systemId), expect, 1e-12) ? 0 : 1;
        channels[static_cast<int>(ch)] = true;
        if ((foreign.empty() || (foreignChannel == Channel::Open && ch != Channel::Open)) && m.stockT < 1.2 * m.targetT)
        {
            foreign = h.id;
            foreignSystem = h.systemId;
            foreignChannel = ch;
        }
    }
    check(checkedRates > 0 && wrongRates == 0 && near(s.customsFeeMult(Channel::Open), 1.0, 1e-12) &&
              near(s.customsFeeMult(Channel::Licensed), 1.5, 1e-12) && near(s.customsFeeMult(Channel::Restricted), 2.5, 1e-12) &&
              near(s.customsFeeMult(Channel::Black), 4.0, 1e-12),
          "duty = the selling hub's style rate x the channel multiplier 1 / 1.5 / 2.5 / 4 (" + std::to_string(checkedRates) +
              " foreign grain markets, channels open " + (channels[0] ? "y" : "n") + " licensed " + (channels[1] ? "y" : "n") +
              " restricted " + (channels[2] ? "y" : "n") + ")");
    if (foreign.empty())
    {
        check(false, "a foreign grain market to sell Sol grain at");
        return;
    }

    // the window shows the duty line and the server charges exactly the window's total
    s.credit(1, "earth_orbital_hub", 0, 200000.0, "test: a trader with money");
    const CargoCapacity cobrett{120.0, 6};
    EconomyServer::TradeOrder o;
    o.playerId = 1;
    o.shipKey = 701;
    o.hubId = "earth_orbital_hub";
    o.systemId = 0;
    o.goodId = "food_staples";
    o.tonnes = c20NetTonnes(0.65);
    o.capacity = cobrett;
    o.requestId = 200;
    check(s.trade(o).ok, "bought 1 C20 of grain in Sol");
    // an older lot from the same polity as the foreign hub (no duty there) + the Sol lot: the FIFO weighs both
    check(s.devAddCargo(1, 701, "food_staples", 5.0, cobrett, foreign).empty(), "plus 5 t of grain loaded at " + foreign);
    int grain = -1;
    const auto& goods = EconomyModel::instance().goods();
    for (std::size_t k = 0; k < goods.size(); ++k)
        if (goods[k].id == "food_staples")
            grain = static_cast<int>(k);
    auto snap = s.tradeSnapshot(1, 701, foreign, foreignSystem, true);
    snap.c20Slots = cobrett.c20Slots;
    snap.netCapacityT = cobrett.netT;
    TradeDealRequest sell;
    sell.good = grain;
    sell.buy = false;
    sell.tonnes = 1.0e9;
    const auto v = buildTradeDeal(snap, sell);
    const double rate = s.importDutyRate("earth_orbital_hub", foreign, foreignSystem);
    const double soldT = c20NetTonnes(0.65) + 5.0;
    bool dutyLine = false;
    double sum = 0.0;
    long long shown = 0;
    for (const auto& l : v.lines)
    {
        dutyLine = dutyLine || (l.kind == "duty" && l.cr < 0.0);
        sum += l.cr;
        shown += l.shownCr;
    }
    check(v.valid && dutyLine && near(v.tonnes, soldT, 1e-9) && near(v.dutyShare, rate * c20NetTonnes(0.65) / soldT, 1e-12) &&
              near(v.quote.dutyCr, v.quote.goodsCr * v.dutyShare, 1e-6) && std::fabs(sum - v.quote.totalCr) < 1e-9 && shown == v.shownTotalCr,
          "the deal window at " + foreign + " (" + channelName(foreignChannel) + "): a duty line " + std::to_string(v.quote.dutyCr) +
              " Cr = goods x " + std::to_string(100.0 * v.dutyShare) + " % (only the Sol lot: " + std::to_string(100.0 * rate) +
              " % on 15.1 of 20.1 t)");
    o.buy = false;
    o.hubId = foreign;
    o.systemId = foreignSystem;
    o.tonnes = v.tonnes;
    o.limitUnitCr = v.quote.unitCr;
    o.quoteDay = snap.day;
    o.requestId = 201;
    w0 = worth(1);
    const auto r = s.trade(o);
    bool serverDuty = false;
    for (const auto& line : r.lines)
        serverDuty = serverDuty || line.rfind("пошлина", 0) == 0;
    bool ledgerDuty = false;
    for (const auto& e : s.ledger(1)->events)
        ledgerDuty = ledgerDuty || e.text.rfind("пошлина", 0) == 0;
    check(r.ok && r.totalCr == v.quote.totalCr && worth(1) - w0 >= (1.0 - s.banks().rules.fxFeeShare) * r.totalCr - 0.05 && serverDuty && ledgerDuty &&
              r.lines.size() == v.lines.size(),
          "the server charged exactly the window's total with the duty (" + std::to_string(r.totalCr) + " vs " +
              std::to_string(v.quote.totalCr) + "), the ledger has a \"пошлина\" line (the account got " + std::to_string(worth(1) - w0) + " Cr after the 1 % FX of a foreign payer)");
}

// the protected minimum (TRADE_DESIGN §1.3, §11 item 6): a garnish of income by a restructured debt never takes the
// client's free money below rules.garnishProtectedCr; the debt falls by exactly what was withheld
void testProtectedMinimum()
{
    BankDirectory d = testBanks();
    d.rules.startAccountCr = 0.0;
    d.rules.startCashCr = 0.0;
    const double M = d.rules.garnishProtectedCr;
    const auto restructured = [&](ClientLedger& l, const std::string& bank, double cr) {
        Loan x;
        x.id = l.nextLoanId++;
        x.bankId = bank;
        x.kind = LoanKind::Restructured;
        x.principal = x.outstanding = toMinor(cr * d.fx(bank));
        x.annualRate = 0.032;
        x.termDays = 720;
        l.loans.push_back(x);
        l.standing = Standing::Restructured;
    };
    const auto debt = [&](const ClientLedger& l) { return totalDebtCr(l, d); };

    ClientLedger l = openLedger(d, 1, "sol", 0);
    restructured(l, "sol", 50000.0);
    receive(l, d, "sol", 3000.0, 1, "sale");
    check(near(freeMoneyCr(l, d), 3000.0, 0.005) && near(debt(l), 50000.0, 0.005) && l.lastWithheldCr == 0.0 && l.lastWithheldDay == 1,
          "below the protected minimum (" + std::to_string(M) + " Cr) nothing is withheld: all 3 000 Cr stay, the debt is unchanged");
    double d0 = debt(l);
    receive(l, d, "sol", 4000.0, 2, "sale");
    check(near(l.lastWithheldCr, 7000.0 - M, 0.005) && near(freeMoneyCr(l, d), M, 0.005) && near(d0 - debt(l), l.lastWithheldCr, 0.005),
          "income across the minimum: only " + std::to_string(l.lastWithheldCr) + " Cr withheld (not 25 % = 1 000), " +
              std::to_string(freeMoneyCr(l, d)) + " Cr stay = the minimum");
    d0 = debt(l);
    receive(l, d, "sol", 20000.0, 3, "sale");
    check(near(l.lastWithheldCr, 5000.0, 0.005) && near(d0 - debt(l), 5000.0, 0.005) && near(freeMoneyCr(l, d), M + 15000.0, 0.005),
          "above the minimum the full 25 % share is withheld (5 000 of 20 000 Cr), the debt falls by exactly that");

    // a random walk: income from the home bank and from a foreign payer (lands home minus 1 % FX), spending, a debt
    // in a foreign currency - after every receipt with a withholding the free money is >= the minimum, and the debt
    // fell by exactly the withheld amount
    ClientLedger r = openLedger(d, 2, "sol", 0);
    restructured(r, "harmony", 80000.0);
    restructured(r, "sol", 30000.0);
    bool floorHolds = true, exact = true, someWithheld = false, someProtected = false;
    for (int day = 1; day <= 400; ++day)
    {
        const double amount = 200.0 + 15000.0 * uniform();
        const double before = debt(r);
        const double freeBefore = freeMoneyCr(r, d);
        receive(r, d, uniform() < 0.5 ? "sol" : "harmony", amount, day, "x");
        const double w = r.lastWithheldDay == day ? r.lastWithheldCr : 0.0;   // no restructured debt left: no garnish today
        floorHolds = floorHolds && (w <= 0.0 || freeMoneyCr(r, d) >= M - 0.02) && freeMoneyCr(r, d) >= freeBefore - 0.02;
        exact = exact && near(before - debt(r), w, 0.01) && w <= 0.25 * amount + 0.005;
        someWithheld = someWithheld || w > 0.0;
        someProtected = someProtected || (w < 0.25 * amount - 0.01 && debt(r) > 0.0);
        if (uniform() < 0.7)   // spend down (fuel, fees) - not below zero
            (void)pay(r, d, "sol", std::min(freeMoneyCr(r, d), 12000.0 * uniform()), Purpose::Trade, day, false, "spend");
    }
    check(floorHolds && someWithheld && someProtected,
          "400 random receipts (CR + HRL payers, CR + HRL debt, spending between): a withholding never leaves less than the minimum "
          "and never lowers the free money");
    check(exact, "every receipt: the debt fell by exactly the withheld amount, never more than 25 % of the income");

    // persistence: economy_state/5 carries the latest withholding; an economy_state/4 file (without it) still loads
    using nlohmann::json;
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  protected minimum persistence (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    ClientLedger* sl = s.ledger(1);
    restructured(*sl, sl->homeBank, 40000.0);
    const double before = debt(*sl);
    receive(*sl, s.banks(), sl->homeBank, 30000.0, 1000, "sale");
    const double withheld = sl->lastWithheldCr;
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(1000);
    const bool restored = t.restoreState(saved);
    const auto w = t.walletSnapshot(1, "earth_orbital_hub", 0);
    check(withheld > 0.0 && near(before - debt(*sl), withheld, 0.005) && restored && t.saveState() == saved && json::parse(saved)["schema"] == "economy_state/14" &&
              t.ledger(1)->lastWithheldDay == 1000 && t.ledger(1)->lastWithheldCr == withheld && near(w.lastWithheldCr, withheld, 1e-9) &&
              near(w.garnishDebtCr, before - withheld, 0.005) && w.protectedCr == s.banks().rules.garnishProtectedCr && w.protectedCr > 0.0,
          "economy_state/5: the latest withholding (" + std::to_string(withheld) + " Cr) survives save / restore byte-exact; the wallet shows debt " +
              std::to_string(w.garnishDebtCr) + ", withheld, protected " + std::to_string(w.protectedCr) + " (national_banks.json)");
    json old = json::parse(saved);
    old["schema"] = "economy_state/4";
    for (auto& lj : old["ledgers"])
        lj.erase("last_withheld");
    EconomyServer u;
    u.load();
    u.start(1000);
    check(u.restoreState(old.dump()) && u.ledger(1) && u.ledger(1)->lastWithheldDay == -1 && u.ledger(1)->lastWithheldCr == 0.0 &&
              near(totalDebtCr(*u.ledger(1), u.banks()), before - withheld, 0.005),
          "an economy_state/4 file (no last_withheld) loads: the debt as saved, no withholding recorded");
}

// TRADE step 7 (TRADE_DESIGN §5.2, §9f): the income tax of the residence polity - the pure book, then the server
void testIncomeTax()
{
    TaxRules rules;   // 90-day periods, due 15 days after, 5 % penalty, 5 % withholding
    TaxBook b;
    check(!rollTaxPeriod(b, 1000, 0.25, 0.02, rules).closed && b.period == 11, "the book opens on its first day (period 11 = days 990..1079)");
    addRevenue(b, 100000.0);
    addExpense(b, 60000.0);
    addWithheld(b, 1000.0, true);
    addWithheld(b, 500.0, false);
    addRevenue(b, -5.0);
    addExpense(b, std::nan(""));
    check(near(estimatedTaxCr(b, 0.25, 0.02), (0.25 * 40000.0 - 1000.0) * 1.02, 1e-9),
          "the estimate: 25 % of the 40 000 profit minus the 1 000 credited abroad, + the 2 % administration fee = 9 180");
    check(!rollTaxPeriod(b, 1079, 0.25, 0.02, rules).closed, "the period's last day does not close it");
    const TaxAssessment a = rollTaxPeriod(b, 1080, 0.25, 0.02, rules);
    check(a.closed && near(a.profitCr, 40000.0, 1e-9) && near(a.taxCr, 9000.0, 1e-9) && near(a.feeCr, 180.0, 1e-9) && a.dueDay == 1095 &&
              !b.settled && b.period == 12 && b.revenueCr == 0.0 && b.expensesCr == 0.0 && b.withheldCreditCr == 0.0,
          "day 1080 closes it: tax 9 000 + fee 180, due day 1095; the new period starts empty");
    check(!taxDue(b, 1094) && taxDue(b, 1095), "due on day 1095, not before");
    const double penalty = settleTax(b, 0.0, 9180.0, rules);
    check(b.settled && near(penalty, 459.0, 1e-9) && near(b.debtCr, 9180.0, 1e-9) && !taxDue(b, 1096),
          "not payable: all 9 180 Cr booked as debt + a one-off 5 % penalty (459 Cr)");
    addRevenue(b, 1000.0);
    addExpense(b, 5000.0);
    const TaxAssessment loss = rollTaxPeriod(b, 1170, 0.25, 0.02, rules);
    check(loss.closed && loss.taxCr == 0.0 && b.settled && !taxDue(b, 1200), "a loss-making period: no tax, nothing due");
    TaxBook zero;
    (void)rollTaxPeriod(zero, 10, 0.0, 0.0, rules);
    addRevenue(zero, 50000.0);
    check(!rollTaxPeriod(zero, 90, 0.0, 0.0, rules).closed || zero.settled, "a 0 % style (theocratic / clan): nothing ever due");

    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  income tax server run (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    s.ensurePlayer(2, 0);
    const CargoCapacity cobrett{120.0, 6};
    const auto worth = [&](std::uint64_t p) { return netWorthCr(*s.ledger(p), s.banks()); };
    const auto sellAll = [&](std::uint64_t p, std::uint64_t ship, double t) {
        EconomyServer::TradeOrder o;
        o.playerId = p;
        o.shipKey = ship;
        o.hubId = "earth_orbital_hub";
        o.systemId = 0;
        o.goodId = "food_staples";
        o.tonnes = t;
        o.buy = false;
        o.capacity = cobrett;
        o.requestId = 7;
        return s.trade(o);
    };
    // DEV goods (cost basis 0): the whole sale is profit
    check(s.devAddCargo(1, 601, "food_staples", 15.0, cobrett, "earth_orbital_hub").empty() &&
              s.devAddCargo(2, 602, "food_staples", 15.0, cobrett, "earth_orbital_hub").empty(), "DEV grain aboard both ships");
    const auto sold = sellAll(1, 601, 15.0);
    (void)sellAll(2, 602, 15.0);
    auto w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    check(sold.ok && near(w.taxRevenueCr, sold.totalCr, 1e-6) && w.taxExpensesCr == 0.0 && near(w.taxRate, 0.25, 1e-9) &&
              near(w.taxAdminFeeShare, 0.02, 1e-9) && w.taxStyleRu == "бюрократический" && w.taxPeriodDays == 90,
          "a sale is revenue (" + std::to_string(sold.totalCr) + " Cr); a Sol resident pays 25 % + 2 % fee (trade_fees.json)");
    const auto dock = s.chargeDocking(1, "call-tax-1", "earth_orbital_hub", 0, 260.0, "");
    w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    check(dock.booked && near(w.taxExpensesCr, dock.feeCr, 1e-6), "the docking fee is a deductible expense");
    const double profit = sold.totalCr - dock.feeCr;
    check(near(w.taxEstimateCr, 0.25 * profit * 1.02, 0.01), "the F5 estimate = 25 % x profit x 1.02");

    (void)s.devSpendAll(2, "earth_orbital_hub", 0);   // player 2 will be broke on the due day
    for (int day = 1001; day <= 1080; ++day)
        s.advanceTo(day);
    w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    check(w.taxAssessedPeriodStartDay == 990 && near(w.taxAssessedCr, 0.25 * profit, 0.01) && near(w.taxAssessedFeeCr, 0.005 * profit, 0.01) &&
              !w.taxSettled && w.taxDueDay == 1095 && w.taxRevenueCr == 0.0,
          "day 1080: the period 990..1079 is assessed (" + std::to_string(w.taxAssessedCr) + " + fee), due day 1095");
    for (int day = 1081; day <= 1094; ++day)
        s.advanceTo(day);
    // measured across the due day only (a day of interest on the accounts is a few Cr)
    const double before1 = worth(1), before2 = worth(2), debt2 = totalDebtCr(*s.ledger(2), s.banks());
    s.advanceTo(1095);
    w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    const double due = w.taxAssessedCr + w.taxAssessedFeeCr;
    check(w.taxSettled && near(w.taxPaidCr, due, 0.01) && w.taxDebtCr == 0.0 && near(before1 - worth(1), due, 10.0),
          "day 1095: player 1 pays " + std::to_string(due) + " Cr at once");
    const auto w2 = s.walletSnapshot(2, "earth_orbital_hub", 0);
    const double due2 = w2.taxAssessedCr + w2.taxAssessedFeeCr;
    check(w2.taxSettled && w2.taxPaidCr == 0.0 && near(w2.taxDebtCr, due2, 0.01) && near(w2.taxPenaltyCr, 0.05 * due2, 0.01) &&
              near(totalDebtCr(*s.ledger(2), s.banks()) - debt2, due2 * 1.05, 10.0) && near(before2 - worth(2), due2 * 1.05, 10.0),
          "a broke player 2: the tax " + std::to_string(due2) + " Cr is booked as debt + a 5 % penalty, never refused");

    // persistence: economy_state/6 carries the books
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(1095);
    check(t.restoreState(saved) && t.saveState() == saved && nlohmann::json::parse(saved)["schema"] == "economy_state/14" &&
              near(t.walletSnapshot(2, "earth_orbital_hub", 0).taxPenaltyCr, w2.taxPenaltyCr, 1e-9),
          "economy_state/6: the tax books survive save / restore byte-exact");
}

// TRADE step 8 (TRADE_DESIGN §2.5): market regimes - the state price, the "not for sale" cap, the corridor ration
void testMarketRegimes()
{
    TradeDealRules r;
    MarketSide free{1000.0, 0.1, 500.0, 1000.0};
    MarketSide state = free;
    state.fixedPriceCr = 850.0;
    check(near(askCr(state), 850.0 * 1.05, 1e-9) && near(bidCr(state), 850.0 * 0.95, 1e-9) &&
              near(buyUnitCr(state, 300.0, r), askCr(state), 1e-9) && near(sellUnitCr(state, 300.0, r), bidCr(state), 1e-9),
          "a state price: the spread only, a big deal does not move it");
    check(buyUnitCr(free, 300.0, r) > askCr(free), "a free market: the same deal moves the price");
    state.giveCapT = 0.0;
    check(hubGivesUpToT(state, r) == 0.0 && explainDeal(true, state, 1.0, FiscalStyle{}, r, 0.0).code == "hub_empty",
          "a cap of 0 (the state's need first): nothing for sale to the player");
    state.giveCapT = 3.0;
    check(near(hubGivesUpToT(state, r), 3.0, 1e-9), "a ration: the hub sells at most the day's quota");

    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  market regimes server run (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    // a planned polity's hub (Tianhe / Harmony / Providence / New Karma): state prices, "нет в продаже" below the need
    const auto& model = EconomyModel::instance();
    std::string plannedHub, freeHub;
    int plannedSystem = -1, freeSystem = -1;
    for (const auto& h : model.hubs())
    {
        MarketSide probe;
        if (!s.marketSide(h.id, "food_staples", probe))
            continue;
        const auto st = s.fiscalStyleOf(h.id, h.systemId);
        if (plannedHub.empty() && st.id == "planned")
            plannedHub = h.id, plannedSystem = h.systemId;
        if (freeHub.empty() && st.id == "corporate")
            freeHub = h.id, freeSystem = h.systemId;
    }
    check(!plannedHub.empty() && !freeHub.empty(), "a planned hub (" + plannedHub + ") and a free one (" + freeHub + ") have markets");
    const auto planned = s.tradeSnapshot(1, 0, plannedHub, plannedSystem, true);
    bool allState = !planned.market.empty(), someCapped = false, capRule = true;
    for (const auto& row : planned.market)
    {
        allState = allState && row.fixedPriceCr > 0.0 && near(row.askCr, row.fixedPriceCr * (1.0 + 0.5 * planned.spread), 1e-6);
        someCapped = someCapped || (row.giveCapT == 0.0 && row.hubGivesUpToT == 0.0 && row.regimeNote.find("нет в продаже") == 0);
        capRule = capRule && near(row.giveCapT, std::max(0.0, row.stockT - 0.7 * row.targetT), 1e-6);
    }
    check(allState && capRule, "planned: every row at the state price; the hub sells only the stock above 70 % of the target");
    std::printf("INFO  planned %s: %s capped rows\n", plannedHub.c_str(), someCapped ? "some" : "no");
    if (!planned.market.empty())
        std::printf("INFO  planned row0: fixed %.2f ask %.2f spread %.3f cap %.2f stock %.1f target %.1f note '%s'\n",
                    planned.market[0].fixedPriceCr, planned.market[0].askCr, planned.spread, planned.market[0].giveCapT,
                    planned.market[0].stockT, planned.market[0].targetT, planned.market[0].regimeNote.c_str());
    const auto freeSnap = s.tradeSnapshot(1, 0, freeHub, freeSystem, true);
    bool noneState = !freeSnap.market.empty();
    for (const auto& row : freeSnap.market)
        noneState = noneState && row.fixedPriceCr == 0.0 && row.giveCapT < 0.0 && row.regimeNote.empty();
    check(noneState, "a free market (" + freeHub + "): no state price, no cap, no note");
    // Sol is a managed corridor: its basket stays in anchor x [0.9, 1.15] for a year
    for (int day = 1001; day <= 1365; ++day)
        s.advanceTo(day);
    const auto sol = s.tradeSnapshot(1, 0, "earth_orbital_hub", 0, true);
    bool inCorridor = true, someBasket = false;
    for (const auto& row : sol.market)
    {
        const auto& g = model.goods()[static_cast<std::size_t>(row.good)];
        if (g.group != "food" && g.group != "water_gas" && g.group != "medical")
            continue;
        someBasket = true;
        if (row.drift < -0.1 - 1e-9 || row.drift > 0.15 + 1e-9)
            std::printf("INFO  Sol out of corridor: %s drift %.4f\n", g.id.c_str(), row.drift);
        inCorridor = inCorridor && row.drift >= -0.1 - 1e-9 && row.drift <= 0.15 + 1e-9;
    }
    check(someBasket && inCorridor, "Sol (corridor): after a year every basket price is within anchor x [0.9, 1.15]");
}

// TRADE step 9 (TRADE_DESIGN §2.8): contraband - legality by the hub's regime family, the customs at docking, the
// seizure + 2 x fine, the black market
void testSmuggling()
{
    const auto& model = EconomyModel::instance();
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  smuggling (economy data not found from this working directory)\n");
        return;
    }
    const auto gi = [&](const char* id) {
        for (std::size_t k = 0; k < model.goods().size(); ++k)
            if (model.goods()[k].id == id)
                return static_cast<int>(k);
        return -1;
    };
    check(model.legalityOf(gi("narcotics"), "strict") == "banned" && model.legalityOf(gi("narcotics"), "open") == "restricted" &&
              model.legalityOf(gi("lotosite"), "open") == "banned" && model.legalityOf(gi("food_staples"), "faith") == "legal" &&
              model.regimeFamily("sol_authority") == "strict" && model.regimeFamily("unknown_regime") == "open",
          "legality by regime family (trade_goods.json): narcotics banned in strict, restricted in open; lotosite banned everywhere");
    s.start(1000);
    s.ensurePlayer(1, 0);
    check(s.legalityAt(gi("narcotics"), "earth_orbital_hub", 0) == "banned" && s.legalityAt(gi("jump_charge"), "earth_orbital_hub", 0) == "restricted",
          "Sol (strict): narcotics are contraband, J charge restricted (open until licences)");
    const CargoCapacity cobrett{120.0, 6};
    EconomyServer::TradeOrder o;
    o.playerId = 1;
    o.shipKey = 701;
    o.hubId = "earth_orbital_hub";
    o.systemId = 0;
    o.goodId = "narcotics";
    o.tonnes = 1.0;
    o.capacity = cobrett;
    o.requestId = 1;
    check(s.trade(o).code == "contraband", "the open market refuses contraband (a buy)");
    check(s.devAddCargo(1, 701, "narcotics", 2.0, cobrett, "earth_orbital_hub").empty() &&
              s.devAddCargo(1, 701, "food_staples", 10.0, cobrett, "earth_orbital_hub").empty(), "DEV: 2 t narcotics + 10 t grain aboard");
    o.buy = false;
    check(s.trade(o).code == "contraband", "the open market refuses contraband (a sale)");

    s.setDevCustomsProbability(0.0);
    const auto none = s.customsInspection(1, 701, "call-a", "earth_orbital_hub", 0);
    check(!none.inspected && s.cargo().find(701)->ownTonnesOf("narcotics") == 2.0, "p = 0: no inspection, the cargo stays");
    s.setDevCustomsProbability(-1.0);
    const auto rules = s.customsInspection(1, 701, "call-b", "earth_orbital_hub", 0);
    check(near(rules.probability, 0.05 * 1.5, 1e-9), "by the rules: own bloc (open 5 %) x strict 1.5 = 7.5 % (" + std::to_string(rules.probability) + ")");
    const double debt0 = totalDebtCr(*s.ledger(1), s.banks()), worth0 = netWorthCr(*s.ledger(1), s.banks());
    s.setDevCustomsProbability(1.0);
    const auto found = s.customsInspection(1, 701, "call-c", "earth_orbital_hub", 0);
    const auto* m = s.cargo().find(701);
    check(found.inspected && found.found && near(found.seizedT, 2.0, 1e-9) && near(found.fineCr, 2.0 * found.valueCr, 0.01) &&
              (!m || m->ownTonnesOf("narcotics") == 0.0) && m && near(m->ownTonnesOf("food_staples"), 10.0, 1e-9),
          "forced inspection: the 2 t of narcotics seized (worth " + std::to_string(found.valueCr) + " Cr), fine 2 x; the grain stays");
    check(near(worth0 - netWorthCr(*s.ledger(1), s.banks()), found.fineCr + found.feeCr, 1.0) && totalDebtCr(*s.ledger(1), s.banks()) >= debt0,
          "the fine + the 600 Cr inspection fee leave the net worth (a debt when unpaid, never refused)");

    // the black market: banned goods at the shadow price x 2.5, not declared income
    check(s.devAddCargo(1, 701, "narcotics", 3.0, cobrett, "earth_orbital_hub").empty(), "DEV: 3 t narcotics again");
    const double bid = s.blackBidCrPerT(1, gi("narcotics"), "earth_orbital_hub", 0);
    const double revenue0 = s.walletSnapshot(1, "earth_orbital_hub", 0).taxRevenueCr, w1 = netWorthCr(*s.ledger(1), s.banks());
    o.black = true;
    o.tonnes = 0.0;   // all aboard
    o.requestId = 2;
    const auto sold = s.trade(o);
    check(sold.ok && sold.code == "black_sold" && near(sold.totalCr, 3.0 * bid, 0.01) && bid > 2.4 * model.goods()[static_cast<std::size_t>(gi("narcotics"))].baseCrPerTonne * 0.5 &&
              near(netWorthCr(*s.ledger(1), s.banks()) - w1, sold.totalCr, 0.05) && s.walletSnapshot(1, "earth_orbital_hub", 0).taxRevenueCr == revenue0,
          "black market: 3 t sold at " + std::to_string(bid) + " Cr/t (shadow x 2.5), not in the tax book");
    o.goodId = "food_staples";
    o.requestId = 3;
    check(s.trade(o).code == "legal_here", "grain is legal here: the black market sends it to the open market");
    const auto snap = s.tradeSnapshot(1, 701, "earth_orbital_hub", 0, true);
    bool grainLegal = false;
    for (const auto& c : snap.cargo)
        grainLegal = grainLegal || (c.good == gi("food_staples") && c.legalityHere == "legal" && c.blackBidCrPerT == 0.0);
    check(grainLegal, "the cargo line knows its legality here (grain: legal, no black price)");

    // attention: a seizure is remembered (economy_state/7)
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(1000);
    check(t.restoreState(saved) && t.saveState() == saved && nlohmann::json::parse(saved)["schema"] == "economy_state/14" &&
              nlohmann::json::parse(saved)["seizures"].size() == 1,
          "economy_state/7: the seizure (customs attention) survives save / restore byte-exact");
}

// TRADE step 10 (TRADE_DESIGN §2.4, §2.6): dumping in a glut, the glut / shortage step limits, NPC speculators
void testNpcMarket()
{
    PriceRules rules;
    const auto settle = [&](double ratio, double shelf) {
        GoodCell c;
        c.traded = true;
        c.anchorCr = c.priceCr = 1000.0;
        c.targetT = 1000.0;
        c.stockT = c.normalStockT = ratio * 1000.0;   // no flow, no restock: the stock stays
        CellDrivers d;
        d.shelfDays = shelf;
        for (int i = 0; i < 300; ++i)
            (void)stepCell(c, d, rules);
        return c.priceCr;
    };
    const double f16 = std::pow(1.0 / 1.6, 0.6) * 1000.0;
    check(near(settle(1.6, 0.0), f16 * (1.0 - 0.25 * 0.25), 0.5) && near(settle(1.6, 20.0), f16 * (1.0 - 0.25 * 0.25 * 2.5), 0.5),
          "dumping at 1.6 x target: ore (keeps) x0.94, fresh food (20-day shelf) x0.84 below the scarcity price");
    check(near(settle(1.0, 20.0), 1000.0, 0.5), "no glut, no dumping");
    GoodCell glut;
    glut.traded = true;
    glut.anchorCr = glut.priceCr = 1000.0;
    glut.targetT = 1000.0;
    glut.stockT = glut.normalStockT = 2500.0;
    GoodCell mild = glut;
    mild.stockT = mild.normalStockT = 1800.0;
    CellDrivers d;
    d.shelfDays = 20.0;
    check(near(stepCell(glut, d, rules), -0.12, 1e-9) && near(stepCell(mild, d, rules), -0.08, 1e-9),
          "a glut (2.5 x) falls up to 12 %/day (clearance), a milder surplus at most 8 %");
    GoodCell scarce = glut;
    scarce.stockT = scarce.normalStockT = 200.0;
    check(near(stepCell(scarce, d, rules), 0.12, 1e-9), "a shortage (0.2 x) rises up to 12 %/day (panic buying)");

    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  NPC market server run (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    for (int day = 1001; day <= 1365; ++day)
        s.advanceTo(day);
    std::printf("INFO  NPC flows over a year: %.0f t shipped, %zu in flight\n", s.npcTonnesShipped(), s.npcShipmentsInFlight());
    check(s.npcTonnesShipped() > 0.0, "NPC speculators moved goods between the hubs of a system over a year");
    // a quiet year stays quiet: Sol's market rows near their anchors
    const auto sol = s.tradeSnapshot(0, 0, "earth_orbital_hub", 0, true);
    double worst = 0.0;
    for (const auto& row : sol.market)
        worst = std::max(worst, std::fabs(row.drift));
    check(!sol.market.empty() && worst < 0.6, "a quiet year: no Sol price runs away (worst drift " + std::to_string(worst) + ")");
}

// TRADE step 11 (TRADE_DESIGN §4): licences - a restricted good needs the polity's licence, else it is contraband
void testLicences()
{
    const auto& model = EconomyModel::instance();
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  licences (economy data not found from this working directory)\n");
        return;
    }
    const auto gi = [&](const char* id) {
        for (std::size_t k = 0; k < model.goods().size(); ++k)
            if (model.goods()[k].id == id)
                return static_cast<int>(k);
        return -1;
    };
    s.start(1000);
    s.ensurePlayer(1, 0);
    const CargoCapacity cobrett{120.0, 6};
    check(s.licenceForGood("jump_charge") == "export_control" && s.licenceForGood("reactor_fuel") == "hazardous_c" &&
              s.licenceForGood("alcohol_tobacco") == "excise" && s.licenceForGood("food_staples").empty(),
          "every restricted good is listed by a licence (trade_fees.json licences)");
    bool covered = true;
    for (const auto& g : model.goods())
        for (const auto& [family, status] : g.legality)
            covered = covered && (status != "restricted" || !s.licenceForGood(g.id).empty());
    check(covered, "no restricted good is left without a licence type");
    check(s.legalityFor(1, gi("jump_charge"), "earth_orbital_hub", 0) == "restricted", "Sol: J charge without the export licence = restricted");
    EconomyServer::TradeOrder o;
    o.playerId = 1;
    o.shipKey = 801;
    o.hubId = "earth_orbital_hub";
    o.systemId = 0;
    o.goodId = "jump_charge";
    o.tonnes = 1.0;
    o.capacity = cobrett;
    o.requestId = 1;
    check(s.trade(o).code == "licence", "the open market refuses a restricted good without the licence");
    check(s.devAddCargo(1, 801, "nav_sensors", 0.5, cobrett, "earth_orbital_hub").empty(), "DEV: 0.5 t nav sensors aboard");
    check(s.blackBidCrPerT(1, gi("nav_sensors"), "earth_orbital_hub", 0) > 0.0, "unlicensed restricted goods: the black market takes them (x1.3)");
    s.setDevCustomsProbability(1.0);
    const auto seized = s.customsInspection(1, 801, "call-l1", "earth_orbital_hub", 0);
    check(seized.found && near(seized.seizedT, 0.5, 1e-9), "customs seize restricted goods carried without the licence");

    const double w0 = netWorthCr(*s.ledger(1), s.banks());
    const auto lic = s.buyLicence(1, "export_control", "earth_orbital_hub", 0, 2);
    check(lic.ok && near(w0 - netWorthCr(*s.ledger(1), s.banks()), 15000.0, 0.01) &&
              s.legalityFor(1, gi("jump_charge"), "earth_orbital_hub", 0) == "licensed" &&
              s.blackBidCrPerT(1, gi("nav_sensors"), "earth_orbital_hub", 0) == 0.0,
          "the export licence (15 000 Cr in Sol): J charge and nav sensors become licensed, no black market for them");
    o.requestId = 3;
    const auto bought = s.trade(o);
    check(bought.ok, "with the licence the open market sells J charge (" + bought.code + ")");
    check(s.devAddCargo(1, 801, "nav_sensors", 0.5, cobrett, "earth_orbital_hub").empty(), "DEV: 0.5 t nav sensors again");
    const auto clean = s.customsInspection(1, 801, "call-l2", "earth_orbital_hub", 0);
    check(clean.inspected && !clean.found, "customs: licensed goods are not contraband");
    s.setDevCustomsProbability(-1.0);
    check(s.legalityFor(1, gi("alcohol_tobacco"), "earth_orbital_hub", 0) == "restricted", "the export licence does not cover alcohol (excise)");
    check(!s.buyLicence(1, "export_control", "", 0, 4).ok && !s.buyLicence(1, "no_such", "earth_orbital_hub", 0, 5).ok,
          "a licence needs a berth and a known type");
    const auto w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    bool offerHeld = false;
    for (const auto& of : w.licenceOffers)
        offerHeld = offerHeld || (of.type == "export_control" && of.held);
    check(w.licences.size() == 1 && w.licences[0].active && w.licences[0].expiresDay == 1360 && w.licenceOffers.size() == 4 && offerHeld &&
              w.pilotCategories.size() == 2 && w.pilotCategories[0].granted && !w.pilotCategories[1].granted,
          "the wallet: 1 licence to day 1360, 4 offers here (incl. the rescue licence), pilot categories + cargo courier / - newtonian");
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(1000);
    check(t.restoreState(saved) && t.saveState() == saved && nlohmann::json::parse(saved)["schema"] == "economy_state/14" &&
              t.legalityFor(1, gi("jump_charge"), "earth_orbital_hub", 0) == "licensed",
          "economy_state/8: the licence survives save / restore byte-exact");
    for (int day = 1001; day <= 1360; ++day)
        s.advanceTo(day);
    check(s.legalityFor(1, gi("jump_charge"), "earth_orbital_hub", 0) == "restricted", "after 360 days the licence has expired");
}

// TRADE step 13 (TRADE_DESIGN §3.6): cargo insurance - 0.4 % premium, 90 % of the insured lots on a loss, no contraband
void testInsurance()
{
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  insurance (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    const CargoCapacity cobrett{120.0, 6};
    check(s.devAddCargo(1, 901, "food_staples", 10.0, cobrett, "earth_orbital_hub").empty() &&
              s.devAddCargo(1, 901, "narcotics", 1.0, cobrett, "earth_orbital_hub").empty() &&
              s.devAddCargo(1, 902, "food_staples", 10.0, cobrett, "earth_orbital_hub").empty(),
          "DEV: ship 901 grain 10 t + narcotics 1 t, ship 902 grain 10 t (uninsured)");
    MarketSide grain;
    check(s.marketSide("earth_orbital_hub", "food_staples", grain), "Sol grain market");
    const auto q = s.quoteCargoInsurance(1, 901, "earth_orbital_hub", 0);
    check(q.why.empty() && q.lotIds.size() == 1 && near(q.valueCr, 10.0 * grain.priceCr, 0.01) && near(q.premiumCr, 0.004 * q.valueCr, 0.01),
          "the quote covers the grain only (narcotics are contraband here): value " + std::to_string(q.valueCr) + ", premium 0.4 %");
    const double w0 = netWorthCr(*s.ledger(1), s.banks());
    const auto ins = s.insureCargo(1, 901, "earth_orbital_hub", 0, 1);
    check(ins.ok && near(w0 - netWorthCr(*s.ledger(1), s.banks()), q.premiumCr, 0.01) && !s.insureCargo(1, 901, "", 0, 2).ok,
          "insured at the berth for the premium; not without a berth");
    const auto snap = s.tradeSnapshot(1, 901, "earth_orbital_hub", 0, true);
    check(near(snap.insuredCr, q.valueCr, 0.01) && snap.insuredUntilDay == 1030, "the snapshot shows the policy to day 1030");
    const double w1 = netWorthCr(*s.ledger(1), s.banks());
    EconomyServer::ShipLoss loss;
    loss.eventId = "loss:901:1";
    loss.shipKey = 901;
    loss.systemId = 0;
    loss.cause = "disabled in combat";
    check(s.recordShipLoss(loss), "ship 901 lost in combat");
    check(near(netWorthCr(*s.ledger(1), s.banks()) - w1, 0.9 * q.valueCr, 0.05), "the payout: 90 % of the insured grain (10 % deductible), nothing for the narcotics");
    const double w2 = netWorthCr(*s.ledger(1), s.banks());
    loss.eventId = "loss:902:1";
    loss.shipKey = 902;
    check(s.recordShipLoss(loss) && near(netWorthCr(*s.ledger(1), s.banks()), w2, 1e-6), "an uninsured ship's cargo pays nothing");
    check(s.devAddCargo(1, 903, "food_staples", 5.0, cobrett, "earth_orbital_hub").empty() && s.insureCargo(1, 903, "earth_orbital_hub", 0, 3).ok,
          "ship 903 insured");
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(1000);
    check(t.restoreState(saved) && t.saveState() == saved && nlohmann::json::parse(saved)["schema"] == "economy_state/14" &&
              nlohmann::json::parse(saved)["cargo_policies"].size() == 1,
          "economy_state/9: the policy survives save / restore byte-exact");
}

// Ship arrest (ECONOMY_BANKING 2026-11-20): the claim after 90 overdue days, the auction proceeds repay that bank first
void testArrest()
{
    BankDirectory d = testBanks();
    ClientLedger l = openLedger(d, 1, "sol", 0);
    Loan x;
    x.id = l.nextLoanId++;
    x.bankId = "sol";
    x.kind = LoanKind::Term;
    x.principal = x.outstanding = toMinor(30000.0);
    x.arrears = toMinor(5000.0);
    x.overdueDays = 95;
    l.loans.push_back(x);
    check(near(debtAtBankCr(l, d, "sol"), 30000.0, 0.005), "the debt at the bank: 30 000 Cr");
    const double left = repayFromProceeds(l, d, "sol", 20000.0, 10, "test");
    check(left == 0.0 && near(debtAtBankCr(l, d, "sol"), 10000.0, 0.005) && l.loans.size() == 1 && l.loans[0].arrears == 0,
          "proceeds 20 000: the arrears and 20 000 of the loan repaid");
    const double rest = repayFromProceeds(l, d, "sol", 15000.0, 11, "test");
    check(near(rest, 5000.0, 0.005) && l.loans.empty() && debtAtBankCr(l, d, "sol") == 0.0, "proceeds 15 000 on 10 000: 5 000 left over, the loan gone");

    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  arrest server run (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    ClientLedger* sl = s.ledger(1);
    check(!s.arrestClaim(1, "earth_orbital_hub", 0).due, "no overdue debt: no arrest");
    Loan y;
    y.id = sl->nextLoanId++;
    y.bankId = sl->homeBank;
    y.principal = y.outstanding = toMinor(40000.0);
    y.overdueDays = 89;
    sl->loans.push_back(y);
    check(!s.arrestClaim(1, "earth_orbital_hub", 0).due, "89 days overdue: not yet");
    sl->loans.back().overdueDays = 90;
    const auto claim = s.arrestClaim(1, "earth_orbital_hub", 0);
    check(claim.due && claim.bankId == sl->homeBank && near(claim.debtCr, 40000.0, 0.005) && s.auctionShare() == 0.6,
          "90 days overdue at the Sol bank, docked at a Sol hub: the arrest claim for 40 000 Cr (auction share 0.6)");
    const double w0 = netWorthCr(*sl, s.banks());
    s.settleArrest(1, claim.bankId, "earth_orbital_hub", 50000.0, {"A -> B"});
    check(totalDebtCr(*s.ledger(1), s.banks()) < 1.0 && near(netWorthCr(*s.ledger(1), s.banks()) - w0, 50000.0, 1.0),
          "settled: the proceeds repay the 40 000 debt, the 10 000 rest lands on the account (the net worth +50 000)");
    s.settleArrest(1, claim.bankId, "earth_orbital_hub", 0.0, {});
    check(s.tradeSnapshot(1, 0, "earth_orbital_hub", 0, true).results.back().code == "arrest", "the client sees the arrest result");
    // a restructured debt (overdue reset by the restructuring) counts from its start while less than half is repaid
    Loan z;
    z.id = s.ledger(1)->nextLoanId++;
    z.bankId = s.ledger(1)->homeBank;
    z.kind = LoanKind::Restructured;
    z.principal = z.outstanding = toMinor(20000.0);
    z.startDay = 1000 - 89;
    s.ledger(1)->loans.push_back(z);
    check(!s.arrestClaim(1, "earth_orbital_hub", 0).due, "restructured 89 days ago: no arrest yet");
    s.ledger(1)->loans.back().startDay = 1000 - 90;
    check(s.arrestClaim(1, "earth_orbital_hub", 0).due, "restructured 90 days ago, less than half repaid: the arrest claim");
    s.ledger(1)->loans.back().outstanding = toMinor(9000.0);
    check(!s.arrestClaim(1, "earth_orbital_hub", 0).due, "more than half repaid: no arrest (the restructuring works)");
}

// TRADE step 15 (TRADE_DESIGN §6.4): bot pilots - the agency, hire onto an owned ship, the abstract trade loop, wages
void testBots()
{
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  bots (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    const auto c1 = s.botCandidatesAt("earth_orbital_hub", 0);
    const auto c2 = s.botCandidatesAt("earth_orbital_hub", 0);
    check(c1.size() == 3 && c1[0].id == c2[0].id && c1[0].name == c2[0].name && c1[0].wageCr > 0.0,
          "the agency: 3 deterministic candidates (" + c1[0].name + ", wage " + std::to_string(c1[0].wageCr) + ")");
    bool embellished = true;
    for (const auto& c : c1)
        embellished = embellished && c.skillShown >= c.traits.skill - 1e-9;
    check(embellished, "a CV never understates the skill (it may embellish it)");
    const CargoCapacity hold{120.0, 6};
    check(!s.hireBot(1, c1[0].id, "earth_orbital_hub", 0, 0, "-", hold, 260.0, 1).ok, "no ship: refused");
    const auto hired = s.hireBot(1, c1[0].id, "earth_orbital_hub", 0, 4242, "test ship", hold, 260.0, 2);
    check(hired.ok && s.bots().size() == 1 && s.botCandidatesAt("earth_orbital_hub", 0).size() == 2, "hired onto ship 4242; the agency lists 2 now");
    const std::string botId = s.bots()[0].id;
    const auto order = s.orderBotLoop(1, botId, "earth_orbital_hub", 0, 3);
    check(order.ok && !s.bots()[0].hubB.empty() && !s.bots()[0].goodId.empty(),
          "the loop order: " + order.message);
    const double w0 = netWorthCr(*s.ledger(1), s.banks());
    for (int day = 1001; day <= 1010; ++day)
        s.advanceTo(day);
    const auto& b = s.bots()[0];
    std::printf("INFO  bot after 10 days: state %s, trips %d, profit %.0f, net worth %.0f -> %.0f\n", b.state.c_str(), b.trips, b.profitCr, w0,
                netWorthCr(*s.ledger(1), s.banks()));
    check(b.trips >= 1 && b.unpaidDays == 0, "10 days: the bot made " + std::to_string(b.trips) + " trip(s), the wage paid daily");
    auto w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    // delivered = exactly the reports whose delivery day (the event day + 1) has come
    std::size_t due = 0;
    bool laterThanEvent = true;
    for (const auto& [visible, text] : b.reports)
        due += visible <= s.day() ? 1u : 0u;
    for (std::size_t k = 0; k < b.reports.size(); ++k)   // hire / order lines are back-dated; the trip reports are day + 1
        if (b.reports[k].second.rfind("погрузка", 0) == 0 || b.reports[k].second.rfind("продажа", 0) == 0)
            laterThanEvent = laterThanEvent && b.reports[k].first >= 1001 + 1;
    check(!w.bots.empty() && !w.bots[0].reports.empty() && w.bots[0].reports.size() == due && laterThanEvent,
          "reports reach the owner a day after the event (" + std::to_string(w.bots[0].reports.size()) + " of " + std::to_string(b.reports.size()) +
              " delivered by day " + std::to_string(s.day()) + ")");
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(1010);
    check(t.restoreState(saved) && t.saveState() == saved && nlohmann::json::parse(saved)["schema"] == "economy_state/14" && t.bots().size() == 1,
          "economy_state/13: the bot survives save / restore byte-exact");
    // unpaid: a trading bot pays its own wage from its sales; an idle one with a broke owner leaves after 30 days
    s.fireBot(1, botId);
    const auto c2list = s.botCandidatesAt("earth_orbital_hub", 0);
    check(!c2list.empty() && s.hireBot(1, c2list[0].id, "earth_orbital_hub", 0, 4343, "idle ship", hold, 260.0, 4).ok, "a second bot, no order");
    (void)s.devSpendAll(1, "earth_orbital_hub", 0);
    for (int day = 1011; day <= 1090; ++day)
    {
        (void)s.devSpendAll(1, "earth_orbital_hub", 0);   // late VAT refunds of the first bot's buys would pay a few wages
        s.advanceTo(day);
    }
    std::printf("INFO  idle bot: state %s, unpaid %d, free money %.0f, debt %.0f\n", s.bots()[1].state.c_str(), s.bots()[1].unpaidDays,
                freeMoneyCr(*s.ledger(1), s.banks()), totalDebtCr(*s.ledger(1), s.banks()));
    check(s.bots()[0].state == "quit" && s.bots()[1].state == "quit", "fired: gone; 30 days without the wage: the idle bot left too");
}

void testConstruction()
{
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  construction (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    s.setBuildableProjects({{"earth_orbital_hub", {"test.project.x05", "hall"}}});
    const auto offers = s.buildOffersAt("earth_orbital_hub");
    check(offers.size() == 3 && offers[0].priceCr >= 3050000.0 * 1.5 * 1.15 - 1.0 && offers[0].workDays >= 27 && offers[0].reliability >= 0.6,
          "3 contractor offers (" + offers[0].contractorRu + ": " + std::to_string(offers[0].priceCr) + " Cr, " + std::to_string(offers[0].workDays) + " days)");
    check(!s.signConstruction(1, offers[0].id, "earth_orbital_hub", 0, 1).ok, "no money for the first milestone: refused");
    s.credit(1, "earth_orbital_hub", 0, offers[0].priceCr * 0.15, "test");
    const auto signedR = s.signConstruction(1, offers[0].id, "earth_orbital_hub", 0, 2);
    check(signedR.ok && s.buildProjects().size() == 1 && s.buildOffersAt("earth_orbital_hub").empty(),
          "signed: s1 paid (" + std::to_string(-signedR.totalCr) + " Cr), the project taken off the offers");
    // broke: the s2 milestone at 25 % halts the work
    const int days = s.buildProjects()[0].offer.workDays;
    for (int day = 1001; day <= 1000 + days / 4 + 3; ++day)
    {
        (void)s.devSpendAll(1, "earth_orbital_hub", 0);
        s.advanceTo(day);
    }
    const auto& p = s.buildProjects()[0];
    check(p.status == "halted" && p.paidMilestones == 1 && s.buildProgress(p, s.day()) < 0.30,
          "s2 unpaid: halted at " + std::to_string(s.buildProgress(p, s.day())) + " (status " + p.status + ")");
    // money: the work resumes and runs to the end
    s.credit(1, "earth_orbital_hub", 0, offers[0].priceCr, "test");
    int day = s.day();
    while (s.buildProjects()[0].status != "done" && day < s.day() + 400)
        s.advanceTo(++day);
    const auto& d = s.buildProjects()[0];
    check(d.status == "done" && d.paidMilestones == 4 && d.retentionPaid && std::abs(d.paidCr - d.offer.priceCr) < 1.0,
          "done on day " + std::to_string(day) + ": 4 milestones + the retention = the price (" + std::to_string(d.paidCr) + ")");
    const double w0 = netWorthCr(*s.ledger(1), s.banks());
    for (int k = 1; k <= 30; ++k)
        s.advanceTo(day + k);
    const double gain = netWorthCr(*s.ledger(1), s.banks()) - w0;
    std::printf("INFO  construction: the module's net income over 30 days %.0f Cr\n", gain);
    check(gain > 0.0, "the finished module pays its owner");
    const auto w = s.walletSnapshot(1, "earth_orbital_hub", 0);
    check(w.buildProjects.size() == 1 && w.buildProjects[0].incomeCrPerDay > 0.0, "the wallet lists the project: " + w.buildProjects[0].statusRu);
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(s.day());
    check(t.restoreState(saved) && t.saveState() == saved && t.buildProjects().size() == 1, "economy_state/13: the project survives save / restore");
}

void testMining()
{
    EconomyServer s;
    if (!s.load())
    {
        std::printf("SKIP  mining (economy data not found from this working directory)\n");
        return;
    }
    s.start(1000);
    s.ensurePlayer(1, 0);
    const std::string hub = "site.000.sol.hygiea";   // a belt hub with a full market
    check(s.siteOffersAt("earth_orbital_hub").empty(), "no concessions at a city hub");
    const auto sites = s.siteOffersAt(hub);
    check(sites.size() == 3 && sites[0].radiusM >= 50.0 && sites[0].reserveT > 0.0 && sites[0].rentYearCr > 2000.0,
          "3 sites at Гигея (" + sites[0].nameRu + ", r " + std::to_string(sites[0].radiusM) + " m, " + sites[0].goodId + ")");
    (void)s.devSpendAll(1, hub, 0);
    check(!s.leaseSite(1, sites[0].id, hub, 0, 1).ok, "no money for the application + survey: refused");
    s.credit(1, hub, 0, 20000000.0, "test");
    check(s.leaseSite(1, sites[0].id, hub, 0, 2).ok && s.concessions().size() == 1 && s.siteOffersAt(hub).size() == 2, "leased; 2 sites left on offer");
    std::vector<EconomyServer::BuildOffer> rigs;
    for (const auto& o : s.buildOffersAt(hub, 1))
        if (o.kind == "minehead")
            rigs.push_back(o);
    check(rigs.size() == 3 && s.buildOffersAt(hub, 2).empty(), "the rig offers: only to the concession's owner (" + std::to_string(rigs.size()) + ")");
    check(s.signConstruction(1, rigs[0].id, hub, 0, 3).ok, "the rig contract signed (" + std::to_string(rigs[0].priceCr) + " Cr)");
    int day = 1000;
    while (s.concessions()[0].status != "mining" && day < 1200)
        s.advanceTo(++day);
    check(s.concessions()[0].status == "mining", "the rig accepted on day " + std::to_string(day) + ": mining");
    const double before = s.concessions()[0].reserveLeftT;
    for (int k = 1; k <= 10; ++k)
        s.advanceTo(day + k);
    const auto& c = s.concessions()[0];
    std::printf("INFO  mine after 10 days: mined %.0f t, sold %.0f t, net proceeds %.0f Cr, today %.1f t\n", c.minedT, c.soldT, c.revenueCr, c.lastOutputT);
    check(c.minedT > 500.0 && c.reserveLeftT < before && c.soldT > 0.0 && c.revenueCr > 0.0, "10 days: the ore mined and sold at the hub's market");
    const auto w = s.walletSnapshot(1, hub, 0);
    check(w.mines.size() == 1 && w.mines[0].lastOutputT > 0.0, "the wallet lists the mine: " + w.mines[0].statusRu);
    const std::string saved = s.saveState();
    EconomyServer t;
    t.load();
    t.start(s.day());
    check(t.restoreState(saved) && t.saveState() == saved && t.concessions().size() == 1 && nlohmann::json::parse(saved)["schema"] == "economy_state/14",
          "economy_state/14: the concession survives save / restore");
    // part 3: keep the ore, haul it in a hold to a better market
    const std::string site = s.concessions()[0].site.id;
    const std::string ore = s.concessions()[0].site.goodId;
    s.setMineSale(1, site, false);
    day = s.day();
    for (int k = 1; k <= 3; ++k)
        s.advanceTo(day + k);
    const double stock = s.mineStockT(site, ore);
    check(stock > 300.0, "kept: the stockpile grows (" + std::to_string(stock) + " t in 3 days)");
    const CargoCapacity hold{100.0, 4};
    check(!s.loadFromMine(1, site, 7070, hold, "earth_orbital_hub", 0.0, 10).ok, "loading away from the mine's hub: refused");
    const auto loaded = s.loadFromMine(1, site, 7070, hold, hub, 0.0, 11);
    check(loaded.ok && std::abs(s.mineStockT(site, ore) - (stock - 100.0)) < 0.05, "loaded what fits (100 t): " + loaded.message);
    EconomyServer::TradeOrder sell;
    sell.buy = false;
    sell.playerId = 1;
    sell.shipKey = 7070;
    sell.hubId = "earth_orbital_hub";
    sell.systemId = 0;
    sell.goodId = ore;
    sell.tonnes = 100.0;
    sell.capacity = hold;
    const auto sold = s.trade(sell);
    std::printf("INFO  100 t of %s hauled to Высокий: %s (%.0f Cr; at the mine the 10-day average was %.1f Cr/t net)\n", ore.c_str(),
                sold.ok ? "sold" : sold.message.c_str(), sold.totalCr, c.soldT > 0.0 ? c.revenueCr / c.soldT : 0.0);
    check(sold.ok || !sold.message.empty(), "the hauled ore is an ordinary lot of the hold (sold or refused by the market)");
    s.setMineSale(1, site, true);
    // broke: no output, then the lease lapses after 90 days
    day = s.day();
    for (int k = 1; k <= 95; ++k)
    {
        (void)s.devSpendAll(1, hub, 0);
        s.advanceTo(day + k);
    }
    check(s.concessions()[0].status == "lapsed", "95 days without rent: the concession lapsed (" + s.concessions()[0].status + ")");
}

int main()
{
    testInterestAndAnnuity();
    testPayments();
    testLifeline();
    testNoDeathSpiral();
    testInDuplum();
    testPriceStep();
    testFx();
    testFuelPriceAndAtomicSave();
    testServer();
    testTradeModel();
    testTradeServer();
    testTradeDealWindow();
    testPortFeesAndDuty();
    testPortCallPersistence();
    testContracts();
    testProtectedMinimum();
    testIncomeTax();
    testMarketRegimes();
    testSmuggling();
    testNpcMarket();
    testLicences();
    testInsurance();
    testArrest();
    testBots();
    testConstruction();
    testMining();
    std::printf("ECONOMY CHECK: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
