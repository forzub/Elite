#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SpaceState.h"

#include "core/StateStack.h"

#include "input/Input.h"


#include "render/DebugGrid.h"
#include "render/ScreenUtils.h"

#include "game/ShipParams.h"
#include "game/signals/SignalPolicy.h"
#include "game/signals/WorldLabelSystem.h"
#include "game/signals/WorldSignalWaves.h"

#include "ui/ConfirmExitState.h"
#include "ui/HudRenderer.h"


// bool signalAllowsDistance(const WorldSignal& sig, float dist); - в SignalPolicy


// =====================================================================================
// Constructor
// =====================================================================================
SpaceState::SpaceState(StateStack& states)
    : GameState(states)
    , m_params{
          // angular limits
          90.0f, 60.0f, 120.0f,

          // angular dynamics
          1000.0f,  // angularAccel
          9.0f,     // angularDamping

          // linear movement
          500.0f,    // maxCombatSpeed (m/s)
          10000.0f,  // maxCruiseSpeed (game-scaled)
          5.0f,      // throttleAccel

          // stabilization
          0.0f,

          // manoeuvre thrusters
          20.0f, 6.0f, 50.0f
      }
{

    m_hudRenderer.init();
    m_worldLabelRenderer.init(context());

    m_ship.position = {0.0f, 5.0f, 10.0f};
    m_camera.setPosition(m_ship.position);


    // world = vacuum
    m_world.linearDrag   = 0.0f;
    m_world.maxSafeDecel = 50.0f;

    m_camera.setAspect(context().aspect);

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
        { 0.0f, 800.0f, -900.0f }, // 2 км вперед, 800 м вверх
        300.0f,   // minRange (слышна)
        1200.0f,   // maxRange
        true,
        "Asterion IV"
    });

    // Станция (ближе, устойчивая)
    m_worldSignals.push_back({
        SignalType::StationClass,
        SignalDisplayClass::Global,
        { -300.0f, 200.0f, -900.0f }, // 800 м вперед
        300.0f,
        1200.0f,
        true,
        "Orbital Station"
    });

    // SOS древний (далеко, нестабильный)
    m_worldSignals.push_back({
        SignalType::SOSAntic,
        SignalDisplayClass::Local,
        { 200.0f, 0.0f, -900.0f }, // 1.8 км
        300.0f,
        1200.0f,
        true,
        "? Unknown"
    });

    // Пиратский транспондер (средняя дистанция)
    m_worldSignals.push_back({
        SignalType::PirateTransponder,
        SignalDisplayClass::Local,
        { -150.0f, 0.0f, -900.0f }, // 900 м
        300.0f,
        1200.0f,
        true,
        "Unknown Vessel"
    });



    // -------------------------- test block -----------------------------
    m_worldSignals.clear(); 

    m_worldSignals.push_back({
        SignalType::Beacon,
        SignalDisplayClass::Local,
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
    //     {0, 10, 50},
    //     20
    // });






    
        
}







// =====================================================================================
// Input
// =====================================================================================
void SpaceState::handleInput()
{

    // --- Cruise (hold J) ---
    m_ship.cruiseActive = Input::instance().isKeyPressed(GLFW_KEY_J);

    // --- Rotation (disabled in cruise) ---
    if (!m_ship.cruiseActive)
    {
        m_ship.pitchInput =
            (Input::instance().isKeyPressed(GLFW_KEY_W) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_S) ? 1.0f : 0.0f);

        m_ship.rollInput =
            (Input::instance().isKeyPressed(GLFW_KEY_A) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_D) ? 1.0f : 0.0f);

        m_ship.yawInput =
            (Input::instance().isKeyPressed(GLFW_KEY_Q) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_E) ? 1.0f : 0.0f);
    }
    else
    {
        m_ship.pitchInput = 0.0f;
        m_ship.rollInput  = 0.0f;
        m_ship.yawInput   = 0.0f;
    }

    // --- Target speed control ---
    m_ship.targetSpeedRate = 0.0f;

    if (Input::instance().isKeyPressed(GLFW_KEY_KP_ADD) ||
        Input::instance().isKeyPressed(GLFW_KEY_EQUAL))
        m_ship.targetSpeedRate = +1.0f;

    if (Input::instance().isKeyPressed(GLFW_KEY_KP_SUBTRACT) ||
        Input::instance().isKeyPressed(GLFW_KEY_MINUS))
        m_ship.targetSpeedRate = -1.0f;

    // --- Manoeuvre thrusters (disabled in cruise) ---
    if (!m_ship.cruiseActive)
    {
        m_ship.strafeInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_6) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_4) ? 1.0f : 0.0f);

        m_ship.forwardInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_8) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_2) ? 1.0f : 0.0f);

        m_ship.liftInput =
            (Input::instance().isKeyPressed(GLFW_KEY_KP_9) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_KP_3) ? 1.0f : 0.0f);
    }
    else
    {
        m_ship.strafeInput  = 0.0f;
        m_ship.forwardInput = 0.0f;
        m_ship.liftInput    = 0.0f;
    }

    
}






//   _   _          _       _           
//  | | | | _ __  | |_  __| | ___  ___ 
//  | | | || '_ \ | __|/ _` |/ _ \/ __|
//  | |_| || |_) || |_| (_| |  __/\__ \
//   \___/ | .__/  \__|\__,_|\___||___/
//         |_|                         





// =====================================================================================
// Update
// =====================================================================================
void SpaceState::update(float dt)
{

    m_signalReceiver.update(
        dt,
        m_ship.position,
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

        if (result.semanticState == SignalSemanticState::Decoded)
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




    
    // -------------------------------------------------------------------------
    // Rotation physics
    // -------------------------------------------------------------------------
    m_ship.pitchRate += m_ship.pitchInput * m_params.angularAccel * dt;
    m_ship.yawRate   += m_ship.yawInput   * m_params.angularAccel * dt;
    m_ship.rollRate  += m_ship.rollInput  * m_params.angularAccel * dt;

    m_ship.pitchRate = glm::clamp(m_ship.pitchRate, -m_params.maxPitchRate, m_params.maxPitchRate);
    m_ship.yawRate   = glm::clamp(m_ship.yawRate,   -m_params.maxYawRate,   m_params.maxYawRate);
    m_ship.rollRate  = glm::clamp(m_ship.rollRate,  -m_params.maxRollRate,  m_params.maxRollRate);

    m_ship.pitch += m_ship.pitchRate * dt;
    m_ship.yaw   += m_ship.yawRate   * dt;
    m_ship.roll  += m_ship.rollRate  * dt;

    float angDamp = std::exp(-m_params.angularDamping * dt);
    m_ship.pitchRate *= angDamp;
    m_ship.yawRate   *= angDamp;
    m_ship.rollRate  *= angDamp;

    // -------------------------------------------------------------------------
    // Orientation vectors
    // -------------------------------------------------------------------------
    glm::mat4 rot(1.0f);
    rot = glm::rotate(rot, glm::radians(m_ship.yaw),   {0,1,0});
    rot = glm::rotate(rot, glm::radians(m_ship.pitch),{1,0,0});
    rot = glm::rotate(rot, glm::radians(m_ship.roll), {0,0,1});

    glm::vec3 forward = glm::normalize(glm::vec3(rot * glm::vec4(0,0,-1,0)));
    glm::vec3 right   = glm::normalize(glm::vec3(rot * glm::vec4(1,0,0,0)));
    glm::vec3 up      = glm::normalize(glm::vec3(rot * glm::vec4(0,1,0,0)));

    // -------------------------------------------------------------------------
    // Manoeuvre thrusters physics
    // -------------------------------------------------------------------------
    m_ship.localVelocity.x += m_ship.strafeInput  * m_params.strafeAccel * dt;
    m_ship.localVelocity.y += m_ship.liftInput    * m_params.strafeAccel * dt;
    m_ship.localVelocity.z += m_ship.forwardInput * m_params.strafeAccel * dt;

    m_ship.localVelocity = glm::clamp(
        m_ship.localVelocity,
        glm::vec3(-m_params.maxStrafeSpeed),
        glm::vec3( m_params.maxStrafeSpeed)
    );

    float strafeDamp = std::exp(-m_params.strafeDamping * dt);
    m_ship.localVelocity *= strafeDamp;

    // -------------------------------------------------------------------------
    // Linear speed control (NOT used in cruise)
    // -------------------------------------------------------------------------
    if (!m_ship.cruiseActive)
    {
        const float targetSpeedAccel = 200.0f;

        m_ship.targetSpeed += m_ship.targetSpeedRate * targetSpeedAccel * dt;
        m_ship.targetSpeed = glm::clamp(m_ship.targetSpeed, 0.0f, m_params.maxCombatSpeed);

        float speedError  = m_ship.targetSpeed - m_ship.forwardVelocity;
        float engineAccel = speedError * m_params.throttleAccel;
        float drag        = -m_ship.forwardVelocity * m_world.linearDrag;

        m_ship.forwardVelocity += (engineAccel + drag) * dt;
        m_ship.forwardVelocity  = glm::max(m_ship.forwardVelocity, 0.0f);
    }

    // -------------------------------------------------------------------------
    // Position integration
    // -------------------------------------------------------------------------
    if (m_ship.cruiseActive)
    {
        m_ship.position += forward * m_params.maxCruiseSpeed * dt;
    }
    else
    {
        glm::vec3 worldVel =
            forward * m_ship.forwardVelocity +
            right   * m_ship.localVelocity.x +
            up      * m_ship.localVelocity.y;

        m_ship.position += worldVel * dt;
    }

    // -------------------------------------------------------------------------
    // Camera
    // -------------------------------------------------------------------------
    float leanRoll  = 0.0f;
    float leanPitch = 0.0f;

    if (!m_ship.cruiseActive)
    {
        leanRoll  = -m_ship.localVelocity.x * 0.05f;
        leanPitch =  m_ship.localVelocity.z * 0.03f;
    }

    m_camera.setVisualLean(
        glm::clamp(leanRoll,  -3.0f, 3.0f),
        glm::clamp(leanPitch, -2.0f, 2.0f)
    );

    m_camera.setPosition(m_ship.position);
    m_camera.setOrientation(m_ship.pitch, m_ship.yaw, m_ship.roll);
}



// =====================================================================================
// Render
// =====================================================================================
void SpaceState::render(){}

void SpaceState::renderUI()
{
    
    glm::mat4 projection = glm::perspective(
        glm::radians(70.0f),
        1280.0f / 720.0f,
        0.1f,
        5000.0f
    );

    glm::mat4 view = m_camera.viewMatrix();

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(glm::value_ptr(projection));
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(glm::value_ptr(view));

    DebugGrid::drawInfinite(m_ship.position, 200.0f, 100);
}



// =====================================================================================
// Render HUD
// =====================================================================================
void SpaceState::renderHUD()
{

    // -------------------------------------------------
    // сразу ставим ортографию и больше её не трогаем - это для 2D графики
    // -------------------------------------------------
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1280, 720, 0, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // -------------------------------------------------
    // 1. Статический HUD (рамки, текст)
    // -------------------------------------------------
    m_hudStatics.clear();
    m_hudRects.clear();

    m_hudRects.push_back({
        { 20.0f / 1280.0f, 20.0f / 720.0f },
        { 180.0f / 1280.0f, 60.0f / 720.0f },
        { 0.0f, 1.0f, 0.0f }
    });

    m_hudStatics.push_back({
        "HUD ONLINE",
        { 30.0f / 1280.0f, 40.0f / 720.0f },
        { 0.0f, 1.0f, 0.0f }
    });

    m_hudRenderer.renderRects(m_hudRects);
    m_hudRenderer.renderText(m_hudStatics);

    // -------------------------------------------------
    // 2. Подготовка матриц для меток
    // -------------------------------------------------

    // --- camera matrices нужны ТОЛЬКО здесь ---
    glm::mat4 view = m_camera.viewMatrix();
    glm::mat4 proj = m_camera.projectionMatrix();

    for (auto& [signal, lbl] : m_worldLabels)
    {
        lbl.onScreen = projectToScreen(
            lbl.data.worldPos,
            view,
            proj,
            TextRenderer::instance().viewportWidth(),
            TextRenderer::instance().viewportHeight(),
            lbl.screenPos
        );

    
        if (!lbl.onScreen)
                continue;

        m_worldLabelRenderer.renderHUD(lbl);
    }
    
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
