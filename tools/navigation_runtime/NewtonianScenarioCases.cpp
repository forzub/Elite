// DEV offline regression scenarios for the Newtonian route autopilot.
//
//   EliteNewtonianScenarios <out dir> <I case> <Y case> <X case> [name...]
//
// Each built-in scenario is planned with the PRODUCTION Newtonian planning
// pipeline (window crossing, SegmentedRouteComposer / NewtonianRoutePlanner,
// the post-plan ship KeepInside check), exactly as SpaceState plans a
// Newtonian route, and written as a replay case per physical model:
//   <out>/<scenario>-<I|Y|X>.case   (input of EliteNewtonianReplay)
//   <out>/<scenario>-<I|Y|X>.geom   (volumes/obstacles/hull for matrix.py)
// The physics of each model comes from a game-dumped case; the hull from the
// production Cobra descriptor. A scenario the planner rejects writes only
// the .geom file with "plan rejected <reason>".
//
// The route request's capability numbers mirror SpaceState's Newtonian
// branch (DEV copy; they move to one shared capability snapshot later).

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/DockingRouteEntryPolicy.h"
#include "src/game/navigation/ManeuverCapabilityAdapters.h"
#include "src/game/navigation/NavigationVehicleProfileAdapters.h"
#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/game/navigation/VehicleGuidanceEnvelope.h"
#include "src/game/navigation/diagnostics/AutopilotCaseFile.h"
#include "src/game/navigation/planner/newtonian/NewtonianPlanningPipeline.h"
#include "src/game/navigation/traffic/NarrowZoneEnvelope.h"
#include "src/game/navigation/traffic/TrafficWindowCrossing.h"
#include "src/game/ship/descriptors/EliteCobraMk1.h"

namespace
{

using namespace game::navigation;
using traffic::CompiledMandatoryTangentStraight;
using traffic::CompiledNavigationVolumeSection;
using traffic::CompiledTrafficRoute;
using traffic::CompiledTrafficStage;

// A straight authored volume block: entry portal -> exit portal with the
// protected boundary straights, as RouteStageCompiler resolves it.
struct Block
{
    glm::dvec3 entry {0.0};
    glm::dvec3 exit {0.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    bool rectangle = false;
    double radius = 0.0;
    double halfWidth = 0.0;
    double halfHeight = 0.0;
    double clearance = 3.0;
    double inbound = 700.0;
    double outbound = 150.0;
    double maxSpeed = 0.0;
};

struct Sphere
{
    glm::dvec3 center {0.0};
    double radius = 0.0;
};

struct Scenario
{
    std::string name;
    std::string purpose;
    glm::dvec3 start {0.0};
    glm::dvec3 hullForward {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    glm::dvec3 velocity {0.0};
    std::vector<glm::dvec3> via;      // free routes only
    std::vector<Block> blocks;
    std::vector<Sphere> obstacles;
    glm::dvec3 goal {0.0};
    glm::dvec3 arrival {0.0, 0.0, 1.0}; // final travel direction
    double tolerance = 64.0;
};

glm::dvec3 unit(const glm::dvec3& v) { return glm::normalize(v); }

Block circleBlock(glm::dvec3 entry, glm::dvec3 exit, double radius, double clearance)
{
    Block b;
    b.entry = entry; b.exit = exit; b.radius = radius; b.clearance = clearance;
    return b;
}

Block rectBlock(glm::dvec3 entry, glm::dvec3 exit, double halfWidth,
    double halfHeight, double clearance = 3.0)
{
    Block b;
    b.entry = entry; b.exit = exit; b.rectangle = true;
    b.halfWidth = halfWidth; b.halfHeight = halfHeight; b.clearance = clearance;
    return b;
}

std::vector<Scenario> scenarios()
{
    std::vector<Scenario> out;
    auto add = [&](Scenario s) { out.push_back(std::move(s)); };
    const glm::dvec3 Z {0.0, 0.0, 1.0};

    { Scenario s; s.name = "long-straight-wide-arc";
      s.purpose = "15 km straight, 45 deg wide turn";
      s.via = {{0, 0, 15000}}; s.goal = {9000, 0, 24000};
      s.arrival = unit(s.goal - s.via[0]); add(s); }
    { Scenario s; s.name = "s-curve";
      s.purpose = "two close opposite 45 deg turns";
      s.via = {{0, 0, 8000}, {1500, 0, 9500}}; s.goal = {1500, 0, 17000};
      s.arrival = Z; add(s); }
    { Scenario s; s.name = "short-straight-brake-burn";
      s.purpose = "two sharp turns joined by a 1.8 km straight";
      s.via = {{0, 0, 8000}, {1500, 0, 7000}}; s.goal = {3000, 0, 15000};
      s.arrival = unit(s.goal - s.via[1]); add(s); }
    { Scenario s; s.name = "oblique-portal";
      s.purpose = "portal window approached from 600 m aside";
      s.start = {500, 0, -2200};
      s.blocks = {circleBlock({0, 0, 0}, {0, 0, 1200}, 100.0, 22.0)};
      s.goal = {-1500, 0, 6000}; s.arrival = unit({-0.3, 0, 1}); add(s); }
    { Scenario s; s.name = "obstacles-both-sides";
      s.purpose = "straight line blocked, gap between two spheres";
      s.obstacles = {{{380, 0, 8000}, 300}, {{-380, 0, 8000}, 300},
                     {{150, 0, 4000}, 250}};
      s.goal = {0, 0, 16000}; s.arrival = Z; add(s); }
    { Scenario s; s.name = "wide-corridor";
      s.purpose = "3 km KeepInside tube r=300";
      s.blocks = {circleBlock({0, 0, 6000}, {0, 0, 9000}, 300.0, 22.0)};
      s.goal = {2000, 0, 14000}; s.arrival = unit({0.4, 0, 1}); add(s); }
    { Scenario s; s.name = "narrow-straight";
      s.purpose = "40x24 m narrow tube, level roll";
      s.blocks = {rectBlock({0, 0, 6000}, {0, 0, 7200}, 20.0, 12.0)};
      s.goal = {1500, 0, 11000}; s.arrival = unit({0.4, 0, 1}); add(s); }
    { Scenario s; s.name = "narrow-roll90";
      s.purpose = "16x40 m narrow tube, best roll 90 deg";
      s.blocks = {rectBlock({0, 0, 6000}, {0, 0, 7200}, 8.0, 20.0)};
      s.goal = {-1500, 0, 11000}; s.arrival = unit({-0.4, 0, 1}); add(s); }
    { Scenario s; s.name = "narrow-short-approach";
      s.purpose = "narrow tube, 200 m inbound straight, oblique start";
      Block b = rectBlock({0, 0, 3000}, {0, 0, 4200}, 20.0, 12.0);
      b.inbound = 200.0;
      s.start = {800, 0, 0}; s.blocks = {b};
      s.goal = {1500, 0, 8000}; s.arrival = unit({0.4, 0, 1}); add(s); }
    { Scenario s; s.name = "two-narrow-zones";
      s.purpose = "level narrow tube, then a turned roll-90 tube";
      const glm::dvec3 axis = unit({0.4, 0.0, 1.0});
      s.blocks = {rectBlock({0, 0, 5000}, {0, 0, 6200}, 20.0, 12.0),
                  rectBlock({600, 0, 9000}, glm::dvec3(600, 0, 9000) + axis * 1200.0,
                            8.0, 20.0)};
      s.goal = glm::dvec3(600, 0, 9000) + axis * 5000.0 + glm::dvec3(-1500, 0, 0);
      s.arrival = unit({-0.2, 0, 1}); add(s); }
    { Scenario s; s.name = "brake-into-narrow";
      s.purpose = "25 km run-up at cruise straight into a narrow tube";
      s.blocks = {rectBlock({0, 0, 25000}, {0, 0, 26200}, 20.0, 12.0)};
      s.goal = {1500, 0, 30000}; s.arrival = unit({0.4, 0, 1}); add(s); }
    { Scenario s; s.name = "out-and-back";
      s.purpose = "180 deg reversal: full stop, new burn the other way";
      s.via = {{0, 0, 7000}}; s.goal = {600, 0, 1500};
      s.arrival = unit({0.1, 0, -1}); add(s); }
    { Scenario s; s.name = "reverse-start";
      s.purpose = "moving 150 m/s away from the route, hull along the motion";
      s.velocity = {0, 0, -150}; s.hullForward = -Z;
      s.goal = {2000, 0, 15000}; s.arrival = Z; add(s); }
    { Scenario s; s.name = "cruise-long";
      s.purpose = "45 km straight at nominal max speed";
      s.goal = {0, 0, 45000}; s.arrival = Z; add(s); }
    for (double v : {399.9, 400.0, 400.01, 400.5, 400.9, 410.0})
    { Scenario s; char name[48];
      std::snprintf(name, sizeof(name), "start-at-%.2f", v);
      s.name = name;
      s.purpose = "moving start along a 30 km straight near nominal max";
      s.velocity = Z * v; s.goal = {0, 0, 30000}; s.arrival = Z; add(s); }
    { Scenario s; s.name = "flip-between-walls";
      s.purpose = "50x14 m tube, 3 km; yaw flip fits, pitch flip does not";
      s.blocks = {rectBlock({0, 0, 8000}, {0, 0, 11000}, 25.0, 7.0)};
      s.goal = {1000, 0, 13000}; s.arrival = unit({0.4, 0, 1}); add(s); }
    return out;
}

CompiledTrafficRoute trafficRouteOf(const Scenario& s)
{
    CompiledTrafficRoute route;
    if (s.blocks.empty())
        return route;
    route.valid = true;
    int portal = 0;
    for (const auto& b : s.blocks)
    {
        const glm::dvec3 axis = unit(b.exit - b.entry);
        const std::string in = "p" + std::to_string(portal++);
        const std::string out = "p" + std::to_string(portal++);

        CompiledTrafficStage free;
        free.kind = traffic::TrafficRouteStageKind::FreeSpace;
        free.toPortalId = in;
        free.toWorldMeters = b.entry;
        route.stages.push_back(free);

        CompiledTrafficStage stage;
        stage.kind = traffic::TrafficRouteStageKind::VolumeTransit;
        stage.fromPortalId = in;
        stage.toPortalId = out;
        stage.fromWorldMeters = b.entry;
        stage.toWorldMeters = b.exit;
        stage.volumeConstraint.volumeId = in + "-" + out;
        stage.volumeConstraint.policy =
            world::navigation::NavigationVolumePolicy::KeepInside;
        stage.volumeConstraint.maxSpeedMps = b.maxSpeed;
        stage.volumeRequiredClearanceMeters = b.clearance;
        for (const auto& c : {b.entry, b.exit})
        {
            CompiledNavigationVolumeSection section;
            section.centerWorldMeters = c;
            section.forwardWorld = axis;
            section.upWorld = b.up;
            section.crossSection = b.rectangle
                ? world::navigation::NavigationCrossSectionKind::Rectangle
                : world::navigation::NavigationCrossSectionKind::Circle;
            section.radiusMeters = b.radius;
            section.halfWidthMeters = b.halfWidth;
            section.halfHeightMeters = b.halfHeight;
            stage.volumeSections.push_back(section);
        }
        stage.requiredCenterlineMeters = {b.entry, b.exit};
        route.stages.push_back(stage);

        CompiledMandatoryTangentStraight inbound;
        inbound.portalId = in;
        inbound.inbound = true;
        inbound.startWorldMeters = b.entry - axis * b.inbound;
        inbound.endWorldMeters = b.entry;
        inbound.forwardWorld = axis;
        inbound.lengthMeters = b.inbound;
        CompiledMandatoryTangentStraight outbound;
        outbound.portalId = out;
        outbound.inbound = false;
        outbound.startWorldMeters = b.exit;
        outbound.endWorldMeters = b.exit + axis * b.outbound;
        outbound.forwardWorld = axis;
        outbound.lengthMeters = b.outbound;
        route.mandatoryTangentStraights.push_back(inbound);
        route.mandatoryTangentStraights.push_back(outbound);
        for (const auto& p : {inbound.startWorldMeters, b.entry, b.exit})
            route.requiredViaPointsMeters.push_back(p);
    }
    CompiledTrafficStage last;
    last.kind = traffic::TrafficRouteStageKind::FreeSpace;
    last.fromPortalId = "p" + std::to_string(portal - 1);
    last.fromWorldMeters = s.blocks.back().exit;
    route.stages.push_back(last);
    return route;
}

void putVec(std::FILE* f, const glm::dvec3& v)
{
    std::fprintf(f, " %.9g %.9g %.9g", v.x, v.y, v.z);
}

bool build(const Scenario& s, const char* model, const ShipParams& physics,
    const std::string& outBase)
{
    const auto& dim = EliteCobraMk1::EliteCobraMk1Descriptor().logicalDimensions();
    VehicleGuidanceEnvelope envelope;
    envelope.lengthMeters = dim.length;
    envelope.widthMeters = dim.width;
    envelope.heightMeters = dim.height;
    envelope.valid = true;
    const auto law = LocalFlightControlLaw::Newtonian;
    const auto ship = makeNavigationVehicleProfile(physics, envelope, law);

    // Mirror of SpaceState's Newtonian route request.
    planner::RoutePlanRequest request;
    request.startMeters = s.start;
    request.hasInitialForward = true;
    request.initialForward =
        DockingRouteEntryPolicy::initialCourse(s.velocity, unit(s.hullForward));
    request.terminalReferenceDistanceMeters =
        std::max(300.0, dim.length * 10.0);
    request.goalMeters = s.goal;
    request.terminalOutward = -unit(s.arrival);
    request.agentRadiusMeters = envelope.conservativeSafetyRadiusMeters();
    request.vehicleLengthMeters = dim.length;
    request.vehicleWidthMeters = dim.width;
    request.vehicleHeightMeters = dim.height;
    const double reserve = DockingAutomaticRecoveryPolicy::linearFeedbackReserveMps2(
        ship.maxForwardAccelerationMps2, ship.maxBrakingAccelerationMps2,
        ship.maxLateralAccelerationMps2);
    request.maxSpeedMps = ship.maxSpeedMps;
    request.acceleratingMps2 =
        std::max(0.1, (ship.maxForwardAccelerationMps2 - reserve) * 0.90);
    request.brakingMps2 =
        std::max(0.1, (ship.maxBrakingAccelerationMps2 - reserve) * 0.90);
    request.lateralMps2 = std::max(
        0.1, newtonianPreferredLateralAccelerationMps2(request.acceleratingMps2));
    request.maxAngularVelocityRadPerSecond = ship.maxAngularVelocityRadPerSecond;
    request.maxAngularAccelerationRadPerSecond2 =
        ship.maxAngularAccelerationRadPerSecond2 * 0.90;
    request.initialSpeedMps = glm::length(s.velocity);
    request.initialForwardLeadMeters = OrdinaryRoutePolicy::requiredStartLeadMeters(
        request.initialSpeedMps, request.maxSpeedMps, request.brakingMps2,
        request.lateralMps2, request.maxAngularVelocityRadPerSecond, dim.length);
    request.gateSpacingMeters = 500.0;
    request.terminalGateSpacingMeters = 250.0;
    request.requiredViaPointsMeters = s.via;
    for (const auto& o : s.obstacles)
    {
        world::navigation::NavigationObstacle obstacle;
        obstacle.id = "sphere";
        obstacle.shape = world::navigation::NavigationObstacleShape::Sphere;
        obstacle.centerMeters = o.center;
        obstacle.radiusMeters = o.radius;
        request.obstacles.push_back(obstacle);
    }

    auto traffic = trafficRouteOf(s);
    const traffic::ShipHullBox hull {
        0.5 * request.vehicleWidthMeters, 0.5 * request.vehicleHeightMeters,
        0.5 * request.vehicleLengthMeters};
    double windowShift = 0.0;
    if (traffic.valid)
        for (const auto& c : traffic::chooseWindowCrossings(traffic,
                 request.startMeters, request.goalMeters,
                 request.agentRadiusMeters, hull,
                 request.narrowZoneAttitudeToleranceRad))
            windowShift = std::max(windowShift, glm::length(c.offsetWorldMeters));

    using Pipeline = planner::newtonian::NewtonianPlanningPipeline;
    auto plan = traffic.valid ? Pipeline::segmented(request, traffic)
                              : Pipeline::route(request);
    std::string rejected = plan.valid() ? "" : "planner: " + plan.failure;
    if (plan.valid())
        for (const auto& stage : traffic.stages)
        {
            if (stage.kind != traffic::TrafficRouteStageKind::VolumeTransit)
                continue;
            const auto containment = traffic::validateKeepInsideForShip(
                stage, plan.routeCurves, hull, request.agentRadiusMeters,
                request.narrowZoneAttitudeToleranceRad);
            if (!containment.valid)
                rejected = "keep-inside: " + containment.failure;
        }

    std::FILE* g = std::fopen((outBase + ".geom").c_str(), "w");
    if (!g)
        return false;
    std::fprintf(g, "scenario %s %s\n", s.name.c_str(), model);
    std::fprintf(g, "purpose %s\n", s.purpose.c_str());
    std::fprintf(g, "hull %.6g %.6g %.6g radius %.6g\n", hull.halfWidthMeters,
        hull.halfHeightMeters, hull.halfLengthMeters, request.agentRadiusMeters);
    std::fprintf(g, "capability max_mps %.6g acc %.6g brake %.6g lat %.6g\n",
        request.maxSpeedMps, request.acceleratingMps2, request.brakingMps2,
        request.lateralMps2);
    std::fprintf(g, "window_shift_m %.3f\n", windowShift);
    for (const auto& stage : traffic.stages)
    {
        if (stage.kind != traffic::TrafficRouteStageKind::VolumeTransit)
            continue;
        const auto& a = stage.volumeSections.front();
        const auto& b = stage.volumeSections.back();
        const auto fit = traffic::analyseVolumeFit(stage, hull,
            request.agentRadiusMeters, request.narrowZoneAttitudeToleranceRad);
        std::fprintf(g, "volume %s clearance %.6g sphere_fits %d rolled90 %d",
            a.crossSection == world::navigation::NavigationCrossSectionKind::Rectangle
                ? "rect" : "circle",
            stage.volumeRequiredClearanceMeters, fit.sphereFits ? 1 : 0,
            fit.rolled90 ? 1 : 0);
        putVec(g, a.centerWorldMeters); putVec(g, b.centerWorldMeters);
        putVec(g, a.upWorld);
        std::fprintf(g, " %.6g %.6g %.6g\n", a.radiusMeters, a.halfWidthMeters,
            a.halfHeightMeters);
    }
    for (const auto& o : s.obstacles)
    {
        std::fprintf(g, "sphere");
        putVec(g, o.center);
        std::fprintf(g, " %.6g\n", o.radius);
    }
    if (!rejected.empty())
    {
        std::fprintf(g, "plan rejected %s\n", rejected.c_str());
        std::fclose(g);
        std::printf("%-26s %s REJECTED %s\n", s.name.c_str(), model,
            rejected.c_str());
        return true;
    }
    double length = 0.0;
    int stops = 0;
    for (std::size_t i = 1; i + 1 < plan.executionGates.size(); ++i)
        if (plan.executionGates[i].speedMps <= 0.5)
            ++stops;
    if (!plan.routeCurves.empty())
        length = plan.routeCurves.back().endProgressMeters;
    std::fprintf(g, "plan ok length_m %.1f curves %zu narrow %zu\n", length,
        plan.routeCurves.size(), plan.narrowZones.size());
    for (const auto& z : plan.narrowZones)
        std::fprintf(g, "narrow %.3f %.3f\n", z.startProgressMeters,
            z.endProgressMeters);
    std::fclose(g);

    diagnostics::AutopilotCase c;
    c.plan = plan;
    c.agent.positionMapMeters = s.start;
    c.agent.velocityMapMetersPerSecond = s.velocity;
    c.agent.forwardMap = unit(s.hullForward);
    c.agent.upMap = unit(s.up - glm::dot(s.up, c.agent.forwardMap) * c.agent.forwardMap);
    c.agent.rightMap = glm::cross(c.agent.forwardMap, c.agent.upMap);
    c.physics = physics;
    c.acceptedAtUniverseTimeSeconds = 1000.0;
    c.requestSerial = 1;
    c.trackingToleranceMeters = s.tolerance;
    c.routeUpReference = s.up;
    c.holdAtTerminal = true;
    if (!diagnostics::writeAutopilotCase((outBase + ".case").c_str(), c))
        return false;
    std::printf("%-26s %s ok length=%.0f m curves=%zu narrow=%zu stops=%d window=%.1f m\n",
        s.name.c_str(), model, length, plan.routeCurves.size(),
        plan.narrowZones.size(), stops, windowShift);
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr,
            "usage: EliteNewtonianScenarios <out dir> <I case> <Y case> <X case> [name...]\n");
        return 2;
    }
    const std::string outDir = argv[1];
    const char* models[3] = {"I", "Y", "X"};
    ShipParams physics[3];
    for (int m = 0; m < 3; ++m)
    {
        diagnostics::AutopilotCase base;
        if (!diagnostics::readAutopilotCase(argv[2 + m], base))
        {
            std::fprintf(stderr, "cannot read case %s\n", argv[2 + m]);
            return 2;
        }
        physics[m] = base.physics;
    }
    std::vector<std::string> only(argv + 5, argv + argc);
    int failures = 0;
    for (const auto& s : scenarios())
    {
        if (!only.empty() &&
            std::find(only.begin(), only.end(), s.name) == only.end())
            continue;
        for (int m = 0; m < 3; ++m)
            if (!build(s, models[m], physics[m], outDir + "/" + s.name + "-" + models[m]))
                ++failures;
    }
    return failures == 0 ? 0 : 1;
}
