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

    root["route_points"] = nlohmann::json::array();
    for (const auto& p : trace.routePoints)
        root["route_points"].push_back(vec3Json(p));

    root["turn_points"] = nlohmann::json::array();
    for (const auto& p : trace.turnPoints)
        root["turn_points"].push_back(vec3Json(p));

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

        frame["has_program_reference"] = f.hasProgramReference;
        if (f.hasProgramReference)
        {
            frame["program_reference_position"] =
                vec3Json(f.programReferencePosition);
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

    for (const auto& p : root.at("route_points"))
        trace.routePoints.push_back(readVec3(p));
    for (const auto& p : root.at("turn_points"))
        trace.turnPoints.push_back(readVec3(p));

    for (const auto& source : root.at("frames"))
    {
        TraceFrame f;
        f.timeSeconds = source.at("time_s").get<double>();
        f.shipPosition = readVec3(source.at("ship_position"));
        f.shipForward = readVec3(source.at("ship_forward"));
        f.shipVelocity = readVec3(source.at("ship_velocity"));

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
