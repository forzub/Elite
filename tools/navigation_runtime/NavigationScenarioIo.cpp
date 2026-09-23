#include "NavigationScenarioIo.h"
#include "NavigationScenarioMath.h"

#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

namespace elite::tools::navigation_runtime
{
namespace
{

using Basis = ScenarioBasis;
using DynamicObstacleDefinition = ScenarioDynamicObstacleDefinition;
using Scenario = ScenarioDefinition;

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

ScenarioDefinition parseScenarioDefinitionFile(
    const std::string& path
)
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

    if (root.contains("world_physics"))
    {
        const auto& world = root.at("world_physics");
        scenario.worldPhysics.linearDrag =
            world.value(
                "linear_drag",
                scenario.worldPhysics.linearDrag
            );
        scenario.worldPhysics.maxSafeDecel =
            world.value(
                "max_safe_decel",
                scenario.worldPhysics.maxSafeDecel
            );
    }

    if (root.contains("reference_frame"))
    {
        const auto& frame = root.at("reference_frame");
        scenario.frame.systemId =
            frame.value("system_id", scenario.frame.systemId);
        scenario.frame.frameId =
            frame.value("frame_id", scenario.frame.frameId);
        scenario.frame.originMeters =
            readVec3(
                frame,
                "origin_m",
                scenario.frame.originMeters
            );
        scenario.frame.linearVelocityMps =
            readVec3(
                frame,
                "linear_velocity_mps",
                scenario.frame.linearVelocityMps
            );
        scenario.frame.linearAccelerationMps2 =
            readVec3(
                frame,
                "linear_acceleration_mps2",
                scenario.frame.linearAccelerationMps2
            );
        scenario.frame.angularVelocityWorldRadPerSecond =
            readVec3(
                frame,
                "angular_velocity_world_rad_s",
                scenario.frame.angularVelocityWorldRadPerSecond
            );
        scenario.frame.angularAccelerationWorldRadPerSecond2 =
            readVec3(
                frame,
                "angular_acceleration_world_rad_s2",
                scenario.frame.angularAccelerationWorldRadPerSecond2
            );

        if (frame.contains("local_x") ||
            frame.contains("local_y") ||
            frame.contains("local_z"))
        {
            const glm::dvec3 localX =
                readVec3(frame, "local_x", {1.0, 0.0, 0.0});
            const glm::dvec3 localY =
                readVec3(frame, "local_y", {0.0, 1.0, 0.0});
            const glm::dvec3 localZ =
                readVec3(frame, "local_z", {0.0, 0.0, 1.0});
            scenario.frame.localToWorldBasis =
                glm::dmat3(localX, localY, localZ);
        }

        scenario.frame.startUniverseTimeSeconds =
            frame.value(
                "start_universe_time_s",
                scenario.frame.startUniverseTimeSeconds
            );
        scenario.frame.universeTimeScale =
            frame.value(
                "universe_time_scale",
                scenario.frame.universeTimeScale
            );
    }

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

} // namespace

ScenarioDefinition loadScenarioDefinition(
    const std::string& scenarioJsonPath
)
{
    return parseScenarioDefinitionFile(scenarioJsonPath);
}

} // namespace elite::tools::navigation_runtime
