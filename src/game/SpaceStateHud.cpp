// SpaceState: Flight HUD and in-session overlays (route planning modal, navigation stress, language, universe time).

#include "src/game/presentation/DockingWidgetText.h"
#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/SpaceStateInternal.h"




// =====================================================================================
// Render HUD
// =====================================================================================
void SpaceState::renderHUD()
{
    const double hudStartMs = nowMs();

    const Viewport& vp = context().viewport();

        if (context().app &&
        context().app->sceneGameUiMode() == GameUiMode::SystemMap &&
        m_client)
    {
        glDisable(GL_DEPTH_TEST);

        Viewport mapVp = vp;

        const float panelRatio = 0.28f;
        mapVp.x = vp.x;
        mapVp.y = vp.y;
        mapVp.width = static_cast<int>(static_cast<float>(vp.width) * (1.0f - panelRatio));
        mapVp.height = vp.height;

        m_systemMapRenderer.setRightPanelRatio(panelRatio);






/* Local-map snapshots were refreshed before input. */


m_systemMapRenderer.render(
    mapVp,
    m_galaxyMapSnapshot,
    m_systemMapSnapshot,
    m_detailMapSnapshot,
    m_hubMapSnapshot,
    m_client->playerNavigation()
);




        glEnable(GL_DEPTH_TEST);


        m_perfHudMs = nowMs() - hudStartMs;
    core::hitch::mark("hud");
        return;
    }






    int vx = vp.width;
    int vy = vp.height;

    float fx = (float)vp.width;
    float fy = (float)vp.height;

    glViewport(vp.x, vp.y, vp.width, vp.height);
    glScissor(vp.x, vp.y, vp.width, vp.height);





    // -------------------------------------------------
    // сразу ставим ортографию и больше её не трогаем - это для 2D графики
    // -------------------------------------------------
    glDisable(GL_DEPTH_TEST);

    elite::render::core_legacy::matrixMode(elite::render::core_legacy::ProjectionToken);
    elite::render::core_legacy::loadIdentity();
    elite::render::core_legacy::ortho(0, vx, vy, 0, -1, 1);

    elite::render::core_legacy::matrixMode(elite::render::core_legacy::ModelViewToken);
    elite::render::core_legacy::loadIdentity();


    // -------------------------------------------------
    // 1. Статический HUD (рамки, текст)
    // -------------------------------------------------


    const auto& ships = m_client->world().ships();
    auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;





    const auto& playerShip = it->second;
    const auto mother = m_motherComputer.snapshot();
    m_motherComputer.publishCockpit(
        game::ship::computer::collectCockpitView(playerShip, mother)
    );
    const auto cockpit = m_motherComputer.cockpitSnapshot(
        static_cast<std::uint64_t>(playerShip.instanceId),
        mother.input.serverTick
    );
    const auto& radarContacts = playerShip.radarContacts;




    // -------------------------------------------------
    // 2. Ship UI / HUD layer
    // -------------------------------------------------
    // One switch for everything drawn as the player's ship interface:
    // HUD boundary, world labels/markers, cockpit overlay, rear/miniview UI, radar.
    if (debug::get().render.shouldRenderShipUi())
    {
        m_playerView->renderHudBoundary();

        // -------------------------------------------------
        // 3. Подготовка матриц для меток
        // -------------------------------------------------

        if (m_activeCameraMode != ShipCameraMode::Drone)
        {
            renderHudWorldLabels(vp);
        }

        // World-anchored navigation presentation is projected onto the glass
        // before the cockpit overlay.  The cockpit frame therefore occludes
        // corridor/compass pixels outside the visible windshield instead of
        // letting them paint over the dashboard or canopy frame.
        if (debug::get().render.shouldRenderCockpit() &&
            m_activeCameraMode != ShipCameraMode::Drone &&
            m_activeMainCamera)
        {
            renderHudGuidanceAndCompass(vp, playerShip);
        }

        // The recon drone is an external camera: no cockpit frame.
        if (debug::get().render.shouldRenderCockpit() &&
            m_activeCameraMode != ShipCameraMode::Drone)
        {
            m_playerView->renderCockpit();
        }

        TextRenderer::instance().beginFrameForViewport(
            vp.width,
            vp.height
        );

        if (debug::get().render.shouldRenderCockpit() &&
            m_activeCameraMode != ShipCameraMode::Drone)
        {
            renderHudCockpitInstruments(vp, playerShip, cockpit);
        }

        // ------------------------------------------------------------
        // Isolated docking status terminal.
        // Presentation-only adapter: it snapshots state into a DTO and renders
        // the pure terminal output. No navigation code can be reached from the
        // terminal module.
        // ------------------------------------------------------------
        {
            renderHudDockingTerminal(vp, playerShip, cockpit);
        }

        uiRoot->render(vp);

        TextRenderer::instance().endFrame();


















    }


    // // 3. векторные приборы
    glEnable(GL_DEPTH_TEST);

    renderUniverseTimeSimulationOverlay(vp);
    renderUiLanguageIndicator(vp);
    renderNavigationStressOverlay(vp);
    renderFreeFlightBanner(vp);
    renderDockingWidget(vp);
    renderZoneViolationBanner(vp);
    if (context().app)
    {
        game::presentation::TemporaryShortcutStrip::render(
            vp,
            context().app->localization()
        );
    }

    m_perfHudMs = nowMs() - hudStartMs;
    core::hitch::mark("hud");
}

// renderHUD / ship UI: world-anchored labels and markers projected with the
// active flight camera (not in the drone view).
void SpaceState::renderHudWorldLabels(const Viewport& vp)
{
    auto it = m_client->world().ships().find(m_playerId.value);
    if (it != m_client->world().ships().end())
    {
        const auto& ship = it->second;

        if (game::runtime::WorldSignalLabelsEnabled)
        {
            m_playerView->renderWorldLabels(
                m_playerView->worldLabels(),
                world::coordinates::legacyFloatMeters(
                    ship.renderTransform.worldPosition
                ),
                m_activeMainCamera->viewMatrix(),
                m_activeMainCamera->projectionMatrix(),
                vp
            );
        }

        game::presentation::NavigationHudVocabulary navVocabulary;
        if (context().app)
        {
            const auto& loc = context().app->localization();
            navVocabulary.objectText =
                loc.text("map.navigation_hud.object", "Object");
            navVocabulary.celestialText =
                loc.text("map.navigation_hud.celestial", "Celestial");
            navVocabulary.startText =
                loc.text("map.navigation_hud.start", "START");
            navVocabulary.finishText =
                loc.text("map.navigation_hud.finish", "FINISH");
            navVocabulary.waypointText =
                loc.text("map.navigation_hud.waypoint", "WAYPOINT");
            navVocabulary.relativeSpeedShort =
                loc.text("map.navigation_hud.relative_speed_short", "REL");
            navVocabulary.globalSpeedShort =
                loc.text("map.navigation_hud.global_speed_short", "GLOB");
        }

        // DEV: ELITE_DEV_CELESTIAL_MARKERS=venus,mars,... adds HUD
        // planet markers through the same tracking path a map info
        // card uses (rememberCelestialBody) - the marker load test.
        static const char* devMarkers = std::getenv("ELITE_DEV_CELESTIAL_MARKERS");
        if (devMarkers && m_client && m_client->celestialSnapshot())
        {
            const std::string list = std::string(",") + devMarkers + ",";
            for (const auto& body : m_client->celestialSnapshot()->bodies)
            {
                const std::size_t slash = body.sourcePath.find_last_of("/\\");
                std::string stem = body.sourcePath.substr(slash == std::string::npos ? 0 : slash + 1);
                if (stem.rfind("planet_", 0) != 0)
                    continue;
                stem = stem.substr(7, stem.size() - 7 - 5); // planet_<name>.json
                if (list.find("," + stem + ",") == std::string::npos &&
                    list.find(",all,") == std::string::npos)
                    continue;
                m_navigationWorkspace.targets().rememberCelestialBody(
                    "dev:" + stem,
                    m_client->celestialSnapshot()->systemId,
                    body.id,
                    "Planet",
                    body.name,
                    world::coordinates::makeWorldPositionFromMeters(body.worldMeters),
                    glm::vec4(0.75f, 0.85f, 1.0f, 1.0f)
                );
            }
        }

        const auto navigationMarkers =
            game::presentation::buildNavigationHudMarkers(
                m_navigationWorkspace,
                m_client->world(),
                ship,
                navVocabulary,
                &m_hubSemanticAnchorCatalog,
                ship.renderReferenceFrame.universeTimeSeconds
            );

        m_playerView->renderNavigationMarkers(
            navigationMarkers,
            m_activeMainCamera->viewMatrix(),
            m_activeMainCamera->projectionMatrix(),
            vp
        );
    }

}

// renderHUD / ship UI: the guidance corridor and the galactic compass on the
// glass, drawn before the cockpit frame occludes them.
void SpaceState::renderHudGuidanceAndCompass(const Viewport& vp, const ClientShipState& playerShip)
{
    const auto guidance =
        game::presentation::buildGuidanceCorridorHudPresentation(
            m_navigationWorkspace,
            playerShip,
            playerShip.renderReferenceFrame.universeTimeSeconds
        );

    TextRenderer::instance().beginFrameForViewport(
        vp.width,
        vp.height
    );
    m_guidanceCorridorRenderer.render(
        guidance,
        m_activeMainCamera->viewMatrix(),
        m_activeMainCamera->projectionMatrix(),
        vp
    );

    const bool dockingGuidanceVisible =
        guidance.visible &&
        guidance.spatialAdvisoryGates &&
        guidance.source ==
            game::navigation::GuidanceSource::DockingComputer &&
        !m_activeDockingGuidanceCorridorId.empty();

    if (dockingGuidanceVisible)
    {
        const auto& dockingRequest =
            m_navigationWorkspace.
                dockingRouteRequests().pending();
        const bool automaticDockingRequested =
            dockingRequest.valid() &&
            dockingRequest.mode ==
                game::navigation::
                    DockingRouteRequest::Mode::Automatic;
        const bool automaticDockingActive =
            automaticDockingRequested &&
            m_clientDockingPhase ==
                ClientDockingPhase::Executing;

        const double dockingBlinkPhase = std::fmod(
            std::max(
                0.0,
                playerShip.renderReferenceFrame.
                    universeTimeSeconds
            ) * 2.0,
            1.0
        );
        if (dockingBlinkPhase < 0.58)
        {
            const std::string dockingText =
                localizedUiText(
                    context().app,
                    automaticDockingActive
                        ? "cockpit.docking.automatic_mode"
                        : automaticDockingRequested
                            ? "cockpit.docking.preparing_mode"
                            : "cockpit.docking.manual_mode",
                    automaticDockingActive
                        ? "AUTOMATIC DOCKING MODE"
                        : automaticDockingRequested
                            ? "PREPARING AUTOMATIC DOCKING"
                            : "MANUAL DOCKING MODE"
                );
            constexpr int dockingPx = 20;
            auto& text = TextRenderer::instance();
            const float width =
                text.measureTextPx(
                    dockingText,
                    dockingPx
                );
            const float x =
                static_cast<float>(vp.width) * 0.5f -
                width * 0.5f;
            const float y = 32.0f;
            text.textDrawPx(
                dockingText,
                x,
                y,
                dockingPx,
                glm::vec4(
                    0.62f,
                    0.96f,
                    1.0f,
                    1.0f
                )
            );
        }
    }

    const bool noSafeGuidance =
        guidance.noSafePrimarySolution ||
        m_noSafeDockingGuidanceSolution;
    const bool guidanceUnavailable =
        !noSafeGuidance && !m_dockingGuidanceFailureReason.empty();
    if (noSafeGuidance || guidanceUnavailable)
    {
        const double blinkPhase = std::fmod(
            std::max(0.0, m_client->universeTimeSeconds()) * 2.0,
            1.0
        );
        if (blinkPhase < 0.58)
        {
            std::string warning = noSafeGuidance
                ? localizedUiText(
                    context().app,
                    "map.navigation_hud.no_safe_guidance_solution",
                    "NO SAFE GUIDANCE SOLUTION"
                  )
                : localizedUiText(
                    context().app,
                    "map.navigation_hud.guidance_unavailable",
                    "GUIDANCE UNAVAILABLE"
                  );
            if (!m_dockingGuidanceFailureReason.empty())
            {
                warning += " [" +
                    m_dockingGuidanceFailureReason + "]";
            }
            constexpr int warningPx = 22;
            auto& text = TextRenderer::instance();
            const float width = text.measureTextPx(warning, warningPx);
            const float x =
                static_cast<float>(vp.width) * 0.5f - width * 0.5f;
            const float y = 62.0f;
            text.solidRectPx(
                x - 12.0f,
                y - 5.0f,
                width + 24.0f,
                warningPx + 12.0f,
                glm::vec4(0.14f, 0.015f, 0.01f, 0.78f)
            );
            text.textDrawPx(
                warning,
                x,
                y,
                warningPx,
                glm::vec4(1.0f, 0.38f, 0.24f, 1.0f)
            );
        }
    }

    game::presentation::GalacticCompassVocabulary compassVocabulary;
    compassVocabulary.galacticCenter =
        localizedUiText(
            context().app,
            "map.navigation_hud.galactic_center",
            "GC"
        );
    compassVocabulary.galacticAnticenter =
        localizedUiText(
            context().app,
            "map.navigation_hud.galactic_anticenter",
            "GAC"
        );
    compassVocabulary.longitude90 =
        localizedUiText(
            context().app,
            "map.navigation_hud.galactic_l90",
            "L90"
        );
    compassVocabulary.longitude270 =
        localizedUiText(
            context().app,
            "map.navigation_hud.galactic_l270",
            "L270"
        );
    compassVocabulary.northGalacticPole =
        localizedUiText(
            context().app,
            "map.navigation_hud.north_galactic_pole",
            "NGP"
        );
    compassVocabulary.southGalacticPole =
        localizedUiText(
            context().app,
            "map.navigation_hud.south_galactic_pole",
            "SGP"
        );

    const bool compassVisible =
        m_navigationWorkspace.modules().enabled(
            game::navigation::NavigationModuleId::HudGalacticCompass
        );
    const auto galacticCompass =
        game::presentation::buildGalacticCompassPresentation(
            m_galacticReferenceFrame,
            glm::dvec3(playerShip.renderTransform.forward()),
            compassVisible,
            compassVocabulary
        );
    m_galacticCompassRenderer.render(galacticCompass, vp);

}

// renderHUD / ship UI: cockpit instruments (engine thrust, systems, boresight,
// emergency working axis, flight instrument text, radar).
void SpaceState::renderHudCockpitInstruments(const Viewport& vp, const ClientShipState& playerShip, const game::ship::computer::MotherComputerCockpitView& cockpit)
{
    const auto engineThrustInstrument =
        game::presentation::buildEngineThrustHudPresentation(
            cockpit
        );
    m_engineThrustIndicatorRenderer.render(
        engineThrustInstrument,
        vp
    );
    // Compact amber system instruments beside the existing motor
    // gauge. Read the same authoritative replicated ship systems
    // seen by MotherComputer; never synthesize temperatures.
    if (cockpit.valid)
        m_engineThrustIndicatorRenderer.renderThermalSystems(
            cockpit.systems, engineThrustInstrument, vp
        );

    // The hull boresight is a fundamental attitude cue, not a velocity
    // cue and not a navigation-module product. Keep it present whenever
    // the cockpit HUD is present.
    m_flightVectorIndicatorRenderer.renderBoresight(vp);

    // Use the installed-motor capability resolved by MotherComputer
    // for this exact snapshot, not a smoothed rendering transform.
    if (cockpit.valid && cockpit.emergencyWorkingThrustAxis)
    {
        // Reproject the physical body-space working ray through
        // the EXACT camera rendering this viewport. The old HUD
        // pretended a hard-coded focal length and zero camera
        // attachment rotation; neither is the real view.
        const auto& sightCamera =
            m_playerView->camera(m_activeCameraMode);
        const glm::dvec4 bodyRay(
            cockpit.workingThrustAxisBody,0.0);
        const glm::dvec3 cameraRay = glm::dvec3(
            glm::dmat4(sightCamera.viewMatrix()) *
            glm::dmat4(playerShip.renderTransform.orientation) *
            bodyRay
        );
        m_flightVectorIndicatorRenderer.renderEmergencyThrustAxis(
            vp,
            cockpit.workingThrustAxisBody,
            cameraRay,
            sightCamera.fov(),
            localizedUiText(
                context().app,
                "cockpit.emergency.thrust_axis",
                "THRUST AXIS"
            ),
            m_playerView->hudEdgeMapper
        );

        // A working-axis sight alone cannot tell a human pilot where
        // the inertial velocity is already going. During an emergency
        // the HUD must show BOTH rays in the same real camera basis:
        // amber = achievable zero-torque thrust, green V = actual
        // hull-relative flight path in the matched local frame.
        if(playerShip.renderReferenceFrame.valid)
        {
            const glm::dvec3 localVelocity =
                playerShip.renderTransform.motion.localVelocityMps;
            if(std::isfinite(glm::length(localVelocity)) &&
               glm::length(localVelocity)>0.5)
            {
                const glm::dvec3 velocityWorld =
                    playerShip.renderReferenceFrame.localToWorldVector(
                        localVelocity);
                const glm::dvec3 velocityCamera=glm::dvec3(
                    glm::dmat4(sightCamera.viewMatrix())*
                    glm::dvec4(velocityWorld,0.0));
                m_flightVectorIndicatorRenderer.
                    renderEmergencyVelocityVector(
                        vp,velocityCamera,sightCamera.fov(),
                        m_playerView->hudEdgeMapper);
            }
        }
    }

    game::presentation::FlightInstrumentTextProfile textProfile;
    textProfile.newtonianModeLabel =
        localizedUiText(context().app, "cockpit.mode.newtonian", "NEWTONIAN");
    textProfile.assistedModeLabel =
        localizedUiText(context().app, "cockpit.mode.assisted", "ASSISTED");
    textProfile.alignForwardLabel =
        localizedUiText(context().app, "cockpit.action.align_forward", "ALIGN +V");
    textProfile.alignBackwardLabel =
        localizedUiText(context().app, "cockpit.action.align_backward", "ALIGN -V");
    textProfile.brakingLabel =
        localizedUiText(context().app, "cockpit.action.brake", "BRAKE");

    if (m_navigationWorkspace.modules().enabled(
            game::navigation::NavigationModuleId::HudFlightVector))
    {
        const auto flightInstrument =
            game::presentation::buildFlightVectorIndicatorPresentation(
                cockpit,
                textProfile
            );

        m_flightVectorIndicatorRenderer.render(
            flightInstrument,
            vp
        );
        // DEV diagnostic (author 2026-11-07: "скорость отображена перпендикулярно корпусу"): the angle
        // between the hull and the velocity the indicator shows, with what it is measured against
        {
            static double lastLog = -1.0e30;
            static const bool enabled = [] { const char* v = std::getenv("ELITE_DEV_HUDVEL_LOG"); return !v || *v != '0'; }();
            const double now = m_client ? m_client->lastSimulationMetadata().serverTimeSeconds : 0.0;
            const auto& mo = cockpit.renderTransform.motion;
            if (enabled && now - lastLog >= 2.0 && flightInstrument.speedMps > 1.0)
            {
                lastLog = now;
                const glm::vec3 fwd = flightInstrument.shipModelToIndicatorBasis[1];
                const double off = glm::degrees(std::acos(std::clamp(static_cast<double>(fwd.y) / std::max(1.0e-6, static_cast<double>(glm::length(fwd))), -1.0, 1.0)));
                std::cerr << "[HudVel] t " << now << " law " << (mo.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted ? "A" : "N")
                          << (mo.assistedFreeRotation ? " free" : "") << " ff " << mo.freeFlight << " J " << mo.jEngine << " ref " << int(mo.velocityReferenceKind)
                          << " ap " << int(mo.freeFlightAutopilotPhase) << " speed " << flightInstrument.speedMps << " m/s, hull-velocity "
                          << off << " deg" << std::endl;
            }
        }
    }

}

// renderHUD / ship UI: the isolated docking status terminal (snapshot DTO ->
// MotherComputer terminal -> text lines + tick audio).
void SpaceState::renderHudDockingTerminal(const Viewport& vp, const ClientShipState& playerShip, const game::ship::computer::MotherComputerCockpitView& cockpit)
{
    game::presentation::DockingTerminalSnapshot terminal;
    terminal.localeId = context().app
        ? context().app->localization().locale() : std::string();
    const game::presentation::TerminalLocalizedText::Lookup terminalText =
        [app = context().app](const char* key)
        {
            return localizedUiText(app, key, key);
        };
    const auto lineText = [&terminalText](
        const char* key, std::initializer_list<std::string> values)
    {
        return game::presentation::TerminalLocalizedText::render(
            terminalText, key, values);
    };

    const double terminalNowSeconds =
        m_client->lastSimulationMetadata().serverTimeSeconds;

    const bool dockingVisible =
        m_clientDockingPhase != ClientDockingPhase::Idle ||
        !m_dockingGuidanceFailureReason.empty() ||
        !m_dockingStatusWarning.empty();

    // Engine HUD and terminal consume the SAME BITE source,
    // including its authoritative ship/tick provenance.
    const auto& biteState = cockpit.bite;

    const auto biteLines =
        game::presentation::DockTerminalBitePresentation::lines(
            biteState, terminalText);
    const bool biteVisible = !biteLines.empty();

    // BITE is allowed to request presentation attention, never control.
    // The same terminal surface is shared with docking diagnostics so
    // a propulsion fault remains visible even while docking is idle.
    const bool ignitionEvent =
        cockpit.valid &&
        cockpit.systems.propulsionSwitchRevision>0;
    // the author wants the terminal on the HUD all the time - "it shows that something is
    // going on" (2026-11-06); events / faults still decide its title and lines
    (void)ignitionEvent;
    terminal.visible = cockpit.valid || dockingVisible || biteVisible;
    terminal.title = terminalText(
        dockingVisible
            ? "cockpit.terminal.title"
            : "cockpit.terminal.system_title");
    terminal.biteNominalText =
        terminalText("cockpit.terminal.bite.nominal");

    for (std::size_t i = 0; i < biteLines.size(); ++i)
    {
        game::presentation::DockingTerminalSeverity severity =
            game::presentation::DockingTerminalSeverity::Info;

        if (i < biteState.faults.size())
        {
            using BiteSeverity = game::bite::Severity;
            switch (biteState.faults[i].severity)
            {
                case BiteSeverity::Failed:
                    severity =
                        game::presentation::DockingTerminalSeverity::Error;
                    break;
                case BiteSeverity::Warning:
                case BiteSeverity::Caution:
                    severity =
                        game::presentation::DockingTerminalSeverity::Warning;
                    break;
                case BiteSeverity::Advisory:
                    break;
            }
        }

        terminal.diagnostics.emplace_back(
            biteLines[i],
            severity
        );
    }

    // The command is acknowledged only by the server's next
    // replicated ShipCoreStatus, never by a local key press.
    if (ignitionEvent)
    {
        const auto& power=cockpit.systems;
        std::string message;
        game::presentation::DockingTerminalSeverity severity =
            game::presentation::DockingTerminalSeverity::Info;
        if(!power.propulsionReactorsOn)
        {
            message="REACTORS OFF / main and RCS thrust disabled";
            severity=game::presentation::DockingTerminalSeverity::Warning;
        }
        else if(power.propulsionStartupSeconds>0.0)
        {
            message="REACTORS STARTING / "+std::to_string(
                static_cast<int>(std::ceil(power.propulsionStartupSeconds)))+
                " s remaining";
        }
        else
        {
            const bool synthetic=std::any_of(
                cockpit.drives.begin(),cockpit.drives.end(),
                [](const auto& d){return d.simulatedPower;});
            const bool live=std::any_of(
                cockpit.drives.begin(),cockpit.drives.end(),
                [](const auto& d){return d.installed && !d.failed &&
                    d.integrity01>0.0f;});
            if(synthetic)
                message="REACTORS READY / diagnostic simulated 5 GW";
            else if(!live)
            {
                message="REACTOR ERROR / no functional installed ER-5";
                severity=game::presentation::DockingTerminalSeverity::Error;
            }
            else
                message="REACTORS READY / propulsion power enabled";
        }
        terminal.diagnostics.emplace_back(message,severity);
    }
    // A system fault reaches the same historical diagnostic terminal
    // as BITE. State changes and reminders are terminal policy only.
    if(cockpit.valid)
        for(const auto& alert : cockpit.systems.alerts)
            terminal.diagnostics.emplace_back(
                alert.system + ": " + alert.message,
                alert.severity>=2
                    ? game::presentation::DockingTerminalSeverity::Error
                    : game::presentation::DockingTerminalSeverity::Warning);
    // Hub dock dispatcher: the server's decision for THIS ship.
    if (playerShip.dockDispatcher.active)
    {
        const auto& dock = playerShip.dockDispatcher;
        game::presentation::DockDispatcherTerminalInput in;
        in.phase = static_cast<game::docking::traffic::DockTrafficPhase>(dock.phase);
        in.queuePosition = dock.queuePosition;
        in.queueLength = dock.queueLength;
        in.departureQueuePosition = dock.departureQueuePosition;
        in.holdPointId = dock.holdPointId;
        in.padId = dock.padId;
        // One line per dispatcher state: the terminal history keeps
        // a new entry only when the state text changes.
        // The docking widget's full phrase (the panel itself shows only values).
        std::string text = m_dockingWidgetConsoleLine;
        if (text.empty())
            for (const auto& line : game::presentation::dockDispatcherTerminalLines(in))
                text += (text.empty() ? "" : " / ") + line;
        terminal.dispatcher = text;
        terminal.visible = true;
    }
    {
        using namespace game::ship::plant;
        const auto model = playerShip.physicalControlModel;
        const auto& slots = physicalControlSlots(model);

        std::size_t aft = 0;
        std::size_t fore = 0;
        std::size_t manoeuvre = 0;
        for (const auto& slot : slots)
        {
            switch (slot.role)
            {
                case EngineSlotRole::AftMain: ++aft; break;
                case EngineSlotRole::ForeMain: ++fore; break;
                case EngineSlotRole::ManoeuvreCluster:
                    ++manoeuvre;
                    break;
            }
        }

        const bool assistedLaw =
            playerShip.renderTransform.motion.localControlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted;
        // Physical model identity is translated as flight family
        // plus its immutable I/Y/X topology code.
        const auto familyLabel = terminalText(
            familyOf(model) == PhysicalControlFamily::Assisted
                ? "cockpit.mode.assisted"
                : "cockpit.mode.newtonian"
        );
        const auto layoutName = [](PropulsionLayout layout)
        {
            switch (layout)
            {
                case PropulsionLayout::Y: return "Y";
                case PropulsionLayout::X: return "X";
                case PropulsionLayout::I:
                default: return "I";
            }
        };
        terminal.ship = lineText(
            "cockpit.terminal.ship",
            {familyLabel + "-" + layoutName(layoutOf(model)),
             terminalText(assistedLaw
                 ? "cockpit.mode.assisted"
                 : "cockpit.mode.newtonian")});
        terminal.physics = lineText(
            "cockpit.terminal.actuators",
            {std::to_string(aft + fore),
             std::to_string(aft),
             std::to_string(fore),
             std::to_string(manoeuvre)});
    }

    // The terminal intentionally occupies the legacy coordinate block
    // directly below FRONT. Preserve coordinates when docking is idle;
    // hide only those five text bindings while terminal is visible.
    for (const char* id : {
             "main_coord_cell",
             "main_coord_x",
             "main_coord_y",
             "main_coord_z",
             "main_coord_v"
         })
    {
        if (auto* component = uiRoot->findById(id))
            component->visible = !terminal.visible;
    }

    switch (m_clientDockingPhase)
    {
        case ClientDockingPhase::Idle:
            terminal.phase = terminalText("cockpit.terminal.idle");
            break;
        case ClientDockingPhase::Stabilizing:
            terminal.phase = terminalText("cockpit.terminal.stabilization");
            break;
        case ClientDockingPhase::Planning:
            terminal.phase = terminalText("cockpit.terminal.planning");
            break;
        case ClientDockingPhase::RouteReady:
            terminal.phase = terminalText("cockpit.terminal.route_ready");
            break;
        case ClientDockingPhase::Executing:
            terminal.phase = terminalText("cockpit.terminal.autopilot");
            break;
        case ClientDockingPhase::SecuringCourse:
            terminal.phase =
                m_dockAssistedSafeRecovery.stopFallback ||
                m_dockSafeCourse.stopFallback
                    ? terminalText("cockpit.terminal.stop")
                    : terminalText("cockpit.terminal.safe_course");
            break;
    }

    if (m_dockAdviceJob)
    {
        const auto& job = *m_dockAdviceJob;
        const std::uint8_t stage =
            job.planningProgressStage.load(
                std::memory_order_acquire
            );
        const double elapsedWallSeconds =
            std::chrono::duration<double>(
                std::chrono::steady_clock::now() -
                job.wallClockStart
            ).count();
        terminal.planner =
            game::presentation::DockPlannerTerminalTelemetry::running(
                terminalText, stage,
                m_dockWorkerCount->load(std::memory_order_acquire)
            );
        terminal.activity =
            game::presentation::DockPlannerTerminalTelemetry::elapsed(
                terminalText, elapsedWallSeconds, job.planningRequest
            );
        terminal.plannerDetails.push_back(
            game::presentation::DockPlannerTerminalTelemetry::presetLine(
                terminalText, job.planningRequest.assistedRoutePreset
            )
        );
        terminal.plannerDetails.push_back(
            game::presentation::DockPlannerTerminalTelemetry::limits(
                terminalText, job.planningRequest
            )
        );
        terminal.plannerDetails.push_back(
            game::presentation::DockPlannerTerminalTelemetry::obstacles(
                terminalText, job.planningRequest
            )
        );
    }
    else if (
        (m_clientDockingPhase == ClientDockingPhase::Executing ||
         m_clientDockingPhase == ClientDockingPhase::RouteReady) &&
        m_dockAdvice.plan.valid()
    )
    {
        terminal.planner =
            game::presentation::DockPlannerTerminalTelemetry::completed(
                terminalText, m_dockAdvice.workerDurationMs
            );
        terminal.activity =
            game::presentation::DockPlannerTerminalTelemetry::route(
                terminalText, m_dockAdvice.plan
            );
        terminal.plannerDetails.push_back(
            game::presentation::DockPlannerTerminalTelemetry::presetLine(
                terminalText, m_dockAdvice.assistedRoutePreset
            )
        );
        for (const auto& line : {
            game::presentation::DockPlannerTerminalTelemetry::legTimings(
                terminalText, m_dockAdvice.plan
            ),
            game::presentation::DockPlannerTerminalTelemetry::legEta(
                terminalText, m_dockAdvice.plan
            ),
            game::presentation::DockPlannerTerminalTelemetry::legEvaluations(
                terminalText, m_dockAdvice.plan
            )
        })
        {
            if (!line.empty())
                terminal.plannerDetails.push_back(line);
        }
    }
    else
    {
        terminal.planner = terminalText("cockpit.terminal.planner_off");
    }

    if (m_dockAssistedSafeRecovery.active)
    {
        terminal.safety = terminalText("cockpit.terminal.safe_course_acquiring");
    }
    else if (m_dockSafeCourse.active)
    {
        terminal.safety = terminalText("cockpit.terminal.safe_course_active");
    }
    else
    {
        terminal.safety = terminalText("cockpit.terminal.safe_course_clear");
    }

    if (m_dockLateEntry.active)
    {
        terminal.handoff =
            terminalText("cockpit.terminal.handoff_rebind") +
            " " +
            std::to_string(
                m_dockLateEntry.rebindAttempts
            );

        terminal.error =
            terminalText(
                m_dockLateEntry.rebindAttempts > 0
                    ? "cockpit.terminal.warning_handoff_retry"
                    : "cockpit.terminal.warning_handoff_rebinding"
            );

        if (m_dockLateEntry.rebindAttempts > 0)
        {
            terminal.error +=
                " " +
                std::to_string(
                    m_dockLateEntry.rebindAttempts
                );
        }
    }
    else if (m_clientDockingPhase == ClientDockingPhase::Executing)
    {
        terminal.handoff = terminalText("cockpit.terminal.handoff_accepted");
    }

    if (!m_dockingGuidanceFailureReason.empty())
    {
        // Raw failure details stay in the diagnostics log.
        // HUD must never emit an unlocalized internal sentence.
        terminal.error =
            game::presentation::DockPlannerTerminalTelemetry::failure(
                terminalText, m_dockingGuidanceFailureReason);
    }
    else if(terminal.error.empty() && m_dockAdviceJob &&
            m_dockAdviceJob->ready.load(
                std::memory_order_acquire) &&
            !m_dockAdviceJob->plan.valid())
    {
        // A worker DID run and declined the physical route:
        // show the existing localized reason category, while
        // full planner diagnostics remain in job_result/log.
        const auto& p=m_dockAdviceJob->plan;
        terminal.error=
            game::presentation::DockPlannerTerminalTelemetry::failure(
                terminalText,
                p.failure.empty()
                    ? std::string("planner route rejected")
                    : p.failure);
    }
    else if (terminal.error.empty() &&
             !m_dockingStatusWarning.empty())
    {
        // Present the SAME pre-planner reason that is logged by
        // DockAuthority; do not hide an engine-axis mismatch behind
        // "navigation error; see log".
        const char* messageKey =
            "cockpit.terminal.failure_generic";
        if(m_dockingStatusWarning ==
               "awaiting authoritative motor telemetry")
            messageKey="cockpit.terminal.warning_authority";
        else if(m_dockingStatusWarning ==
                    "working thrust axis mismatch")
            messageKey="cockpit.terminal.warning_thrust_axis_mismatch";
        else if(m_dockingStatusWarning ==
                    "installed motor matrix unavailable")
            messageKey="cockpit.terminal.warning_motor_matrix";
        else if(m_dockingStatusWarning ==
                    "installed physics envelope unavailable")
            messageKey="cockpit.terminal.warning_physics_envelope";
        else if(m_dockingStatusWarning ==
                    "physical flight doctrine rejected")
            messageKey="cockpit.terminal.warning_flight_doctrine";
        terminal.error=terminalText(messageKey);
    }

    // Presenters may compose local navigation phase strings, but
    // the actual terminal receives its complete state only from
    // MotherComputer, sharing cockpit BITE, ship identity and tick.
    game::ship::computer::MotherComputerTerminalView terminalView;
    terminalView.valid = cockpit.valid;
    terminalView.shipInstanceId = cockpit.shipInstanceId;
    terminalView.serverTick = cockpit.serverTick;
    terminalView.presentation = std::move(terminal);
    m_motherComputer.publishTerminal(std::move(terminalView));
    const auto currentTerminal = m_motherComputer.terminalSnapshot(
        cockpit.shipInstanceId,cockpit.serverTick
    );
    const auto terminalOutput =
        m_dockingStatusTerminal.update(
            currentTerminal.valid
                ? currentTerminal.presentation
                : game::presentation::DockingTerminalSnapshot {},
            terminalNowSeconds
        );

    auto& text = TextRenderer::instance();

    const float screenW =
        static_cast<float>(vp.width);
    const float screenH =
        static_cast<float>(vp.height);

    const int fontPx =
        std::clamp(
            static_cast<int>(screenH * 0.0145f),
            11,
            17
        );

    const float terminalLeft =
        screenW * 0.06f;
    const float terminalTop =
        screenH * 0.175f;
    const float terminalWidth =
        screenW * 0.25f;
    const float terminalBottom =
        screenH * 0.56f;
    const float lineStep =
        static_cast<float>(fontPx) * 1.28f;

    float y = terminalTop;

    for (const auto& line : terminalOutput.lines)
    {
        if (y + lineStep > terminalBottom)
            break;

        std::string visibleText = line.text;
        if (line.cursor)
            visibleText += "_";

        // Hard visual width budget: no frame, no clipping rectangle,
        // simply trim the text to the requested quarter-screen column.
        while (!visibleText.empty() &&
               text.measureTextPx(
                   visibleText,
                   fontPx
               ) > terminalWidth)
        {
            visibleText.pop_back();
        }

        const glm::vec4 color =
            line.severity ==
                    game::presentation::
                        DockingTerminalSeverity::Error
                ? glm::vec4(
                      0.74f,
                      0.53f,
                      0.58f,
                      0.39f
                  )
                : line.severity ==
                        game::presentation::
                            DockingTerminalSeverity::Warning
                    ? glm::vec4(
                          0.68f,
                          0.56f,
                          0.60f,
                          0.36f
                      )
                    : glm::vec4(
                          0.53f,
                          0.66f,
                          0.72f,
                          0.36f
                      );

        text.textDrawPx(
            visibleText,
            terminalLeft,
            y,
            fontPx,
            color
        );

        y += lineStep;
    }

    // Presentation-only audio seam. The terminal itself remains pure:
    // it reports how many characters appeared; the adapter rate-limits
    // that event; AudioEngine owns the replaceable platform backend.
    if (m_terminalAudioAdapter.shouldPlayTick(
            terminalOutput.typedCharacters,
            terminalNowSeconds
        ))
    {
        audio::AudioEngine::instance().playTerminalTick();
    }

    if (!terminal.visible)
        m_terminalAudioAdapter.reset();

}


void SpaceState::renderInSessionPresentationOverlay()
{
    if (!context().app)
        return;

    const Viewport& vp = context().viewport();
    const GameUiTarget target = context().app->sceneGameUiTarget();
    const auto& loc = context().app->localization();

    if (target.mode == GameUiMode::ServicePanel)
    {
        if (!renderServicePage(vp, target.service))
            m_inSessionPresentationRenderer.renderServicePanel(
                vp,
                loc,
                target.service
            );
        renderUiLanguageIndicator(vp);
        renderNavigationStressOverlay(vp);
        game::presentation::TemporaryShortcutStrip::render(vp, loc);
        return;
    }

    if (target.mode != GameUiMode::SystemMap || !m_client)
        return;

    const auto panel = buildNativeSystemMapPanelPresentation();
    m_inSessionPresentationRenderer.renderSystemMapPanel(vp, loc, panel);
    renderRoutePlanningModal(vp);
    renderUniverseTimeSimulationOverlay(vp);
    renderUiLanguageIndicator(vp);
    renderNavigationStressOverlay(vp);
    game::presentation::TemporaryShortcutStrip::render(vp, loc);
}

void SpaceState::renderRoutePlanningModal(
    const Viewport& viewport
)
{
    if (m_clientDockingPhase != ClientDockingPhase::Planning ||
        !m_dockAdviceJob ||
        viewport.width <= 0 ||
        viewport.height <= 0)
    {
        return;
    }

    const int permille = std::clamp(
        m_dockAdviceJob->planningProgressPermille.load(
            std::memory_order_acquire
        ),
        0,
        1000
    );
    const std::uint8_t stage =
        m_dockAdviceJob->planningProgressStage.load(
            std::memory_order_acquire
        );

    std::string stageText;
    switch (stage)
    {
        case 0:
            stageText = localizedUiText(
                context().app,
                "map.route_planning.preparing",
                "PREPARING ROUTE"
            );
            break;
        case 1:
            stageText = localizedUiText(
                context().app,
                "map.route_planning.geometry",
                "CALCULATING ROUTE"
            );
            break;
        case 2:
            stageText = localizedUiText(
                context().app,
                "map.route_planning.guidance",
                "BUILDING GUIDANCE"
            );
            break;
        case 3:
            stageText = localizedUiText(
                context().app,
                "map.route_planning.autopilot",
                "PREPARING AUTOPILOT"
            );
            break;
        default:
            stageText = localizedUiText(
                context().app,
                "map.route_planning.ready",
                "ROUTE READY"
            );
            break;
    }

    stageText += "  " + std::to_string(permille / 10) + "%";

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    const float screenW = static_cast<float>(viewport.width);
    const float screenH = static_cast<float>(viewport.height);

    const float modalWidth =
        std::clamp(screenW * 0.28f, 360.0f, 520.0f);
    constexpr float modalHeight = 118.0f;
    const float x = (screenW - modalWidth) * 0.5f;
    const float y = (screenH - modalHeight) * 0.5f;

    // Passive modal: no controls, no close button. The map remains visible
    // behind it and the window disappears only when planning completes/fails.
    text.solidRectPx(
        x - 2.0f,
        y - 2.0f,
        modalWidth + 4.0f,
        modalHeight + 4.0f,
        glm::vec4(0.22f, 0.52f, 0.72f, 0.72f)
    );
    text.solidRectPx(
        x,
        y,
        modalWidth,
        modalHeight,
        glm::vec4(0.018f, 0.035f, 0.052f, 0.96f)
    );

    const std::string title = localizedUiText(
        context().app,
        "map.route_planning.title",
        "ROUTE CALCULATION"
    );

    constexpr int titlePx = 20;
    constexpr int stagePx = 15;
    const float titleWidth =
        text.measureTextPx(title, titlePx);
    text.textDrawPx(
        title,
        x + (modalWidth - titleWidth) * 0.5f,
        y + 18.0f,
        titlePx,
        glm::vec4(0.90f, 0.96f, 1.0f, 1.0f)
    );

    const float stageWidth =
        text.measureTextPx(stageText, stagePx);
    text.textDrawPx(
        stageText,
        x + (modalWidth - stageWidth) * 0.5f,
        y + 52.0f,
        stagePx,
        glm::vec4(0.66f, 0.84f, 0.94f, 0.96f)
    );

    constexpr float inset = 24.0f;
    constexpr float barHeight = 12.0f;
    const float barX = x + inset;
    const float barY = y + 87.0f;
    const float barWidth = modalWidth - inset * 2.0f;
    const float fillWidth =
        barWidth * (static_cast<float>(permille) / 1000.0f);

    text.solidRectPx(
        barX,
        barY,
        barWidth,
        barHeight,
        glm::vec4(0.08f, 0.13f, 0.17f, 1.0f)
    );

    if (fillWidth > 0.0f)
    {
        text.solidRectPx(
            barX,
            barY,
            fillWidth,
            barHeight,
            glm::vec4(0.35f, 0.78f, 0.94f, 1.0f)
        );
    }

    text.endFrame();
}


void SpaceState::renderNavigationStressOverlay(
    const Viewport& viewport
)
{
    if (!m_navigationStressHarness.enabled() ||
        viewport.width <= 0 ||
        viewport.height <= 0)
    {
        return;
    }

    const auto& pending =
        m_navigationWorkspace.dockingRouteRequests().pending();

    const char* phase = "IDLE";
    switch (m_clientDockingPhase)
    {
        case ClientDockingPhase::Stabilizing:
            phase =
                m_dockLateEntry.active
                    ? "ENTRY STABILIZING"
                    : "STABILIZING";
            break;
        case ClientDockingPhase::Planning:
            phase =
                m_dockLateEntry.active
                    ? "ENTRY PLANNING"
                    : "PLANNING";
            break;
        case ClientDockingPhase::RouteReady:
            phase = "ROUTE READY";
            break;
        case ClientDockingPhase::Executing:
            phase = "AUTOPILOT ACTIVE";
            break;
        case ClientDockingPhase::SecuringCourse:
            phase =
                m_dockSafeCourse.stopFallback
                    ? "EMERGENCY STOP"
                    : m_dockAdviceJob
                        ? "SECURING + PLANNING"
                        : "SECURING SAFE COURSE";
            break;
        case ClientDockingPhase::Idle:
        default:
            phase =
                m_noSafeDockingGuidanceSolution
                    ? "FAILED"
                    : "IDLE";
            break;
    }

    const game::navigation::planner::RouteStageSpan*
        blueStage = nullptr;

    for (const auto& stage : m_dockAdvice.plan.stages)
    {
        if (stage.kind ==
                game::navigation::planner::
                    RouteStageKind::VolumeTransit &&
            stage.hardContainment)
        {
            blueStage = &stage;
            break;
        }
    }

    if (m_clientDockingPhase ==
            ClientDockingPhase::Executing &&
        blueStage)
    {
        if (m_clientDockingRouteProgressMeters + 1.0 <
            blueStage->startProgressMeters)
        {
            phase = "BLUE APPROACH";
        }
        else if (m_clientDockingRouteProgressMeters <=
            blueStage->endProgressMeters + 1.0)
        {
            phase = "BLUE TRANSIT";
        }
        else
        {
            phase = "BLUE EXIT";
        }
    }

    std::string target = "none";
    if (pending.valid())
    {
        target =
            pending.target.stableObjectId +
            ":" +
            pending.target.semanticAnchorId;
    }

    double speedMps = 0.0;
    double omegaRadPerSec = 0.0;
    if (m_client)
    {
        const auto playerIt =
            m_client->world().ships().find(m_playerId.value);
        if (playerIt != m_client->world().ships().end())
        {
            const auto& transform = playerIt->second.transform;
            speedMps =
                glm::length(transform.motion.localVelocityMps);
            omegaRadPerSec =
                std::sqrt(
                    transform.pitchRate * transform.pitchRate +
                    transform.yawRate * transform.yawRate +
                    transform.rollRate * transform.rollRate
                );
        }
    }

    const int workers =
        m_dockWorkerCount
            ? m_dockWorkerCount->load(std::memory_order_acquire)
            : 0;

    std::ostringstream line1;
    if (m_navigationStressHarness.simpleCorridor() &&
        m_navigationStressHarness.matrixRunTotal() > 0)
    {
        line1
            << "PHYSICAL MATRIX  "
            << m_navigationStressHarness.matrixRunIndex()
            << "/"
            << m_navigationStressHarness.matrixRunTotal()
            << "  "
            << m_navigationStressHarness.physicalModelLabel()
            << "  "
            << phase;
    }
    else
    {
        line1
            << "NAV STRESS  "
            << m_navigationStressHarness.currentStep()
            << "/"
            << m_navigationStressHarness.totalStepsForScenario()
            << "  "
            << phase;
    }

    std::ostringstream line2;
    line2
        << target
        << "   v="
        << std::fixed << std::setprecision(1)
        << speedMps
        << " m/s   omega="
        << std::setprecision(4)
        << omegaRadPerSec
        << " rad/s   workers="
        << workers;

    std::string line3;
    if (m_noSafeDockingGuidanceSolution &&
        !m_dockingGuidanceFailureReason.empty())
    {
        line3 = "FAIL: " + m_dockingGuidanceFailureReason;
    }
    else if (blueStage &&
        m_clientDockingPhase == ClientDockingPhase::Executing)
    {
        std::ostringstream blue;
        blue
            << "BLUE "
            << blueStage->volumeId
            << "  s="
            << std::fixed << std::setprecision(0)
            << m_clientDockingRouteProgressMeters
            << "  ["
            << blueStage->startProgressMeters
            << ", "
            << blueStage->endProgressMeters
            << "] m";
        line3 = blue.str();
    }
    else if (m_navigationStressHarness.complete())
    {
        line3 =
            "COMPLETE  " +
            m_navigationStressHarness.physicalModelLabel();
    }
    else if (m_navigationStressHarness.simpleCorridor())
    {
        line3 =
            "one live corridor pass; report: " +
            m_navigationStressHarness.reportPath();
    }
    else
    {
        line3 =
            "report: " +
            m_navigationStressHarness.reportPath();
    }

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(
        viewport.width,
        viewport.height
    );

    constexpr float x = 18.0f;
    constexpr float y = 18.0f;
    constexpr float width = 590.0f;
    constexpr float height = 92.0f;

    text.solidRectPx(
        x,
        y,
        width,
        height,
        glm::vec4(0.015f, 0.025f, 0.035f, 0.90f)
    );
    text.solidRectPx(
        x,
        y,
        5.0f,
        height,
        m_noSafeDockingGuidanceSolution
            ? glm::vec4(0.94f, 0.25f, 0.22f, 1.0f)
            : m_clientDockingPhase == ClientDockingPhase::Executing
                ? glm::vec4(0.25f, 0.92f, 0.43f, 1.0f)
                : glm::vec4(0.30f, 0.70f, 0.95f, 1.0f)
    );

    text.textDrawPx(
        line1.str(),
        x + 16.0f,
        y + 24.0f,
        18,
        glm::vec4(0.93f, 0.97f, 1.0f, 1.0f)
    );
    text.textDrawPx(
        line2.str(),
        x + 16.0f,
        y + 50.0f,
        14,
        glm::vec4(0.76f, 0.86f, 0.92f, 1.0f)
    );
    text.textDrawPx(
        line3,
        x + 16.0f,
        y + 74.0f,
        13,
        m_noSafeDockingGuidanceSolution
            ? glm::vec4(1.0f, 0.50f, 0.45f, 1.0f)
            : glm::vec4(0.60f, 0.77f, 0.87f, 1.0f)
    );

    text.endFrame();
}


namespace
{
// J HUD units by magnitude: m/s -> km/s; m -> km -> thousand / million km -> AU
std::string jSpeed(double mps, bool signedValue = false)
{
    char t[48];
    const double a = std::abs(mps);
    const char* sign = signedValue ? (mps < 0.0 ? "-" : "+") : (mps < 0.0 ? "-" : "");
    if (a < 1000.0)
        std::snprintf(t, sizeof t, "%s%.0f м/с", sign, a);
    else if (a < 100000.0)
        std::snprintf(t, sizeof t, "%s%.2f км/с", sign, a / 1000.0);
    else
        std::snprintf(t, sizeof t, "%s%.0f км/с", sign, a / 1000.0);
    return t;
}

std::string jDistance(double m)
{
    char t[48];
    const double a = std::abs(m);
    if (a < 10000.0)
        std::snprintf(t, sizeof t, "%.0f м", m);
    else if (a < 1.0e7)
        std::snprintf(t, sizeof t, "%.1f км", m / 1000.0);
    else if (a < 1.0e9)
        std::snprintf(t, sizeof t, "%.0f тыс. км", m / 1.0e6);
    else if (a < 1.5e10)
        std::snprintf(t, sizeof t, "%.2f млн км", m / 1.0e9);
    else
        std::snprintf(t, sizeof t, "%.3f а.е.", m / 1.495978707e11);
    return t;
}
}

void SpaceState::renderDockingWidget(const Viewport& viewport)
{
    if (!m_client || viewport.width <= 0 || viewport.height <= 0)
        return;
    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
        return;
    const auto& ship = it->second;
    const auto& pending = m_navigationWorkspace.dockingRouteRequests().pending();
    game::presentation::DockingWidgetInput in;
    in.requestPending = pending.valid() && pending.mode == game::navigation::DockingRouteRequest::Mode::Automatic;
    in.clientPhase = static_cast<game::presentation::DockingWidgetClientPhase>(static_cast<int>(m_clientDockingPhase));
    const auto& dock = ship.dockDispatcher;
    in.dispatcherActive = dock.active;
    in.phase = static_cast<game::docking::traffic::DockTrafficPhase>(dock.phase);
    in.queuePosition = dock.queuePosition;
    in.queueLength = dock.queueLength;
    in.departureQueuePosition = dock.departureQueuePosition;
    in.movementsAhead = dock.movementsAhead;
    in.estimatedWaitSeconds = dock.estimatedWaitSeconds;
    in.holdPointId = dock.holdPointId;
    in.padId = dock.padId;
    in.distanceKnown = dock.active && glm::length(dock.entryLocalMeters) > 1.0;
    in.distanceToEntryMeters = glm::length(dock.entryLocalMeters - ship.transform.motion.localPositionMeters);
    in.speedMps = glm::length(ship.transform.motion.localVelocityMps);
    if (m_noSafeDockingGuidanceSolution && !m_dockingGuidanceFailureReason.empty())
        in.failure = m_dockingGuidanceFailureReason;
    const auto widget = game::presentation::dockingWidgetText(in);
    m_dockingWidgetConsoleLine = widget.visible ? widget.consoleLine : std::string();
    if (!widget.visible)
        return;
    // A compact panel: title + status chip, then label / value pairs in two columns.
    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const float width = 250.0f;
    const std::size_t rows = (widget.fields.size() + 1) / 2;
    const float height = 40.0f + 34.0f * static_cast<float>(rows) + 6.0f;
    const float x = std::max(8.0f, static_cast<float>(viewport.width) - width - 24.0f);
    const float y = 170.0f;
    const glm::vec4 tone = widget.chipTone == 1 ? glm::vec4(1.0f, 0.78f, 0.30f, 1.0f)
        : widget.chipTone == 2 ? glm::vec4(0.40f, 1.0f, 0.50f, 1.0f)
        : widget.chipTone == 3 ? glm::vec4(1.0f, 0.42f, 0.32f, 1.0f)
                               : glm::vec4(0.55f, 0.88f, 1.0f, 1.0f);
    text.solidRectPx(x, y, width, height, glm::vec4(0.02f, 0.03f, 0.04f, 0.80f));
    text.solidRectPx(x, y, width, 2.0f, glm::vec4(tone.r, tone.g, tone.b, 0.9f));
    text.textDrawPx(widget.title, x + 12.0f, y + 22.0f, 12, glm::vec4(0.60f, 0.70f, 0.78f, 0.95f));
    const float chipW = text.measureTextPx(widget.chip, 14) + 16.0f;
    const float chipX = x + width - chipW - 10.0f;
    text.solidRectPx(chipX, y + 8.0f, chipW, 20.0f, glm::vec4(tone.r * 0.25f, tone.g * 0.25f, tone.b * 0.25f, 0.9f));
    text.textDrawPx(widget.chip, chipX + 8.0f, y + 23.0f, 14, tone);
    for (std::size_t i = 0; i < widget.fields.size(); ++i)
    {
        const float cx = x + 12.0f + (i % 2 == 0 ? 0.0f : width * 0.5f);
        const float cy = y + 40.0f + 34.0f * static_cast<float>(i / 2);
        text.textDrawPx(widget.fields[i].label, cx, cy + 10.0f, 10, glm::vec4(0.55f, 0.62f, 0.68f, 0.95f));
        text.textDrawPx(widget.fields[i].value, cx, cy + 28.0f, 16, glm::vec4(0.92f, 0.95f, 0.97f, 1.0f));
    }
}

void SpaceState::renderFreeFlightBanner(const Viewport& viewport)
{
    if (!m_client || viewport.width <= 0 || viewport.height <= 0)
        return;
    const auto& ships = m_client->world().ships();
    const auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;
    game::navigation::DynamicMotionState motionView = it->second.transform.motion;
    applyClientBodyAutopilotDisplay(motionView);   // the client flies the body autopilot
    const auto& motion = motionView;
    // a refused J engine is shown for a few seconds even in a hub frame
    static std::uint8_t lastRefusal = 0;
    static double refusalUntil = 0.0;
    const double now = glfwGetTime();
    if (motion.freeFlightCaptureRefusal != lastRefusal)
    {
        lastRefusal = motion.freeFlightCaptureRefusal;
        refusalUntil = lastRefusal ? now + 6.0 : 0.0;
    }
    const bool refusalShown = lastRefusal != 0 && (motion.freeFlight || now < refusalUntil);
    const int kind = motion.velocityReferenceKind;
    // the flight mode banner: normal flight, stabilization on the own orbit / over the ground;
    // the hub frame (ordinary stabilization) only while a refusal is shown
    if (!motion.freeFlight && kind != 3 && kind != 4 && !refusalShown)
        return;
    const auto shortBody = [](std::string id) {
        if (const auto dot = id.rfind('.'); dot != std::string::npos)
            id = id.substr(dot + 1);   // a body id: system_0.Sol.Земля -> Земля
        return id;
    };

    std::string reference = motion.freeFlightReferenceId;
    if (const auto hub = m_client->world().hubs().find(reference); hub != m_client->world().hubs().end() && !hub->second.name.empty())
        reference = hub->second.name;
    else if (const auto dot = reference.rfind('.'); dot != std::string::npos)
        reference = reference.substr(dot + 1);   // a body id: system_0.Sol.Земля -> Земля
    const glm::dvec3 relative = motion.travelFrame.linearVelocityMps +
        motion.travelFrame.localToWorldVector(motion.localVelocityMps) - motion.freeFlightReferenceVelocityMps;

    // the reference is the body whose sphere of influence holds the ship; beyond
    // every planet's sphere it is the star and the speed is the global one
    const bool global = motion.freeFlightReferenceIsRoot != 0;
    const char* engine = motion.jEngine ? "двигатель J" : "маршевые двигатели";
    std::ostringstream line1;
    if (!motion.freeFlight)
    {
        const double local = glm::length(motion.localVelocityMps);
        if (kind == 3)
            line1 << "СТАБИЛИЗАЦИЯ ОРБИТЫ " << shortBody(motion.freeFlightOrbitBodyId) << "  ·  манёвровая скорость "
                  << jSpeed(local) << " (предел 500 м/с)  ·  нет хаба-ориентира: орбита может уйти";
        else if (kind == 4)
            line1 << "СТАБИЛИЗАЦИЯ НАД ГРУНТОМ " << shortBody(motion.freeFlightOrbitBodyId) << "  ·  скорость над грунтом "
                  << jSpeed(local) << " (предел 500 м/с)";
        else
            line1 << "СТАБИЛИЗАЦИЯ В ХАБЕ  ·  манёвровая скорость " << jSpeed(local) << " (предел 500 м/с)";
    }
    else if (global)
        line1 << "НОРМАЛЬНЫЙ ПОЛЁТ  ·  " << engine << "  ·  глобальная скорость " << jSpeed(glm::length(relative))
              << "  ·  гравитация планет пренебрежима";
    else
        line1 << "НОРМАЛЬНЫЙ ПОЛЁТ  ·  " << engine << "  ·  скорость " << jSpeed(glm::length(relative)) << " отн. " << reference
              << " (в сфере её притяжения)";
    std::string thrust = motion.freeFlightThrust == 1 ? "ТЯГА ПОЛНАЯ" : motion.freeFlightThrust == 2 ? "ТОРМОЖЕНИЕ" : "ТЯГА ВЫКЛ";
    // the autopilot flies on the J drive, not on the latched main-engine command
    if (motion.freeFlightAutopilotPhase >= 1 && motion.freeFlightAutopilotPhase != 5)
    {
        char drive[96];
        // signed along the velocity: "-" is braking (the speed falls), "+" the speed grows
        // phase 11 (the de-orbit) flies on the main engines, the rest of the autopilot on its J drive
        std::snprintf(drive, sizeof drive, "%s %+.1f g %s",
                      motion.freeFlightAutopilotPhase == 11 ? "МАРШЕВЫЕ АВТОПИЛОТА" : "J-ПРИВОД АВТОПИЛОТА",
                      motion.freeFlightAutopilotAccelMps2 / 9.81,
                      motion.freeFlightAutopilotAccelMps2 < -0.05f ? "(торможение)" : "(разгон)");
        thrust = drive;
    }
    if (motion.jEngine && !(motion.freeFlightAutopilotPhase >= 1 && motion.freeFlightAutopilotPhase != 5))
    {
        char j[96];
        std::snprintf(j, sizeof j, "%s J %+.0f g (задано %+.0f g)", motion.jStopping ? "J ВЫКЛЮЧАЕТСЯ:" : "УСКОРЕНИЕ",
                      motion.jAccelMps2 / 9.81, motion.jAccelCommandMps2 / 9.81);
        thrust = j;
    }
    std::string routeHint;
    if (motion.routeState == 2)
    {
        char r[160];
        std::snprintf(r, sizeof r, "МАРШРУТ НА ПАУЗЕ (точка %d из %d): ручной режим  ·  продолжить: карта → финиш → ПРОДОЛЖИТЬ МАРШРУТ   ",
                      static_cast<int>(motion.routeLegIndex), static_cast<int>(motion.routeLegCount));
        routeHint = r;
    }
    if (motion.freeFlight && motion.jChargeCapacityT > 0.0)
    {
        char c[64];
        std::snprintf(c, sizeof c, "  ·  заряд J %.3f / %.0f т", motion.jChargeT, motion.jChargeCapacityT);
        line1 << c;
    }
    const char* keys = motion.jEngine
        ? "   HOME +макс · END -макс · INSERT 0 · +/- ускорение · Ctrl+J выкл J · Ctrl+K стабилизация"
        : "   HOME тяга · END торм. · INSERT выкл · PgDn посадка · Ctrl+J двигатель J · Ctrl+K стабилизация";
    std::string line2 = motion.freeFlight
        ? thrust + keys
        : std::string("Ctrl+K нормальный полёт (без предела скорости)  ·  Ctrl+J двигатель J");
    line2 = routeHint + line2;
    if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::NoLocalFrame))
        line2 = "Нет доступной локальной системы отсчёта (хаб дальше зоны захвата)   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::CaptureTooFast))
        line2 = "Захват невозможен: слишком высокая скорость относительно хаба   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::JDroppedNearObject))
        line2 = "J выключен: рядом объект   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::BelowJFloorMainEngines))
        line2 = "Ниже пола J: маршевые двигатели   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::JRefusedNearObject))
        line2 = "J не включается: рядом объект (хаб / станция 20 км, корабль 5 км)   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::JChargeEmpty))
        line2 = "J: заряд прыжкового двигателя исчерпан   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::JRefusedBelowFloor))
        line2 = "J не включается: ниже пола J   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::LandingThrustTooLow))
        line2 = "Посадка невозможна: тяги маршевых не хватит, чтобы сесть и взлететь   " + line2;
    else if (motion.freeFlightCaptureRefusal == game::navigation::code(game::navigation::FlightRefusal::LandingAtmosphere))
        line2 = "Посадка невозможна: атмосфера - садятся только челноки   " + line2;

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const float w1 = text.measureTextPx(line1.str(), 17);
    const float w2 = text.measureTextPx(line2, 14);
    const float width = std::max(w1, w2) + 40.0f;
    const float x = std::max(8.0f, (static_cast<float>(viewport.width) - width) * 0.5f);
    const float y = 96.0f;
    text.solidRectPx(x, y, width, 56.0f, glm::vec4(0.03f, 0.02f, 0.01f, 0.82f));
    text.solidRectPx(x, y, width, 2.0f, glm::vec4(1.0f, 0.62f, 0.20f, 0.95f));
    text.textDrawPx(line1.str(), x + 20.0f, y + 22.0f, 17, glm::vec4(1.0f, 0.78f, 0.45f, 1.0f));
    text.textDrawPx(line2, x + 20.0f, y + 44.0f, 14,
                    motion.freeFlightCaptureRefusal ? glm::vec4(1.0f, 0.50f, 0.42f, 1.0f) : glm::vec4(0.86f, 0.80f, 0.70f, 0.95f));
    if (!motion.freeFlight && motion.freeFlightAutopilotPhase == 0)
        return;

    // ---- J autopilot: target, phase, height, speed, drive acceleration
    if (motion.freeFlightAutopilotPhase != 0)
    {
        std::string target = motion.freeFlightAutopilotTargetId;
        if (const auto dot = target.rfind('.'); dot != std::string::npos)
            target = target.substr(dot + 1);
        static const char* phases[] = {"", "уход от тела", "перелёт", "подход", "снижение на площадку", "на поверхности",
                                       "к площадке", "выбор площадки", "орбита: нет ровной площадки, поиск",
                                       "выход из зоны хаба (J мягко)", "стабилизация над грунтом",
                                       "сход с орбиты (нормальный полёт, маршевые)", "на орбите (стабилизация), ждём схода"};
        const int phase = std::min<int>(motion.freeFlightAutopilotPhase, 12);
        char ap[256];
        if (phase == 5)
            std::snprintf(ap, sizeof ap, "АВТОПИЛОТ -> %s:  %s", target.c_str(), phases[phase]);
        else
            std::snprintf(ap, sizeof ap, "АВТОПИЛОТ -> %s:  %s   ·   %s %s   ·   скорость %s   ·   ускорение %+.1f g",
                          target.c_str(), phases[phase], phase == 6 || phase == 11 ? "до площадки" : "до поверхности",
                          jDistance(motion.freeFlightAutopilotRangeM).c_str(),
                          jSpeed(motion.freeFlightAutopilotRelSpeedMps).c_str(), motion.freeFlightAutopilotAccelMps2 / 9.81);
        const float aw = text.measureTextPx(ap, 16) + 40.0f;
        const float ax = std::max(8.0f, (static_cast<float>(viewport.width) - aw) * 0.5f);
        const float ay = y + 104.0f;
        text.solidRectPx(ax, ay, aw, 30.0f, glm::vec4(0.01f, 0.05f, 0.08f, 0.85f));
        text.solidRectPx(ax, ay, aw, 2.0f, glm::vec4(0.30f, 0.80f, 1.0f, 0.95f));
        text.textDrawPx(ap, ax + 20.0f, ay + 21.0f, 16, glm::vec4(0.62f, 0.90f, 1.0f, 1.0f));
    }

    if (!motion.freeFlight)
        return;
    // ---- orbit panel, lower right inside the HUD frame: 1st cosmic speed for this
    // body and altitude, speed, altitude / minimum safe altitude; colours warn
    // grey -> white -> yellow -> red -> crimson as the danger grows
    if (!motion.freeFlightOrbitBodyId.empty())
    {
        const glm::vec4 grey(0.62f, 0.68f, 0.74f, 0.85f), white(0.96f, 0.97f, 1.0f, 1.0f), yellow(1.0f, 0.86f, 0.30f, 1.0f),
            red(1.0f, 0.32f, 0.26f, 1.0f), crimson(0.72f, 0.04f, 0.10f, 1.0f);
        const double safe = std::max(1.0, motion.freeFlightOrbitSafeAltM);
        const double margin = (motion.freeFlightOrbitAltitudeM - safe) / safe;
        const glm::vec4 altColour = margin < 0.0 ? crimson : margin < 0.3 ? red : margin < 1.0 ? yellow : margin < 2.0 ? white : grey;
        const double ratio = motion.freeFlightOrbitCircularMps > 0.0 ? motion.freeFlightOrbitSpeedMps / motion.freeFlightOrbitCircularMps : 1.0;
        const glm::vec4 speedColour = ratio < 0.80 ? crimson : ratio < 0.90 ? red : ratio < 0.97 ? yellow
                                    : ratio < 0.995 ? white : motion.freeFlightOrbitSpeedMps > motion.freeFlightOrbitEscapeMps ? yellow : grey;
        char a[96], b[96], c[128];
        // units by magnitude (109309 m was read as "100 000 km"); the minimum is the
        // altitude where drag at orbital speed reaches 0.1 m/s2 (airless: a terrain
        // margin). Beyond every planet's sphere: altitude and 1st cosmic are dashes.
        if (global)
        {
            std::snprintf(a, sizeof a, "1-я косм.  —");
            std::snprintf(b, sizeof b, "V  %s  (глобальная)", jSpeed(motion.freeFlightOrbitSpeedMps).c_str());
            std::snprintf(c, sizeof c, "H  —");
        }
        else
        {
            std::snprintf(a, sizeof a, "1-я косм.  %s", jSpeed(motion.freeFlightOrbitCircularMps).c_str());
            std::snprintf(b, sizeof b, "V  %s   (%s верт.)", jSpeed(motion.freeFlightOrbitSpeedMps).c_str(),
                          jSpeed(motion.freeFlightOrbitRadialMps, true).c_str());
            // near the ground the height that matters is above the ground (under the
            // hull); H over the mean radius (the Moon's highlands are +5 km) misled
            if (motion.freeFlightGroundMaterial >= 0 && motion.freeFlightOrbitAltitudeM < 15000.0)
                std::snprintf(c, sizeof c, "H  %s над грунтом   (%s над ср. радиусом)",
                              jDistance(motion.freeFlightOrbitAltitudeM - motion.freeFlightGroundHeightM).c_str(),
                              jDistance(motion.freeFlightOrbitAltitudeM).c_str());
            else
                std::snprintf(c, sizeof c, "H  %s   мин. %s", jDistance(motion.freeFlightOrbitAltitudeM).c_str(), jDistance(safe).c_str());
        }
        const float px = static_cast<float>(viewport.width) * 0.70f;
        const float py = static_cast<float>(viewport.height) * 0.64f;
        text.textDrawPx(a, px, py, 14, grey);
        text.textDrawPx(b, px, py + 20.0f, 15, global ? white : speedColour);
        text.textDrawPx(c, px, py + 41.0f, 15, global ? grey : altColour);
        if (motion.freeFlightOrbitDenseAltM > 0.0)
        {
            char d[128];
            std::snprintf(d, sizeof d, "плотные слои %.1f км   сопр. %.2f м/с2",
                          motion.freeFlightOrbitDenseAltM / 1000.0, motion.freeFlightDragMps2);
            text.textDrawPx(d, px, py + 61.0f, 13, motion.freeFlightOrbitAltitudeM < motion.freeFlightOrbitDenseAltM ? crimson : grey);
            text.textDrawPx("мин.: сопр. 0.1 м/с2 на орб. скорости · плотные: 1 g",
                            px, py + 79.0f, 11, grey);
        }

        // the ground under the ship (when low): material, slope, terrain feature
        if (motion.freeFlightGroundMaterial >= 0 && motion.freeFlightOrbitAltitudeM < 15000.0)
        {
            static const char* materials[] = {"скала", "базальт", "реголит", "песок / барханы", "грунт", "растительность",
                                              "лёд", "снег", "соль", "лава"};
            const char* material = motion.freeFlightGroundMaterial == 100 ? "вода"
                : motion.freeFlightGroundMaterial < 10 ? materials[motion.freeFlightGroundMaterial] : "?";
            const char* feature = motion.freeFlightGroundFeature == 3 ? "  ·  кратер"
                : motion.freeFlightGroundFeature == 2 ? "  ·  горы"
                : motion.freeFlightGroundFeature == 4 ? "  ·  вулкан" : "";
            char g[160];
            std::snprintf(g, sizeof g, "под нами: %s, уклон %.0f°%s  ·  над рельефом %.0f м", material,
                          motion.freeFlightGroundSlopeDeg, feature,
                          motion.freeFlightOrbitAltitudeM - motion.freeFlightGroundHeightM);
            text.textDrawPx(g, px, py + 97.0f, 13,
                            motion.freeFlightGroundSlopeDeg > 25.0f || motion.freeFlightGroundMaterial == 9 ? red : white);
        }

        // terminal warning: losing altitude toward the atmosphere / surface
        std::string warning;
        if (motion.freeFlightSurfaceState == 2)
            warning = "КРУШЕНИЕ: удар о поверхность";
        else if (motion.freeFlightSurfaceState == 1)
            warning = "НА ПОВЕРХНОСТИ   HOME - взлёт";
        else if (motion.freeFlightDragMps2 > 9.81)
            warning = "ПЛОТНЫЕ СЛОИ АТМОСФЕРЫ: нагрев и удар по корпусу";
        else if (motion.freeFlightAutopilotPhase >= 1 && motion.freeFlightAutopilotPhase != 5)
            ;   // the autopilot flies the descent: its own panel tells the state
        else if (motion.freeFlightOrbitAltitudeM < 20000.0 && motion.freeFlightOrbitRadialMps < -1.0 &&
                 motion.freeFlightOrbitSpeedMps < 1500.0)
        {
            char low[128];
            std::snprintf(low, sizeof low, "СНИЖЕНИЕ: %.0f м/с вниз  (посадка < 8, выживание < 34 м/с)", -motion.freeFlightOrbitRadialMps);
            warning = low;
        }
        else if (motion.freeFlightDragMps2 > 0.1)
        {
            char atm[128];
            std::snprintf(atm, sizeof atm, "ВХОД В АТМОСФЕРУ: сопротивление %.1f м/с2 - скорость падает", motion.freeFlightDragMps2);
            warning = atm;
        }
        else if (!global && motion.freeFlightOrbitRadialMps < -1.0 && motion.freeFlightOrbitPeriapsisAltM < safe)
            warning = "ОПАСНОСТЬ СВАЛИВАНИЯ: перицентр ниже безопасной высоты - поднимите скорость";
        if (!warning.empty())
        {
            const float ww = text.measureTextPx(warning, 18) + 40.0f;
            const float wx = std::max(8.0f, (static_cast<float>(viewport.width) - ww) * 0.5f);
            text.solidRectPx(wx, y + 64.0f, ww, 32.0f, glm::vec4(0.25f, 0.0f, 0.02f, 0.85f));
            text.textDrawPx(warning, wx + 20.0f, y + 86.0f, 18, red);
        }
    }
    text.endFrame();
}

void SpaceState::renderUiLanguageIndicator(const Viewport& viewport)
{
    if (!context().app)
        return;

    const auto& localization = context().app->localization();

    std::string label =
        "UI: " + localization.languageIndicator();

    if (context().app->clientModeState().constellationsEnabled)
    {
        const std::string cultureName =
            m_sceneRenderer.constellationCultureDisplayName(
                localization.locale()
            );

        if (!cultureName.empty())
            label += "  ·  " + cultureName;
    }

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);

    constexpr int fontPx = 11;
    const float labelWidth = text.measureTextPx(label, fontPx);
    const float baselineY =
        std::max(14.0f, static_cast<float>(viewport.height) - 14.0f);
    const float textX = std::max(8.0f,
        static_cast<float>(viewport.width) - 12.0f - labelWidth);

    text.solidRectPx(
        textX - 7.0f,
        baselineY - 14.0f,
        labelWidth + 14.0f,
        20.0f,
        glm::vec4(0.01f, 0.025f, 0.04f, 0.74f)
    );

    text.textDrawPx(
        label,
        textX,
        baselineY,
        fontPx,
        glm::vec4(0.64f, 0.79f, 0.90f, 0.78f)
    );

    text.endFrame();
}

void SpaceState::renderUniverseTimeSimulationOverlay(
    const Viewport& viewport
)
{
    if (!m_client ||
        !m_client->hasSessionSnapshot() ||
        !m_client->sessionSnapshot().universeTimeSimulation)
    {
        return;
    }

    auto& text = TextRenderer::instance();

    text.beginFrameForViewport(
        viewport.width,
        viewport.height
    );

    std::ostringstream label;
    const std::string modeLabel =
        context().app
            ? context().app->localization().text(
                "hud.time_simulation_mode",
                "TIME SIMULATION MODE"
            )
            : "TIME SIMULATION MODE";
    label
        << modeLabel << "  x"
        << std::fixed
        << std::setprecision(1)
        << m_client->sessionSnapshot().universeTimeScale
        << "  |  "
        << m_client->sessionSnapshot().universeDate;

    text.textDrawPx(
        label.str(),
        24.0f,
        28.0f,
        14,
        glm::vec4(1.0f, 0.42f, 0.18f, 0.98f)
    );

    text.endFrame();
}
