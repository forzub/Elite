#include "src/game/presentation/TradePresentation.h"

#include <algorithm>
#include <cmath>

namespace game::presentation
{
TradePresentation buildTradePresentation(
    const std::string& hubId,
    bool atHub,
    double universeSeconds,
    int tab,
    const std::string& selectedHub,
    const std::function<std::string(const std::string&)>& distanceText
)
{
    using game::economy::EconomyModel;
    TradePresentation page;
    const auto& model = EconomyModel::instance();
    const auto* here = model.hub(hubId);
    if (!model.loaded() || !here)
        return page;

    page.valid = true;
    page.hubId = hubId;
    page.hubName = here->name;
    page.atHub = atHub;
    page.day = EconomyModel::dayOf(universeSeconds);
    page.tab = std::clamp(tab, 0, 2);
    page.market = model.market(hubId, page.day);
    page.events = model.events(hubId, page.day);
    page.hubs = model.hubsOfSystem(here->systemId);

    for (auto& entry : model.board(hubId, page.day))
    {
        TradeBoardRow row;
        if (const auto* dest = model.hub(entry.destinationHub))
        {
            row.destinationName = dest->name;
            row.destinationSystem = dest->systemName;
        }
        if (entry.good >= 0 && entry.good < static_cast<int>(model.goods().size()))
            row.goodRu = model.goods()[static_cast<std::size_t>(entry.good)].ru;
        row.daysLeft = entry.deadlineDay - page.day;
        row.entry = std::move(entry);
        page.board.push_back(std::move(row));
    }

    // the card: the selected hub, else the first board destination
    page.selectedHub = selectedHub;
    if (page.selectedHub.empty() && !page.board.empty())
        page.selectedHub = page.board.front().entry.destinationHub;
    if (const auto* dest = model.hub(page.selectedHub))
    {
        auto& card = page.card;
        card.valid = true;
        card.info = *dest;
        card.distanceText = distanceText ? distanceText(dest->id) : std::string();
        card.channel = model.channel(hubId, dest->id);
        const double delayH = dest->id == hubId ? 0.0 : model.infoDelayHours(dest->id, hubId);
        card.connected = delayH >= 0.0;
        if (card.connected)
        {
            // bulletins leave every 24 h: the newest one that already arrived
            const double nowH = universeSeconds / 3600.0;
            const double publishedH = std::floor((nowH - delayH) / 24.0) * 24.0;
            card.infoAgeHours = std::max(0.0, nowH - publishedH);
            const int knownDay = EconomyModel::dayOf(publishedH * 3600.0);
            for (const auto& q : model.market(dest->id, knownDay))
                if (q.traded)
                    card.knownPrices.push_back(q);
            card.knownEvents = model.events(dest->id, knownDay);
        }
    }
    return page;
}
} // namespace game::presentation
