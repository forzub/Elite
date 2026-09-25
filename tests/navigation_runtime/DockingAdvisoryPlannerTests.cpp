#include "src/game/navigation/DockingAdvisoryPlanner.h"
#include "src/game/navigation/DockingAdvisoryCorridor.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/navigation/NavigationWorldPredictor.h"
#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/HubCoMovingFrame.h"
#include <glm/gtx/quaternion.hpp>
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

int main()
{
    using namespace game::navigation;
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
    station.halfExtentsMeters={1100.0,1200.0,1100.0};
    r.obstacles={station};
    const auto result=DockingAdvisoryPlanner::plan(r);
    if (!result.valid() || result.gates.size()<3 ||
        glm::length(result.gates.back().positionMeters-
            (r.entranceMeters+r.standoffMeters*r.outward))>1e-6 ||
        result.gates.back().speedMps!=0.0)
    { std::cerr << "docking advisory failed: " << result.failure << '\n'; return 1; }
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
    curved.startMeters={-4000.0,0.0,2500.0};
    curved.entranceMeters={0.0,0.0,0.0};
    curved.outward={0.0,0.0,1.0};
    curved.standoffMeters=300.0;
    curved.hullRadiusMeters=10.0;
    curved.maxSpeedMps=100.0;
    curved.brakingMps2=10.0;
    curved.lateralMps2=5.0;
    curved.gateSpacingMeters=500.0;
    curved.terminalGateSpacingMeters=100.0;
    curved.terminalDenseDistanceMeters=2000.0;
    curved.terminalApproachLengthMeters=3000.0;
    curved.terminalTurnSegmentFraction=0.75;
    curved.preferredTerminalTurnRadiusMeters=1500.0;
    const auto curvedPlan=DockingAdvisoryPlanner::plan(curved);
    if(!curvedPlan.valid())
    {
        std::cerr << "circular fillet fixture failed: "
                  << curvedPlan.failure << "\n";
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
    const auto incomingRaw=curvedAlign-curved.startMeters;
    const auto outgoingRaw=curvedStop-curvedAlign;
    const double incomingLength=glm::length(incomingRaw);
    const double outgoingLength=glm::length(outgoingRaw);
    const auto incoming=incomingRaw/incomingLength;
    const auto outgoing=outgoingRaw/outgoingLength;
    const double turnAngle=std::acos(std::clamp(
        glm::dot(incoming,outgoing),-1.0,1.0));
    const double tangentScale=std::tan(turnAngle*0.5);
    const double desiredRadius=std::max(
        20.0,
        curved.maxSpeedMps*curved.maxSpeedMps/curved.lateralMps2
    );
    const double tangentDistance=std::min({
        incomingLength*curved.terminalTurnSegmentFraction,
        outgoingLength*curved.terminalTurnSegmentFraction,
        desiredRadius*tangentScale
    });
    const double expectedRadius=tangentDistance/tangentScale;
    if(expectedRadius<1500.0)
    {
        std::cerr << "manual Assisted terminal radius too small: "
                  << expectedRadius << "\n";
        return 26;
    }
    const auto entry=curvedAlign-tangentDistance*incoming;
    const auto turnNormal=glm::normalize(glm::cross(incoming,outgoing));
    const auto inwardNormal=glm::normalize(glm::cross(turnNormal,incoming));
    const auto circleCenter=entry+expectedRadius*inwardNormal;
    std::size_t gatesOnCircle=0;
    for(const auto& gate:curvedPlan.gates)
    {
        if(std::abs(glm::length(gate.positionMeters-circleCenter)-
                    expectedRadius)<0.5)
            ++gatesOnCircle;
    }
    if(gatesOnCircle<4)
    {
        std::cerr << "terminal turn is not sampled as a circular fillet; count="
                  << gatesOnCircle << "\n";
        return 24;
    }

    // Blocking only the preferred circular arc must not cancel manual
    // guidance. Planner first changes coarse topology and keeps the preferred
    // radius if a wider collision-free approach exists.
    const auto startRadial=entry-circleCenter;
    const double midPhi=turnAngle*0.5;
    const auto midRadial=
        startRadial*std::cos(midPhi)+
        glm::cross(turnNormal,startRadial)*std::sin(midPhi)+
        turnNormal*glm::dot(turnNormal,startRadial)*
            (1.0-std::cos(midPhi));
    world::navigation::NavigationObstacle arcBlocker;
    arcBlocker.id="terminal_arc_blocker";
    arcBlocker.shape=world::navigation::NavigationObstacleShape::Sphere;
    arcBlocker.centerMeters=circleCenter+midRadial;
    arcBlocker.radiusMeters=120.0;

    auto rerouted=curved;
    rerouted.obstacles={arcBlocker};
    const auto reroutedPlan=DockingAdvisoryPlanner::plan(rerouted);
    if(!reroutedPlan.valid() ||
       !reroutedPlan.terminalDetourUsed ||
       reroutedPlan.terminalTurnRadiusRelaxed ||
       reroutedPlan.terminalTurnRadiusMeters+1.0e-6<
           rerouted.preferredTerminalTurnRadiusMeters)
    {
        std::cerr << "preferred terminal arc blocker cancelled instead of rerouting: "
                  << reroutedPlan.failure
                  << " detour=" << reroutedPlan.terminalDetourUsed
                  << " relaxed=" << reroutedPlan.terminalTurnRadiusRelaxed
                  << " radius=" << reroutedPlan.terminalTurnRadiusMeters
                  << "\n";
        return 27;
    }

    // If no topology can physically fit the preferred radius because the
    // semantic final-axis segment is too short, guidance must still survive.
    // The planner accepts the widest feasible terminal arc rather than
    // treating the preference as a task-failure threshold.
    auto tightened=curved;
    tightened.startMeters={-4000.0,0.0,1200.0};
    tightened.terminalApproachLengthMeters=0.0;
    tightened.obstacles.clear();
    const auto tightenedPlan=DockingAdvisoryPlanner::plan(tightened);
    if(!tightenedPlan.valid() ||
       !tightenedPlan.terminalTurnRadiusRelaxed ||
       tightenedPlan.terminalTurnRadiusMeters<=0.0 ||
       tightenedPlan.terminalTurnRadiusMeters+1.0e-6>=
           tightened.preferredTerminalTurnRadiusMeters)
    {
        std::cerr << "unavailable preferred radius cancelled instead of tightening: "
                  << tightenedPlan.failure
                  << " relaxed=" << tightenedPlan.terminalTurnRadiusRelaxed
                  << " radius=" << tightenedPlan.terminalTurnRadiusMeters
                  << "\n";
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
    if (release.lateralToleranceMeters!=75.0 ||
        release.verticalToleranceMeters!=75.0 ||
        !dockingAdvisoryNearBoundary(49.0,0.0,0.0,transit,100.0) ||
        dockingAdvisoryNearBoundary(10.0,10.0,10.0,transit,100.0))
    { std::cerr << "corridor warning/release bands failed\n"; return 20; }
    DockingAdvisoryCorridorTracker tracking;
    if (tracking.observe(false,false,0.0)!=
            DockingAdvisoryTrackingResult::AwaitingEntry ||
        tracking.observe(true,true,0.1)!=DockingAdvisoryTrackingResult::Inside ||
        tracking.observe(false,true,0.2)!=DockingAdvisoryTrackingResult::Warning ||
        tracking.observe(false,false,0.3)!=DockingAdvisoryTrackingResult::Warning ||
        tracking.observe(false,false,0.7)!=DockingAdvisoryTrackingResult::Left)
    { std::cerr << "corridor warning/hysteresis semantics failed\n"; return 21; }
    std::cout << "FAR DOCK PASS gates=" << farPlan.gates.size() << '\n';
    std::cout << "DOCK ADVISORY PASS gates=" << result.gates.size() << '\n';
}
