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

    // Ctrl+F10 switches the *local* flight law on F10 release, not on press.
    // This makes the chord deliberate and prevents a held key from feeling
    // like a hair trigger. A short release debounce also filters a transient
    // up/down sample before the command is committed. Ctrl is required only
    // to arm the chord; once armed, releasing Ctrl before F10 does not cancel
    // the intended switch.
    const bool f10Down = keys.isKeyPressed(GLFW_KEY_F10);

    switch (m_ctrlF10State)
    {
        case CtrlF10State::Idle:
            if (ctrlDown && f10Down)
            {
                m_ctrlF10State = CtrlF10State::Pressed;
                m_ctrlF10ReleaseSamples = 0;
            }
            break;

        case CtrlF10State::Pressed:
            if (!f10Down)
            {
                m_ctrlF10State = CtrlF10State::ReleaseDebounce;
                m_ctrlF10ReleaseSamples = 1;
            }
            break;

        case CtrlF10State::ReleaseDebounce:
            if (f10Down)
            {
                // A brief return to DOWN is treated as release bounce. Keep
                // the already-armed chord, but restart release qualification.
                m_ctrlF10State = CtrlF10State::Pressed;
                m_ctrlF10ReleaseSamples = 0;
                break;
            }

            ++m_ctrlF10ReleaseSamples;
            if (m_ctrlF10ReleaseSamples >= kCtrlF10ReleaseDebounceSamples)
            {
                ctrl.localControlLawCommandValid = true;
                ctrl.requestedLocalControlLaw =
                    currentLocalControlLaw ==
                            game::navigation::LocalFlightControlLaw::Newtonian
                        ? game::navigation::LocalFlightControlLaw::Assisted
                        : game::navigation::LocalFlightControlLaw::Newtonian;

                m_ctrlF10State = CtrlF10State::Idle;
                m_ctrlF10ReleaseSamples = 0;
            }
            break;
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
