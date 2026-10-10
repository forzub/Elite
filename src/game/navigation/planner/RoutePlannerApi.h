#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation::planner
{

enum class RoutePlanDisposition : std::uint8_t
{
    Ready = 0,
    NeedsRefinement,
    WaitForWindow,
    PhysicallyImpossible,
    InvalidWorldData
};

enum class RoutePlanFailureCode : std::uint8_t
{
    None = 0,
    InvalidRequest,
    InvalidWorldGeometry,
    HullDoesNotFit,
    GoalGeometricallyIsolated,
    UnavoidableCollision,
    PropulsionInsufficient,
    NoCollisionFreeCandidate,
    DynamicWindowUnavailable,
    BackendFailure
};

[[nodiscard]] const char* routePlanFailureMessage(
    RoutePlanFailureCode code
) noexcept;

struct MandatoryTangentStraightConstraint
{
    glm::dvec3 startMeters {0.0};
    glm::dvec3 endMeters {0.0};

    // Protected minimum straight. Planner may extend the same axis outward
    // beyond this segment to obtain room for a tangent fillet, but may never
    // shorten, bend or round away this authored segment.
    double minimumStraightMeters = 0.0;

    // True when route order is free-space -> start -> end -> constrained
    // portal/volume. False for constrained portal/volume -> start -> end ->
    // free-space.
    bool inbound = true;
};

struct RoutePlanRequest
{
    glm::dvec3 startMeters {0.0};

    // Generic terminal-approach contract. goalMeters is the route endpoint.
    // terminalOutward points from the terminal structure into approach space.
    // terminalReferenceDistanceMeters is a scale used by the current backend
    // to size the protected terminal approach.
    glm::dvec3 goalMeters {0.0};
    glm::dvec3 terminalOutward {0.0, 0.0, 1.0};
    double terminalReferenceDistanceMeters = 300.0;

    double agentRadiusMeters = 0.0;
    double maxSpeedMps = 100.0;
    double acceleratingMps2 = 5.0;
    double brakingMps2 = 5.0;
    double lateralMps2 = 3.0;
    double maxAngularVelocityRadPerSecond = 0.0;
    double maxAngularAccelerationRadPerSecond2 = 0.0;
    double initialSpeedMps = 0.0;

    bool roundTurns = true;

    bool hasInitialForward = false;
    glm::dvec3 initialForward {0.0, 0.0, -1.0};
    double initialForwardLeadMeters = 0.0;

    double gateSpacingMeters = 500.0;
    double terminalGateSpacingMeters = 250.0;
    double terminalDenseDistanceMeters = 2000.0;
    double terminalApproachLengthMeters = 0.0;
    double terminalTurnSegmentFraction = 0.40;
    bool deriveTerminalTurnRadiusFromVehicle = false;
    double preferredTerminalTurnRadiusMeters = 0.0;

    // Ordered semantic transit constraints resolved before geometric planning.
    // Planner must visit these points in order; they are not optional hints.
    // This is the first bridge from protected traffic topology into authored
    // route geometry. Follower still receives one continuous RoutePlan.
    std::vector<glm::dvec3> requiredViaPointsMeters;

    // Hard route-boundary straights compiled from semantic traffic portals.
    // These are stronger than via-points: rounding may use an outward axis
    // extension, but the protected segment itself must survive unchanged.
    std::vector<MandatoryTangentStraightConstraint>
        mandatoryTangentStraights;

    std::vector<world::navigation::NavigationObstacle> obstacles;
};

struct RouteGate
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};

    // Route-local speed ceiling at this station, NOT a mandatory vehicle
    // state. Execution may be below it; if the measured craft is already
    // above it, the correct response is braking while continuing forward
    // along the route. A zero value means a real zero-speed constraint only
    // when the Planner intentionally authored one (for example HOLD/final).
    //
    // Field name is retained for source compatibility; semantically this is
    // maxSpeedMps.
    double speedMps = 0.0;
};

enum class RouteCurveKind : std::uint8_t
{
    Line = 0,
    CircularArc,
    CubicBezier
};

// Authoritative Planner -> Follower route geometry.
//
// A segment is NOT merely a set of sampled waypoints. It is a parameterized
// curve with a stable path-progress interval [startProgressMeters,
// endProgressMeters]. Dense samples may still be generated for rendering,
// collision proof and diagnostics, but they are a representation of this
// geometry rather than the geometry itself.
//
// Current docking Planner authors Line/CircularArc segments. CubicBezier is
// part of the public contract now so later route backends can publish Bezier
// geometry without forcing the Follower back to point-cloud reconstruction.
struct RouteCurveArcLengthKnot
{
    // Curve parameter in [0,1].
    double parameter01 = 0.0;

    // Distance in metres from this segment's start.
    double localProgressMeters = 0.0;
};

struct RouteCurveSegment
{
    RouteCurveKind kind = RouteCurveKind::Line;

    double startProgressMeters = 0.0;
    double endProgressMeters = 0.0;

    // Route-local speed ceiling over this primitive.
    double maxSpeedMps = 0.0;

    // Endpoint frame. For Line these are sufficient.
    glm::dvec3 startMeters {0.0};
    glm::dvec3 endMeters {0.0};
    glm::dvec3 startForward {0.0, 0.0, -1.0};
    glm::dvec3 endForward {0.0, 0.0, -1.0};
    glm::dvec3 startUp {0.0, 1.0, 0.0};
    glm::dvec3 endUp {0.0, 1.0, 0.0};

    // CircularArc representation.
    glm::dvec3 arcCenterMeters {0.0};
    glm::dvec3 arcNormal {0.0, 1.0, 0.0};
    double arcRadiusMeters = 0.0;
    double arcSweepRadians = 0.0;

    // Cubic Bezier representation:
    // B(t)=(1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t)t^2 P2 + t^3 P3.
    // P0/P3 are startMeters/endMeters; P1/P2 are these control points.
    glm::dvec3 bezierControl1Meters {0.0};
    glm::dvec3 bezierControl2Meters {0.0};

    // Optional arc-length inversion support for non-uniform parametric
    // curves such as Bezier. These knots do NOT define route geometry; they
    // only map physical progress s to the curve parameter t. Line and circle
    // segments can evaluate s<->t analytically and need no knots.
    std::vector<RouteCurveArcLengthKnot> arcLengthKnots;

    [[nodiscard]] double parameterAtProgress(
        double routeProgressMeters
    ) const noexcept
    {
        const double length =
            std::max(0.0, endProgressMeters - startProgressMeters);
        if (length <= 1.0e-12)
            return 0.0;

        const double local =
            std::clamp(
                routeProgressMeters - startProgressMeters,
                0.0,
                length
            );

        if (kind != RouteCurveKind::CubicBezier ||
            arcLengthKnots.size() < 2)
        {
            return std::clamp(local / length, 0.0, 1.0);
        }

        auto upper = std::upper_bound(
            arcLengthKnots.begin(),
            arcLengthKnots.end(),
            local,
            [](double value, const RouteCurveArcLengthKnot& knot)
            {
                return value < knot.localProgressMeters;
            }
        );
        if (upper == arcLengthKnots.begin())
            return upper->parameter01;
        if (upper == arcLengthKnots.end())
            return arcLengthKnots.back().parameter01;

        const auto& b = *upper;
        const auto& a = *(upper - 1);
        const double ds =
            b.localProgressMeters - a.localProgressMeters;
        const double u =
            ds > 1.0e-12
                ? std::clamp(
                    (local - a.localProgressMeters) / ds,
                    0.0,
                    1.0
                  )
                : 0.0;
        return std::clamp(
            a.parameter01 * (1.0 - u) +
            b.parameter01 * u,
            0.0,
            1.0
        );
    }

    [[nodiscard]] glm::dvec3 positionAtParameter(
        double parameter01
    ) const noexcept
    {
        const double t = std::clamp(parameter01, 0.0, 1.0);

        if (kind == RouteCurveKind::CircularArc)
        {
            const glm::dvec3 radial0 =
                startMeters - arcCenterMeters;
            const double normalLength = glm::length(arcNormal);
            if (normalLength <= 1.0e-12)
                return startMeters * (1.0 - t) + endMeters * t;

            const glm::dvec3 n = arcNormal / normalLength;
            const double angle = arcSweepRadians * t;
            const double c = std::cos(angle);
            const double si = std::sin(angle);
            const glm::dvec3 radial =
                radial0 * c +
                glm::cross(n, radial0) * si +
                n * glm::dot(n, radial0) * (1.0 - c);
            return arcCenterMeters + radial;
        }

        if (kind == RouteCurveKind::CubicBezier)
        {
            const double u = 1.0 - t;
            return
                startMeters * (u * u * u) +
                bezierControl1Meters * (3.0 * u * u * t) +
                bezierControl2Meters * (3.0 * u * t * t) +
                endMeters * (t * t * t);
        }

        return startMeters * (1.0 - t) + endMeters * t;
    }

    [[nodiscard]] glm::dvec3 derivativeAtParameter(
        double parameter01
    ) const noexcept
    {
        const double t = std::clamp(parameter01, 0.0, 1.0);

        if (kind == RouteCurveKind::CircularArc)
        {
            const glm::dvec3 p = positionAtParameter(t);
            const double normalLength = glm::length(arcNormal);
            if (normalLength <= 1.0e-12)
                return endMeters - startMeters;
            const glm::dvec3 n = arcNormal / normalLength;
            return
                glm::cross(n, p - arcCenterMeters) *
                arcSweepRadians;
        }

        if (kind == RouteCurveKind::CubicBezier)
        {
            const double u = 1.0 - t;
            return
                (bezierControl1Meters - startMeters) *
                    (3.0 * u * u) +
                (bezierControl2Meters - bezierControl1Meters) *
                    (6.0 * u * t) +
                (endMeters - bezierControl2Meters) *
                    (3.0 * t * t);
        }

        return endMeters - startMeters;
    }

    [[nodiscard]] glm::dvec3 secondDerivativeAtParameter(
        double parameter01
    ) const noexcept
    {
        const double t = std::clamp(parameter01, 0.0, 1.0);

        if (kind == RouteCurveKind::CircularArc)
        {
            const glm::dvec3 p = positionAtParameter(t);
            const double w = arcSweepRadians;
            return
                -(p - arcCenterMeters) * (w * w);
        }

        if (kind == RouteCurveKind::CubicBezier)
        {
            const double u = 1.0 - t;
            return
                (bezierControl2Meters -
                 2.0 * bezierControl1Meters +
                 startMeters) *
                    (6.0 * u) +
                (endMeters -
                 2.0 * bezierControl2Meters +
                 bezierControl1Meters) *
                    (6.0 * t);
        }

        return glm::dvec3(0.0);
    }

    [[nodiscard]] glm::dvec3 tangentAtProgress(
        double routeProgressMeters
    ) const noexcept
    {
        const glm::dvec3 d =
            derivativeAtParameter(
                parameterAtProgress(routeProgressMeters)
            );
        const double length = glm::length(d);
        if (length <= 1.0e-12)
            return startForward;
        return d / length;
    }

    [[nodiscard]] double curvatureAtProgress(
        double routeProgressMeters
    ) const noexcept
    {
        if (kind == RouteCurveKind::Line)
            return 0.0;

        if (kind == RouteCurveKind::CircularArc)
            return
                arcRadiusMeters > 1.0e-12
                    ? 1.0 / arcRadiusMeters
                    : 0.0;

        const double t = parameterAtProgress(routeProgressMeters);
        const glm::dvec3 d1 = derivativeAtParameter(t);
        const glm::dvec3 d2 = secondDerivativeAtParameter(t);
        const double speed = glm::length(d1);
        if (speed <= 1.0e-12)
            return 0.0;

        return
            glm::length(glm::cross(d1, d2)) /
            (speed * speed * speed);
    }
};

enum class RouteStageKind : std::uint8_t
{
    FreeTransit = 0,
    VolumeEntryCapture,
    VolumeTransit,
    VolumeExit,
    TerminalApproach
};

enum class RouteStageFramePolicy : std::uint8_t
{
    // Rotation-minimizing frame inherited from neighbouring geometry.
    Transported = 0,

    // Stage owns an authored/static navigation frame (BLUE/GREEN corridor,
    // gate, canyon, tunnel, etc.).
    NavigationFrame,

    // Stage owns the live terminal docking frame. Dynamic dock phase may be
    // refreshed while this stage is active without rotating earlier stages.
    LiveDockFrame
};

enum class RouteStageRefreshPolicy : std::uint8_t
{
    FrozenAtPlanning = 0,
    RefreshOnStageEntry,
    LiveDuringStage
};

// One continuous RoutePlan is composed of ordered semantic stages.
//
// A stage owns a route-progress interval and the navigation contract valid
// over that interval. Geometry/speed planning remains continuous across stage
// boundaries, but frame ownership and dynamic-data refresh are explicitly
// local to the stage instead of being global route state.
struct RouteStageSpan
{
    std::string id;
    RouteStageKind kind = RouteStageKind::FreeTransit;

    double startProgressMeters = 0.0;
    double endProgressMeters = 0.0;

    RouteStageFramePolicy framePolicy =
        RouteStageFramePolicy::Transported;
    RouteStageRefreshPolicy refreshPolicy =
        RouteStageRefreshPolicy::FrozenAtPlanning;

    // Static/authored stage frame. For LiveDockFrame this is the planning
    // epoch fallback/reference; execution may refresh the live frame.
    glm::dvec3 frameForward {0.0, 0.0, -1.0};
    glm::dvec3 frameUp {0.0, 1.0, 0.0};

    // Semantic ownership. Empty for unconstrained free transit.
    std::string sourceId;
    std::string volumeId;

    // True for hard containment stages such as BLUE KeepInside.
    bool hardContainment = false;
};

struct RouteFrameAnchor
{
    double progressMeters = 0.0;
    glm::dvec3 upReference {0.0, 1.0, 0.0};
    double liveDockPhaseWeight = 0.0;
};

struct RoutePlan
{
    RoutePlanDisposition disposition = RoutePlanDisposition::NeedsRefinement;
    RoutePlanFailureCode failureCode = RoutePlanFailureCode::BackendFailure;
    std::string failure;
    std::string userMessage;
    std::string diagnosticSummary;

    // Presentation and execution are samplings of the same authoritative
    // parameterized route geometry. routeCurves carries the geometry itself;
    // gates/executionGates remain sampled views for HUD, collision proof and
    // compatibility while consumers migrate.
    std::vector<RouteCurveSegment> routeCurves;
    std::vector<RouteGate> gates;
    std::vector<RouteGate> executionGates;

    // Ordered semantic pieces of this one continuous route.
    std::vector<RouteStageSpan> stages;

    // Internal frame interpolation support. Consumers should prefer stages
    // for ownership/refresh decisions; anchors are a derived implementation
    // detail used by RouteFrameField.
    std::vector<RouteFrameAnchor> routeFrameAnchors;

    bool terminalDetourUsed = false;

    // Planner diagnostics are carried through the public value API so
    // orchestration can explain a decision without depending on the private
    // docking-route backend type.
    bool terminalTurnRadiusRelaxed = false;
    double terminalTurnRequestedRadiusMeters = 0.0;
    double terminalTurnSpeedMps = 0.0;
    double terminalTurnLateralRadiusMeters = 0.0;
    double terminalTurnAngularRadiusMeters = 0.0;
    double terminalTurnAngularRampMeters = 0.0;

    int terminalArcCandidatesTested = 0;
    int terminalArcRouteable = 0;
    int terminalArcRouteRejected = 0;
    int terminalArcTransitReady = 0;
    int terminalArcTransitRejected = 0;
    int terminalArcCollisionRejected = 0;
    int terminalArcAcceptedCandidates = 0;
    int terminalArcAxisPassesTested = 0;
    std::string terminalArcLastRejection;

    std::string terminalArcDominantBlockerId;
    int terminalArcDominantBlockerHits = 0;
    glm::dvec3 terminalArcDominantBlockerCenterMeters {0.0};
    glm::dvec3 terminalArcDominantBlockerHalfExtentsMeters {0.0};
    double terminalArcDominantBlockerRadiusMeters = 0.0;

    double terminalArcRotationDegrees = 0.0;

    bool initialTurnPresent = false;
    double initialTurnRadiusMeters = 0.0;

    bool terminalApproachShortened = false;
    bool terminalApproachExtended = false;
    double terminalApproachLengthMeters = 0.0;
    double terminalTurnRadiusMeters = 0.0;

    [[nodiscard]] bool valid() const noexcept
    {
        return
            disposition == RoutePlanDisposition::Ready &&
            failure.empty() &&
            gates.size() >= 2 &&
            executionGates.size() >= 2;
    }

    [[nodiscard]] bool retryable() const noexcept
    {
        return
            disposition == RoutePlanDisposition::NeedsRefinement ||
            disposition == RoutePlanDisposition::WaitForWindow;
    }
};

class RoutePlanner final
{
public:
    [[nodiscard]] static RoutePlan plan(const RoutePlanRequest& request);
};

} // namespace game::navigation::planner
