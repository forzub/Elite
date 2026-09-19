#include "ManeuverProgramSampler.h"

#include <algorithm>
#include <cmath>

namespace game::navigation
{
namespace
{

using Program = AcceptedManeuverProgram;
using Sample = Program::ReferenceSample;

constexpr double kEpsilon = 1.0e-12;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

bool nonNegativeFinite(double value) noexcept
{
    return finite(value) && value >= 0.0;
}

bool validSample(const Sample& sample) noexcept
{
    return
        finite(sample.timeOffsetSeconds) &&
        sample.timeOffsetSeconds >= 0.0 &&
        finite(sample.positionMapMeters) &&
        finite(sample.velocityMapMetersPerSecond) &&
        finite(sample.linearAccelerationFeedForwardMapMps2) &&
        finite(sample.forwardMap) &&
        finite(sample.rightMap) &&
        finite(sample.upMap) &&
        finite(sample.angularVelocityMapRadPerSecond) &&
        finite(sample.angularAccelerationFeedForwardMapRadPerSec2);
}

bool validProgram(const Program& program) noexcept
{
    if (!program.valid ||
        program.revision == 0 ||
        !finite(program.acceptedAtUniverseTimeSeconds) ||
        !finite(program.validUntilUniverseTimeSeconds) ||
        program.validUntilUniverseTimeSeconds <
            program.acceptedAtUniverseTimeSeconds ||
        program.sampleCount == 0 ||
        program.sampleCount > Program::kMaxSamples ||
        !nonNegativeFinite(program.terminalTolerance.positionMeters) ||
        !nonNegativeFinite(program.terminalTolerance.linearVelocityMps) ||
        !nonNegativeFinite(program.terminalTolerance.forwardAngleRad) ||
        !nonNegativeFinite(program.terminalTolerance.angularVelocityRadPerSec) ||
        !nonNegativeFinite(program.tracking.positionErrorMeters) ||
        !nonNegativeFinite(program.tracking.linearVelocityErrorMps) ||
        !nonNegativeFinite(program.tracking.forwardAngleErrorRad) ||
        !nonNegativeFinite(program.tracking.angularVelocityErrorRadPerSec) ||
        !nonNegativeFinite(program.tracking.linearFeedbackReserveMps2) ||
        !nonNegativeFinite(program.tracking.angularFeedbackReserveRadPerSec2) ||
        !finite(program.hazardUrgency01))
    {
        return false;
    }

    double previousTime = -1.0;
    for (std::size_t i = 0; i < program.sampleCount; ++i)
    {
        const Sample& sample = program.samples[i];
        if (!validSample(sample) ||
            sample.timeOffsetSeconds <= previousTime)
        {
            return false;
        }
        previousTime = sample.timeOffsetSeconds;
    }

    // Programs are anchored at acceptance. Requiring t=0 makes program age and
    // invalidation deterministic and prevents hidden pre-history.
    if (std::abs(program.samples[0].timeOffsetSeconds) > kEpsilon)
        return false;

    const double lastAbsoluteTime =
        program.acceptedAtUniverseTimeSeconds +
        program.samples[program.sampleCount - 1].timeOffsetSeconds;

    return
        finite(lastAbsoluteTime) &&
        lastAbsoluteTime <= program.validUntilUniverseTimeSeconds + kEpsilon;
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double lengthSquared = glm::dot(value, value);
    if (!finite(lengthSquared) || lengthSquared <= kEpsilon)
        return fallback;
    return value / std::sqrt(lengthSquared);
}

glm::dvec3 lerp(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double alpha
) noexcept
{
    return a + (b - a) * alpha;
}

Sample interpolate(
    const Sample& a,
    const Sample& b,
    double alpha,
    double elapsed
) noexcept
{
    Sample out;
    out.timeOffsetSeconds = elapsed;
    out.positionMapMeters =
        lerp(a.positionMapMeters, b.positionMapMeters, alpha);
    out.velocityMapMetersPerSecond =
        lerp(a.velocityMapMetersPerSecond,
             b.velocityMapMetersPerSecond,
             alpha);
    out.linearAccelerationFeedForwardMapMps2 =
        lerp(a.linearAccelerationFeedForwardMapMps2,
             b.linearAccelerationFeedForwardMapMps2,
             alpha);

    glm::dvec3 forward = normalizedOr(
        lerp(a.forwardMap, b.forwardMap, alpha),
        normalizedOr(a.forwardMap, glm::dvec3(0.0, 0.0, -1.0))
    );

    glm::dvec3 upCandidate =
        lerp(a.upMap, b.upMap, alpha);
    upCandidate -= forward * glm::dot(upCandidate, forward);
    glm::dvec3 up = normalizedOr(
        upCandidate,
        normalizedOr(a.upMap, glm::dvec3(0.0, 1.0, 0.0))
    );

    glm::dvec3 right = normalizedOr(
        glm::cross(forward, up),
        normalizedOr(a.rightMap, glm::dvec3(1.0, 0.0, 0.0))
    );
    up = normalizedOr(
        glm::cross(right, forward),
        up
    );

    out.forwardMap = forward;
    out.rightMap = right;
    out.upMap = up;

    out.angularVelocityMapRadPerSecond =
        lerp(a.angularVelocityMapRadPerSecond,
             b.angularVelocityMapRadPerSecond,
             alpha);
    out.angularAccelerationFeedForwardMapRadPerSec2 =
        lerp(a.angularAccelerationFeedForwardMapRadPerSec2,
             b.angularAccelerationFeedForwardMapRadPerSec2,
             alpha);
    return out;
}

} // namespace

ManeuverProgramSampler::Result ManeuverProgramSampler::sample(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds
) noexcept
{
    Result result;
    if (!validProgram(program) || !finite(universeTimeSeconds))
        return result;

    const std::size_t lastIndex =
        static_cast<std::size_t>(program.sampleCount - 1);

    result.elapsedSeconds =
        universeTimeSeconds - program.acceptedAtUniverseTimeSeconds;

    if (result.elapsedSeconds <= 0.0)
    {
        result.status =
            result.elapsedSeconds < 0.0
                ? Status::BeforeStart
                : Status::Active;
        result.reference = program.samples[0];
        return result;
    }

    const double lastOffset =
        program.samples[lastIndex].timeOffsetSeconds;
    if (result.elapsedSeconds >= lastOffset)
    {
        result.status =
            result.elapsedSeconds > lastOffset
                ? Status::AfterEnd
                : Status::Active;
        result.reference = program.samples[lastIndex];
        result.lowerSampleIndex = lastIndex;
        result.upperSampleIndex = lastIndex;
        result.interpolation01 = 1.0;
        return result;
    }

    std::size_t upper = 1;
    while (upper < program.sampleCount &&
           program.samples[upper].timeOffsetSeconds <
               result.elapsedSeconds)
    {
        ++upper;
    }

    const std::size_t lower = upper - 1;
    const double lowerTime =
        program.samples[lower].timeOffsetSeconds;
    const double upperTime =
        program.samples[upper].timeOffsetSeconds;
    const double duration = upperTime - lowerTime;
    if (!finite(duration) || duration <= kEpsilon)
        return Result {};

    const double alpha = std::clamp(
        (result.elapsedSeconds - lowerTime) / duration,
        0.0,
        1.0
    );

    result.status = Status::Active;
    result.lowerSampleIndex = lower;
    result.upperSampleIndex = upper;
    result.interpolation01 = alpha;
    result.reference = interpolate(
        program.samples[lower],
        program.samples[upper],
        alpha,
        result.elapsedSeconds
    );
    return result;
}

} // namespace game::navigation
