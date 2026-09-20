#include "NavigationScenarioRuntime.h"

#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/ManeuverProgramSampler.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/NavigationRuntimePlanner.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/world/WorldParams.h"
#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/map/NavigationMap.h"
#include "src/world/navigation/space/NavigationSpace.h"
#include "src/world/navigation/space/NavigationStaticQueryApi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace elite::tools::navigation_runtime
{
namespace
{

using Program = game::navigation::AcceptedManeuverProgram;
using Sampler = game::navigation::ManeuverProgramSampler;
using Follower = game::navigation::TrajectoryFollower;
using Bridge = game::navigation::NavigationRuntimeControlBridge;
using Planner = game::navigation::NavigationRuntimePlanner;
using Law = game::navigation::LocalFlightControlLaw;
using Map = world::navigation::NavigationMap;
using Space = world::navigation::NavigationSpace;
using StaticQueries = world::navigation::NavigationStaticQueryApi;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kReplanPeriodSeconds = 0.50;
constexpr double kMaximumScenarioSeconds = 180.0;
const glm::dvec3 kBodyHalfExtents {13.0, 2.5, 11.1};
const double kHullBoundingRadiusMeters = glm::length(kBodyHalfExtents);

struct Basis
{
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 right {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

struct Endpoint
{
    glm::dvec3 position {300.0, 0.0, 0.0};
    bool requireForward = false;
    bool requireUp = false;
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    double speedMps = 0.0;
};

struct MotionPath
{
    std::vector<glm::dvec3> points;
    double speedMps = 0.0;
    bool loop = false;
};

struct DynamicObstacle
{
    std::uint64_t entityId = 0;
    std::string id;
    glm::dvec3 initialPosition {0.0};
    glm::dvec3 linearVelocity {0.0};
    MotionPath path {};
    double activationTimeSeconds = 0.0;
    double radiusMeters = 5.0;

    bool spawnRelativeToShip = false;
    glm::dvec3 spawnRelativeFruMeters {0.0};
    bool spawned = false;
    glm::dvec3 spawnedPosition {0.0};
};

struct Scenario
{
    glm::dvec3 startPosition {0.0};
    glm::dvec3 startVelocity {6.0, 0.0, 0.0};
    Basis startBasis {};

    std::vector<glm::dvec3> shipRoutePoints;
    Endpoint finish;

    std::vector<world::navigation::NavigationObstacle> staticObstacles;
    std::vector<DynamicObstacle> dynamicObstacles;
    DynamicObstacle suddenObstacle;
    bool hasSuddenObstacle = false;

    double standardSpeedMps = 10.0;
    double extremeSpeedMps = 18.0;
};

glm::dvec3 readVec3(
    const nlohmann::json& object,
    const char* key,
    const glm::dvec3& fallback
)
{
    if (!object.contains(key))
        return fallback;

    const auto& j = object.at(key);
    if (!j.is_array() || j.size() != 3)
        throw std::runtime_error(std::string(key) + " must be [x,y,z]");

    return {
        j.at(0).get<double>(),
        j.at(1).get<double>(),
        j.at(2).get<double>()
    };
}

glm::dvec3 readVec3Value(
    const nlohmann::json& j,
    const char* what
)
{
    if (!j.is_array() || j.size() != 3)
        throw std::runtime_error(std::string(what) + " must be [x,y,z]");

    return {
        j.at(0).get<double>(),
        j.at(1).get<double>(),
        j.at(2).get<double>()
    };
}

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    if (!std::isfinite(length) || length <= 1.0e-9)
        return fallback;
    return value / length;
}

Basis basisFromForwardUp(
    const glm::dvec3& forwardInput,
    const glm::dvec3& upInput
)
{
    const glm::dvec3 forward =
        normalizedOr(forwardInput, {1.0, 0.0, 0.0});

    glm::dvec3 up =
        upInput -
        forward * glm::dot(upInput, forward);

    if (glm::length(up) <= 1.0e-9)
    {
        up = {0.0, 1.0, 0.0};
        if (std::abs(glm::dot(up, forward)) > 0.92)
            up = {0.0, 0.0, 1.0};
        up -= forward * glm::dot(up, forward);
    }

    up = glm::normalize(up);
    const glm::dvec3 right =
        glm::normalize(glm::cross(forward, up));
    up = glm::normalize(glm::cross(right, forward));
    return {forward, right, up};
}

glm::dquat quaternionForBasis(const Basis& basis)
{
    glm::dmat3 m(1.0);
    m[0] = basis.right;
    m[1] = basis.up;
    m[2] = -basis.forward;
    return glm::normalize(glm::quat_cast(m));
}

Basis basisForQuaternion(const glm::dquat& q)
{
    const glm::dmat3 m = glm::mat3_cast(glm::normalize(q));
    return {
        -glm::dvec3(m[2]),
        glm::dvec3(m[0]),
        glm::dvec3(m[1])
    };
}

glm::dvec3 angularVelocityBetween(
    glm::dquat a,
    glm::dquat b,
    double dt
)
{
    if (dt <= 1.0e-12)
        return glm::dvec3(0.0);

    if (glm::dot(a, b) < 0.0)
        b = -b;

    glm::dquat delta = glm::normalize(b * glm::inverse(a));
    if (delta.w < 0.0)
        delta = -delta;

    const double w = std::clamp(delta.w, -1.0, 1.0);
    const double angle = 2.0 * std::acos(w);
    const double sinHalf =
        std::sqrt(std::max(0.0, 1.0 - w * w));

    if (angle <= 1.0e-12 || sinHalf <= 1.0e-12)
        return glm::dvec3(0.0);

    const glm::dvec3 axis =
        glm::normalize(
            glm::dvec3(delta.x, delta.y, delta.z) / sinHalf
        );
    return axis * (angle / dt);
}

Bridge::PilotSkillProfile pilotProfile(PilotLevel level)
{
    Bridge::PilotSkillProfile profile;

    switch (level)
    {
        case PilotLevel::Expert:
            profile.execution.reactionDelaySeconds = 0.0;
            profile.execution.perceptionDecisionRateHz = 100.0;
            profile.execution.commandLatencySeconds = 0.0;
            profile.execution.responseFrequencyHz = 10.0;
            profile.execution.dampingRatio = 1.0;
            profile.execution.commandGain = 1.0;
            profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
            profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
            profile.execution.deterministicSeed = 0xE0E0E001ull;
            profile.policy.anticipationSeconds = 1.0;
            profile.policy.riskPreference01 = 0.25;
            profile.policy.comfortPreference01 = 0.35;
            break;

        case PilotLevel::Average:
            profile.execution.reactionDelaySeconds = 0.16;
            profile.execution.perceptionDecisionRateHz = 12.0;
            profile.execution.commandLatencySeconds = 0.08;
            profile.execution.responseFrequencyHz = 3.0;
            profile.execution.dampingRatio = 0.9;
            profile.execution.commandGain = 0.95;
            profile.execution.maxLinearCommandSlewMetersPerSec3 = 25.0;
            profile.execution.maxAngularCommandSlewRadPerSec3 = 12.0;
            profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 =
                0.12;
            profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 =
                0.02;
            profile.execution.deterministicSeed = 0xA0A0A002ull;
            profile.policy.anticipationSeconds = 0.55;
            profile.policy.riskPreference01 = 0.45;
            profile.policy.comfortPreference01 = 0.55;
            break;

        case PilotLevel::Loser:
            profile.execution.reactionDelaySeconds = 0.45;
            profile.execution.perceptionDecisionRateHz = 5.0;
            profile.execution.commandLatencySeconds = 0.18;
            profile.execution.responseFrequencyHz = 1.5;
            profile.execution.dampingRatio = 0.72;
            profile.execution.commandGain = 0.85;
            profile.execution.maxLinearCommandSlewMetersPerSec3 = 10.0;
            profile.execution.maxAngularCommandSlewRadPerSec3 = 5.0;
            profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 =
                0.45;
            profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 =
                0.08;
            profile.execution.emergencyResponseThreshold01 = 0.60;
            profile.execution.emergencyReactionDelayScale = 0.55;
            profile.execution.deterministicSeed = 0x10105E03ull;
            profile.policy.anticipationSeconds = 0.20;
            profile.policy.riskPreference01 = 0.65;
            profile.policy.comfortPreference01 = 0.30;
            break;
    }

    return profile;
}

ShipParams cobraParams()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;

    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 20.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.manoeuvreGasUsePerSecond = 0.0f;
    p.manoeuvreGasRechargePerSecond = 0.0f;

    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

void setTransformBasis(ShipTransform& transform, const Basis& basis)
{
    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] =
        glm::vec4(glm::vec3(basis.right), 0.0f);
    transform.orientation[1] =
        glm::vec4(glm::vec3(basis.up), 0.0f);
    transform.orientation[2] =
        glm::vec4(glm::vec3(-basis.forward), 0.0f);
}

struct Vehicle
{
    ShipTransform transform {};
    ShipParams params {};
    WorldParams world {};
    game::navigation::KinematicFrame frame {};
    Bridge bridge;
    double timeSeconds = 0.0;

    Vehicle(
        Law law,
        const Scenario& scenario,
        PilotLevel pilot
    )
        : params(cobraParams()),
          bridge(pilotProfile(pilot))
    {
        frame.systemId = 1;
        frame.frameId = "navigation-runtime-tool";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw = law;
        transform.motion.localPositionMeters = scenario.startPosition;
        transform.motion.localVelocityMps = scenario.startVelocity;
        transform.setWorldPositionMeters(scenario.startPosition);
        setTransformBasis(transform, scenario.startBasis);

        Bridge::Intent initial;
        initial.revision = 1;
        initial.targetRevision = 0;
        if (!bridge.reset(0.0, initial))
            throw std::runtime_error("pilot bridge reset failed");
    }
};

Basis actualBasis(const Vehicle& v)
{
    return {
        glm::dvec3(v.transform.forward()),
        glm::dvec3(v.transform.right()),
        glm::dvec3(v.transform.up())
    };
}

Follower::AgentState followerAgent(const Vehicle& v)
{
    Follower::AgentState a;
    a.positionMapMeters = v.transform.motion.localPositionMeters;
    a.velocityMapMetersPerSecond = v.transform.motion.localVelocityMps;
    a.forwardMap = glm::dvec3(v.transform.forward());
    a.rightMap = glm::dvec3(v.transform.right());
    a.upMap = glm::dvec3(v.transform.up());
    a.pitchRateRadPerSec = v.transform.pitchRate;
    a.yawRateRadPerSec = v.transform.yawRate;
    a.rollRateRadPerSec = v.transform.rollRate;
    return a;
}

Planner::AgentState plannerAgent(const Vehicle& v, Law law)
{
    Planner::AgentState a;
    a.entityId = 42;
    a.positionMapMeters = v.transform.motion.localPositionMeters;
    a.velocityMapMetersPerSecond = v.transform.motion.localVelocityMps;
    a.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    a.radiusMeters = kHullBoundingRadiusMeters;
    a.forwardMap = glm::dvec3(v.transform.forward());
    a.rightMap = glm::dvec3(v.transform.right());
    a.upMap = glm::dvec3(v.transform.up());
    a.pitchRateRadPerSec = v.transform.pitchRate;
    a.yawRateRadPerSec = v.transform.yawRate;
    a.rollRateRadPerSec = v.transform.rollRate;
    a.hullHalfExtentsBodyMeters = kBodyHalfExtents;
    a.controlMode =
        law == Law::Newtonian
            ? Planner::MovingPassage::ControlMode::Newtonian
            : Planner::MovingPassage::ControlMode::EliteAssisted;
    a.assistedMaxVelocityToForwardAngleRad = 0.35;

    a.linearCapability.maxForwardAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    a.linearCapability.maxReverseAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    a.linearCapability.maxLateralAccelerationMetersPerSec2 = 2.0;
    a.linearCapability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    a.angularCapability.maxAngularAccelerationRadPerSec2 = 3.0;
    a.angularCapability.maxAngularSpeedRadPerSec = 2.5;
    return a;
}

game::navigation::NavigationSystemControlIntent toSystemIntent(
    const game::navigation::NavigationLocalControlIntent& local
)
{
    game::navigation::NavigationSystemControlIntent system;
    system.revision = local.revision;
    system.targetRevision = local.targetRevision;
    system.idealLinearAccelerationSystemMps2 =
        local.idealLinearAccelerationLocalMps2;
    system.idealAngularAccelerationSystemRadPerSec2 =
        local.idealAngularAccelerationLocalRadPerSec2;
    system.emergency = local.emergency;
    system.hazardUrgency01 = local.hazardUrgency01;
    return system;
}

const char* plannerStatusName(Planner::Status status)
{
    switch (status)
    {
        case Planner::Status::NominalClear: return "nominal_clear";
        case Planner::Status::AdjustedClear: return "adjusted_clear";
        case Planner::Status::ConflictHold: return "conflict_hold";
        case Planner::Status::StaleHold: return "stale_hold";
        case Planner::Status::StaticHold: return "static_hold";
        case Planner::Status::InvalidInput: return "invalid_input";
        case Planner::Status::MovingPassageClear: return "moving_passage_clear";
        case Planner::Status::PortalCapture: return "portal_capture";
        case Planner::Status::PortalTransit: return "portal_transit";
    }
    return "unknown";
}

Scenario loadScenario(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("cannot open scenario JSON: " + path);

    nlohmann::json root;
    stream >> root;

    Scenario s;

    if (root.contains("start"))
    {
        const auto& start = root.at("start");
        s.startPosition =
            readVec3(start, "position", s.startPosition);
        s.startVelocity =
            readVec3(start, "velocity", s.startVelocity);
        const glm::dvec3 forward =
            readVec3(start, "forward", s.startBasis.forward);
        const glm::dvec3 up =
            readVec3(start, "up", s.startBasis.up);
        s.startBasis = basisFromForwardUp(forward, up);
    }

    if (root.contains("ship_route_points"))
    {
        for (const auto& p : root.at("ship_route_points"))
            s.shipRoutePoints.push_back(
                readVec3Value(p, "ship_route_points[]")
            );
    }

    if (root.contains("finish"))
    {
        const auto& finish = root.at("finish");
        s.finish.position =
            readVec3(finish, "position", s.finish.position);

        if (finish.contains("forward"))
        {
            s.finish.requireForward = true;
            s.finish.forward =
                readVec3(
                    finish,
                    "forward",
                    s.finish.forward
                );
        }

        if (finish.contains("up"))
        {
            s.finish.requireUp = true;
            s.finish.up =
                readVec3(
                    finish,
                    "up",
                    s.finish.up
                );
        }

        s.finish.speedMps =
            finish.value("speed_mps", 0.0);
    }

    s.standardSpeedMps =
        root.value("standard_speed_mps", 10.0);
    s.extremeSpeedMps =
        root.value("extreme_speed_mps", 18.0);

    std::uint32_t staticEntity = 50000;
    if (root.contains("static_obstacles"))
    {
        for (const auto& src : root.at("static_obstacles"))
        {
            world::navigation::NavigationObstacle o;
            o.id = src.value(
                "id",
                std::string("static_") +
                    std::to_string(staticEntity)
            );
            o.entityId = staticEntity++;
            o.centerMeters =
                readVec3(src, "center", {0.0, 0.0, 0.0});
            o.requiredClearanceMeters =
                src.value("required_clearance_m", 0.0);

            const std::string shape =
                src.value("shape", std::string("sphere"));

            if (shape == "box")
            {
                o.shape =
                    world::navigation::NavigationObstacleShape::Box;
                o.halfExtentsMeters =
                    readVec3(
                        src,
                        "half_extents",
                        {1.0, 1.0, 1.0}
                    );
            }
            else if (shape == "capsule")
            {
                o.shape =
                    world::navigation::NavigationObstacleShape::Capsule;
                o.radiusMeters =
                    src.value("radius_m", 1.0);
                o.capsuleHalfLengthMeters =
                    src.value("half_length_m", 1.0);
            }
            else
            {
                o.shape =
                    world::navigation::NavigationObstacleShape::Sphere;
                o.radiusMeters =
                    src.value("radius_m", 1.0);
            }

            s.staticObstacles.push_back(std::move(o));
        }
    }

    auto parseDynamic =
        [](const nlohmann::json& src, std::uint64_t entity)
        {
            DynamicObstacle o;
            o.entityId = entity;
            o.id =
                src.value(
                    "id",
                    std::string("dynamic_") +
                        std::to_string(entity)
                );
            o.initialPosition =
                readVec3(src, "position", {0.0, 0.0, 0.0});
            o.linearVelocity =
                readVec3(src, "velocity", {0.0, 0.0, 0.0});
            o.activationTimeSeconds =
                src.value("activation_time_s", 0.0);
            o.radiusMeters =
                src.value("radius_m", 5.0);

            if (src.contains("route_points"))
            {
                for (const auto& p : src.at("route_points"))
                {
                    o.path.points.push_back(
                        readVec3Value(
                            p,
                            "moving obstacle route_points[]"
                        )
                    );
                }
                o.path.speedMps =
                    src.value("route_speed_mps", 5.0);
                o.path.loop =
                    src.value("route_loop", false);
            }

            if (src.contains("spawn_relative_to_ship_fru"))
            {
                o.spawnRelativeToShip = true;
                o.spawnRelativeFruMeters =
                    readVec3(
                        src,
                        "spawn_relative_to_ship_fru",
                        {0.0, 0.0, 0.0}
                    );
            }

            return o;
        };

    std::uint64_t dynamicEntity = 60000;
    if (root.contains("moving_obstacles"))
    {
        for (const auto& src : root.at("moving_obstacles"))
            s.dynamicObstacles.push_back(
                parseDynamic(src, dynamicEntity++)
            );
    }

    if (root.contains("sudden_obstacle"))
    {
        s.suddenObstacle =
            parseDynamic(
                root.at("sudden_obstacle"),
                69999
            );
        s.hasSuddenObstacle = true;
    }

    return s;
}

Space buildStaticSpace(const Scenario& scenario)
{
    glm::dvec3 minimum = scenario.startPosition;
    glm::dvec3 maximum = scenario.startPosition;

    auto include = [&](const glm::dvec3& p)
    {
        minimum = glm::min(minimum, p);
        maximum = glm::max(maximum, p);
    };

    include(scenario.finish.position);
    for (const auto& p : scenario.shipRoutePoints)
        include(p);

    for (const auto& o : scenario.staticObstacles)
    {
        const double r =
            std::max(1.0, o.conservativeRadiusMeters());
        include(o.centerMeters - glm::dvec3(r));
        include(o.centerMeters + glm::dvec3(r));
    }

    const glm::dvec3 padding(500.0);
    minimum -= padding;
    maximum += padding;

    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 1;

    Space::RegionInput region;
    region.regionId = 1;
    region.boundsMapMeters.minMapMeters =
        {minimum.x, minimum.y, minimum.z};
    region.boundsMapMeters.maxMapMeters =
        {maximum.x, maximum.y, maximum.z};
    region.clearanceRadiusMeters = 1.0e6;
    region.geometryRevision = 1;
    update.regions.push_back(region);
    update.obstacles = scenario.staticObstacles;

    space.replaceStaticWorld(std::move(update));
    return space;
}

Planner::Policy plannerPolicy(FlightStyle style)
{
    Planner::Policy policy;

    policy.corridor.distanceWeight = 1.0;
    policy.corridor.preferredClearanceMultiple =
        style == FlightStyle::Extreme ? 1.0 : 1.25;
    policy.corridor.clearancePenaltyMeters =
        style == FlightStyle::Extreme ? 8.0 : 24.0;
    policy.corridor.turnPenaltyMetersPerRadian =
        style == FlightStyle::Extreme ? 1.0 : 7.0;

    policy.horizon.lookAheadSeconds =
        style == FlightStyle::Extreme ? 5.0 : 4.0;
    policy.horizon.maxResultAgeSeconds = 0.25;
    policy.horizon.maxBrakingAccelerationMetersPerSecond2 = 8.0;
    policy.horizon.turnDistanceMeters =
        style == FlightStyle::Extreme ? 15.0 : 24.0;
    policy.horizon.safetyMarginMeters = 2.0;
    policy.horizon.minimumHorizonMeters =
        style == FlightStyle::Extreme ? 40.0 : 30.0;

    policy.avoidance.lateralGridHalfExtentSamples = 6;
    policy.avoidance.minimumLateralStepMeters = 4.0;
    policy.avoidance.lateralStepEnvelopeMultiplier = 1.0;
    policy.avoidance.maximumLateralOffsetMeters = 90.0;
    policy.avoidance.longitudinalSamples = 3;
    policy.avoidance.projectionPaddingMeters = 1.5;
    policy.avoidance.trajectorySamples = 48;
    policy.avoidance.staticAdditionalClearanceMeters = 0.0;

    policy.portalTraversal.enabled = false;
    return policy;
}

glm::dvec3 positionOnPath(
    const MotionPath& path,
    double elapsed,
    glm::dvec3& velocityOut
)
{
    velocityOut = {0.0, 0.0, 0.0};

    if (path.points.empty())
        return {0.0, 0.0, 0.0};

    if (path.points.size() == 1 || path.speedMps <= 1.0e-9)
        return path.points.front();

    std::vector<double> lengths;
    lengths.reserve(path.points.size() - 1);

    double total = 0.0;
    for (std::size_t i = 1; i < path.points.size(); ++i)
    {
        const double length =
            glm::length(path.points[i] - path.points[i - 1]);
        lengths.push_back(length);
        total += length;
    }

    if (total <= 1.0e-9)
        return path.points.front();

    double distance =
        std::max(0.0, elapsed) * path.speedMps;

    if (path.loop)
        distance = std::fmod(distance, total);
    else if (distance >= total)
        return path.points.back();

    for (std::size_t i = 0; i < lengths.size(); ++i)
    {
        const double segmentLength = lengths[i];
        if (segmentLength <= 1.0e-9)
            continue;

        if (distance <= segmentLength)
        {
            const glm::dvec3 delta =
                path.points[i + 1] - path.points[i];
            const glm::dvec3 direction =
                delta / segmentLength;
            velocityOut = direction * path.speedMps;
            return
                path.points[i] +
                direction * distance;
        }

        distance -= segmentLength;
    }

    return path.points.back();
}

glm::dvec3 obstaclePosition(
    DynamicObstacle& obstacle,
    double timeSeconds,
    const Vehicle& vehicle,
    glm::dvec3& velocityOut
)
{
    velocityOut = {0.0, 0.0, 0.0};

    if (timeSeconds < obstacle.activationTimeSeconds)
        return obstacle.initialPosition;

    if (obstacle.spawnRelativeToShip && !obstacle.spawned)
    {
        const Basis basis = actualBasis(vehicle);
        obstacle.spawnedPosition =
            vehicle.transform.motion.localPositionMeters +
            basis.forward * obstacle.spawnRelativeFruMeters.x +
            basis.right * obstacle.spawnRelativeFruMeters.y +
            basis.up * obstacle.spawnRelativeFruMeters.z;
        obstacle.spawned = true;
    }

    const double elapsed =
        timeSeconds - obstacle.activationTimeSeconds;

    if (!obstacle.path.points.empty())
    {
        glm::dvec3 pathVelocity;
        const glm::dvec3 p =
            positionOnPath(
                obstacle.path,
                elapsed,
                pathVelocity
            );
        velocityOut = pathVelocity;
        return p;
    }

    const glm::dvec3 start =
        obstacle.spawnRelativeToShip
            ? obstacle.spawnedPosition
            : obstacle.initialPosition;

    velocityOut = obstacle.linearVelocity;
    return start + obstacle.linearVelocity * elapsed;
}

Map::QueryResult publishDynamicWorld(
    Map& map,
    std::vector<DynamicObstacle>& obstacles,
    DynamicObstacle* sudden,
    bool enableSudden,
    Vehicle& vehicle,
    std::uint64_t revision,
    double lookAheadSeconds
)
{
    Map::DynamicWorldUpdate update;
    update.sourceRevision = revision;

    auto appendActor =
        [&](DynamicObstacle& o)
        {
            if (vehicle.timeSeconds < o.activationTimeSeconds)
                return;

            glm::dvec3 velocity;
            const glm::dvec3 position =
                obstaclePosition(
                    o,
                    vehicle.timeSeconds,
                    vehicle,
                    velocity
                );

            Map::DynamicActorInput actor;
            actor.entityId = o.entityId;
            actor.positionMapMeters =
                {position.x, position.y, position.z};
            actor.velocityMapMetersPerSecond =
                {velocity.x, velocity.y, velocity.z};
            actor.accelerationMapMetersPerSecond2 =
                {0.0, 0.0, 0.0};
            actor.radiusMeters = o.radiusMeters;
            actor.motionRevision = revision;
            update.actors.push_back(actor);
        };

    for (auto& o : obstacles)
        appendActor(o);

    if (enableSudden && sudden)
        appendActor(*sudden);

    map.replaceDynamicWorld(std::move(update));

    Map::SphereQuery query;
    query.centerMapMeters = {
        vehicle.transform.motion.localPositionMeters.x,
        vehicle.transform.motion.localPositionMeters.y,
        vehicle.transform.motion.localPositionMeters.z
    };
    query.radiusMeters = 260.0;
    query.lookAheadSeconds = lookAheadSeconds;
    return map.querySphere(query);
}

struct Curve
{
    std::array<glm::dvec3, 6> c {};
    double duration = 1.0;
};

Curve makeCurve(
    const glm::dvec3& start,
    const glm::dvec3& startVelocity,
    const glm::dvec3& end,
    const glm::dvec3& endVelocity,
    double duration
)
{
    Curve q;
    q.duration = duration;
    q.c[0] = start;
    q.c[1] = startVelocity * duration;
    q.c[2] = {0.0, 0.0, 0.0};

    const glm::dvec3 p =
        end - q.c[0] - q.c[1];
    const glm::dvec3 v =
        endVelocity * duration - q.c[1];

    q.c[3] = 10.0 * p - 4.0 * v;
    q.c[4] = -15.0 * p + 7.0 * v;
    q.c[5] = 6.0 * p - 3.0 * v;
    return q;
}

void sampleCurve(
    const Curve& q,
    double t,
    glm::dvec3& position,
    glm::dvec3& velocity,
    glm::dvec3& acceleration
)
{
    const double u =
        q.duration > 1.0e-12
            ? std::clamp(t / q.duration, 0.0, 1.0)
            : 1.0;
    const double u2 = u * u;
    const double u3 = u2 * u;
    const double u4 = u3 * u;
    const double u5 = u4 * u;

    position =
        q.c[0] +
        q.c[1] * u +
        q.c[2] * u2 +
        q.c[3] * u3 +
        q.c[4] * u4 +
        q.c[5] * u5;

    const glm::dvec3 d1 =
        q.c[1] +
        2.0 * q.c[2] * u +
        3.0 * q.c[3] * u2 +
        4.0 * q.c[4] * u3 +
        5.0 * q.c[5] * u4;

    const glm::dvec3 d2 =
        2.0 * q.c[2] +
        6.0 * q.c[3] * u +
        12.0 * q.c[4] * u2 +
        20.0 * q.c[5] * u3;

    velocity =
        q.duration > 1.0e-12
            ? d1 / q.duration
            : glm::dvec3(0.0);
    acceleration =
        q.duration > 1.0e-12
            ? d2 / (q.duration * q.duration)
            : glm::dvec3(0.0);
}

enum class AttitudeMode
{
    Preserve,
    VelocityAligned,
    TargetBasis
};

Program makeShortProgram(
    std::uint64_t revision,
    const Vehicle& vehicle,
    const glm::dvec3& requestedTarget,
    const glm::dvec3& requestedVelocity,
    Law law,
    FlightStyle flightStyle,
    AttitudeMode attitudeMode,
    const Basis& targetBasis
)
{
    const glm::dvec3 start =
        vehicle.transform.motion.localPositionMeters;
    const glm::dvec3 startVelocity =
        vehicle.transform.motion.localVelocityMps;
    const Basis startBasis = actualBasis(vehicle);

    glm::dvec3 delta = requestedTarget - start;
    const double distance = glm::length(delta);

    const double speed =
        std::max(
            4.0,
            glm::length(requestedVelocity)
        );

    const double maximumSegmentMeters =
        flightStyle == FlightStyle::Extreme
            ? 55.0
            : 42.0;

    glm::dvec3 target = requestedTarget;
    if (distance > maximumSegmentMeters && distance > 1.0e-9)
        target = start + delta / distance * maximumSegmentMeters;

    const double clippedDistance =
        glm::length(target - start);

    double duration =
        clippedDistance / speed;

    if (flightStyle == FlightStyle::Extreme)
        duration = std::clamp(duration, 1.4, 3.0);
    else
        duration = std::clamp(duration, 1.8, 4.0);

    glm::dvec3 endVelocity = requestedVelocity;
    const double endSpeed = glm::length(endVelocity);
    if (endSpeed > speed && endSpeed > 1.0e-9)
        endVelocity = endVelocity / endSpeed * speed;

    const Curve curve =
        makeCurve(
            start,
            startVelocity,
            target,
            endVelocity,
            duration
        );

    Program p;
    p.valid = true;
    p.revision = revision;
    p.objectiveRevision = 1;
    p.family =
        attitudeMode == AttitudeMode::TargetBasis
            ? Program::ManeuverFamily::PrecisionCapture
            : Program::ManeuverFamily::PrecisionTransit;
    p.acceptedAtUniverseTimeSeconds = vehicle.timeSeconds;
    p.validUntilUniverseTimeSeconds =
        vehicle.timeSeconds + duration + 1.0;
    p.completionTriggersReplan = true;

    p.terminalTolerance.positionMeters = 1.5;
    p.terminalTolerance.linearVelocityMps = 0.8;
    p.terminalTolerance.forwardAngleRad = 0.08;
    p.terminalTolerance.angularVelocityRadPerSec = 0.12;

    p.tracking.positionErrorMeters =
        flightStyle == FlightStyle::Extreme ? 16.0 : 12.0;
    p.tracking.linearVelocityErrorMps =
        flightStyle == FlightStyle::Extreme ? 9.0 : 6.0;
    p.tracking.forwardAngleErrorRad =
        law == Law::Newtonian ? 1.20 : 0.55;
    p.tracking.angularVelocityErrorRadPerSec = 0.9;
    p.tracking.linearFeedbackReserveMps2 = 0.55;
    p.tracking.angularFeedbackReserveRadPerSec2 = 0.35;

    p.capability.revision = 1;
    p.capability.maxForwardAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    p.capability.maxReverseAccelerationMetersPerSec2 =
        7.5 * 9.80665;
    p.capability.maxLateralAccelerationMetersPerSec2 = 2.0;
    p.capability.maxVerticalAccelerationMetersPerSec2 = 2.0;
    p.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    p.capability.maxAngularSpeedRadPerSec = 2.5;

    p.sampleCount =
        static_cast<std::uint8_t>(Program::kMaxSamples);

    std::array<glm::dquat, Program::kMaxSamples> orientations {};
    Basis previous = startBasis;

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        const double u =
            static_cast<double>(i) /
            static_cast<double>(Program::kMaxSamples - 1);
        const double t = duration * u;

        glm::dvec3 position;
        glm::dvec3 velocity;
        glm::dvec3 acceleration;
        sampleCurve(curve, t, position, velocity, acceleration);

        Basis basis = startBasis;

        if (attitudeMode == AttitudeMode::VelocityAligned)
        {
            basis =
                basisFromForwardUp(
                    glm::length(velocity) > 0.25
                        ? velocity
                        : previous.forward,
                    previous.up
                );
        }
        else if (attitudeMode == AttitudeMode::TargetBasis)
        {
            const glm::dquat q0 =
                quaternionForBasis(startBasis);
            const glm::dquat q1 =
                quaternionForBasis(targetBasis);
            basis =
                basisForQuaternion(
                    glm::slerp(q0, q1, u)
                );
        }

        if (law == Law::Newtonian &&
            attitudeMode == AttitudeMode::VelocityAligned)
        {
            basis = startBasis;
        }

        previous = basis;
        orientations[i] = quaternionForBasis(basis);

        auto& sample = p.samples[i];
        sample.timeOffsetSeconds = t;
        sample.positionMapMeters = position;
        sample.velocityMapMetersPerSecond = velocity;
        sample.linearAccelerationFeedForwardMapMps2 = acceleration;
        sample.forwardMap = basis.forward;
        sample.rightMap = basis.right;
        sample.upMap = basis.up;
    }

    const double sampleDt =
        duration /
        static_cast<double>(Program::kMaxSamples - 1);

    std::array<glm::dvec3, Program::kMaxSamples> omega {};
    for (std::size_t i = 1; i < Program::kMaxSamples; ++i)
    {
        if (i + 1 == Program::kMaxSamples)
        {
            omega[i] =
                angularVelocityBetween(
                    orientations[i - 1],
                    orientations[i],
                    sampleDt
                );
        }
        else
        {
            omega[i] =
                angularVelocityBetween(
                    orientations[i - 1],
                    orientations[i + 1],
                    2.0 * sampleDt
                );
        }
        p.samples[i].angularVelocityMapRadPerSecond = omega[i];
    }

    for (std::size_t i = 0; i < Program::kMaxSamples; ++i)
    {
        glm::dvec3 alpha(0.0);
        if (i == 0)
            alpha = omega[1] / sampleDt;
        else if (i + 1 == Program::kMaxSamples)
            alpha = (omega[i] - omega[i - 1]) / sampleDt;
        else
            alpha = (omega[i + 1] - omega[i - 1]) / (2.0 * sampleDt);

        p.samples[i].angularAccelerationFeedForwardMapRadPerSec2 =
            alpha;
    }

    p.proof.minimumLinearAuthorityReserveMps2 = 0.05;
    p.proof.minimumAngularAuthorityReserveRadPerSec2 = 0.05;
    return p;
}

Program makeBrakeProgram(
    std::uint64_t revision,
    const Vehicle& vehicle
)
{
    const glm::dvec3 velocity =
        vehicle.transform.motion.localVelocityMps;
    const glm::dvec3 target =
        vehicle.transform.motion.localPositionMeters +
        velocity * 0.8;

    return makeShortProgram(
        revision,
        vehicle,
        target,
        {0.0, 0.0, 0.0},
        vehicle.transform.motion.localControlLaw,
        FlightStyle::Standard,
        AttitudeMode::Preserve,
        actualBasis(vehicle)
    );
}

TraceFrame makeTraceFrame(
    const Vehicle& vehicle,
    const Program* program,
    const Planner::Result* plan,
    const DynamicObstacle* primaryHazard,
    glm::dvec3 hazardPosition,
    glm::dvec3 hazardVelocity,
    double lookAheadSeconds,
    const std::string& phase,
    bool replanEvent
)
{
    TraceFrame frame;
    frame.timeSeconds = vehicle.timeSeconds;
    frame.shipPosition =
        vehicle.transform.motion.localPositionMeters;
    frame.shipForward =
        glm::dvec3(vehicle.transform.forward());
    frame.shipRight =
        glm::dvec3(vehicle.transform.right());
    frame.shipUp =
        glm::dvec3(vehicle.transform.up());
    frame.shipVelocity =
        vehicle.transform.motion.localVelocityMps;

    if (program)
    {
        const auto sampled =
            Sampler::sample(*program, vehicle.timeSeconds);
        if (sampled.status == Sampler::Status::Active ||
            sampled.status == Sampler::Status::AfterEnd)
        {
            frame.hasProgramReference = true;
            frame.programReferencePosition =
                sampled.reference.positionMapMeters;
            frame.programReferenceForward =
                sampled.reference.forwardMap;
            frame.programReferenceRight =
                sampled.reference.rightMap;
            frame.programReferenceUp =
                sampled.reference.upMap;
            frame.programTrackingCorridorRadiusMeters =
                program->tracking.positionErrorMeters;
        }
    }

    if (plan)
    {
        frame.plannerStatus =
            plannerStatusName(plan->status);
        frame.hasSelectedTarget = true;
        frame.selectedTarget =
            plan->selectedTargetMapMeters;

        if (plan->adjustedTarget)
        {
            frame.hasReacquisitionTarget = true;
            frame.reacquisitionTarget =
                plan->localBypassMergeTargetMapMeters;
        }
    }

    frame.phase = phase;
    frame.replanEvent = replanEvent;

    if (primaryHazard)
    {
        frame.hazardActive = true;
        frame.hazardPosition = hazardPosition;
        frame.hazardVelocity = hazardVelocity;
        frame.hazardRadiusMeters =
            primaryHazard->radiusMeters;
        frame.hazardCollisionEnvelopeRadiusMeters =
            primaryHazard->radiusMeters +
            kHullBoundingRadiusMeters;
        frame.hazardPlannerEnvelopeRadiusMeters =
            frame.hazardCollisionEnvelopeRadiusMeters +
            3.5;
        frame.plannerLookAheadSeconds =
            lookAheadSeconds;
        frame.dynamicClearanceMeters =
            glm::length(frame.shipPosition - hazardPosition) -
            frame.hazardCollisionEnvelopeRadiusMeters;
    }

    return frame;
}

bool closeEnough(
    const Vehicle& vehicle,
    const glm::dvec3& target,
    double radius
)
{
    return
        glm::length(
            vehicle.transform.motion.localPositionMeters -
            target
        ) <= radius;
}

double forwardErrorRad(
    const Vehicle& vehicle,
    const glm::dvec3& desired
)
{
    const glm::dvec3 a =
        normalizedOr(
            glm::dvec3(vehicle.transform.forward()),
            {1.0, 0.0, 0.0}
        );
    const glm::dvec3 b =
        normalizedOr(desired, a);
    return
        std::acos(
            std::clamp(glm::dot(a, b), -1.0, 1.0)
        );
}

bool finalSatisfied(
    const Vehicle& vehicle,
    const Endpoint& finish
)
{
    if (!closeEnough(vehicle, finish.position, 2.0))
        return false;

    const double actualSpeed =
        glm::length(
            vehicle.transform.motion.localVelocityMps
        );

    if (std::abs(actualSpeed - finish.speedMps) > 1.0)
        return false;

    if (finish.requireForward &&
        forwardErrorRad(vehicle, finish.forward) > 0.10)
    {
        return false;
    }

    if (finish.requireUp)
    {
        const glm::dvec3 actualUp =
            normalizedOr(
                glm::dvec3(vehicle.transform.up()),
                {0.0, 1.0, 0.0}
            );
        const glm::dvec3 desiredUp =
            normalizedOr(
                finish.up,
                actualUp
            );
        if (std::acos(
                std::clamp(
                    glm::dot(actualUp, desiredUp),
                    -1.0,
                    1.0
                )
            ) > 0.12)
        {
            return false;
        }
    }

    return true;
}

} // namespace

ScenarioRunResult calculateScenario(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings
)
{
    ScenarioRunResult out;

    try
    {
        Scenario scenario = loadScenario(scenarioJsonPath);

        const Law law =
            settings.controlMode == ControlMode::Newtonian
                ? Law::Newtonian
                : Law::Assisted;

        Vehicle vehicle(law, scenario, settings.pilot);
        Space space = buildStaticSpace(scenario);
        const StaticQueries staticQueries(space);

        const Planner::Policy policy =
            plannerPolicy(settings.flightStyle);

        Map::Config mapConfig;
        mapConfig.halfExtentMeters = 2000.0;
        mapConfig.cellSizeMeters = 50.0;
        mapConfig.predictionHorizonSeconds =
            policy.horizon.lookAheadSeconds;
        mapConfig.interactionMarginMeters = 0.0;
        Map map(mapConfig);

        TraceDocument trace;
        trace.version = 2;
        trace.law =
            law == Law::Newtonian
                ? "newtonian"
                : "assisted";
        trace.shipHalfExtentsMeters = kBodyHalfExtents;
        trace.routePoints.push_back(scenario.startPosition);
        trace.turnPoints.clear();

        for (const auto& obstacle : scenario.staticObstacles)
        {
            TraceStaticObstacle outObstacle;
            outObstacle.id = obstacle.id;
            outObstacle.center = obstacle.centerMeters;
            outObstacle.halfExtents = obstacle.halfExtentsMeters;
            outObstacle.radiusMeters = obstacle.radiusMeters;
            outObstacle.capsuleHalfLengthMeters =
                obstacle.capsuleHalfLengthMeters;

            switch (obstacle.shape)
            {
                case world::navigation::NavigationObstacleShape::Box:
                    outObstacle.shape = "box";
                    break;
                case world::navigation::NavigationObstacleShape::Capsule:
                    outObstacle.shape = "capsule";
                    break;
                case world::navigation::NavigationObstacleShape::Sphere:
                default:
                    outObstacle.shape = "sphere";
                    break;
            }

            trace.staticObstacles.push_back(std::move(outObstacle));
        }

        std::vector<glm::dvec3> objectives =
            scenario.shipRoutePoints;
        objectives.push_back(scenario.finish.position);

        const double cruiseSpeed =
            settings.flightStyle == FlightStyle::Extreme
                ? scenario.extremeSpeedMps
                : scenario.standardSpeedMps;

        std::uint64_t programRevision = 1000;
        std::uint64_t dynamicRevision = 1;

        if (
            trace.routePoints.empty() ||
            glm::length(
                trace.routePoints.back() -
                scenario.finish.position
            ) > 1.0)
        {
            trace.routePoints.push_back(
                scenario.finish.position
            );
        }

        trace.frames.push_back(
            makeTraceFrame(
                vehicle,
                nullptr,
                nullptr,
                nullptr,
                {},
                {},
                policy.horizon.lookAheadSeconds,
                "initial",
                false
            )
        );

        bool failed = false;
        std::string failureMessage;

        for (std::size_t objectiveIndex = 0;
             objectiveIndex < objectives.size() && !failed;
             ++objectiveIndex)
        {
            const bool finalObjective =
                objectiveIndex + 1 == objectives.size();
            const glm::dvec3 objective =
                objectives[objectiveIndex];

            while (vehicle.timeSeconds < kMaximumScenarioSeconds)
            {
                if (!finalObjective &&
                    closeEnough(vehicle, objective, 4.0))
                {
                    break;
                }

                if (finalObjective &&
                    finalSatisfied(vehicle, scenario.finish))
                {
                    break;
                }

                auto dynamic =
                    publishDynamicWorld(
                        map,
                        scenario.dynamicObstacles,
                        scenario.hasSuddenObstacle
                            ? &scenario.suddenObstacle
                            : nullptr,
                        settings.enableSuddenObstacle,
                        vehicle,
                        dynamicRevision++,
                        policy.horizon.lookAheadSeconds
                    );

                Planner::Goal goal;
                goal.revision =
                    static_cast<std::uint64_t>(
                        objectiveIndex + 1
                    );
                goal.targetPositionMapMeters = objective;
                goal.maximumTargetSpeedMps = cruiseSpeed;
                goal.velocityResponsePerSecond =
                    settings.flightStyle == FlightStyle::Extreme
                        ? 1.0
                        : 0.75;
                goal.angularDampingPerSecond = 2.0;
                goal.arrivalRadiusMeters =
                    finalObjective ? 1.5 : 4.0;

                if (finalObjective)
                {
                    if (scenario.finish.speedMps > 0.0)
                    {
                        const glm::dvec3 forward =
                            scenario.finish.requireForward
                                ? normalizedOr(
                                    scenario.finish.forward,
                                    {1.0, 0.0, 0.0}
                                  )
                                : normalizedOr(
                                    objective -
                                    vehicle.transform.motion.localPositionMeters,
                                    {1.0, 0.0, 0.0}
                                  );
                        goal.targetVelocityMapMetersPerSecond =
                            forward * scenario.finish.speedMps;
                    }
                }

                const Planner::Result plan =
                    Planner::plan(
                        plannerAgent(vehicle, law),
                        goal,
                        dynamic,
                        vehicle.timeSeconds,
                        staticQueries,
                        policy
                    );

                if (
                    trace.routePoints.empty() ||
                    glm::length(
                        trace.routePoints.back() -
                        plan.selectedTargetMapMeters
                    ) > 1.0)
                {
                    trace.routePoints.push_back(
                        plan.selectedTargetMapMeters
                    );
                }

                if (
                    plan.adjustedTarget &&
                    (
                        trace.turnPoints.empty() ||
                        glm::length(
                            trace.turnPoints.back() -
                            plan.selectedTargetMapMeters
                        ) > 2.0
                    )
                )
                {
                    trace.turnPoints.push_back(
                        plan.selectedTargetMapMeters
                    );
                }

                const bool hardHold =
                    plan.status == Planner::Status::ConflictHold ||
                    plan.status == Planner::Status::StaleHold ||
                    plan.status == Planner::Status::StaticHold ||
                    plan.status == Planner::Status::InvalidInput;

                glm::dvec3 requestedVelocity =
                    plan.desiredVelocityMapMetersPerSecond;

                const double requestedSpeed =
                    glm::length(requestedVelocity);
                if (requestedSpeed > cruiseSpeed &&
                    requestedSpeed > 1.0e-9)
                {
                    requestedVelocity *=
                        cruiseSpeed / requestedSpeed;
                }

                AttitudeMode attitudeMode =
                    law == Law::Assisted
                        ? AttitudeMode::VelocityAligned
                        : AttitudeMode::Preserve;

                Basis targetBasis = actualBasis(vehicle);

                if (finalObjective)
                {
                    const double distanceToFinish =
                        glm::length(
                            scenario.finish.position -
                            vehicle.transform.motion.localPositionMeters
                        );

                    if (distanceToFinish < 45.0 &&
                        (scenario.finish.requireForward ||
                         scenario.finish.requireUp))
                    {
                        const glm::dvec3 forward =
                            scenario.finish.requireForward
                                ? scenario.finish.forward
                                : targetBasis.forward;
                        const glm::dvec3 up =
                            scenario.finish.requireUp
                                ? scenario.finish.up
                                : targetBasis.up;
                        targetBasis =
                            basisFromForwardUp(forward, up);
                        attitudeMode =
                            AttitudeMode::TargetBasis;
                    }

                    if (distanceToFinish < 20.0)
                    {
                        requestedVelocity =
                            goal.targetVelocityMapMetersPerSecond;
                    }
                }

                Program program =
                    hardHold
                        ? makeBrakeProgram(
                            programRevision++,
                            vehicle
                          )
                        : makeShortProgram(
                            programRevision++,
                            vehicle,
                            plan.selectedTargetMapMeters,
                            requestedVelocity,
                            law,
                            settings.flightStyle,
                            attitudeMode,
                            targetBasis
                          );

                const DynamicObstacle* primaryHazard = nullptr;
                glm::dvec3 hazardPosition(0.0);
                glm::dvec3 hazardVelocity(0.0);

                auto considerHazard =
                    [&](DynamicObstacle& obstacle)
                    {
                        if (vehicle.timeSeconds <
                            obstacle.activationTimeSeconds)
                        {
                            return;
                        }

                        glm::dvec3 velocity;
                        const glm::dvec3 position =
                            obstaclePosition(
                                obstacle,
                                vehicle.timeSeconds,
                                vehicle,
                                velocity
                            );

                        if (!primaryHazard ||
                            glm::length(
                                position -
                                vehicle.transform.motion.localPositionMeters
                            ) <
                            glm::length(
                                hazardPosition -
                                vehicle.transform.motion.localPositionMeters
                            ))
                        {
                            primaryHazard = &obstacle;
                            hazardPosition = position;
                            hazardVelocity = velocity;
                        }
                    };

                for (auto& obstacle : scenario.dynamicObstacles)
                    considerHazard(obstacle);

                if (settings.enableSuddenObstacle &&
                    scenario.hasSuddenObstacle)
                {
                    considerHazard(scenario.suddenObstacle);
                }

                trace.frames.push_back(
                    makeTraceFrame(
                        vehicle,
                        &program,
                        &plan,
                        primaryHazard,
                        hazardPosition,
                        hazardVelocity,
                        policy.horizon.lookAheadSeconds,
                        hardHold
                            ? "dynamic_brake_0"
                            : (
                                plan.adjustedTarget
                                    ? "dynamic_bypass_0"
                                    : "nominal_segment"
                              ),
                        true
                    )
                );

                const double executeUntil =
                    vehicle.timeSeconds +
                    kReplanPeriodSeconds;

                while (vehicle.timeSeconds + 1.0e-9 < executeUntil)
                {
                    const auto follower =
                        Follower::follow(
                            program,
                            vehicle.timeSeconds,
                            followerAgent(vehicle)
                        );

                    if (follower.status ==
                        Follower::Status::InvalidInput)
                    {
                        failed = true;
                        failureMessage =
                            "follower rejected accepted program";
                        break;
                    }

                    const auto bridgeResult =
                        vehicle.bridge.step(
                            vehicle.timeSeconds + kDt,
                            kDt,
                            toSystemIntent(follower.intent)
                        );

                    if (bridgeResult.status !=
                        Bridge::PilotExecutor::Status::Ok)
                    {
                        failed = true;
                        failureMessage =
                            "pilot executor rejected navigation command";
                        break;
                    }

                    SharedShipPhysics::integrate(
                        vehicle.transform,
                        vehicle.params,
                        bridgeResult.control,
                        vehicle.world,
                        static_cast<float>(kDt)
                    );

                    game::navigation::DynamicMotionSystem::
                        applySystemAccelerationDemand(
                            vehicle.transform.motion,
                            vehicle.params,
                            bridgeResult.control.
                                navigationLinearAccelerationDemandSystemMps2,
                            vehicle.transform.forward()
                        );

                    game::navigation::DynamicMotionSystem::
                        updateLocalFrameMotion(
                            vehicle.transform.motion,
                            vehicle.transform.worldPosition,
                            vehicle.frame,
                            vehicle.params,
                            kDt
                        );

                    vehicle.transform.syncLegacyPositionFromWorld();
                    vehicle.timeSeconds += kDt;

                    if (primaryHazard)
                    {
                        glm::dvec3 velocity;
                        hazardPosition =
                            obstaclePosition(
                                *const_cast<DynamicObstacle*>(
                                    primaryHazard
                                ),
                                vehicle.timeSeconds,
                                vehicle,
                                velocity
                            );
                        hazardVelocity = velocity;
                    }

                    trace.frames.push_back(
                        makeTraceFrame(
                            vehicle,
                            &program,
                            &plan,
                            primaryHazard,
                            hazardPosition,
                            hazardVelocity,
                            policy.horizon.lookAheadSeconds,
                            hardHold
                                ? "dynamic_brake_0"
                                : (
                                    plan.adjustedTarget
                                        ? "dynamic_bypass_0"
                                        : "nominal_segment"
                                  ),
                            false
                        )
                    );
                }

                if (failed)
                    break;
            }

            if (!failed &&
                vehicle.timeSeconds >= kMaximumScenarioSeconds)
            {
                failed = true;
                failureMessage =
                    "scenario exceeded maximum simulation time";
            }
        }

        trace.frames.push_back(
            makeTraceFrame(
                vehicle,
                nullptr,
                nullptr,
                nullptr,
                {},
                {},
                policy.horizon.lookAheadSeconds,
                failed ? "failed" : "complete",
                false
            )
        );

        out.trace = std::move(trace);
        out.success = !failed;
        out.message =
            failed
                ? failureMessage
                : "calculation complete";
    }
    catch (const std::exception& e)
    {
        out.success = false;
        out.message = e.what();
    }

    return out;
}

} // namespace elite::tools::navigation_runtime
