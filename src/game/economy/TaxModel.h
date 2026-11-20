#pragma once

#include <string>

// The income tax of the player's residence polity (docs/TRADE_DESIGN.md §5.2, §9 row 7, §9f) - the minimal set.
//
// PURE model: no I/O, no clock, no globals; EconomyServer owns one TaxBook per player and feeds it.
//   - cash basis: what came in (sale proceeds after the deal's own fees, contract rewards and advances) is revenue; what
//     went out for the business (purchases net of the input VAT that comes back, port fees, fuel, repairs, contract
//     penalties) is an expense. Fines are not deductible (as in real tax law).
//   - a period of rules.periodDays (90: a quarter) closes on the first day of the next one: tax = rate x max(0, profit)
//     minus the tax withheld at source abroad that the home country credits (a treaty partner: channel open / licensed),
//     never below 0; plus the tax administration fee (bureaucratic style 2 % of the tax). A loss is not carried forward
//     (minimal set). Due rules.declareDays after the period ends.
//   - on the due day the server charges it with payOrDebt (never refused): what cannot be paid becomes a tax debt, and a
//     one-off late penalty (rules.latePenaltyShare of the unpaid part) is booked on top.
// Not in the minimal set (flagged in §9f): monthly advances, loss carry-forward, the 5 % VAT-refund fee's own line.
namespace game::economy
{
struct TaxRules
{
    int periodDays = 90;
    int declareDays = 15;
    double latePenaltyShare = 0.05;   // one-off, of the part that could not be paid on the due day
    double withholdingShare = 0.05;   // at source, on a contract reward from an issuer of another polity
};

struct TaxBook
{
    int period = -1;                  // the open period (day / periodDays); -1 = not started
    double revenueCr = 0.0;           // the open period
    double expensesCr = 0.0;
    double withheldCreditCr = 0.0;    // withheld abroad and credited at home (treaty channel)
    double withheldLostCr = 0.0;      // withheld abroad, NOT credited (restricted / black channel: double taxation)
    // the last closed period's assessment
    int assessedPeriod = -1;
    double assessedProfitCr = 0.0;
    double assessedTaxCr = 0.0;       // after the withholding credit
    double assessedFeeCr = 0.0;       // the administration fee
    int dueDay = -1;                  // -1 = nothing due
    bool settled = true;
    double paidCr = 0.0;              // on the due day: paid at once
    double debtCr = 0.0;              //                  booked as debt
    double penaltyCr = 0.0;           //                  the late penalty
    double paidTotalCr = 0.0;         // all income tax paid or booked so far
};

struct TaxAssessment
{
    bool closed = false;              // a period was closed by this roll
    int period = -1;
    double profitCr = 0.0;
    double taxCr = 0.0;
    double feeCr = 0.0;
    int dueDay = -1;
};

[[nodiscard]] inline int taxPeriodOf(int day, const TaxRules& rules) { return day < 0 ? -1 : day / rules.periodDays; }
void addRevenue(TaxBook& book, double cr);
void addExpense(TaxBook& book, double cr);
// the tax withheld at source abroad: credited at home (a treaty) or lost (double taxation)
void addWithheld(TaxBook& book, double cr, bool credited);
// Opens the book on its first day; on a new period closes the open one into an assessment (rate = the residence style's
// income tax, adminFeeShare = its administration fee). An earlier assessment still unsettled is left as it is.
TaxAssessment rollTaxPeriod(TaxBook& book, int day, double rate, double adminFeeShare, const TaxRules& rules);
// the tax of the open period so far if it closed today (the F5 estimate)
[[nodiscard]] double estimatedTaxCr(const TaxBook& book, double rate, double adminFeeShare);
// the due day came: what to charge (tax + fee); the caller pays it and reports the unpaid part with settleTax
[[nodiscard]] inline bool taxDue(const TaxBook& book, int day) { return !book.settled && book.dueDay >= 0 && day >= book.dueDay; }
// returns the late penalty to book (0 when paid in full)
double settleTax(TaxBook& book, double paidCr, double debtCr, const TaxRules& rules);
} // namespace game::economy
