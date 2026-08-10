#pragma once

namespace game::runtime
{
// Radar simulation is temporarily disabled until the perception pipeline is
// rebuilt around scan intervals and spatial broad-phase queries.
inline constexpr bool RadarSimulationEnabled = false;

// Keep the HUD side disabled with the server side so no radar render targets,
// shaders, or contact buffers are created while the subsystem is offline.
inline constexpr bool RadarHudEnabled = false;

// Activation diagnostics remain enabled while the staged runtime execution
// policy is being validated against real scenes.
inline constexpr bool ActivationShadowDiagnosticsEnabled = true;

// Stage 3E gates only NPC tactical AI think cadence. Physics, controls,
// HubTactical integration, signals and snapshots still run exactly as before.
// This is deliberately reversible while coarse/scheduled motion is not yet
// implemented.
inline constexpr bool ActivationNpcAiCadenceEnabled = true;
}
