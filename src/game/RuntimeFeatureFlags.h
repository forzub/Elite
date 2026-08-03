#pragma once

namespace game::runtime
{
// Radar simulation is temporarily disabled until the perception pipeline is
// rebuilt around scan intervals and spatial broad-phase queries.
inline constexpr bool RadarSimulationEnabled = false;

// Keep the HUD side disabled with the server side so no radar render targets,
// shaders, or contact buffers are created while the subsystem is offline.
inline constexpr bool RadarHudEnabled = false;
}
