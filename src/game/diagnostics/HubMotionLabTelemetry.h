#pragma once

#include <cstdint>

namespace game::diagnostics
{

// Diagnostic capture is intentionally tied to Hub Motion Lab. It must not
// become a production telemetry dependency of normal entity presentation.
inline constexpr bool HubMotionLabTelemetryCsvEnabled = true;
inline constexpr const char* HubMotionLabTelemetryCsvPath =
    "hub_motion_lab_presentation.csv";

struct HubMotionLabPresentationSample
{
    std::uint64_t frameIndex = 0;
    double frameDtMilliseconds = 0.0;

    double requestedRenderTimeSeconds = 0.0;
    double actualRenderTimeSeconds = 0.0;
    double oldestSnapshotTimeSeconds = 0.0;
    double newestSnapshotTimeSeconds = 0.0;
    double requestedMinusNewestMilliseconds = 0.0;

    bool hasSnapshots = false;
    bool clampedToOldest = false;
    bool clampedToNewest = false;
    bool hasInterpolationBracket = false;

    std::uint64_t olderServerTick = 0;
    std::uint64_t newerServerTick = 0;
    double interpolationAlpha = -1.0;

    // Error against the shared analytic server-time function for the two
    // scripted remote NPC probes. These values measure the presentation path,
    // not server motion generation.
    double slowNpcLocalErrorMeters = -1.0;
    double fastNpcLocalErrorMeters = -1.0;

    // MATCH is sampled on the delayed remote timeline while the local player
    // is predicted close to "server now". This first value therefore measures
    // the visible cross-timeline separation from the locally predicted player;
    // it is diagnostic, not an interpolation-accuracy error.
    double matchVsPredictedPlayerDistanceDeltaMeters = -1.0;

    // Compare MATCH against the player's authoritative state sampled at the
    // same delayed render epoch. This is the actual remote-interpolation error.
    double matchVsDelayedPlayerErrorMeters = -1.0;

    // Local-player presentation diagnostics. Fixed prediction advances at the
    // simulation rate; fractionalTarget is the presentation-only sample at the
    // accumulator remainder between two fixed ticks.
    double playerPredictionRemainderMilliseconds = -1.0;
    double playerFixedToFractionalTargetMeters = -1.0;
    double playerRenderToFractionalTargetMeters = -1.0;
    double playerRenderStepMeters = -1.0;
};

} // namespace game::diagnostics
