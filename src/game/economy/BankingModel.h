#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// National banks, wallets, accounts, loans (docs/ECONOMY_BANKING.md, PROPOSAL 2026-11-19).
//
// PURE model: no I/O, no clock, no globals. The server (EconomyServer) owns the state and calls these
// functions once per world day and on every payment. Amounts are integer minor units (1/100 of the
// currency's unit) so a ledger never drifts by float rounding; interest keeps its sub-minor remainder
// in a per-item carry.
//
// The no-death-spiral guarantees (the author's main principle, each tested in tests/economy):
//   1. lifeline: fuel (up to rules.lifelineFuelMaxT), a tow and a minimum repair are always payable -
//      on credit if nothing else is left (tow never refused, even beyond the 90-day limit);
//   2. in duplum: the interest charged on a debt never exceeds inDuplumCap x its principal;
//   3. restructuring: 60 days overdue -> the debts of that bank become one restructured loan at the
//      bank's policy rate, repaid ONLY from rules.garnishShare of incoming money; no income in the last
//      30 days -> no interest (hardship freeze), no penalty, no seizure;
//   4. auto-debits never push an account into its overdraft; the sole ship is never seized.
namespace game::economy
{
using Minor = std::int64_t;   // 1/100 of a currency unit

[[nodiscard]] Minor toMinor(double units);
[[nodiscard]] inline double toUnits(Minor m) { return static_cast<double>(m) / 100.0; }

enum class Purpose : int
{
    Fuel = 0,
    Tow = 1,
    Repair = 2,
    Docking = 3,
    Trade = 4,
    Fee = 5,
    Other = 6,
    Fine = 7,    // an administrative fine (traffic control, navigation zones): never refused - booked as debt
    Parking = 8, // the berth fee beyond the hours the docking fee covers (TRADE_DESIGN §5.2, step 4): never refused
};
[[nodiscard]] inline bool isLifelinePurpose(Purpose p) { return p == Purpose::Fuel || p == Purpose::Tow || p == Purpose::Repair; }

enum class Channel : int
{
    Open = 0,
    Licensed = 1,
    Restricted = 2,
    Black = 3,
    Closed = 4,
};
[[nodiscard]] Channel channelFromString(const std::string& s);
[[nodiscard]] const char* channelName(Channel c);

struct NationalBank
{
    std::string id;            // = polity id (world_economy)
    std::string polityName;
    std::string name;
    std::string currencyName;
    std::string code;          // "CR", "HRL", ...
    std::string bloc;
    int systemId = -1;
    double quality = 0.5;      // institutional quality 0..1 (regime)
    double inflation = 0.02;   // annual
    double policyRate = 0.04;  // annual
    double depositRate = 0.03;
    double loanRate = 0.07;
    double overdraftRate = 0.13;
    double lifelineRate = 0.05;
    bool reserve = false;      // the Cr itself (Sol Authority)
    double fxAnchorPerCr = 1.0;    // local units per 1 Cr, the managed-float centre
    double fxSigmaDay = 0.0;       // daily volatility
    double fxHalfLifeDays = 730.0; // mean reversion towards the anchor
    double fxBand = 0.0;           // +- share around the anchor the bank defends
    std::string regime;            // the polity's form of government (national_banks.json): its fiscal style (trade_fees.json)
};

struct TransferFee
{
    bool allowed = true;
    double share = 0.0;
    double minCr = 0.0;
};

struct BankRules
{
    int graceDays = 30;
    int restructureAfterDays = 60;
    double lateFeeShare = 0.02;
    int restructureTermDays = 720;
    double garnishShare = 0.25;
    double inDuplumCap = 1.0;
    double lifelineLimitCr90d = 25000.0;
    int lifelineFreeDays = 90;
    double lifelineFuelMaxT = 1.0;
    double towCostCr = 14000.0;
    double minRepairCr = 10000.0;
    double fxFeeShare = 0.01;
    double cashExchangeShare = 0.03;
    TransferFee transfer[5] = {{true, 0.001, 2.0}, {true, 0.005, 10.0}, {true, 0.02, 50.0}, {false, 0, 0}, {false, 0, 0}};
    double startAccountCr = 20000.0;
    double startCashCr = 1000.0;
    double startOverdraftCr = 4000.0;
};

// banks + the live exchange rates + the bloc channel matrix
class BankDirectory
{
public:
    void add(NationalBank bank);
    void setChannel(const std::string& fromBloc, const std::string& toBloc, Channel c);
    void setLifelineBank(const std::string& polityWithoutBank, const std::string& bankId);

    [[nodiscard]] const NationalBank* bank(const std::string& id) const;
    [[nodiscard]] const NationalBank* reserve() const;
    // the bank that serves payments at a hub of `polity` (no bank -> its lifeline bank -> the reserve)
    [[nodiscard]] const NationalBank* bankForPolity(const std::string& polity) const;
    [[nodiscard]] double fx(const std::string& bankId) const;   // local units per Cr (1 for unknown)
    void setFx(const std::string& bankId, double perCr);
    [[nodiscard]] Channel channel(const std::string& fromBank, const std::string& toBank) const;
    [[nodiscard]] const std::vector<NationalBank>& banks() const { return m_banks; }

    BankRules rules;

private:
    std::vector<NationalBank> m_banks;
    std::vector<double> m_fx;
    std::unordered_map<std::string, std::size_t> m_index;
    std::map<std::pair<std::string, std::string>, Channel> m_matrix;
    std::unordered_map<std::string, std::string> m_lifelineBank;
};

struct Account
{
    std::string bankId;
    Minor balance = 0;          // bank currency
    Minor overdraftLimit = 0;   // arranged overdraft (>= 0)
    bool frozen = false;        // delinquent / restructured: no new overdraft draws
    int overdueDays = 0;        // days spent beyond the arranged limit
    Minor held = 0;             // blocked by holds (escrow, docs/TRADE_DESIGN.md §7.1): not spendable
    double carry = 0.0;         // sub-minor interest remainder
};

enum class LoanKind : int
{
    Term = 0,
    Lifeline = 1,
    Restructured = 2,
};

struct Loan
{
    int id = 0;
    std::string bankId;
    LoanKind kind = LoanKind::Term;
    Minor principal = 0;        // what was lent (the in-duplum base)
    Minor outstanding = 0;      // what is owed now (principal left + charged interest + fees)
    Minor interestCharged = 0;  // all interest ever charged (capped by inDuplumCap x principal)
    Minor arrears = 0;          // due and unpaid
    Minor installment = 0;      // per 30 days
    double annualRate = 0.0;
    int startDay = 0;
    int termDays = 360;
    int nextDueDay = 0;
    int overdueDays = 0;
    bool lateFeeCharged = false;
    bool hardship = false;      // restructured, no income: interest frozen
    double carry = 0.0;
};

enum class Standing : int
{
    Good = 0,
    Grace = 1,
    Delinquent = 2,
    Restructured = 3,
};
[[nodiscard]] const char* standingName(Standing s);
[[nodiscard]] const char* standingRu(Standing s);

// A hold (escrow, TRADE_DESIGN §7.1): money blocked on an account - still the client's, not spendable until released
struct Hold
{
    int id = 0;
    std::string bankId;
    Minor amount = 0;   // bank currency
    int day = 0;
    std::string memo;
};

struct LedgerEvent
{
    int day = 0;
    std::string text;
};

struct ClientLedger
{
    std::uint64_t ownerId = 0;
    std::string homeBank;
    std::map<std::string, Minor> cash;   // bank id -> notes of that currency
    std::vector<Account> accounts;
    std::vector<Loan> loans;
    Standing standing = Standing::Good;
    int nextLoanId = 1;
    int lastDay = -1;
    // trailing income (Cr) by day % 30, with the day it belongs to
    double incomeCr[30] = {};
    int incomeDay[30] = {};
    std::vector<std::pair<int, double>> lifelineDraws;   // (day, Cr) - the 90-day limit window
    std::vector<LedgerEvent> events;                     // newest last, at most 24
    double freeFuelListCr = 0.0;                         // list value of fuel received free of charge
    std::vector<Hold> holds;                             // blocked money (escrow): collateral, a deal in flight
    int nextHoldId = 1;
};

struct PaymentResult
{
    bool ok = false;
    double amountCr = 0.0;
    double feesCr = 0.0;
    double lifelineCr = 0.0;   // drawn on lifeline credit
    std::string reason;        // when !ok
};

// ---- ledger construction / lookup
[[nodiscard]] ClientLedger openLedger(const BankDirectory& dir, std::uint64_t ownerId, const std::string& homeBank, int day);
[[nodiscard]] Account* findAccount(ClientLedger& ledger, const std::string& bankId);
[[nodiscard]] const Account* findAccount(const ClientLedger& ledger, const std::string& bankId);
Account& ensureAccount(ClientLedger& ledger, const std::string& bankId);
void logEvent(ClientLedger& ledger, int day, std::string text);

// ---- money flows
// Pays `amountCr` (Cr value) to a payee banking with `payeeBank` (its currency). Sources in order: cash
// of that currency, the account at that bank (incl. its arranged overdraft unless frozen), other accounts
// (home first; FX fee + inter-bank transfer fee by the bloc channel; black / closed channels refuse),
// other cash (exchange office), then - for a lifeline purpose and `allowLifeline` - lifeline credit.
// All or nothing: a failed payment changes nothing.
PaymentResult pay(ClientLedger& ledger, const BankDirectory& dir, const std::string& payeeBank, double amountCr, Purpose purpose,
                  int day, bool allowLifeline, const std::string& memo);
// The most pay() (no lifeline) can cover to `payeeBank` right now, Cr: the same sources in the same order with their
// FX / transfer / exchange fees taken off (TRADE_DESIGN step 3: the deal window's money limit = what the server allows).
[[nodiscard]] double spendableCr(const ClientLedger& ledger, const BankDirectory& dir, const std::string& payeeBank);
// An administrative fine payable to the authority banking with `authorityBank` (docs/HUB_SECURITY.md): paid like any
// payment (cash, the account incl. its arranged overdraft, other accounts); what cannot be paid is NOT refused - it is
// booked as a fine debt at that bank (a 0 % term loan over 360 days, PaymentResult::lifelineCr = the booked part), so a
// fine never empties the ship or blocks fuel (the no-death-spiral rules then cover the debt like any other).
PaymentResult payFine(ClientLedger& ledger, const BankDirectory& dir, const std::string& authorityBank, double amountCr, int day,
                      const std::string& memo);
// The same never-refused rule for any obligation to an authority / a port (docking, berth fees - TRADE_DESIGN step 4:
// a port fee must not strand the player): paid like any payment, else booked whole as a 0 % debt (12 x 30 days) with
// `debtLabel` in the ledger ("сбор в долг", "штраф в долг").
PaymentResult payOrDebt(ClientLedger& ledger, const BankDirectory& dir, const std::string& authorityBank, double amountCr, Purpose purpose,
                        int day, const std::string& memo, const std::string& debtLabel);
// Income paid by someone banking with `payerBank`: garnish share to restructured debt first, the rest to
// the account at that bank if the client has one, else home (converted, FX fee).
void receive(ClientLedger& ledger, const BankDirectory& dir, const std::string& payerBank, double amountCr, int day,
             const std::string& memo);

// ---- credit
[[nodiscard]] Minor annuityInstallment(Minor principal, double annualRate, int periods30d);
// BANKING.md: limit = min(LTV x collateral, 3 x annual net income); rate = loanRate + risk premium
[[nodiscard]] double creditLimitCr(double annualNetIncomeCr, double collateralCr, double ltv);
[[nodiscard]] double riskPremium(double annualNetIncomeCr, double routeRisk, double legalRisk, int priorDefaults, double debtToIncome);
Loan* takeLoan(ClientLedger& ledger, const BankDirectory& dir, const std::string& bankId, double amountCr, int termDays,
               double extraRate, int day);
[[nodiscard]] double lifelineLeftCr(const ClientLedger& ledger, const BankDirectory& dir, int day);

// ---- holds (escrow): block `amountCr` on the account at `bankId` (its own money only - not the overdraft); 0 = not
// enough free money there. release returns the money to the free balance (step 5 adds capture to a payee).
int placeHold(ClientLedger& ledger, const BankDirectory& dir, const std::string& bankId, double amountCr, int day, const std::string& memo);
bool releaseHold(ClientLedger& ledger, int holdId);
[[nodiscard]] double heldCr(const ClientLedger& ledger, const BankDirectory& dir);

// ---- the daily step (interest, due dates, standing machine, restructuring)
struct LedgerDayReport
{
    double interestEarnedCr = 0.0;
    double interestChargedCr = 0.0;
    double collectedCr = 0.0;
    bool standingChanged = false;
    bool restructured = false;
};
LedgerDayReport stepLedgerDay(ClientLedger& ledger, const BankDirectory& dir, int day);

[[nodiscard]] double trailingIncomeCr(const ClientLedger& ledger, int day);
[[nodiscard]] double netWorthCr(const ClientLedger& ledger, const BankDirectory& dir);
[[nodiscard]] double totalDebtCr(const ClientLedger& ledger, const BankDirectory& dir);

// ---- exchange rates: one managed-float day (Ornstein-Uhlenbeck to the anchor, kept inside the band)
// `shock` is a standard-normal-ish draw supplied by the caller (deterministic hash noise on the server).
[[nodiscard]] double stepFx(const NationalBank& bank, double fxNow, double shock);
} // namespace game::economy
