#pragma once

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
