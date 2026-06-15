#include <glad/gl.h>
#include <iostream>
#include <cstdio>
#include <cmath>

#include <chrono>
#include <algorithm>
#include <functional>

#include <fstream>
#include <iomanip>
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
#include "src/galaxy/GalaxyDatabase.h"

#include "src/render/camera/RenderCameraViewport.h"
#include "src/debug/DebugSettings.h"
#include "ui/components/UIText.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "ui/components/radar/RadarWidgetBase.h"
#include "src/game/network/ClientMessage.h"

#include "src/render/InitShaders.h"

#include "src/game/ship/view/PlayerShipView.h"

#include "game/debug/AttachmentEditorPayload.h"
#include "game/debug/VolumeViewerPayload.h"
#include "ui/html/HtmlUiManager.h"
#include "ui/html/HtmlUiPanelId.h"
#include "ui/html/HtmlUiMessage.h"
#include <mutex>

#include "src/core/Application.h"

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


    json vec4ToJson(const glm::vec4& v)
    {
        return json::array({ v.x, v.y, v.z, v.w });
    }

    glm::vec4 jsonToVec4(const json& j, const glm::vec4& fallback)
    {
        if (!j.is_array() || j.size() < 4)
            return fallback;

        return glm::vec4(
            j[0].get<float>(),
            j[1].get<float>(),
            j[2].get<float>(),
            j[3].get<float>()
        );
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


    glm::mat4 makePromoPitchOnlyOrientationClient(
        const glm::vec3& toTarget
    )
    {
        // Promo rule: player ship may only pitch while tracking the wing.
        // X/right axis is fixed, so no yaw/roll can leak from look-at/slerp.
        glm::vec3 f = glm::vec3(0.0f, toTarget.y, toTarget.z);

        if (glm::length2(f) < 0.000001f)
            f = glm::vec3(0.0f, 0.0f, -1.0f);

        f = glm::normalize(f);

        const glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 up = glm::normalize(glm::cross(right, f));

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

    InitShaders();

    // Важно: после InitShaders(), потому что starfield renderer использует
    // galaxy_starfield / galaxy_haze shader paths.
    m_sceneRenderer.initializeStaticResources();
    m_systemMapRenderer.init();

    requestGalaxyMapSnapshotOnce();

    initHUD();

    
    // =======================================================================
    // load galaxy database + test
    // =======================================================================
    
    GalaxyDatabase galaxy;
    galaxy.loadFromDirectory("assets/data/galaxy");
    galaxy.validate();
    
    std::cerr
    << "Actors:  " << galaxy.actorCount()  << "\n"
    << "Systems: " << galaxy.systemCount() << "\n"
    << "Nodes:   " << galaxy.nodeCount()   << "\n"
    << "Routes:  " << galaxy.routeCount()  << "\n";
    
    

    // =======================================================================
    // world = vacuum
    // =======================================================================

    m_server->world().linearDrag = 0.0f;
    m_server->world().maxSafeDecel = 50.0f;


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
    if (!m_server)
        return;

    if (m_hasGalaxyMapSnapshot)
        return;

    m_galaxyMapSnapshot =
        m_server->buildGalaxyMapSnapshot();

    m_hasGalaxyMapSnapshot = true;
}





void SpaceState::requestSystemMapSnapshot(
    int systemId,
    bool forceRefresh
)
{
    if (!m_server)
        return;

    if (!forceRefresh &&
        m_hasSystemMapSnapshot &&
        m_loadedSystemMapId == systemId)
    {
        return;
    }

    m_systemMapSnapshot =
        m_server->buildSystemMapSnapshot(systemId);

    m_loadedSystemMapId = systemId;
    m_hasSystemMapSnapshot = true;
}









void SpaceState::updateSystemMapLiveFlags()
{
    m_systemMapVisible =
        context().app &&
        context().app->gameUiMode() == GameUiMode::SystemMap;

    m_systemMapLiveSnapshotsEnabled =
        m_systemMapVisible &&
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System;

    if (!m_systemMapLiveSnapshotsEnabled)
    {
        m_liveSystemMapId = -1;
        m_systemMapLiveRefreshTimer = 0.0;
        return;
    }

    if (m_systemMapRenderer.focusedSystemId() >= 0)
    {
        m_liveSystemMapId =
            m_systemMapRenderer.focusedSystemId();
    }
    else if (m_server)
    {
        m_liveSystemMapId =
            m_server->playerNavigation().currentSystemId;
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
        m_server != nullptr;
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
// Input
// =====================================================================================
void SpaceState::handleInput()
{
    if (context().app &&
        context().app->gameUiMode() == GameUiMode::SystemMap)
    {
        if (m_server)
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

            m_systemMapRenderer.handleInput(
                mapVp,
                m_galaxyMapSnapshot
            );
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
    const bool ok = m_server->ejectShipCockpitCapsule(m_playerId);

    m_server->debugRefreshSnapshot();

    return;
}



const bool ctrlDown =
    Input::instance().isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
    Input::instance().isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

if (ctrlDown && Input::instance().isKeyPressedOnce(GLFW_KEY_R))
{
    const bool ok = m_server->startBestRepairJobForFirstMissingSlot(
        m_playerId
    );

    m_server->debugRefreshSnapshot();



    return;
}

    // === управление кораблём ===
    m_inputMapper.update(m_playerControl);
m_localTick++;
m_playerControl.controlTick = m_localTick;

m_sentInputs.push_back(m_playerControl);

m_server->submitCommand(m_playerId, m_playerControl);

// ВРЕМЕННО отключаем, чтобы не было двойной системы ввода.
// Если клиентский loopback тоже шлёт команду на сервер,
// он может перетирать/дублировать состояние.
 // m_client->submitInput(m_playerControl);


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

    const float rawFrameDt = dt;
    int debugFixedSteps = 0;

    const double htmlStartMs = nowMs();
    processHtmlCommands();
    m_perfProcessHtmlMs = nowMs() - htmlStartMs;



    const double simStartMs = nowMs();

// Не даём стартовому лагу/фризу накопить огромный долг симуляции.
// Иначе первые секунды сервер будет "догонять время",
// делая по 2 fixed-step за кадр, и станция визуально крутится быстрее.
const float clampedFrameDt = std::min(dt, 0.05f);

m_simAccumulator += clampedFrameDt;

constexpr int MAX_SIM_STEPS_PER_FRAME = 1;

int steps = 0;

while (m_simAccumulator >= SIM_FIXED_DT && steps < MAX_SIM_STEPS_PER_FRAME)
{
    m_transport->update(SIM_FIXED_DT);
    m_server->update(SIM_FIXED_DT);
    m_transport->update(0.0f);

    m_simAccumulator -= SIM_FIXED_DT;
    steps++;
    debugFixedSteps++;
}

// Если симуляция не успела догнать — не тащим долг дальше.
// Лучше потерять кусок времени, чем первые секунды жить в ускоренной вселенной.
if (m_simAccumulator >= SIM_FIXED_DT)
{
    m_simAccumulator = 0.0;
}

    m_perfFixedSimMs = nowMs() - simStartMs;

        const double clientStartMs = nowMs();

    m_client->update(
        clampedFrameDt,
        m_server->world(),
        SIM_FIXED_DT
    );

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



        m_stationTrafficSystem.setup(m_client->world());
        m_stationTrafficSystem.update(
            m_client->world(),
            dt
        );
    }

    m_perfClientUpdateMs = nowMs() - clientStartMs;


// Сначала двигаем visual ships и поворачиваем renderTransform игрока.
// Только потом PlayerShipView получает уже финальный transform кадра.
// Promo tracking отключён для нормального старта возле станции.
// Иначе клиент насильно возвращает игрока в старую promo-позицию.
if (isPromo1SceneMode() && game::scene::GameSceneSetupConfig::PromoScene)
{
    updatePromoPlayerShipTracking(static_cast<float>(dt));
}

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
                    msg.type = game::network::ClientMessageType::ClientShipCommand;
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
                    msg.type = game::network::ClientMessageType::ClientShipCommand;
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
    if (m_radarWidget)
    {
        m_radarWidget->setPlayerTransform(
            world::coordinates::legacyFloatMeters(
                ship.renderTransform.worldPosition
            ),
            ship.renderTransform.orientation
        );

        std::vector<RadarContactView> views;

        for (const auto& c : ship.radarContacts)
        {
            views.push_back({
                c.id,
                c.localPosition
            });
        }

        m_radarWidget->setContacts(views);
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



updateSystemMapLiveFlags();

if (shouldRefreshSystemMapSnapshot())
{
    m_systemMapLiveRefreshTimer += dt;

    const double refreshSec =
        static_cast<double>(debug::get().render.systemMapLiveRefreshSec);

    if (m_systemMapLiveRefreshTimer >= refreshSec)
    {
        m_systemMapLiveRefreshTimer = 0.0;

        requestSystemMapSnapshot(
            m_liveSystemMapId,
            true
        );
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
        // System map is a full-screen tactical/navigation mode.
        // Do not render cockpit/world cameras under it.
        m_perfRenderUiMs = nowMs() - renderUiStartMs;
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
        m_server)
    {
        glDisable(GL_DEPTH_TEST);

        Viewport mapVp = vp;

        const float panelRatio = 0.28f;
        mapVp.x = vp.x;
        mapVp.y = vp.y;
        mapVp.width = static_cast<int>(static_cast<float>(vp.width) * (1.0f - panelRatio));
        mapVp.height = vp.height;

        m_systemMapRenderer.setRightPanelRatio(panelRatio);

        






if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System)
{
    const int focusedId =
        m_systemMapRenderer.focusedSystemId() >= 0
            ? m_systemMapRenderer.focusedSystemId()
            : m_server->playerNavigation().currentSystemId;

    requestSystemMapSnapshot(
        focusedId,
        false
    );
}





        m_systemMapRenderer.render(
            mapVp,
            m_galaxyMapSnapshot,
            m_systemMapSnapshot,
            m_server->playerNavigation()
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
if (m_server)
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

        const auto systemSnapshot =
            m_server->buildSystemMapSnapshot(
                m_server->playerNavigation().currentSystemId
            );

        bool haveEarth = false;
        bool haveMoon = false;
        bool haveHub = false;

        glm::dvec3 earthM {0.0};
        glm::dvec3 moonM {0.0};
        glm::dvec3 hubM {0.0};

        for (const auto& b : systemSnapshot.bodies)
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

        for (const auto& obj : systemSnapshot.objects)
        {
            if (obj.name.find("Earth High Orbital") != std::string::npos)
            {
                hubM =
                    obj.positionAu * world::celestial::MetersPerAu;

                haveHub = true;
                break;
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

    
   
    m_perfHudMs = nowMs() - hudStartMs;
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

        m_server->debugDestroyShipModule(id, moduleId);
        m_server->debugRefreshSnapshot();
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

        m_server->debugDetachShipModule(id, moduleId);
        m_server->debugRefreshSnapshot();
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

        m_server->debugHangShipModule(id, moduleId);
        m_server->debugRefreshSnapshot();
    }

    pushStructureDebugState();
    continue;
}



if (msg.command == "reevaluate_structure")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_server->debugReevaluateShipStructure(id);
    m_server->debugRefreshSnapshot();

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

        m_server->debugRestoreShipModule(id, moduleId);
        m_server->debugRefreshSnapshot();
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
                            m_server->debugSetShipStructuralLinkHealth(
                                id,
                                linkId,
                                health,
                                destroyed
                            );
                            m_server->debugRefreshSnapshot();
                        }

                        pushStructureDebugState();
                        continue;
                    }

if (msg.command == "reset_ship")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_server->debugResetShipStructure(id);
    m_server->debugRefreshSnapshot();

    pushStructureDebugState();
    continue;
}

if (msg.command == "reset_all_ships")
{
    m_server->debugResetAllShipStructures();
    m_server->debugRefreshSnapshot();

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

    m_server->debugRefreshSnapshot();

    const auto& snapshot = m_server->snapshot();

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

    json payload;
    payload["drawMeshes"] = dbg.drawMeshes;
    payload["renderCockpit"] = dbg.renderCockpit;
    payload["renderShipUi"] = dbg.renderShipUi;
    payload["renderStarfield"] = dbg.renderStarfield;
    payload["showStarLabels"] = dbg.showStarLabels;
    payload["showAllStarLabels"] = dbg.showAllStarLabels;
    payload["sceneMode"] = dbg.sceneMode;
    payload["showConstellationHover"] = dbg.showConstellationHover;
    payload["constellationHoverRadiusPx"] = dbg.constellationHoverRadiusPx;
    payload["renderRearCamera"] = dbg.renderRearCamera;

    payload["renderPlayerShip"] = dbg.renderPlayerShip;
    payload["renderNpcShips"] = dbg.renderNpcShips;
    payload["renderTrafficShips"] = dbg.renderTrafficShips;
    payload["renderRealShips"] = dbg.renderRealShips;
    payload["renderVisualShips"] = dbg.renderVisualShips;
    payload["renderHubs"] = dbg.renderHubs;
    payload["renderLargeObjects"] = dbg.renderLargeObjects;
    payload["renderCelestialBodies"] = dbg.renderCelestialBodies;
    payload["renderSystemMapObjects"] = dbg.renderSystemMapObjects;
    payload["debugControlAutoUpdates"] = dbg.debugControlAutoUpdates;
    payload["systemMapLiveRefreshSec"] = dbg.systemMapLiveRefreshSec;
    
    payload["drawAxes"] = dbg.drawAxes;
    payload["drawWorldAxes"] = dbg.drawWorldAxes;
    payload["drawObjectAxes"] = dbg.drawObjectAxes;

    payload["drawModulePivots"] = dbg.drawModulePivots;
    payload["drawHitVolumes"] = dbg.drawHitVolumes;
    payload["hitVolumeFilterMode"] = dbg.hitVolumeFilterMode;
    payload["publishObjectOrientation"] = dbg.publishObjectOrientation;
    payload["hitVolumesOverlay"] = dbg.hitVolumesOverlay;
    payload["hideMeshesWhenDrawingHitVolumes"] = dbg.hideMeshesWhenDrawingHitVolumes;

    payload["worldAxisLength"] = dbg.worldAxisLength;
    payload["shipAxisLength"] = dbg.shipAxisLength;
    payload["objectAxisLength"] = dbg.objectAxisLength;
    payload["moduleAxisLength"] = dbg.moduleAxisLength;
    payload["moduleCrossSize"] = dbg.moduleCrossSize;
    payload["rotAxisLength"] = dbg.rotAxisLength;

    payload["axesOverlay"] = dbg.axesOverlay;
    payload["crossesOverlay"] = dbg.crossesOverlay;
    payload["linesOverlay"] = dbg.linesOverlay;

    payload["axisXColor"] = vec4ToJson(dbg.axisXColor);
    payload["axisYColor"] = vec4ToJson(dbg.axisYColor);
    payload["axisZColor"] = vec4ToJson(dbg.axisZColor);
    payload["moduleOriginColor"] = vec4ToJson(dbg.moduleOriginColor);
    payload["modulePivotColor"] = vec4ToJson(dbg.modulePivotColor);
    payload["rotationAxisColor"] = vec4ToJson(dbg.rotationAxisColor);

    SceneRenderStats totalStats = m_perfMainStats;
    totalStats.add(m_perfRearStats);

    json perf;
    perf["frameIndex"] = m_perfFrameIndex;

    perf["fps"] = m_perfFps;
    perf["frameMs"] = m_perfFrameMs;

    perf["updateMs"] = m_perfUpdateMs;
    perf["processHtmlMs"] = m_perfProcessHtmlMs;
    perf["fixedSimMs"] = m_perfFixedSimMs;
    perf["clientUpdateMs"] = m_perfClientUpdateMs;
    perf["playerViewMs"] = m_perfPlayerViewMs;
    perf["uiRootUpdateMs"] = m_perfUiRootUpdateMs;

    perf["mainRenderMs"] = m_perfMainRenderMs;
    perf["rearCameraMs"] = m_perfRearCameraMs;
    perf["renderUiMs"] = m_perfRenderUiMs;
    perf["hudMs"] = m_perfHudMs;

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
        m_server
            ? m_server->debugFastUniverseTime()
            : false;
    payload["debugUniverseTimeScale"] =
    m_server
        ? m_server->debugUniverseTimeScale()
        : 1.0;

    payload["systemMapVisible"] =
        m_systemMapVisible;

    payload["systemMapLiveSnapshotsEnabled"] =
        m_systemMapLiveSnapshotsEnabled;

    payload["liveSystemMapId"] =
        m_liveSystemMapId;

    context().htmlUi().broadcastState(HtmlUiPanelId::DebugControl, payload);
}

void SpaceState::applyDebugControlPayload(const json& payload)
{
    auto& dbg = debug::get().render;

    dbg.drawMeshes = payload.value("drawMeshes", dbg.drawMeshes);
    dbg.renderCockpit = payload.value("renderCockpit", dbg.renderCockpit);
    dbg.renderShipUi = payload.value("renderShipUi", dbg.renderShipUi);
    dbg.renderStarfield = payload.value("renderStarfield", dbg.renderStarfield);
    dbg.showStarLabels = payload.value("showStarLabels", dbg.showStarLabels);
    dbg.showAllStarLabels = payload.value("showAllStarLabels", dbg.showAllStarLabels);

    const std::string previousSceneMode = dbg.sceneMode;
    dbg.sceneMode = payload.value("sceneMode", dbg.sceneMode);
    if (dbg.sceneMode != "game" && dbg.sceneMode != "promo1")
        dbg.sceneMode = "game";

    if (dbg.sceneMode != previousSceneMode)
    {
        if (m_client)
            m_promoSceneScenario.reset(m_client->world());

        m_promoTrackingInitialized = false;
        m_promoPlayerOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    dbg.showConstellationHover = payload.value("showConstellationHover", dbg.showConstellationHover);
    dbg.constellationHoverRadiusPx = payload.value("constellationHoverRadiusPx", dbg.constellationHoverRadiusPx);
    dbg.renderRearCamera = payload.value("renderRearCamera", dbg.renderRearCamera);

    dbg.renderPlayerShip = payload.value("renderPlayerShip", dbg.renderPlayerShip);
    dbg.renderNpcShips = payload.value("renderNpcShips", dbg.renderNpcShips);
    dbg.renderTrafficShips = payload.value("renderTrafficShips", dbg.renderTrafficShips);
    dbg.renderRealShips = payload.value("renderRealShips", dbg.renderRealShips);
    dbg.renderVisualShips = payload.value("renderVisualShips", dbg.renderVisualShips);
    dbg.renderHubs = payload.value("renderHubs", dbg.renderHubs);
    dbg.renderLargeObjects = payload.value("renderLargeObjects", dbg.renderLargeObjects);
    dbg.renderCelestialBodies = payload.value("renderCelestialBodies", dbg.renderCelestialBodies);
    dbg.renderSystemMapObjects = payload.value("renderSystemMapObjects", dbg.renderSystemMapObjects);
    dbg.debugControlAutoUpdates = payload.value("debugControlAutoUpdates", dbg.debugControlAutoUpdates);
    dbg.systemMapLiveRefreshSec = payload.value("systemMapLiveRefreshSec", dbg.systemMapLiveRefreshSec);

    if (dbg.systemMapLiveRefreshSec < 0.02f)
        dbg.systemMapLiveRefreshSec = 0.02f;

    if (dbg.systemMapLiveRefreshSec > 5.0f)
        dbg.systemMapLiveRefreshSec = 5.0f;

    dbg.drawAxes = payload.value("drawAxes", dbg.drawAxes);
    dbg.drawWorldAxes = payload.value("drawWorldAxes", dbg.drawWorldAxes);
    dbg.drawObjectAxes = payload.value("drawObjectAxes", dbg.drawObjectAxes);

    dbg.drawModulePivots = payload.value("drawModulePivots", dbg.drawModulePivots);
    dbg.drawHitVolumes = payload.value("drawHitVolumes", dbg.drawHitVolumes);
    dbg.hitVolumeFilterMode = payload.value("hitVolumeFilterMode", dbg.hitVolumeFilterMode); 
    dbg.publishObjectOrientation = payload.value("publishObjectOrientation", dbg.publishObjectOrientation);
    dbg.hitVolumesOverlay = payload.value("hitVolumesOverlay", dbg.hitVolumesOverlay);
    dbg.hideMeshesWhenDrawingHitVolumes = payload.value("hideMeshesWhenDrawingHitVolumes", dbg.hideMeshesWhenDrawingHitVolumes);

    dbg.worldAxisLength = payload.value("worldAxisLength", dbg.worldAxisLength);
    dbg.shipAxisLength = payload.value("shipAxisLength", dbg.shipAxisLength);
    dbg.objectAxisLength = payload.value("objectAxisLength", dbg.objectAxisLength);
    dbg.moduleAxisLength = payload.value("moduleAxisLength", dbg.moduleAxisLength);
    dbg.moduleCrossSize = payload.value("moduleCrossSize", dbg.moduleCrossSize);
    dbg.rotAxisLength = payload.value("rotAxisLength", dbg.rotAxisLength);

    dbg.axesOverlay = payload.value("axesOverlay", dbg.axesOverlay);
    dbg.crossesOverlay = payload.value("crossesOverlay", dbg.crossesOverlay);
    dbg.linesOverlay = payload.value("linesOverlay", dbg.linesOverlay);

    if (payload.contains("axisXColor"))         dbg.axisXColor = jsonToVec4(payload["axisXColor"], dbg.axisXColor);
    if (payload.contains("axisYColor"))         dbg.axisYColor = jsonToVec4(payload["axisYColor"], dbg.axisYColor);
    if (payload.contains("axisZColor"))         dbg.axisZColor = jsonToVec4(payload["axisZColor"], dbg.axisZColor);
    if (payload.contains("moduleOriginColor"))  dbg.moduleOriginColor = jsonToVec4(payload["moduleOriginColor"], dbg.moduleOriginColor);
    if (payload.contains("modulePivotColor"))   dbg.modulePivotColor = jsonToVec4(payload["modulePivotColor"], dbg.modulePivotColor);
    if (payload.contains("rotationAxisColor"))  dbg.rotationAxisColor = jsonToVec4(payload["rotationAxisColor"], dbg.rotationAxisColor);


    if (m_server)
    {
        m_server->setDebugFastUniverseTime(
            payload.value(
                "debugFastUniverseTime",
                m_server->debugFastUniverseTime()
            )
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



void SpaceState::updatePromoPlayerShipTracking(float dt)
{
    if constexpr (!game::promo::PromoSceneScenario::Enabled)
        return;

    if (!m_promoSceneScenario.cameraTargetValid(m_client->world()))
        return;

    auto& ships =
        m_client->world().ships();

    auto it =
        ships.find(m_playerId.value);

    if (it == ships.end())
        return;

    ClientShipState& playerShip =
        it->second;

    const glm::vec3 playerPos =
        glm::vec3(0.0f, 50.0f, 0.0f);

    const glm::vec3 target =
        m_promoSceneScenario.cameraTarget(m_client->world());

    glm::vec3 toTarget =
        target - playerPos;

    if (glm::length2(toTarget) < 0.000001f)
        return;

    const glm::vec3 forward =
        glm::normalize(toTarget);

    glm::mat4 targetOrientation =
        makePromoPitchOnlyOrientationClient(toTarget);

    

    glm::quat targetQ =
        glm::quat_cast(targetOrientation);

    if (glm::dot(m_promoPlayerOrientation, targetQ) < 0.0f)
    {
        targetQ = -targetQ;
    }

    if (!m_promoTrackingInitialized)
    {
        m_promoPlayerOrientation =
            targetQ;

        m_promoTrackingInitialized = true;
    }

    // Плавное сопровождение.
    // Теперь цель — реальное звено, а не прогноз по времени,
    // поэтому можно держать response выше без "обгона".
    const float response =
        2.6f;

    const float blend =
        1.0f - std::exp(-response * dt);

    m_promoPlayerOrientation =
        glm::slerp(
            m_promoPlayerOrientation,
            targetQ,
            blend
        );

    m_promoPlayerOrientation =
        glm::normalize(m_promoPlayerOrientation);

    playerShip.transform.setWorldPositionMeters(glm::dvec3(playerPos));
    playerShip.renderTransform.setWorldPosition(playerShip.transform.worldPosition);

    playerShip.transform.orientation =
        glm::toMat4(m_promoPlayerOrientation);

    playerShip.renderTransform.orientation =
        glm::toMat4(m_promoPlayerOrientation);

    playerShip.transform.forwardVelocity = 0.0f;
    playerShip.transform.targetSpeed = 0.0f;
    playerShip.transform.localVelocity = glm::vec3(0.0f);

    playerShip.renderTransform.forwardVelocity = 0.0f;
    playerShip.renderTransform.targetSpeed = 0.0f;
    playerShip.renderTransform.localVelocity = glm::vec3(0.0f);
}











void SpaceState::pushSystemMapState()
{
    if (!m_server)
        return;

    const auto& atlas = m_server->starAtlas();
    const auto& celestial = m_server->celestialSnapshot();
    const auto& nav = m_server->playerNavigation();

    json payload;

    payload["universeTimeSeconds"] =
        m_server->universeClock().timeSeconds();

    payload["universeDate"] =
        m_server->universeClock().dateTimeString();

    payload["universeTimeScale"] =
        m_server->universeClock().timeScale();

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



void SpaceState::selectSystemMapSystem(int systemId)
{
    if (!m_server)
        return;

    requestGalaxyMapSnapshotOnce();

    m_systemMapRenderer.focusGalaxySystem(
        systemId,
        m_galaxyMapSnapshot
    );

    if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy)
    {
        // В режиме галактики список систем только выбирает систему,
        // ставит метку и центрирует камеру на выбранной звезде.
        pushSystemMapPanelState();
        return;
    }

    // В режиме системной карты список систем переключает
    // содержимое карты на выбранную систему.
    requestSystemMapSnapshot(
        systemId,
        true
    );

    m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);

    pushSystemMapPanelState();
}





void SpaceState::setSystemMapGalaxyMode()
{
    requestGalaxyMapSnapshotOnce();

    m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Galaxy);

    pushSystemMapPanelState();
}



void SpaceState::setSystemMapCurrentSystemMode()
{
    if (!m_server)
        return;

    const int selectedId =
        m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : m_server->playerNavigation().currentSystemId;

    m_systemMapRenderer.focusGalaxySystem(
        selectedId,
        m_galaxyMapSnapshot
    );

    requestSystemMapSnapshot(
    selectedId,
    true
);

    m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);

    pushSystemMapPanelState();
}




void SpaceState::pushSystemMapPanelState()
{
    requestGalaxyMapSnapshotOnce();

    if (!m_server || !context().app)
        return;

    const auto& atlas = m_server->starAtlas();
    const auto& celestial = m_server->celestialSnapshot();
    const auto& nav = m_server->playerNavigation();

    json payload;

    payload["universeTimeSeconds"] =
        m_server->universeClock().timeSeconds();

    payload["universeDate"] =
        m_server->universeClock().dateTimeString();

    payload["universeTimeScale"] =
        m_server->universeClock().timeScale();

    payload["mode"] =
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy
            ? "Galaxy"
            : "System";

    payload["systemsCount"] = m_galaxyMapSnapshot.systems.size();
    payload["currentSystemId"] = nav.currentSystemId;
    payload["currentSystemName"] = celestial.systemName;

    const int selectedId =
        m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : nav.currentSystemId;

    payload["selectedSystemId"] = selectedId;

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