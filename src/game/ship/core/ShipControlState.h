#pragma once
#include <cstdint>

#include "src/game/navigation/LocalFlightControlLaw.h"

struct ShipControlState
{
    bool cruiseActive = false;
    bool jumpActive   = false;

    float pitchInput = 0.0f;
    float yawInput   = 0.0f;
    float rollInput  = 0.0f;

    // +/- meaning depends on localControlLaw:
    // Newtonian -> signed main-engine thrust command.
    // Assisted  -> signed target-VREL change command.
    float targetSpeedRate = 0.0f;

    float strafeInput  = 0.0f;
    float liftInput    = 0.0f;
    float forwardInput = 0.0f;

    // Ctrl+F10 sends an explicit requested mode instead of a non-idempotent
    // toggle. That keeps server replay and fractional presentation prediction
    // deterministic even when the same input sample is evaluated twice.
    bool localControlLawCommandValid = false;
    game::navigation::LocalFlightControlLaw requestedLocalControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;

    // HOME / INSERT / END. The mode persists in DynamicMotionState after the
    // one-frame command so alignment can finish at bounded angular rates.
    game::navigation::VelocityAlignmentMode velocityAlignmentCommand =
        game::navigation::VelocityAlignmentMode::None;

    std::uint64_t controlTick = 0;
};
