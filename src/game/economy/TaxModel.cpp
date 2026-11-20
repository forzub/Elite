#include "src/game/economy/TaxModel.h"

#include <algorithm>
#include <cmath>

namespace game::economy
{
void addRevenue(TaxBook& book, double cr)
{
    if (cr > 0.0 && std::isfinite(cr))
        book.revenueCr += cr;
}

void addExpense(TaxBook& book, double cr)
{
    if (cr > 0.0 && std::isfinite(cr))
        book.expensesCr += cr;
}

void addWithheld(TaxBook& book, double cr, bool credited)
{
    if (!(cr > 0.0) || !std::isfinite(cr))
        return;
    (credited ? book.withheldCreditCr : book.withheldLostCr) += cr;
}

double estimatedTaxCr(const TaxBook& book, double rate, double adminFeeShare)
{
    const double tax = std::max(0.0, rate * std::max(0.0, book.revenueCr - book.expensesCr) - book.withheldCreditCr);
    return tax * (1.0 + adminFeeShare);
}

TaxAssessment rollTaxPeriod(TaxBook& book, int day, double rate, double adminFeeShare, const TaxRules& rules)
{
    TaxAssessment a;
    const int period = taxPeriodOf(day, rules);
    if (period < 0)
        return a;
    if (book.period < 0)
    {
        book.period = period;   // the book opens on the first day it is seen: no back taxes for days before it
        return a;
    }
    if (period <= book.period)
        return a;
    a.closed = true;
    a.period = book.period;
    a.profitCr = book.revenueCr - book.expensesCr;
    a.taxCr = std::max(0.0, rate * std::max(0.0, a.profitCr) - book.withheldCreditCr);
    a.feeCr = a.taxCr * adminFeeShare;
    a.dueDay = (book.period + 1) * rules.periodDays + rules.declareDays;
    if (book.settled)   // one open assessment at a time (declareDays < periodDays keeps them apart)
    {
        book.assessedPeriod = a.period;
        book.assessedProfitCr = a.profitCr;
        book.assessedTaxCr = a.taxCr;
        book.assessedFeeCr = a.feeCr;
        book.dueDay = a.dueDay;
        book.settled = a.taxCr + a.feeCr <= 0.0;
        book.paidCr = book.debtCr = book.penaltyCr = 0.0;
    }
    book.period = period;
    book.revenueCr = book.expensesCr = book.withheldCreditCr = book.withheldLostCr = 0.0;
    return a;
}

double settleTax(TaxBook& book, double paidCr, double debtCr, const TaxRules& rules)
{
    book.settled = true;
    book.paidCr = std::max(0.0, paidCr);
    book.debtCr = std::max(0.0, debtCr);
    book.penaltyCr = std::round(book.debtCr * rules.latePenaltyShare * 100.0) / 100.0;
    book.paidTotalCr += book.paidCr + book.debtCr;
    return book.penaltyCr;
}
} // namespace game::economy
