#include "src/game/navigation/DockingAdvisoryPlanner.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <iterator>
#include <iostream>
#include <utility>
#include <unordered_map>
#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

namespace game::navigation
{
DockingAdvisoryPlan DockingAdvisoryPlanner::plan(const DockingAdvisoryRequest& r)
{
    DockingAdvisoryPlan out;
    const auto finite = [](const glm::dvec3& p)
    { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); };
    if (!finite(r.startMeters) || !finite(r.entranceMeters) ||
        !finite(r.outward) || glm::length(r.outward) < 0.9 ||
        !std::isfinite(r.standoffMeters) || r.standoffMeters <= 0 ||
        !std::isfinite(r.hullRadiusMeters) || r.hullRadiusMeters <= 0 ||
        !std::isfinite(r.maxSpeedMps) || r.maxSpeedMps <= 0 ||
        !std::isfinite(r.acceleratingMps2) || r.acceleratingMps2 <= 0 ||
        !std::isfinite(r.brakingMps2) || r.brakingMps2 <= 0 ||
        !std::isfinite(r.lateralMps2) || r.lateralMps2 <= 0 ||
        !std::isfinite(r.initialSpeedMps) || r.initialSpeedMps < 0.0 ||
        !std::isfinite(r.gateSpacingMeters) || r.gateSpacingMeters <= 0 ||
        !std::isfinite(r.initialForwardLeadMeters) ||
            r.initialForwardLeadMeters < 0.0 ||
        (r.hasInitialForward &&
            (!finite(r.initialForward) ||
             glm::length(r.initialForward) < 0.9)) ||
        !std::isfinite(r.terminalGateSpacingMeters) ||
            r.terminalGateSpacingMeters <= 0 ||
        !std::isfinite(r.terminalDenseDistanceMeters) ||
            r.terminalDenseDistanceMeters <= 0 ||
        !std::isfinite(r.terminalApproachLengthMeters) ||
            r.terminalApproachLengthMeters < 0 ||
        !std::isfinite(r.terminalTurnSegmentFraction) ||
            r.terminalTurnSegmentFraction <= 0.0 ||
            r.terminalTurnSegmentFraction > 0.90 ||
        !std::isfinite(r.preferredTerminalTurnRadiusMeters) ||
            r.preferredTerminalTurnRadiusMeters < 0.0 ||
        (r.deriveTerminalTurnRadiusFromVehicle &&
            (!std::isfinite(r.maxAngularVelocityRadPerSecond) ||
             r.maxAngularVelocityRadPerSecond <= 0.0 ||
             !std::isfinite(r.maxAngularAccelerationRadPerSecond2) ||
             r.maxAngularAccelerationRadPerSecond2 <= 0.0)))
    { out.failure = "invalid dock advisory input"; return out; }

    for (const auto& via : r.requiredViaPointsMeters)
    {
        if (!finite(via))
        {
            out.failure = "invalid mandatory traffic via point";
            return out;
        }
    }

    auto plannerViaPoints = r.requiredViaPointsMeters;
    const bool traceTraffic =
        !r.mandatoryTangentStraights.empty();

    if (traceTraffic)
    {
        std::cerr
            << "[TrafficPlannerInput]"
            << " start=("
            << r.startMeters.x << ","
            << r.startMeters.y << ","
            << r.startMeters.z << ")"
            << " via_points=" << r.requiredViaPointsMeters.size()
            << " hard_straights=" << r.mandatoryTangentStraights.size()
            << " obstacles=" << r.obstacles.size()
            << " hull_radius_m=" << r.hullRadiusMeters
            << std::endl;

        for (std::size_t i = 0;
             i < r.requiredViaPointsMeters.size();
             ++i)
        {
            const auto& p = r.requiredViaPointsMeters[i];
            std::cerr
                << "[TrafficPlannerInputVia]"
                << " i=" << i
                << " p=("
                << p.x << "," << p.y << "," << p.z << ")"
                << std::endl;
        }

        for (std::size_t i = 0;
             i < r.mandatoryTangentStraights.size();
             ++i)
        {
            const auto& straight = r.mandatoryTangentStraights[i];
            std::cerr
                << "[TrafficPlannerInputStraight]"
                << " i=" << i
                << " inbound=" << (straight.inbound ? 1 : 0)
                << " minimum_m=" << straight.minimumStraightMeters
                << " start=("
                << straight.startMeters.x << ","
                << straight.startMeters.y << ","
                << straight.startMeters.z << ")"
                << " end=("
                << straight.endMeters.x << ","
                << straight.endMeters.y << ","
                << straight.endMeters.z << ")"
                << std::endl;
        }
    }

    struct ExactPlannerStraight
    {
        glm::dvec3 startMeters {0.0};
        glm::dvec3 endMeters {0.0};
        bool inbound = true;
    };
    std::vector<ExactPlannerStraight> exactPlannerStraights;

    const auto samePoint =
        [](const glm::dvec3& a, const glm::dvec3& b)
        {
            return glm::length(a - b) <= 1.0e-6;
        };

    for (const auto& straight : r.mandatoryTangentStraights)
    {
        if (!finite(straight.startMeters) ||
            !finite(straight.endMeters) ||
            !std::isfinite(straight.minimumStraightMeters) ||
            straight.minimumStraightMeters <= 0.0)
        {
            out.failure = "invalid mandatory tangent straight";
            return out;
        }

        const glm::dvec3 delta =
            straight.endMeters - straight.startMeters;
        const double hardLength = glm::length(delta);
        if (hardLength + 1.0e-6 < straight.minimumStraightMeters ||
            hardLength <= 1.0e-6)
        {
            out.failure = "mandatory tangent straight shorter than minimum";
            return out;
        }

        const glm::dvec3 axis = delta / hardLength;
        const double workingExtensionMeters = std::max({
            500.0,
            straight.minimumStraightMeters,
            30.0 * r.hullRadiusMeters
        });

        if (straight.inbound)
        {
            const auto it = std::find_if(
                plannerViaPoints.begin(),
                plannerViaPoints.end(),
                [&](const glm::dvec3& p)
                {
                    return samePoint(p, straight.startMeters);
                }
            );
            if (it == plannerViaPoints.end())
            {
                out.failure =
                    "mandatory inbound straight start missing from via route";
                return out;
            }

            const glm::dvec3 workingJoin =
                straight.startMeters - axis * workingExtensionMeters;

            // The whole working axis from the tangent join to the portal must
            // stay straight. Only the authored final portion is HARD, but if
            // the extension were allowed to approach HARD start from an
            // arbitrary direction, generic corner rounding would consume the
            // HARD segment itself.
            exactPlannerStraights.push_back(
                {workingJoin, straight.endMeters, true}
            );

            if (traceTraffic)
            {
                std::cerr
                    << "[TrafficWorkingAxis]"
                    << " inbound=1"
                    << " working_join=("
                    << workingJoin.x << ","
                    << workingJoin.y << ","
                    << workingJoin.z << ")"
                    << " hard_start=("
                    << straight.startMeters.x << ","
                    << straight.startMeters.y << ","
                    << straight.startMeters.z << ")"
                    << " portal_end=("
                    << straight.endMeters.x << ","
                    << straight.endMeters.y << ","
                    << straight.endMeters.z << ")"
                    << std::endl;
            }

            plannerViaPoints.insert(it, workingJoin);
        }
        else
        {
            const auto it = std::find_if(
                plannerViaPoints.begin(),
                plannerViaPoints.end(),
                [&](const glm::dvec3& p)
                {
                    return samePoint(p, straight.endMeters);
                }
            );
            if (it == plannerViaPoints.end())
            {
                out.failure =
                    "mandatory outbound straight end missing from via route";
                return out;
            }

            const glm::dvec3 workingJoin =
                straight.endMeters + axis * workingExtensionMeters;

            exactPlannerStraights.push_back(
                {straight.startMeters, workingJoin, false}
            );

            if (traceTraffic)
            {
                std::cerr
                    << "[TrafficWorkingAxis]"
                    << " inbound=0"
                    << " portal_start=("
                    << straight.startMeters.x << ","
                    << straight.startMeters.y << ","
                    << straight.startMeters.z << ")"
                    << " hard_end=("
                    << straight.endMeters.x << ","
                    << straight.endMeters.y << ","
                    << straight.endMeters.z << ")"
                    << " working_join=("
                    << workingJoin.x << ","
                    << workingJoin.y << ","
                    << workingJoin.z << ")"
                    << std::endl;
            }

            plannerViaPoints.insert(std::next(it), workingJoin);
        }
    }

    if (traceTraffic)
    {
        for (std::size_t i = 0; i < plannerViaPoints.size(); ++i)
        {
            const auto& p = plannerViaPoints[i];
            std::cerr
                << "[TrafficPlannerVia]"
                << " i=" << i
                << " p=("
                << p.x << "," << p.y << "," << p.z << ")"
                << std::endl;
        }
    }

    const auto outward = glm::normalize(r.outward);
    const auto stop = r.entranceMeters + outward * r.standoffMeters;

    const auto clear = [&](const glm::dvec3& a,const glm::dvec3& b)
    { return world::navigation::segmentClearOfNavigationObstacles(
        a,b,r.obstacles,r.hullRadiusMeters); };

    const auto firstBlockingObstacle =
        [&](const glm::dvec3& a,
            const glm::dvec3& b)
            -> const world::navigation::NavigationObstacle*
        {
            for (const auto& obstacle : r.obstacles)
            {
                if (world::navigation::segmentIntersectsNavigationObstacle(
                        a,
                        b,
                        obstacle,
                        r.hullRadiusMeters))
                {
                    return &obstacle;
                }
            }
            return nullptr;
        };

    glm::dvec3 routeSearchStart=r.startMeters;
    bool initialForwardLeadActive=false;
    double initialForwardAcceptedLeadMeters=0.0;
    double initialForwardProtectedStraightMeters=0.0;
    if(r.hasInitialForward && r.initialForwardLeadMeters>1.0e-6)
    {
        const glm::dvec3 initialForward=glm::normalize(r.initialForward);
        const double desiredLead=r.initialForwardLeadMeters;
        const double minimumLead=std::min(
            desiredLead,
            std::max(25.0,r.hullRadiusMeters*2.0)
        );

        double acceptedLead=desiredLead;
        if(!clear(
                r.startMeters,
                r.startMeters+initialForward*acceptedLead))
        {
            if(!clear(
                    r.startMeters,
                    r.startMeters+initialForward*minimumLead))
            {
                out.failure="initial forward corridor blocked";
                return out;
            }

            double clearLead=minimumLead;
            double blockedLead=desiredLead;
            for(int i=0;i<24;++i)
            {
                const double probe=0.5*(clearLead+blockedLead);
                if(clear(
                        r.startMeters,
                        r.startMeters+initialForward*probe))
                    clearLead=probe;
                else
                    blockedLead=probe;
            }
            acceptedLead=clearLead;
        }

        routeSearchStart=
            r.startMeters+initialForward*acceptedLead;
        initialForwardLeadActive=true;
        initialForwardAcceptedLeadMeters=acceptedLead;

        // The launch contract needs a visibly straight section through the
        // windshield, but the rest of the lead must remain available for a
        // tangent arc into the geometric route. The previous implementation
        // protected the entire lead and therefore created a hard corner at
        // routeSearchStart.
        initialForwardProtectedStraightMeters=std::min(
            acceptedLead,
            std::max(500.0, acceptedLead*0.50)
        );
    }

    // Only the near-port ingress is semantic hard geometry. The much longer
    // manual-Assisted lead is a preference used to make the terminal turn
    // human-flyable. Treating the whole preferred lead as mandatory made any
    // unrelated obstacle several kilometres in front of the port cancel the
    // route before rerouting even started.
    const double mandatoryApproachLengthMeters=std::max(
        700.0,
        3*r.standoffMeters
    );
    const auto mandatoryAlign =
        stop + outward * mandatoryApproachLengthMeters;
    if (!clear(mandatoryAlign,stop))
    {
        out.failure = "dock mandatory ingress blocked";
        return out;
    }

    const double preferredApproachLengthMeters=std::max(
        mandatoryApproachLengthMeters,
        r.terminalApproachLengthMeters
    );

    double finalApproachLengthMeters=preferredApproachLengthMeters;
    auto align=stop+outward*finalApproachLengthMeters;

    if (!clear(align,stop))
    {
        // Segment-clear is monotonic along one ray: once an obstacle is hit,
        // every longer segment contains the same hit. Find the longest clear
        // prefix instead of rejecting the navigation task. This preserves as
        // much room as physically available for the broad terminal fillet.
        double clearLength=mandatoryApproachLengthMeters;
        double blockedLength=preferredApproachLengthMeters;
        for (int i=0;i<24;++i)
        {
            const double probeLength=
                0.5*(clearLength+blockedLength);
            const auto probeAlign=stop+outward*probeLength;
            if (clear(probeAlign,stop))
                clearLength=probeLength;
            else
                blockedLength=probeLength;
        }

        // Do not place the route endpoint on the exact contact
        // boundary. The visibility graph needs room to connect a support node
        // to the docking-axis join without grazing the same inflated
        // obstacle. Back away into the already-proved clear prefix.
        const double axisRejoinMarginMeters=std::max(
            50.0,
            r.hullRadiusMeters*4.0
        );
        finalApproachLengthMeters=std::max(
            mandatoryApproachLengthMeters,
            clearLength-axisRejoinMarginMeters
        );
        align=stop+outward*finalApproachLengthMeters;
        if(!clear(align,stop))
        {
            finalApproachLengthMeters=mandatoryApproachLengthMeters;
            align=mandatoryAlign;
        }
        out.terminalApproachShortened=true;
    }

    out.terminalApproachLengthMeters=finalApproachLengthMeters;

    world::navigation::GeometricPathRequest search;
    search.startMeters = routeSearchStart;
    search.goalMeters = align;
    search.obstacles = r.obstacles;
    search.params.agentRadiusMeters = r.hullRadiusMeters;
    search.params.maxConsideredObstacles = 48;

    const auto exactStraightForLeg =
        [&](const glm::dvec3& legStart,
            const glm::dvec3& legGoal)
            -> const ExactPlannerStraight*
        {
            for (const auto& straight : exactPlannerStraights)
            {
                const glm::dvec3 axisDelta =
                    straight.endMeters - straight.startMeters;
                const double axisLength = glm::length(axisDelta);
                if (axisLength <= 1.0e-9)
                    continue;

                const glm::dvec3 axis = axisDelta / axisLength;
                const auto liesOnAxisInterval =
                    [&](const glm::dvec3& point)
                    {
                        const glm::dvec3 rel =
                            point - straight.startMeters;
                        const double along = glm::dot(rel, axis);
                        const glm::dvec3 cross =
                            rel - axis * along;
                        return glm::length(cross) <= 1.0e-6 &&
                            along >= -1.0e-6 &&
                            along <= axisLength + 1.0e-6;
                    };

                if (liesOnAxisInterval(legStart) &&
                    liesOnAxisInterval(legGoal) &&
                    glm::dot(legGoal - legStart, axis) > 1.0e-6)
                {
                    return &straight;
                }
            }
            return nullptr;
        };

    int trafficPlanGeometryCall = 0;

    const auto planGeometryToGoal =
        [&](const glm::dvec3& routeGoalMeters,
            double additionalClearanceMeters,
            std::size_t maxConsideredObstacles)
    {
        const int traceCall = ++trafficPlanGeometryCall;
        if (traceTraffic)
        {
            std::cerr
                << "[TrafficPlanGeometry]"
                << " call=" << traceCall
                << " clearance_extra_m=" << additionalClearanceMeters
                << " max_obstacles=" << maxConsideredObstacles
                << " search_start=("
                << search.startMeters.x << ","
                << search.startMeters.y << ","
                << search.startMeters.z << ")"
                << " search_goal=("
                << routeGoalMeters.x << ","
                << routeGoalMeters.y << ","
                << routeGoalMeters.z << ")"
                << std::endl;
        }
        const auto planOneLeg =
            [&](const glm::dvec3& legStart,
                const glm::dvec3& legGoal)
            {
                auto query = search;
                query.startMeters = legStart;
                query.goalMeters = legGoal;
                query.params.additionalClearanceMeters =
                    std::max(0.0, additionalClearanceMeters);
                query.params.maxConsideredObstacles =
                    maxConsideredObstacles;
                return world::navigation::GeometricPathPlanner::plan(query);
            };

        if (plannerViaPoints.empty())
            return planOneLeg(search.startMeters, routeGoalMeters);

        world::navigation::GeometricPathResult combined;
        combined.valid = true;
        glm::dvec3 legStart = search.startMeters;

        const auto appendLeg =
            [&](const world::navigation::GeometricPathResult& leg)
            {
                if (!leg.valid || leg.pointsMeters.size() < 2)
                    return false;

                combined.obstacleDetourUsed =
                    combined.obstacleDetourUsed ||
                    leg.obstacleDetourUsed;
                combined.startEscaped =
                    combined.startEscaped || leg.startEscaped;
                combined.goalEscaped =
                    combined.goalEscaped || leg.goalEscaped;
                combined.lengthMeters += leg.lengthMeters;

                for (std::size_t i = 0;
                     i < leg.pointsMeters.size();
                     ++i)
                {
                    if (!combined.pointsMeters.empty() &&
                        glm::length(
                            combined.pointsMeters.back() -
                            leg.pointsMeters[i]
                        ) <= 1.0e-6)
                    {
                        continue;
                    }
                    combined.pointsMeters.push_back(
                        leg.pointsMeters[i]
                    );
                }
                return true;
            };

        for (const auto& via : plannerViaPoints)
        {
            if (glm::length(via - legStart) <= 1.0e-6)
                continue;

            world::navigation::GeometricPathResult leg;
            if (const auto* exactStraight =
                    exactStraightForLeg(legStart, via))
            {
                (void)exactStraight;

                if (!clear(legStart, via))
                {
                    const auto* blocker =
                        firstBlockingObstacle(legStart, via);
                    combined.valid = false;
                    combined.message =
                        "mandatory tangent straight blocked blocker=" +
                        (blocker && !blocker->id.empty()
                            ? blocker->id
                            : std::string("unknown"));
                    combined.pointsMeters.clear();
                    return combined;
                }

                leg.valid = true;
                leg.pointsMeters = {legStart, via};
                leg.lengthMeters = glm::length(via - legStart);
            }
            else
            {
                leg = planOneLeg(legStart, via);
            }

            if (!appendLeg(leg))
            {
                combined.valid = false;
                combined.message =
                    "mandatory traffic leg failed: " +
                    (leg.message.empty()
                        ? std::string("no geometric route")
                        : leg.message);
                combined.pointsMeters.clear();
                return combined;
            }
            if (traceTraffic)
            {
                std::cerr
                    << "[TrafficLeg]"
                    << " call=" << traceCall
                    << " from=("
                    << legStart.x << "," << legStart.y << ","
                    << legStart.z << ")"
                    << " to=("
                    << via.x << "," << via.y << "," << via.z << ")"
                    << " exact_axis="
                    << (exactStraightForLeg(legStart, via) ? 1 : 0)
                    << " points=" << leg.pointsMeters.size()
                    << " length_m=" << leg.lengthMeters
                    << " detour=" << (leg.obstacleDetourUsed ? 1 : 0)
                    << std::endl;

                for (std::size_t pi = 0;
                     pi < leg.pointsMeters.size();
                     ++pi)
                {
                    const auto& p = leg.pointsMeters[pi];
                    std::cerr
                        << "[TrafficLegPoint]"
                        << " call=" << traceCall
                        << " i=" << pi
                        << " p=("
                        << p.x << "," << p.y << "," << p.z << ")"
                        << std::endl;
                }
            }

            legStart = via;
        }

        if (glm::length(routeGoalMeters - legStart) > 1.0e-6)
        {
            const auto leg =
                planOneLeg(legStart, routeGoalMeters);
            if (!appendLeg(leg))
            {
                combined.valid = false;
                combined.message =
                    "terminal leg after mandatory traffic failed: " +
                    (leg.message.empty()
                        ? std::string("no geometric route")
                        : leg.message);
                combined.pointsMeters.clear();
                return combined;
            }
        }

        combined.valid = combined.pointsMeters.size() >= 2;
        if (!combined.valid && combined.message.empty())
            combined.message = "mandatory traffic route too short";
        return combined;
    };

    const auto planGeometry =
        [&](double additionalClearanceMeters,
            std::size_t maxConsideredObstacles)
        {
            return planGeometryToGoal(
                search.goalMeters,
                additionalClearanceMeters,
                maxConsideredObstacles
            );
        };

    auto nominalGeometry = planGeometry(0.0, 48);
    // The geometric planner's obstacle cap is a work bound, never proof that
    // the world has no route. Retry against all static obstacles before
    // declaring even the nominal topology unavailable.
    if (!nominalGeometry.valid && r.obstacles.size() > 48)
        nominalGeometry = planGeometry(0.0, 0);

    // A shortened docking-axis join can still be too close to the obstacle
    // that forced the shortening for a discretized visibility graph to reach
    // it robustly. That is not route impossibility. Retreat the join farther
    // toward the port and retry before giving up the navigation task.
    if (!nominalGeometry.valid && out.terminalApproachShortened)
    {
        double axisRetreatMeters=100.0;
        for (int attempt=0; attempt<8 && !nominalGeometry.valid; ++attempt)
        {
            const double candidateLength=std::max(
                mandatoryApproachLengthMeters,
                finalApproachLengthMeters-axisRetreatMeters
            );
            if(candidateLength>=finalApproachLengthMeters-1.0e-6)
                break;

            const auto candidateAlign=stop+outward*candidateLength;
            if(!clear(candidateAlign,stop))
            {
                axisRetreatMeters*=2.0;
                continue;
            }

            search.goalMeters=candidateAlign;
            auto candidateGeometry=planGeometry(0.0,48);
            if(!candidateGeometry.valid && r.obstacles.size()>48)
                candidateGeometry=planGeometry(0.0,0);

            if(candidateGeometry.valid &&
               candidateGeometry.pointsMeters.size()>=2)
            {
                finalApproachLengthMeters=candidateLength;
                align=candidateAlign;
                out.terminalApproachLengthMeters=
                    finalApproachLengthMeters;
                nominalGeometry=std::move(candidateGeometry);
                break;
            }

            if(candidateLength<=mandatoryApproachLengthMeters+1.0e-6)
                break;
            axisRetreatMeters*=2.0;
        }
    }

    if ((!nominalGeometry.valid ||
         nominalGeometry.pointsMeters.size() < 2) &&
        !r.deriveTerminalTurnRadiusFromVehicle &&
        r.preferredTerminalTurnRadiusMeters <= 0.0)
    {
        out.failure = nominalGeometry.message.empty()
            ? "no collision-free docking route"
            : nominalGeometry.message;
        return out;
    }

    const auto prependInitialForwardLead =
        [&](std::vector<glm::dvec3> points)
        {
            if(!initialForwardLeadActive)
                return points;

            if(points.empty() ||
               glm::length(points.front()-routeSearchStart)>1.0e-6)
                points.insert(points.begin(),routeSearchStart);
            if(glm::length(points.front()-r.startMeters)>1.0e-6)
                points.insert(points.begin(),r.startMeters);
            return points;
        };

    if(nominalGeometry.valid &&
       nominalGeometry.pointsMeters.size()>=2)
    {
        nominalGeometry.pointsMeters=
            prependInitialForwardLead(
                std::move(nominalGeometry.pointsMeters)
            );
    }

    struct RoundedCandidate
    {
        bool valid = false;
        bool initialTurnPresent = false;
        double initialTurnRadiusMeters = 0.0;
        bool terminalTurnPresent = false;
        double terminalTurnRadiusMeters = 0.0;
        double terminalArcRotationDegrees = 0.0;
        double terminalApproachLengthMeters = 0.0;
        double lengthMeters = 0.0;
        std::string failure;
        std::vector<glm::dvec3> samples;
        std::vector<planner::RouteCurveSegment> curves;
    };

    struct StageTransitionArcCandidate
    {
        bool valid = false;
        glm::dvec3 startMeters {0.0};
        glm::dvec3 endMeters {0.0};
        glm::dvec3 centerMeters {0.0};
        glm::dvec3 normal {0.0, 1.0, 0.0};
        double radiusMeters = 0.0;
        double sweepRadians = 0.0;
        double planeRotationRadians = 0.0;
        double tangentErrorRadians =
            std::numeric_limits<double>::infinity();
        std::vector<glm::dvec3> samples;
    };

    const auto stablePerpendicularBasis =
        [](const glm::dvec3& axis)
        {
            const glm::dvec3 seed =
                std::abs(axis.y) < 0.85
                    ? glm::dvec3(0.0, 1.0, 0.0)
                    : glm::dvec3(1.0, 0.0, 0.0);
            const glm::dvec3 a =
                glm::normalize(glm::cross(axis, seed));
            const glm::dvec3 b =
                glm::normalize(glm::cross(axis, a));
            return std::pair<glm::dvec3, glm::dvec3>{a, b};
        };

    const auto rotateVectorAroundAxis =
        [](const glm::dvec3& v,
           const glm::dvec3& axis,
           double angle)
        {
            const double co = std::cos(angle);
            const double si = std::sin(angle);
            return
                v * co +
                glm::cross(axis, v) * si +
                axis * glm::dot(axis, v) * (1.0 - co);
        };

    const auto findRotatingStageTransitionArc =
        [&](const glm::dvec3& neighbourMeters,
            const glm::dvec3& boundaryMeters,
            const glm::dvec3& requiredForward,
            double radiusMeters,
            bool inbound)
        {
            StageTransitionArcCandidate best;
            if (radiusMeters <= 1.0e-6)
                return best;

            const double axisLength = glm::length(requiredForward);
            if (axisLength <= 1.0e-9)
                return best;
            const glm::dvec3 axis = requiredForward / axisLength;

            const auto [basisA, basisB] =
                stablePerpendicularBasis(axis);

            constexpr int PlaneSamples = 72;
            constexpr int SweepSamples = 29;
            constexpr double MinSweepDegrees = 20.0;
            constexpr double MaxSweepDegrees = 160.0;
            constexpr double MaxTangentErrorDegrees = 8.0;
            const double maxTangentError =
                glm::radians(MaxTangentErrorDegrees);

            double bestScore =
                std::numeric_limits<double>::infinity();

            for (int planeIndex = 0;
                 planeIndex < PlaneSamples;
                 ++planeIndex)
            {
                const double phi =
                    (2.0 * std::acos(-1.0)) *
                    static_cast<double>(planeIndex) /
                    static_cast<double>(PlaneSamples);
                const glm::dvec3 normal = glm::normalize(
                    basisA * std::cos(phi) +
                    basisB * std::sin(phi)
                );

                for (int sweepIndex = 0;
                     sweepIndex < SweepSamples;
                     ++sweepIndex)
                {
                    const double sweepDegrees =
                        MinSweepDegrees +
                        (MaxSweepDegrees - MinSweepDegrees) *
                        static_cast<double>(sweepIndex) /
                        static_cast<double>(SweepSamples - 1);
                    const double sweep =
                        glm::radians(sweepDegrees);

                    glm::dvec3 arcStart;
                    glm::dvec3 arcEnd;
                    glm::dvec3 center;
                    glm::dvec3 freeTangent;

                    const glm::dvec3 boundaryRadial =
                        glm::cross(axis, normal) * radiusMeters;

                    if (inbound)
                    {
                        arcEnd = boundaryMeters;
                        center = arcEnd - boundaryRadial;

                        const glm::dvec3 startRadial =
                            rotateVectorAroundAxis(
                                boundaryRadial,
                                normal,
                                -sweep
                            );
                        arcStart = center + startRadial;
                        freeTangent = glm::normalize(
                            glm::cross(normal, startRadial)
                        );

                        const glm::dvec3 line =
                            arcStart - neighbourMeters;
                        const double lineLength = glm::length(line);
                        if (lineLength <= 1.0e-6)
                            continue;

                        const glm::dvec3 lineForward =
                            line / lineLength;
                        const double alignment = std::clamp(
                            glm::dot(lineForward, freeTangent),
                            -1.0,
                            1.0
                        );
                        const double error = std::acos(alignment);
                        if (error > maxTangentError)
                            continue;

                        if (!clear(neighbourMeters, arcStart))
                            continue;
                    }
                    else
                    {
                        arcStart = boundaryMeters;
                        center = arcStart - boundaryRadial;

                        const glm::dvec3 endRadial =
                            rotateVectorAroundAxis(
                                boundaryRadial,
                                normal,
                                sweep
                            );
                        arcEnd = center + endRadial;
                        freeTangent = glm::normalize(
                            glm::cross(normal, endRadial)
                        );

                        const glm::dvec3 line =
                            neighbourMeters - arcEnd;
                        const double lineLength = glm::length(line);
                        if (lineLength <= 1.0e-6)
                            continue;

                        const glm::dvec3 lineForward =
                            line / lineLength;
                        const double alignment = std::clamp(
                            glm::dot(freeTangent, lineForward),
                            -1.0,
                            1.0
                        );
                        const double error = std::acos(alignment);
                        if (error > maxTangentError)
                            continue;

                        if (!clear(arcEnd, neighbourMeters))
                            continue;
                    }

                    const double arcLength =
                        radiusMeters * sweep;
                    const int arcSegments = std::clamp(
                        static_cast<int>(
                            std::ceil(arcLength / 10.0)
                        ),
                        8,
                        512
                    );

                    std::vector<glm::dvec3> samples;
                    samples.reserve(
                        static_cast<std::size_t>(arcSegments) + 1
                    );

                    const glm::dvec3 startRadial =
                        arcStart - center;
                    glm::dvec3 previous = arcStart;
                    bool safe = true;
                    for (int j = 0; j <= arcSegments; ++j)
                    {
                        const double u =
                            static_cast<double>(j) /
                            static_cast<double>(arcSegments);
                        const glm::dvec3 radial =
                            rotateVectorAroundAxis(
                                startRadial,
                                normal,
                                sweep * u
                            );
                        const glm::dvec3 point = center + radial;

                        if (j > 0 && !clear(previous, point))
                        {
                            safe = false;
                            break;
                        }

                        samples.push_back(point);
                        previous = point;
                    }

                    if (!safe || samples.empty())
                        continue;

                    const glm::dvec3 freeLine =
                        inbound
                            ? arcStart - neighbourMeters
                            : neighbourMeters - arcEnd;
                    const double freeLineLength =
                        glm::length(freeLine);
                    const glm::dvec3 lineForward =
                        freeLineLength > 1.0e-9
                            ? freeLine / freeLineLength
                            : freeTangent;
                    const double error = std::acos(
                        std::clamp(
                            glm::dot(lineForward, freeTangent),
                            -1.0,
                            1.0
                        )
                    );

                    // Tangency dominates. A small secondary length/sweep term
                    // breaks ties without making the planner prefer cramped
                    // geometry merely because it is shorter.
                    const double score =
                        error * 100000.0 +
                        freeLineLength * 0.001 +
                        sweep;

                    if (score >= bestScore)
                        continue;

                    bestScore = score;
                    best.valid = true;
                    best.startMeters = arcStart;
                    best.endMeters = arcEnd;
                    best.centerMeters = center;
                    best.normal = normal;
                    best.radiusMeters = radiusMeters;
                    best.sweepRadians = sweep;
                    best.planeRotationRadians = phi;
                    best.tangentErrorRadians = error;
                    best.samples = std::move(samples);
                }
            }

            return best;
        };

    const auto roundGeometry =
        [&](const std::vector<glm::dvec3>& geometryPoints,
            double requiredTerminalRadiusMeters,
            const glm::dvec3& finalPoint)
    {
        RoundedCandidate candidate;
        if (geometryPoints.size() < 2)
        {
            candidate.failure = "geometric route has too few points";
            return candidate;
        }

        const auto appendLineCurve =
            [&](const glm::dvec3& start, const glm::dvec3& end)
            {
                const double length=glm::length(end-start);
                if(length<=1.0e-9)
                    return;

                planner::RouteCurveSegment curve;
                curve.kind=planner::RouteCurveKind::Line;
                curve.startProgressMeters=
                    candidate.curves.empty()
                        ? 0.0
                        : candidate.curves.back().endProgressMeters;
                curve.endProgressMeters=
                    curve.startProgressMeters+length;
                curve.maxSpeedMps=0.8*r.maxSpeedMps;
                curve.startMeters=start;
                curve.endMeters=end;
                curve.startForward=(end-start)/length;
                curve.endForward=curve.startForward;
                candidate.curves.push_back(std::move(curve));
            };

        const auto appendArcCurve =
            [&](const glm::dvec3& start,
                const glm::dvec3& end,
                const glm::dvec3& center,
                const glm::dvec3& normal,
                double radius,
                double sweep)
            {
                if(radius<=1.0e-9 || std::abs(sweep)<=1.0e-12)
                    return;

                planner::RouteCurveSegment curve;
                curve.kind=planner::RouteCurveKind::CircularArc;
                curve.startProgressMeters=
                    candidate.curves.empty()
                        ? 0.0
                        : candidate.curves.back().endProgressMeters;
                curve.endProgressMeters=
                    curve.startProgressMeters+
                    radius*std::abs(sweep);
                const double lateralLimit=
                    std::sqrt(std::max(0.0,r.lateralMps2*radius));
                const double angularLimit=
                    r.maxAngularVelocityRadPerSecond>1.0e-9
                        ? r.maxAngularVelocityRadPerSecond*radius
                        : r.maxSpeedMps;
                curve.maxSpeedMps=std::min({
                    0.8*r.maxSpeedMps,
                    lateralLimit,
                    angularLimit
                });
                curve.startMeters=start;
                curve.endMeters=end;
                curve.arcCenterMeters=center;
                curve.arcNormal=normal;
                curve.arcRadiusMeters=radius;
                curve.arcSweepRadians=sweep;
                const glm::dvec3 startRadial=start-center;
                const glm::dvec3 endRadial=end-center;
                curve.startForward=glm::normalize(
                    glm::cross(normal,startRadial)
                );
                curve.endForward=glm::normalize(
                    glm::cross(normal,endRadial)
                );
                candidate.curves.push_back(std::move(curve));
            };

        auto vertices = geometryPoints;
        if (vertices.empty() ||
            glm::length(vertices.back() - finalPoint) > 1.0e-6)
        {
            vertices.push_back(finalPoint);
        }
        candidate.samples = {vertices.front()};

        if (!r.roundTurns)
        {
            candidate.samples = vertices;
            candidate.lengthMeters = 0.0;
            for (std::size_t i = 1; i < candidate.samples.size(); ++i)
            {
                if (!clear(
                        candidate.samples[i - 1],
                        candidate.samples[i]))
                {
                    const auto* blocker =
                        firstBlockingObstacle(
                            candidate.samples[i - 1],
                            candidate.samples[i]
                        );
                    candidate.failure =
                        "piecewise-straight docking route obstructed blocker=" +
                        (blocker && !blocker->id.empty()
                            ? blocker->id
                            : std::string("unknown"));
                    candidate.samples.clear();
                    return candidate;
                }

                appendLineCurve(
                    candidate.samples[i - 1],
                    candidate.samples[i]
                );
                candidate.lengthMeters += glm::length(
                    candidate.samples[i] - candidate.samples[i - 1]
                );
            }
            candidate.valid =
                candidate.samples.size() >= 2 &&
                candidate.lengthMeters >= 1.0;
            if (!candidate.valid)
                candidate.failure = "route too short";
            return candidate;
        }

        for (std::size_t i=1;i+1<vertices.size();++i)
        {
            const auto stageStraightAtBoundary =
                [&]() -> const ExactPlannerStraight*
                {
                    for (const auto& straight :
                         exactPlannerStraights)
                    {
                        if (straight.inbound &&
                            samePoint(
                                vertices[i],
                                straight.startMeters
                            ))
                        {
                            return &straight;
                        }

                        if (!straight.inbound &&
                            samePoint(
                                vertices[i],
                                straight.endMeters
                            ))
                        {
                            return &straight;
                        }
                    }
                    return nullptr;
                }();

            if (stageStraightAtBoundary)
            {
                const glm::dvec3 axisDelta =
                    stageStraightAtBoundary->endMeters -
                    stageStraightAtBoundary->startMeters;
                const double axisLength = glm::length(axisDelta);

                if (axisLength > 1.0e-9)
                {
                    const glm::dvec3 requiredForward =
                        axisDelta / axisLength;

                    // Manual-friendly stage capture is authored geometry, not
                    // an emergency "fit whatever radius is left" fillet.
                    // Keep this independent from cruise speed; the speed
                    // profile will slow to the accepted curvature.
                    const double comfortRadiusMeters =
                        std::max(
                            150.0,
                            10.0 * r.hullRadiusMeters
                        );

                    const glm::dvec3 neighbour =
                        stageStraightAtBoundary->inbound
                            ? candidate.samples.back()
                            : vertices[i + 1];

                    const auto transition =
                        findRotatingStageTransitionArc(
                            neighbour,
                            vertices[i],
                            requiredForward,
                            comfortRadiusMeters,
                            stageStraightAtBoundary->inbound
                        );

                    if (transition.valid)
                    {
                        if (stageStraightAtBoundary->inbound)
                        {
                            appendLineCurve(
                                candidate.samples.back(),
                                transition.startMeters
                            );
                        }
                        else
                        {
                            appendLineCurve(
                                candidate.samples.back(),
                                vertices[i]
                            );
                            if (candidate.samples.empty() ||
                                glm::length(
                                    candidate.samples.back() -
                                    vertices[i]
                                ) > 1.0e-6)
                            {
                                candidate.samples.push_back(
                                    vertices[i]
                                );
                            }
                        }

                        appendArcCurve(
                            transition.startMeters,
                            transition.endMeters,
                            transition.centerMeters,
                            transition.normal,
                            transition.radiusMeters,
                            transition.sweepRadians
                        );

                        if (stageStraightAtBoundary->inbound)
                        {
                            candidate.samples.insert(
                                candidate.samples.end(),
                                transition.samples.begin(),
                                transition.samples.end()
                            );
                        }
                        else
                        {
                            candidate.samples.insert(
                                candidate.samples.end(),
                                transition.samples.begin() + 1,
                                transition.samples.end()
                            );
                        }

                        std::cerr
                            << "[StageTransitionArc]"
                            << " inbound="
                            << (stageStraightAtBoundary->inbound
                                    ? 1
                                    : 0)
                            << " radius_m="
                            << transition.radiusMeters
                            << " sweep_deg="
                            << glm::degrees(
                                   transition.sweepRadians
                               )
                            << " plane_deg="
                            << glm::degrees(
                                   transition.planeRotationRadians
                               )
                            << " tangent_error_deg="
                            << glm::degrees(
                                   transition.tangentErrorRadians
                               )
                            << " boundary=("
                            << vertices[i].x << ","
                            << vertices[i].y << ","
                            << vertices[i].z << ")"
                            << std::endl;

                        continue;
                    }

                    std::cerr
                        << "[StageTransitionArc]"
                        << " inbound="
                        << (stageStraightAtBoundary->inbound ? 1 : 0)
                        << " radius_m=" << comfortRadiusMeters
                        << " result=no-single-arc-candidate"
                        << " fallback=legacy-fillet"
                        << std::endl;
                }
            }

            const auto a=vertices[i]-vertices[i-1];
            const auto b=vertices[i+1]-vertices[i];
            const double la=glm::length(a), lb=glm::length(b);
            if (la < 1e-6 || lb < 1e-6)
                continue;

            const auto u=a/la,v=b/lb;
            const double cosTurn=std::clamp(glm::dot(u,v),-1.0,1.0);
            if (cosTurn>0.9999)
                continue;

            const double turnAngle=std::acos(cosTurn);
            const double tangentScale=std::tan(turnAngle*0.5);
            const auto turnNormalRaw=glm::cross(u,v);
            const double turnNormalLength=glm::length(turnNormalRaw);
            if (!std::isfinite(tangentScale) || tangentScale<=1.0e-9 ||
                turnNormalLength<=1.0e-9)
            {
                candidate.failure="degenerate docking turn";
                return candidate;
            }

            const bool terminalTurn=(i+1==vertices.size()-1);
            const double authoredCruiseSpeed =
                0.8 * r.maxSpeedMps;
            const double desiredRadius=std::max({
                20.0,
                authoredCruiseSpeed*authoredCruiseSpeed/r.lateralMps2,
                terminalTurn ? requiredTerminalRadiusMeters : 0.0
            });
            // Generic transit fillets are geometry, not a command to hold
            // cruise speed through every bend. The accepted dense route below
            // derives a curvature speed limit and then applies acceleration /
            // braking passes. Rejecting a smaller open-space fillet here based
            // on 0.8 * maxSpeed made a stopped ship unable to plan after any
            // substantial heading change: the 1 km nose-first lead leaves
            // finite tangent room, while the old cruise-radius floor demanded
            // kilometres. Keep the exact terminal primitive strict, but let
            // ordinary transit turns be traversed at the speed their curvature
            // physically permits.
            const double segmentFraction=
                terminalTurn
                    ? r.terminalTurnSegmentFraction
                    : 0.40;
            double tangentDistance=std::min({
                la*segmentFraction,
                lb*segmentFraction,
                desiredRadius*tangentScale
            });

            if(initialForwardLeadActive && i==1)
            {
                const double maximumLaunchCut=std::max(
                    0.0,
                    initialForwardAcceptedLeadMeters-
                        initialForwardProtectedStraightMeters
                );
                tangentDistance=std::min(
                    tangentDistance,
                    maximumLaunchCut
                );

                // If obstacle shortening left no room for a tangent arc, keep
                // the proved straight lead and expose the sharp topology
                // honestly. Normal 500 m launch leads retain hundreds of
                // metres for a smooth first turn.
                if(tangentDistance<0.25)
                {
                    appendLineCurve(
                        candidate.samples.back(),
                        vertices[i]
                    );
                    candidate.samples.push_back(vertices[i]);
                    continue;
                }
            }

            if(terminalTurn &&
               requiredTerminalRadiusMeters>0.0 &&
               tangentDistance/tangentScale+1.0e-6 <
                   requiredTerminalRadiusMeters)
            {
                candidate.failure="preferred terminal turn radius unavailable on candidate";
                return candidate;
            }

            bool rounded=false;
            for (int attempt=0;
                 attempt<12 && tangentDistance>=0.25;
                 ++attempt,tangentDistance*=0.5)
            {
                const double radius=tangentDistance/tangentScale;
                if(terminalTurn &&
                   requiredTerminalRadiusMeters>0.0 &&
                   radius+1.0e-6<requiredTerminalRadiusMeters)
                    break;

                const auto entry=vertices[i]-tangentDistance*u;
                const auto exit=vertices[i]+tangentDistance*v;
                const auto turnNormal=turnNormalRaw/turnNormalLength;
                const auto inwardNormal=glm::normalize(
                    glm::cross(turnNormal,u)
                );
                const auto center=entry+radius*inwardNormal;
                const auto startRadial=entry-center;

                const double arcLength=radius*turnAngle;
                const int arcSegments=std::clamp(
                    static_cast<int>(std::ceil(arcLength/10.0)),
                    4,
                    512
                );

                std::vector<glm::dvec3> arc;
                arc.reserve(static_cast<std::size_t>(arcSegments)+1);
                auto previous=candidate.samples.back();
                bool safe=true;
                for (int j=0;j<=arcSegments;++j)
                {
                    const double t=double(j)/double(arcSegments);
                    const double phi=turnAngle*t;
                    const double cp=std::cos(phi);
                    const double sp=std::sin(phi);
                    const auto radial=
                        startRadial*cp+
                        glm::cross(turnNormal,startRadial)*sp+
                        turnNormal*glm::dot(turnNormal,startRadial)*(1.0-cp);
                    const auto point=center+radial;
                    if (!clear(previous,point))
                    {
                        const auto* blocker =
                            firstBlockingObstacle(previous,point);
                        candidate.failure =
                            "circular route fillet obstructed blocker=" +
                            (blocker && !blocker->id.empty()
                                ? blocker->id
                                : std::string("unknown"));
                        safe=false;
                        break;
                    }
                    arc.push_back(point);
                    previous=point;
                }
                if (safe && !arc.empty() &&
                    glm::length(arc.back()-exit)<=1.0e-5)
                {
                    appendLineCurve(
                        candidate.samples.back(),
                        entry
                    );
                    appendArcCurve(
                        entry,
                        exit,
                        center,
                        turnNormal,
                        radius,
                        turnAngle
                    );
                    candidate.samples.insert(
                        candidate.samples.end(),arc.begin(),arc.end());
                    if(initialForwardLeadActive && i==1)
                    {
                        candidate.initialTurnPresent=true;
                        candidate.initialTurnRadiusMeters=radius;
                    }
                    if (terminalTurn)
                    {
                        candidate.terminalTurnPresent=true;
                        candidate.terminalTurnRadiusMeters=radius;
                    }
                    rounded=true;
                    break;
                }
            }

            if (!rounded)
            {
                if (candidate.failure.empty())
                {
                    candidate.failure =
                        terminalTurn
                            ? "no clearance for terminal circular route fillet"
                            : "no clearance for circular route fillet";
                }
                return candidate;
            }
        }

        // roundGeometry owns only the endpoint explicitly supplied
        // by its caller. The old helper always appended docking HOLD here,
        // which corrupted route-to-entry calls into
        // route -> entry -> HOLD before the exact terminal arc was appended.
        if(candidate.samples.empty() ||
           glm::length(candidate.samples.back()-finalPoint)>1.0e-6)
        {
            if(!candidate.samples.empty())
                appendLineCurve(candidate.samples.back(),finalPoint);
            candidate.samples.push_back(finalPoint);
        }
        for (std::size_t i=1;i<candidate.samples.size();++i)
        {
            if (!clear(candidate.samples[i-1],candidate.samples[i]))
            {
                const auto* blocker =
                    firstBlockingObstacle(
                        candidate.samples[i-1],
                        candidate.samples[i]
                    );
                candidate.failure=
                    "rounded route obstructed blocker=" +
                    (blocker && !blocker->id.empty()
                        ? blocker->id
                        : std::string("unknown"));
                candidate.samples.clear();
                return candidate;
            }
            candidate.lengthMeters +=
                glm::length(candidate.samples[i]-candidate.samples[i-1]);
        }

        if (candidate.lengthMeters<1.0)
        {
            candidate.failure="route too short";
            candidate.samples.clear();
            return candidate;
        }

        candidate.valid=true;
        return candidate;
    };

    // Terminal turn geometry is derived from the state actually handed to
    // Planner, not from a fixed fraction of the ship's top speed. Automatic
    // docking normally arrives here after stop-and-settle, so a stationary
    // craft is no longer forced to reserve a multi-kilometre 400 m/s arc.
    //
    // The hull-scaled floor prevents an absurd pivot-sized path. Once geometry
    // is authored, the dense speed profile below is still free to accelerate
    // on straights and will cap the turn speed by lateral/angular authority.
    const double planningOriginSpeedMps =
        std::clamp(r.initialSpeedMps, 0.0, r.maxSpeedMps);

    // A stopped planning origin says nothing about the radius that is pleasant
    // or even sensible once the craft has accelerated into the route. The old
    // rule derived R from initialSpeedMps, so stop-and-settle collapsed Cobra's
    // terminal arc to roughly four hull radii (~69 m): effectively a hairpin.
    //
    // Author geometry from a design turn speed instead. The speed profile may
    // still slow further for obstacles or refinement, but it must not shrink
    // the authored curve merely because planning began at v=0.
    const double designTurnSpeedMps =
        r.deriveTerminalTurnRadiusFromVehicle
            ? std::clamp(
                  std::max(
                      planningOriginSpeedMps,
                      0.80 * r.maxSpeedMps
                  ),
                  0.5,
                  r.maxSpeedMps
              )
            : std::max(0.5, 0.8 * r.maxSpeedMps);

    // Manual flight needs a visually and physically comfortable arc,
    // not merely a dynamically feasible fillet.  Thirty hull radii keeps the
    // tunnel broad enough to hand-fly while still scaling naturally from
    // Cobra-sized craft to 100-150 m ships. Planner may still search another
    // sector/approach when this preferred radius is obstructed.
    const double minimumTerminalRadiusMeters =
        std::max(150.0, 30.0 * r.hullRadiusMeters);
    const double designLateralRadiusMeters =
        designTurnSpeedMps * designTurnSpeedMps /
        r.lateralMps2;
    const double designAngularRadiusMeters =
        r.deriveTerminalTurnRadiusFromVehicle
            ? designTurnSpeedMps /
                r.maxAngularVelocityRadPerSecond
            : 0.0;
    const double preferredTerminalRadius =
        r.deriveTerminalTurnRadiusFromVehicle
            ? std::max({
                  minimumTerminalRadiusMeters,
                  designLateralRadiusMeters,
                  designAngularRadiusMeters
              })
            : r.preferredTerminalTurnRadiusMeters;

    const double terminalTurnSpeedMps =
        r.deriveTerminalTurnRadiusFromVehicle
            ? std::max(
                  0.5,
                  std::min({
                      designTurnSpeedMps,
                      std::sqrt(
                          r.lateralMps2 *
                          preferredTerminalRadius
                      ),
                      r.maxAngularVelocityRadPerSecond *
                          preferredTerminalRadius
                  })
              )
            : designTurnSpeedMps;
    const double lateralTerminalRadiusMeters =
        terminalTurnSpeedMps * terminalTurnSpeedMps /
        r.lateralMps2;
    const double angularTerminalRadiusMeters =
        r.deriveTerminalTurnRadiusFromVehicle
            ? terminalTurnSpeedMps /
                r.maxAngularVelocityRadPerSecond
            : 0.0;

    if(preferredTerminalRadius>0.0)
    {
        out.terminalTurnSpeedMps = terminalTurnSpeedMps;
        out.terminalTurnLateralRadiusMeters =
            lateralTerminalRadiusMeters;
        out.terminalTurnAngularRadiusMeters =
            angularTerminalRadiusMeters;
        out.terminalTurnRequestedRadiusMeters =
            preferredTerminalRadius;
    }

    const auto estimatedTraversalSeconds =
        [&](const RoundedCandidate& candidate)
        {
            if (!candidate.valid || candidate.samples.size() < 2)
                return std::numeric_limits<double>::infinity();

            const std::size_t count = candidate.samples.size();
            const double cruiseSpeed = std::max(0.5, 0.8 * r.maxSpeedMps);
            std::vector<double> speeds(count, cruiseSpeed);
            speeds.front() =
                std::max(0.0, r.initialSpeedMps);
            speeds.back() = 0.0;

            for (std::size_t i = 1; i + 1 < count; ++i)
            {
                const glm::dvec3 a =
                    candidate.samples[i] - candidate.samples[i - 1];
                const glm::dvec3 b =
                    candidate.samples[i + 1] - candidate.samples[i];
                const double la = glm::length(a);
                const double lb = glm::length(b);
                const double across = glm::length(
                    candidate.samples[i + 1] -
                    candidate.samples[i - 1]
                );
                const double denom = la * lb * across;
                if (denom <= 1.0e-9)
                    continue;

                const double curvature =
                    2.0 * glm::length(glm::cross(a, b)) / denom;
                if (curvature > 1.0e-9)
                {
                    speeds[i] = std::min(
                        speeds[i],
                        std::sqrt(r.lateralMps2 / curvature)
                    );
                }
            }

            for (std::size_t i = count - 1; i > 0; --i)
            {
                const double ds = glm::length(
                    candidate.samples[i] -
                    candidate.samples[i - 1]
                );
                speeds[i - 1] = std::min(
                    speeds[i - 1],
                    std::sqrt(
                        speeds[i] * speeds[i] +
                        2.0 * r.brakingMps2 * ds
                    )
                );
            }

            for (std::size_t i = 1; i < count; ++i)
            {
                const double ds = glm::length(
                    candidate.samples[i] -
                    candidate.samples[i - 1]
                );
                speeds[i] = std::min(
                    speeds[i],
                    std::sqrt(
                        speeds[i - 1] * speeds[i - 1] +
                        2.0 * r.acceleratingMps2 * ds
                    )
                );
            }

            double seconds = 0.0;
            for (std::size_t i = 1; i < count; ++i)
            {
                const double ds = glm::length(
                    candidate.samples[i] -
                    candidate.samples[i - 1]
                );
                const double speedSum = speeds[i - 1] + speeds[i];
                if (speedSum <= 1.0e-6)
                    return std::numeric_limits<double>::infinity();
                seconds += 2.0 * ds / speedSum;
            }
            return seconds;
        };

    // Phase 1: routes without a preferred terminal radius keep the ordinary
    // rounded topology. A preferred docking radius uses a different contract:
    // 'align' is the START OF THE FINAL STRAIGHT, not the virtual sharp corner
    // consumed by a fillet.
    RoundedCandidate selected;
    double selectedTraversalSeconds =
        std::numeric_limits<double>::infinity();
    bool detourUsed=false;
    bool radiusRelaxed=false;

    if (preferredTerminalRadius<=0.0)
    {
        selected=roundGeometry(nominalGeometry.pointsMeters,0.0,stop);
        selectedTraversalSeconds=estimatedTraversalSeconds(selected);
    }

    const auto considerPreferredCandidate =
        [&](RoundedCandidate candidate, bool candidateIsDetour)
        {
            if (!candidate.valid)
                return;

            const double candidateSeconds =
                estimatedTraversalSeconds(candidate);
            const bool better =
                !selected.valid ||
                candidateSeconds + 1.0e-6 <
                    selectedTraversalSeconds ||
                (std::abs(
                     candidateSeconds -
                     selectedTraversalSeconds
                 ) <= 1.0e-6 &&
                 candidate.lengthMeters + 1.0e-6 <
                     selected.lengthMeters);

            if (!better)
                return;

            selected = std::move(candidate);
            selectedTraversalSeconds = candidateSeconds;
            detourUsed = candidateIsDetour;
        };

    // Phase 2: author the terminal circular primitive first, then route TO it.
    //
    // HARD CONTRACT:
    //   route -> preEntry -> entry -> exact quarter-circle(R) -> ALIGN -> HOLD
    //
    // For Assisted docking R is authored geometry, not a preference. Planner
    // may rotate the turn plane around the docking axis and, if the entire
    // ring around the current ALIGN is blocked, move ALIGN farther outward.
    // It must never silently replace the requested arc with a tiny fallback.
    if (preferredTerminalRadius>0.0)
    {
        const double authoredCruiseSpeed =
            terminalTurnSpeedMps;
        const double terminalPrimitiveRadius=
            preferredTerminalRadius;

        const double terminalAngularRateRadPerSecond =
            authoredCruiseSpeed / terminalPrimitiveRadius;
        const double angularRampSeconds =
            r.deriveTerminalTurnRadiusFromVehicle
                ? terminalAngularRateRadPerSecond /
                    r.maxAngularAccelerationRadPerSecond2
                : 0.0;
        const double angularRampDistanceMeters =
            authoredCruiseSpeed * angularRampSeconds;
        out.terminalTurnAngularRampMeters =
            angularRampDistanceMeters;

        // This straight settles the preceding route onto the terminal tangent
        // and provides enough distance for the hull angular rate to ramp up
        // within the real angular-acceleration envelope. It still does not
        // resize or redraw the terminal arc itself.
        const double preArcStraightMeters =
            std::max({
                1000.0,
                angularRampDistanceMeters,
                2.0 * r.hullRadiusMeters
            });

        constexpr int terminalIngressSamples=36;
        constexpr double axisOffsetFactors[] = {
            0.0,
            -0.5, 0.5,
            -1.0, 1.0,
            -2.0, 2.0,
            -4.0, 4.0
        };
        constexpr double twoPi=
            6.283185307179586476925286766559;
        constexpr double halfPi=
            1.5707963267948966192313216916398;

        std::unordered_map<std::string,int> blockerHits;
        const auto recordBlocker =
            [&](const world::navigation::NavigationObstacle* blocker,
                int hits = 1)
            {
                if(!blocker)
                    return;
                const std::string key=
                    blocker->id.empty()
                        ? std::string("unnamed")
                        : blocker->id;
                const int total=(blockerHits[key]+=hits);
                if(total<=out.terminalArcDominantBlockerHits)
                    return;

                out.terminalArcDominantBlockerId=key;
                out.terminalArcDominantBlockerHits=total;
                out.terminalArcDominantBlockerCenterMeters=
                    blocker->centerMeters;
                out.terminalArcDominantBlockerHalfExtentsMeters=
                    blocker->halfExtentsMeters;
                out.terminalArcDominantBlockerRadiusMeters=
                    blocker->radiusMeters;
            };

        // Search the nearest acceptable terminal axis first. If all 36
        // rotations fail, slide ALIGN along the docking axis while preserving
        // the exact same R. Moving inward lets a broad arc peel away before a
        // far-axis blocker; moving outward helps when the local ring itself is
        // crowded. Never trade radius for convenience.
        std::vector<double> testedApproachLengths;
        for(const double offsetFactor : axisOffsetFactors)
        {
            if(selected.valid)
                break;

            const double candidateApproachLengthMeters =
                std::max(
                    mandatoryApproachLengthMeters,
                    finalApproachLengthMeters +
                        offsetFactor*terminalPrimitiveRadius
                );

            bool duplicateAxis=false;
            for(const double tested : testedApproachLengths)
            {
                if(std::abs(
                       tested-candidateApproachLengthMeters
                   )<=1.0e-6)
                {
                    duplicateAxis=true;
                    break;
                }
            }
            if(duplicateAxis)
                continue;
            testedApproachLengths.push_back(
                candidateApproachLengthMeters
            );
            ++out.terminalArcAxisPassesTested;

            const glm::dvec3 candidateAlign =
                stop + outward*candidateApproachLengthMeters;
            const glm::dvec3 finalDirection =
                glm::normalize(stop-candidateAlign);

            // Extending the final straight is legal only while the complete
            // axis remains collision-free. If it is blocked, every rotation
            // at this axis station is impossible for the same concrete reason.
            if(!clear(candidateAlign,stop))
            {
                out.terminalArcCandidatesTested +=
                    terminalIngressSamples;
                out.terminalArcCollisionRejected +=
                    terminalIngressSamples;

                const auto* blocker=
                    firstBlockingObstacle(candidateAlign,stop);
                recordBlocker(blocker,terminalIngressSamples);
                out.terminalArcLastRejection =
                    "final-straight-obstructed blocker=" +
                    (blocker && !blocker->id.empty()
                        ? blocker->id
                        : std::string("unknown"));
                continue;
            }

            const glm::dvec3 basisSeed =
                std::abs(finalDirection.y)<0.90
                    ? glm::dvec3(0.0,1.0,0.0)
                    : glm::dvec3(1.0,0.0,0.0);
            const glm::dvec3 lateralA =
                glm::normalize(
                    glm::cross(finalDirection,basisSeed)
                );
            const glm::dvec3 lateralB =
                glm::normalize(
                    glm::cross(finalDirection,lateralA)
                );

            for(int sample=0;
                sample<terminalIngressSamples;
                ++sample)
            {
                ++out.terminalArcCandidatesTested;

                const double angle=
                    twoPi*double(sample)/
                    double(terminalIngressSamples);
                const glm::dvec3 incoming=
                    lateralA*std::cos(angle)+
                    lateralB*std::sin(angle);

                // Exact quarter-circle ending at candidateAlign:
                //   center = ALIGN - incoming*R
                //   entry  = center - finalDirection*R
                //
                // Tangent(entry)=incoming,
                // tangent(ALIGN)=finalDirection.
                const glm::dvec3 center =
                    candidateAlign -
                    incoming*terminalPrimitiveRadius;
                const glm::dvec3 entry =
                    center -
                    finalDirection*terminalPrimitiveRadius;
                const glm::dvec3 preEntry =
                    entry -
                    incoming*preArcStraightMeters;

                // Terminal-arc search MUST preserve the already
                // assigned traffic topology. Calling the bare geometric
                // planner here used to route directly from start to preEntry,
                // silently discarding BLUE via points and mandatory tangent
                // straights before candidate selection.
                const auto ingressGeometry =
                    planGeometryToGoal(
                        preEntry,
                        0.0,
                        0
                    );
                if(!ingressGeometry.valid ||
                   ingressGeometry.pointsMeters.size()<2)
                {
                    ++out.terminalArcRouteRejected;
                    out.terminalArcLastRejection =
                        "route-to-pre-entry:" +
                        (ingressGeometry.message.empty()
                            ? std::string("invalid")
                            : ingressGeometry.message);
                    continue;
                }
                ++out.terminalArcRouteable;

                auto points=ingressGeometry.pointsMeters;
                points=
                    prependInitialForwardLead(
                        std::move(points)
                    );

                // IMPORTANT: generic rounding ends at ENTRY. It does not know
                // about ALIGN or HOLD and therefore cannot redraw the exact
                // terminal primitive.
                auto transitCandidate =
                    roundGeometry(points,0.0,entry);
                if(!transitCandidate.valid)
                {
                    ++out.terminalArcTransitRejected;
                    out.terminalArcLastRejection =
                        "transit-to-entry:" +
                        transitCandidate.failure;
                    continue;
                }
                if(transitCandidate.samples.empty() ||
                   glm::length(
                       transitCandidate.samples.back()-entry
                   )>1.0e-6)
                {
                    ++out.terminalArcTransitRejected;
                    out.terminalArcLastRejection =
                        "transit-endpoint-mismatch";
                    continue;
                }
                ++out.terminalArcTransitReady;

                RoundedCandidate candidate =
                    std::move(transitCandidate);
                candidate.terminalTurnPresent=true;
                candidate.terminalTurnRadiusMeters=
                    terminalPrimitiveRadius;
                candidate.terminalArcRotationDegrees =
                    angle * 180.0 /
                    3.1415926535897932384626433832795;
                candidate.terminalApproachLengthMeters =
                    candidateApproachLengthMeters;

                const glm::dvec3 turnNormal =
                    glm::normalize(
                        glm::cross(incoming,finalDirection)
                    );
                // Preserve the exact terminal primitive before sampling it.
                // transitCandidate.curves already ends at ENTRY.
                {
                    planner::RouteCurveSegment curve;
                    curve.kind=planner::RouteCurveKind::CircularArc;
                    curve.startProgressMeters=
                        candidate.curves.empty()
                            ? 0.0
                            : candidate.curves.back().endProgressMeters;
                    curve.endProgressMeters=
                        curve.startProgressMeters+
                        terminalPrimitiveRadius*halfPi;
                    curve.maxSpeedMps=terminalTurnSpeedMps;
                    curve.startMeters=entry;
                    curve.endMeters=candidateAlign;
                    curve.startForward=incoming;
                    curve.endForward=finalDirection;
                    curve.arcCenterMeters=center;
                    curve.arcNormal=turnNormal;
                    curve.arcRadiusMeters=terminalPrimitiveRadius;
                    curve.arcSweepRadians=halfPi;
                    candidate.curves.push_back(std::move(curve));
                }
                const glm::dvec3 startRadial =
                    entry-center;
                const double arcLength =
                    terminalPrimitiveRadius*halfPi;
                const int arcSegments=std::clamp(
                    static_cast<int>(
                        std::ceil(arcLength/10.0)
                    ),
                    12,
                    1024
                );

                bool arcClear=true;
                glm::dvec3 previous=entry;
                for(int j=1;j<=arcSegments;++j)
                {
                    const double t=
                        double(j)/double(arcSegments);
                    const double phi=halfPi*t;
                    const double cp=std::cos(phi);
                    const double sp=std::sin(phi);
                    const glm::dvec3 radial=
                        startRadial*cp+
                        glm::cross(
                            turnNormal,
                            startRadial
                        )*sp+
                        turnNormal*
                            glm::dot(
                                turnNormal,
                                startRadial
                            )*
                            (1.0-cp);
                    glm::dvec3 point=
                        center+radial;
                    if(j==arcSegments)
                        point=candidateAlign;

                    if(!clear(previous,point))
                    {
                        const auto* blocker=
                            firstBlockingObstacle(
                                previous,
                                point
                            );
                        recordBlocker(blocker);
                        out.terminalArcLastRejection =
                            "terminal-arc-obstructed blocker=" +
                            (blocker && !blocker->id.empty()
                                ? blocker->id
                                : std::string("unknown"));
                        arcClear=false;
                        break;
                    }

                    candidate.samples.push_back(point);
                    previous=point;
                }

                if(!arcClear)
                {
                    ++out.terminalArcCollisionRejected;
                    continue;
                }

                if(glm::length(
                       candidate.samples.back()-stop
                   )>1.0e-6)
                {
                    const glm::dvec3 lineStart=
                        candidate.samples.back();
                    const double lineLength=
                        glm::length(stop-lineStart);
                    if(lineLength>1.0e-9)
                    {
                        planner::RouteCurveSegment curve;
                        curve.kind=planner::RouteCurveKind::Line;
                        curve.startProgressMeters=
                            candidate.curves.empty()
                                ? 0.0
                                : candidate.curves.back().endProgressMeters;
                        curve.endProgressMeters=
                            curve.startProgressMeters+lineLength;
                        curve.maxSpeedMps=0.8*r.maxSpeedMps;
                        curve.startMeters=lineStart;
                        curve.endMeters=stop;
                        curve.startForward=(stop-lineStart)/lineLength;
                        curve.endForward=curve.startForward;
                        candidate.curves.push_back(std::move(curve));
                    }
                    candidate.samples.push_back(stop);
                }

                candidate.lengthMeters=0.0;
                bool wholeRouteClear=true;
                for(std::size_t i=1;
                    i<candidate.samples.size();
                    ++i)
                {
                    if(!clear(
                            candidate.samples[i-1],
                            candidate.samples[i]))
                    {
                        const auto* blocker=
                            firstBlockingObstacle(
                                candidate.samples[i-1],
                                candidate.samples[i]
                            );
                        recordBlocker(blocker);
                        out.terminalArcLastRejection =
                            "combined-route-obstructed blocker=" +
                            (blocker && !blocker->id.empty()
                                ? blocker->id
                                : std::string("unknown"));
                        wholeRouteClear=false;
                        break;
                    }

                    candidate.lengthMeters +=
                        glm::length(
                            candidate.samples[i]-
                            candidate.samples[i-1]
                        );
                }

                if(!wholeRouteClear)
                {
                    ++out.terminalArcCollisionRejected;
                    continue;
                }

                candidate.valid=true;
                candidate.failure.clear();
                ++out.terminalArcAcceptedCandidates;
                considerPreferredCandidate(
                    std::move(candidate),
                    true
                );
            }
        }

        if(!selected.valid)
        {
            out.failure =
                "no collision-free exact-radius terminal arc";
            return out;
        }
    }

    // No preferred Assisted terminal primitive: ordinary generic routing is
    // still allowed to use its own local rounding policy.
    if (!selected.valid)
    {
        out.failure =
            selected.failure.empty()
                ? "no collision-free docking route"
                : selected.failure;
        return out;
    }

    if(preferredTerminalRadius>0.0)
    {
        finalApproachLengthMeters=
            selected.terminalApproachLengthMeters;
        align=
            stop+outward*finalApproachLengthMeters;
        out.terminalApproachLengthMeters=
            finalApproachLengthMeters;
        out.terminalApproachShortened=
            finalApproachLengthMeters+1.0e-6<
                preferredApproachLengthMeters;
        out.terminalApproachExtended=
            finalApproachLengthMeters>
                preferredApproachLengthMeters+1.0e-6;
    }

    auto& samples=selected.samples;
    out.terminalDetourUsed=detourUsed;
    out.terminalTurnRadiusRelaxed=radiusRelaxed;
    out.terminalTurnRadiusMeters=selected.terminalTurnRadiusMeters;
    out.terminalArcRotationDegrees=
        selected.terminalArcRotationDegrees;
    out.initialTurnPresent=selected.initialTurnPresent;
    out.initialTurnRadiusMeters=selected.initialTurnRadiusMeters;

    // Build the execution path without ever skipping an authored vertex.
    // Global-distance resampling used to place one sample just before a
    // semantic vertex (e.g. terminal align) and the next just after it. The
    // resulting chord silently cut the corner even though Planner had authored
    // the correct geometry. Subdivide EACH authored segment independently and
    // always emit its exact endpoint.
    std::vector<DockingAdvisoryGate> dense;
    const double denseSpacingMeters =
        std::min(10.0,r.gateSpacingMeters);

    if(!samples.empty())
    {
        glm::dvec3 firstForward(0.0,0.0,-1.0);
        if(samples.size()>=2 &&
           glm::length(samples[1]-samples[0])>1.0e-9)
        {
            firstForward=
                glm::normalize(samples[1]-samples[0]);
        }
        dense.push_back({
            samples.front(),
            firstForward,
            r.maxSpeedMps * 0.8
        });
    }

    for(std::size_t i=1;i<samples.size();++i)
    {
        const glm::dvec3 segment=
            samples[i]-samples[i-1];
        const double length=glm::length(segment);
        if(length<=1.0e-9)
            continue;

        const glm::dvec3 direction=segment/length;
        // Sampling density is approximate, not a geometric grid.
        // Split the complete authored segment into N equal pieces so its exact
        // endpoint is always retained. Using floor gives a step close to the
        // requested density without creating a short remainder fragment.
        const int subdivisions=std::max(
            1,
            static_cast<int>(
                std::floor(length/denseSpacingMeters)
            )
        );

        for(int j=1;j<=subdivisions;++j)
        {
            const double t=
                static_cast<double>(j)/
                static_cast<double>(subdivisions);
            dense.push_back({
                j==subdivisions
                    ? samples[i]
                    : glm::mix(samples[i-1],samples[i],t),
                direction,
                r.maxSpeedMps * 0.8
            });
        }
    }

    if(!dense.empty())
        dense.back().speedMps=0.0;

    for (std::size_t i=1;i+1<dense.size();++i)
    {
        const double angle=std::acos(std::clamp(
            glm::dot(dense[i-1].forward,dense[i+1].forward),-1.0,1.0));
        const double span=glm::length(
            dense[i+1].positionMeters-dense[i-1].positionMeters);
        if (angle>1e-6)
            dense[i].speedMps=std::min(
                dense[i].speedMps,
                std::sqrt(r.lateralMps2*span/angle)
            );
    }

    for (std::size_t i=dense.size()-1;i>0;--i)
    {
        const double ds=glm::length(
            dense[i].positionMeters-dense[i-1].positionMeters);
        dense[i-1].speedMps=std::min(
            dense[i-1].speedMps,
            std::sqrt(
                dense[i].speedMps*dense[i].speedMps+
                2*r.brakingMps2*ds
            )
        );
    }

    // Initial speed is measured state, NOT the speed constraint at s=0.
    // Keep the first gate's geometric/braking ceiling intact. Use measured
    // speed only as the seed for forward reachability of later route samples.
    // If the craft is already above the first ceiling, execution must brake
    // while continuing along the accepted route; it must not reinterpret that
    // fact as an impossible zero-speed boundary.
    double reachableSpeedMps =
        std::max(0.0, r.initialSpeedMps);
    for (std::size_t i=1;i<dense.size();++i)
    {
        const double ds=glm::length(
            dense[i].positionMeters-dense[i-1].positionMeters);
        reachableSpeedMps =
            std::sqrt(
                reachableSpeedMps*reachableSpeedMps+
                2*r.acceleratingMps2*ds
            );
        dense[i].speedMps=std::min(
            dense[i].speedMps,
            reachableSpeedMps
        );
        reachableSpeedMps=dense[i].speedMps;
    }

    // Preserve the exact accepted dense route for Automatic execution. The
    // sparse public gates below are only presentation samples of this same
    // product and must never be reinterpreted into a different flight path.
    out.executionGates = dense;
    out.routeCurves = selected.curves;

    if (traceTraffic)
    {
        std::cerr
            << "[TrafficSelectedRoute]"
            << " curves=" << out.routeCurves.size()
            << " dense_gates=" << out.executionGates.size()
            << std::endl;

        for (std::size_t i = 0; i < out.routeCurves.size(); ++i)
        {
            const auto& curve = out.routeCurves[i];
            const char* kind = "unknown";
            switch (curve.kind)
            {
                case planner::RouteCurveKind::Line:
                    kind = "line";
                    break;
                case planner::RouteCurveKind::CircularArc:
                    kind = "arc";
                    break;
                case planner::RouteCurveKind::CubicBezier:
                    kind = "bezier";
                    break;
            }

            std::cerr
                << "[TrafficCurve]"
                << " i=" << i
                << " kind=" << kind
                << " s0=" << curve.startProgressMeters
                << " s1=" << curve.endProgressMeters
                << " start=("
                << curve.startMeters.x << ","
                << curve.startMeters.y << ","
                << curve.startMeters.z << ")"
                << " end=("
                << curve.endMeters.x << ","
                << curve.endMeters.y << ","
                << curve.endMeters.z << ")";

            if (curve.kind == planner::RouteCurveKind::CircularArc)
            {
                std::cerr
                    << " center=("
                    << curve.arcCenterMeters.x << ","
                    << curve.arcCenterMeters.y << ","
                    << curve.arcCenterMeters.z << ")"
                    << " radius_m=" << curve.arcRadiusMeters
                    << " sweep_rad=" << curve.arcSweepRadians;
            }
            else if (curve.kind == planner::RouteCurveKind::CubicBezier)
            {
                std::cerr
                    << " c1=("
                    << curve.bezierControl1Meters.x << ","
                    << curve.bezierControl1Meters.y << ","
                    << curve.bezierControl1Meters.z << ")"
                    << " c2=("
                    << curve.bezierControl2Meters.x << ","
                    << curve.bezierControl2Meters.y << ","
                    << curve.bezierControl2Meters.z << ")";
            }

            std::cerr << std::endl;
        }
    }

    const auto pointOnProtectedLine =
        [&](const glm::dvec3& point,
            const glm::dvec3& axis)
        {
            for (const auto& curve : out.routeCurves)
            {
                if (curve.kind != planner::RouteCurveKind::Line)
                    continue;

                const glm::dvec3 segment =
                    curve.endMeters - curve.startMeters;
                const double segmentLength = glm::length(segment);
                if (segmentLength <= 1.0e-9)
                    continue;

                const glm::dvec3 direction = segment / segmentLength;
                if (std::abs(glm::dot(direction, axis)) < 0.999999)
                    continue;

                const glm::dvec3 rel = point - curve.startMeters;
                const double along = glm::dot(rel, direction);
                if (along < -1.0e-5 ||
                    along > segmentLength + 1.0e-5)
                {
                    continue;
                }

                const glm::dvec3 crossTrack =
                    rel - direction * along;
                if (glm::length(crossTrack) <= 1.0e-5)
                    return true;
            }
            return false;
        };

    for (std::size_t straightIndex = 0;
         straightIndex < r.mandatoryTangentStraights.size();
         ++straightIndex)
    {
        const auto& straight =
            r.mandatoryTangentStraights[straightIndex];
        const glm::dvec3 delta =
            straight.endMeters - straight.startMeters;
        const double length = glm::length(delta);
        if (length <= 1.0e-9)
        {
            out.failure = "accepted route lost mandatory tangent straight";
            out.gates.clear();
            out.executionGates.clear();
            out.routeCurves.clear();
            return out;
        }

        const glm::dvec3 axis = delta / length;
        const std::size_t samples = std::max<std::size_t>(
            2,
            static_cast<std::size_t>(
                std::ceil(length / 25.0)
            ) + 1
        );

        for (std::size_t i = 0; i < samples; ++i)
        {
            const double t =
                static_cast<double>(i) /
                static_cast<double>(samples - 1);
            const glm::dvec3 point =
                straight.startMeters * (1.0 - t) +
                straight.endMeters * t;

            const bool covered =
                pointOnProtectedLine(point, axis);

            if (traceTraffic)
            {
                std::cerr
                    << "[TrafficStraightCheck]"
                    << " straight=" << straightIndex
                    << " sample=" << i
                    << "/" << (samples - 1)
                    << " t=" << t
                    << " covered=" << (covered ? 1 : 0)
                    << " point=("
                    << point.x << "," << point.y << "," << point.z << ")"
                    << std::endl;
            }

            if (!covered)
            {
                std::cerr
                    << "[TrafficStraightFailure]"
                    << " straight=" << straightIndex
                    << " inbound=" << (straight.inbound ? 1 : 0)
                    << " minimum_m=" << straight.minimumStraightMeters
                    << " failed_sample=" << i
                    << " t=" << t
                    << " point=("
                    << point.x << "," << point.y << "," << point.z << ")"
                    << " hard_start=("
                    << straight.startMeters.x << ","
                    << straight.startMeters.y << ","
                    << straight.startMeters.z << ")"
                    << " hard_end=("
                    << straight.endMeters.x << ","
                    << straight.endMeters.y << ","
                    << straight.endMeters.z << ")"
                    << std::endl;

                out.failure =
                    "accepted route bent or removed mandatory tangent straight";
                out.gates.clear();
                out.executionGates.clear();
                out.routeCurves.clear();
                return out;
            }
        }
    }

    std::vector<double> denseProgress(dense.size(),0.0);
    for(std::size_t i=1;i<dense.size();++i)
        denseProgress[i]=denseProgress[i-1]+glm::length(
            dense[i].positionMeters-dense[i-1].positionMeters
        );

    out.gates.push_back(dense.front());
    std::size_t previous=0;
    while (previous+1<dense.size())
    {
        std::size_t next=previous+1;

        // USER-CONTRACT: published docking frames use the authored
        // cadence only. Normal guidance is 500 m and terminal guidance is
        // 250 m for the current docking request. Do not inject a special
        // fractional transition frame merely to land exactly on the dense
        // boundary. Start terminal cadence up to one normal interval early so
        // the final dense region is still fully covered.
        const double remainingFromPrevious=
            denseProgress.back()-denseProgress[previous];
        const double terminalSpacing=std::min(
            r.gateSpacingMeters,
            r.terminalGateSpacingMeters
        );
        const double terminalActivationRemaining=
            r.terminalDenseDistanceMeters+r.gateSpacingMeters;

        const double spacingMeters=
            remainingFromPrevious<=terminalActivationRemaining
                ? terminalSpacing
                : r.gateSpacingMeters;

        while(next+1<dense.size())
        {
            const std::size_t candidate=next+1;
            const double routeDistance=
                denseProgress[candidate]-denseProgress[previous];
            if(routeDistance>spacingMeters+1.0e-6)
                break;
            next=candidate;
        }

        while(next>previous+1 &&
              !clear(dense[previous].positionMeters,dense[next].positionMeters))
            --next;
        if(!clear(dense[previous].positionMeters,dense[next].positionMeters))
        {
            out.failure="display gate chord obstructed";
            out.gates.clear();
            return out;
        }

        auto gate=dense[next];
        for(std::size_t j=previous+1;j<=next;++j)
            gate.speedMps=std::min(gate.speedMps,dense[j].speedMps);
        out.gates.push_back(gate);
        previous=next;
    }

    return out;
}

std::string dockingAdvisoryPlanDiagnosticSummary(
    const DockingAdvisoryPlan& plan
)
{
    return
        " turn_speed_mps=" +
        std::to_string(plan.terminalTurnSpeedMps) +
        " lateral_radius_m=" +
        std::to_string(plan.terminalTurnLateralRadiusMeters) +
        " angular_radius_m=" +
        std::to_string(plan.terminalTurnAngularRadiusMeters) +
        " angular_ramp_m=" +
        std::to_string(plan.terminalTurnAngularRampMeters) +
        " requested_radius_m=" +
        std::to_string(plan.terminalTurnRequestedRadiusMeters) +
        " selected_radius_m=" +
        std::to_string(plan.terminalTurnRadiusMeters) +
        " final_axis_m=" +
        std::to_string(plan.terminalApproachLengthMeters) +
        " axis_extended=" +
        std::to_string(plan.terminalApproachExtended ? 1 : 0) +
        " axis_passes=" +
        std::to_string(plan.terminalArcAxisPassesTested) +
        " rotation_deg=" +
        std::to_string(plan.terminalArcRotationDegrees) +
        " candidates=" +
        std::to_string(plan.terminalArcCandidatesTested) +
        " routeable=" +
        std::to_string(plan.terminalArcRouteable) +
        " route_rejected=" +
        std::to_string(plan.terminalArcRouteRejected) +
        " transit_ready=" +
        std::to_string(plan.terminalArcTransitReady) +
        " transit_rejected=" +
        std::to_string(plan.terminalArcTransitRejected) +
        " collision_rejected=" +
        std::to_string(plan.terminalArcCollisionRejected) +
        " accepted=" +
        std::to_string(plan.terminalArcAcceptedCandidates) +
        " relaxed=" +
        std::to_string(plan.terminalTurnRadiusRelaxed ? 1 : 0) +
        " blocker=" +
        (plan.terminalArcDominantBlockerId.empty()
            ? std::string("none")
            : plan.terminalArcDominantBlockerId) +
        " blocker_hits=" +
        std::to_string(plan.terminalArcDominantBlockerHits) +
        " blocker_center=(" +
        std::to_string(
            plan.terminalArcDominantBlockerCenterMeters.x
        ) + "," +
        std::to_string(
            plan.terminalArcDominantBlockerCenterMeters.y
        ) + "," +
        std::to_string(
            plan.terminalArcDominantBlockerCenterMeters.z
        ) + ")" +
        " blocker_half=(" +
        std::to_string(
            plan.terminalArcDominantBlockerHalfExtentsMeters.x
        ) + "," +
        std::to_string(
            plan.terminalArcDominantBlockerHalfExtentsMeters.y
        ) + "," +
        std::to_string(
            plan.terminalArcDominantBlockerHalfExtentsMeters.z
        ) + ")" +
        " blocker_radius_m=" +
        std::to_string(
            plan.terminalArcDominantBlockerRadiusMeters
        ) +
        " last_rejection=" +
        (plan.terminalArcLastRejection.empty()
            ? std::string("none")
            : plan.terminalArcLastRejection);
}
} // namespace game::navigation
