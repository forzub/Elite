// DEV scripted acceptance driver of the client (see DevScriptRunner.h).
// Moved verbatim out of SpaceState::update (audit 2026-11-06 rec. 8 / 10):
// the verbs, their order and every log line are unchanged.

#include "src/game/dev/DevScriptRunner.h"

#include "src/game/SpaceStateInternal.h"
#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"

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
    {"io", "system_0.Sol.Юпитер.Ио"}, {"europa", "system_0.Sol.Юпитер.Европа"}};
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
           startupElapsedMs(s.m_startupTotalBegin) >= steps[m_nextSolMapStep].first * 1000.0)
    {
        const std::string cmd = steps[m_nextSolMapStep++].second;
        std::cerr << "[SolMapScript] " << cmd << " (t=" << glfwGetTime() << ")\n";
        if (!runMapVerb(s, cmd) && !runRouteVerb(s, cmd) && !runObjectListVerb(s, cmd) &&
            !runShipVerb(s, cmd))
            runReportVerb(s, cmd);
    }
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
    // the map menu's jump rows through the client's own handler: "jumpui:calc|start|cancel:<system id>"
    else if (cmd.rfind("jumpui:", 0) == 0)
    {
        const std::string rest = cmd.substr(7);
        const int kind = rest.rfind("calc", 0) == 0 ? 1 : rest.rfind("start", 0) == 0 ? 2 : 3;
        const auto colon = rest.find(':');
        s.handleJumpRequest(kind, colon == std::string::npos ? -1 : std::atoi(rest.c_str() + colon + 1));
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
        target.stableObjectId = DevDockModuleId;
        target.semanticAnchorId = DevDockAnchorId;
        const auto serial = s.m_navigationWorkspace.dockingRouteRequests().request(
            target, game::navigation::DockingRouteRequest::Mode::Automatic);
        // the same modules a request from the dock card enables (route + corridor shown)
        s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::RoutePlanning, true);
        s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::LocalGuidance, true);
        s.m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::HudGuidanceCorridor, true);
        std::cerr << "[DevDockRequest] request=" << serial << " target=" << DevDockModuleId << ':' << DevDockAnchorId << '\n';
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
