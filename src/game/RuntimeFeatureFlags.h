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

// Stage 4A/4B: planned activation mode controls materialized runtime work.
// Stage 4B decimates expensive motion-control evaluation for Prewarm/Coarse
// ships while cheap authoritative kinematic propagation remains on every fixed
// tick. Signals and entity presence in replication remain full-rate/full-presence
// until sparse replication/materialization semantics are introduced explicitly.
inline constexpr bool ActivationNpcAiCadenceEnabled = true;
inline constexpr bool ActivationShipMotionControlCadenceEnabled = true;
inline constexpr bool ActivationShipSystemsCadenceEnabled = true;
inline constexpr bool ActivationShipMaintenanceCadenceEnabled = true;
}
