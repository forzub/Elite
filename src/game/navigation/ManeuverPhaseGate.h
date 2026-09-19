#pragma once

#include <cstdint>

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"

namespace game::navigation
{

// B9/B10 compound-program handoff gate.
//
// This component does not sample, track, plan, or mutate a maneuver program.
// It owns only the semantic decision whether the current physical phase may
// hand control to the next phase.
//
// ScheduledMoving:
//   a continuous moving reference phase advances when its nominal program
//   horizon ends.
//
// StateCapture:
//   after the nominal horizon, execution keeps tracking the terminal sample
//   until TrajectoryFollower reports the real terminal P/V/attitude/omega
//   envelope captured. A bounded overrun prevents a failed capture from
//   hanging navigation forever.
class ManeuverPhaseGate final
{
public:
    enum class Mode : std::uint8_t
    {
        ScheduledMoving = 0,
        StateCapture
    };

    enum class Status : std::uint8_t
    {
        InvalidInput = 0,
        Continue,
        Advance,
        CaptureTimedOut
    };

    struct Policy
    {
        Mode mode = Mode::StateCapture;

        // Only used by StateCapture. Time after the nominal program end during
        // which the terminal sample may continue to be tracked.
        double maximumCaptureOverrunSeconds = 6.0;
    };

    struct Result
    {
        Status status = Status::InvalidInput;

        double nominalEndUniverseTimeSeconds = 0.0;
        double captureOverrunSeconds = 0.0;
        bool nominalEndReached = false;
    };

    [[nodiscard]] static Result evaluate(
        const AcceptedManeuverProgram& program,
        double universeTimeSeconds,
        TrajectoryFollower::Status followerStatus,
        const Policy& policy
    ) noexcept;
};

} // namespace game::navigation
