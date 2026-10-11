#include "src/game/navigation/planner/assisted/AssistedRoutePlannerApi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>

#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation::planner::assisted
{
namespace
{

constexpr double Pi = 3.1415926535897932384626433832795;
constexpr double TwoPi = 2.0 * Pi;

bool finite3(const glm::dvec3& v)
{
    return
        std::isfinite(v.x) &&
        std::isfinite(v.y) &&
        std::isfinite(v.z);
}

glm::dvec3 safeNormalize(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    if (std::isfinite(length) && length > 1.0e-9)
        return value / length;
    return fallback;
}

struct ArcPrimitive
{
    bool valid = false;
    glm::dvec3 start {0.0};
    glm::dvec3 end {0.0};
    glm::dvec3 center {0.0};
    glm::dvec3 normal {0.0, 1.0, 0.0};
    glm::dvec3 startForward {0.0, 0.0, -1.0};
    glm::dvec3 endForward {0.0, 0.0, -1.0};
    double radius = 0.0;
    double sweep = 0.0;
};

glm::dvec3 rotateAround(
    const glm::dvec3& vector,
    const glm::dvec3& axisUnit,
    double angle
)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return
        vector * c +
        glm::cross(axisUnit, vector) * s +
        axisUnit * glm::dot(axisUnit, vector) * (1.0 - c);
}

bool clearSegment(
    const glm::dvec3& a,
    const glm::dvec3& b,
    const Request& request
)
{
    return world::navigation::segmentClearOfNavigationObstacles(
        a,
        b,
        request.obstacles,
        request.vehicle.radiusMeters
    );
}

bool clearArc(
    const ArcPrimitive& arc,
    const Request& request
)
{
    if (!arc.valid)
        return false;

    // Identity arc means this boundary already has the correct tangent and no
    // turn primitive is needed. It is a legitimate graph edge of zero length.
    if (std::abs(arc.sweep) <= 1.0e-9)
        return true;

    const double spacing =
        std::max(5.0, request.geometry.denseSampleSpacingMeters);
    const double length =
        arc.radius * std::abs(arc.sweep);
    const int steps =
        std::clamp(
            static_cast<int>(std::ceil(length / spacing)),
            8,
            2048
        );

    glm::dvec3 previous = arc.start;
    const glm::dvec3 startRadial = arc.start - arc.center;

    for (int i = 1; i <= steps; ++i)
    {
        const double t =
            static_cast<double>(i) /
            static_cast<double>(steps);
        glm::dvec3 point =
            arc.center +
            rotateAround(
                startRadial,
                arc.normal,
                arc.sweep * t
            );
        if (i == steps)
            point = arc.end;

        if (!clearSegment(previous, point, request))
            return false;
        previous = point;
    }
    return true;
}

std::pair<glm::dvec3, glm::dvec3> perpendicularBasis(
    const glm::dvec3& tangent
)
{
    glm::dvec3 reference =
        std::abs(tangent.y) < 0.85
            ? glm::dvec3(0.0, 1.0, 0.0)
            : glm::dvec3(1.0, 0.0, 0.0);

    glm::dvec3 a =
        safeNormalize(
            glm::cross(tangent, reference),
            glm::dvec3(1.0, 0.0, 0.0)
        );
    glm::dvec3 b =
        safeNormalize(
            glm::cross(tangent, a),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    return {a, b};
}

ArcPrimitive departureArc(
    const glm::dvec3& start,
    const glm::dvec3& tangent,
    double radius,
    double planeAngle,
    double sweep
)
{
    ArcPrimitive arc;
    if (radius <= 1.0e-6 || sweep <= 1.0e-6)
        return arc;

    const auto [a, b] = perpendicularBasis(tangent);
    const glm::dvec3 centerDirection =
        safeNormalize(
            a * std::cos(planeAngle) +
            b * std::sin(planeAngle),
            a
        );

    const glm::dvec3 center =
        start + centerDirection * radius;
    const glm::dvec3 startRadial =
        start - center;

    // Positive sweep must have tangent exactly equal to input tangent.
    const glm::dvec3 normal =
        safeNormalize(
            glm::cross(tangent, centerDirection),
            glm::dvec3(0.0, 1.0, 0.0)
        );

    const glm::dvec3 endRadial =
        rotateAround(startRadial, normal, sweep);
    const glm::dvec3 end =
        center + endRadial;
    const glm::dvec3 endForward =
        safeNormalize(
            glm::cross(normal, endRadial),
            tangent
        );

    arc.valid = true;
    arc.start = start;
    arc.end = end;
    arc.center = center;
    arc.normal = normal;
    arc.startForward = tangent;
    arc.endForward = endForward;
    arc.radius = radius;
    arc.sweep = sweep;
    return arc;
}

ArcPrimitive reverseArcForArrival(
    const ArcPrimitive& reverseArc
)
{
    ArcPrimitive out;
    if (!reverseArc.valid)
        return out;

    out.valid = true;
    out.start = reverseArc.end;
    out.end = reverseArc.start;
    out.center = reverseArc.center;
    out.startForward = -reverseArc.endForward;
    out.endForward = -reverseArc.startForward;
    out.radius = reverseArc.radius;
    out.sweep = reverseArc.sweep;

    if (std::abs(reverseArc.sweep) <= 1.0e-9)
    {
        out.normal = reverseArc.normal;
        return out;
    }

    out.normal = -reverseArc.normal;
    return out;
}

double radiusForSpeed(
    const Request& request,
    double speedMps
)
{
    const double speed =
        std::max(0.5, speedMps);
    const double lateralRadius =
        speed * speed /
        std::max(1.0e-6, request.vehicle.lateralMps2);
    const double angularRadius =
        request.vehicle.maxAngularVelocityRadPerSecond > 1.0e-9
            ? speed /
                request.vehicle.maxAngularVelocityRadPerSecond
            : 0.0;

    return std::max({
        request.geometry.minimumComfortRadiusMeters,
        30.0 * request.vehicle.radiusMeters,
        lateralRadius,
        angularRadius
    });
}

double curveSpeedLimit(
    const Request& request,
    double radius
)
{
    if (radius <= 1.0e-6)
        return request.vehicle.maxSpeedMps;

    const double lateral =
        std::sqrt(
            std::max(
                0.0,
                request.vehicle.lateralMps2 * radius
            )
        );
    const double angular =
        request.vehicle.maxAngularVelocityRadPerSecond > 1.0e-9
            ? request.vehicle.maxAngularVelocityRadPerSecond *
                radius
            : request.vehicle.maxSpeedMps;

    return std::min({
        request.vehicle.maxSpeedMps,
        lateral,
        angular
    });
}

RouteCurveSegment lineCurve(
    const glm::dvec3& start,
    const glm::dvec3& end,
    double speedLimit,
    double progressStart
)
{
    RouteCurveSegment curve;
    curve.kind = RouteCurveKind::Line;
    curve.startMeters = start;
    curve.endMeters = end;
    const double length = glm::length(end - start);
    curve.startProgressMeters = progressStart;
    curve.endProgressMeters = progressStart + length;
    curve.maxSpeedMps = speedLimit;
    if (length > 1.0e-9)
    {
        curve.startForward = (end - start) / length;
        curve.endForward = curve.startForward;
    }
    return curve;
}

RouteCurveSegment arcCurve(
    const ArcPrimitive& arc,
    double speedLimit,
    double progressStart
)
{
    RouteCurveSegment curve;
    curve.kind = RouteCurveKind::CircularArc;
    curve.startMeters = arc.start;
    curve.endMeters = arc.end;
    curve.startForward = arc.startForward;
    curve.endForward = arc.endForward;
    curve.arcCenterMeters = arc.center;
    curve.arcNormal = arc.normal;
    curve.arcRadiusMeters = arc.radius;
    curve.arcSweepRadians = arc.sweep;
    curve.startProgressMeters = progressStart;
    curve.endProgressMeters =
        progressStart +
        arc.radius * std::abs(arc.sweep);
    curve.maxSpeedMps = speedLimit;
    return curve;
}

double curvesLength(
    const std::vector<RouteCurveSegment>& curves
)
{
    if (curves.empty())
        return 0.0;
    return curves.back().endProgressMeters;
}

bool appendLineIfNonZero(
    std::vector<RouteCurveSegment>& curves,
    const glm::dvec3& start,
    const glm::dvec3& end,
    double speedLimit,
    const Request& request
)
{
    if (glm::length(end - start) <= 1.0e-6)
        return true;
    if (!clearSegment(start, end, request))
        return false;

    curves.push_back(
        lineCurve(
            start,
            end,
            speedLimit,
            curvesLength(curves)
        )
    );
    return true;
}

bool appendArcIfClear(
    std::vector<RouteCurveSegment>& curves,
    const ArcPrimitive& arc,
    double speedLimit,
    const Request& request
)
{
    if (!clearArc(arc, request))
        return false;

    if (std::abs(arc.sweep) <= 1.0e-9)
        return true;

    curves.push_back(
        arcCurve(
            arc,
            speedLimit,
            curvesLength(curves)
        )
    );
    return true;
}

// Adds a fixed-radius fillet. Radius is HARD: this routine never shrinks it.
bool appendRoundedPolyline(
    std::vector<RouteCurveSegment>& curves,
    const std::vector<glm::dvec3>& points,
    double radius,
    double speedLimit,
    const Request& request
)
{
    if (points.size() < 2)
        return false;

    glm::dvec3 cursor = points.front();

    for (std::size_t i = 1; i + 1 < points.size(); ++i)
    {
        const glm::dvec3 incoming =
            points[i] - points[i - 1];
        const glm::dvec3 outgoing =
            points[i + 1] - points[i];
        const double la = glm::length(incoming);
        const double lb = glm::length(outgoing);
        if (la <= 1.0e-6 || lb <= 1.0e-6)
            return false;

        const glm::dvec3 u = incoming / la;
        const glm::dvec3 v = outgoing / lb;
        const double cosTurn =
            std::clamp(glm::dot(u, v), -1.0, 1.0);

        if (cosTurn > 0.9999)
            continue;

        const double angle = std::acos(cosTurn);
        const double tanHalf =
            std::tan(angle * 0.5);
        const glm::dvec3 normalRaw =
            glm::cross(u, v);
        const double normalLength =
            glm::length(normalRaw);

        if (!std::isfinite(tanHalf) ||
            tanHalf <= 1.0e-9 ||
            normalLength <= 1.0e-9)
        {
            return false;
        }

        const double tangentDistance =
            radius * tanHalf;

        // Hard radius contract. If it does not fit, this candidate is invalid.
        if (tangentDistance > la * 0.45 ||
            tangentDistance > lb * 0.45)
        {
            return false;
        }

        const glm::dvec3 entry =
            points[i] - u * tangentDistance;
        const glm::dvec3 exit =
            points[i] + v * tangentDistance;

        if (!appendLineIfNonZero(
                curves,
                cursor,
                entry,
                speedLimit,
                request))
        {
            return false;
        }

        const glm::dvec3 normal =
            normalRaw / normalLength;
        const glm::dvec3 inward =
            safeNormalize(
                glm::cross(normal, u),
                glm::dvec3(0.0)
            );
        const glm::dvec3 center =
            entry + inward * radius;

        ArcPrimitive arc;
        arc.valid = true;
        arc.start = entry;
        arc.end = exit;
        arc.center = center;
        arc.normal = normal;
        arc.startForward = u;
        arc.endForward = v;
        arc.radius = radius;
        arc.sweep = angle;

        if (!appendArcIfClear(
                curves,
                arc,
                curveSpeedLimit(request, radius),
                request))
        {
            return false;
        }

        cursor = exit;
    }

    return appendLineIfNonZero(
        curves,
        cursor,
        points.back(),
        speedLimit,
        request
    );
}

struct EndArcCandidate
{
    ArcPrimitive arc;
    double heuristic = 0.0;
};

std::vector<EndArcCandidate> makeDepartureCandidates(
    const Request& request,
    const glm::dvec3& arcStart,
    const glm::dvec3& tangent,
    const glm::dvec3& targetHint,
    double radius
)
{
    std::vector<EndArcCandidate> out;

    const std::size_t planeSamples =
        std::max<std::size_t>(
            8,
            request.geometry.departurePlaneSamples
        );
    const std::size_t sweepSamples =
        std::max<std::size_t>(
            6,
            request.geometry.departureSweepSamples
        );

    out.reserve(1 + planeSamples * sweepSamples);

    // "No departure arc" is a first-class candidate. If the current tangent
    // already connects correctly, forcing a minimum 20-degree turn would
    // create an unnecessary maneuver and can make an otherwise trivial route
    // unsatisfiable. The same identity candidate is used in reverse for the
    // arrival boundary.
    {
        ArcPrimitive identity;
        identity.valid = true;
        identity.start = arcStart;
        identity.end = arcStart;
        identity.center = arcStart;
        identity.normal = glm::dvec3(0.0, 1.0, 0.0);
        identity.startForward = tangent;
        identity.endForward = tangent;
        identity.radius = radius;
        identity.sweep = 0.0;

        const glm::dvec3 toTarget =
            targetHint - arcStart;
        const double targetDistance =
            glm::length(toTarget);
        const glm::dvec3 targetDir =
            safeNormalize(toTarget, tangent);
        const double headingPenalty =
            1.0 -
            std::clamp(
                glm::dot(tangent, targetDir),
                -1.0,
                1.0
            );

        EndArcCandidate candidate;
        candidate.arc = identity;
        candidate.heuristic =
            targetDistance +
            radius * 2.0 * headingPenalty;
        out.push_back(candidate);
    }

    for (std::size_t pi = 0; pi < planeSamples; ++pi)
    {
        const double planeAngle =
            TwoPi *
            static_cast<double>(pi) /
            static_cast<double>(planeSamples);

        for (std::size_t si = 0; si < sweepSamples; ++si)
        {
            // 20..180 degrees. Include real U-turn candidates.
            const double t =
                sweepSamples == 1
                    ? 1.0
                    : static_cast<double>(si) /
                        static_cast<double>(sweepSamples - 1);
            const double sweep =
                (20.0 + 160.0 * t) *
                Pi / 180.0;

            auto arc =
                departureArc(
                    arcStart,
                    tangent,
                    radius,
                    planeAngle,
                    sweep
                );
            if (!arc.valid || !clearArc(arc, request))
                continue;

            const glm::dvec3 toTarget =
                targetHint - arc.end;
            const double targetDistance =
                glm::length(toTarget);
            const glm::dvec3 targetDir =
                safeNormalize(
                    toTarget,
                    arc.endForward
                );
            const double headingPenalty =
                1.0 -
                std::clamp(
                    glm::dot(
                        arc.endForward,
                        targetDir
                    ),
                    -1.0,
                    1.0
                );

            EndArcCandidate candidate;
            candidate.arc = arc;
            candidate.heuristic =
                targetDistance +
                radius * 2.0 * headingPenalty;
            out.push_back(std::move(candidate));
        }
    }

    std::sort(
        out.begin(),
        out.end(),
        [](const auto& a, const auto& b)
        {
            return a.heuristic < b.heuristic;
        }
    );

    const std::size_t keep =
        std::min(
            out.size(),
            std::max<std::size_t>(
                4,
                request.geometry.shortlistCount
            )
        );
    out.resize(keep);
    return out;
}

bool connectorTangentCompatible(
    const ArcPrimitive& departure,
    const std::vector<glm::dvec3>& connector,
    const ArcPrimitive& arrival
)
{
    if (connector.size() < 2)
        return false;

    const glm::dvec3 first =
        safeNormalize(
            connector[1] - connector[0],
            departure.endForward
        );
    const glm::dvec3 last =
        safeNormalize(
            connector.back() -
                connector[connector.size() - 2],
            arrival.startForward
        );

    // Keep endpoint discontinuity small. Internal connector corners are
    // handled with hard-radius fillets.
    constexpr double CosTolerance =
        0.96592582628906828675; // 15 degrees

    return
        glm::dot(first, departure.endForward) >= CosTolerance &&
        glm::dot(last, arrival.startForward) >= CosTolerance;
}

struct Candidate
{
    bool valid = false;
    double cost = std::numeric_limits<double>::infinity();
    double designSpeedMps = 0.0;
    double radiusMeters = 0.0;
    std::vector<RouteCurveSegment> curves;
};

Candidate buildCandidate(
    const Request& request,
    const glm::dvec3& departureStraightEnd,
    const glm::dvec3& arrivalStraightStart,
    const ArcPrimitive& departure,
    const ArcPrimitive& arrival,
    double radius,
    double designSpeed
)
{
    Candidate result;

    world::navigation::GeometricPathRequest search;
    search.startMeters = departure.end;
    search.goalMeters = arrival.start;
    search.obstacles = request.obstacles;
    search.params.agentRadiusMeters =
        request.vehicle.radiusMeters;
    search.params.maxConsideredObstacles = 48;

    auto middle =
        world::navigation::GeometricPathPlanner::plan(search);
    if (!middle.valid && request.obstacles.size() > 48)
    {
        search.params.maxConsideredObstacles = 0;
        middle =
            world::navigation::GeometricPathPlanner::plan(search);
    }
    if (!middle.valid || middle.pointsMeters.size() < 2)
        return result;

    if (!connectorTangentCompatible(
            departure,
            middle.pointsMeters,
            arrival))
    {
        return result;
    }

    std::vector<RouteCurveSegment> curves;
    const double straightSpeed =
        request.vehicle.maxSpeedMps;
    const double arcSpeed =
        std::min(
            designSpeed,
            curveSpeedLimit(request, radius)
        );

    if (!appendLineIfNonZero(
            curves,
            request.start.positionMeters,
            departureStraightEnd,
            straightSpeed,
            request))
    {
        return result;
    }

    if (!appendArcIfClear(
            curves,
            departure,
            arcSpeed,
            request))
    {
        return result;
    }

    // The connector owns no start/end turns: departure/arrival primitives
    // already provide those tangents. Only internal support-node corners are
    // filleted, and the same radius is a hard lower bound.
    if (middle.pointsMeters.size() > 2)
    {
        if (!appendRoundedPolyline(
                curves,
                middle.pointsMeters,
                radius,
                straightSpeed,
                request))
        {
            return result;
        }
    }
    else
    {
        if (!appendLineIfNonZero(
                curves,
                middle.pointsMeters.front(),
                middle.pointsMeters.back(),
                straightSpeed,
                request))
        {
            return result;
        }
    }

    if (!appendArcIfClear(
            curves,
            arrival,
            arcSpeed,
            request))
    {
        return result;
    }

    if (!appendLineIfNonZero(
            curves,
            arrivalStraightStart,
            request.goal.positionMeters,
            straightSpeed,
            request))
    {
        return result;
    }

    double length = curvesLength(curves);
    const double cruise =
        std::max(0.5, designSpeed);
    const double traversalSeconds =
        length / cruise;

    // The candidate was built at one design speed, so there is no intentional
    // slow-down/re-acceleration inside it. Keep the term explicit for future
    // mixed-radius graphs.
    const double wastedDeltaV = 0.0;
    const double maneuverCount =
        static_cast<double>(
            std::count_if(
                curves.begin(),
                curves.end(),
                [](const RouteCurveSegment& curve)
                {
                    return
                        curve.kind ==
                        RouteCurveKind::CircularArc;
                }
            )
        );

    result.valid = true;
    result.designSpeedMps = designSpeed;
    result.radiusMeters = radius;
    result.curves = std::move(curves);
    result.cost =
        request.cost.pathLength * length +
        request.cost.wastedDeltaV * wastedDeltaV +
        request.cost.maneuverCount * maneuverCount +
        request.cost.traversalTime * traversalSeconds;
    return result;
}

glm::dvec3 sampleArcPoint(
    const RouteCurveSegment& curve,
    double t
)
{
    const glm::dvec3 radial =
        curve.startMeters -
        curve.arcCenterMeters;
    return
        curve.arcCenterMeters +
        rotateAround(
            radial,
            curve.arcNormal,
            curve.arcSweepRadians * t
        );
}

glm::dvec3 sampleArcForward(
    const RouteCurveSegment& curve,
    double t
)
{
    const glm::dvec3 point =
        sampleArcPoint(curve, t);
    return safeNormalize(
        glm::cross(
            curve.arcNormal,
            point - curve.arcCenterMeters
        ),
        curve.startForward
    );
}

void buildGates(
    RoutePlan& out,
    const Request& request
)
{
    out.executionGates.clear();
    if (out.routeCurves.empty())
        return;

    const double denseSpacing =
        std::max(
            1.0,
            request.geometry.denseSampleSpacingMeters
        );

    const auto appendGate =
        [&](const glm::dvec3& position,
            const glm::dvec3& forward,
            double speedLimit)
        {
            if (!out.executionGates.empty() &&
                glm::length(
                    out.executionGates.back().positionMeters -
                    position
                ) <= 1.0e-6)
            {
                out.executionGates.back().forward = forward;
                out.executionGates.back().speedMps =
                    std::min(
                        out.executionGates.back().speedMps,
                        speedLimit
                    );
                return;
            }

            RouteGate gate;
            gate.positionMeters = position;
            gate.forward = forward;
            gate.speedMps = speedLimit;
            out.executionGates.push_back(gate);
        };

    for (const auto& curve : out.routeCurves)
    {
        const double length =
            curve.endProgressMeters -
            curve.startProgressMeters;
        const int segments =
            std::max(
                1,
                static_cast<int>(
                    std::ceil(length / denseSpacing)
                )
            );

        for (int i = 0; i <= segments; ++i)
        {
            const double t =
                static_cast<double>(i) /
                static_cast<double>(segments);

            glm::dvec3 point;
            glm::dvec3 forward;

            if (curve.kind == RouteCurveKind::CircularArc)
            {
                point = sampleArcPoint(curve, t);
                forward = sampleArcForward(curve, t);
            }
            else
            {
                point =
                    glm::mix(
                        curve.startMeters,
                        curve.endMeters,
                        t
                    );
                forward =
                    safeNormalize(
                        curve.endMeters -
                            curve.startMeters,
                        curve.startForward
                    );
            }

            appendGate(
                point,
                forward,
                std::max(
                    0.5,
                    curve.maxSpeedMps
                )
            );
        }
    }

    if (out.executionGates.empty())
        return;

    // The real goal state owns its terminal speed.
    out.executionGates.back().speedMps =
        std::max(0.0, request.goal.speedMps);

    for (std::size_t i =
             out.executionGates.size() - 1;
         i > 0;
         --i)
    {
        const double ds =
            glm::length(
                out.executionGates[i].positionMeters -
                out.executionGates[i - 1].positionMeters
            );
        out.executionGates[i - 1].speedMps =
            std::min(
                out.executionGates[i - 1].speedMps,
                std::sqrt(
                    out.executionGates[i].speedMps *
                        out.executionGates[i].speedMps +
                    2.0 *
                        request.vehicle.brakingMps2 *
                        ds
                )
            );
    }

    double reachable =
        std::max(0.0, request.start.speedMps);
    for (std::size_t i = 1;
         i < out.executionGates.size();
         ++i)
    {
        const double ds =
            glm::length(
                out.executionGates[i].positionMeters -
                out.executionGates[i - 1].positionMeters
            );
        reachable =
            std::sqrt(
                reachable * reachable +
                2.0 *
                    request.vehicle.acceleratingMps2 *
                    ds
            );
        out.executionGates[i].speedMps =
            std::min(
                out.executionGates[i].speedMps,
                reachable
            );
        reachable =
            out.executionGates[i].speedMps;
    }

    // Sparse display gates are sampled from the authoritative dense route.
    out.gates.clear();
    const double sparseSpacing =
        std::max(
            denseSpacing,
            request.geometry.sparseGateSpacingMeters
        );

    out.gates.push_back(out.executionGates.front());
    double accumulated = 0.0;
    for (std::size_t i = 1;
         i < out.executionGates.size();
         ++i)
    {
        accumulated +=
            glm::length(
                out.executionGates[i].positionMeters -
                out.executionGates[i - 1].positionMeters
            );

        if (accumulated + 1.0e-6 >= sparseSpacing)
        {
            out.gates.push_back(out.executionGates[i]);
            accumulated = 0.0;
        }
    }

    if (glm::length(
            out.gates.back().positionMeters -
            out.executionGates.back().positionMeters
        ) > 1.0e-6)
    {
        out.gates.push_back(out.executionGates.back());
    }
}

RoutePlan invalidPlan(
    RoutePlanFailureCode code,
    const char* message
)
{
    RoutePlan out;
    out.disposition =
        RoutePlanDisposition::InvalidWorldData;
    out.failureCode = code;
    out.failure = message;
    out.userMessage = message;
    return out;
}

} // namespace

RoutePlan AssistedRoutePlanner::plan(
    const Request& request
)
{
    if (!finite3(request.start.positionMeters) ||
        !finite3(request.start.forward) ||
        !finite3(request.goal.positionMeters) ||
        !finite3(request.goal.forward) ||
        request.vehicle.radiusMeters <= 0.0 ||
        request.vehicle.maxSpeedMps <= 0.0 ||
        request.vehicle.acceleratingMps2 <= 0.0 ||
        request.vehicle.brakingMps2 <= 0.0 ||
        request.vehicle.lateralMps2 <= 0.0 ||
        request.geometry.departurePlaneSamples < 4 ||
        request.geometry.departureSweepSamples < 4)
    {
        return invalidPlan(
            RoutePlanFailureCode::InvalidRequest,
            "assisted planner request is invalid"
        );
    }

    const glm::dvec3 startForward =
        safeNormalize(
            request.start.forward,
            glm::dvec3(0.0, 0.0, -1.0)
        );
    const glm::dvec3 goalForward =
        safeNormalize(
            request.goal.forward,
            glm::dvec3(0.0, 0.0, -1.0)
        );

    const double departureStraight =
        std::max(
            0.0,
            request.geometry.protectedDepartureStraightMeters
        );
    const double arrivalStraight =
        std::max(
            0.0,
            request.geometry.protectedArrivalStraightMeters
        );

    const glm::dvec3 departureArcStart =
        request.start.positionMeters +
        startForward * departureStraight;

    const glm::dvec3 arrivalArcEnd =
        request.goal.positionMeters -
        goalForward * arrivalStraight;

    if (!clearSegment(
            request.start.positionMeters,
            departureArcStart,
            request) ||
        !clearSegment(
            arrivalArcEnd,
            request.goal.positionMeters,
            request))
    {
        RoutePlan out;
        out.disposition =
            RoutePlanDisposition::NeedsRefinement;
        out.failureCode =
            RoutePlanFailureCode::NoCollisionFreeCandidate;
        out.failure =
            "protected Assisted boundary straight is blocked";
        out.userMessage = out.failure;
        return out;
    }

    double preferredDesignSpeed =
        request.start.speedMps > 0.5
            ? std::min(
                  request.start.speedMps,
                  request.vehicle.maxSpeedMps
              )
            : request.geometry.designSpeedFraction *
                request.vehicle.maxSpeedMps;

    preferredDesignSpeed =
        std::clamp(
            preferredDesignSpeed,
            0.5,
            request.vehicle.maxSpeedMps
        );

    // Speed is the relaxation variable. Radius is NEVER silently reduced for
    // a fixed design speed.
    constexpr std::array<double, 6> SpeedFactors = {
        1.0, 0.85, 0.70, 0.55, 0.40, 0.25
    };

    Candidate best;

    for (const double factor : SpeedFactors)
    {
        const double designSpeed =
            std::max(
                0.5,
                preferredDesignSpeed * factor
            );
        const double radius =
            radiusForSpeed(
                request,
                designSpeed
            );

        const auto departureCandidates =
            makeDepartureCandidates(
                request,
                departureArcStart,
                startForward,
                arrivalArcEnd,
                radius
            );

        // Build arrival candidates by solving the same departure problem in
        // reverse. This keeps start and goal boundary treatment symmetric.
        Request reverseRequest = request;
        reverseRequest.start.positionMeters =
            arrivalArcEnd;
        reverseRequest.start.forward =
            -goalForward;
        reverseRequest.start.speedMps =
            request.goal.speedMps;

        const auto reverseCandidates =
            makeDepartureCandidates(
                reverseRequest,
                arrivalArcEnd,
                -goalForward,
                departureArcStart,
                radius
            );

        if (departureCandidates.empty() ||
            reverseCandidates.empty())
        {
            continue;
        }

        for (const auto& departureCandidate :
             departureCandidates)
        {
            for (const auto& reverseCandidate :
                 reverseCandidates)
            {
                const ArcPrimitive arrival =
                    reverseArcForArrival(
                        reverseCandidate.arc
                    );

                Candidate candidate =
                    buildCandidate(
                        request,
                        departureArcStart,
                        arrivalArcEnd,
                        departureCandidate.arc,
                        arrival,
                        radius,
                        designSpeed
                    );

                if (!candidate.valid)
                    continue;

                if (!best.valid ||
                    candidate.cost + 1.0e-6 <
                        best.cost)
                {
                    best =
                        std::move(candidate);
                }
            }
        }

        // Highest feasible design speed wins. We only sacrifice speed when no
        // admissible geometry exists at the current speed.
        if (best.valid)
            break;
    }

    if (!best.valid)
    {
        RoutePlan out;
        out.disposition =
            RoutePlanDisposition::NeedsRefinement;
        out.failureCode =
            RoutePlanFailureCode::NoCollisionFreeCandidate;
        out.failure =
            "no admissible fixed-radius Assisted route candidate";
        out.userMessage = out.failure;
        return out;
    }

    RoutePlan out;
    out.disposition =
        RoutePlanDisposition::Ready;
    out.failureCode =
        RoutePlanFailureCode::None;
    out.userMessage =
        routePlanFailureMessage(out.failureCode);
    out.routeCurves =
        std::move(best.curves);

    out.initialTurnPresent = true;
    out.initialTurnRadiusMeters =
        best.radiusMeters;
    out.terminalTurnRadiusMeters =
        best.radiusMeters;
    out.terminalTurnSpeedMps =
        best.designSpeedMps;
    out.terminalTurnRequestedRadiusMeters =
        best.radiusMeters;

    buildGates(out, request);

    if (out.gates.size() < 2 ||
        out.executionGates.size() < 2)
    {
        out.disposition =
            RoutePlanDisposition::NeedsRefinement;
        out.failureCode =
            RoutePlanFailureCode::BackendFailure;
        out.failure =
            "Assisted planner produced incomplete route samples";
        out.userMessage = out.failure;
    }

    out.diagnosticSummary =
        "assisted-v3 fixed-radius candidate search radius_m=" +
        std::to_string(best.radiusMeters) +
        " design_speed_mps=" +
        std::to_string(best.designSpeedMps) +
        " cost=" +
        std::to_string(best.cost);

    return out;
}

} // namespace game::navigation::planner::assisted
