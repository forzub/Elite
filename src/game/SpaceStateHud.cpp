#include "src/audio/CockpitRadio.h"
// SpaceState: Flight HUD and in-session overlays (route planning modal, navigation stress, language, universe time).

#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/game/presentation/HudSteadyNumber.h"
#include "src/game/navigation/rules/JDrive.h"
#include <cstring>
#include <chrono>
#include <algorithm>
#include "src/game/navigation/rules/CapsuleRules.h"
#include "src/game/fuel/FuelStationCatalog.h"
#include <cstdio>
#include "src/game/presentation/DockingWidgetText.h"
#include "src/game/docking/model/HexDockFacility.h"
#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/HubPlanPads.h"
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
        // the pilot's glass (the marker frame, the markers, the guidance, the cockpit and its instruments) only in the
        // front view; the rear, drone and bottom cameras are cameras (author 2026-11-21: «в остальных двух - нет панели
        // пилота и ограничительной рамки для меток. метки - не выводятся»; the bottom one wide format)
        const bool pilotGlass = m_activeCameraMode == ShipCameraMode::Cockpit;
        if (pilotGlass)
            m_playerView->renderHudBoundary();

        // -------------------------------------------------
        // 3. Подготовка матриц для меток
        // -------------------------------------------------

        if (pilotGlass)
        {
            renderHudWorldLabels(vp);
        }

        // World-anchored navigation presentation is projected onto the glass
        // before the cockpit overlay.  The cockpit frame therefore occludes
        // corridor/compass pixels outside the visible windshield instead of
        // letting them paint over the dashboard or canopy frame.
        if (debug::get().render.shouldRenderCockpit() &&
            pilotGlass &&
            m_activeMainCamera)
        {
            renderHudGuidanceAndCompass(vp, playerShip);
        }

        // the jump's star rays: on the glass, under the cockpit frame (like the guidance)
        if (pilotGlass && m_activeMainCamera)
            renderJumpStarRays(vp);

        // The recon drone is an external camera: no cockpit frame.
        if (debug::get().render.shouldRenderCockpit() &&
            pilotGlass)
        {
            m_playerView->renderCockpit();
        }

        TextRenderer::instance().beginFrameForViewport(
            vp.width,
            vp.height
        );

        if (debug::get().render.shouldRenderCockpit() &&
            pilotGlass)
        {
            renderHudCockpitInstruments(vp, playerShip, cockpit);
        }

        // ------------------------------------------------------------
        // Isolated docking status terminal.
        // Presentation-only adapter: it snapshots state into a DTO and renders
        // the pure terminal output. No navigation code can be reached from the
        // terminal module.
        // ------------------------------------------------------------
        // the bottom camera: wide format, the frame letterboxed to 2.35:1
        if (m_activeCameraMode == ShipCameraMode::Bottom)
        {
            // the landing guide: the hub's zones and the pads' capture boxes (no markers, no panel - a camera view)
            if (m_client && m_activeMainCamera)
                m_hubCorridorHudRenderer.render(m_client->world(), playerShip, m_activeMainCamera->viewMatrix(),
                                                m_activeMainCamera->projectionMatrix(), vp);
            const float band = std::max(0.0f, 0.5f * (static_cast<float>(vp.height) - static_cast<float>(vp.width) / 2.35f));
            TextRenderer::instance().solidRectPx(0.0f, 0.0f, static_cast<float>(vp.width), band, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            TextRenderer::instance().solidRectPx(0.0f, static_cast<float>(vp.height) - band, static_cast<float>(vp.width), band,
                                                 glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            // the landing readout (author 2026-11-21: «высоту от конца опор до того, что сейчас под брюхом корабля -
            // конкретно, даже пролетающий внизу корабль; максимум 500 м; скорость спуска; при превышении - красным»): a ray
            // from the hull's centre down its belly (-Y of the ship) against the hub city's roofs, pad decks and plane and
            // against the other ships (their size spheres); the leg tips are half the hull + the legs under the centre
            if (m_client)
            {
                const glm::dvec3 shipW = world::coordinates::fullMeters(playerShip.renderTransform.worldPosition);
                const glm::dvec3 relV(playerShip.renderTransform.relativeVelocityMetersPerSecond);
                const glm::dvec3 down = -glm::normalize(glm::dvec3(glm::mat3(playerShip.renderTransform.orientation)[1]));
                double half = 3.0;
                if (playerShip.descriptor)
                    if (const auto& dims = playerShip.descriptor->logicalDimensions(); dims.enabled && dims.height > 0.0)
                        half = 0.5 * dims.height;
                const double reach = 500.0 + half + game::navigation::HubPadLegLengthMeters;
                double hit = reach + 1.0;
                glm::dvec3 surfaceUp(0.0);   // the nearest city's up (the level bubble)
                for (const auto& [oid, object] : m_client->world().objects())
                {
                    if (!object.hubAttachment.valid)
                        continue;
                    const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId);
                    if (!plan)
                        continue;
                    const glm::dvec3 origin = world::coordinates::fullMeters(object.renderWorldPosition);
                    if (glm::length(shipW - origin) > 9000.0)
                        continue;
                    const glm::dmat3 R(glm::mat3(object.renderOrientation));
                    const glm::dvec3 q = glm::transpose(R) * (shipW - origin);
                    const glm::dvec3 d = glm::transpose(R) * down;
                    surfaceUp = R * glm::dvec3(0.0, 1.0, 0.0);
                    // a horizontal surface at height y spanning [x0,x1] x [z0,z1] (the plane: unbounded)
                    const auto surface = [&](double y, double x0, double x1, double z0, double z1, bool bounded) {
                        if (std::abs(d.y) < 1.0e-6)
                            return;
                        const double t = (y - q.y) / d.y;
                        if (t <= 0.0 || t >= hit)
                            return;
                        const glm::dvec3 p = q + d * t;
                        if (!bounded || (p.x >= x0 && p.x <= x1 && p.z >= z0 && p.z <= z1))
                            hit = t;
                    };
                    // the city plane within the city's bounds only (beyond its edge the planet is under the belly - live 2026-11-21:
                    // an unbounded plane read 350 m over the planet)
                    // the plane where it is built: the blocks and the streets (between them there is nothing - live 2026-11-21:
                    // the whole city rectangle read 377 m over the void)
                    for (const auto& b : plan->blocks())
                        surface(0.0, b.centre.x - b.halfMeters, b.centre.x + b.halfMeters, b.centre.y - b.halfMeters,
                                b.centre.y + b.halfMeters, true);
                    for (const auto& st : plan->streets())
                    {
                        const double w = st.avenue ? 20.0 : 13.0;
                        surface(0.0, std::min(st.a.x, st.b.x) - w, std::max(st.a.x, st.b.x) + w, std::min(st.a.y, st.b.y) - w,
                                std::max(st.a.y, st.b.y) + w, true);
                    }
                    for (const auto& m : plan->modules())
                    {
                        if (m.under)
                            continue;
                        surface(m.y1, std::min(m.rect.x, m.rect.z), std::max(m.rect.x, m.rect.z), std::min(m.rect.y, m.rect.w),
                                std::max(m.rect.y, m.rect.w), true);
                        if (m.pad)
                        {
                            const double s = 0.5 * game::navigation::HubPadDeckSide;
                            surface(m.padPosition.y, m.padPosition.x - s, m.padPosition.x + s, m.padPosition.z - s,
                                    m.padPosition.z + s, true);
                        }
                    }
                }
                // the other ships under the belly (a size sphere each)
                for (const auto& [sid, other] : m_client->world().ships())
                {
                    if (sid == m_playerId.value || !other.descriptor)
                        continue;
                    const glm::dvec3 c = world::coordinates::fullMeters(other.renderTransform.worldPosition);
                    const glm::dvec3 rel = c - shipW;
                    const double along = glm::dot(rel, down);
                    if (along <= 0.0 || along > hit + 60.0)
                        continue;
                    const auto& od = other.descriptor->logicalDimensions();
                    const double r = od.enabled ? 0.5 * std::max({static_cast<double>(od.length), static_cast<double>(od.width),
                                                                  static_cast<double>(od.height)}) : 15.0;
                    const double off2 = glm::dot(rel, rel) - along * along;
                    if (off2 > r * r)
                        continue;
                    hit = std::min(hit, along - std::sqrt(r * r - off2));
                }
                double heightM = hit - half - game::navigation::HubPadLegLengthMeters;
                {
                    // smoothed (~0.25 s): the ship and the city are interpolated apart - their difference wanders by a
                    // few metres at orbital speed and the figures shook (live 2026-11-21); a jump of the target restarts it
                    static double smoothH = -1.0;
                    static std::chrono::steady_clock::time_point smoothAt {};
                    const auto t = std::chrono::steady_clock::now();
                    const double dtH = std::chrono::duration<double>(t - smoothAt).count();
                    smoothAt = t;
                    if (hit > reach || smoothH < 0.0 || std::abs(heightM - smoothH) > 20.0 || dtH > 0.5)
                        smoothH = heightM;
                    else
                        smoothH += (heightM - smoothH) * std::min(1.0, dtH / 0.25);
                    heightM = smoothH;
                }
                // the closing speed to what is under the belly: the height's own rate (smoothed, real time) - right for a held
                // ship too (its replicated relative velocity is not the service's) and for a ship passing underneath; a jump of
                // the target (it flew out from under the belly) restarts it
                static double lastHeight = -1.0, rate = 0.0;
                static std::chrono::steady_clock::time_point lastAt {};
                const auto nowAt = std::chrono::steady_clock::now();
                const double dtS = std::chrono::duration<double>(nowAt - lastAt).count();
                if (hit <= reach && lastHeight >= 0.0 && dtS > 1.0e-3 && dtS < 0.5 && std::abs(heightM - lastHeight) < 20.0)
                    rate += (std::clamp((lastHeight - heightM) / dtS, -500.0, 500.0) - rate) * std::min(1.0, dtS / 0.3);
                else if (hit > reach)
                    rate = glm::dot(relV, down);
                lastHeight = hit <= reach ? heightM : -1.0;
                lastAt = nowAt;
                double descentMps = rate;
                // on the deck (the pad service holds the ship): exactly zero - a 260 t ship standing on its legs does not drift
                // by 0.3 m / 0.1 m/s (live 2026-11-21)
                if (playerShip.padService.rfind("phase:3", 0) == 0)
                {
                    heightM = 0.0;
                    descentMps = 0.0;
                    rate = 0.0;
                }
                char text[160];
                // steady figures (author 2026-11-21: «дрожит высотомер и спидометр - гистерезис или пониженная точность»): a
                // shown value moves only out of its band; tenths below 10 m, whole metres above
                const double shownH =
                    game::presentation::hudSteady("f4.height", std::max(0.0, heightM), 0.02, heightM < 10.0 ? 0.1 : 0.6);
                const double shownV = game::presentation::hudSteady("f4.descent", descentMps, 0.06, 0.15);
                if (hit > reach)
                    std::snprintf(text, sizeof text, "ВЫСОТА  > 500 м        СПУСК  %.1f м/с", shownV);
                else if (shownH < 10.0)
                    std::snprintf(text, sizeof text, "ВЫСОТА  %.1f м        СПУСК  %.1f м/с", shownH, shownV);
                else
                    std::snprintf(text, sizeof text, "ВЫСОТА  %.0f м        СПУСК  %.1f м/с", shownH, shownV);
                // too fast for the height: 2.5 m/s within 30 m of the surface, more allowed higher up (room to brake)
                const double allowed = game::navigation::HubPadTouchdownMaxMps + std::max(0.0, std::min(heightM, 500.0) - 30.0) * 0.2;
                const bool tooFast = descentMps > allowed;
                const glm::vec4 colour = tooFast ? glm::vec4(1.0f, 0.28f, 0.22f, 1.0f) : glm::vec4(0.80f, 0.95f, 1.0f, 0.95f);
                const int px = 24;
                auto& tr = TextRenderer::instance();
                const float w = tr.measureTextPx(text, px);
                const float y = band > 40.0f ? static_cast<float>(vp.height) - band * 0.5f + px * 0.35f
                                             : static_cast<float>(vp.height) - 40.0f;
                tr.textDrawPx(text, 0.5f * (static_cast<float>(vp.width) - w), y, px, colour);
                // the level bubble (author 2026-11-21: «пузырёк уровня, чтобы выровнять корпус относительно нижней
                // поверхности, иначе можно сесть на жопу»): the surface's up in the ship's own axes - the bubble sits in
                // the middle when the hull is level, shifts by roll (x) and pitch (y); green within 3 deg, red beyond 8
                if (glm::length(surfaceUp) > 0.5)
                {
                    const glm::dmat3 shipBasis(glm::mat3(playerShip.renderTransform.orientation));
                    const glm::dvec3 n = glm::transpose(shipBasis) * glm::normalize(surfaceUp);
                    const double tiltDeg = glm::degrees(std::acos(std::clamp(n.y, -1.0, 1.0)));
                    const float radius = 30.0f;
                    const float cx = static_cast<float>(vp.width) * 0.82f;
                    const float cy = band > 40.0f ? static_cast<float>(vp.height) - band * 0.5f : static_cast<float>(vp.height) - 60.0f;
                    const glm::vec4 ring(0.70f, 0.85f, 0.95f, 0.85f);
                    for (int k = 0; k < 36; ++k)
                    {
                        const float a = 6.2831853f * static_cast<float>(k) / 36.0f;
                        tr.solidRectPx(cx + radius * std::cos(a) - 1.0f, cy + radius * std::sin(a) - 1.0f, 2.0f, 2.0f, ring);
                    }
                    tr.solidRectPx(cx - radius, cy - 0.5f, 2.0f * radius, 1.0f, glm::vec4(ring.r, ring.g, ring.b, 0.35f));
                    tr.solidRectPx(cx - 0.5f, cy - radius, 1.0f, 2.0f * radius, glm::vec4(ring.r, ring.g, ring.b, 0.35f));
                    // 15 deg of tilt reach the ring
                    const float scale = radius / static_cast<float>(std::sin(glm::radians(15.0)));
                    const float bx = cx + std::clamp(static_cast<float>(n.x) * scale, -radius, radius);
                    const float by = cy - std::clamp(static_cast<float>(-n.z) * scale, -radius, radius);
                    const glm::vec4 bubble = tiltDeg <= 3.0   ? glm::vec4(0.35f, 1.0f, 0.45f, 1.0f)
                                           : tiltDeg <= 8.0   ? glm::vec4(1.0f, 0.85f, 0.30f, 1.0f)
                                                              : glm::vec4(1.0f, 0.28f, 0.22f, 1.0f);
                    tr.solidRectPx(bx - 5.0f, by - 5.0f, 10.0f, 10.0f, bubble);
                    char tilt[48];
                    std::snprintf(tilt, sizeof tilt, "%.0f°", game::presentation::hudSteady("f4.tilt", tiltDeg, 0.0, 0.8));
                    tr.textDrawPx(tilt, cx + radius + 8.0f, cy + 6.0f, 16, bubble);
                }
            }
        }

        {
            renderHudDockingTerminal(vp, playerShip, cockpit);

        // the landing legs, always in sight (author 2026-11-21: «какой-то индикатор на HUD, что шасси убраны / выпущены»):
        // the front view at the glass's lower right, the bottom camera over its readout band; green out, grey in, red broken
        if (m_activeCameraMode == ShipCameraMode::Cockpit || m_activeCameraMode == ShipCameraMode::Bottom)
        {
            const std::uint8_t gear = playerShip.landingGear;
            // the legs are wanted now (author 2026-11-21: «при готовности к посадке ... надо писать - выпустить опоры - и мигать
            // этим текстом, иначе не догадаются»): the service sets the ship down / asked for them, or a pad is near
            bool legsWanted = playerShip.padService.rfind("phase:2", 0) == 0 || playerShip.padService == "pad:gear_up" ||
                              playerShip.padService == "pad:no_gear";
            if (gear != 1 && !legsWanted && m_client)
            {
                const glm::dvec3 shipW = world::coordinates::fullMeters(playerShip.renderTransform.worldPosition);
                for (const auto& [oid, object] : m_client->world().objects())
                {
                    if (legsWanted || !object.hubAttachment.valid)
                        continue;
                    const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId);
                    const glm::dvec3 origin = world::coordinates::fullMeters(object.renderWorldPosition);
                    if (!plan || glm::length(shipW - origin) > 9000.0)
                        continue;
                    const glm::dvec3 q = glm::transpose(glm::dmat3(glm::mat3(object.renderOrientation))) * (shipW - origin);
                    for (const auto& m : plan->modules())
                        if (m.pad && glm::length(m.padPosition - q) < game::navigation::HubPadZoneShowMeters)
                        {
                            legsWanted = true;
                            break;
                        }
                }
            }
            const bool blinkOn = std::fmod(glfwGetTime(), 0.8) < 0.5;
            const char* label = gear == 1 ? "ОПОРЫ: ВЫПУЩЕНЫ"
                              : gear == 2 ? "ОПОРЫ: СЛОМАНЫ"
                              : legsWanted ? (blinkOn ? "ВЫПУСТИТЕ ОПОРЫ (G)" : "")
                                           : "ОПОРЫ: УБРАНЫ (G)";
            const glm::vec4 colour = gear == 1   ? glm::vec4(0.35f, 1.0f, 0.45f, 0.95f)
                                   : gear == 2 ? glm::vec4(1.0f, 0.28f, 0.22f, 1.0f)
                                   : legsWanted ? glm::vec4(1.0f, 0.62f, 0.18f, 1.0f)
                                               : glm::vec4(0.62f, 0.70f, 0.78f, 0.80f);
            auto& text = TextRenderer::instance();
            const int px = 16;
            const float w = text.measureTextPx(label, px);
            const float band = std::max(0.0f, 0.5f * (static_cast<float>(vp.height) - static_cast<float>(vp.width) / 2.35f));
            const float x = m_activeCameraMode == ShipCameraMode::Bottom ? 0.5f * (static_cast<float>(vp.width) - w)
                                                                         : static_cast<float>(vp.width) * 0.70f;
            const float y = m_activeCameraMode == ShipCameraMode::Bottom ? static_cast<float>(vp.height) - band - 12.0f
                                                                         : static_cast<float>(vp.height) * 0.70f;
            text.textDrawPx(label, x, y, px, colour);
        }
        }

        uiRoot->render(vp);

        TextRenderer::instance().endFrame();

        // the jump: the cockpit and its instruments shake / swell / are flung forward
        applyJumpScreenEffect();


















    }


    // // 3. векторные приборы
    glEnable(GL_DEPTH_TEST);

    renderUniverseTimeSimulationOverlay(vp);
    renderUiLanguageIndicator(vp);
    renderNavigationStressOverlay(vp);
    renderFreeFlightBanner(vp);
    renderContainmentBeacon(vp);
    renderHudJump(vp);
    renderDockingWidget(vp);
    renderZoneViolationBanner(vp);
    renderPilotLostOverlay(vp);
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
    auto guidance =
        game::presentation::buildGuidanceCorridorHudPresentation(
            m_navigationWorkspace,
            playerShip,
            playerShip.renderReferenceFrame.universeTimeSeconds
        );
    // DEV: ELITE_DEV_RENDER_EPOCH_CHECK=1 - the client's one-epoch invariant: the city is fixed in its hub frame, so its
    // place in the player's RENDER reference frame must not move; whatever it does is how far the drawn world and the drawn
    // ship disagree (the tunnel / frames shake by that much). Once a second: the worst frame-to-frame step and the drift.
    if (static const bool renderEpochCheck = [] { const char* v = std::getenv("ELITE_DEV_RENDER_EPOCH_CHECK"); return v && *v == '1'; }();
        renderEpochCheck && m_client && playerShip.renderReferenceFrame.valid && !playerShip.renderReferenceFrame.hubId.empty())
    {
        const auto& fr = playerShip.renderReferenceFrame;
        for (const auto& [oid, object] : m_client->world().objects())
        {
            if (!object.hubAttachment.valid || object.hubAttachment.hubId != fr.hubId)
                continue;
            const std::string& mid = object.hubAttachment.moduleId;
            if (mid.size() < 5 || mid.compare(mid.size() - 5, 5, ".city") != 0)
                continue;
            const glm::dmat3 B(fr.progradeAxis, fr.radialAxis, fr.normalAxis);
            const glm::dvec3 local = glm::transpose(B) * (world::coordinates::fullMeters(object.renderWorldPosition) - fr.originMeters);
            static glm::dvec3 prev(0.0), first(0.0);
            static bool have = false;
            static double worstStep = 0.0, worstDrift = 0.0;
            static auto logAt = std::chrono::steady_clock::now();
            if (have && glm::length(local - prev) < 5000.0)
            {
                worstStep = std::max(worstStep, glm::length(local - prev));
                worstDrift = std::max(worstDrift, glm::length(local - first));
            }
            else
                first = local;
            prev = local;
            have = true;
            if (std::chrono::steady_clock::now() - logAt > std::chrono::seconds(1))
            {
                logAt = std::chrono::steady_clock::now();
                std::cerr << "[RenderEpochCheck] the city in the player's render frame: worst step " << worstStep
                          << " m, drift " << worstDrift << " m" << std::endl;
                worstStep = worstDrift = 0.0;
                first = local;
            }
            break;
        }
    }
    // the pad service's route as the docking tunnel (author 2026-11-21: «нет рамок, показывающих маршрут»; «тоннель
    // маршрута, как при посадке в док»): the same gates along what is left of the route (hubPadRoute) from here
    // only while the service flies the ship there (phases 0-2): landed / handed over / cancelled - no tunnel
    if (m_client && playerShip.padService.size() > 8 && playerShip.padService.rfind("phase:", 0) == 0 &&
        playerShip.padService[6] != '3')
    {
        const char phase = playerShip.padService[6];
        const std::string address = playerShip.padService.substr(8);
        const glm::dvec3 shipW = world::coordinates::fullMeters(playerShip.renderTransform.worldPosition);
        for (const auto& [oid, object] : m_client->world().objects())
        {
            if (!object.hubAttachment.valid)
                continue;
            const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(object.hubAttachment.moduleId);
            if (!plan)
                continue;
            const game::navigation::HubPlanModule* pad = nullptr;
            for (const auto& m : plan->modules())
                if (m.address == address)
                    pad = &m;
            if (!pad)
                continue;
            const glm::dvec3 origin = world::coordinates::fullMeters(object.renderWorldPosition);
            const glm::dmat3 R(glm::mat3(object.renderOrientation));
            // the ship in the city frame, smoothed (~0.25 s): the ship and the city are interpolated apart, their difference
            // wanders by metres at orbital speed - the gates shook (live 2026-11-21); a big jump restarts it
            static glm::dvec3 qSmooth(0.0);
            static std::chrono::steady_clock::time_point qAt {};
            // ... in the plan's STRIP metres (a ring hub: unbent, HubRingBend.h) - the service's route is there
            const auto& ring = plan->ring();
            const glm::dvec3 qRaw = ring.toStrip(glm::transpose(R) * (shipW - origin));
            const auto qNow = std::chrono::steady_clock::now();
            const double qDt = std::chrono::duration<double>(qNow - qAt).count();
            qAt = qNow;
            if (qDt > 0.5 || glm::length(qRaw - qSmooth) > 80.0)
                qSmooth = qRaw;
            else
                qSmooth += (qRaw - qSmooth) * std::min(1.0, qDt / 0.25);
            const glm::dvec3 q = qSmooth;
            double tallest = 0.0;
            for (const auto& b : plan->blocks())
                tallest = std::max(tallest, pad->under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
            const double transit = std::max({std::min(380.0, tallest + 80.0), std::abs(pad->padPosition.y) + 60.0, game::navigation::traffic::laneTubeCeilingMeters(game::navigation::traffic::HubLaneCatalog::forFrameModule(object.hubAttachment.moduleId), pad->under) + 40.0});
            // the route the SERVER flies: computed once where the service took the ship (as GameSimulationPadLanding
            // does), then only what is left of it ahead - recomputed every frame from the moving ship it picked other
            // tower corners and jumped about, "connecting the ship to who knows what" (author 2026-11-22)
            static std::string routeKey;
            static std::vector<glm::dvec3> serviceRoute;
            static std::size_t serviceLeg = 0;
            const std::string key = std::string(object.hubAttachment.moduleId) + "/" + address;
            if (phase == '2')
                routeKey.clear();
            else if (key != routeKey)
            {
                routeKey = key;
                serviceRoute = game::navigation::hubPadRoute(
                    *plan, *pad, q,
                    game::navigation::traffic::hubPadServiceTransit(transit, std::abs(pad->padPosition.y),
                                                                    pad->under ? -q.y : q.y),
                    true);
                serviceLeg = 0;
            }
            std::vector<glm::dvec3> route;
            if (phase == '0' || phase == '1')
            {
                // the leg the ship is on: the waypoint ahead is the first one not yet passed (never goes back)
                while (serviceLeg + 1 < serviceRoute.size())
                {
                    const glm::dvec3 a = serviceRoute[serviceLeg], b = serviceRoute[serviceLeg + 1];
                    const glm::dvec3 ab = b - a;
                    const double len2 = glm::dot(ab, ab);
                    if (glm::length(q - a) < 60.0 || (len2 > 1.0e-6 && glm::dot(q - a, ab) / len2 > 0.02))
                        ++serviceLeg;
                    else
                        break;
                }
                for (std::size_t i = serviceLeg; i < serviceRoute.size(); ++i)
                    route.push_back(serviceRoute[i]);
            }
            const glm::dvec3 out = pad->under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
            if (pad->padKind == game::navigation::HubPlanModule::PadKind::Hangar)
            {
                // the bay the dispatcher gave US (our own "addr#bay@ship" in the city's occupancy), not always bay 0
                int bay = 0;
                const std::string mine = address + "#";
                const std::string who = "@" + std::to_string(m_playerId.value);
                for (const auto& b : object.occupiedBerths)
                    if (b.compare(0, mine.size(), mine) == 0 && b.find(who) != std::string::npos)
                        bay = std::atoi(b.c_str() + mine.size());
                route.push_back(game::navigation::hubHangarDoorFront(*pad, bay));
                route.push_back(game::navigation::hubHangarBayEntry(*pad, bay));
                route.push_back(game::navigation::hubBerthTouchdown(*pad, bay) + out * 3.0);
            }
            else
                route.push_back(pad->autoLanding ? pad->padPosition + out * 3.0
                                                 : pad->padPosition + out * game::navigation::HubPadZoneHeight);
            // the service flies the ship: ITS tunnel, whatever guidance was left (author 2026-11-22: «потерял тоннель на пол
            // дороги» - the route handed over 2.2 km out, a stale guidance hid the service tunnel)
            guidance.frames.clear();
            guidance.visible = true;
            guidance.spatialAdvisoryGates = true;
            guidance.corridorId = "pad:" + address;
            glm::dvec3 prev = q;
            for (const auto& wp : route)
            {
                const glm::dvec3 seg = wp - prev;
                const double len = glm::length(seg);
                if (len < 1.0)
                {
                    prev = wp;
                    continue;
                }
                const glm::dvec3 dir = seg / len;
                const glm::dvec3 upHint = std::abs(glm::dot(dir, out)) > 0.95 ? glm::dvec3(0.0, 0.0, -1.0) : out;
                const glm::dquat local = glm::quatLookAt(dir, upHint);
                for (double s = std::fmod(len, 50.0); s <= len + 1.0e-6 && guidance.frames.size() < 96; s += 50.0)
                {
                    const glm::dvec3 p = prev + dir * s;
                    const double dist = glm::length(p - q);
                    if (dist < 20.0)
                        continue;
                    game::presentation::GuidanceHudFramePresentation f;
                    f.relativeCenterMeters = R * ring.toFrame(p) + origin - shipW;
                    f.orientation = glm::quat_cast(R * ring.basisAt(p.x)) * local;
                    f.widthMeters = 40.0;
                    f.heightMeters = 30.0;
                    // translucent like the docking tunnel (author 2026-11-21: «тоннель надо сделать полупрозрачным»)
                    f.opacity = game::presentation::tunnelFrameOpacity(dist, 0.45);
                    guidance.frames.push_back(f);
                }
                prev = wp;
            }
            break;
        }
    }

    TextRenderer::instance().beginFrameForViewport(
        vp.width,
        vp.height
    );
    // the cockpit radio: its notices to the pilot, its display line (bottom left, over the panels)
    {
        auto& radio = audio::CockpitRadio::instance();
        // the reception, twice a second: the best transmitter among the hubs / beacons of this system (radio_regions.json)
        // - full signal within its reach, then fading (log of the distance) into noise at noiseFactor x the reach
        // (author 2026-11-23: «чем дальше от маяка - тем сильнее шум. как в реальности»)
        static double receptionAt = -1.0;
        if (m_client && radio.on() && radio.source() == audio::CockpitRadio::Source::Radio && glfwGetTime() - receptionAt > 0.5)
        {
            receptionAt = glfwGetTime();
            const glm::dvec3 me = world::coordinates::fullMeters(playerShip.renderTransform.worldPosition);
            std::string best;
            double bestQ = 0.0;
            for (const auto& [oid, object] : m_client->world().objects())
            {
                if (!object.hubAttachment.valid || object.systemId != playerShip.transform.motion.systemId)
                    continue;
                const auto* tx = radio.transmitter(object.hubAttachment.hubId);
                if (!tx || tx->polity.empty())
                    continue;
                const double reach = radio.rangeMeters(tx->beacon);
                const double d = glm::length(world::coordinates::fullMeters(object.renderWorldPosition) - me);
                const double q = d <= reach ? 1.0 : std::max(0.0, 1.0 - std::log10(d / reach) / std::log10(radio.noiseFactor()));
                if (q > bestQ + 1.0e-9)
                {
                    bestQ = q;
                    best = tx->polity;
                }
            }
            radio.setReception(bestQ > 0.0 ? best : std::string(), bestQ);
        }
        radio.update();
        if (std::string n = radio.takeNotice(); !n.empty())
            m_pilotNotice = n;
    }
    // the hub's corridors (green lanes / red zones), faint, under the guidance corridor
    if (m_client)
        m_hubCorridorHudRenderer.render(
            m_client->world(),
            playerShip,
            m_activeMainCamera->viewMatrix(),
            m_activeMainCamera->projectionMatrix(),
            vp
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
    // the magnitola right above the engine group (src/render/cockpit/CockpitRadioWidget.h)
    if (cockpit.valid)
        m_cockpitRadioWidget.render(m_engineThrustIndicatorRenderer.groupFrame(), vp);

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
    terminal.recoveredPrefix =
        terminalText("cockpit.terminal.recovered_prefix");

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
            message=terminalText("cockpit.terminal.reactor.off");
            severity=game::presentation::DockingTerminalSeverity::Warning;
        }
        else if(power.propulsionStartupSeconds>0.0)
        {
            message=lineText("cockpit.terminal.reactor.starting",{std::to_string(
                static_cast<int>(std::ceil(power.propulsionStartupSeconds)))});
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
                message=terminalText("cockpit.terminal.reactor.ready_simulated");
            else if(!live)
            {
                message=terminalText("cockpit.terminal.reactor.error");
                severity=game::presentation::DockingTerminalSeverity::Error;
            }
            else
                message=terminalText("cockpit.terminal.reactor.ready");
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
        // fuel (docs/FUEL_STATIONS.md): docked at a station - what it sells and Ctrl+U; a running job (a dock hose
        // or a tender alongside) - its progress; an ended job - its result for 10 s
        namespace gf = game::fuel;
        const auto& m = playerShip.transform.motion;
        const auto num = [](const char* format, double value) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), format, value);
            return std::string(buffer);
        };
        static std::uint16_t seenSerial = 0;
        static double endedAt = -1.0e30;
        if (m.refuelSerial != seenSerial)
        {
            seenSerial = m.refuelSerial;
            endedAt = glfwGetTime();
        }
        const auto phase = static_cast<gf::RefuelPhase>(m.refuelState);
        const double capacity = m.jChargeCapacityT;
        const double charge = m.jChargeT < 0.0 ? capacity : m.jChargeT;
        const bool docked = playerShip.dockDispatcher.active &&
                            playerShip.dockDispatcher.phase == static_cast<std::uint8_t>(game::docking::traffic::DockTrafficPhase::Docked);
        const auto stationName = [&](const std::string& hubId) {
            const auto* s = gf::FuelStationCatalog::instance().find(hubId);
            const std::string fallback = s ? s->name : hubId;
            return context().app ? context().app->localization().catalogName("hubs", hubId, fallback) : fallback;
        };
        using Sev = game::presentation::DockingTerminalSeverity;
        if (phase == gf::RefuelPhase::Hookup)
            terminal.diagnostics.emplace_back(lineText("cockpit.terminal.fuel.hookup", {stationName(m.hubId)}), Sev::Info);
        else if (phase == gf::RefuelPhase::Pumping)
            terminal.diagnostics.emplace_back(
                lineText("cockpit.terminal.fuel.pumping", {num("%.2f", charge), num("%.0f", capacity), num("%.1f", m.refuelRateTpm)}), Sev::Info);
        else if (phase == gf::RefuelPhase::Disconnect)
            terminal.diagnostics.emplace_back(lineText("cockpit.terminal.fuel.disconnect", {num("%.2f", m.refuelDeliveredT)}), Sev::Info);
        else if (glfwGetTime() - endedAt < 10.0 && phase == gf::RefuelPhase::Done)
            terminal.diagnostics.emplace_back(
                lineText("cockpit.terminal.fuel.done", {num("%.2f", m.refuelDeliveredT), num("%.2f", charge), num("%.0f", capacity)}), Sev::Info);
        else if (glfwGetTime() - endedAt < 10.0 && phase == gf::RefuelPhase::Cancelled)
            terminal.diagnostics.emplace_back(lineText("cockpit.terminal.fuel.cancelled", {num("%.2f", m.refuelDeliveredT)}), Sev::Warning);
        else if (glfwGetTime() - endedAt < 10.0 && phase == gf::RefuelPhase::Refused)
        {
            const std::string why = "cockpit.terminal.fuel.why." + std::to_string(m.refuelRefusal);
            terminal.diagnostics.emplace_back(lineText("cockpit.terminal.fuel.refused", {terminalText(why.c_str())}), Sev::Warning);
        }
        else if (docked && capacity > 0.0)
        {
            const auto* s = gf::FuelStationCatalog::instance().find(m.hubId);
            if (!s || s->stockT <= 0.0)
                terminal.diagnostics.emplace_back(lineText("cockpit.terminal.fuel.none", {stationName(m.hubId)}), Sev::Info);
            else if (charge >= capacity - gf::FullEpsilonT)
                terminal.diagnostics.emplace_back(lineText("cockpit.terminal.fuel.full", {num("%.0f", capacity)}), Sev::Info);
            else
                terminal.diagnostics.emplace_back(
                    lineText("cockpit.terminal.fuel.offer", {stationName(m.hubId), num("%.2f", capacity - charge),
                                                             num("%.1f", gf::refuelEtaSeconds(s->pump, capacity - charge) / 60.0)}),
                    Sev::Info);
        }
    }
    {
        // rescue zone (author 2026-11-08): in an inhabited zone the capsule is picked up in 10-20 min,
        // outside - as luck has it; the text changes only with the zone, so the history stays short
        namespace rz = game::navigation::rescue;
        const int systemId = playerShip.transform.motion.systemId;
        std::string hub, hubId;
        const auto e = rescueAt(world::coordinates::fullMeters(playerShip.transform.worldPosition), systemId, &hub,
                                &hubId);
        // the hub's display name in the UI language (localization world/star_systems "hubs")
        if (context().app && !hubId.empty())
            hub = context().app->localization().catalogName("hubs", hubId, hub);
        if (e.zone == rz::Zone::Inside)
            terminal.diagnostics.emplace_back(lineText("cockpit.terminal.sos.inside", {hub}),
                                              game::presentation::DockingTerminalSeverity::Info);
        else if (e.zone == rz::Zone::Outside)
            terminal.diagnostics.emplace_back(lineText("cockpit.terminal.sos.outside", {hub}),
                                              game::presentation::DockingTerminalSeverity::Warning);
        else
            terminal.diagnostics.emplace_back(terminalText("cockpit.terminal.sos.no_service"),
                                              game::presentation::DockingTerminalSeverity::Warning);
    }
    {
        // a notice lives a few seconds, so the same warning can be printed again later
        static std::string shown;
        static double shownAt = 0.0;
        if (m_pilotNotice != shown)
        {
            shown = m_pilotNotice;
            shownAt = glfwGetTime();
        }
        else if (!shown.empty() && glfwGetTime() - shownAt > 6.0)
            m_pilotNotice.clear();
    }
    if (!m_pilotNotice.empty())
    {
        terminal.pilotNotice = m_pilotNotice;
        terminal.visible = true;
    }
    // THE DISPATCHER'S LIGHT (author 2026-11-22: «если док занят - сообщение красным в терминале HUD и время ожидания. как
    // только док свободен - зелёный свет»): a refused place is watched - red with the wait counting down while all its
    // places are taken, green (15 s) the moment one is free
    if (m_berthWatch.active && m_client)
    {
        int places = 0, taken = 0;
        if (const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(m_berthWatch.frameModuleId))
            for (const auto& m : plan->modules())
                if (m.address == m_berthWatch.address)
                    places = game::navigation::hubBerthPlaces(m);
        for (const auto& [oid, object] : m_client->world().objects())
            if (object.hubAttachment.valid && object.hubAttachment.moduleId == m_berthWatch.frameModuleId)
                for (const auto& b : object.occupiedBerths)
                    if (b.compare(0, m_berthWatch.address.size() + 1, m_berthWatch.address + "#") == 0)
                        ++taken;
        const double now = glfwGetTime();
        if (places > 0 && taken < places)
        {
            if (m_berthWatch.freeSince < 0.0)
                m_berthWatch.freeSince = now;
            terminal.diagnostics.emplace_back("СВОБОДНО: " + m_berthWatch.address + " - место освободилось, можно садиться",
                                              game::presentation::DockingTerminalSeverity::Go);
            if (now - m_berthWatch.freeSince > 15.0)
                m_berthWatch = {};
        }
        else
        {
            m_berthWatch.freeSince = -1.0;
            // the wait in 5-minute steps: the line changes (is printed again) every 5 minutes, not every minute
            const int raw = std::max(1, m_berthWatch.waitMin - static_cast<int>((now - m_berthWatch.since) / 60.0));
            const int left = raw <= 5 ? raw : 5 * ((raw + 4) / 5);
            terminal.diagnostics.emplace_back("ЗАНЯТО: " + m_berthWatch.address + " - все места заняты, ожидание ~" +
                                                  std::to_string(left) + " мин",
                                              game::presentation::DockingTerminalSeverity::Alert);
            if (now - m_berthWatch.since > 1800.0)
                m_berthWatch = {};
        }
        terminal.visible = true;
    }
    terminal.jumpLines = m_jumpTerminalLines;
    terminal.live = m_jumpTerminalLive;
    if (!m_jumpTerminalLive.empty())
        terminal.visible = true;
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

    // author 2026-11-18: «текст терминала HUD процентов на 20 меньше, сделать его прозрачным,
    // чтобы он не перетягивал на себя внимание» - ~17 px at 1080 lines (was ~21)
    const int fontPx =
        std::clamp(
            static_cast<int>(screenH * 0.0156f),
            11,
            21
        );

    const float terminalLeft =
        screenW * 0.06f;
    // below the flight banner (the larger font reached under it)
    const float terminalTop =
        screenH * 0.205f;
    const float terminalWidth =
        screenW * 0.30f;
    const float terminalBottom =
        screenH * 0.60f;
    const float lineStep =
        static_cast<float>(fontPx) * 1.28f;

    // long lines wrap at word boundaries (they used to be cut at the column's edge); the newest
    // rows are the ones that fit
    struct TerminalRow
    {
        std::string text;
        game::presentation::DockingTerminalSeverity severity;
    };
    std::vector<TerminalRow> rows;
    for (const auto& line : terminalOutput.lines)
    {
        std::string rest = line.text;
        if (line.cursor)
            rest += "_";
        bool first = true;
        while (!rest.empty())
        {
            const std::string indent = first ? "" : "  ";
            std::string row = indent + rest;
            if (text.measureTextPx(row, fontPx) > terminalWidth)
            {
                // the longest word-boundary prefix that fits, else the longest prefix (UTF-8 safe)
                std::size_t cut = 0;
                std::size_t lastSpace = std::string::npos;
                for (std::size_t i = 0; i < rest.size();)
                {
                    std::size_t next = i + 1;
                    while (next < rest.size() && (static_cast<unsigned char>(rest[next]) & 0xC0u) == 0x80u)
                        ++next;
                    if (text.measureTextPx(indent + rest.substr(0, next), fontPx) > terminalWidth)
                        break;
                    if (rest[i] == ' ')
                        lastSpace = i;
                    cut = next;
                    i = next;
                }
                if (lastSpace != std::string::npos && lastSpace > 0 && cut < rest.size())
                    cut = lastSpace;
                if (cut == 0)
                    cut = std::min<std::size_t>(rest.size(), 1);
                row = indent + rest.substr(0, cut);
                rest = rest.substr(cut);
                while (!rest.empty() && rest.front() == ' ')
                    rest.erase(rest.begin());
            }
            else
                rest.clear();
            rows.push_back({row, line.severity});
            first = false;
        }
    }
    const std::size_t fitRows = static_cast<std::size_t>(std::max(1.0f, (terminalBottom - terminalTop) / lineStep));
    if (rows.size() > fitRows)
        rows.erase(rows.begin(), rows.begin() + static_cast<std::ptrdiff_t>(rows.size() - fitRows));

    float y = terminalTop;

    for (const auto& line : rows)
    {
        const std::string& visibleText = line.text;

        // author 2026-11-18: «сделать его прозрачным, чтобы он не перетягивал на себя внимание» (alpha ~-40%)
        const glm::vec4 color =
            line.severity == game::presentation::DockingTerminalSeverity::Alert ? glm::vec4(1.0f, 0.32f, 0.28f, 0.92f)
            : line.severity == game::presentation::DockingTerminalSeverity::Go  ? glm::vec4(0.35f, 1.0f, 0.45f, 0.92f)
            : line.severity ==
                    game::presentation::
                        DockingTerminalSeverity::Error
                ? glm::vec4(
                      0.80f,
                      0.50f,
                      0.55f,
                      0.38f
                  )
                : line.severity ==
                        game::presentation::
                            DockingTerminalSeverity::Warning
                    ? glm::vec4(
                          0.80f,
                          0.66f,
                          0.50f,
                          0.36f
                      )
                    : glm::vec4(
                          0.58f,
                          0.74f,
                          0.80f,
                          0.32f
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
using HudLookup = game::presentation::TerminalLocalizedText::Lookup;

// the active UI language's text for the flight HUD (src/assets/localization/ui/cockpit/flight.json)
HudLookup hudLookup(const Application* app)
{
    return [app](const char* key) { return localizedUiText(app, key, key); };
}

std::string hudText(const HudLookup& text, const char* key, std::initializer_list<std::string> args = {})
{
    return game::presentation::TerminalLocalizedText::render(text, key, args);
}

// a number in printf form (signs, widths) for a localized template
std::string hudNumber(const char* format, double value)
{
    char t[48];
    std::snprintf(t, sizeof t, format, value);
    return t;
}

// J HUD units by magnitude: m/s -> km/s; m -> km -> thousand / million km -> AU
std::string jSpeed(const HudLookup& text, double mps, bool signedValue = false)
{
    const double a = std::abs(mps);
    const std::string sign = signedValue ? (mps < 0.0 ? "-" : "+") : (mps < 0.0 ? "-" : "");
    if (a < 1000.0)
        return hudText(text, "cockpit.unit.speed_mps", {sign + hudNumber("%.0f", a)});
    if (a < 100000.0)
        return hudText(text, "cockpit.unit.speed_kmps", {sign + hudNumber("%.2f", a / 1000.0)});
    return hudText(text, "cockpit.unit.speed_kmps", {sign + hudNumber("%.0f", a / 1000.0)});
}

// a speed for a steady HUD line: hysteresis (0.05 %, 0.5 m/s) and 4 significant digits above 10 000 km/s
std::string jSpeedSteady(const HudLookup& text, const char* key, double mps)
{
    using namespace game::presentation;
    const double v = hudSteady(key, mps, 0.0005, 0.5);
    return jSpeed(text, std::abs(v) >= 1.0e7 ? hudSignificant(v / 1000.0, 4) * 1000.0 : v);
}

std::string jDistance(const HudLookup& text, double m)
{
    const double a = std::abs(m);
    if (a < 10000.0)
        return hudText(text, "cockpit.unit.distance_m", {hudNumber("%.0f", m)});
    if (a < 1.0e7)
        return hudText(text, "cockpit.unit.distance_km", {hudNumber("%.1f", m / 1000.0)});
    if (a < 1.0e9)
        return hudText(text, "cockpit.unit.distance_thousand_km", {hudNumber("%.0f", m / 1.0e6)});
    if (a < 1.5e10)
        return hudText(text, "cockpit.unit.distance_million_km", {hudNumber("%.2f", m / 1.0e9)});
    return hudText(text, "cockpit.unit.distance_au", {hudNumber("%.3f", m / 1.495978707e11)});
}

const char* capsuleStateKey(game::navigation::capsule::State s)
{
    using game::navigation::capsule::State;
    switch (s)
    {
    case State::PilotAlive: return "cockpit.capsule.state.pilot_alive";
    case State::PilotKilled: return "cockpit.capsule.state.pilot_killed";
    case State::Destroyed: return "cockpit.capsule.state.destroyed";
    case State::NoCapsule: return "cockpit.capsule.state.no_capsule";
    default: return nullptr;
    }
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
    in.padLanding = pending.valid() && pending.target.semanticAnchorId.rfind("pad:", 0) == 0;
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
    in.stationControlled = dock.active && dock.stationControlled;
    if (in.stationControlled && glm::length(dock.axisOutLocal) > 0.5)
    {
        // the station releases a departing ship at rest at the departure turn point
        const auto& g = game::docking::hex::hexDockGeometry();
        const double along = glm::dot(ship.transform.motion.localPositionMeters - dock.entryLocalMeters, dock.axisOutLocal);
        in.releaseKnown = true;
        in.releaseDistanceMeters = std::max(0.0, (g.entryFaceZ - g.departureAxisStopZ) - along) +
                                   std::hypot(g.departurePetalRadius, g.departurePetalZ - g.departureAxisStopZ);
        in.beyondGate = along > 30.0;
    }
    // the station let the ship go: "управление ваше" for a few seconds
    const double nowGlfw = glfwGetTime();
    if (m_lastStationControlled && !in.stationControlled && in.phase != game::docking::traffic::DockTrafficPhase::StationControl &&
        in.phase != game::docking::traffic::DockTrafficPhase::Docked)
    {
        m_stationReleasedAtSeconds = nowGlfw;
        m_pilotNotice = localizedUiText(context().app, "cockpit.notice.station_released", "cockpit.notice.station_released");
    }
    m_lastStationControlled = in.stationControlled;
    in.justReleased = m_stationReleasedAtSeconds >= 0.0 && nowGlfw - m_stationReleasedAtSeconds < 8.0;
    in.takeoverSecondsLeft = m_takeoverSecondsLeft;
    const auto widget = game::presentation::dockingWidgetText(in, hudLookup(context().app));
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
    const HudLookup lookup = hudLookup(context().app);
    const auto tr = [&lookup](const char* key, std::initializer_list<std::string> args = {}) { return hudText(lookup, key, args); };
    const std::string engine = tr(motion.jEngine ? "cockpit.flight.engine_j" : "cockpit.flight.engine_main");
    std::ostringstream line1;
    if (!motion.freeFlight)
    {
        const double local = glm::length(motion.localVelocityMps);
        if (kind == 3)
            line1 << tr("cockpit.flight.stab_orbit", {shortBody(motion.freeFlightOrbitBodyId), jSpeed(lookup, local)});
        else if (kind == 4)
            line1 << tr("cockpit.flight.stab_ground", {shortBody(motion.freeFlightOrbitBodyId), jSpeed(lookup, local)});
        else
            line1 << tr("cockpit.flight.stab_hub", {jSpeed(lookup, local)});
    }
    else if (kind == 0)
        line1 << tr("cockpit.flight.normal_hub", {engine, jSpeedSteady(lookup, "hud.line1.speed", glm::length(relative)), reference});
    else if (global)
        line1 << tr("cockpit.flight.normal_global", {engine, jSpeedSteady(lookup, "hud.line1.speed", glm::length(relative))});
    else
        line1 << tr("cockpit.flight.normal_body", {engine, jSpeedSteady(lookup, "hud.line1.speed", glm::length(relative)), reference});
    std::string thrust = tr(motion.freeFlightThrust == 1 ? "cockpit.flight.thrust_full"
                            : motion.freeFlightThrust == 2 ? "cockpit.flight.thrust_brake" : "cockpit.flight.thrust_off");
    // the autopilot flies on the J drive, not on the latched main-engine command
    if (motion.freeFlightAutopilotPhase >= 1 && motion.freeFlightAutopilotPhase != 5)
    {
        // signed along the velocity: "-" is braking (the speed falls), "+" the speed grows
        // phase 11 (the de-orbit) flies on the main engines, the rest of the autopilot on its J drive
        thrust = tr(motion.freeFlightAutopilotPhase == 11 ? "cockpit.flight.autopilot_main_drive" : "cockpit.flight.autopilot_j_drive",
                    {hudNumber("%+.1f", motion.freeFlightAutopilotAccelMps2 / 9.81)}) +
                 " " + tr(motion.freeFlightAutopilotAccelMps2 < -0.05f ? "cockpit.flight.braking_paren" : "cockpit.flight.accelerating_paren");
    }
    if (motion.jEngine && !(motion.freeFlightAutopilotPhase >= 1 && motion.freeFlightAutopilotPhase != 5))
    {
        using game::presentation::hudSignificant;
        using game::presentation::hudSteady;
        thrust = tr(motion.jStopping ? "cockpit.flight.j_stopping" : "cockpit.flight.j_accel",
                    {hudNumber("%+.0f", hudSignificant(hudSteady("hud.j.accel", motion.jAccelMps2 / 9.81, 0.01, 5.0), 3)),
                     hudNumber("%+.0f", hudSignificant(hudSteady("hud.j.command", motion.jAccelCommandMps2 / 9.81, 0.01, 1.0), 3))});
        // the medium (author 2026-11-12): above ~0.1 c the swept-up particles resist - say how much of the thrust
        // goes into holding the speed
        if (const double medium = game::navigation::jdrive::mediumResistanceMps2(glm::length(relative)); medium >= 0.5 * 9.81)
            thrust += "  ·  " + tr("cockpit.flight.medium_drag",
                                   {hudNumber("%.0f", hudSignificant(hudSteady("hud.j.medium", medium / 9.81, 0.01, 5.0), 3))});
    }
    std::string routeHint;
    if (motion.routeState == 2)
        routeHint = tr("cockpit.flight.route_paused", {std::to_string(static_cast<int>(motion.routeLegIndex)),
                                                       std::to_string(static_cast<int>(motion.routeLegCount))}) + "   ";
    if (motion.freeFlight && motion.jChargeCapacityT > 0.0)
        line1 << "  ·  " << tr("cockpit.flight.j_charge", {hudNumber("%.3f", motion.jChargeT), hudNumber("%.0f", motion.jChargeCapacityT)});
    const std::string keys = "   " + tr(motion.jEngine ? "cockpit.flight.keys_j" : "cockpit.flight.keys_main");
    std::string line2 = motion.freeFlight
        ? thrust + keys
        : tr("cockpit.flight.keys_stabilized");
    line2 = routeHint + line2;
    {
        using game::navigation::FlightRefusal;
        const char* refusalKey = nullptr;
        const auto refusal = motion.freeFlightCaptureRefusal;
        if (refusal == game::navigation::code(FlightRefusal::NoLocalFrame))
            refusalKey = "cockpit.flight.refusal.no_local_frame";
        else if (refusal == game::navigation::code(FlightRefusal::CaptureTooFast))
            refusalKey = "cockpit.flight.refusal.capture_too_fast";
        else if (refusal == game::navigation::code(FlightRefusal::JDroppedNearObject))
            refusalKey = "cockpit.flight.refusal.j_dropped_near_object";
        else if (refusal == game::navigation::code(FlightRefusal::BelowJFloorMainEngines))
            refusalKey = "cockpit.flight.refusal.below_j_floor_main";
        else if (refusal == game::navigation::code(FlightRefusal::JRefusedNearObject))
            refusalKey = "cockpit.flight.refusal.j_refused_near_object";
        else if (refusal == game::navigation::code(FlightRefusal::JChargeEmpty))
            refusalKey = "cockpit.flight.refusal.j_charge_empty";
        else if (refusal == game::navigation::code(FlightRefusal::JRefusedBelowFloor))
            refusalKey = "cockpit.flight.refusal.j_refused_below_floor";
        else if (refusal == game::navigation::code(FlightRefusal::LandingThrustTooLow))
            refusalKey = "cockpit.flight.refusal.landing_thrust_too_low";
        else if (refusal == game::navigation::code(FlightRefusal::LandingAtmosphere))
            refusalKey = "cockpit.flight.refusal.landing_atmosphere";
        if (refusalKey)
            line2 = tr(refusalKey) + "   " + line2;
    }

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
        static const char* phases[] = {nullptr, "cockpit.autopilot.phase.leave_body", "cockpit.autopilot.phase.transfer",
                                       "cockpit.autopilot.phase.approach", "cockpit.autopilot.phase.descend_pad",
                                       "cockpit.autopilot.phase.on_surface", "cockpit.autopilot.phase.to_pad",
                                       "cockpit.autopilot.phase.choose_pad", "cockpit.autopilot.phase.orbit_search",
                                       "cockpit.autopilot.phase.leave_hub", "cockpit.autopilot.phase.ground_stab",
                                       "cockpit.autopilot.phase.deorbit", "cockpit.autopilot.phase.orbit_wait"};
        const int phase = std::min<int>(motion.freeFlightAutopilotPhase, 12);
        const std::string phaseText = phases[phase] ? tr(phases[phase]) : std::string();
        std::string ap;
        if (phase == 5)
            ap = tr("cockpit.autopilot.line_short", {target, phaseText});
        else
            ap = tr("cockpit.autopilot.line",
                    {target, phaseText, tr(phase == 6 || phase == 11 ? "cockpit.autopilot.to_pad" : "cockpit.autopilot.to_surface"),
                     jDistance(lookup, motion.freeFlightAutopilotRangeM), jSpeed(lookup, motion.freeFlightAutopilotRelSpeedMps),
                     hudNumber("%+.1f", motion.freeFlightAutopilotAccelMps2 / 9.81)});
        const float aw = text.measureTextPx(ap, 16) + 40.0f;
        const float ax = std::max(8.0f, (static_cast<float>(viewport.width) - aw) * 0.5f);
        const float ay = y + 104.0f;
        text.solidRectPx(ax, ay, aw, 30.0f, glm::vec4(0.01f, 0.05f, 0.08f, 0.85f));
        text.solidRectPx(ax, ay, aw, 2.0f, glm::vec4(0.30f, 0.80f, 1.0f, 0.95f));
        text.textDrawPx(ap, ax + 20.0f, ay + 21.0f, 16, glm::vec4(0.62f, 0.90f, 1.0f, 1.0f));
        // uncharted space: the autopilot holds the speed by its sensors (docs/SENSOR_LIMITED_SPEED.md)
        if (m_bodyAutopilotDisplay.sensorSpeedLimitMps > 0.0)
        {
            // why: the hazard that set it (variant 1, 2026-11-20: uncharted space is bound only in / near a hazard)
            static const char* hazardKeys[] = {"cockpit.autopilot.hazard.belt", "cockpit.autopilot.hazard.belt", "cockpit.autopilot.hazard.rings",
                                               "cockpit.autopilot.hazard.body", "cockpit.autopilot.hazard.debris"};
            std::string why = tr(hazardKeys[std::clamp(m_sensorHazardKind, 0, 4)], {m_sensorHazardName});
            if (m_sensorHazardDistanceM > 0.0)
                why = tr("cockpit.autopilot.hazard.ahead", {why, jDistance(lookup, m_sensorHazardDistanceM)});
            const std::string sl = tr("cockpit.autopilot.sensor_speed",
                                      {jDistance(lookup, m_sensorShownRangeM), jSpeed(lookup, m_bodyAutopilotDisplay.sensorSpeedLimitMps), why});
            const float sw = text.measureTextPx(sl, 14) + 40.0f;
            const float sx = std::max(8.0f, (static_cast<float>(viewport.width) - sw) * 0.5f);
            text.solidRectPx(sx, ay + 32.0f, sw, 24.0f, glm::vec4(0.08f, 0.05f, 0.01f, 0.85f));
            text.textDrawPx(sl, sx + 20.0f, ay + 49.0f, 14, glm::vec4(1.0f, 0.80f, 0.40f, 1.0f));
        }
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
        std::string a, b, c;
        // units by magnitude (109309 m was read as "100 000 km"); the minimum is the
        // altitude where drag at orbital speed reaches 0.1 m/s2 (airless: a terrain
        // margin). Beyond every planet's sphere: altitude and 1st cosmic are dashes.
        if (global)
        {
            a = tr("cockpit.orbit_panel.circular", {"—"});
            b = tr("cockpit.orbit_panel.v_global", {jSpeedSteady(lookup, "hud.v.speed", motion.freeFlightOrbitSpeedMps)});
            c = "H  —";
        }
        else
        {
            a = tr("cockpit.orbit_panel.circular", {jSpeed(lookup, motion.freeFlightOrbitCircularMps)});
            b = tr("cockpit.orbit_panel.v_radial", {jSpeedSteady(lookup, "hud.v.speed", motion.freeFlightOrbitSpeedMps),
                                                    jSpeed(lookup, motion.freeFlightOrbitRadialMps, true)});
            // near the ground the height that matters is above the ground (under the
            // hull); H over the mean radius (the Moon's highlands are +5 km) misled
            if (motion.freeFlightGroundMaterial >= 0 && motion.freeFlightOrbitAltitudeM < 15000.0)
                c = tr("cockpit.orbit_panel.h_ground", {jDistance(lookup, motion.freeFlightOrbitAltitudeM - motion.freeFlightGroundHeightM),
                                                        jDistance(lookup, motion.freeFlightOrbitAltitudeM)});
            else
                c = tr("cockpit.orbit_panel.h_min", {jDistance(lookup, motion.freeFlightOrbitAltitudeM), jDistance(lookup, safe)});
        }
        const float px = static_cast<float>(viewport.width) * 0.70f;
        const float py = static_cast<float>(viewport.height) * 0.64f;
        text.textDrawPx(a, px, py, 14, grey);
        text.textDrawPx(b, px, py + 20.0f, 15, global ? white : speedColour);
        text.textDrawPx(c, px, py + 41.0f, 15, global ? grey : altColour);
        if (motion.freeFlightOrbitDenseAltM > 0.0)
        {
            const std::string d = tr("cockpit.orbit_panel.dense", {hudNumber("%.1f", motion.freeFlightOrbitDenseAltM / 1000.0),
                                                                   hudNumber("%.2f", motion.freeFlightDragMps2)});
            text.textDrawPx(d, px, py + 61.0f, 13, motion.freeFlightOrbitAltitudeM < motion.freeFlightOrbitDenseAltM ? crimson : grey);
            text.textDrawPx(tr("cockpit.orbit_panel.dense_hint"),
                            px, py + 79.0f, 11, grey);
        }

        // the ground under the ship (when low): material, slope, terrain feature
        if (motion.freeFlightGroundMaterial >= 0 && motion.freeFlightOrbitAltitudeM < 15000.0)
        {
            static const char* materials[] = {"cockpit.ground.material.rock", "cockpit.ground.material.basalt",
                                              "cockpit.ground.material.regolith", "cockpit.ground.material.sand",
                                              "cockpit.ground.material.soil", "cockpit.ground.material.vegetation",
                                              "cockpit.ground.material.ice", "cockpit.ground.material.snow",
                                              "cockpit.ground.material.salt", "cockpit.ground.material.lava"};
            const std::string material = motion.freeFlightGroundMaterial == 100 ? tr("cockpit.ground.material.water")
                : motion.freeFlightGroundMaterial < 10 ? tr(materials[motion.freeFlightGroundMaterial]) : std::string("?");
            const char* featureKey = motion.freeFlightGroundFeature == 3 ? "cockpit.ground.feature.crater"
                : motion.freeFlightGroundFeature == 2 ? "cockpit.ground.feature.mountains"
                : motion.freeFlightGroundFeature == 4 ? "cockpit.ground.feature.volcano" : nullptr;
            const std::string feature = featureKey ? "  ·  " + tr(featureKey) : std::string();
            const std::string g = tr("cockpit.ground.line", {material, hudNumber("%.0f", motion.freeFlightGroundSlopeDeg), feature,
                                                            hudNumber("%.0f", motion.freeFlightOrbitAltitudeM - motion.freeFlightGroundHeightM)});
            text.textDrawPx(g, px, py + 97.0f, 13,
                            motion.freeFlightGroundSlopeDeg > 25.0f || motion.freeFlightGroundMaterial == 9 ? red : white);
        }

        // terminal warning: losing altitude toward the atmosphere / surface
        std::string warning;
        if (motion.destroyedCause != 0)
        {
            namespace cap = game::navigation::capsule;
            warning = tr(motion.destroyedCause == 1 ? "cockpit.warning.destroyed_impact"
                    : motion.destroyedCause == 3 ? "cockpit.warning.disabled_combat"
                    : motion.destroyedCause == 4 ? "cockpit.warning.destroyed_collision"
                    : "cockpit.warning.destroyed_overload");
            const auto cs = static_cast<cap::State>(motion.capsuleState);
            if (cs != cap::State::None)
            {
                const char* stateKey = capsuleStateKey(cs);
                const std::string state = stateKey ? tr(stateKey) : std::string();
                std::string tail;
                if (cs == cap::State::PilotAlive || cs == cap::State::PilotKilled)
                    tail = " · " + tr("cockpit.capsule.g_tail", {state, hudNumber("%.0f", motion.capsuleBodyG), hudNumber("%.0f", cap::BodyLimitG)});
                // difficulty layer "capsule survival" (settings): the waiting game is a later slice
                if (cs == cap::State::PilotAlive && context().app && context().app->clientModeState().capsuleSurvivalEnabled)
                    tail += " · " + tr("cockpit.capsule.survival_mode");
                else
                    tail = " · " + state;
                warning += tail;
            }
        }
        else if (motion.freeFlightSurfaceState == 2)
            warning = tr("cockpit.warning.crash_impact");
        else if (motion.freeFlightSurfaceState == 1)
            warning = tr("cockpit.warning.on_surface");
        else if (motion.freeFlightDragMps2 > 9.81)
            warning = tr("cockpit.warning.dense_atmosphere");
        else if (motion.freeFlightAutopilotPhase >= 1 && motion.freeFlightAutopilotPhase != 5)
            ;   // the autopilot flies the descent: its own panel tells the state
        else if (motion.freeFlightOrbitAltitudeM < 20000.0 && motion.freeFlightOrbitRadialMps < -1.0 &&
                 motion.freeFlightOrbitSpeedMps < 1500.0)
            warning = tr("cockpit.warning.descent", {hudNumber("%.0f", -motion.freeFlightOrbitRadialMps)});
        else if (motion.freeFlightDragMps2 > 0.1)
            warning = tr("cockpit.warning.atmosphere_entry", {hudNumber("%.1f", motion.freeFlightDragMps2)});
        else if (!global && motion.freeFlightOrbitRadialMps < -1.0 && motion.freeFlightOrbitPeriapsisAltM < safe)
            warning = tr("cockpit.warning.stall");
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

// Reactor containment emergency (author 2026-11-10: "a red flashing beacon instead of a siren"): while the replicated
// ship status carries a Containment alert, a red lamp flashes at the top of the screen (2 Hz) with the alert text.
void SpaceState::renderContainmentBeacon(const Viewport& viewport)
{
    if (!m_client || viewport.width <= 0 || viewport.height <= 0)
        return;
    const auto& ships = m_client->world().ships();
    const auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;
    const game::AlertStatus* alarm = nullptr;
    for (const auto& alert : it->second.shipCoreStatus.alerts)
        if (alert.system == "Containment")
        {
            alarm = &alert;
            break;
        }
    if (!alarm)
        return;
    const double now = glfwGetTime();
    const bool on = std::fmod(now * 2.0, 1.0) < 0.55;
    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const auto bar = alarm->message.find('|');
    const std::string head = alarm->message.substr(0, bar);
    const std::string advice = bar == std::string::npos ? std::string() : alarm->message.substr(bar + 1);
    const std::string line = hudText(hudLookup(context().app), "cockpit.containment.line",
                                     {head, hudNumber("%.0f", alarm->value), hudNumber("%.0f", alarm->threshold)});
    const float tw = std::max(text.measureTextPx(line, 18), text.measureTextPx(advice, 14));
    const float w = tw + 90.0f;
    const float x = std::max(8.0f, (static_cast<float>(viewport.width) - w) * 0.5f);
    const float y = 118.0f;
    const glm::vec4 red(1.0f, 0.12f, 0.08f, 1.0f);
    text.solidRectPx(x, y, w, 58.0f, on ? glm::vec4(0.55f, 0.02f, 0.02f, 0.9f) : glm::vec4(0.18f, 0.0f, 0.0f, 0.85f));
    // the lamp: a bright square with a halo while lit
    if (on)
        text.solidRectPx(x + 10.0f, y + 12.0f, 34.0f, 34.0f, glm::vec4(1.0f, 0.25f, 0.15f, 0.35f));
    text.solidRectPx(x + 17.0f, y + 19.0f, 20.0f, 20.0f, on ? glm::vec4(1.0f, 0.2f, 0.1f, 1.0f) : glm::vec4(0.35f, 0.05f, 0.04f, 1.0f));
    text.textDrawPx(line, x + 60.0f, y + 24.0f, 18, on ? glm::vec4(1.0f, 0.9f, 0.85f, 1.0f) : red);
    text.textDrawPx(advice, x + 60.0f, y + 46.0f, 14, glm::vec4(1.0f, 0.75f, 0.65f, 1.0f));
    text.endFrame();
}
