#pragma once

#include "src/game/presentation/EngineThrustHudPresentation.h"
#include "src/render/types/Viewport.h"
#include "src/game/simulation/ShipCoreStatus.h"

namespace render::cockpit
{

class EngineThrustIndicatorRenderer final
{
public:
    void renderThermalSystems(
        const game::ShipCoreStatus& status,
        const game::presentation::EngineThrustHudPresentation& engines,
        const Viewport& viewport
    );
    void render(
        const game::presentation::EngineThrustHudPresentation& presentation,
        const Viewport& viewport
    );
    // the engine group's frame on screen (px) as last drawn: the widgets above it anchor here (the cockpit radio)
    struct GroupFrame
    {
        bool valid = false;
        float left = 0.0f, right = 0.0f, top = 0.0f, scale = 1.0f;
    };
    const GroupFrame& groupFrame() const { return m_groupFrame; }

private:
    GroupFrame m_groupFrame;
};

} // namespace render::cockpit
