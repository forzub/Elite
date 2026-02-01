#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SpaceState.h"

#include "core/StateStack.h"
#include "core/Log.h"

#include "input/Input.h"


#include "render/DebugGrid.h"
#include "render/ScreenUtils.h"


#include "src/game/equipment/signalNode/processing/SignalPolicy.h"

#include "game/ship/hud/worldlabels/WorldLabelSystem.h"
#include "game/ship/hud/worldlabels/WorldSignalWaves.h"
#include "game/equipment/signalNode/processing/WorldSignalTxSystem.h"

#include "game/ship/descriptors/EliteCobraMk1.h"
#include "game/ship/ShipRole.h"


#include "ui/ConfirmExitState.h"
#include "ui/HudRenderer.h"



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

    LOG("[SpaceState] ctor");

    
    // =======================================================================
    // устанавливает ограничение HUD для меток за экраном
    // =======================================================================
    const Viewport& vp = context().viewport();

    
    // начальная позиция
    // m_playerShip.transform.position = {0.0f, 5.0f, 10.0f};
    m_playerShip.init(
        context(),
        ShipRole::Player,
        getEliteCobraMk1(),
        {0.0f, 5.0f, 10.0f}
    );


    // --- NPC #1 ---
    m_npcShips.emplace_back();
    m_npcShips.back().init(
        context(),
        ShipRole::NPC,
        getEliteCobraMk1(),
        {20.0f, 0.0f, -50.0f}
    );

    // --- NPC #2 ---
    m_npcShips.emplace_back();
    m_npcShips.back().init(
        context(),
        ShipRole::NPC,
        getEliteCobraMk1(),
        {-70.0f, 50.0f, -70.0f}
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

     const Viewport& vp = context().viewport();

    float fx = (float)vp.width;
    float fy = (float)vp.height;

    // -------------------------------
    // 3D ПРОЕКЦИЯ
    // -------------------------------
    glm::mat4 projection = glm::perspective(
        glm::radians(70.0f),
        fx / fy,
        0.1f,
        5000.0f
    );

    glm::mat4 view = m_playerShip.camera.viewMatrix();

    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // -------------------------------
    // 3D МИР
    // -------------------------------
    DebugGrid::drawInfinite(
        m_playerShip.transform.position,
        200.0f,
        100
    );
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
    m_hudStatics.clear();
    m_hudRects.clear();

    m_hudRects.push_back({
        { 20.0f / fx, 20.0f / fy },
        { 180.0f / fx, 60.0f / fy },
        { 0.0f, 1.0f, 0.0f }
    });

    m_hudStatics.push_back({
        "HUD ONLINE",
        { 30.0f / fx, 40.0f / fy },
        { 0.0f, 1.0f, 0.0f }
    });

    LOG("[SpaceState] renderHUD after rects");
    m_hudRenderer.renderRects(m_hudRects);

    LOG("[SpaceState] renderHUD after text");
    m_hudRenderer.renderText(m_hudStatics);


    // -------------------------------------------------
    // 2. HUD ограничитель
    // -------------------------------------------------
    {
        const auto& contour = m_playerShip.hudEdgeMapper.boundaryPx();
        if (contour.size() >= 2)
        {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(0.2f, 0.8f, 1.0f, 0.25f); // голубой, ~25% прозрачности

            glBegin(GL_LINE_LOOP);
            for (const auto& p : contour)
            {
                glVertex2f(p.x, p.y);
            }
            glEnd();
        }
    }

    // если произошел ресайз окна
    // m_playerShip.hudEdgeMapper.setBoundary(
    //     m_playerShip.desc->hud.edgeBoundary.contour,
    //     vp.width,
    //     vp.height
    // );

    // -------------------------------------------------
    // 3. Подготовка матриц для меток
    // -------------------------------------------------

    // --- camera matrices нужны ТОЛЬКО здесь ---
    glm::mat4 view = m_playerShip.camera.viewMatrix();
    glm::mat4 proj = m_playerShip.camera.projectionMatrix();
    // glm::mat4 view = m_camera.viewMatrix();
    // glm::mat4 proj = m_camera.projectionMatrix();

    
    
    glm::vec2 screenCenter(vp.width  * 0.5f,vp.height * 0.5f);


    for (auto& [signal, label] :
     m_playerShip.signalPresentation.labels())
{
    glm::vec2 projectedPos;

    bool projected = projectToScreen(
        label.data.worldPos,
        view,
        proj,
        vp.width,
        vp.height,
        projectedPos
    );

    glm::vec3 toTarget =
        glm::normalize(label.data.worldPos - m_playerShip.transform.position);

    glm::vec4 viewDir4 = view * glm::vec4(toTarget, 0.0f);
    glm::vec2 dir2D(viewDir4.x, -viewDir4.y);

    if (glm::length(dir2D) < 1e-4f)
        continue;

    dir2D = glm::normalize(dir2D);
    label.edgeDir = dir2D;

    bool insideHud = false;

    if (projected)
        insideHud = m_playerShip.hudEdgeMapper.isInsideBoundary(projectedPos);

    if (projected && insideHud)
    {
        label.onScreen  = true;
        label.screenPos = projectedPos;
    }
    else
    {
        glm::vec2 edgePos;
        if (m_playerShip.hudEdgeMapper.projectDirection(
                screenCenter,
                dir2D,
                edgePos))
        {
            label.onScreen  = false;
            label.screenPos = edgePos;
        }
    }

    m_worldLabelRenderer.renderHUD(label);
}

    
    glEnable(GL_DEPTH_TEST);
    LOG("[SpaceState] renderHUD after text");
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


