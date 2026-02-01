#pragma once

#include "render/HUD/WorldLabel.h"

class WorldLabelSystem
{
public:
    static void updateVisualState(
        float dt,
        SignalSemanticState semanticState,
        float snr,
        WorldLabelVisualState& visual
    );
};
