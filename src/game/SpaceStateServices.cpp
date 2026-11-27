// SpaceState: F5 Government services, F6 Shipyard, F7 Repair & Refit and F8 Trade native pages. Page models
// come from the ship catalog / runtime descriptors (F6) and from the player's
// replicated ship instances + the server's Repair & Refit previews (F7).

#include "src/core/BuildFlavor.h"
#include "src/game/SpaceStateInternal.h"
#include "src/game/localization/LocalizationService.h"

#include <algorithm>

#include <GLFW/glfw3.h>

#include "src/input/Input.h"
#include "src/game/client/GameClient.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/navigation/rules/NavDataBanks.h"

namespace l10n = game::localization;

namespace
{
// the nav-data rows' words in the UI language (the rules' own texts stay for the server's logs)
std::string laneStateName(game::navigation::jump::LaneState s)
{
    namespace jr = game::navigation::jump;
    switch (s)
    {
    case jr::LaneState::Known: return l10n::tr("cockpit.jump.lane_state.known", "known");
    case jr::LaneState::Estimate: return l10n::tr("cockpit.jump.lane_state.estimate", "estimate");
    case jr::LaneState::Outdated: return l10n::tr("cockpit.jump.lane_state.outdated", "outdated");
    default: return l10n::tr("cockpit.jump.lane_state.unknown", "unknown");
    }
}
std::string dataTierName(game::navigation::jump::DataTier t)
{
    namespace jr = game::navigation::jump;
    switch (t)
    {
    case jr::DataTier::Military: return l10n::tr("cockpit.navdata.tier.military", "military clearance");
    case jr::DataTier::Secret: return l10n::tr("cockpit.navdata.tier.secret", "secret");
    default: return l10n::tr("cockpit.navdata.tier.public", "public");
    }
}
} // namespace

game::presentation::RepairRefitPresentation SpaceState::buildRepairRefitPage()
{
    std::vector<game::presentation::RefitShipInput> owned;
    if (!m_client)
        return {};
    const auto& ships = m_client->world().ships();
    const auto controlled = m_client->localControlledEntityId();
    // the controlled ship first, then every other ship of THIS player's fleet: refit.owned is the server's "owned by
    // the receiving player" (ShipOwnershipRegistry) - the client compares no owner ids
    const auto add = [&](const ClientShipState& s) {
        owned.push_back({s.id, s.descriptor, &s.modules, &s.refit});
    };
    // (the controlled ship was listed unconditionally: a hire's employer ship - flown, not owned - showed in F7 while
    // the navigation fleet, the same ownership, did not list it; live 2026-11-27)
    if (const auto it = ships.find(controlled.value); it != ships.end() && it->second.descriptor && it->second.refit.owned)
        add(it->second);
    std::vector<const ClientShipState*> others;
    for (const auto& [id, s] : ships)
        if (id != controlled.value && s.descriptor && s.refit.owned)
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
    if (service == ui::services::ServiceUiId::GovernmentServices)
    {
        m_servicePanels.renderGovernment(viewport, buildGovernmentPage());
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
            return km < 100000.0 ? l10n::tr("cockpit.unit.distance_km", {{"0", l10n::formatNumber(km, 0)}}, "{0} km")
                                 : l10n::tr("cockpit.unit.distance_million_km", {{"0", l10n::formatNumber(km / 1.0e6, 2)}}, "{0}M km");
        }
        const auto* target = game::economy::EconomyModel::instance().hub(targetId);
        if (!target)
            return {};
        if (target->systemId == playerSystem)
            return l10n::tr("cockpit.services.in_this_system", "in this system");
        if (const auto* atlas = m_client->starAtlas())
        {
            const auto* a = atlas->findSystemSummary(playerSystem);
            const auto* b = atlas->findSystemSummary(target->systemId);
            if (a && b)
            {
                return l10n::tr("cockpit.unit.distance_ly",
                                {{"0", l10n::formatNumber(glm::length(glm::dvec3(b->positionLy) - glm::dvec3(a->positionLy)), 2)}}, "{0} ly");
            }
        }
        return {};
    };
    auto page = game::presentation::buildTradePresentation(hubId, atHub, m_client->universeTimeSeconds(), m_tradeTab, m_tradeHub,
                                                           distanceText);
    // the finance tab: the server's ledger for this session (docs/ECONOMY_BANKING.md)
    page.finance = m_client->sessionSnapshot().wallet;
    // ФЛОТ (step 15): the first owned ship the player does not fly takes a new bot (F7's list: the controlled one first)
    if (m_client)
    {
        const auto& ships = m_client->world().ships();
        std::vector<std::uint32_t> ids;
        for (const auto& [sid, sh] : ships)
            if (sid != m_playerId.value && sh.refit.owned)   // the server's "owned by this player"
                ids.push_back(sid);
        std::sort(ids.begin(), ids.end());
        if (!ids.empty())
        {
            page.botShipEntity = ids.front();
            const auto& sh = ships.at(ids.front());
            page.botShipName = sh.refit.shipName.empty() ? std::to_string(ids.front()) : sh.refit.shipName;
        }
    }
    for (const char* key : {"garnish", "garnish_value"})
        page.financeText[key] = localizedUiText(context().app, std::string("service.trade.finance.") + key, key);
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
    // the CONTRACTS tab (trade step 5): the server's board of this hub and the player's contracts are in page.trade
    page.selectedContract = m_tradeContract;
    page.contractConfirm = !m_tradeContractAsk.empty();
    page.contractRequest = m_tradeContractRequest;
    {
        static const char* const ContractKeys[] = {
            "tab", "board", "mine", "empty_board", "empty_mine", "kind_supply", "kind_crew_shift", "kind_construction", "route",
            "cargo", "people", "reward", "advance", "deposit", "penalty", "deadline", "on_board", "status_open", "status_taken",
            "status_npc", "status_expired", "status_active", "status_delivered", "status_late", "status_failed", "status_abandoned",
            "status_cancelled", "take", "confirm_take", "cancel", "abandon", "confirm_abandon", "issuer", "cause", "waiting",
            "aboard", "not_aboard", "payout", "late", "keys", "take_hint", "days_hours", "hours", "overdue", "select_hint",
            "penalty_paid", "late_rule", "no_deposit_hint", "day", "load", "deadline_label", "board_left", "refused"};
        for (const char* key : ContractKeys)
            page.contractText[key] = localizedUiText(context().app, std::string("service.trade.contracts.") + key, key);
    }
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
            "handling_buy", "handling_sell", "vat_buy", "vat_sell", "duty", "turnover", "total_buy", "total_sell", "vat_refund",
            "confirm_buy", "confirm_sell", "cancel", "max", "waiting", "ok_buy", "ok_sell", "refused", "price_moved", "retry",
            "not_docked", "zero", "money", "keys", "account", "unit_price", "boxes"};
        for (const char* key : DealKeys)
            page.dealText[key] = localizedUiText(context().app, std::string("service.trade.deal.") + key, key);
    }
    return page;
}

game::presentation::GovernmentPresentation SpaceState::buildGovernmentPage()
{
    game::presentation::GovernmentPresentation page;
    page.tab = std::clamp(m_governmentTab, 0, 1);
    page.section = std::clamp(m_governmentSection, 0, 5);
    static const char* const Keys[] = {
        "app", "app_subtitle", "nav_data", "taxes", "tax_no_data", "tax_residence", "tax_style", "tax_rate", "tax_admin_fee",
        "tax_rules", "tax_days", "tax_declare", "tax_open_period", "tax_revenue", "tax_expenses", "tax_profit", "tax_withheld_credit",
        "tax_withheld_lost", "tax_estimate", "tax_last_period", "tax_none_yet", "tax_period", "tax_assessed", "tax_due", "tax_until_day",
        "tax_in", "tax_status", "tax_nothing", "tax_paid", "tax_debt", "tax_penalty", "licences", "lic_pilot", "lic_control", "lic_held",
        "lic_none", "lic_until", "lic_expired", "lic_offers", "lic_dock_hint", "lic_buy", "lic_extend", "lic_covers", "jobs", "job_current",
        "job_none", "job_quit", "job_wage", "job_days", "job_ship", "job_since", "job_vacancies", "job_no_vacancies", "job_dock_hint",
        "job_hire", "job_term", "rescue", "sos_licence_yes", "sos_licence_no", "sos_active", "sos_fly_to", "sos_list", "sos_none", "sos_until",
        "sos_take", "sos_status_taken", "sos_status_done", "sos_status_expired", "galley", "galley_subtitle", "galley_not_enlisted", "galley_unit", "galley_ship",
        "galley_rank", "galley_term", "galley_reports", "galley_report_transfer_ship", "galley_report_transfer_unit",
        "galley_report_leave", "galley_report_discharge", "galley_hint", "build", "build_mine", "build_none", "build_stage", "build_paid",
        "build_day", "build_offers", "build_no_offers", "build_days", "build_reliability", "build_sign", "build_dock_hint", "mine_mine", "mine_reserve",
        "mine_today", "mine_sold", "mine_revenue", "mine_rent", "mine_year", "mine_offers", "mine_grade", "mine_lease_cost", "mine_lease", "mine_stock", "mine_selling", "mine_keeping", "mine_keep", "mine_sell", "mine_load"};
    for (const char* key : Keys)
        page.text[key] = localizedUiText(context().app, std::string("service.government.") + key, key);
    if (!m_client)
        return page;
    page.wallet = m_client->sessionSnapshot().wallet;
    page.docked = m_client->sessionSnapshot().trade.valid && m_client->sessionSnapshot().trade.docked;
    // the navigation data (JUMP_MECHANICS 10l; moved from F8, TRADE_DESIGN §11a)
    bool atHub = false;
    int playerSystem = -1;
    if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
    {
        const auto& motion = it->second.transform.motion;
        playerSystem = motion.systemId;
        atHub = !motion.hubId.empty() && !motion.freeFlight;
    }
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
            row.stateText = laneStateName(jr::laneState(*shell, r, universeS));
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
            nav.carriers.push_back(l10n::tr("cockpit.navdata.carrier", {{"label", c.label}, {"count", std::to_string(c.records.size())}},
                                            "{label} ({count} rec.)"));
        if (const auto* bank = jr::navDataBankInSystem(playerSystem); bank && atlas)
        {
            nav.bankName = bank->name;
            for (const auto& r : bank->records)
            {
                game::presentation::NavDataRow row;
                describe(jr::laneRecordOf(*atlas, *bank, r), row);
                row.id = r.id;
                row.tierText = dataTierName(r.tier);
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
    if (apply && core::isPreviewBuild())
        return;   // HantedPreview: the DEV apply (free of charge) is unavailable
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
    // audit ECO-4: the same command twice within 0.6 s is a double click (the cargo insurance F8 and the government
    // buttons had no guard: INSURE charged the premium twice) - not sent again
    {
        const std::string key = std::to_string(op) + "|" + goodId + "|" + std::to_string(tonnes) + "|" + std::to_string(limitUnitCr);
        const double now = glfwGetTime();
        if (key == m_lastEconomySendKey && now - m_lastEconomySendAt < 0.6)
        {
            std::cerr << "[Trade] the same request again within 0.6 s (a double click) - not sent" << std::endl;
            return 0;
        }
        m_lastEconomySendKey = key;
        m_lastEconomySendAt = now;
    }
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
        service != ui::services::ServiceUiId::Trade && service != ui::services::ServiceUiId::GovernmentServices)
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
    if (service == ui::services::ServiceUiId::GovernmentServices)
        for (const int key : {GLFW_KEY_1, GLFW_KEY_2})
        {
            const bool down = glfwGetKey(window, key) == GLFW_PRESS;
            bool& was = m_tradeKeyWasDown[key];
            if (down && !was)
                m_governmentTab = key - GLFW_KEY_1;
            was = down;
        }
    const auto action = m_servicePanels.handleInput(viewport, mx, my, leftDown, scrollY);
    if (!action)
        return true;

    if (service == ui::services::ServiceUiId::GovernmentServices)
    {
        if (action->type == ServicePanelActionType::GovernmentTab)
            m_governmentTab = std::atoi(action->text.c_str());
        else if (action->type == ServicePanelActionType::GovernmentSection)
            m_governmentSection = std::atoi(action->text.c_str());
        else if (action->type == ServicePanelActionType::GovernmentMineSale && action->text.size() > 2)   // part 3
            sendEconomyCommand(game::network::ClientEconomyCommand::SetMineSale, action->text.substr(2), action->text[0] == '1' ? 1.0 : 0.0);
        else if (action->type == ServicePanelActionType::GovernmentMineLoad)   // part 3: what fits (the server decides)
            sendEconomyCommand(game::network::ClientEconomyCommand::LoadFromMine, action->text, 0.0);
        else if (action->type == ServicePanelActionType::GovernmentLeaseSite)   // step 16 part 2: the server decides (money, offer)
            sendEconomyCommand(game::network::ClientEconomyCommand::LeaseSite, action->text, 0.0);
        else if (action->type == ServicePanelActionType::GovernmentSignBuild)   // step 16: the server decides (money, offer)
            sendEconomyCommand(game::network::ClientEconomyCommand::SignConstruction, action->text, 0.0);
        else if (action->type == ServicePanelActionType::GovernmentTakeSos)   // step 12: the server decides (licence, call)
            sendEconomyCommand(game::network::ClientEconomyCommand::TakeSos, action->text, 0.0);
        else if ((action->type == ServicePanelActionType::GovernmentHire || action->type == ServicePanelActionType::GovernmentQuit) && m_client)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = action->type == ServicePanelActionType::GovernmentHire ? ClientShipCommand::HireJob : ClientShipCommand::QuitJob;
            command.detail = action->text;
            msg.payload = command;
            m_client->sendMessage(msg);
        }
        else if (action->type == ServicePanelActionType::GovernmentBuyLicence)   // step 11: the server decides (money, berth)
            sendEconomyCommand(game::network::ClientEconomyCommand::BuyLicence, action->text, 0.0);
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
        return true;
    }

    if (service == ui::services::ServiceUiId::Trade)
    {
        if (action->type == ServicePanelActionType::BotHire || action->type == ServicePanelActionType::BotLoop ||
            action->type == ServicePanelActionType::BotFire)   // F8 ФЛОТ (step 15): the server decides
        {
            const auto op = action->type == ServicePanelActionType::BotHire ? game::network::ClientEconomyCommand::HireBot
                            : action->type == ServicePanelActionType::BotLoop ? game::network::ClientEconomyCommand::OrderBotLoop
                                                                              : game::network::ClientEconomyCommand::FireBot;
            const auto page = buildTradePage();
            sendEconomyCommand(op, action->text, action->type == ServicePanelActionType::BotHire ? static_cast<double>(page.botShipEntity) : 0.0);
        }
        else if (action->type == ServicePanelActionType::CargoInsure)   // the own lots aboard (step 13)
            sendEconomyCommand(game::network::ClientEconomyCommand::InsureCargo, std::string(), 0.0);
        else if (action->type == ServicePanelActionType::CargoBlackSell)   // all of it aboard to the black market (step 9)
            sendEconomyCommand(game::network::ClientEconomyCommand::SellBlack, action->text, 0.0);
        else if (action->type >= ServicePanelActionType::TradeContractSelect && action->type <= ServicePanelActionType::TradeContractCancel)
            applyTradeContractAction(*action);
        else if (action->type >= ServicePanelActionType::TradeSelectGood)
            applyTradeDealAction(*action);
        else if (action->type == ServicePanelActionType::TradeTab)
            m_tradeTab = std::atoi(action->text.c_str());
        else if (action->type == ServicePanelActionType::TradeSelectHub)
            m_tradeHub = action->text;
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
        else if (action->type == ServicePanelActionType::ShipyardBuy && m_client)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::BuyShip;
            command.detail = m_shipyardModel;
            command.target = action->text;
            msg.payload = command;
            m_client->sendMessage(msg);
        }
        return true;
    }

    const auto page = buildRepairRefitPage();
    const auto selectedId = [&]() -> std::string {
        return page.selectedModule >= 0 ? page.modules[static_cast<std::size_t>(page.selectedModule)].moduleId : std::string();
    };
    if (action->type == ServicePanelActionType::RefitBoard && m_client)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::BoardShip;
        command.amount = std::atof(action->text.c_str());
        msg.payload = command;
        m_client->sendMessage(msg);
        return true;
    }
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
        m_tradeTab = game::presentation::TradeTabOrder[key - GLFW_KEY_1];   // keys = the shown order (TRADE_DESIGN §11a)
        return;
    }
    if (m_tradeTab == 0)
    {
        const auto& t = m_client->sessionSnapshot().trade;
        std::vector<std::string> ids;
        for (const auto& r : t.board)
            ids.push_back(r.id);
        for (const auto& r : t.contracts)
            ids.push_back(r.id);
        if ((key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) && !ids.empty())
        {
            const auto it = std::find(ids.begin(), ids.end(), m_tradeContract);
            const int n = static_cast<int>(ids.size());
            int at = it == ids.end() ? -1 : static_cast<int>(it - ids.begin());
            at = at < 0 ? (key == GLFW_KEY_DOWN ? 0 : n - 1) : (at + (key == GLFW_KEY_DOWN ? 1 : n - 1)) % n;
            applyTradeContractAction({ServicePanelActionType::TradeContractSelect, ids[static_cast<std::size_t>(at)]});
        }
        else if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && !m_tradeContract.empty())
        {
            if (!m_tradeContractAsk.empty())
                applyTradeContractAction({ServicePanelActionType::TradeContractConfirm, {}});
            else
            {
                bool mine = false;
                for (const auto& r : t.contracts)
                    mine = mine || (r.id == m_tradeContract && r.status == "active");
                applyTradeContractAction({mine ? ServicePanelActionType::TradeContractAbandon : ServicePanelActionType::TradeContractTake,
                                          m_tradeContract});
            }
        }
        else if (key == GLFW_KEY_BACKSPACE)
            applyTradeContractAction({ServicePanelActionType::TradeContractCancel, {}});
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

// ---------------------------------------------------------------- F8 CONTRACTS (TRADE_DESIGN §8.2, trade step 5)
// The tab shows the server's board and contracts; a take / abandon is asked first, then sent - the server decides.
void SpaceState::applyTradeContractAction(const ui::presentation::ServicePanelAction& action)
{
    using ui::presentation::ServicePanelActionType;
    if (!m_client)
        return;
    switch (action.type)
    {
        case ServicePanelActionType::TradeContractSelect:
            if (m_tradeContract != action.text)
                m_tradeContractAsk.clear();
            m_tradeContract = action.text;
            m_tradeTab = 0;
            for (const auto& r : m_client->sessionSnapshot().trade.board)
                if (r.id == action.text)
                    m_tradeHub = r.destinationHub;
            for (const auto& r : m_client->sessionSnapshot().trade.contracts)
                if (r.id == action.text)
                    m_tradeHub = r.destinationHub;
            break;
        case ServicePanelActionType::TradeContractTake:
            m_tradeContract = action.text;
            m_tradeContractAsk = "take";
            break;
        case ServicePanelActionType::TradeContractAbandon:
            m_tradeContract = action.text;
            m_tradeContractAsk = "abandon";
            break;
        case ServicePanelActionType::TradeContractConfirm:
            if (!m_tradeContract.empty() && !m_tradeContractAsk.empty())
            {
                m_tradeContractRequest = sendContractCommand(m_tradeContractAsk == "take" ? game::network::ClientEconomyCommand::TakeContract
                                                                                          : game::network::ClientEconomyCommand::AbandonContract,
                                                             m_tradeContract);
                m_tradeContractAsk.clear();
            }
            break;
        case ServicePanelActionType::TradeContractCancel:
            m_tradeContractAsk.clear();
            break;
        default:
            break;
    }
    logContracts("action");
}

std::uint32_t SpaceState::sendContractCommand(int op, const std::string& contractId)
{
    if (!m_client)
        return 0;
    game::network::ClientEconomyCommand command;
    command.op = static_cast<game::network::ClientEconomyCommand::Op>(
        std::clamp(op, 0, static_cast<int>(game::network::ClientEconomyCommand::Last)));
    command.requestId = ++m_economyRequestId;
    command.shipEntityId = m_client->localControlledEntityId().value;
    command.contractId = contractId;
    const auto& trade = m_client->sessionSnapshot().trade;
    if (trade.valid && trade.docked)
        command.hubId = trade.hubId;
    command.quoteDay = trade.valid ? trade.day : -1;
    game::network::ClientMessage msg;
    msg.payload = command;
    m_client->sendMessage(msg);
    std::cerr << "[Contract] client: request " << command.requestId << " op " << op << " " << contractId << std::endl;
    return command.requestId;
}

// DEV / evidence: the replicated board and contracts with their timers ("[ContractReport]")
void SpaceState::logContracts(const char* why)
{
    if (!m_client)
        return;
    const auto& t = m_client->sessionSnapshot().trade;
    const auto left = [&](double day) {
        char b[48];
        const double d = day - t.dayNow;
        std::snprintf(b, sizeof b, "%+.2f d", d);
        return std::string(b);
    };
    std::cerr << "[ContractReport] " << why << ": day " << t.day << " (" << t.dayNow << "), board of '" << t.boardHubId << "': "
              << t.board.size() << " posting(s), my contracts " << t.contracts.size() << ", selected '" << m_tradeContract << "'"
              << (m_tradeContractAsk.empty() ? std::string() : " ASK " + m_tradeContractAsk) << '\n';
    for (const auto& r : t.board)
        std::cerr << "[ContractReport]   board " << r.id << " [" << r.status << "] " << r.kind << " " << r.originHub << " -> " << r.destinationHub
                  << " (" << r.destinationName << "): " << r.tonnes << " t " << r.goodId << (r.people ? " " + std::to_string(r.people) + " people" : "")
                  << ", C20 " << r.c20 << ", reward " << r.rewardCr << ", advance " << r.advanceCr << ", deposit " << r.depositCr << ", penalty "
                  << r.penaltyCr << ", off the board in " << left(r.expiresDay) << ", deadline day " << r.deadlineDay << " (" << left(r.deadlineDay + 1.0)
                  << ")" << '\n';
    for (const auto& r : t.contracts)
        std::cerr << "[ContractReport]   mine " << r.id << " [" << r.status << "] " << r.originHub << " -> " << r.destinationHub << ": "
                  << r.tonnes << " t " << r.goodId << ", aboard " << r.aboard << ", deadline day " << r.deadlineDay << " (" << left(r.deadlineDay + 1.0)
                  << "), late " << r.lateDays << " d, paid " << r.paidCr << ", penalty " << r.penaltyPaidCr << '\n';
    for (const auto& r : t.results)
        if (r.requestId == m_tradeContractRequest)
            std::cerr << "[ContractReport]   answer #" << r.requestId << " " << (r.ok ? "OK " : "REFUSED ") << r.code << ": " << r.message << '\n';
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
