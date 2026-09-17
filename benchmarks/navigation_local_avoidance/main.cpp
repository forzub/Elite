#include "world/navigation/local/LocalAvoidancePlanner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Avoidance = world::navigation::LocalAvoidancePlanner;
using Horizon = world::navigation::LocalHorizonPlanner;
using Map = world::navigation::NavigationMap;
using Space = world::navigation::NavigationSpace;
using Clock = std::chrono::steady_clock;

volatile std::uint64_t gSink = 0;

enum class ScenarioMode
{
    NominalClear,
    EarlyAdjust,
    AllStaticRejected,
    AllDynamicRejected
};

struct Options
{
    int warmup = 5;
    int iterations = 30;
    std::filesystem::path output = "navigation_local_avoidance_benchmark.csv";
};

struct ScenarioSpec
{
    const char* name = "";
    std::size_t candidateCount = 0;
    ScenarioMode mode = ScenarioMode::NominalClear;
    std::size_t expectedHorizonEvaluations = 1;
    std::size_t expectedStaticPointQueries = 0;
};

struct ScenarioData
{
    Avoidance::Query query;
    Map::QueryResult dynamic;
    Space space;
};

struct Result
{
    std::string scenario;
    std::size_t candidateCount = 0;
    std::size_t callsPerSample = 0;
    double medianUs = 0.0;
    double p95Us = 0.0;
    std::size_t targetProbesExamined = 0;
    std::size_t staticRejected = 0;
    std::size_t dynamicRejected = 0;
    std::size_t horizonEvaluations = 0;
    std::size_t candidateVisitsEstimate = 0;
    std::size_t staticPointQueries = 0;
    Avoidance::Status status = Avoidance::Status::StaticHold;
};

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto value = [&](const char* name) -> const char* {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + name);
            return argv[++i];
        };

        if (arg == "--warmup")
            options.warmup = std::stoi(value("--warmup"));
        else if (arg == "--iterations")
            options.iterations = std::stoi(value("--iterations"));
        else if (arg == "--output")
            options.output = value("--output");
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "navigation_local_avoidance_benchmark [--warmup N] "
                   "[--iterations N] [--output file.csv]\n";
            std::exit(0);
        }
        else
            throw std::runtime_error("unknown argument: " + arg);
    }

    if (options.warmup < 0 || options.iterations <= 0)
        throw std::runtime_error("warmup/iterations must be non-negative/positive");
    return options;
}

Avoidance::Query baseQuery()
{
    Avoidance::Query query;
    query.horizon.agent.entityId = 100;
    query.horizon.agent.positionMapMeters = {0.0, 0.0, 0.0};
    query.horizon.agent.velocityMapMetersPerSecond = {10.0, 0.0, 0.0};
    query.horizon.agent.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    query.horizon.agent.radiusMeters = 2.0;

    query.horizon.nominalTarget.positionMapMeters = {100.0, 0.0, 0.0};
    query.horizon.nominalTarget.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    query.horizon.nominalTarget.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

    query.horizon.dynamicResultAgeSeconds = 0.0;
    query.horizon.policy.lookAheadSeconds = 3.0;
    query.horizon.policy.maxResultAgeSeconds = 0.25;
    query.horizon.policy.maxBrakingAccelerationMetersPerSecond2 = 10.0;
    query.horizon.policy.turnDistanceMeters = 0.0;
    query.horizon.policy.safetyMarginMeters = 1.0;
    query.horizon.policy.minimumHorizonMeters = 10.0;

    query.avoidance.primaryDeflectionRadians = 0.2617993877991494;
    query.avoidance.secondaryDeflectionRadians = 0.5235987755982988;
    query.avoidance.azimuthSamples = 8;
    query.avoidance.staticAdditionalClearanceMeters = 0.0;
    return query;
}

Map::Candidate stationaryCandidate(
    Map::EntityId id,
    Map::Vec3d position,
    double radius,
    double sweptRadius
)
{
    Map::Candidate candidate;
    candidate.entityId = id;
    candidate.positionMapMeters = position;
    candidate.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    candidate.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    candidate.predictedEndPositionMapMeters = position;
    candidate.conservativeSweptCenterMapMeters = position;
    candidate.actorRadiusMeters = radius;
    candidate.conservativeSweptRadiusMeters = sweptRadius;
    candidate.motionRevision = 1;
    return candidate;
}

Map::Candidate movingCandidate(
    Map::EntityId id,
    Map::Vec3d position,
    Map::Vec3d velocity,
    double radius,
    double sweptRadius
)
{
    Map::Candidate candidate = stationaryCandidate(id, position, radius, sweptRadius);
    candidate.velocityMapMetersPerSecond = velocity;
    candidate.predictedEndPositionMapMeters = {
        position.x + velocity.x * 3.0,
        position.y + velocity.y * 3.0,
        position.z + velocity.z * 3.0
    };
    candidate.conservativeSweptCenterMapMeters = {
        0.5 * (candidate.positionMapMeters.x + candidate.predictedEndPositionMapMeters.x),
        0.5 * (candidate.positionMapMeters.y + candidate.predictedEndPositionMapMeters.y),
        0.5 * (candidate.positionMapMeters.z + candidate.predictedEndPositionMapMeters.z)
    };
    return candidate;
}

Map::Candidate clearCandidate(std::size_t index)
{
    const double offset = static_cast<double>(index);
    return stationaryCandidate(
        static_cast<Map::EntityId>(1000 + index),
        {
            80.0 + std::fmod(offset * 13.0, 60.0),
            120.0 + std::fmod(offset * 17.0, 80.0),
            -90.0 + std::fmod(offset * 19.0, 70.0)
        },
        1.0,
        2.0
    );
}

Space makeSingleRegionSpace(double lateralHalfExtent)
{
    Space space;
    Space::StaticSpaceUpdate update;
    update.sourceRevision = 31;

    Space::RegionInput region;
    region.regionId = 1;
    region.boundsMapMeters.minMapMeters = {-20.0, -lateralHalfExtent, -lateralHalfExtent};
    region.boundsMapMeters.maxMapMeters = {200.0, lateralHalfExtent, lateralHalfExtent};
    region.clearanceRadiusMeters = 1000.0;
    region.geometryRevision = 1;
    update.regions.push_back(region);

    space.replaceStaticWorld(std::move(update));
    return space;
}

ScenarioData buildScenario(const ScenarioSpec& spec)
{
    ScenarioData data;
    data.query = baseQuery();
    data.dynamic.mapRevision = 7;
    data.dynamic.sourceRevision = 11;
    data.dynamic.candidates.reserve(spec.candidateCount);

    const double lateralHalfExtent =
        spec.mode == ScenarioMode::AllStaticRejected ? 2.5 : 100.0;
    data.space = makeSingleRegionSpace(lateralHalfExtent);

    for (std::size_t i = 0; i < spec.candidateCount; ++i)
        data.dynamic.candidates.push_back(clearCandidate(i));

    if (spec.mode == ScenarioMode::EarlyAdjust ||
        spec.mode == ScenarioMode::AllStaticRejected)
    {
        if (data.dynamic.candidates.empty())
            throw std::runtime_error("adjustment scenarios require candidates");
        data.dynamic.candidates[0] = stationaryCandidate(
            200,
            {8.0, 8.0, 0.0},
            1.0,
            5.0
        );
    }
    else if (spec.mode == ScenarioMode::AllDynamicRejected)
    {
        if (data.dynamic.candidates.empty())
            throw std::runtime_error("dynamic-reject scenario requires candidates");
        data.dynamic.candidates[0] = movingCandidate(
            201,
            {30.0, 0.0, 0.0},
            {-10.0, 0.0, 0.0},
            2.0,
            35.0
        );
    }

    return data;
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    const std::size_t index = static_cast<std::size_t>(
        std::ceil(clamped * static_cast<double>(values.size()))
    ) - (clamped > 0.0 ? 1u : 0u);
    return values[std::min(index, values.size() - 1)];
}

std::size_t callsPerSample(const ScenarioSpec& spec)
{
    constexpr std::size_t targetWorkUnits = 131072;
    constexpr std::size_t staticQueryWeight = 64;
    const std::size_t workUnits = std::max<std::size_t>(
        1,
        spec.candidateCount * spec.expectedHorizonEvaluations +
            spec.expectedStaticPointQueries * staticQueryWeight
    );
    return std::clamp<std::size_t>(targetWorkUnits / workUnits, 4, 4096);
}

const char* statusName(Avoidance::Status status)
{
    switch (status)
    {
    case Avoidance::Status::NominalClear:
        return "NominalClear";
    case Avoidance::Status::AdjustedClear:
        return "AdjustedClear";
    case Avoidance::Status::ConflictHold:
        return "ConflictHold";
    case Avoidance::Status::StaleHold:
        return "StaleHold";
    case Avoidance::Status::StaticHold:
        return "StaticHold";
    }
    return "Unknown";
}

std::size_t horizonEvaluations(const Avoidance::Result& result)
{
    return 1 + result.dynamicRejected + (result.adjustedTarget ? 1u : 0u);
}

std::size_t staticPointQueries(const Avoidance::Result& result)
{
    if (result.status == Avoidance::Status::NominalClear ||
        result.status == Avoidance::Status::StaleHold)
    {
        return 0;
    }
    return 1 + result.targetProbesExamined;
}

void validateResult(const ScenarioSpec& spec, const Avoidance::Result& result)
{
    const std::size_t horizonPasses = horizonEvaluations(result);
    const std::size_t staticQueries = staticPointQueries(result);

    if (horizonPasses != spec.expectedHorizonEvaluations)
        throw std::runtime_error(std::string("horizon evaluation count mismatch: ") + spec.name);
    if (staticQueries != spec.expectedStaticPointQueries)
        throw std::runtime_error(std::string("static point-query count mismatch: ") + spec.name);

    switch (spec.mode)
    {
    case ScenarioMode::NominalClear:
        if (result.status != Avoidance::Status::NominalClear ||
            result.targetProbesExamined != 0 || result.adjustedTarget)
        {
            throw std::runtime_error(std::string("nominal-clear contract failed: ") + spec.name);
        }
        break;

    case ScenarioMode::EarlyAdjust:
        if (result.status != Avoidance::Status::AdjustedClear ||
            !result.adjustedTarget || result.targetProbesExamined != 1 ||
            result.staticRejected != 0 || result.dynamicRejected != 0)
        {
            throw std::runtime_error(std::string("early-adjust contract failed: ") + spec.name);
        }
        break;

    case ScenarioMode::AllStaticRejected:
        if (result.status != Avoidance::Status::ConflictHold ||
            result.adjustedTarget || result.targetProbesExamined != 16 ||
            result.staticRejected != 16 || result.dynamicRejected != 0)
        {
            throw std::runtime_error(std::string("all-static-rejected contract failed: ") + spec.name);
        }
        break;

    case ScenarioMode::AllDynamicRejected:
        if (result.status != Avoidance::Status::ConflictHold ||
            result.adjustedTarget || result.targetProbesExamined != 16 ||
            result.staticRejected != 0 || result.dynamicRejected != 16)
        {
            throw std::runtime_error(std::string("all-dynamic-rejected contract failed: ") + spec.name);
        }
        break;
    }
}

Result runScenario(const ScenarioSpec& spec, const Options& options)
{
    ScenarioData data = buildScenario(spec);
    Avoidance planner;
    Avoidance::Result last;
    const std::size_t calls = callsPerSample(spec);

    std::vector<double> samplesUs;
    samplesUs.reserve(static_cast<std::size_t>(options.iterations));

    const int total = options.warmup + options.iterations;
    for (int iteration = 0; iteration < total; ++iteration)
    {
        const auto begin = Clock::now();
        for (std::size_t call = 0; call < calls; ++call)
        {
            last = planner.evaluate(data.query, data.dynamic, data.space);
            gSink ^= static_cast<std::uint64_t>(
                last.targetProbesExamined +
                last.staticRejected * 3u +
                last.dynamicRejected * 7u +
                static_cast<std::size_t>(last.status)
            );
        }
        const auto end = Clock::now();

        if (iteration >= options.warmup)
        {
            const double totalUs =
                std::chrono::duration<double, std::micro>(end - begin).count();
            samplesUs.push_back(totalUs / static_cast<double>(calls));
        }
    }

    validateResult(spec, last);

    Result result;
    result.scenario = spec.name;
    result.candidateCount = spec.candidateCount;
    result.callsPerSample = calls;
    result.medianUs = percentile(samplesUs, 0.50);
    result.p95Us = percentile(samplesUs, 0.95);
    result.targetProbesExamined = last.targetProbesExamined;
    result.staticRejected = last.staticRejected;
    result.dynamicRejected = last.dynamicRejected;
    result.horizonEvaluations = horizonEvaluations(last);
    result.candidateVisitsEstimate = result.horizonEvaluations * spec.candidateCount;
    result.staticPointQueries = staticPointQueries(last);
    result.status = last.status;
    return result;
}

void printResult(const Result& result)
{
    std::cout << std::fixed << std::setprecision(4)
        << "[NavLocalAvoidBench] scenario=" << result.scenario
        << " candidates=" << result.candidateCount
        << " calls_per_sample=" << result.callsPerSample
        << " median_us=" << result.medianUs
        << " p95_us=" << result.p95Us
        << " probes=" << result.targetProbesExamined
        << " static_rejected=" << result.staticRejected
        << " dynamic_rejected=" << result.dynamicRejected
        << " horizon_evaluations=" << result.horizonEvaluations
        << " candidate_visits_estimate=" << result.candidateVisitsEstimate
        << " static_point_queries=" << result.staticPointQueries
        << " status=" << statusName(result.status)
        << '\n';
}

void writeCsvHeader(std::ofstream& csv)
{
    csv << "scenario,candidates,calls_per_sample,median_us,p95_us,probes,"
           "static_rejected,dynamic_rejected,horizon_evaluations,"
           "candidate_visits_estimate,static_point_queries,status\n";
}

void writeCsvRow(std::ofstream& csv, const Result& result)
{
    csv << result.scenario << ','
        << result.candidateCount << ','
        << result.callsPerSample << ','
        << result.medianUs << ','
        << result.p95Us << ','
        << result.targetProbesExamined << ','
        << result.staticRejected << ','
        << result.dynamicRejected << ','
        << result.horizonEvaluations << ','
        << result.candidateVisitsEstimate << ','
        << result.staticPointQueries << ','
        << statusName(result.status) << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        std::cout << "[NavLocalAvoidBench] warmup=" << options.warmup
                  << " iterations=" << options.iterations << '\n';

        const std::vector<ScenarioSpec> scenarios {
            {"nominal_clear_64", 64, ScenarioMode::NominalClear, 1, 0},
            {"early_adjust_64", 64, ScenarioMode::EarlyAdjust, 2, 2},
            {"all_static_rejected_64", 64, ScenarioMode::AllStaticRejected, 1, 17},
            {"all_dynamic_rejected_16", 16, ScenarioMode::AllDynamicRejected, 17, 17},
            {"all_dynamic_rejected_64", 64, ScenarioMode::AllDynamicRejected, 17, 17},
            {"all_dynamic_rejected_256", 256, ScenarioMode::AllDynamicRejected, 17, 17},
            {"all_dynamic_rejected_1024", 1024, ScenarioMode::AllDynamicRejected, 17, 17}
        };

        const std::filesystem::path output =
            std::filesystem::absolute(options.output);
        std::ofstream csv(output, std::ios::out | std::ios::trunc);
        if (!csv)
            throw std::runtime_error("failed to open CSV output: " + output.string());
        writeCsvHeader(csv);

        for (const auto& scenario : scenarios)
        {
            const Result result = runScenario(scenario, options);
            printResult(result);
            writeCsvRow(csv, result);
            csv.flush();
        }

        std::cout << "[NavLocalAvoidBench] csv=\"" << output.string() << "\"\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[NavLocalAvoidBench][FAIL] " << error.what() << '\n';
        return 1;
    }
}
