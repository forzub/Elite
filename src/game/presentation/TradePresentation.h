#pragma once

#include <functional>
#include <string>
#include <vector>

#include "src/game/economy/EconomyModel.h"
#include "src/game/simulation/ClientWalletSnapshot.h"
#include "src/game/simulation/ClientTradeSnapshot.h"

// F8 Trade page model: the bulletin board of the hub the player is at, its
// market, the hubs of the system, and the card of a selected destination
// (owner, distance, last known prices aged by the beacon-network delay,
// running events). The board, the hub list and the remote cards come from the deterministic EconomyModel; the
// market here, the cargo and the results are the SERVER's (filled by the caller from the session snapshot).
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

// the navigation data tab (JUMP_MECHANICS 10l): the colony bank, the onboard computer's memory, the data carriers
struct NavDataRow
{
    std::string id;           // the bank record id ("" for a memory / carrier line)
    std::string corridor;     // "Фортуна-1"
    std::string place;        // the shell's place name
    std::string tierText;
    bool open = true;
    double surveyYear = 0.0;
    double ageYears = 0.0;
    std::string stateText;    // known / estimate / outdated / unknown now
    int priceCr = 0;
};

struct NavDataPage
{
    std::string bankName;                 // "" = no bank in this system
    bool atHub = false;
    int computerState = 0;                // 0 unknown, 1 working, 2 down
    std::vector<NavDataRow> bank;
    std::vector<NavDataRow> memory;
    std::vector<std::string> carriers;    // "label (N rec.)"
    std::string selected;                 // the selected bank record id
};

struct TradePresentation
{
    bool valid = false;
    std::string hubId;                  // the hub the page belongs to (player's / nearest)
    std::string hubName;
    bool atHub = false;
    int day = 0;
    int tab = 0;                        // 0 board, 1 market, 2 hubs, 3 navigation data, 4 finance, 5 cargo
    std::vector<TradeBoardRow> board;
    // the market of the hub the ship is at AS THE SERVER PRICES IT (session.trade - one price of record; audit
    // 2026-11-19: it was the stateless EconomyModel::market, while the server charged its own cells)
    std::vector<game::economy::MarketQuote> market;
    std::vector<game::simulation::ClientMarketRow> marketLimits;   // the same rows: target, the hub limits
    std::vector<game::economy::ActiveEvent> events;
    std::vector<const game::economy::HubEconomyInfo*> hubs;
    std::string selectedHub;
    TradeHubCard card;
    NavDataPage nav;
    game::simulation::ClientWalletSnapshot finance;   // the server ledger as replicated to this session
    game::simulation::ClientTradeSnapshot trade;      // the server market, the manifest, the latest results
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
