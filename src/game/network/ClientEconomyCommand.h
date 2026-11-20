#pragma once

#include <cstdint>
#include <string>

// A trade-layer request of the client (docs/TRADE_DESIGN.md §7.4) - not a ship control (ClientShipCommand). The
// server queues it per controlled ship and runs it in that ship's system context, like the ship commands; the reply
// (ClientTradeResult) comes back in the session's ClientTradeSnapshot. The client never decides a price.
namespace game::network
{
struct ClientEconomyCommand
{
    enum Op : std::uint8_t
    {
        Buy = 0,         // goodId, quantity t at hubId (docked); limitUnitCr: not dearer than (0 = no limit)
        Sell = 1,        // limitUnitCr: not cheaper than
        DevCargoAdd = 2, // DEV: put `quantity` t of goodId aboard without paying (cost basis 0)
        DevCargoClear = 3,
        TakeContract = 4,    // contractId: a posting of the board of hubId (docked there); its cargo is loaded at once
        AbandonContract = 5, // contractId: at its origin hub before the deadline = cancel (fee); elsewhere = failure
        DevEconomyDays = 6,  // DEV: the economy clock runs `quantity` days ahead (the board's days without waiting)
        DevSpendAll = 7,     // DEV: the player spends all spendable money here (cash + the overdraft to its limit)
        SellBlack = 8,       // goodId, quantity t: to the black market of the hub (contraband here: banned / restricted; step 9)
        DevCustoms = 9,      // DEV: the customs inspection probability forced to `quantity` (< 0 = the rules again)
        BuyLicence = 10,     // goodId = licence type: from the polity of the hub the ship is docked at (step 11)
        InsureCargo = 11,    // the own lots aboard, at a berth (step 13)
        DevGrant = 12,       // DEV: `quantity` Cr credited to the player (live checks of purchases)
        TakeSos = 13,        // goodId = SOS call id (F5 МЧС, step 12): the licensee takes it
        HireBot = 14,        // goodId = candidate id, quantity = the owned ship's entity id (step 15)
        OrderBotLoop = 15,   // goodId = bot id: the trade loop from the bot ship's hub
        FireBot = 16,        // goodId = bot id
        SignConstruction = 17,   // goodId = offer id (step 16): the contract on a planned project of this hub
        LeaseSite = 18,          // goodId = site offer id (step 16 part 2): a mining concession at this hub
        Last = LeaseSite
    };
    Op op = Buy;
    std::uint32_t requestId = 0;
    std::uint64_t shipEntityId = 0;   // informational: the server acts on the session's controlled ship
    std::string hubId;
    std::string goodId;
    double quantity = 0.0;            // tonnes
    double limitUnitCr = 0.0;
    std::int32_t quoteDay = -1;       // the economy day the client priced it on (-1 = any)
    std::string contractId;           // TakeContract / AbandonContract (trade step 5)
};
} // namespace game::network
