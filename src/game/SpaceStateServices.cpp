// SpaceState: F6 Shipyard and F7 Repair & Refit native pages. Page models
// come from the ship catalog / runtime descriptors (F6) and from the player's
// replicated ship instances + the server's Repair & Refit previews (F7).

#include "src/game/SpaceStateInternal.h"

#include <algorithm>

#include <GLFW/glfw3.h>

#include "src/input/Input.h"
#include "src/game/client/GameClient.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/navigation/rules/NavDataBanks.h"

game::presentation::RepairRefitPresentation SpaceState::buildRepairRefitPage()
{
    std::vector<game::presentation::RefitShipInput> owned;
    if (!m_client)
        return {};
    const auto& ships = m_client->world().ships();
    const auto controlled = m_client->localControlledEntityId();
    std::uint32_t owner = 0;
    if (const auto it = ships.find(controlled.value); it != ships.end() && it->second.refit.owned)
        owner = it->second.refit.ownerActor;
    // the controlled ship first, then every other ship of the same owner
    const auto add = [&](const ClientShipState& s) {
        owned.push_back({s.id, s.descriptor, &s.modules, &s.refit});
    };
    if (const auto it = ships.find(controlled.value); it != ships.end() && it->second.descriptor)
        add(it->second);
    std::vector<const ClientShipState*> others;
    for (const auto& [id, s] : ships)
        if (id != controlled.value && s.descriptor && s.refit.owned && owner != 0 && s.refit.ownerActor == owner)
            others.push_back(&s);
    std::sort(others.begin(), others.end(), [](const auto* a, const auto* b) { return a->id.value < b->id.value; });
    for (const auto* s : others)
        add(*s);
    if (m_refitShip.value == 0)
        m_refitShip = controlled;
    return game::presentation::buildRepairRefitPresentation(owned, m_refitShip, m_refitGroup, m_refitModule, m_refitSeenApplySerial);
}

bool SpaceState::renderServicePage(const Viewport& viewport, ui::services::ServiceUiId service)
{
    if (service == ui::services::ServiceUiId::Shipyard)
    {
        const auto page = game::presentation::buildShipyardPresentation(m_shipyardModel, m_shipyardPassport);
        if (m_shipyardModel.empty() && page.selected >= 0)
            m_shipyardModel = page.models[static_cast<std::size_t>(page.selected)].passportId;
        m_servicePanels.renderShipyard(viewport, page);
        return true;
    }
    if (service == ui::services::ServiceUiId::RepairRefit && m_client)
    {
        m_servicePanels.renderRepairRefit(viewport, buildRepairRefitPage());
        return true;
    }
    if (service == ui::services::ServiceUiId::Trade && m_client)
    {
        m_servicePanels.renderTrade(viewport, buildTradePage());
        return true;
    }
    return false;
}

game::presentation::TradePresentation SpaceState::buildTradePage()
{
    // the hub the player is in (hub frame), else the nearest replicated hub
    std::string hubId;
    bool atHub = false;
    glm::dvec3 playerWorld(0.0);
    int playerSystem = -1;
    if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
    {
        const auto& tr = it->second.transform;
        playerWorld = world::coordinates::fullMeters(tr.worldPosition);
        playerSystem = tr.motion.systemId;
        if (!tr.motion.hubId.empty() && !tr.motion.freeFlight)
        {
            hubId = tr.motion.hubId;
            atHub = true;
        }
    }
    if (hubId.empty())
    {
        double best = 1e300;
        for (const auto& [id, hub] : m_client->world().hubs())
        {
            const double d = glm::length(world::coordinates::fullMeters(hub.worldPosition) - playerWorld);
            if (d < best)
            {
                best = d;
                hubId = id;
            }
        }
    }

    const auto distanceText = [&](const std::string& targetId) -> std::string {
        const auto& hubs = m_client->world().hubs();
        if (const auto it = hubs.find(targetId); it != hubs.end())
        {
            const double km = glm::length(world::coordinates::fullMeters(it->second.worldPosition) - playerWorld) / 1000.0;
            char b[48];
            std::snprintf(b, sizeof b, km < 100000.0 ? "%.0f km" : "%.2f млн км", km < 100000.0 ? km : km / 1.0e6);
            return b;
        }
        const auto* target = game::economy::EconomyModel::instance().hub(targetId);
        if (!target)
            return {};
        if (target->systemId == playerSystem)
            return "в этой системе";
        if (const auto* atlas = m_client->starAtlas())
        {
            const auto* a = atlas->findSystemSummary(playerSystem);
            const auto* b = atlas->findSystemSummary(target->systemId);
            if (a && b)
            {
                char buf[48];
                std::snprintf(buf, sizeof buf, "%.2f св. лет", glm::length(glm::dvec3(b->positionLy) - glm::dvec3(a->positionLy)));
                return buf;
            }
        }
        return {};
    };
    auto page = game::presentation::buildTradePresentation(hubId, atHub, m_client->universeTimeSeconds(), m_tradeTab, m_tradeHub,
                                                           distanceText);
    // the finance tab: the server's ledger for this session (docs/ECONOMY_BANKING.md)
    page.finance = m_client->sessionSnapshot().wallet;
    // the market and the cargo: the server's (docs/TRADE_DESIGN.md §7.5) - one price of record, what is shown is what
    // a Buy / Sell is charged at; the market of the hub the ship is at, none elsewhere
    page.trade = m_client->sessionSnapshot().trade;
    if (page.trade.valid && page.trade.hubId == page.hubId)
        for (const auto& row : page.trade.market)
        {
            game::economy::MarketQuote q;
            q.good = row.good;
            q.traded = row.traded;
            q.buyCr = row.askCr;
            q.sellCr = row.bidCr;
            q.stockT = row.stockT;
            q.drift = row.drift;
            page.market.push_back(q);
            page.marketLimits.push_back(row);
        }
    if (page.card.valid && page.card.info.id == page.hubId)
        page.card.knownPrices = page.market;
    // the MARKET deal window (trade step 3): priced with the server's TradeQuoteModel on the replicated inputs
    if (page.tab == 1)
    {
        if (m_tradeDeal.good >= 0 && page.trade.hubId == page.hubId)
            page.deal = game::presentation::buildTradeDeal(page.trade, m_tradeDeal);
        if (page.deal.sentRequest != 0 && page.deal.answer != game::presentation::TradeDealAnswer::Waiting &&
            m_tradeDealAnswerLogged != page.deal.sentRequest)
        {
            m_tradeDealAnswerLogged = page.deal.sentRequest;
            logTradeDeal("answer");
        }
        static const char* const DealKeys[] = {
            "hint", "buy", "sell", "units_t", "units_c20", "aboard", "quantity", "limit", "why_hold_c20", "why_payload",
            "why_hub_buy", "why_hub_sell", "why_money", "why_aboard", "list_ask", "list_bid", "impact", "goods_buy", "goods_sell",
            "handling_buy", "handling_sell", "vat_buy", "vat_sell", "turnover", "total_buy", "total_sell", "vat_refund",
            "confirm_buy", "confirm_sell", "cancel", "max", "waiting", "ok_buy", "ok_sell", "refused", "price_moved", "retry",
            "not_docked", "zero", "money", "keys", "account", "unit_price", "boxes"};
        for (const char* key : DealKeys)
            page.dealText[key] = localizedUiText(context().app, std::string("service.trade.deal.") + key, key);
    }
    // the navigation data tab (JUMP_MECHANICS 10l)
    namespace jr = game::navigation::jump;
    auto& nav = page.nav;
    nav.atHub = atHub;
    nav.selected = m_navDataSelected;
    const auto* atlas = m_client->starAtlas();
    const double universeS = m_client->lastSimulationMetadata().universeTimeSeconds;
    const auto describe = [&](const jr::LaneRecord& r, game::presentation::NavDataRow& row) {
        row.ageYears = (universeS - r.surveyUniverseS) / (365.25 * 86400.0);
        if (const auto* shell = atlas ? jr::approachShellOf(*atlas, r.systemId) : nullptr)
        {
            row.place = shell->name;
            if (r.corridor >= 1 && r.corridor <= static_cast<int>(shell->corridors.size()))
                row.corridor = shell->corridors[static_cast<std::size_t>(r.corridor - 1)].name;
            row.stateText = jr::laneStateText(jr::laneState(*shell, r, universeS));
        }
    };
    if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
    {
        const auto& motion = it->second.transform.motion;
        nav.computerState = motion.navComputerState;
        for (const auto& r : jr::parseLaneRecords(motion.laneRecords))
        {
            game::presentation::NavDataRow row;
            describe(r, row);
            row.tierText = r.source;
            nav.memory.push_back(row);
        }
        for (const auto& c : jr::parseDataCarriers(motion.dataCarriers))
            nav.carriers.push_back(c.label + " (" + std::to_string(c.records.size()) + " зап.)");
        if (const auto* bank = jr::navDataBankInSystem(playerSystem); bank && atlas)
        {
            nav.bankName = bank->name;
            for (const auto& r : bank->records)
            {
                game::presentation::NavDataRow row;
                describe(jr::laneRecordOf(*atlas, *bank, r), row);
                row.id = r.id;
                row.tierText = jr::dataTierText(r.tier);
                row.open = r.tier == jr::DataTier::Public;
                row.surveyYear = r.surveyYear;
                row.priceCr = r.priceCr;
                nav.bank.push_back(row);
            }
        }
    }
    return page;
}

void SpaceState::sendRefitCommand(bool apply, const game::ship::refit::RefitOperation& operation)
{
    if (!m_client)
        return;
    ClientShipCommand command;
    command.type = apply ? ClientShipCommand::RefitApply : ClientShipCommand::RefitPreview;
    command.index = static_cast<int>(operation.kind);
    command.target = operation.moduleId;
    command.detail = operation.productId;
    const auto controlled = m_client->localControlledEntityId();
    command.amount = m_refitShip.value == controlled.value ? 0.0 : static_cast<double>(m_refitShip.value);
    game::network::ClientMessage msg;
    msg.payload = command;
    m_client->sendMessage(msg);
    m_refitLastOperation = operation;
}

std::uint32_t SpaceState::sendEconomyCommand(int op, const std::string& goodId, double tonnes, double limitUnitCr)
{
    if (!m_client)
        return 0;
    game::network::ClientEconomyCommand command;
    command.op = static_cast<game::network::ClientEconomyCommand::Op>(
        std::clamp(op, 0, static_cast<int>(game::network::ClientEconomyCommand::Last)));
    command.requestId = ++m_economyRequestId;
    command.shipEntityId = m_client->localControlledEntityId().value;
    command.goodId = goodId;
    command.quantity = tonnes;
    const auto& trade = m_client->sessionSnapshot().trade;
    if (trade.valid && trade.docked)
        command.hubId = trade.hubId;
    // the price and the day the player SAW (re-audit 2026-11-19: neither was sent, the server's "price moved" / "the
    // day changed" guards never fired). No explicit limit: the shown ask (buy) / bid (sell) with a 2 % tolerance for
    // the deal's own impact; the deal window (trade step 3) sends its exact quoted unit price instead.
    command.quoteDay = trade.valid ? trade.day : -1;
    command.limitUnitCr = limitUnitCr;
    if (!(limitUnitCr > 0.0) && trade.valid)
    {
        int good = -1;
        const auto& goods = game::economy::EconomyModel::instance().goods();
        for (std::size_t k = 0; k < goods.size(); ++k)
            if (goods[k].id == goodId)
                good = static_cast<int>(k);
        for (const auto& row : trade.market)
            if (row.good == good && row.traded)
            {
                if (command.op == game::network::ClientEconomyCommand::Buy && row.askCr > 0.0)
                    command.limitUnitCr = row.askCr * 1.02;
                else if (command.op == game::network::ClientEconomyCommand::Sell && row.bidCr > 0.0)
                    command.limitUnitCr = row.bidCr * 0.98;
            }
    }
    game::network::ClientMessage msg;
    msg.payload = command;
    m_client->sendMessage(msg);
    std::cerr << "[Trade] client: request " << command.requestId << " op " << op << " " << goodId << " " << tonnes << " t" << std::endl;
    return command.requestId;
}

bool SpaceState::handleServicePageInput(const Viewport& viewport, ui::services::ServiceUiId service)
{
    using ui::presentation::ServicePanelActionType;
    if (service != ui::services::ServiceUiId::Shipyard && service != ui::services::ServiceUiId::RepairRefit &&
        service != ui::services::ServiceUiId::Trade)
        return false;
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return false;
    double mx = 0.0, my = 0.0;
    glfwGetCursorPos(window, &mx, &my);
    const bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    const double scrollY = Input::instance().consumeScrollY();
    // F8 keys (TRADE_DESIGN §8): Input is reset while a service page owns the screen - read the keys directly
    if (service == ui::services::ServiceUiId::Trade)
    {
        const bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        static const int Keys[] = {GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT, GLFW_KEY_TAB, GLFW_KEY_ENTER,
                                   GLFW_KEY_KP_ENTER, GLFW_KEY_BACKSPACE, GLFW_KEY_R, GLFW_KEY_U, GLFW_KEY_1, GLFW_KEY_2,
                                   GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5, GLFW_KEY_6};
        for (const int key : Keys)
        {
            const bool down = glfwGetKey(window, key) == GLFW_PRESS;
            bool& was = m_tradeKeyWasDown[key];
            if (down && !was)
                tradeDealKey(key, shift);
            was = down;
        }
    }
    const auto action = m_servicePanels.handleInput(viewport, mx, my, leftDown, scrollY);
    if (!action)
        return true;

    if (service == ui::services::ServiceUiId::Trade)
    {
        if (action->type >= ServicePanelActionType::TradeSelectGood)
            applyTradeDealAction(*action);
        else if (action->type == ServicePanelActionType::TradeTab)
            m_tradeTab = std::atoi(action->text.c_str());
        else if (action->type == ServicePanelActionType::TradeSelectHub)
            m_tradeHub = action->text;
        else if (action->type == ServicePanelActionType::NavDataSelect)
            m_navDataSelected = action->text;
        else if (action->type == ServicePanelActionType::NavDataLoad || action->type == ServicePanelActionType::NavDataCarrier ||
                 action->type == ServicePanelActionType::NavDataInsert)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::NavData;
            command.index = action->type == ServicePanelActionType::NavDataLoad ? 0 : action->type == ServicePanelActionType::NavDataCarrier ? 1 : 2;
            if (command.index == 2)
                command.amount = std::atof(action->text.c_str());
            else
                command.detail = action->text;
            msg.payload = command;
            m_client->sendMessage(msg);
        }
        else if (action->type == ServicePanelActionType::TradeShowOnMap && context().app)
        {
            // the System map focused on that hub (the same path as a list pick)
            m_tradeHub = action->text;
            m_systemMapRenderer.devFocusHub(action->text);
            context().app->requestNavigationView(NavigationPresentationView::System);
        }
        return true;
    }

    if (service == ui::services::ServiceUiId::Shipyard)
    {
        if (action->type == ServicePanelActionType::ShipyardSelectModel)
            m_shipyardModel = action->text;
        else if (action->type == ServicePanelActionType::ShipyardTogglePassport)
            m_shipyardPassport = !m_shipyardPassport;
        return true;
    }

    const auto page = buildRepairRefitPage();
    const auto selectedId = [&]() -> std::string {
        return page.selectedModule >= 0 ? page.modules[static_cast<std::size_t>(page.selectedModule)].moduleId : std::string();
    };
    switch (action->type)
    {
        case ServicePanelActionType::RefitPreviousShip:
        case ServicePanelActionType::RefitNextShip:
            if (!page.ships.empty())
            {
                const int n = static_cast<int>(page.ships.size());
                const int step = action->type == ServicePanelActionType::RefitNextShip ? 1 : n - 1;
                m_refitShip = page.ships[static_cast<std::size_t>((page.currentShip + step) % n)].id;
                m_refitGroup.clear();
                m_refitModule.clear();
            }
            break;
        case ServicePanelActionType::RefitSelectGroup:
            m_refitGroup = m_refitGroup == action->text ? std::string() : action->text;
            m_refitModule.clear();
            break;
        case ServicePanelActionType::RefitSelectModule:
            m_refitModule = action->text;
            // a damaged module: ask the server at once what repairing it changes
            for (const auto& m : page.modules)
                if (m.moduleId == action->text && m.damaged())
                    sendRefitCommand(false, {game::ship::refit::RefitOperationKind::Repair, m.moduleId, {}});
            break;
        case ServicePanelActionType::RefitPreviewRepair:
            if (!selectedId().empty())
                sendRefitCommand(false, {game::ship::refit::RefitOperationKind::Repair, selectedId(), {}});
            break;
        case ServicePanelActionType::RefitPreviewReplace:
            if (!selectedId().empty())
                sendRefitCommand(false, {game::ship::refit::RefitOperationKind::Replace, selectedId(), action->text});
            break;
        case ServicePanelActionType::RefitApply:
            if (page.previewForSelection && page.preview.ok && page.preview.applicable)
            {
                // remember the apply serial seen so far: the next one is this click's result
                const auto subject = m_refitShip.value ? m_refitShip.value : m_client->localControlledEntityId().value;
                if (const auto it = m_client->world().ships().find(subject); it != m_client->world().ships().end())
                    m_refitSeenApplySerial = it->second.refit.applySerial;
                sendRefitCommand(true, page.preview.operation);
            }
            break;
        default:
            break;
    }
    return true;
}

// ---------------------------------------------------------------- F8 MARKET deal window (TRADE_DESIGN §8.1, step 3)
// The window only shows and requests: the preview is TradeQuoteModel on the server's replicated inputs; Confirm sends
// Buy / Sell with the EXACT quoted unit price as the limit and the snapshot day, the server re-prices and charges.
void SpaceState::applyTradeDealAction(const ui::presentation::ServicePanelAction& action)
{
    using ui::presentation::ServicePanelActionType;
    if (!m_client)
        return;
    const auto view = [&]() { return game::presentation::buildTradeDeal(m_client->sessionSnapshot().trade, m_tradeDeal); };
    switch (action.type)
    {
        case ServicePanelActionType::TradeSelectGood:
        {
            const int good = std::atoi(action.text.c_str());
            if (good != m_tradeDeal.good)
            {
                m_tradeDeal.good = good;
                m_tradeDeal.tonnes = -1.0;
                m_tradeDeal.sentRequest = 0;
            }
            m_tradeTab = 1;
            break;
        }
        case ServicePanelActionType::TradeDealSide:
            if (m_tradeDeal.buy != (action.text == "buy"))
            {
                m_tradeDeal.buy = action.text == "buy";
                m_tradeDeal.tonnes = -1.0;
                m_tradeDeal.sentRequest = 0;
            }
            break;
        case ServicePanelActionType::TradeDealUnits:
            m_tradeDeal.unitsC20 = action.text == "c20";
            break;
        case ServicePanelActionType::TradeDealStep:
            m_tradeDeal.tonnes = game::presentation::tradeDealStep(view(), std::atoi(action.text.c_str()));
            break;
        case ServicePanelActionType::TradeDealSlider:
            m_tradeDeal.tonnes = game::presentation::tradeDealAtFraction(view(), std::atof(action.text.c_str()));
            break;
        case ServicePanelActionType::TradeDealMax:
            m_tradeDeal.tonnes = view().maxT;
            break;
        case ServicePanelActionType::TradeDealConfirm:
        case ServicePanelActionType::TradeDealRetry:
            confirmTradeDeal();
            return;
        case ServicePanelActionType::TradeDealCancel:
            // waiting: forget the request (its result still lands in the list); else close the window
            if (m_tradeDeal.sentRequest != 0)
                m_tradeDeal.sentRequest = 0;
            else
                m_tradeDeal.good = -1;
            break;
        default:
            return;
    }
    logTradeDeal("action");
}

void SpaceState::tradeDealKey(int key, bool shift)
{
    using ui::presentation::ServicePanelAction;
    using ui::presentation::ServicePanelActionType;
    if (!m_client)
        return;
    if (key >= GLFW_KEY_1 && key <= GLFW_KEY_6)
    {
        m_tradeTab = key - GLFW_KEY_1;
        return;
    }
    if (m_tradeTab != 1)
        return;
    const auto& market = m_client->sessionSnapshot().trade.market;
    switch (key)
    {
        case GLFW_KEY_UP:
        case GLFW_KEY_DOWN:
        {
            if (market.empty())
                return;
            int at = -1;
            for (std::size_t i = 0; i < market.size(); ++i)
                if (market[i].good == m_tradeDeal.good)
                    at = static_cast<int>(i);
            const int n = static_cast<int>(market.size());
            at = at < 0 ? (key == GLFW_KEY_DOWN ? 0 : n - 1) : (at + (key == GLFW_KEY_DOWN ? 1 : n - 1)) % n;
            applyTradeDealAction({ServicePanelActionType::TradeSelectGood, std::to_string(market[static_cast<std::size_t>(at)].good)});
            return;
        }
        case GLFW_KEY_LEFT:
        case GLFW_KEY_RIGHT:
            applyTradeDealAction({ServicePanelActionType::TradeDealStep, std::to_string((key == GLFW_KEY_RIGHT ? 1 : -1) * (shift ? 10 : 1))});
            return;
        case GLFW_KEY_TAB:
            applyTradeDealAction({ServicePanelActionType::TradeDealSide, m_tradeDeal.buy ? "sell" : "buy"});
            return;
        case GLFW_KEY_U:
            applyTradeDealAction({ServicePanelActionType::TradeDealUnits, m_tradeDeal.unitsC20 ? "t" : "c20"});
            return;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
        case GLFW_KEY_R:
            applyTradeDealAction({ServicePanelActionType::TradeDealConfirm, {}});
            return;
        case GLFW_KEY_BACKSPACE:
            applyTradeDealAction({ServicePanelActionType::TradeDealCancel, {}});
            return;
        default:
            return;
    }
}

void SpaceState::confirmTradeDeal()
{
    if (!m_client)
        return;
    const auto v = game::presentation::buildTradeDeal(m_client->sessionSnapshot().trade, m_tradeDeal);
    if (!v.valid || !v.canConfirm)
    {
        std::cerr << "[TradeDeal] confirm ignored: " << (v.valid ? v.blockKind : std::string("no row selected")) << std::endl;
        return;
    }
    const std::uint32_t id = sendEconomyCommand(v.buy ? game::network::ClientEconomyCommand::Buy : game::network::ClientEconomyCommand::Sell,
                                                v.goodId, v.tonnes, v.quote.unitCr);
    m_tradeDeal.tonnes = v.tonnes;
    m_tradeDeal.sentRequest = id;
    m_tradeDeal.sentUnitCr = v.quote.unitCr;
    m_tradeDeal.sentTonnes = v.tonnes;
    m_tradeDeal.sentBuy = v.buy;
    logTradeDeal("confirm");
}

void SpaceState::logTradeDeal(const char* why)
{
    if (!m_client)
        return;
    const auto& snap = m_client->sessionSnapshot().trade;
    const auto v = game::presentation::buildTradeDeal(snap, m_tradeDeal);
    if (!v.valid)
    {
        std::cerr << "[TradeDeal] " << why << ": no row selected (tab " << m_tradeTab << ")" << std::endl;
        return;
    }
    std::ostringstream o;
    o.precision(10);
    o << "[TradeDeal] " << why << ": " << (v.buy ? "BUY " : "SELL ") << v.goodId << " " << v.tonnes << " t (" << v.c20 << " C20, "
      << (v.unitsC20 ? "C20 units" : "t units") << "), max " << v.maxT << " t by " << v.limitKind << " [hold " << v.holdLimitT << ", hub "
      << v.hubLimitT << ", money " << v.moneyLimitT << ", aboard " << v.aboardT << "], day " << snap.day << ", list " << v.quote.listUnitCr
      << " -> deal " << v.quote.unitCr << " Cr/t; lines";
    for (const auto& l : v.lines)
        o << " " << l.kind << " " << l.cr << " (" << l.shownCr << ")";
    o << "; total " << v.quote.totalCr << " (" << v.shownTotalCr << "); spendable " << snap.spendableCr << "; "
      << (v.canConfirm ? "can confirm" : "blocked: " + v.blockKind);
    if (v.sentRequest != 0)
    {
        o << "; request #" << v.sentRequest << " ";
        using game::presentation::TradeDealAnswer;
        o << (v.answer == TradeDealAnswer::Waiting ? "waiting"
              : v.answer == TradeDealAnswer::Ok     ? "OK"
              : v.answer == TradeDealAnswer::PriceMoved ? "PRICE MOVED"
                                                        : "REFUSED");
        if (v.answer != TradeDealAnswer::Waiting)
            o << " (" << v.result.code << ") " << v.result.message << " total " << v.result.totalCr;
    }
    std::cerr << o.str() << std::endl;
}
