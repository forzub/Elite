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
        Last = DevEconomyDays
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
