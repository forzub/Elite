#include "src/world/navigation/local/LocalHorizonPlanner.h"

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
using Planner = world::navigation::LocalHorizonPlanner;
using Map = world::navigation::NavigationMap;
using Clock = std::chrono::steady_clock;

volatile std::uint64_t gSink = 0;

enum class ScenarioMode
{
    Clear,
    Conflict,
    Stale
};

struct Options
{
    int warmup = 5;
    int iterations = 20;
    std::filesystem::path output = "navigation_local_benchmark.csv";
};

struct ScenarioSpec
{
    const char* name = "";
    std::size_t candidateCount = 0;
    ScenarioMode mode = ScenarioMode::Clear;
};

struct ScenarioData
{
    Planner::Query query;
    Map::QueryResult candidates;
};

struct Result
{
    std::string scenario;
    std::size_t candidateCount = 0;
    std::size_t callsPerSample = 0;
    double medianUs = 0.0;
    double p95Us = 0.0;
    double p95NsPerCandidate = 0.0;
    std::size_t candidatesExamined = 0;
    std::size_t conflictsFound = 0;
    Planner::Status status = Planner::Status::StaleHold;
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
                << "navigation_local_benchmark [--warmup N] [--iterations N] "
                   "[--output file.csv]\n";
            std::exit(0);
        }
        else
            throw std::runtime_error("unknown argument: " + arg);
    }

    if (options.warmup < 0 || options.iterations <= 0)
        throw std::runtime_error("warmup/iterations must be non-negative/positive");
    return options;
}

Map::Vec3d add(const Map::Vec3d& a, const Map::Vec3d& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Map::Vec3d scale(const Map::Vec3d& v, double s)
{
    return {v.x * s, v.y * s, v.z * s};
}

Map::Vec3d positionAt(
    const Map::Vec3d& position,
    const Map::Vec3d& velocity,
    const Map::Vec3d& acceleration,
    double seconds
)
{
    return add(
        add(position, scale(velocity, seconds)),
        scale(acceleration, 0.5 * seconds * seconds)
    );
}

Map::Candidate makeClearCandidate(std::size_t index)
{
    Map::Candidate candidate;
    candidate.entityId = static_cast<Map::EntityId>(1000 + index);
    candidate.positionMapMeters = {
        80.0 + static_cast<double>(index % 97) * 17.0,
        1400.0 + static_cast<double>(index % 19) * 23.0,
        -700.0 + static_cast<double>(index % 13) * 31.0
    };
    candidate.velocityMapMetersPerSecond = {
        static_cast<double>(static_cast<int>(index % 7) - 3) * 2.0,
        static_cast<double>(static_cast<int>(index % 5) - 2) * 1.5,
        0.0
    };
    candidate.accelerationMapMetersPerSecond2 = {
        0.0,
        0.0,
        static_cast<double>(static_cast<int>(index % 3) - 1) * 0.2
    };
    candidate.predictedEndPositionMapMeters = positionAt(
        candidate.positionMapMeters,
        candidate.velocityMapMetersPerSecond,
        candidate.accelerationMapMetersPerSecond2,
        3.0
    );
    candidate.conservativeSweptCenterMapMeters = scale(
        add(candidate.positionMapMeters, candidate.predictedEndPositionMapMeters),
        0.5
    );
    candidate.actorRadiusMeters = 4.0 + static_cast<double>(index % 3);
    candidate.conservativeSweptRadiusMeters = candidate.actorRadiusMeters + 18.0;
    candidate.motionRevision = 1;
    return candidate;
}

Map::Candidate makeConflictCandidate()
{
    Map::Candidate candidate;
    candidate.entityId = 42;
    candidate.positionMapMeters = {320.0, 0.0, 0.0};
    candidate.velocityMapMetersPerSecond = {-35.0, 0.0, 0.0};
    candidate.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    candidate.predictedEndPositionMapMeters = {215.0, 0.0, 0.0};
    candidate.conservativeSweptCenterMapMeters = {267.5, 0.0, 0.0};
    candidate.actorRadiusMeters = 8.0;
    candidate.conservativeSweptRadiusMeters = 60.0;
    candidate.motionRevision = 1;
    return candidate;
}

ScenarioData buildScenario(const ScenarioSpec& spec)
{
    ScenarioData data;
    data.query.agent.entityId = 1;
    data.query.agent.positionMapMeters = {0.0, 0.0, 0.0};
    data.query.agent.velocityMapMetersPerSecond = {120.0, 0.0, 0.0};
    data.query.agent.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};
    data.query.agent.radiusMeters = 6.0;

    data.query.nominalTarget.positionMapMeters = {5000.0, 0.0, 0.0};
    data.query.nominalTarget.velocityMapMetersPerSecond = {0.0, 0.0, 0.0};
    data.query.nominalTarget.accelerationMapMetersPerSecond2 = {0.0, 0.0, 0.0};

    data.query.policy.lookAheadSeconds = 3.0;
    data.query.policy.maxResultAgeSeconds = 0.25;
    data.query.policy.maxBrakingAccelerationMetersPerSecond2 = 15.0;
    data.query.policy.turnDistanceMeters = 50.0;
    data.query.policy.safetyMarginMeters = 20.0;
    data.query.policy.minimumHorizonMeters = 100.0;
    data.query.dynamicResultAgeSeconds =
        spec.mode == ScenarioMode::Stale ? 0.50 : 0.05;

    data.candidates.mapRevision = 7;
    data.candidates.sourceRevision = 11;
    data.candidates.candidates.reserve(spec.candidateCount);
    for (std::size_t i = 0; i < spec.candidateCount; ++i)
        data.candidates.candidates.push_back(makeClearCandidate(i));

    if (spec.mode == ScenarioMode::Conflict && !data.candidates.candidates.empty())
    {
        data.candidates.candidates[spec.candidateCount / 2] =
            makeConflictCandidate();
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

std::size_t callsPerSample(std::size_t candidateCount)
{
    constexpr std::size_t targetCandidateVisits = 65536;
    const std::size_t divisor = std::max<std::size_t>(candidateCount, 1);
    return std::clamp<std::size_t>(
        targetCandidateVisits / divisor,
        16,
        4096
    );
}

const char* statusName(Planner::Status status)
{
    switch (status)
    {
    case Planner::Status::Clear:
        return "Clear";
    case Planner::Status::ConflictHold:
        return "ConflictHold";
    case Planner::Status::StaleHold:
        return "StaleHold";
    }
    return "Unknown";
}

void validateResult(
    const ScenarioSpec& spec,
    const Planner::Result& result
)
{
    if (spec.mode == ScenarioMode::Stale)
    {
        if (result.status != Planner::Status::StaleHold ||
            result.candidatesExamined != 0)
        {
            throw std::runtime_error(
                std::string("stale scenario contract failed: ") + spec.name
            );
        }
        return;
    }

    if (result.candidatesExamined != spec.candidateCount)
    {
        throw std::runtime_error(
            std::string("candidate examination count mismatch: ") + spec.name
        );
    }

    if (spec.mode == ScenarioMode::Conflict)
    {
        if (result.status != Planner::Status::ConflictHold ||
            result.conflictsFound == 0)
        {
            throw std::runtime_error(
                std::string("conflict scenario contract failed: ") + spec.name
            );
        }
    }
    else if (result.status != Planner::Status::Clear ||
             result.conflictsFound != 0)
    {
        throw std::runtime_error(
            std::string("clear scenario contract failed: ") + spec.name
        );
    }
}

Result runScenario(const ScenarioSpec& spec, const Options& options)
{
    const ScenarioData data = buildScenario(spec);
    Planner planner;
    Planner::Result last;
    const std::size_t calls = callsPerSample(spec.candidateCount);

    std::vector<double> samplesUs;
    samplesUs.reserve(static_cast<std::size_t>(options.iterations));

    const int total = options.warmup + options.iterations;
    for (int iteration = 0; iteration < total; ++iteration)
    {
        const auto begin = Clock::now();
        for (std::size_t call = 0; call < calls; ++call)
        {
            last = planner.evaluate(data.query, data.candidates);
            gSink = gSink ^
                static_cast<std::uint64_t>(last.candidatesExamined +
                                           last.conflictsFound +
                                           last.primaryConflictEntityId);
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
    result.p95NsPerCandidate = spec.candidateCount == 0
        ? 0.0
        : result.p95Us * 1000.0 / static_cast<double>(spec.candidateCount);
    result.candidatesExamined = last.candidatesExamined;
    result.conflictsFound = last.conflictsFound;
    result.status = last.status;
    return result;
}

void printResult(const Result& result)
{
    std::cout << std::fixed << std::setprecision(4)
        << "[NavLocalBench] scenario=" << result.scenario
        << " candidates=" << result.candidateCount
        << " calls_per_sample=" << result.callsPerSample
        << " median_us=" << result.medianUs
        << " p95_us=" << result.p95Us
        << " p95_ns_per_candidate=" << result.p95NsPerCandidate
        << " candidates_examined=" << result.candidatesExamined
        << " conflicts_found=" << result.conflictsFound
        << " status=" << statusName(result.status)
        << '\n';
}

void writeCsvHeader(std::ofstream& csv)
{
    csv << "scenario,candidates,calls_per_sample,median_us,p95_us,"
           "p95_ns_per_candidate,candidates_examined,conflicts_found,status\n";
}

void writeCsvRow(std::ofstream& csv, const Result& result)
{
    csv << result.scenario << ','
        << result.candidateCount << ','
        << result.callsPerSample << ','
        << result.medianUs << ','
        << result.p95Us << ','
        << result.p95NsPerCandidate << ','
        << result.candidatesExamined << ','
        << result.conflictsFound << ','
        << statusName(result.status) << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        std::cout << "[NavLocalBench] warmup=" << options.warmup
                  << " iterations=" << options.iterations << '\n';

        const std::vector<ScenarioSpec> scenarios {
            {"clear_0",       0, ScenarioMode::Clear},
            {"clear_16",     16, ScenarioMode::Clear},
            {"clear_64",     64, ScenarioMode::Clear},
            {"clear_256",   256, ScenarioMode::Clear},
            {"clear_1024", 1024, ScenarioMode::Clear},
            {"conflict_16",   16, ScenarioMode::Conflict},
            {"conflict_64",   64, ScenarioMode::Conflict},
            {"conflict_256", 256, ScenarioMode::Conflict},
            {"conflict_1024", 1024, ScenarioMode::Conflict},
            {"stale_1024",  1024, ScenarioMode::Stale}
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

        std::cout << "[NavLocalBench] csv=\"" << output.string() << "\"\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[NavLocalBench][FAIL] " << error.what() << '\n';
        return 1;
    }
}
