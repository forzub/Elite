#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

#include "src/world/navigation/GeometricPathPlanner.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/game/navigation/OrdinaryPhysicalManeuverCompiler.h"

namespace
{

using Planner = world::navigation::GeometricPathPlanner;
using Obstacle = world::navigation::NavigationObstacle;
using Clock = std::chrono::steady_clock;
using Physical = game::navigation::OrdinaryPhysicalManeuverCompiler;

std::uint32_t nextRandom(std::uint32_t& state)
{
    state = state * 1664525u + 1013904223u;
    return state;
}

double uniform(std::uint32_t& state, double low, double high)
{
    return low + (high - low) *
        (double(nextRandom(state)) / double(UINT32_MAX));
}

std::vector<Obstacle> barrels()
{
    std::vector<Obstacle> result;
    result.reserve(100);
    std::uint32_t seed = 0xE11E2026u;
    for (int row = 0; row < 10; ++row)
    for (int col = 0; col < 10; ++col)
    {
        Obstacle obstacle;
        obstacle.entityId = std::uint32_t(result.size() + 1);
        obstacle.id = "barrel-" + std::to_string(obstacle.entityId);
        obstacle.shape = world::navigation::NavigationObstacleShape::Capsule;
        obstacle.radiusMeters = 8.0;
        obstacle.capsuleHalfLengthMeters = 4.0;
        obstacle.centerMeters = {
            -360.0 + 80.0 * col + uniform(seed, -7.0, 7.0),
            -270.0 + 60.0 * row + uniform(seed, -7.0, 7.0),
            0.0
        };
        result.push_back(obstacle);
    }
    return result;
}

std::vector<Obstacle> doglegWalls()
{
    std::vector<Obstacle> result;
    const auto wall = [&](std::uint32_t id, double x, double y,
                          double halfX, double halfY)
    {
        Obstacle value;
        value.id = "wall-" + std::to_string(id);
        value.entityId = id;
        value.shape = world::navigation::NavigationObstacleShape::Box;
        value.centerMeters = {x, y, 0.0};
        value.halfExtentsMeters = {halfX, halfY, 1000.0};
        result.push_back(value);
    };
    wall(1, -100.0, -100.0, 12.0, 200.0); // open above
    wall(2, 100.0, 100.0, 12.0, 200.0);   // open below
    wall(3, 0.0, -300.0, 1000.0, 50.0);  // field boundary
    wall(4, 0.0, 300.0, 1000.0, 50.0);
    return result;
}

world::navigation::GeometricPathRequest shipRequest(
    std::size_t index,
    const std::vector<Obstacle>& scene,
    std::size_t obstacleLimit,
    bool dogleg,
    bool separated
)
{
    std::uint32_t seed = 0xC0B2026u + std::uint32_t(index * 73939133u);
    world::navigation::GeometricPathRequest request;
    if (separated)
    {
        // 25 x 20 lanes, 30 m apart: both endpoints are distinct and the
        // active 13 m-radius hulls do not overlap at either boundary.
        const double y = (double(index % 20) - 9.5) * 30.0;
        const double z = (double(index / 20) - 12.0) * 30.0;
        request.startMeters = {-500.0, y, z};
        request.goalMeters = {500.0, y, z};
    }
    else
    {
        request.startMeters = dogleg
            ? glm::dvec3(-350.0, uniform(seed, -70.0, 70.0), 0.0)
            : glm::dvec3(-480.0, uniform(seed, -300.0, 300.0), 0.0);
        request.goalMeters = dogleg
            ? glm::dvec3(350.0, uniform(seed, -70.0, 70.0), 0.0)
            : glm::dvec3(480.0, uniform(seed, -300.0, 300.0), 0.0);
    }
    request.obstacles = scene;
    // Cobra Mk1 logical hull: 26.0 x 5.0 x 22.2 m. The active geometric
    // projection uses max half extent, i.e. 13.0 m.
    request.params.agentRadiusMeters = 13.0;
    request.params.additionalClearanceMeters = 2.0;
    request.params.maxConsideredObstacles = obstacleLimit;
    return request;
}

double percentile(const std::vector<double>& sorted, double fraction)
{
    if (sorted.empty())
        return 0.0;
    const std::size_t index = std::min(
        sorted.size() - 1,
        std::size_t(std::ceil(fraction * double(sorted.size()))) - 1
    );
    return sorted[index];
}

Physical::Result firstPrimitive(const world::navigation::GeometricPathResult& route,
                                const world::navigation::GeometricPathRequest& request)
{
    Physical::Query query;
    query.state.positionMapMeters = request.startMeters;
    query.state.velocityMapMetersPerSecond = {20.0, 0.0, 0.0};
    query.state.forwardMap = {1.0, 0.0, 0.0};
    query.state.rightMap = {0.0, 0.0, 1.0};
    query.state.upMap = {0.0, 1.0, 0.0};
    query.capability.maxForwardAccelerationMps2 = 73.5;
    query.capability.maxReverseAccelerationMps2 = 2.0;
    query.capability.maxLateralAccelerationMps2 = 2.0;
    query.capability.maxVerticalAccelerationMps2 = 2.0;
    query.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    query.capability.maxAngularSpeedRadPerSec = 2.5;
    query.linearFeedbackReserveMps2 = 0.5;
    query.angularFeedbackReserveRadPerSec2 = 0.1;
    query.controlResponseReserveSeconds = 0.18;
    query.velocityResponsePerSecond = 0.75;
    query.maximumProgramSeconds = 4.0;

    for (const auto& point : route.pointsMeters)
    {
        const glm::dvec3 delta = point - request.startMeters;
        if (glm::length(delta) <= 1.0e-6)
            continue;
        query.geometricTargetPositionMapMeters = point;
        query.desiredVelocityMapMetersPerSecond =
            20.0 * glm::normalize(delta);
        return Physical::compile(query);
    }
    return {};
}

struct ChainResult
{
    enum class Stop {
        Reached, NoPhysicalCandidate, Contact, NoProgress, Budget,
        TerminalStateMiss
    };
    Stop stop = Stop::NoPhysicalCandidate;
    std::size_t legsReached = 0;
    std::size_t primitives = 0;
    std::size_t attempts = 0;
    std::size_t spatialRejections = 0;
    std::size_t angularStateRejections = 0;
    std::size_t horizonRejections = 0;
    double finalSpeedMps = 0.0;
    double finalForwardDot = 0.0;
    double simulatedSeconds = 0.0;
};

// Diagnostic only: chains the exact B5 terminal sample state through the
// chosen visibility-graph points. It does not turn B5 into an accepted program.
ChainResult chainObserver(
    const world::navigation::GeometricPathResult& route,
    const world::navigation::GeometricPathRequest& request
)
{
    ChainResult result;
    if (!route.valid)
        return result;

    Physical::Query base;
    base.state.positionMapMeters = request.startMeters;
    base.state.velocityMapMetersPerSecond = {20.0, 0.0, 0.0};
    base.state.forwardMap = {1.0, 0.0, 0.0};
    base.state.rightMap = {0.0, 0.0, 1.0};
    base.state.upMap = {0.0, 1.0, 0.0};
    base.capability.maxForwardAccelerationMps2 = 73.5;
    base.capability.maxReverseAccelerationMps2 = 2.0;
    base.capability.maxLateralAccelerationMps2 = 2.0;
    base.capability.maxVerticalAccelerationMps2 = 2.0;
    base.capability.maxAngularAccelerationRadPerSec2 = 3.0;
    base.capability.maxAngularSpeedRadPerSec = 2.5;
    base.linearFeedbackReserveMps2 = 0.5;
    base.angularFeedbackReserveRadPerSec2 = 0.1;
    base.controlResponseReserveSeconds = 0.18;
    base.velocityResponsePerSecond = 0.75;
    base.targetCaptureRadiusMeters = 8.0;

    for (std::size_t leg = 1; leg < route.pointsMeters.size(); ++leg)
    {
        const glm::dvec3 target = route.pointsMeters[leg];
        std::size_t legPrimitives = 0;
        while (glm::length(target - base.state.positionMapMeters) >
               base.targetCaptureRadiusMeters)
        {
            if (legPrimitives >= 32 || result.primitives >= 256)
            {
                result.stop = ChainResult::Stop::Budget;
                return result;
            }
            const double initialDistance =
                glm::length(target - base.state.positionMapMeters);
            bool compiled = false;
            bool clear = false;
            bool progress = false;
            bool selected = false;
            double bestDistance = initialDistance;
            Physical::State bestState;
            double bestDuration = 0.0;

            const glm::dvec3 direction =
                glm::normalize(target - base.state.positionMapMeters);
            const glm::dvec3 exitDirection =
                leg + 1 < route.pointsMeters.size()
                    ? glm::normalize(route.pointsMeters[leg + 1] - target)
                    : glm::dvec3(0.0);

            for (double speed : {20.0, 10.0, 5.0, 0.0})
            for (double horizon : {1.0, 2.0, 4.0, 6.0, 8.0})
            {
                // Avoid choosing a full stop hundreds of metres before the
                // next capture region merely because it wins one greedy step.
                if (initialDistance > 80.0 && speed < 10.0)
                    continue;
                Physical::Query query = base;
                query.geometricTargetPositionMapMeters = target;
                // Start blending toward the exit direction before the bend.
                const double blend = std::clamp(
                    1.0 - initialDistance / 80.0, 0.0, 1.0
                );
                query.desiredVelocityMapMetersPerSecond =
                    speed * glm::normalize(
                        direction * (1.0 - blend) +
                        exitDirection * blend + direction * 1.0e-9
                    );
                if (leg + 1 == route.pointsMeters.size())
                    query.desiredVelocityMapMetersPerSecond = speed * direction;
                query.maximumProgramSeconds = horizon;

                ++result.attempts;
                const auto compiledResult = Physical::compile(query);
                if (compiledResult.status != Physical::Status::Compiled)
                {
                    using Reason = Physical::InfeasibilityReason;
                    result.spatialRejections +=
                        compiledResult.infeasibility.reason ==
                        Reason::SpatialTargetNotApproached;
                    result.angularStateRejections +=
                        compiledResult.infeasibility.reason ==
                        Reason::InitialAngularStateUnsupported;
                    result.horizonRejections +=
                        compiledResult.infeasibility.reason ==
                        Reason::ProgramHorizonTooShort;
                    continue;
                }
                compiled = true;
                for (std::size_t ci = 0; ci < compiledResult.candidateCount; ++ci)
                {
                    const auto& candidate = compiledResult.candidates[ci];
                    bool candidateClear = true;
                    for (std::size_t si = 1; si < candidate.sampleCount; ++si)
                    {
                        candidateClear = candidateClear &&
                            world::navigation::segmentClearOfNavigationObstacles(
                                candidate.samples[si - 1].positionMapMeters,
                                candidate.samples[si].positionMapMeters,
                                request.obstacles,
                                request.params.agentRadiusMeters,
                                request.params.additionalClearanceMeters
                            );
                    }
                    if (!candidateClear)
                        continue;
                    clear = true;
                    const auto& last = candidate.samples[candidate.sampleCount - 1];
                    const double distance = glm::length(
                        target - last.positionMapMeters
                    );
                    if (distance >= initialDistance - 1.0e-6 &&
                        distance > base.targetCaptureRadiusMeters)
                        continue;
                    if (distance > base.targetCaptureRadiusMeters &&
                        glm::dot(last.velocityMapMetersPerSecond,
                                 target - last.positionMapMeters) <= 0.0)
                        continue;
                    progress = true;
                    if (selected && distance >= bestDistance)
                        continue;
                    selected = true;
                    bestDistance = distance;
                    bestDuration = last.timeOffsetSeconds;
                    bestState.positionMapMeters = last.positionMapMeters;
                    bestState.velocityMapMetersPerSecond =
                        last.velocityMapMetersPerSecond;
                    bestState.forwardMap = last.forwardMap;
                    bestState.rightMap = last.rightMap;
                    bestState.upMap = last.upMap;
                    bestState.angularVelocityMapRadPerSecond =
                        last.angularVelocityMapRadPerSecond;
                }
            }
            if (!selected)
            {
                if (std::getenv("ELITE_ROUTE_CHAIN_TRACE"))
                    std::cerr << "failed_leg=" << leg
                              << " primitives=" << result.primitives
                              << " distance=" << initialDistance
                              << " compiled=" << compiled
                              << " clear=" << clear
                              << " spatial_rejections=" << result.spatialRejections
                              << " horizon_rejections=" << result.horizonRejections
                              << " position=" << base.state.positionMapMeters.x
                              << "," << base.state.positionMapMeters.y
                              << " velocity=" << base.state.velocityMapMetersPerSecond.x
                              << "," << base.state.velocityMapMetersPerSecond.y
                              << " target=" << target.x << "," << target.y
                              << '\n';
                result.stop = !compiled ? ChainResult::Stop::NoPhysicalCandidate
                    : !clear ? ChainResult::Stop::Contact
                    : !progress ? ChainResult::Stop::NoProgress
                    : ChainResult::Stop::NoProgress;
                return result;
            }
            base.state = bestState;
            if (std::getenv("ELITE_ROUTE_CHAIN_TRACE"))
                std::cerr << "accepted_leg=" << leg
                          << " distance_before=" << initialDistance
                          << " distance_after=" << bestDistance
                          << " duration=" << bestDuration
                          << " position=" << bestState.positionMapMeters.x
                          << "," << bestState.positionMapMeters.y
                          << " velocity=" << bestState.velocityMapMetersPerSecond.x
                          << "," << bestState.velocityMapMetersPerSecond.y
                          << " target=" << target.x << "," << target.y
                          << '\n';
            result.simulatedSeconds += bestDuration;
            ++result.primitives;
            ++legPrimitives;
        }
        ++result.legsReached;
    }
    // The benchmark contract asks for a stationary ship facing +X at the
    // finish. Merely entering the final 8 m position sphere is insufficient.
    const double finalSpeed = glm::length(base.state.velocityMapMetersPerSecond);
    const double finalForward = glm::dot(
        glm::normalize(base.state.forwardMap), glm::dvec3(1.0, 0.0, 0.0)
    );
    result.finalSpeedMps = finalSpeed;
    result.finalForwardDot = finalForward;
    result.stop = finalSpeed <= 2.0 &&
                  finalForward >= std::cos(10.0 * 3.141592653589793 / 180.0)
        ? ChainResult::Stop::Reached
        : ChainResult::Stop::TerminalStateMiss;
    return result;
}

void run(std::size_t ships, std::size_t obstacleLimit,
         const std::string& sceneName, bool chain)
{
    const bool separated = sceneName == "open_separated";
    const bool dogleg = sceneName == "dogleg" || sceneName == "open";
    const auto scene = sceneName == "open" || separated
        ? std::vector<Obstacle>{}
        : dogleg ? doglegWalls() : barrels();
    std::vector<double> latenciesMs;
    std::vector<double> firstPrimitiveMs;
    std::vector<double> chainMs;
    std::vector<double> terminalSpeeds;
    latenciesMs.reserve(ships);
    firstPrimitiveMs.reserve(ships);
    std::size_t valid = 0;
    std::size_t detours = 0;
    std::size_t fullSceneValid = 0;
    std::size_t points = 0;
    std::size_t turnsOver45 = 0;
    std::size_t turnsOver90 = 0;
    std::size_t routesOver90 = 0;
    double routeLength = 0.0;
    std::array<std::size_t, 6> chainStops {};
    std::size_t chainPrimitives = 0;
    std::size_t chainAttempts = 0;
    std::size_t chainLegsReached = 0;
    std::size_t terminalSpeedMisses = 0;
    std::size_t terminalAttitudeMisses = 0;
    std::size_t chainSpatialRejections = 0;
    std::size_t chainAngularStateRejections = 0;
    std::size_t chainHorizonRejections = 0;
    double chainedTimeSeconds = 0.0;
    std::size_t physicalCandidate = 0;
    std::size_t physicalRejected = 0;

    // Keep fixture construction outside the timed region. The production
    // planner still copies and filters the scene internally on every query.
    std::vector<world::navigation::GeometricPathRequest> requests;
    requests.reserve(ships);
    for (std::size_t i = 0; i < ships; ++i)
        requests.push_back(shipRequest(i, scene, obstacleLimit, dogleg,
                                       separated));

    // A throughput batch does not imply that these actors can occupy the
    // scene at the same instant. Audit the initial/terminal spatial contract.
    std::size_t coincidentStarts = 0;
    std::size_t coincidentGoals = 0;
    std::size_t overlappingStartPairs = 0;
    std::size_t overlappingGoalPairs = 0;
    double minimumStartSeparation = std::numeric_limits<double>::infinity();
    double minimumGoalSeparation = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < requests.size(); ++i)
    for (std::size_t j = i + 1; j < requests.size(); ++j)
    {
        const double startDistance = glm::length(
            requests[i].startMeters - requests[j].startMeters
        );
        const double goalDistance = glm::length(
            requests[i].goalMeters - requests[j].goalMeters
        );
        minimumStartSeparation = std::min(minimumStartSeparation, startDistance);
        minimumGoalSeparation = std::min(minimumGoalSeparation, goalDistance);
        coincidentStarts += startDistance < 1.0e-9;
        coincidentGoals += goalDistance < 1.0e-9;
        overlappingStartPairs += startDistance < 26.0;
        overlappingGoalPairs += goalDistance < 26.0;
    }

    const auto totalStart = Clock::now();
    for (const auto& request : requests)
    {
        const auto start = Clock::now();
        const auto result = Planner::plan(request);
        const auto end = Clock::now();
        latenciesMs.push_back(
            std::chrono::duration<double, std::milli>(end - start).count()
        );
        if (result.valid)
        {
            const auto physicalStart = Clock::now();
            const auto physical = firstPrimitive(result, request);
            const auto physicalEnd = Clock::now();
            firstPrimitiveMs.push_back(
                std::chrono::duration<double, std::milli>(
                    physicalEnd - physicalStart
                ).count()
            );
            physicalCandidate +=
                physical.status == Physical::Status::Compiled &&
                physical.candidateCount > 0;
            physicalRejected += physical.status != Physical::Status::Compiled;
            if (chain)
            {
                const auto chainStart = Clock::now();
                const auto outcome = chainObserver(result, request);
                const auto chainEnd = Clock::now();
                chainMs.push_back(std::chrono::duration<double, std::milli>(
                    chainEnd - chainStart
                ).count());
                ++chainStops[static_cast<std::size_t>(outcome.stop)];
                chainPrimitives += outcome.primitives;
                chainAttempts += outcome.attempts;
                chainLegsReached += outcome.legsReached;
                if (outcome.stop == ChainResult::Stop::TerminalStateMiss ||
                    outcome.stop == ChainResult::Stop::Reached)
                {
                    terminalSpeeds.push_back(outcome.finalSpeedMps);
                    terminalSpeedMisses += outcome.finalSpeedMps > 2.0;
                    terminalAttitudeMisses += outcome.finalForwardDot <
                        std::cos(10.0 * 3.141592653589793 / 180.0);
                }
                chainSpatialRejections += outcome.spatialRejections;
                chainAngularStateRejections += outcome.angularStateRejections;
                chainHorizonRejections += outcome.horizonRejections;
                chainedTimeSeconds += outcome.simulatedSeconds;
            }
        }
        valid += result.valid;
        detours += result.obstacleDetourUsed;
        bool allSegmentsClear = result.valid;
        for (std::size_t i = 1; i < result.pointsMeters.size(); ++i)
        {
            allSegmentsClear = allSegmentsClear &&
                world::navigation::segmentClearOfNavigationObstacles(
                    result.pointsMeters[i - 1], result.pointsMeters[i],
                    scene, request.params.agentRadiusMeters,
                    request.params.additionalClearanceMeters
                );
        }
        fullSceneValid += allSegmentsClear;
        points += result.pointsMeters.size();
        bool sharpRoute = false;
        for (std::size_t i = 1; i + 1 < result.pointsMeters.size(); ++i)
        {
            const glm::dvec3 a = result.pointsMeters[i] -
                result.pointsMeters[i - 1];
            const glm::dvec3 b = result.pointsMeters[i + 1] -
                result.pointsMeters[i];
            if (glm::length(a) <= 1.0e-9 || glm::length(b) <= 1.0e-9)
                continue;
            const double dot = std::clamp(
                glm::dot(glm::normalize(a), glm::normalize(b)), -1.0, 1.0
            );
            const double degrees = std::acos(dot) * 180.0 / 3.141592653589793;
            turnsOver45 += degrees > 45.0;
            turnsOver90 += degrees > 90.0;
            sharpRoute = sharpRoute || degrees > 90.0;
        }
        routesOver90 += sharpRoute;
        routeLength += result.lengthMeters;
    }
    const double wallMs = std::chrono::duration<double, std::milli>(
        Clock::now() - totalStart
    ).count();
    std::sort(latenciesMs.begin(), latenciesMs.end());
    std::sort(firstPrimitiveMs.begin(), firstPrimitiveMs.end());
    std::sort(chainMs.begin(), chainMs.end());
    std::sort(terminalSpeeds.begin(), terminalSpeeds.end());

    std::cout << std::fixed << std::setprecision(3)
              << "scene=" << sceneName
              << " ships=" << ships << " obstacles=" << scene.size()
              << " coincident_start_pairs=" << coincidentStarts
              << " coincident_goal_pairs=" << coincidentGoals
              << " start_overlap_pairs=" << overlappingStartPairs
              << " goal_overlap_pairs=" << overlappingGoalPairs
              << " min_start_spacing_m=" << minimumStartSeparation
              << " min_goal_spacing_m=" << minimumGoalSeparation
              << " limit=" << obstacleLimit
              << " valid=" << valid << " detours=" << detours
              << " full_scene_clear=" << fullSceneValid
              << " points=" << points
              << " turns_over_45=" << turnsOver45
              << " turns_over_90=" << turnsOver90
              << " routes_over_90=" << routesOver90
              << " route_length_m=" << routeLength
              << " wall_ms=" << wallMs
              << " per_ship_ms_p50=" << percentile(latenciesMs, 0.50)
              << " per_ship_ms_p95=" << percentile(latenciesMs, 0.95)
              << " per_ship_ms_p99=" << percentile(latenciesMs, 0.99)
              << " per_ship_ms_max=" << percentile(latenciesMs, 1.0)
              << " first_unproved_candidates=" << physicalCandidate
              << " first_rejected=" << physicalRejected
              << " first_physical_ms_p50=" << percentile(firstPrimitiveMs, 0.50)
              << " first_physical_ms_p95=" << percentile(firstPrimitiveMs, 0.95)
              << " chain_reached=" << chainStops[0]
              << " chain_no_candidate=" << chainStops[1]
              << " chain_contact=" << chainStops[2]
              << " chain_no_progress=" << chainStops[3]
              << " chain_budget=" << chainStops[4]
              << " chain_terminal_state_miss=" << chainStops[5]
              << " terminal_speed_miss=" << terminalSpeedMisses
              << " terminal_attitude_miss=" << terminalAttitudeMisses
              << " terminal_speed_mps_p50=" << percentile(terminalSpeeds, 0.50)
              << " terminal_speed_mps_p95=" << percentile(terminalSpeeds, 0.95)
              << " chain_legs_reached=" << chainLegsReached
              << " chain_primitives=" << chainPrimitives
              << " chain_attempts=" << chainAttempts
              << " chain_spatial_rejections=" << chainSpatialRejections
              << " chain_angular_state_rejections=" << chainAngularStateRejections
              << " chain_horizon_rejections=" << chainHorizonRejections
              << " chain_seconds_partial=" << chainedTimeSeconds
              << " chain_ms_p50=" << percentile(chainMs, 0.50)
              << " chain_ms_p95=" << percentile(chainMs, 0.95)
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    const std::size_t ships = argc > 1 ? std::stoul(argv[1]) : 500;
    const std::size_t obstacleLimit = argc > 2 ? std::stoul(argv[2]) : 32;
    const std::string sceneName = argc > 3 ? argv[3] : "barrels";
    const bool chain = argc > 4 && std::string(argv[4]) == "chain";
    if (ships == 0 || ships > 10000 || obstacleLimit > 100)
        return 2;
    if (sceneName != "barrels" && sceneName != "dogleg" &&
        sceneName != "open" && sceneName != "open_separated")
        return 2;
    run(ships, obstacleLimit, sceneName, chain);
}
