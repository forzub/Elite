#include "src/audio/AudioEngine.h"
#include <stdexcept>
#include "src/render/legacy/CoreGlLegacyBridge.h"
#include <glad/gl.h>
#include "src/core/RuntimeTrace.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <chrono>
#include <limits>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <new>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "SpaceState.h"
#include "core/StateStack.h"
#include "core/log.h"
#include "input/Input.h"
#include <glm/gtx/norm.hpp>

#include "render/DebugGrid.h"

#include "src/game/player/ActorIdProvider.h"
#include "src/game/player/ActorCodeGenerator.h"


#include "src/render/camera/RenderCameraViewport.h"
#include "src/debug/DebugSettings.h"
#include "src/game/debug/DebugControlSettingsCodec.h"
#include "ui/components/UIText.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "ui/components/radar/RadarWidgetBase.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/ClientObservationInterest.h"
#include "src/game/RuntimeFeatureFlags.h"

#include "src/render/InitShaders.h"

#include "src/game/ship/view/PlayerShipView.h"

#include "game/debug/AttachmentEditorPayload.h"
#include "game/debug/VolumeViewerPayload.h"
#include "ui/html/HtmlUiManager.h"
#include "ui/html/HtmlUiPanelId.h"
#include "ui/html/HtmlUiMessage.h"
#include <mutex>

#include "src/core/Application.h"
#include "src/game/session/IGameSession.h"
#include "src/game/client/ClientCelestialMapBridge.h"
#include "src/game/client/ClientGalaxyMapBridge.h"
#include "src/game/client/ClientModuleViewBuilder.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/presentation/ClientHudPresentation.h"
#include "src/game/presentation/ShipBitePresentation.h"
#include "src/game/presentation/NavigationHudPresentation.h"
#include "src/game/presentation/GuidanceHudPresentation.h"
#include "src/game/presentation/GalacticCompassPresentation.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/planner/assisted/AssistedPlanningPipeline.h"
#include "src/game/navigation/planner/newtonian/NewtonianPlanningPipeline.h"
#include "src/game/navigation/traffic/TrafficRouteGraphCatalog.h"
#include "src/game/navigation/NavigationVolumeCatalog.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"
#include "src/game/navigation/traffic/TrafficRoutePlanAdapter.h"
#include "src/game/navigation/traffic/SegmentedRouteComposer.h"
#include "src/game/navigation/LocalFlightControlStateMachine.h"
#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/RouteFrameField.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/DockingRouteEntryPolicy.h"
#include "src/game/navigation/assisted/AssistedSafeCourseExecutor.h"
#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/game/navigation/NavigationVehicleProfileAdapters.h"
#include "src/game/ship/ShipPropulsionState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/client/ClientNavigationPlanningSnapshotFactory.h"

#include <chrono>
#include <algorithm>


#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include "core/Application.h"
#include "game/scene/GameSceneSetup.h"
#include "render/HUD/TextRenderer.h"
#include "src/game/presentation/TemporaryShortcutStrip.h"

namespace
{
    using ClientStartupClock = std::chrono::steady_clock;

    double startupElapsedMs(const ClientStartupClock::time_point& begin)
    {
        return std::chrono::duration<double, std::milli>(
            ClientStartupClock::now() - begin
        ).count();
    }

    unsigned long startupProcessId()
    {
#ifdef _WIN32
        return static_cast<unsigned long>(GetCurrentProcessId());
#else
        return 0ul;
#endif
    }

    unsigned long startupForegroundProcessId()
    {
#ifdef _WIN32
        const HWND foreground = GetForegroundWindow();
        if (!foreground)
            return 0ul;

        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        return static_cast<unsigned long>(pid);
#else
        return 0ul;
#endif
    }

    void traceStartupStage(
        const char* stage,
        const ClientStartupClock::time_point& stageBegin,
        const ClientStartupClock::time_point& totalBegin)
    {
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-STARTUP][space] pid=" << startupProcessId()
                << " stage=" << stage
                << " duration_ms=" << startupElapsedMs(stageBegin)
                << " total_ms=" << startupElapsedMs(totalBegin)
    #ifdef _WIN32
                << " uptime_ms=" << static_cast<unsigned long long>(GetTickCount64())
    #endif
                << " foreground_pid=" << startupForegroundProcessId()
                << " thread=" << std::this_thread::get_id()
                << "\n";
    }

    double stableBodyPhaseRadians(const std::string& id)
    {
        uint32_t h = 2166136261u;

        for (unsigned char c : id)
        {
            h ^= c;
            h *= 16777619u;
        }

        const double t =
            static_cast<double>(h % 10000u) / 10000.0;

        return t * glm::two_pi<double>();
    }





    double nowMs()
    {
        using clock = std::chrono::high_resolution_clock;
        return std::chrono::duration<double, std::milli>(
            clock::now().time_since_epoch()
        ).count();
    }

    std::string localizedUiText(
        const Application* app,
        const std::string& key,
        const std::string& fallback
    )
    {
        return app
            ? app->localization().text(key, fallback)
            : fallback;
    }

    std::filesystem::path debugControlDefaultsPath()
    {
        namespace fs = std::filesystem;

#ifdef _WIN32
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            if (*localAppData != '\0')
            {
                return fs::path(localAppData) /
                    "EliteGame" /
                    "debug_control_defaults.json";
            }
        }
#endif

        if (const char* xdgConfig = std::getenv("XDG_CONFIG_HOME"))
        {
            if (*xdgConfig != '\0')
            {
                return fs::path(xdgConfig) /
                    "EliteGame" /
                    "debug_control_defaults.json";
            }
        }

        if (const char* home = std::getenv("HOME"))
        {
            if (*home != '\0')
            {
                return fs::path(home) /
                    ".config" /
                    "EliteGame" /
                    "debug_control_defaults.json";
            }
        }

        return fs::current_path() /
            "debug_control_defaults.json";
    }




    bool isPromo1SceneMode()
    {
        return debug::get().render.sceneMode == "promo1";
    }

    template<typename TShipMap>
    uint64_t findAnyShipEntityId(const TShipMap& ships)
    {
        if (ships.empty())
            return 0;

        return ships.begin()->first;
    }

    template<typename TShipMap>
    uint64_t findPlayerShipEntityId(const TShipMap& ships)
    {
        for (const auto& [id, ship] : ships)
        {
            if (ship.role == ShipRole::Player)
                return id;
        }

        return findAnyShipEntityId(ships);
    }




    glm::mat4 makeCameraLookOrientation(
        const glm::vec3& cameraPos,
        const glm::vec3& target,
        const glm::vec3& upHint = glm::vec3(0.0f, 1.0f, 0.0f)
    )
    {
        glm::vec3 forward =
            target - cameraPos;

        if (glm::length2(forward) < 0.000001f)
            forward = glm::vec3(0.0f, 0.0f, -1.0f);

        forward =
            glm::normalize(forward);

        glm::vec3 right =
            glm::cross(forward, upHint);

        if (glm::length2(right) < 0.000001f)
            right = glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f));

        right =
            glm::normalize(right);

        glm::vec3 up =
            glm::normalize(glm::cross(right, forward));

        glm::mat4 orientation(1.0f);

        orientation[0] = glm::vec4(right, 0.0f);
        orientation[1] = glm::vec4(up, 0.0f);
        orientation[2] = glm::vec4(-forward, 0.0f);
        orientation[3] = glm::vec4(0, 0, 0, 1);

        return orientation;
    }




    glm::vec3 safeNormalizePromo(
        const glm::vec3& v,
        const glm::vec3& fallback
    )
    {
        if (glm::length2(v) < 0.000001f)
            return fallback;

        return glm::normalize(v);
    }

    float smootherStepPromo(float x)
    {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
    }

    glm::mat4 makePromoLookOrientationClient(
        const glm::vec3& forward
    )
    {
        const glm::vec3 f =
            safeNormalizePromo(
                forward,
                glm::vec3(0.0f, 0.0f, 1.0f)
            );

        const glm::vec3 worldUp =
            glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 right =
            glm::cross(f, worldUp);

        if (glm::length2(right) < 0.000001f)
        {
            right =
                glm::cross(
                    f,
                    glm::vec3(1.0f, 0.0f, 0.0f)
                );
        }

        right =
            glm::normalize(right);

        glm::vec3 up =
            glm::normalize(glm::cross(right, f));

        glm::mat4 m(1.0f);

        // Engine convention:
        // +X = right
        // +Y = up
        // -Z = forward
        m[0] = glm::vec4(right, 0.0f);
        m[1] = glm::vec4(up, 0.0f);
        m[2] = glm::vec4(-f, 0.0f);
        m[3] = glm::vec4(0, 0, 0, 1);

        return m;
    }


    json celestialDefinitionToJson(
        const world::celestial::CelestialSystemDefinition& system
    )
    {
        json bodies = json::array();

        std::unordered_map<std::string, glm::dvec3> positions;

        int index = 0;

        for (const auto& b : system.bodies)
        {
            glm::dvec3 pos = b.staticPositionAu;

            if (b.distanceAu > 0.0)
            {
                glm::dvec3 parentPos(0.0);

                auto parentIt = positions.find(b.parentId);
                if (parentIt != positions.end())
                    parentPos = parentIt->second;

                const double phase = double(index) * 0.77;

                pos = parentPos + glm::dvec3(
                    std::cos(phase) * b.distanceAu,
                    0.0,
                    std::sin(phase) * b.distanceAu
                );
            }

            positions[b.id] = pos;

            json item;
            item["id"] = b.id;
            item["name"] = b.name;
            item["type"] = world::celestial::toString(b.type);
            item["parentId"] = b.parentId;
            item["radiusKm"] = b.radiusKm;

            item["positionAu"] = {
                {"x", pos.x},
                {"y", pos.y},
                {"z", pos.z}
            };

            bodies.push_back(std::move(item));
            ++index;
        }

        return bodies;
    }





} // namespace












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
    const bool damageScenarioConfigured =
        m_shipRuntimeDamageOracle.configure(m_runtimeScenarioSelection);

    if (!damageScenarioConfigured)
    {
        throw std::runtime_error(
            std::string(
                "invalid runtime diagnostic scenario: "
            ) +
            m_runtimeScenarioSelection.spec +
            " expected ship-damage:<PhysicalModel@slot,...>"
        );
    }

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
            << " damage_enabled="
            << (m_shipRuntimeDamageOracle.enabled() ? 1 : 0)
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


bool SpaceState::buildPlayerDetailTarget(
    world::celestial::DetailTarget& outTarget,
    bool preferReferenceContext
) const
{
    using namespace world::celestial;

    outTarget = {};

    if (!m_client)
        return false;

    const auto& nav = m_client->playerNavigation();
    if (nav.currentSystemId < 0)
        return false;

    outTarget.systemId = nav.currentSystemId;

    // Galactocentric placement is static endpoint-local catalog data. Detail
    // composition must not depend on the Galaxy overlay RPC having run first.
    if (const auto* atlas = m_client->starAtlas())
    {
        if (const auto* summary = atlas->findSystemSummary(nav.currentSystemId))
            outTarget.systemPositionLy = summary->positionLy;
    }

    const auto& ships = m_client->world().ships();
    const auto playerIt = ships.find(m_playerId.value);

    const game::navigation::DynamicMotionState* motion = nullptr;
    if (playerIt != ships.end())
        motion = &playerIt->second.transform.motion;

    if (preferReferenceContext && motion)
    {
        const std::string bodyId =
            !motion->parentBodyId.empty()
                ? motion->parentBodyId
                : motion->primaryGravityBodyId;

        const bool activeHubReference =
            motion->matchedToReferenceFrame && !motion->hubId.empty();

        if (activeHubReference && !bodyId.empty())
        {
            outTarget.sceneKind = DetailSceneKind::CelestialBody;
            outTarget.focusClass = DetailObjectClass::Hub;
            outTarget.anchorId = bodyId;
            outTarget.focusId = motion->hubId;
            return outTarget.valid();
        }

        if (!bodyId.empty())
        {
            outTarget.sceneKind = DetailSceneKind::CelestialBody;
            outTarget.focusClass = DetailObjectClass::CelestialBody;
            outTarget.anchorId = bodyId;
            outTarget.focusId = bodyId;
            return outTarget.valid();
        }

        if (activeHubReference)
        {
            outTarget.sceneKind = DetailSceneKind::LocalObject;
            outTarget.focusClass = DetailObjectClass::Hub;
            outTarget.anchorId = motion->hubId;
            outTarget.focusId = motion->hubId;
            return outTarget.valid();
        }
    }

    // No semantic body/hub target (or F12 explicitly requested local space):
    // address the exact terminal system-navigation cube that contains player.
    game::navigation::SystemNavigationGrid grid;
    grid.activateSystem(nav.currentSystemId);

    const int level = grid.maximumLevel();
    const auto index =
        grid.nearestIndexForPosition(nav.systemLocalAu, level);
    const auto cell = grid.cell(index, level);

    outTarget.sceneKind = DetailSceneKind::SpatialVolume;
    outTarget.focusClass = DetailObjectClass::None;
    outTarget.spatialCell.level = cell.level;
    outTarget.spatialCell.maximumLevel = grid.maximumLevel();
    outTarget.spatialCell.x = cell.index.x;
    outTarget.spatialCell.y = cell.index.y;
    outTarget.spatialCell.z = cell.index.z;
    outTarget.spatialCell.centerAu = cell.center;
    outTarget.spatialCell.edgeAu = cell.size;

    return outTarget.valid();
}


void SpaceState::requestGalaxyMapSnapshotOnce()
{
    if (!m_client)
        return;

    // Galaxy presentation must never wait for the jurisdiction overlay RPC.
    // Build the deterministic catalog layer immediately from the endpoint-local
    // StarAtlas, then merge the authoritative overlay whenever it arrives.
    if (!m_hasGalaxyMapSnapshot)
    {
        if (const auto* atlas = m_client->starAtlas())
        {
            world::celestial::GalaxyMapSnapshot local;
            local.universeTimeSeconds = m_client->universeTimeSeconds();
            local.universeDate = m_client->sessionSnapshot().universeDate;
            game::client::rebuildGalaxyMapCatalogLayer(local, *atlas);
            m_galaxyMapSnapshot = std::move(local);
            m_hasGalaxyMapSnapshot = true;
        }
    }

    if (m_hasGalaxyMapOverlay)
        return;

    // The request is an asynchronous overlay refresh, not an opening barrier.
    m_client->requestGalaxyMapSnapshot();
    const auto* snapshot = m_client->galaxyMapSnapshot();
    if (!snapshot)
        return;

    m_galaxyMapSnapshot = *snapshot;
    m_hasGalaxyMapSnapshot = true;
    m_hasGalaxyMapOverlay = true;
}




bool SpaceState::composeSystemMapSnapshot(int systemId)
{
    if (!m_client)
        return false;

    m_systemMapShowsEmptySector = false;
    const bool composed = m_client->composeSystemMapSnapshot(systemId);
    const auto* snapshot = m_client->systemMapSnapshot(systemId);
    const auto& metadata = m_client->systemMapMetadata();

    const bool sameTarget =
        m_hasSystemMapSnapshot && m_loadedSystemMapId == systemId;
    const bool newerSnapshot =
        snapshot != nullptr && metadata.serverTick > m_appliedSystemMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return composed && sameTarget;

        m_systemMapSnapshot = *snapshot;
        m_loadedSystemMapId = systemId;
        m_hasSystemMapSnapshot = true;
        m_appliedSystemMapServerTick = metadata.serverTick;
    }

    return m_hasSystemMapSnapshot && m_loadedSystemMapId == systemId;
}





bool SpaceState::composeDetailMapSnapshot(
    const world::celestial::DetailTarget& target)
{
    if (!m_client || !target.valid())
        return false;

    const bool composed = m_client->composeDetailMapSnapshot(target);
    const auto* snapshot = m_client->detailMapSnapshot(target);
    const auto& metadata = m_client->detailMapMetadata();

    const bool sameTarget =
        m_hasDetailMapSnapshot && m_loadedDetailTarget == target;
    const bool newerSnapshot =
        snapshot != nullptr && metadata.serverTick > m_appliedDetailMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return composed && sameTarget;

        m_authoritativeMapInterpolator.acceptDetail(
            *snapshot,
            metadata.serverTimeSeconds,
            metadata.universeTimelineRevision
        );
        m_detailMapSnapshot = m_authoritativeMapInterpolator.detail();
        m_loadedDetailTarget = target;
        m_hasDetailMapSnapshot = m_detailMapSnapshot.valid;
        m_appliedDetailMapServerTick = metadata.serverTick;
    }

    return m_hasDetailMapSnapshot && m_loadedDetailTarget == target;
}



bool SpaceState::composeHubMapSnapshot(
    int systemId,
    const std::string& hubId)
{
    if (!m_client || systemId < 0 || hubId.empty())
        return false;

    const bool composed = m_client->composeHubMapSnapshot(systemId, hubId);
    const auto* snapshot = m_client->hubMapSnapshot(systemId, hubId);
    const auto& metadata = m_client->hubMapMetadata();

    const bool sameTarget =
        m_hasHubMapSnapshot &&
        m_loadedHubMapSystemId == systemId &&
        m_loadedHubMapHubId == hubId;
    const bool newerSnapshot =
        snapshot != nullptr && metadata.serverTick > m_appliedHubMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return composed && sameTarget;

        m_authoritativeMapInterpolator.acceptHub(
            *snapshot,
            metadata.serverTimeSeconds,
            metadata.universeTimelineRevision
        );
        m_hubMapSnapshot = m_authoritativeMapInterpolator.hub();
        m_loadedHubMapSystemId = systemId;
        m_loadedHubMapHubId = hubId;
        m_hasHubMapSnapshot = m_hubMapSnapshot.valid;
        m_appliedHubMapServerTick = metadata.serverTick;
    }

    return m_hasHubMapSnapshot &&
           m_loadedHubMapSystemId == systemId &&
           m_loadedHubMapHubId == hubId;
}





void SpaceState::beginSystemMapHubTransition(
    int systemId,
    const std::string& hubId
)
{
    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, systemId, hubId]()
        {
            if (!m_client ||
                !m_hasHubMapSnapshot ||
                m_loadedHubMapSystemId != systemId ||
                m_loadedHubMapHubId != hubId)
            {
                return;
            }

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::Hub
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::Local);
        }
    );
}


void SpaceState::setSystemMapHubMode()
{
    const auto mode = m_systemMapRenderer.mode();
    if (!m_client ||
        (mode != SystemMapRenderer::Mode::System &&
         mode != SystemMapRenderer::Mode::Detail))
    {
        return;
    }

    // A local-map drill belongs to the map context that is already loaded,
    // never to the player's unrelated current navigation membership.
    const int selectedId = m_loadedSystemMapId;
    const std::string hubId = m_systemMapRenderer.selectedHubId();

    // For a free-space tactical target, HUB means "open this object's local
    // neighborhood".  The renderer has already resolved the object's current
    // position to a terminal System cube, so the correct local view is Details
    // rather than a fictitious Hub snapshot.
    if (hubId.empty())
    {
        if (mode == SystemMapRenderer::Mode::System &&
            m_systemMapRenderer.canOpenSelectedLocalContext() &&
            m_systemMapRenderer.selectedTerminalDetailCell().has_value())
        {
            setSystemMapDetailMode();
        }
        return;
    }

    // Physical Hub composition requires a real system. Synthetic negative
    // empty-sector ids remain valid for the terminal-cube branch above.
    if (selectedId < 0)
        return;

    // System -> Hub is a shortcut, not a hierarchy bypass. Prepare the exact
    // parent Details scene as well, so the Hub panel can always navigate back
    // to Details even when the user never opened that layer first.
    if (mode == SystemMapRenderer::Mode::System)
    {
        world::celestial::DetailTarget detailTarget;
        if (!buildSelectedMapDetailTarget(detailTarget) ||
            !composeDetailMapSnapshot(detailTarget))
        {
            return;
        }
    }

    if (!composeHubMapSnapshot(selectedId, hubId))
        return;

    beginSystemMapHubTransition(selectedId, hubId);
}



void SpaceState::updateLiveMapSnapshots(float dt)
{
    if (m_client && !m_hasGalaxyMapOverlay)
        requestGalaxyMapSnapshotOnce();

    if (!m_client || !m_systemMapVisible)
    {
        m_systemMapLiveRefreshTimer = 0.0;
        m_detailMapLiveRefreshTimer = 0.0;
        m_hubMapLiveRefreshTimer = 0.0;
        return;
    }

    const double refreshSeconds = std::max(
        0.02,
        static_cast<double>(debug::get().render.systemMapLiveRefreshSec)
    );

    using Mode = SystemMapRenderer::Mode;
    switch (m_systemMapRenderer.mode())
    {
        case Mode::System:
            m_detailMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;
            if (!shouldRefreshSystemMapSnapshot())
            {
                m_systemMapLiveRefreshTimer = 0.0;
                return;
            }
            m_systemMapLiveRefreshTimer += dt;
            if (m_systemMapLiveRefreshTimer >= refreshSeconds)
            {
                m_systemMapLiveRefreshTimer = 0.0;
                composeSystemMapSnapshot(m_liveSystemMapId);
            }
            return;

        case Mode::Detail:
            m_systemMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;
            if (!m_loadedDetailTarget.valid())
            {
                m_detailMapLiveRefreshTimer = 0.0;
                return;
            }
            m_detailMapLiveRefreshTimer += dt;
            if (m_detailMapLiveRefreshTimer >= refreshSeconds)
            {
                m_detailMapLiveRefreshTimer = 0.0;
                composeDetailMapSnapshot(m_loadedDetailTarget);
            }
            return;

        case Mode::Hub:
        {
            m_systemMapLiveRefreshTimer = 0.0;
            m_detailMapLiveRefreshTimer = 0.0;
            const int systemId = m_loadedHubMapSystemId;
            if (systemId < 0 || m_loadedHubMapHubId.empty())
            {
                m_hubMapLiveRefreshTimer = 0.0;
                return;
            }
            m_hubMapLiveRefreshTimer += dt;
            if (m_hubMapLiveRefreshTimer >= refreshSeconds)
            {
                m_hubMapLiveRefreshTimer = 0.0;
                composeHubMapSnapshot(systemId, m_loadedHubMapHubId);
            }
            return;
        }

        default:
            m_systemMapLiveRefreshTimer = 0.0;
            m_detailMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;
            return;
    }
}



void SpaceState::updateLocalMapPresentationSnapshots(float /*dt*/)
{
    if (m_client)
    {
        m_authoritativeMapInterpolator.update(
            m_client->renderServerTimeSeconds(),
            m_client->renderUniverseTimeSeconds()
        );
    }

    if (m_hasDetailMapSnapshot &&
        m_authoritativeMapInterpolator.hasDetail())
    {
        m_detailMapSnapshot =
            m_authoritativeMapInterpolator.detail();
    }

    if (m_hasHubMapSnapshot &&
        m_authoritativeMapInterpolator.hasHub())
    {
        m_hubMapSnapshot =
            m_authoritativeMapInterpolator.hub();
    }

    /*
        Celestial rotation is predictable state. It is reconstructed every
        client frame from the synchronized universe clock, instead of being
        held/interpolated from a low-rate map response.
    */
    if (m_client)
    {
        const auto* celestial =
            m_client->celestialSnapshot();

        if (celestial)
        {
            if (m_hasDetailMapSnapshot)
            {
                game::client::applyClientCelestialPresentation(
                    m_detailMapSnapshot,
                    *celestial
                );
            }

            if (m_hasHubMapSnapshot)
            {
                game::client::applyClientCelestialPresentation(
                    m_hubMapSnapshot,
                    *celestial
                );
            }
        }
    }
}


void SpaceState::updateSystemMapLiveFlags()
{
    const bool wasSystemMapVisible =
        m_systemMapVisible;

    m_systemMapVisible =
        context().app &&
        context().app->gameUiMode() == GameUiMode::SystemMap;

    /*
        Первый вход в карту либо вход после прыжка.

        Сам SystemMapRenderer решает:
        - сохранить старую камеру;
        - или центрировать её на кубе игрока.
    */
    if (m_systemMapVisible &&
        !wasSystemMapVisible &&
        m_client &&
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy)
    {
        // Galaxy camera entry belongs only to an actual Galaxy presentation.
        // F10-F12 may now prepare another mode before the map is revealed.
        m_systemMapRenderer.onGalaxyMapEntered(
            m_galaxyMapSnapshot,
            m_client->playerNavigation()
        );
    }

    if (!m_systemMapVisible && wasSystemMapVisible)
    {
        // A hidden map must not retain a half-finished crossfade or an
        // asynchronous user-navigation request that can mutate its mode later.
        m_systemMapRenderer.cancelMapTransition();
    }

    m_systemMapLiveSnapshotsEnabled =
        m_systemMapVisible &&
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System;

    if (!m_systemMapLiveSnapshotsEnabled)
    {
        m_liveSystemMapId = -1;
        m_systemMapLiveRefreshTimer = 0.0;
        return;
    }

    if (m_systemMapShowsEmptySector)
    {
        /*
            У пустого сектора нет серверного systemId.
            Нельзя подменять его live snapshot текущей системы
            игрока на следующем update().
        */
        m_liveSystemMapId = -1;
    }
    else if (m_systemMapRenderer.focusedSystemId() >= 0)
    {
        m_liveSystemMapId =
            m_systemMapRenderer.focusedSystemId();
    }
    else if (m_client)
    {
        m_liveSystemMapId =
            m_client->playerNavigation().currentSystemId;
    }
    else
    {
        m_liveSystemMapId = -1;
    }
}

bool SpaceState::shouldRefreshSystemMapSnapshot() const
{
    return
        m_systemMapLiveSnapshotsEnabled &&
        m_liveSystemMapId >= 0 &&
        m_client != nullptr;
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

    if (m_shipRuntimeDamageOracle.enabled())
    {
        m_playerControl = m_shipRuntimeDamageOracle.control();
        m_client->submitInput(m_playerControl);
    }
    else if (m_shipRuntimeControlOracle.enabled())
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


void SpaceState::updateDockingAdvisory()
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;

    if (!m_client)
        return;

    auto& workspace = m_navigationWorkspace;
    auto& requests = workspace.dockingRouteRequests();
    const auto& pending = requests.pending();
    auto& guidance = workspace.guidance();

    if (pending.valid() &&
        pending.serial != m_lastDockingRequestTraceSerial)
    {
        m_lastDockingRequestTraceSerial = pending.serial;
        std::cerr
            << "[DockClientFlow] observed-request"
            << " serial=" << pending.serial
            << " mode="
            << (pending.mode == DockingRouteRequest::Mode::Automatic
                    ? "automatic"
                    : "guidance")
            << " last_path_serial=" << m_lastDockingPathRequestSerial
            << " phase=" << static_cast<int>(m_clientDockingPhase)
            << " target=" << pending.target.stableObjectId
            << ":" << pending.target.semanticAnchorId
            << std::endl;
    }

    const auto eraseVisibleRoute = [&]()
    {
        if (m_dockAdviceJob)
        {
            m_dockAdviceJob->cancelled.store(
                true,
                std::memory_order_release
            );
        }

        if (!m_activeDockingGuidanceCorridorId.empty())
        {
            guidance.erase(m_activeDockingGuidanceCorridorId);
            guidance.erase(m_activeDockingGuidanceCorridorId + ":frames");
        }
        m_activeDockingGuidanceCorridorId.clear();
        m_dockAdvice = {};
        m_dockAdviceJob.reset();
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockSafeCourse = {};
        m_clientDockingRouteProgressMeters = 0.0;
        m_clientDockingRouteCurveIndex = 0;
        m_clientAutopilotCrossTrackMeters = 0.0;
        m_clientAutopilotCourseErrorRad = 0.0;
        m_clientAutopilotHullErrorRad = 0.0;
        m_clientAutopilotTargetSpeedMps = 0.0;
        m_clientAutopilotTargetLongitudinalAccelerationMps2 = 0.0;
    };

    const auto releaseClientAutopilot = [&]()
    {
        m_clientAutopilotControlActive = false;
        m_clientAutopilotControl = {};
        m_clientDockingSettledSinceServerSeconds = -1.0;
        m_clientDockingAssistedStabilizePilotController.reset();
        m_clientDockingNewtonianStabilizePilotController.reset();
        AssistedClientAutopilot::stop(m_clientAssistedRouteAutopilot);
        NewtonianClientAutopilot::stop(m_clientNewtonianRouteAutopilot);
    };

    const auto fail = [&](const std::string& reason)
    {
        std::cerr
            << "[DockClient] request="
            << (pending.valid() ? pending.serial : 0)
            << " phase=failed reason=" << reason
            << std::endl;
        m_dockingGuidanceFailureReason = reason;
        m_noSafeDockingGuidanceSolution = true;
        releaseClientAutopilot();
        m_clientDockingPhase = ClientDockingPhase::Idle;
        m_dockAdviceJob.reset();
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockSafeCourse = {};
    };

    if (!pending.valid())
    {
        if (m_clientDockingPhase != ClientDockingPhase::Idle ||
            m_clientAutopilotControlActive)
        {
            std::cerr
                << "[DockClient] phase=idle reason=no-pending-request"
                << std::endl;
        }
        releaseClientAutopilot();
        m_clientDockingPhase = ClientDockingPhase::Idle;
        m_lastDockingPathRequestSerial = 0;
        eraseVisibleRoute();
        return;
    }

    const bool automatic =
        pending.mode == DockingRouteRequest::Mode::Automatic;

    const auto playerIt =
        m_client->world().ships().find(m_playerId.value);
    if (playerIt == m_client->world().ships().end() ||
        !playerIt->second.descriptor)
    {
        fail("authoritative ship state unavailable");
        return;
    }

    const auto& player = playerIt->second;
    const auto& transform = player.transform;
    const auto& motion = transform.motion;

    if (motion.systemId != pending.target.systemId ||
        !motion.matchedToReferenceFrame ||
        motion.matchedReferenceFrameId.empty() ||
        motion.hubId.empty() ||
        motion.matchedReferenceFrameId != motion.hubId ||
        !player.referenceFrame.valid)
    {
        fail("ship is not matched to target Hub frame");
        return;
    }

    const auto* definition = m_hubSemanticAnchorCatalog.find(
        pending.target.stableObjectId,
        pending.target.semanticAnchorId
    );
    const auto* runtime = m_dockingPortRuntimeStateCatalog.find(
        pending.target.stableObjectId,
        pending.target.semanticAnchorId
    );
    if (!definition || !runtime ||
        !definition->enabled ||
        definition->kind != HubSemanticAnchorKind::DockingPort)
    {
        fail("dock state unavailable");
        return;
    }

    const auto& dim =
        player.descriptor->logicalDimensions();
    ShipDockingEnvelope hull;
    hull.valid =
        dim.enabled &&
        dim.length > 0 &&
        dim.width > 0 &&
        dim.height > 0;
    hull.lengthMeters = dim.length;
    hull.widthMeters = dim.width;
    hull.heightMeters = dim.height;

    const auto fit =
        evaluateDockingCompatibility(
            hull,
            *definition,
            *runtime
        );
    if (!fit.routeAvailable)
    {
        fail("dock unavailable or hull does not fit");
        return;
    }

    const auto plannerSlotState =
        player.physicalControlModel !=
                player.descriptor->physicalControlModel
            ? game::ship::plant::healthySlotState(
                  game::ship::plant::physicalControlSlots(
                      player.physicalControlModel
                  )
              )
            : game::ship::capability::buildPhysicalControlSlotState(
                  *player.descriptor,
                  player.physicalControlModel,
                  player.modules
              );

    const auto plannerCapability =
        game::ship::capability::buildShipCapability(
            *player.descriptor,
            player.physicalControlModel,
            plannerSlotState
        );

    ShipParams effectivePhysics =
        plannerCapability.valid
            ? plannerCapability.effectivePhysics
            : ShipParams {};

    const auto controlLaw =
        motion.localControlLaw;

    const auto frame =
        player.referenceFrame.kinematicFrame();

    const auto makeAgent = [&]() -> FollowerAgent
    {
        FollowerAgent agent;
        agent.positionMapMeters =
            motion.localPositionMeters;
        agent.velocityMapMetersPerSecond =
            motion.localVelocityMps;
        agent.forwardMap = glm::normalize(
            frame.worldToLocalVector(
                glm::dvec3(transform.forward())
            )
        );
        agent.rightMap = glm::normalize(
            frame.worldToLocalVector(
                glm::dvec3(transform.right())
            )
        );
        agent.upMap = glm::normalize(
            frame.worldToLocalVector(
                glm::dvec3(transform.up())
            )
        );
        agent.pitchRateRadPerSec = transform.pitchRate;
        agent.yawRateRadPerSec = transform.yawRate;
        agent.rollRateRadPerSec = transform.rollRate;
        return agent;
    };

    const auto submitAutopilotControl =
        [&](const ShipControlState& control)
        {
            m_clientAutopilotControl = control;
            m_clientAutopilotControlActive = true;

            // This is deliberately the same input path used by the keyboard.
            // Server physics does not know whether the sample came from a
            // human or from the client virtual pilot.
            m_client->submitInput(m_clientAutopilotControl);
        };

    const auto coastForPlanningControl = [&]()
    {
        // NEUTRAL PHYSICAL CONTROL, not a direct-navigation bypass.
        //
        // Newtonian: zero ordinary controls naturally means ballistic coast.
        // Assisted: zero ordinary controls MUST leave the normal FA-on law
        // alive.  It may remove residual lateral slip relative to the hull;
        // bypassing that with navigationAccelerationDemandValid=0 m/s^2 makes
        // Assisted behave Newtonian and visibly separates nose from velocity.
        return ShipControlState {};
    };

    const auto stabilizeAngularForPlanningControl = [&]()
    {
        const auto agent = makeAgent();

        game::navigation::autopilot::AutopilotControlRequest hold;

        // Preserve the measured translational state exactly: no linear
        // acceleration is requested while the old route is being released.
        hold.desiredVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        hold.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);

        // Freeze the CURRENT hull pose while HullAttitudeControl damps
        // pitch/yaw/roll rates to zero. terminalAttitudeHold selects explicit
        // hull-pose tracking instead of velocity-course tracking; the target
        // pose equals the current pose, so only angular-rate removal remains.
        hold.desiredForwardMap = agent.forwardMap;
        hold.desiredUpMap = agent.upMap;
        hold.terminalAttitudeHold = true;
        hold.desiredCourseAngularRateMapRadPerSec = glm::dvec3(0.0);
        hold.desiredRollRateRadPerSec = 0.0;

        hold.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        hold.forwardMap = agent.forwardMap;
        hold.rightMap = agent.rightMap;
        hold.upMap = agent.upMap;
        hold.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        hold.yawRateRadPerSec = agent.yawRateRadPerSec;
        hold.rollRateRadPerSec = agent.rollRateRadPerSec;
        hold.stopRequested = false;
        hold.deltaSeconds = std::max(
            1.0e-4,
            m_client->serverFixedStepSeconds()
        );

        return controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted
            ? m_clientDockingAssistedStabilizePilotController.update(
                  hold,
                  effectivePhysics
              )
            : m_clientDockingNewtonianStabilizePilotController.update(
                  hold,
                  effectivePhysics
              );
    };

    const auto manualStopControl = [&]()
    {
        const auto agent = makeAgent();

        game::navigation::autopilot::AutopilotControlRequest stop;
        stop.desiredVelocityMapMps = glm::dvec3(0.0);
        stop.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
        stop.desiredForwardMap = agent.forwardMap;
        stop.desiredUpMap = agent.upMap;
        stop.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        stop.forwardMap = agent.forwardMap;
        stop.rightMap = agent.rightMap;
        stop.upMap = agent.upMap;
        stop.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        stop.yawRateRadPerSec = agent.yawRateRadPerSec;
        stop.rollRateRadPerSec = agent.rollRateRadPerSec;
        stop.stopRequested = true;
        stop.deltaSeconds = std::max(
            1.0e-4,
            m_client->serverFixedStepSeconds()
        );

        return controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted
            ? m_clientDockingAssistedStabilizePilotController.update(
                  stop,
                  effectivePhysics
              )
            : m_clientDockingNewtonianStabilizePilotController.update(
                  stop,
                  effectivePhysics
              );
    };

    const auto legacyNewtonianSafeCourseControl =
        [&](const glm::dvec3& safeDirection)
        {
            const auto agent = makeAgent();
            const double actualSpeed =
                glm::length(agent.velocityMapMetersPerSecond);
            const glm::dvec3 desiredCourse =
                glm::length(safeDirection) > 1.0e-9
                    ? glm::normalize(safeDirection)
                    : agent.forwardMap;
            const double targetSpeed =
                m_dockSafeCourse.targetSpeedMps > 0.5
                    ? m_dockSafeCourse.targetSpeedMps
                    : actualSpeed;

            game::navigation::autopilot::AutopilotControlRequest turn;

            // Legacy Newtonian bridge only. Assisted recovery owns a separate
            // committed-trajectory API and must never enter this mutable
            // direction controller.
            const glm::dvec3 targetVelocity =
                desiredCourse * targetSpeed;
            const glm::dvec3 deltaVelocity =
                targetVelocity -
                agent.velocityMapMetersPerSecond;
            const double deltaSpeed =
                glm::length(deltaVelocity);

            turn.desiredVelocityMapMps = targetVelocity;
            turn.desiredForwardMap = desiredCourse;

            const double requestedAcceleration =
                std::max(
                    0.0,
                    m_dockSafeCourse.courseAccelerationMps2
                );
            turn.desiredLinearAccelerationMapMps2 =
                deltaSpeed > 1.0e-6 &&
                requestedAcceleration > 1.0e-9
                    ? deltaVelocity / deltaSpeed *
                        requestedAcceleration
                    : glm::dvec3(0.0);

            turn.desiredUpMap = agent.upMap;
            turn.actualVelocityMapMps =
                agent.velocityMapMetersPerSecond;
            turn.forwardMap = agent.forwardMap;
            turn.rightMap = agent.rightMap;
            turn.upMap = agent.upMap;
            turn.pitchRateRadPerSec = agent.pitchRateRadPerSec;
            turn.yawRateRadPerSec = agent.yawRateRadPerSec;
            turn.rollRateRadPerSec = agent.rollRateRadPerSec;
            turn.stopRequested = false;
            turn.terminalAttitudeHold = false;
            turn.deltaSeconds = std::max(
                1.0e-4,
                m_client->serverFixedStepSeconds()
            );

            return m_clientDockingNewtonianStabilizePilotController.update(
                turn,
                effectivePhysics
            );
        };

    // Automatic replanning has no wall-clock start deadline. Assisted either
    // follows one committed, collision-proven recovery maneuver or coasts
    // ballistically while the asynchronous route is built. Newtonian keeps
    // its legacy safety bridge until its separate recovery executor exists.
    // Route handoff remains spatially validated.

    if (pending.serial != m_lastDockingPathRequestSerial)
    {
        eraseVisibleRoute();
        releaseClientAutopilot();

        m_lastDockingPathRequestSerial = pending.serial;
        m_lastDockingRequestTraceSerial = pending.serial;
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockSafeCourse = {};
        m_clientDockingPhase =
            ClientDockingPhase::Stabilizing;
        m_noSafeDockingGuidanceSolution = false;
        m_dockingGuidanceFailureReason.clear();
        m_dockingStatusWarning.clear();

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase=stabilizing"
            << " mode=" << (automatic ? "automatic" : "guidance")
            << " system=" << pending.target.systemId
            << " module=" << pending.target.stableObjectId
            << " anchor=" << pending.target.semanticAnchorId
            << " execution=client-input"
            << std::endl;
    }

    const double fixedDt =
        std::max(1.0e-4, m_client->serverFixedStepSeconds());
    const double authoritativeServerSeconds =
        m_client->lastSimulationMetadata().serverTimeSeconds;
    const double universeTimeSeconds =
        m_client->lastSimulationMetadata().universeTimeSeconds;

    if (m_clientDockingPhase ==
        ClientDockingPhase::SecuringCourse)
    {
        if (automatic &&
            controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted &&
            m_dockAssistedSafeRecovery.active &&
            m_dockAssistedSafeRecovery.serial == pending.serial)
        {
            const auto agent = makeAgent();
            const double speedMps =
                glm::length(agent.velocityMapMetersPerSecond);
            const double angularSpeedRadPerSec =
                std::sqrt(
                    agent.pitchRateRadPerSec *
                        agent.pitchRateRadPerSec +
                    agent.yawRateRadPerSec *
                        agent.yawRateRadPerSec +
                    agent.rollRateRadPerSec *
                        agent.rollRateRadPerSec
                );

            if (m_dockAssistedSafeRecovery.stopFallback)
            {
                submitAutopilotControl(manualStopControl());

                if (speedMps <= 0.05 &&
                    angularSpeedRadPerSec <= 0.01)
                {
                    std::cerr
                        << "[AssistedSafeCourse]"
                        << " request=" << pending.serial
                        << " phase=stopped"
                        << " fallback=1"
                        << std::endl;

                    m_dockAssistedSafeRecovery = {};
                    m_clientDockingPhase =
                        ClientDockingPhase::Stabilizing;
                    m_clientDockingSettledSinceServerSeconds =
                        -1.0;
                }
                return;
            }

            game::navigation::assisted::
                AssistedSafeCourseExecutor::Input safeInput;
            safeInput.actualVelocityMapMps =
                agent.velocityMapMetersPerSecond;
            safeInput.selectedCourseMap =
                m_dockAssistedSafeRecovery.direction;
            safeInput.forwardMap = agent.forwardMap;
            safeInput.rightMap = agent.rightMap;
            safeInput.upMap = agent.upMap;
            safeInput.pitchRateRadPerSec =
                agent.pitchRateRadPerSec;
            safeInput.yawRateRadPerSec =
                agent.yawRateRadPerSec;
            safeInput.rollRateRadPerSec =
                agent.rollRateRadPerSec;
            safeInput.selectedSpeedMps =
                m_dockAssistedSafeRecovery.targetSpeedMps;
            safeInput.maxPitchRateRadPerSec =
                effectivePhysics.maxPitchRate;
            safeInput.maxYawRateRadPerSec =
                effectivePhysics.maxYawRate;
            safeInput.maxRollRateRadPerSec =
                effectivePhysics.maxRollRate;
            safeInput.angularAccelerationAuthorityRadPerSec2 =
                game::ship::angularAccelerationLimitRadPerSec2(
                    effectivePhysics
                );
            safeInput.deltaSeconds = fixedDt;

            const auto safeOutput =
                game::navigation::assisted::
                    AssistedSafeCourseExecutor::evaluate(
                        safeInput
                    );

            if (!safeOutput.valid)
            {
                fail("Assisted safe-course executor input invalid");
                return;
            }

            if (safeOutput.acquired)
            {
                // ============================================================
                // ASSISTED DOCKING ENTRY CONTRACT -- DO NOT REFACTOR AWAY.
                //
                // Once the ONE selected collision-free velocity course has
                // been acquired, safe-course work is finished.  Remove linear
                // acceleration and coast.  Do NOT:
                //   - track a time-indexed recovery arc;
                //   - compare position with an imaginary circle;
                //   - select another course because of timing/position error;
                //   - cancel/restart the expensive route planner.
                //
                // The planner is allowed to finish from its stale start.  A
                // single projected entry bind below joins the live ballistic
                // craft to the immutable downstream route.
                // ============================================================
                submitAutopilotControl(coastForPlanningControl());

                const bool plannerAlreadyLaunched =
                    m_dockAdviceJob &&
                    m_dockAdviceJob->context.serial ==
                        pending.serial;

                std::cerr
                    << "[AssistedSafeCourse]"
                    << " request=" << pending.serial
                    << " phase=acquired"
                    << " speed_mps=" << speedMps
                    << " planner_overlap="
                    << (plannerAlreadyLaunched ? 1 : 0)
                    << std::endl;

                m_dockAssistedSafeRecovery = {};
                m_clientDockingPhase =
                    plannerAlreadyLaunched
                        ? ClientDockingPhase::Planning
                        : ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            // The selected direction is immutable for this docking-entry
            // correction.  No 0.25 s reassessment/reselection loop exists.
            // Safe-course already emits ordinary Assisted controls.  Do not
            // route it through AssistedAutopilot/VelocityCourseGuidance: that
            // would create a second course controller on top of physical FA-on.
            submitAutopilotControl(safeOutput.control);
            return;
        }

        if (!automatic ||
            !m_dockSafeCourse.active ||
            m_dockSafeCourse.serial != pending.serial)
        {
            m_dockSafeCourse = {};
            m_clientDockingPhase =
                ClientDockingPhase::Stabilizing;
            m_clientDockingSettledSinceServerSeconds = -1.0;
            return;
        }

        const auto agent = makeAgent();
        const double speedMps =
            glm::length(agent.velocityMapMetersPerSecond);
        const double angularSpeedRadPerSec =
            std::sqrt(
                agent.pitchRateRadPerSec * agent.pitchRateRadPerSec +
                agent.yawRateRadPerSec * agent.yawRateRadPerSec +
                agent.rollRateRadPerSec * agent.rollRateRadPerSec
            );

        if (m_dockSafeCourse.stopFallback)
        {
            submitAutopilotControl(manualStopControl());

            if (speedMps <= 0.05 &&
                angularSpeedRadPerSec <= 0.01)
            {
                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=stopped"
                    << " fallback=1"
                    << std::endl;

                m_dockSafeCourse = {};
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
            }
            return;
        }

        const bool shouldRecheck =
            m_dockSafeCourse.lastRecheckServerSeconds < 0.0 ||
            authoritativeServerSeconds -
                m_dockSafeCourse.lastRecheckServerSeconds >= 0.25;

        if (shouldRecheck)
        {
            game::navigation::PreplanningSafetyCourseRequest safety;
            safety.positionMeters = agent.positionMapMeters;
            safety.velocityMps =
                agent.velocityMapMetersPerSecond;
            safety.hullForward = agent.forwardMap;
            safety.preferredDirection =
                m_dockSafeCourse.preferredDirection;
            safety.hullRadiusMeters =
                m_dockSafeCourse.hullRadiusMeters;
            safety.additionalClearanceMeters =
                m_dockSafeCourse.additionalClearanceMeters;
            safety.courseAccelerationMps2 =
                m_dockSafeCourse.courseAccelerationMps2;
            safety.brakingAccelerationMps2 =
                m_dockSafeCourse.brakingAccelerationMps2;
            safety.maxAngularVelocityRadPerSecond =
                m_dockSafeCourse.maxAngularVelocityRadPerSecond;
            safety.newtonian =
                m_dockSafeCourse.newtonian;
            safety.obstacles =
                m_dockSafeCourse.obstacles;

            const auto check =
                game::navigation::PreplanningSafetyCourse::
                    evaluate(safety);

            m_dockSafeCourse.lastRecheckServerSeconds =
                authoritativeServerSeconds;

            constexpr double SafeCourseAlignmentCosine =
                0.9961946980917455; // cos(5 deg)

            const glm::dvec3 actualCourse =
                speedMps > 0.5
                    ? glm::normalize(
                          agent.velocityMapMetersPerSecond
                      )
                    : glm::normalize(agent.forwardMap);

            const glm::dvec3 selectedCourse =
                glm::normalize(m_dockSafeCourse.direction);

            const bool selectedCourseAcquired =
                glm::dot(actualCourse, selectedCourse) >=
                    SafeCourseAlignmentCosine;

            if (check.currentCourseSafe &&
                selectedCourseAcquired)
            {
                // The physical course is now collision-proven. A speculative
                // planner worker may already be running (or even ready), so do
                // not throw away the overlap by returning to a fresh planning
                // start. Planning phase will finish angular damping before any
                // route handoff is accepted.
                const bool plannerAlreadyLaunched =
                    m_dockAdviceJob &&
                    m_dockAdviceJob->context.serial == pending.serial;

                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );

                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=acquired"
                    << " speed_mps=" << speedMps
                    << " ray_m=" << check.sceneRayMeters
                    << " planner_overlap="
                    << (plannerAlreadyLaunched ? 1 : 0)
                    << std::endl;

                m_dockSafeCourse = {};
                m_clientDockingPhase =
                    plannerAlreadyLaunched
                        ? ClientDockingPhase::Planning
                        : ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            if (!check.currentCourseSafe)
            {
                if (check.safeCourseFound)
                {
                    // The previous escape direction is no longer proven from
                    // the live position. Replace it with the newly proven one.
                    // Any speculative route already running remains safe to
                    // keep as a backbone candidate; spatial handoff will force
                    // a late-entry rebind if this direction change matters.
                    m_dockSafeCourse.direction =
                        check.safeCourse;
                    m_dockSafeCourse.sceneRayMeters =
                        check.sceneRayMeters;
                    m_dockSafeCourse.selectedTurnAngleRad =
                        check.turnAngleRad;
                    m_dockSafeCourse.selectedTurnRadiusMeters =
                        check.turnRadiusMeters;
                }
                else
                {
                    // No proven escape turn. Do not keep coasting into the
                    // blocker and do not hand this state back to the route
                    // planner. Fall back to the ordinary physical STOP.
                    m_dockSafeCourse.stopFallback = true;

                    if (m_dockAdviceJob)
                    {
                        m_dockAdviceJob->cancelled.store(
                            true,
                            std::memory_order_release
                        );
                    }

                    std::cerr
                        << "[DockSafetyCourse]"
                        << " request=" << pending.serial
                        << " phase=emergency-stop"
                        << " stop_corridor_clear="
                        << (check.stopFallbackClear ? 1 : 0)
                        << " reason=\"" << check.message << "\""
                        << std::endl;

                    submitAutopilotControl(manualStopControl());
                    return;
                }
            }
        }

        submitAutopilotControl(
            legacyNewtonianSafeCourseControl(
                m_dockSafeCourse.direction
            )
        );
        return;
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::Stabilizing)
    {
        const double relativeSpeedMps =
            glm::length(motion.localVelocityMps);
        const double angularSpeedRadPerSec =
            std::sqrt(
                transform.pitchRate * transform.pitchRate +
                transform.yawRate * transform.yawRate +
                transform.rollRate * transform.rollRate
            );

        // MANUAL GUIDANCE CONTRACT:
        // Manual and Automatic consume the SAME route topology/geometry.
        // Manual differs only by reaching a real zero translational/angular
        // state before route calculation.
        if (!automatic)
        {
            constexpr double ManualStoppedSpeedMps = 0.05;
            constexpr double ManualStoppedAngularRadPerSec = 0.01;
            constexpr double ManualSettledSeconds = 0.50;

            submitAutopilotControl(manualStopControl());

            const bool stopped =
                relativeSpeedMps <= ManualStoppedSpeedMps &&
                angularSpeedRadPerSec <=
                    ManualStoppedAngularRadPerSec;

            if (!stopped)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                return;
            }

            if (authoritativeServerSeconds -
                    m_clientDockingSettledSinceServerSeconds <
                ManualSettledSeconds)
            {
                return;
            }
        }
        else
        {
            // ================================================================
            // AUTOMATIC SETTLE-BEFORE-SNAPSHOT CONTRACT -- DO NOT WEAKEN.
            //
            // A new docking request first removes translational acceleration
            // and kills hull angular velocity.  Linear VELOCITY is preserved.
            //
            // The planner/compiler MUST NOT see a snapshot captured while the
            // hull is still rotating.  Waiting only at route adoption is too
            // late: the worker compiles the initial angular trajectory from
            // the snapshot and can permanently reject an otherwise valid
            // route as initial-omega-outside-capability.
            //
            // Required order:
            //   1. zero requested linear acceleration;
            //   2. damp pitch/yaw/roll rates;
            //   3. hold the settled angular state briefly;
            //   4. only then capture the authoritative planning snapshot;
            //   5. after launch, expensive planning may run asynchronously
            //      while the craft continues ballistic translation.
            // ================================================================
            constexpr double AutomaticSettledAngularRadPerSec = 0.01;
            constexpr double AutomaticAngularSettledSeconds = 0.20;

            submitAutopilotControl(
                stabilizeAngularForPlanningControl()
            );

            const bool angularStopped =
                angularSpeedRadPerSec <=
                    AutomaticSettledAngularRadPerSec;

            if (!angularStopped)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                return;
            }

            if (authoritativeServerSeconds -
                    m_clientDockingSettledSinceServerSeconds <
                AutomaticAngularSettledSeconds)
            {
                return;
            }

            // Snapshot below now observes the contract state: no requested
            // linear acceleration and effectively zero hull angular velocity.
            submitAutopilotControl(coastForPlanningControl());
        }

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase="
            << (automatic ? "settled-planning" : "stopped-planning")
            << " vrel_mps=" << relativeSpeedMps
            << " omega_radps=" << angularSpeedRadPerSec
            << std::endl;

        const auto snapshot =
            game::client::ClientNavigationPlanningSnapshotFactory::
                buildAuthoritativeHubSnapshot(
                    m_client->world(),
                    m_client->lastSimulationMetadata(),
                    m_playerId,
                    pending.target.systemId,
                    pending.target.stableObjectId
                );
        if (!snapshot.ready())
        {
            fail("authoritative planning snapshot unavailable");
            return;
        }

        const auto localPort =
            resolveDockingAdvisoryLocalPortAt(
                snapshot.targetObject.hubAttachment,
                *definition,
                snapshot.epoch.universeTimeSeconds
            );
        if (!localPort.valid)
        {
            fail("dock local pose unavailable at planning epoch");
            return;
        }

        VehicleGuidanceEnvelope envelope;
        envelope.lengthMeters = hull.lengthMeters;
        envelope.widthMeters = hull.widthMeters;
        envelope.heightMeters = hull.heightMeters;
        envelope.valid = true;

        const auto shipProfile =
            makeNavigationVehicleProfile(
                effectivePhysics,
                envelope,
                controlLaw
            );
        const bool assisted =
            controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted;

        const bool lateEntryRebind =
            automatic &&
            m_dockLateEntry.active &&
            m_dockLateEntry.serial == pending.serial;

        game::navigation::planner::RoutePlanRequest request;
        const glm::dvec3 startVelocityMps =
            automatic
                ? snapshot.controlledShip.localVelocityMps
                : glm::dvec3(0.0);
        request.startMeters =
            snapshot.controlledShip.localPositionMeters;

        if (lateEntryRebind)
        {
            // One-shot projected entry.  The downstream route is immutable;
            // only its short live connector is rebuilt.  Project the current
            // ballistic craft to the handoff time selected from the previous
            // route's first-turn travel time.
            request.startMeters +=
                startVelocityMps * m_dockLateEntry.leadSeconds;
        }

        request.hasInitialForward = true;
        const glm::dvec3 hullForwardLocal =
            glm::normalize(
                snapshot.planningFrame.worldToLocalVector(
                    glm::dvec3(transform.forward())
                )
            );
        request.initialForward =
            game::navigation::DockingRouteEntryPolicy::initialCourse(
                startVelocityMps,
                hullForwardLocal
            );
        request.initialForwardLeadMeters =
            std::max(1000.0, hull.lengthMeters * 10.0);
        request.terminalReferenceDistanceMeters =
            std::max(
                300.0,
                hull.lengthMeters * 10.0 +
                    definition->requiredClearanceMeters * 4.0
            );
        request.goalMeters =
            localPort.positionMeters +
            localPort.forward *
                request.terminalReferenceDistanceMeters;
        request.terminalOutward = localPort.forward;
        request.agentRadiusMeters =
            envelope.conservativeSafetyRadiusMeters();
        request.vehicleLengthMeters = hull.lengthMeters;

        // Keep execution reserve on the client exactly where the virtual pilot
        // lives. Planner never assumes all installed authority is available.
        const double linearReserve =
            game::navigation::DockingAutomaticRecoveryPolicy::
                linearFeedbackReserveMps2(
                    shipProfile.maxForwardAccelerationMps2,
                    shipProfile.maxBrakingAccelerationMps2,
                    shipProfile.maxLateralAccelerationMps2
                );
        // Speed is not an execution authority reserve. Publish the real
        // physical maximum to the route layer; cruise policy applies its
        // single explicit 0.8 factor later. Curvature/braking/terminal limits
        // are local and must not be hidden behind stacked global margins.
        request.maxSpeedMps =
            shipProfile.maxSpeedMps;
        request.acceleratingMps2 =
            std::max(
                0.1,
                (shipProfile.maxForwardAccelerationMps2 -
                 linearReserve) * 0.90
            );
        request.brakingMps2 =
            std::max(
                0.1,
                (shipProfile.maxBrakingAccelerationMps2 -
                 linearReserve) * 0.90
            );
        const double executionCurveLateralMps2 =
            std::max(
                0.0,
                (shipProfile.maxLateralAccelerationMps2 -
                 linearReserve) * 0.90
            );

        // Route TOPOLOGY and route EXECUTION need different Newtonian
        // authority semantics. The global obstacle/portal solver must not turn
        // a small TVC side component into a 20-50 km geometric detour. For
        // Newtonian it chooses compact straight-first geometry with the normal
        // main-engine maneuver scale; the accepted trajectory below still
        // uses the real TVC budget to cap speed on any finite curvature and
        // forces v=0 at tangent discontinuities.
        request.lateralMps2 =
            assisted
                ? std::max(0.1, executionCurveLateralMps2)
                : request.acceleratingMps2;
        request.maxAngularVelocityRadPerSecond =
            shipProfile.maxAngularVelocityRadPerSecond;
        request.maxAngularAccelerationRadPerSecond2 =
            shipProfile.maxAngularAccelerationRadPerSecond2 * 0.90;
        request.initialSpeedMps = glm::length(startVelocityMps);

        request.initialForwardLeadMeters =
            game::navigation::OrdinaryRoutePolicy::
                requiredStartLeadMeters(
                    request.initialSpeedMps,
                    request.maxSpeedMps,
                    request.brakingMps2,
                    request.lateralMps2,
                    request.maxAngularVelocityRadPerSecond,
                    hull.lengthMeters
                );

        std::cout
            << "[DockEntryPolicy]"
            << " request=" << pending.serial
            << " initial_speed_mps=" << request.initialSpeedMps
            << " route_max_mps=" << request.maxSpeedMps
            << " forward_lead_m="
            << request.initialForwardLeadMeters
            << " geometry_lat_mps2="
            << request.lateralMps2
            << " execution_curve_lat_mps2="
            << executionCurveLateralMps2
            << " initial_course=("
            << request.initialForward.x << ","
            << request.initialForward.y << ","
            << request.initialForward.z << ")"
            << std::endl;

        // USER CONTRACT: visual tunnel cadence.
        request.gateSpacingMeters = 500.0;
        request.terminalGateSpacingMeters = 250.0;

        if (assisted)
        {
            // Ordinary terminal geometry is sized from the speed that may
            // arrive at the final approach and the actual terminal crossing
            // limit. Speed yields to geometry; no fixed 9 km approach.
            request.terminalApproachLengthMeters =
                game::navigation::OrdinaryRoutePolicy::
                    requiredTerminalApproachMeters(
                        request.maxSpeedMps,
                        definition->maxEntrySpeedMps,
                        request.brakingMps2,
                        request.terminalReferenceDistanceMeters,
                        hull.lengthMeters
                    );
            request.terminalTurnSegmentFraction = 0.85;
            request.deriveTerminalTurnRadiusFromVehicle = true;
        }

        request.obstacles = snapshot.navigationObstacles;

        bool planningWhileSecuringCourse = false;

        if (automatic && assisted && lateEntryRebind)
        {
            // ENTRY-REBIND IS NOT A NEW DOCKING REQUEST.
            //
            // The primary Assisted safety course has already been selected
            // and physically acquired. Re-entering PreplanningSafetyCourse
            // here used to let a rotating station trigger a second unrelated
            // course correction while the connector was being rebound,
            // invalidating the very handoff we were trying to repair.
            //
            // Keep the already accepted inertial state and rebuild only the
            // connector to the immutable backbone.
            submitAutopilotControl(coastForPlanningControl());
        }
        else if (automatic)
        {
            const glm::dvec3 preferredSafetyDirection =
                glm::length(request.goalMeters - request.startMeters) >
                        1.0e-9
                    ? glm::normalize(
                          request.goalMeters -
                          request.startMeters
                      )
                    : request.initialForward;

            if (assisted)
            {
                // ============================================================
                // RESTORED ASSISTED DOCKING-ENTRY CONTRACT.
                //
                // 1) linear acceleration is released;
                // 2) inspect the CURRENT ballistic velocity corridor;
                // 3) only if blocked, choose ONE collision-free course;
                // 4) turn the velocity vector to that course once;
                // 5) release acceleration and coast.
                //
                // The expensive route planner starts in parallel and is NEVER
                // restarted merely because the craft moved while it worked.
                // Its stale entry is repaired later by ONE projected bind.
                // ============================================================
                game::navigation::PreplanningSafetyCourseRequest safety;
                safety.positionMeters = request.startMeters;
                safety.velocityMps = startVelocityMps;
                safety.hullForward = hullForwardLocal;
                safety.preferredDirection =
                    preferredSafetyDirection;
                safety.hullRadiusMeters =
                    request.agentRadiusMeters;
                safety.additionalClearanceMeters =
                    std::max(
                        25.0,
                        definition->requiredClearanceMeters
                    );
                safety.courseSafetySpotRadiusMeters =
                    std::clamp(
                        request.agentRadiusMeters * 0.35 +
                            safety.additionalClearanceMeters * 0.25,
                        15.0,
                        50.0
                    );
                safety.courseAccelerationMps2 =
                    request.lateralMps2;
                safety.brakingAccelerationMps2 =
                    request.brakingMps2;
                safety.maxAngularVelocityRadPerSecond =
                    request.maxAngularVelocityRadPerSecond;
                safety.newtonian = false;
                safety.obstacles = request.obstacles;

                const auto safetyResult =
                    game::navigation::PreplanningSafetyCourse::
                        evaluate(safety);

                std::cerr
                    << "[AssistedSafeCourse]"
                    << " request=" << pending.serial
                    << " phase=check"
                    << " speed_mps=" << safetyResult.speedMps
                    << " current_safe="
                    << (safetyResult.currentCourseSafe ? 1 : 0)
                    << " escape_found="
                    << (safetyResult.safeCourseFound ? 1 : 0)
                    << " stop_clear="
                    << (safetyResult.stopFallbackClear ? 1 : 0)
                    << " turn_deg="
                    << safetyResult.turnAngleRad *
                        57.2957795130823208768
                    << " ray_m=" << safetyResult.sceneRayMeters
                    << " reason="" << safetyResult.message << """
                    << std::endl;

                if (!safetyResult.currentCourseSafe)
                {
                    m_dockAssistedSafeRecovery = {};
                    m_dockAssistedSafeRecovery.active = true;
                    m_dockAssistedSafeRecovery.serial =
                        pending.serial;
                    m_dockAssistedSafeRecovery.stopFallback =
                        !safetyResult.safeCourseFound;
                    m_dockAssistedSafeRecovery.direction =
                        safetyResult.safeCourseFound
                            ? safetyResult.safeCourse
                            : request.initialForward;
                    m_dockAssistedSafeRecovery.targetSpeedMps =
                        safetyResult.speedMps;
                    m_dockAssistedSafeRecovery.sceneRayMeters =
                        safetyResult.sceneRayMeters;

                    m_clientDockingPhase =
                        ClientDockingPhase::SecuringCourse;
                    m_clientDockingSettledSinceServerSeconds =
                        -1.0;

                    if (m_dockAssistedSafeRecovery.stopFallback)
                    {
                        std::cerr
                            << "[AssistedSafeCourse]"
                            << " request=" << pending.serial
                            << " phase=emergency-stop"
                            << std::endl;
                        submitAutopilotControl(manualStopControl());
                        return;
                    }

                    planningWhileSecuringCourse = true;

                    std::cerr
                        << "[AssistedSafeCourse]"
                        << " request=" << pending.serial
                        << " phase=selected+planning"
                        << " course=("
                        << m_dockAssistedSafeRecovery.direction.x
                        << ","
                        << m_dockAssistedSafeRecovery.direction.y
                        << ","
                        << m_dockAssistedSafeRecovery.direction.z
                        << ")"
                        << std::endl;

                    // Start the expensive planner from the snapshot we already
                    // have.  Do not pretend to know where safe-course turning
                    // will end; the single projected entry bind owns that.
                    submitAutopilotControl(
                        coastForPlanningControl()
                    );
                }
                else
                {
                    // Current ballistic course is already collision-free.
                    // Planning may begin immediately with zero acceleration.
                    submitAutopilotControl(
                        coastForPlanningControl()
                    );
                }
            }
            else
            {
                // Newtonian intentionally remains on the old selector/executor
                // until its separate rotate/settle/burn recovery contract is
                // implemented. Do not pretend the Assisted arc API applies.
                game::navigation::PreplanningSafetyCourseRequest safety;
                safety.positionMeters = request.startMeters;
                safety.velocityMps = startVelocityMps;
                safety.hullForward = hullForwardLocal;
                safety.preferredDirection =
                    preferredSafetyDirection;
                safety.hullRadiusMeters =
                    request.agentRadiusMeters;
                safety.additionalClearanceMeters =
                    std::max(
                        25.0,
                        definition->requiredClearanceMeters
                    );
                safety.courseSafetySpotRadiusMeters =
                    std::clamp(
                        request.agentRadiusMeters * 0.35 +
                            safety.additionalClearanceMeters * 0.25,
                        15.0,
                        50.0
                    );
                safety.courseAccelerationMps2 =
                    request.acceleratingMps2;
                safety.brakingAccelerationMps2 =
                    request.brakingMps2;
                safety.maxAngularVelocityRadPerSecond =
                    request.maxAngularVelocityRadPerSecond;
                safety.newtonian = true;
                safety.obstacles = request.obstacles;

                const auto safetyResult =
                    game::navigation::PreplanningSafetyCourse::
                        evaluate(safety);

                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=check"
                    << " speed_mps=" << safetyResult.speedMps
                    << " current_safe="
                    << (safetyResult.currentCourseSafe ? 1 : 0)
                    << " escape_found="
                    << (safetyResult.safeCourseFound ? 1 : 0)
                    << " stop_clear="
                    << (safetyResult.stopFallbackClear ? 1 : 0)
                    << " ray_m=" << safetyResult.sceneRayMeters
                    << " turn_deg="
                    << safetyResult.turnAngleRad *
                        57.2957795130823208768
                    << " turn_radius_m="
                    << safetyResult.turnRadiusMeters
                    << " reason=\""
                    << safetyResult.message
                    << "\""
                    << std::endl;

                if (!safetyResult.currentCourseSafe)
                {
                    m_dockSafeCourse = {};
                    m_dockSafeCourse.active = true;
                    m_dockSafeCourse.serial = pending.serial;
                    m_dockSafeCourse.stopFallback =
                        !safetyResult.safeCourseFound;
                    m_dockSafeCourse.direction =
                        safetyResult.safeCourse;
                    m_dockSafeCourse.targetSpeedMps =
                        safetyResult.speedMps;
                    m_dockSafeCourse.sceneRayMeters =
                        safetyResult.sceneRayMeters;
                    m_dockSafeCourse.selectedTurnAngleRad =
                        safetyResult.turnAngleRad;
                    m_dockSafeCourse.selectedTurnRadiusMeters =
                        safetyResult.turnRadiusMeters;
                    m_dockSafeCourse.lastRecheckServerSeconds =
                        authoritativeServerSeconds;
                    m_dockSafeCourse.hullRadiusMeters =
                        safety.hullRadiusMeters;
                    m_dockSafeCourse.additionalClearanceMeters =
                        safety.additionalClearanceMeters;
                    m_dockSafeCourse.courseAccelerationMps2 =
                        safety.courseAccelerationMps2;
                    m_dockSafeCourse.brakingAccelerationMps2 =
                        safety.brakingAccelerationMps2;
                    m_dockSafeCourse.
                        maxAngularVelocityRadPerSecond =
                            safety.maxAngularVelocityRadPerSecond;
                    m_dockSafeCourse.newtonian = true;
                    m_dockSafeCourse.preferredDirection =
                        safety.preferredDirection;
                    m_dockSafeCourse.obstacles =
                        std::move(safety.obstacles);

                    m_clientDockingPhase =
                        ClientDockingPhase::SecuringCourse;
                    m_clientDockingSettledSinceServerSeconds =
                        -1.0;

                    if (m_dockSafeCourse.stopFallback)
                    {
                        std::cerr
                            << "[DockSafetyCourse]"
                            << " request=" << pending.serial
                            << " phase=emergency-stop"
                            << " stop_corridor_clear="
                            << (
                                safetyResult.stopFallbackClear
                                    ? 1
                                    : 0
                            )
                            << std::endl;
                        submitAutopilotControl(
                            manualStopControl()
                        );
                        return;
                    }

                    planningWhileSecuringCourse = true;
                    request.initialForward =
                        m_dockSafeCourse.direction;

                    std::cerr
                        << "[DockSafetyCourse]"
                        << " request=" << pending.serial
                        << " phase=securing+planning"
                        << " course=("
                        << m_dockSafeCourse.direction.x << ","
                        << m_dockSafeCourse.direction.y << ","
                        << m_dockSafeCourse.direction.z << ")"
                        << std::endl;

                    submitAutopilotControl(
                        legacyNewtonianSafeCourseControl(
                            m_dockSafeCourse.direction
                        )
                    );
                }
            }
        }

        // Planner workers are serialized. If an older worker is still
        // unwinding, the live safety controller keeps ownership of the craft
        // while we wait; no new optimizer is stacked on top of it.
        const int livePlannerWorkers =
            m_dockWorkerCount->load(std::memory_order_acquire);
        if (livePlannerWorkers > 0)
            return;

        game::navigation::traffic::CompiledTrafficRoute
            compiledTrafficRoute;

        // TEMPORARY A/B DIAGNOSTIC GATE:
        // Disable semantic traffic / BLUE mandatory traversal completely.
        // This leaves the legacy-normal RoutePlanner -> dock approach path
        // untouched so we can prove whether the regression belongs to the
        // traffic boundary layer rather than the base planner/autopilot.
        constexpr bool EnableMandatoryBlueTransit = true;

        if (!lateEntryRebind &&
            EnableMandatoryBlueTransit &&
            pending.target.stableObjectId == "guidance_dock_cube_a" &&
            pending.target.semanticAnchorId == "dock_gate_front")
        {
            game::navigation::traffic::TrafficRouteGraphCatalog trafficCatalog;
            if (!trafficCatalog.load(
                    "assets/data/navigation/hub_traffic_route_graph.json"))
            {
                fail("traffic graph unavailable: " + trafficCatalog.failure());
                return;
            }

            game::navigation::NavigationVolumeCatalog volumeCatalog;
            if (!volumeCatalog.load(
                    "assets/data/navigation/hub_navigation_volumes.json"))
            {
                fail("navigation volume catalog unavailable");
                return;
            }

            const auto trafficGraph = trafficCatalog.graph();
            const auto trafficRoute = trafficGraph->resolve(
                "cylinder_b.entry_front",
                "cube_a.dock_front"
            );
            if (!trafficRoute.valid)
            {
                fail(
                    "assigned blue traffic route is invalid: " +
                    trafficRoute.failure
                );
                return;
            }

            game::navigation::traffic::RouteStageCompiler::
                ReferenceFrameMap trafficFrames;

            for (const auto& object : snapshot.objects)
            {
                const auto& attachment = object.hubAttachment;
                if (!attachment.valid ||
                    !attachment.inheritHubOrientation ||
                    attachment.moduleId.empty())
                {
                    continue;
                }

                const glm::dvec3 angles =
                    attachment.localRotationDeg +
                    attachment.localAngularVelocityDegPerSecond *
                        snapshot.epoch.universeTimeSeconds;
                const glm::dmat3 visualRotation(
                    glm::mat3(
                        game::navigation::hubLocalEulerDegToMatrix(angles)
                    )
                );

                game::navigation::traffic::NavigationReferenceFramePose
                    framePose;
                framePose.originWorldMeters =
                    game::navigation::hubVisualToTacticalVector(
                        attachment.localOffsetMeters
                    );

                framePose.localToWorldBasis = glm::dmat3(
                    game::navigation::hubVisualToTacticalVector(
                        visualRotation * glm::dvec3(1.0, 0.0, 0.0)
                    ),
                    game::navigation::hubVisualToTacticalVector(
                        visualRotation * glm::dvec3(0.0, 1.0, 0.0)
                    ),
                    game::navigation::hubVisualToTacticalVector(
                        visualRotation * glm::dvec3(0.0, 0.0, 1.0)
                    )
                );

                trafficFrames[attachment.moduleId] = framePose;
            }

            compiledTrafficRoute =
                game::navigation::traffic::RouteStageCompiler::compile(
                    trafficRoute,
                    *trafficGraph,
                    m_hubSemanticAnchorCatalog,
                    volumeCatalog,
                    trafficFrames
                );

            if (!compiledTrafficRoute.valid)
            {
                fail(
                    "traffic route compilation failed: " +
                    compiledTrafficRoute.failure
                );
                return;
            }

            std::cout
                << "[TrafficRouteCompile]"
                << " request=" << pending.serial
                << " stages=" << compiledTrafficRoute.stages.size()
                << " via_points="
                << compiledTrafficRoute.requiredViaPointsMeters.size()
                << " hard_straights="
                << compiledTrafficRoute.mandatoryTangentStraights.size()
                << std::endl;

            for (std::size_t i = 0;
                 i < compiledTrafficRoute.stages.size();
                 ++i)
            {
                const auto& stage = compiledTrafficRoute.stages[i];
                std::cout
                    << "[TrafficStage]"
                    << " request=" << pending.serial
                    << " i=" << i
                    << " kind=" << static_cast<int>(stage.kind)
                    << " from=("
                    << stage.fromWorldMeters.x << ","
                    << stage.fromWorldMeters.y << ","
                    << stage.fromWorldMeters.z << ")"
                    << " to=("
                    << stage.toWorldMeters.x << ","
                    << stage.toWorldMeters.y << ","
                    << stage.toWorldMeters.z << ")"
                    << " volume=" << stage.volumeConstraint.volumeId
                    << " policy="
                    << static_cast<int>(stage.volumeConstraint.policy)
                    << std::endl;
            }

            for (std::size_t i = 0;
                 i < compiledTrafficRoute.requiredViaPointsMeters.size();
                 ++i)
            {
                const auto& p =
                    compiledTrafficRoute.requiredViaPointsMeters[i];
                std::cout
                    << "[TrafficVia]"
                    << " request=" << pending.serial
                    << " i=" << i
                    << " p=("
                    << p.x << "," << p.y << "," << p.z << ")"
                    << std::endl;
            }

            for (std::size_t i = 0;
                 i < compiledTrafficRoute.mandatoryTangentStraights.size();
                 ++i)
            {
                const auto& straight =
                    compiledTrafficRoute.mandatoryTangentStraights[i];
                std::cout
                    << "[TrafficHardStraight]"
                    << " request=" << pending.serial
                    << " i=" << i
                    << " portal=" << straight.portalId
                    << " inbound=" << (straight.inbound ? 1 : 0)
                    << " length_m=" << straight.lengthMeters
                    << " start=("
                    << straight.startWorldMeters.x << ","
                    << straight.startWorldMeters.y << ","
                    << straight.startWorldMeters.z << ")"
                    << " end=("
                    << straight.endWorldMeters.x << ","
                    << straight.endWorldMeters.y << ","
                    << straight.endWorldMeters.z << ")"
                    << std::endl;
            }

            request.requiredViaPointsMeters =
                compiledTrafficRoute.requiredViaPointsMeters;

            // The compiled traffic route is generic and therefore includes
            // its destination portal. For docking, however, the terminal
            // docking portal/approach is owned by DockingAdvisoryPlanner:
            // it authors preEntry -> exact terminal arc -> final axis -> dock.
            // Keeping the dock portal as a mandatory transit via would force
            // the route to visit the dock first and then fly back out to
            // preEntry before entering the terminal arc.
            if (!request.requiredViaPointsMeters.empty())
            {
                const glm::dvec3& terminalTrafficVia =
                    request.requiredViaPointsMeters.back();

                if (glm::length(
                        terminalTrafficVia - localPort.positionMeters
                    ) <= 1.0e-3)
                {
                    std::cout
                        << "[TrafficDockAdapter]"
                        << " request=" << pending.serial
                        << " drop_terminal_via=("
                        << terminalTrafficVia.x << ","
                        << terminalTrafficVia.y << ","
                        << terminalTrafficVia.z << ")"
                        << " reason=docking-terminal-owned-by-planner"
                        << std::endl;

                    request.requiredViaPointsMeters.pop_back();
                }
            }

            request.mandatoryTangentStraights.clear();
            request.mandatoryTangentStraights.reserve(
                compiledTrafficRoute.mandatoryTangentStraights.size()
            );

            for (const auto& straight :
                 compiledTrafficRoute.mandatoryTangentStraights)
            {
                game::navigation::planner::
                    MandatoryTangentStraightConstraint constraint;
                constraint.startMeters = straight.startWorldMeters;
                constraint.endMeters = straight.endWorldMeters;
                constraint.minimumStraightMeters = straight.lengthMeters;
                constraint.inbound = straight.inbound;
                request.mandatoryTangentStraights.push_back(constraint);
            }

            const auto* lane =
                trafficGraph->lane("cylinder_b.blue_lane");
            if (!lane ||
                lane->policy !=
                    world::navigation::NavigationVolumePolicy::KeepInside)
            {
                fail("assigned blue lane lost KeepInside policy");
                return;
            }

            const auto transitObjectIt = std::find_if(
                snapshot.objects.begin(),
                snapshot.objects.end(),
                [&](const auto& object)
                {
                    return object.hubAttachment.valid &&
                        object.hubAttachment.moduleId ==
                            "guidance_dock_cylinder_b";
                }
            );

            if (transitObjectIt == snapshot.objects.end())
            {
                fail("blue transit module unavailable in planning snapshot");
                return;
            }

            // Cylinder B render/collision solid surrounds an authored physical
            // passage. The semantic BLUE volume owns navigation inside that
            // passage, therefore the enclosing module must not simultaneously
            // appear as a solid obstacle to the route planner.
            const std::uint32_t transitEntityId =
                transitObjectIt->id.value;
            request.obstacles.erase(
                std::remove_if(
                    request.obstacles.begin(),
                    request.obstacles.end(),
                    [&](const auto& obstacle)
                    {
                        return obstacle.entityId == transitEntityId;
                    }
                ),
                request.obstacles.end()
            );

            std::cout
                << "[TrafficRoute] request=" << pending.serial
                << " stages=" << compiledTrafficRoute.stages.size()
                << " via_points="
                << compiledTrafficRoute.requiredViaPointsMeters.size()
                << " tangent_straights="
                << compiledTrafficRoute.mandatoryTangentStraights.size()
                << " lane=" << lane->id
                << " volume=" << lane->navigationVolumeId
                << " policy=KeepInside"
                << " void_bridge_entity=" << transitEntityId
                << std::endl;
        }

        bool hasPreferredEntryAnchor = false;
        glm::dvec3 preferredEntryAnchor {0.0};
        glm::dvec3 preferredEntryForward {0.0, 0.0, -1.0};

        if (compiledTrafficRoute.valid)
        {
            for (const auto& stage : compiledTrafficRoute.stages)
            {
                if (stage.kind !=
                        game::navigation::traffic::
                            TrafficRouteStageKind::VolumeTransit ||
                    (stage.volumeConstraint.policy !=
                         world::navigation::
                             NavigationVolumePolicy::KeepInside &&
                     stage.volumeConstraint.policy !=
                         world::navigation::
                             NavigationVolumePolicy::PreferInside))
                {
                    continue;
                }

                glm::dvec3 traversalForward =
                    stage.toWorldMeters - stage.fromWorldMeters;
                if (glm::length(traversalForward) <= 1.0e-9 &&
                    !stage.volumeSections.empty())
                {
                    traversalForward =
                        stage.volumeSections.front().forwardWorld;
                }

                if (glm::length(traversalForward) <= 1.0e-9)
                    break;

                traversalForward = glm::normalize(traversalForward);

                preferredEntryAnchor = stage.fromWorldMeters;
                for (const auto& straight :
                     compiledTrafficRoute.mandatoryTangentStraights)
                {
                    if (straight.inbound &&
                        straight.portalId == stage.fromPortalId)
                    {
                        preferredEntryAnchor =
                            straight.startWorldMeters;
                        break;
                    }
                }

                hasPreferredEntryAnchor = true;
                preferredEntryForward = traversalForward;
                break;
            }
        }

        if (m_dockWorkerCount->load(std::memory_order_acquire) != 0)
        {
            // Stabilizing above normally owns this wait. Keep this guard
            // non-fatal in case a worker completes between snapshot creation
            // and launch.
            return;
        }

        auto job = std::make_shared<DockAdviceJob>();
        job->planningProgressStage.store(0, std::memory_order_release);
        job->planningProgressPermille.store(100, std::memory_order_release);
        job->timelineRevision =
            snapshot.epoch.universeTimelineRevision;
        job->startedServerSeconds =
            snapshot.epoch.serverTimeSeconds;
        job->executionStartUniverseTimeSeconds =
            snapshot.epoch.universeTimeSeconds +
            (lateEntryRebind ? m_dockLateEntry.leadSeconds : 0.0);
        job->planningRequest = request;
        job->entryRebind = lateEntryRebind;
        if (lateEntryRebind)
            job->entryBackbone = m_dockLateEntry.backbone;
        job->hasPreferredEntryAnchor = hasPreferredEntryAnchor;
        job->preferredEntryAnchor = preferredEntryAnchor;
        job->preferredEntryForward = preferredEntryForward;
        job->context.serial = pending.serial;
        job->context.systemId = snapshot.systemId;
        job->context.hubId =
            snapshot.hubPredictionSource.hubId;
        job->context.targetObjectId =
            snapshot.targetObject.id.value;
        job->context.timelineRevision =
            snapshot.epoch.universeTimelineRevision;
        job->context.portDefinition = *definition;
        job->context.portAttachment =
            snapshot.targetObject.hubAttachment;
        job->context.routeUpReference = localPort.up;
        job->context.dockCenterReference =
            localPort.positionMeters;
        job->context.dockBottomReference =
            localPort.positionMeters - localPort.up;
        job->context.standoffMeters =
            request.terminalReferenceDistanceMeters;
        job->context.widthMeters =
            fit.openingWidthMeters;
        job->context.heightMeters =
            fit.openingHeightMeters;
        job->context.shipWidthMeters =
            hull.widthMeters;
        job->context.shipHeightMeters =
            hull.heightMeters;
        job->context.lateralToleranceMeters =
            fit.widthMarginMeters * 0.5;
        job->context.verticalToleranceMeters =
            fit.heightMarginMeters * 0.5;

        auto compileAgent = makeAgent();
        compileAgent.positionMapMeters = request.startMeters;
        compileAgent.velocityMapMetersPerSecond = startVelocityMps;

        if (planningWhileSecuringCourse)
        {
            // Compile against the collision-proven state we are physically
            // acquiring, not the transient pre-turn velocity/attitude. The
            // spatial handoff still validates the live craft before adoption.
            const glm::dvec3 predictedForward =
                glm::normalize(request.initialForward);

            compileAgent.velocityMapMetersPerSecond =
                predictedForward * request.initialSpeedMps;
            compileAgent.forwardMap = predictedForward;

            glm::dvec3 predictedUp =
                compileAgent.upMap -
                predictedForward *
                    glm::dot(
                        compileAgent.upMap,
                        predictedForward
                    );

            if (glm::length(predictedUp) <= 1.0e-9)
            {
                const glm::dvec3 fallbackUp =
                    std::abs(predictedForward.y) < 0.9
                        ? glm::dvec3(0.0, 1.0, 0.0)
                        : glm::dvec3(1.0, 0.0, 0.0);
                predictedUp =
                    fallbackUp -
                    predictedForward *
                        glm::dot(
                            fallbackUp,
                            predictedForward
                        );
            }

            predictedUp = glm::normalize(predictedUp);
            compileAgent.rightMap =
                glm::normalize(
                    glm::cross(
                        predictedForward,
                        predictedUp
                    )
                );
            compileAgent.upMap =
                glm::normalize(
                    glm::cross(
                        compileAgent.rightMap,
                        predictedForward
                    )
                );
        }

        const double compileTolerance =
            std::max(
                20.0,
                std::max(
                    job->context.lateralToleranceMeters,
                    job->context.verticalToleranceMeters
                )
            );
        const auto compileControlLaw = controlLaw;
        const auto compilePhysics = effectivePhysics;
        const std::uint64_t compileRequestSerial = pending.serial;
        const double compileExecutionUniverseTimeSeconds =
            job->executionStartUniverseTimeSeconds;
        job->autopilotCompileRequested = automatic;

        m_dockWorkerCount->fetch_add(
            1,
            std::memory_order_acq_rel
        );
        m_dockAdviceJob = job;
        m_clientDockingPhase =
            planningWhileSecuringCourse
                ? ClientDockingPhase::SecuringCourse
                : ClientDockingPhase::Planning;

        const double planningMaxSpeedMps = request.maxSpeedMps;
        const double planningInitialForwardLeadMeters =
            request.initialForwardLeadMeters;
        const glm::dvec3 planningSnapshotStartMeters =
            request.startMeters;
        const double planningInitialSpeedMps =
            request.initialSpeedMps;

        std::thread(
            [job,
             count = m_dockWorkerCount,
             request = std::move(request),
             compiledTrafficRoute = std::move(compiledTrafficRoute),
             compileAgent,
             compileControlLaw,
             compilePhysics,
             compileRequestSerial,
             compileTolerance,
             compileExecutionUniverseTimeSeconds]() mutable
            {
                const auto workerStarted =
                    std::chrono::steady_clock::now();

                const auto finishWorker = [&]()
                {
                    job->workerDurationMs =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            workerStarted
                        ).count();

                    job->ready.store(
                        true,
                        std::memory_order_release
                    );
                    count->fetch_sub(
                        1,
                        std::memory_order_acq_rel
                    );
                };

                try
                {
                    job->planningProgressStage.store(
                        1,
                        std::memory_order_release
                    );
                    job->planningProgressPermille.store(
                        200,
                        std::memory_order_release
                    );

                    if (compileControlLaw ==
                        game::navigation::LocalFlightControlLaw::Assisted)
                    {
                        using Pipeline =
                            game::navigation::planner::assisted::
                                AssistedPlanningPipeline;
                        job->plan =
                            job->entryRebind
                                ? Pipeline::bind(
                                      request,
                                      job->entryBackbone
                                  )
                                : compiledTrafficRoute.valid
                                    ? Pipeline::segmented(
                                          request,
                                          compiledTrafficRoute
                                      )
                                    : Pipeline::route(request);
                    }
                    else
                    {
                        using Pipeline =
                            game::navigation::planner::newtonian::
                                NewtonianPlanningPipeline;
                        job->plan =
                            job->entryRebind
                                ? Pipeline::bind(
                                      request,
                                      job->entryBackbone
                                  )
                                : compiledTrafficRoute.valid
                                    ? Pipeline::segmented(
                                          request,
                                          compiledTrafficRoute
                                      )
                                    : Pipeline::route(request);
                    }

                    job->planningProgressPermille.store(
                        700,
                        std::memory_order_release
                    );

                    if (job->cancelled.load(std::memory_order_acquire))
                    {
                        finishWorker();
                        return;
                    }

                    if (job->plan.valid() &&
                        compiledTrafficRoute.valid)
                    {
                        for (const auto& stage :
                             compiledTrafficRoute.stages)
                        {
                            if (stage.kind !=
                                    game::navigation::traffic::
                                        TrafficRouteStageKind::VolumeTransit ||
                                stage.volumeConstraint.policy !=
                                    world::navigation::
                                        NavigationVolumePolicy::KeepInside)
                            {
                                continue;
                            }

                            const auto containment =
                                game::navigation::traffic::
                                    RouteVolumeContainmentValidator::
                                        validateKeepInsideRouteInterval(
                                            stage,
                                            job->plan.routeCurves,
                                            stage.fromWorldMeters,
                                            stage.toWorldMeters,
                                            request.agentRadiusMeters
                                        );

                            if (!containment.valid)
                            {
                                job->plan.disposition =
                                    game::navigation::planner::
                                        RoutePlanDisposition::NeedsRefinement;
                                job->plan.failureCode =
                                    game::navigation::planner::
                                        RoutePlanFailureCode::
                                            NoCollisionFreeCandidate;
                                job->plan.failure =
                                    "assigned BLUE corridor containment failed: " +
                                    containment.failure;
                                job->plan.userMessage =
                                    job->plan.failure;
                                break;
                            }
                        }
                    }

                    if (job->plan.valid() &&
                        compiledTrafficRoute.valid)
                    {
                        // Explicit semantic -> generic contract seam.
                        // SpaceState only composes the modules; it does not
                        // derive route-stage progress or frame ownership.
                        const auto stageContract =
                            game::navigation::traffic::
                                TrafficRoutePlanAdapter::
                                    buildStageContract(
                                        compiledTrafficRoute,
                                        job->plan,
                                        job->context.routeUpReference
                                    );

                        if (!stageContract.valid)
                        {
                            job->plan.disposition =
                                game::navigation::planner::
                                    RoutePlanDisposition::NeedsRefinement;
                            job->plan.failureCode =
                                game::navigation::planner::
                                    RoutePlanFailureCode::BackendFailure;
                            job->plan.failure =
                                "traffic route-plan adapter failed: " +
                                stageContract.failure;
                            job->plan.userMessage =
                                job->plan.failure;
                        }
                        else
                        {
                            job->plan.stages = stageContract.stages;
                            job->plan.routeFrameAnchors =
                                stageContract.frameAnchors;

                            for (const auto& stage :
                                 job->plan.stages)
                            {
                                std::cerr
                                    << "[RouteStage]"
                                    << " id=" << stage.id
                                    << " kind="
                                    << static_cast<int>(stage.kind)
                                    << " s0="
                                    << stage.startProgressMeters
                                    << " s1="
                                    << stage.endProgressMeters
                                    << " frame_policy="
                                    << static_cast<int>(
                                           stage.framePolicy
                                       )
                                    << " refresh_policy="
                                    << static_cast<int>(
                                           stage.refreshPolicy
                                       )
                                    << " source=" << stage.sourceId
                                    << " volume=" << stage.volumeId
                                    << " hard="
                                    << (stage.hardContainment ? 1 : 0)
                                    << std::endl;
                            }
                        }
                    }

                    if (job->cancelled.load(std::memory_order_acquire))
                    {
                        finishWorker();
                        return;
                    }

                    // Route presentation and autopilot compilation belong to
                    // every valid planner route. They are not traffic-route
                    // concerns: a direct/free-space docking route must pass
                    // through exactly the same execution pipeline.
                    if (job->plan.valid())
                    {
                        std::vector<
                            game::navigation::RouteFrameField::Anchor>
                            visualAnchors;
                        visualAnchors.reserve(
                            job->plan.routeFrameAnchors.size()
                        );
                        for (const auto& anchor :
                             job->plan.routeFrameAnchors)
                        {
                            game::navigation::RouteFrameField::Anchor visual;
                            visual.progressMeters = anchor.progressMeters;
                            visual.upReference = anchor.upReference;
                            visual.dynamicRollPhaseWeight =
                                anchor.dynamicRollPhaseWeight;
                            visualAnchors.push_back(visual);
                        }

                        const auto visualFrameField =
                            game::navigation::RouteFrameField::build(
                                job->plan.routeCurves,
                                job->context.routeUpReference,
                                visualAnchors
                            );
                        job->context.visualGateUp =
                            game::navigation::RouteFrameField::
                                sampleUpForGates(
                                    visualFrameField,
                                    job->plan.gates
                                );
                        job->context.visualExecutionGateUp =
                            game::navigation::RouteFrameField::
                                sampleUpForGates(
                                    visualFrameField,
                                    job->plan.executionGates
                                );
                        job->context.visualGateDockPhaseWeight =
                            game::navigation::RouteFrameField::
                                sampleDynamicRollPhaseWeightForGates(
                                    visualFrameField,
                                    job->plan.gates
                                );
                        job->context.visualExecutionGateDockPhaseWeight =
                            game::navigation::RouteFrameField::
                                sampleDynamicRollPhaseWeightForGates(
                                    visualFrameField,
                                    job->plan.executionGates
                                );

                        job->planningProgressStage.store(
                            2,
                            std::memory_order_release
                        );
                        job->planningProgressPermille.store(
                            850,
                            std::memory_order_release
                        );

                        if (job->cancelled.load(std::memory_order_acquire))
                        {
                            finishWorker();
                            return;
                        }

                        if (job->autopilotCompileRequested)
                        {
                            job->planningProgressStage.store(
                                3,
                                std::memory_order_release
                            );
                            job->planningProgressPermille.store(
                                930,
                                std::memory_order_release
                            );

                            job->compiledAutopilotLaw =
                                compileControlLaw;
                            if (compileControlLaw ==
                                game::navigation::
                                    LocalFlightControlLaw::Assisted)
                            {
                                job->autopilotCompiled =
                                    game::navigation::autopilot::
                                        AssistedClientRouteAutopilot::start(
                                            job->compiledAssistedAutopilotState,
                                            job->plan,
                                            compileAgent,
                                            compilePhysics,
                                            compileExecutionUniverseTimeSeconds,
                                            compileRequestSerial,
                                            compileTolerance,
                                            job->context.routeUpReference,
                                            true,
                                            &job->autopilotCompileFailure
                                        );
                            }
                            else
                            {
                                job->autopilotCompiled =
                                    game::navigation::autopilot::
                                        NewtonianClientRouteAutopilot::start(
                                            job->compiledNewtonianAutopilotState,
                                            job->plan,
                                            compileAgent,
                                            compilePhysics,
                                            compileExecutionUniverseTimeSeconds,
                                            compileRequestSerial,
                                            compileTolerance,
                                            job->context.routeUpReference,
                                            true,
                                            &job->autopilotCompileFailure
                                        );
                            }
                        }
                    }

                    job->planningProgressStage.store(
                        4,
                        std::memory_order_release
                    );
                    job->planningProgressPermille.store(
                        1000,
                        std::memory_order_release
                    );
                }
                catch (const std::exception& error)
                {
                    job->plan.failure =
                        std::string("planner/compile exception: ") +
                        error.what();
                }
                catch (...)
                {
                    job->plan.failure =
                        "planner/compile unknown exception";
                }

                finishWorker();
            }
        ).detach();

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase="
            << (planningWhileSecuringCourse
                    ? "planning+securing-course"
                    : "planning")
            << " route_source=client"
            << " max_speed_mps=" << planningMaxSpeedMps
            << " initial_forward_lead_m="
            << planningInitialForwardLeadMeters
            << " snapshot_start_m=("
            << planningSnapshotStartMeters.x << ","
            << planningSnapshotStartMeters.y << ","
            << planningSnapshotStartMeters.z << ")"
            << " initial_speed_mps="
            << planningInitialSpeedMps
            << std::endl;
        return;
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::Planning)
    {
        bool automaticMotionSettled = true;

        if (automatic)
        {
            // The worker is allowed to run concurrently with physical
            // stabilization, but route handoff is not. Keep damping angular
            // velocity with zero linear acceleration until the craft has been
            // truly ballistic for a short settled interval.
            constexpr double AutomaticSettledAngularRadPerSec = 0.01;
            constexpr double AutomaticAngularSettledSeconds = 0.20;

            const auto liveAgent = makeAgent();
            const double angularSpeedRadPerSec =
                std::sqrt(
                    liveAgent.pitchRateRadPerSec *
                        liveAgent.pitchRateRadPerSec +
                    liveAgent.yawRateRadPerSec *
                        liveAgent.yawRateRadPerSec +
                    liveAgent.rollRateRadPerSec *
                        liveAgent.rollRateRadPerSec
                );

            if (angularSpeedRadPerSec >
                AutomaticSettledAngularRadPerSec)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                automaticMotionSettled = false;
                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );
            }
            else if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                automaticMotionSettled = false;
                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );
            }
            else if (authoritativeServerSeconds -
                         m_clientDockingSettledSinceServerSeconds <
                     AutomaticAngularSettledSeconds)
            {
                automaticMotionSettled = false;
                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );
            }
            else
            {
                submitAutopilotControl(coastForPlanningControl());
            }
        }
        else
        {
            // Manual route was authored from a settled zero state. Keep that
            // state while asynchronous planning finishes.
            submitAutopilotControl(manualStopControl());
        }

        if (!m_dockAdviceJob ||
            !m_dockAdviceJob->ready.load(
                std::memory_order_acquire
            ))
        {
            return;
        }

        // A worker may finish before the craft has completed the physical
        // transition to its ballistic handoff state. Keep the ready result,
        // but do not adopt/validate it until motion settling is real.
        if (automatic && !automaticMotionSettled)
            return;

        auto job = std::move(m_dockAdviceJob);

        if (automatic &&
            job->entryRebind &&
            universeTimeSeconds + fixedDt <
                job->executionStartUniverseTimeSeconds)
        {
            // The final connector was intentionally authored from a future
            // ballistic point.  If calculation finished early, wait for that
            // point; do not invent another rebind.
            submitAutopilotControl(coastForPlanningControl());
            m_dockAdviceJob = std::move(job);
            return;
        }

        if (m_client->lastSimulationMetadata().
                universeTimelineRevision !=
                job->timelineRevision)
        {
            fail("planning snapshot expired");
            return;
        }

        if (!job->plan.valid())
        {
            const std::string reason =
                !job->plan.userMessage.empty()
                    ? job->plan.userMessage
                    : job->plan.failure;

            if (job->plan.retryable())
            {
                std::cerr
                    << "[DockPlanFailure]"
                    << " request=" << pending.serial
                    << " failure_code="
                    << static_cast<int>(job->plan.failureCode)
                    << " disposition="
                    << static_cast<int>(job->plan.disposition)
                    << " raw_failure=\"" << job->plan.failure << "\""
                    << " terminal_last_rejection=\""
                    << job->plan.terminalArcLastRejection << "\""
                    << " dominant_blocker=\""
                    << job->plan.terminalArcDominantBlockerId << "\""
                    << " blocker_hits="
                    << job->plan.terminalArcDominantBlockerHits
                    << " diagnostics=\""
                    << job->plan.diagnosticSummary << "\""
                    << std::endl;

                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=replan"
                    << " reason=" << reason
                    << std::endl;
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            fail(reason);
            return;
        }

        if (automatic)
        {
            const auto liveAgent = makeAgent();
            const double handoffTolerance =
                std::max(
                    20.0,
                    std::max(
                        job->context.lateralToleranceMeters,
                        job->context.verticalToleranceMeters
                    )
                );

            const auto prefix =
                game::navigation::planner::RouteEntryBinder::
                    inspectBallisticPrefix(
                        job->plan,
                        liveAgent.positionMapMeters,
                        handoffTolerance
                    );

            std::cerr
                << "[DockLateEntry]"
                << " request=" << pending.serial
                << " disposition="
                << static_cast<int>(prefix.disposition)
                << " along_m=" << prefix.alongMeters
                << " prefix_m=" << prefix.prefixLengthMeters
                << " remaining_m=" << prefix.remainingMeters
                << " cross_m=" << prefix.crossTrackMeters
                << " worker_ms=" << job->workerDurationMs
                << " rebound=" << (job->entryRebind ? 1 : 0)
                << std::endl;

            if (prefix.disposition !=
                    game::navigation::planner::
                        BallisticPrefixDisposition::OnPrefix)
            {
                if (job->entryRebind)
                {
                    // Connector miss is recoverable. The expensive downstream
                    // backbone remains valid; do NOT fail the docking request
                    // and do NOT run the primary safe-course selector again.
                    //
                    // Re-project from the latest live ballistic state and
                    // retry only the cheap connector. Worker duration is the
                    // minimum useful lead because a connector authored for a
                    // point already passed cannot possibly be adopted.
                    ++m_dockLateEntry.rebindAttempts;
                    const double workerSeconds =
                        std::max(0.0, job->workerDurationMs * 0.001);
                    m_dockLateEntry.leadSeconds =
                        std::max(
                            0.75,
                            workerSeconds * 1.25
                        );

                    m_dockingStatusWarning =
                        "warning: handoff drift; connector retry " +
                        std::to_string(
                            m_dockLateEntry.rebindAttempts
                        );

                    std::cerr
                        << "[DockClient]"
                        << " request=" << pending.serial
                        << " phase=entry-rebind-retry"
                        << " attempt="
                        << m_dockLateEntry.rebindAttempts
                        << " cross_m="
                        << prefix.crossTrackMeters
                        << " lead_s="
                        << m_dockLateEntry.leadSeconds
                        << std::endl;

                    m_clientDockingPhase =
                        ClientDockingPhase::Stabilizing;
                    m_clientDockingSettledSinceServerSeconds = -1.0;
                    submitAutopilotControl(
                        coastForPlanningControl()
                    );
                    return;
                }

                auto backbone =
                    game::navigation::planner::RouteEntryBinder::
                        extractBackbone(
                            job->plan,
                            job->hasPreferredEntryAnchor,
                            job->preferredEntryAnchor,
                            job->preferredEntryForward
                        );

                if (!backbone.valid)
                {
                    fail(
                        "late-bound route entry unavailable: " +
                        backbone.failure
                    );
                    return;
                }

                double firstTurnProgressMeters = 0.0;
                for (const auto& curve : job->plan.routeCurves)
                {
                    if (curve.kind !=
                        game::navigation::planner::RouteCurveKind::Line)
                    {
                        firstTurnProgressMeters =
                            std::max(
                                0.0,
                                curve.startProgressMeters
                            );
                        break;
                    }
                }

                if (firstTurnProgressMeters <= 1.0e-6 &&
                    !job->plan.routeCurves.empty())
                {
                    firstTurnProgressMeters =
                        std::max(
                            0.0,
                            job->plan.routeCurves.front().
                                endProgressMeters
                        );
                }

                const double liveSpeedMps =
                    glm::length(
                        liveAgent.velocityMapMetersPerSecond
                    );

                const double firstTurnTravelSeconds =
                    liveSpeedMps > 0.5
                        ? firstTurnProgressMeters / liveSpeedMps
                        : 0.0;

                // Use only measured/route-derived time scales.  The first-turn
                // travel time is the historical handoff delay.  Worker time is
                // a lower bound so the projected point cannot be reached
                // before a similarly expensive entry calculation can finish.
                const double measuredWorkerSeconds =
                    std::max(0.0, job->workerDurationMs * 0.001);
                const double leadSeconds =
                    std::max(
                        firstTurnTravelSeconds,
                        measuredWorkerSeconds
                    );

                m_dockLateEntry = {};
                m_dockLateEntry.active = true;
                m_dockLateEntry.serial = pending.serial;
                m_dockLateEntry.backbone =
                    std::move(backbone);
                m_dockLateEntry.leadSeconds =
                    leadSeconds;
                m_dockLateEntry.rebindAttempts = 0;
                m_dockingStatusWarning =
                    "handoff drift: rebinding connector";

                // ============================================================
                // PROJECTED ENTRY CONTRACT -- DO NOT TURN THIS INTO A LOOP.
                //
                // The expensive planner has finished.  Keep its downstream
                // route.  Estimate the available handoff delay from the
                // route's start-to-first-turn travel time, project the live
                // ballistic craft by that delay, and calculate ONLY the final
                // entry connector from that predicted point.
                //
                // Exactly one rebind is allowed.  Never discard/re-run the
                // expensive route because its original start became stale.
                // ============================================================
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;

                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=entry-rebind"
                    << " first_turn_m="
                    << firstTurnProgressMeters
                    << " live_speed_mps=" << liveSpeedMps
                    << " lead_s=" << leadSeconds
                    << " worker_s=" << measuredWorkerSeconds
                    << std::endl;
                return;
            }
        }

        m_dockLateEntry = {};
        m_dockingStatusWarning.clear();

        m_dockAdvice = std::move(job->context);
        m_dockAdvice.plan = std::move(job->plan);
        m_clientDockingRouteProgressMeters = 0.0;
        m_clientDockingRouteCurveIndex = 0;
        m_clientAutopilotCrossTrackMeters = 0.0;
        m_clientAutopilotCourseErrorRad = 0.0;
        m_clientAutopilotHullErrorRad = 0.0;
        m_clientAutopilotTargetSpeedMps = 0.0;
        m_clientAutopilotTargetLongitudinalAccelerationMps2 = 0.0;

        // WORKING ASYNC CONTRACT: expensive route compilation and visual
        // frame-field construction completed in the worker. Main thread only
        // adopts ready-to-use products here.

        m_activeDockingGuidanceCorridorId =
            "dock:" +
            pending.target.stableObjectId +
            ":" +
            pending.target.semanticAnchorId;
        m_dockingGuidanceFailureReason.clear();
        m_noSafeDockingGuidanceSolution = false;

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase=route-ready"
            << " execution_points="
            << m_dockAdvice.plan.executionGates.size()
            << " hud_gates="
            << m_dockAdvice.plan.gates.size()
            << " semantic_stages="
            << m_dockAdvice.plan.stages.size()
            << " terminal_radius_m="
            << m_dockAdvice.plan.terminalTurnRadiusMeters
            << " worker_ms="
            << job->workerDurationMs
            << std::endl;

        for (std::size_t stageIndex = 0;
             stageIndex < m_dockAdvice.plan.stages.size();
             ++stageIndex)
        {
            const auto& stage =
                m_dockAdvice.plan.stages[stageIndex];

            std::cerr
                << "[DockRouteStage]"
                << " request=" << pending.serial
                << " index=" << stageIndex
                << " kind=" << static_cast<int>(stage.kind)
                << " start_m=" << stage.startProgressMeters
                << " end_m=" << stage.endProgressMeters
                << " source=" << stage.sourceId
                << " volume=" << stage.volumeId
                << " hard="
                << (stage.hardContainment ? 1 : 0)
                << std::endl;
        }

        if (automatic)
        {
            if (!job->autopilotCompiled)
            {
                const auto agent = makeAgent();
                std::cerr
                    << "[DockClientStartFailure]"
                    << " request=" << pending.serial
                    << " reason=\"" << job->autopilotCompileFailure << "\""
                    << " speed_mps="
                    << glm::length(agent.velocityMapMetersPerSecond)
                    << " velocity=("
                    << agent.velocityMapMetersPerSecond.x << ","
                    << agent.velocityMapMetersPerSecond.y << ","
                    << agent.velocityMapMetersPerSecond.z << ")"
                    << " forward=("
                    << agent.forwardMap.x << ","
                    << agent.forwardMap.y << ","
                    << agent.forwardMap.z << ")"
                    << " route_first_forward=("
                    << m_dockAdvice.plan.executionGates.front().forward.x << ","
                    << m_dockAdvice.plan.executionGates.front().forward.y << ","
                    << m_dockAdvice.plan.executionGates.front().forward.z << ")"
                    << std::endl;

                fail(
                    "client autopilot could not accept planner route: " +
                    (job->autopilotCompileFailure.empty()
                        ? std::string("unknown-start-failure")
                        : job->autopilotCompileFailure)
                );
                return;
            }

            if (job->compiledAutopilotLaw ==
                game::navigation::LocalFlightControlLaw::Assisted)
            {
                m_clientAssistedRouteAutopilot =
                    std::move(job->compiledAssistedAutopilotState);
                NewtonianClientAutopilot::stop(
                    m_clientNewtonianRouteAutopilot
                );
            }
            else
            {
                m_clientNewtonianRouteAutopilot =
                    std::move(job->compiledNewtonianAutopilotState);
                AssistedClientAutopilot::stop(
                    m_clientAssistedRouteAutopilot
                );
            }

            m_clientDockingPhase =
                ClientDockingPhase::Executing;
            std::cerr
                << "[DockClient] request=" << pending.serial
                << " phase=executing"
                << " control_path=ShipControlState"
                << std::endl;
        }
        else
        {
            m_clientDockingPhase =
                ClientDockingPhase::RouteReady;
            m_clientAutopilotControlActive = false;
            m_clientAutopilotControl = {};
            std::cerr
                << "[DockClient] request=" << pending.serial
                << " phase=manual"
                << " human_control=1"
                << std::endl;
        }
    }

    const auto currentTunnelRollPhase =
        [&](double sampleUniverseTimeSeconds) -> double
        {
            if (!m_dockAdvice.serial ||
                !m_dockAdvice.plan.valid() ||
                m_dockAdvice.plan.gates.empty())
            {
                return 0.0;
            }

            const auto livePort =
                resolveDockingAdvisoryLocalPortAt(
                    m_dockAdvice.portAttachment,
                    m_dockAdvice.portDefinition,
                    sampleUniverseTimeSeconds
                );
            if (!livePort.valid)
                return 0.0;

            glm::dvec3 terminalRouteAxis =
                m_dockAdvice.plan.gates.back().forward;
            if (glm::length(terminalRouteAxis) <= 1.0e-9)
                return 0.0;
            terminalRouteAxis = glm::normalize(terminalRouteAxis);

            const auto liveReference =
                game::navigation::TwoPointRollGeometry::
                    fromCenterAndUp(
                        livePort.positionMeters,
                        livePort.up
                    );

            return game::navigation::RouteFrameField::
                synchronizedTunnelRollPhase(
                    terminalRouteAxis,
                    m_dockAdvice.dockCenterReference,
                    m_dockAdvice.dockBottomReference,
                    liveReference.center,
                    liveReference.radialPoint
                );
        };

    if (m_clientDockingPhase ==
        ClientDockingPhase::Executing)
    {
        const auto handleOutput = [&](const auto& output) -> bool
        {
            if (output.valid)
            {
                m_clientDockingRouteCurveIndex =
                    output.routeCurveIndex;
                m_clientAutopilotCrossTrackMeters =
                    output.crossTrackErrorMeters;
                m_clientAutopilotCourseErrorRad =
                    output.courseErrorRad;
                m_clientAutopilotHullErrorRad =
                    output.hullForwardErrorRad;
                m_clientAutopilotTargetSpeedMps =
                    output.targetSpeedMps;
                m_clientAutopilotTargetLongitudinalAccelerationMps2 =
                    output.targetLongitudinalAccelerationMps2;
    
                const double routeTotalMeters =
                    m_dockAdvice.plan.routeCurves.empty()
                        ? 0.0
                        : m_dockAdvice.plan.routeCurves.back().
                            endProgressMeters;
    
                m_clientDockingRouteProgressMeters =
                    std::clamp(
                        routeTotalMeters -
                            output.exactRemainingRouteMeters,
                        0.0,
                        std::max(0.0, routeTotalMeters)
                    );
            }
    
            if (!output.valid)
            {
                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=execution-replan"
                    << " reason=follower-invalid"
                    << std::endl;
                releaseClientAutopilot();
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                submitAutopilotControl(coastForPlanningControl());
                return false;
            }
    
            if (output.complete)
            {
                releaseClientAutopilot();
                m_clientDockingPhase =
                    ClientDockingPhase::RouteReady;
    
                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=complete"
                    << " human_control=1"
                    << std::endl;
            }
            else
            {
                submitAutopilotControl(output.control);
    
                if (m_clientDockingTraceTick == 0 ||
                    m_client->lastSimulationMetadata().serverTick -
                        m_clientDockingTraceTick >= 60)
                {
                    m_clientDockingTraceTick =
                        m_client->lastSimulationMetadata().serverTick;
    
                    std::cerr
                        << "[DockClientTrack] request="
                        << pending.serial
                        << " page=" << output.pageIndex
                        << " segment=" << output.segmentIndex
                        << " cross_track_m="
                        << output.crossTrackErrorMeters
                        << " remaining_m="
                        << output.remainingDistanceMeters
                        << " target_speed_mps="
                        << output.targetSpeedMps
                        << " actual_speed_mps="
                        << glm::length(motion.localVelocityMps)
                        << " checkpoint="
                        << output.checkpointIndex
                        << " profile_rev="
                        << output.speedProfileRevision
                        << " target_accel_mps2="
                        << output.targetLongitudinalAccelerationMps2
                        << " cross_closing_mps="
                        << output.crossTrackClosingSpeedMps
                        << " course_lead_m="
                        << output.courseLeadDistanceMeters
                        << " predicted_cross_m="
                        << output.predictedCrossTrackMeters
                        << " center_deadband_m="
                        << output.centeringDeadbandMeters
                        << " curve=" << output.routeCurveIndex
                        << " curvature_1pm="
                        << output.routeCurvaturePerMeter
                        << " radius_m="
                        << output.routeRadiusMeters
                        << " exact_remaining_m="
                        << output.exactRemainingRouteMeters
                        << " desired_course_omega_radps="
                        << output.desiredCourseAngularRateRadPerSec
                        << " actual_course_omega_radps="
                        << output.actualCourseAngularRateRadPerSec
                        << " hull_omega_radps="
                        << output.actualAngularRateRadPerSec
                        << " phase_lead_deg="
                        << output.coursePhaseLeadAngleRad *
                            57.2957795130823208768
                        << " course_tau_s="
                        << output.courseResponseSeconds
                        << " brake_a_mps2="
                        << output.effectiveBrakingAuthorityMps2
                        << " stop_need_m="
                        << output.requiredTerminalStopDistanceMeters
                        << " terminal_brake="
                        << (output.terminalBrakeActive ? 1 : 0)
                        << " turn_vmax_mps="
                        << output.turnSpeedCeilingMps
                        << " turn_distance_m="
                        << output.distanceToTurnMeters
                        << " turn_need_m="
                        << output.requiredTurnSlowdownDistanceMeters
                        << " turn_slew_s="
                        << output.turnSpeedSetpointSlewSeconds
                        << " capture_angle_deg="
                        << output.crossTrackCorrectionAngleRad *
                            57.2957795130823208768
                        << " capture_omega_radps="
                        << output.desiredCaptureAngularRateRadPerSec
                        << " roll_err_deg="
                        << output.signedRollErrorRad *
                            57.2957795130823208768
                        << " roll_target_omega_radps="
                        << output.desiredRollRateRadPerSec
                        << " tunnel_roll_phase_deg="
                        << output.liveTunnelRollPhaseRad *
                            57.2957795130823208768
                        << " tunnel_roll_omega_radps="
                        << output.liveTunnelRollRateRadPerSec
                        << " brake_attitude_lock="
                        << (output.brakeAttitudeLockActive ? 1 : 0)
                        << " terminal_attitude_capture="
                        << (output.terminalAttitudeCaptureActive ? 1 : 0)
                        << " forward_err_deg="
                        << output.forwardErrorRad *
                            57.2957795130823208768
                        << " up_err_deg="
                        << output.upErrorRad *
                            57.2957795130823208768
                        << " terminal_hold="
                        << (output.terminalHold ? 1 : 0)
                        << " pitch=" << output.control.pitchInput
                        << " yaw=" << output.control.yawInput
                        << " roll=" << output.control.rollInput
                        << " strafe=" << output.control.strafeInput
                        << " lift=" << output.control.liftInput
                        << " speed_rate="
                        << output.control.targetSpeedRate
                        << " course_err_deg="
                        << output.courseErrorRad *
                            57.2957795130823208768
                        << " hull_err_deg="
                        << output.hullForwardErrorRad *
                            57.2957795130823208768
                        << " gimbal_cmd_r="
                        << output.control.mainGimbalRightInput
                        << " gimbal_cmd_u="
                        << output.control.mainGimbalUpInput
                        << " main_a_mps2="
                        << glm::length(
                            motion.mainEngineAccelerationMps2
                        )
                        << " rcs_a_mps2="
                        << glm::length(
                            motion.manoeuvreAccelerationMps2
                        )
                        << " gimbal_yaw_deg="
                        << motion.mainGimbalYawRad *
                            57.2957795130823208768
                        << " gimbal_pitch_deg="
                        << motion.mainGimbalPitchRad *
                            57.2957795130823208768
                        << std::endl;
                }
            }
    
            return true;
        };

        const double tunnelRollPhase =
            currentTunnelRollPhase(universeTimeSeconds);

        if (controlLaw ==
            game::navigation::LocalFlightControlLaw::Assisted)
        {
            AssistedClientAutopilot::setLiveTunnelRollPhase(
                m_clientAssistedRouteAutopilot,
                tunnelRollPhase,
                fixedDt
            );
            const auto output =
                AssistedClientAutopilot::update(
                    m_clientAssistedRouteAutopilot,
                    makeAgent(),
                    effectivePhysics,
                    universeTimeSeconds,
                    fixedDt
                );
            if (!handleOutput(output))
                return;
        }
        else
        {
            NewtonianClientAutopilot::setLiveTunnelRollPhase(
                m_clientNewtonianRouteAutopilot,
                tunnelRollPhase,
                fixedDt
            );
            const auto output =
                NewtonianClientAutopilot::update(
                    m_clientNewtonianRouteAutopilot,
                    makeAgent(),
                    effectivePhysics,
                    universeTimeSeconds,
                    fixedDt
                );
            if (!handleOutput(output))
                return;
        }
    }

    auto& active = m_dockAdvice;
    if (!active.serial || !active.plan.valid())
        return;

    // Presentation uses the exact same client Planner product that Automatic
    // executes. Server navigation state is not involved.
    const auto& playerRenderFrame =
        player.renderReferenceFrame;
    if (!playerRenderFrame.valid ||
        playerRenderFrame.systemId != active.systemId ||
        playerRenderFrame.hubId != active.hubId ||
        playerRenderFrame.type != MotionMode::HubTactical)
    {
        return;
    }

    const double renderTime =
        playerRenderFrame.universeTimeSeconds;
    auto renderFrame =
        playerRenderFrame.kinematicFrame();
    renderFrame.frameId =
        playerRenderFrame.hubId;

    // WORKING CONTRACT: HUD and hull sample the same pure two-point dock
    // phase function. Only sample time differs (render time vs fixed-sim time).
    const double dockRollPhaseRad =
        currentTunnelRollPhase(renderTime);

    const auto makeRoute =
        [&](const std::vector<
                game::navigation::planner::RouteGate>& routeGates,
            const std::vector<glm::dvec3>& routeFrameUp,
            const std::vector<double>& dockPhaseWeights,
            const std::string& id,
            bool sparseFrames)
        {
            GuidanceCorridor route;
            route.id = id;
            route.systemId = active.systemId;
            route.source = GuidanceSource::DockingComputer;
            route.purpose = GuidancePurpose::Approach;
            route.advisoryOnly = !automatic;
            route.noSafePrimarySolution =
                m_noSafeDockingGuidanceSolution;
            route.priority = automatic ? 100 : 50;
            route.generatedAtUniverseTimeSeconds =
                renderTime;
            route.spatialAdvisoryGates = sparseFrames;
            route.hubLocalFrameId = active.hubId;
            route.frames.reserve(routeGates.size());
            route.hubLocalGatePositionsMeters.reserve(
                routeGates.size()
            );

            for (std::size_t index = 0;
                 index < routeGates.size();
                 ++index)
            {
                const auto& gate = routeGates[index];
                route.hubLocalGatePositionsMeters.push_back(
                    gate.positionMeters
                );

                GuidanceFrame frameView;
                frameView.universeTimeSeconds = renderTime;
                frameView.centerMeters =
                    renderFrame.localToWorldPosition(
                        gate.positionMeters
                    );

                glm::dvec3 forward =
                    renderFrame.localToWorldVector(
                        gate.forward
                    );
                if (glm::length(forward) <= 1.0e-9)
                    forward =
                        glm::dvec3(0.0, 0.0, -1.0);
                forward = glm::normalize(forward);

                glm::dvec3 localFrameUp =
                    index < routeFrameUp.size()
                        ? routeFrameUp[index]
                        : active.routeUpReference;

                glm::dvec3 localForward = gate.forward;
                if (glm::length(localForward) <= 1.0e-9)
                    localForward = glm::dvec3(0.0, 0.0, -1.0);
                localForward = glm::normalize(localForward);

                // Dynamic dock roll belongs only to the
                // terminal-owned part of the route. Traffic-volume stages
                // retain their own frame; transition spans blend ownership.
                const double dockPhaseWeight =
                    index < dockPhaseWeights.size()
                        ? std::clamp(
                            dockPhaseWeights[index],
                            0.0,
                            1.0
                          )
                        : 1.0;
                localFrameUp =
                    game::navigation::RouteFrameField::
                        rotateUpAroundForward(
                            localForward,
                            localFrameUp,
                            dockRollPhaseRad * dockPhaseWeight
                        );

                glm::dvec3 up =
                    renderFrame.localToWorldVector(
                        localFrameUp
                    );
                up -=
                    forward * glm::dot(up, forward);
                if (glm::length(up) <= 1.0e-9)
                {
                    const glm::dvec3 seed =
                        std::abs(forward.y) < 0.90
                            ? glm::dvec3(0.0, 1.0, 0.0)
                            : glm::dvec3(1.0, 0.0, 0.0);
                    up =
                        seed -
                        forward *
                            glm::dot(seed, forward);
                }
                up = glm::normalize(up);
                const glm::dvec3 right =
                    glm::normalize(
                        glm::cross(forward, up)
                    );
                up =
                    glm::normalize(
                        glm::cross(right, forward)
                    );

                frameView.orientation =
                    glm::normalize(
                        glm::quat_cast(
                            glm::dmat3(
                                right,
                                up,
                                -forward
                            )
                        )
                    );

                const double remainingMeters =
                    glm::length(
                        routeGates.back().positionMeters -
                        gate.positionMeters
                    );
                const auto section =
                    dockingAdvisoryCrossSection(
                        remainingMeters,
                        active.lateralToleranceMeters,
                        active.verticalToleranceMeters
                    );

                frameView.widthMeters =
                    dockingAdvisoryFrameExtentMeters(
                        active.shipWidthMeters,
                        section.lateralToleranceMeters
                    );
                frameView.heightMeters =
                    dockingAdvisoryFrameExtentMeters(
                        active.shipHeightMeters,
                        section.verticalToleranceMeters
                    );
                frameView.lateralToleranceMeters =
                    section.lateralToleranceMeters;
                frameView.verticalToleranceMeters =
                    section.verticalToleranceMeters;
                frameView.recommendedSpeedMps =
                    std::max(0.0, gate.speedMps);
                frameView.requiredVehiclePose =
                    index + 1 == routeGates.size();

                route.frames.push_back(
                    std::move(frameView)
                );
            }

            return route;
        };

    guidance.publish(
        makeRoute(
            active.plan.executionGates,
            active.visualExecutionGateUp,
            active.visualExecutionGateDockPhaseWeight,
            m_activeDockingGuidanceCorridorId,
            false
        )
    );

    guidance.publish(
        makeRoute(
            active.plan.gates,
            active.visualGateUp,
            active.visualGateDockPhaseWeight,
            m_activeDockingGuidanceCorridorId + ":frames",
            true
        )
    );
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


    // Optional real-runtime damaged-propulsion oracle.
    if (m_shipRuntimeDamageOracle.enabled())
    {
        game::debug::ShipRuntimeDamageOracle::Observation obs;
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
            obs.speedMps = glm::length(motion.localVelocityMps);
            obs.pitchRateRadPerSec = transform.pitchRate;
            obs.yawRateRadPerSec = transform.yawRate;
            obs.rollRateRadPerSec = transform.rollRate;
            obs.trimLoad01 = motion.mainTrimLoad01;
            obs.trimUsableScale01 = motion.mainTrimUsableScale01;

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
                obs.mainForwardAccelerationMps2 =
                    glm::dot(motion.mainEngineAccelerationMps2, forwardLocal);
                obs.mainRightAccelerationMps2 =
                    glm::dot(motion.mainEngineAccelerationMps2, rightLocal);
                obs.mainUpAccelerationMps2 =
                    glm::dot(motion.mainEngineAccelerationMps2, upLocal);
            }

            obs.mainAlphaPitchRadPerSec2 =
                motion.mainPropulsionAngularAccelerationBodyRadPerSec2.x;
            obs.mainAlphaYawRadPerSec2 =
                motion.mainPropulsionAngularAccelerationBodyRadPerSec2.y;
            obs.mainAlphaRollRadPerSec2 =
                motion.mainPropulsionAngularAccelerationBodyRadPerSec2.z;
        }

        m_shipRuntimeDamageOracle.update(obs);

        game::ship::plant::PhysicalControlModelId requestedModel;
        if (m_shipRuntimeDamageOracle.takeModelRequest(requestedModel))
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type =
                ClientShipCommand::SelectPhysicalControlModel;
            command.index = static_cast<int>(requestedModel);
            msg.payload = command;
            m_client->sendMessage(msg);

            std::cerr
                << "[ShipDamageOracle] requested model="
                << game::ship::plant::physicalControlModelName(requestedModel)
                << std::endl;
        }

        std::size_t slotIndex = 0;
        if (m_shipRuntimeDamageOracle.takeDamageRequest(slotIndex))
        {
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type =
                ClientShipCommand::DebugFailPhysicalPropulsionSlot;
            command.index = static_cast<int>(slotIndex);
            msg.payload = command;
            m_client->sendMessage(msg);

            std::cerr
                << "[ShipDamageOracle] requested failed propulsion slot="
                << slotIndex
                << std::endl;
        }

        if (m_shipRuntimeDamageOracle.complete() &&
            !m_shipDamageOracleCompleteAnnounced)
        {
            m_shipDamageOracleCompleteAnnounced = true;
            std::cerr
                << "[ShipDamageOracle] COMPLETE report="
                << m_shipRuntimeDamageOracle.reportPath()
                << std::endl;
        }
    }


    // Optional real-runtime ship control oracle. Enabled only with
    // ELITE_SHIP_CONTROL_ORACLE=1. It drives the exact ordinary pilot-control
    // stream in handleInput() and observes the authoritative replicated ship
    // state here. No planner, corridor or direct physics shortcut participates.
    if (m_shipRuntimeControlOracle.enabled() &&
        !m_shipRuntimeDamageOracle.enabled())
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
            std::cerr
                << "[ShipControlOracle] COMPLETE report="
                << m_shipRuntimeControlOracle.reportPath()
                << std::endl;
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

    const double dockingGuidanceStartMs = nowMs();

    const bool directControlScenario =
        m_runtimeScenarioSelection.kind ==
            game::debug::RuntimeScenarioKind::ShipControl ||
        m_runtimeScenarioSelection.kind ==
            game::debug::RuntimeScenarioKind::ShipDamage;

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
            game::presentation::PlayerHudTelemetryTextProfile textProfile;
            textProfile.cellLabel = localizedUiText(context().app, "cockpit.cell", "CELL");
            textProfile.relativeVelocityLabel = localizedUiText(context().app, "cockpit.vrel", "VREL");
            textProfile.newtonianModeLabel =
                localizedUiText(context().app, "cockpit.mode.newtonian", "NEWTONIAN");
            textProfile.assistedModeLabel =
                localizedUiText(context().app, "cockpit.mode.assisted", "ASSISTED");

            const auto telemetry =
                game::presentation::buildPlayerHudTelemetry(
                    it->second,
                    textProfile
                );

            game::presentation::applyPlayerHudTelemetry(
                *uiRoot,
                telemetry
            );
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
                    playerShip
                );
            m_engineThrustIndicatorRenderer.render(
                engineThrustInstrument,
                vp
            );

            // The hull boresight is a fundamental attitude cue, not a velocity
            // cue and not a navigation-module product. Keep it present whenever
            // the cockpit HUD is present.
            m_flightVectorIndicatorRenderer.renderBoresight(vp);

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
                        playerShip,
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

            const double terminalNowSeconds =
                m_client->lastSimulationMetadata().serverTimeSeconds;

            const bool dockingVisible =
                m_clientDockingPhase != ClientDockingPhase::Idle ||
                !m_dockingGuidanceFailureReason.empty() ||
                !m_dockingStatusWarning.empty();

            const auto biteState =
                playerShip.descriptor
                    ? game::presentation::buildShipBiteState(
                          *playerShip.descriptor,
                          playerShip.modules
                      )
                    : game::bite::State {};

            const auto biteLines =
                game::bite::BiteApi::terminalLines(biteState);
            const bool biteVisible = !biteLines.empty();

            // BITE is allowed to request presentation attention, never control.
            // The same terminal surface is shared with docking diagnostics so
            // a propulsion fault remains visible even while docking is idle.
            terminal.visible = dockingVisible || biteVisible;
            terminal.title = localizedUiText(
                context().app,
                dockingVisible
                    ? "cockpit.terminal.title"
                    : "cockpit.terminal.system_title",
                dockingVisible ? "DOCKING" : "SYSTEM"
            );

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

                std::ostringstream shipLine;
                shipLine
                    << "SHIP " << physicalControlModelName(model)
                    << " LAW "
                    << game::navigation::localFlightControlLawName(
                           playerShip.renderTransform.motion.localControlLaw
                       );
                terminal.ship = shipLine.str();

                std::ostringstream physicsLine;
                physicsLine
                    << "ACT main=" << (aft + fore)
                    << " aft=" << aft
                    << " fore=" << fore
                    << " rcs=" << manoeuvre;
                terminal.physics = physicsLine.str();
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
                    terminal.phase = localizedUiText(
                        context().app,
                        "cockpit.terminal.idle",
                        "idle"
                    );
                    break;
                case ClientDockingPhase::Stabilizing:
                    terminal.phase = localizedUiText(
                        context().app,
                        "cockpit.terminal.stabilization",
                        "stabilization"
                    );
                    break;
                case ClientDockingPhase::Planning:
                    terminal.phase = localizedUiText(
                        context().app,
                        "cockpit.terminal.planning",
                        "planning"
                    );
                    break;
                case ClientDockingPhase::RouteReady:
                    terminal.phase = localizedUiText(
                        context().app,
                        "cockpit.terminal.route_ready",
                        "route ready"
                    );
                    break;
                case ClientDockingPhase::Executing:
                    terminal.phase = localizedUiText(
                        context().app,
                        "cockpit.terminal.autopilot",
                        "autopilot"
                    );
                    break;
                case ClientDockingPhase::SecuringCourse:
                    terminal.phase =
                        m_dockAssistedSafeRecovery.stopFallback ||
                        m_dockSafeCourse.stopFallback
                            ? localizedUiText(
                                  context().app,
                                  "cockpit.terminal.stop",
                                  "stop"
                              )
                            : localizedUiText(
                                  context().app,
                                  "cockpit.terminal.safe_course",
                                  "safe-course"
                              );
                    break;
            }

            if (m_dockAdviceJob)
            {
                const int permille =
                    std::clamp(
                        m_dockAdviceJob->planningProgressPermille.load(
                            std::memory_order_acquire
                        ),
                        0,
                        1000
                    );
                const int stage =
                    static_cast<int>(
                        m_dockAdviceJob->planningProgressStage.load(
                            std::memory_order_acquire
                        )
                    );

                std::ostringstream plannerLine;
                plannerLine
                    << localizedUiText(
                           context().app,
                           "cockpit.terminal.planner_on",
                           "planner on"
                       )
                    << " S" << stage
                    << " " << (permille / 10) << "%"
                    << " W="
                    << m_dockWorkerCount->load(
                           std::memory_order_acquire
                       );
                terminal.planner = plannerLine.str();

                // Presentation-only activity trace. This is intentionally a
                // deterministic pseudo-hex visualization of diagnostic state,
                // not memory contents and not planner input/output.
                const std::uint64_t bucket =
                    static_cast<std::uint64_t>(
                        std::max(0.0, std::floor(terminalNowSeconds))
                    );
                std::uint64_t x =
                    m_dockAdviceJob->context.serial *
                        0x9E3779B97F4A7C15ULL ^
                    static_cast<std::uint64_t>(stage + 1) *
                        0xBF58476D1CE4E5B9ULL ^
                    static_cast<std::uint64_t>(permille + 1) *
                        0x94D049BB133111EBULL ^
                    bucket;
                auto nextHexWord = [&]()
                {
                    x ^= x >> 12;
                    x ^= x << 25;
                    x ^= x >> 27;
                    x *= 0x2545F4914F6CDD1DULL;
                    return static_cast<unsigned int>(x & 0xFFFFULL);
                };

                std::ostringstream activity;
                activity
                    << "CALC "
                    << std::hex << std::uppercase << std::setfill('0')
                    << std::setw(4) << nextHexWord() << " "
                    << std::setw(4) << nextHexWord() << " "
                    << std::setw(4) << nextHexWord() << " "
                    << std::setw(4) << nextHexWord();
                terminal.activity = activity.str();
            }
            else
            {
                terminal.planner =
                    m_clientDockingPhase == ClientDockingPhase::Executing ||
                    m_clientDockingPhase == ClientDockingPhase::RouteReady
                        ? localizedUiText(
                              context().app,
                              "cockpit.terminal.planner_ready",
                              "planner ready"
                          )
                        : localizedUiText(
                              context().app,
                              "cockpit.terminal.planner_off",
                              "planner off"
                          );
            }

            if (m_dockAssistedSafeRecovery.active)
            {
                terminal.safety = localizedUiText(
                    context().app,
                    "cockpit.terminal.safe_course_acquiring",
                    "safe-course acquiring"
                );
            }
            else if (m_dockSafeCourse.active)
            {
                terminal.safety = localizedUiText(
                    context().app,
                    "cockpit.terminal.safe_course_active",
                    "safe-course active"
                );
            }
            else
            {
                terminal.safety = localizedUiText(
                    context().app,
                    "cockpit.terminal.safe_course_clear",
                    "safe-course clear"
                );
            }

            if (m_dockLateEntry.active)
            {
                terminal.handoff =
                    localizedUiText(
                        context().app,
                        "cockpit.terminal.handoff_rebind",
                        "handoff rebind"
                    ) +
                    " " +
                    std::to_string(
                        m_dockLateEntry.rebindAttempts
                    );

                terminal.error =
                    localizedUiText(
                        context().app,
                        m_dockLateEntry.rebindAttempts > 0
                            ? "cockpit.terminal.warning_handoff_retry"
                            : "cockpit.terminal.warning_handoff_rebinding",
                        m_dockLateEntry.rebindAttempts > 0
                            ? "warning: handoff drift; connector retry"
                            : "warning: handoff drift; rebinding connector"
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
                terminal.handoff = localizedUiText(
                    context().app,
                    "cockpit.terminal.handoff_accepted",
                    "handoff accepted"
                );
            }

            if (!m_dockingGuidanceFailureReason.empty())
            {
                terminal.error =
                    localizedUiText(
                        context().app,
                        "cockpit.terminal.error_prefix",
                        "error:"
                    ) +
                    " " +
                    m_dockingGuidanceFailureReason;
            }
            else if (terminal.error.empty())
            {
                terminal.error = m_dockingStatusWarning;
            }

            const auto terminalOutput =
                m_dockingStatusTerminal.update(
                    terminal,
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
}

game::presentation::SystemMapPanelPresentation
SpaceState::buildNativeSystemMapPanelPresentation()
{
    game::presentation::SystemMapPanelPresentation empty;
    if (!context().app || !m_client)
        return empty;

    const auto& nav = m_client->playerNavigation();
    const auto& loc = context().app->localization();
    std::string currentSystemName = loc.text(
        "map.interstellar",
        "INTERSTELLAR"
    );

    if (nav.currentSystemId >= 0 && m_client->resolveCelestialSnapshot())
    {
        if (const auto* celestial = m_client->celestialSnapshot())
        {
            currentSystemName = loc.catalogName(
                "systems",
                std::to_string(nav.currentSystemId),
                celestial->systemName
            );
        }
    }

    int selectedId = -1;
    if (!m_systemMapShowsEmptySector)
    {
        selectedId = m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : nav.currentSystemId;
    }

    game::presentation::SystemMapPanelPresentationInput input;
    input.universeTimeSeconds = m_client->universeTimeSeconds();
    input.universeDate = m_client->sessionSnapshot().universeDate;
    input.universeTimeScale = m_client->sessionSnapshot().universeTimeScale;
    input.mode = m_systemMapRenderer.mode();
    input.galaxy = m_hasGalaxyMapSnapshot ? &m_galaxyMapSnapshot : nullptr;
    input.system = &m_systemMapSnapshot;
    input.navigation = &nav;
    input.currentSystemName = currentSystemName;
    input.selectedEmptySector = m_systemMapShowsEmptySector;
    input.selectedSystemId = selectedId;
    input.selectedBodyId = m_systemMapRenderer.selectedBodyId();
    input.selectedHubId = m_systemMapRenderer.selectedHubId();

    input.systemLayerIsSpace = m_systemMapShowsEmptySector;
    if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy)
    {
        const auto entryIntent =
            m_systemMapRenderer.selectedGalaxyEntryIntent(m_galaxyMapSnapshot);
        if (entryIntent.has_value())
        {
            input.systemLayerIsSpace =
                entryIntent->type == game::system_map::MapIntentType::EnterEmptySector;
        }
        else
        {
            input.systemLayerIsSpace = nav.currentSystemId < 0;
        }
    }

    input.canOpenDetail =
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System &&
        m_systemMapRenderer.canOpenSelectedDetail();
    input.selectedDetailCell = m_systemMapRenderer.selectedTerminalDetailCell();
    input.canOpenHub =
        (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System ||
         m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Detail) &&
        m_systemMapRenderer.canOpenSelectedLocalContext();

    return game::presentation::buildSystemMapPanelPresentation(input);
}

void SpaceState::openSelectedGalaxyMapTarget()
{
    if (!m_client)
        return;

    const auto intent = m_systemMapRenderer.selectedGalaxyEntryIntent(
        m_galaxyMapSnapshot);

    if (intent.has_value())
    {
        using game::system_map::MapIntentType;
        if (intent->type == MapIntentType::EnterKnownSystem)
        {
            setSystemMapKnownSystemMode(intent->systemId);
            return;
        }
        if (intent->type == MapIntentType::EnterEmptySector)
        {
            setSystemMapEmptySectorMode(intent->positionLy);
            return;
        }
    }

    // No explicit Galaxy target: preserve the old panel behavior and open the
    // player's current system (or current interstellar sector).
    setSystemMapPlayerSystemMode();
}

void SpaceState::applyNativeSystemMapPanelAction(
    const game::presentation::SystemMapPanelAction& action)
{
    using game::presentation::SystemMapPanelCommandType;

    const auto command = game::presentation::resolveSystemMapPanelAction(
        action,
        m_systemMapRenderer.mode());

    switch (command.type)
    {
        case SystemMapPanelCommandType::None:
            return;

        case SystemMapPanelCommandType::OpenSelectedGalaxyTarget:
            openSelectedGalaxyMapTarget();
            return;

        case SystemMapPanelCommandType::SelectSystem:
            if (command.systemId >= 0)
                selectSystemMapSystem(command.systemId);
            return;

        case SystemMapPanelCommandType::Galaxy:
            setSystemMapGalaxyMode();
            return;

        case SystemMapPanelCommandType::LoadedSystem:
            setSystemMapLoadedSystemMode();
            return;

        case SystemMapPanelCommandType::LoadedDetail:
            setSystemMapLoadedDetailMode();
            return;

        case SystemMapPanelCommandType::SelectedDetail:
            setSystemMapDetailMode();
            return;

        case SystemMapPanelCommandType::Hub:
            setSystemMapHubMode();
            return;
    }
}

bool SpaceState::handleNativeSystemMapPanelInput(const Viewport& viewport)
{
    if (!m_client)
        return false;

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return false;

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    const bool insidePanel =
        m_inSessionPresentationRenderer.systemMapPanelContains(
            viewport,
            mouseX,
            mouseY);

    const bool leftDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // Scroll belongs to exactly one owner. The old WebView panel naturally
    // swallowed wheel input; the native panel must preserve that boundary.
    const double scrollY = insidePanel
        ? Input::instance().consumeScrollY()
        : 0.0;

    const auto panel = buildNativeSystemMapPanelPresentation();
    const auto action =
        m_inSessionPresentationRenderer.handleSystemMapPanelInput(
            viewport,
            panel,
            mouseX,
            mouseY,
            leftDown,
            scrollY);

    if (action.has_value())
        applyNativeSystemMapPanelAction(*action);

    return insidePanel;
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



void SpaceState::handleResize(int width, int height)
{
    if (m_playerView)
    {
        m_playerView->resize(width, height);
        m_playerView->updateBoundary(width, height);


    }
}



void SpaceState::pushAttachmentEditorState()
{
    json payload;
    payload["shipTypes"] = json::array();
    payload["selectedShipTypeId"] = m_attachmentEditorSelectedShipTypeId;

    // =========================================================
    // 1. List of editable ship types
    // =========================================================
    // Здесь добавь реальные типы из своего registry/descriptors.
    // Ниже пример структуры.
    //
    // payload["shipTypes"].push_back({
    //     {"typeId", "cobra_mk1"},
    //     {"displayName", "Cobra Mk1"}
    // });
    //
    // payload["shipTypes"].push_back({
    //     {"typeId", "station01"},
    //     {"displayName", "Station 01"}
    // });

    // ======= ПРИМЕР: если у тебя пока только Cobra =======
    payload["shipTypes"].push_back({
        {"typeId", "cobra_mk1"},
        {"displayName", "Cobra Mk1"}
    });

    // =========================================================
    // 2. Preview ship for selected type
    // =========================================================
    //
    // Здесь надо собрать exactly тот же JSON, который сейчас
    // attachment_editor уже умеет рисовать:
    // {
    //   shipId,
    //   displayName,
    //   meshParts,
    //   attachments,
    //   visualBasisRotationDeg, ...
    // }
    //
    // Но НЕ из runtime ship, а из descriptor/type preview.
    //
    // Ниже логика:
    //
    // json preview = buildAttachmentEditorPreviewForType(m_attachmentEditorSelectedShipTypeId);
    // payload["selectedShip"] = preview;

    // Пока назови поле именно selectedShip
    // чтобы потом HTML не таскал массив objects[0].
    payload["selectedShip"] = buildAttachmentEditorPreviewForType(*this, m_attachmentEditorSelectedShipTypeId);

    context().htmlUi().broadcastState(HtmlUiPanelId::AttachmentEditor, payload);
}





void SpaceState::processHtmlCommands()
{





    auto cmds = context().htmlUi().popCommands();

    for (const auto& msg : cmds)
    {

        // ------------------------------
        // SYSTEM MAP
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::SystemMap)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                if (context().htmlUi().state().activePanel != HtmlUiPanelId::SystemMap)
                {
                    context().htmlUi().setActivePanel(HtmlUiPanelId::SystemMap);
                }

                pushSystemMapState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {



                if (msg.command == "request_snapshot")
                {
                    pushSystemMapState();
                    continue;
                }


                if (msg.command == "close")
                {
                    if (Application* app = context().app)
                    {
                        // Legacy HtmlUi SystemMap close follows the same
                        // explicit presentation-router contract as the
                        // persistent map panel: return to the last selected
                        // F1-F4 flight view, never to an implicit UI-none
                        // gameplay fallback.
                        app->requestLastFlightView();
                    }

                    continue;
                }
            }
        }





        // ------------------------------
        // ATTACHMENT EDITOR
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::AttachmentEditor)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushAttachmentEditorState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushAttachmentEditorState();
                    continue;
                }

                if (msg.command == "select_ship_type")
                {
                    m_attachmentEditorSelectedShipTypeId =
                        msg.payload.value("shipTypeId", m_attachmentEditorSelectedShipTypeId);

                    pushAttachmentEditorState();
                    continue;
                }

                if (msg.command == "update_attachment")
                {
                    const std::string attachmentId = msg.payload.value("attachmentId", "");
                    if (attachmentId.empty())
                        continue;

                    const auto posArr = msg.payload.value("localPosition", std::vector<float>{0,0,0});
                    const auto rotArr = msg.payload.value("localRotationDeg", std::vector<float>{0,0,0});
                    const bool enabled = msg.payload.value("enabled", true);

                    if (posArr.size() < 3 || rotArr.size() < 3)
                        continue;

                    auto& ov = m_attachmentEditorOverrides[attachmentId];
                    ov.localPosition = glm::vec3(posArr[0], posArr[1], posArr[2]);
                    ov.localRotationDeg = glm::vec3(rotArr[0], rotArr[1], rotArr[2]);
                    ov.enabled = enabled;

                    std::cerr
                        << "[AttachmentEditor APPLY] id=" << attachmentId
                        << " pos=("
                        << ov.localPosition.x << ", "
                        << ov.localPosition.y << ", "
                        << ov.localPosition.z << ") rot=("
                        << ov.localRotationDeg.x << ", "
                        << ov.localRotationDeg.y << ", "
                        << ov.localRotationDeg.z << ") enabled="
                        << (ov.enabled ? "true" : "false")
                        << " overridesCount=" << m_attachmentEditorOverrides.size()
                        << "\n";

                    pushAttachmentEditorState();
                    continue;
                }
            }
        }
                // ------------------------------
        // VOLUME VIEWER
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::VolumeViewer)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushVolumeViewerState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushVolumeViewerState();
                    continue;
                }

                if (msg.command == "select_ship_type")
                {
                    m_attachmentEditorSelectedShipTypeId =
                        msg.payload.value("shipTypeId", m_attachmentEditorSelectedShipTypeId);

                    pushVolumeViewerState();
                    continue;
                }
            }
        }

        // ------------------------------
        // DEBUG CONTROL
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::DebugControl)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushDebugControlState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushDebugControlState();
                    continue;
                }

                if (msg.command == "apply_settings")
                {
                    applyDebugControlPayload(msg.payload);
                    ++m_debugControlSettingsRevision;
                    deferDebugControlStatePush();
                    continue;
                }

                if (msg.command == "save_defaults")
                {
                    applyDebugControlPayload(msg.payload);
                    saveDebugControlDefaults(msg.payload);
                    ++m_debugControlSettingsRevision;
                    deferDebugControlStatePush();
                    continue;
                }

                if (msg.command == "reset_settings")
                {
                    resetDebugControlSettings();
                    ++m_debugControlSettingsRevision;
                    deferDebugControlStatePush();
                    continue;
                }
            }
        }



                // ------------------------------
        // STRUCTURE DEBUG
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::StructureDebug)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                requestStructureDebugStateRefresh();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    requestStructureDebugStateRefresh();
                    continue;
                }

                if (msg.command == "select_ship_entity")
                {
                    m_structureDebugSelectedShipEntityId =
                        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

                    pushStructureDebugState();
                    continue;
                }

if (msg.command == "destroy_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->destroyShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "detach_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->detachShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "hang_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->hangShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "reevaluate_structure")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_debugSession->reevaluateShipStructure(id);
    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "restore_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->restoreShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}


                if (msg.command == "set_link_health")
                    {
                        const uint64_t entityId =
                            msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

                        const std::string linkId =
                            msg.payload.value("linkId", std::string{});

                        const float health =
                            msg.payload.value("health", 0.0f);

                        const bool destroyed =
                            msg.payload.value("destroyed", health <= 0.0f);

                        if (!linkId.empty())
                        {
                            EntityId id{ static_cast<uint32_t>(entityId) };
                            m_debugSession->setShipStructuralLinkHealth(
                                id,
                                linkId,
                                health,
                                destroyed
                            );
                        }

                        requestStructureDebugStateRefresh();
                        continue;
                    }

if (msg.command == "reset_ship")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_debugSession->resetShipStructure(id);
    requestStructureDebugStateRefresh();
    continue;
}

if (msg.command == "reset_all_ships")
{
    m_debugSession->resetAllShipStructures();
    requestStructureDebugStateRefresh();
    continue;
}
            }
        }


        // ------------------------------
        // SHIP CORE
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::ShipCore)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushShipCoreState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushShipCoreState();
                    continue;
                }

                if (msg.command == "select_ship_entity")
                {
                    m_shipCoreSelectedShipEntityId =
                        msg.payload.value("entityId", m_shipCoreSelectedShipEntityId);

                    pushShipCoreState();
                    continue;
                }

                if (msg.command == "repair_all_panels")
                {
                    if (m_shipCoreSelectedShipEntityId != m_playerId.value)
                    {
                        pushShipCoreState();
                        continue;
                    }

                    ClientShipCommand cmd;
                    cmd.type = ClientShipCommand::RepairAllPanels;

                    std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
                    m_debugCommands.push_back(cmd);
                    continue;
                }

                if (msg.command == "damage_radiator")
                {
                    // ClientShipCommand is player-owned on the authoritative
                    // server. Do not let a browser selection make this command
                    // appear to target an NPC while actually damaging the player.
                    if (m_shipCoreSelectedShipEntityId != m_playerId.value)
                    {
                        pushShipCoreState();
                        continue;
                    }

                    ClientShipCommand cmd;
                    cmd.type = ClientShipCommand::DamageRadiator;
                    cmd.index = msg.payload.value("panel_index", 0);
                    cmd.amount = msg.payload.value("amount", 0.2);

                    std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
                    m_debugCommands.push_back(cmd);
                    continue;
                }
            }
        }

        // ------------------------------
        // FRUSTUM DEBUG
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::FrustumDebug)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command &&
                msg.command == "request_snapshot")
            {
                continue;
            }
        }
    }
}


void SpaceState::requestStructureDebugStateRefresh()
{
    if (!m_debugSession)
        return;

    m_structureDebugAwaitingSnapshotRevision =
        m_debugSession->snapshotRevision();
    m_structureDebugRefreshPending = true;
    m_debugSession->refreshStructureSnapshot();
}

void SpaceState::deferDebugControlStatePush()
{
    if (!m_debugSession)
    {
        pushDebugControlState();
        return;
    }

    m_debugControlAwaitingStateRevision =
        m_debugSession->stateRevision();
    m_debugControlStatePushPending = true;
}

void SpaceState::flushPendingDebugUiState()
{
    if (m_structureDebugRefreshPending && m_debugSession &&
        m_debugSession->snapshotRevision() >
            m_structureDebugAwaitingSnapshotRevision)
    {
        m_structureDebugRefreshPending = false;
        pushStructureDebugState();
    }

    if (m_debugControlStatePushPending && m_debugSession &&
        m_debugSession->stateRevision() >
            m_debugControlAwaitingStateRevision)
    {
        m_debugControlStatePushPending = false;
        pushDebugControlState();
    }
}


void SpaceState::pushStructureDebugState()
{

    json payload;
    payload["ships"] = json::array();
    payload["selectedShipEntityId"] = 0;
    payload["modules"] = json::array();
    payload["links"] = json::array();
    payload["hasData"] = false;
    payload["reason"] = "no_server_ships";

    // snapshot() is a copied diagnostic value. A refresh request is issued by
    // the UI command path before the server advances; never retain a reference
    // into authoritative runtime memory here.
    const auto snapshot = m_debugSession->snapshot();

    if (snapshot.ships.empty())
    {
        context().htmlUi().broadcastState(HtmlUiPanelId::StructureDebug, payload);
        return;
    }

    payload["hasData"] = true;
    payload["reason"] = "ok";

    const ShipSnapshot* selectedShip = nullptr;

    for (const auto& s : snapshot.ships)
    {
        if (s.id.value == m_structureDebugSelectedShipEntityId)
        {
            selectedShip = &s;
            break;
        }
    }

    if (!selectedShip)
    {
        for (const auto& s : snapshot.ships)
        {
            if (s.id.value == m_playerId.value)
            {
                selectedShip = &s;
                m_structureDebugSelectedShipEntityId = s.id.value;
                break;
            }
        }
    }

    if (!selectedShip)
    {
        selectedShip = &snapshot.ships.front();
        m_structureDebugSelectedShipEntityId = selectedShip->id.value;
    }

    const auto& ship = *selectedShip;

    payload["selectedShipEntityId"] = m_structureDebugSelectedShipEntityId;

    for (const auto& s : snapshot.ships)
    {
        const uint64_t id = s.id.value;

        json item;
        item["entityId"] = id;
        item["displayName"] = std::string("Ship #") + std::to_string(id);
        item["role"] = (s.role == ShipRole::Player) ? "Player" : "Npc";
        item["isPlayer"] = (s.role == ShipRole::Player);

        payload["ships"].push_back(std::move(item));
    }

    if (ship.graph.hasModules)
    {
        // The debug page needs a rich view, but static ship/module definitions
        // are deliberately not replicated. Rehydrate them from the same local
        // descriptor catalog used by the normal client presentation.
        const auto& descriptor = ObjectDescriptorRegistry::get(ship.typeId);
        const auto moduleViews = game::client::buildModuleViews(
            descriptor.moduleDescriptors(),
            ship.graph.modules
        );

        for (const auto& mod : moduleViews)
        {
            json m;

            m["moduleId"] = mod.moduleId;
            m["parentModuleId"] = mod.parentModuleId;
            m["subsystemId"] = mod.subsystemId;

            m["state"] = mod.state;
            m["health"] = mod.health;
            m["maxHealth"] = mod.maxHealth;

            m["destructible"] = mod.destructible;
            m["detachable"] = mod.detachable;
            m["hangable"] = mod.hangable;

            m["destroyPolicy"] = mod.destroyPolicy;
            m["detachPolicy"] = mod.detachPolicy;
            m["attachmentType"] = mod.attachmentType;

            m["meshPartIds"] = mod.meshPartIds;
            m["supportModuleIds"] = mod.supportModuleIds;

            m["minSupportsForAttached"] = mod.minSupportsForAttached;
            m["minSupportsForStable"] = mod.minSupportsForStable;
            m["aliveSupportCount"] = mod.aliveSupportCount;

            m["localPosition"] = json::array({ 0.0, 0.0, 0.0 });
            m["localRotationDeg"] = json::array({ 0.0, 0.0, 0.0 });

            payload["modules"].push_back(std::move(m));
        }
    }

    if (ship.graph.hasStructuralLinks)
    {
        for (const auto& link : ship.graph.structuralLinks)
        {
            json l;

            l["id"] = link.id;
            l["ownerModuleId"] = link.ownerModuleId;
            l["moduleAId"] = link.moduleAId;
            l["moduleBId"] = link.moduleBId;
            l["kind"] = link.kind;

            l["health"] = link.health;
            l["maxHealth"] = link.maxHealth;
            l["impulseTolerance"] = link.impulseTolerance;

            l["loadBearing"] = link.loadBearing;
            l["destroyed"] = link.destroyed;
            l["autoGenerated"] = link.autoGenerated;

            l["center"] = json::array({
                double(link.center.x),
                double(link.center.y),
                double(link.center.z)
            });

            l["halfSize"] = json::array({
                double(link.halfSize.x),
                double(link.halfSize.y),
                double(link.halfSize.z)
            });

            json orientation = json::array();
            orientation.push_back(json::array({
                double(link.orientation[0].x),
                double(link.orientation[0].y),
                double(link.orientation[0].z)
            }));
            orientation.push_back(json::array({
                double(link.orientation[1].x),
                double(link.orientation[1].y),
                double(link.orientation[1].z)
            }));
            orientation.push_back(json::array({
                double(link.orientation[2].x),
                double(link.orientation[2].y),
                double(link.orientation[2].z)
            }));
            l["orientation"] = std::move(orientation);

            payload["links"].push_back(std::move(l));
        }
    }

    context().htmlUi().broadcastState(HtmlUiPanelId::StructureDebug, payload);
}


void SpaceState::pushShipCoreState()
{
    const auto& ships = m_client->world().ships();
    if (ships.empty())
        return;

    if (m_shipCoreSelectedShipEntityId == 0 || ships.find(m_shipCoreSelectedShipEntityId) == ships.end())
    {
        auto itPlayer = ships.find(m_playerId.value);
        if (itPlayer != ships.end())
            m_shipCoreSelectedShipEntityId = itPlayer->first;
        else
            m_shipCoreSelectedShipEntityId = ships.begin()->first;
    }

    auto it = ships.find(m_shipCoreSelectedShipEntityId);
    if (it == ships.end())
        return;

    const auto& ship = it->second;

    json payload = shipCoreStatusToJson(ship.shipCoreStatus);

    // список доступных кораблей для будущего selector в HTML
    payload["ships"] = json::array();
    for (const auto& [id, s] : ships)
    {
        json item;
        item["entityId"] = id;
        item["displayName"] = std::string("Ship #") + std::to_string(id);
        item["role"] = (s.role == ShipRole::Player) ? "Player" : "Npc";
        item["isPlayer"] = (s.role == ShipRole::Player);
        payload["ships"].push_back(item);
    }

    payload["selectedShipEntityId"] = m_shipCoreSelectedShipEntityId;

    context().htmlUi().broadcastState(HtmlUiPanelId::ShipCore, payload);
}


void SpaceState::pushFrustumDebugState(const json& payload)
{
    // Frustum telemetry is an external diagnostic stream. It must not steal
    // the global in-game activePanel from other browser tools.
    context().htmlUi().broadcastState(HtmlUiPanelId::FrustumDebug, payload);
}


void SpaceState::pushDebugControlState()
{
    const auto& dbg = debug::get().render;

    json payload =
        game::debug_control::encodeRenderSettings(dbg);
    payload["debugSettingsRevision"] = m_debugControlSettingsRevision;

    SceneRenderStats totalStats = m_perfMainStats;
    totalStats.add(m_perfRearStats);

    json perf;
    perf["frameIndex"] = m_perfFrameIndex;

    perf["fps"] = m_perfFps;
    perf["frameMs"] = m_perfFrameMs;

    perf["updateMs"] = m_perfUpdateMs;
    perf["processHtmlMs"] = m_perfProcessHtmlMs;
    perf["fixedSimMs"] = m_perfFixedSimMs;
    perf["serverFixedSteps"] = m_perfServerFixedSteps;
    perf["serverTickDebtMs"] = m_perfServerTickDebtMs;
    perf["serverDiscardedMs"] = m_perfServerDiscardedMs;
    perf["serverTotalDiscardedMs"] = m_perfServerTotalDiscardedMs;
    perf["serverCatchUpLimited"] = m_perfServerCatchUpLimited;
    perf["clientUpdateMs"] = m_perfClientUpdateMs;
    perf["dockingGuidanceMs"] = m_perfDockingGuidanceMs;
    perf["dockingRequestActive"] = m_perfDockingRequestActive;
    perf["scenePrepareMs"] = m_perfScenePrepareMs;
    perf["playerViewMs"] = m_perfPlayerViewMs;
    perf["uiRootUpdateMs"] = m_perfUiRootUpdateMs;

    perf["mainRenderMs"] = m_perfMainRenderMs;
    perf["rearCameraMs"] = m_perfRearCameraMs;
    perf["renderUiMs"] = m_perfRenderUiMs;
    perf["hudMs"] = m_perfHudMs;

    perf["sceneCpuTotalMs"] = m_perfMainStats.cpuTotalMs;
    perf["sceneCpuSetupMs"] = m_perfMainStats.cpuSetupMs;
    perf["sceneCpuStarfieldMs"] = m_perfMainStats.cpuStarfieldMs;
    perf["sceneCpuFarPassesMs"] = m_perfMainStats.cpuFarPassesMs;
    perf["sceneCpuLabelsMs"] = m_perfMainStats.cpuLabelsMs;
    perf["sceneCpuRealShipsMs"] = m_perfMainStats.cpuRealShipsMs;
    perf["sceneCpuVisualShipsMs"] = m_perfMainStats.cpuVisualShipsMs;
    perf["sceneCpuVisualDronesMs"] = m_perfMainStats.cpuVisualDronesMs;
    perf["sceneCpuObjectsMs"] = m_perfMainStats.cpuObjectsMs;
    perf["sceneCpuDebugCallbackMs"] = m_perfMainStats.cpuDebugCallbackMs;

    const auto& hubPerf =
        m_systemMapRenderer
            .hubMapPerformanceStats();

    perf["hubMapActive"] =
        m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Hub;

    perf["hubCpuTotalMs"] =
        hubPerf.cpuTotalMs;

    perf["hubCpuBackgroundMs"] =
        hubPerf.cpuBackgroundMs;

    perf["hubCpuPlanetBackdropMs"] =
        hubPerf.cpuPlanetBackdropMs;

    perf["hubCpuGeometryMs"] =
        hubPerf.cpuGeometryMs;

    perf["hubCpuLabelsMs"] =
        hubPerf.cpuLabelsMs;

    perf["hubGpuValid"] =
        hubPerf.gpuValid;

    perf["hubGpuTotalMs"] =
        hubPerf.gpuTotalMs;

    perf["hubGpuBackgroundMs"] =
        hubPerf.gpuBackgroundMs;

    perf["hubGpuFallbackBodyMs"] =
        hubPerf.gpuFallbackBodyMs;

    perf["hubGpuSurfaceMs"] =
        hubPerf.gpuSurfaceMs;

    perf["hubGpuCloudsMs"] =
        hubPerf.gpuCloudsMs;

    perf["hubGpuAtmosphereMs"] =
        hubPerf.gpuAtmosphereMs;

    perf["hubGpuGeometryMs"] =
        hubPerf.gpuGeometryMs;

    perf["hubGpuLabelsMs"] =
        hubPerf.gpuLabelsMs;

    perf["drawCalls"] = totalStats.drawCalls;
    perf["modulesDrawn"] = totalStats.modulesDrawn;
    perf["modulesCulled"] = totalStats.modulesCulled;
    perf["partsDrawn"] = totalStats.partsDrawn;
    perf["partsCulled"] = totalStats.partsCulled;


    perf["realShipsDrawn"] = totalStats.realShipsDrawn;
    perf["realShipPartsDrawn"] = totalStats.realShipPartsDrawn;

    perf["visualShipsDrawn"] = totalStats.visualShipsDrawn;
    perf["visualShipsCulled"] = totalStats.visualShipsCulled;

    perf["visualProxyShipsDrawn"] = totalStats.visualProxyShipsDrawn;
    perf["visualFullShipsDrawn"] = totalStats.visualFullShipsDrawn;
    perf["visualShipPartsDrawn"] = totalStats.visualShipPartsDrawn;

    payload["performance"] = perf;
    payload["debugFastUniverseTime"] =
        m_debugSession
            ? m_debugSession->fastUniverseTime()
            : false;

    /*
        These values come from the latest copied server-side debug state.
        Apply/reset waits for stateRevision() to advance before this payload is
        pushed, so the HTML page never needs a direct read of ServerRuntime.
    */
    payload["debugUniverseTimeSimulation"] =
        m_debugSession
            ? m_debugSession->universeTimeSimulation()
            : false;

    payload["debugUniverseTimeScale"] =
        m_debugSession
            ? m_debugSession->universeTimeScale()
            : 1.0;

    payload["debugUniverseTimeConfiguredScale"] =
        m_debugSession
            ? m_debugSession->configuredUniverseTimeScale()
            : 10000.0;

    payload["systemMapVisible"] =
        m_systemMapVisible;

    payload["systemMapLiveSnapshotsEnabled"] =
        m_systemMapLiveSnapshotsEnabled;

    payload["liveSystemMapId"] =
        m_liveSystemMapId;

    payload["detailMapSampleMode"] =
        m_authoritativeMapInterpolator.detailSampleModeName();
    payload["detailMapNewestGapMs"] =
        m_authoritativeMapInterpolator.detailNewestGapSeconds() * 1000.0;
    payload["detailMapBufferedSnapshots"] =
        m_authoritativeMapInterpolator.detailBufferedSnapshotCount();

    payload["hubMapSampleMode"] =
        m_authoritativeMapInterpolator.hubSampleModeName();
    payload["hubMapNewestGapMs"] =
        m_authoritativeMapInterpolator.hubNewestGapSeconds() * 1000.0;
    payload["hubMapBufferedSnapshots"] =
        m_authoritativeMapInterpolator.hubBufferedSnapshotCount();

    context().htmlUi().broadcastState(HtmlUiPanelId::DebugControl, payload);
}

void SpaceState::loadDebugControlDefaults()
{
    namespace fs = std::filesystem;

    const fs::path path = debugControlDefaultsPath();
    if (!fs::exists(path))
        return;

    try
    {
        std::ifstream input(path);
        if (!input)
            return;

        json payload;
        input >> payload;
        if (!payload.is_object())
            return;

        /*
            Older Debug Control files may predate the transactional diagnostic
            contract and contain an active simulation flag. Startup preferences
            may configure the scale, but they must never start a debug timeline.
        */
        payload.erase("debugUniverseTimeSimulation");
        payload.erase("debugFastUniverseTime");

        applyDebugControlPayload(payload);

        std::cout
            << "[DebugControl] loaded startup defaults from "
            << path.string()
            << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[DebugControl] failed to load startup defaults: "
            << e.what()
            << "\n";
    }
}

bool SpaceState::saveDebugControlDefaults(const json& payload)
{
    namespace fs = std::filesystem;

    const fs::path path = debugControlDefaultsPath();

    try
    {
        if (!path.parent_path().empty())
            fs::create_directories(path.parent_path());

        std::ofstream output(path, std::ios::trunc);
        if (!output)
            return false;

        json persistentPayload = payload;

        // Runtime session state is transactional, not a startup preference.
        // Persist the configured scale if desired, but never boot directly
        // into an accelerated diagnostic branch.
        persistentPayload.erase("debugUniverseTimeSimulation");
        persistentPayload.erase("debugFastUniverseTime");

        output << std::setw(2) << persistentPayload << '\n';

        std::cout
            << "[DebugControl] saved startup defaults to "
            << path.string()
            << "\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[DebugControl] failed to save startup defaults: "
            << e.what()
            << "\n";
        return false;
    }
}

void SpaceState::resetDebugControlSettings()
{
    namespace fs = std::filesystem;

    debug::get().render = debug::DebugRenderSettings{};

    if (m_debugSession)
        m_debugSession->setUniverseTimeSimulation(false, 1.0);

    const fs::path path = debugControlDefaultsPath();
    std::error_code ec;
    fs::remove(path, ec);

    if (ec)
    {
        std::cerr
            << "[DebugControl] failed to remove startup defaults: "
            << ec.message()
            << "\n";
    }
}

void SpaceState::applyDebugControlPayload(const json& payload)
{
    auto& dbg = debug::get().render;
    const std::string previousSceneMode = dbg.sceneMode;

    game::debug_control::applyRenderSettings(dbg, payload);

    if (dbg.sceneMode != previousSceneMode)
    {
        if (m_client)
            m_promoSceneScenario.reset(m_client->world());
    }

    if (m_debugSession)
    {
        const bool simulationEnabled =
            payload.value(
                "debugUniverseTimeSimulation",
                m_debugSession->universeTimeSimulation()
            );

        const double simulationScale =
            payload.value(
                "debugUniverseTimeScale",
                m_debugSession->configuredUniverseTimeScale()
            );

        m_debugSession->setUniverseTimeSimulation(
            simulationEnabled,
            simulationScale
        );
    }


}



void SpaceState::pushVolumeViewerState()
{
    json payload;

    if (m_attachmentEditorSelectedShipTypeId.empty())
        m_attachmentEditorSelectedShipTypeId = "cobra_mk1";

    payload["shipTypes"] = json::array({
        {
            {"typeId", "cobra_mk1"},
            {"displayName", "Cobra Mk1"}
        },
        {
            {"typeId", "station_01"},
            {"displayName", "Station 01"}
        }
    });

    payload["selectedShipTypeId"] = m_attachmentEditorSelectedShipTypeId;
    payload["selectedShip"] =
        buildVolumeViewerPreviewForType(*this, m_attachmentEditorSelectedShipTypeId);

    context().htmlUi().broadcastState(HtmlUiPanelId::VolumeViewer, payload);
}



void SpaceState::pushSystemMapState()
{
    if (!m_client)
        return;

    if (!m_client->resolveCelestialSnapshot())
        return;

    const auto* atlasPtr = m_client->starAtlas();
    const auto* celestialPtr = m_client->celestialSnapshot();
    if (!atlasPtr || !celestialPtr)
        return;

    const auto& atlas = *atlasPtr;
    const auto& celestial = *celestialPtr;
    const auto& nav = m_client->playerNavigation();

    json payload;

    payload["universeTimeSeconds"] =
        m_client->universeTimeSeconds();

    payload["universeDate"] =
        m_client->sessionSnapshot().universeDate;

    payload["universeTimeScale"] =
        m_client->sessionSnapshot().universeTimeScale;

    payload["currentSystemId"] = nav.currentSystemId;
    payload["system"]["id"] = celestial.systemId;
    payload["system"]["name"] = celestial.systemName;
    payload["simTimeSeconds"] = celestial.simTimeSeconds;

    payload["player"]["positionAu"] = {
        {"x", nav.systemLocalAu.x},
        {"y", nav.systemLocalAu.y},
        {"z", nav.systemLocalAu.z}
    };

    payload["player"]["forward"] = {
        {"x", nav.forward.x},
        {"y", nav.forward.y},
        {"z", nav.forward.z}
    };

    payload["systems"] = json::array();

    const world::celestial::GalaxyMapSystem* currentSystem = nullptr;



for (const auto& s : m_galaxyMapSnapshot.systems)
{
    if (s.id == nav.currentSystemId)
    {
        currentSystem = &s;
        break;
    }
}

auto distanceFromPlayerLy =
    [&](const world::celestial::GalaxyMapSystem& s) -> double
{
    if (!currentSystem)
        return 0.0;

    const double dx = s.positionLy.x - currentSystem->positionLy.x;
    const double dy = s.positionLy.y - currentSystem->positionLy.y;
    const double dz = s.positionLy.z - currentSystem->positionLy.z;

    return std::sqrt(dx * dx + dy * dy + dz * dz);
};


const int selectedId =
    m_systemMapRenderer.selectedSystemId() >= 0
        ? m_systemMapRenderer.selectedSystemId()
        : nav.currentSystemId;

for (const auto& s : m_galaxyMapSnapshot.systems)
{
    json item;
    item["id"] = s.id;
    item["name"] = s.name;
    item["starType"] = s.starType;
    item["starsCount"] = s.starsCount;
    item["xLy"] = s.positionLy.x;
    item["yLy"] = s.positionLy.y;
    item["zLy"] = s.positionLy.z;
    item["current"] = (s.id == nav.currentSystemId);
    item["selected"] = (s.id == selectedId);

    item["distanceFromPlayerLy"] = distanceFromPlayerLy(s);
    item["jurisdiction"] = s.jurisdiction.empty() ? "Unregistered" : s.jurisdiction;

    payload["systems"].push_back(std::move(item));
}

    payload["systemBodiesById"] = json::object();

    for (const auto& s : m_galaxyMapSnapshot.systems)
    {
        const auto* def = atlas.findSystem(s.id);
        if (!def)
            continue;

        payload["systemBodiesById"][std::to_string(s.id)] =
            celestialDefinitionToJson(*def);
    }

    payload["bodies"] = json::array();

    for (const auto& b : celestial.bodies)
    {
        json item;
        item["id"] = b.id;
        item["name"] = b.name;
        item["type"] = world::celestial::toString(b.type);
        item["parentId"] = b.parentId;

        item["positionAu"] = {
            {"x", b.positionAu.x},
            {"y", b.positionAu.y},
            {"z", b.positionAu.z}
        };

        item["radiusKm"] = b.radiusKm;
        item["orbitalPhaseRad"] = b.orbitalPhaseRad;
        item["rotationPhaseRad"] = b.rotationPhaseRad;

        item["rings"] = json::array();

        for (const auto& r : b.rings)
        {
            item["rings"].push_back({
                {"name", r.name},
                {"innerRadiusKm", r.innerRadiusKm},
                {"outerRadiusKm", r.outerRadiusKm},
                {"composition", r.composition}
            });
        }

        payload["bodies"].push_back(std::move(item));
    }

    context().htmlUi().broadcastState(HtmlUiPanelId::SystemMap, payload);
}





void SpaceState::selectSystemMapSystem(
    int systemId
)
{
    if (!m_client)
        return;

    /*
        В Galaxy выбор системы является движением внутри
        одной сцены. Здесь используется плавный перелёт камеры,
        без fade.
    */
    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Galaxy)
    {
        m_systemMapShowsEmptySector =
            false;

        m_systemMapRenderer.focusGalaxySystem(
            systemId,
            m_galaxyMapSnapshot
        );

        return;
    }

    /*
        System-system switching and Galaxy-system entry use the same
        atomic transition path.
    */
    setSystemMapKnownSystemMode(
        systemId
    );
}








void SpaceState::setSystemMapGalaxyMode()
{
    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Galaxy)
    {
        return;
    }

    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),

        [this]()
        {
            if (!m_client)
                return;

            requestGalaxyMapSnapshotOnce();

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::Galaxy
            );

            m_systemMapRenderer.onGalaxyMapEntered(
                m_galaxyMapSnapshot,
                m_client->playerNavigation()
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::Galaxy);

        }
    );
}










void SpaceState::setSystemMapEmptySectorMode(
    const glm::dvec3& positionLy
)
{
    if (!m_client)
        return;

    world::celestial::SystemMapSnapshot
        emptySector;

    /*
        Каждый пустой сектор получает отдельный отрицательный
        runtime-id. Благодаря этому SystemNavigationGrid и камера
        сбрасываются даже при переходе из одного пустого сектора
        в другой.
    */
    emptySector.systemId =
        m_nextEmptySystemMapId--;

    emptySector.systemName =
        "Deep Space Sector";

    emptySector.universeTimeSeconds =
        m_client->universeTimeSeconds();

    emptySector.universeTimeScale =
        m_client->sessionSnapshot().universeTimeScale;

    emptySector.universeDate =
        m_client->sessionSnapshot().universeDate;

    emptySector.systemPositionLy =
        positionLy;

    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),

        [this, emptySector]()
        {
            if (!m_client)
                return;

            m_systemMapSnapshot =
                emptySector;

            m_loadedSystemMapId =
                emptySector.systemId;

            m_hasSystemMapSnapshot =
                true;

            m_systemMapShowsEmptySector =
                true;

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::System
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::System);

        }
    );
}





void SpaceState::beginSystemMapSystemTransition(int systemId)
{
    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, systemId]()
        {
            if (!m_client ||
                !m_hasSystemMapSnapshot ||
                m_loadedSystemMapId != systemId)
            {
                return;
            }

            m_systemMapShowsEmptySector = false;
            m_systemMapRenderer.focusGalaxySystem(
                systemId,
                m_galaxyMapSnapshot
            );
            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::System
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::System);
        }
    );
}


void SpaceState::setSystemMapKnownSystemMode(int systemId)
{
    if (!m_client || systemId < 0)
        return;

    if (!composeSystemMapSnapshot(systemId))
        return;

    beginSystemMapSystemTransition(systemId);
}





void SpaceState::setSystemMapLoadedSystemMode()
{
    if (!m_client ||
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System)
    {
        return;
    }

    if (!m_hasSystemMapSnapshot)
    {
        setSystemMapPlayerSystemMode();
        return;
    }

    const int loadedSystemMapId = m_loadedSystemMapId;
    const bool loadedMapIsEmptySector = m_systemMapShowsEmptySector;

    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, loadedSystemMapId, loadedMapIsEmptySector]()
        {
            if (!m_client ||
                !m_hasSystemMapSnapshot ||
                m_loadedSystemMapId != loadedSystemMapId)
            {
                return;
            }

            m_systemMapShowsEmptySector = loadedMapIsEmptySector;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);
            if (context().app)
            {
                context().app->adoptNavigationView(
                    NavigationPresentationView::System);
            }
        }
    );
}


void SpaceState::setSystemMapCurrentSystemMode()
{
    if (!m_client)
        return;

    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::System)
    {
        return;
    }

    const int selectedId =
        m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : m_client->playerNavigation().currentSystemId;

    setSystemMapKnownSystemMode(
        selectedId
    );
}








bool SpaceState::preparePlayerNavigationMapLevel(PlayerNavigationMapLevel level)
{
    if (!m_client)
        return false;

    // Direct F9-F12 selection is presentation preparation, not an internal
    // map animation. The global coordinator keeps the committed owner until
    // this concrete Navigation scene is ready. Cancel any renderer-internal
    // drill/crossfade so an outgoing map snapshot can never be painted over a
    // direct function-key destination.
    m_systemMapRenderer.cancelMapTransition();

    switch (level)
    {
        case PlayerNavigationMapLevel::Galaxy:
        {
            requestGalaxyMapSnapshotOnce();
            if (!m_hasGalaxyMapSnapshot)
                return false;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Galaxy);
            m_systemMapRenderer.onGalaxyMapEntered(
                m_galaxyMapSnapshot,
                m_client->playerNavigation()
            );
            return true;
        }

        case PlayerNavigationMapLevel::System:
        {
            const auto& nav = m_client->playerNavigation();
            if (nav.currentSystemId >= 0)
            {
                if (!composeSystemMapSnapshot(nav.currentSystemId))
                    return false;
                m_systemMapShowsEmptySector = false;
                m_systemMapRenderer.focusGalaxySystem(
                    nav.currentSystemId,
                    m_galaxyMapSnapshot
                );
                m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);
                return true;
            }

            glm::dvec3 playerGalacticPositionLy {0.0};
            if (!resolvePlayerGalacticPositionLy(playerGalacticPositionLy))
                return false;

            world::celestial::SystemMapSnapshot emptySector;
            emptySector.systemId = m_nextEmptySystemMapId--;
            emptySector.systemName = "Deep Space Sector";
            emptySector.universeTimeSeconds = m_client->universeTimeSeconds();
            emptySector.universeTimeScale = m_client->sessionSnapshot().universeTimeScale;
            emptySector.universeDate = m_client->sessionSnapshot().universeDate;
            emptySector.systemPositionLy = playerGalacticPositionLy;
            m_systemMapSnapshot = std::move(emptySector);
            m_loadedSystemMapId = m_systemMapSnapshot.systemId;
            m_hasSystemMapSnapshot = true;
            m_systemMapShowsEmptySector = true;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);
            return true;
        }

        case PlayerNavigationMapLevel::Detail:
        {
            world::celestial::DetailTarget target;
            if (!buildPlayerDetailTarget(target, true))
                return preparePlayerNavigationMapLevel(PlayerNavigationMapLevel::System);
            if (!composeDetailMapSnapshot(target))
                return false;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Detail);
            return true;
        }

        case PlayerNavigationMapLevel::Local:
        {
            const auto& nav = m_client->playerNavigation();
            if (nav.currentSystemId < 0)
                return preparePlayerNavigationMapLevel(PlayerNavigationMapLevel::System);

            const auto& ships = m_client->world().ships();
            const auto playerIt = ships.find(m_playerId.value);
            if (playerIt != ships.end())
            {
                const auto& motion = playerIt->second.transform.motion;
                if (motion.matchedToReferenceFrame && !motion.hubId.empty())
                {
                    if (!composeSystemMapSnapshot(nav.currentSystemId))
                        return false;

                    world::celestial::DetailTarget parentTarget;
                    if (!buildPlayerDetailTarget(parentTarget, true) ||
                        !composeDetailMapSnapshot(parentTarget) ||
                        !composeHubMapSnapshot(nav.currentSystemId, motion.hubId))
                    {
                        return false;
                    }
                    m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Hub);
                    return true;
                }
            }

            world::celestial::DetailTarget target;
            if (!buildPlayerDetailTarget(target, false) ||
                !composeDetailMapSnapshot(target))
            {
                return false;
            }
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Detail);
            return true;
        }
    }

    return false;
}


void SpaceState::setSystemMapPlayerSystemMode()
{
    if (!m_client)
        return;

    const auto& nav = m_client->playerNavigation();
    if (nav.currentSystemId >= 0)
    {
        setSystemMapKnownSystemMode(nav.currentSystemId);
        return;
    }

    glm::dvec3 playerGalacticPositionLy {0.0};
    if (resolvePlayerGalacticPositionLy(playerGalacticPositionLy))
    {
        setSystemMapEmptySectorMode(playerGalacticPositionLy);
        return;
    }

}


void SpaceState::setSystemMapPlayerDetailMode()
{
    if (!m_client)
        return;

    world::celestial::DetailTarget target;
    if (!buildPlayerDetailTarget(target, true))
    {
        // Interstellar space has no system-local Details address. F10 remains
        // the highest meaningful navigation level there.
        setSystemMapPlayerSystemMode();
        return;
    }

    if (!composeDetailMapSnapshot(target))
        return;

    beginSystemMapDetailTransition(target);
}



void SpaceState::setSystemMapPlayerLocalMode()
{
    if (!m_client)
        return;

    const auto& nav = m_client->playerNavigation();
    if (nav.currentSystemId < 0)
    {
        setSystemMapPlayerSystemMode();
        return;
    }

    const auto& ships = m_client->world().ships();
    const auto playerIt = ships.find(m_playerId.value);
    if (playerIt != ships.end())
    {
        const auto& motion = playerIt->second.transform.motion;
        if (motion.matchedToReferenceFrame && !motion.hubId.empty())
        {
            if (!composeSystemMapSnapshot(nav.currentSystemId))
                return;

            world::celestial::DetailTarget parentTarget;
            if (!buildPlayerDetailTarget(parentTarget, true) ||
                !composeDetailMapSnapshot(parentTarget) ||
                !composeHubMapSnapshot(nav.currentSystemId, motion.hubId))
            {
                return;
            }
            beginSystemMapHubTransition(nav.currentSystemId, motion.hubId);
            return;
        }
    }

    world::celestial::DetailTarget target;
    if (!buildPlayerDetailTarget(target, false))
        return;

    if (!composeDetailMapSnapshot(target))
        return;

    beginSystemMapDetailTransition(target);
}




bool SpaceState::buildSelectedMapDetailTarget(
    world::celestial::DetailTarget& target
) const
{
    using namespace world::celestial;

    target = {};
    if (!m_client || !m_hasSystemMapSnapshot)
        return false;

    // Details is a refinement of the currently displayed System/Space map.
    // The loaded map context is authoritative for this drill; the player's
    // current physical membership may point at a completely different system.
    target.systemId = m_loadedSystemMapId;
    target.systemPositionLy = m_systemMapSnapshot.systemPositionLy;

    const std::string selectedBodyId = m_systemMapRenderer.selectedBodyId();
    const std::string selectedHubId = m_systemMapRenderer.selectedHubId();
    const std::string selectedHubParentBodyId =
        m_systemMapRenderer.selectedHubParentBodyId();

    if (!selectedBodyId.empty())
    {
        target.sceneKind = DetailSceneKind::CelestialBody;
        target.focusClass = DetailObjectClass::CelestialBody;
        target.anchorId = selectedBodyId;
        target.focusId = selectedBodyId;
    }
    else if (!selectedHubId.empty() && !selectedHubParentBodyId.empty())
    {
        target.sceneKind = DetailSceneKind::CelestialBody;
        target.focusClass = DetailObjectClass::Hub;
        target.anchorId = selectedHubParentBodyId;
        target.focusId = selectedHubId;
    }
    else if (!selectedHubId.empty())
    {
        target.sceneKind = DetailSceneKind::LocalObject;
        target.focusClass = DetailObjectClass::Hub;
        target.anchorId = selectedHubId;
        target.focusId = selectedHubId;
    }
    else
    {
        const auto selectedCell =
            m_systemMapRenderer.selectedTerminalDetailCell();
        if (!selectedCell)
            return false;

        target.sceneKind = DetailSceneKind::SpatialVolume;
        target.focusClass = DetailObjectClass::None;
        target.spatialCell = *selectedCell;
    }

    return target.valid();
}


void SpaceState::setSystemMapDetailMode()
{
    if (!m_client ||
        m_systemMapRenderer.mode() != SystemMapRenderer::Mode::System)
    {
        return;
    }

    world::celestial::DetailTarget target;
    if (!buildSelectedMapDetailTarget(target) ||
        !composeDetailMapSnapshot(target))
    {
        return;
    }

    beginSystemMapDetailTransition(target);
}



void SpaceState::beginSystemMapDetailTransition(
    const world::celestial::DetailTarget& target
)
{
    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, target]()
        {
            if (!m_client ||
                !m_hasDetailMapSnapshot ||
                m_loadedDetailTarget != target)
            {
                return;
            }

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::Detail
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::Detail);
        }
    );
}




void SpaceState::setSystemMapLoadedDetailMode()
{
    if (!m_client || m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Detail)
        return;

    const world::celestial::DetailTarget target = m_loadedDetailTarget;
    if (!target.valid())
    {
        setSystemMapPlayerDetailMode();
        return;
    }

    if (!m_hasDetailMapSnapshot && !composeDetailMapSnapshot(target))
        return;

    beginSystemMapDetailTransition(target);
}









void SpaceState::applyClientCatalogLocalization()
{
    if (!context().app)
        return;

    const auto& loc = context().app->localization();

    for (auto& system : m_galaxyMapSnapshot.systems)
    {
        system.name = loc.catalogName(
            "systems",
            std::to_string(system.id),
            system.name
        );
    }

    for (auto& object : m_galaxyMapSnapshot.objects)
    {
        object.name = loc.catalogName(
            "galaxy_objects",
            object.id,
            object.name
        );
    }

    std::unordered_map<int, std::string> systemDisplayNames;
    systemDisplayNames.reserve(m_galaxyMapSnapshot.systems.size());
    for (const auto& system : m_galaxyMapSnapshot.systems)
        systemDisplayNames[system.id] = system.name;
    m_sceneRenderer.setGameSystemDisplayNames(systemDisplayNames);

    if (m_systemMapSnapshot.systemId >= 0)
    {
        m_systemMapSnapshot.systemName = loc.catalogName(
            "systems",
            std::to_string(m_systemMapSnapshot.systemId),
            m_systemMapSnapshot.systemName
        );

        for (auto& body : m_systemMapSnapshot.bodies)
        {
            body.name = loc.catalogName(
                "bodies",
                std::to_string(m_systemMapSnapshot.systemId) + ":" + body.id,
                body.name
            );
        }

        for (auto& object : m_systemMapSnapshot.objects)
        {
            if (!object.stableId.empty())
            {
                object.name = loc.catalogName(
                    "hubs",
                    object.stableId,
                    object.name
                );
            }
        }
    }

    const auto localizeSceneObject =
        [&](world::celestial::LocalSceneObject& object, int systemId)
        {
            if (object.stableId.empty())
                return;

            switch (object.objectClass)
            {
                case world::celestial::DetailObjectClass::CelestialBody:
                    if (systemId >= 0)
                    {
                        object.name = loc.catalogName(
                            "bodies",
                            std::to_string(systemId) + ":" + object.stableId,
                            object.name
                        );
                    }
                    break;

                case world::celestial::DetailObjectClass::Hub:
                    object.name = loc.catalogName(
                        "hubs",
                        object.stableId,
                        object.name
                    );
                    break;

                default:
                    break;
            }
        };

    if (m_hasDetailMapSnapshot && m_detailMapSnapshot.valid)
    {
        if (m_detailMapSnapshot.systemId >= 0 &&
            !m_detailMapSnapshot.planetBodyId.empty())
        {
            m_detailMapSnapshot.planetName = loc.catalogName(
                "bodies",
                std::to_string(m_detailMapSnapshot.systemId) + ":" +
                    m_detailMapSnapshot.planetBodyId,
                m_detailMapSnapshot.planetName
            );
        }

        for (auto& orbit : m_detailMapSnapshot.hubOrbits)
        {
            orbit.name = loc.catalogName(
                "hubs",
                orbit.id,
                orbit.name
            );
        }

        for (auto& object : m_detailMapSnapshot.scene.objects)
            localizeSceneObject(object, m_detailMapSnapshot.systemId);
    }

    if (m_hasHubMapSnapshot && m_hubMapSnapshot.valid)
    {
        m_hubMapSnapshot.displayName = loc.catalogName(
            "hubs",
            m_hubMapSnapshot.hubId,
            m_hubMapSnapshot.displayName
        );

        for (auto& object : m_hubMapSnapshot.scene.objects)
            localizeSceneObject(object, m_hubMapSnapshot.systemId);
    }

    render::navigation::NavigationOverlayTextProfile overlayText;
    overlayText.player = loc.text("overlay.player", "PLAYER");
    overlayText.selected = loc.text("overlay.selected", "SELECTED");
    overlayText.cursor = loc.text("overlay.cursor", "CURSOR");
    overlayText.galaxy = loc.text("overlay.galaxy", "GALAXY");
    overlayText.system = loc.text("overlay.system", "SYSTEM");
    overlayText.edge = loc.text("overlay.edge", "EDGE");
    overlayText.format = loc.text("overlay.format", "FORMAT");
    overlayText.level = loc.text("overlay.level", "LEVEL");
    overlayText.track = loc.text("overlay.track", "TRACK");
    overlayText.trackOn = loc.text("overlay.track_on", "TRACK ON");
    overlayText.hierarchicalFormat =
        loc.text("nav.format.hierarchical", "STRAIGHT THERE");
    overlayText.axisFormat =
        loc.text("nav.format.axis", "THREE AXES");
    overlayText.packedFormat =
        loc.text("nav.format.packed", "VERY SECRET CODE");
    m_systemMapRenderer.setNavigationOverlayTextProfile(overlayText);

    game::system_map::NavigationMapTextProfile mapText;
    mapText.type = loc.text("map.object_info.type");
    mapText.name = loc.text("map.object_info.name");
    mapText.localSpeed = loc.text("map.object_info.local_speed");
    mapText.globalSpeed = loc.text("map.object_info.global_speed");
    mapText.azimuth = loc.text("map.object_info.azimuth");
    mapText.elevation = loc.text("map.object_info.elevation");
    mapText.owner = loc.text("map.object_info.owner");
    mapText.radius = loc.text("map.object_info.radius");
    mapText.address = loc.text("map.object_info.address");
    mapText.setWaypoint = loc.text("map.object_info.set_waypoint");
    mapText.setRendezvous = loc.text("map.object_info.set_rendezvous");
    mapText.cancelWaypoint = loc.text("map.object_info.cancel_waypoint");
    mapText.setFinish = loc.text("map.object_info.set_finish");
    mapText.cancelFinish = loc.text("map.object_info.cancel_finish");
    mapText.setIntermediate = loc.text("map.object_info.set_intermediate");
    mapText.cancelIntermediate = loc.text("map.object_info.cancel_intermediate");
    mapText.spaceTarget = loc.text("map.object_info.space_target");
    mapText.finishTarget = loc.text("map.object_info.finish_target");
    mapText.intermediateTarget = loc.text("map.object_info.intermediate_target");
    mapText.navigationPoint = loc.text("map.object_info.navigation_point");
    mapText.dockingPorts = loc.text("map.object_info.docking_ports");
    mapText.dockOpening = loc.text("map.object_info.dock_opening");
    mapText.shipEnvelope = loc.text("map.object_info.ship_envelope");
    mapText.dockFit = loc.text("map.object_info.dock_fit");
    mapText.dockStatus = loc.text("map.object_info.dock_status");
    mapText.dockAccess = loc.text("map.object_info.dock_access");
    mapText.dockOperational = loc.text("map.object_info.dock_operational");
    mapText.dockClearance = loc.text("map.object_info.dock_clearance");
    mapText.dockMaxEntrySpeed = loc.text("map.object_info.dock_max_entry_speed");
    mapText.calculateRoute = loc.text("map.object_info.calculate_route");
    mapText.showDockingRoute = loc.text("map.object_info.show_docking_route");
    mapText.startDocking = loc.text("map.object_info.start_docking");
    mapText.statusAvailable = loc.text("map.status.available");
    mapText.statusUnavailable = loc.text("map.status.unavailable");
    mapText.statusFree = loc.text("map.status.free");
    mapText.statusOccupied = loc.text("map.status.occupied");
    mapText.statusReserved = loc.text("map.status.reserved");
    mapText.statusAllowed = loc.text("map.status.allowed");
    mapText.statusDenied = loc.text("map.status.denied");
    mapText.statusClearanceRequired = loc.text("map.status.clearance_required");
    mapText.statusOnline = loc.text("map.status.online");
    mapText.statusOffline = loc.text("map.status.offline");
    mapText.statusDamaged = loc.text("map.status.damaged");
    mapText.statusUnknown = loc.text("map.status.unknown");
    mapText.routeTitle = loc.text("map.route.title");
    mapText.showOnHud = loc.text("map.route.show_on_hud");
    mapText.start = loc.text("map.route.start");
    mapText.waypoint = loc.text("map.route.waypoint");
    mapText.finish = loc.text("map.route.finish");
    mapText.dragWaypoints = loc.text("map.route.drag_waypoints");
    mapText.deleteRoute = loc.text("map.route.delete_route");
    mapText.deleteWaypoint = loc.text("map.route.delete_waypoint");
    mapText.yes = loc.text("confirm.yes");
    mapText.no = loc.text("confirm.no");
    mapText.arrivalSafeZone = loc.text("map.route.arrival.safe_zone");
    mapText.arrivalFollow = loc.text("map.route.arrival.follow");
    mapText.arrivalFormation = loc.text("map.route.arrival.formation");
    mapText.arrivalParade = loc.text("map.route.arrival.parade");
    m_systemMapRenderer.setNavigationMapTextProfile(mapText);
    m_systemMapRenderer.setNavigationNamingLocale(loc.locale());
}
