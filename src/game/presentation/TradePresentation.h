#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "src/game/economy/EconomyModel.h"
#include "src/game/simulation/ClientWalletSnapshot.h"
#include "src/game/simulation/ClientTradeSnapshot.h"
#include "src/game/presentation/TradeDealPresentation.h"

// F8 Trade page model: the contract board of the hub the player is at, its
// market, the hubs of the system, and the card of a selected destination
// (owner, distance, last known prices aged by the beacon-network delay,
// running events). The hub list and the remote cards come from the deterministic EconomyModel; the board and the
// contracts (trade step 5), the market here, the cargo and the results are the SERVER's (filled by the caller from the
// session snapshot).
namespace game::presentation
{
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
    int tab = 0;                        // 0 contracts, 1 market, 2 hubs, 3 navigation data, 4 finance, 5 cargo
    // the CONTRACTS tab (trade step 5): the server's board of this hub and the player's contracts come in `trade`
    // (ClientTradeSnapshot board / contracts); the selected posting and the confirm state are the window's
    std::string selectedContract;
    bool contractConfirm = false;           // "take?" asked, waiting for the second click / Enter
    std::uint32_t contractRequest = 0;      // the last take / abandon request sent
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
    // the MARKET tab's deal window (trade step 3): the selected row's deal priced like the server will charge it
    TradeDealView deal;
    // its strings in the UI language ("service.trade.deal.<key>" without the prefix; resolved by the caller)
    std::map<std::string, std::string> dealText;
    // the CONTRACTS tab's strings ("service.trade.contracts.<key>" without the prefix)
    std::map<std::string, std::string> contractText;
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
