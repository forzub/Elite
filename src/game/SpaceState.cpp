#include "src/game/SpaceStateInternal.h"













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
SpaceState::SpaceState(
    StateStack& states,
    StartupMode startupMode
)
    : GameState(states),
      m_systemMapRenderer(m_navigationWorkspace)
{
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
    updateLocalMapPresentationSnapshots(std::max(0.0f, dt));

    applyClientCatalogLocalization();
}


// =====================================================================================
// Input
// =====================================================================================
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

    if (m_client)
    {
        const auto& ships = m_client->world().ships();
        const auto playerIt = ships.find(m_playerId.value);
        if (playerIt != ships.end())
            currentLocalControlLaw =
                playerIt->second.transform.motion.localControlLaw;
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
    else if (!m_clientAutopilotControlActive)
    {
        // Human input owns the ordinary control stream only while the local
        // virtual pilot is inactive. Client autopilot submission happens from
        // updateDockingAdvisory(), so it continues even while a map/UI screen
        // causes handleInput() to return before reaching this block.
        m_inputMapper.update(m_playerControl, currentLocalControlLaw);
        m_client->submitInput(m_playerControl);
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


void SpaceState::setFlightScreenLayout(ScreenLayout layout)
{
    // F1-F4 leave the Navigation presentation domain immediately. Do not let
    // an unfinished map-to-map snapshot/crossfade survive behind the next
    // Flight frame, even when the selected flight layout is already active.
    m_systemMapRenderer.cancelMapTransition();

    if (m_layout == layout)
        return;

    PlayerShipView::g_debugLogNextFrame = true;
    m_layout = layout;
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


void SpaceState::update(float dt)
{
    const double updateStartMs = nowMs();
    m_perfFrameIndex++;

    const double htmlStartMs = nowMs();
    processHtmlCommands();
    m_perfProcessHtmlMs = nowMs() - htmlStartMs;

    // DEV (map performance measurements): ELITE_DEV_GALAXY_MAP_AT=<s> opens
    // the Galaxy Map once, <s> seconds after the client started.
    {
        static const double openAtSeconds = [] {
            const char* v = std::getenv("ELITE_DEV_GALAXY_MAP_AT");
            return v && *v ? std::atof(v) : -1.0;
        }();
        static bool opened = false;
        if (openAtSeconds >= 0.0 && !opened && context().app && m_client &&
            startupElapsedMs(m_startupTotalBegin) >= openAtSeconds * 1000.0)
        {
            opened = true;
            context().app->requestNavigationView(NavigationPresentationView::Galaxy);
        }

        // DEV: ELITE_DEV_SYSTEM_MAP_AT=<s> opens the System Map once.
        static const double openSystemAtSeconds = [] {
            const char* v = std::getenv("ELITE_DEV_SYSTEM_MAP_AT");
            return v && *v ? std::atof(v) : -1.0;
        }();
        static bool systemOpened = false;
        if (openSystemAtSeconds >= 0.0 && !systemOpened && context().app && m_client &&
            startupElapsedMs(m_startupTotalBegin) >= openSystemAtSeconds * 1000.0)
        {
            systemOpened = true;
            context().app->requestNavigationView(NavigationPresentationView::System);
        }
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
                    "guidance_dock_cube_a";
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
        updateDockingAdvisory();
    }

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

    // Client-side station traffic.
    // Это не настоящие ShipCore-корабли, а лёгкий визуальный трафик.
    // Он не попадает в server snapshot и не грузит GameSimulation.
    if (!isPromo1SceneMode())
    {



        try
        {
            m_stationTrafficSystem.setup(m_client->world());
            m_stationTrafficSystem.update(
                m_client->world(),
                dt
            );
        }
        catch (const std::bad_alloc&)
        {
            std::cerr
                << "[HubMotionLab][bad_alloc] phase=station-traffic"
                << " visualShips=" << m_client->world().visualShips().size()
                << " realShips=" << m_client->world().ships().size()
                << "\n";
            throw;
        }
    }

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
    ship.transform.forwardVelocity,
    ship.transform.targetSpeed,
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
            m_radarWidget->setPlayerTransform(
                world::coordinates::legacyFloatMeters(
                    ship.renderTransform.worldPosition
                ),
                ship.renderTransform.orientation
            );

            std::vector<RadarContactView> views;
            views.reserve(ship.radarContacts.size());

            for (const auto& c : ship.radarContacts)
            {
                views.push_back({
                    c.id,
                    c.localPosition
                });
            }

            m_radarWidget->setContacts(views);
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

void SpaceState::renderUI()
{
    const double renderUiStartMs = nowMs();


        if (context().app &&
            context().app->sceneGameUiMode() != GameUiMode::Flight)
        {
            /*
                Navigation/Service presentation owns the frame; gameplay cameras are not rendered underneath it.

                Обнуляем их профайлеры, иначе debug panel показывает
                значения от последнего кадра игрового режима.
            */
            m_perfMainRenderMs = 0.0;
            m_perfRearCameraMs = 0.0;

            m_perfMainStats.reset();
            m_perfRearStats.reset();

            m_activeMainCamera = nullptr;

            m_perfRenderUiMs =
                nowMs() -
                renderUiStartMs;

            return;
        }



    Camera*                                     mainCam = nullptr;
    Camera*                                     miniCam = nullptr;

    const std::string frontLabel =
        localizedUiText(context().app, "cockpit.front", "FRONT");
    const std::string rearLabel =
        localizedUiText(context().app, "cockpit.rear", "REAR");
    const std::string droneLabel =
        localizedUiText(context().app, "cockpit.drone", "DRONE");



    switch (m_layout)
    {
        case ScreenLayout::Front_Main_Rear_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            miniCam = &m_playerView->camera(ShipCameraMode::Rear);
            m_activeCameraMode = ShipCameraMode::Cockpit;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = frontLabel;}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = rearLabel;}
            }
            break;

        case ScreenLayout::Rear_Main_Front_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Rear);
            miniCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            m_activeCameraMode = ShipCameraMode::Rear;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = rearLabel;}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = frontLabel;}
            }
            break;

        case ScreenLayout::Front_Main_Drone_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            miniCam = &m_playerView->camera(ShipCameraMode::Drone);
            m_activeCameraMode = ShipCameraMode::Cockpit;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = frontLabel;}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = droneLabel;}
            }
            break;

        case ScreenLayout::Drone_Main_Front_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Drone);
            miniCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            m_activeCameraMode = ShipCameraMode::Drone;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = droneLabel;}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = frontLabel;}
            }
            break;
    }

    m_activeMainCamera = mainCam;


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




    // -------------------------------
    // Rear - камера корабля.
    // -------------------------------

    static uint32_t rearCameraFrameCounter = 0;
rearCameraFrameCounter++;

if (debug::get().render.shouldRenderRearCamera())
{
    // Рендерим заднюю камеру не каждый кадр.
    // 2 = через кадр. 3 = каждый третий кадр.
    constexpr uint32_t kRearCameraFrameStride = 2;

    if ((rearCameraFrameCounter % kRearCameraFrameStride) == 0)
    {
        const double rearStartMs = nowMs();

        rearView->camera = miniCam;

        // Secondary camera consumes the scene already prepared for this frame.
        // Calling rearView->drawCallback here would invoke SceneRenderer::render
        // and prepare the whole nearby station/ship scene a second time.
        SceneRenderPolicy rearPolicy;
        const auto& rearDbg = debug::get().render;
        rearPolicy.drawLabels = false;
        rearPolicy.drawDebug = false;
        rearPolicy.drawStarfield = rearDbg.renderStarfield;
        rearPolicy.drawCelestial = rearDbg.renderCelestialBodies;
        rearPolicy.drawFarStationProxy = rearDbg.renderHubs;
        rearPolicy.drawHubs = rearDbg.renderHubs;
        rearPolicy.drawLargeObjects = rearDbg.renderLargeObjects;
        rearPolicy.drawObjects =
            rearDbg.renderHubs ||
            rearDbg.renderLargeObjects ||
            rearDbg.renderCelestialBodies;
        rearPolicy.drawRealShips = rearDbg.renderRealShips;
        rearPolicy.drawPlayerShip = rearDbg.renderPlayerShip;
        rearPolicy.drawNpcShips = rearDbg.renderNpcShips;
        rearPolicy.drawVisualShips = rearDbg.renderVisualShips;
        rearPolicy.drawTrafficShips = rearDbg.renderTrafficShips;
        rearPolicy.drawVisualDrones = rearDbg.renderVisualShips;
        rearPolicy.maxVisualShipsToDraw = 24;
        rearPolicy.forceAssemblyLod1 = true;

        rearView->renderToTexture(
            vp,
            [&](const glm::mat4& view, const glm::mat4& proj)
            {
                SceneCameraParams rearCamera;
                rearCamera.view = view;
                rearCamera.proj = proj;
                rearCamera.cameraId = 1;
                rearCamera.cameraName = "secondCam";

                m_sceneRenderer.renderPrepared(
                    m_preparedScene,
                    rearCamera,
                    rearPolicy
                );
                m_perfRearStats = m_sceneRenderer.lastStats();
            }
        );

        m_perfRearCameraMs = nowMs() - rearStartMs;
        core::hitch::mark("scene_rear_camera");
    }
}
else
{
    m_perfRearCameraMs = 0.0;
    m_perfRearStats.reset();
}

{
    const double observedRenderMs =
        m_perfMainRenderMs + m_perfRearCameraMs;
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
                << " rear_ms=" << m_perfRearCameraMs
                << " fixed_sim_ms=" << m_perfFixedSimMs
                << " client_ms=" << m_perfClientUpdateMs
                << " dock_ms=" << m_perfDockingGuidanceMs
                << " dock_request=" << (m_perfDockingRequestActive ? 1 : 0)
                << " draw_calls=" << m_perfMainStats.drawCalls
                << " parts=" << m_perfMainStats.partsDrawn
                << '\n';
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