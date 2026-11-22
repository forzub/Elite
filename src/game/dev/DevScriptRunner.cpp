// DEV scripted acceptance driver of the client (see DevScriptRunner.h).
// Moved verbatim out of SpaceState::update (audit 2026-11-06 rec. 8 / 10):
// the verbs, their order and every log line are unchanged.

#include "src/core/DevFrameCapture.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/game/dev/DevScriptRunner.h"
#include "src/game/ship/refit/ShipMarket.h"
#include "src/game/economy/CargoRegistry.h"
#include "src/game/economy/EconomyModel.h"
#include "src/game/network/ClientEconomyCommand.h"

#include "src/game/SpaceStateInternal.h"
#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace game::dev
{

namespace
{
// DEV data: ASCII aliases of world ids (the environment of a Windows process
// is not UTF-8). Each verb keeps its own table; a name missing from a verb's
// table is passed through unchanged, as before.
using Alias = std::pair<const char*, const char*>;

// body:<name>
constexpr Alias FocusBodyAliases[] = {
    {"earth", "system_0.Sol.Земля"}, {"moon", "system_0.Sol.Земля.Луна"}, {"mars", "system_0.Sol.Марс"},
    {"jupiter", "system_0.Sol.Юпитер"}, {"saturn", "system_0.Sol.Сатурн"}, {"venus", "system_0.Sol.Венера"},
    {"mercury", "system_0.Sol.Меркурий"}, {"belt", "system_0.Sol.asteroid_belt"},
    {"phobos", "system_0.Sol.Марс.Фобос"}, {"deimos", "system_0.Sol.Марс.Деймос"},
    {"io", "system_0.Sol.Юпитер.Ио"}, {"europa", "system_0.Sol.Юпитер.Европа"},
    {"amalthea", "system_0.Sol.Юпитер.Амальтея"}, {"himalia", "system_0.Sol.Юпитер.Гималия"},
    {"proteus", "system_0.Sol.Нептун.Протей"}, {"phoebe", "system_0.Sol.Сатурн.Феба"},
    {"uranus", "system_0.Sol.Уран"}, {"neptune", "system_0.Sol.Нептун"}};
// enter:<name>
constexpr Alias EnterBodyAliases[] = {
    {"earth", "system_0.Sol.Земля"}, {"mars", "system_0.Sol.Марс"}, {"jupiter", "system_0.Sol.Юпитер"},
    {"saturn", "system_0.Sol.Сатурн"}, {"belt", "system_0.Sol.asteroid_belt"}};
// finish: / via: body:<name>
constexpr Alias RouteBodyAliases[] = {
    {"moon", "system_0.Sol.Земля.Луна"}, {"earth", "system_0.Sol.Земля"}, {"mars", "system_0.Sol.Марс"},
    {"mercury", "system_0.Sol.Меркурий"}, {"venus", "system_0.Sol.Венера"}, {"jupiter", "system_0.Sol.Юпитер"},
    {"saturn", "system_0.Sol.Сатурн"}, {"uranus", "system_0.Sol.Уран"}, {"neptune", "system_0.Sol.Нептун"}};
// leg:orbit:<name>:<alt km> (prefix replacement)
constexpr Alias LegOrbitPrefixAliases[] = {
    {"orbit:moon:", "orbit:system_0.Sol.Земля.Луна:"}, {"orbit:earth:", "orbit:system_0.Sol.Земля:"},
    {"orbit:mars:", "orbit:system_0.Sol.Марс:"}};
// autopilot:<name>[@...]
constexpr Alias AutopilotBodyAliases[] = {
    {"moon", "system_0.Sol.Земля.Луна"}, {"earth", "system_0.Sol.Земля"}, {"mars", "system_0.Sol.Марс"},
    {"here", ""}};
// report: the hubs the player's relative speed / distance is printed against
constexpr const char* ReportHubIds[] = {"earth_orbital_hub", "earth_hub.tikhiy", "site.000.sol.ceres"};
// ELITE_DEV_FF_TRACE_SECONDS: the hub the rendered position is traced against
constexpr const char* FreeFlightTraceHubId = "earth_orbital_hub";
// ELITE_DEV_DOCK_REQUEST_AT: the dock gate requested
constexpr const char* DevDockModuleId = "hub_hex_dock_a";
constexpr const char* DevDockAnchorId = "dock_gate_front";

double envSeconds(const char* name, double fallback)
{
    const char* v = std::getenv(name);
    return v && *v ? std::atof(v) : fallback;
}
} // namespace

DevScriptRunner& DevScriptRunner::instance()
{
    static DevScriptRunner runner;
    return runner;
}

DevScriptRunner::DevScriptRunner()
    : m_config(readConfig())
{
}

DevScriptRunner::Config DevScriptRunner::readConfig()
{
    Config c;
    c.galaxyMapAtSeconds = envSeconds("ELITE_DEV_GALAXY_MAP_AT", -1.0);
    c.systemMapAtSeconds = envSeconds("ELITE_DEV_SYSTEM_MAP_AT", -1.0);
    {
        const char* v = std::getenv("ELITE_DEV_SOL_MAP_SCRIPT");
        std::string s = v ? v : "";
        std::size_t p = 0;
        while (p < s.size())
        {
            const std::size_t e = s.find(';', p);
            const std::string item = s.substr(p, e == std::string::npos ? std::string::npos : e - p);
            const std::size_t colon = item.find(':');
            if (colon != std::string::npos)
                c.solMapScript.push_back({std::atof(item.substr(0, colon).c_str()), item.substr(colon + 1)});
            if (e == std::string::npos)
                break;
            p = e + 1;
        }
    }
    c.serviceScriptAtSeconds = envSeconds("ELITE_DEV_SERVICE_SCRIPT", -1.0);
    c.dockRequestAtSeconds = envSeconds("ELITE_DEV_DOCK_REQUEST_AT", -1.0);
    c.freeFlightAtSeconds = envSeconds("ELITE_DEV_FREE_FLIGHT_AT", -1.0);
    c.freeFlightTraceSeconds = envSeconds("ELITE_DEV_FF_TRACE_SECONDS", 3.0);   // default 3 s
    c.freeFlightThrustAtSeconds = envSeconds("ELITE_DEV_FREE_FLIGHT_THRUST_AT", -1.0);
    // ELITE_DEV_FREE_FLIGHT_THRUST_CMD=2 brakes (END) instead of full thrust
    if (const char* cmdEnv = std::getenv("ELITE_DEV_FREE_FLIGHT_THRUST_CMD"); cmdEnv && *cmdEnv)
        c.freeFlightThrustCommand = std::atoi(cmdEnv);
    c.droneViewAtSeconds = envSeconds("ELITE_DEV_DRONE_VIEW_AT", -1.0);
    if (const char* v = std::getenv("ELITE_DEV_DESTROY_MODULES_AT"); v && *v)
    {
        const std::string spec(v);
        const auto colon = spec.find(':');
        c.destroyModulesAtSeconds = std::atof(spec.substr(0, colon).c_str());
        std::string cur;
        for (std::size_t i = colon == std::string::npos ? spec.size() : colon + 1; i <= spec.size(); ++i)
        {
            if (i == spec.size() || spec[i] == ',') { if (!cur.empty()) c.destroyModules.push_back(cur); cur.clear(); }
            else cur += spec[i];
        }
    }
    return c;
}

void DevScriptRunner::update(SpaceState& s)
{
    openMapsWhenDue(s);
    runSolMapScript(s);
    runServiceScript(s);
    requestDockingWhenDue(s);
    toggleFreeFlightWhenDue(s);
    traceFreeFlight(s);
    latchFreeFlightThrustWhenDue(s);
    openDroneViewWhenDue(s);
    destroyModulesWhenDue(s);
}

// DEV (map performance measurements): ELITE_DEV_GALAXY_MAP_AT=<s> opens the
// Galaxy Map once, <s> seconds after the client started;
// ELITE_DEV_SYSTEM_MAP_AT=<s> opens the System Map once.
void DevScriptRunner::openMapsWhenDue(SpaceState& s)
{
    if (m_config.galaxyMapAtSeconds >= 0.0 && !m_galaxyOpened && s.context().app && s.m_client &&
        startupElapsedMs(s.m_startupTotalBegin) >= m_config.galaxyMapAtSeconds * 1000.0)
    {
        m_galaxyOpened = true;
        s.context().app->requestNavigationView(NavigationPresentationView::Galaxy);
    }

    if (m_config.systemMapAtSeconds >= 0.0 && !m_systemOpened && s.context().app && s.m_client &&
        startupElapsedMs(s.m_startupTotalBegin) >= m_config.systemMapAtSeconds * 1000.0)
    {
        m_systemOpened = true;
        s.context().app->requestNavigationView(NavigationPresentationView::System);
    }
}

// DEV: ELITE_DEV_SOL_MAP_SCRIPT="t:cmd;t:cmd;..." - System Map acceptance steps at t seconds:
//   system | grid | body:<id> | hub:<id> | object:<id> | hover:<id> | zoom:<steps> | transfer:<hubId> | report
// (no IDs are needed by a human; the script only replays what a click / key does).
void DevScriptRunner::runSolMapScript(SpaceState& s)
{
    const auto& steps = m_config.solMapScript;
    while (m_nextSolMapStep < steps.size() && s.context().app && s.m_client &&
           startupElapsedMs(s.m_startupTotalBegin) >= steps[m_nextSolMapStep].first * 1000.0 + m_scriptShiftMs)
    {
        const std::string& step = steps[m_nextSolMapStep].second;
        if (step.rfind("waitfor:", 0) == 0)
        {
            // waitfor:system:<id> | waitfor:route (a launched route ran and ended) | waitfor:hubnear:<hub id>:<km>
            // | waitfor:refueled (the job asked by "refuel" ended)
            // optional "@<max seconds>": give up after that (logged) and go on
            const double now = startupElapsedMs(s.m_startupTotalBegin);
            std::string condition = step.substr(8);
            double maxS = 1.0e9;
            if (const auto at = condition.rfind('@'); at != std::string::npos)
            {
                maxS = std::atof(condition.c_str() + at + 1);
                condition.resize(at);
            }
            if (m_waitStartMs < 0.0)
            {
                m_waitStartMs = now;
                m_waitRouteSeen = false;
                std::cerr << "[SolMapScript] waitfor " << condition << " (t=" << glfwGetTime() << ")\n";
            }
            const bool done = waitSatisfied(s, condition);
            const bool timeout = now - m_waitStartMs > maxS * 1000.0;
            if (!done && !timeout)
                break;
            std::cerr << "[SolMapScript] waitfor " << condition << (done ? " satisfied" : " TIMED OUT") << " after "
                      << (now - m_waitStartMs) / 1000.0 << " s (t=" << glfwGetTime() << ")\n";
            m_scriptShiftMs = now - steps[m_nextSolMapStep].first * 1000.0;
            m_waitStartMs = -1.0;
            ++m_nextSolMapStep;
            continue;
        }
        const std::string cmd = steps[m_nextSolMapStep++].second;
        std::cerr << "[SolMapScript] " << cmd << " (t=" << glfwGetTime() << ")\n";
        if (cmd == "undock")
        {
            // Ctrl+L: the dock dispatcher flies the ship out (a tour: leave for the next jump)
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::RequestDockDeparture;
            msg.payload = command;
            s.m_client->sendMessage(msg);
            continue;
        }
        if (cmd.rfind("dock:", 0) == 0)
        {
            // dock:<module id>:<anchor id> - automatic docking as from the dock card (a tour: the flight through the
            // city's green lanes to its dock)
            const std::string t = cmd.substr(5);
            const auto colon = t.rfind(':');
            game::navigation::RouteTargetRef target;
            target.kind = game::navigation::NavigationRouteAnchorKind::SemanticAnchor;
            if (const auto it = s.m_client->world().ships().find(s.m_playerId.value); it != s.m_client->world().ships().end())
                target.systemId = it->second.transform.motion.systemId;
            target.stableObjectId = colon == std::string::npos ? t : t.substr(0, colon);
            target.semanticAnchorId = colon == std::string::npos ? std::string(DevDockAnchorId) : t.substr(colon + 1);
            const auto serial = s.m_navigationWorkspace.dockingRouteRequests().request(
                target, game::navigation::DockingRouteRequest::Mode::Automatic);
            s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::RoutePlanning, true);
            s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::LocalGuidance, true);
            s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::HudGuidanceCorridor, true);
            std::cerr << "[DevDockRequest] script request=" << serial << " target=" << target.stableObjectId << ':'
                      << target.semanticAnchorId << '\n';
            continue;
        }
        if (cmd.rfind("capture:", 0) == 0)
        {
            // capture:<png path> - this frame (the tour's evidence; ELITE_DEV_FRAME_CAPTURE takes fixed times only)
            core::dev::frameCaptureRequests().push_back({glfwGetTime(), cmd.substr(8), false});
            continue;
        }
        if (!runMapVerb(s, cmd) && !runRouteVerb(s, cmd) && !runObjectListVerb(s, cmd) &&
            !runShipVerb(s, cmd))
            runReportVerb(s, cmd);
    }
}

// the conditions of "waitfor:" (a tour through jumps: author 2026-11-18 «экскурсию по нескольким хабам разных колоний
// ... через прыжки, заодно проверим надежность механизма прыжков»)
bool DevScriptRunner::waitSatisfied(SpaceState& s, const std::string& condition)
{
    if (!s.m_client)
        return false;
    if (condition.rfind("system:", 0) == 0)
        return s.m_client->world().playerSystemId() == std::atoi(condition.c_str() + 7);
    if (condition == "route")
    {
        // a launched route: seen running, then ended
        const int state = s.clientRouteState();
        if (state != 0)
            m_waitRouteSeen = true;
        return m_waitRouteSeen && state == 0;
    }
    if (condition == "free")
    {
        // released by the dock dispatcher (or never docked)
        const auto it = s.m_client->world().ships().find(s.m_playerId.value);
        return it != s.m_client->world().ships().end() && !it->second.dockDispatcher.active;
    }
    if (condition == "docked")
    {
        const auto it = s.m_client->world().ships().find(s.m_playerId.value);
        return it != s.m_client->world().ships().end() && it->second.dockDispatcher.active &&
               it->second.dockDispatcher.phase == static_cast<std::uint8_t>(game::docking::traffic::DockTrafficPhase::Docked);
    }
    if (condition == "dockdone")
    {
        // a docking request: seen running (any phase but Idle), then ended (docked or failed - the log tells which)
        if (s.m_clientDockingPhase != SpaceState::ClientDockingPhase::Idle)
            m_waitRouteSeen = true;
        return m_waitRouteSeen && s.m_clientDockingPhase == SpaceState::ClientDockingPhase::Idle;
    }
    if (condition == "refueled")
    {
        // the refuel job asked by "refuel" ended (its serial moved on): done / refused / cancelled
        const auto it = s.m_client->world().ships().find(s.m_playerId.value);
        if (it == s.m_client->world().ships().end())
            return false;
        const auto& m = it->second.transform.motion;
        if (m_refuelSerialAtRequest < 0 || m.refuelSerial == static_cast<std::uint16_t>(m_refuelSerialAtRequest))
            return false;
        static const char* const phases[] = {"none", "hookup", "pumping", "disconnect", "done", "refused", "cancelled"};
        std::cerr << "[SolMapScript] refueled: " << phases[std::min<int>(m.refuelState, 6)] << " (refusal " << int(m.refuelRefusal)
                  << "), delivered " << m.refuelDeliveredT << " t, J charge " << m.jChargeT << " / " << m.jChargeCapacityT << " t\n";
        m_refuelSerialAtRequest = -1;
        return true;
    }
    if (condition.rfind("hubnear:", 0) == 0)
    {
        // hubnear:<hub id>:<km> - the player within <km> of any object of that hub
        const std::string rest = condition.substr(8);
        const auto colon = rest.rfind(':');
        const std::string hub = rest.substr(0, colon);
        const double km = colon == std::string::npos ? 30.0 : std::atof(rest.c_str() + colon + 1);
        const auto ship = s.m_client->world().ships().find(s.m_playerId.value);
        if (ship == s.m_client->world().ships().end())
            return false;
        const glm::dvec3 at = ship->second.transform.fullWorldMeters();
        for (const auto& [id, object] : s.m_client->world().objects())
            if (object.hubAttachment.valid && object.hubAttachment.hubId == hub &&
                glm::length(world::coordinates::fullMeters(object.worldPosition) - at) < km * 1000.0)
                return true;
        return false;
    }
    std::cerr << "[SolMapScript] waitfor: unknown condition '" << condition << "'\n";
    return true;
}

// Map / panel verbs: system, grid, body:, enter:, tradetab:, tradehub:,
// trademap:, detail, hub:, object:, hover:, pointer:, panel:, overlay,
// mapturn:, zoom:
bool DevScriptRunner::runMapVerb(SpaceState& s, const std::string& cmd)
{
    auto arg = [&](const char* k) { return cmd.substr(std::string(k).size()); };
    if (cmd == "system")
    {
        // from a Detail / Hub map back to the System map the way the panel's СИСТЕМА button does
        // (requestNavigationView alone kept the renderer in Detail: a later body: / detail did nothing)
        s.context().app->requestNavigationView(NavigationPresentationView::System);
        if (s.m_systemMapRenderer.mode() != SystemMapRenderer::Mode::System)
            s.setSystemMapLoadedSystemMode();
    }
    // F12: the local map (the player's hub, or the cube the player can be followed in)
    else if (cmd == "local")
        s.context().app->requestNavigationView(NavigationPresentationView::Local);
    else if (cmd == "grid")
        s.m_systemMapRenderer.toggleSystemGridOverlay();
    else if (cmd.rfind("body:", 0) == 0)
    {
        // ASCII aliases: the environment of a Windows process is not UTF-8
        std::string id = arg("body:");
        for (const auto& [k, v] : FocusBodyAliases)
            if (id == k)
                id = v;
        s.m_systemMapRenderer.devFocusBody(id);
    }
    else if (cmd.rfind("enter:", 0) == 0)
    {
        std::string id = arg("enter:");
        for (const auto& [k, v] : EnterBodyAliases)
            if (id == k)
                id = v;
        s.m_systemMapRenderer.devEnter(id);
    }
    // F8 Trade: the same actions a click makes
    else if (cmd.rfind("tradetab:", 0) == 0)
        s.m_tradeTab = std::atoi(arg("tradetab:").c_str());
    else if (cmd.rfind("tradehub:", 0) == 0)
        s.m_tradeHub = arg("tradehub:");
    else if (cmd.rfind("trademap:", 0) == 0)
    {
        s.m_tradeHub = arg("trademap:");
        s.m_systemMapRenderer.devFocusHub(s.m_tradeHub);
        s.context().app->requestNavigationView(NavigationPresentationView::System);
    }
    // F8 MARKET deal window (TRADE_DESIGN step 3), the same paths a player takes:
    //   tradeclick:<what>[:<arg>] - a click on the region of the LAST RENDERED frame that carries that action (good:<good id>,
    //     side:buy|sell, units:t|c20, step:+1|-10, slider:<fraction 0..1>, max, confirm, cancel, retry); a region the
    //     frame did not draw is logged, not faked
    //   tradekey:<up|down|left|right|tab|enter|backspace|r|u|1..5>[+shift] - the key handler (1..5 = the shown tab order)
    //   tradedeal:report - the window's preview / limits / answer to the log
    else if (cmd.rfind("tradeclick:", 0) == 0)
    {
        using ui::presentation::ServicePanelActionType;
        std::string what = arg("tradeclick:"), param;
        if (const auto colon = what.find(':'); colon != std::string::npos)
        {
            param = what.substr(colon + 1);
            what.resize(colon);
        }
        ServicePanelActionType type = ServicePanelActionType::TradeDealConfirm;
        double fraction = 0.5;
        if (what == "good")
        {
            type = ServicePanelActionType::TradeSelectGood;
            const auto& goods = game::economy::EconomyModel::instance().goods();
            for (std::size_t k = 0; k < goods.size(); ++k)
                if (goods[k].id == param)
                    param = std::to_string(k);
        }
        else if (what == "side") type = ServicePanelActionType::TradeDealSide;
        else if (what == "units") type = ServicePanelActionType::TradeDealUnits;
        else if (what == "step") type = ServicePanelActionType::TradeDealStep;
        else if (what == "slider")
        {
            type = ServicePanelActionType::TradeDealSlider;
            fraction = std::atof(param.c_str());
        }
        else if (what == "max") type = ServicePanelActionType::TradeDealMax;
        else if (what == "cancel") type = ServicePanelActionType::TradeDealCancel;
        else if (what == "retry") type = ServicePanelActionType::TradeDealRetry;
        if (const auto action = s.m_servicePanels.devClick(type, param, fraction))
            s.applyTradeDealAction(*action);
        else
            std::cerr << "[TradeDeal] DEV click " << what << ":" << param << ": the last frame has no such region" << std::endl;
    }
    else if (cmd.rfind("tradekey:", 0) == 0)
    {
        std::string name = arg("tradekey:");
        bool shift = false;
        if (const auto plus = name.find("+shift"); plus != std::string::npos)
        {
            shift = true;
            name.resize(plus);
        }
        static const std::pair<const char*, int> Keys[] = {
            {"up", GLFW_KEY_UP}, {"down", GLFW_KEY_DOWN}, {"left", GLFW_KEY_LEFT}, {"right", GLFW_KEY_RIGHT}, {"tab", GLFW_KEY_TAB},
            {"enter", GLFW_KEY_ENTER}, {"backspace", GLFW_KEY_BACKSPACE}, {"r", GLFW_KEY_R}, {"u", GLFW_KEY_U}, {"1", GLFW_KEY_1},
            {"2", GLFW_KEY_2}, {"3", GLFW_KEY_3}, {"4", GLFW_KEY_4}, {"5", GLFW_KEY_5}, {"6", GLFW_KEY_6}};
        for (const auto& [k, code] : Keys)
            if (name == k)
                s.tradeDealKey(code, shift);
    }
    else if (cmd == "tradedeal:report")
        s.logTradeDeal("report");
    // the contract board (TRADE_DESIGN §3, trade step 5):
    //   contract:take:<id> | contract:take:pick:<part of an id> - the request the CONTRACTS tab's confirm sends
    //   contract:abandon:<id> | contract:report ("[ContractReport]": the replicated board / contracts with timers)
    //   econ:days:<n> - DEV: the server's economy clock runs n days ahead (the board's days without waiting)
    //   econ:await:<part of a series id> - DEV: ahead to that series' next publication at this hub (0 if open now)
    //   econ:spendall - DEV: the player spends all spendable money (cash + the overdraft to its limit)
    //   contractclick:<select:<id|pick:<part>>|take|abandon|confirm|cancel> - a click on the region the last frame drew
    else if (cmd.rfind("contract:take:", 0) == 0 || cmd.rfind("contract:abandon:", 0) == 0 || cmd.rfind("contractclick:", 0) == 0)
    {
        using ui::presentation::ServicePanelActionType;
        const auto& t = s.m_client->sessionSnapshot().trade;
        const auto resolve = [&](std::string id) {
            if (id.rfind("pick:", 0) == 0)
            {
                const std::string part = id.substr(5);
                id.clear();
                for (const auto& r : t.board)
                    if (id.empty() && r.status == "open" && r.id.find(part) != std::string::npos)
                        id = r.id;
                for (const auto& r : t.contracts)
                    if (id.empty() && r.status == "active" && r.id.find(part) != std::string::npos)
                        id = r.id;
                if (id.empty())
                    std::cerr << "[Contract] DEV pick '" << part << "': no open posting / active contract matches" << std::endl;
            }
            return id;
        };
        if (cmd.rfind("contract:take:", 0) == 0 || cmd.rfind("contract:abandon:", 0) == 0)
        {
            const bool take = cmd.rfind("contract:take:", 0) == 0;
            const std::string id = resolve(cmd.substr(take ? 14 : 17));
            if (!id.empty())
            {
                s.m_tradeContract = id;
                s.m_tradeContractRequest = s.sendContractCommand(take ? game::network::ClientEconomyCommand::TakeContract
                                                                      : game::network::ClientEconomyCommand::AbandonContract,
                                                                 id);
            }
        }
        else
        {
            std::string what = arg("contractclick:"), param;
            if (const auto colon = what.find(':'); colon != std::string::npos)
            {
                param = what.substr(colon + 1);
                what.resize(colon);
            }
            ServicePanelActionType type = ServicePanelActionType::TradeContractConfirm;
            if (what == "select")
            {
                type = ServicePanelActionType::TradeContractSelect;
                param = resolve(param);
            }
            else if (what == "take") type = ServicePanelActionType::TradeContractTake;
            else if (what == "abandon") type = ServicePanelActionType::TradeContractAbandon;
            else if (what == "cancel") type = ServicePanelActionType::TradeContractCancel;
            if (const auto action = s.m_servicePanels.devClick(type, param))
                s.applyTradeContractAction(*action);
            else if (type == ServicePanelActionType::TradeContractSelect && !param.empty())
            {
                // a row below the visible part of the list: the key / wheel path selects it (the list scrolls to it)
                std::cerr << "[Contract] DEV select " << param << ": off the visible list - selected as by the keys" << std::endl;
                s.applyTradeContractAction({type, param});
            }
            else
                std::cerr << "[Contract] DEV click " << what << ":" << param << ": the last frame has no such region" << std::endl;
        }
    }
    else if (cmd == "contract:report")
        s.logContracts("report");
    else if (cmd.rfind("econ:days:", 0) == 0)
        s.sendEconomyCommand(game::network::ClientEconomyCommand::DevEconomyDays, std::string(), std::atof(cmd.c_str() + 10));
    // TRADE step 9: "trade:black:<good>" - all of it aboard to the black market (the ГРУЗ button); "econ:customs:<p>" - DEV:
    // the customs inspection probability forced to p (1 = always inspect and find; -1 = by the rules)
    else if (cmd.rfind("trade:black:", 0) == 0)
        s.sendEconomyCommand(game::network::ClientEconomyCommand::SellBlack, cmd.substr(12), 0.0);
    else if (cmd.rfind("econ:customs:", 0) == 0)
        s.sendEconomyCommand(game::network::ClientEconomyCommand::DevCustoms, std::string(), std::atof(cmd.c_str() + 13));
    else if (cmd.rfind("econ:grant:", 0) == 0)   // DEV: Cr credited (live checks of purchases)
        s.sendEconomyCommand(game::network::ClientEconomyCommand::DevGrant, std::string(), std::atof(cmd.c_str() + 11));
    else if (cmd == "econ:spendall")   // DEV: a broke pilot - all spendable money spent (cash, account, overdraft)
        s.sendEconomyCommand(game::network::ClientEconomyCommand::DevSpendAll, std::string(), 0.0);
    else if (cmd.rfind("econ:await:", 0) == 0)   // DEV: the clock runs to the next publication of that series here
        s.sendEconomyCommand(game::network::ClientEconomyCommand::DevEconomyDays, cmd.substr(11), 0.0);
    else if (cmd == "detail")
    {
        // the Details tab: the selected body's Detail map
        world::celestial::DetailTarget target;
        if (s.buildSelectedMapDetailTarget(target) && s.composeDetailMapSnapshot(target))
            s.m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Detail);
    }
    else if (cmd.rfind("hub:", 0) == 0)
        s.m_systemMapRenderer.devFocusHub(arg("hub:"));
    else if (cmd.rfind("object:", 0) == 0)
        s.m_systemMapRenderer.devActivateObject(arg("object:"));
    else if (cmd == "launch" && s.m_client)   // DEV: Ctrl+L on a city place (LandOnPad index 2: off it on the manoeuvre thrusters)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::LandOnPad;
        command.index = 2;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    else if (cmd.rfind("camera:", 0) == 0)    // DEV: the main camera like F1-F4 (front / rear / drone / bottom) - see it as the player
    {
        const std::string c = arg("camera:");
        s.setFlightCameraMode(c == "rear" ? ShipCameraMode::Rear : c == "drone" ? ShipCameraMode::Drone
                              : c == "bottom" ? ShipCameraMode::Bottom : ShipCameraMode::Cockpit);
    }
    else if (cmd.rfind("join:", 0) == 0)      // DEV: join a ship's formation (join:<ship id>:<wedge|...>:<slot>, join:off)
        s.devJoinFormation(arg("join:"));
    else if (cmd.rfind("padland:", 0) == 0)   // DEV: a hub city pad landing (padland:roof|garage|stilts[@frame])
        s.m_systemMapRenderer.devRequestPadLanding(arg("padland:"));
    else if (cmd.rfind("hover:", 0) == 0)
        s.m_systemMapRenderer.devHover(arg("hover:"));
    // a scripted pointer gesture through the production input path:
    // pointer:click:<id> | pointer:dclick:<id> | pointer:rclick:<id> | pointer:menu:<command>
    else if (cmd.rfind("pointer:", 0) == 0)
        s.m_systemMapRenderer.devPointer(arg("pointer:"));
    // a native panel navigation button (the same typed action a click on it emits):
    // panel:hub | panel:detail | panel:system | panel:galaxy | panel:select:<objectId>
    else if (cmd.rfind("panel:", 0) == 0)
    {
        using PanelAction = game::presentation::SystemMapPanelActionType;
        const std::string which = arg("panel:");
        game::presentation::SystemMapPanelAction action;
        if (which.rfind("select:", 0) == 0)
        {
            // a row of the hub / beacon list
            action.type = PanelAction::SelectObject;
            action.objectId = which.substr(7);
        }
        else if (which.rfind("module:", 0) == 0)
        {
            // a composition row of the info block (the Hub map frames its city part)
            action.type = PanelAction::SelectHubModuleType;
            action.objectId = which.substr(7);
        }
        else
        action.type = which == "hub"      ? PanelAction::OpenHub
                      : which == "detail" ? PanelAction::OpenDetail
                      : which == "galaxy" ? PanelAction::OpenGalaxy
                                          : PanelAction::OpenSystem;
        s.applyNativeSystemMapPanelAction(action);
        std::cerr << "[SolMapScript] panel " << which << " -> mode "
                  << static_cast<int>(s.m_systemMapRenderer.mode()) << " hub map " << s.m_loadedHubMapHubId
                  << std::endl;
    }
    else if (cmd == "overlay")
    {
        std::cerr << "[SolMapScript] overlay " << s.m_systemMapRenderer.devOverlayReport() << '\n';
        const auto panel = s.buildNativeSystemMapPanelPresentation();
        std::cerr << "[SolMapScript] panel '" << panel.infoTitle << "' canOpenHub " << panel.canOpenHub << ':';
        for (const auto& line : panel.info)
            std::cerr << " {" << line.label << (line.label.empty() ? "" : "=") << line.value << '}';
        // the runtime objects attached to the selected hub (what "operating" means in the panel)
        std::cerr << "\n[SolMapScript] selected hub '" << s.m_systemMapRenderer.selectedHubId() << "' attached:";
        for (const auto& [id, object] : s.m_client->world().objects())
            if (object.hubAttachment.valid && object.hubAttachment.hubId == s.m_systemMapRenderer.selectedHubId())
                std::cerr << " {" << id << " type " << static_cast<int>(object.type) << ' ' << object.displayName << '}';
        std::cerr << "\n[SolMapScript] loaded detail kind " << static_cast<int>(s.m_loadedDetailTarget.sceneKind)
                  << " focus " << s.m_loadedDetailTarget.focusId << " cell " << s.m_loadedDetailTarget.spatialCell.x << ','
                  << s.m_loadedDetailTarget.spatialCell.y << ',' << s.m_loadedDetailTarget.spatialCell.z
                  << " hub map " << s.m_loadedHubMapHubId << " mode " << static_cast<int>(s.m_systemMapRenderer.mode()) << std::endl;
    }
    // mapturn:<yaw>,<pitch> (radians): the Detail map camera, as a drag turns it
    else if (cmd.rfind("mapturn:", 0) == 0)
    {
        const std::string a = arg("mapturn:");
        const auto comma = a.find(',');
        s.m_systemMapRenderer.devDetailOrbit(std::atof(a.c_str()),
                                           comma == std::string::npos ? 0.0 : std::atof(a.c_str() + comma + 1));
    }
    else if (cmd.rfind("zoom:", 0) == 0)
        s.m_systemMapRenderer.devZoom(std::atof(arg("zoom:").c_str()));
    else
        return false;
    return true;
}

// Route / autopilot verbs: transfer:, route:official|unofficial|calc|pause|resume,
// finish:, via:, leg:, autopilot:
bool DevScriptRunner::runRouteVerb(SpaceState& s, const std::string& cmd)
{
    auto arg = [&](const char* k) { return cmd.substr(std::string(k).size()); };
    if (cmd.rfind("transfer:", 0) == 0)
        s.requestOrbitalTransfer(arg("transfer:"));
    else if (cmd == "route:official" || cmd == "route:unofficial")
        s.launchPlannedRoute(cmd == "route:official");
    else if (cmd == "route:calc")
        s.launchPlannedRoute(true, false);
    else if (cmd == "route:pause" || cmd == "route:resume")
        s.sendRouteControl(cmd == "route:pause");
    else if (cmd.rfind("finish:", 0) == 0 || cmd.rfind("via:", 0) == 0)
    {
        // DEV: "finish:hub:<id>" | "finish:body:<moon|earth|mars>" | "via:hub:<id>[:refuel]"
        const bool isFinish = cmd.rfind("finish:", 0) == 0;
        std::string spec = arg(isFinish ? "finish:" : "via:");
        std::string action;
        // suffixes (any order at the end): :refuel :pause :land :orbit :dock :stay (no take-off)
        bool pause = false, stay = false;
        game::navigation::NavigationWaypointProgram program = game::navigation::NavigationWaypointProgram::Auto;
        for (bool more = true; more;)
        {
            more = false;
            static const std::pair<const char*, int> sfx[] = {{":refuel", 0}, {":pause", 1}, {":land", 2}, {":orbit", 3}, {":dock", 4}, {":stay", 5}};
            for (const auto& [k, kind] : sfx)
            {
                const std::size_t n = std::char_traits<char>::length(k);
                if (spec.size() > n && spec.compare(spec.size() - n, n, k) == 0)
                {
                    spec.resize(spec.size() - n);
                    more = true;
                    if (kind == 0) action = "refuel";
                    if (kind == 1) pause = true;
                    if (kind == 2) program = game::navigation::NavigationWaypointProgram::Land;
                    if (kind == 3) program = game::navigation::NavigationWaypointProgram::Orbit;
                    if (kind == 4) program = game::navigation::NavigationWaypointProgram::Dock;
                    if (kind == 5) stay = true;
                }
            }
        }
        game::navigation::RouteTargetRef target;
        world::coordinates::WorldPosition where;
        std::string name = spec;
        if (spec.rfind("hub:", 0) == 0)
        {
            target.kind = game::navigation::NavigationRouteAnchorKind::Hub;
            target.stableObjectId = spec.substr(4);
            if (const auto h = s.m_client->world().hubs().find(target.stableObjectId); h != s.m_client->world().hubs().end())
            {
                where = h->second.worldPosition;
                name = h->second.name;
            }
        }
        else if (spec.rfind("ship:", 0) == 0)
        {
            // DEV: a rendezvous with the ship of that server entity id
            const auto sit = s.m_client->world().ships().find(static_cast<std::uint32_t>(std::atoi(spec.c_str() + 5)));
            if (sit != s.m_client->world().ships().end())
            {
                target.kind = game::navigation::NavigationRouteAnchorKind::Ship;
                target.shipInstanceId = sit->second.instanceId;
                where = sit->second.transform.worldPosition;
                program = game::navigation::NavigationWaypointProgram::Rendezvous;
            }
        }
        else if (spec.rfind("site:", 0) == 0)
        {
            // a surface settlement: its body as the target, the program = its pad
            const std::string body = world::celestial::settlements::surfaceSettlementBody(spec.substr(5));
            target.kind = game::navigation::NavigationRouteAnchorKind::CelestialBody;
            target.stableObjectId = body;
            program = game::navigation::NavigationWaypointProgram::LandAtSettlement;
            if (const auto* snap = s.m_client->celestialSnapshot())
                for (const auto& b : snap->bodies)
                    if (b.id == body)
                        where = world::coordinates::makeWorldPositionFromMeters(b.worldMeters);
        }
        else if (spec.rfind("body:", 0) == 0)
        {
            std::string id = spec.substr(5);
            for (const auto& [k, v] : RouteBodyAliases)
                if (id == k)
                    id = v;
            target.kind = game::navigation::NavigationRouteAnchorKind::CelestialBody;
            target.stableObjectId = id;
            if (const auto* snap = s.m_client->celestialSnapshot())
                for (const auto& b : snap->bodies)
                    if (b.id == id)
                        where = world::coordinates::makeWorldPositionFromMeters(b.worldMeters);
        }
        target.systemId = 0;
        auto& plan = s.m_navigationWorkspace.routePlan();
        auto& w = plan.rememberCandidate(target, spec.rfind("site:", 0) == 0 ? spec : "dev:" + spec, where, std::string(), name);
        w.program = program;
        w.actions.pauseHere = pause;
        w.actions.takeOffAfter = !stay;
        w.actions.refuel = action == "refuel";
        if (spec.rfind("site:", 0) == 0)
            w.actions.landingSite = spec;
        if (w.role != (isFinish ? game::navigation::NavigationWaypointRole::Finish
                                : game::navigation::NavigationWaypointRole::Intermediate))
            plan.toggleRole(w.id, isFinish ? game::navigation::NavigationWaypointRole::Finish
                                           : game::navigation::NavigationWaypointRole::Intermediate);
        w.stopAction = action;
    }
    else if (cmd.rfind("leg:", 0) == 0)
    {
        // DEV: "leg:hub:<id>" | "leg:orbit:<moon|earth|mars>:<alt km>" | "leg:ship:<callsign|any>"
        std::string spec = arg("leg:");
        for (const auto& [k, v] : LegOrbitPrefixAliases)
            if (spec.rfind(k, 0) == 0)
                spec = v + spec.substr(std::char_traits<char>::length(k));
        // a single stop leg flown by the client's route runner
        game::navigation::route::RoutePoint p;
        p.stop = true;
        if (spec.rfind("hub:", 0) == 0)
        {
            p.kind = game::navigation::route::RoutePoint::Kind::Hub;
            p.hubId = spec.substr(4);
        }
        else if (spec.rfind("orbit:", 0) == 0)
        {
            p.kind = game::navigation::route::RoutePoint::Kind::Orbit;
            const std::string rest = spec.substr(6);
            const auto colon = rest.rfind(':');
            p.bodyId = rest.substr(0, colon);
            p.orbitAltitudeM = colon == std::string::npos ? 100000.0 : std::atof(rest.c_str() + colon + 1) * 1000.0;
        }
        else if (spec.rfind("ship:", 0) == 0)
        {
            // a rendezvous: "ship:<callsign>" of a replicated traffic ship, or "ship:any" (the first one)
            const std::string callsign = spec.substr(5);
            p.kind = game::navigation::route::RoutePoint::Kind::Ship;
            for (const auto& [id, ship] : s.m_client->world().ships())
                if (id != s.m_playerId.value && ship.traffic.active && (callsign == "any" || ship.traffic.callsign == callsign))
                {
                    p.shipEntityId = id;
                    std::cerr << "[DevScript] rendezvous with " << ship.traffic.callsign << " (entity " << id << ")" << std::endl;
                    break;
                }
            if (p.shipEntityId == 0)
            {
                std::cerr << "[DevScript] no traffic ship " << callsign << " to meet" << std::endl;
                return true;
            }
        }
        else
            return true;
        s.startClientRoute({p});
    }
    else if (cmd.rfind("autopilot:", 0) == 0)
    {
        std::string id = arg("autopilot:");
        // "autopilot:moon@pad:<lat>,<lon>" lands on a pad, "autopilot:moon@site:<site id>" at a
        // settlement; plain = wild site search
        std::string landingSpec;
        if (const auto at = id.find('@'); at != std::string::npos)
        {
            landingSpec = id.substr(at + 1);
            id.resize(at);
        }
        for (const auto& [k, v] : AutopilotBodyAliases)
            if (id == k)
                id = v;
        if (id == "off")
        {
            s.cancelClientBodyAutopilot();
            s.cancelClientRoute();
        }
        else
            s.requestFreeFlightAutopilot(id, landingSpec);
    }
    else
        return false;
    return true;
}

// The panel object list: listopen, list:<objectId>
bool DevScriptRunner::runObjectListVerb(SpaceState& s, const std::string& cmd)
{
    auto arg = [&](const char* k) { return cmd.substr(std::string(k).size()); };
    // the panel list: open it / pick a row (the same action a click on the row makes)
    if (cmd == "listopen")
        s.m_inSessionPresentationRenderer.devOpenObjectList();
    else if (cmd.rfind("list:", 0) == 0)
    {
        game::presentation::SystemMapPanelAction action;
        action.type = game::presentation::SystemMapPanelActionType::SelectObject;
        action.objectId = arg("list:");
        s.m_inSessionPresentationRenderer.devOpenObjectList(false);   // a row click closes the list
        s.applyNativeSystemMapPanelAction(action);
    }
    else
        return false;
    return true;
}

// Ship command verbs: fire:on|off, missile, laser:next, missile:next, target:,
// freeflight, stabilization, jengine, reactors, repair, eject, backspace, home, end, insert, thrust:,
// yaw:, roll:, pitch:
bool DevScriptRunner::runShipVerb(SpaceState& s, const std::string& cmd)
{
    auto arg = [&](const char* k) { return cmd.substr(std::string(k).size()); };
    // DEV combat: "fire:on" | "fire:off" | "missile" | "laser:next" | "missile:next" | "target:<m>"
    if (cmd == "fire:on" || cmd == "fire:off" || cmd == "missile" || cmd == "laser:next" ||
             cmd == "missile:next" || cmd.rfind("target:", 0) == 0)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::FireWeapon;
        command.index = cmd == "fire:off" ? 0 : cmd == "fire:on" ? 1 : cmd == "missile" ? 2
                      : cmd == "laser:next" ? 3 : cmd == "missile:next" ? 4 : 5;
        if (command.index == 5)
            command.amount = std::atof(arg("target:").c_str());
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    // "gear": the landing legs out / in (G)
    else if (cmd == "gear")
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::ToggleLandingGear;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    // navigation data (JUMP_MECHANICS 10l): "navdata:load:<record id>" into the computer, "navdata:carrier:<record id>"
    // onto a new carrier, "navdata:insert:<carrier number>"; shown in F5 ("service:government")
    else if (cmd.rfind("navdata:", 0) == 0)
    {
        const std::string rest = cmd.substr(8);
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::NavData;
        command.index = rest.rfind("load:", 0) == 0 ? 0 : rest.rfind("carrier:", 0) == 0 ? 1 : 2;
        const std::string value = rest.substr(rest.find(':') + 1);
        if (command.index == 2)
            command.amount = std::atof(value.c_str());
        else
            command.detail = value;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    else if (cmd.rfind("navsel:", 0) == 0)
        s.m_navDataSelected = cmd.substr(7);
    // the trade layer (docs/TRADE_DESIGN.md §9 steps 1-2): "cargo:add:<good>:<t>" (DEV: aboard without paying),
    // "cargo:clear", "trade:buy|sell:<good>:<qty>" with qty = tonnes | "c20x<N>" (N containers of that good) | "all"
    // (sell what is aboard), as the F8 deal button will send it; "trade:report" logs the replicated trade snapshot
    else if (cmd.rfind("cargo:add:", 0) == 0 || cmd == "cargo:clear" || cmd.rfind("trade:buy:", 0) == 0 ||
             cmd.rfind("trade:sell:", 0) == 0)
    {
        const bool add = cmd.rfind("cargo:add:", 0) == 0;
        const bool buy = cmd.rfind("trade:buy:", 0) == 0;
        if (cmd == "cargo:clear")
            s.sendEconomyCommand(game::network::ClientEconomyCommand::DevCargoClear, std::string(), 0.0);
        else
        {
            const std::string rest = cmd.substr(add ? 10 : buy ? 10 : 11);
            const auto colon = rest.find(':');
            const std::string good = rest.substr(0, colon);
            std::string qty = colon == std::string::npos ? std::string("1") : rest.substr(colon + 1);
            // cargo:add:<good>:<t>:<origin hub> - DEV goods "bought" at another hub (the import duty of trade step 4)
            std::string origin;
            if (const auto o = qty.find(':'); add && o != std::string::npos)
            {
                origin = qty.substr(o + 1);
                qty.resize(o);
            }
            double tonnes = std::atof(qty.c_str());
            if (qty.rfind("c20x", 0) == 0)
            {
                double density = 0.0;
                for (const auto& g : game::economy::EconomyModel::instance().goods())
                    if (g.id == good)
                        density = g.densityTM3;
                tonnes = std::atof(qty.c_str() + 4) * game::economy::c20NetTonnes(density);
            }
            else if (qty == "all")
            {
                tonnes = 0.0;
                const auto& trade = s.m_client->sessionSnapshot().trade;
                for (const auto& c : trade.cargo)
                    if (c.good >= 0 && game::economy::EconomyModel::instance().goods()[static_cast<std::size_t>(c.good)].id == good &&
                        c.contractId.empty())
                        tonnes += c.tonnes;
            }
            s.sendEconomyCommand(add ? game::network::ClientEconomyCommand::DevCargoAdd
                                 : buy ? game::network::ClientEconomyCommand::Buy : game::network::ClientEconomyCommand::Sell,
                                 origin.empty() ? good : good + "@" + origin, tonnes);
        }
    }
    else if (cmd == "trade:report")
    {
        const auto& t = s.m_client->sessionSnapshot().trade;
        const auto& w = s.m_client->sessionSnapshot().wallet;
        std::cerr << "[TradeReport] valid " << t.valid << " day " << t.day << " hub '" << t.hubId << "' docked " << t.docked << " market rows "
                  << t.market.size() << " (" << t.fiscalStyleRu << ", VAT " << t.vatShare << ")" << '\n';
        for (const auto& a : w.accounts)
            std::cerr << "[TradeReport] account " << a.label << ": " << a.amount << " " << a.currency << '\n';
        std::cerr << "[TradeReport] cargo net " << t.cargoNetT << " t, gross " << t.cargoGrossT << " t, C20 " << t.c20Used << "/" << t.c20Slots
                  << ", ship mass " << t.shipMassT << " t, jump range " << t.jumpRangeLy << " ly (empty hold " << t.jumpRangeEmptyLy
                  << " ly), VAT to reclaim " << t.vatReclaimCr << " Cr" << '\n';
        const auto& goods = game::economy::EconomyModel::instance().goods();
        for (const auto& c : t.cargo)
            std::cerr << "[TradeReport]   lot " << c.lotId << " " << (c.good >= 0 ? goods[static_cast<std::size_t>(c.good)].id : "?") << " "
                      << c.tonnes << " t, C20 " << c.c20 << ", basis " << c.costBasisCrPerT << " Cr/t, from " << c.originHub << '\n';
        for (const auto& row : t.market)
            if (row.good >= 0 && goods[static_cast<std::size_t>(row.good)].id == "food_staples")
                std::cerr << "[TradeReport]   market food_staples: stock " << row.stockT << " t, bid " << row.bidCr << ", ask " << row.askCr
                          << ", hub takes <= " << row.hubTakesUpToT << " t" << '\n';
        for (const auto& r : t.results)
            std::cerr << "[TradeReport]   result #" << r.requestId << " " << (r.ok ? "OK " : "REFUSED ") << r.code << " " << r.message
                      << " total " << r.totalCr << '\n';
    }
    // per-instance configuration (F7, 2026-11-20): "refitship:<n>" selects the player's n-th owned ship in F7;
    // "refit:replace:<module id | radar | drive>:<product id | first>" applies another product (the F7 apply request);
    // "refit:report" logs what the CLIENT sees: the overrides and the product of the last module in its descriptor
    else if (cmd.rfind("refitship:", 0) == 0)
    {
        const auto page = s.buildRepairRefitPage();
        const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + 10)));
        if (n < page.ships.size())
            s.m_refitShip = page.ships[n].id;
        std::cerr << "[RefitReport] selected ship " << s.m_refitShip.value << " (" << page.ships.size() << " owned)" << '\n';
    }
    else if (cmd.rfind("refit:replace:", 0) == 0)
    {
        std::string module = cmd.substr(14), product = "first";
        if (const auto colon = module.find(':'); colon != std::string::npos)
        {
            product = module.substr(colon + 1);
            module.resize(colon);
        }
        const auto page = s.buildRepairRefitPage();
        if (module == "radar" || module == "drive")
            for (const auto& m : page.modules)
                if ((module == "radar" && m.group == "sensors") || (module == "drive" && m.group == "propulsion"))
                {
                    module = m.moduleId;
                    break;
                }
        const auto it = s.m_client->world().ships().find(s.m_refitShip.value);
        if (it != s.m_client->world().ships().end() && it->second.descriptor && product == "first")
            for (const auto& r : game::ship::refit::compatibleReplacements(*it->second.descriptor, module))
                if (!r.sameProduct && product == "first")
                    product = r.productId;
        if (it != s.m_client->world().ships().end())
            s.m_refitSeenApplySerial = it->second.refit.applySerial;
        s.m_refitModule = module;
        s.sendRefitCommand(true, {game::ship::refit::RefitOperationKind::Replace, module, product});
        std::cerr << "[RefitReport] apply ship " << s.m_refitShip.value << " module " << module << " <- " << product << '\n';
    }
    else if (cmd == "refit:report")
    {
        const auto it = s.m_client->world().ships().find(s.m_refitShip.value);
        if (it == s.m_client->world().ships().end() || !it->second.descriptor)
            std::cerr << "[RefitReport] no such ship " << s.m_refitShip.value << '\n';
        else
        {
            std::string overrides;
            for (const auto& o : it->second.refit.productOverrides)
                overrides += o + " ";
            std::cerr << "[RefitReport] ship " << s.m_refitShip.value << " apply #" << it->second.refit.applySerial << " result '"
                      << it->second.refit.applyResult << "' overrides [" << overrides << "] client module " << s.m_refitModule << " product "
                      << game::ship::refit::moduleFacts(*it->second.descriptor, s.m_refitModule).productId << '\n';
        }
    }
    // base 2: "board:<n>" - the pilot moves to the n-th owned ship of F7 (Player-role, <= 20 km; the server decides)
    else if (cmd.rfind("board:", 0) == 0)
    {
        const auto page = s.buildRepairRefitPage();
        const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + 6)));
        if (n < page.ships.size())
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::BoardShip;
            command.amount = static_cast<double>(page.ships[n].id.value);
            msg.payload = command;
            s.m_client->sendMessage(msg);
            std::cerr << "[Control] DEV board ship " << page.ships[n].id.value << " (from " << s.m_playerId.value << ")" << std::endl;
        }
    }
    // step 14: "buyship:<passport id>" - buys the manufacturer's (new) offer of that model, as the F6 button
    else if (cmd.rfind("buyship:", 0) == 0)
    {
        const std::string passport = cmd.substr(8);
        if (const auto* design = game::catalog::ships::ShipDesignCatalog::instance().findShip(passport))
            for (const auto& o : game::ship::refit::devShipOffers(*design))
                if (o.condition == "new")
                {
                    game::network::ClientMessage msg;
                    ClientShipCommand command;
                    command.type = ClientShipCommand::BuyShip;
                    command.detail = passport;
                    command.target = o.offerId;
                    msg.payload = command;
                    s.m_client->sendMessage(msg);
                    std::cerr << "[Shipyard] DEV buy " << passport << " offer " << o.offerId << " " << o.priceCr << " Cr" << std::endl;
                    break;
                }
    }
    // the employment service: "job:hire:<n>" - the n-th vacancy of this hub (the F5 button), "job:quit"
    // rescue (step 12): "sos:take:<n>" - the n-th SOS call of this system (the F5 button); "sos:report"
    else if (cmd.rfind("sos:take:", 0) == 0)
    {
        // "sos:take:<n>" by index, or "sos:take:<part of a hub id>" - the first call at that hub
        const auto& calls = s.m_client->sessionSnapshot().wallet.sos;
        const std::string what = cmd.substr(9);
        std::string id = "?";
        if (!what.empty() && std::isdigit(static_cast<unsigned char>(what[0])))
        {
            const std::size_t n = static_cast<std::size_t>(std::atoi(what.c_str()));
            if (n < calls.size())
                id = calls[n].id;
        }
        else
            for (const auto& c : calls)
                if (id == "?" && c.hubId.find(what) != std::string::npos)
                    id = c.id;
        s.sendEconomyCommand(game::network::ClientEconomyCommand::TakeSos, id, 0.0);
        std::cerr << "[Rescue] DEV take " << id << " (" << calls.size() << " calls here)" << std::endl;
    }
    else if (cmd == "sos:goto")   // the route's finish = the hub of the accepted call (as finish:hub:<id>)
    {
        const std::string& id = s.m_client->sessionSnapshot().wallet.activeSosId;
        const auto at = id.rfind('@');
        if (id.rfind("sos:", 0) == 0 && at != std::string::npos)
        {
            const std::string hub = id.substr(4, at - 4);
            std::cerr << "[Rescue] DEV goto " << hub << std::endl;
            (void)runRouteVerb(s, "finish:hub:" + hub);
        }
        else
            std::cerr << "[Rescue] DEV goto: no accepted call" << std::endl;
    }
    else if (cmd == "sos:report")
    {
        const auto& w = s.m_client->sessionSnapshot().wallet;
        std::cerr << "[Rescue] report: licence " << w.sarLicence << ", active '" << w.activeSosId << "', " << w.sos.size() << " calls" << std::endl;
        for (const auto& c : w.sos)
            std::cerr << "[Rescue]   " << c.id << " " << c.kindRu << " at " << c.hubId << " +" << c.rewardCr << " " << c.status << std::endl;
    }
    // step 16: "build:sign:<n>" (the n-th contractor offer here), "build:report"
    else if (cmd.rfind("build:", 0) == 0)
    {
        const auto& w = s.m_client->sessionSnapshot().wallet;
        // part 3: "build:keep:<n>" / "build:sell:<n>" (the n-th mine's sale mode), "build:load:<n>" (what fits)
        if ((cmd.rfind("build:keep:", 0) == 0 || cmd.rfind("build:sell:", 0) == 0 || cmd.rfind("build:load:", 0) == 0))
        {
            const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + 11)));
            if (n < w.mines.size())
            {
                if (cmd.rfind("build:load:", 0) == 0)
                    s.sendEconomyCommand(game::network::ClientEconomyCommand::LoadFromMine, w.mines[n].id, 0.0);
                else
                    s.sendEconomyCommand(game::network::ClientEconomyCommand::SetMineSale, w.mines[n].id, cmd.rfind("build:sell:", 0) == 0 ? 1.0 : 0.0);
            }
        }
        if (cmd.rfind("build:lease:", 0) == 0)
        {
            const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + 12)));
            if (n < w.siteOffers.size())
                s.sendEconomyCommand(game::network::ClientEconomyCommand::LeaseSite, w.siteOffers[n].id, 0.0);
        }
        for (const auto& so : w.siteOffers)
            std::cerr << "[Mine]   site " << so.id << " " << so.nameRu << " " << so.goodRu << " r " << so.radiusM << " grade " << so.grade << std::endl;
        for (const auto& m : w.mines)
            std::cerr << "[Mine]   mine " << m.id << " " << m.statusRu << " left " << m.reserveLeftT << " today " << m.lastOutputT << " sold " << m.soldT
                      << " revenue " << m.revenueCr << std::endl;
        if (cmd.rfind("build:sign:", 0) == 0)
        {
            const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + 11)));
            if (n < w.buildOffers.size())
                s.sendEconomyCommand(game::network::ClientEconomyCommand::SignConstruction, w.buildOffers[n].id, 0.0);
        }
        std::cerr << "[Build] DEV " << cmd << ": " << w.buildOffers.size() << " offer(s), " << w.buildProjects.size() << " project(s)" << std::endl;
        for (const auto& o : w.buildOffers)
            std::cerr << "[Build]   offer " << o.id << " " << o.projectRu << " " << o.contractorRu << " " << o.priceCr << " Cr " << o.workDays << " d" << std::endl;
        for (const auto& p : w.buildProjects)
            std::cerr << "[Build]   project " << p.id << " " << p.projectRu << " s" << p.stage << " " << p.progress << " paid " << p.paidCr << " / "
                      << p.priceCr << " " << p.statusRu << std::endl;
    }
    // step 15: "bot:hire:<n>" (the n-th candidate onto the first owned ship not flown), "bot:loop:<n>", "bot:fire:<n>" (the n-th bot)
    else if (cmd.rfind("bot:", 0) == 0)
    {
        const auto& w = s.m_client->sessionSnapshot().wallet;
        const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + cmd.rfind(':') + 1)));
        const auto page = s.buildTradePage();
        if (cmd.rfind("bot:hire:", 0) == 0 && n < w.botCandidates.size())
            s.sendEconomyCommand(game::network::ClientEconomyCommand::HireBot, w.botCandidates[n].id, static_cast<double>(page.botShipEntity));
        else if (cmd.rfind("bot:loop:", 0) == 0 && n < w.bots.size())
            s.sendEconomyCommand(game::network::ClientEconomyCommand::OrderBotLoop, w.bots[n].id, 0.0);
        else if (cmd.rfind("bot:fire:", 0) == 0 && n < w.bots.size())
            s.sendEconomyCommand(game::network::ClientEconomyCommand::FireBot, w.bots[n].id, 0.0);
        std::cerr << "[Bots] DEV " << cmd << " (candidates " << w.botCandidates.size() << ", bots " << w.bots.size() << ", ship " << page.botShipEntity
                  << ")" << std::endl;
    }
    else if (cmd.rfind("job:hire:", 0) == 0 || cmd == "job:quit")
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = cmd == "job:quit" ? ClientShipCommand::QuitJob : ClientShipCommand::HireJob;
        if (command.type == ClientShipCommand::HireJob)
        {
            const auto& vacancies = s.m_client->sessionSnapshot().wallet.vacancies;
            const std::size_t n = static_cast<std::size_t>(std::max(0, std::atoi(cmd.c_str() + 9)));
            command.detail = n < vacancies.size() ? vacancies[n].id : std::string("?");
        }
        msg.payload = command;
        s.m_client->sendMessage(msg);
        std::cerr << "[Employment] DEV " << cmd << " " << command.detail << std::endl;
    }
    else if (cmd == "service:shipyard")
        s.context().app->requestServicePanel(ui::services::ServiceUiId::Shipyard);
    else if (cmd == "service:refit")   // F7
        s.context().app->requestServicePanel(ui::services::ServiceUiId::RepairRefit);
    else if (cmd == "service:trade")
        s.context().app->requestServicePanel(ui::services::ServiceUiId::Trade);
    // F5 Government services (TRADE_DESIGN §11a): "service:government", "govtab:<0 state app | 1 Галера+>"
    else if (cmd == "service:government")
        s.context().app->requestServicePanel(ui::services::ServiceUiId::GovernmentServices);
    else if (cmd.rfind("govtab:", 0) == 0)
        s.m_governmentTab = std::atoi(cmd.c_str() + 7);
    else if (cmd == "cargo:insure")   // step 13: the F8 ГРУЗ insure button's request
        s.sendEconomyCommand(game::network::ClientEconomyCommand::InsureCargo, std::string(), 0.0);
    else if (cmd.rfind("licence:buy:", 0) == 0)   // step 11: the F5 buy button's request (licence type)
        s.sendEconomyCommand(game::network::ClientEconomyCommand::BuyLicence, cmd.substr(12), 0.0);
    else if (cmd.rfind("govsec:", 0) == 0)   // the state app's section: 0 taxes, 1 licences, 2 navigation data
        s.m_governmentSection = std::atoi(cmd.c_str() + 7);
    // refuel at the station the ship stands at, as Ctrl+U (docs/FUEL_STATIONS.md): "refuel" | "refuel:cancel";
    // "waitfor:refueled" waits for the job to end (done / refused / cancelled - the log says which)
    else if (cmd == "refuel" || cmd == "refuel:cancel")
    {
        if (cmd == "refuel")
        {
            const auto it = s.m_client->world().ships().find(s.m_playerId.value);
            instance().m_refuelSerialAtRequest = it != s.m_client->world().ships().end() ? it->second.transform.motion.refuelSerial : -1;
        }
        s.requestClientRefuel(cmd == "refuel" ? 1 : 0);
    }
    // the map menu's jump rows through the client's own handler: "jumpui:calc|start|cancel:<system id>"
    else if (cmd.rfind("jumpui:", 0) == 0)
    {
        const std::string rest = cmd.substr(7);
        const int kind = rest.rfind("calc", 0) == 0 ? 1 : rest.rfind("start", 0) == 0 ? 2 : 3;
        const auto colon = rest.find(':');
        // "jumpui:calc|start:<system id>:<body>" - the in-system jump (the body: an ASCII alias or an id)
        std::string body;
        if (colon != std::string::npos)
            if (const auto second = rest.find(':', colon + 1); second != std::string::npos)
            {
                body = rest.substr(second + 1);
                for (const auto& [k, v] : FocusBodyAliases)
                    if (body == k)
                        body = v;
            }
        s.handleJumpRequest(kind, colon == std::string::npos ? -1 : std::atoi(rest.c_str() + colon + 1), body);
    }
    // the interstellar jump: "jump:<system id>[:<exit body id>]" (START), "jump:cancel"
    else if (cmd.rfind("jump:", 0) == 0)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::RequestJump;
        const std::string rest = cmd.substr(5);
        command.index = rest == "cancel" ? -1 : std::atoi(rest.c_str());
        if (const auto colon = rest.find(':'); colon != std::string::npos)
            command.detail = rest.substr(colon + 1);
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    // Ctrl+K stabilization on / off (= normal flight off / on), Ctrl+J the J engine
    else if (cmd == "freeflight" || cmd == "stabilization" || cmd == "jengine")
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = cmd == "jengine" ? ClientShipCommand::ToggleJEngine : ClientShipCommand::ToggleStabilization;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    // the emergency keys (author 2026-11-10 containment trials): Ctrl+Alt+Shift+I propulsion reactors on / off,
    // Ctrl+R the repair drone, L-Ctrl+Q eject the cockpit
    else if (cmd == "reactors" || cmd == "repair" || cmd == "eject")
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = cmd == "reactors" ? ClientShipCommand::TogglePropulsionReactors
                     : cmd == "repair" ? ClientShipCommand::StartBestRepairJob : ClientShipCommand::EjectCockpitCapsule;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    // free-flight thrust keys: home / end / insert (what the keys send)
    else if (cmd == "backspace")
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::ToggleAssistedFreeRotation;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    else if (cmd == "home" || cmd == "end" || cmd == "insert")
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::SetFreeFlightThrust;
        command.index = cmd == "home" ? 1 : cmd == "end" ? 2 : 0;
        msg.payload = command;
        s.m_client->sendMessage(msg);
    }
    else if (cmd.rfind("thrust:", 0) == 0)
        s.m_devThrustUntilMs = startupElapsedMs(s.m_startupTotalBegin) + std::atof(arg("thrust:").c_str()) * 1000.0;
    else if (cmd.rfind("yaw:", 0) == 0)
        s.m_devYawUntilMs = startupElapsedMs(s.m_startupTotalBegin) + std::atof(arg("yaw:").c_str()) * 1000.0;
    else if (cmd.rfind("roll:", 0) == 0)
        s.m_devRollUntilMs = startupElapsedMs(s.m_startupTotalBegin) + std::atof(arg("roll:").c_str()) * 1000.0;
    else if (cmd.rfind("pitch:", 0) == 0)
        s.m_devPitchUntilMs = startupElapsedMs(s.m_startupTotalBegin) + std::atof(arg("pitch:").c_str()) * 1000.0;
    else
        return false;
    return true;
}

// Report verbs: jstate, report
bool DevScriptRunner::runReportVerb(SpaceState& s, const std::string& cmd)
{
    if (cmd == "jstate")
    {
        // DEV: global (frame) and local speed of the player ship
        const auto shipIt = s.m_client->world().ships().find(s.m_playerId.value);
        if (shipIt != s.m_client->world().ships().end())
        {
            const auto& mo = shipIt->second.transform.motion;
            std::cerr << "[DevJState] freeFlight " << mo.freeFlight << " frame " << mo.travelFrame.frameId
                      << " global " << glm::length(mo.travelFrame.linearVelocityMps) / 1000.0 << " km/s, local "
                      << glm::length(mo.localVelocityMps) << " m/s, server altitude " << mo.freeFlightOrbitAltitudeM
                      << " m (sphere), ground+hull " << mo.freeFlightGroundHeightM << " m, up (" << shipIt->second.transform.up().x << ", "
                      << shipIt->second.transform.up().y << ", " << shipIt->second.transform.up().z << ")" << std::endl;
        }
    }
    else if (cmd == "report")
    {
        const auto ids = s.m_systemMapRenderer.devVisibleObjectIds();
        std::cerr << "[SolMapScript] visible objects " << ids.size() << ':';
        for (const auto& id : ids)
            std::cerr << ' ' << id;
        std::cerr << "\n[SolMapScript] network route lines " << s.m_systemMapRenderer.devNetworkRouteLines()
                  << ", transfer tunnel " << (s.m_systemMapRenderer.devTransferTunnelShown() ? "shown" : "none")
                  << ", grid " << (s.m_systemMapRenderer.systemGridOverlayVisible() ? "on" : "off")
                  << ", active " << s.m_systemMapRenderer.activeObjectId() << '\n';
        const auto shipIt = s.m_client->world().ships().find(s.m_playerId.value);
        if (shipIt != s.m_client->world().ships().end())
        {
            const auto& mo = shipIt->second.transform.motion;
            std::cerr << "[SolMapScript] player hub " << mo.hubId << " matched " << mo.matchedToReferenceFrame
                      << " world speed " << glm::length(mo.worldVelocityMps) << " m/s local speed " << glm::length(mo.localVelocityMps) << " m/s";
            for (const char* hubId : ReportHubIds)
            {
                const auto h = s.m_client->world().hubs().find(hubId);
                if (h != s.m_client->world().hubs().end())
                    std::cerr << " | vs " << hubId << ": " << glm::length(mo.worldVelocityMps - h->second.worldVelocityMps) << " m/s, "
                              << glm::length(world::coordinates::fullMeters(shipIt->second.transform.worldPosition) -
                                             world::coordinates::fullMeters(h->second.worldPosition)) / 1000.0 << " km";
            }
            std::cerr << '\n';
        }
    }
    else
        return false;
    return true;
}

// DEV: ELITE_DEV_SERVICE_SCRIPT=<s> opens F6 at s, F7 at s+6, steps to
// the next owned ship, selects its radar module (preview requested
// by the selection), and applies the repair at s+14 (frame captures
// are taken by ELITE_DEV_FRAME_CAPTURE).
void DevScriptRunner::runServiceScript(SpaceState& s)
{
    const double serviceAtSeconds = m_config.serviceScriptAtSeconds;
    int& serviceStep = m_serviceStep;
    if (serviceAtSeconds >= 0.0 && s.m_client && s.context().app)
    {
        const double t = startupElapsedMs(s.m_startupTotalBegin) / 1000.0 - serviceAtSeconds;
        if (serviceStep == 0 && t >= 0.0)
        {
            ++serviceStep;
            s.context().app->requestServicePanel(ui::services::ServiceUiId::Shipyard);
            std::cerr << "[ServiceScript] F6" << '\n';
        }
        else if (serviceStep == 1 && t >= 6.0)
        {
            ++serviceStep;
            s.context().app->requestServicePanel(ui::services::ServiceUiId::RepairRefit);
            std::cerr << "[ServiceScript] F7" << '\n';
        }
        else if (serviceStep == 2 && t >= 8.0)
        {
            ++serviceStep;
            const auto page = s.buildRepairRefitPage();
            if (page.ships.size() > 1)
                s.m_refitShip = page.ships[1].id;
            const auto next = s.buildRepairRefitPage();
            for (const auto& m : next.modules)
                if (m.group == "sensors" && m.failed())
                {
                    s.m_refitModule = m.moduleId;
                    s.sendRefitCommand(false, {game::ship::refit::RefitOperationKind::Repair, m.moduleId, {}});
                    break;
                }
            std::cerr << "[ServiceScript] ship " << s.m_refitShip.value << " module " << s.m_refitModule << '\n';
        }
        else if (serviceStep == 3 && t >= 14.0)
        {
            ++serviceStep;
            const auto page = s.buildRepairRefitPage();
            if (page.previewForSelection && page.preview.ok && page.preview.applicable)
            {
                if (const auto it = s.m_client->world().ships().find(s.m_refitShip.value); it != s.m_client->world().ships().end())
                    s.m_refitSeenApplySerial = it->second.refit.applySerial;
                s.sendRefitCommand(true, page.preview.operation);
            }
            std::cerr << "[ServiceScript] apply preview ok=" << page.preview.ok << " deltas=" << page.preview.deltas.size() << '\n';
        }
    }
}

// DEV: ELITE_DEV_DOCK_REQUEST_AT=<s> requests automatic docking at the
// Earth hub hex dock once, in the normal scene (freeze captures).
void DevScriptRunner::requestDockingWhenDue(SpaceState& s)
{
    if (m_config.dockRequestAtSeconds >= 0.0 && !m_dockRequestSent && s.m_client &&
        startupElapsedMs(s.m_startupTotalBegin) / 1000.0 >= m_config.dockRequestAtSeconds)
    {
        m_dockRequestSent = true;
        game::navigation::RouteTargetRef target;
        target.kind = game::navigation::NavigationRouteAnchorKind::SemanticAnchor;
        if (const auto it = s.m_client->world().ships().find(s.m_playerId.value); it != s.m_client->world().ships().end())
            target.systemId = it->second.transform.motion.systemId;
        // ELITE_DEV_DOCK_TARGET="<module id>:<anchor id>" (e.g. plane_demo.dock:dock_gate_front) overrides the gate
        std::string module = DevDockModuleId, anchor = DevDockAnchorId;
        if (const char* v = std::getenv("ELITE_DEV_DOCK_TARGET"); v && *v)
        {
            const std::string t(v);
            const auto colon = t.rfind(':');
            module = colon == std::string::npos ? t : t.substr(0, colon);
            if (colon != std::string::npos)
                anchor = t.substr(colon + 1);
        }
        target.stableObjectId = module;
        target.semanticAnchorId = anchor;
        const auto serial = s.m_navigationWorkspace.dockingRouteRequests().request(
            target, game::navigation::DockingRouteRequest::Mode::Automatic);
        // the same modules a request from the dock card enables (route + corridor shown)
        s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::RoutePlanning, true);
        s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::LocalGuidance, true);
        s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::HudGuidanceCorridor, true);
        std::cerr << "[DevDockRequest] request=" << serial << " target=" << module << ':' << anchor << '\n';
    }
}

// DEV: ELITE_DEV_FREE_FLIGHT_AT=<s> toggles free flight (J) once.
void DevScriptRunner::toggleFreeFlightWhenDue(SpaceState& s)
{
    if (m_config.freeFlightAtSeconds >= 0.0 && !m_freeFlightSent && s.m_client &&
        startupElapsedMs(s.m_startupTotalBegin) / 1000.0 >= m_config.freeFlightAtSeconds)
    {
        m_freeFlightSent = true;
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::ToggleStabilization;
        msg.payload = command;
        s.m_client->sendMessage(msg);
        std::cerr << "[DevFreeFlight] toggle sent" << std::endl;
    }
}

// DEV trace for ELITE_DEV_FF_TRACE_SECONDS (3 s) after the toggle: the
// player's rendered position vs the hub
void DevScriptRunner::traceFreeFlight(SpaceState& s)
{
    const bool freeFlightSent = m_freeFlightSent;
    double& freeFlightTraceStart = m_freeFlightTraceStart;
    const double freeFlightTraceSeconds = m_config.freeFlightTraceSeconds;
    if (freeFlightSent && freeFlightTraceStart < 0.0)
        freeFlightTraceStart = startupElapsedMs(s.m_startupTotalBegin) / 1000.0;
    if (freeFlightTraceStart >= 0.0 && s.m_client &&
        startupElapsedMs(s.m_startupTotalBegin) / 1000.0 - freeFlightTraceStart < freeFlightTraceSeconds)
    {
        const auto it = s.m_client->world().ships().find(s.m_playerId.value);
        const auto& hubs = s.m_client->world().hubs();
        const auto hub = hubs.find(FreeFlightTraceHubId);
        if (it != s.m_client->world().ships().end() && hub != hubs.end())
        {
            const auto& r = it->second.renderTransform;
            const auto& a = it->second.transform;
            const glm::dvec3 hubW = world::coordinates::fullMeters(hub->second.worldPosition);
            const glm::dvec3 rr = world::coordinates::fullMeters(r.worldPosition) - hubW;
            const glm::dvec3 aa = world::coordinates::fullMeters(a.worldPosition) - hubW;
            // as DRAWN: the nearest scene object's (the station's) render position
            double drawnDist = -1.0;
            for (const auto& [oid, obj] : s.m_client->world().objects())
            {
                const double dd = glm::length(world::coordinates::fullMeters(obj.renderWorldPosition) -
                                              world::coordinates::fullMeters(r.worldPosition));
                if (drawnDist < 0.0 || dd < drawnDist)
                    drawnDist = dd;
            }
            std::cerr << "[FFTrace] drawn=" << drawnDist << " t=" << startupElapsedMs(s.m_startupTotalBegin) / 1000.0 - freeFlightTraceStart
                      << " render_rel=(" << rr.x << "," << rr.y << "," << rr.z << ") auth_rel=(" << aa.x << "," << aa.y << "," << aa.z
                      << ") ff=" << a.motion.freeFlight << " frame=" << a.motion.travelFrame.frameId
                      << " render_frame=" << r.motion.travelFrame.frameId << " local=(" << r.motion.localPositionMeters.x << ","
                      << r.motion.localPositionMeters.y << "," << r.motion.localPositionMeters.z << ")" << std::endl;
        }
    }
}

// DEV: ELITE_DEV_FREE_FLIGHT_THRUST_AT=<s> latches full free-flight thrust (HOME) once.
void DevScriptRunner::latchFreeFlightThrustWhenDue(SpaceState& s)
{
    if (m_config.freeFlightThrustAtSeconds >= 0.0 && !m_freeThrustSent && s.m_client &&
        startupElapsedMs(s.m_startupTotalBegin) / 1000.0 >= m_config.freeFlightThrustAtSeconds)
    {
        m_freeThrustSent = true;
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::SetFreeFlightThrust;
        // ELITE_DEV_FREE_FLIGHT_THRUST_CMD=2 brakes (END) instead of full thrust
        command.index = m_config.freeFlightThrustCommand;
        msg.payload = command;
        s.m_client->sendMessage(msg);
        std::cerr << "[DevFreeFlight] thrust command " << command.index << " sent" << std::endl;
    }
}

// DEV: ELITE_DEV_DRONE_VIEW_AT=<s> switches the flight view to the
// external drone camera once (style captures of the player ship).
void DevScriptRunner::openDroneViewWhenDue(SpaceState& s)
{
    if (m_config.droneViewAtSeconds >= 0.0 && !m_droneSet && s.m_client && s.context().app &&
        startupElapsedMs(s.m_startupTotalBegin) >= m_config.droneViewAtSeconds * 1000.0)
    {
        m_droneSet = true;
        std::cerr << "[FlightCamera] DEV drone view request" << '\n';
        s.context().app->requestFlightView(FlightPresentationView::Drone);
    }
}

// DEV (step 3 S4): destroys modules of the player ship once through the debug session (the same server path
// as the dev panel's "destroy module"), to see the derived compartment breaches in flight.
void DevScriptRunner::destroyModulesWhenDue(SpaceState& s)
{
    if (m_config.destroyModulesAtSeconds < 0.0 || m_modulesDestroyed || !s.m_debugSession ||
        startupElapsedMs(s.m_startupTotalBegin) < m_config.destroyModulesAtSeconds * 1000.0)
        return;
    m_modulesDestroyed = true;
    for (const auto& id : m_config.destroyModules)
    {
        std::cerr << "[DevBreach] destroy module " << id << std::endl;
        s.m_debugSession->destroyShipModule(s.m_playerId, id);
    }
}

} // namespace game::dev
