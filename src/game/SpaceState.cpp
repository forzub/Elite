#include "src/core/HitchProfiler.h"
#include "src/game/navigation/rules/PadLawRules.h"
#include "src/game/navigation/rules/RingDeckRules.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/navigation/RoutePlannerSetup.h"
#include "src/audio/CockpitRadio.h"
#include "src/game/navigation/HubBerthOccupancy.h"
#include "src/game/navigation/HubPlanPads.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/server/ServerTickProfile.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/navigation/rules/JDrive.h"
#include "src/world/celestial/settlements/SurfaceSettlementLayout.h"
#include "src/game/SpaceStateInternal.h"
#include "src/game/dev/DevScriptRunner.h"
#include <algorithm>
#include <fstream>
#include <cstdio>













//                                        ##                                  ##
//                                        ##                                  ##
//   ####     ####    #####     #####    #####   ######   ##  ##    ####     #####    ####    ######
//  ##  ##   ##  ##   ##  ##   ##         ##      ##  ##  ##  ##   ##  ##     ##     ##  ##    ##  ##
//  ##       ##  ##   ##  ##    #####     ##      ##      ##  ##   ##         ##     ##  ##    ##
//  ##  ##   ##  ##   ##  ##        ##    ## ##   ##      ##  ##   ##  ##     ## ##  ##  ##    ##
//   ####     ####    ##  ##   ######      ###   ####      ######   ####       ###    ####    ####
// =====================================================================================
// Constructor
// =====================================================================================

namespace
{
// the ring dispatcher's line from its notice "pad:rim:<addr>:<g>:<m/s>:<lift g>:<need g>:<deg/s>" / "refused:rim:..."
// (RingDeckRules.h) - the same words at the request and at the hand-over
std::string ringDispatcherLine(const std::string& status)
{
    const bool refused = status.rfind("refused:", 0) == 0;
    std::vector<std::string> f;
    const std::string rest = status.substr(refused ? 12 : 8);
    for (std::size_t a = 0;;)
    {
        const auto b = rest.find(':', a);
        f.push_back(rest.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos)
            break;
        a = b + 1;
    }
    if (f.size() < 6)
        return {};
    if (refused)
        return "ДИСПЕТЧЕР КОЛЬЦА: посадка на " + f[0] + " отклонена - у площадки тяжесть " + f[1] + " g, палуба идёт " +
               f[2] + " м/с; вертикальная тяга корабля " + f[3] + " g, нужно " + f[4] +
               " g. Стыкуйтесь в осевой док (Док) - на оси палуба стоит";
    return "ДИСПЕТЧЕР КОЛЬЦА: " + f[0] + " - посадка разрешена. Кольцо вращается " + f[5] + " град/с: у площадки тяжесть " +
           f[1] + " g к ободу, палуба идёт " + f[2] + " м/с. Заход носом по вращению, держите вертикальную тягу (у вас " +
           f[3] + " g)";
}
} // namespace

SpaceState::SpaceState(
    StateStack& states,
    StartupMode startupMode
)
    : GameState(states),
      m_systemMapRenderer(m_navigationWorkspace)
{
    game::navigation::configureRoutePlannerFromEnvironment();   // the route planner's options (RoutePlannerSetup.h)
    m_runtimeScenarioSelection =
        game::debug::RuntimeScenarioSelection::fromProcessEnvironment();
    m_navigationStressHarness.configure(m_runtimeScenarioSelection);
    m_shipRuntimeControlOracle.configure(m_runtimeScenarioSelection);
    if (m_runtimeScenarioSelection.active())
    {
        std::cerr
            << "[RuntimeScenario] selected spec="
            << m_runtimeScenarioSelection.spec
            << " kind="
            << static_cast<int>(m_runtimeScenarioSelection.kind)
            << " payload=\""
            << m_runtimeScenarioSelection.payload
            << "\""
            << " nav_enabled="
            << (m_navigationStressHarness.enabled() ? 1 : 0)
            << " control_enabled="
            << (m_shipRuntimeControlOracle.enabled() ? 1 : 0)
            << "\n";
    }

    m_startupTotalBegin = ClientStartupClock::now();

    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-STARTUP][space] pid=" << startupProcessId()
            << " stage=constructor-begin"
    #ifdef _WIN32
            << " uptime_ms=" << static_cast<unsigned long long>(GetTickCount64())
    #endif
            << " foreground_pid=" << startupForegroundProcessId()
            << " thread=" << std::this_thread::get_id()
            << "\n";

    if (startupMode == StartupMode::Immediate)
    {
        while (!advanceStartupInitialization())
        {
        }
    }
}

bool SpaceState::flightVisualsSettled() const
{
    return m_sceneRenderer.celestialStats().pendingJobs == 0;
}

bool SpaceState::advanceStartupInitialization()
{
    if (m_startupStage == StartupStage::Complete)
        return true;

    const auto stageBegin = ClientStartupClock::now();

    switch (m_startupStage)
    {
        case StartupStage::InitServer:
            initServer();
            traceStartupStage(
                "init-server",
                stageBegin,
                m_startupTotalBegin
            );
            m_startupStage = StartupStage::InitClient;
            return false;

        case StartupStage::InitClient:
            initClient();
            traceStartupStage(
                "init-client",
                stageBegin,
                m_startupTotalBegin
            );
            m_startupStage = StartupStage::DebugDefaults;
            return false;

        case StartupStage::DebugDefaults:
            loadDebugControlDefaults();
            traceStartupStage(
                "debug-defaults",
                stageBegin,
                m_startupTotalBegin
            );
            m_startupStage = StartupStage::Shaders;
            return false;

        case StartupStage::Shaders:
            InitShaders();
            traceStartupStage(
                "shaders",
                stageBegin,
                m_startupTotalBegin
            );
            if (core::runtimeTraceEnabled())
                std::cerr << "[HubMotionLab][startup] shaders-ready\n";
            m_startupStage = StartupStage::SceneRenderer;
            return false;

        case StartupStage::SceneRenderer:
            try
            {
                m_sceneRenderer.initializeStaticResources();
            }
            catch (const std::bad_alloc&)
            {
                std::cerr
                    << "[HubMotionLab][bad_alloc] phase=scene-renderer-initialize\n";
                throw;
            }
            traceStartupStage(
                "scene-renderer",
                stageBegin,
                m_startupTotalBegin
            );
            if (core::runtimeTraceEnabled())
                std::cerr << "[HubMotionLab][startup] scene-renderer-ready\n";
            m_startupStage = StartupStage::SceneLocale;
            return false;

        case StartupStage::SceneLocale:
            applyClientModeState();
            traceStartupStage(
                "scene-locale",
                stageBegin,
                m_startupTotalBegin
            );
            m_startupStage = StartupStage::SystemMapRenderer;
            return false;

        case StartupStage::SystemMapRenderer:
            try
            {
                m_systemMapRenderer.init();
            }
            catch (const std::bad_alloc&)
            {
                std::cerr
                    << "[HubMotionLab][bad_alloc] phase=system-map-renderer-init\n";
                throw;
            }
            traceStartupStage(
                "system-map-renderer",
                stageBegin,
                m_startupTotalBegin
            );
            if (core::runtimeTraceEnabled())
                std::cerr
                    << "[HubMotionLab][startup] system-map-renderer-ready\n";
            m_startupStage = StartupStage::GalaxyRequest;
            return false;

        case StartupStage::GalaxyRequest:
            try
            {
                requestGalaxyMapSnapshotOnce();
            }
            catch (const std::bad_alloc&)
            {
                std::cerr
                    << "[HubMotionLab][bad_alloc] phase=galaxy-snapshot-request\n";
                throw;
            }
            traceStartupStage(
                "galaxy-request",
                stageBegin,
                m_startupTotalBegin
            );
            if (core::runtimeTraceEnabled())
                std::cerr << "[HubMotionLab][startup] galaxy-request-ready\n";
            m_startupStage = StartupStage::Hud;
            return false;

        case StartupStage::Hud:
            try
            {
                initHUD();
            }
            catch (const std::bad_alloc&)
            {
                std::cerr
                    << "[HubMotionLab][bad_alloc] phase=hud-init\n";
                throw;
            }
            traceStartupStage(
                "hud",
                stageBegin,
                m_startupTotalBegin
            );
            if (core::runtimeTraceEnabled())
                std::cerr << "[HubMotionLab][startup] hud-ready\n";
            m_startupStage = StartupStage::Finalize;
            return false;

        case StartupStage::Finalize:
            if (core::runtimeTraceEnabled())
                std::cerr
                    << "[M8E-STARTUP][space] pid=" << startupProcessId()
                    << " stage=constructor-end"
                    << " total_ms=" << startupElapsedMs(m_startupTotalBegin)
    #ifdef _WIN32
                    << " uptime_ms="
                    << static_cast<unsigned long long>(GetTickCount64())
    #endif
                    << " foreground_pid=" << startupForegroundProcessId()
                    << " thread=" << std::this_thread::get_id()
                    << "\n";

            /*
                The legacy political GalaxyDatabase is intentionally not
                loaded here. It is a separate, currently unused population/
                politics layer whose numeric system IDs do not match the
                physical StarAtlas IDs. Loading it only for startup counters
                created the false impression that it was part of the active
                server world. It can be reintroduced later behind an explicit
                server-owned world service and a stable cross-catalog key.
            */

            // InterferenceSource jammer;
            // jammer.type     = InterferenceType::Active;
            // jammer.position = {0, 0, 155};
            // jammer.power    = 300.0f;
            // jammer.radius   = 100.0f;
            // jammer.enabled  = false;

            // m_interferenceSources.push_back(jammer);

            // m_simulation->planets().clear();

            // m_planets.push_back({
            //     {0, 00, -50},
            //     20
            // });

            testDamageSystem();
            m_startupStage = StartupStage::Complete;
            return true;

        case StartupStage::Complete:
        default:
            return true;
    }
}



bool SpaceState::resolvePlayerGalacticPositionLy(
    glm::dvec3& outPositionLy
) const
{
    if (!m_client)
        return false;

    const auto& navigation = m_client->playerNavigation();
    if (navigation.currentSystemId < 0)
    {
        outPositionLy = world::coordinates::toGalacticLy(
            navigation.worldPosition
        );
        return true;
    }

    if (!m_hasGalaxyMapSnapshot)
        return false;

    outPositionLy = game::presentation::resolveGalaxyPlayerMarkerPosition(
        m_galaxyMapSnapshot,
        navigation
    ).positionLy;
    return true;
}


void SpaceState::toggleConstellationOverlay()
{
    if (!context().app)
        return;

    const bool next =
        !context().app->clientModeState().constellationsEnabled;
    (void)context().app->setConstellationsEnabled(next);
    applyClientModeState();

    std::cout
        << "[Constellations] gameplay layer "
        << (context().app->clientModeState().constellationsEnabled
                ? "enabled"
                : "disabled")
        << std::endl;
}


bool SpaceState::navigationModuleEnabled(
    game::navigation::NavigationModuleId module
) const noexcept
{
    return m_navigationWorkspace.modules().enabled(module);
}

void SpaceState::setNavigationModuleEnabled(
    game::navigation::NavigationModuleId module,
    bool enabled
) noexcept
{
    m_navigationWorkspace.modules().setEnabled(module, enabled);
}

bool SpaceState::toggleNavigationModule(
    game::navigation::NavigationModuleId module
) noexcept
{
    return m_navigationWorkspace.modules().toggle(module);
}

void SpaceState::setAllNavigationHudLayersEnabled(bool enabled) noexcept
{
    m_navigationWorkspace.modules().setAllHudLayers(enabled);
}


void SpaceState::applyClientModeState()
{
    if (!context().app)
        return;

    const auto& modes = context().app->clientModeState();

    m_sceneRenderer.setUiLocale(modes.uiLocale);
    m_sceneRenderer.setConstellationOverlayEnabled(
        modes.constellationsEnabled
    );

    if (!modes.skyCultureId.empty())
    {
        (void)m_sceneRenderer.setConstellationCultureId(
            modes.skyCultureId
        );
    }
    else
    {
        // Renderer catalog default is bootstrap data only. Adopt it once into
        // authoritative client state; subsequent changes never originate in
        // the renderer.
        const std::string initialCulture =
            m_sceneRenderer.constellationCultureId();
        if (!initialCulture.empty())
            (void)context().app->setSkyCultureId(initialCulture);
    }
}

void SpaceState::cycleSkyCulture()
{
    if (!context().app)
        return;

    const auto& modes = context().app->clientModeState();
    const std::string next =
        m_sceneRenderer.nextConstellationCultureId(
            modes.skyCultureId
        );
    if (next.empty())
        return;

    (void)context().app->setSkyCultureId(next);
    applyClientModeState();

    std::cout
        << "[Constellations] sky culture="
        << context().app->clientModeState().skyCultureId
        << " / "
        << m_sceneRenderer.constellationCultureDisplayName(
            context().app->clientModeState().uiLocale
        )
        << std::endl;
}

void SpaceState::onUiLanguageChanged()
{
    applyClientModeState();
    applyClientCatalogLocalization();
}




















//     ###                       ##                                  ##
//      ##                       ##                                  ##
//      ##    ####     #####    #####   ######   ##  ##    ####     #####    ####    ######
//   #####   ##  ##   ##         ##      ##  ##  ##  ##   ##  ##     ##     ##  ##    ##  ##
//  ##  ##   ######    #####     ##      ##      ##  ##   ##         ##     ##  ##    ##
//  ##  ##   ##            ##    ## ##   ##      ##  ##   ##  ##     ## ##  ##  ##    ##
//   ######   #####   ######      ###   ####      ######   ####       ###    ####    ####

SpaceState::~SpaceState()
{
    LOG("[SpaceState] dtor");
}






//   ##                                  ##
//                                        ##
//   ###     #####    ######   ##  ##    #####
//    ##     ##  ##    ##  ##  ##  ##     ##
//    ##     ##  ##    ##  ##  ##  ##     ##
//    ##     ##  ##    #####   ##  ##     ## ##
//   ####    ##  ##    ##       ######     ###
//                    ####
// =====================================================================================
// Frame preparation
// =====================================================================================
void SpaceState::prepareFrame(float dt)
{
    /*
        Consume authoritative client/network state before map preparation and
        before input. The server advances later in update(), so any new
        universe-timeline branch becomes visible only on the next frame as one
        coherent world/map boundary instead of halfway through a rendered
        frame.
    */
    if (m_client)
    {
        try
        {
            m_client->prepareGameplayFrame(
                static_cast<double>(std::max(0.0f, dt))
            );
            core::hitch::mark("prep_client_gameplay");
        }
        catch (const std::bad_alloc&)
        {
            std::cerr << "[HubMotionLab][bad_alloc] phase=client-prepareGameplayFrame\n";
            throw;
        }
    }

    if (m_client && m_client->hasSessionSnapshot())
    {
        const std::uint64_t revision =
            m_client->sessionSnapshot().universeTimelineRevision;

        if (m_mapUniverseTimelineRevision != 0 &&
            revision != m_mapUniverseTimelineRevision)
        {
            /*
                A debug rewind is a new universe-time branch. Keep no local
                map snapshot, transition or interpolation state from the old
                branch while fresh authoritative map data is requested.
            */
            m_galaxyMapSnapshot = {};
            m_systemMapSnapshot = {};
            m_detailMapSnapshot = {};
            m_hubMapSnapshot = {};

            m_hasGalaxyMapSnapshot = false;
            m_hasSystemMapSnapshot = false;
            m_hasDetailMapSnapshot = false;
            m_hasHubMapSnapshot = false;

            /*
                Keep the semantic target (system/detail/hub) across the fence.
                Only the data from the old timeline is invalid. Clearing the
                target itself would strand an already-open Detail/Hub view
                with no way to request its first snapshot on the new branch.
            */

            m_appliedSystemMapServerTick = 0;
            m_appliedDetailMapServerTick = 0;
            m_appliedHubMapServerTick = 0;

            m_systemMapLiveRefreshTimer = 0.0;
            m_detailMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;

                m_authoritativeMapInterpolator = {};
        }

        m_mapUniverseTimelineRevision = revision;
    }

    /*
        Resolve the map frame once before input. handleInput() and renderHUD()
        then consume the same local snapshot for the whole application frame.
        Network/server updates performed later by update() become visible on
        the next frame instead of mutating picking geometry between input and
        rendering.
    */
    updateSystemMapLiveFlags();
    updateLiveMapSnapshots(std::max(0.0f, dt));
    core::hitch::mark("prep_live_maps");
    updateLocalMapPresentationSnapshots(std::max(0.0f, dt));
    core::hitch::mark("prep_local_maps");

    applyClientCatalogLocalization();
    core::hitch::mark("prep_localization");
}


// =====================================================================================
// Input
// =====================================================================================
// The pilot takes the controls back from a virtual pilot (author 2026-11-08):
// touching the controls while an autopilot flies the ship gives a warning; if
// the pilot keeps handling them for 3 s the autopilot is switched off and its
// route / docking request is dropped. Letting go for a second before that
// cancels the takeover. A route flown by hand (guidance mode) is never dropped
// by the controls - only by removing its card on the map. The station's
// departure: beyond the dock gate the same takeover releases the ship early;
// inside the dock the station keeps it (only a notice).
void SpaceState::updatePilotTakeover(bool touching)
{
    using game::docking::traffic::DockTrafficPhase;
    // a hub city pad (author 2026-11-21): a building card's СТЫКОВКА asks the city's pad service to land the ship;
    // touching the controls on the way or on the pad gives it back to the pilot at once (as the station's departure)
    const auto sendPadLanding = [&](const std::string& frameModuleId, const std::string& address, bool land) {
        if (!m_client)
            return;
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::LandOnPad;
        command.index = land ? 1 : 0;
        command.target = frameModuleId;
        command.detail = address;
        msg.payload = command;
        m_client->sendMessage(msg);
    };
    static double padRequestedAt = -1.0e9;   // the click itself (the mouse) is not a takeover: 1.5 s grace
    // the cards' landing-place buttons read the dispatcher's state of the cities in sight
    if (m_client)
        for (const auto& [oid, object] : m_client->world().objects())
            if (object.hubAttachment.valid && game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId))
            {
                m_systemMapRenderer.setBerthOccupancy(object.hubAttachment.moduleId, object.occupiedBerths, m_playerId.value);
                auto& reg = game::navigation::HubBerthOccupancy::instance();
                reg.byFrameModule[object.hubAttachment.moduleId] = object.occupiedBerths;
                reg.playerShipId = m_playerId.value;
            }
    auto padRequest = m_systemMapRenderer.takePadLandingRequest();
    if (padRequest && m_client)
    {
        // the dispatcher at the click: all the places of that module taken -> refused here and now, the red line with
        // the wait (not after the whole route to it - author 2026-11-22: «если места все заняты - вход запрещён»)
        int places = 0, taken = 0;
        if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(padRequest->frameModuleId))
            for (const auto& m : plan->modules())
                if (m.address == padRequest->address)
                    places = m.padKind == game::navigation::HubPlanModule::PadKind::Hangar ? 0   // a hangar queues: never refused
                                                                                            : game::navigation::hubBerthPlaces(m);
        for (const auto& [oid, object] : m_client->world().objects())
            if (object.hubAttachment.valid && object.hubAttachment.moduleId == padRequest->frameModuleId)
                for (const auto& b : object.occupiedBerths)
                    if (b.compare(0, padRequest->address.size() + 1, padRequest->address + "#") == 0 &&
                        b.find("@" + std::to_string(m_playerId.value)) == std::string::npos)
                        ++taken;
        if (places > 0 && taken >= places)
        {
            m_berthWatch = {};
            m_berthWatch.active = true;
            m_berthWatch.frameModuleId = padRequest->frameModuleId;
            m_berthWatch.address = padRequest->address;
            m_berthWatch.waitMin = 20;
            m_berthWatch.since = glfwGetTime();
            std::cerr << "[PadLanding] ui: " << padRequest->address << " - all " << places
                      << " place(s) taken, refused at the click" << std::endl;
            padRequest.reset();
        }
    }
    // THE RING'S DISPATCHER answers at the request, before any route (author 2026-11-25: «эта информация должна выдаваться
    // игроку через диспетчера»): a rim pad of a turning ring - its weight, the deck's speed, the approach; too little lift
    // - refused here, the axial dock (RingDeckRules.h; the server decides the same at the hand-over)
    std::string rimNotice;
    // CITY PADS ARE CLOSED TO THE NEWTONIAN LAW (rules/PadLawRules.h): answered at the click, before any route
    if (padRequest && m_client)
        if (const auto shipIt = m_client->world().ships().find(m_playerId.value); shipIt != m_client->world().ships().end())
            if (!game::navigation::rules::cityPadOpenTo(game::ship::plant::familyOf(shipIt->second.physicalControlModel) ==
                                                        game::ship::plant::PhysicalControlFamily::Newtonian))
            {
                const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(padRequest->frameModuleId);
                m_pilotNotice = game::navigation::rules::newtonianPadLine(game::navigation::rules::newtonianPadNotice(
                    padRequest->address, plan && plan->ring().valid && plan->ring().periodSeconds > 0.0));
                std::cerr << "[PadLanding] HUD: " << m_pilotNotice << std::endl;
                padRequest.reset();
            }
    if (padRequest && m_client)
        if (const auto shipIt = m_client->world().ships().find(m_playerId.value); shipIt != m_client->world().ships().end() &&
                                                                                  shipIt->second.descriptor)
            if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(padRequest->frameModuleId))
                for (const auto& m : plan->modules())
                    if (m.address == padRequest->address && m.pad)
                    {
                        const auto& player = shipIt->second;
                        const auto slots = player.physicalControlModel != player.descriptor->physicalControlModel
                            ? game::ship::plant::healthySlotState(game::ship::plant::physicalControlSlots(player.physicalControlModel))
                            : game::ship::capability::buildPhysicalControlSlotState(*player.descriptor, player.physicalControlModel, player.modules);
                        const auto capability = game::ship::capability::buildShipCapability(*player.descriptor, player.physicalControlModel, slots);
                        if (!capability.valid)
                            break;
                        const auto caps = game::ship::telemetry::applyAuthorityToControlParams(capability.effectivePhysics,
                                                                                                 player.controlAuthorityTelemetry);
                        const auto brief = game::navigation::rules::rimPadBriefing(
                            plan->ring(), m.padPosition,
                            game::navigation::rules::rimLiftMps2(
                                caps, game::ship::plant::familyOf(player.physicalControlModel) ==
                                          game::ship::plant::PhysicalControlFamily::Newtonian));
                        if (brief.rim)
                            rimNotice = game::navigation::rules::rimPadNotice(brief, padRequest->address);
                        break;
                    }
    if (rimNotice.rfind("refused:", 0) == 0)
    {
        m_pilotNotice = ringDispatcherLine(rimNotice);
        std::cerr << "[PadLanding] HUD: " << m_pilotNotice << std::endl;
        padRequest.reset();
    }
    if (auto request = std::move(padRequest))
    {
        padRequestedAt = glfwGetTime();
        // the route to the pad is the planner's and the autopilot's, as to a dock (author 2026-11-21: «для места посадки
        // использовал бы планировщик и автопилот, а не скрипты»; the route drawn with the docking frames): the request
        // targets the pad's anchor over the city frame module; over the pad the pad service takes the ship for the descent
        // on its legs (updateDockingExecuting -> LandOnPad)
        // ... a HOP from pad to pad (the ship already over the city at its transit height, nearly at rest): the pad
        // service flies it straight across over the roofs (round the tall buildings, hubPadRoute) - the very leg every
        // pad route ends with; the planner's direct leg through that sky was "infeasible" every time (5 retries, the
        // planner looked dead - author 2026-11-22: «после 5 перелёта завис ... планировщик не работает»)
        bool hop = false;
        if (m_client)
            if (const auto shipIt = m_client->world().ships().find(m_playerId.value); shipIt != m_client->world().ships().end())
                if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(request->frameModuleId))
                    if (const auto* city = padRouteCityObject(request->frameModuleId))
                        for (const auto& m : plan->modules())
                            if (m.address == request->address && m.pad)
                            {
                                double tallest = 0.0;
                                for (const auto& b : plan->blocks())
                                    tallest = std::max(tallest, m.under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
                                const double transit = std::min(380.0, tallest + 80.0);   // the city transit level (the service climbs to a tower pad itself)
                                const glm::dvec3 q = padRouteCityLocal(*city, shipIt->second);
                                const double height = m.under ? -q.y : q.y;
                                const glm::dvec3 flat(q.x - m.padPosition.x, 0.0, q.z - m.padPosition.z);
                                const double speed = glm::length(shipIt->second.transform.motion.localVelocityMps);
                                // OFF (author 2026-11-22: «на территории хаба зелёная зона приоритетнее пустого
                                // пространства из соображений безопасности» - the hop flew over the roofs past the
                                // lanes); kept for a later use (a service hop between neighbouring pads)
                                constexpr bool PadHopEnabled = false;
                                hop = PadHopEnabled && std::abs(height - transit) < 200.0 && glm::length(flat) < 8000.0 &&
                                      speed < 15.0;
                                break;
                            }
        if (hop)
        {
            sendPadLanding(request->frameModuleId, request->address, true);
            std::cerr << "[PadLanding] hop: over the city at the transit height - the pad service flies the ship across to "
                      << request->address << std::endl;
        }
        else
        {
            game::navigation::RouteTargetRef target;
            target.kind = game::navigation::NavigationRouteAnchorKind::SemanticAnchor;
            if (m_client)
                if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
                    target.systemId = it->second.transform.motion.systemId;
            target.stableObjectId = request->frameModuleId;
            target.semanticAnchorId = "pad:" + request->address;
            m_navigationWorkspace.dockingRouteRequests().request(target, game::navigation::DockingRouteRequest::Mode::Automatic);
            m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::RoutePlanning, true);
            m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::LocalGuidance, true);
            m_navigationWorkspace.modules().setEnabled(game::navigation::NavigationModuleId::HudGuidanceCorridor, true);
        }
        m_padLandingObjectId = request->objectId;
        m_systemMapRenderer.setPadLandingActive(request->objectId);
        m_pilotNotice = rimNotice.empty() ? "СЛУЖБА ПЛОЩАДОК: заход на площадку " + request->address + " - расчёт маршрута"
                                          : ringDispatcherLine(rimNotice);
        if (!rimNotice.empty())
            std::cerr << "[PadLanding] HUD: " << m_pilotNotice << std::endl;
        std::cerr << "[PadLanding] request sent: " << request->frameModuleId << "/" << request->address << std::endl;
        // the answer you can see (author 2026-11-21: «после нажатия на кнопку посадка визуально ничего не меняется»): back to
        // the front view - the service's line in the terminal, its route tunnel ahead
        if (context().app)
            context().app->requestFlightView(FlightPresentationView::Front);
    }
    if (std::string notice = m_systemMapRenderer.takePadLandingNotice(); !notice.empty())
        m_pilotNotice = notice;
    // the landing card closed on the map: the landing is cancelled, the pilot has the ship
    if (m_systemMapRenderer.consumePadLandingCancel())
    {
        // the route to the pad (the planner's) ends too
        if (const auto& pending = m_navigationWorkspace.dockingRouteRequests().pending();
            pending.valid() && pending.target.semanticAnchorId.rfind("pad:", 0) == 0)
            m_navigationWorkspace.dockingRouteRequests().clear();
        sendPadLanding(std::string(), std::string(), false);
        m_padLandingObjectId.clear();
        m_pilotNotice = "СЛУЖБА ПЛОЩАДОК: посадка отменена (карточка закрыта) - управление у пилота";
        std::cerr << "[PadLanding] cancelled: the card was closed" << std::endl;
    }
    // the pad service's steps in the HUD terminal (author 2026-11-21: «информацию ... выводить в терминале на HUD»)
    if (m_client)
        if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
        {
            const std::string& status = it->second.padService;
            // [PadHold] probe (author 2026-11-25: «стоял ровно без дрожи»): a landed ship as DRAWN against its city as
            // drawn - the frame-to-frame step of its place and attitude in the city's frame (0 = it stands still on the pad)
            if (status.rfind("phase:3", 0) == 0)
            {
                static glm::dvec3 lastQ(0.0);
                static glm::dmat3 lastA(1.0);
                static bool have = false;
                static double windowAt = glfwGetTime(), maxStepMm = 0.0, maxTurnDeg = 0.0;
                static int frames = 0;
                const glm::dvec3 shipW = world::coordinates::fullMeters(it->second.renderTransform.worldPosition);
                for (const auto& [oid, object] : m_client->world().objects())
                {
                    if (!object.hubAttachment.valid || !game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId))
                        continue;
                    const glm::dvec3 origin = world::coordinates::fullMeters(object.renderWorldPosition);
                    if (glm::length(shipW - origin) > 9000.0)
                        continue;
                    const glm::dmat3 Rc(glm::mat3(object.renderOrientation));
                    const glm::dvec3 q = glm::transpose(Rc) * (shipW - origin);
                    const glm::dmat3 A = glm::transpose(Rc) * glm::dmat3(glm::mat3(it->second.renderTransform.orientation));
                    if (have)
                    {
                        maxStepMm = std::max(maxStepMm, 1000.0 * glm::length(q - lastQ));
                        // the turn between the frames from the skew part (acos near 1 read float noise as 0.03 deg)
                        const glm::dmat3 D = A * glm::transpose(lastA);
                        const glm::dvec3 w(D[1][2] - D[2][1], D[2][0] - D[0][2], D[0][1] - D[1][0]);
                        maxTurnDeg = std::max(maxTurnDeg, glm::degrees(std::asin(std::min(1.0, 0.5 * glm::length(w)))));
                        if (1000.0 * glm::length(q - lastQ) > 50.0)
                            std::cerr << "[PadHold] a step of " << 1000.0 * glm::length(q - lastQ) << " mm: city frame ("
                                      << lastQ.x << "," << lastQ.y << "," << lastQ.z << ") -> (" << q.x << "," << q.y << ","
                                      << q.z << ")" << std::endl;
                    }
                    lastQ = q;
                    lastA = A;
                    have = true;
                    ++frames;
                    break;
                }
                if (glfwGetTime() - windowAt >= 5.0)
                {
                    std::cerr << "[PadHold] landed, drawn against the city: max step " << maxStepMm << " mm, max turn "
                              << maxTurnDeg << " deg per frame over " << frames << " frames; city frame (" << lastQ.x << ","
                              << lastQ.y << "," << lastQ.z << "); client deck " << it->second.transform.motion.deckActive << " omega "
                              << glm::length(it->second.transform.motion.deckOmegaLocal) << " render deck "
                              << it->second.renderTransform.motion.deckActive << " rates " << it->second.transform.pitchRate << ","
                              << it->second.transform.yawRate << "," << it->second.transform.rollRate << std::endl;
                    windowAt = glfwGetTime();
                    maxStepMm = maxTurnDeg = 0.0;
                    frames = 0;
                }
            }
            // the service holds the ship though this client did not ask (a capture zone, a hand-flown touchdown): the same
            // 3 s takeover gives it back; nothing at all (no notice, no service): no takeover owner
            if (status.rfind("phase:", 0) == 0 && m_padLandingObjectId.empty())
                m_padLandingObjectId = "pad-service";
            else if (status.empty() && !m_padLandingObjectId.empty() && glfwGetTime() - padRequestedAt > 2.0)
                m_padLandingObjectId.clear();
            if (status.rfind("pad:manual:", 0) == 0)
                m_padLandingObjectId.clear();   // a hand-flown pad: the pilot flies now
            static std::string lastStatus;
            if (status != lastStatus)
            {
                lastStatus = status;
                std::string line;
                if (status == "pad:no_thrust")
                    line = "СЛУЖБА ПЛОЩАДОК: корабль не управляем (нет тяги) - служба отпускает его, корабль идёт по инерции";
                else if (status == "gear:out")
                    line = "ОПОРЫ: выпущены - касание не быстрее 2.5 м/с, от 4 м/с опоры ломаются (F4 - нижняя камера)";
                else if (status == "gear:in")
                    line = "ОПОРЫ: убраны";
                else if (status == "gear:broken")
                    line = "ОПОРЫ: сломаны - посадка невозможна до ремонта";
                else if (status == "gear:broke_hard")
                    line = "ОПОРЫ: СЛОМАНЫ при жёсткой посадке";
                else if (status == "pad:gear_up")
                    line = "СЛУЖБА ПЛОЩАДОК: выпустите опоры (G) - без них посадка не начнётся";
                else if (status == "pad:no_gear")
                    line = "ВНИМАНИЕ: посадка без опор (G)";
                else if (status.rfind("pad:capture:", 0) == 0)
                    line = "СЛУЖБА ПЛОЩАДОК: зона захвата - автоматическая посадка на " + status.substr(12);
                else if (status.rfind("pad:manual:", 0) == 0)
                    line = "СЛУЖБА ПЛОЩАДОК: площадка " + status.substr(11) + " ручная - садитесь сами, опоры выпущены?";
                else if (status.rfind("pad:landed_manual:", 0) == 0)
                    line = "ПОСАДКА: на площадке " + status.substr(18) + " - причал: торговля F8, ремонт F7, заправка Ctrl+U (руль - взлёт)";
                else if (status.rfind("pad:launched:", 0) == 0)
                    line = "ЗАПУСК с " + status.substr(13) + ": только манёвровые до выхода из зоны площадки";
                else if (status.rfind("pad:queued:", 0) == 0)
                {
                    const std::string rest = status.substr(11);
                    const auto c = rest.rfind(':');
                    line = "АНГАР " + rest.substr(0, c) + ": оба места заняты - очередь " + rest.substr(c + 1) +
                           ", ожидание перед воротами";
                }
                else if (status.rfind("pad:bay_free:", 0) == 0)
                {
                    const std::string rest = status.substr(13);
                    const auto c = rest.rfind(':');
                    line = "АНГАР " + rest.substr(0, c) + ": место " + rest.substr(c + 1) + " свободно - заход в ворота";
                }
                else if (status == "pad:engines_free")
                    line = "ЗОНА ПЛОЩАДКИ пройдена: все двигатели доступны";
                else if (status == "taken:flight")
                    line = "СЛУЖБА ПЛОЩАДОК: нормальный полёт выключен, корабль взят в кадр хаба";
                else if (status == "taken:frame")
                    line = "СЛУЖБА ПЛОЩАДОК: корабль взят в кадр хаба";
                else if (status.rfind("refused:too_big:", 0) == 0)
                {
                    // "refused:too_big:<address>:<ship m>:<place m>"
                    const std::string rest = status.substr(16);
                    const auto a = rest.find(':'), b = rest.rfind(':');
                    line = a != std::string::npos && b != std::string::npos && b > a
                               ? "ДИСПЕТЧЕР: площадка " + rest.substr(0, a) + " - корабль " + rest.substr(a + 1, b - a - 1) +
                                     " м не помещается на место " + rest.substr(b + 1) + " м, выберите другую площадку"
                               : "ДИСПЕТЧЕР: корабль не помещается на эту площадку";
                }
                else if (status.rfind("refused:taken:", 0) == 0)
                {
                    // "refused:taken:<frame>/<address>:<wait min>" - the dispatcher's red line follows (SpaceStateHud)
                    const std::string rest = status.substr(14);
                    const auto slash = rest.find('/'), colon = rest.rfind(':');
                    if (slash != std::string::npos && colon != std::string::npos && colon > slash)
                    {
                        m_berthWatch = {};
                        m_berthWatch.active = true;
                        m_berthWatch.frameModuleId = rest.substr(0, slash);
                        m_berthWatch.address = rest.substr(slash + 1, colon - slash - 1);
                        m_berthWatch.waitMin = std::atoi(rest.c_str() + colon + 1);
                        m_berthWatch.since = glfwGetTime();
                    }
                    m_padLandingObjectId.clear();
                    m_systemMapRenderer.setPadLandingActive(std::string());
                }
                else if (status.rfind("refused:newtonian:", 0) == 0)
                {
                    line = game::navigation::rules::newtonianPadLine(status);
                    m_padLandingObjectId.clear();
                    m_systemMapRenderer.setPadLandingActive(std::string());
                }
                else if (status.rfind("pad:rim:", 0) == 0 || status.rfind("refused:rim:", 0) == 0)
                {
                    line = ringDispatcherLine(status);
                    if (status[0] == 'r')
                    {
                        m_padLandingObjectId.clear();
                        m_systemMapRenderer.setPadLandingActive(std::string());
                    }
                }
                else if (status.rfind("refused:", 0) == 0)
                {
                    const std::string why = status.substr(8);
                    line = std::string("СЛУЖБА ПЛОЩАДОК: отказ - ") +
                           (why == "no_thrust" ? "корабль не управляем (нет тяги)"
                            : why == "no_pad"  ? "у здания нет площадки"
                            : why == "far"     ? "дальше 60 км от города"
                            : why == "station" ? "корабль ведёт станция"
                            : why == "no_city" ? "нет такого города"
                                               : "кадр хаба недоступен");
                    m_padLandingObjectId.clear();
                    m_systemMapRenderer.setPadLandingActive(std::string());
                }
                else if (status.rfind("phase:", 0) == 0 && status.size() > 8)
                {
                    const char phase = status[6];
                    // "phase:<n>:<frame>/<address>": the address for the pilot
                    const std::string where = status.substr(8);
                    const std::string address = where.substr(where.rfind('/') == std::string::npos ? 0 : where.rfind('/') + 1);
                    line = phase == '0'   ? "СЛУЖБА ПЛОЩАДОК: набор высоты транзита над городом"
                         : phase == '1' ? "СЛУЖБА ПЛОЩАДОК: над городом к площадке " + address
                         : phase == '2' ? "СЛУЖБА ПЛОЩАДОК: снижение на площадку " + address
                                        : "СЛУЖБА ПЛОЩАДОК: посадка выполнена, площадка " + address +
                                              " - причал: торговля F8, ремонт F7, заправка Ctrl+U (руль - взлёт)";
                }
                // a phase line comes back after each short notice: printed once (no repeats of the same step)
                static std::string lastPhaseLine;
                if (status.rfind("phase:", 0) == 0)
                {
                    if (line == lastPhaseLine)
                        line.clear();
                    else
                        lastPhaseLine = line;
                }
                if (!line.empty())
                {
                    m_pilotNotice = line;
                    std::cerr << "[PadLanding] HUD: " << line << std::endl;
                }
            }
        }
    enum class Owner { None, Docking, Route, Body, StationAxis, StationInside, Pad };
    Owner owner = Owner::None;
    const auto& pending = m_navigationWorkspace.dockingRouteRequests().pending();
    bool stationControlled = false;
    if (m_client)
        if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
        {
            const auto& dock = it->second.dockDispatcher;
            const auto phase = static_cast<DockTrafficPhase>(dock.phase);
            // a release already sent: the snapshot may still say "station" for a moment
            stationControlled = dock.active && dock.stationControlled && glfwGetTime() - m_takeoverReleaseSentSeconds > 3.0;
            if (stationControlled)
            {
                // beyond the dock gate on the departure axis?
                const double along = glm::dot(it->second.transform.motion.localPositionMeters - dock.entryLocalMeters, dock.axisOutLocal);
                const bool departing = phase == DockTrafficPhase::DepartureCleared || phase == DockTrafficPhase::Departed;
                owner = departing && along > 30.0 ? Owner::StationAxis : Owner::StationInside;
            }
        }
    if (!stationControlled)
    {
        if (m_clientRouteOwnsControl)
            owner = Owner::Route;
        else if (m_bodyAutopilotOwnsControl)
            owner = Owner::Body;
        else if (m_clientAutopilotControlActive && pending.valid() && pending.mode == game::navigation::DockingRouteRequest::Mode::Automatic)
            owner = Owner::Docking;
    }
    // the city pad service flies / holds the ship: the same 3 s takeover (author 2026-11-21: «раньше было лучше ... задержка
    // 3 сек с предупреждением»; the click on the card itself is not a touch: 1.5 s grace)
    if (!m_padLandingObjectId.empty() && glfwGetTime() - padRequestedAt > 1.5)
        owner = Owner::Pad;
    // landed on a place: the engines are locked - touching the controls does not take the ship, Ctrl+L launches it
    if (owner == Owner::Pad && m_client)
        if (const auto it = m_client->world().ships().find(m_playerId.value);
            it != m_client->world().ships().end() && it->second.padService.rfind("phase:3", 0) == 0)
        {
            m_takeoverSinceSeconds = -1.0;
            m_takeoverSecondsLeft = -1.0;
            if (touching)
                m_pilotNotice = "СТОЯНКА: двигатели заблокированы - Ctrl+L запуск манёвровых";
            return;
        }
    const double now = glfwGetTime();
    const auto reset = [&] {
        m_takeoverSinceSeconds = -1.0;
        m_takeoverSecondsLeft = -1.0;
    };
    if (owner == Owner::None)
    {
        reset();
        return;
    }
    if (owner == Owner::StationInside)
    {
        reset();
        if (touching)
            m_pilotNotice = "СТАНЦИЯ: в доке руль у станции, ждите ворот";
        return;
    }
    if (touching)
    {
        if (m_takeoverSinceSeconds < 0.0)
        {
            m_takeoverSinceSeconds = now;
            m_pilotNotice = owner == Owner::StationAxis ? "ВНИМАНИЕ: руль - станция отпустит через 3 с"
                          : owner == Owner::Pad       ? "ВНИМАНИЕ: руль - служба площадок отпустит через 3 с"
                                                      : "ВНИМАНИЕ: руль - автопилот выкл. через 3 с";
            std::cerr << "[Takeover] the pilot touches the controls - warning (3 s)" << std::endl;
        }
        m_takeoverLastInputSeconds = now;
    }
    else if (m_takeoverSinceSeconds >= 0.0 && now - m_takeoverLastInputSeconds > 1.0)
    {
        reset();
        m_pilotNotice = owner == Owner::StationAxis ? "Руль отпущен: станция ведёт дальше"
                      : owner == Owner::Pad       ? "Руль отпущен: служба площадок ведёт дальше"
                                                  : "Руль отпущен: автопилот продолжает";
        std::cerr << "[Takeover] controls released - the autopilot continues" << std::endl;
        return;
    }
    if (m_takeoverSinceSeconds < 0.0)
        return;
    // the station's departure: any control input takes over at once (author 2026-11-21); the autopilots: 3 s
    m_takeoverSecondsLeft = std::max(0.0, (owner == Owner::StationAxis ? 0.0 : 3.0) - (now - m_takeoverSinceSeconds));
    if (m_takeoverSecondsLeft > 0.0)
        return;
    reset();
    switch (owner)
    {
        case Owner::Docking:
            m_navigationWorkspace.dockingRouteRequests().clear();
            m_pilotNotice = "АВТОПИЛОТ ВЫКЛ.: стыковка прервана";
            break;
        case Owner::Route:
            cancelClientRoute();
            m_pilotNotice = "АВТОПИЛОТ ВЫКЛ.: маршрут прерван";
            break;
        case Owner::Body:
            cancelClientBodyAutopilot();
            m_pilotNotice = "АВТОПИЛОТ ВЫКЛ.: управление у пилота";
            break;
        case Owner::StationAxis:
            if (m_client)
            {
                game::network::ClientMessage msg;
                ClientShipCommand command;
                command.type = ClientShipCommand::RequestDockDeparture;
                command.index = 1;   // the pilot takes the controls during the departure
                msg.payload = command;
                m_client->sendMessage(msg);
            }
            m_pilotNotice = "СТАНЦИЯ: управление передано пилоту";
            m_takeoverReleaseSentSeconds = now;   // the release is on its way: no second takeover
            break;
        case Owner::Pad:
            sendPadLanding(std::string(), std::string(), false);
            m_padLandingObjectId.clear();
            m_systemMapRenderer.setPadLandingActive(std::string());
            m_pilotNotice = "СЛУЖБА ПЛОЩАДОК: управление передано пилоту";
            break;
        default:
            break;
    }
    std::cerr << "[Takeover] " << m_pilotNotice << std::endl;
}

void SpaceState::handleInput()
{
    // Restore the temporary manual construction preference once the
    // authoritative player ship has arrived.
    //
    // An explicit ELITE_SHIP_PHYSICAL_TEST_MODEL owns diagnostic construction
    // for the whole process. Never replay a persisted Ctrl+F9/F10 preference
    // over that server-selected model: doing so makes the stress overlay label
    // one model while the authoritative ship is physically running another.
    const char* explicitDiagnosticModel =
        std::getenv("ELITE_SHIP_PHYSICAL_TEST_MODEL");
    if (!m_persistedPhysicalModelApplied &&
        explicitDiagnosticModel &&
        *explicitDiagnosticModel)
    {
        m_persistedPhysicalModelApplied = true;
    }

    if (!m_persistedPhysicalModelApplied &&
        context().app &&
        m_client)
    {
        const auto playerIt =
            m_client->world().ships().find(m_playerId.value);
        if (playerIt != m_client->world().ships().end())
        {
            using Model =
                game::ship::plant::PhysicalControlModelId;
            Model preferred = playerIt->second.physicalControlModel;
            const std::string& saved =
                context().app->preferredPhysicalControlModel();

            if (!saved.empty() &&
                game::ship::plant::parsePhysicalControlModel(
                    saved,
                    preferred
                ) &&
                preferred != playerIt->second.physicalControlModel)
            {
                game::network::ClientMessage msg;
                ClientShipCommand command;
                command.type =
                    ClientShipCommand::SelectPhysicalControlModel;
                command.index = static_cast<int>(preferred);
                msg.payload = command;
                m_client->sendMessage(msg);
            }

            m_persistedPhysicalModelApplied = true;
        }
    }
    // TEMPORARY (author 2026-11-21): Ctrl+Alt+R - the ship fully restored. Read straight from GLFW and before every other
    // input owner: with the map or a service page open the key never reached the flight input (live 2026-11-21)
    if (GLFWwindow* w = m_client ? glfwGetCurrentContext() : nullptr)
    {
        const bool combo = (glfwGetKey(w, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) &&
                           (glfwGetKey(w, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS) &&
                           glfwGetKey(w, GLFW_KEY_R) == GLFW_PRESS;
        if (combo && !m_devRestoreComboDown)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::DevFullRestore;
            msg.payload = command;
            m_client->sendMessage(msg);
            m_pilotNotice = "DEV: корабль полностью восстановлен";
            std::cerr << "[DevRestore] Ctrl+Alt+R sent" << std::endl;
        }
        m_devRestoreComboDown = combo;
        if (combo)
            return;
    }
    // the pilot is dead: no flight controls any more (the end screen; Esc - the menu - is the application's)
    if (playerPilotLost())
        return;


    if (context().app &&
        context().app->gameUiMode() == GameUiMode::ServicePanel)
    {
        if (handleServicePageInput(context().viewport(), context().app->gameUiTarget().service))
            return;
    }

    if (context().app &&
        context().app->gameUiMode() == GameUiMode::SystemMap)
    {
        if (m_client)
        {
            const Viewport& fullVp = context().viewport();

            // The native STAR ATLAS panel owns the right-hand 28% of the
            // single OpenGL surface. Handle it before the map viewport so a
            // button/list gesture can never leak through to camera/picking.
            if (handleNativeSystemMapPanelInput(fullVp))
                return;

            const float panelRatio = 0.28f;

            Viewport mapVp = fullVp;
            mapVp.x = fullVp.x;
            mapVp.y = fullVp.y;
            mapVp.width = static_cast<int>(
                static_cast<float>(fullVp.width) * (1.0f - panelRatio)
            );
            mapVp.height = fullVp.height;



            /*
                prepareFrame() resolved the local map snapshot before input.
                Input and rendering consume that same immutable local copy.
            */

            const auto mapIntent =
                m_systemMapRenderer.handleInput(
                    mapVp,
                    m_galaxyMapSnapshot,
                    m_systemMapSnapshot,
                    m_detailMapSnapshot,
                    m_hubMapSnapshot
                );

            if (mapIntent.has_value())
            {
                using game::system_map::MapIntentType;

                if (mapIntent->type ==
                    MapIntentType::EnterKnownSystem)
                {
                    setSystemMapKnownSystemMode(
                        mapIntent->systemId
                    );

                    return;
                }

                if (mapIntent->type ==
                    MapIntentType::EnterEmptySector)
                {
                    setSystemMapEmptySectorMode(
                        mapIntent->positionLy
                    );

                    return;
                }

                if (mapIntent->type == MapIntentType::RecallRouteMap)
                {
                    if (mapIntent->requestedMapMode ==
                        game::system_map::MapMode::Galaxy)
                    {
                        setSystemMapGalaxyMode();
                    }
                    else if (mapIntent->requestedMapMode ==
                             game::system_map::MapMode::System)
                    {
                        setSystemMapLoadedSystemMode();
                    }
                    return;
                }

                if (mapIntent->type == MapIntentType::RestoreMap)
                {
                    restorePreviousSystemMapMode(*mapIntent);
                    return;
                }

                if (mapIntent->type == MapIntentType::OpenBody)
                {
                    setSystemMapDetailMode();
                    return;
                }

                if (mapIntent->type == MapIntentType::OpenHub)
                {
                    setSystemMapHubMode();
                    return;
                }
            }
        }



        if (Input::instance().isKeyPressedOnce(GLFW_KEY_P))
        {
            setSystemMapDetailMode();
            return;
        }

        // T: hub traffic routes + authored dock lanes / holds on the maps.
        if (Input::instance().isKeyPressedOnce(GLFW_KEY_T))
            m_systemMapRenderer.toggleTrafficLanes();

        // O: Orbital Transfer to the hub selected on the map (same as the context-menu row).
        if (Input::instance().isKeyPressedOnce(GLFW_KEY_O))
        {
            bool inFreeFlight = false;
            if (m_client)
            {
                const auto ship = m_client->world().ships().find(m_playerId.value);
                inFreeFlight = ship != m_client->world().ships().end() && ship->second.transform.motion.freeFlight;
            }
            const std::string body = m_systemMapRenderer.selectedBodyId();
            const std::string object = m_systemMapRenderer.activeObjectId();
            // a surface settlement: fly there and land on its pad (from a hub too - the server
            // leaves the stabilization itself)
            if (object.rfind("site:", 0) == 0 &&
                !world::celestial::settlements::surfaceSettlementBody(object.substr(5)).empty())
                requestFreeFlightAutopilot({}, object);
            else if (inFreeFlight && m_systemMapRenderer.selectedHubId().empty() && !body.empty())
                requestFreeFlightAutopilot(body);
            else
                m_systemMapRenderer.requestOrbitalTransferToSelectedHub();
        }
        if (const auto target = m_systemMapRenderer.consumeOrbitalTransferRequest())
            requestOrbitalTransfer(*target);
        // the map's body flight facts (finish choice: orbit altitude with v1, landing) - once
        static bool bodyFactsSet = false;
        if (!bodyFactsSet && m_client && m_client->starAtlas())
        {
            bodyFactsSet = true;
            m_systemMapRenderer.setBodyFlightFacts([this](const std::string& bodyId)
                -> std::optional<::SystemMapRenderer::BodyFlightFacts> {
                if (!m_client || !m_client->starAtlas() || !m_client->celestialSnapshot())
                    return std::nullopt;
                const auto* system = m_client->starAtlas()->findSystem(m_client->celestialSnapshot()->systemId);
                if (!system)
                    return std::nullopt;
                for (const auto& b : system->bodies)
                    if (b.id == bodyId)
                    {
                        ::SystemMapRenderer::BodyFlightFacts f;
                        f.muM3s2 = b.gravitationalParameterM3s2;
                        f.radiusM = b.radiusKm * 1000.0;
                        const double atmo = std::max(f.radiusM, b.atmosphereOuterRadiusKm * 1000.0);
                        f.floorAltitudeM = game::navigation::rules::floorAltitudeM(f.radiusM, atmo);
                        f.landable = f.radiusM > 0.0 && game::navigation::rules::isAirless(f.radiusM, atmo);   // else only shuttles
                        return f;
                    }
                return std::nullopt;
            });
        }
        if (const auto jump = m_systemMapRenderer.consumeJumpRequest(); jump.kind != 0)
            handleJumpRequest(jump.kind, jump.systemId, jump.bodyId);
        if (m_client)
            if (const auto self = m_client->world().ships().find(m_playerId.value); self != m_client->world().ships().end())
            {
                const auto& jm = self->second.transform.motion;
                const bool charging = jm.jumpState == 1 || jm.jumpState == 2 || jm.jumpState == 4 || jm.jumpState == 5;
                const bool inSystem = charging && jm.jumpTargetSystemId == jm.systemId;
                m_systemMapRenderer.setJumpState(m_jumpCalculatedSystemId, charging && !inSystem ? jm.jumpTargetSystemId : -1,
                                                 m_jumpCalculatedBodyId, inSystem ? m_jumpRequestedBodyId : std::string());
            }
        // a pad landing asked from a building / block card goes out at once, the map open (the flight input path that
        // also takes it does not run under the map; the request waited for the map to close - «ничего не меняется»)
        updatePilotTakeover(false);
        if (const int launch = m_systemMapRenderer.consumeRouteLaunchRequest())
            launchPlannedRoute(launch != 2, launch != 3);
        if (const int control = m_systemMapRenderer.consumeRouteControlRequest())
            sendRouteControl(control == 1);

        // G: optional cube overlay (hybrid System Map); arrows walk sibling cubes in the screen plane.
        if (Input::instance().isKeyPressedOnce(GLFW_KEY_G))
            m_systemMapRenderer.toggleSystemGridOverlay();
        // V: plan (2.5D) <-> free 3D view; N: beacon network filter
        if (Input::instance().isKeyPressedOnce(GLFW_KEY_V))
        {
            if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Hub)
                m_systemMapRenderer.toggleHubPlanView();   // the hub's 2D plan <-> 3D (author 2026-11-18)
            else
                m_systemMapRenderer.toggleSystemPlanView();
        }
        if (Input::instance().isKeyPressedOnce(GLFW_KEY_N))
            m_systemMapRenderer.toggleSystemBeacons();
        // B: the onboard computer's own marks (the journal layer); Ctrl+B is the radio
        if (!Input::instance().isKeyPressed(GLFW_KEY_LEFT_CONTROL) && !Input::instance().isKeyPressed(GLFW_KEY_RIGHT_CONTROL) &&
            Input::instance().isKeyPressedOnce(GLFW_KEY_B))
        {
            m_systemMapRenderer.setOnboardJournal(&m_onboardJournal);
            m_systemMapRenderer.toggleJournalMarks();
        }
        {
            const int dx = (Input::instance().isKeyPressedOnce(GLFW_KEY_RIGHT) ? 1 : 0) - (Input::instance().isKeyPressedOnce(GLFW_KEY_LEFT) ? 1 : 0);
            const int dy = (Input::instance().isKeyPressedOnce(GLFW_KEY_UP) ? 1 : 0) - (Input::instance().isKeyPressedOnce(GLFW_KEY_DOWN) ? 1 : 0);
            if (dx || dy)
                m_systemMapRenderer.moveSystemCubeSelection(dx, dy, glfwGetTime());
        }

        if (Input::instance().isKeyPressedOnce(GLFW_KEY_BACKSPACE))
        {
            setSystemMapCurrentSystemMode();
            return;
        }





        return;
    }

    // F1-F12 are application-level presentation selectors. SpaceState no
    // longer owns F1-F4 camera switching or the old debug F8 shortcut; this
    // keeps flight, service and navigation destinations on one deterministic
    // routing layer even while WebView2 owns keyboard focus.

// if (Input::instance().isKeyPressedOnce(GLFW_KEY_F11))
// {
//     toggleSystemMap();
//     return;
// }

// Emergency cockpit capsule ejection.
const bool leftCtrlDown =
    Input::instance().isKeyPressed(GLFW_KEY_LEFT_CONTROL);

const bool ejectPressed =
    leftCtrlDown &&
    Input::instance().isKeyPressedOnce(GLFW_KEY_Q);




if (ejectPressed)
{
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::EjectCockpitCapsule;
    msg.payload = command;

    m_client->sendMessage(msg);
    return;
}



const bool ctrlDown =
    Input::instance().isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
    Input::instance().isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

// TEMPORARY (author 2026-11-21): Ctrl+Alt+R - the ship fully restored (before Ctrl+R, the repair job)
if (ctrlDown && (Input::instance().isKeyPressed(GLFW_KEY_LEFT_ALT) || Input::instance().isKeyPressed(GLFW_KEY_RIGHT_ALT)) &&
    Input::instance().isKeyPressedOnce(GLFW_KEY_R))
{
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::DevFullRestore;
    msg.payload = command;
    m_client->sendMessage(msg);
    m_pilotNotice = "DEV: корабль полностью восстановлен";
    std::cerr << "[DevRestore] Ctrl+Alt+R sent" << std::endl;
    return;
}
if (ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_R))
{
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::StartBestRepairJob;
    msg.payload = command;

    m_client->sendMessage(msg);
    return;
}

    // Explicit propulsion lifecycle: one chord controls every installed ER-5.
    const bool altDown = Input::instance().isKeyPressed(GLFW_KEY_LEFT_ALT) ||
        Input::instance().isKeyPressed(GLFW_KEY_RIGHT_ALT);
    const bool shiftDown = Input::instance().isKeyPressed(GLFW_KEY_LEFT_SHIFT) ||
        Input::instance().isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
    if(ctrlDown && altDown && shiftDown &&
       Input::instance().isKeyPressedOnce(GLFW_KEY_I))
    {
        if(m_client)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type=ClientShipCommand::TogglePropulsionReactors;
            msg.payload=command;
            m_client->sendMessage(msg);
        }
        return;
    }

    // Explicit test fault: fail the next still-operating physical main drive
    // in the active layout. The server selects the module and applies the
    // same persistent equipment-damage law used by all installed equipment.
    if(ctrlDown && altDown && shiftDown &&
       Input::instance().isKeyPressedOnce(GLFW_KEY_F))
    {
        if(m_client)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type=ClientShipCommand::InjectEquipmentFailure;
            command.index=-1;     // next available installed main motor
            command.amount=1.0;  // complete functional failure
            msg.payload=command;
            m_client->sendMessage(msg);
        }
        return;
    }

    // Ctrl+L (LAUNCH; plain L too): docked at the hub dock -> ask the dock dispatcher for departure.
    // The server decides; the terminal shows DEPARTURE QUEUED / CLEARED; the station flies the ship
    // out along the dock axis and releases it at rest beyond the queue gates (author 2026-11-07).
    if(Input::instance().isKeyPressedOnce(GLFW_KEY_L) && m_client)
    {
        const auto it=m_client->world().ships().find(m_playerId.value);
        // ... landed on a city place: LAUNCH - off it on the manoeuvre thrusters (the main drive after its zone)
        if (it != m_client->world().ships().end() && it->second.padService.rfind("phase:3", 0) == 0)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::LandOnPad;
            command.index = 2;
            msg.payload = command;
            m_client->sendMessage(msg);
            m_pilotNotice = "ЗАПУСК: манёвровые включены - маршевые после выхода из зоны площадки";
        }
        else
        if(it!=m_client->world().ships().end() && it->second.dockDispatcher.active &&
           it->second.dockDispatcher.phase==static_cast<std::uint8_t>(
               game::docking::traffic::DockTrafficPhase::Docked))
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type=ClientShipCommand::RequestDockDeparture;
            msg.payload=command;
            m_client->sendMessage(msg);
        }
    }

    // Ctrl+U: refuel - docked at a station (or held still in a hub's frame: a tender comes alongside) fill the jump
    // charge; pressed again while it runs - stop. The server decides (GameSimulationRefuel.cpp, docs/FUEL_STATIONS.md).
    if(ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_U) && m_client)
        requestClientRefuel(-1);

    // the cockpit RADIO («магнитола», src/audio/CockpitRadio.h): Ctrl+B power, Ctrl+Shift+B playlist <-> radio,
    // Ctrl+[ / Ctrl+] previous / next, Ctrl+Shift+[ / ] volume
    if (ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_B))
        shiftDown ? audio::CockpitRadio::instance().switchSource() : audio::CockpitRadio::instance().togglePower();
    if (ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_LEFT_BRACKET))
        shiftDown ? audio::CockpitRadio::instance().volumeStep(-1) : audio::CockpitRadio::instance().previous();
    if (ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_RIGHT_BRACKET))
        shiftDown ? audio::CockpitRadio::instance().volumeStep(1) : audio::CockpitRadio::instance().next();

    // Ctrl+J: the J engine on / off (the server refuses it near objects / below the J floor).
    // Ctrl+K: stabilization on / off (off = normal flight, no speed envelope; on = the hub /
    // own orbit / ground frame with the 500 m/s manoeuvre envelope; the server picks it).
    // G: the landing legs out / in (author 2026-11-21: «выдвижные ноги-подставки ... их надо выпустить, только после этого -
    // посадка»); the server decides (a broken set stays broken)
    if (!ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_G) && m_client)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::ToggleLandingGear;
        msg.payload = command;
        m_client->sendMessage(msg);
    }
    const bool jChord = ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_J);
    const bool kChord = ctrlDown && !jChord && Input::instance().isKeyPressedOnce(GLFW_KEY_K);
    if(jChord || kChord)
    {
        if(m_client)
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type=jChord ? ClientShipCommand::ToggleJEngine : ClientShipCommand::ToggleStabilization;
            msg.payload=command;
            m_client->sendMessage(msg);
        }
        return;
    }

    // Weapons: SPACE held = lasers (the server fires every live muzzle along the
    // hull's axis), M = missile at the ship nearest the boresight, ',' / '.' =
    // next laser / missile type (DEV, weapons.json).
    if(m_client)
    {
        const bool space=Input::instance().isKeyPressed(GLFW_KEY_SPACE);
        const auto sendFire=[&](int index){
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type=ClientShipCommand::FireWeapon;
            command.index=index;
            msg.payload=command;
            m_client->sendMessage(msg);
        };
        if(space!=m_laserKeyDown)
        {
            m_laserKeyDown=space;
            sendFire(space ? 1 : 0);
        }
        if(Input::instance().isKeyPressedOnce(GLFW_KEY_M) && !ctrlDown)
            sendFire(2);
        if(Input::instance().isKeyPressedOnce(GLFW_KEY_COMMA))
            sendFire(3);
        if(Input::instance().isKeyPressedOnce(GLFW_KEY_PERIOD))
            sendFire(4);
    }

    // Free flight: HOME full thrust, END brake (flip and burn), INSERT thrust off (server-latched).
    if(m_client)
    {
        const auto it=m_client->world().ships().find(m_playerId.value);
        if(it!=m_client->world().ships().end() && it->second.transform.motion.freeFlight)
        {
            if(Input::instance().isKeyPressedOnce(GLFW_KEY_PAGE_DOWN))
            {
                // the body autopilot is the client's (NAVIGATION_V2 §1); a server-run one (a route
                // leg's landing) is cancelled on the server
                if(clientBodyAutopilotFlying())
                    cancelClientBodyAutopilot();
                else
                    requestFreeFlightAutopilot({});
            }
            int thrust=-1;
            if(Input::instance().isKeyPressedOnce(GLFW_KEY_HOME)) thrust=1;
            else if(Input::instance().isKeyPressedOnce(GLFW_KEY_END)) thrust=2;
            else if(Input::instance().isKeyPressedOnce(GLFW_KEY_INSERT)) thrust=0;
            if(thrust>=0)
            {
                game::network::ClientMessage msg;
                ClientShipCommand command;
                command.type=ClientShipCommand::SetFreeFlightThrust;
                command.index=thrust;
                msg.payload=command;
                m_client->sendMessage(msg);
            }
        }
    }

    // Backspace toggles independent hull attitude only in Assisted flight.
    // The SystemMap already handles its own Backspace before this block.
    if(Input::instance().isKeyPressedOnce(GLFW_KEY_BACKSPACE))
    {
        if(m_client)
        {
            const auto it=m_client->world().ships().find(m_playerId.value);
            if(it!=m_client->world().ships().end() &&
               it->second.transform.motion.localControlLaw ==
                   game::navigation::LocalFlightControlLaw::Assisted)
            {
                game::network::ClientMessage msg;
                ClientShipCommand command;
                command.type=ClientShipCommand::ToggleAssistedFreeRotation;
                msg.payload=command;
                m_client->sendMessage(msg);
            }
        }
        return;
    }

    // === управление кораблём ===
    // The mapper derives the requested opposite mode from the client's
    // current predicted/authoritative ship state. It must not keep a private
    // shadow copy of the flight law: a dropped/retried command or a loaded
    // game that starts in Assisted would otherwise desynchronize the chord.
    auto currentLocalControlLaw =
        game::navigation::LocalFlightControlStateMachine::defaultLaw();
    bool playerFreeFlight = false;

    if (m_client)
    {
        const auto& ships = m_client->world().ships();
        const auto playerIt = ships.find(m_playerId.value);
        if (playerIt != ships.end())
        {
            currentLocalControlLaw =
                playerIt->second.transform.motion.localControlLaw;
            playerFreeFlight = playerIt->second.transform.motion.freeFlight;
        }
    }

    if (m_shipRuntimeControlOracle.enabled() &&
             !m_shipRuntimeControlOracle.complete())
    {
        // In-game control oracle owns the exact same ordinary pilot-control
        // surface as keyboard input. No direct physics/engine calls are
        // permitted here; the purpose of this probe is to test the real
        // game runtime control -> server physics -> snapshot chain.
        m_playerControl = m_shipRuntimeControlOracle.control();
        m_client->submitInput(m_playerControl);
    }
    else
    {
        // The pilot's controls are read every control frame. Human input owns
        // the ordinary control stream only while the local virtual pilots
        // (docking, the body autopilot, the route) are inactive; their
        // submission happens from updateDockingAdvisory() / the route, so it
        // continues even while a map/UI screen returns before this block.
        // While a virtual pilot (or the station) flies, touching the controls
        // is the pilot's takeover (updatePilotTakeover, author 2026-11-08).
        ShipControlState sampled;
        m_inputMapper.update(sampled, currentLocalControlLaw, playerFreeFlight);
        if (m_devThrustUntilMs > 0.0 && startupElapsedMs(m_startupTotalBegin) < m_devThrustUntilMs)
            sampled.targetSpeedRate = 1.0f;      // DEV map script "thrust:<s>": '+' held
        if (m_devYawUntilMs > 0.0 && startupElapsedMs(m_startupTotalBegin) < m_devYawUntilMs)
            sampled.yawInput = 1.0f;             // DEV "yaw:<s>": yaw key held
        if (m_devPitchUntilMs > 0.0 && startupElapsedMs(m_startupTotalBegin) < m_devPitchUntilMs)
            sampled.pitchInput = 1.0f;           // DEV "pitch:<s>": pitch key held
        if (m_devRollUntilMs > 0.0 && startupElapsedMs(m_startupTotalBegin) < m_devRollUntilMs)
            sampled.rollInput = 1.0f;            // DEV "roll:<s>": roll key held
        const bool virtualPilot = m_clientAutopilotControlActive || m_bodyAutopilotOwnsControl || m_clientRouteOwnsControl ||
                                  m_capsuleRescue.ownsControl;
        m_manualSampledThisFrame = true;
        m_lastPilotSample = sampled;
        m_unsampledAssistSubmitted = false;
        if (!virtualPilot)
        {
            // the collision assist (by the difficulty setting): a warning, or the reflex takes the stick only when a
            // touch is otherwise unavoidable
            m_playerControl = applyManualCollisionAssist(sampled, m_client->lastSimulationMetadata().universeTimeSeconds);
            // the pilot's own flight: over a turning ring deck the law holds the deck's velocity (DeckFrame.h) - unless
            // the reflex took the stick this sample
            m_playerControl.deckRelative = !m_playerControl.cancelVelocityAlignment;
            m_client->submitInput(m_playerControl);
        }
        else
        {
            m_manualReflex.reset();
            m_manualAssistActive = false;   // NAV-10: a virtual pilot took over - no stale "assist active"
        }
        const bool touching = sampled.pitchInput != 0.0f || sampled.yawInput != 0.0f || sampled.rollInput != 0.0f ||
                              sampled.strafeInput != 0.0f || sampled.liftInput != 0.0f || sampled.forwardInput != 0.0f ||
                              sampled.targetSpeedRate != 0.0f || sampled.assistedMaxSpeedCommand ||
                              sampled.velocityAlignmentCommand != game::navigation::VelocityAlignmentMode::None;
        updatePilotTakeover(touching);
    }


}






//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####


void SpaceState::cyclePhysicalControlLayout()
{
    if (!m_client)
    {
        std::cout << "[ShipModel] Ctrl+F9 ignored reason=no-client\n";
        return;
    }

    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
    {
        std::cout
            << "[ShipModel] Ctrl+F9 ignored reason=player-not-replicated"
            << " entity=" << m_playerId.value << "\n";
        return;
    }

    using namespace game::ship::plant;
    const auto current = it->second.physicalControlModel;
    const auto next = modelFor(
        familyOf(current),
        nextLayout(layoutOf(current))
    );

    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::SelectPhysicalControlModel;
    command.index = static_cast<int>(next);
    msg.payload = command;
    m_client->sendMessage(msg);

    if (context().app)
        context().app->setPreferredPhysicalControlModel(
            physicalControlModelName(next)
        );

    std::cout
        << "[ShipModel] Ctrl+F9 requested current="
        << physicalControlModelName(current)
        << " next=" << physicalControlModelName(next)
        << "\n";
}

void SpaceState::cyclePhysicalControlFamily()
{
    if (!m_client)
    {
        std::cout << "[ShipModel] Ctrl+F10 ignored reason=no-client\n";
        return;
    }

    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
    {
        std::cout
            << "[ShipModel] Ctrl+F10 ignored reason=player-not-replicated"
            << " entity=" << m_playerId.value << "\n";
        return;
    }

    using namespace game::ship::plant;
    const auto current = it->second.physicalControlModel;
    const auto nextFamily =
        familyOf(current) == PhysicalControlFamily::Assisted
            ? PhysicalControlFamily::Newtonian
            : PhysicalControlFamily::Assisted;
    const auto next = modelFor(nextFamily, layoutOf(current));

    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::SelectPhysicalControlModel;
    command.index = static_cast<int>(next);
    msg.payload = command;
    m_client->sendMessage(msg);

    if (context().app)
        context().app->setPreferredPhysicalControlModel(
            physicalControlModelName(next)
        );

    std::cout
        << "[ShipModel] Ctrl+F10 requested current="
        << physicalControlModelName(current)
        << " next=" << physicalControlModelName(next)
        << "\n";
}


void SpaceState::setFlightCameraMode(ShipCameraMode mode)
{
    // F1-F3 leave the Navigation presentation domain immediately. Do not let
    // an unfinished map-to-map snapshot/crossfade survive behind the next
    // Flight frame, even when the selected flight layout is already active.
    m_systemMapRenderer.cancelMapTransition();

    if (m_activeCameraMode == mode)
        return;

    PlayerShipView::g_debugLogNextFrame = true;
    m_activeCameraMode = mode;
    std::cerr << "[FlightCamera] main camera mode="
              << (mode == ShipCameraMode::Cockpit ? "front" : mode == ShipCameraMode::Rear ? "rear" : mode == ShipCameraMode::Bottom ? "bottom" : "drone")
              << '\n';
}

void SpaceState::updateShipObservationInterest(float dt)
{
    if (!m_client || !m_playerView)
        return;

    m_shipObservationInterestAccumulatorSeconds +=
        std::max(0.0, static_cast<double>(dt));

    constexpr double PublishIntervalSeconds = 0.10;
    if (m_shipObservationInterestAccumulatorSeconds + 1.0e-9 <
        PublishIntervalSeconds)
    {
        return;
    }
    m_shipObservationInterestAccumulatorSeconds = 0.0;

    if (context().app &&
        context().app->sceneGameUiMode() != GameUiMode::Flight)
    {
        // Do not claim visibility from a hidden gameplay camera. The previous
        // server hint expires automatically after 350 ms.
        return;
    }

    const auto& ships = m_client->world().ships();
    const auto playerIt = ships.find(m_playerId.value);
    if (playerIt == ships.end())
        return;

    const auto& player = playerIt->second;
    const int playerSystemId = player.renderTransform.motion.systemId;
    if (playerSystemId < 0)
        return;

    Camera& camera = m_playerView->camera(m_activeCameraMode);
    const glm::mat4 cameraWorld = glm::inverse(camera.viewMatrix());
    glm::dvec3 cameraForward(
        -static_cast<double>(cameraWorld[2].x),
        -static_cast<double>(cameraWorld[2].y),
        -static_cast<double>(cameraWorld[2].z)
    );

    const double forwardLength = glm::length(cameraForward);
    if (forwardLength <= 1.0e-9)
        return;
    cameraForward /= forwardLength;

    const double verticalHalfRad =
        glm::radians(static_cast<double>(camera.fov()) * 0.5);
    const double horizontalHalfRad =
        std::atan(
            std::tan(verticalHalfRad) *
            std::max(0.1, static_cast<double>(camera.aspect()))
        );

    // A small guard band promotes before an object reaches the exact screen
    // edge, hiding LOD transition latency without granting combat authority.
    const double interestHalfAngleRad =
        std::min(
            glm::radians(89.0),
            std::max(verticalHalfRad, horizontalHalfRad) +
                glm::radians(5.0)
        );
    const double minimumAlignment = std::cos(interestHalfAngleRad);

    struct Candidate
    {
        EntityId id {0};
        game::network::ClientObservationDetail detail =
            game::network::ClientObservationDetail::Kinematic;
        double distanceMeters = 0.0;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(
        std::min<std::size_t>(
            ships.size(),
            game::network::MaxClientObservationInterestEntries * 2
        )
    );

    constexpr double KinematicDistanceMeters = 15000.0;
    constexpr double ResponseDistanceMeters = 4000.0;

    for (const auto& [entityValue, target] : ships)
    {
        if (entityValue == m_playerId.value ||
            target.renderTransform.motion.systemId != playerSystemId)
        {
            continue;
        }

        const glm::dvec3 delta =
            world::coordinates::relativeMeters(
                target.renderTransform.worldPosition,
                player.renderTransform.worldPosition
            );
        const double distanceMeters = glm::length(delta);
        if (!std::isfinite(distanceMeters) ||
            distanceMeters <= 1.0e-6 ||
            distanceMeters > KinematicDistanceMeters)
        {
            continue;
        }

        const double alignment =
            glm::dot(delta / distanceMeters, cameraForward);
        if (alignment < minimumAlignment)
            continue;

        Candidate candidate;
        candidate.id = EntityId{entityValue};
        candidate.distanceMeters = distanceMeters;
        candidate.detail =
            distanceMeters <= ResponseDistanceMeters
                ? game::network::ClientObservationDetail::Response
                : game::network::ClientObservationDetail::Kinematic;
        candidates.push_back(candidate);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const Candidate& a, const Candidate& b)
        {
            if (a.detail != b.detail)
            {
                return static_cast<std::uint8_t>(a.detail) >
                    static_cast<std::uint8_t>(b.detail);
            }
            return a.distanceMeters < b.distanceMeters;
        }
    );

    if (candidates.size() >
        game::network::MaxClientObservationInterestEntries)
    {
        candidates.resize(
            game::network::MaxClientObservationInterestEntries
        );
    }

    game::network::ClientObservationInterest interest;
    interest.serial = ++m_shipObservationInterestSerial;
    interest.entries.reserve(candidates.size());

    for (const auto& candidate : candidates)
    {
        interest.entries.push_back({
            candidate.id,
            candidate.detail
        });
    }

    game::network::ClientMessage message;
    message.clientTick = 0;
    message.payload = std::move(interest);
    m_client->sendMessage(message);
}


void SpaceState::followControlledShip()
{
    if (!m_client)
        return;
    const EntityId now = m_client->localControlledEntityId();
    if (now.value == 0 || now.value == m_playerId.value)
        return;
    const auto it = m_client->world().ships().find(now.value);
    if (it == m_client->world().ships().end() || !it->second.descriptor)
        return;   // not replicated yet: the next frame
    std::cerr << "[Control] SpaceState: the cockpit moves " << m_playerId.value << " -> " << now.value << std::endl;
    cancelClientRoute();
    cancelClientBodyAutopilot();
    m_playerId = now;
    m_playerView = std::make_unique<PlayerShipView>();
    m_playerView->init(context(), it->second.descriptor, it->second.transform);
    m_playerView->setAttachmentOverrides(&m_attachmentEditorOverrides);
    m_refitShip = now;
}

void SpaceState::update(float dt)
{
    const double updateStartMs = nowMs();
    m_perfFrameIndex++;

    const double htmlStartMs = nowMs();
    processHtmlCommands();
    m_perfProcessHtmlMs = nowMs() - htmlStartMs;
    // the cockpit UI's pointer and actions (the render only lays it out and draws)
    routeCockpitRadioUi();

    // DEV scripted acceptance steps (ELITE_DEV_SOL_MAP_SCRIPT, *_AT timers,
    // service script): src/game/dev/DevScriptRunner.
    game::dev::DevScriptRunner::instance().update(*this);
    followControlledShip();
    updateUnsampledCollisionAssist();
    updateOnboardJournal(dt);
    devCheckHubAttachedPoses(dt);
    updateCapsuleRescue(dt);
    // the hub map follows the hub the ship is in (a new hub entered -> its map; author: the hub map did not switch)
    if (m_client)
        if (const auto shipIt = m_client->world().ships().find(m_playerId.value); shipIt != m_client->world().ships().end())
        {
            // the hub the ship is AT: the nearest of its system within 25 km (motion.hubId is the frame's / dock owner's
            // hub, not where the ship is - 5 km off the ring hub it still named Высокий)
            const glm::dvec3 me = world::coordinates::fullMeters(shipIt->second.transform.worldPosition);
            const int system = shipIt->second.transform.motion.systemId;
            const std::string* nearestId = nullptr;
            const std::string* nearestBody = nullptr;
            double best = 25000.0;
            for (const auto& [id, hub] : m_client->world().hubs())
            {
                if (hub.systemId != system)
                    continue;
                const double d = glm::length(world::coordinates::fullMeters(hub.worldPosition) - me);
                if (d < best)
                {
                    best = d;
                    nearestId = &id;
                    nearestBody = &hub.parentBodyId;
                }
            }
            // hysteresis: the followed hub stays while the new one is not clearly nearer (2 km and 25 %) - the author's map
            // flipped Высокий <-> the ring hub back and forth on the way between them (40 km apart)
            if (nearestId && *nearestId != m_systemMapRenderer.followedPlayerHubId())
                if (const auto cur = m_client->world().hubs().find(m_systemMapRenderer.followedPlayerHubId());
                    cur != m_client->world().hubs().end() && cur->second.systemId == system)
                {
                    const double dCur = glm::length(world::coordinates::fullMeters(cur->second.worldPosition) - me);
                    if (dCur < 25000.0 && !(best < dCur - 2000.0 && best < 0.75 * dCur))
                        nearestId = nullptr;
                }
            if (nearestId)
                m_systemMapRenderer.followPlayerHub(*nearestId, *nearestBody);
        }



    game::session::GameSessionAdvanceResult serverAdvance;
    try
    {
        serverAdvance =
            m_session->advance(
                static_cast<double>(dt)
            );
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[HubMotionLab][bad_alloc] phase=server-advance\n";
        throw;
    }

    // ServerWorker overlaps the current client/render frame, so measuring the
    // duration of session.advance() would now report pipeline back-pressure,
    // not authoritative CPU work. Keep the existing "Fixed simulation" metric
    // attached to the server batch that actually completed.
    core::hitch::mark("session_advance");
    m_perfFixedSimMs =
        serverAdvance.serverExecutionWallSeconds * 1000.0;
    m_perfServerFixedSteps = serverAdvance.stepsExecuted;
    m_perfServerTickDebtMs =
        serverAdvance.remainingDebtSeconds * 1000.0;
    m_perfServerDiscardedMs =
        serverAdvance.discardedSeconds * 1000.0;
    m_perfServerTotalDiscardedMs =
        serverAdvance.totalDiscardedSeconds * 1000.0;
    m_perfServerCatchUpLimited =
        serverAdvance.catchUpLimited;
    // DEV (ELITE_DEV_JUMP_ENTRY_TIMING=1): the server's catch-up - a frame that ran > 2 fixed steps or left a debt
    if (static const bool entryTiming = [] {
            const char* v = std::getenv("ELITE_DEV_JUMP_ENTRY_TIMING");
            return v && *v == '1';
        }();
        entryTiming && (serverAdvance.stepsExecuted > 2 || serverAdvance.remainingDebtSeconds > 0.04))
        std::cerr << "[JumpEntryTiming] frame dt " << dt * 1000.0f << " ms: server steps " << serverAdvance.stepsExecuted
                  << ", debt left " << serverAdvance.remainingDebtSeconds * 1000.0 << " ms"
                  << (serverAdvance.catchUpLimited ? " (catch-up limited)" : "") << std::endl;

    // Debug/control commands are asynchronous requests to ServerRuntime. Flush
    // UI responses only after a newer copied diagnostic revision has crossed
    // back over that boundary. This remains correct when the server later runs
    // on a worker thread and a response takes more than one render frame.
    flushPendingDebugUiState();

    // Client prediction remains protected from a single extreme frame spike.
    // The authoritative server uses its own accumulator and no longer loses
    // ordinary fixed steps when client FPS falls below 50.
    const float clientFrameDt =
        std::clamp(dt, 0.0f, 0.05f);

    const double clientStartMs = nowMs();
    core::hitch::mark("pre_client");

    try
    {
        m_client->update(
            clientFrameDt,
            static_cast<float>(
                m_session->fixedStepSeconds()
            ),
            static_cast<double>(std::max(0.0f, dt))
        );
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[HubMotionLab][bad_alloc] phase=client-update\n";
        throw;
    }

    // One local ship computer per controlled player ship. Only new
    // replicated server ticks are sampled, not every render/prediction frame.
    // This is a COPY-only server->client read, never a new server query.
    {
        const auto& ships = m_client->world().ships();
        const auto shipIt = ships.find(m_playerId.value);
        const std::uint64_t observedTick =
            m_client->lastSimulationMetadata().serverTick;
        if (shipIt != ships.end() &&
            observedTick != 0 &&
            (
                observedTick != m_motherObservedServerTick ||
                static_cast<std::uint64_t>(shipIt->second.instanceId) !=
                    m_motherObservedShipInstanceId
            ))
        {
            auto motherInput =
                game::ship::computer::collectReplicatedShipFacts(
                    shipIt->second, observedTick
                );
            // collectReplicatedShipFacts already resolves the selected
            // per-instance installed I/Y/X descriptor and evaluates BITE
            // against exactly the current module graph. Never replace it
            // with a second native-type BITE projection in the UI layer.
#ifdef _WIN32
            // Diagnostics ONLY for the two damaged-flight acceptance runs:
            // record what ACTUALLY crossed server -> client, rather than
            // guessing from the engine-colored HUD. No extra state/cache
            // feeds this trace, MotherComputer or the real flight controller.
            if (const char* preset = std::getenv("ELITE_SHIP_DAMAGE_PRESET"))
            {
                const std::string scenario(preset);
                if (scenario == "aft-50" || scenario == "aft-failed")
                {
                    static std::uint64_t lastTraceTick = 0;
                    static std::string lastTraceScenario;
                    const bool firstSample =
                        lastTraceScenario != scenario;
                    if (firstSample ||
                        observedTick >= lastTraceTick + 50)
                    {
                        lastTraceTick = observedTick;
                        lastTraceScenario = scenario;
                        const auto& ship = shipIt->second;
                        const auto failed = std::find_if(
                            ship.modules.begin(), ship.modules.end(),
                            [](const auto& m) {
                                return m.moduleId == "ship_engine_L";
                            });
                        const bool found = failed != ship.modules.end();
                        double projected = -1.0;
                        if (ship.descriptor)
                        {
                            const auto installed =
                                game::ship::capability::
                                    replicatedInstalledAssistedDescriptor(
                                        *ship.descriptor,
                                        ship.physicalControlModel);
                            const auto& definition =
                                installed ? *installed : *ship.descriptor;
                            const auto states =
                                game::ship::capability::
                                    buildPhysicalControlSlotState(
                                        definition,
                                        ship.physicalControlModel,
                                        ship.modules);
                            if (!states.empty())
                                projected =
                                    game::ship::plant::
                                        effectiveSlotThrust01(states.front());
                        }
                        std::error_code error;
                        std::filesystem::create_directories(
                            "D:/tmp",error);
                        const std::string path =
                            "D:/tmp/elite_equipment_state_" +
                            scenario + ".csv";
                        std::ofstream trace(path,
                            firstSample ? std::ios::trunc : std::ios::app);
                        if (trace)
                        {
                            if (firstSample)
                                trace <<
                                    "tick,model,module_count,found,state,"
                                    "health,internal_failure,projected_available,"
                                    "live_available,live_thrust,bite_fault_count,"
                                    "bite_hud_attention,bite_failed,"
                                    "physical_envelope_valid,emergency_axis,"
                                    "axis_x,axis_y,axis_z\n";
                            const auto& motor =
                                ship.transform.motion.engineTelemetry;
                            trace << observedTick << ','
                                << static_cast<int>(ship.physicalControlModel)
                                << ',' << ship.modules.size() << ','
                                << (found?1:0) << ','
                                << (found?static_cast<int>(failed->state):-1)
                                << ',' << (found?failed->health:-1.0f)
                                << ','
                                << (found?failed->internalFailureDamage01:-1.0f)
                                << ',' << projected << ','
                                << (motor.valid && motor.count>0
                                    ?motor.available01[0]:-1.0) << ','
                                << (motor.valid && motor.count>0
                                    ?motor.thrust01[0]:-1.0) << ','
                                << motherInput.installedEquipmentBite.faults.size()
                                << ',' << (std::any_of(
                                    motherInput.installedEquipmentBite.faults.begin(),
                                    motherInput.installedEquipmentBite.faults.end(),
                                    [](const auto& fault){return fault.hudAttention;}
                                )?1:0)
                                << ',' << motherInput.installedEquipmentBite.summary.failedCount
                                << ',' << (motherInput.physicalEnvelopeValid?1:0)
                                << ','
                                << (motherInput.physicalEnvelopeValid &&
                                    motherInput.physicalEnvelope.
                                        assistedWorkingThrustAxisEmergency?1:0)
                                << ',' << motherInput.workingThrustAxisBody.x
                                << ',' << motherInput.workingThrustAxisBody.y
                                << ',' << motherInput.workingThrustAxisBody.z
                                << '\n';
                        }
                    }
                }
            }
#endif
            m_motherComputer.observe(std::move(motherInput));
            // The engineering/diagnostic HTML panel may refresh BEFORE
            // renderUI(); publish its first factual snapshot on this same
            // server tick, then update only interpolation during rendering.
            m_motherComputer.publishCockpit(
                game::ship::computer::collectCockpitView(
                    shipIt->second, m_motherComputer.snapshot()
                )
            );
            m_motherObservedServerTick = observedTick;
            m_motherObservedShipInstanceId =
                static_cast<std::uint64_t>(
                    shipIt->second.instanceId
                );
        }
    }

    if (m_client->hasSessionSnapshot())
    {
        m_navigationWorkspace.syncOwnedAssets(
            m_client->sessionSnapshot().ownedNavigationAssets,
            m_client->controlledShipInstanceId()
        );
    }

    const std::uint64_t navigationExecutionSnapshotTick =
        m_client->lastSimulationMetadata().serverTick;
    if (navigationExecutionSnapshotTick != 0 &&
        navigationExecutionSnapshotTick !=
            m_lastNavigationExecutionSnapshotTick)
    {
        std::vector<game::navigation::ReplicatedNavigationExecution>
            replicatedExecution;
        replicatedExecution.reserve(m_client->world().ships().size());

        for (const auto& [entityValue, shipState] :
             m_client->world().ships())
        {
            if (!shipState.navigationExecution.valid)
                continue;

            game::navigation::ReplicatedNavigationExecution entry;
            entry.entityId = EntityId{entityValue};
            entry.shipInstanceId = shipState.instanceId;
            entry.execution = shipState.navigationExecution;
            replicatedExecution.push_back(std::move(entry));
        }

        m_navigationWorkspace.syncReplicatedNavigationExecution(
            std::move(replicatedExecution)
        );
        m_lastNavigationExecutionSnapshotTick =
            navigationExecutionSnapshotTick;
    }


    // Optional real-runtime ship control oracle. Enabled only by the explicit
    // ship-control runtime scenario. It drives the exact ordinary pilot-control
    // stream in handleInput() and observes the authoritative replicated ship
    // state here. No planner, corridor or direct physics shortcut participates.
    if (m_shipRuntimeControlOracle.enabled() &&
        !m_shipRuntimeControlOracle.complete())
    {
        game::debug::ShipRuntimeControlOracle::Observation obs;
        obs.universeSeconds =
            m_client->lastSimulationMetadata().universeTimeSeconds;

        const auto playerIt =
            m_client->world().ships().find(m_playerId.value);
        if (playerIt != m_client->world().ships().end())
        {
            const auto& transform = playerIt->second.transform;
            const auto& motion = transform.motion;

            obs.worldReady =
                motion.matchedToReferenceFrame &&
                motion.travelFrame.valid;
            obs.physicalModelLabel =
                game::ship::plant::physicalControlModelName(
                    playerIt->second.physicalControlModel
                );
            obs.law = motion.localControlLaw;

            if (motion.travelFrame.valid)
            {
                const glm::dvec3 forwardLocal =
                    glm::normalize(
                        motion.travelFrame.worldToLocalVector(
                            glm::dvec3(transform.forward())
                        )
                    );
                const glm::dvec3 rightLocal =
                    glm::normalize(
                        motion.travelFrame.worldToLocalVector(
                            glm::dvec3(transform.right())
                        )
                    );
                const glm::dvec3 upLocal =
                    glm::normalize(
                        motion.travelFrame.worldToLocalVector(
                            glm::dvec3(transform.up())
                        )
                    );

                obs.forwardVelocityMps =
                    glm::dot(motion.localVelocityMps, forwardLocal);
                obs.rightVelocityMps =
                    glm::dot(motion.localVelocityMps, rightLocal);
                obs.upVelocityMps =
                    glm::dot(motion.localVelocityMps, upLocal);
                obs.speedMps =
                    glm::length(motion.localVelocityMps);

                obs.mainForwardAccelerationMps2 =
                    glm::dot(
                        motion.mainEngineAccelerationMps2,
                        forwardLocal
                    );
                obs.mainRightAccelerationMps2 =
                    glm::dot(
                        motion.mainEngineAccelerationMps2,
                        rightLocal
                    );
                obs.mainUpAccelerationMps2 =
                    glm::dot(
                        motion.mainEngineAccelerationMps2,
                        upLocal
                    );
                obs.manoeuvreRightAccelerationMps2 =
                    glm::dot(
                        motion.manoeuvreAccelerationMps2,
                        rightLocal
                    );
                obs.manoeuvreUpAccelerationMps2 =
                    glm::dot(
                        motion.manoeuvreAccelerationMps2,
                        upLocal
                    );
                obs.assistedRightAccelerationMps2 =
                    glm::dot(
                        motion.assistedStabilizationAccelerationMps2,
                        rightLocal
                    );
                obs.assistedUpAccelerationMps2 =
                    glm::dot(
                        motion.assistedStabilizationAccelerationMps2,
                        upLocal
                    );
            }

            obs.pitchRateRadPerSec = transform.pitchRate;
            obs.yawRateRadPerSec = transform.yawRate;
            obs.rollRateRadPerSec = transform.rollRate;
            obs.gimbalYawRad = motion.mainGimbalYawRad;
            obs.gimbalPitchRad = motion.mainGimbalPitchRad;
        }

        m_shipRuntimeControlOracle.update(obs);

        game::ship::plant::PhysicalControlModelId requestedModel;
        if (m_shipRuntimeControlOracle.takeModelRequest(requestedModel))
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type =
                ClientShipCommand::SelectPhysicalControlModel;
            command.index = static_cast<int>(requestedModel);
            msg.payload = command;
            m_client->sendMessage(msg);

            std::cerr
                << "[ShipControlOracle] requested model="
                << game::ship::plant::physicalControlModelName(
                       requestedModel
                   )
                << std::endl;
        }

        if (m_shipRuntimeControlOracle.complete() &&
            !m_shipControlOracleCompleteAnnounced)
        {
            m_shipControlOracleCompleteAnnounced = true;
            const bool failed = m_shipRuntimeControlOracle.failed();
            std::cerr
                << "[ShipControlOracle] "
                << (failed ? "FAILED" : "PASSED 6/6 models, 84/84 checks")
                << " report="
                << m_shipRuntimeControlOracle.reportPath()
                << std::endl;

#ifdef _WIN32
            // The scenario drives the graphical client without a console.
            // One blocking, foreground completion dialog makes its result
            // unmistakable; no new HUD system or simulation control needed.
            MessageBoxW(
                nullptr,
                failed
                    ? L"SHIP CONTROL TEST FAILED\n\n"
                      L"Automatic commands have stopped.\n"
                      L"Inspect D:\\tmp\\elite\\ship_control_oracle_matrix.log"
                    : L"SHIP CONTROL TEST PASSED\n\n"
                      L"6 / 6 ship models completed\n"
                      L"84 / 84 control checks passed\n\n"
                      L"Report: D:\\tmp\\elite\\ship_control_oracle_matrix.log",
                failed
                    ? L"Elite - control test FAILED"
                    : L"Elite - control test PASSED",
                MB_OK | MB_SETFOREGROUND | MB_TOPMOST |
                    (failed ? MB_ICONERROR : MB_ICONINFORMATION)
            );
#endif
        }
    }


    // Optional real-runtime navigation stress scenario. Enabled only with
    // ELITE_NAV_STRESS=1. It drives the same DockingRouteRequestState used by
    // UI actions and observes real player physics; no simulation shortcut is
    // introduced.
    if (m_navigationStressHarness.enabled())
    {
        // Stress runs must remain visually inspectable even though they bypass
        // the ordinary dock-card UI action that normally enables these layers.
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::RoutePlanning,
            true
        );
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::LocalGuidance,
            true
        );
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::HudGuidanceCorridor,
            true
        );

        game::navigation::debug::NavigationStressHarness::Observation obs;
        obs.universeSeconds =
            m_client->lastSimulationMetadata().universeTimeSeconds;
        obs.dockingPhase =
            static_cast<int>(m_clientDockingPhase);
        obs.autopilotActive = m_clientAutopilotControlActive;
        obs.failed = m_noSafeDockingGuidanceSolution;
        obs.failureReason = m_dockingGuidanceFailureReason;
        obs.authorityStatus = m_dockingStatusWarning;
        // Capture the exact pre-planner MotherComputer gate in the SAME
        // nav-stress SAMPLE/EVENT log. No parallel diagnostics file or
        // synthetic capability reconstruction is involved.
        const auto motherGate = m_motherComputer.snapshot();
        if(motherGate.valid)
        {
            const auto& facts=motherGate.input;
            const auto observedPlayer =
                m_client->world().ships().find(m_playerId.value);
            obs.motherboardCurrent =
                observedPlayer != m_client->world().ships().end() &&
                facts.shipInstanceId ==
                    static_cast<std::uint64_t>(
                        observedPlayer->second.instanceId) &&
                facts.serverTick ==
                    m_client->lastSimulationMetadata().serverTick;
            obs.motherboardEnvelopeValid=facts.physicalEnvelopeValid;
            obs.motherboardMotorAuthorityValid=
                facts.installedMotorAuthority.valid;
            obs.motherboardModelMatches=
                facts.installedMotorAuthority.valid &&
                static_cast<int>(facts.installedMotorAuthority.model)==
                    facts.physicalControlModel;
            obs.motherboardDoctrineValid=motherGate.flightProfile.valid;
            obs.motherboardDoctrine=
                static_cast<int>(motherGate.flightProfile.doctrine);
            obs.motherboardServerAxisEmergency=
                facts.emergencyWorkingThrustAxis;
            obs.motherboardClientAxisEmergency=
                facts.physicalEnvelopeValid &&
                facts.physicalEnvelope.assistedWorkingThrustAxisEmergency;
            if(facts.physicalEnvelopeValid)
            {
                const auto& server=facts.workingThrustAxisBody;
                const auto& client=
                    facts.physicalEnvelope.assistedWorkingThrustAxisBody;
                const double sn=glm::length(server);
                const double cn=glm::length(client);
                if(std::isfinite(sn) && std::isfinite(cn) &&
                   sn>1.0e-9 && cn>1.0e-9)
                    obs.motherboardAxisAgreement=
                        glm::dot(server/sn,client/cn);
            }
            const auto& motors=facts.installedMotorAuthority;
            obs.motherboardHullForwardMps2=motors.negative[2];
            obs.motherboardWorkingForwardMps2=
                motors.workingForwardAccelerationMps2;
            obs.motherboardSteeringMps2=std::min({
                motors.positive[3],motors.negative[3],
                motors.positive[4],motors.negative[4]});
            obs.motherboardLateralMps2=std::min({
                motors.positive[0],motors.negative[0],
                motors.positive[1],motors.negative[1]});
            obs.motherboardReverseMps2=motors.positive[2];
            if(obs.requestPending || m_navigationWorkspace.dockingRouteRequests().pending().valid())
            {
                if(!obs.motherboardCurrent)
                    obs.authorityStatus="waiting for current ship/tick";
                else if(!obs.motherboardEnvelopeValid)
                    obs.authorityStatus="missing installed physics envelope";
                else if(!obs.motherboardMotorAuthorityValid)
                    obs.authorityStatus="missing installed motor basis";
                else if(!obs.motherboardModelMatches)
                    obs.authorityStatus="installed motor basis model mismatch";
                else if(!obs.motherboardDoctrineValid)
                    obs.authorityStatus="rejected physical flight doctrine";
                else if(obs.authorityStatus.empty())
                    obs.authorityStatus="authority ready";
            }
        }
        else if(m_navigationWorkspace.dockingRouteRequests().pending().valid())
            obs.authorityStatus="motherboard has no ship snapshot";
        obs.workerCount =
            m_dockWorkerCount
                ? m_dockWorkerCount->load(std::memory_order_acquire)
                : 0;

        if (m_dockAdviceJob)
        {
            obs.jobSerial = m_dockAdviceJob->context.serial;
            obs.jobReady = m_dockAdviceJob->ready.load(
                std::memory_order_acquire
            );
            obs.jobCancelled = m_dockAdviceJob->cancelled.load(
                std::memory_order_acquire
            );
            obs.jobPlanValid = m_dockAdviceJob->ready.load(
                std::memory_order_acquire
            ) && m_dockAdviceJob->plan.valid();
            obs.jobStage = static_cast<int>(
                m_dockAdviceJob->planningProgressStage.load(
                    std::memory_order_acquire
                )
            );
            obs.jobPermille =
                m_dockAdviceJob->planningProgressPermille.load(
                    std::memory_order_acquire
                );

            // ready acquire publishes the worker's plan/result strings.
            if (obs.jobReady)
            {
                if (!m_dockAdviceJob->plan.failure.empty())
                {
                    obs.jobResult = m_dockAdviceJob->plan.failure;
                    if (!m_dockAdviceJob->plan.diagnosticSummary.empty())
                    {
                        obs.jobResult += " | ";
                        obs.jobResult +=
                            m_dockAdviceJob->plan.diagnosticSummary;
                    }
                }
                else
                {
                    obs.jobResult =
                        m_dockAdviceJob->plan.diagnosticSummary;
                }
            }
        }

        obs.safetyCourseActive =
            m_dockAssistedSafeRecovery.active ||
            m_dockSafeCourse.active;
        obs.safetyStopFallback =
            (
                m_dockAssistedSafeRecovery.active &&
                m_dockAssistedSafeRecovery.stopFallback
            ) ||
            (
                m_dockSafeCourse.active &&
                m_dockSafeCourse.stopFallback
            );

        if (m_dockAssistedSafeRecovery.active)
        {
            const auto stressPlayerIt =
                m_client->world().ships().find(m_playerId.value);

            if (stressPlayerIt != m_client->world().ships().end())
            {
                const auto& stressTransform =
                    stressPlayerIt->second.transform;
                const auto& stressMotion =
                    stressTransform.motion;

                const double safeSpeed =
                    glm::length(stressMotion.localVelocityMps);
                const double selectedLength =
                    glm::length(m_dockAssistedSafeRecovery.direction);

                if (safeSpeed > 1.0e-9 &&
                    selectedLength > 1.0e-9 &&
                    stressMotion.travelFrame.valid)
                {
                    const glm::dvec3 currentCourse =
                        stressMotion.localVelocityMps / safeSpeed;
                    const glm::dvec3 selectedCourse =
                        m_dockAssistedSafeRecovery.direction /
                        selectedLength;

                    obs.safeCourseErrorRad =
                        std::acos(
                            std::clamp(
                                glm::dot(currentCourse, selectedCourse),
                                -1.0,
                                1.0
                            )
                        );

                    constexpr double MaxLeadRad =
                        8.0 * 3.14159265358979323846 / 180.0;
                    const double step =
                        std::min(obs.safeCourseErrorRad, MaxLeadRad);

                    glm::dvec3 intermediate = selectedCourse;
                    if (obs.safeCourseErrorRad > 1.0e-9)
                    {
                        glm::dvec3 axis =
                            glm::cross(currentCourse, selectedCourse);
                        const double axisLength = glm::length(axis);
                        if (axisLength > 1.0e-9)
                        {
                            axis /= axisLength;
                            intermediate =
                                currentCourse * std::cos(step) +
                                glm::cross(axis, currentCourse) *
                                    std::sin(step) +
                                axis * glm::dot(axis, currentCourse) *
                                    (1.0 - std::cos(step));
                        }
                    }

                    const glm::dvec3 hullForward =
                        glm::normalize(
                            stressMotion.travelFrame.worldToLocalVector(
                                glm::dvec3(stressTransform.forward())
                            )
                        );

                    obs.safeCourseHullTargetErrorRad =
                        std::acos(
                            std::clamp(
                                glm::dot(
                                    hullForward,
                                    glm::normalize(intermediate)
                                ),
                                -1.0,
                                1.0
                            )
                        );
                }
            }
        }

        const auto& stressRequests =
            m_navigationWorkspace.dockingRouteRequests();
        obs.requestPending = stressRequests.pending().valid();
        obs.requestSerial = stressRequests.pending().serial;
        if (obs.requestPending)
        {
            obs.requestTarget =
                stressRequests.pending().target.stableObjectId +
                ":" +
                stressRequests.pending().target.semanticAnchorId;
        }

        // Stress diagnostics read the state that actually owns execution.
        // Do not re-derive a flight law here: the two autopilots are isolated
        // and at most one of them may be active at a time.
        const bool observeAssisted =
            m_clientAssistedRouteAutopilot.active ||
            !m_clientNewtonianRouteAutopilot.active;
        obs.pageIndex =
            observeAssisted
                ? m_clientAssistedRouteAutopilot.currentPage
                : m_clientNewtonianRouteAutopilot.currentPage;
        obs.segmentIndex =
            observeAssisted
                ? m_clientAssistedRouteAutopilot.currentContinuousSegment
                : m_clientNewtonianRouteAutopilot.currentContinuousSegment;
        obs.checkpointIndex =
            observeAssisted
                ? m_clientAssistedRouteAutopilot.nextCheckpointIndex
                : m_clientNewtonianRouteAutopilot.nextCheckpointIndex;
        obs.routeCurveIndex =
            m_clientDockingRouteCurveIndex;
        obs.routeProgressMeters =
            m_clientDockingRouteProgressMeters;
        obs.speedProfileRevision =
            observeAssisted
                ? m_clientAssistedRouteAutopilot.speedProfileRevision
                : m_clientNewtonianRouteAutopilot.speedProfileRevision;
        obs.crossTrackMeters =
            m_clientAutopilotCrossTrackMeters;
        obs.courseErrorRad =
            m_clientAutopilotCourseErrorRad;
        obs.hullErrorRad =
            m_clientAutopilotHullErrorRad;
        obs.targetSpeedMps =
            m_clientAutopilotTargetSpeedMps;
        obs.targetLongitudinalAccelerationMps2 =
            m_clientAutopilotTargetLongitudinalAccelerationMps2;
        obs.mainCommand01 =
            static_cast<double>(m_clientAutopilotControl.targetSpeedRate);
        obs.pitchCommand =
            static_cast<double>(m_clientAutopilotControl.pitchInput);
        obs.yawCommand =
            static_cast<double>(m_clientAutopilotControl.yawInput);
        obs.rollCommand =
            static_cast<double>(m_clientAutopilotControl.rollInput);

        const auto stressPlayerIt =
            m_client->world().ships().find(m_playerId.value);
        if (stressPlayerIt != m_client->world().ships().end())
        {
            const auto& stressTransform = stressPlayerIt->second.transform;
            obs.worldReady =
                stressTransform.motion.matchedToReferenceFrame &&
                !stressTransform.motion.hubId.empty();
            obs.actualPhysicalModelLabel =
                game::ship::plant::physicalControlModelName(
                    stressPlayerIt->second.physicalControlModel
                );
            obs.actualControlLawLabel =
                game::navigation::localFlightControlLawName(
                    stressTransform.motion.localControlLaw
                );

            const auto stressSlotState =
                stressPlayerIt->second.physicalControlModel !=
                        stressPlayerIt->second.descriptor->physicalControlModel
                    ? game::ship::plant::healthySlotState(
                          game::ship::plant::physicalControlSlots(
                              stressPlayerIt->second.physicalControlModel
                          )
                      )
                    : game::ship::capability::buildPhysicalControlSlotState(
                          *stressPlayerIt->second.descriptor,
                          stressPlayerIt->second.physicalControlModel,
                          stressPlayerIt->second.modules
                      );

            const auto stressCapability =
                game::ship::capability::buildShipCapability(
                    *stressPlayerIt->second.descriptor,
                    stressPlayerIt->second.physicalControlModel,
                    stressSlotState
                );

            obs.actualCapabilityValid = stressCapability.valid;
            obs.actualPropulsionSlotCount = stressSlotState.size();
            if (stressCapability.valid)
            {
                obs.actualForwardMainAvailable =
                    stressCapability.effectivePhysics.
                        forwardMainEngineAvailable;
                obs.actualReverseMainAvailable =
                    stressCapability.effectivePhysics.
                        reverseMainEngineAvailable;
            }

            obs.speedMps =
                glm::length(stressTransform.motion.localVelocityMps);
            obs.angularSpeedRadPerSec =
                std::sqrt(
                    stressTransform.pitchRate * stressTransform.pitchRate +
                    stressTransform.yawRate * stressTransform.yawRate +
                    stressTransform.rollRate * stressTransform.rollRate
                );
            obs.pitchRateRadPerSec = stressTransform.pitchRate;
            obs.yawRateRadPerSec = stressTransform.yawRate;
            obs.rollRateRadPerSec = stressTransform.rollRate;

            if (stressTransform.motion.travelFrame.valid &&
                obs.speedMps > 0.5)
            {
                const glm::dvec3 velocityWorld =
                    stressTransform.motion.travelFrame.localToWorldVector(
                        stressTransform.motion.localVelocityMps
                    );
                const double velocityLength =
                    glm::length(velocityWorld);
                const glm::dvec3 hullForward =
                    glm::normalize(
                        glm::dvec3(stressTransform.forward())
                    );

                if (velocityLength > 1.0e-9)
                {
                    obs.hullToVelocityErrorRad =
                        std::acos(
                            std::clamp(
                                glm::dot(
                                    hullForward,
                                    velocityWorld / velocityLength
                                ),
                                -1.0,
                                1.0
                            )
                        );
                }
            }
            obs.mainAccelerationMps2 =
                glm::length(
                    stressTransform.motion.mainEngineAccelerationMps2
                );
            obs.manoeuvreAccelerationMps2 =
                glm::length(
                    stressTransform.motion.manoeuvreAccelerationMps2
                );
            obs.gimbalRightCommand01 =
                stressTransform.motion.mainGimbalRightCommand01;
            obs.gimbalUpCommand01 =
                stressTransform.motion.mainGimbalUpCommand01;
            obs.gimbalYawRad =
                stressTransform.motion.mainGimbalYawRad;
            obs.gimbalPitchRad =
                stressTransform.motion.mainGimbalPitchRad;
            obs.mainTorquePitchNm =
                stressTransform.motion.mainPropulsionTorqueBodyNm.x;
            obs.mainTorqueYawNm =
                stressTransform.motion.mainPropulsionTorqueBodyNm.y;
            obs.mainTorqueRollNm =
                stressTransform.motion.mainPropulsionTorqueBodyNm.z;
            obs.mainAngularAccelPitchRadPerSec2 =
                stressTransform.motion.
                    mainPropulsionAngularAccelerationBodyRadPerSec2.x;
            obs.mainAngularAccelYawRadPerSec2 =
                stressTransform.motion.
                    mainPropulsionAngularAccelerationBodyRadPerSec2.y;
            obs.mainAngularAccelRollRadPerSec2 =
                stressTransform.motion.
                    mainPropulsionAngularAccelerationBodyRadPerSec2.z;
        }

        obs.routeValid = m_dockAdvice.plan.valid();

        for (const auto& stage : m_dockAdvice.plan.stages)
        {
            if (stage.kind !=
                    game::navigation::planner::
                        RouteStageKind::VolumeTransit ||
                stage.volumeId.empty())
            {
                continue;
            }

            obs.blueStagePresent = true;
            obs.blueHardContainment =
                stage.hardContainment;
            obs.blueStartProgressMeters =
                stage.startProgressMeters;
            obs.blueEndProgressMeters =
                stage.endProgressMeters;
            obs.blueVolumeId = stage.volumeId;

            if (m_clientDockingRouteProgressMeters + 1.0 <
                stage.startProgressMeters)
            {
                obs.bluePhase = 1;
            }
            else if (m_clientDockingRouteProgressMeters <=
                stage.endProgressMeters + 1.0)
            {
                obs.bluePhase = 2;
            }
            else
            {
                obs.bluePhase = 3;
            }
            break;
        }

        obs.routeCurveCount = m_dockAdvice.plan.routeCurves.size();
        obs.executionGateCount =
            m_dockAdvice.plan.executionGates.size();
        obs.routeDiagnostic =
            m_dockAdvice.plan.diagnosticSummary;

        if (!m_dockAdvice.plan.routeCurves.empty())
        {
            const auto& first =
                m_dockAdvice.plan.routeCurves.front();
            const auto& last =
                m_dockAdvice.plan.routeCurves.back();
            obs.firstCurveKind =
                static_cast<int>(first.kind);
            obs.firstCurveLengthMeters =
                std::max(
                    0.0,
                    first.endProgressMeters -
                    first.startProgressMeters
                );
            obs.routeLengthMeters =
                std::max(0.0, last.endProgressMeters);
        }

        const auto action =
            m_navigationStressHarness.update(obs);

        if (action.clear)
        {
            m_navigationWorkspace
                .dockingRouteRequests()
                .clear();
        }

        if (action.request)
        {
            game::navigation::RouteTargetRef target;
            target.kind =
                game::navigation::NavigationRouteAnchorKind::
                    SemanticAnchor;
            target.systemId =
                stressPlayerIt != m_client->world().ships().end()
                    ? stressPlayerIt->second.transform.motion.systemId
                    : 0;

            if (action.target ==
                game::navigation::debug::
                    NavigationStressHarness::Target::CylinderRear)
            {
                target.stableObjectId =
                    "guidance_dock_cylinder_b";
                target.semanticAnchorId =
                    "dock_gate_rear";
            }
            else
            {
                target.stableObjectId =
                    "hub_hex_dock_a";
                target.semanticAnchorId =
                    "dock_gate_front";
            }

            const auto serial =
                m_navigationWorkspace
                    .dockingRouteRequests()
                    .request(
                        target,
                        game::navigation::DockingRouteRequest::
                            Mode::Automatic
                    );

            std::cerr
                << "[NavStress] request=" << serial
                << " target=" << target.stableObjectId
                << ":" << target.semanticAnchorId
                << " report="
                << m_navigationStressHarness.reportPath()
                << std::endl;
        }

        if (m_navigationStressHarness.complete() &&
            !m_navigationStressCompleteAnnounced)
        {
            m_navigationStressCompleteAnnounced = true;
            std::cerr
                << "[NavStress] COMPLETE report="
                << m_navigationStressHarness.reportPath()
                << std::endl;
        }
    }

    core::hitch::mark("client_update_and_navigation");
    const double dockingGuidanceStartMs = nowMs();

    const bool directControlScenario =
        m_runtimeScenarioSelection.kind ==
            game::debug::RuntimeScenarioKind::ShipControl;

    if (directControlScenario)
    {
        // Runtime control scenarios exclusively own ShipControlState. A stale
        // docking request/autopilot must never submit a second sample later in
        // the same frame and overwrite the scenario command.
        m_navigationWorkspace.dockingRouteRequests().clear();
        m_clientAutopilotControlActive = false;
        m_clientAutopilotControl = ShipControlState {};
    }
    else
    {
        arbitrateFlightTask();
        updateDockingAdvisory();
        updateClientRoute();
        updateClientBodyAutopilot();
        {
            static double lastT = glfwGetTime();
            const double now = glfwGetTime();
            updateFormationTask(std::clamp(now - lastT, 0.0, 0.1));
            lastT = now;
        }
    }
    updateZoneViolations(dt);

    m_perfDockingGuidanceMs = nowMs() - dockingGuidanceStartMs;
    core::hitch::mark("docking_guidance");
    m_perfDockingRequestActive =
        m_navigationWorkspace.dockingRouteRequests().pending().valid();

    if constexpr (game::promo::PromoSceneScenario::Enabled)
    {
        if (isPromo1SceneMode())
        {
            m_promoSceneScenario.setup(m_client->world());
            m_promoSceneScenario.update(
                m_client->world(),
                dt
            );
        }
    }

    // No client-side decorative traffic: every ship the player sees is a
    // replicated server entity (hub dock traffic = DockServiceTraffic shells).

    m_perfClientUpdateMs = nowMs() - clientStartMs;
    core::hitch::mark("client_tail");


// Promo ships are presentation-only visual entities.
// The replicated player ship is never repositioned or reoriented on the client.
const auto& ships = m_client->world().ships();

auto it = ships.find(m_playerId.value);
if (it == ships.end())
    return;

const auto& ship = it->second;

const double playerViewStartMs = nowMs();

const float viewDt =
    std::min(dt, 0.02f); // ограничиваем визуальный dt

m_playerView->update(
    viewDt,
    ship.role,
    ship.renderTransform,
    ship.detachedFragments
);

updateShipObservationInterest(viewDt);









m_playerView->updateCockpitStateFromSnapshot(
    static_cast<float>(ship.transform.forwardSpeedMps()),
    static_cast<float>(ship.transform.commandedForwardSpeedMps()),
    static_cast<float>(ship.transform.motion.manoeuvreGasPressure01),
    ship.transform.cruiseActive,
    game::runtime::WorldSignalLabelsEnabled
        ? ship.signalPresentation.labelsVector()
        : std::vector<WorldLabel>{}
);

    m_perfPlayerViewMs = nowMs() - playerViewStartMs;
    core::hitch::mark("player_view");


    // --------------------------------------------------
    // DEBUG: Обрабатываем отладочные команды из очереди
    // --------------------------------------------------
    {
        std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
        for (const auto& cmd : m_debugCommands)
        {

            switch (cmd.type)
            {
                case ClientShipCommand::DamageRadiator:
                {
                    ClientShipCommand ship_cmd;
                    ship_cmd.type = ClientShipCommand::DamageRadiator;
                    ship_cmd.index = cmd.index;
                    ship_cmd.amount = cmd.amount;

                    game::network::ClientMessage  msg;
                    msg.clientTick = 0;
                    msg.payload = ship_cmd;
                    m_client->sendMessage(msg);

                    break;
                }
                case ClientShipCommand::RepairAllPanels:
                {
                    ClientShipCommand ship_cmd;
                    ship_cmd.type = ClientShipCommand::RepairAllPanels;

                    game::network::ClientMessage  msg;
                    msg.clientTick = 0;
                    msg.payload = ship_cmd;
                    m_client->sendMessage(msg);
                    break;
                }
                // ... другие команды
            }
        }
        m_debugCommands.clear();
    }
    // --------------------------------------------------



    // const auto& o = ship.renderTransform.orientation;


    // ========= обновление радара ====================
    if constexpr (game::runtime::RadarHudEnabled)
    {
        if (m_radarWidget)
        {
            // the scope belongs to the pilot's panel: the front view only (author 2026-11-21: the camera views without it)
            m_radarWidget->visible = m_activeCameraMode == ShipCameraMode::Cockpit;
            m_radarWidget->setPlayerTransform(
                world::coordinates::legacyFloatMeters(
                    ship.renderTransform.worldPosition
                ),
                ship.renderTransform.orientation
            );

            // Tracks of the player's installed radar: the server sensor unit
            // publishes them session-private (sensor-local metres, the frame
            // the scope expects). A destroyed / unpowered radar shows nothing.
            const auto& sensors = m_client->navigationSensors();
            const bool radarOn = sensors.radarInstalled && sensors.radarOperational;
            // Scope modes: around the ship at 5 / 20 / 50 / 150 km (R cycles);
            // in J (free / global flight) a forward sector along the hull axis
            // out to 2 000 km - early warning of what lies on the course.
            static constexpr double ScopeRangesMeters[] = {5000.0, 20000.0, 50000.0, 150000.0};   // 5 km: the close hub traffic (author 2026-11-24)
            constexpr double ForwardRangeMeters = 2.0e6;
            constexpr double ForwardHalfAngleCos = 0.866;   // 30 deg half-angle
            const bool ctrlHeld =
                Input::instance().isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                Input::instance().isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            if (!ctrlHeld && Input::instance().isKeyPressedOnce(GLFW_KEY_R))
                m_radarRangeIndex = (m_radarRangeIndex + 1) % 4;
            const bool forwardScope = ship.transform.motion.freeFlight && ship.transform.motion.jEngine;   // the J sector only with the J engine on
            m_radarDisplayRangeMeters = forwardScope ? ForwardRangeMeters : ScopeRangesMeters[m_radarRangeIndex];

            std::vector<RadarContactView> views;
            if (radarOn && sensors.hasRadarScan)
            {
                views.reserve(sensors.latestRadarScan.tracks.size());
                for (const auto& t : sensors.latestRadarScan.tracks)
                {
                    if (forwardScope)
                    {
                        // sensor frame: -Z is the nose
                        const double r = std::max(1.0, t.rangeMeters);
                        if (t.rangeMeters > ForwardRangeMeters || -t.relativePositionMeters.z / r < ForwardHalfAngleCos)
                            continue;
                    }
                    views.push_back({
                        EntityId {static_cast<uint32_t>(t.trackId.value)},
                        glm::vec3(t.relativePositionMeters),
                        static_cast<int>(t.kind)
                    });
                }
            }
            // the interstellar jump blinds the scope (author 2026-11-08): from 3 s before the charge
            // completes, while charged and waiting, and 3 s after the jump - noise, no contacts (the
            // previous system's ships never show in the new one)
            {
                const auto& m = ship.transform.motion;
                const double sinceJump = glfwGetTime() - m_jumpFxStart;
                float jam = 0.0f;
                if (m.jumpState == 2 || m.jumpState == 3 || (sinceJump >= 0.0 && sinceJump < 3.0))
                    jam = 1.0f;
                else if (m.jumpState == 1 && m.jumpChargeDurationS - m.jumpChargeElapsedS < 3.0)
                    jam = static_cast<float>(std::clamp(1.0 - (m.jumpChargeDurationS - m.jumpChargeElapsedS) / 3.0, 0.0, 1.0));
                if (jam > 0.3f)
                    views.clear();
                m_radarWidget->setJamLevel(jam);
            }
            m_radarWidget->configureFromDesc(
                m_radarSweepDegPerSec,
                static_cast<float>(m_radarDisplayRangeMeters)
            );
            m_radarWidget->setContacts(views);

            // DEV: ELITE_DEV_RADAR_LOG=1 prints the scan (range / kind / bearing) every 5 s
            static const bool radarLog = [] {
                const char* v = std::getenv("ELITE_DEV_RADAR_LOG");
                return v && *v == '1';
            }();
            static double radarLogTimer = 0.0;
            radarLogTimer += dt;
            if (radarLog && radarLogTimer >= 5.0 && sensors.hasRadarScan)
            {
                radarLogTimer = 0.0;
                std::cerr << "[RadarScope] on=" << radarOn << " scale_m=" << m_radarDisplayRangeMeters
                          << " tracks=" << sensors.latestRadarScan.tracks.size() << '\n';
                for (const auto& t : sensors.latestRadarScan.tracks)
                    std::cerr << "  track=" << t.trackId.value << " kind=" << int(t.kind) << " range_m=" << t.rangeMeters
                              << " rel=(" << t.relativePositionMeters.x << "," << t.relativePositionMeters.y << ","
                              << t.relativePositionMeters.z << ")" << '\n';
            }

            if (auto* label = dynamic_cast<UIText*>(uiRoot->findById("radar_range")))
            {
                char text[96];
                if (!sensors.radarInstalled)
                    std::snprintf(text, sizeof text, "RADAR: NOT INSTALLED");
                else if (!radarOn)
                    std::snprintf(text, sizeof text, "RADAR: OFF (no power / damaged)");
                else
                    std::snprintf(text, sizeof text, forwardScope ? "RADAR FWD %.0f km  |  %zu tracks" : "RADAR %.0f km  |  %zu tracks  [R]",
                                  m_radarDisplayRangeMeters / 1000.0, views.size());
                label->label = text;
                label->visible = m_activeCameraMode == ShipCameraMode::Cockpit;   // with the scope: the front view only
            }
        }
    }

    // ========= обновление UI ====================
    const double uiRootStartMs = nowMs();
    uiRoot->update(dt);
    m_perfUiRootUpdateMs = nowMs() - uiRootStartMs;
    core::hitch::mark("ui_root");


    // Browser diagnostics request their own live snapshots. They must not
    // compete for the single in-game activePanel slot.

    m_perfFrameMs = static_cast<double>(dt) * 1000.0;

    if (dt > 0.00001f)
        m_perfFps = 1.0 / static_cast<double>(dt);

    m_perfPushTimer += dt;

    if (m_perfPushTimer >= 0.25f)
    {
        m_perfPushTimer = 0.0f;

        if (context().app &&
            context().app->gameUiMode() == GameUiMode::SystemMap)
        {
        }
    }



    m_perfUpdateMs = nowMs() - updateStartMs;
    core::hitch::mark("update_tail");

}






//                                ###
//                                 ##
//  ######    ####    #####        ##    ####    ######
//   ##  ##  ##  ##   ##  ##    #####   ##  ##    ##  ##
//   ##      ######   ##  ##   ##  ##   ######    ##
//   ##      ##       ##  ##   ##  ##   ##        ##
//  ####      #####   ##  ##    ######   #####   ####
// =====================================================================================
// Render
// =====================================================================================
void SpaceState::render(){}

// DEV: ELITE_DEV_ROT_TRACE=<csv> - the player's rendered attitude per frame
// (turn smoothness: the render step angle vs the frame time, the authoritative /
// predicted attitude step, the motion mode). Measurement only.
static void devTraceRenderedAttitude(const ClientWorldState& world, std::uint64_t playerId)
{
    static const char* path = std::getenv("ELITE_DEV_ROT_TRACE");
    if (!path || !*path)
        return;
    static std::ofstream out(path);
    static bool header = false;
    static double lastT = -1.0;
    static glm::quat lastRender(1.0f, 0.0f, 0.0f, 0.0f), lastAuth(1.0f, 0.0f, 0.0f, 0.0f);
    const auto it = world.ships().find(playerId);
    if (it == world.ships().end() || !out)
        return;
    if (!header)
    {
        out << "t,dt_ms,mode,free_flight,render_step_deg,render_rate_dps,auth_step_deg\n";
        header = true;
    }
    const double t = glfwGetTime();
    const glm::quat render = glm::normalize(glm::quat_cast(it->second.renderTransform.orientation));
    const glm::quat auth = glm::normalize(glm::quat_cast(it->second.transform.orientation));
    if (lastT >= 0.0)
    {
        const auto angle = [](const glm::quat& a, const glm::quat& b) {
            return glm::degrees(2.0 * std::acos(std::min(1.0, std::abs(static_cast<double>(glm::dot(a, b))))));
        };
        const double dt = t - lastT;
        const double step = angle(lastRender, render);
        out << t << ',' << dt * 1000.0 << ',' << static_cast<int>(it->second.transform.motion.mode) << ','
            << (it->second.transform.motion.freeFlight ? 1 : 0) << ',' << step << ',' << (dt > 0.0 ? step / dt : 0.0) << ','
            << angle(lastAuth, auth) << '\n';
    }
    lastT = t;
    lastRender = render;
    lastAuth = auth;
}

void SpaceState::renderUI()
{
    const double renderUiStartMs = nowMs();
    if (m_client)
        devTraceRenderedAttitude(m_client->world(), m_playerId.value);
    updateJumpPresentation();


        if (context().app &&
            context().app->sceneGameUiMode() != GameUiMode::Flight)
        {
            /*
                Navigation/Service presentation owns the frame; gameplay cameras are not rendered underneath it.

                Обнуляем их профайлеры, иначе debug panel показывает
                значения от последнего кадра игрового режима.
            */
            m_perfMainRenderMs = 0.0;

            m_perfMainStats.reset();

            m_activeMainCamera = nullptr;

            m_perfRenderUiMs =
                nowMs() -
                renderUiStartMs;

            return;
        }



    // One full world render per frame: F1/F2/F3 only choose where the main
    // camera looks; celestial and object LOD plan against this camera.
    Camera* mainCam = &m_playerView->camera(m_activeCameraMode);
    if (auto* comp = uiRoot->findById("main_label"))
    {
        if (auto* text = dynamic_cast<UIText*>(comp))
        {
            switch (m_activeCameraMode)
            {
                case ShipCameraMode::Cockpit:
                    text->label = localizedUiText(context().app, "cockpit.front", "FRONT");
                    break;
                case ShipCameraMode::Rear:
                    text->label = localizedUiText(context().app, "cockpit.rear", "REAR");
                    break;
                case ShipCameraMode::Drone:
                    text->label = localizedUiText(context().app, "cockpit.drone", "RECON DRONE");
                    break;
                case ShipCameraMode::Bottom:
                    text->label = localizedUiText(context().app, "cockpit.bottom", "BOTTOM");
                    break;
            }
        }
    }

    m_activeMainCamera = mainCam;

    // J near a body: the hull shakes with the atmospheric deceleration and on
    // every impact (dense-layer entry, surface contact)
    if (m_client)
    {
        const auto it = m_client->world().ships().find(m_playerId.value);
        if (it != m_client->world().ships().end() && it->second.transform.motion.freeFlight)
        {
            const auto& m = it->second.transform.motion;
            if (m.freeFlightImpactSerial != m_jImpactSerialSeen)
            {
                if (m_jImpactSerialSeen != 0 || m.freeFlightImpactSerial > 0)
                    m_jShakePulse = 1.0f;
                m_jImpactSerialSeen = m.freeFlightImpactSerial;
            }
            const float drag = static_cast<float>(std::clamp(m.freeFlightDragMps2 / 30.0, 0.0, 1.0));
            const float intensity = std::clamp(drag + m_jShakePulse, 0.0f, 1.5f);
            static double lastShakeT = 0.0;
            const double nowT = startupElapsedMs(m_startupTotalBegin) / 1000.0;
            const float shakeDt = static_cast<float>(std::clamp(nowT - lastShakeT, 0.0, 0.1));
            lastShakeT = nowT;
            m_jShakePulse = std::max(0.0f, m_jShakePulse - shakeDt * 0.7f);
            if (intensity > 0.002f)
            {
                const double t = startupElapsedMs(m_startupTotalBegin) / 1000.0;
                const float a = 0.010f * intensity;
                const float pitch = a * static_cast<float>(std::sin(t * 37.0) * 0.6 + std::sin(t * 91.0) * 0.4);
                const float yaw = a * static_cast<float>(std::sin(t * 43.0 + 1.3) * 0.6 + std::sin(t * 77.0) * 0.4);
                glm::mat4 shake = glm::rotate(glm::mat4(1.0f), pitch, glm::vec3(1, 0, 0));
                shake = glm::rotate(shake, yaw, glm::vec3(0, 1, 0));
                mainCam->setOrientationMatrix(mainCam->orientationMatrix() * shake);
            }
        }
    }


    // -------------------------------------------------
    // HUD telemetry: client presentation -> visible UIText bindings.
    // The same presenter is exercised by Client Acceptance.
    // -------------------------------------------------
    {
        const auto& ships = m_client->world().ships();
        const auto it = ships.find(m_playerId.value);

        if (it != ships.end())
        {
            const auto mother = m_motherComputer.snapshot();
            m_motherComputer.publishCockpit(
                game::ship::computer::collectCockpitView(
                    it->second, mother
                )
            );
            const auto cockpit = m_motherComputer.cockpitSnapshot(
                static_cast<std::uint64_t>(it->second.instanceId),
                mother.input.serverTick
            );
            if (cockpit.valid)
            {
            game::presentation::PlayerHudTelemetryTextProfile textProfile;
            textProfile.cellLabel = localizedUiText(context().app, "cockpit.cell", "CELL");
            textProfile.relativeVelocityLabel = localizedUiText(context().app, "cockpit.vrel", "VREL");
            textProfile.newtonianModeLabel =
                localizedUiText(context().app, "cockpit.mode.newtonian", "NEWTONIAN");
            textProfile.assistedModeLabel =
                localizedUiText(context().app, "cockpit.mode.assisted", "ASSISTED");

            const auto telemetry =
                game::presentation::buildPlayerHudTelemetry(
                    cockpit,
                    textProfile
                );

            game::presentation::applyPlayerHudTelemetry(
                *uiRoot,
                telemetry
            );

            // J: speed along the orbit of the dominant body - falling or
            // climbing, circular / escape speed, periapsis / apoapsis
            {
                std::string line1, line2;
                if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
                {
                    const auto& m = it->second.transform.motion;
                    if (m.freeFlight && !m.freeFlightOrbitBodyId.empty())
                    {
                        std::string body = m.freeFlightOrbitBodyId;
                        if (const auto dot = body.rfind('.'); dot != std::string::npos)
                            body = body.substr(dot + 1);
                        char a[160], b[160];
                        std::snprintf(a, sizeof a, "%s %s: V %.2f km/s  %s %+.2f km/s  (%s %.2f / %s %.2f)",
                                      localizedUiText(context().app, "cockpit.orbit", "ORBIT").c_str(), body.c_str(),
                                      m.freeFlightOrbitSpeedMps / 1000.0,
                                      localizedUiText(context().app, "cockpit.orbit.radial", "RADIAL").c_str(),
                                      m.freeFlightOrbitRadialMps / 1000.0,
                                      localizedUiText(context().app, "cockpit.orbit.circular", "CIRC").c_str(),
                                      m.freeFlightOrbitCircularMps / 1000.0,
                                      localizedUiText(context().app, "cockpit.orbit.escape", "ESC").c_str(),
                                      m.freeFlightOrbitEscapeMps / 1000.0);
                        if (m.freeFlightOrbitBound)
                            std::snprintf(b, sizeof b, "H %.0f km   Pe %.0f km   Ap %.0f km%s",
                                          m.freeFlightOrbitAltitudeM / 1000.0, m.freeFlightOrbitPeriapsisAltM / 1000.0,
                                          m.freeFlightOrbitApoapsisAltM / 1000.0,
                                          m.freeFlightOrbitPeriapsisAltM < 0.0 ? "   !" : "");
                        else
                            std::snprintf(b, sizeof b, "H %.0f km   Pe %.0f km   %s",
                                          m.freeFlightOrbitAltitudeM / 1000.0, m.freeFlightOrbitPeriapsisAltM / 1000.0,
                                          localizedUiText(context().app, "cockpit.orbit.escaping", "ESCAPING").c_str());
                        line1 = a;
                        line2 = b;
                    }
                }
                for (const auto& [id, text] : {std::pair<const char*, const std::string*>{"main_orbit_1", &line1},
                                               std::pair<const char*, const std::string*>{"main_orbit_2", &line2}})
                    if (auto* label = dynamic_cast<UIText*>(uiRoot->findById(id)))
                        label->label = *text;
            }
            }
        }
    }





    const Viewport& vp = context().viewport();
    float aspect = (float)vp.width / (float)vp.height;

    if (mainCam)
    {
        mainCam->setAspect(aspect);
    }


    const double mainRenderStartMs = nowMs();

    // -------------------------------
    // 3D ПРОЕКЦИЯ
    // -------------------------------

    SceneRenderPolicy mainPolicy;


    const auto& dbg = debug::get().render;

    mainPolicy.drawStarfield = dbg.renderStarfield;
    mainPolicy.drawCelestial = dbg.renderCelestialBodies;
    mainPolicy.drawFarStationProxy = dbg.renderHubs;
    mainPolicy.drawHubs = dbg.renderHubs;
    mainPolicy.drawLargeObjects = dbg.renderLargeObjects;
    mainPolicy.drawObjects =
        dbg.renderHubs ||
        dbg.renderLargeObjects ||
        dbg.renderCelestialBodies;
    mainPolicy.drawRealShips = dbg.renderRealShips;
    mainPolicy.drawPlayerShip = dbg.renderPlayerShip;
    mainPolicy.drawNpcShips = dbg.renderNpcShips;
    mainPolicy.drawVisualShips = dbg.renderVisualShips;
    mainPolicy.drawTrafficShips = dbg.renderTrafficShips;
    mainPolicy.drawVisualDrones = dbg.renderVisualShips;




    glm::dvec3 observerGalacticPositionLy {0.0};
    const glm::dvec3* observerGalacticPositionPtr = nullptr;
    if (resolvePlayerGalacticPositionLy(observerGalacticPositionLy))
        observerGalacticPositionPtr = &observerGalacticPositionLy;

    const double scenePrepareStartMs = nowMs();
    m_sceneRenderer.prepareScene(
        m_preparedScene,
        m_client->world(),
        m_playerId,
        observerGalacticPositionPtr
    );
    m_preparedScene.celestialSnapshot =
        m_client ? m_client->celestialSnapshot() : nullptr;
    // the map textures of the body the ship is near (its sphere of influence, or a hub's parent): baked
    // in the background before F10 / F11 / F12 opens (the clouds took ~0.7 s on open)
    if (m_preparedScene.celestialSnapshot && nowMs() - m_mapPrewarmAtMs > 1000.0)
    {
        m_mapPrewarmAtMs = nowMs();
        const auto& ships = m_client->world().ships();
        if (const auto it = ships.find(m_playerId.value); it != ships.end())
        {
            const auto& motion = it->second.transform.motion;
            std::string body = motion.freeFlightReferenceIsRoot != 0 ? std::string() : motion.freeFlightReferenceId;
            if (const auto hub = m_client->world().hubs().find(body); hub != m_client->world().hubs().end())
                body = hub->second.parentBodyId;
            if (body.find('.') != std::string::npos)
                m_systemMapRenderer.prewarmBodyMap(m_preparedScene.celestialSnapshot->systemId, body,
                                                   m_preparedScene.celestialSnapshot->simTimeSeconds);
        }
    }
    m_preparedScene.celestialSystem =
        m_preparedScene.celestialSnapshot && m_client->starAtlas()
            ? m_client->starAtlas()->findSystem(
                  m_preparedScene.celestialSnapshot->systemId)
            : nullptr;
    if (context().app)
    {
        // DEV: ELITE_DEV_CELESTIAL_STYLE=elite|modern|anime overrides the
        // user setting for scripted captures (never persisted).
        static const char* devStyle = std::getenv("ELITE_DEV_CELESTIAL_STYLE");
        m_sceneRenderer.setCelestialRenderStyle(
            devStyle ? std::string(devStyle)
                     : context().app->clientModeState().celestialRenderStyleId
        );
    }
    m_perfScenePrepareMs = nowMs() - scenePrepareStartMs;
    core::hitch::mark("scene_prepare");



    RenderCameraViewport::render(
        *mainCam,
        vp,
        vp.x,
        vp.y,
        vp.width,
        vp.height,
        [&](auto view, auto proj)
        {
            SceneCameraParams mainCamera;
            mainCamera.view = view;
            mainCamera.proj = proj;
            mainCamera.cameraId = 0;
            mainCamera.cameraName = "mainCam";

            m_sceneRenderer.renderPrepared(
                m_preparedScene,
                mainCamera,
                mainPolicy
            );

            m_perfMainStats = m_sceneRenderer.lastStats();
        }
    );

    m_perfMainRenderMs = nowMs() - mainRenderStartMs;
    core::hitch::mark("scene_main_render");




{
    const double observedRenderMs = m_perfMainRenderMs;
    if (observedRenderMs > 18.0 ||
        m_perfFixedSimMs > 8.0 ||
        m_perfClientUpdateMs > 8.0 ||
        m_perfDockingGuidanceMs > 2.0)
    {
        static double lastFramePerfReportMs = -1.0e9;
        const double now = nowMs();
        if (now - lastFramePerfReportMs >= 500.0)
        {
            std::cerr
                << "[FramePerf] main_ms=" << m_perfMainRenderMs
                << " prepare_ms=" << m_perfScenePrepareMs
                << " scene_submit_ms=" << m_perfMainStats.cpuTotalMs
                << " fixed_sim_ms=" << m_perfFixedSimMs
                << " client_ms=" << m_perfClientUpdateMs
                << " dock_ms=" << m_perfDockingGuidanceMs
                << " dock_request=" << (m_perfDockingRequestActive ? 1 : 0)
                << " draw_calls=" << m_perfMainStats.drawCalls
                << " parts=" << m_perfMainStats.partsDrawn
                << '\n';
            // the embedded server's stages since the last report (ServerTickProfile; EliteServer --load-test prints
            // the same) - where a slow fixed_sim goes
            {
                auto& p = game::server::serverTickProfile();
                const double n = std::max<long long>(1, p.ticks);
                std::fprintf(stderr,
                             "[SimPerf] ticks %lld, %.1f system passes, %.0f ships: per tick ms simulation %.2f snapshot %.2f "
                             "sessions %.2f | stages: ships %.2f hubs %.2f frames+freeflight %.2f static %.2f ai/controls %.2f "
                             "physics %.2f radar %.2f\n",
                             p.ticks, p.systemPasses / n, p.simulatedShips / n, 1000 * p.simulation / n, 1000 * p.snapshotBuild / n,
                             1000 * (p.sessionPlan + p.sessionSelect + p.sessionCopy + p.sessionPublish) / n,
                             1000 * p.simStage[0] / n, 1000 * p.simStage[1] / n, 1000 * p.simStage[2] / n,
                             1000 * p.simStage[3] / n, 1000 * p.simStage[4] / n, 1000 * p.simStage[5] / n,
                             1000 * p.simStage[6] / n);
                p = {};
            }
            lastFramePerfReportMs = now;
        }
    }
}

// ------------------------------------------------------------
// DEBUG: full render frame log after rear camera
// Пишем каждый кадр, но ограниченно: 1000 строк.
// Потом по нему видно, совпадают ли фризы с rear camera/full objects.
// ------------------------------------------------------------






   m_perfRenderUiMs = nowMs() - renderUiStartMs;
}



void SpaceState::handleResize(int width, int height)
{
    if (m_playerView)
    {
        m_playerView->resize(width, height);
        m_playerView->updateBoundary(width, height);


    }
}
// Refuel at the station the ship stands at (docs/FUEL_STATIONS.md): the server decides and runs the job
// (GameSimulationRefuel.cpp); mode 1 start, 0 stop, -1 toggle by the replicated job phase.
void SpaceState::requestClientRefuel(int mode)
{
    if (!m_client)
        return;
    if (mode < 0)
    {
        const auto it = m_client->world().ships().find(m_playerId.value);
        const std::uint8_t phase = it != m_client->world().ships().end() ? it->second.transform.motion.refuelState : 0;
        mode = phase >= 1 && phase <= 3 ? 0 : 1;   // hookup / pumping / disconnect: running -> stop
    }
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::RequestRefuel;
    command.index = mode;
    msg.payload = command;
    m_client->sendMessage(msg);
    m_pilotNotice = mode ? "ЗАПРАВКА: запрос станции" : "ЗАПРАВКА: остановлена";
    std::cerr << "[Refuel] client: " << (mode ? "refuel requested" : "refuel stop requested") << std::endl;
}
