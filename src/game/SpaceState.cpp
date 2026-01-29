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


#include "game/signals/SignalPolicy.h"
#include "game/signals/WorldLabelSystem.h"
#include "game/signals/WorldSignalWaves.h"
#include "game/ship/descriptors/EliteCobraMk1.h"


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

    m_playerShip.initialize(
        getEliteCobraMk1(),
        vp.width,
        vp.height
    );
    // начальная позиция
    m_playerShip.transform.position = {0.0f, 5.0f, 10.0f};
    m_camera.setPosition(m_playerShip.transform.position);
    m_camera.setAspect(context().aspect());


    // инициализация параметров рендера
    m_hudRenderer.init(context());
    m_worldLabelRenderer.init(context());


    // world = vacuum
    m_world.linearDrag   = 0.0f;
    m_world.maxSafeDecel = 50.0f;



    // описание физических и сигнальных объектов
    WorldObject station;
    station.position = glm::vec3(0.0f, 0.0f, -500.0f);
    station.label = "LAVE STATION";
    m_worldObjects.push_back(station);

    m_worldObjects.push_back({
        glm::vec3(0.0f, 0.0f, -200.0f),
        "RELAY BEACON 3766"
    });

   
    // m_planets.push_back({
    //     glm::vec3(0.0f, 0.0f, 0.0f),
    //     6000.0f
    // });

    m_receiverNoiseFloor = VisualTuning::receiverBaseNoise;

    // тест сигналов 
    m_worldSignals.clear(); 
    
        
    // Планета (высоко над плоскостью, дальняя, всегда стабильная)
    m_worldSignals.push_back({
        SignalType::Planets,
        SignalDisplayClass::Global,
        { 0.0f, 800.0f, 900.0f }, // 2 км вперед, 800 м вверх
        300.0f,   // minRange (слышна)
        1200.0f,   // maxRange
        true,
        "Asterion IV"
    });

    // Станция (ближе, устойчивая)
    m_worldSignals.push_back({
        SignalType::StationClass,
        SignalDisplayClass::Global,
        { 300.0f, -200.0f, 900.0f }, // 800 м вперед
        700.0f,
        1200.0f,
        true,
        "Orbital Station"
    });

    // SOS древний (далеко, нестабильный)
    m_worldSignals.push_back({
        SignalType::SOSAntic,
        SignalDisplayClass::Local,
        { -200.0f, 0.0f, -900.0f }, // 1.8 км
        70000.0f,
        120000.0f,
        true,
        "? Unknown"
    });

    // Пиратский транспондер (средняя дистанция)
    m_worldSignals.push_back({
        SignalType::PirateTransponder,
        SignalDisplayClass::Local,
        { -150.0f, 0.0f, -900.0f }, // 900 м
        700.0f,
        1200.0f,
        true,
        "Unknown Vessel"
    });



    // -------------------------- test block -----------------------------
    // m_worldSignals.clear(); 

    m_worldSignals.push_back({
        SignalType::Beacon,
        SignalDisplayClass::Other,
        {0, 0, -70}, 
        500.0f,   
        1000.0f,   
        true,
        "TEST_SIGNAL"
    });



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

    // --- Cruise (hold J) ---

    auto& ctrl = m_playerShip.control;
    ctrl = ShipControlState{};

    ctrl.cruiseActive = Input::instance().isKeyPressed(GLFW_KEY_J);

    // --- Rotation (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.pitchInput =
            (Input::instance().isKeyPressed(GLFW_KEY_W) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_S) ? 1.0f : 0.0f);

        ctrl.rollInput =
            (Input::instance().isKeyPressed(GLFW_KEY_A) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_D) ? 1.0f : 0.0f);

        ctrl.yawInput =
            (Input::instance().isKeyPressed(GLFW_KEY_Q) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_E) ? 1.0f : 0.0f);
    }
    else
    {
        ctrl.pitchInput = 0.0f;
        ctrl.rollInput  = 0.0f;
        ctrl.yawInput   = 0.0f;
    }

    // --- Target speed control ---
    ctrl.targetSpeedRate = 0.0f;

    if (Input::instance().isKeyPressed(GLFW_KEY_KP_ADD) ||
        Input::instance().isKeyPressed(GLFW_KEY_EQUAL))
        ctrl.targetSpeedRate = +1.0f;

    if (Input::instance().isKeyPressed(GLFW_KEY_KP_SUBTRACT) ||
        Input::instance().isKeyPressed(GLFW_KEY_MINUS))
        ctrl.targetSpeedRate = -1.0f;

    // --- Manoeuvre thrusters (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.strafeInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_6) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_4) ? 1.0f : 0.0f);

        ctrl.forwardInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_8) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_2) ? 1.0f : 0.0f);

        ctrl.liftInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_9) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_3) ? 1.0f : 0.0f);
    }
    else
    {
        ctrl.strafeInput  = 0.0f;
        ctrl.forwardInput = 0.0f;
        ctrl.liftInput    = 0.0f;
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


// =====================================================================================
// Update
// =====================================================================================
void SpaceState::update(float dt)
{
    LOG("[SpaceState] update begin");
    m_signalReceiver.update(
        dt,
        m_playerShip.transform.position,
        m_worldSignals,
        m_planets,
        m_interferenceSources,
        m_receiverNoiseFloor,
        m_signalResults
    );



    for (const SignalReceptionResult& result : m_signalResults)
    {
        WorldLabel& label = getOrCreateWorldLabel(result); // как у тебя реализовано

        // --- 1. Семантика (data)
        label.data.worldPos             = result.sourceWorldPos;
        label.data.semanticState        = result.semanticState;
        label.data.signalToNoiseRatio   = result.signalToNoiseRatio;
        label.data.receivedPower        = result.receivedPower;
        label.data.stability            = result.stability;
        label.data.displayClass         = result.source->displayClass;

        if (result.semanticState == SignalSemanticState::Decoded || label.data.displayClass == SignalDisplayClass::Global)
        {
            label.data.displayName = result.source->label; // или из WorldSignal
            label.data.distance = result.distance;
            label.data.hasDistance = true;
            label.visual.visibility = 1.0;
            label.visual.presence = SignalPresence::Present;
        }
        else
        {
            label.data.displayName = "undefined";
            label.data.hasDistance = false;
        }     
        
        // --- 2. Визуальная инерция

        WorldLabelSystem::updateVisualState(
            dt,
            result.semanticState,
            result.signalToNoiseRatio,
            label.visual
        );

        // std::cout
        // << "[Label] src=" << result.source
        // << " presence=" << (int)label.visual.presence
        // << " vis=" << label.visual.visibility
        // << " state=" << (int)label.data.semanticState
        // << " snr=" << label.data.signalToNoiseRatio
        // << std::endl;
        
        bool allowWaveSpawn =
        ((label.data.semanticState == SignalSemanticState::Noise) && 
        (label.data.displayClass != SignalDisplayClass::Global) &&
        (label.visual.presence != SignalPresence::FadingOut) &&
        (label.visual.presence != SignalPresence::Absent));

        if(label.data.semanticState == SignalSemanticState::Decoded && label.data.displayClass == SignalDisplayClass::Other)
            allowWaveSpawn = true;

            
        
        updateWorldSignalWaves(
            label.visual,
            dt,
            allowWaveSpawn
        );

    }



    // // -------------------------------------------------------------------------
    // // Rotation physics
    // // -------------------------------------------------------------------------
    // m_shipController.update(
    //     dt,
    //     m_params,
    //     m_ship,
    //     m_world
    // );


    // // -------------------------------------------------------------------------
    // // Camera
    // // -------------------------------------------------------------------------
    // m_cameraController.update(dt, m_ship, m_camera);
    // LOG("[SpaceState] update end");

    m_playerShip.update(dt, m_world);
    m_playerShip.updateCamera(m_camera);
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

    int vx = vp.width;
    int vy = vp.height;

    float fx = (float)vp.width;
    float fy = (float)vp.height;
    
    glm::mat4 projection = glm::perspective(
        glm::radians(70.0f),
        fx / fy,
        0.1f,
        5000.0f
    );

    glm::mat4 view = m_camera.viewMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    // DebugGrid::drawInfinite(m_ship.position, 200.0f, 100);
    DebugGrid::drawInfinite(m_playerShip.transform.position, 200.0f, 100);
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
    glm::mat4 view = m_camera.viewMatrix();
    glm::mat4 proj = m_camera.projectionMatrix();

    
    
    glm::vec2 screenCenter(vp.width  * 0.5f,vp.height * 0.5f);


    for (auto& [signal, lbl] : m_worldLabels)
    {
        glm::vec2 projectedPos;

        bool projected = projectToScreen(
            lbl.data.worldPos,
            view,
            proj,
            vp.width,
            vp.height,
            projectedPos
        );

        glm::vec3 toTarget =
            glm::normalize(lbl.data.worldPos - m_playerShip.transform.position);

        glm::vec4 viewDir4 = view * glm::vec4(toTarget, 0.0f);
        glm::vec2 dir2D(viewDir4.x, -viewDir4.y);

        if (glm::length(dir2D) < 1e-4f)
            continue;

        dir2D = glm::normalize(dir2D);

        lbl.edgeDir = dir2D;

        // --- решаем, обычная метка или edge ---
        bool insideHud = false;

        if (projected)
        {
            insideHud = m_playerShip.hudEdgeMapper.isInsideBoundary(projectedPos);
        }

        if (projected && insideHud)
        {
            // обычная метка
            lbl.onScreen  = true;
            lbl.screenPos = projectedPos;
        }
        else
        {
            // edge-метка
            glm::vec2 edgePos;
            if (m_playerShip.hudEdgeMapper.projectDirection(
                    screenCenter,
                    dir2D,
                    edgePos))
            {
                lbl.onScreen  = false;
                lbl.screenPos = edgePos;
            }
        }


        m_worldLabelRenderer.renderHUD(lbl);
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


// =====================================================================================
// get Or Create WorldLabel
// =====================================================================================
WorldLabel& SpaceState::getOrCreateWorldLabel(const SignalReceptionResult& result)
{
    const WorldSignal* key = result.source;

    auto it = m_worldLabels.find(key);
    if (it != m_worldLabels.end())
        return it->second;

    // --- создаём новый WorldLabel
    WorldLabel label;
    label.visual.presence = SignalPresence::Absent;
    label.visual.visibility = 0.0f;
    label.visual.presenceTimer = 0.0f;
    label.visual.noisePhase = 0.0f;

    auto [newIt, inserted] = m_worldLabels.emplace(key, std::move(label));
    return newIt->second;
}
