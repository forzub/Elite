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
