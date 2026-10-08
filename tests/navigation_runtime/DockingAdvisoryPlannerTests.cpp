#include "src/game/navigation/DockingAdvisoryPlanner.h"
#include "src/game/navigation/DockingAdvisoryCorridor.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/NavigationWorldPredictor.h"
#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/RouteFrameField.h"
#include "src/game/navigation/HubCoMovingFrame.h"
#include <glm/gtx/quaternion.hpp>
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

namespace
{
void printTerminalArcDiagnostics(
    const game::navigation::DockingAdvisoryPlan& plan
)
{
    std::cerr
        << " terminal-arc diagnostics:"
        << game::navigation::
            dockingAdvisoryPlanDiagnosticSummary(plan)
        << "\n";
}
} // namespace

int main()
{
    using namespace game::navigation;

    // Visual frame-field regression: one authoritative route must produce a
    // continuous rotation-minimizing frame independent of HUD sample cadence.
    std::vector<planner::RouteCurveSegment> frameCurves;

    planner::RouteCurveSegment frameLineA;
    frameLineA.kind = planner::RouteCurveKind::Line;
    frameLineA.startProgressMeters = 0.0;
    frameLineA.endProgressMeters = 100.0;
    frameLineA.startMeters = {0.0, 0.0, 0.0};
    frameLineA.endMeters = {100.0, 0.0, 0.0};
    frameLineA.startForward = {1.0, 0.0, 0.0};
    frameLineA.endForward = {1.0, 0.0, 0.0};
    frameCurves.push_back(frameLineA);

    planner::RouteCurveSegment frameBezier;
    frameBezier.kind = planner::RouteCurveKind::CubicBezier;
    frameBezier.startProgressMeters = 100.0;
    frameBezier.endProgressMeters = 250.0;
    frameBezier.startMeters = {100.0, 0.0, 0.0};
    frameBezier.endMeters = {200.0, 100.0, 0.0};
    frameBezier.bezierControl1Meters = {140.0, 0.0, 0.0};
    frameBezier.bezierControl2Meters = {200.0, 60.0, 0.0};
    frameBezier.startForward = {1.0, 0.0, 0.0};
    frameBezier.endForward = {0.0, 1.0, 0.0};
    frameCurves.push_back(frameBezier);

    planner::RouteCurveSegment frameLineB;
    frameLineB.kind = planner::RouteCurveKind::Line;
    frameLineB.startProgressMeters = 250.0;
    frameLineB.endProgressMeters = 350.0;
    frameLineB.startMeters = {200.0, 100.0, 0.0};
    frameLineB.endMeters = {200.0, 200.0, 0.0};
    frameLineB.startForward = {0.0, 1.0, 0.0};
    frameLineB.endForward = {0.0, 1.0, 0.0};
    frameCurves.push_back(frameLineB);

    const auto frameFieldFine =
        RouteFrameField::build(
            frameCurves,
            glm::dvec3(0.0, 0.0, 1.0),
            5.0
        );
    const auto frameFieldCoarse =
        RouteFrameField::build(
            frameCurves,
            glm::dvec3(0.0, 0.0, 1.0),
            25.0
        );

    if (frameFieldFine.size() < 10 || frameFieldCoarse.size() < 4)
    {
        std::cerr << "route frame field did not sample test geometry\n";
        return 105;
    }

    for (std::size_t i = 1; i < frameFieldFine.size(); ++i)
    {
        if (glm::dot(frameFieldFine[i - 1].up, frameFieldFine[i].up) <= 0.0)
        {
            std::cerr << "route frame field changed to opposite up branch\n";
            return 106;
        }
        if (std::abs(
                glm::dot(
                    frameFieldFine[i].forward,
                    frameFieldFine[i].up
                )
            ) > 1.0e-9)
        {
            std::cerr << "route frame field lost orthogonality\n";
            return 107;
        }
    }

    for (double s :
         {0.0, 50.0, 100.0, 125.0, 175.0, 225.0, 250.0, 300.0, 350.0})
    {
        const glm::dvec3 fineUp =
            RouteFrameField::upAtProgress(frameFieldFine, s);
        const glm::dvec3 coarseUp =
            RouteFrameField::upAtProgress(frameFieldCoarse, s);
        if (glm::dot(glm::normalize(fineUp), glm::normalize(coarseUp)) <
            0.9999)
        {
            std::cerr
                << "route frame orientation depends on sampling cadence at s="
                << s << "\n";
            return 108;
        }
    }

    // Terminal anchoring contract: the final frame is not arbitrary.
    // It must inherit the docking aperture up direction at the planning epoch.
    const glm::dvec3 terminalAnchor =
        glm::normalize(glm::dvec3(1.0, 0.0, 1.0));
    const auto terminalAnchoredField =
        RouteFrameField::build(
            frameCurves,
            terminalAnchor,
            10.0
        );
    if (terminalAnchoredField.empty())
    {
        std::cerr << "terminal-anchored route frame field is empty\n";
        return 109;
    }

    const glm::dvec3 terminalForward =
        terminalAnchoredField.back().forward;
    glm::dvec3 expectedTerminalUp =
        terminalAnchor -
        terminalForward *
            glm::dot(terminalAnchor, terminalForward);
    expectedTerminalUp = glm::normalize(expectedTerminalUp);

    if (glm::dot(
            terminalAnchoredField.back().up,
            expectedTerminalUp
        ) < 0.999999)
    {
        std::cerr
            << "terminal route frame is not phase-locked to dock aperture\n";
        return 110;
    }

    // Dynamic visual roll contract: the same dock phase must rotate every
    // tunnel frame by the same signed angle around that frame's own tangent.
    const double quarterTurn = glm::radians(90.0);
    const glm::dvec3 forwardA = glm::normalize(glm::dvec3(0.0, 0.0, -1.0));
    const glm::dvec3 upA = glm::dvec3(0.0, 1.0, 0.0);
    const glm::dvec3 rotatedA =
        RouteFrameField::rotateUpAroundForward(
            forwardA,
            upA,
            quarterTurn
        );
    if (glm::dot(rotatedA, glm::dvec3(1.0, 0.0, 0.0)) < 0.999999)
    {
        std::cerr << "visual tunnel roll phase used wrong sign/axis on straight frame\n";
        return 111;
    }

    const glm::dvec3 forwardB =
        glm::normalize(glm::dvec3(1.0, 1.0, -1.0));
    glm::dvec3 upB =
        glm::dvec3(0.0, 1.0, 0.0) -
        forwardB * glm::dot(glm::dvec3(0.0, 1.0, 0.0), forwardB);
    upB = glm::normalize(upB);
    const glm::dvec3 rotatedB =
        RouteFrameField::rotateUpAroundForward(
            forwardB,
            upB,
            quarterTurn
        );
    if (std::abs(glm::dot(rotatedB, forwardB)) > 1.0e-9 ||
        std::abs(glm::dot(rotatedB, upB)) > 1.0e-6)
    {
        std::cerr << "visual tunnel roll phase was not local to frame tangent\n";
        return 112;
    }

    // Manual guidance must leave a stopped ship through the windshield. The
    // route may turn later, but its first published segment must preserve the
    // authored hull-forward axis.
    DockingAdvisoryRequest forwardLaunch;
    forwardLaunch.startMeters={0.0,0.0,0.0};
    forwardLaunch.entranceMeters={5000.0,0.0,5000.0};
    forwardLaunch.outward={0.0,0.0,1.0};
    forwardLaunch.standoffMeters=300.0;
    forwardLaunch.hullRadiusMeters=10.0;
    forwardLaunch.maxSpeedMps=100.0;
    forwardLaunch.brakingMps2=10.0;
    forwardLaunch.lateralMps2=5.0;
    forwardLaunch.gateSpacingMeters=500.0;
    forwardLaunch.hasInitialForward=true;
    forwardLaunch.initialForward={1.0,0.0,0.0};
    forwardLaunch.initialForwardLeadMeters=1000.0;

    const auto forwardLaunchPlan=
        DockingAdvisoryPlanner::plan(forwardLaunch);
    if(!forwardLaunchPlan.valid() || forwardLaunchPlan.gates.size()<2)
    {
        std::cerr << "forward-launch docking route failed: "
                  << forwardLaunchPlan.failure << "\\n";
        return 31;
    }
    const auto firstPublishedDirection=glm::normalize(
        forwardLaunchPlan.gates[1].positionMeters-
        forwardLaunchPlan.gates[0].positionMeters
    );
    if(glm::dot(firstPublishedDirection,
                glm::normalize(forwardLaunch.initialForward))<0.995)
    {
        std::cerr << "manual docking corridor did not start along hull nose\\n";
        return 32;
    }

    if(forwardLaunchPlan.gates.size()<4)
    {
        std::cerr << "nose-first corridor has too few launch/turn frames\n";
        return 33;
    }

    const auto initialForward=
        glm::normalize(forwardLaunch.initialForward);
    const double firstFrameGap=glm::length(
        forwardLaunchPlan.gates[1].positionMeters-
        forwardLaunchPlan.gates[0].positionMeters
    );
    if(firstFrameGap<490.0 || firstFrameGap>500.0+1.0e-5)
    {
        std::cerr
            << "manual docking launch cadence is not 500 m: "
            << firstFrameGap << "\n";
        return 34;
    }

    // The user-facing corridor is intentionally sparse (500 m), so a large
    // angle between two displayed chords is not proof of a geometric kink.
    // Verify the actual planner product instead: a tangent circular launch
    // fillet must exist, while the sparse corridor must eventually leave the
    // initial hull axis.
    if(!forwardLaunchPlan.initialTurnPresent ||
       !(forwardLaunchPlan.initialTurnRadiusMeters>0.0))
    {
        std::cerr
            << "nose-first route did not author a continuous launch fillet\n";
        return 35;
    }
    // A launch fillet is allowed to be tighter than the radius implied by
    // cruise speed. Dense gate speeds carry the dynamic limit; geometry must
    // not reject an otherwise clear route merely because it must slow down.
    const double launchTurnSpeedLimit =
        std::sqrt(
            forwardLaunch.lateralMps2 *
            forwardLaunchPlan.initialTurnRadiusMeters
        );
    if(!std::isfinite(launchTurnSpeedLimit) ||
       launchTurnSpeedLimit <= 0.0)
    {
        std::cerr << "nose-first route produced invalid launch speed limit\n";
        return 37;
    }

    bool sawLaunchTurn=false;
    double travelled=firstFrameGap;
    for(std::size_t i=1;
        i+1<forwardLaunchPlan.gates.size() && travelled<2500.0;
        ++i)
    {
        const auto segment=
            forwardLaunchPlan.gates[i+1].positionMeters-
            forwardLaunchPlan.gates[i].positionMeters;
        const double segmentLength=glm::length(segment);
        if(segmentLength<=1.0e-6)
            continue;

        const auto direction=segment/segmentLength;
        if(glm::dot(direction,initialForward)<0.98)
            sawLaunchTurn=true;
        travelled+=segmentLength;
    }
    if(!sawLaunchTurn)
    {
        std::cerr
            << "nose-first corridor never transitioned into a launch arc\n";
        return 36;
    }

    // Regression: a stopped Assisted ship may have a high top speed and a
    // heading inherited from an arbitrary previous manoeuvre. The old planner
    // demanded that the first bend support 80% of max speed even though the
    // route starts at zero speed. With a 1 km nose-first lead that made this
    // perfectly open route impossible after turning the ship.
    auto postManeuverLaunch=forwardLaunch;
    postManeuverLaunch.maxSpeedMps=500.0;
    postManeuverLaunch.acceleratingMps2=52.0;
    postManeuverLaunch.brakingMps2=52.0;
    postManeuverLaunch.lateralMps2=52.0;
    const auto postManeuverPlan=
        DockingAdvisoryPlanner::plan(postManeuverLaunch);
    if(!postManeuverPlan.valid() ||
       !postManeuverPlan.initialTurnPresent)
    {
        std::cerr
            << "stopped post-manoeuvre ship could not build a slower launch turn: "
            << postManeuverPlan.failure << "\n";
        return 38;
    }
    const double retiredCruiseRadiusFloor=
        0.50 *
        std::pow(0.8 * postManeuverLaunch.maxSpeedMps,2.0) /
        postManeuverLaunch.lateralMps2;
    if(postManeuverPlan.initialTurnRadiusMeters + 1.0e-6 >=
       retiredCruiseRadiusFloor)
    {
        std::cerr
            << "post-manoeuvre fixture no longer exercises the retired cruise-radius veto: radius="
            << postManeuverPlan.initialTurnRadiusMeters
            << " old_floor=" << retiredCruiseRadiusFloor << "\n";
        return 39;
    }

    DockingAdvisoryRequest r;
    r.startMeters={-10000.0,2500.0,0.0};
    r.entranceMeters={3000.0,350.0,-450.0};
    r.outward={0.0,0.0,-1.0};
    r.hullRadiusMeters=13.0;
    r.maxSpeedMps=150.0;
    r.brakingMps2=10.0;
    r.lateralMps2=5.0;
    r.gateSpacingMeters=500.0;
    world::navigation::NavigationObstacle station;
    station.id="station";
    station.shape=world::navigation::NavigationObstacleShape::Box;
    station.halfExtentsMeters={1150.0,1200.0,1100.0};
    r.obstacles={station};
    const auto result=DockingAdvisoryPlanner::plan(r);
    if (!result.valid() || result.gates.size()<3 ||
        glm::length(result.gates.back().positionMeters-
            (r.entranceMeters+r.standoffMeters*r.outward))>1e-6 ||
        result.gates.back().speedMps!=0.0)
    { std::cerr << "docking advisory failed: " << result.failure << '\n'; return 1; }
    if(result.executionGates.size()<=result.gates.size() ||
       glm::length(result.executionGates.front().positionMeters-
                   result.gates.front().positionMeters)>1e-6 ||
       glm::length(result.executionGates.back().positionMeters-
                   result.gates.back().positionMeters)>1e-6)
    {
        std::cerr << "dense automatic route is not the same accepted advisory path\n";
        return 38;
    }
    bool sawNominal500mGate=false;
    for (std::size_t i=1;i<result.gates.size();++i)
    {
        const double gateDistance=glm::length(
            result.gates[i].positionMeters-result.gates[i-1].positionMeters);
        if(gateDistance>r.gateSpacingMeters+1e-5) return 11;
        if(gateDistance>=490.0 && gateDistance<=500.0+1e-5)
            sawNominal500mGate=true;
        if(!world::navigation::segmentClearOfNavigationObstacles(
            result.gates[i-1].positionMeters,result.gates[i].positionMeters,
            r.obstacles,r.hullRadiusMeters)) return 2;
        if (!std::isfinite(result.gates[i].speedMps)) return 3;
    }
    if(!sawNominal500mGate)
    { std::cerr << "no nominal 500 m advisory gate spacing\n"; return 12; }

    // The final 2 km must already be inside the dense cadence. The planner
    // activates that cadence one terminal interval early, preventing a
    // 500-ish metre sparse chord from crossing the 2 km boundary.
    double remainingPublishedMeters=0.0;
    double nearestBoundaryError=1.0e30;
    for (std::size_t i=result.gates.size()-1;i>0;--i)
    {
        const double gap=glm::length(
            result.gates[i].positionMeters-result.gates[i-1].positionMeters
        );
        nearestBoundaryError=std::min(
            nearestBoundaryError,
            std::abs(
                remainingPublishedMeters-r.terminalDenseDistanceMeters
            )
        );
        if (remainingPublishedMeters<=r.terminalDenseDistanceMeters+1.0e-6 &&
            gap>r.terminalGateSpacingMeters+1.0e-5)
        {
            std::cerr << "terminal advisory gate spacing too sparse: "
                      << gap << "\n";
            return 22;
        }
        remainingPublishedMeters+=gap;
    }
    nearestBoundaryError=std::min(
        nearestBoundaryError,
        std::abs(remainingPublishedMeters-r.terminalDenseDistanceMeters)
    );
    if(nearestBoundaryError>r.terminalGateSpacingMeters+1.0e-5)
    {
        std::cerr << "no guidance frame anchors the 2 km density transition\n";
        return 25;
    }

    // A clean 3D corner into the docking axis must be a genuine circular
    // fillet, not a quadratic Bezier. Use a denser terminal display sample and
    // verify several published gates share the analytically expected radius.
    DockingAdvisoryRequest curved;
    curved.startMeters={-10000.0,0.0,9300.0};
    curved.entranceMeters={0.0,0.0,0.0};
    curved.outward={0.0,0.0,1.0};
    curved.standoffMeters=300.0;
    curved.hullRadiusMeters=10.0;
    curved.maxSpeedMps=100.0;
    curved.brakingMps2=10.0;
    curved.lateralMps2=5.0;
    curved.maxAngularVelocityRadPerSecond=0.04;
    curved.maxAngularAccelerationRadPerSecond2=0.10;
    curved.initialSpeedMps=25.0;
    curved.gateSpacingMeters=500.0;
    curved.terminalGateSpacingMeters=100.0;
    curved.terminalDenseDistanceMeters=2000.0;
    curved.terminalApproachLengthMeters=9000.0;
    curved.terminalTurnSegmentFraction=0.85;
    curved.deriveTerminalTurnRadiusFromVehicle=true;
    const auto curvedPlan=DockingAdvisoryPlanner::plan(curved);
    if(!curvedPlan.valid())
    {
        std::cerr << "circular fillet fixture failed: "
                  << curvedPlan.failure << "\n";
        printTerminalArcDiagnostics(curvedPlan);
        return 23;
    }
    const auto curvedStop=
        curved.entranceMeters+curved.outward*curved.standoffMeters;
    const auto curvedAlign=
        curvedStop+curved.outward*std::max({
            700.0,
            3*curved.standoffMeters,
            curved.terminalApproachLengthMeters
        });
    const auto curvedFinalDirection=glm::normalize(curvedStop-curvedAlign);

    const double expectedDesignTurnSpeed=std::clamp(
        std::max(
            curved.initialSpeedMps,
            0.80*curved.maxSpeedMps
        ),
        0.5,
        curved.maxSpeedMps
    );
    const double expectedDesignLateralRadius=
        expectedDesignTurnSpeed*expectedDesignTurnSpeed/
        curved.lateralMps2;
    const double expectedDesignAngularRadius=
        expectedDesignTurnSpeed/
        curved.maxAngularVelocityRadPerSecond;
    const double expectedTerminalRadius=std::max({
        150.0,
        30.0*curved.hullRadiusMeters,
        expectedDesignLateralRadius,
        expectedDesignAngularRadius
    });
    const double expectedTurnSpeed=std::max(
        0.5,
        std::min({
            expectedDesignTurnSpeed,
            std::sqrt(
                curved.lateralMps2*expectedTerminalRadius
            ),
            curved.maxAngularVelocityRadPerSecond*
                expectedTerminalRadius
        })
    );
    const double expectedAngularRamp=
        expectedTurnSpeed *
        ((expectedTurnSpeed/expectedTerminalRadius) /
         curved.maxAngularAccelerationRadPerSecond2);
    if(std::abs(
           curvedPlan.terminalTurnRadiusMeters-
           expectedTerminalRadius
       )>1.0e-6 ||
       std::abs(
           curvedPlan.terminalTurnSpeedMps-
           expectedTurnSpeed
       )>1.0e-6 ||
       std::abs(
           curvedPlan.terminalTurnAngularRampMeters-
           expectedAngularRamp
       )>1.0e-6)
    {
        std::cerr
            << "Assisted terminal radius not derived from planned speed"
            << " expected_radius=" << expectedTerminalRadius
            << " actual_radius=" << curvedPlan.terminalTurnRadiusMeters
            << " expected_speed=" << expectedTurnSpeed
            << " actual_speed=" << curvedPlan.terminalTurnSpeedMps
            << " expected_angular_ramp=" << expectedAngularRamp
            << " actual_angular_ramp="
            << curvedPlan.terminalTurnAngularRampMeters
            << "\n";
        printTerminalArcDiagnostics(curvedPlan);
        return 26;
    }

    // The semantic final straight starts at curvedAlign. A preferred circular
    // turn is authored BEFORE that point and must not consume the straight.
    // The dense accepted route therefore has to pass through align and then
    // remain on the docking axis to HOLD.
    double nearestAlignDistance=1.0e30;
    std::size_t alignGateIndex=curvedPlan.executionGates.size();
    for(std::size_t i=0;i<curvedPlan.executionGates.size();++i)
    {
        const double distance=glm::length(
            curvedPlan.executionGates[i].positionMeters-curvedAlign
        );
        if(distance<nearestAlignDistance)
        {
            nearestAlignDistance=distance;
            alignGateIndex=i;
        }
    }
    if(nearestAlignDistance>1.0e-6 ||
       alignGateIndex>=curvedPlan.executionGates.size())
    {
        std::cerr << "execution path lost exact terminal align vertex; miss="
                  << nearestAlignDistance << "\n";
        printTerminalArcDiagnostics(curvedPlan);
        return 24;
    }

    auto stoppedCurved=curved;
    stoppedCurved.initialSpeedMps=0.0;
    const auto stoppedCurvedPlan=
        DockingAdvisoryPlanner::plan(stoppedCurved);
    if(!stoppedCurvedPlan.valid() ||
       std::abs(
           stoppedCurvedPlan.terminalTurnRadiusMeters-
           curvedPlan.terminalTurnRadiusMeters
       )>1.0e-6 ||
       std::abs(
           stoppedCurvedPlan.terminalTurnSpeedMps-
           curvedPlan.terminalTurnSpeedMps
       )>1.0e-6)
    {
        std::cerr
            << "stop-and-settle incorrectly collapsed the authored terminal turn"
            << " moving_radius="
            << curvedPlan.terminalTurnRadiusMeters
            << " stopped_radius="
            << stoppedCurvedPlan.terminalTurnRadiusMeters
            << " moving_speed="
            << curvedPlan.terminalTurnSpeedMps
            << " stopped_speed="
            << stoppedCurvedPlan.terminalTurnSpeedMps
            << "\n";
        return 41;
    }

    // Regression: route-to-entry rounding must end at ENTRY. It must never
    // smuggle HOLD into the transit path before the exact terminal primitive.
    for(std::size_t i=0;i<alignGateIndex;++i)
    {
        if(glm::length(
               curvedPlan.executionGates[i].positionMeters-curvedStop
           )<1.0e-6)
        {
            std::cerr
                << "docking HOLD appeared before terminal ALIGN"
                << " gate_index=" << i
                << " align_gate_index=" << alignGateIndex
                << "\n";
            printTerminalArcDiagnostics(curvedPlan);
            return 42;
        }
    }

    // ALIGN is an authored semantic vertex and is now preserved exactly.
    // Therefore final-straight validation must follow ROUTE ORDER, not infer
    // "after align" from one coordinate projection. A perfectly valid detour
    // may cross the plane through ALIGN earlier while still being kilometres
    // off the docking axis.
    if(alignGateIndex+1>=curvedPlan.executionGates.size())
    {
        std::cerr << "terminal align is the last execution gate; no final straight\n";
        printTerminalArcDiagnostics(curvedPlan);
        return 41;
    }

    for(std::size_t i=alignGateIndex+1;
        i<curvedPlan.executionGates.size();
        ++i)
    {
        const auto& a=curvedPlan.executionGates[i-1];
        const auto& b=curvedPlan.executionGates[i];
        const auto segment=b.positionMeters-a.positionMeters;
        if(glm::length(segment)<=1.0e-6)
            continue;

        const double directionDot=
            glm::dot(
                glm::normalize(segment),
                curvedFinalDirection
            );
        const glm::dvec3 fromAxis=
            b.positionMeters-curvedAlign;
        const double along=
            glm::dot(fromAxis,curvedFinalDirection);
        const double crossTrack=
            glm::length(
                fromAxis-curvedFinalDirection*along
            );

        if(directionDot<0.9999 || crossTrack>1.0e-6)
        {
            std::cerr
                << "final straight bent after exact ALIGN"
                << " align_gate_index=" << alignGateIndex
                << " gate_index=" << i
                << " dot=" << directionDot
                << " cross_track_m=" << crossTrack
                << " a=("
                << a.positionMeters.x << ","
                << a.positionMeters.y << ","
                << a.positionMeters.z << ")"
                << " b=("
                << b.positionMeters.x << ","
                << b.positionMeters.y << ","
                << b.positionMeters.z << ")"
                << " align=("
                << curvedAlign.x << ","
                << curvedAlign.y << ","
                << curvedAlign.z << ")"
                << "\n";
            printTerminalArcDiagnostics(curvedPlan);
            return 39;
        }
    }

    // An obstacle may block only the far, preferred part of the 9 km
    // docking-axis lead. That must shorten the available straight lead and
    // continue planning; it is NOT the same thing as blocking the mandatory
    // near-port ingress.
    auto shortenedAxis=curved;
    world::navigation::NavigationObstacle farAxisBlocker;
    farAxisBlocker.id="far_axis_blocker";
    farAxisBlocker.shape=world::navigation::NavigationObstacleShape::Sphere;
    farAxisBlocker.centerMeters=
        curvedStop+curved.outward*8500.0;
    farAxisBlocker.radiusMeters=100.0;
    shortenedAxis.obstacles={farAxisBlocker};

    const auto shortenedAxisPlan=
        DockingAdvisoryPlanner::plan(shortenedAxis);
    const double farAxisInflatedRadius=
        farAxisBlocker.radiusMeters+shortenedAxis.hullRadiusMeters;
    const auto shortenedAxisJoin=
        curvedStop+
        curved.outward*shortenedAxisPlan.terminalApproachLengthMeters;
    const double shortenedAxisJoinClearance=
        glm::length(shortenedAxisJoin-farAxisBlocker.centerMeters)-
        farAxisInflatedRadius;
    if(!shortenedAxisPlan.valid() ||
       !shortenedAxisPlan.terminalApproachShortened ||
       shortenedAxisPlan.terminalTurnRadiusRelaxed ||
       shortenedAxisPlan.terminalTurnRadiusMeters+1.0e-6<
           shortenedAxisPlan.terminalTurnRequestedRadiusMeters ||
       shortenedAxisPlan.terminalApproachLengthMeters>=
           shortenedAxis.terminalApproachLengthMeters-1.0 ||
       shortenedAxisPlan.terminalApproachLengthMeters<=700.0 ||
       shortenedAxisJoinClearance<25.0)
    {
        std::cerr
            << "far preferred-axis blocker cancelled route instead of shortening lead: "
            << shortenedAxisPlan.failure
            << " shortened=" << shortenedAxisPlan.terminalApproachShortened
            << " final_axis_m="
            << shortenedAxisPlan.terminalApproachLengthMeters
            << " radius_m="
            << shortenedAxisPlan.terminalTurnRadiusMeters
            << " join_clearance_m="
            << shortenedAxisJoinClearance
            << "\n";
        printTerminalArcDiagnostics(shortenedAxisPlan);
        return 29;
    }

    // Blocking the actual near-port semantic ingress remains a real failure.
    auto blockedIngress=curved;
    world::navigation::NavigationObstacle ingressBlocker;
    ingressBlocker.id="mandatory_ingress_blocker";
    ingressBlocker.shape=world::navigation::NavigationObstacleShape::Sphere;
    ingressBlocker.centerMeters=
        curvedStop+curved.outward*400.0;
    ingressBlocker.radiusMeters=120.0;
    blockedIngress.obstacles={ingressBlocker};
    const auto blockedIngressPlan=
        DockingAdvisoryPlanner::plan(blockedIngress);
    if(blockedIngressPlan.valid() ||
       blockedIngressPlan.failure!="dock mandatory ingress blocked")
    {
        std::cerr
            << "mandatory ingress blocker was not rejected: "
            << blockedIngressPlan.failure << "\n";
        return 30;
    }

    // Blocking only the selected preferred circular arc must not cancel
    // guidance. Put a small obstacle on the actual accepted turn and require
    // Planner to rotate the same-radius terminal primitive to another side of
    // the docking axis.
    world::navigation::NavigationObstacle arcBlocker;
    arcBlocker.id="terminal_arc_blocker";
    arcBlocker.shape=world::navigation::NavigationObstacleShape::Sphere;
    bool foundArcProbe=false;
    for(std::size_t i=curvedPlan.executionGates.size();i>0;--i)
    {
        const auto& gate=curvedPlan.executionGates[i-1];
        const double alignment=
            glm::dot(glm::normalize(gate.forward),curvedFinalDirection);
        if(alignment<0.95)
        {
            arcBlocker.centerMeters=gate.positionMeters;
            foundArcProbe=true;
            break;
        }
    }
    if(!foundArcProbe)
    {
        std::cerr << "could not locate terminal arc probe point\n";
        return 40;
    }
    arcBlocker.radiusMeters=120.0;

    auto rerouted=curved;
    rerouted.obstacles={arcBlocker};
    const auto reroutedPlan=DockingAdvisoryPlanner::plan(rerouted);
    if(!reroutedPlan.valid() ||
       !reroutedPlan.terminalDetourUsed ||
       reroutedPlan.terminalTurnRadiusRelaxed ||
       reroutedPlan.terminalTurnRadiusMeters+1.0e-6<
           reroutedPlan.terminalTurnRequestedRadiusMeters)
    {
        std::cerr << "preferred terminal arc blocker cancelled instead of rerouting: "
                  << reroutedPlan.failure
                  << " detour=" << reroutedPlan.terminalDetourUsed
                  << " relaxed=" << reroutedPlan.terminalTurnRadiusRelaxed
                  << " radius=" << reroutedPlan.terminalTurnRadiusMeters
                  << "\n";
        printTerminalArcDiagnostics(reroutedPlan);
        return 27;
    }

    // Final-straight length and turn radius are independent geometry. Even a
    // short semantic final axis must keep the vehicle-derived turn radius when
    // open space exists; the arc is moved/rotated instead of being squeezed
    // into the final straight.
    auto tightened=curved;
    tightened.startMeters={-4000.0,0.0,1200.0};
    tightened.terminalApproachLengthMeters=0.0;
    tightened.obstacles.clear();
    const auto tightenedPlan=DockingAdvisoryPlanner::plan(tightened);
    if(!tightenedPlan.valid() ||
       tightenedPlan.terminalTurnRadiusRelaxed ||
       tightenedPlan.terminalTurnRadiusMeters+1.0e-6<
           tightenedPlan.terminalTurnRequestedRadiusMeters)
    {
        std::cerr << "short final straight incorrectly squeezed terminal radius: "
                  << tightenedPlan.failure
                  << " relaxed=" << tightenedPlan.terminalTurnRadiusRelaxed
                  << " radius=" << tightenedPlan.terminalTurnRadiusMeters
                  << "\n";
        printTerminalArcDiagnostics(tightenedPlan);
        return 28;
    }

    world::navigation::NavigationObstacle blocked;
    blocked.shape=world::navigation::NavigationObstacleShape::Sphere;
    blocked.centerMeters=r.startMeters;
    blocked.radiusMeters=100.0;
    r.obstacles.push_back(blocked);
    if(DockingAdvisoryPlanner::plan(r).valid()) return 4;

    // The far fixture has a 27 degree yaw and a second dock behind the ship.
    const glm::dmat3 yaw = glm::dmat3(glm::rotate(
        glm::dmat4(1.0), glm::radians(27.0), glm::dvec3(0.0,1.0,0.0)));
    DockingAdvisoryRequest far = r;
    far.obstacles = {station};
    far.entranceMeters = glm::dvec3(3000.0,350.0,0.0) +
        yaw * glm::dvec3(0.0,0.0,-450.0);
    far.outward = yaw * glm::dvec3(0.0,0.0,-1.0);
    world::navigation::NavigationObstacle nearDock;
    nearDock.id="near_dock";
    nearDock.shape=world::navigation::NavigationObstacleShape::Box;
    nearDock.centerMeters={-3000.0,-250.0,0.0};
    nearDock.halfExtentsMeters={200.0,120.0,600.0};
    world::navigation::NavigationObstacle farDock;
    farDock.id="far_dock";
    farDock.shape=world::navigation::NavigationObstacleShape::Box;
    farDock.centerMeters={3000.0,350.0,0.0};
    farDock.localToWorldBasis=yaw;
    farDock.halfExtentsMeters={190.0,110.0,450.0};
    far.obstacles.push_back(nearDock);
    far.obstacles.push_back(farDock);
    const auto farPlan=DockingAdvisoryPlanner::plan(far);
    if (!farPlan.valid())
    { std::cerr << "far dock failed: " << farPlan.failure << '\n'; return 5; }
    for (std::size_t i=1;i<farPlan.gates.size();++i)
    {
        if(!world::navigation::segmentClearOfNavigationObstacles(
            farPlan.gates[i-1].positionMeters,farPlan.gates[i].positionMeters,
            far.obstacles,far.hullRadiusMeters)) return 6;
    }
    const auto finalDirection=glm::normalize(
        farPlan.gates.back().positionMeters -
        farPlan.gates[farPlan.gates.size()-2].positionMeters);
    if(glm::dot(finalDirection,-far.outward)<0.999 ||
        farPlan.gates.back().speedMps!=0.0) return 7;
    game::navigation::HubSemanticAnchorDefinition portDefinition;
    portDefinition.localPositionMeters={0.0,0.0,-450.0};
    portDefinition.hubModuleId="guidance_dock_cube_a";
    portDefinition.localForward={0.0,0.0,-1.0};
    portDefinition.localUp={0.0,1.0,0.0};
    const glm::dmat4 moduleRotation=glm::rotate(glm::dmat4(1.0),
        glm::radians(27.0),glm::dvec3(0.0,1.0,0.0));
    const auto authoredOmega=game::navigation::hubAttachedAngularVelocityWorld(
        {0.0,0.0,-1.0}, {0.0,1.0,0.0}, {1.0,0.0,0.0},
        {0.0,27.0,0.0}, {0.0,0.0,2.0});
    const auto expectedOmega=glm::dvec3(moduleRotation *
        glm::dvec4(0.0,0.0,glm::radians(2.0),0.0));
    if (glm::length(authoredOmega-expectedOmega)>1e-12)
    { std::cerr << "authored spin axis ignores module yaw\n"; return 8; }
    const auto port=game::navigation::resolveHubSemanticAnchor(
        portDefinition,0,0.0,{3000.0,350.0,0.0},{0.0,0.0,0.0},
        glm::mat4(moduleRotation), authoredOmega);
    const auto later=game::navigation::predictHubSemanticAnchorAt(port,45.0);
    std::cerr << "dock_spin pos=" << glm::length(port.positionMeters-later.positionMeters)
              << " forward=" << glm::length(port.forward()-later.forward())
              << " up_dot=" << glm::dot(port.up(),later.up()) << '\n';
    if (glm::length(port.positionMeters-later.positionMeters)>1e-5 ||
        glm::length(port.forward()-later.forward())>1e-6 ||
        glm::dot(port.up(),later.up())>0.1)
    { std::cerr << "spinning dock pose inconsistent\n"; return 8; }

    // In the orbital fixture the module has no authored Hub-local translation.
    // The fixed gate and the spinning port must be evaluated in one Hub frame;
    // linear world-velocity extrapolation of the anchor loses the curved orbit.
    world::orbits::OrbitalMotion orbit;
    orbit.enabled=true;
    orbit.parentRadiusMeters=6380000.0;
    orbit.altitudeMeters=420000.0;
    orbit.orbitalPeriodSeconds=5400.0;
    const double sourceTime=10.0;
    HubPredictionSource hub;
    hub.systemId=0;
    hub.hubId="earth_orbital_hub";
    hub.sourceUniverseTimeSeconds=sourceTime;
    hub.orbitalMotion=orbit;
    hub.positionMeters=world::orbits::computeOrbitPositionMeters(orbit,sourceTime);
    hub.velocityMps=world::orbits::computeOrbitVelocityMetersPerSecond(orbit,sourceTime);
    const auto radial=glm::normalize(hub.positionMeters-orbit.centerMeters);
    const auto prograde=glm::normalize(hub.velocityMps);
    const auto normal=glm::normalize(glm::cross(prograde,radial));
    hub.orientation=hubVisualOrientation(prograde,radial,normal);
    const auto sourceFrame=NavigationWorldPredictor::predictHubFrameAt(hub,sourceTime);
    const glm::dvec3 moduleOffset={3000.0,350.0,0.0};
    const glm::dvec3 moduleYaw={0.0,27.0,0.0};
    const glm::dvec3 moduleSpin={0.0,0.0,2.0};
    game::simulation::HubAttachmentSnapshot attachment;
    attachment.valid=true;
    attachment.systemId=0;
    attachment.hubId=hub.hubId;
    attachment.moduleId=portDefinition.hubModuleId;
    attachment.localOffsetMeters=moduleOffset;
    attachment.localRotationDeg=moduleYaw;
    attachment.localAngularVelocityDegPerSecond=moduleSpin;
    const auto sourceModule=NavigationWorldPredictor::resolveHubAttachmentAt(
        sourceFrame,sourceTime,moduleOffset,moduleYaw,moduleSpin);
    const auto sourcePort=resolveHubSemanticAnchor(
        portDefinition,0,sourceTime,sourceModule.positionMeters,
        sourceModule.velocityMps,sourceModule.orientation,
        sourceModule.angularVelocityWorldRadPerSecond);
    const auto sourceLocal=resolveDockingAdvisoryLocalPortAt(
        attachment,portDefinition,sourceTime);
    if (!sourceLocal.valid ||
        glm::length(sourceFrame.worldToLocalPosition(sourcePort.positionMeters)-
                    sourceLocal.positionMeters)>0.01 ||
        glm::length(sourceFrame.worldToLocalVector(sourcePort.forward())-
                    sourceLocal.forward)>1e-5) return 16;
    const double standoff=300.0;
    const auto fixedGateLocal=
        sourceLocal.positionMeters+standoff*sourceLocal.forward;
    for (double elapsed : {1.0,45.0})
    {
        const double time=sourceTime+elapsed;
        const auto frame=NavigationWorldPredictor::predictHubFrameAt(hub,time);
        const auto module=NavigationWorldPredictor::resolveHubAttachmentAt(
            frame,time,moduleOffset,moduleYaw,moduleSpin);
        const auto predictedPort=resolveHubSemanticAnchor(
            portDefinition,0,time,module.positionMeters,module.velocityMps,
            module.orientation,module.angularVelocityWorldRadPerSecond);
        const auto localPort=resolveDockingAdvisoryLocalPortAt(
            attachment,portDefinition,time);
        if (!localPort.valid) return 17;
        const double axisError=glm::length(fixedGateLocal-
            (localPort.positionMeters+standoff*localPort.forward));
        const double worldToLocalError=glm::length(
            frame.worldToLocalPosition(predictedPort.positionMeters)-
            localPort.positionMeters);
        const double relativeModuleSpeed=glm::length(
            frame.worldToLocalVelocity(module.positionMeters,module.velocityMps));
        if (!frame.valid || !module.valid || axisError>0.05 ||
            worldToLocalError>0.01 ||
            relativeModuleSpeed>1e-7 ||
            glm::length(predictedPort.forward()-sourcePort.forward())>0.05)
        { std::cerr << "orbital dock axis drift=" << axisError
                    << " relative_speed=" << relativeModuleSpeed << '\n'; return 13; }
        if (elapsed==45.0)
        {
            const auto linearPort=predictHubSemanticAnchorAt(sourcePort,time);
            const double oldError=glm::length(
                frame.localToWorldPosition(fixedGateLocal)-
                (linearPort.positionMeters+standoff*linearPort.forward()));
            if(oldError<=2.0)
            { std::cerr << "orbital regression fixture did not reproduce drift\n"; return 14; }
            auto offAxisAttachment=attachment;
            offAxisAttachment.localAngularVelocityDegPerSecond={0.0,2.0,0.0};
            const auto offAxisPort=resolveDockingAdvisoryLocalPortAt(
                offAxisAttachment,portDefinition,time);
            if (!offAxisPort.valid ||
                glm::length(fixedGateLocal-
                    (offAxisPort.positionMeters+standoff*offAxisPort.forward))<=2.0)
            { std::cerr << "off-axis dock motion escaped the guard\n"; return 15; }
        }
    }
    // Cockpit presentation lags behind the current update. A fixed local
    // gate and a rendered ship must be projected through that same epoch;
    // mixing their world positions displaces the tunnel by hundreds of m.
    const auto currentFrame=NavigationWorldPredictor::predictHubFrameAt(
        hub,sourceTime+1.0);
    const auto renderFrame=NavigationWorldPredictor::predictHubFrameAt(
        hub,sourceTime+0.9);
    const glm::dvec3 shipLocal=fixedGateLocal+glm::dvec3(0.0,0.0,1000.0);
    const auto renderedShipWorld=renderFrame.localToWorldPosition(shipLocal);
    const auto cockpitRelative=
        renderFrame.localToWorldPosition(fixedGateLocal)-renderedShipWorld;
    const auto expectedRelative=renderFrame.localToWorldVector(
        fixedGateLocal-shipLocal);
    const auto mixedEpochRelative=
        currentFrame.localToWorldPosition(fixedGateLocal)-renderedShipWorld;
    if (glm::length(cockpitRelative-expectedRelative)>1e-6 ||
        glm::length(mixedEpochRelative-expectedRelative)<100.0 ||
        glm::length(currentFrame.worldToLocalPosition(renderedShipWorld)-
                    shipLocal)<100.0)
    { std::cerr << "mixed-epoch Hub gate/ship regression failed\n"; return 18; }
    // The Hub map currently seeds its own co-moving frame from replicated
    // orbital position/velocity. Its projection must stay equivalent to the
    // canonical Hub predictor for a fixed local advisory gate.
    const auto mapSeed=makeHubCoMovingFrameSeed(
        hub.systemId,hub.hubId,sourceTime,hub.positionMeters,hub.velocityMps,
        orbit.centerMeters,{0.0,0.0,0.0},prograde,radial,normal);
    for (double elapsed : {1.0,45.0,120.0})
    {
        const auto canonical=NavigationWorldPredictor::predictHubFrameAt(
            hub,sourceTime+elapsed);
        const auto mapFrame=predictHubCoMovingFrameAt(mapSeed,sourceTime+elapsed);
        if (!mapFrame.valid ||
            glm::length(canonical.localToWorldPosition(fixedGateLocal)-
                        mapFrame.localToWorldPosition(fixedGateLocal))>0.1)
        { std::cerr << "Hub map frame diverged from advisory frame\n"; return 19; }
    }

    // A pilot can ask to see a route while still outside its first gate.
    // The dock opening constrains only the final approach; after entering,
    // crossing the flight corridor boundary cancels the advisory.
    const auto transit=dockingAdvisoryCrossSection(8000.0,34.0,21.0);
    const auto staging=dockingAdvisoryCrossSection(350.0,34.0,21.0);
    const auto terminal=dockingAdvisoryCrossSection(0.0,34.0,21.0);
    if (transit.lateralToleranceMeters!=60.0 ||
        transit.verticalToleranceMeters!=60.0 ||
        !(staging.lateralToleranceMeters<60.0 &&
          staging.lateralToleranceMeters>34.0) ||
        terminal.lateralToleranceMeters!=34.0 ||
        terminal.verticalToleranceMeters!=21.0)
    { std::cerr << "corridor should narrow only near dock\n"; return 9; }
    if (dockingAdvisoryFrameExtentMeters(28.0,34.0)!=96.0 ||
        dockingAdvisoryFrameExtentMeters(12.0,21.0)!=54.0)
    { std::cerr << "corridor frame extent lost ship+tolerance semantics\n"; return 10; }
    const auto release=dockingAdvisoryReleaseCrossSection(transit);
    if (release.lateralToleranceMeters!=120.0 ||
        release.verticalToleranceMeters!=120.0 ||
        !dockingAdvisoryNearBoundary(49.0,0.0,0.0,transit,100.0) ||
        dockingAdvisoryNearBoundary(10.0,10.0,10.0,transit,100.0))
    { std::cerr << "corridor warning/release bands failed\n"; return 20; }
    DockingAdvisoryCorridorTracker tracking;
    if (tracking.observe(false,false,0.0)!=
            DockingAdvisoryTrackingResult::AwaitingEntry ||
        tracking.observe(true,true,0.1)!=DockingAdvisoryTrackingResult::Inside ||
        tracking.observe(false,true,0.2)!=DockingAdvisoryTrackingResult::Warning ||
        tracking.observe(false,false,0.3)!=DockingAdvisoryTrackingResult::Warning ||
        tracking.observe(false,false,0.7)!=DockingAdvisoryTrackingResult::Warning ||
        tracking.observe(false,false,1.31)!=DockingAdvisoryTrackingResult::Left ||
        tracking.observe(true,true,1.40)!=DockingAdvisoryTrackingResult::Inside)
    { std::cerr << "corridor warning/leave/reentry semantics failed\n"; return 21; }

    const double recoveryReserve =
        DockingAutomaticRecoveryPolicy::linearFeedbackReserveMps2(
            73.549875,
            73.549875,
            73.549875
        );
    if (std::abs(recoveryReserve - 14.709975) > 1.0e-6 ||
        DockingAutomaticRecoveryPolicy::holdCaptureDistanceMeters(25.0) !=
            25.0 ||
        !DockingAutomaticRecoveryPolicy::canCaptureHoldWhileBraking(
            0.05, 25.0, 4.38238, 58.0
        ) ||
        DockingAutomaticRecoveryPolicy::canCaptureHoldWhileBraking(
            24.9, 25.0, 20.0, 58.0
        ) ||
        !DockingAutomaticRecoveryPolicy::recoverableDynamicExcursion(
            24.6798, 25.0,
            8.02537, 8.0,
            glm::radians(3.33), glm::radians(15.0),
            0.381041, 0.25,
            0.40, 3.0
        ) ||
        DockingAutomaticRecoveryPolicy::recoverableDynamicExcursion(
            25.01, 25.0,
            1.0, 8.0,
            0.01, glm::radians(15.0),
            0.01, 0.25,
            0.01, 3.0
        ))
    {
        std::cerr
            << "automatic docking recovery policy lost safe in-place correction semantics\n";
        return 39;
    }
    std::cout << "FAR DOCK PASS gates=" << farPlan.gates.size() << '\n';
    std::cout << "DOCK ADVISORY PASS gates=" << result.gates.size() << '\n';
}
