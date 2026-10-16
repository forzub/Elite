#include "src/game/ship/controller/PlayerInputMapper.h"

#include "src/input/Input.h"
#include "src/game/navigation/LocalFlightControlStateMachine.h"

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

void PlayerInputMapper::update(
    ShipControlState& control,
    game::navigation::LocalFlightControlLaw currentLocalControlLaw
)
{
    const RuntimePlayerInputKeyState keys;
    updateFromKeyState(control, keys, currentLocalControlLaw);
}

void PlayerInputMapper::updateFromKeyState(
    ShipControlState& control,
    const IPlayerInputKeyState& keys,
    game::navigation::LocalFlightControlLaw currentLocalControlLaw
)
{
    auto& ctrl = control;
    ctrl = ShipControlState{};

    const bool ctrlDown =
        keys.isKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
        keys.isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

    // Modified F-keys are application-level discrete commands. Ctrl+F9/F10
    // now mutate the authoritative PhysicalControlModelId through
    // ClientShipCommand; ordinary ship input must not maintain a second
    // law-only toggle path.

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
            if (currentLocalControlLaw ==
                    game::navigation::LocalFlightControlLaw::Assisted)
            {
                ctrl.assistedMaxSpeedCommand = true;
            }
            else
            {
                ctrl.velocityAlignmentCommand =
                    game::navigation::VelocityAlignmentMode::ForwardToVelocity;
            }
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
