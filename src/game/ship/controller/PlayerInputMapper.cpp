#include "src/game/ship/controller/PlayerInputMapper.h"

#include "src/input/Input.h"

namespace
{
class RuntimePlayerInputKeyState final : public IPlayerInputKeyState
{
public:
    bool isKeyPressed(int key) const override
    {
        return Input::instance().isKeyPressed(key);
    }
};
}

void PlayerInputMapper::update(ShipControlState& control)
{
    const RuntimePlayerInputKeyState keys;
    updateFromKeyState(control, keys);
}

void PlayerInputMapper::updateFromKeyState(
    ShipControlState& control,
    const IPlayerInputKeyState& keys
) const
{
    auto& ctrl = control;
    ctrl = ShipControlState{};

    ctrl.cruiseActive = keys.isKeyPressed(GLFW_KEY_J);

    // --- Rotation (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.pitchInput =
            (keys.isKeyPressed(GLFW_KEY_S) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_W) ? 1.0f : 0.0f);

        ctrl.rollInput =
            (keys.isKeyPressed(GLFW_KEY_D) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_A) ? 1.0f : 0.0f);

        const bool ctrlDown =
            keys.isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
            keys.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

        ctrl.yawInput =
            (!ctrlDown && keys.isKeyPressed(GLFW_KEY_Q) ? 1.0f : 0.0f) -
            (!ctrlDown && keys.isKeyPressed(GLFW_KEY_E) ? 1.0f : 0.0f);
    }

    // --- Target speed control ---
    if (keys.isKeyPressed(GLFW_KEY_KP_ADD) ||
        keys.isKeyPressed(GLFW_KEY_EQUAL))
    {
        ctrl.targetSpeedRate = +1.0f;
    }

    if (keys.isKeyPressed(GLFW_KEY_KP_SUBTRACT) ||
        keys.isKeyPressed(GLFW_KEY_MINUS))
    {
        ctrl.targetSpeedRate = -1.0f;
    }

    // --- Manoeuvre thrusters (disabled in cruise) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.strafeInput =
            (keys.isKeyPressed(GLFW_KEY_KP_6) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_KP_4) ? 1.0f : 0.0f);

        ctrl.forwardInput =
            (keys.isKeyPressed(GLFW_KEY_KP_8) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_KP_2) ? 1.0f : 0.0f);

        ctrl.liftInput =
            (keys.isKeyPressed(GLFW_KEY_KP_9) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_KP_3) ? 1.0f : 0.0f);
    }
}
