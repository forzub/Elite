#pragma once

#include "src/game/navigation/RoutePlan.h"
#include "src/game/navigation/TargetTrackingState.h"

namespace game::navigation
{

/*
    Client-owned navigation product state shared by maps, HUD and future
    trajectory/autopilot layers. Renderers edit/present this workspace but do
    not own it.
*/
class ClientNavigationWorkspace
{
public:
    TargetTrackingState& targets() noexcept { return m_targets; }
    const TargetTrackingState& targets() const noexcept { return m_targets; }

    RoutePlan& routePlan() noexcept { return m_routePlan; }
    const RoutePlan& routePlan() const noexcept { return m_routePlan; }

private:
    TargetTrackingState m_targets;
    RoutePlan m_routePlan;
};

} // namespace game::navigation
