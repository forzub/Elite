#include <glad/gl.h>
#include <iostream>

// #include <GLFW/glfw3.h>
// #include <glm/glm.hpp>
// #include <glm/gtc/matrix_transform.hpp>
// #include <glm/gtc/type_ptr.hpp>
// #include "render/ScreenUtils.h"
// #include "render/bitmap/TextureLoader.h"
// #include "game/ship/cockpit/CockpitMeshBuilder.h"
// #include "game/ship/cockpit/CockpitStrokeBuilder.h"
// #include "src/game/equipment/signalNode/processing/SignalPolicy.h"
// #include "src/game/ship/hud/worldlabels/WorldLabelSystem.h"
// #include "src/game/ship/hud/worldlabels/WorldSignalWaves.h"
// #include "src/game/ship/core/ShipRole.h"
// #include "ui/HudRenderer.h"

#include "SpaceState.h"
#include "core/StateStack.h"
#include "core/Log.h"
#include "input/Input.h"

#include "render/DebugGrid.h"

// #include "src/game/ship/ShipDescriptorRegistry.h"
// #include "src/game/ship/descriptors/EliteCobraMk1.h"

#include "src/game/player/ActorIdProvider.h"
#include "src/game/player/ActorCodeGenerator.h"

#include "ui/ConfirmExitState.h"
#include "src/galaxy/GalaxyDatabase.h"

#include "src/render/camera/RenderCameraViewport.h"
#include "ui/components/UIText.h"
#include "src/game/network/LocalLoopbackTransport.h"
#include "src/game/equipment/radar/RadarDesc.h"
// #include "src/ui/components/RadarWidgetFactory.h"
#include "ui/components/RadarWidgetBase.h"



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

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F1))
        m_layout = ScreenLayout::Front_Main_Rear_Mini;

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F2))
        m_layout = ScreenLayout::Rear_Main_Front_Mini;

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F3))
        m_layout = ScreenLayout::Front_Main_Drone_Mini;

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F4))
        m_layout = ScreenLayout::Drone_Main_Front_Mini;

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
    // LOG("[SpaceState] update begin");

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


    // std::cout << "CAMERA POS: "
    //       << m_activeMainCamera->position().x << " "
    //       << m_activeMainCamera->position().y << " "
    //       << m_activeMainCamera->position().z
    //       << std::endl;
  

    const Viewport& vp = context().viewport();
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
                m_playerId
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







