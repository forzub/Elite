#include "world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h"

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

using Evaluator = world::navigation::ContinuousPassageTrajectoryEvaluator;
using Clock = std::chrono::steady_clock;

constexpr double kPi = 3.141592653589793238462643383279502884;
volatile std::uint64_t gSink = 0;

struct Options
{
    int warmup = 8;
    int iterations = 50;
};

struct Scenario
{
    std::string name;
    std::vector<Evaluator::Query> queries;
    std::vector<Evaluator::Status> expectedStatuses;
};

struct Result
{
    std::string name;
    std::size_t queriesPerBatch = 0;
    std::size_t callsPerSample = 0;
    double medianBatchUs = 0.0;
    double p95BatchUs = 0.0;
    double medianPerQueryUs = 0.0;
    double p95PerQueryUs = 0.0;
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
            std::cout
                << "navigation_trajectory_continuous_benchmark "
                << "[--warmup N] [--iterations N]\n";
            std::exit(0);
        }
        else
        {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.warmup < 0 || options.iterations <= 0)
        throw std::runtime_error("warmup/iterations must be non-negative/positive");
    return options;
}

Evaluator::Basis3d rolled90()
{
    Evaluator::Basis3d basis;
    basis.right = {0.0, 1.0, 0.0};
    basis.up = {-1.0, 0.0, 0.0};
    basis.forward = {0.0, 0.0, 1.0};
    return basis;
}

Evaluator::Passage zPassage(double halfWidth, double halfHeight)
{
    Evaluator::Passage passage;
    passage.centerMapMeters = {0.0, 0.0, 0.0};
    passage.halfWidthMeters = halfWidth;
    passage.halfHeightMeters = halfHeight;
    return passage;
}

Evaluator::Query straightNewton()
{
    Evaluator::Query query;
    query.hull.halfExtentsBodyMeters = {1.0, 0.5, 2.0};
    query.passage = zPassage(2.0, 2.0);
    query.start.pose.centerMapMeters = {0.0, 0.0, -5.0};
    query.end.pose.centerMapMeters = {0.0, 0.0, 5.0};
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 5.0};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 5.0};
    query.durationSeconds = 2.0;
    query.controlMode = Evaluator::ControlMode::Newtonian;
    return query;
}

Evaluator::Query rolledNewton(bool blocked)
{
    Evaluator::Query query = straightNewton();
    query.hull.halfExtentsBodyMeters = {3.0, 1.0, 1.0};
    query.passage = zPassage(blocked ? 3.05 : 3.60, 10.0);
    query.end.pose.bodyToMap = rolled90();
    query.durationSeconds = 4.0;
    query.start.linearVelocityMapMetersPerSec = {0.0, 0.0, 2.5};
    query.end.linearVelocityMapMetersPerSec = {0.0, 0.0, 2.5};
    query.angularCapability.maxAngularAccelerationRadPerSec2 = 10.0;
    query.angularCapability.maxAngularSpeedRadPerSec = 10.0;
    return query;
}

Evaluator::Query lateralNewton()
{
    Evaluator::Query query = straightNewton();
    query.passage = zPassage(20.0, 20.0);
    query.start.pose.centerMapMeters.x = 1.0;
    query.end.pose.centerMapMeters.x = 0.0;
    query.linearCapability.maxForwardAccelerationMetersPerSec2 = 5.0;
    query.linearCapability.maxReverseAccelerationMetersPerSec2 = 5.0;
    query.linearCapability.maxLateralAccelerationMetersPerSec2 = 2.0;
    query.linearCapability.maxVerticalAccelerationMetersPerSec2 = 5.0;
    return query;
}

Evaluator::Query eliteAligned()
{
    Evaluator::Query query = straightNewton();
    query.controlMode = Evaluator::ControlMode::EliteAssisted;
    query.assistedMaxVelocityToForwardAngleRad = 10.0 * kPi / 180.0;
    return query;
}

Scenario single(
    const char* name,
    const Evaluator::Query& query,
    Evaluator::Status expected
)
{
    Scenario scenario;
    scenario.name = name;
    scenario.queries.push_back(query);
    scenario.expectedStatuses.push_back(expected);
    return scenario;
}

Scenario fullPrecisionBatch8()
{
    Scenario scenario;
    scenario.name = "full_precision_batch8";
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        scenario.queries.push_back(straightNewton());
        scenario.expectedStatuses.push_back(Evaluator::Status::Feasible);

        scenario.queries.push_back(rolledNewton(false));
        scenario.expectedStatuses.push_back(Evaluator::Status::Feasible);

        scenario.queries.push_back(lateralNewton());
        scenario.expectedStatuses.push_back(Evaluator::Status::Feasible);

        scenario.queries.push_back(eliteAligned());
        scenario.expectedStatuses.push_back(Evaluator::Status::Feasible);
    }
    return scenario;
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

std::size_t callsPerSample(std::size_t queriesPerBatch)
{
    constexpr std::size_t targetEvaluations = 2048;
    return std::clamp<std::size_t>(
        targetEvaluations / std::max<std::size_t>(1, queriesPerBatch),
        8,
        2048
    );
}

void validateScenario(const Scenario& scenario)
{
    if (scenario.queries.empty() ||
        scenario.queries.size() != scenario.expectedStatuses.size())
    {
        throw std::runtime_error("invalid scenario definition: " + scenario.name);
    }

    for (std::size_t i = 0; i < scenario.queries.size(); ++i)
    {
        const Evaluator::Result result = Evaluator::evaluate(scenario.queries[i]);
        if (result.status != scenario.expectedStatuses[i])
            throw std::runtime_error("scenario status mismatch: " + scenario.name);

        if (result.status != Evaluator::Status::AngularAuthorityExceeded &&
            result.status != Evaluator::Status::InvalidInput)
        {
            if (result.samplesEvaluated != Evaluator::kPoseSamples ||
                result.intervalsProven + 1 != Evaluator::kPoseSamples)
            {
                throw std::runtime_error(
                    "scenario did not exercise full continuous verifier: " +
                    scenario.name
                );
            }
        }
    }
}

Result runScenario(const Scenario& scenario, const Options& options)
{
    validateScenario(scenario);
    const std::size_t calls = callsPerSample(scenario.queries.size());
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(options.iterations));

    for (int iteration = 0; iteration < options.warmup + options.iterations; ++iteration)
    {
        const auto begin = Clock::now();
        for (std::size_t call = 0; call < calls; ++call)
        {
            for (const Evaluator::Query& query : scenario.queries)
            {
                const Evaluator::Result result = Evaluator::evaluate(query);
                gSink ^= static_cast<std::uint64_t>(result.status);
                gSink += static_cast<std::uint64_t>(
                    result.samplesEvaluated + 17 * result.intervalsProven
                );
            }
        }
        const auto end = Clock::now();

        if (iteration >= options.warmup)
        {
            const double totalUs =
                std::chrono::duration<double, std::micro>(end - begin).count();
            samples.push_back(totalUs / static_cast<double>(calls));
        }
    }

    Result result;
    result.name = scenario.name;
    result.queriesPerBatch = scenario.queries.size();
    result.callsPerSample = calls;
    result.medianBatchUs = percentile(samples, 0.50);
    result.p95BatchUs = percentile(samples, 0.95);
    result.medianPerQueryUs =
        result.medianBatchUs / static_cast<double>(result.queriesPerBatch);
    result.p95PerQueryUs =
        result.p95BatchUs / static_cast<double>(result.queriesPerBatch);
    return result;
}

void printResult(const Result& result)
{
    std::cout << std::fixed << std::setprecision(4)
              << "[NavTrajectoryContinuousBench] scenario=" << result.name
              << " queries_per_batch=" << result.queriesPerBatch
              << " calls_per_sample=" << result.callsPerSample
              << " median_batch_us=" << result.medianBatchUs
              << " p95_batch_us=" << result.p95BatchUs
              << " median_per_query_us=" << result.medianPerQueryUs
              << " p95_per_query_us=" << result.p95PerQueryUs
              << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        const std::vector<Scenario> scenarios {
            single(
                "straight_newton",
                straightNewton(),
                Evaluator::Status::Feasible
            ),
            single(
                "rolled_newton",
                rolledNewton(false),
                Evaluator::Status::Feasible
            ),
            single(
                "lateral_newton",
                lateralNewton(),
                Evaluator::Status::Feasible
            ),
            single(
                "elite_aligned",
                eliteAligned(),
                Evaluator::Status::Feasible
            ),
            single(
                "geometry_blocked_roll",
                rolledNewton(true),
                Evaluator::Status::GeometryBlocked
            ),
            fullPrecisionBatch8(),
        };

        for (const Scenario& scenario : scenarios)
            printResult(runScenario(scenario, options));

        std::cout
            << "NAVIGATION TRAJECTORY CONTINUOUS BENCHMARK: PASS sink="
            << gSink << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "NAVIGATION TRAJECTORY CONTINUOUS BENCHMARK: FAIL: "
            << error.what() << '\n';
        return 1;
    }
}
