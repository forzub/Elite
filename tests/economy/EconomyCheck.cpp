// Unit checks of the pure economy parts (docs/ECONOMY_BANKING.md): interest, payments, lifeline, the insolvency
// path (no death spiral), in duplum, the daily price step, FX; plus a data-driven server run (365 days) when the
// economy data is reachable from the working directory (run from the repository root).
//
//   EliteEconomyCheck            -> "ECONOMY CHECK: N passed, 0 failed", exit code = failures
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "src/game/economy/BankingModel.h"
#include "src/game/economy/EconomyServer.h"
#include "src/game/economy/PriceModel.h"

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
    testServer();
    std::printf("ECONOMY CHECK: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail;
}
