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
#include "src/game/equipment/signalNode/processing/WorldSignalTxSystem.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"
#include "src/game/player/ActorIdProvider.h"
#include "src/game/player/ActorCodeGenerator.h"
#include "ui/ConfirmExitState.h"
#include "src/galaxy/GalaxyDatabase.h"

#include "src/render/camera/RenderCameraViewport.h"
#include "ui/components/UIText.h"


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
    const Viewport& vp = context().viewport();

    uiRoot = std::make_unique<UIContainer>();

    // ================== задняя камера =========================

    auto rear = std::make_unique<UICameraView>();
    
    rear->id                    = "rear_camera";
    rear->position              = {0.72f, 0.05f};
    rear->size                  = {0.25f, 0.18f};
    rear->cornerRadiusRel       = 0.12f;
    rear->borderThicknessRel    = 0.008f;
    rear->camera                = &m_playerShip.view.camera(ShipCameraMode::Rear);
    rear->borderColor           = {0.2f, 0.8f, 1.0f};

    rear->drawCallback =
        [&](const glm::mat4&, const glm::mat4&)
    {
        DebugGrid::drawInfinite(
            m_playerShip.core.transform().position,
            20000.0f,
            100
        );
    };

    rearView = rear.get();          // ← сохраняем raw pointer если нужен
    uiRoot->addChild(std::move(rear));   // ← теперь отдаём владение


    auto labelMiniViewText = std::make_unique<UIText>();
    labelMiniViewText->id = "rear_label";
    labelMiniViewText->label = "REAR";
    labelMiniViewText->fontPath = "assets/fonts/Roboto-Light.ttf";
    labelMiniViewText->fontSizeRel = 0.025f; // относительный размер от высоты экрана 2.5% от высоты
    labelMiniViewText->color = {0.2f, 0.8f, 1.0f, 0.5f};
    labelMiniViewText->position = {0.03f, 0.15f};

    

    rearView->addChild(std::move(labelMiniViewText));

    // ================== текст передней камеры =========================

    auto labelMainViewText = std::make_unique<UIText>();
    labelMainViewText->id = "main_label";
    labelMainViewText->label = "FRONT";
    labelMainViewText->fontPath = "assets/fonts/Roboto-BOLD.ttf";
    labelMainViewText->fontSizeRel = 0.03f; // относительный размер от высоты экрана 2.5% от высоты
    labelMainViewText->color = {0.2f, 0.8f, 1.0f, 0.1f};
    labelMainViewText->position = {0.06f, 0.13f};

    uiRoot->addChild(std::move(labelMainViewText));

    LOG("[SpaceState] ctor");

    // =======================================================================
    // galaxy test
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
    // устанавливает ограничение HUD для меток за экраном
    // =======================================================================
   
    // начальная позиция
    // m_playerShip
    ShipVisualIdentity visualIdentity = {
        .shipType = "Cobra MK1",
        .shipName = "Jeraya"
    };

    ShipRegistry registry = {
        .instanceId      = 1,                  // пока вручную, потом генератор
        .ownerName       = "Jeraya",
        .ownerActor      = ActorIds::Player(),        // или конкретный ActorId
        .registrationId  = "PL-0001",
        .homePort        = "Lave",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerCard = new CryptoCard(
    generateActorCode(),
    "Player Access Card" );
    playerCard->actor = ActorIds::Player();

    ShipInitData initData;
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = {playerCard};

    m_playerShip.init(
        context(),
        ShipRole::Player,
        EliteCobraMk1::EliteCobraMk1Descriptor(ShipRole::Player),
        {0.0f, 50.0f, 10.0f},
        initData
    );




    

    visualIdentity = {
        .shipType = "Cobra MK3",
        .shipName = "Scarlet Hawk Moth"
    };

    registry = {
        .instanceId      = 2,                  // пока вручную, потом генератор
        .ownerName       = "Digital Lion",
        .ownerActor      =  ActorIds::Player(),        // или конкретный ActorId
        .registrationId  = "PL-0002",
        .homePort        = "Lave",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerNPC1Card = new CryptoCard(
    generateActorCode(),
    "Player Access Card" );
    playerNPC1Card->actor = ActorIds::Player();

    
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = { playerNPC1Card };
    // --- NPC #1 ---
  
    m_npcShips.emplace_back();
    m_npcShips.back().init(
        context(),
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(ShipRole::NPC),
        {20.0f, 0.0f, -50.0f},
        initData
    );




    visualIdentity = {
        .shipType = "Cobra MK3",
        .shipName = "Hooded snake"
    };

    registry = {
        .instanceId      = 3,                  // пока вручную, потом генератор
        .ownerName       = "Pipito Karnelio",
        .ownerActor      = ActorIds::Player(),        // или конкретный ActorId
        .registrationId  = "PL-0003",
        .homePort        = "Lave",
        .shipRole        = ShipRoleType::Civilian
    };

    auto* playerNPC2Card = new CryptoCard(
    generateActorCode(),
    "Player Access Card" );
    playerNPC2Card->actor = ActorIds::Player();

    
    initData.visual = visualIdentity;
    initData.registry = registry;
    initData.initialInventory = { playerNPC2Card };


    // --- NPC #2 ---
    
    m_npcShips.emplace_back();
    m_npcShips.back().init(
        context(),
        ShipRole::NPC,
        EliteCobraMk1::EliteCobraMk1Descriptor(ShipRole::NPC),
        {-70.0f, 50.0f, -70.0f},
        initData
    );






    // инициализация параметров рендера
    m_hudRenderer.init(context());
    m_worldLabelRenderer.init(context());





    // world = vacuum
    m_world.linearDrag   = 0.0f;
    m_world.maxSafeDecel = 50.0f;





    // // описание физических и сигнальных объектов
    // WorldObject station;
    // station.position = glm::vec3(0.0f, 0.0f, -500.0f);
    // station.label = "LAVE STATION";
    // m_worldObjects.push_back(station);

    // m_worldObjects.push_back({
    //     glm::vec3(0.0f, 0.0f, -200.0f),
    //     "RELAY BEACON 3766"
    // });

   
    // m_planets.push_back({
    //     glm::vec3(0.0f, 0.0f, 0.0f),
    //     6000.0f
    // });

    

    



    InterferenceSource jammer;
    jammer.type     = InterferenceType::Active;
    jammer.position = {0, 0, 155};
    jammer.power    = 300.0f;
    jammer.radius   = 100.0f;
    jammer.enabled  = false;

    // m_interferenceSources.push_back(jammer);


    
    m_planets.clear();

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

    // режимы управления камерой
    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F1))
    m_layout = ScreenLayout::Front_Main_Rear_Mini;

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F2))
        m_layout = ScreenLayout::Rear_Main_Front_Mini;

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F3))
        m_layout = ScreenLayout::Front_Main_Drone_Mini;

    if (Input::instance().isKeyPressedOnce(GLFW_KEY_F4))
        m_layout = ScreenLayout::Drone_Main_Front_Mini;

    // управление кораблем
    m_playerShip.handleInput();
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
    LOG("[SpaceState] update begin");
    

    std::vector<Ship*> worlds_signals_ships;
    worlds_signals_ships.push_back(&m_playerShip);

    for (auto& npc : m_npcShips)
        worlds_signals_ships.push_back(&npc);


    WorldSignalTxSystem::collectFromShips(
        worlds_signals_ships,
        m_worldSignals
    );


    // m_playerShip.updateSignals(
    //     dt,
    //     m_worldSignals,
    //     m_planets,
    //     m_interferenceSources
    // );


    m_playerShip.update(
        dt,
        m_world,
        m_worldSignals,
        m_planets,
        m_interferenceSources
    );



    for (auto& npc : m_npcShips)
    {
        npc.update(
            dt,
            m_world,
            m_worldSignals,
            m_planets,
            m_interferenceSources
        );
    }
   
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
            mainCam = &m_playerShip.view.camera(ShipCameraMode::Cockpit);
            miniCam = &m_playerShip.view.camera(ShipCameraMode::Rear);
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
            mainCam = &m_playerShip.view.camera(ShipCameraMode::Rear);
            miniCam = &m_playerShip.view.camera(ShipCameraMode::Cockpit);
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
            mainCam = &m_playerShip.view.camera(ShipCameraMode::Cockpit);
            miniCam = &m_playerShip.view.camera(ShipCameraMode::Drone);
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
            mainCam = &m_playerShip.view.camera(ShipCameraMode::Drone);
            miniCam = &m_playerShip.view.camera(ShipCameraMode::Cockpit);
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
            DebugGrid::drawInfinite(
                m_playerShip.core.transform().position,
                20000.0f,
                100
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
    // m_hudStatics.clear();
    // m_hudRects.clear();

    // m_hudRects.push_back({
    //     { 20.0f / fx, 20.0f / fy },
    //     { 180.0f / fx, 60.0f / fy },
    //     { 0.0f, 1.0f, 0.0f }
    // });

    // m_hudStatics.push_back({
    //     "HUD ONLINE",
    //     { 30.0f / fx, 40.0f / fy },
    //     { 0.0f, 1.0f, 0.0f }
    // });

    // --- рамка мини-камеры ---
    {
        // const Viewport& vp = context().viewport();

        // int miniW = 300;
        // int miniH = 200;
        // int miniX = vp.width - miniW - 20;
        // int miniY = 20;

        // glDisable(GL_DEPTH_TEST);

        // glColor3f(0.2f, 0.8f, 1.0f);

        // glBegin(GL_LINE_LOOP);
        // glVertex2f(miniX, miniY);
        // glVertex2f(miniX + miniW, miniY);
        // glVertex2f(miniX + miniW, miniY + miniH);
        // glVertex2f(miniX, miniY + miniH);
        // glEnd();
    }

    
    // -------------------------------------------------
    // 2. HUD ограничитель
    // -------------------------------------------------
    
    m_playerShip.view.renderHudBoundary();
    
    // -------------------------------------------------
    // 3. Подготовка матриц для меток
    // -------------------------------------------------
    
    if (m_activeCameraMode != ShipCameraMode::Drone)
    {
        m_playerShip.view.renderWorldLabels(
            m_playerShip.core.signalPresentation().labels(),
            m_playerShip.core.transform().position,
            m_activeMainCamera->viewMatrix(),
            m_activeMainCamera->projectionMatrix(),
            vp
        );
    }

    m_playerShip.view.renderCockpit();


    uiRoot->render(vp);

    // // 3. векторные приборы
    // cockpitPass.render(m_playerShip.view.getCockpitState());

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





