#include "ManeuverPhaseGate.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool validProgramEnvelope(
    const AcceptedManeuverProgram& program
) noexcept
{
    if (!program.valid ||
        program.revision == 0 ||
        program.sampleCount == 0 ||
        program.sampleCount > AcceptedManeuverProgram::kMaxSamples ||
        !finite(program.acceptedAtUniverseTimeSeconds) ||
        !finite(program.sequenceStartOffsetSeconds) ||
        program.sequenceStartOffsetSeconds < 0.0)
    {
        return false;
    }

    const auto& last =
        program.samples[
            static_cast<std::size_t>(program.sampleCount - 1)
        ];

    return
        finite(last.timeOffsetSeconds) &&
        last.timeOffsetSeconds >= 0.0;
}

} // namespace

ManeuverPhaseGate::Result ManeuverPhaseGate::evaluate(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds,
    TrajectoryFollower::Status followerStatus,
    const Policy& policy
) noexcept
{
    Result result;

    if (!validProgramEnvelope(program) ||
        !finite(universeTimeSeconds) ||
        !finite(policy.maximumCaptureOverrunSeconds) ||
        policy.maximumCaptureOverrunSeconds < 0.0 ||
        followerStatus == TrajectoryFollower::Status::InvalidInput)
    {
        return result;
    }

    const auto& last =
        program.samples[
            static_cast<std::size_t>(program.sampleCount - 1)
        ];

    result.nominalEndUniverseTimeSeconds =
        program.acceptedAtUniverseTimeSeconds +
        program.sequenceStartOffsetSeconds +
        last.timeOffsetSeconds;

    if (!finite(result.nominalEndUniverseTimeSeconds))
        return Result {};

    result.captureOverrunSeconds =
        std::max(
            0.0,
            universeTimeSeconds -
                result.nominalEndUniverseTimeSeconds
        );

    result.nominalEndReached =
        universeTimeSeconds >=
        result.nominalEndUniverseTimeSeconds;

    if (!result.nominalEndReached)
    {
        result.status = Status::Continue;
        return result;
    }

    if (policy.mode == Mode::ScheduledMoving)
    {
        result.status = Status::Advance;
        return result;
    }

    if (followerStatus == TrajectoryFollower::Status::Complete)
    {
        result.status = Status::Advance;
        return result;
    }

    result.status =
        result.captureOverrunSeconds >
            policy.maximumCaptureOverrunSeconds
            ? Status::CaptureTimedOut
            : Status::Continue;

    return result;
}

} // namespace game::navigation
