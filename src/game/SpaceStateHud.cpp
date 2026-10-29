// SpaceState: Flight HUD and in-session overlays (route planning modal, navigation stress, language, universe time).

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

        // World-anchored navigation presentation is projected onto the glass
        // before the cockpit overlay.  The cockpit frame therefore occludes
        // corridor/compass pixels outside the visible windshield instead of
        // letting them paint over the dashboard or canopy frame.
        if (debug::get().render.shouldRenderCockpit() &&
            m_activeCameraMode != ShipCameraMode::Drone &&
            m_activeMainCamera)
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

        if (debug::get().render.shouldRenderCockpit())
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
            }
        }

        // ------------------------------------------------------------
        // Isolated docking status terminal.
        // Presentation-only adapter: it snapshots state into a DTO and renders
        // the pure terminal output. No navigation code can be reached from the
        // terminal module.
        // ------------------------------------------------------------
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
            terminal.visible = dockingVisible || biteVisible ||
                ignitionEvent ||
                (cockpit.valid && !cockpit.systems.alerts.empty());
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

        uiRoot->render(vp);

        TextRenderer::instance().endFrame();


















    }


    // // 3. векторные приборы
    glEnable(GL_DEPTH_TEST);

    renderUniverseTimeSimulationOverlay(vp);
    renderUiLanguageIndicator(vp);
    renderNavigationStressOverlay(vp);
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


void SpaceState::renderInSessionPresentationOverlay()
{
    if (!context().app)
        return;

    const Viewport& vp = context().viewport();
    const GameUiTarget target = context().app->sceneGameUiTarget();
    const auto& loc = context().app->localization();

    if (target.mode == GameUiMode::ServicePanel)
    {
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
