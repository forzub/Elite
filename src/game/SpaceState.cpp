#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "SpaceState.h"
#include "core/StateStack.h"
#include "input/Input.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/DebugGrid.h"
#include "game/ShipParams.h"

#include <iostream>

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
    m_ship.position = {0.0f, 5.0f, 10.0f};
    m_camera.setPosition(m_ship.position);

    // world = vacuum
    m_world.linearDrag   = 0.0f;
    m_world.maxSafeDecel = 50.0f;
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

    if (Input::instance().isKeyPressed(GLFW_KEY_ESCAPE))
        m_states.pop();
}

// =====================================================================================
// Update
// =====================================================================================
void SpaceState::update(float dt)
{
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
void SpaceState::render()
{
    glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
