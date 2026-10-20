#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string_view>
#include <system_error>

#include <glm/glm.hpp>

#include "src/game/navigation/planner/RoutePlannerApi.h"

namespace game::navigation::debug
{

// Diagnostics belong to the client orchestration boundary, NEVER to the
// pure route optimizer or the server-side ship physics hot path.
//
// Writes one bounded report per finished route worker (ready or failed),
// not one message per frame. File is reopened/closed for each report so
// a normal or unexpected game exit will not swallow buffered diagnostics.
//
// Native Windows: D:\tmp\elite_docking_planner.log
[[nodiscard]] inline bool appendDockPlannerReport(
    std::uint64_t requestSerial,
    std::string_view event,
    double workerDurationMs,
    double entrySolveDurationMs,
    const planner::RoutePlanRequest& request,
    const planner::RoutePlan& plan
) noexcept
{
#ifndef _WIN32
    (void)requestSerial;
    (void)event;
    (void)workerDurationMs;
    (void)entrySolveDurationMs;
    (void)request;
    (void)plan;
    return false;
#else
    try
    {
        static std::mutex writeMutex;
        const std::lock_guard<std::mutex> lock(writeMutex);

        const std::filesystem::path path(
            "D:/tmp/elite_docking_planner.log"
        );
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
            return false;

        // Append so several docking requests and crashes remain available
        // for analysis. The user can delete the file between test sessions.
        std::ofstream out(path, std::ios::out | std::ios::app);
        if (!out)
            return false;

        const auto nowMs =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

        out << std::fixed << std::setprecision(3)
            << "\n[DockPlannerFile] epoch_ms=" << nowMs
            << " request=" << requestSerial
            << " event=" << event
            << " worker_ms=" << workerDurationMs
            << " connector_solve_ms=" << entrySolveDurationMs
            << " valid=" << (plan.valid() ? 1 : 0)
            << " disposition=" << static_cast<int>(plan.disposition)
            << " failure_code=" << static_cast<int>(plan.failureCode)
            << " curve_count=" << plan.routeCurves.size()
            << " stage_count=" << plan.stages.size()
            << " gate_count=" << plan.gates.size()
            << " execution_gate_count=" << plan.executionGates.size()
            << '\n';

        out << "[DockPlannerRequest]"
            << " assisted_preset="
            << planner::assistedRoutePresetName(
                request.assistedRoutePreset
            )
            << " start_xyz_m=("
            << request.startMeters.x << ','
            << request.startMeters.y << ','
            << request.startMeters.z << ')'
            << " goal_xyz_m=("
            << request.goalMeters.x << ','
            << request.goalMeters.y << ','
            << request.goalMeters.z << ')'
            << " initial_speed_mps=" << request.initialSpeedMps
            << " max_speed_mps=" << request.maxSpeedMps
            << " braking_mps2=" << request.brakingMps2
            << " lateral_mps2=" << request.lateralMps2
            << " initial_lead_m=" << request.initialForwardLeadMeters
            << " protected_start_m=" << request.initialProtectedStraightMeters
            << " terminal_approach_m=" << request.terminalApproachLengthMeters
            << " continuous_assisted="
            << (request.useContinuousAssistedOptimizer ? 1 : 0)
            << " assisted_compact="
            << (request.assistedCompactStartLead ? 1 : 0)
            << '\n';

        if (!plan.routeCurves.empty())
        {
            const double routeLengthMeters =
                plan.routeCurves.back().endProgressMeters -
                plan.routeCurves.front().startProgressMeters;
            const double directDistanceMeters =
                glm::length(request.goalMeters - request.startMeters);
            out << "[DockPlannerGeometry]"
                << " route_length_m=" << routeLengthMeters
                << " direct_distance_m=" << directDistanceMeters
                << " terminal_radius_m=" << plan.terminalTurnRadiusMeters
                << '\n';
        }

        for (std::size_t i = 0; i < plan.stages.size(); ++i)
        {
            const auto& stage = plan.stages[i];
            out << "[DockPlannerStage]"
                << " index=" << i
                << " kind=" << static_cast<int>(stage.kind)
                << " id=" << std::quoted(stage.id)
                << " start_m=" << stage.startProgressMeters
                << " end_m=" << stage.endProgressMeters
                << " length_m="
                << (stage.endProgressMeters - stage.startProgressMeters)
                << " hard=" << (stage.hardContainment ? 1 : 0)
                << " frame_policy=" << static_cast<int>(stage.framePolicy)
                << " refresh_policy=" << static_cast<int>(stage.refreshPolicy)
                << " source=" << std::quoted(stage.sourceId)
                << " volume=" << std::quoted(stage.volumeId)
                << '\n';
        }

        out << "[DockPlannerResult]"
            << " failure=" << std::quoted(plan.failure)
            << " message=" << std::quoted(plan.userMessage)
            << " diagnostics=" << std::quoted(plan.diagnosticSummary)
            << '\n';
        out.flush();
        return out.good();
    }
    catch (...)
    {
        // File-system failures must never change the autopilot control path.
        return false;
    }
#endif
}

} // namespace game::navigation::debug
