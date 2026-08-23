#include <glad/gl.h>
#include "src/core/RuntimeTrace.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <chrono>
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
#include "src/game/presentation/NavigationHudPresentation.h"
#include "src/game/presentation/GuidanceHudPresentation.h"
#include "src/game/presentation/GalacticCompassPresentation.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/navigation/LocalGuidancePlanner.h"

#include <chrono>
#include <algorithm>


#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include "core/Application.h"
#include "game/scene/GameSceneSetup.h"
#include "render/HUD/TextRenderer.h"

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
            if (context().app)
            {
                m_sceneRenderer.setUiLocale(
                    context().app->localization().locale()
                );
            }
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
    m_constellationOverlayEnabled =
        !m_constellationOverlayEnabled;

    m_sceneRenderer.setConstellationOverlayEnabled(
        m_constellationOverlayEnabled
    );

    std::cout
        << "[Constellations] gameplay layer "
        << (m_constellationOverlayEnabled ? "enabled" : "disabled")
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


void SpaceState::cycleSkyCulture()
{
    if (!m_sceneRenderer.cycleConstellationCulture())
        return;

    const std::string locale =
        context().app ? context().app->localization().locale() : "en";

    std::cout
        << "[Constellations] sky culture="
        << m_sceneRenderer.constellationCultureId()
        << " / "
        << m_sceneRenderer.constellationCultureDisplayName(locale)
        << std::endl;
}

void SpaceState::onUiLanguageChanged()
{
    if (!context().app)
        return;

    m_sceneRenderer.setUiLocale(context().app->localization().locale());
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

    if (context().app)
        m_sceneRenderer.setUiLocale(context().app->localization().locale());
    applyClientCatalogLocalization();
}


// =====================================================================================
// Input
// =====================================================================================
void SpaceState::handleInput()
{
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
        game::navigation::LocalFlightControlLaw::Newtonian;

    if (m_client)
    {
        const auto& ships = m_client->world().ships();
        const auto playerIt = ships.find(m_playerId.value);
        if (playerIt != ships.end())
            currentLocalControlLaw =
                playerIt->second.transform.motion.localControlLaw;
    }

    m_inputMapper.update(m_playerControl, currentLocalControlLaw);
    m_client->submitInput(m_playerControl);


}






//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####


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


// =====================================================================================
// Update
// =====================================================================================
void SpaceState::updateGuidanceTestLab(float dt)
{
    constexpr const char* CorridorId =
        "lab:guidance_dock_cube_a:dock_gate_front";

    if (!m_client)
        return;

    auto& modules = m_navigationWorkspace.modules();
    const bool computationEnabled =
        modules.enabled(game::navigation::NavigationModuleId::TrajectoryPrediction) &&
        modules.enabled(game::navigation::NavigationModuleId::SafetyEvaluation) &&
        modules.enabled(game::navigation::NavigationModuleId::LocalGuidance);

    if (!computationEnabled)
    {
        m_navigationWorkspace.guidance().erase(CorridorId);
        return;
    }

    m_guidanceLabReplanAccumulatorSeconds +=
        static_cast<double>(std::max(0.0f, dt));
    if (m_guidanceLabReplanAccumulatorSeconds < 0.20)
        return;
    m_guidanceLabReplanAccumulatorSeconds = 0.0;

    const auto playerIt =
        m_client->world().ships().find(m_playerId.value);
    if (playerIt == m_client->world().ships().end())
        return;

    const ClientShipState& player = playerIt->second;
    const int systemId = player.renderTransform.motion.systemId;
    if (systemId < 0)
    {
        m_navigationWorkspace.guidance().erase(CorridorId);
        return;
    }

    const ClientObjectState* targetObject = nullptr;
    for (const auto& [id, object] : m_client->world().objects())
    {
        (void)id;
        if (object.systemId == systemId &&
            object.type == ObjectType::GuidanceDockCube &&
            object.hubAttachment.valid &&
            object.hubAttachment.moduleId == "guidance_dock_cube_a")
        {
            targetObject = &object;
            break;
        }
    }

    const auto* anchorDefinition = m_hubSemanticAnchorCatalog.find(
        "guidance_dock_cube_a",
        "dock_gate_front"
    );
    if (!targetObject || !anchorDefinition)
    {
        m_navigationWorkspace.guidance().erase(CorridorId);
        return;
    }

    const double now = m_client->universeTimeSeconds();
    const auto targetAnchor = game::navigation::resolveHubSemanticAnchor(
        *anchorDefinition,
        systemId,
        now,
        world::coordinates::fullMeters(targetObject->renderWorldPosition),
        targetObject->linearVelocityMps,
        targetObject->renderOrientation,
        targetObject->angularVelocityWorldRadPerSecond
    );

    game::navigation::NavigationPlanningSnapshot environment;
    environment.systemId = systemId;
    environment.generatedAtUniverseTimeSeconds = now;
    environment.validUntilUniverseTimeSeconds = now + 2.0;

    // The client and server share the same packaged celestial catalog. Combine
    // current celestial kinematics with catalog GM/radius so the local lab uses
    // the same nearby-body gravity terms as the shared predictor contract.
    const auto* atlas = m_client->starAtlas();
    const auto* celestial = m_client->celestialSnapshot();
    if (atlas && celestial && celestial->systemId == systemId)
    {
        if (const auto* definition = atlas->findSystem(systemId))
        {
            for (const auto& state : celestial->bodies)
            {
                const world::celestial::CelestialBodyDefinition* bodyDef = nullptr;
                for (const auto& candidate : definition->bodies)
                {
                    if (candidate.id == state.id)
                    {
                        bodyDef = &candidate;
                        break;
                    }
                }

                if (!bodyDef || bodyDef->gravitationalParameterM3s2 <= 0.0)
                    continue;

                game::navigation::GravityBody gravity;
                gravity.id = state.id;
                gravity.centerMeters = state.worldMeters;
                gravity.radiusMeters = state.radiusKm * 1000.0;
                gravity.gravitationalParameterM3s2 =
                    bodyDef->gravitationalParameterM3s2;
                environment.gravityBodies.push_back(std::move(gravity));
            }
        }
    }

    // Diagnostic infrastructure other than the selected docking module becomes
    // known local traffic/obstacle geometry. V1 uses conservative spheres; the
    // semantic target itself is excluded because crossing its gate is intended.
    for (const auto& [id, object] : m_client->world().objects())
    {
        if (&object == targetObject || object.systemId != systemId)
            continue;
        if (object.type != ObjectType::GuidanceDockCube &&
            object.type != ObjectType::GuidanceDockCylinder)
        {
            continue;
        }

        game::navigation::NavigationObstacle obstacle;
        obstacle.id = "object:" + std::to_string(id);
        obstacle.systemId = systemId;
        obstacle.epochUniverseTimeSeconds = now;
        obstacle.positionMeters =
            world::coordinates::fullMeters(object.renderWorldPosition);
        obstacle.velocityMps = object.linearVelocityMps;
        obstacle.physicalRadiusMeters =
            object.type == ObjectType::GuidanceDockCylinder ? 650.0 : 520.0;
        obstacle.requiredClearanceMeters = 80.0;
        obstacle.positionUncertaintyMeters = 1.0;
        obstacle.velocityUncertaintyMps = 0.05;
        obstacle.validFromUniverseTimeSeconds = now - 1.0;
        obstacle.validUntilUniverseTimeSeconds = now + 60.0;
        obstacle.source =
            game::navigation::NavigationKnowledgeSource::AuthoritativeWorld;
        environment.obstacles.push_back(std::move(obstacle));
    }

    const glm::dvec3 playerMeters = world::coordinates::fullMeters(
        player.renderTransform.worldPosition
    );
    const double distanceToGate = glm::length(
        targetAnchor.positionMeters - playerMeters
    );

    game::navigation::LocalGuidanceRequest request;
    request.corridorId = CorridorId;
    request.systemId = systemId;
    request.startUniverseTimeSeconds = now;
    request.actorState.positionMeters = playerMeters;
    request.actorState.velocityMps = player.renderTransform.motion.worldVelocityMps;
    request.actorState.accelerationMps2 = glm::dvec3(0.0);
    request.actorProperAccelerationMps2 = glm::dvec3(0.0);
    request.target = targetAnchor;
    request.environment = std::move(environment);
    request.profile.purpose = game::navigation::GuidancePurpose::Docking;
    request.profile.horizonSeconds = std::clamp(
        distanceToGate / 120.0,
        8.0,
        30.0
    );
    request.profile.frameIntervalSeconds = 0.5;
    request.profile.predictorIntegrationStepSeconds = 0.05;
    request.profile.shipSafetyRadiusMeters = 24.0;
    request.profile.recommendedSpeedMps = targetAnchor.maxEntrySpeedMps;
    request.profile.maxClosureRateMps = targetAnchor.maxEntrySpeedMps;
    request.profile.motionEnvelope.maxProperAccelerationMps2 =
        6.0 * game::navigation::StandardGravityMps2;
    request.profile.motionEnvelope.maxProperJerkMps3 =
        1.5 * game::navigation::StandardGravityMps2;

    const auto result = game::navigation::LocalGuidancePlanner::plan(request);
    if (result.ready())
    {
        m_navigationWorkspace.guidance().publish(result.corridor);
    }
    else
    {
        // Never keep presenting a stale unsafe lab corridor. A future warning
        // layer can expose the conflict while an alternate planner searches.
        m_navigationWorkspace.guidance().erase(CorridorId);
    }

    m_navigationWorkspace.guidance().pruneExpired(now);
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


    updateGuidanceTestLab(clientFrameDt);

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









m_playerView->updateCockpitStateFromSnapshot(
    ship.transform.forwardVelocity,
    ship.transform.targetSpeed,
    ship.transform.cruiseActive,
    ship.signalPresentation.labelsVector()
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

    m_preparedScene =
        m_sceneRenderer.prepareScene(
            m_client->world(),
            m_playerId,
            observerGalacticPositionPtr
        );



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

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, vx, vy, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


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

                m_playerView->renderWorldLabels(
                    m_playerView->worldLabels(),
                    world::coordinates::legacyFloatMeters(
                        ship.renderTransform.worldPosition
                    ),
                    m_activeMainCamera->viewMatrix(),
                    m_activeMainCamera->projectionMatrix(),
                    vp
                );

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
                        navVocabulary
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
                    m_client->universeTimeSeconds()
                );

            m_guidanceCorridorRenderer.render(
                guidance,
                m_activeMainCamera->viewMatrix(),
                m_activeMainCamera->projectionMatrix(),
                vp
            );

            TextRenderer::instance().beginFrameForViewport(
                vp.width,
                vp.height
            );

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

        uiRoot->render(vp);



        TextRenderer::instance().endFrame();


















    }


    // // 3. векторные приборы
    glEnable(GL_DEPTH_TEST);

    renderUniverseTimeSimulationOverlay(vp);
    renderUiLanguageIndicator(vp);

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
        return;
    }

    if (target.mode != GameUiMode::SystemMap || !m_client)
        return;

    const auto panel = buildNativeSystemMapPanelPresentation();
    m_inSessionPresentationRenderer.renderSystemMapPanel(vp, loc, panel);
    renderUniverseTimeSimulationOverlay(vp);
    renderUiLanguageIndicator(vp);
}

void SpaceState::renderUiLanguageIndicator(const Viewport& viewport)
{
    if (!context().app)
        return;

    const auto& localization = context().app->localization();

    std::string label =
        "UI: " + localization.languageIndicator();

    if (m_constellationOverlayEnabled)
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
    perf["playerViewMs"] = m_perfPlayerViewMs;
    perf["uiRootUpdateMs"] = m_perfUiRootUpdateMs;

    perf["mainRenderMs"] = m_perfMainRenderMs;
    perf["rearCameraMs"] = m_perfRearCameraMs;
    perf["renderUiMs"] = m_perfRenderUiMs;
    perf["hudMs"] = m_perfHudMs;

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
