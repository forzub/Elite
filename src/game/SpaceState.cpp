#include <glad/gl.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <chrono>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <new>

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "SpaceState.h"
#include "core/StateStack.h"
#include "core/Log.h"
#include "input/Input.h"
#include <glm/gtx/norm.hpp>

#include "render/DebugGrid.h"

#include "src/game/player/ActorIdProvider.h"
#include "src/game/player/ActorCodeGenerator.h"

#include "ui/ConfirmExitState.h"

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



    std::string fmtMeters0(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(0) << v;
        return ss.str();
    }

    std::string fmtKm2(double meters)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << (meters / 1000.0);
        return ss.str();
    }

    glm::dvec3 worldPositionToMeters(
        const world::coordinates::WorldPosition& wp
    )
    {
        return glm::dvec3(
            static_cast<double>(wp.cell.x),
            static_cast<double>(wp.cell.y),
            static_cast<double>(wp.cell.z)
        ) * world::coordinates::GalacticCellSizeM + wp.localMeters;
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
SpaceState::SpaceState(StateStack& states)
    : GameState(states)
{

    initServer();
    initClient();
    loadDebugControlDefaults();

    InitShaders();

    std::cerr << "[HubMotionLab][startup] shaders-ready\n";

    // Важно: после InitShaders(), потому что starfield renderer использует
    // galaxy_starfield / galaxy_haze shader paths.
    try
    {
        m_sceneRenderer.initializeStaticResources();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[HubMotionLab][bad_alloc] phase=scene-renderer-initialize\n";
        throw;
    }
    std::cerr << "[HubMotionLab][startup] scene-renderer-ready\n";

    try
    {
        m_systemMapRenderer.init();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[HubMotionLab][bad_alloc] phase=system-map-renderer-init\n";
        throw;
    }
    std::cerr << "[HubMotionLab][startup] system-map-renderer-ready\n";

    try
    {
        requestGalaxyMapSnapshotOnce();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[HubMotionLab][bad_alloc] phase=galaxy-snapshot-request\n";
        throw;
    }
    std::cerr << "[HubMotionLab][startup] galaxy-request-ready\n";

    try
    {
        initHUD();
    }
    catch (const std::bad_alloc&)
    {
        std::cerr << "[HubMotionLab][bad_alloc] phase=hud-init\n";
        throw;
    }
    std::cerr << "[HubMotionLab][startup] hud-ready\n";


    /*
        The legacy political GalaxyDatabase is intentionally not loaded here.
        It is a separate, currently unused population/politics layer whose
        numeric system IDs do not match the physical StarAtlas IDs. Loading it
        only for startup counters created the false impression that it was part
        of the active server world. It can be reintroduced later behind an
        explicit server-owned world service and a stable cross-catalog key.
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


}


void SpaceState::requestGalaxyMapSnapshotOnce()
{
    if (!m_client)
        return;

    if (m_hasGalaxyMapSnapshot)
        return;

    if (!m_client->requestGalaxyMapSnapshot())
        return;

    const auto* snapshot =
        m_client->galaxyMapSnapshot();

    if (!snapshot)
        return;

    m_galaxyMapSnapshot = *snapshot;
    m_hasGalaxyMapSnapshot = true;
}





bool SpaceState::requestSystemMapSnapshot(
    int systemId,
    bool forceRefresh
)
{
    if (!m_client)
        return false;

    m_systemMapShowsEmptySector = false;

    const bool requestReady =
        m_client->requestSystemMapSnapshot(systemId, forceRefresh);

    const auto* snapshot =
        m_client->systemMapSnapshot(systemId);

    const auto& metadata =
        m_client->systemMapMetadata();

    const bool sameTarget =
        m_hasSystemMapSnapshot &&
        m_loadedSystemMapId == systemId;

    const bool newerSnapshot =
        snapshot != nullptr &&
        metadata.serverTick > m_appliedSystemMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return requestReady && sameTarget;

        m_systemMapSnapshot = *snapshot;
        m_loadedSystemMapId = systemId;
        m_hasSystemMapSnapshot = true;
        m_appliedSystemMapServerTick = metadata.serverTick;
    }

    return m_hasSystemMapSnapshot &&
           m_loadedSystemMapId == systemId;
}




bool SpaceState::requestDetailMapSnapshot(
    const world::celestial::DetailTarget& target,
    bool forceRefresh
)
{
    if (!m_client || !target.valid())
        return false;

    const bool requestReady =
        m_client->requestDetailMapSnapshot(target, forceRefresh);

    const auto* snapshot =
        m_client->detailMapSnapshot(target);

    const auto& metadata =
        m_client->detailMapMetadata();

    const bool sameTarget =
        m_hasDetailMapSnapshot &&
        m_loadedDetailTarget == target;

    const bool newerSnapshot =
        snapshot != nullptr &&
        metadata.serverTick > m_appliedDetailMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return requestReady && sameTarget;

        m_authoritativeMapInterpolator.acceptDetail(
            *snapshot,
            metadata.serverTimeSeconds,
            metadata.universeTimelineRevision
        );

        m_detailMapSnapshot =
            m_authoritativeMapInterpolator.detail();
        m_loadedDetailTarget = target;
        m_hasDetailMapSnapshot = m_detailMapSnapshot.valid;
        m_appliedDetailMapServerTick = metadata.serverTick;
    }

    return m_hasDetailMapSnapshot &&
           m_loadedDetailTarget == target;
}


bool SpaceState::requestHubMapSnapshot(
    int systemId,
    const std::string& hubId,
    bool forceRefresh
)
{
    if (!m_client || systemId < 0 || hubId.empty())
        return false;

    const bool requestReady =
        m_client->requestHubMapSnapshot(systemId, hubId, forceRefresh);

    const auto* snapshot =
        m_client->hubMapSnapshot(systemId, hubId);

    const auto& metadata =
        m_client->hubMapMetadata();

    const bool sameTarget =
        m_hasHubMapSnapshot &&
        m_loadedHubMapSystemId == systemId &&
        m_loadedHubMapHubId == hubId;

    const bool newerSnapshot =
        snapshot != nullptr &&
        metadata.serverTick > m_appliedHubMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return requestReady && sameTarget;

        m_authoritativeMapInterpolator.acceptHub(
            *snapshot,
            metadata.serverTimeSeconds,
            metadata.universeTimelineRevision
        );

        m_hubMapSnapshot =
            m_authoritativeMapInterpolator.hub();
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
            pushSystemMapPanelState();
        }
    );
}


void SpaceState::setSystemMapHubMode()
{
    if (!m_client)
        return;

    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Hub)
    {
        return;
    }

    const int selectedId =
        m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : m_client->playerNavigation().currentSystemId;

    const std::string hubId =
        m_systemMapRenderer.selectedHubId();

    if (hubId.empty())
        return;

    if (requestHubMapSnapshot(selectedId, hubId, true) &&
        game::client::MapTransitionController::simulationHasReached(
            m_client->hubMapMetadata(),
            m_client->lastSimulationMetadata()))
    {
        beginSystemMapHubTransition(selectedId, hubId);
        return;
    }

    m_mapTransitions.beginHub(selectedId, hubId);
}


void SpaceState::updatePendingMapTransition(float /*dt*/)
{
    if (!m_mapTransitions.pending() || !m_client)
        return;

    auto cancelTransition = [this](const char* message)
    {
        std::cerr << "[SystemMap] " << message << std::endl;
        m_mapTransitions.clear();
    };

    using Transition = game::client::MapTransitionController;

    switch (m_mapTransitions.kind())
    {
        case Transition::Kind::System:
        {
            if (Transition::requestFailed(
                    m_client->systemMapRequestStatus()))
            {
                cancelTransition("system map request failed");
                return;
            }

            const int systemId = m_mapTransitions.systemId();
            if (!requestSystemMapSnapshot(systemId, false))
                return;

            if (!Transition::simulationHasReached(
                    m_client->systemMapMetadata(),
                    m_client->lastSimulationMetadata()))
            {
                return;
            }

            m_mapTransitions.clear();
            beginSystemMapSystemTransition(systemId);
            return;
        }

        case Transition::Kind::Detail:
        {
            if (Transition::requestFailed(
                    m_client->detailMapRequestStatus()))
            {
                cancelTransition("detail map request failed");
                return;
            }

            const auto target = m_mapTransitions.detailTarget();
            if (!requestDetailMapSnapshot(target, false))
                return;

            if (!Transition::simulationHasReached(
                    m_client->detailMapMetadata(),
                    m_client->lastSimulationMetadata()))
            {
                return;
            }

            m_mapTransitions.clear();
            beginSystemMapDetailTransition(target);
            return;
        }

        case Transition::Kind::Hub:
        {
            if (Transition::requestFailed(
                    m_client->hubMapRequestStatus()))
            {
                cancelTransition("hub map request failed");
                return;
            }

            const int systemId = m_mapTransitions.systemId();
            const std::string hubId = m_mapTransitions.hubId();
            if (!requestHubMapSnapshot(systemId, hubId, false))
                return;

            if (!Transition::simulationHasReached(
                    m_client->hubMapMetadata(),
                    m_client->lastSimulationMetadata()))
            {
                return;
            }

            m_mapTransitions.clear();
            beginSystemMapHubTransition(systemId, hubId);
            return;
        }

        case Transition::Kind::None:
            return;
    }
}


void SpaceState::updateLiveMapSnapshots(float dt)
{
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
        {
            m_detailMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;

            if (!shouldRefreshSystemMapSnapshot())
            {
                m_systemMapLiveRefreshTimer = 0.0;
                return;
            }

            // First consume a completed response, if one arrived.
            requestSystemMapSnapshot(m_liveSystemMapId, false);

            m_systemMapLiveRefreshTimer += dt;
            if (m_systemMapLiveRefreshTimer >= refreshSeconds &&
                m_client->systemMapRequestStatus() !=
                    game::client::ClientRequestStatus::Pending)
            {
                m_systemMapLiveRefreshTimer = 0.0;
                requestSystemMapSnapshot(m_liveSystemMapId, true);
            }
            return;
        }

        case Mode::Detail:
        {
            m_systemMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;

            if (!m_loadedDetailTarget.valid())
            {
                m_detailMapLiveRefreshTimer = 0.0;
                return;
            }

            // Consume before scheduling the next refresh. This guarantees
            // that a completed response is copied into SpaceState exactly
            // once instead of being hidden by an immediate force-refresh.
            requestDetailMapSnapshot(m_loadedDetailTarget, false);

            m_detailMapLiveRefreshTimer += dt;
            if (m_detailMapLiveRefreshTimer >= refreshSeconds &&
                m_client->detailMapRequestStatus() !=
                    game::client::ClientRequestStatus::Pending)
            {
                m_detailMapLiveRefreshTimer = 0.0;
                requestDetailMapSnapshot(m_loadedDetailTarget, true);
            }
            return;
        }

        case Mode::Hub:
        {
            m_systemMapLiveRefreshTimer = 0.0;
            m_detailMapLiveRefreshTimer = 0.0;

            const int systemId =
                m_systemMapRenderer.focusedSystemId() >= 0
                    ? m_systemMapRenderer.focusedSystemId()
                    : m_client->playerNavigation().currentSystemId;

            if (systemId < 0 || m_loadedHubMapHubId.empty())
            {
                m_hubMapLiveRefreshTimer = 0.0;
                return;
            }

            requestHubMapSnapshot(
                systemId,
                m_loadedHubMapHubId,
                false
            );

            m_hubMapLiveRefreshTimer += dt;
            if (m_hubMapLiveRefreshTimer >= refreshSeconds &&
                m_client->hubMapRequestStatus() !=
                    game::client::ClientRequestStatus::Pending)
            {
                m_hubMapLiveRefreshTimer = 0.0;
                requestHubMapSnapshot(
                    systemId,
                    m_loadedHubMapHubId,
                    true
                );
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
        m_client)
    {
        m_systemMapRenderer.onGalaxyMapEntered(
            m_galaxyMapSnapshot,
            m_client->playerNavigation()
        );
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

            m_mapTransitions.clear();
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
}


// =====================================================================================
// Input
// =====================================================================================
void SpaceState::handleInput()
{
    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F12))
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

    if (context().app &&
        context().app->gameUiMode() == GameUiMode::SystemMap)
    {
        if (m_client)
        {
            const Viewport& fullVp = context().viewport();

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

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F1)){
        PlayerShipView::g_debugLogNextFrame = true;
        m_layout = ScreenLayout::Front_Main_Rear_Mini;
    }

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F2)){
        PlayerShipView::g_debugLogNextFrame = true;
        m_layout = ScreenLayout::Rear_Main_Front_Mini;
    }

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F3)){
        PlayerShipView::g_debugLogNextFrame = true;
        m_layout = ScreenLayout::Front_Main_Drone_Mini;
    }

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F4)){
        PlayerShipView::g_debugLogNextFrame = true;
        m_layout = ScreenLayout::Drone_Main_Front_Mini;
    }



    // дебажная обработка F8
if (Input::instance().isKeyPressedOnce(GLFW_KEY_F8)) {
    pushAttachmentEditorState();
}

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
    m_inputMapper.update(m_playerControl);
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


// =====================================================================================
// Update
// =====================================================================================
void SpaceState::update(float dt)
{
    const double updateStartMs = nowMs();
    m_perfFrameIndex++;

    const double htmlStartMs = nowMs();
    processHtmlCommands();
    m_perfProcessHtmlMs = nowMs() - htmlStartMs;



    const double simStartMs = nowMs();

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

    m_perfFixedSimMs = nowMs() - simStartMs;
    m_perfServerFixedSteps = serverAdvance.stepsExecuted;
    m_perfServerTickDebtMs =
        serverAdvance.remainingDebtSeconds * 1000.0;
    m_perfServerDiscardedMs =
        serverAdvance.discardedSeconds * 1000.0;
    m_perfServerTotalDiscardedMs =
        serverAdvance.totalDiscardedSeconds * 1000.0;
    m_perfServerCatchUpLimited =
        serverAdvance.catchUpLimited;

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

    updatePendingMapTransition(clientFrameDt);

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


    // DEBUG: отправляем состояние корабля в браузер
    // --------------------------------------------
    static float shipCoreTimer = 0.0f;
    shipCoreTimer += dt;

    if (shipCoreTimer > 0.25f)
    {
        shipCoreTimer = 0.0f;

        if (context().htmlUi().state().activePanel == HtmlUiPanelId::ShipCore)
        {
            pushShipCoreState();
        }
    }




    static float structureDebugTimer = 0.0f;
    structureDebugTimer += dt;
    if (structureDebugTimer > 0.25f)
    {
        structureDebugTimer = 0.0f;

        if (context().htmlUi().state().activePanel == HtmlUiPanelId::StructureDebug)
        {
            pushStructureDebugState();
        }
    }



    m_perfFrameMs = static_cast<double>(dt) * 1000.0;

    if (dt > 0.00001f)
        m_perfFps = 1.0 / static_cast<double>(dt);

    m_perfPushTimer += dt;

    if (m_perfPushTimer >= 0.25f)
    {
        m_perfPushTimer = 0.0f;

        if (context().htmlUi().state().activePanel == HtmlUiPanelId::DebugControl &&
            debug::get().render.debugControlAutoUpdates)
        {
            pushDebugControlState();
        }

        if (context().app &&
            context().app->gameUiMode() == GameUiMode::SystemMap)
        {
            pushSystemMapPanelState();
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
            context().app->gameUiMode() == GameUiMode::SystemMap)
        {
            /*
                В режиме карты обычные игровые камеры не рендерятся.

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



    switch (m_layout)
    {
        case ScreenLayout::Front_Main_Rear_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            miniCam = &m_playerView->camera(ShipCameraMode::Rear);
            m_activeCameraMode = ShipCameraMode::Cockpit;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "FRONT";}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "REAR";}
            }
            break;

        case ScreenLayout::Rear_Main_Front_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Rear);
            miniCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            m_activeCameraMode = ShipCameraMode::Rear;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "REAR";}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "FRONT";}
            }
            break;

        case ScreenLayout::Front_Main_Drone_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            miniCam = &m_playerView->camera(ShipCameraMode::Drone);
            m_activeCameraMode = ShipCameraMode::Cockpit;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "FRONT";}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "DRONE";}
            }
            break;

        case ScreenLayout::Drone_Main_Front_Mini:
            mainCam = &m_playerView->camera(ShipCameraMode::Drone);
            miniCam = &m_playerView->camera(ShipCameraMode::Cockpit);
            m_activeCameraMode = ShipCameraMode::Drone;
            if (auto* comp = uiRoot->findById("main_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "DRONE";}
            }
            if (auto* comp = uiRoot->findById("rear_label"))
            {
                if (auto* text = dynamic_cast<UIText*>(comp)){text->label = "FRONT";}
            }
            break;
    }

    m_activeMainCamera = mainCam;


    // -------------------------------------------------
    // HUD telemetry: player global coordinates + speed
    // -------------------------------------------------
    {
        auto setText =
            [&](const std::string& id, const std::string& value)
            {
                if (auto* comp = uiRoot->findById(id))
                {
                    if (auto* text = dynamic_cast<UIText*>(comp))
                        text->label = value;
                }
            };

        const auto& ships = m_client->world().ships();
        auto it = ships.find(m_playerId.value);

        if (it != ships.end())
        {
            const auto& ship = it->second;
            const auto& wp = ship.renderTransform.worldPosition;

            const glm::dvec3 globalMeters =
                glm::dvec3(
                    static_cast<double>(wp.cell.x),
                    static_cast<double>(wp.cell.y),
                    static_cast<double>(wp.cell.z)
                ) * world::coordinates::GalacticCellSizeM +
                wp.localMeters;

            double speedMps =
                glm::length(glm::dvec3(ship.renderTransform.localVelocity));

            if (std::abs(static_cast<double>(ship.renderTransform.forwardVelocity)) > speedMps)
                speedMps = std::abs(static_cast<double>(ship.renderTransform.forwardVelocity));

            char buf[128];

            std::snprintf(
                buf,
                sizeof(buf),
                "CELL %lld %lld %lld",
                static_cast<long long>(wp.cell.x),
                static_cast<long long>(wp.cell.y),
                static_cast<long long>(wp.cell.z)
            );

            setText("main_coord_cell", buf);


            std::snprintf(buf, sizeof(buf), "X %.0f m", globalMeters.x);
            setText("main_coord_x", buf);

            std::snprintf(buf, sizeof(buf), "Y %.0f m", globalMeters.y);
            setText("main_coord_y", buf);

            std::snprintf(buf, sizeof(buf), "Z %.0f m", globalMeters.z);
            setText("main_coord_z", buf);


            std::snprintf(buf, sizeof(buf), "V %.1f m/s", speedMps);
            setText("main_coord_v", buf);

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
    mainPolicy.drawObjects = dbg.renderHubs || dbg.renderLargeObjects;
    mainPolicy.drawVisualShips = dbg.renderVisualShips;
    mainPolicy.drawVisualDrones = dbg.renderVisualShips;




    m_preparedScene =
        m_sceneRenderer.prepareScene(
            m_client->world(),
            m_playerId
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
        rearView->renderToTexture(vp, rearView->drawCallback);

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
        context().app->gameUiMode() == GameUiMode::SystemMap &&
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

        renderUniverseTimeSimulationOverlay(vp);

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
            }
        }

        if (debug::get().render.shouldRenderCockpit())
        {
            m_playerView->renderCockpit();
        }

        TextRenderer::instance().beginFrameForViewport(
            vp.width,
            vp.height
        );

        uiRoot->render(vp);



        TextRenderer::instance().endFrame();











        // -------------------------------------------------
// DEBUG: координатная таблица ключевых объектов
// -------------------------------------------------
if (m_client)
{
    const auto& ships = m_client->world().ships();
    auto playerIt = ships.find(m_playerId.value);

    if (playerIt != ships.end())
    {
        const auto& playerShip = playerIt->second;

        const glm::dvec3 playerM =
            worldPositionToMeters(
                playerShip.renderTransform.worldPosition
            );

        const auto* systemSnapshot =
            m_client->systemMapSnapshot(
                m_client->playerNavigation().currentSystemId
            );

        bool haveEarth = false;
        bool haveMoon = false;
        bool haveHub = false;

        glm::dvec3 earthM {0.0};
        glm::dvec3 moonM {0.0};
        glm::dvec3 hubM {0.0};

        if (systemSnapshot)
        {
            for (const auto& b : systemSnapshot->bodies)
            {
                const glm::dvec3 bodyM =
                    b.positionAu * world::celestial::MetersPerAu;

                if (b.id == "system_0.Sol.Земля")
                {
                    earthM = bodyM;
                    haveEarth = true;
                }

                if (b.name == "Луна" ||
                    b.id.find("Луна") != std::string::npos)
                {
                    moonM = bodyM;
                    haveMoon = true;
                }
            }

            for (const auto& obj : systemSnapshot->objects)
            {
                if (obj.name.find("Earth High Orbital") != std::string::npos)
                {
                    hubM =
                        obj.positionAu * world::celestial::MetersPerAu;

                    haveHub = true;
                    break;
                }
            }
        }

        const double distPlayerEarthM =
            haveEarth ? glm::length(playerM - earthM) : -1.0;

        const double distPlayerMoonM =
            haveMoon ? glm::length(playerM - moonM) : -1.0;

        const double distPlayerHubM =
            haveHub ? glm::length(playerM - hubM) : -1.0;



        // -------------------------------------------------
        // SCREEN OVERLAY: то же самое на HUD
        // -------------------------------------------------
        auto& text = TextRenderer::instance();

        text.beginFrameForViewport(
            vp.width,
            vp.height
        );

        float x = 24.0f;
        float y = 230.0f;
        const float line = 15.0f;

        auto drawLine =
            [&](const std::string& s, const glm::vec4& color)
            {
                text.textDrawPx(
                    s,
                    x,
                    y,
                    11,
                    color
                );

                y += line;
            };

        const glm::vec4 headerColor {1.0f, 0.78f, 0.35f, 0.95f};
        const glm::vec4 normalColor {0.45f, 0.82f, 1.0f, 0.78f};
        const glm::vec4 warnColor   {1.0f, 0.38f, 0.28f, 0.90f};

        drawLine("COORD DEBUG", headerColor);

        drawLine(
            "PLAYER X " + fmtMeters0(playerM.x) +
            " Y " + fmtMeters0(playerM.y) +
            " Z " + fmtMeters0(playerM.z),
            normalColor
        );

        if (haveEarth)
        {
            drawLine(
                "DIST PLAYER-EARTH " +
                fmtKm2(distPlayerEarthM) +
                " km",
                distPlayerEarthM < 6371000.0 ? warnColor : normalColor
            );
        }
        else
        {
            drawLine("EARTH NOT FOUND", warnColor);
        }

        if (haveMoon)
        {
            drawLine(
                "DIST PLAYER-MOON " +
                fmtKm2(distPlayerMoonM) +
                " km",
                normalColor
            );
        }
        else
        {
            drawLine("MOON NOT FOUND", warnColor);
        }

        if (haveHub)
        {
            drawLine(
                "DIST PLAYER-HUB " +
                fmtKm2(distPlayerHubM) +
                " km",
                distPlayerHubM > 50000.0 ? warnColor : normalColor
            );
        }
        else
        {
            drawLine("HUB NOT FOUND", warnColor);
        }

        text.endFrame();
    }
}








    }


    // // 3. векторные приборы
    glEnable(GL_DEPTH_TEST);

    renderUniverseTimeSimulationOverlay(vp);

    m_perfHudMs = nowMs() - hudStartMs;
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
    label
        << "TIME SIMULATION MODE  x"
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



// =====================================================================================
// wantsConfirmExit
// =====================================================================================

bool SpaceState::wantsConfirmExit() const
{

    return true; // есть активная игровая сессия
}


// =====================================================================================
// onGlobalEscape
// =====================================================================================

bool SpaceState::onGlobalEscape()
{



    ConfirmExitOptions opts;
    opts.canSave = isInSafeZone();
    opts.canLoad = isInSafeZone();

    m_states.push(std::make_unique<ConfirmExitState>(m_states, opts));
    return true;
}


// =====================================================================================
// isInSafeZone
// =====================================================================================

bool SpaceState::isInSafeZone() const
{
    // ВРЕМЕННО:
    // пока считаем, что игрок ВСЕГДА в безопасной зоне
    return true;
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
                        app->closeGameUi();
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
                context().htmlUi().setActivePanel(HtmlUiPanelId::DebugControl);
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
                    pushDebugControlState();
                    continue;
                }

                if (msg.command == "save_defaults")
                {
                    applyDebugControlPayload(msg.payload);
                    saveDebugControlDefaults(msg.payload);
                    ++m_debugControlSettingsRevision;
                    pushDebugControlState();
                    continue;
                }

                if (msg.command == "reset_settings")
                {
                    resetDebugControlSettings();
                    ++m_debugControlSettingsRevision;
                    pushDebugControlState();
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
                context().htmlUi().setActivePanel(HtmlUiPanelId::StructureDebug);
                pushStructureDebugState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    context().htmlUi().setActivePanel(HtmlUiPanelId::StructureDebug);
                    pushStructureDebugState();
                    continue;
                }

                if (msg.command == "select_ship_entity")
                {
                    m_structureDebugSelectedShipEntityId =
                        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

                    context().htmlUi().setActivePanel(HtmlUiPanelId::StructureDebug);
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
        m_debugSession->refreshSnapshot();
    }

    pushStructureDebugState();
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
        m_debugSession->refreshSnapshot();
    }

    pushStructureDebugState();
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
        m_debugSession->refreshSnapshot();
    }

    pushStructureDebugState();
    continue;
}



if (msg.command == "reevaluate_structure")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_debugSession->reevaluateShipStructure(id);
    m_debugSession->refreshSnapshot();

    pushStructureDebugState();
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
        m_debugSession->refreshSnapshot();
    }

    pushStructureDebugState();
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
                            m_debugSession->refreshSnapshot();
                        }

                        pushStructureDebugState();
                        continue;
                    }

if (msg.command == "reset_ship")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_debugSession->resetShipStructure(id);
    m_debugSession->refreshSnapshot();

    pushStructureDebugState();
    continue;
}

if (msg.command == "reset_all_ships")
{
    m_debugSession->resetAllShipStructures();
    m_debugSession->refreshSnapshot();

    pushStructureDebugState();
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
                context().htmlUi().setActivePanel(HtmlUiPanelId::ShipCore);
                pushShipCoreState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    context().htmlUi().setActivePanel(HtmlUiPanelId::ShipCore);
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
            }
        }

        // ------------------------------
        // FRUSTUM DEBUG
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::FrustumDebug)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                context().htmlUi().setActivePanel(HtmlUiPanelId::FrustumDebug);
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command &&
                msg.command == "request_snapshot")
            {
                context().htmlUi().setActivePanel(HtmlUiPanelId::FrustumDebug);
                continue;
            }
        }
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

    m_debugSession->refreshSnapshot();

    const auto& snapshot = m_debugSession->snapshot();

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
        for (const auto& mod : ship.graph.modules)
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
    context().htmlUi().setActivePanel(HtmlUiPanelId::FrustumDebug);
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
        These are debug settings, so publish them from the authoritative
        debug session itself. The client session snapshot is telemetry and
        may still be one network publication behind immediately after Apply.
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

auto jurisdictionForSystem =
    [](int systemId) -> std::string
{
    if (systemId == 0)
        return "Sol Authority";

    if (systemId >= 1 && systemId <= 9)
        return "Core Jurisdiction";

    if (systemId >= 10 && systemId <= 29)
        return "Colonial Administration";

    if (systemId >= 30 && systemId <= 44)
        return "Frontier / Independent";

    return "Unregistered";
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
    item["jurisdiction"] = jurisdictionForSystem(s.id);

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

    requestGalaxyMapSnapshotOnce();

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

        pushSystemMapPanelState();
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

            pushSystemMapPanelState();
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

            pushSystemMapPanelState();
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
            pushSystemMapPanelState();
        }
    );
}


void SpaceState::setSystemMapKnownSystemMode(
    int systemId
)
{
    if (!m_client || systemId < 0)
        return;

    requestGalaxyMapSnapshotOnce();

    if (requestSystemMapSnapshot(systemId, true) &&
        game::client::MapTransitionController::simulationHasReached(
            m_client->systemMapMetadata(),
            m_client->lastSimulationMetadata()))
    {
        beginSystemMapSystemTransition(systemId);
        return;
    }

    m_mapTransitions.beginSystem(systemId);
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









void SpaceState::setSystemMapDetailMode()
{
    using namespace world::celestial;

    if (!m_client)
        return;

    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Detail)
    {
        return;
    }

    const int selectedId =
        m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : m_client->playerNavigation().currentSystemId;

    const std::string selectedBodyId =
        m_systemMapRenderer.selectedBodyId();
    const std::string selectedHubId =
        m_systemMapRenderer.selectedHubId();
    const std::string selectedHubParentBodyId =
        m_systemMapRenderer
            .selectedHubParentBodyId();

    DetailTarget target;
    target.systemId = selectedId;
    target.systemPositionLy =
        m_systemMapSnapshot.systemPositionLy;

    if (!selectedBodyId.empty())
    {
        target.sceneKind =
            DetailSceneKind::CelestialBody;
        target.focusClass =
            DetailObjectClass::CelestialBody;
        target.anchorId = selectedBodyId;
        target.focusId = selectedBodyId;
    }
    else if (!selectedHubId.empty() &&
        !selectedHubParentBodyId.empty())
    {
        /*
            An orbital hub is infrastructure, but Details keeps the parent
            body as the scene anchor so the existing planet/hub workflow and
            the HUB button remain intact.
        */
        target.sceneKind =
            DetailSceneKind::CelestialBody;
        target.focusClass =
            DetailObjectClass::Hub;
        target.anchorId =
            selectedHubParentBodyId;
        target.focusId = selectedHubId;
    }
    else if (!selectedHubId.empty())
    {
        target.sceneKind =
            DetailSceneKind::LocalObject;
        target.focusClass =
            DetailObjectClass::Hub;
        target.anchorId = selectedHubId;
        target.focusId = selectedHubId;
    }
    else
    {
        const auto selectedCell =
            m_systemMapRenderer
                .selectedTerminalDetailCell();

        if (!selectedCell)
            return;

        target.sceneKind =
            DetailSceneKind::SpatialVolume;
        target.focusClass =
            DetailObjectClass::None;
        target.spatialCell = *selectedCell;
    }

    if (!target.valid())
        return;

    if (requestDetailMapSnapshot(target, true) &&
        game::client::MapTransitionController::simulationHasReached(
            m_client->detailMapMetadata(),
            m_client->lastSimulationMetadata()))
    {
        beginSystemMapDetailTransition(target);
        return;
    }

    m_mapTransitions.beginDetail(target);
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
            pushSystemMapPanelState();
        }
    );
}




void SpaceState::setSystemMapLoadedDetailMode()
{
    if (!m_client)
        return;

    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Detail)
    {
        return;
    }

    /*
        Hub -> Detail returns to the semantic Detail target that opened this
        Hub. A timeline-revision fence invalidates the cached snapshot bytes,
        but deliberately preserves m_loadedDetailTarget. If the cache is gone,
        reacquire the same target on the active revision instead of disabling
        navigation.
    */
    const world::celestial::DetailTarget target =
        m_loadedDetailTarget;

    if (!target.valid())
        return;

    if (m_hasDetailMapSnapshot)
    {
        beginSystemMapDetailTransition(target);
        return;
    }

    if (requestDetailMapSnapshot(target, true) &&
        game::client::MapTransitionController::simulationHasReached(
            m_client->detailMapMetadata(),
            m_client->lastSimulationMetadata()))
    {
        beginSystemMapDetailTransition(target);
        return;
    }

    m_mapTransitions.beginDetail(target);
}








void SpaceState::pushSystemMapPanelState()
{
    requestGalaxyMapSnapshotOnce();

    if (!m_client || !context().app)
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

    if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy)
    {
        payload["mode"] = "Galaxy";
    }
    else if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Detail)
    {
        payload["mode"] = "Detail";
    }
    else if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Hub)
    {

        payload["mode"] = "Hub";
    }
    else
    {
        payload["mode"] = "System";
    }

    payload["systemsCount"] = m_galaxyMapSnapshot.systems.size();
    payload["currentSystemId"] = nav.currentSystemId;
    payload["currentSystemName"] = celestial.systemName;

    int selectedId = -1;

    if (!m_systemMapShowsEmptySector)
    {
        selectedId =
            m_systemMapRenderer.selectedSystemId() >= 0
                ? m_systemMapRenderer.selectedSystemId()
                : nav.currentSystemId;
    }

    payload["selectedSystemId"] = selectedId;
    payload["selectedEmptySector"] =
        m_systemMapShowsEmptySector;

    if (m_systemMapShowsEmptySector)
    {
        payload["selectedEmptySectorPositionLy"] = {
            {"x", m_systemMapSnapshot.systemPositionLy.x},
            {"y", m_systemMapSnapshot.systemPositionLy.y},
            {"z", m_systemMapSnapshot.systemPositionLy.z}
        };
    }

    payload["systems"] = json::array();
    payload["selectedBodyId"] =
        m_systemMapRenderer.selectedBodyId();
    payload["selectedHubId"] =
        m_systemMapRenderer.selectedHubId();

    payload["canOpenDetail"] =
        m_systemMapRenderer.mode() ==
            SystemMapRenderer::Mode::System &&
        m_systemMapRenderer.canOpenSelectedDetail();

    if (const auto selectedCell =
            m_systemMapRenderer
                .selectedTerminalDetailCell())
    {
        payload["selectedDetailCell"] = {
            {"level", selectedCell->level},
            {"maximumLevel", selectedCell->maximumLevel},
            {"x", selectedCell->x},
            {"y", selectedCell->y},
            {"z", selectedCell->z},
            {"edgeAu", selectedCell->edgeAu}
        };
    }

    payload["canOpenHub"] =
        m_systemMapRenderer.mode() ==
            SystemMapRenderer::Mode::Detail &&
        !m_systemMapRenderer.selectedHubId().empty();



const world::celestial::GalaxyMapSystem* currentSystem = nullptr;

for (const auto& s : m_galaxyMapSnapshot.systems)
{
    if (s.id == nav.currentSystemId)
    {
        currentSystem = &s;
        break;
    }
}

// fallback: если currentSystemId не найден — считаем от Sol
if (!currentSystem)
{
    for (const auto& s : m_galaxyMapSnapshot.systems)
    {
        if (s.id == 0)
        {
            currentSystem = &s;
            break;
        }
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

auto jurisdictionForSystem =
    [](int systemId) -> std::string
{
    if (systemId == 0)
        return "Sol Authority";

    if (systemId >= 1 && systemId <= 9)
        return "Core Jurisdiction";

    if (systemId >= 10 && systemId <= 29)
        return "Colonial Administration";

    if (systemId >= 30 && systemId <= 44)
        return "Frontier / Independent";

    return "Unregistered";
};

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
    item["jurisdiction"] = jurisdictionForSystem(s.id);

    payload["systems"].push_back(std::move(item));
}

    context().app->evalGameUiScript(
        "if (window.setSystemMapPanel) window.setSystemMapPanel(" +
        payload.dump() +
        ");"
    );
}
