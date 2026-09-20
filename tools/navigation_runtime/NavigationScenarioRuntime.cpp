#include "NavigationScenarioRuntime.h"

#include "src/game/navigation/NominalRoutePlanner.h"
#include "src/world/navigation/NavigationObstacle.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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

} // namespace

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

} // namespace elite::tools::navigation_runtime
