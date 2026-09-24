#include "src/game/navigation/DockingAdvisoryPlanner.h"
#include "src/game/navigation/DockingAdvisoryCorridor.h"
#include "src/game/navigation/HubSemanticAnchor.h"
#include "src/game/navigation/HubFrameBasis.h"
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
    DockingAdvisoryCorridorTracker tracking;
    if (tracking.observe(false)!=DockingAdvisoryTrackingResult::AwaitingEntry ||
        tracking.observe(true)!=DockingAdvisoryTrackingResult::Inside ||
        tracking.observe(false)!=DockingAdvisoryTrackingResult::Left)
    { std::cerr << "corridor entry/exit semantics failed\n"; return 10; }
    std::cout << "FAR DOCK PASS gates=" << farPlan.gates.size() << '\n';
    std::cout << "DOCK ADVISORY PASS gates=" << result.gates.size() << '\n';
}
