#include "src/game/economy/BankingModel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace game::economy
{
Minor toMinor(double units)
{
    return static_cast<Minor>(std::llround(units * 100.0));
}

Channel channelFromString(const std::string& s)
{
    if (s == "open") return Channel::Open;
    if (s == "licensed") return Channel::Licensed;
    if (s == "restricted") return Channel::Restricted;
    if (s == "black") return Channel::Black;
    if (s == "closed") return Channel::Closed;
    return Channel::Licensed;
}

const char* channelName(Channel c)
{
    switch (c)
    {
    case Channel::Open: return "open";
    case Channel::Licensed: return "licensed";
    case Channel::Restricted: return "restricted";
    case Channel::Black: return "black";
    case Channel::Closed: return "closed";
    }
    return "licensed";
}

const char* standingName(Standing s)
{
    switch (s)
    {
    case Standing::Good: return "good";
    case Standing::Grace: return "grace";
    case Standing::Delinquent: return "delinquent";
    case Standing::Restructured: return "restructured";
    }
    return "good";
}

const char* standingRu(Standing s)
{
    switch (s)
    {
    case Standing::Good: return "в порядке";
    case Standing::Grace: return "льготный период";
    case Standing::Delinquent: return "просрочка: кредит заморожен";
    case Standing::Restructured: return "реструктуризация: 25 % дохода в погашение";
    }
    return "";
}

// ---------------------------------------------------------------- directory
void BankDirectory::add(NationalBank bank)
{
    m_index[bank.id] = m_banks.size();
    m_fx.push_back(bank.fxAnchorPerCr > 0.0 ? bank.fxAnchorPerCr : 1.0);
    m_banks.push_back(std::move(bank));
}

void BankDirectory::setChannel(const std::string& fromBloc, const std::string& toBloc, Channel c)
{
    m_matrix[{fromBloc, toBloc}] = c;
}

void BankDirectory::setLifelineBank(const std::string& polityWithoutBank, const std::string& bankId)
{
    m_lifelineBank[polityWithoutBank] = bankId;
}

const NationalBank* BankDirectory::bank(const std::string& id) const
{
    const auto it = m_index.find(id);
    return it == m_index.end() ? nullptr : &m_banks[it->second];
}

const NationalBank* BankDirectory::reserve() const
{
    for (const auto& b : m_banks)
        if (b.reserve)
            return &b;
    return m_banks.empty() ? nullptr : &m_banks.front();
}

const NationalBank* BankDirectory::bankForPolity(const std::string& polity) const
{
    if (const auto* b = bank(polity))
        return b;
    if (const auto it = m_lifelineBank.find(polity); it != m_lifelineBank.end())
        if (const auto* b = bank(it->second))
            return b;
    return reserve();
}

double BankDirectory::fx(const std::string& bankId) const
{
    const auto it = m_index.find(bankId);
    return it == m_index.end() ? 1.0 : m_fx[it->second];
}

void BankDirectory::setFx(const std::string& bankId, double perCr)
{
    const auto it = m_index.find(bankId);
    if (it != m_index.end() && perCr > 0.0 && std::isfinite(perCr))
        m_fx[it->second] = perCr;
}

Channel BankDirectory::channel(const std::string& fromBank, const std::string& toBank) const
{
    const auto* a = bank(fromBank);
    const auto* b = bank(toBank);
    if (!a || !b)
        return Channel::Licensed;
    if (a->bloc == b->bloc)
        return Channel::Open;
    // the matrix is not symmetric in the data: the stricter direction wins (a transfer needs both ends)
    Channel worst = Channel::Open;
    bool found = false;
    for (const auto& key : {std::make_pair(a->bloc, b->bloc), std::make_pair(b->bloc, a->bloc)})
        if (const auto it = m_matrix.find(key); it != m_matrix.end())
        {
            worst = std::max(worst, it->second);
            found = true;
        }
    return found ? worst : Channel::Licensed;
}

// ---------------------------------------------------------------- ledger basics
ClientLedger openLedger(const BankDirectory& dir, std::uint64_t ownerId, const std::string& homeBank, int day)
{
    ClientLedger l;
    l.ownerId = ownerId;
    const auto* home = dir.bank(homeBank) ? dir.bank(homeBank) : dir.reserve();
    l.homeBank = home ? home->id : homeBank;
    l.lastDay = day - 1;
    for (int i = 0; i < 30; ++i)
        l.incomeDay[i] = std::numeric_limits<int>::min() / 2;
    const double fx = dir.fx(l.homeBank);
    auto& acc = ensureAccount(l, l.homeBank);
    acc.balance = toMinor(dir.rules.startAccountCr * fx);
    acc.overdraftLimit = toMinor(dir.rules.startOverdraftCr * fx);
    l.cash[l.homeBank] = toMinor(dir.rules.startCashCr * fx);
    logEvent(l, day, "счёт открыт в банке " + (home ? home->name : l.homeBank));
    return l;
}

Account* findAccount(ClientLedger& ledger, const std::string& bankId)
{
    for (auto& a : ledger.accounts)
        if (a.bankId == bankId)
            return &a;
    return nullptr;
}

const Account* findAccount(const ClientLedger& ledger, const std::string& bankId)
{
    for (const auto& a : ledger.accounts)
        if (a.bankId == bankId)
            return &a;
    return nullptr;
}

Account& ensureAccount(ClientLedger& ledger, const std::string& bankId)
{
    if (auto* a = findAccount(ledger, bankId))
        return *a;
    Account a;
    a.bankId = bankId;
    a.frozen = ledger.standing == Standing::Delinquent || ledger.standing == Standing::Restructured;
    ledger.accounts.push_back(a);
    return ledger.accounts.back();
}

void logEvent(ClientLedger& ledger, int day, std::string text)
{
    ledger.events.push_back({day, std::move(text)});
    if (ledger.events.size() > 24)
        ledger.events.erase(ledger.events.begin(), ledger.events.begin() + static_cast<long>(ledger.events.size() - 24));
}

namespace
{
std::string money(double units, const std::string& code)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.2f %s", units, code.c_str());
    return b;
}

const std::string& codeOf(const BankDirectory& dir, const std::string& bankId)
{
    static const std::string cr = "CR";
    const auto* b = dir.bank(bankId);
    return b ? b->code : cr;
}

Loan* findLoan(ClientLedger& ledger, const std::string& bankId, LoanKind kind)
{
    for (auto& l : ledger.loans)
        if (l.bankId == bankId && l.kind == kind)
            return &l;
    return nullptr;
}

// interest of one day on `base` minor units at `annualRate`, sub-minor remainder kept in `carry`
Minor dailyInterest(Minor base, double annualRate, double& carry)
{
    const double x = static_cast<double>(base) * annualRate / 365.0 + carry;
    const Minor m = static_cast<Minor>(std::trunc(x));
    carry = x - static_cast<double>(m);
    return m;
}

// cover as much of `remainingCr` as `availableMinor` (in a currency of `fxSrc` units / Cr) allows at a fee of
// max(feeMinCr, feeShare x covered); returns (covered Cr, debit minor, fee Cr)
struct Take
{
    double coveredCr = 0.0;
    Minor debit = 0;
    double feeCr = 0.0;
};
Take takeFrom(double remainingCr, Minor availableMinor, double fxSrc, double feeShare, double feeMinCr)
{
    Take t;
    if (availableMinor <= 0 || remainingCr <= 1.0e-9 || fxSrc <= 0.0)
        return t;
    const double availCr = toUnits(availableMinor) / fxSrc;
    const auto fee = [&](double c) { return c > 0.0 ? std::max(feeMinCr, feeShare * c) : 0.0; };
    double c = remainingCr;
    if (c + fee(c) > availCr)
    {
        c = availCr / (1.0 + feeShare);
        if (feeShare * c < feeMinCr)
            c = availCr - feeMinCr;
        c = std::max(0.0, std::min(c, remainingCr));
    }
    if (c <= 1.0e-9)
        return t;
    t.coveredCr = c;
    t.feeCr = fee(c);
    t.debit = std::min(availableMinor, toMinor((c + t.feeCr) * fxSrc));
    return t;
}
} // namespace

// ---------------------------------------------------------------- payments
PaymentResult pay(ClientLedger& ledger, const BankDirectory& dir, const std::string& payeeBank, double amountCr, Purpose purpose,
                  int day, bool allowLifeline, const std::string& memo)
{
    PaymentResult r;
    r.amountCr = amountCr;
    if (!(amountCr > 0.0))
    {
        r.ok = true;
        return r;
    }
    const NationalBank* payee = dir.bank(payeeBank) ? dir.bank(payeeBank) : dir.reserve();
    if (!payee)
    {
        r.reason = "нет банков";
        return r;
    }
    const std::string& pid = payee->id;
    const double fxP = dir.fx(pid);

    enum Kind { CashKind, AccountKind };
    struct Debit
    {
        Kind kind;
        std::string bank;
        Minor minor;
    };
    std::vector<Debit> plan;
    double remaining = amountCr;
    const auto available = [](const Account& a) { return a.frozen ? std::max<Minor>(0, a.balance) : a.balance + a.overdraftLimit; };

    // 1. notes of the payee's currency, 2. the account at the payee's bank
    if (const auto it = ledger.cash.find(pid); it != ledger.cash.end())
    {
        const Take t = takeFrom(remaining, it->second, fxP, 0.0, 0.0);
        if (t.debit > 0)
        {
            plan.push_back({CashKind, pid, t.debit});
            remaining -= t.coveredCr;
        }
    }
    if (const auto* a = findAccount(ledger, pid))
    {
        const Take t = takeFrom(remaining, available(*a), fxP, 0.0, 0.0);
        if (t.debit > 0)
        {
            plan.push_back({AccountKind, pid, t.debit});
            remaining -= t.coveredCr;
        }
    }
    // 3. other accounts, home first: FX fee + transfer fee by the bloc channel
    std::vector<const Account*> others;
    for (const auto& a : ledger.accounts)
        if (a.bankId != pid)
            others.push_back(&a);
    std::stable_sort(others.begin(), others.end(),
                     [&](const Account* x, const Account* y) { return (x->bankId == ledger.homeBank) > (y->bankId == ledger.homeBank); });
    for (const auto* a : others)
    {
        const Channel ch = dir.channel(a->bankId, pid);
        const TransferFee& fee = dir.rules.transfer[static_cast<int>(ch)];
        if (!fee.allowed)
            continue;
        const bool sameCurrency = codeOf(dir, a->bankId) == payee->code;
        const Take t = takeFrom(remaining, available(*a), dir.fx(a->bankId), fee.share + (sameCurrency ? 0.0 : dir.rules.fxFeeShare),
                                fee.minCr);
        if (t.debit > 0)
        {
            plan.push_back({AccountKind, a->bankId, t.debit});
            remaining -= t.coveredCr;
            r.feesCr += t.feeCr;
        }
    }
    // 4. other notes at the exchange office
    for (const auto& [bank, minor] : ledger.cash)
    {
        if (bank == pid)
            continue;
        const Take t = takeFrom(remaining, minor, dir.fx(bank), dir.rules.cashExchangeShare, 0.0);
        if (t.debit > 0)
        {
            plan.push_back({CashKind, bank, t.debit});
            remaining -= t.coveredCr;
            r.feesCr += t.feeCr;
        }
    }
    // 5. lifeline credit: fuel / tow / minimum repair are never refused for lack of money
    double lifeline = 0.0;
    if (remaining > 0.005 && allowLifeline && isLifelinePurpose(purpose))
    {
        const double left = purpose == Purpose::Tow ? remaining : lifelineLeftCr(ledger, dir, day);
        lifeline = std::min(remaining, std::max(0.0, left));
        remaining -= lifeline;
    }
    if (remaining > 0.005)
    {
        r.reason = "недостаточно средств";
        return r;
    }

    // commit
    for (const auto& d : plan)
    {
        if (d.kind == CashKind)
            ledger.cash[d.bank] -= d.minor;
        else if (auto* a = findAccount(ledger, d.bank))
            a->balance -= d.minor;
    }
    if (lifeline > 0.0)
    {
        Loan* loan = findLoan(ledger, pid, LoanKind::Lifeline);
        if (!loan)
        {
            Loan l;
            l.id = ledger.nextLoanId++;
            l.bankId = pid;
            l.kind = LoanKind::Lifeline;
            l.annualRate = payee->lifelineRate;
            l.startDay = day;
            l.termDays = 360;
            l.nextDueDay = day + dir.rules.lifelineFreeDays;
            ledger.loans.push_back(l);
            loan = &ledger.loans.back();
        }
        const Minor m = toMinor(lifeline * fxP);
        loan->principal += m;
        loan->outstanding += m;
        ledger.lifelineDraws.emplace_back(day, lifeline);
        r.lifelineCr = lifeline;
    }
    r.ok = true;
    logEvent(ledger, day, "-" + money(amountCr * fxP, payee->code) + " " + memo +
                              (lifeline > 0.0 ? " (спасательный кредит " + money(lifeline * fxP, payee->code) + ")" : std::string()) +
                              (r.feesCr > 0.005 ? " (комиссии " + money(r.feesCr, "Cr") + ")" : std::string()));
    return r;
}

void receive(ClientLedger& ledger, const BankDirectory& dir, const std::string& payerBank, double amountCr, int day,
             const std::string& memo)
{
    if (!(amountCr > 0.0))
        return;
    const NationalBank* payer = dir.bank(payerBank) ? dir.bank(payerBank) : dir.reserve();
    const std::string pid = payer ? payer->id : ledger.homeBank;
    const int idx = ((day % 30) + 30) % 30;
    if (ledger.incomeDay[idx] != day)
    {
        ledger.incomeDay[idx] = day;
        ledger.incomeCr[idx] = 0.0;
    }
    ledger.incomeCr[idx] += amountCr;

    double rest = amountCr;
    double garnish = amountCr * dir.rules.garnishShare;
    for (auto& l : ledger.loans)
    {
        if (l.kind != LoanKind::Restructured || garnish <= 0.0 || l.outstanding <= 0)
            continue;
        const double fx = dir.fx(l.bankId);
        const double payCr = std::min(garnish, toUnits(l.outstanding) / fx);
        const Minor m = std::min(l.outstanding, toMinor(payCr * fx));
        l.outstanding -= m;
        l.hardship = false;
        garnish -= payCr;
        rest -= payCr;
    }
    rest = std::max(0.0, rest);
    Account* target = findAccount(ledger, pid);
    double fee = 0.0;
    if (!target)
    {
        target = &ensureAccount(ledger, ledger.homeBank);
        if (codeOf(dir, ledger.homeBank) != codeOf(dir, pid))
            fee = rest * dir.rules.fxFeeShare;
    }
    target->balance += toMinor((rest - fee) * dir.fx(target->bankId));
    logEvent(ledger, day, "+" + money(amountCr * dir.fx(pid), codeOf(dir, pid)) + " " + memo +
                              (rest < amountCr - 0.005 ? " (в погашение долга " + money(amountCr - rest, "Cr") + ")" : std::string()));
}

// ---------------------------------------------------------------- credit
Minor annuityInstallment(Minor principal, double annualRate, int periods30d)
{
    if (principal <= 0)
        return 0;
    const int n = std::max(1, periods30d);
    const double r = annualRate * 30.0 / 365.0;
    const double p = static_cast<double>(principal);
    const double pay = r <= 1.0e-12 ? p / n : p * r / (1.0 - std::pow(1.0 + r, -n));
    return static_cast<Minor>(std::ceil(pay));
}

double creditLimitCr(double annualNetIncomeCr, double collateralCr, double ltv)
{
    return std::max(0.0, std::min(ltv * collateralCr, 3.0 * annualNetIncomeCr));
}

double riskPremium(double annualNetIncomeCr, double routeRisk, double legalRisk, int priorDefaults, double debtToIncome)
{
    const double incomeScore = std::clamp(annualNetIncomeCr / 300000.0, 0.0, 1.0);
    return 0.02 * (1.0 - incomeScore) + 0.03 * std::clamp(routeRisk, 0.0, 1.0) + 0.02 * std::clamp(legalRisk, 0.0, 1.0) +
           0.04 * (priorDefaults > 0 ? 1.0 : 0.0) + 0.01 * std::clamp(debtToIncome, 0.0, 2.0);
}

Loan* takeLoan(ClientLedger& ledger, const BankDirectory& dir, const std::string& bankId, double amountCr, int termDays,
               double extraRate, int day)
{
    const auto* b = dir.bank(bankId);
    if (!b || !(amountCr > 0.0) || ledger.standing != Standing::Good)
        return nullptr;
    Loan l;
    l.id = ledger.nextLoanId++;
    l.bankId = bankId;
    l.kind = LoanKind::Term;
    l.principal = toMinor(amountCr * dir.fx(bankId));
    l.outstanding = l.principal;
    l.annualRate = b->loanRate + extraRate;
    l.startDay = day;
    l.termDays = std::max(30, termDays);
    l.nextDueDay = day + 30;
    l.installment = annuityInstallment(l.principal, l.annualRate, l.termDays / 30);
    ensureAccount(ledger, bankId).balance += l.principal;
    ledger.loans.push_back(l);
    logEvent(ledger, day, "кредит " + money(toUnits(l.principal), b->code) + " на " + std::to_string(l.termDays) + " сут под " +
                              money(l.annualRate * 100.0, "%"));
    return &ledger.loans.back();
}

double lifelineLeftCr(const ClientLedger& ledger, const BankDirectory& dir, int day)
{
    double used = 0.0;
    for (const auto& [d, cr] : ledger.lifelineDraws)
        if (d > day - 90)
            used += cr;
    return std::max(0.0, dir.rules.lifelineLimitCr90d - used);
}

double trailingIncomeCr(const ClientLedger& ledger, int day)
{
    double s = 0.0;
    for (int i = 0; i < 30; ++i)
        if (ledger.incomeDay[i] > day - 30 && ledger.incomeDay[i] <= day)
            s += ledger.incomeCr[i];
    return s;
}

double netWorthCr(const ClientLedger& ledger, const BankDirectory& dir)
{
    double s = 0.0;
    for (const auto& [bank, minor] : ledger.cash)
        s += toUnits(minor) / dir.fx(bank);
    for (const auto& a : ledger.accounts)
        s += toUnits(a.balance) / dir.fx(a.bankId);
    return s - totalDebtCr(ledger, dir);
}

double totalDebtCr(const ClientLedger& ledger, const BankDirectory& dir)
{
    double s = 0.0;
    for (const auto& l : ledger.loans)
        s += toUnits(l.outstanding) / dir.fx(l.bankId);
    return s;
}

// ---------------------------------------------------------------- the daily step
LedgerDayReport stepLedgerDay(ClientLedger& ledger, const BankDirectory& dir, int day)
{
    LedgerDayReport rep;
    if (day <= ledger.lastDay)
        return rep;
    ledger.lastDay = day;
    const BankRules& rules = dir.rules;

    // accounts: deposit interest on credit balances, overdraft interest on debit balances
    for (auto& a : ledger.accounts)
    {
        const auto* b = dir.bank(a.bankId);
        if (!b)
            continue;
        const double rate = a.balance >= 0 ? b->depositRate : b->overdraftRate;
        const Minor m = dailyInterest(a.balance, rate, a.carry);
        a.balance += m;
        (m >= 0 ? rep.interestEarnedCr : rep.interestChargedCr) += std::abs(toUnits(m)) / dir.fx(a.bankId);
        a.overdueDays = a.balance < -a.overdraftLimit ? a.overdueDays + 1 : 0;
    }

    // loans: interest (in duplum), due dates, auto-debit from a CREDIT balance only
    const double income30 = trailingIncomeCr(ledger, day);
    for (auto& l : ledger.loans)
    {
        const auto* b = dir.bank(l.bankId);
        if (!b)
            continue;
        const double fx = dir.fx(l.bankId);
        if (l.kind == LoanKind::Restructured)
            l.hardship = income30 <= 0.0;
        const bool freePeriod = l.kind == LoanKind::Lifeline && day < l.startDay + rules.lifelineFreeDays;
        if (!freePeriod && !l.hardship)
        {
            Minor m = dailyInterest(l.outstanding, l.annualRate, l.carry);
            const Minor cap = static_cast<Minor>(rules.inDuplumCap * static_cast<double>(l.principal)) - l.interestCharged;
            m = std::clamp<Minor>(m, 0, std::max<Minor>(0, cap));
            l.outstanding += m;
            l.interestCharged += m;
            rep.interestChargedCr += toUnits(m) / fx;
        }
        if (l.kind == LoanKind::Restructured)
        {
            // repaid only by garnishing income (receive); the installment is shown, never put in arrears
            const int periodsLeft = std::max(1, (l.startDay + l.termDays - day) / 30);
            l.installment = std::min(annuityInstallment(l.outstanding, l.annualRate, periodsLeft),
                                     toMinor(rules.garnishShare * income30 * fx));
            l.arrears = 0;
            l.overdueDays = 0;
            continue;
        }
        if (l.nextDueDay > 0 && day >= l.nextDueDay)
        {
            if (l.installment <= 0)
                l.installment = annuityInstallment(l.outstanding, l.annualRate, std::max(1, l.termDays / 30));
            l.arrears = std::min(l.outstanding, l.arrears + l.installment);
            l.nextDueDay += 30;
        }
        if (l.arrears > 0)
        {
            if (auto* a = findAccount(ledger, l.bankId); a && a->balance > 0)
            {
                const Minor m = std::min(a->balance, l.arrears);
                a->balance -= m;
                l.arrears -= m;
                l.outstanding -= m;
                rep.collectedCr += toUnits(m) / fx;
            }
        }
        l.overdueDays = l.arrears > 0 ? l.overdueDays + 1 : 0;
        if (l.overdueDays > rules.graceDays && !l.lateFeeCharged)
        {
            // a one-off late fee, inside the in-duplum cap like interest
            const Minor cap = std::max<Minor>(0, static_cast<Minor>(rules.inDuplumCap * static_cast<double>(l.principal)) - l.interestCharged);
            const Minor fee = std::min(cap, toMinor(toUnits(l.arrears) * rules.lateFeeShare));
            l.outstanding += fee;
            l.arrears += fee;
            l.interestCharged += fee;
            l.lateFeeCharged = true;
            logEvent(ledger, day, "штраф за просрочку " + money(toUnits(fee), b->code));
        }
    }
    for (auto it = ledger.loans.begin(); it != ledger.loans.end();)
    {
        if (it->outstanding <= 0)
        {
            logEvent(ledger, day, "долг погашен (" + codeOf(dir, it->bankId) + ")");
            it = ledger.loans.erase(it);
        }
        else
            ++it;
    }

    // the standing machine
    int maxOverdue = 0;
    for (const auto& l : ledger.loans)
        maxOverdue = std::max(maxOverdue, l.overdueDays);
    for (const auto& a : ledger.accounts)
        maxOverdue = std::max(maxOverdue, a.overdueDays);

    if (maxOverdue > rules.restructureAfterDays)
    {
        // consolidate the overdue debts of each bank into one restructured loan at the policy rate
        std::vector<std::string> banks;
        for (const auto& l : ledger.loans)
            if (l.kind != LoanKind::Restructured && l.overdueDays > rules.restructureAfterDays)
                banks.push_back(l.bankId);
        for (const auto& a : ledger.accounts)
            if (a.overdueDays > rules.restructureAfterDays)
                banks.push_back(a.bankId);
        std::sort(banks.begin(), banks.end());
        banks.erase(std::unique(banks.begin(), banks.end()), banks.end());
        for (const auto& bankId : banks)
        {
            const auto* b = dir.bank(bankId);
            if (!b)
                continue;
            Minor sum = 0;
            for (auto it = ledger.loans.begin(); it != ledger.loans.end();)
            {
                if (it->bankId == bankId && it->kind != LoanKind::Restructured)
                {
                    sum += it->outstanding;
                    it = ledger.loans.erase(it);
                }
                else
                    ++it;
            }
            if (auto* a = findAccount(ledger, bankId); a && a->balance < 0)
            {
                sum += -a->balance;
                a->balance = 0;
                a->overdueDays = 0;
            }
            if (sum <= 0)
                continue;
            Loan* r = findLoan(ledger, bankId, LoanKind::Restructured);
            if (!r)
            {
                Loan l;
                l.id = ledger.nextLoanId++;
                l.bankId = bankId;
                l.kind = LoanKind::Restructured;
                l.annualRate = b->policyRate;
                l.startDay = day;
                l.termDays = rules.restructureTermDays;
                ledger.loans.push_back(l);
                r = &ledger.loans.back();
            }
            r->principal += sum;
            r->outstanding += sum;
            r->hardship = income30 <= 0.0;
            rep.restructured = true;
            logEvent(ledger, day, "реструктуризация в банке " + b->name + ": " + money(toUnits(sum), b->code) + " под " +
                                      money(b->policyRate * 100.0, "%") + ", погашение 25 % дохода");
        }
        maxOverdue = 0;
        for (const auto& l : ledger.loans)
            maxOverdue = std::max(maxOverdue, l.overdueDays);
        for (const auto& a : ledger.accounts)
            maxOverdue = std::max(maxOverdue, a.overdueDays);
    }

    bool restructuredLeft = false;
    for (const auto& l : ledger.loans)
        restructuredLeft = restructuredLeft || l.kind == LoanKind::Restructured;
    const Standing next = restructuredLeft ? Standing::Restructured
                          : maxOverdue == 0 ? Standing::Good
                          : maxOverdue <= rules.graceDays ? Standing::Grace
                                                          : Standing::Delinquent;
    if (next != ledger.standing)
    {
        rep.standingChanged = true;
        ledger.standing = next;
        logEvent(ledger, day, std::string("статус: ") + standingRu(next));
    }
    const bool freeze = next == Standing::Delinquent || next == Standing::Restructured;
    for (auto& a : ledger.accounts)
        a.frozen = freeze;

    // forget lifeline draws older than the window
    ledger.lifelineDraws.erase(std::remove_if(ledger.lifelineDraws.begin(), ledger.lifelineDraws.end(),
                                              [&](const auto& d) { return d.first <= day - 90; }),
                               ledger.lifelineDraws.end());
    return rep;
}

double stepFx(const NationalBank& bank, double fxNow, double shock)
{
    if (bank.reserve || bank.fxAnchorPerCr <= 0.0)
        return bank.fxAnchorPerCr > 0.0 ? bank.fxAnchorPerCr : 1.0;
    const double theta = std::log(2.0) / std::max(1.0, bank.fxHalfLifeDays);
    double x = std::log(std::max(1.0e-9, fxNow) / bank.fxAnchorPerCr);
    x += -theta * x + bank.fxSigmaDay * std::clamp(shock, -4.0, 4.0);
    const double band = std::log(1.0 + std::max(0.0, bank.fxBand));
    x = std::clamp(x, -band, band);
    return bank.fxAnchorPerCr * std::exp(x);
}
} // namespace game::economy
