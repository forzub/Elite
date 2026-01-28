#pragma once

struct SignalDebugInfo
{
    float distance;
    float receivedPower;
    float interference;
    float totalNoise;
    float snr;
    int semanticState;
    bool occluded;
};