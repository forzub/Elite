#include "ManeuverProgramSampler.h"

#include "src/game/navigation/ManeuverProgramTimeline.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
        !nonNegativeFinite(program.sequenceStartOffsetSeconds) ||
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

    const auto pageWindow =
        ManeuverProgramTimeline::pageWindow(program);

    return
        pageWindow.valid &&
        pageWindow.endUniverseTimeSeconds <=
            program.validUntilUniverseTimeSeconds + kEpsilon;
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
        ManeuverProgramTimeline::elapsedPageSeconds(
            program,
            universeTimeSeconds
        );

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

ManeuverProgramSampler::Result ManeuverProgramSampler::sampleSpatial(
    const AcceptedManeuverProgram& program,
    double universeTimeSeconds,
    const glm::dvec3& positionMapMeters,
    std::size_t minimumSegmentIndex
) noexcept
{
    Result result;
    if (!validProgram(program) ||
        !finite(universeTimeSeconds) ||
        !finite(positionMapMeters) ||
        program.sampleCount < 2)
    {
        return result;
    }

    const auto window =
        ManeuverProgramTimeline::pageWindow(program);
    if (!window.valid)
        return result;

    // Spatial storage pages share one accepted maneuver epoch. Their local
    // nominal page start is metadata only; physical progress may legitimately
    // cross a storage boundary earlier or later than nominal time.
    if (universeTimeSeconds <
        program.acceptedAtUniverseTimeSeconds)
    {
        result.status = Status::BeforeStart;
        result.reference = program.samples[0];
        return result;
    }

    const std::size_t lastSegment =
        static_cast<std::size_t>(program.sampleCount - 2);
    const std::size_t firstSegment =
        std::min(minimumSegmentIndex, lastSegment);

    double bestDistance2 =
        std::numeric_limits<double>::infinity();
    std::size_t bestLower = firstSegment;
    double bestAlpha = 0.0;

    for (std::size_t lower = firstSegment;
         lower <= lastSegment;
         ++lower)
    {
        const auto& a = program.samples[lower];
        const auto& b = program.samples[lower + 1];
        const glm::dvec3 segment =
            b.positionMapMeters - a.positionMapMeters;
        const double segmentLength2 =
            glm::dot(segment, segment);

        double alpha = 0.0;
        if (finite(segmentLength2) &&
            segmentLength2 > kEpsilon)
        {
            alpha = std::clamp(
                glm::dot(
                    positionMapMeters - a.positionMapMeters,
                    segment
                ) / segmentLength2,
                0.0,
                1.0
            );
        }

        const glm::dvec3 projected =
            a.positionMapMeters + alpha * segment;
        const glm::dvec3 delta =
            positionMapMeters - projected;
        const double distance2 = glm::dot(delta, delta);
        if (!finite(distance2))
            return Result {};

        // Equal-distance joins belong to the later segment. Combined with the
        // caller's monotonic minimumSegmentIndex this prevents path progress
        // from bouncing backwards at sample/page boundaries.
        if (distance2 < bestDistance2 - kEpsilon ||
            (std::abs(distance2 - bestDistance2) <= kEpsilon &&
             lower > bestLower))
        {
            bestDistance2 = distance2;
            bestLower = lower;
            bestAlpha = alpha;
        }
    }

    const std::size_t bestUpper = bestLower + 1;
    const double lowerTime =
        program.samples[bestLower].timeOffsetSeconds;
    const double upperTime =
        program.samples[bestUpper].timeOffsetSeconds;
    const double sampledOffset =
        lowerTime + (upperTime - lowerTime) * bestAlpha;

    result.status = Status::Active;
    result.elapsedSeconds = sampledOffset;
    result.lowerSampleIndex = bestLower;
    result.upperSampleIndex = bestUpper;
    result.interpolation01 = bestAlpha;
    result.spatialReference = true;
    result.spatialDistanceMeters =
        std::sqrt(std::max(0.0, bestDistance2));
    result.reference = interpolate(
        program.samples[bestLower],
        program.samples[bestUpper],
        bestAlpha,
        sampledOffset
    );
    return result;
}

} // namespace game::navigation
