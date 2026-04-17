#include <glad/gl.h>
#include <iostream>

#include "SpaceState.h"
#include "core/StateStack.h"
#include "core/Log.h"
#include "input/Input.h"

#include "render/DebugGrid.h"

#include "src/game/player/ActorIdProvider.h"
#include "src/game/player/ActorCodeGenerator.h"

#include "ui/ConfirmExitState.h"
#include "src/galaxy/GalaxyDatabase.h"

#include "src/render/camera/RenderCameraViewport.h"
#include "ui/components/UIText.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "ui/components/radar/RadarWidgetBase.h"
#include "src/game/network/ClientMessage.h"

#include "src/render/InitShaders.h"

#include "src/game/ship/view/PlayerShipView.h"

#include "game/debug/AttachmentEditorPayload.h"
#include "ui/html/HtmlUiManager.h"
#include "ui/html/HtmlUiPanelId.h"
#include "ui/html/HtmlUiMessage.h"
#include <mutex>



namespace
{
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
}




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
    initHUD();

    
    // =======================================================================
    // load galaxy database + test
    // =======================================================================
    
    GalaxyDatabase galaxy;
    galaxy.loadFromDirectory("assets/data/galaxy");
    galaxy.validate();
    
    std::cout
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

    // === управление кораблём ===
    m_inputMapper.update(m_playerControl);

    


    m_localTick++;

    m_playerControl.controlTick = m_localTick;

    m_sentInputs.push_back(m_playerControl);

    m_server->submitCommand(m_playerId, m_playerControl);

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
   

    // команды для отображения во внешнем браузере.
    processHtmlCommands();
    // processHtmlDebugCommands();



    m_simAccumulator += dt;

    while (m_simAccumulator >= SIM_FIXED_DT)
    {
        
        m_transport->update(SIM_FIXED_DT);
        m_server->update(SIM_FIXED_DT);
        m_transport->update(0.0f);
        m_simAccumulator -= SIM_FIXED_DT;
    }

    m_client->update(
        dt,
        m_server->world(),
        SIM_FIXED_DT
    );



    const auto& ships = m_client->world().ships();

    auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;

    const auto& ship = it->second;

     m_playerView->update(
        dt,
        ship.role,
        ship.renderTransform.position,
        ship.renderTransform.orientation
    );

    m_playerView->updateCockpitStateFromSnapshot(
        ship.transform.forwardVelocity,
        ship.transform.targetSpeed,
        ship.transform.cruiseActive,
        ship.signalPresentation.labelsVector()
    ); 
    
    
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
            ship.renderTransform.position,
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
    uiRoot->update(dt);


    // DEBUG: отправляем состояние корабля в браузер
    // --------------------------------------------
    static float debugTimer = 0.0f;
    debugTimer += dt;
    if (debugTimer > 0.1f)
    {
        debugTimer = 0.0f;
        pushShipCoreState();
    }
    // static float debugTimer = 0.0f;
    // debugTimer += dt;
    // if (debugTimer > 0.1f)
    // {
    //     debugTimer = 0.0f;

    //     const auto& status = ship.shipCoreStatus;
    //     json j = shipCoreStatusToJson(status);

    //     context().htmlUi().broadcastState(HtmlUiPanelId::ShipCore, j);
    // }



    
    
    
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

    const Viewport& vp = context().viewport();
    float aspect = (float)vp.width / (float)vp.height;

    if (mainCam)
    {
        mainCam->setAspect(aspect);
    }


    
    // -------------------------------
    // 3D ПРОЕКЦИЯ
    // -------------------------------    
    RenderCameraViewport::render(
        *mainCam,
        vp,
        0,
        0,
        vp.width,
        vp.height,
        [&](auto view, auto proj)
        {
            m_sceneRenderer.render(
                m_client->world(),
                m_playerId,
                view,
                proj,
                0,
                "mainCam"
            );

        }
    );



    // -------------------------------
    // Rear - камера корабля.
    // -------------------------------

    rearView->camera = miniCam;
    rearView->renderToTexture(vp, rearView->drawCallback);

    
}



// =====================================================================================
// Render HUD
// =====================================================================================
void SpaceState::renderHUD()
{
    LOG("[SpaceState] renderHUD begin");
    const Viewport& vp = context().viewport();

    int vx = vp.width;
    int vy = vp.height;

    float fx = (float)vp.width;
    float fy = (float)vp.height;

    

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
        
    const glm::vec3 playerPos = it->second.renderTransform.position;


    
    const auto& playerShip = it->second;    
    const auto& radarContacts = playerShip.radarContacts;



    
    // -------------------------------------------------
    // 2. HUD ограничитель
    // -------------------------------------------------
    
    m_playerView->renderHudBoundary();
    
    // -------------------------------------------------
    // 3. Подготовка матриц для меток
    // -------------------------------------------------
    
    if (m_activeCameraMode != ShipCameraMode::Drone)
    {
       
        auto it = m_client->world().ships().find(m_playerId.value);
        if (it != m_client->world().ships().end())
        {
            const auto& shipState = it->second;

            // Найдём snapshot игрока (для labels)
            auto it = m_client->world().ships().find(m_playerId.value);
            if (it != m_client->world().ships().end())
            {
                const auto& ship = it->second;

                m_playerView->renderWorldLabels(
                    ship.signalPresentation.labelsVector(),
                    ship.renderTransform.position,
                    m_activeMainCamera->viewMatrix(),
                    m_activeMainCamera->projectionMatrix(),
                    vp
                );
            }
        }
    }

    m_playerView->renderCockpit();
    uiRoot->render(vp);


    // // 3. векторные приборы
    glEnable(GL_DEPTH_TEST);

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

                    std::cout
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
                    pushDebugControlState();
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


void SpaceState::pushShipCoreState()
{
    context().htmlUi().setActivePanel(HtmlUiPanelId::ShipCore);

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
    payload["drawAxes"] = dbg.drawAxes;
    payload["drawWorldAxes"] = dbg.drawWorldAxes;
    payload["drawObjectAxes"] = dbg.drawObjectAxes;
    payload["drawModulePivots"] = dbg.drawModulePivots;
    payload["drawHitVolumes"] = dbg.drawHitVolumes;
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

    context().htmlUi().broadcastState(HtmlUiPanelId::DebugControl, payload);
}

void SpaceState::applyDebugControlPayload(const json& payload)
{
    auto& dbg = debug::get().render;

    dbg.drawMeshes = payload.value("drawMeshes", dbg.drawMeshes);
    dbg.drawAxes = payload.value("drawAxes", dbg.drawAxes);
    dbg.drawWorldAxes = payload.value("drawWorldAxes", dbg.drawWorldAxes);
    dbg.drawObjectAxes = payload.value("drawObjectAxes", dbg.drawObjectAxes);
    dbg.drawModulePivots = payload.value("drawModulePivots", dbg.drawModulePivots);
    dbg.drawHitVolumes = payload.value("drawHitVolumes", dbg.drawHitVolumes);
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
}