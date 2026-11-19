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
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include "src/game/economy/BankingModel.h"
#include "src/game/economy/EconomyServer.h"
#include "src/game/economy/PriceModel.h"
#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/TradeQuoteModel.h"

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
    o.goodId = "computing";
    o.tonnes = 10.0;
    const double w2 = worth(2);
    unchanged("a broke player buying 10 t of computers", s.trade(o), "no_money");
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
} // namespace

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
    std::printf("ECONOMY CHECK: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
