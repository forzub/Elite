#pragma once

struct ShipControlState
{
    bool cruiseActive = false;
    bool jumpActive   = false;

    float pitchInput = 0.0f;
    float yawInput   = 0.0f;
    float rollInput  = 0.0f;

    float targetSpeedRate = 0.0f;

    float strafeInput  = 0.0f;
    float liftInput    = 0.0f;
    float forwardInput = 0.0f;
};
