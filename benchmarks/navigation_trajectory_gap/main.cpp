#include "world/navigation/trajectory/BoundedGapCandidateBuilder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Builder = world::navigation::BoundedGapCandidateBuilder;
using Clock = std::chrono::steady_clock;

volatile std::uint64_t gSink = 0;

struct Options
{
    int warmup = 5;
    int iterations = 30;
};

struct Scenario
{
    const char* name;
    std::size_t neighbors;
    bool denseAccepted;
};

struct Result
{
    std::string name;
    std::size_t neighbors = 0;
    std::size_t candidates = 0;
    std::size_t callsPerSample = 0;
    double medianUs = 0.0;
    double p95Us = 0.0;
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
        else if (arg == "--help" || arg == "-h")
        {
            std::cout << "navigation_trajectory_gap_benchmark [--warmup N] [--iterations N]\n";
            std::exit(0);
        }
        else
            throw std::runtime_error("unknown argument: " + arg);
    }

    if (options.warmup < 0 || options.iterations <= 0)
        throw std::runtime_error("warmup/iterations must be non-negative/positive");
    return options;
}

Builder::ObstacleWitness witness(
    std::uint64_t id,
    double x,
    double y,
    double z,
    double radius
)
{
    Builder::ObstacleWitness value;
    value.obstacleId = id;
    value.snapshotRevision = 42;
    value.centerMapMeters = {x, y, z};
    value.conservativeRadiusMeters = radius;
    return value;
}

Builder::Query makeQuery(const Scenario& scenario)
{
    Builder::Query query;
    query.referencePointMapMeters = {0.0, 0.0, 0.0};
    query.travelDirectionMap = {0.0, 0.0, 1.0};
    query.primary = witness(1, -2.5, 0.0, 20.0, 1.0);

    query.policy.minimumClearSeparationMeters = 0.25;
    query.policy.maximumClearSeparationMeters = 50.0;
    query.policy.secondaryClearanceMeters = 20.0;
    query.policy.minimumForwardDistanceMeters = 0.0;
    query.policy.maximumForwardDistanceMeters = 100.0;
    query.policy.maximumCenterlineOffsetMeters = 50.0;
    query.policy.maximumAbsSeparationTravelDot = 0.25;
    query.policy.maxCandidates = Builder::kHardCandidateLimit;

    query.neighbors.reserve(scenario.neighbors);
    for (std::size_t i = 0; i < scenario.neighbors; ++i)
    {
        const double fi = static_cast<double>(i);
        if (scenario.denseAccepted)
        {
            const double x = 2.5 + std::fmod(fi * 0.37, 12.0);
            const double y = std::fmod(fi * 0.23, 4.0) - 2.0;
            const double z = 20.0 + std::fmod(fi * 0.11, 8.0);
            query.neighbors.push_back(witness(100 + i, x, y, z, 1.0));
        }
        else
        {
            const double x = 80.0 + std::fmod(fi * 7.0, 40.0);
            const double y = 60.0 + std::fmod(fi * 11.0, 30.0);
            const double z = 150.0 + std::fmod(fi * 13.0, 50.0);
            query.neighbors.push_back(witness(100 + i, x, y, z, 1.0));
        }
    }
    return query;
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const std::size_t index = std::min(
        values.size() - 1,
        static_cast<std::size_t>(std::ceil(fraction * values.size())) - 1
    );
    return values[index];
}

std::size_t callsPerSample(std::size_t neighbors)
{
    constexpr std::size_t targetNeighborVisits = 262144;
    return std::clamp<std::size_t>(targetNeighborVisits / std::max<std::size_t>(1, neighbors), 8, 4096);
}

Result runScenario(const Scenario& scenario, const Options& options)
{
    const Builder::Query query = makeQuery(scenario);
    const std::size_t calls = callsPerSample(scenario.neighbors);
    Builder::Result last;
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(options.iterations));

    for (int iteration = 0; iteration < options.warmup + options.iterations; ++iteration)
    {
        const auto begin = Clock::now();
        for (std::size_t call = 0; call < calls; ++call)
        {
            last = Builder::build(query);
            gSink ^= static_cast<std::uint64_t>(
                last.candidates.size() + last.diagnostics.neighborsExamined
            );
        }
        const auto end = Clock::now();

        if (iteration >= options.warmup)
        {
            const double totalUs = std::chrono::duration<double, std::micro>(end - begin).count();
            samples.push_back(totalUs / static_cast<double>(calls));
        }
    }

    if (!last.validInput || last.diagnostics.neighborsExamined != scenario.neighbors)
        throw std::runtime_error(std::string("scenario contract failed: ") + scenario.name);
    if (last.candidates.size() > Builder::kHardCandidateLimit)
        throw std::runtime_error(std::string("candidate cap failed: ") + scenario.name);

    Result result;
    result.name = scenario.name;
    result.neighbors = scenario.neighbors;
    result.candidates = last.candidates.size();
    result.callsPerSample = calls;
    result.medianUs = percentile(samples, 0.50);
    result.p95Us = percentile(samples, 0.95);
    return result;
}

void printResult(const Result& result)
{
    std::cout << std::fixed << std::setprecision(4)
              << "[NavTrajectoryGapBench] scenario=" << result.name
              << " neighbors=" << result.neighbors
              << " candidates=" << result.candidates
              << " calls_per_sample=" << result.callsPerSample
              << " median_us=" << result.medianUs
              << " p95_us=" << result.p95Us
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        const std::vector<Scenario> scenarios {
            {"reject_16", 16, false},
            {"reject_64", 64, false},
            {"reject_256", 256, false},
            {"reject_1024", 1024, false},
            {"top8_16", 16, true},
            {"top8_64", 64, true},
            {"top8_256", 256, true},
            {"top8_1024", 1024, true},
        };

        for (const Scenario& scenario : scenarios)
            printResult(runScenario(scenario, options));

        std::cout << "NAVIGATION TRAJECTORY GAP BENCHMARK: PASS sink=" << gSink << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY GAP BENCHMARK: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
