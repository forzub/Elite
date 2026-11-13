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
    const auto action = m_servicePanels.handleInput(viewport, mx, my, leftDown, scrollY);
    if (!action)
        return true;

    if (service == ui::services::ServiceUiId::Trade)
    {
        if (action->type == ServicePanelActionType::TradeTab)
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
