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
    // Newtonian -> '+' commands the main engine; '-' is intentionally ignored.
    //              Braking is performed by turning the hull and applying the
    //              same forward thrust (or by END autobrake).
    // Assisted  -> signed target-VREL change command.
    float targetSpeedRate = 0.0f;

    // One-shot Assisted command: set the persistent target VREL magnitude to
    // this ship's ordinary local maximum. HOME emits this in Assisted mode.
    // It is explicit instead of overloading targetSpeedRate with a magic value
    // so replay/network prediction retain deterministic command semantics.
    bool assistedMaxSpeedCommand = false;

    float strafeInput  = 0.0f;
    float liftInput    = 0.0f;
    float forwardInput = 0.0f;

    // Ctrl+F10 sends an explicit requested mode instead of a non-idempotent
    // toggle. That keeps server replay and fractional presentation prediction
    // deterministic even when the same input sample is evaluated twice.
    bool localControlLawCommandValid = false;
    game::navigation::LocalFlightControlLaw requestedLocalControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;

    // Newtonian HOME / INSERT and both-law END alignment commands. The mode
    // persists in DynamicMotionState after the one-frame command so alignment
    // can finish at bounded angular rates. Assisted HOME uses the explicit
    // max-speed command above instead.
    game::navigation::VelocityAlignmentMode velocityAlignmentCommand =
        game::navigation::VelocityAlignmentMode::None;

    std::uint64_t controlTick = 0;
};
