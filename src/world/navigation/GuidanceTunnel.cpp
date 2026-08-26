#include "src/world/navigation/GuidanceTunnel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/gtc/constants.hpp>

#include "src/world/navigation/NavigationOrientation.h"
#include "src/world/navigation/SmoothPathOptimizer.h"

namespace world::navigation
{
namespace
{
constexpr double Epsilon = 1.0e-9;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite3(const glm::dvec3& value) noexcept
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}

double smoothStep01(double value) noexcept
{
    const double u = std::clamp(value, 0.0, 1.0);
    return u * u * (3.0 - 2.0 * u);
}

glm::dvec3 normalizedVectorOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
) noexcept
{
    const double n2 = glm::dot(value, value);
    if (!finite(n2) || n2 <= Epsilon)
        return fallback;
    return value / std::sqrt(n2);
}

glm::dquat normalizedQuatOr(
    const glm::dquat& value,
    const glm::dquat& fallback
) noexcept
{
    const double n2 = glm::dot(value, value);
    if (!finite(n2) || n2 <= Epsilon)
        return fallback;
    return glm::normalize(value);
}

struct TrajectoryPoint
{
    glm::dvec3 positionMeters {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    double sourceProgressMeters = 0.0;
    double speedMps = 0.0;
};

TrajectoryPoint sampleTrajectoryAtProgress(
    const Trajectory& trajectory,
    double progressMeters
)
{
    TrajectoryPoint out;
    if (trajectory.samples.empty())
        return out;

    const double clamped = std::clamp(
        progressMeters,
        trajectory.samples.front().pathProgressMeters,
        trajectory.samples.back().pathProgressMeters
    );
    std::size_t hi = 1;
    while (hi < trajectory.samples.size() &&
           trajectory.samples[hi].pathProgressMeters < clamped)
    {
        ++hi;
    }
    if (hi >= trajectory.samples.size())
    {
        const auto& last = trajectory.samples.back();
        return {
            last.positionMeters,
            last.orientation,
            last.sourcePathProgressMeters,
            last.speedMps
        };
    }

    const auto& a = trajectory.samples[hi - 1];
    const auto& b = trajectory.samples[hi];
    const double span = std::max(Epsilon,
        b.pathProgressMeters - a.pathProgressMeters);
    const double u = std::clamp(
        (clamped - a.pathProgressMeters) / span,
        0.0,
        1.0
    );
    out.positionMeters = a.positionMeters +
        (b.positionMeters - a.positionMeters) * u;
    glm::dquat qa = normalizedQuatOr(a.orientation, glm::dquat(1.0, 0.0, 0.0, 0.0));
    glm::dquat qb = normalizedQuatOr(b.orientation, qa);
    if (glm::dot(qa, qb) < 0.0)
        qb = -qb;
    out.orientation = glm::normalize(glm::slerp(qa, qb, u));
    out.sourceProgressMeters = a.sourcePathProgressMeters +
        (b.sourcePathProgressMeters - a.sourcePathProgressMeters) * u;
    out.speedMps = a.speedMps + (b.speedMps - a.speedMps) * u;
    return out;
}

double sampleSpeedAtSourceProgress(
    const Trajectory& trajectory,
    double sourceProgressMeters
)
{
    if (trajectory.samples.empty())
        return 0.0;
    std::size_t hi = 1;
    while (hi < trajectory.samples.size() &&
           trajectory.samples[hi].sourcePathProgressMeters < sourceProgressMeters)
    {
        ++hi;
    }
    if (hi >= trajectory.samples.size())
        return trajectory.samples.back().speedMps;
    const auto& a = trajectory.samples[hi - 1];
    const auto& b = trajectory.samples[hi];
    const double span = std::max(
        Epsilon,
        b.sourcePathProgressMeters - a.sourcePathProgressMeters
    );
    const double u = std::clamp(
        (sourceProgressMeters - a.sourcePathProgressMeters) / span,
        0.0,
        1.0
    );
    return a.speedMps + (b.speedMps - a.speedMps) * u;
}

double nearestTrajectoryProgress(
    const Trajectory& trajectory,
    const glm::dvec3& pointMeters
)
{
    double bestProgress = trajectory.samples.front().pathProgressMeters;
    double bestDistance2 = std::numeric_limits<double>::infinity();
    for (std::size_t i = 1; i < trajectory.samples.size(); ++i)
    {
        const auto& a = trajectory.samples[i - 1];
        const auto& b = trajectory.samples[i];
        const glm::dvec3 segment = b.positionMeters - a.positionMeters;
        const double length2 = glm::dot(segment, segment);
        const double u = length2 > Epsilon
            ? std::clamp(glm::dot(pointMeters - a.positionMeters, segment) / length2, 0.0, 1.0)
            : 0.0;
        const glm::dvec3 projected = a.positionMeters + segment * u;
        const glm::dvec3 delta = pointMeters - projected;
        const double distance2 = glm::dot(delta, delta);
        if (distance2 < bestDistance2)
        {
            bestDistance2 = distance2;
            bestProgress = a.pathProgressMeters +
                (b.pathProgressMeters - a.pathProgressMeters) * u;
        }
    }
    return bestProgress;
}

void appendControl(
    std::vector<glm::dvec3>& points,
    std::vector<double>& sourceProgress,
    const glm::dvec3& point,
    double source
)
{
    if (!points.empty() && glm::length(points.back() - point) <= 1.0e-6)
    {
        sourceProgress.back() = std::max(sourceProgress.back(), source);
        return;
    }
    points.push_back(point);
    sourceProgress.push_back(source);
}

struct DynamicCurve
{
    std::vector<SmoothPathPoint> points;
    double passedTrajectoryProgressMeters = 0.0;
    double maxCurvaturePerMeter = 0.0;
};

DynamicCurve buildTrajectoryBackboneCurve(
    const GuidanceTunnelRequest& request
)
{
    DynamicCurve out;
    const Trajectory& trajectory = *request.trajectory;
    if (trajectory.samples.size() < 2)
        return out;

    // A manual tunnel created by CALCULATE ROUTE is a presentation sampling
    // of the trajectory that has already passed geometric/swept-volume safety
    // validation. Do not run a second B-spline optimizer here: doing so can
    // cut across a newly-added obstacle (for example the station hull) and
    // incorrectly turn a valid trajectory into NO SAFE GUIDANCE SOLUTION.
    //
    // TrajectoryBackbone is used for the initial CALCULATE ROUTE publication.
    // Keep the complete accepted trajectory here.  Using the render-time ship
    // position to retire progress during this first build mixes a later
    // presentation sample into the planning-epoch backbone and can, on a
    // curved/looping path, select the terminal segment and collapse the tunnel
    // to a single point. Passed gates are retired by the rolling tracker after
    // publication instead.
    const double startProgress = trajectory.samples.front().pathProgressMeters;
    out.passedTrajectoryProgressMeters = startProgress;

    const TrajectoryPoint first = sampleTrajectoryAtProgress(
        trajectory,
        startProgress
    );
    out.points.push_back({
        first.positionMeters,
        first.sourceProgressMeters
    });

    for (const auto& sample : trajectory.samples)
    {
        if (sample.pathProgressMeters <= startProgress + Epsilon)
            continue;
        const SmoothPathPoint point {
            sample.positionMeters,
            sample.sourcePathProgressMeters
        };
        if (glm::length(
                out.points.back().positionMeters - point.positionMeters
            ) <= 1.0e-7)
        {
            out.points.back().sourceProgressMeters = std::max(
                out.points.back().sourceProgressMeters,
                point.sourceProgressMeters
            );
            continue;
        }
        out.points.push_back(point);
    }

    return out;
}

DynamicCurve buildDynamicCurve(const GuidanceTunnelRequest& request)
{
    DynamicCurve out;
    const Trajectory& trajectory = *request.trajectory;
    const double startProgress = nearestTrajectoryProgress(
        trajectory,
        request.currentPositionMeters
    );
    const double endProgress = trajectory.samples.back().pathProgressMeters;
    out.passedTrajectoryProgressMeters = startProgress;

    const TrajectoryPoint startSample = sampleTrajectoryAtProgress(
        trajectory,
        startProgress
    );
    const TrajectoryPoint endSample = sampleTrajectoryAtProgress(
        trajectory,
        endProgress
    );
    const glm::dquat currentOrientation = normalizedQuatOr(
        request.currentOrientation,
        startSample.orientation
    );
    const glm::dquat terminalOrientation = normalizedQuatOr(
        request.terminalOrientation,
        endSample.orientation
    );

    // The spatial curve follows actual translational motion, not the nose.
    // This is essential in Newton flight where hull attitude and velocity may
    // legitimately point in different directions.
    const double speedMps = glm::length(request.currentVelocityMps);
    const glm::dvec3 hullForward = normalizedVectorOr(
        currentOrientation * glm::dvec3(0.0, 0.0, -1.0),
        normalizedVectorOr(
            endSample.positionMeters - request.currentPositionMeters,
            glm::dvec3(0.0, 0.0, -1.0)
        )
    );
    const glm::dvec3 travelForward = speedMps > 2.0
        ? normalizedVectorOr(request.currentVelocityMps, hullForward)
        : hullForward;
    const glm::dvec3 terminalForward = normalizedVectorOr(
        terminalOrientation * glm::dvec3(0.0, 0.0, -1.0),
        travelForward
    );

    glm::dvec3 upHint = normalizedVectorOr(
        currentOrientation * glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0)
    );
    glm::dvec3 right = glm::cross(travelForward, upHint);
    if (glm::length(right) <= 1.0e-6)
        right = glm::cross(travelForward, glm::dvec3(0.0, 0.0, 1.0));
    if (glm::length(right) <= 1.0e-6)
        right = glm::cross(travelForward, glm::dvec3(0.0, 1.0, 0.0));
    right = normalizedVectorOr(right, glm::dvec3(1.0, 0.0, 0.0));
    const glm::dvec3 up = normalizedVectorOr(
        glm::cross(right, travelForward),
        upHint
    );

    const double remaining = std::max(0.0, endProgress - startProgress);
    const double minimumTurnRadius = std::max(
        0.0,
        request.minimumTurnRadiusMeters
    );
    double startLead = std::min(
        std::max({
            request.gateSpacingMeters * 4.0,
            request.startCaptureDistanceMeters,
            minimumTurnRadius * 0.70
        }),
        std::max(request.gateSpacingMeters * 4.0, remaining * 0.35)
    );
    double terminalLineDistance = std::min(
        std::max({
            request.terminalAlignmentDistanceMeters,
            request.gateSpacingMeters * 10.0,
            minimumTurnRadius * 1.10
        }),
        std::max(request.gateSpacingMeters * 10.0, remaining * 0.55)
    );

    // Capture and terminal alignment are boundary conditions, not permission
    // to collapse the route interior. On a short remaining path the nominal
    // 35% + 55% caps can leave only a few metres between both transition
    // zones; the five route supports then bunch into that sliver and create a
    // high-curvature B-spline with visibly snapping gate frames. Reserve an
    // interior span of up to four gates (or 20% of the remaining source path)
    // and scale both boundary zones together when necessary.
    const double minimumInteriorSupportDistance = std::min(
        remaining * 0.20,
        request.gateSpacingMeters * 4.0
    );
    const double transitionBudget = std::max(
        0.0,
        remaining - minimumInteriorSupportDistance
    );
    const double requestedTransitionDistance =
        startLead + terminalLineDistance;
    if (requestedTransitionDistance > transitionBudget + Epsilon &&
        requestedTransitionDistance > Epsilon)
    {
        const double scale = transitionBudget / requestedTransitionDistance;
        startLead *= scale;
        terminalLineDistance *= scale;
    }
    const double terminalPathStart = std::max(
        startProgress,
        endProgress - terminalLineDistance
    );
    const TrajectoryPoint terminalPathSample = sampleTrajectoryAtProgress(
        trajectory,
        terminalPathStart
    );

    std::vector<glm::dvec3> baseControls;
    std::vector<double> baseSource;
    baseControls.reserve(14);
    baseSource.reserve(14);
    appendControl(
        baseControls,
        baseSource,
        request.currentPositionMeters,
        startSample.sourceProgressMeters
    );

    const double routeSupportStartProgress = std::min(
        terminalPathStart,
        startProgress + startLead
    );
    const TrajectoryPoint routeSupportStart = sampleTrajectoryAtProgress(
        trajectory,
        routeSupportStartProgress
    );
    appendControl(
        baseControls,
        baseSource,
        request.currentPositionMeters + travelForward * startLead,
        routeSupportStart.sourceProgressMeters
    );

    // The accepted trajectory remains a topology hint, not a mandatory line.
    // Wide candidate generation below is allowed to move these supports by
    // kilometres when that buys a gentler turn.
    constexpr int RouteSupports = 5;
    for (int i = 1; i <= RouteSupports; ++i)
    {
        const double u = static_cast<double>(i) /
            static_cast<double>(RouteSupports + 1);
        const double p = routeSupportStartProgress +
            (terminalPathStart - routeSupportStartProgress) * u;
        if (p <= routeSupportStartProgress + Epsilon ||
            p >= terminalPathStart - Epsilon)
        {
            continue;
        }
        const TrajectoryPoint sample = sampleTrajectoryAtProgress(
            trajectory,
            p
        );
        appendControl(
            baseControls,
            baseSource,
            sample.positionMeters,
            sample.sourceProgressMeters
        );
    }

    const glm::dvec3 terminalOuter =
        request.terminalPositionMeters - terminalForward * terminalLineDistance;
    const glm::dvec3 terminalMiddle =
        request.terminalPositionMeters -
        terminalForward * (terminalLineDistance * 0.45);
    appendControl(
        baseControls,
        baseSource,
        terminalOuter,
        terminalPathSample.sourceProgressMeters
    );
    appendControl(
        baseControls,
        baseSource,
        terminalMiddle,
        terminalPathSample.sourceProgressMeters +
            (endSample.sourceProgressMeters -
             terminalPathSample.sourceProgressMeters) * 0.55
    );
    appendControl(
        baseControls,
        baseSource,
        request.terminalPositionMeters,
        endSample.sourceProgressMeters
    );

    struct Candidate
    {
        std::vector<glm::dvec3> points;
        std::vector<double> source;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(9);
    candidates.push_back({baseControls, baseSource});

    const std::size_t movableBegin = std::min<std::size_t>(2, baseControls.size());
    const std::size_t movableEnd = baseControls.size() > 3
        ? baseControls.size() - 3
        : movableBegin;
    const double broadOffset = std::max({
        minimumTurnRadius * 1.35,
        request.gateSpacingMeters * 20.0,
        800.0
    });

    auto appendBulge = [&](const glm::dvec3& axis, double amplitude)
    {
        if (movableEnd <= movableBegin)
            return;
        Candidate c {baseControls, baseSource};
        const double count = static_cast<double>(movableEnd - movableBegin + 1);
        for (std::size_t i = movableBegin; i < movableEnd; ++i)
        {
            const double u = static_cast<double>(i - movableBegin + 1) / count;
            const double weight = std::sin(glm::pi<double>() * u);
            c.points[i] += axis * (amplitude * weight);
        }
        candidates.push_back(std::move(c));
    };

    if (minimumTurnRadius > 1.0)
    {
        appendBulge(right, broadOffset);
        appendBulge(-right, broadOffset);
        appendBulge(up, broadOffset);
        appendBulge(-up, broadOffset);
    }

    // A sharp reversal cannot be fixed by merely moving old supports. Supply
    // two broad horseshoe candidates only when a hard radius is requested.
    // They are intentionally much longer; a readable loop is preferable to a
    // kinked short route.
    auto appendLoop = [&](const glm::dvec3& axis)
    {
        if (baseControls.size() < 5)
            return;
        const double radius = std::max(minimumTurnRadius, broadOffset * 0.75);
        Candidate c;
        c.points.reserve(baseControls.size() + 2);
        c.source.reserve(baseSource.size() + 2);
        c.points.push_back(request.currentPositionMeters);
        c.source.push_back(startSample.sourceProgressMeters);
        c.points.push_back(
            request.currentPositionMeters + travelForward * radius
        );
        c.source.push_back(startSample.sourceProgressMeters);
        c.points.push_back(
            request.currentPositionMeters + travelForward * radius +
            axis * (radius * 2.0)
        );
        c.source.push_back(startSample.sourceProgressMeters);
        c.points.push_back(
            request.currentPositionMeters - travelForward * radius +
            axis * (radius * 2.0)
        );
        c.source.push_back(startSample.sourceProgressMeters);
        for (std::size_t i = movableBegin; i < baseControls.size(); ++i)
        {
            c.points.push_back(baseControls[i]);
            c.source.push_back(baseSource[i]);
        }
        candidates.push_back(std::move(c));
    };
    if (minimumTurnRadius > 1.0)
    {
        appendLoop(right);
        appendLoop(-right);
    }

    double bestScore = std::numeric_limits<double>::infinity();
    for (auto& candidate : candidates)
    {
        SmoothPathRequest smooth;
        smooth.pathPointsMeters = std::move(candidate.points);
        smooth.sourceProgressMeters = std::move(candidate.source);
        smooth.obstacles = request.obstacles;
        smooth.vehicle = request.vehicle;
        smooth.maxSampleSpacingMeters = std::max(
            4.0,
            request.curveSampleSpacingMeters
        );
        smooth.maxChordErrorMeters = std::max(
            0.02,
            request.curveChordErrorMeters
        );
        smooth.maxSupportLevel = request.maxSmoothSupportLevel;
        smooth.maxCurvaturePerMeter = minimumTurnRadius > 1.0
            ? 1.0 / minimumTurnRadius
            : 0.0;
        smooth.allowPolylineFallback = false;
        smooth.terminalAllowedObstacleId = request.terminalAllowedObstacleId;
        smooth.terminalObstacleEntrySourceProgressMeters =
            request.terminalObstacleEntrySourceProgressMeters;

        const auto result = SmoothPathOptimizer::optimize(smooth);
        if (!result.valid || result.points.size() < 2)
            continue;

        const double score =
            result.diagnostics.maxCurvaturePerMeter * 1.0e9 +
            result.diagnostics.curvatureVariation * 1.0e8 +
            result.diagnostics.optimizedLengthMeters * 1.0e-4;
        if (score >= bestScore)
            continue;

        bestScore = score;
        out.points = result.points;
        out.maxCurvaturePerMeter =
            result.diagnostics.maxCurvaturePerMeter;
    }
    return out;
}

std::vector<double> curveProgress(const std::vector<SmoothPathPoint>& points)
{
    std::vector<double> out(points.size(), 0.0);
    for (std::size_t i = 1; i < points.size(); ++i)
        out[i] = out[i - 1] + glm::length(points[i].positionMeters - points[i - 1].positionMeters);
    return out;
}

SmoothPathPoint sampleCurveAtDistance(
    const std::vector<SmoothPathPoint>& points,
    const std::vector<double>& progress,
    double distanceMeters
)
{
    if (points.size() == 1)
        return points.front();
    const double clamped = std::clamp(distanceMeters, 0.0, progress.back());
    std::size_t hi = 1;
    while (hi < progress.size() && progress[hi] < clamped)
        ++hi;
    if (hi >= progress.size())
        return points.back();
    const double span = std::max(Epsilon, progress[hi] - progress[hi - 1]);
    const double u = std::clamp((clamped - progress[hi - 1]) / span, 0.0, 1.0);
    return {
        points[hi - 1].positionMeters +
            (points[hi].positionMeters - points[hi - 1].positionMeters) * u,
        points[hi - 1].sourceProgressMeters +
            (points[hi].sourceProgressMeters - points[hi - 1].sourceProgressMeters) * u
    };
}

glm::dvec3 tangentAt(
    const std::vector<SmoothPathPoint>& points,
    std::size_t index,
    const glm::dvec3& fallback
)
{
    glm::dvec3 delta(0.0);
    if (index == 0 && points.size() > 1)
        delta = points[1].positionMeters - points[0].positionMeters;
    else if (index + 1 >= points.size() && index > 0)
        delta = points[index].positionMeters - points[index - 1].positionMeters;
    else if (index > 0 && index + 1 < points.size())
        delta = points[index + 1].positionMeters - points[index - 1].positionMeters;
    return normalizedVectorOr(delta, fallback);
}

glm::dquat minimalRotation(const glm::dvec3& from, const glm::dvec3& to)
{
    const glm::dvec3 a = normalizedVectorOr(from, glm::dvec3(0.0, 0.0, -1.0));
    const glm::dvec3 b = normalizedVectorOr(to, a);
    const double c = std::clamp(glm::dot(a, b), -1.0, 1.0);
    if (c > 1.0 - 1.0e-10)
        return glm::dquat(1.0, 0.0, 0.0, 0.0);
    if (c < -1.0 + 1.0e-10)
    {
        glm::dvec3 axis = glm::cross(a, glm::dvec3(1.0, 0.0, 0.0));
        if (glm::length(axis) <= Epsilon)
            axis = glm::cross(a, glm::dvec3(0.0, 1.0, 0.0));
        return glm::angleAxis(glm::pi<double>(), glm::normalize(axis));
    }
    const glm::dvec3 axis = glm::cross(a, b);
    return glm::angleAxis(std::acos(c), glm::normalize(axis));
}

double signedAngleAround(
    const glm::dvec3& axis,
    const glm::dvec3& from,
    const glm::dvec3& to
)
{
    const glm::dvec3 n = normalizedVectorOr(axis, glm::dvec3(0.0, 0.0, -1.0));
    const glm::dvec3 a = normalizedVectorOr(from - n * glm::dot(from, n), glm::dvec3(0.0, 1.0, 0.0));
    const glm::dvec3 b = normalizedVectorOr(to - n * glm::dot(to, n), a);
    return std::atan2(glm::dot(n, glm::cross(a, b)), glm::dot(a, b));
}

std::vector<glm::dquat> rotationMinimizingOrientations(
    const std::vector<SmoothPathPoint>& points,
    const std::vector<double>& progress,
    const glm::dquat& currentOrientation,
    const glm::dquat& terminalOrientation
)
{
    std::vector<glm::dquat> out(points.size(), glm::dquat(1.0, 0.0, 0.0, 0.0));
    if (points.empty())
        return out;

    std::vector<glm::dvec3> tangent(points.size());
    const glm::dvec3 currentForward = normalizedVectorOr(
        currentOrientation * glm::dvec3(0.0, 0.0, -1.0),
        glm::dvec3(0.0, 0.0, -1.0)
    );
    for (std::size_t i = 0; i < points.size(); ++i)
        tangent[i] = tangentAt(points, i, i ? tangent[i - 1] : currentForward);

    std::vector<glm::dvec3> up(points.size());
    up[0] = normalizedVectorOr(
        currentOrientation * glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(0.0, 1.0, 0.0)
    );
    up[0] = normalizedVectorOr(
        up[0] - tangent[0] * glm::dot(up[0], tangent[0]),
        glm::dvec3(0.0, 1.0, 0.0)
    );
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const glm::dquat transport = minimalRotation(tangent[i - 1], tangent[i]);
        up[i] = normalizedVectorOr(
            transport * up[i - 1] - tangent[i] *
                glm::dot(transport * up[i - 1], tangent[i]),
            up[i - 1]
        );
    }

    const glm::dvec3 terminalUp = normalizedVectorOr(
        terminalOrientation * glm::dvec3(0.0, 1.0, 0.0),
        up.back()
    );
    const double terminalTwist = signedAngleAround(
        tangent.back(),
        up.back(),
        terminalUp
    );
    const double total = std::max(Epsilon, progress.back());
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const double u = smoothStep01(progress[i] / total);
        const glm::dquat twist = glm::angleAxis(terminalTwist * u, tangent[i]);
        const glm::dvec3 twistedUp = normalizedVectorOr(twist * up[i], up[i]);
        out[i] = orientationForForwardUp(tangent[i], twistedUp);
    }
    out.front() = normalizedQuatOr(currentOrientation, out.front());
    out.back() = normalizedQuatOr(terminalOrientation, out.back());
    return out;
}

bool validRequest(const GuidanceTunnelRequest& request)
{
    return request.trajectory && request.trajectory->ready() &&
        finite3(request.currentPositionMeters) &&
        finite3(request.currentVelocityMps) &&
        finite3(request.terminalPositionMeters) &&
        finite(request.minimumTurnRadiusMeters) &&
        request.minimumTurnRadiusMeters >= 0.0 &&
        finite(request.gateSpacingMeters) && request.gateSpacingMeters > 0.0 &&
        finite(request.gateWidthMeters) && request.gateWidthMeters > 0.0 &&
        finite(request.gateHeightMeters) && request.gateHeightMeters > 0.0 &&
        finite(request.lateralToleranceMeters) && request.lateralToleranceMeters >= 0.0 &&
        finite(request.verticalToleranceMeters) && request.verticalToleranceMeters >= 0.0;
}

} // namespace

GuidanceTunnel GuidanceTunnelBuilder::build(const GuidanceTunnelRequest& request)
{
    GuidanceTunnel out;
    if (!validRequest(request))
        return out;

    const DynamicCurve dynamic =
        request.buildMode == GuidanceTunnelBuildMode::TrajectoryBackbone
            ? buildTrajectoryBackboneCurve(request)
            : buildDynamicCurve(request);
    if (dynamic.points.size() < 2)
        return out;

    const auto progress = curveProgress(dynamic.points);
    const double total = progress.back();
    if (!finite(total) || total <= Epsilon)
        return out;

    const auto denseOrientation = rotationMinimizingOrientations(
        dynamic.points,
        progress,
        normalizedQuatOr(request.currentOrientation, glm::dquat(1.0, 0.0, 0.0, 0.0)),
        normalizedQuatOr(request.terminalOrientation, glm::dquat(1.0, 0.0, 0.0, 0.0))
    );

    auto orientationAtDistance = [&](double distance)
    {
        const double clamped = std::clamp(distance, 0.0, total);
        std::size_t hi = 1;
        while (hi < progress.size() && progress[hi] < clamped)
            ++hi;
        if (hi >= progress.size())
            return denseOrientation.back();
        const double span = std::max(Epsilon, progress[hi] - progress[hi - 1]);
        const double u = std::clamp((clamped - progress[hi - 1]) / span, 0.0, 1.0);
        glm::dquat a = denseOrientation[hi - 1];
        glm::dquat b = denseOrientation[hi];
        if (glm::dot(a, b) < 0.0)
            b = -b;
        return glm::normalize(glm::slerp(a, b, u));
    };

    const double spacing = std::max(1.0, request.gateSpacingMeters);
    const std::size_t fullSteps = static_cast<std::size_t>(std::floor(total / spacing));
    out.gates.reserve(fullSteps + 2);

    auto appendGate = [&](double distance)
    {
        const SmoothPathPoint point = sampleCurveAtDistance(dynamic.points, progress, distance);
        GuidanceTunnelGate gate;
        gate.distanceAlongTunnelMeters = distance;
        gate.sourceTrajectoryProgressMeters = point.sourceProgressMeters;
        gate.positionMeters = point.positionMeters;
        gate.orientation = orientationAtDistance(distance);
        gate.widthMeters = request.gateWidthMeters;
        gate.heightMeters = request.gateHeightMeters;
        gate.lateralToleranceMeters = request.lateralToleranceMeters;
        gate.verticalToleranceMeters = request.verticalToleranceMeters;
        gate.recommendedSpeedMps = sampleSpeedAtSourceProgress(
            *request.trajectory,
            point.sourceProgressMeters
        );
        out.gates.push_back(std::move(gate));
    };

    appendGate(0.0);
    for (std::size_t step = 1; step <= fullSteps; ++step)
    {
        const double distance = spacing * static_cast<double>(step);
        if (distance >= total - 1.0e-7)
            break;
        appendGate(distance);
    }
    appendGate(total);

    if (out.gates.size() < 2)
        return GuidanceTunnel{};
    if (request.buildMode == GuidanceTunnelBuildMode::ReconnectCurrentPose)
    {
        out.gates.front().positionMeters = request.currentPositionMeters;
        out.gates.front().orientation = normalizedQuatOr(
            request.currentOrientation,
            out.gates.front().orientation
        );
        out.gates.back().positionMeters = request.terminalPositionMeters;
        out.gates.back().orientation = normalizedQuatOr(
            request.terminalOrientation,
            out.gates.back().orientation
        );
    }
    out.systemId = request.trajectory->systemId;
    out.frameId = request.trajectory->frameId;
    out.passedTrajectoryProgressMeters = dynamic.passedTrajectoryProgressMeters;
    out.maxCurvaturePerMeter = dynamic.maxCurvaturePerMeter;
    out.minimumTurnRadiusMeters = request.minimumTurnRadiusMeters;
    out.valid = true;
    return out;
}

} // namespace world::navigation
