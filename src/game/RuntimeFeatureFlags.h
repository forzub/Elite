#pragma once

namespace game::runtime
{
// Legacy all-ships perception radar remains disabled. The player radar slot is
// temporarily backed by TEST_IDEAL_RADAR, whose server-side device publishes
// discrete session-private RadarScanReport data without using this old path.
inline constexpr bool RadarSimulationEnabled = false;

// The test sensor intentionally has no radar HUD/presentation.
inline constexpr bool RadarHudEnabled = false;

// Activation diagnostics remain enabled while the staged runtime execution
// policy is being validated against real scenes.
inline constexpr bool ActivationShadowDiagnosticsEnabled = true;

// Stage 4A/4B: planned activation mode controls materialized runtime work.
// Stage 4B decimates expensive motion-control evaluation for Prewarm/Coarse
// ships while cheap authoritative kinematic propagation remains on every fixed
// tick. Stage M7 separately decimates per-session ship transport rows; signals,
// objects/hubs and the authoritative source snapshot keep their existing cadence.
inline constexpr bool ActivationNpcAiCadenceEnabled = true;
inline constexpr bool ActivationShipMotionControlCadenceEnabled = true;
inline constexpr bool ActivationShipSystemsCadenceEnabled = true;
inline constexpr bool ActivationShipMaintenanceCadenceEnabled = true;
}
