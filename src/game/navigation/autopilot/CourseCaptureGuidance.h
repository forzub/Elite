#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation::autopilot
{

// Pure route-recapture geometry.
//
// Inputs are only measured translation state plus the Planner-authored route.
// Hull attitude, engines, ShipControlState and control-law details do not
// belong here.
//
// While centered, desiredCourseMap is the exact current route tangent.
// When displaced, the layer chooses a future meeting station and authors a
// temporary cubic Bezier whose:
//   - start tangent is the ACTUAL velocity direction,
//   - end tangent is the route tangent at the meeting station.
//
// The immediate steering course is sampled one measured course-response
// horizon into that capture curve. Thus there is no fixed 50-250 m lookahead:
// faster craft / slower response naturally look farther, and small errors
// produce small course corrections.
class CourseCaptureGuidance final
{
public:
    struct Request
    {
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 actualVelocityMapMps {0.0};

        const std::vector<planner::RouteCurveSegment>* routeCurves = nullptr;
        double currentRouteProgressMeters = 0.0;

        double centeringDeadbandMeters = 0.0;
        double courseResponseSeconds = 0.0;
        double velocityDirectionThresholdMps = 0.5;

        // Meeting-point quality constraint. The ray from craft to meeting
        // station should already be reasonably close to the route tangent
        // there, so capture joins rather than crosses the route.
        double maximumMeetingJoinAngleRad = 0.20;
    };

    struct Result
    {
        bool valid = false;
        bool captureActive = false;

        double crossTrackErrorMeters = 0.0;
        double meetingRouteProgressMeters = 0.0;
        double meetingJoinAngleRad = 0.0;
        double capturePreviewParameter01 = 0.0;

        glm::dvec3 currentRoutePointMapMeters {0.0};
        glm::dvec3 currentRouteTangentMap {0.0, 0.0, -1.0};

        glm::dvec3 meetingPointMapMeters {0.0};
        glm::dvec3 meetingTangentMap {0.0, 0.0, -1.0};

        // Temporary cubic capture geometry.
        glm::dvec3 captureP0 {0.0};
        glm::dvec3 captureP1 {0.0};
        glm::dvec3 captureP2 {0.0};
        glm::dvec3 captureP3 {0.0};

        glm::dvec3 desiredCourseMap {0.0, 0.0, -1.0};
        double courseErrorRad = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& request) noexcept
    {
        Result out;

        if (!request.routeCurves || request.routeCurves->empty())
            return out;

        const auto& curves = *request.routeCurves;
        const double routeEnd = curves.back().endProgressMeters;
        if (!(std::isfinite(routeEnd) && routeEnd > 0.0))
            return out;

        const double progress = std::clamp(
            request.currentRouteProgressMeters,
            curves.front().startProgressMeters,
            routeEnd
        );

        const auto current = sampleRoute(curves, progress);
        if (!current.valid)
            return out;

        out.currentRoutePointMapMeters = current.position;
        out.currentRouteTangentMap = current.tangent;

        const glm::dvec3 positionError =
            request.positionMapMeters - current.position;
        const glm::dvec3 crossError =
            positionError -
            current.tangent * glm::dot(positionError, current.tangent);
        out.crossTrackErrorMeters = glm::length(crossError);

        if (!std::isfinite(out.crossTrackErrorMeters))
            return out;

        const double speed = glm::length(request.actualVelocityMapMps);
        const double velocityThreshold =
            std::max(0.0, request.velocityDirectionThresholdMps);
        const glm::dvec3 actualCourse =
            std::isfinite(speed) && speed > velocityThreshold
                ? request.actualVelocityMapMps / speed
                : current.tangent;

        const double deadband =
            std::max(0.0, request.centeringDeadbandMeters);

        if (out.crossTrackErrorMeters <= deadband)
        {
            out.captureActive = false;
            out.meetingRouteProgressMeters = progress;
            out.meetingPointMapMeters = current.position;
            out.meetingTangentMap = current.tangent;
            out.captureP0 = request.positionMapMeters;
            out.captureP1 = request.positionMapMeters;
            out.captureP2 = current.position;
            out.captureP3 = current.position;
            out.desiredCourseMap = current.tangent;
            out.courseErrorRad =
                angleBetween(actualCourse, out.desiredCourseMap);
            out.valid =
                finiteVec(out.desiredCourseMap) &&
                std::isfinite(out.courseErrorRad);
            return out;
        }

        out.captureActive = true;

        const double responseSeconds =
            std::max(0.05, request.courseResponseSeconds);
        const double responseDistance =
            std::max(1.0, speed * responseSeconds);

        // Cross-track determines how much longitudinal room is needed to
        // return without a sharp intercept. Response distance ensures a fast
        // or sluggish craft is not given a meeting point it cannot steer to.
        const double minimumAhead =
            std::max({
                5.0,
                responseDistance,
                out.crossTrackErrorMeters * 2.0
            });
        const double searchSpan =
            std::max({
                minimumAhead * 8.0,
                responseDistance * 8.0,
                out.crossTrackErrorMeters * 16.0
            });

        const double searchStart =
            std::min(routeEnd, progress + minimumAhead);
        const double searchEnd =
            std::min(routeEnd, progress + searchSpan);

        constexpr int CandidateCount = 48;
        bool foundMeeting = false;
        double bestAngle = std::numeric_limits<double>::infinity();
        double bestProgress = searchStart;
        RouteSample bestSample {};

        const double maxJoinAngle =
            std::clamp(
                request.maximumMeetingJoinAngleRad,
                0.01,
                1.2
            );

        for (int i = 0; i < CandidateCount; ++i)
        {
            const double u =
                CandidateCount > 1
                    ? static_cast<double>(i) /
                        static_cast<double>(CandidateCount - 1)
                    : 0.0;
            const double candidateProgress =
                searchStart * (1.0 - u) + searchEnd * u;

            const auto candidate =
                sampleRoute(curves, candidateProgress);
            if (!candidate.valid)
                continue;

            const glm::dvec3 ray =
                candidate.position - request.positionMapMeters;
            const double rayLength = glm::length(ray);
            if (!(std::isfinite(rayLength) && rayLength > 1.0e-6))
                continue;

            const double joinAngle =
                angleBetween(ray / rayLength, candidate.tangent);

            if (joinAngle < bestAngle)
            {
                bestAngle = joinAngle;
                bestProgress = candidateProgress;
                bestSample = candidate;
            }

            // Nearest acceptable meeting point wins. This keeps recovery local
            // while still demanding a near-tangent join.
            if (joinAngle <= maxJoinAngle)
            {
                foundMeeting = true;
                bestAngle = joinAngle;
                bestProgress = candidateProgress;
                bestSample = candidate;
                break;
            }
        }

        if (!bestSample.valid)
            return out;

        (void)foundMeeting;

        out.meetingRouteProgressMeters = bestProgress;
        out.meetingJoinAngleRad = bestAngle;
        out.meetingPointMapMeters = bestSample.position;
        out.meetingTangentMap = bestSample.tangent;

        const glm::dvec3 chord =
            out.meetingPointMapMeters - request.positionMapMeters;
        const double chordLength = glm::length(chord);
        if (!(std::isfinite(chordLength) && chordLength > 1.0e-6))
            return out;

        // Cubic Hermite-equivalent handles. Endpoint derivatives therefore
        // exactly match actual course and route tangent.
        const double startHandle =
            std::clamp(
                std::max(responseDistance, chordLength * 0.25),
                chordLength * 0.15,
                chordLength * 0.45
            );
        const double endHandle =
            std::clamp(
                chordLength * 0.30,
                chordLength * 0.15,
                chordLength * 0.45
            );

        out.captureP0 = request.positionMapMeters;
        out.captureP1 =
            out.captureP0 + actualCourse * startHandle;
        out.captureP3 = out.meetingPointMapMeters;
        out.captureP2 =
            out.captureP3 - out.meetingTangentMap * endHandle;

        // Look one measured response horizon along the temporary capture
        // curve. This is a physical horizon, not a hard-coded route distance.
        const double previewDistance =
            std::min(chordLength, responseDistance);
        out.capturePreviewParameter01 =
            std::clamp(
                previewDistance / chordLength,
                0.0,
                0.50
            );

        const glm::dvec3 derivative =
            cubicDerivative(
                out.captureP0,
                out.captureP1,
                out.captureP2,
                out.captureP3,
                out.capturePreviewParameter01
            );
        out.desiredCourseMap =
            normalizedOr(derivative, actualCourse);
        out.courseErrorRad =
            angleBetween(actualCourse, out.desiredCourseMap);

        out.valid =
            finiteVec(out.meetingPointMapMeters) &&
            finiteVec(out.meetingTangentMap) &&
            finiteVec(out.captureP0) &&
            finiteVec(out.captureP1) &&
            finiteVec(out.captureP2) &&
            finiteVec(out.captureP3) &&
            finiteVec(out.desiredCourseMap) &&
            std::isfinite(out.meetingJoinAngleRad) &&
            std::isfinite(out.capturePreviewParameter01) &&
            std::isfinite(out.courseErrorRad);

        return out;
    }

private:
    struct RouteSample
    {
        bool valid = false;
        glm::dvec3 position {0.0};
        glm::dvec3 tangent {0.0, 0.0, -1.0};
    };

    [[nodiscard]] static RouteSample sampleRoute(
        const std::vector<planner::RouteCurveSegment>& curves,
        double progressMeters
    ) noexcept
    {
        RouteSample out;
        if (curves.empty())
            return out;

        const planner::RouteCurveSegment* curve = &curves.back();
        for (const auto& candidate : curves)
        {
            if (progressMeters <=
                candidate.endProgressMeters + 1.0e-9)
            {
                curve = &candidate;
                break;
            }
        }

        const double p = std::clamp(
            progressMeters,
            curve->startProgressMeters,
            curve->endProgressMeters
        );
        const double t = curve->parameterAtProgress(p);
        out.position = curve->positionAtParameter(t);
        out.tangent = normalizedOr(
            curve->tangentAtProgress(p),
            curve->startForward
        );
        out.valid =
            finiteVec(out.position) &&
            finiteVec(out.tangent);
        return out;
    }

    [[nodiscard]] static glm::dvec3 cubicDerivative(
        const glm::dvec3& p0,
        const glm::dvec3& p1,
        const glm::dvec3& p2,
        const glm::dvec3& p3,
        double parameter01
    ) noexcept
    {
        const double t =
            std::clamp(parameter01, 0.0, 1.0);
        const double u = 1.0 - t;
        return
            (p1 - p0) * (3.0 * u * u) +
            (p2 - p1) * (6.0 * u * t) +
            (p3 - p2) * (3.0 * t * t);
    }

    [[nodiscard]] static bool finiteVec(const glm::dvec3& v) noexcept
    {
        return
            std::isfinite(v.x) &&
            std::isfinite(v.y) &&
            std::isfinite(v.z);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        if (std::isfinite(length) && length > 1.0e-12)
            return value / length;

        const double fallbackLength = glm::length(fallback);
        return fallbackLength > 1.0e-12
            ? fallback / fallbackLength
            : glm::dvec3(0.0, 0.0, -1.0);
    }

    [[nodiscard]] static double angleBetween(
        const glm::dvec3& a,
        const glm::dvec3& b
    ) noexcept
    {
        return std::acos(
            std::clamp(
                glm::dot(
                    normalizedOr(a, {0.0, 0.0, -1.0}),
                    normalizedOr(b, {0.0, 0.0, -1.0})
                ),
                -1.0,
                1.0
            )
        );
    }
};

} // namespace game::navigation::autopilot
