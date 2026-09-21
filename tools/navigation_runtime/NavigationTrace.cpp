#include "NavigationTrace.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace elite::tools::navigation_runtime
{
namespace
{

nlohmann::json vec3Json(const glm::dvec3& v)
{
    return nlohmann::json::array({v.x, v.y, v.z});
}

glm::dvec3 readVec3(const nlohmann::json& j)
{
    if (!j.is_array() || j.size() != 3)
        throw std::runtime_error("navigation trace vec3 must contain three values");

    return {
        j.at(0).get<double>(),
        j.at(1).get<double>(),
        j.at(2).get<double>()
    };
}

} // namespace

void saveTraceJson(
    const TraceDocument& trace,
    const std::string& path
)
{
    nlohmann::json root;
    root["version"] = trace.version;
    root["law"] = trace.law;
    root["ship_half_extents_m"] = vec3Json(trace.shipHalfExtentsMeters);
    root["has_scene_endpoints"] = trace.hasSceneEndpoints;
    if (trace.hasSceneEndpoints)
    {
        root["scene_start"] = vec3Json(trace.sceneStartMapMeters);
        root["scene_finish"] = vec3Json(trace.sceneFinishMapMeters);
    }

    root["route_points"] = nlohmann::json::array();
    for (const auto& p : trace.routePoints)
        root["route_points"].push_back(vec3Json(p));

    root["turn_points"] = nlohmann::json::array();
    for (const auto& p : trace.turnPoints)
        root["turn_points"].push_back(vec3Json(p));

    root["execution_guide_points"] = nlohmann::json::array();
    for (const auto& p : trace.executionGuidePoints)
        root["execution_guide_points"].push_back(vec3Json(p));

    root["calculated_trajectory_points"] = nlohmann::json::array();
    for (const auto& p : trace.calculatedTrajectoryPoints)
        root["calculated_trajectory_points"].push_back(vec3Json(p));

    root["static_obstacles"] = nlohmann::json::array();
    for (const TraceStaticObstacle& o : trace.staticObstacles)
    {
        nlohmann::json obstacle;
        obstacle["id"] = o.id;
        obstacle["shape"] = o.shape;
        obstacle["center"] = vec3Json(o.center);
        obstacle["half_extents"] = vec3Json(o.halfExtents);
        obstacle["radius_m"] = o.radiusMeters;
        obstacle["capsule_half_length_m"] =
            o.capsuleHalfLengthMeters;
        root["static_obstacles"].push_back(std::move(obstacle));
    }

    root["frames"] = nlohmann::json::array();
    for (const TraceFrame& f : trace.frames)
    {
        nlohmann::json frame;
        frame["time_s"] = f.timeSeconds;
        frame["ship_position"] = vec3Json(f.shipPosition);
        frame["ship_forward"] = vec3Json(f.shipForward);
        frame["ship_right"] = vec3Json(f.shipRight);
        frame["ship_up"] = vec3Json(f.shipUp);
        frame["ship_velocity"] = vec3Json(f.shipVelocity);
        frame["ship_angular_rate_pyr_rad_s"] =
            vec3Json(f.shipAngularRatePyrRadPerSec);
        frame["main_engine_acceleration_mps2"] =
            vec3Json(f.mainEngineAccelerationMps2);
        frame["manoeuvre_acceleration_mps2"] =
            vec3Json(f.manoeuvreAccelerationMps2);
        frame["engine_acceleration_mps2"] =
            vec3Json(f.engineAccelerationMps2);
        frame["ideal_linear_acceleration_demand_mps2"] =
            vec3Json(f.idealLinearAccelerationDemandMps2);
        frame["ideal_angular_acceleration_demand_rad_s2"] =
            vec3Json(f.idealAngularAccelerationDemandRadPerSec2);
        frame["executed_linear_acceleration_demand_mps2"] =
            vec3Json(f.executedLinearAccelerationDemandMps2);
        frame["executed_angular_acceleration_demand_rad_s2"] =
            vec3Json(f.executedAngularAccelerationDemandRadPerSec2);
        frame["has_runtime_control_law"] = f.hasRuntimeControlLaw;
        if (f.hasRuntimeControlLaw)
            frame["runtime_control_law"] = f.runtimeControlLaw;
        frame["main_engine_throttle_01"] =
            f.mainEngineThrottle01;

        frame["has_program_reference"] = f.hasProgramReference;
        if (f.hasProgramReference)
        {
            frame["program_reference_position"] =
                vec3Json(f.programReferencePosition);
            frame["program_reference_velocity"] =
                vec3Json(f.programReferenceVelocity);
            frame["program_speed_corridor_half_width_mps"] =
                f.programSpeedCorridorHalfWidthMps;
            frame["program_progress_corridor_half_width_m"] =
                f.programProgressCorridorHalfWidthMeters;
            frame["program_reference_forward"] =
                vec3Json(f.programReferenceForward);
            frame["program_reference_right"] =
                vec3Json(f.programReferenceRight);
            frame["program_reference_up"] =
                vec3Json(f.programReferenceUp);
            frame["program_tracking_corridor_radius_m"] =
                f.programTrackingCorridorRadiusMeters;
        }

        frame["hazard_active"] = f.hazardActive;
        frame["hazard_position"] = vec3Json(f.hazardPosition);
        frame["hazard_velocity"] = vec3Json(f.hazardVelocity);
        frame["planner_lookahead_s"] = f.plannerLookAheadSeconds;
        frame["hazard_radius_m"] = f.hazardRadiusMeters;
        frame["hazard_collision_envelope_radius_m"] =
            f.hazardCollisionEnvelopeRadiusMeters;
        frame["hazard_planner_envelope_radius_m"] =
            f.hazardPlannerEnvelopeRadiusMeters;
        frame["dynamic_clearance_m"] = f.dynamicClearanceMeters;

        frame["phase"] = f.phase;
        frame["planner_status"] = f.plannerStatus;
        frame["replan_event"] = f.replanEvent;

        if (f.hasSelectedTarget)
            frame["selected_target"] = vec3Json(f.selectedTarget);
        if (f.hasReacquisitionTarget)
            frame["reacquisition_target"] = vec3Json(f.reacquisitionTarget);
        if (f.hasPortalTarget)
            frame["portal_target"] = vec3Json(f.portalTarget);

        root["frames"].push_back(std::move(frame));
    }

    const std::filesystem::path output(path);
    if (!output.parent_path().empty())
        std::filesystem::create_directories(output.parent_path());

    std::ofstream stream(output);
    if (!stream)
        throw std::runtime_error("could not open navigation trace for writing: " + path);

    stream << root.dump(2) << "\n";
}

TraceDocument loadTraceJson(const std::string& path)
{
    std::ifstream stream(path);
    if (!stream)
        throw std::runtime_error("could not open navigation trace: " + path);

    nlohmann::json root;
    stream >> root;

    TraceDocument trace;
    trace.version = root.value("version", 1);
    trace.law = root.value("law", std::string {});
    trace.shipHalfExtentsMeters =
        readVec3(root.at("ship_half_extents_m"));

    trace.hasSceneEndpoints =
        root.value("has_scene_endpoints", false);
    if (trace.hasSceneEndpoints)
    {
        trace.sceneStartMapMeters = readVec3(root.at("scene_start"));
        trace.sceneFinishMapMeters = readVec3(root.at("scene_finish"));
    }

    for (const auto& p : root.at("route_points"))
        trace.routePoints.push_back(readVec3(p));
    for (const auto& p : root.at("turn_points"))
        trace.turnPoints.push_back(readVec3(p));

    if (root.contains("execution_guide_points"))
    {
        for (const auto& p : root.at("execution_guide_points"))
            trace.executionGuidePoints.push_back(readVec3(p));
    }

    if (root.contains("calculated_trajectory_points"))
    {
        for (const auto& p : root.at("calculated_trajectory_points"))
            trace.calculatedTrajectoryPoints.push_back(readVec3(p));
    }

    if (root.contains("static_obstacles"))
    {
        for (const auto& source : root.at("static_obstacles"))
        {
            TraceStaticObstacle o;
            o.id = source.value("id", std::string {});
            o.shape = source.value("shape", std::string("sphere"));
            o.center = readVec3(source.at("center"));
            o.halfExtents =
                source.contains("half_extents")
                    ? readVec3(source.at("half_extents"))
                    : glm::dvec3(1.0);
            o.radiusMeters = source.value("radius_m", 1.0);
            o.capsuleHalfLengthMeters =
                source.value("capsule_half_length_m", 0.0);
            trace.staticObstacles.push_back(std::move(o));
        }
    }

    for (const auto& source : root.at("frames"))
    {
        TraceFrame f;
        f.timeSeconds = source.at("time_s").get<double>();
        f.shipPosition = readVec3(source.at("ship_position"));
        f.shipForward = readVec3(source.at("ship_forward"));
        f.shipVelocity = readVec3(source.at("ship_velocity"));
        if (source.contains("ship_angular_rate_pyr_rad_s"))
            f.shipAngularRatePyrRadPerSec =
                readVec3(source.at("ship_angular_rate_pyr_rad_s"));
        if (source.contains("main_engine_acceleration_mps2"))
            f.mainEngineAccelerationMps2 =
                readVec3(source.at("main_engine_acceleration_mps2"));
        if (source.contains("manoeuvre_acceleration_mps2"))
            f.manoeuvreAccelerationMps2 =
                readVec3(source.at("manoeuvre_acceleration_mps2"));
        if (source.contains("engine_acceleration_mps2"))
            f.engineAccelerationMps2 =
                readVec3(source.at("engine_acceleration_mps2"));
        if (source.contains("ideal_linear_acceleration_demand_mps2"))
            f.idealLinearAccelerationDemandMps2 =
                readVec3(source.at("ideal_linear_acceleration_demand_mps2"));
        if (source.contains("ideal_angular_acceleration_demand_rad_s2"))
            f.idealAngularAccelerationDemandRadPerSec2 =
                readVec3(source.at("ideal_angular_acceleration_demand_rad_s2"));
        if (source.contains("executed_linear_acceleration_demand_mps2"))
            f.executedLinearAccelerationDemandMps2 =
                readVec3(source.at("executed_linear_acceleration_demand_mps2"));
        if (source.contains("executed_angular_acceleration_demand_rad_s2"))
            f.executedAngularAccelerationDemandRadPerSec2 =
                readVec3(source.at("executed_angular_acceleration_demand_rad_s2"));
        f.hasRuntimeControlLaw =
            source.value("has_runtime_control_law", false);
        if (f.hasRuntimeControlLaw)
        {
            f.runtimeControlLaw =
                source.value("runtime_control_law", std::string {});
        }
        f.mainEngineThrottle01 =
            source.value("main_engine_throttle_01", 0.0);

        if (source.contains("ship_right") && source.contains("ship_up"))
        {
            f.shipRight = readVec3(source.at("ship_right"));
            f.shipUp = readVec3(source.at("ship_up"));
        }
        else
        {
            // Backward compatibility with v1 traces: reconstruct only when the
            // old file genuinely did not record full body attitude.
            glm::dvec3 forward = f.shipForward;
            if (glm::length(forward) <= 1.0e-12)
                forward = {1.0, 0.0, 0.0};
            forward = glm::normalize(forward);

            glm::dvec3 upSeed(0.0, 1.0, 0.0);
            if (std::abs(glm::dot(forward, upSeed)) > 0.92)
                upSeed = {1.0, 0.0, 0.0};

            f.shipRight = glm::normalize(glm::cross(forward, upSeed));
            f.shipUp = glm::normalize(glm::cross(f.shipRight, forward));
        }

        f.hasProgramReference =
            source.value("has_program_reference", false);
        if (f.hasProgramReference)
        {
            f.programReferencePosition =
                readVec3(source.at("program_reference_position"));
            if (source.contains("program_reference_velocity"))
            {
                f.programReferenceVelocity =
                    readVec3(source.at("program_reference_velocity"));
            }
            f.programSpeedCorridorHalfWidthMps =
                source.value(
                    "program_speed_corridor_half_width_mps",
                    0.0
                );
            f.programProgressCorridorHalfWidthMeters =
                source.value(
                    "program_progress_corridor_half_width_m",
                    0.0
                );
            f.programReferenceForward =
                readVec3(source.at("program_reference_forward"));
            f.programReferenceRight =
                readVec3(source.at("program_reference_right"));
            f.programReferenceUp =
                readVec3(source.at("program_reference_up"));
            f.programTrackingCorridorRadiusMeters =
                source.value(
                    "program_tracking_corridor_radius_m",
                    0.0
                );
        }

        f.hazardActive = source.value("hazard_active", false);
        f.hazardPosition = readVec3(source.at("hazard_position"));
        if (source.contains("hazard_velocity"))
            f.hazardVelocity = readVec3(source.at("hazard_velocity"));
        f.plannerLookAheadSeconds =
            source.value("planner_lookahead_s", 0.0);
        f.hazardRadiusMeters =
            source.value("hazard_radius_m", 0.0);
        f.hazardCollisionEnvelopeRadiusMeters =
            source.value("hazard_collision_envelope_radius_m", 0.0);
        f.hazardPlannerEnvelopeRadiusMeters =
            source.value("hazard_planner_envelope_radius_m", 0.0);
        f.dynamicClearanceMeters =
            source.value("dynamic_clearance_m", 0.0);

        f.phase = source.value("phase", std::string {});
        f.plannerStatus =
            source.value("planner_status", std::string {});
        f.replanEvent = source.value("replan_event", false);

        if (source.contains("selected_target"))
        {
            f.hasSelectedTarget = true;
            f.selectedTarget =
                readVec3(source.at("selected_target"));
        }
        if (source.contains("reacquisition_target"))
        {
            f.hasReacquisitionTarget = true;
            f.reacquisitionTarget =
                readVec3(source.at("reacquisition_target"));
        }
        if (source.contains("portal_target"))
        {
            f.hasPortalTarget = true;
            f.portalTarget =
                readVec3(source.at("portal_target"));
        }

        trace.frames.push_back(std::move(f));
    }

    return trace;
}

} // namespace elite::tools::navigation_runtime
