#pragma once

#include <string>
#include <vector>

// Per-session projection of the player's money (server-authoritative ledger, src/game/economy/EconomyServer).
// The client only shows it (F8 Trade -> "ФИНАНСЫ"); it never changes a balance itself.
namespace game::simulation
{
struct ClientMoneyLine
{
    std::string bankId;
    std::string label;        // bank name / "наличные" / loan kind
    std::string currency;     // code: CR, HRL, ...
    double amount = 0.0;      // in that currency (a debt is positive)
    double amountCr = 0.0;    // the same in Cr at today's rate
    double limit = 0.0;       // arranged overdraft / installment
    double annualRate = 0.0;
    std::string state;        // "", "заморожен", "просрочка 12 сут", ...
};

// licences (TRADE_DESIGN §4, step 11; shown in F5)
struct ClientLicenceLine
{
    std::string type;
    std::string ru;
    std::string polity;        // the issuing polity (name)
    int expiresDay = 0;
    bool active = false;
};

struct ClientLicenceOffer
{
    std::string type;
    std::string ru;
    double feeCr = 0.0;        // at this hub's polity, x its port index
    std::string goodsRu;       // what it covers
    bool held = false;         // already active here
};

struct ClientPilotCategory
{
    std::string ru;
    std::string control;       // "assisted" / "newtonian"
    bool granted = false;
    std::string note;
};

// the employment service (F5, TRADE_DESIGN §11b)
struct ClientVacancy
{
    std::string id;
    std::string kind;          // "courier" | "military"
    std::string employerRu;
    std::string roleRu;
    std::string shipRu;
    std::string requirementRu;
    double wageCr = 0.0;       // per 30 days
    int termDays = 0;          // 0 = open-ended
    bool eligible = true;
    std::string whyNotRu;
};

// SOS calls of this system (F5 МЧС, step 12)
struct ClientSos
{
    std::string id;
    std::string kindRu;
    std::string hubId;
    std::string hubName;
    double rewardCr = 0.0;
    int expiresDay = 0;
    std::string status;        // open / taken / done / expired
    bool mine = false;
};

struct ClientWalletSnapshot
{
    bool valid = false;
    int day = 0;
    std::string homeBank;
    std::string standing;     // good / grace / delinquent / restructured
    std::string standingRu;
    std::vector<ClientMoneyLine> cash;
    std::vector<ClientMoneyLine> accounts;
    std::vector<ClientMoneyLine> loans;
    double netWorthCr = 0.0;
    double lifelineLeftCr = 0.0;
    double freeFuelListCr = 0.0;   // list value of the fuel received free of charge so far
    double heldCr = 0.0;           // blocked by holds (escrow): the client's money, not spendable
    // income garnish (TRADE_DESIGN §1.3): the restructured debt, the latest withholding, the protected minimum (Cr)
    double garnishDebtCr = 0.0;
    double lastWithheldCr = 0.0;
    int lastWithheldDay = -1;
    double protectedCr = 0.0;
    // the income tax of the residence polity (TRADE_DESIGN §5.2, step 7; shown in F5)
    std::string taxResidence;
    std::string taxStyleRu;
    double taxRate = 0.0;
    double taxAdminFeeShare = 0.0;
    int taxPeriodDays = 90;
    int taxDeclareDays = 15;
    double taxWithholdingShare = 0.0;
    int taxPeriodStartDay = -1;           // the open period
    double taxRevenueCr = 0.0;
    double taxExpensesCr = 0.0;
    double taxWithheldCreditCr = 0.0;
    double taxWithheldLostCr = 0.0;
    double taxEstimateCr = 0.0;           // if the period closed today
    int taxAssessedPeriodStartDay = -1;   // the last closed period (-1: none yet)
    double taxAssessedProfitCr = 0.0;
    double taxAssessedCr = 0.0;
    double taxAssessedFeeCr = 0.0;
    int taxDueDay = -1;
    bool taxSettled = true;
    double taxPaidCr = 0.0;
    double taxDebtCr = 0.0;
    double taxPenaltyCr = 0.0;
    // licences (step 11): held, offered by this hub's polity, the pilot licence categories
    std::vector<ClientLicenceLine> licences;
    std::vector<ClientLicenceOffer> licenceOffers;
    std::string licencePolity;
    std::vector<ClientPilotCategory> pilotCategories;
    // the employment service: this hub's vacancies and the pilot's current job
    std::vector<ClientVacancy> vacancies;
    bool employed = false;
    std::string jobKind;
    std::string jobEmployerRu;
    std::string jobRoleRu;
    std::string jobShipRu;
    std::string jobUnitRu;
    std::string jobRankRu;
    double jobWageCr = 0.0;
    int jobSinceDay = -1;
    int jobTermEndDay = -1;
    // rescue (step 12): the SOS calls of this system, the pilot's active call, the licence here
    std::vector<ClientSos> sos;
    std::string activeSosId;
    bool sarLicence = false;

    // where the ship is: the polity whose bank serves payments here
    std::string localPolity;
    std::string localBank;
    std::string localCurrency;
    double localFxPerCr = 1.0;

    // the fuel price here (the docked hub, or the system's reference hub)
    bool fuelQuoteValid = false;
    std::string fuelHubName;
    bool fuelFree = true;
    double jumpChargeListPerT = 0.0;      // local currency
    double jumpChargeChargedPerT = 0.0;   // what is actually charged (0 while fuel is free)
    double jumpChargeListCrPerT = 0.0;
    double jumpChargeStockT = 0.0;
    double reactorFuelListPerKg = 0.0;    // local currency
    std::vector<std::string> fuelBreakdown;   // "сырьё (изотопы) 2 083 Cr/т", ...

    std::vector<std::string> notices;     // the ledger's latest events, newest last
};
} // namespace game::simulation
