#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "src/world/navigation/NavigationObstacle.h"

namespace game::navigation
{
struct DockingAdvisoryRequest
{
    glm::dvec3 startMeters {0.0};
    glm::dvec3 entranceMeters {0.0};
    glm::dvec3 outward {0.0, 0.0, 1.0};
    double standoffMeters = 300.0;
    double hullRadiusMeters = 0.0;
    double maxSpeedMps = 100.0;
    double acceleratingMps2 = 5.0;
    double brakingMps2 = 5.0;
    double lateralMps2 = 3.0;

    // Assisted ships prefer fly-through arcs. Newtonian/heavy ships may ask
    // for piecewise-straight geometry so their later maneuver compiler can
    // coast, rotate and burn instead of pretending to be an aircraft.
    bool roundTurns = true;

    double gateSpacingMeters = 500.0;

    // Optional launch-heading contract. Manual docking uses the real hull nose
    // so the first visible corridor segment leaves the stopped ship straight
    // through the windshield instead of immediately turning sideways. Planner
    // may shorten the requested lead when geometry blocks it, but it never
    // changes the authored initial direction.
    bool hasInitialForward = false;
    glm::dvec3 initialForward {0.0, 0.0, -1.0};
    double initialForwardLeadMeters = 0.0;

    // Manual guidance stays sparse in open transit but becomes denser on the
    // final station approach so a curved turn is presented as a usable tunnel
    // rather than a few long chords.
    double terminalGateSpacingMeters = 250.0;
    double terminalDenseDistanceMeters = 2000.0;

    // Optional manual-guidance geometry. Zero keeps the historical minimum
    // final-axis length. terminalTurnSegmentFraction controls how much of each
    // adjacent segment a circular fillet may consume.
    double terminalApproachLengthMeters = 0.0;
    double terminalTurnSegmentFraction = 0.40;

    // Preferred human-flyable radius. For Assisted docking this is authored
    // geometry, not a hint: Planner may rotate the arc around the docking axis
    // and move its ALIGN farther outward, but it must not silently squeeze the
    // requested radius into a different maneuver.
    double preferredTerminalTurnRadiusMeters = 0.0;

    std::vector<world::navigation::NavigationObstacle> obstacles;
};
struct DockingAdvisoryGate
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    double speedMps = 0.0;
};
struct DockingAdvisoryPlan
{
    std::string failure;

    // Sparse user-facing corridor frames.
    std::vector<DockingAdvisoryGate> gates;

    // Dense samples of the same accepted route, including the same speed
    // profile. Automatic execution consumes this product instead of asking a
    // second planner to reinterpret sparse display gates into another curve.
    std::vector<DockingAdvisoryGate> executionGates;

    // Diagnostics for route-selection policy. A detour means Planner changed
    // coarse geometry before conceding turn radius. relaxed means no route
    // preserving the preferred radius was found and the widest feasible local
    // terminal arc was accepted instead.
    bool terminalDetourUsed = false;
    bool terminalTurnRadiusRelaxed = false;
    double terminalTurnRadiusMeters = 0.0;

    // Exact preferred terminal-arc diagnostics. These counters make a failed
    // route explain itself instead of collapsing every cause into "invalid".
    double terminalTurnRequestedRadiusMeters = 0.0;
    int terminalArcCandidatesTested = 0;
    int terminalArcRouteable = 0;
    int terminalArcRouteRejected = 0;
    int terminalArcTransitReady = 0;
    int terminalArcTransitRejected = 0;
    int terminalArcCollisionRejected = 0;
    int terminalArcAcceptedCandidates = 0;
    int terminalArcAxisPassesTested = 0;
    std::string terminalArcLastRejection;

    // Dominant concrete obstacle that rejected exact-radius candidates.
    // This makes a live failure actionable instead of reporting only
    // "terminal-arc-obstructed".
    std::string terminalArcDominantBlockerId;
    int terminalArcDominantBlockerHits = 0;
    glm::dvec3 terminalArcDominantBlockerCenterMeters {0.0};
    glm::dvec3 terminalArcDominantBlockerHalfExtentsMeters {0.0};
    double terminalArcDominantBlockerRadiusMeters = 0.0;

    // Selected rotation of the terminal circular primitive around the docking
    // axis. Zero is one basis direction; other values prove Planner actually
    // searched another side instead of squeezing the same local corner.
    double terminalArcRotationDegrees = 0.0;

    // First nose-first launch turn is authored as a tangent circular fillet.
    // Published 500 m corridor frames are only sparse chords and must not be
    // mistaken for the continuity proof of the underlying geometry.
    bool initialTurnPresent = false;
    double initialTurnRadiusMeters = 0.0;

    // terminalApproachLengthMeters is the actually accepted collision-free
    // straight docking-axis lead. The request value is preferred geometry, not
    // a hard semantic requirement; only the minimum ingress immediately in
    // front of the port is mandatory.
    bool terminalApproachShortened = false;
    bool terminalApproachExtended = false;
    double terminalApproachLengthMeters = 0.0;

    bool valid() const noexcept
    {
        return failure.empty() &&
            gates.size() >= 2 &&
            executionGates.size() >= 2;
    }
};

// Canonical one-line diagnostics used by client preflight and authoritative
// Automatic planning. Presentation/state layers must not reconstruct their own
// interpretation of Planner decisions.
std::string dockingAdvisoryPlanDiagnosticSummary(
    const DockingAdvisoryPlan& plan
);

class DockingAdvisoryPlanner
{
public:
    static DockingAdvisoryPlan plan(const DockingAdvisoryRequest& input);
};
} // namespace game::navigation
