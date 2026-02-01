#pragma once

#include "render/HUD/WorldLabel.h"

// Обновляет состояние волн WorldLabel
// - двигает существующие волны
// - порождает новые (если разрешено)
// - НЕ занимается рендером
// - НЕ трогает presence FSM

void updateWorldSignalWaves(
    WorldLabelVisualState& visual,
    float dt,
    bool allowSpawn
);
