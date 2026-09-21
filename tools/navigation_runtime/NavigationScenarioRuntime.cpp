#include "NavigationScenarioRuntime.h"

#include "src/game/navigation/NominalRoutePlanner.h"
#include "src/game/navigation/AcceptedManeuverProgram.h"
#include "src/game/navigation/TrajectoryFollower.h"
#include "src/game/navigation/ManeuverProgramSampler.h"
#include "src/game/navigation/ManeuverPhaseGate.h"
#include "src/game/navigation/NavigationRuntimeControlBridge.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"
#include "src/world/navigation/NavigationObstacle.h"
#include "src/world/navigation/TrajectoryGenerator.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <glm/gtc/quaternion.hpp>

namespace elite::tools::navigation_runtime
{
namespace
{

const glm::dvec3 kBodyHalfExtents {13.0, 2.5, 11.1};

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

// Parsed now so the Stage-1 scenario format already has the correct ownership
// boundary for Stage 2. These records are intentionally NOT passed to the
// nominal static route planner.
struct DynamicObstacleDefinition
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
};

struct Scenario
{
    std::uint64_t goalRevision = 1;
    std::uint64_t staticWorldRevision = 1;
    std::uint64_t dynamicWorldRevision = 1;

    glm::dvec3 startPosition {0.0};
    glm::dvec3 startVelocity {6.0, 0.0, 0.0};
    glm::dvec3 startAcceleration {0.0};
    double startPitchRateRadPerSec = 0.0;
    double startYawRateRadPerSec = 0.0;
    double startRollRateRadPerSec = 0.0;
    Basis startBasis {};

    std::vector<glm::dvec3> shipRoutePoints;
    Endpoint finish;

    std::vector<world::navigation::NavigationObstacle> staticObstacles;
    std::vector<DynamicObstacleDefinition> dynamicObstacles;
    DynamicObstacleDefinition suddenObstacle;
    bool hasSuddenObstacle = false;

    double standardSpeedMps = 10.0;
    double extremeSpeedMps = 18.0;

    // Coarse Stage-1 route/corridor abstraction only. Exact oriented-hull
    // swept-volume clearance belongs to the later physical tunnel stage.
    double routeEnvelopeRadiusMeters = 13.0;
    double routeClearanceMeters = 0.0;
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
    if (!(length > 1.0e-9))
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
        upInput - forward * glm::dot(upInput, forward);

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

DynamicObstacleDefinition parseDynamicObstacle(
    const nlohmann::json& src,
    std::uint64_t entity
)
{
    DynamicObstacleDefinition obstacle;
    obstacle.entityId = entity;
    obstacle.id =
        src.value(
            "id",
            std::string("dynamic_") + std::to_string(entity)
        );
    obstacle.initialPosition =
        readVec3(src, "position", {0.0, 0.0, 0.0});
    obstacle.linearVelocity =
        readVec3(src, "velocity", {0.0, 0.0, 0.0});
    obstacle.activationTimeSeconds =
        src.value("activation_time_s", 0.0);
    obstacle.radiusMeters =
        src.value("radius_m", 5.0);

    if (src.contains("route_points"))
    {
        for (const auto& p : src.at("route_points"))
        {
            obstacle.path.points.push_back(
                readVec3Value(
                    p,
                    "moving obstacle route_points[]"
                )
            );
        }
        obstacle.path.speedMps =
            src.value("route_speed_mps", 5.0);
        obstacle.path.loop =
            src.value("route_loop", false);
    }

    if (src.contains("spawn_relative_to_ship_fru"))
    {
        obstacle.spawnRelativeToShip = true;
        obstacle.spawnRelativeFruMeters =
            readVec3(
                src,
                "spawn_relative_to_ship_fru",
                {0.0, 0.0, 0.0}
            );
    }

    return obstacle;
}

Scenario loadScenario(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("cannot open scenario JSON: " + path);

    nlohmann::json root;
    stream >> root;

    Scenario scenario;
    scenario.goalRevision =
        root.value("goal_revision", std::uint64_t{1});
    scenario.staticWorldRevision =
        root.value("static_world_revision", std::uint64_t{1});
    scenario.dynamicWorldRevision =
        root.value("dynamic_world_revision", std::uint64_t{1});

    if (root.contains("start"))
    {
        const auto& start = root.at("start");
        scenario.startPosition =
            readVec3(start, "position", scenario.startPosition);
        scenario.startVelocity =
            readVec3(start, "velocity", scenario.startVelocity);
        scenario.startAcceleration =
            readVec3(start, "acceleration", scenario.startAcceleration);
        scenario.startPitchRateRadPerSec =
            start.value("pitch_rate_rad_s", 0.0);
        scenario.startYawRateRadPerSec =
            start.value("yaw_rate_rad_s", 0.0);
        scenario.startRollRateRadPerSec =
            start.value("roll_rate_rad_s", 0.0);

        const glm::dvec3 forward =
            readVec3(start, "forward", scenario.startBasis.forward);
        const glm::dvec3 up =
            readVec3(start, "up", scenario.startBasis.up);
        scenario.startBasis = basisFromForwardUp(forward, up);
    }

    if (root.contains("ship_route_points"))
    {
        for (const auto& p : root.at("ship_route_points"))
        {
            scenario.shipRoutePoints.push_back(
                readVec3Value(p, "ship_route_points[]")
            );
        }
    }

    if (root.contains("finish"))
    {
        const auto& finish = root.at("finish");
        scenario.finish.position =
            readVec3(
                finish,
                "position",
                scenario.finish.position
            );

        if (finish.contains("forward"))
        {
            scenario.finish.requireForward = true;
            scenario.finish.forward =
                readVec3(
                    finish,
                    "forward",
                    scenario.finish.forward
                );
        }

        if (finish.contains("up"))
        {
            scenario.finish.requireUp = true;
            scenario.finish.up =
                readVec3(
                    finish,
                    "up",
                    scenario.finish.up
                );
        }

        scenario.finish.speedMps =
            finish.value("speed_mps", 0.0);
    }

    scenario.standardSpeedMps =
        root.value("standard_speed_mps", 10.0);
    scenario.extremeSpeedMps =
        root.value("extreme_speed_mps", 18.0);
    scenario.routeEnvelopeRadiusMeters =
        root.value("route_envelope_radius_m", 13.0);
    scenario.routeClearanceMeters =
        root.value("route_clearance_m", 0.0);

    std::uint32_t staticEntity = 50000;
    if (root.contains("static_obstacles"))
    {
        for (const auto& src : root.at("static_obstacles"))
        {
            world::navigation::NavigationObstacle obstacle;
            obstacle.id =
                src.value(
                    "id",
                    std::string("static_") +
                        std::to_string(staticEntity)
                );
            obstacle.entityId = staticEntity++;
            obstacle.centerMeters =
                readVec3(src, "center", {0.0, 0.0, 0.0});
            obstacle.requiredClearanceMeters =
                src.value("required_clearance_m", 0.0);

            const std::string shape =
                src.value("shape", std::string("sphere"));

            if (shape == "box")
            {
                obstacle.shape =
                    world::navigation::NavigationObstacleShape::Box;
                obstacle.halfExtentsMeters =
                    readVec3(
                        src,
                        "half_extents",
                        {1.0, 1.0, 1.0}
                    );
            }
            else if (shape == "capsule")
            {
                obstacle.shape =
                    world::navigation::NavigationObstacleShape::Capsule;
                obstacle.radiusMeters =
                    src.value("radius_m", 1.0);
                obstacle.capsuleHalfLengthMeters =
                    src.value("half_length_m", 1.0);
            }
            else
            {
                obstacle.shape =
                    world::navigation::NavigationObstacleShape::Sphere;
                obstacle.radiusMeters =
                    src.value("radius_m", 1.0);
            }

            scenario.staticObstacles.push_back(std::move(obstacle));
        }
    }

    std::uint64_t dynamicEntity = 60000;
    if (root.contains("moving_obstacles"))
    {
        for (const auto& src : root.at("moving_obstacles"))
        {
            scenario.dynamicObstacles.push_back(
                parseDynamicObstacle(src, dynamicEntity++)
            );
        }
    }

    if (root.contains("sudden_obstacle"))
    {
        scenario.suddenObstacle =
            parseDynamicObstacle(
                root.at("sudden_obstacle"),
                69999
            );
        scenario.hasSuddenObstacle = true;
    }

    return scenario;
}

TraceStaticObstacle traceObstacle(
    const world::navigation::NavigationObstacle& obstacle
)
{
    TraceStaticObstacle out;
    out.id = obstacle.id;
    out.center = obstacle.centerMeters;
    out.halfExtents = obstacle.halfExtentsMeters;
    out.radiusMeters = obstacle.radiusMeters;
    out.capsuleHalfLengthMeters =
        obstacle.capsuleHalfLengthMeters;

    switch (obstacle.shape)
    {
        case world::navigation::NavigationObstacleShape::Box:
            out.shape = "box";
            break;
        case world::navigation::NavigationObstacleShape::Capsule:
            out.shape = "capsule";
            break;
        case world::navigation::NavigationObstacleShape::Sphere:
        default:
            out.shape = "sphere";
            break;
    }

    return out;
}

TraceFrame routeFrame(
    const Scenario& scenario,
    bool routeValid
)
{
    TraceFrame frame;
    frame.timeSeconds = 0.0;
    frame.shipPosition = scenario.startPosition;
    frame.shipForward = scenario.startBasis.forward;
    frame.shipRight = scenario.startBasis.right;
    frame.shipUp = scenario.startBasis.up;
    frame.shipVelocity = scenario.startVelocity;
    frame.phase =
        routeValid ? "route_ready" : "route_failed";
    frame.plannerStatus =
        routeValid
            ? "static_route_ready"
            : "static_route_failed";

    if (routeValid)
    {
        frame.hasSelectedTarget = true;
        frame.selectedTarget = scenario.finish.position;
    }

    return frame;
}

std::string formatVec3(const glm::dvec3& value)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out << std::setprecision(2)
        << "("
        << value.x << ", "
        << value.y << ", "
        << value.z << ")";
    return out.str();
}

std::vector<std::string> previewDiagnostics(
    const Scenario& scenario
)
{
    return {
        "SCENE: LOADED",
        "PLANNER: NOT RUN",
        "FOLLOWER: NOT RUN (STAGE 1)",
        "START: " + formatVec3(scenario.startPosition),
        "START VELOCITY: " + formatVec3(scenario.startVelocity) + " M/S",
        "START ACCELERATION: " + formatVec3(scenario.startAcceleration) + " M/S2",
        "START ANGULAR RATE P/Y/R: (" +
            std::to_string(scenario.startPitchRateRadPerSec) + ", " +
            std::to_string(scenario.startYawRateRadPerSec) + ", " +
            std::to_string(scenario.startRollRateRadPerSec) + ") RAD/S",
        "FINISH: " + formatVec3(scenario.finish.position),
        "STATIC OBSTACLES: " +
            std::to_string(scenario.staticObstacles.size()),
        "DYNAMIC INPUTS RESERVED: " +
            std::to_string(
                scenario.dynamicObstacles.size() +
                (scenario.hasSuddenObstacle ? 1u : 0u)
            )
    };
}

std::vector<std::string> routeDiagnostics(
    const Scenario& scenario,
    const game::navigation::NominalRoutePlanner::Plan& route
)
{
    std::ostringstream length;
    length.setf(std::ios::fixed);
    length << std::setprecision(2) << route.lengthMeters;

    std::vector<std::string> lines {
        "SCENE: LOADED",
        std::string("PLANNER: ") + (route.valid ? "OK" : "FAIL"),
        "FOLLOWER: NOT RUN (STAGE 1)",
        "START: " + formatVec3(scenario.startPosition),
        "START VELOCITY: " + formatVec3(scenario.startVelocity) + " M/S",
        "START ACCELERATION: " + formatVec3(scenario.startAcceleration) + " M/S2",
        "START ANGULAR RATE P/Y/R: (" +
            std::to_string(scenario.startPitchRateRadPerSec) + ", " +
            std::to_string(scenario.startYawRateRadPerSec) + ", " +
            std::to_string(scenario.startRollRateRadPerSec) + ") RAD/S",
        "FINISH: " + formatVec3(scenario.finish.position),
        "STATIC OBSTACLES: " +
            std::to_string(scenario.staticObstacles.size()),
        "REQUIRED WAYPOINTS: " +
            std::to_string(scenario.shipRoutePoints.size()),
        "ROUTE POINTS: " +
            std::to_string(route.pointsMapMeters.size()),
        "ROUTE LENGTH: " + length.str() + " M",
        std::string("STATIC DETOUR: ") +
            (route.staticDetourUsed ? "YES" : "NO"),
        "GOAL REVISION: " +
            std::to_string(route.goalRevision),
        "STATIC WORLD REVISION: " +
            std::to_string(route.staticWorldRevision),
        "LOG: last_route_plan.log"
    };

    if (!route.message.empty())
        lines.push_back("PLANNER MESSAGE: " + route.message);

    for (std::size_t i = 0; i < route.pointsMapMeters.size(); ++i)
    {
        lines.push_back(
            "ROUTE POINT " + std::to_string(i) + ": " +
            formatVec3(route.pointsMapMeters[i])
        );
    }

    return lines;
}

void writeRouteDiagnostics(
    const std::string& scenarioJsonPath,
    const std::vector<std::string>& diagnostics
)
{
    const std::filesystem::path scenarioPath(scenarioJsonPath);
    const std::filesystem::path output =
        scenarioPath.parent_path() / "last_route_plan.log";

    std::ofstream stream(output);
    if (!stream)
        throw std::runtime_error(
            "cannot write route diagnostics: " + output.string()
        );

    for (const auto& line : diagnostics)
    {
        stream << line << "\n";
        std::cout << "[NAV-STAGE1] " << line << "\n";
    }
}

void setSceneEndpoints(
    TraceDocument& trace,
    const Scenario& scenario
)
{
    trace.hasSceneEndpoints = true;
    trace.sceneStartMapMeters = scenario.startPosition;
    trace.sceneFinishMapMeters = scenario.finish.position;
}

// -----------------------------------------------------------------------------
// Stage 2 — execute the retained Stage-1 route.
// The global/static route is an input here and is never rebuilt.
// -----------------------------------------------------------------------------

using Program = game::navigation::AcceptedManeuverProgram;
using Follower = game::navigation::TrajectoryFollower;
using Bridge = game::navigation::NavigationRuntimeControlBridge;
using Law = game::navigation::LocalFlightControlLaw;

constexpr double kExecutionDt = 1.0 / 120.0;
constexpr double kTraceSampleSeconds = 1.0 / 30.0;
constexpr double kStandardGravity = 9.80665;

ShipParams cobraParams()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;

    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 29979245.0f;
    p.throttleAccel = 5.0f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 20.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 2.0f;
    p.manoeuvreGasUsePerSecond = 0.20f;
    p.manoeuvreGasRechargePerSecond = 0.08f;
    p.manoeuvreGasRestartFraction = 0.20f;

    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

Bridge::PilotSkillProfile pilotProfile(PilotLevel level)
{
    Bridge::PilotSkillProfile profile;
    profile.execution.deterministicSeed = 0xC0A1B17Eull;

    switch (level)
    {
        case PilotLevel::Average:
            profile.execution.reactionDelaySeconds = 0.15;
            profile.execution.perceptionDecisionRateHz = 20.0;
            profile.execution.commandLatencySeconds = 0.08;
            profile.execution.responseFrequencyHz = 5.0;
            profile.execution.dampingRatio = 1.0;
            profile.execution.commandGain = 0.96;
            profile.execution.maxLinearCommandSlewMetersPerSec3 = 180.0;
            profile.execution.maxAngularCommandSlewRadPerSec3 = 18.0;
            profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 =
                0.08;
            profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 =
                0.015;
            break;

        case PilotLevel::Loser:
            profile.execution.reactionDelaySeconds = 0.40;
            profile.execution.perceptionDecisionRateHz = 8.0;
            profile.execution.commandLatencySeconds = 0.18;
            profile.execution.responseFrequencyHz = 2.5;
            profile.execution.dampingRatio = 0.85;
            profile.execution.commandGain = 0.88;
            profile.execution.maxLinearCommandSlewMetersPerSec3 = 70.0;
            profile.execution.maxAngularCommandSlewRadPerSec3 = 7.0;
            profile.execution.deterministicLinearNoiseAmplitudeMetersPerSec2 =
                0.22;
            profile.execution.deterministicAngularNoiseAmplitudeRadPerSec2 =
                0.045;
            break;

        case PilotLevel::Expert:
        default:
            profile.execution.reactionDelaySeconds = 0.0;
            profile.execution.perceptionDecisionRateHz = 100.0;
            profile.execution.commandLatencySeconds = 0.0;
            profile.execution.responseFrequencyHz = 10.0;
            profile.execution.dampingRatio = 1.0;
            profile.execution.commandGain = 1.0;
            profile.execution.maxLinearCommandSlewMetersPerSec3 = 1000.0;
            profile.execution.maxAngularCommandSlewRadPerSec3 = 1000.0;
            break;
    }

    return profile;
}

const char* pilotName(PilotLevel level)
{
    switch (level)
    {
        case PilotLevel::Average: return "AVERAGE";
        case PilotLevel::Loser: return "LOSER";
        case PilotLevel::Expert:
        default: return "EXPERT";
    }
}

const char* flightStyleName(FlightStyle style)
{
    return style == FlightStyle::Extreme
        ? "EXTREME"
        : "STANDARD";
}

Law controlLaw(ControlMode mode)
{
    return mode == ControlMode::Newtonian
        ? Law::Newtonian
        : Law::Assisted;
}

void setTransformBasis(
    ShipTransform& transform,
    const Basis& basis
)
{
    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] =
        glm::vec4(glm::vec3(basis.right), 0.0f);
    transform.orientation[1] =
        glm::vec4(glm::vec3(basis.up), 0.0f);
    transform.orientation[2] =
        glm::vec4(glm::vec3(-basis.forward), 0.0f);
}

Basis transportedBasisForForward(
    const glm::dvec3& requestedForward,
    const Basis& previous
)
{
    if (glm::length(requestedForward) <= 1.0e-9)
        return previous;

    const glm::dvec3 forward = glm::normalize(requestedForward);

    glm::dvec3 right =
        previous.right -
        forward * glm::dot(previous.right, forward);

    if (glm::length(right) <= 1.0e-9)
    {
        glm::dvec3 upSeed = previous.up;
        upSeed -= forward * glm::dot(upSeed, forward);

        if (glm::length(upSeed) <= 1.0e-9)
        {
            upSeed =
                std::abs(forward.y) < 0.92
                    ? glm::dvec3(0.0, 1.0, 0.0)
                    : glm::dvec3(0.0, 0.0, 1.0);
            upSeed -= forward * glm::dot(upSeed, forward);
        }

        upSeed = glm::normalize(upSeed);
        right = glm::cross(forward, upSeed);
    }

    right = glm::normalize(right);
    glm::dvec3 up =
        glm::normalize(glm::cross(right, forward));

    if (glm::dot(up, previous.up) < 0.0)
    {
        right = -right;
        up = -up;
    }

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

Basis basisFromQuaternion(const glm::dquat& q)
{
    const glm::dmat3 m = glm::mat3_cast(glm::normalize(q));
    Basis basis;
    basis.right = glm::normalize(glm::dvec3(m[0]));
    basis.up = glm::normalize(glm::dvec3(m[1]));
    basis.forward = glm::normalize(-glm::dvec3(m[2]));
    return basis;
}

double quaternionAngle(glm::dquat a, glm::dquat b)
{
    if (glm::dot(a, b) < 0.0)
        b = -b;
    const glm::dquat delta =
        glm::normalize(b * glm::inverse(a));
    return 2.0 * std::acos(
        std::clamp(std::abs(delta.w), 0.0, 1.0)
    );
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

    glm::dquat delta =
        glm::normalize(b * glm::inverse(a));
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

struct ReferenceAttitude
{
    Basis basis {};
    glm::dvec3 angularVelocity {0.0};
    glm::dvec3 angularAcceleration {0.0};
};

std::vector<ReferenceAttitude> buildReferenceAttitudes(
    const world::navigation::Trajectory& trajectory,
    const Scenario& scenario,
    Law law,
    const ShipParams& params
)
{
    std::vector<ReferenceAttitude> out(
        trajectory.samples.size()
    );
    if (trajectory.samples.empty())
        return out;

    Basis previous = scenario.startBasis;
    glm::dquat previousQ = quaternionForBasis(previous);
    glm::dvec3 previousOmega(0.0);

    for (std::size_t i = 0; i < trajectory.samples.size(); ++i)
    {
        const auto& sample = trajectory.samples[i];

        glm::dvec3 requestedForward = previous.forward;
        const double speed = glm::length(sample.velocityMps);
        const double acceleration = glm::length(sample.accelerationMps2);

        if (law == Law::Newtonian && acceleration > 0.35)
        {
            // Newtonian autopilot points the main thrust axis along the
            // requested acceleration. This is intentionally different from
            // Assisted velocity alignment.
            requestedForward =
                glm::normalize(sample.accelerationMps2);
        }
        else if (speed > 0.25)
        {
            requestedForward =
                glm::normalize(sample.velocityMps);
        }

        Basis desired =
            transportedBasisForForward(
                requestedForward,
                previous
            );

        // Final orientation is a Stage-2 execution requirement. Blend toward
        // it before the last sample so the follower receives a physically
        // trackable attitude program rather than an instantaneous terminal
        // snap.
        if (
            scenario.finish.requireForward ||
            scenario.finish.requireUp)
        {
            constexpr double kTerminalBlendMeters = 35.0;
            const double remaining =
                glm::length(
                    scenario.finish.position -
                    sample.positionMeters
                );
            const double u =
                std::clamp(
                    1.0 - remaining / kTerminalBlendMeters,
                    0.0,
                    1.0
                );
            const double blend =
                u * u * (3.0 - 2.0 * u);

            const glm::dvec3 terminalForward =
                scenario.finish.requireForward
                    ? normalizedOr(
                        scenario.finish.forward,
                        desired.forward
                      )
                    : desired.forward;
            const glm::dvec3 terminalUp =
                scenario.finish.requireUp
                    ? normalizedOr(
                        scenario.finish.up,
                        desired.up
                      )
                    : desired.up;

            const Basis terminalBasis =
                basisFromForwardUp(
                    terminalForward,
                    terminalUp
                );

            glm::dquat motionQ =
                quaternionForBasis(desired);
            glm::dquat terminalQ =
                quaternionForBasis(terminalBasis);
            if (glm::dot(motionQ, terminalQ) < 0.0)
                terminalQ = -terminalQ;

            desired =
                basisFromQuaternion(
                    glm::normalize(
                        glm::slerp(
                            motionQ,
                            terminalQ,
                            blend
                        )
                    )
                );
        }

        glm::dquat desiredQ = quaternionForBasis(desired);
        if (glm::dot(previousQ, desiredQ) < 0.0)
            desiredQ = -desiredQ;

        double dt = 0.0;
        if (i > 0)
        {
            dt =
                sample.timeOffsetSeconds -
                trajectory.samples[i - 1].timeOffsetSeconds;
        }

        glm::dquat currentQ = desiredQ;
        if (i == 0)
        {
            currentQ = previousQ;
        }
        else if (dt > 1.0e-9)
        {
            const double maxRate = std::max({
                0.1,
                static_cast<double>(params.maxPitchRate),
                static_cast<double>(params.maxYawRate),
                static_cast<double>(params.maxRollRate)
            });
            const double angle =
                quaternionAngle(previousQ, desiredQ);
            const double maxAngle = maxRate * dt;
            const double alpha =
                angle <= 1.0e-9
                    ? 1.0
                    : std::clamp(maxAngle / angle, 0.0, 1.0);
            currentQ =
                glm::normalize(
                    glm::slerp(previousQ, desiredQ, alpha)
                );
        }

        ReferenceAttitude reference;
        reference.basis = basisFromQuaternion(currentQ);
        reference.angularVelocity =
            i == 0
                ? glm::dvec3(0.0)
                : angularVelocityBetween(previousQ, currentQ, dt);
        reference.angularAcceleration =
            i == 0 || dt <= 1.0e-9
                ? glm::dvec3(0.0)
                : (
                    reference.angularVelocity - previousOmega
                  ) / dt;

        out[i] = reference;
        previous = reference.basis;
        previousQ = currentQ;
        previousOmega = reference.angularVelocity;
    }

    return out;
}

world::navigation::NavigationVehicleProfile executionVehicleProfile(
    const Scenario& scenario,
    const ScenarioRunSettings& settings,
    const ShipParams& params
)
{
    world::navigation::NavigationVehicleProfile profile;
    profile.collisionRadiusMeters =
        std::max(0.0, scenario.routeEnvelopeRadiusMeters);
    profile.preferredClearanceMeters =
        std::max(0.0, scenario.routeClearanceMeters);
    profile.maxSpeedMps =
        settings.flightStyle == FlightStyle::Extreme
            ? scenario.extremeSpeedMps
            : scenario.standardSpeedMps;

    const double mainAcceleration =
        std::max(
            0.1,
            static_cast<double>(params.maxLinearGs) *
                kStandardGravity
        );

    profile.maxForwardAccelerationMps2 =
        mainAcceleration;
    profile.maxBrakingAccelerationMps2 =
        mainAcceleration;

    // Translation sideways relative to the hull is physically limited by the
    // real manoeuvre thrusters, not the old 20 m/s2 planning placeholder.
    profile.maxLateralAccelerationMps2 =
        std::max(
            0.1,
            static_cast<double>(params.manoeuvreThrusterAccel)
        );

    profile.maxAngularVelocityRadPerSecond =
        std::max({
            static_cast<double>(params.maxPitchRate),
            static_cast<double>(params.maxYawRate),
            static_cast<double>(params.maxRollRate)
        });
    profile.maxAngularAccelerationRadPerSecond2 =
        std::max(0.1, static_cast<double>(params.angularAccel));
    return profile;
}

world::navigation::TrajectoryGenerationResult buildExecutionTrajectory(
    const Scenario& scenario,
    const ScenarioRunSettings& settings,
    const TraceDocument& calculatedRoute,
    const ShipParams& params
)
{
    world::navigation::TrajectoryGenerationRequest request;
    request.systemId = 1;
    request.frameId = "navigation-runtime-stage2";
    request.startUniverseTimeSeconds = 0.0;
    request.universeTimeScale = 1.0;
    request.pathPointsMeters = calculatedRoute.routePoints;
    request.obstacles = scenario.staticObstacles;
    request.vehicle =
        executionVehicleProfile(
            scenario,
            settings,
            params
        );
    request.initialVelocityMps = scenario.startVelocity;
    request.initialAccelerationMps2 = scenario.startAcceleration;

    if (!request.pathPointsMeters.empty())
    {
        double progress = 0.0;
        for (std::size_t i = 1;
             i < request.pathPointsMeters.size();
             ++i)
        {
            progress += glm::length(
                request.pathPointsMeters[i] -
                request.pathPointsMeters[i - 1]
            );
        }

        world::navigation::TrajectoryPointSpeedConstraint finish;
        finish.sourcePathProgressMeters = progress;
        finish.maxSpeedMps =
            std::max(0.0, scenario.finish.speedMps);
        request.pointSpeedConstraints.push_back(finish);
    }

    request.hasTerminalOrientation =
        scenario.finish.requireForward ||
        scenario.finish.requireUp;
    request.terminalForward = scenario.finish.forward;
    request.terminalUp = scenario.finish.up;
    request.terminalOrientationBlendDistanceMeters = 35.0;

    return world::navigation::TrajectoryGenerator::generate(request);
}

Program makeProgramPhase(
    const world::navigation::Trajectory& trajectory,
    const std::vector<ReferenceAttitude>& attitudes,
    std::size_t first,
    std::size_t last,
    std::uint64_t revision,
    const Scenario& scenario,
    const ShipParams& params
)
{
    Program program;
    program.valid = true;
    program.revision = revision;
    program.objectiveRevision = scenario.goalRevision;
    program.family = Program::ManeuverFamily::FreeTransit;

    // Accepted time is assigned when the phase actually becomes active.
    // The template stores only phase-relative reference time.
    program.acceptedAtUniverseTimeSeconds = 0.0;

    const std::size_t available = last - first + 1;
    const std::size_t count =
        std::min<std::size_t>(
            Program::kMaxSamples,
            available
        );

    if (count < 2)
        return {};

    program.sampleCount =
        static_cast<std::uint8_t>(count);

    const double sourceStartTime =
        trajectory.samples[first].timeOffsetSeconds;

    std::size_t previousSource = first;
    for (std::size_t i = 0; i < count; ++i)
    {
        std::size_t sourceIndex = first;
        if (i + 1 == count)
        {
            sourceIndex = last;
        }
        else if (i > 0)
        {
            const double u =
                static_cast<double>(i) /
                static_cast<double>(count - 1);
            sourceIndex =
                first +
                static_cast<std::size_t>(
                    std::llround(
                        u * static_cast<double>(last - first)
                    )
                );

            sourceIndex =
                std::max(sourceIndex, previousSource + 1);
            const std::size_t remainingSlots =
                count - 1 - i;
            sourceIndex =
                std::min(
                    sourceIndex,
                    last - remainingSlots
                );
        }

        previousSource = sourceIndex;

        const auto& source = trajectory.samples[sourceIndex];
        const auto& attitude = attitudes[sourceIndex];
        auto& target = program.samples[i];

        target.timeOffsetSeconds =
            source.timeOffsetSeconds - sourceStartTime;
        target.positionMapMeters = source.positionMeters;
        target.velocityMapMetersPerSecond = source.velocityMps;
        target.linearAccelerationFeedForwardMapMps2 =
            source.accelerationMps2;
        target.forwardMap = attitude.basis.forward;
        target.rightMap = attitude.basis.right;
        target.upMap = attitude.basis.up;
        target.angularVelocityMapRadPerSecond =
            attitude.angularVelocity;
        target.angularAccelerationFeedForwardMapRadPerSec2 =
            attitude.angularAcceleration;
    }

    const double duration =
        program.samples[count - 1].timeOffsetSeconds;
    if (!(duration > 0.0))
        return {};

    program.validUntilUniverseTimeSeconds =
        duration + 5.0;

    program.terminalTolerance.positionMeters = 4.0;
    program.terminalTolerance.linearVelocityMps = 2.0;
    program.terminalTolerance.forwardAngleRad = 0.20;
    program.terminalTolerance.angularVelocityRadPerSec = 0.50;

    program.tracking.positionErrorMeters = 18.0;
    program.tracking.linearVelocityErrorMps = 8.0;
    program.tracking.forwardAngleErrorRad = 0.75;
    program.tracking.angularVelocityErrorRadPerSec = 1.2;
    program.tracking.linearFeedbackReserveMps2 = 1.5;
    program.tracking.angularFeedbackReserveRadPerSec2 = 0.8;

    const double mainAcceleration =
        static_cast<double>(params.maxLinearGs) *
        kStandardGravity;

    program.capability.revision = scenario.staticWorldRevision;
    program.capability.maxForwardAccelerationMetersPerSec2 =
        mainAcceleration;
    program.capability.maxReverseAccelerationMetersPerSec2 =
        mainAcceleration;
    program.capability.maxLateralAccelerationMetersPerSec2 =
        static_cast<double>(params.manoeuvreThrusterAccel);
    program.capability.maxVerticalAccelerationMetersPerSec2 =
        static_cast<double>(params.manoeuvreThrusterAccel);
    program.capability.maxAngularAccelerationRadPerSec2 =
        static_cast<double>(params.angularAccel);
    program.capability.maxAngularSpeedRadPerSec =
        std::max({
            static_cast<double>(params.maxPitchRate),
            static_cast<double>(params.maxYawRate),
            static_cast<double>(params.maxRollRate)
        });

    program.proof.mapRevision = scenario.staticWorldRevision;
    program.proof.mapSourceRevision = scenario.staticWorldRevision;
    program.proof.minimumClearanceMeters =
        std::max(0.0, scenario.routeClearanceMeters);

    program.completionTriggersReplan = false;
    return program;
}

std::vector<double> routeProgressTable(
    const std::vector<glm::dvec3>& route
)
{
    std::vector<double> progress(route.size(), 0.0);
    for (std::size_t i = 1; i < route.size(); ++i)
    {
        progress[i] =
            progress[i - 1] +
            glm::length(route[i] - route[i - 1]);
    }
    return progress;
}

std::size_t sampleNearestSourceProgress(
    const world::navigation::Trajectory& trajectory,
    double targetProgress,
    std::size_t beginIndex
)
{
    std::size_t best = beginIndex;
    double bestError =
        std::abs(
            trajectory.samples[beginIndex].
                sourcePathProgressMeters -
            targetProgress
        );

    for (std::size_t i = beginIndex + 1;
         i < trajectory.samples.size();
         ++i)
    {
        const double error =
            std::abs(
                trajectory.samples[i].
                    sourcePathProgressMeters -
                targetProgress
            );

        if (error <= bestError)
        {
            best = i;
            bestError = error;
            continue;
        }

        if (
            trajectory.samples[i].sourcePathProgressMeters >
            targetProgress)
        {
            break;
        }
    }

    return best;
}

std::vector<Program> buildRoutePrograms(
    const world::navigation::Trajectory& trajectory,
    const std::vector<ReferenceAttitude>& attitudes,
    const std::vector<glm::dvec3>& retainedRoute,
    const Scenario& scenario,
    const ShipParams& params
)
{
    std::vector<Program> programs;
    if (
        trajectory.samples.size() < 2 ||
        attitudes.size() != trajectory.samples.size() ||
        retainedRoute.size() < 2)
    {
        return programs;
    }

    const auto routeProgress =
        routeProgressTable(retainedRoute);

    std::size_t first = 0;
    std::uint64_t revision = 1000;

    for (std::size_t leg = 0;
         leg + 1 < retainedRoute.size();
         ++leg)
    {
        std::size_t last =
            sampleNearestSourceProgress(
                trajectory,
                routeProgress[leg + 1],
                first
            );

        if (leg + 2 == retainedRoute.size())
            last = trajectory.samples.size() - 1;

        if (last <= first)
            continue;

        Program phase =
            makeProgramPhase(
                trajectory,
                attitudes,
                first,
                last,
                revision++,
                scenario,
                params
            );

        if (!phase.valid || phase.sampleCount < 2)
            return {};

        programs.push_back(std::move(phase));
        first = last;
    }

    if (!programs.empty())
        programs.back().completionTriggersReplan = true;

    return programs;
}

void activateProgramPhase(
    Program& program,
    double actualStartTimeSeconds
)
{
    const std::size_t lastIndex =
        static_cast<std::size_t>(program.sampleCount - 1);
    const double duration =
        program.samples[lastIndex].timeOffsetSeconds;

    program.acceptedAtUniverseTimeSeconds =
        actualStartTimeSeconds;
    program.validUntilUniverseTimeSeconds =
        actualStartTimeSeconds + duration + 5.0;
}

struct ExecutionVehicle
{
    ShipTransform transform {};
    ShipParams params {};
    WorldParams world {};
    game::navigation::KinematicFrame frame {};
    Bridge bridge;
    double timeSeconds = 0.0;

    ExecutionVehicle(
        const Scenario& scenario,
        const ScenarioRunSettings& settings
    )
        : params(cobraParams()),
          bridge(pilotProfile(settings.pilot))
    {
        frame.systemId = 1;
        frame.frameId = "navigation-runtime-stage2";
        frame.originMeters = {0.0, 0.0, 0.0};
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;

        transform.motion.mode =
            game::navigation::MotionMode::HubTactical;
        transform.motion.systemId = 1;
        transform.motion.travelFrame = frame;
        transform.motion.localControlLaw =
            controlLaw(settings.controlMode);
        transform.motion.localPositionMeters =
            scenario.startPosition;
        transform.motion.localVelocityMps =
            scenario.startVelocity;
        transform.setWorldPositionMeters(
            scenario.startPosition
        );
        setTransformBasis(
            transform,
            scenario.startBasis
        );
        transform.pitchRate =
            static_cast<float>(scenario.startPitchRateRadPerSec);
        transform.yawRate =
            static_cast<float>(scenario.startYawRateRadPerSec);
        transform.rollRate =
            static_cast<float>(scenario.startRollRateRadPerSec);

        Bridge::Intent initial;
        // Reset on neutral revision zero so the first real route intent
        // (goalRevision) exercises the selected pilot's reaction-delay model.
        initial.revision = 0;
        initial.targetRevision = 0;
        if (!bridge.reset(0.0, initial))
            throw std::runtime_error(
                "Stage 2 pilot bridge reset failed"
            );
    }
};

Follower::AgentState followerAgent(
    const ExecutionVehicle& vehicle
)
{
    Follower::AgentState agent;
    agent.positionMapMeters =
        vehicle.transform.motion.localPositionMeters;
    agent.velocityMapMetersPerSecond =
        vehicle.transform.motion.localVelocityMps;
    agent.forwardMap =
        glm::dvec3(vehicle.transform.forward());
    agent.rightMap =
        glm::dvec3(vehicle.transform.right());
    agent.upMap =
        glm::dvec3(vehicle.transform.up());
    agent.pitchRateRadPerSec =
        vehicle.transform.pitchRate;
    agent.yawRateRadPerSec =
        vehicle.transform.yawRate;
    agent.rollRateRadPerSec =
        vehicle.transform.rollRate;
    return agent;
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

double distancePointToSegment(
    const glm::dvec3& point,
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    const glm::dvec3 ab = b - a;
    const double lengthSquared = glm::dot(ab, ab);
    if (lengthSquared <= 1.0e-12)
        return glm::length(point - a);

    const double t =
        std::clamp(
            glm::dot(point - a, ab) / lengthSquared,
            0.0,
            1.0
        );
    return glm::length(point - (a + ab * t));
}

double distancePointToPolyline(
    const glm::dvec3& point,
    const std::vector<glm::dvec3>& route
)
{
    if (route.empty())
        return 0.0;
    if (route.size() == 1)
        return glm::length(point - route.front());

    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 1; i < route.size(); ++i)
    {
        best = std::min(
            best,
            distancePointToSegment(
                point,
                route[i - 1],
                route[i]
            )
        );
    }
    return std::isfinite(best) ? best : 0.0;
}

TraceFrame executionTraceFrame(
    const ExecutionVehicle& vehicle,
    const Program& program,
    const Scenario& scenario,
    const std::string& status
)
{
    TraceFrame frame;
    frame.timeSeconds = vehicle.timeSeconds;
    frame.shipPosition =
        vehicle.transform.motion.localPositionMeters;
    frame.shipVelocity =
        vehicle.transform.motion.localVelocityMps;
    frame.shipForward =
        glm::dvec3(vehicle.transform.forward());
    frame.shipRight =
        glm::dvec3(vehicle.transform.right());
    frame.shipUp =
        glm::dvec3(vehicle.transform.up());
    frame.phase = "route_execution";
    frame.plannerStatus = status;
    frame.hasSelectedTarget = true;
    frame.selectedTarget = scenario.finish.position;

    const auto sampled =
        game::navigation::ManeuverProgramSampler::sample(
            program,
            vehicle.timeSeconds
        );

    if (sampled.status !=
        game::navigation::ManeuverProgramSampler::Status::InvalidInput)
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
            program.tracking.positionErrorMeters;
    }

    return frame;
}

void writeExecutionDiagnostics(
    const std::string& scenarioJsonPath,
    const std::vector<std::string>& diagnostics
)
{
    const std::filesystem::path scenarioPath(scenarioJsonPath);
    const std::filesystem::path output =
        scenarioPath.parent_path() / "last_execution.log";

    std::ofstream stream(output);
    if (!stream)
        throw std::runtime_error(
            "cannot write execution diagnostics: " +
            output.string()
        );

    for (const auto& line : diagnostics)
    {
        stream << line << "\n";
        std::cout << "[NAV-STAGE2] " << line << "\n";
    }
}

} // namespace

ScenarioRunResult loadScenarioPreview(
    const std::string& scenarioJsonPath
)
{
    ScenarioRunResult out;

    try
    {
        const Scenario scenario = loadScenario(scenarioJsonPath);

        TraceDocument trace;
        trace.version = 2;
        trace.law = "newtonian";
        trace.shipHalfExtentsMeters = kBodyHalfExtents;
        setSceneEndpoints(trace, scenario);

        for (const auto& obstacle : scenario.staticObstacles)
            trace.staticObstacles.push_back(traceObstacle(obstacle));

        TraceFrame frame = routeFrame(scenario, true);
        frame.phase = "scene_preview";
        frame.plannerStatus = "scene_loaded";
        frame.hasSelectedTarget = true;
        frame.selectedTarget = scenario.finish.position;
        trace.frames.push_back(std::move(frame));

        out.trace = std::move(trace);
        out.success = true;
        out.message = "СЦЕНА ЗАГРУЖЕНА — МАРШРУТ ЕЩЁ НЕ РАССЧИТАН";
        out.diagnostics = previewDiagnostics(scenario);
    }
    catch (const std::exception& e)
    {
        out.success = false;
        out.message = e.what();
    }

    return out;
}

ScenarioRunResult calculateScenario(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings
)
{
    ScenarioRunResult out;

    try
    {
        const Scenario scenario = loadScenario(scenarioJsonPath);

        // Stage 1 consumes only static route facts. Control law, pilot skill,
        // doctrine and dynamic actors are intentionally Stage-2 inputs.
        (void)settings.pilot;
        (void)settings.flightStyle;
        (void)settings.enableSuddenObstacle;
        (void)scenario.dynamicWorldRevision;
        (void)scenario.dynamicObstacles;
        (void)scenario.suddenObstacle;
        (void)scenario.hasSuddenObstacle;
        (void)scenario.standardSpeedMps;
        (void)scenario.extremeSpeedMps;
        (void)scenario.finish.requireForward;
        (void)scenario.finish.requireUp;
        (void)scenario.finish.forward;
        (void)scenario.finish.up;
        (void)scenario.finish.speedMps;

        game::navigation::NominalRoutePlanner::Request request;
        request.goalRevision = scenario.goalRevision;
        request.staticWorldRevision =
            scenario.staticWorldRevision;
        request.startMapMeters = scenario.startPosition;
        request.goalMapMeters = scenario.finish.position;
        request.requiredWaypointsMapMeters =
            scenario.shipRoutePoints;
        request.staticObstacles = scenario.staticObstacles;
        request.navigationEnvelopeRadiusMeters =
            std::max(0.0, scenario.routeEnvelopeRadiusMeters);
        request.additionalRouteClearanceMeters =
            std::max(0.0, scenario.routeClearanceMeters);

        const auto route =
            game::navigation::NominalRoutePlanner::plan(request);

        TraceDocument trace;
        trace.version = 2;
        trace.law =
            settings.controlMode == ControlMode::Newtonian
                ? "newtonian"
                : "assisted";
        trace.shipHalfExtentsMeters = kBodyHalfExtents;
        setSceneEndpoints(trace, scenario);

        for (const auto& obstacle : scenario.staticObstacles)
            trace.staticObstacles.push_back(traceObstacle(obstacle));

        if (route.valid)
        {
            trace.routePoints = route.pointsMapMeters;

            if (trace.routePoints.size() > 2)
            {
                trace.turnPoints.assign(
                    trace.routePoints.begin() + 1,
                    trace.routePoints.end() - 1
                );
            }
        }

        trace.frames.push_back(routeFrame(scenario, route.valid));

        out.diagnostics = routeDiagnostics(scenario, route);
        writeRouteDiagnostics(
            scenarioJsonPath,
            out.diagnostics
        );

        out.trace = std::move(trace);
        out.success = route.valid;
        out.message =
            route.valid
                ? "ЭТАП 1: СТАТИЧЕСКИЙ МАРШРУТ ПОСТРОЕН"
                : "ЭТАП 1: МАРШРУТ НЕ ПОСТРОЕН — " +
                    route.message;
    }
    catch (const std::exception& e)
    {
        out.success = false;
        out.message = e.what();
    }

    return out;
}


ScenarioRunResult executeCalculatedRoute(
    const std::string& scenarioJsonPath,
    const ScenarioRunSettings& settings,
    const TraceDocument& calculatedRoute
)
{
    ScenarioRunResult out;

    try
    {
        const Scenario scenario = loadScenario(scenarioJsonPath);

        TraceDocument trace = calculatedRoute;
        trace.frames.clear();
        trace.law =
            settings.controlMode == ControlMode::Newtonian
                ? "newtonian"
                : "assisted";

        if (calculatedRoute.routePoints.size() < 2)
        {
            out.trace = std::move(trace);
            out.success = false;
            out.message =
                "ЭТАП 2: НЕТ РАССЧИТАННОГО МАРШРУТА";
            out.diagnostics = {
                "SCENE: LOADED",
                "PLANNER: NO CACHED ROUTE",
                "TRAJECTORY: NOT RUN",
                "FOLLOWER: NOT RUN"
            };
            writeExecutionDiagnostics(
                scenarioJsonPath,
                out.diagnostics
            );
            return out;
        }

        if (scenario.finish.speedMps > 1.0e-6)
        {
            out.trace = std::move(trace);
            out.success = false;
            out.message =
                "ЭТАП 2: НЕНУЛЕВАЯ ФИНАЛЬНАЯ СКОРОСТЬ "
                "ЕЩЁ НЕ ПОДДЕРЖАНА RUCKIG ROUTE BACKEND";
            out.diagnostics = {
                "SCENE: LOADED",
                "PLANNER: CACHED ROUTE OK",
                "TRAJECTORY: FAIL",
                "FOLLOWER: NOT RUN",
                "FINAL SPEED CONTRACT: UNSUPPORTED"
            };
            writeExecutionDiagnostics(
                scenarioJsonPath,
                out.diagnostics
            );
            return out;
        }

        const ShipParams params = cobraParams();
        const auto trajectoryResult =
            buildExecutionTrajectory(
                scenario,
                settings,
                calculatedRoute,
                params
            );

        if (!trajectoryResult.ready())
        {
            TraceFrame failed = routeFrame(scenario, true);
            failed.phase = "execution_failed";
            failed.plannerStatus = "trajectory_failed";
            trace.frames.push_back(std::move(failed));

            out.trace = std::move(trace);
            out.success = false;
            out.message =
                "ЭТАП 2: RUCKIG НЕ ПОСТРОИЛ ТРАЕКТОРИЮ";

            out.diagnostics = {
                "SCENE: LOADED",
                "PLANNER: CACHED ROUTE OK",
                "TRAJECTORY: FAIL",
                "TRAJECTORY MESSAGE: " +
                    trajectoryResult.trajectory.message,
                "FOLLOWER: NOT RUN",
                "LOG: last_execution.log"
            };
            writeExecutionDiagnostics(
                scenarioJsonPath,
                out.diagnostics
            );
            return out;
        }

        std::vector<double> retainedWaypointSpeedsMps;
        if (calculatedRoute.routePoints.size() > 2)
        {
            const auto retainedProgress =
                routeProgressTable(calculatedRoute.routePoints);
            for (std::size_t i = 1;
                 i + 1 < calculatedRoute.routePoints.size();
                 ++i)
            {
                const std::size_t sample =
                    sampleNearestSourceProgress(
                        trajectoryResult.trajectory,
                        retainedProgress[i],
                        0
                    );
                retainedWaypointSpeedsMps.push_back(
                    trajectoryResult.trajectory.samples[sample].speedMps
                );
            }
        }

        const auto attitudes =
            buildReferenceAttitudes(
                trajectoryResult.trajectory,
                scenario,
                controlLaw(settings.controlMode),
                params
            );

        auto programs =
            buildRoutePrograms(
                trajectoryResult.trajectory,
                attitudes,
                calculatedRoute.routePoints,
                scenario,
                params
            );

        if (programs.empty())
        {
            out.trace = std::move(trace);
            out.success = false;
            out.message =
                "ЭТАП 2: НЕ СОЗДАНО НИ ОДНОЙ ПРОГРАММЫ FOLLOWER";
            out.diagnostics = {
                "SCENE: LOADED",
                "PLANNER: CACHED ROUTE OK",
                "TRAJECTORY: OK",
                "FOLLOWER PROGRAMS: 0",
                "FOLLOWER: NOT RUN",
                "LOG: last_execution.log"
            };
            writeExecutionDiagnostics(
                scenarioJsonPath,
                out.diagnostics
            );
            return out;
        }

        ExecutionVehicle vehicle(scenario, settings);

        double plannedExecutionSeconds = 0.0;
        for (const auto& phase : programs)
        {
            const std::size_t lastIndex =
                static_cast<std::size_t>(phase.sampleCount - 1);
            plannedExecutionSeconds +=
                phase.samples[lastIndex].timeOffsetSeconds;
        }

        const double maximumEnd =
            plannedExecutionSeconds + 12.0;

        std::size_t activeProgram = 0;
        std::size_t phaseHandoffs = 0;
        double nextTraceTime = 0.0;
        bool followerInvalid = false;
        bool bridgeInvalid = false;
        bool coarseStaticContact = false;
        bool phaseCaptureTimedOut = false;
        bool routeExecutionComplete = false;
        std::string followerFailureReason = "NONE";
        std::size_t followerFailureProgramIndex = 0;
        double followerFailureTimeSeconds = 0.0;
        double followerFailureAcceptedAtSeconds = 0.0;
        double maximumCrossTrack = 0.0;
        double maximumFollowerPositionError = 0.0;
        double maximumBodyVelocityAngleRad = 0.0;

        activateProgramPhase(
            programs.front(),
            vehicle.timeSeconds
        );

        trace.frames.push_back(
            executionTraceFrame(
                vehicle,
                programs.front(),
                scenario,
                "follower_running"
            )
        );

        glm::dvec3 previousPosition =
            vehicle.transform.motion.localPositionMeters;

        while (vehicle.timeSeconds < maximumEnd - 1.0e-9)
        {
            Program& program = programs[activeProgram];

            const auto preSample =
                game::navigation::ManeuverProgramSampler::sample(
                    program,
                    vehicle.timeSeconds
                );

            if (
                preSample.status ==
                    game::navigation::ManeuverProgramSampler::Status::InvalidInput ||
                preSample.status ==
                    game::navigation::ManeuverProgramSampler::Status::BeforeStart)
            {
                followerInvalid = true;
                followerFailureReason =
                    preSample.status ==
                        game::navigation::ManeuverProgramSampler::Status::BeforeStart
                        ? "PROGRAM_BEFORE_START"
                        : "PROGRAM_INVALID";
                followerFailureProgramIndex = activeProgram;
                followerFailureTimeSeconds = vehicle.timeSeconds;
                followerFailureAcceptedAtSeconds =
                    program.acceptedAtUniverseTimeSeconds;
                break;
            }

            const auto follower =
                Follower::follow(
                    program,
                    vehicle.timeSeconds,
                    followerAgent(vehicle)
                );

            if (follower.status == Follower::Status::InvalidInput)
            {
                followerInvalid = true;
                followerFailureReason = "FOLLOWER_OR_TRACKER_INVALID";
                followerFailureProgramIndex = activeProgram;
                followerFailureTimeSeconds = vehicle.timeSeconds;
                followerFailureAcceptedAtSeconds =
                    program.acceptedAtUniverseTimeSeconds;
                break;
            }

            maximumFollowerPositionError =
                std::max(
                    maximumFollowerPositionError,
                    follower.crossTrackErrorMeters
                );

            game::navigation::ManeuverPhaseGate::Policy gatePolicy;
            gatePolicy.mode =
                activeProgram + 1 < programs.size()
                    ? game::navigation::ManeuverPhaseGate::Mode::ScheduledMoving
                    : game::navigation::ManeuverPhaseGate::Mode::StateCapture;
            gatePolicy.maximumCaptureOverrunSeconds = 6.0;

            const auto gate =
                game::navigation::ManeuverPhaseGate::evaluate(
                    program,
                    vehicle.timeSeconds,
                    follower.status,
                    gatePolicy
                );

            if (
                gate.status ==
                game::navigation::ManeuverPhaseGate::Status::InvalidInput)
            {
                followerInvalid = true;
                followerFailureReason = "PHASE_GATE_INVALID";
                followerFailureProgramIndex = activeProgram;
                followerFailureTimeSeconds = vehicle.timeSeconds;
                followerFailureAcceptedAtSeconds =
                    program.acceptedAtUniverseTimeSeconds;
                break;
            }

            if (
                gate.status ==
                game::navigation::ManeuverPhaseGate::Status::CaptureTimedOut)
            {
                phaseCaptureTimedOut = true;
                followerFailureReason = "FINAL_CAPTURE_TIMEOUT";
                followerFailureProgramIndex = activeProgram;
                followerFailureTimeSeconds = vehicle.timeSeconds;
                followerFailureAcceptedAtSeconds =
                    program.acceptedAtUniverseTimeSeconds;
                break;
            }

            if (
                gate.status ==
                game::navigation::ManeuverPhaseGate::Status::Advance)
            {
                if (activeProgram + 1 < programs.size())
                {
                    ++activeProgram;
                    ++phaseHandoffs;
                    activateProgramPhase(
                        programs[activeProgram],
                        vehicle.timeSeconds
                    );

                    trace.frames.push_back(
                        executionTraceFrame(
                            vehicle,
                            programs[activeProgram],
                            scenario,
                            "phase_handoff"
                        )
                    );
                    continue;
                }

                routeExecutionComplete = true;
                break;
            }

            const auto bridgeResult =
                vehicle.bridge.step(
                    vehicle.timeSeconds + kExecutionDt,
                    kExecutionDt,
                    toSystemIntent(follower.intent)
                );

            if (bridgeResult.status !=
                Bridge::PilotExecutor::Status::Ok)
            {
                bridgeInvalid = true;
                break;
            }

            SharedShipPhysics::integrate(
                vehicle.transform,
                vehicle.params,
                bridgeResult.control,
                vehicle.world,
                static_cast<float>(kExecutionDt)
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
                    kExecutionDt
                );

            vehicle.transform.syncLegacyPositionFromWorld();
            vehicle.timeSeconds += kExecutionDt;

            const glm::dvec3 currentPosition =
                vehicle.transform.motion.localPositionMeters;

            const glm::dvec3 currentVelocity =
                vehicle.transform.motion.localVelocityMps;
            const double currentSpeed = glm::length(currentVelocity);
            if (currentSpeed > 0.25)
            {
                const glm::dvec3 velocityDirection =
                    currentVelocity / currentSpeed;
                const glm::dvec3 bodyForward =
                    normalizedOr(
                        glm::dvec3(vehicle.transform.forward()),
                        velocityDirection
                    );
                maximumBodyVelocityAngleRad =
                    std::max(
                        maximumBodyVelocityAngleRad,
                        std::acos(
                            std::clamp(
                                glm::dot(
                                    velocityDirection,
                                    bodyForward
                                ),
                                -1.0,
                                1.0
                            )
                        )
                    );
            }

            maximumCrossTrack =
                std::max(
                    maximumCrossTrack,
                    distancePointToPolyline(
                        currentPosition,
                        calculatedRoute.routePoints
                    )
                );

            if (!world::navigation::
                    segmentClearOfNavigationObstacles(
                        previousPosition,
                        currentPosition,
                        scenario.staticObstacles,
                        std::max(
                            0.0,
                            scenario.routeEnvelopeRadiusMeters
                        ),
                        std::max(
                            0.0,
                            scenario.routeClearanceMeters
                        )
                    ))
            {
                coarseStaticContact = true;
            }

            previousPosition = currentPosition;

            if (vehicle.timeSeconds + 1.0e-9 >= nextTraceTime)
            {
                trace.frames.push_back(
                    executionTraceFrame(
                        vehicle,
                        program,
                        scenario,
                        "follower_running"
                    )
                );
                nextTraceTime += kTraceSampleSeconds;
            }
        }

        const glm::dvec3 finalPosition =
            vehicle.transform.motion.localPositionMeters;
        const double finalPositionError =
            glm::length(
                finalPosition - scenario.finish.position
            );
        const double finalSpeed =
            glm::length(
                vehicle.transform.motion.localVelocityMps
            );
        const double finalSpeedError =
            std::abs(finalSpeed - scenario.finish.speedMps);

        auto angleBetween = [](
            const glm::dvec3& a,
            const glm::dvec3& b)
        {
            const double la = glm::length(a);
            const double lb = glm::length(b);
            if (la <= 1.0e-12 || lb <= 1.0e-12)
                return 0.0;
            return std::acos(
                std::clamp(
                    glm::dot(a / la, b / lb),
                    -1.0,
                    1.0
                )
            );
        };

        const double finalForwardError =
            scenario.finish.requireForward
                ? angleBetween(
                    glm::dvec3(vehicle.transform.forward()),
                    scenario.finish.forward
                  )
                : 0.0;
        const double finalUpError =
            scenario.finish.requireUp
                ? angleBetween(
                    glm::dvec3(vehicle.transform.up()),
                    scenario.finish.up
                  )
                : 0.0;

        const bool finalStateReached =
            finalPositionError <= 5.0 &&
            finalSpeedError <= 1.5 &&
            finalForwardError <= 0.25 &&
            finalUpError <= 0.25;

        const bool success =
            !followerInvalid &&
            !bridgeInvalid &&
            !phaseCaptureTimedOut &&
            !coarseStaticContact &&
            routeExecutionComplete &&
            finalStateReached;

        if (!trace.frames.empty())
        {
            trace.frames.back().phase =
                success
                    ? "execution_complete"
                    : "execution_failed";
            trace.frames.back().plannerStatus =
                followerInvalid
                    ? "follower_invalid"
                    : bridgeInvalid
                        ? "pilot_bridge_invalid"
                        : phaseCaptureTimedOut
                            ? "capture_timeout"
                            : coarseStaticContact
                                ? "static_contact"
                                : success
                                    ? "follower_complete"
                                    : "terminal_miss";
        }

        auto number = [](double value)
        {
            std::ostringstream stream;
            stream.setf(std::ios::fixed);
            stream << std::setprecision(2) << value;
            return stream.str();
        };

        std::ostringstream waypointSpeeds;
        waypointSpeeds.setf(std::ios::fixed);
        waypointSpeeds << std::setprecision(2);
        if (retainedWaypointSpeedsMps.empty())
        {
            waypointSpeeds << "NONE";
        }
        else
        {
            for (std::size_t i = 0;
                 i < retainedWaypointSpeedsMps.size();
                 ++i)
            {
                if (i != 0)
                    waypointSpeeds << ", ";
                waypointSpeeds
                    << "P" << (i + 1) << "="
                    << retainedWaypointSpeedsMps[i];
            }
            waypointSpeeds << " M/S";
        }

        out.diagnostics = {
            "SCENE: LOADED",
            "PLANNER: CACHED ROUTE OK",
            "TRAJECTORY: RUCKIG OK",
            "TRAJECTORY SAMPLES: " +
                std::to_string(
                    trajectoryResult.trajectory.samples.size()
                ),
            "PROGRAM PHASES: " +
                std::to_string(programs.size()),
            "PHASE HANDOFFS: " +
                std::to_string(phaseHandoffs),
            std::string("ROUTE EXECUTION COMPLETE: ") +
                (routeExecutionComplete ? "YES" : "NO"),
            std::string("FOLLOWER: ") +
                (followerInvalid ? "FAIL" : "EXECUTED"),
            "FOLLOWER FAIL REASON: " + followerFailureReason,
            "FOLLOWER FAIL PROGRAM: " +
                std::to_string(followerFailureProgramIndex),
            "FOLLOWER FAIL TIME: " +
                number(followerFailureTimeSeconds) + " S",
            "FOLLOWER PROGRAM ACCEPTED AT: " +
                number(followerFailureAcceptedAtSeconds) + " S",
            std::string("PILOT BRIDGE: ") +
                (bridgeInvalid ? "FAIL" : "EXECUTED"),
            "PILOT PROFILE: " +
                std::string(pilotName(settings.pilot)),
            "FLIGHT STYLE: " +
                std::string(flightStyleName(settings.flightStyle)),
            "CONTROL LAW: " +
                std::string(
                    settings.controlMode == ControlMode::Newtonian
                        ? "NEWTONIAN"
                        : "ASSISTED"
                ),
            "DYNAMIC AVOIDANCE: NOT ENABLED IN STATIC PASS",
            "EXECUTION FRAMES: " +
                std::to_string(trace.frames.size()),
            "FINAL POSITION ERROR: " +
                number(finalPositionError) + " M",
            "FINAL SPEED: " +
                number(finalSpeed) + " M/S",
            "MAX ROUTE DEVIATION: " +
                number(maximumCrossTrack) + " M",
            "MAX FOLLOWER ERROR: " +
                number(maximumFollowerPositionError) + " M",
            "RETAINED WAYPOINT SPEEDS: " +
                waypointSpeeds.str(),
            "MAX BODY/VELOCITY ANGLE: " +
                number(
                    maximumBodyVelocityAngleRad *
                    180.0 / 3.14159265358979323846
                ) + " DEG",
            std::string("COARSE STATIC CONTACT: ") +
                (coarseStaticContact ? "YES" : "NO"),
            "LOG: last_execution.log"
        };

        writeExecutionDiagnostics(
            scenarioJsonPath,
            out.diagnostics
        );

        out.trace = std::move(trace);
        out.success = success;
        out.message =
            success
                ? "ЭТАП 2: FOLLOWER ПРОШЁЛ МАРШРУТ"
                : "ЭТАП 2: ИСПОЛНЕНИЕ МАРШРУТА ЗАВЕРШИЛОСЬ ОШИБКОЙ";
    }
    catch (const std::exception& e)
    {
        out.success = false;
        out.message = e.what();
    }

    return out;
}

} // namespace elite::tools::navigation_runtime
