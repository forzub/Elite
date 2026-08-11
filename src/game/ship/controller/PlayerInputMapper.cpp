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
)
{
    auto& ctrl = control;
    ctrl = ShipControlState{};

    const bool ctrlDown =
        keys.isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
        keys.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

    // Ctrl+F10 switches the *local* flight law. Send the requested state as an
    // idempotent command: prediction/replay may evaluate one control sample
    // more than once, but setting a mode twice is harmless.
    const bool ctrlF10Down =
        ctrlDown && keys.isKeyPressed(GLFW_KEY_F10);

    if (!ctrlF10Down)
    {
        m_ctrlF10Latch = false;
    }
    else if (!m_ctrlF10Latch)
    {
        m_ctrlF10Latch = true;
        m_requestedLocalControlLaw =
            m_requestedLocalControlLaw ==
                    game::navigation::LocalFlightControlLaw::Newtonian
                ? game::navigation::LocalFlightControlLaw::Assisted
                : game::navigation::LocalFlightControlLaw::Newtonian;

        ctrl.localControlLawCommandValid = true;
        ctrl.requestedLocalControlLaw = m_requestedLocalControlLaw;
    }

    ctrl.cruiseActive = keys.isKeyPressed(GLFW_KEY_J);

    // --- Rotation (disabled in J/cruise placeholder) ---
    if (!ctrl.cruiseActive)
    {
        ctrl.pitchInput =
            (keys.isKeyPressed(GLFW_KEY_S) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_W) ? 1.0f : 0.0f);

        ctrl.rollInput =
            (keys.isKeyPressed(GLFW_KEY_D) ? 1.0f : 0.0f) -
            (keys.isKeyPressed(GLFW_KEY_A) ? 1.0f : 0.0f);

        ctrl.yawInput =
            (!ctrlDown && keys.isKeyPressed(GLFW_KEY_Q) ? 1.0f : 0.0f) -
            (!ctrlDown && keys.isKeyPressed(GLFW_KEY_E) ? 1.0f : 0.0f);
    }

    // +/- is deliberately a generic longitudinal command. DynamicMotionSystem
    // interprets '+' as main-engine thrust in Newtonian mode; '-' is ignored
    // there because braking requires turning the craft and using the same main
    // engine. Assisted mode keeps +/- as a held target-speed trim; releasing
    // the key freezes the setpoint at the speed actually reached.
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

    // --- Manoeuvre thrusters (disabled in J/cruise placeholder) ---
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

        if (keys.isKeyPressed(GLFW_KEY_HOME))
        {
            ctrl.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::ForwardToVelocity;
        }
        else if (keys.isKeyPressed(GLFW_KEY_INSERT))
        {
            ctrl.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::BackwardToVelocity;
        }
        else if (keys.isKeyPressed(GLFW_KEY_END))
        {
            ctrl.velocityAlignmentCommand =
                game::navigation::VelocityAlignmentMode::BrakeToStop;
        }
    }
}
