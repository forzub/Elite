#pragma once

#include <functional>
#include <string>
#include <vector>

#include "src/game/economy/EconomyModel.h"

// F8 Trade page model: the bulletin board of the hub the player is at, its
// market, the hubs of the system, and the card of a selected destination
// (owner, distance, last known prices aged by the beacon-network delay,
// running events). Built from the deterministic EconomyModel only.
namespace game::presentation
{
struct TradeBoardRow
{
    game::economy::BoardEntry entry;
    std::string destinationName;
    std::string destinationSystem;
    std::string goodRu;
    int daysLeft = 0;
};

struct TradeHubCard
{
    bool valid = false;
    game::economy::HubEconomyInfo info;
    std::string distanceText;
    bool connected = false;
    double infoAgeHours = 0.0;          // age of the newest price bulletin that reached us
    std::vector<game::economy::MarketQuote> knownPrices;   // traded goods at that age
    std::vector<game::economy::ActiveEvent> knownEvents;
    std::string channel;
};

struct TradePresentation
{
    bool valid = false;
    std::string hubId;                  // the hub the page belongs to (player's / nearest)
    std::string hubName;
    bool atHub = false;
    int day = 0;
    int tab = 0;                        // 0 board, 1 market, 2 hubs
    std::vector<TradeBoardRow> board;
    std::vector<game::economy::MarketQuote> market;
    std::vector<game::economy::ActiveEvent> events;
    std::vector<const game::economy::HubEconomyInfo*> hubs;
    std::string selectedHub;
    TradeHubCard card;
};

// distanceText(hubId) -> "12 400 km" / "4.3 ly" / "" (unknown)
[[nodiscard]] TradePresentation buildTradePresentation(
    const std::string& hubId,
    bool atHub,
    double universeSeconds,
    int tab,
    const std::string& selectedHub,
    const std::function<std::string(const std::string&)>& distanceText
);
} // namespace game::presentation
