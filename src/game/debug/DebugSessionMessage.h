#pragma once

#include <cstdint>
#include <string>

#include "src/scene/EntityID.h"

namespace game::debug
{
enum class DebugCommandType
{
    RefreshSnapshot,
    RefreshStructureSnapshot,
    DestroyShipModule,
    RestoreShipModule,
    ResetShipStructure,
    ResetAllShipStructures,
    DetachShipModule,
    HangShipModule,
    ReevaluateShipStructure,
    SetShipStructuralLinkHealth,
    SetUniverseTimeSimulation
};

struct DebugCommand
{
    DebugCommandType type = DebugCommandType::RefreshSnapshot;
    EntityId shipId {0};
    std::string itemId;
    float health = 0.0f;
    bool destroyed = false;
    bool enabled = false;
    double timeScale = 1.0;
};

struct DebugSessionState
{
    bool fastUniverseTime = false;
    bool universeTimeSimulation = false;
    double universeTimeScale = 1.0;
    double configuredUniverseTimeScale = 10000.0;
};
}
