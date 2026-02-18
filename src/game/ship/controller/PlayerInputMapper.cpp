#include "src/game/ship/controller/PlayerInputMapper.h"

#include "src/input/Input.h"



void PlayerInputMapper::update(ShipControlState& control){


    auto& ctrl = control;
    ctrl = ShipControlState{};

    ctrl.cruiseActive = Input::instance().isKeyPressed(GLFW_KEY_J);

    // --- Rotation (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.pitchInput =
            (Input::instance().isKeyPressed(GLFW_KEY_S) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_W) ? 1.0f : 0.0f);

        ctrl.rollInput =
            (Input::instance().isKeyPressed(GLFW_KEY_D) ? 1.0f : 0.0f) -
            (Input::instance().isKeyPressed(GLFW_KEY_A) ? 1.0f : 0.0f);

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