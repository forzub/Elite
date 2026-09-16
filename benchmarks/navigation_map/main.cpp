#include "world/navigation/map/NavigationMap.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using Map = world::navigation::NavigationMap;
using Clock = std::chrono::steady_clock;

constexpr std::uint32_t kSeed = 0x51A7C0DEu;

struct Scenario
{
    const char* name = "";
    double spawnHalfExtent = 0.0;
    double maxSpeed = 0.0;
    double maxAcceleration = 0.0;
    double minRadius = 0.0;
    double maxRadius = 0.0;
    Map::Vec3d corridorA {};
    Map::Vec3d corridorB {};
    double corridorRadius = 0.0;
    Map::Vec3d sphereCenter {};
    double sphereRadius = 0.0;
};

struct Options
{
    int warmupIterations = 5;
    int measuredIterations = 30;
    double horizonSeconds = 3.0;
    std::filesystem::path outputPath = "navigation_map_cpu_benchmark.csv";
};

struct Result
{
    std::string scenario;
    std::size_t actorCount = 0;
    double rebuildMedianMs = 0.0;
    double rebuildP95Ms = 0.0;
    double corridorMedianMs = 0.0;
    double corridorP95Ms = 0.0;
    double sphereMedianMs = 0.0;
    double sphereP95Ms = 0.0;
    std::size_t corridorCandidates = 0;
    std::size_t sphereCandidates = 0;
    Map::QueryDiagnostics corridorDiagnostics {};
    Map::QueryDiagnostics sphereDiagnostics {};
    Map::Stats stats {};
};

Map::Vec3d operator*(const Map::Vec3d& v, double s)
{
    return {v.x * s, v.y * s, v.z * s};
}

double lengthSquared(const Map::Vec3d& v)
{
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

Map::Vec3d normalized(const Map::Vec3d& v)
{
    const double len = std::sqrt(lengthSquared(v));
    if (len <= 1.0e-12)
        return {1.0, 0.0, 0.0};
    return v * (1.0 / len);
}

Map::Vec3d randomDirection(std::mt19937& rng)
{
    std::uniform_real_distribution<double> component(-1.0, 1.0);
    for (;;)
    {
        const Map::Vec3d v{component(rng), component(rng), component(rng)};
        const double l2 = lengthSquared(v);
        if (l2 > 1.0e-10 && l2 <= 1.0)
            return normalized(v);
    }
}

std::vector<Map::DynamicActorInput> generateActors(
    std::size_t count,
    const Scenario& scenario,
    std::uint32_t seed
)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> position(
        -scenario.spawnHalfExtent,
        scenario.spawnHalfExtent
    );
    std::uniform_real_distribution<double> speed(0.0, scenario.maxSpeed);
    std::uniform_real_distribution<double> acceleration(0.0, scenario.maxAcceleration);
    std::uniform_real_distribution<double> radius(scenario.minRadius, scenario.maxRadius);

    std::vector<Map::DynamicActorInput> actors;
    actors.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        Map::DynamicActorInput actor;
        actor.entityId = static_cast<Map::EntityId>(i + 1u);
        actor.positionSystemMeters = {
            position(rng), position(rng), position(rng)
        };
        actor.velocitySystemMetersPerSecond = randomDirection(rng) * speed(rng);
        actor.accelerationSystemMetersPerSecond2 =
            randomDirection(rng) * acceleration(rng);
        actor.radiusMeters = radius(rng);
        actor.flags = 0u;
        actor.motionRevision = 1u;
        actors.push_back(actor);
    }

    return actors;
}

double percentile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0.0;

    std::sort(values.begin(), values.end());
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    const std::size_t index = static_cast<std::size_t>(std::ceil(
        clamped * static_cast<double>(values.size())
    )) - (clamped > 0.0 ? 1u : 0u);
    return values[std::min(index, values.size() - 1)];
}

template <typename Function>
double timedMs(Function&& function)
{
    const auto begin = Clock::now();
    function();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto requireValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + name);
            return argv[++i];
        };

        if (arg == "--warmup")
            options.warmupIterations = std::stoi(requireValue("--warmup"));
        else if (arg == "--iterations")
            options.measuredIterations = std::stoi(requireValue("--iterations"));
        else if (arg == "--horizon")
            options.horizonSeconds = std::stod(requireValue("--horizon"));
        else if (arg == "--output")
            options.outputPath = requireValue("--output");
        else if (arg == "--help" || arg == "-h")
        {
            std::cout
                << "navigation_map_benchmark [--warmup N] [--iterations N] "
                   "[--horizon seconds] [--output file.csv]\n";
            std::exit(0);
        }
        else
            throw std::runtime_error("unknown argument: " + arg);
    }

    if (options.warmupIterations < 0 || options.measuredIterations <= 0)
        throw std::runtime_error("iteration counts must be non-negative / positive");
    if (!(options.horizonSeconds > 0.0))
        throw std::runtime_error("horizon must be positive");
    return options;
}

Map::DynamicWorldUpdate makeUpdate(
    const std::vector<Map::DynamicActorInput>& actors,
    Map::Revision sourceRevision
)
{
    Map::DynamicWorldUpdate update;
    update.sourceRevision = sourceRevision;
    update.workingFrame.originSystemMeters = {1.0e12, -2.0e12, 3.0e12};
    update.actors = actors;

    for (auto& actor : update.actors)
    {
        actor.positionSystemMeters.x += update.workingFrame.originSystemMeters.x;
        actor.positionSystemMeters.y += update.workingFrame.originSystemMeters.y;
        actor.positionSystemMeters.z += update.workingFrame.originSystemMeters.z;
    }

    return update;
}

Result runScenario(
    const Scenario& scenario,
    std::size_t actorCount,
    const Options& options
)
{
    Map::Config config;
    config.halfExtentMeters = 12000.0;
    config.cellSizeMeters = 600.0;
    config.predictionHorizonSeconds = options.horizonSeconds;
    config.interactionMarginMeters = 60.0;

    const auto actors = generateActors(
        actorCount,
        scenario,
        kSeed ^ static_cast<std::uint32_t>(actorCount) ^
            static_cast<std::uint32_t>(scenario.name[0])
    );

    Map map(config);
    Map::CorridorQuery corridor;
    corridor.startMapMeters = scenario.corridorA;
    corridor.endMapMeters = scenario.corridorB;
    corridor.radiusMeters = scenario.corridorRadius;

    Map::SphereQuery sphere;
    sphere.centerMapMeters = scenario.sphereCenter;
    sphere.radiusMeters = scenario.sphereRadius;

    Map::Revision revision = 1u;
    for (int i = 0; i < options.warmupIterations; ++i)
    {
        map.replaceDynamicWorld(makeUpdate(actors, revision++));
        (void)map.queryCorridor(corridor);
        (void)map.querySphere(sphere);
    }

    std::vector<double> rebuildMs;
    std::vector<double> corridorMs;
    std::vector<double> sphereMs;
    rebuildMs.reserve(options.measuredIterations);
    corridorMs.reserve(options.measuredIterations);
    sphereMs.reserve(options.measuredIterations);

    Map::QueryResult lastCorridor;
    Map::QueryResult lastSphere;

    for (int i = 0; i < options.measuredIterations; ++i)
    {
        Map::DynamicWorldUpdate update = makeUpdate(actors, revision++);
        rebuildMs.push_back(timedMs([&] {
            map.replaceDynamicWorld(std::move(update));
        }));

        corridorMs.push_back(timedMs([&] {
            lastCorridor = map.queryCorridor(corridor);
        }));

        sphereMs.push_back(timedMs([&] {
            lastSphere = map.querySphere(sphere);
        }));
    }

    Result result;
    result.scenario = scenario.name;
    result.actorCount = actorCount;
    result.rebuildMedianMs = percentile(rebuildMs, 0.50);
    result.rebuildP95Ms = percentile(rebuildMs, 0.95);
    result.corridorMedianMs = percentile(corridorMs, 0.50);
    result.corridorP95Ms = percentile(corridorMs, 0.95);
    result.sphereMedianMs = percentile(sphereMs, 0.50);
    result.sphereP95Ms = percentile(sphereMs, 0.95);
    result.corridorCandidates = lastCorridor.candidates.size();
    result.sphereCandidates = lastSphere.candidates.size();
    result.corridorDiagnostics = lastCorridor.diagnostics;
    result.sphereDiagnostics = lastSphere.diagnostics;
    result.stats = map.stats();
    return result;
}

void printResult(const Result& r)
{
    std::cout << std::fixed << std::setprecision(4)
              << "[NavMapCpuBench] scenario=" << r.scenario
              << " actors=" << r.actorCount
              << " rebuild_med_ms=" << r.rebuildMedianMs
              << " rebuild_p95_ms=" << r.rebuildP95Ms
              << " corridor_med_ms=" << r.corridorMedianMs
              << " corridor_p95_ms=" << r.corridorP95Ms
              << " sphere_med_ms=" << r.sphereMedianMs
              << " sphere_p95_ms=" << r.sphereP95Ms
              << " corridor_candidates=" << r.corridorCandidates
              << " corridor_examined=" << r.corridorDiagnostics.actorsExamined
              << " sphere_candidates=" << r.sphereCandidates
              << " sphere_examined=" << r.sphereDiagnostics.actorsExamined
              << " occupied_cells=" << r.stats.occupiedCellCount
              << " out_of_bounds=" << r.stats.outOfBoundsActorCount
              << " rejected=" << r.stats.rejectedActorCount
              << '\n';
}

void writeHeader(std::ofstream& csv)
{
    csv << "scenario,actors,rebuild_median_ms,rebuild_p95_ms,"
           "corridor_median_ms,corridor_p95_ms,sphere_median_ms,sphere_p95_ms,"
           "corridor_candidates,corridor_grid_cells,corridor_occupied_cells,"
           "corridor_actors_examined,sphere_candidates,sphere_grid_cells,"
           "sphere_occupied_cells,sphere_actors_examined,indexed_actors,"
           "occupied_cells,out_of_bounds,rejected,max_sweep_radius_m\n";
}

void writeRow(std::ofstream& csv, const Result& r)
{
    csv << r.scenario << ','
        << r.actorCount << ','
        << r.rebuildMedianMs << ',' << r.rebuildP95Ms << ','
        << r.corridorMedianMs << ',' << r.corridorP95Ms << ','
        << r.sphereMedianMs << ',' << r.sphereP95Ms << ','
        << r.corridorCandidates << ','
        << r.corridorDiagnostics.gridCellsVisited << ','
        << r.corridorDiagnostics.occupiedCellsVisited << ','
        << r.corridorDiagnostics.actorsExamined << ','
        << r.sphereCandidates << ','
        << r.sphereDiagnostics.gridCellsVisited << ','
        << r.sphereDiagnostics.occupiedCellsVisited << ','
        << r.sphereDiagnostics.actorsExamined << ','
        << r.stats.indexedActorCount << ','
        << r.stats.occupiedCellCount << ','
        << r.stats.outOfBoundsActorCount << ','
        << r.stats.rejectedActorCount << ','
        << r.stats.maxConservativeSweptRadiusMeters << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        const std::array<Scenario, 2> scenarios{{
            {
                "cruise", 9000.0, 250.0, 8.0, 4.0, 28.0,
                {0.0, 0.0, 0.0}, {9000.0, 0.0, 0.0}, 600.0,
                {0.0, 0.0, 0.0}, 1200.0
            },
            {
                "hub", 3500.0, 120.0, 6.0, 3.0, 24.0,
                {0.0, 0.0, 0.0}, {5000.0, 0.0, 0.0}, 350.0,
                {0.0, 0.0, 0.0}, 900.0
            }
        }};
        const std::array<std::size_t, 3> actorCounts{{1000u, 5000u, 10000u}};

        const std::filesystem::path absoluteOutput =
            std::filesystem::absolute(options.outputPath);
        std::ofstream csv(absoluteOutput, std::ios::out | std::ios::trunc);
        if (!csv)
            throw std::runtime_error("failed to open CSV output: " + absoluteOutput.string());
        writeHeader(csv);

        std::cout << "[NavMapCpuBench] horizon_s=" << options.horizonSeconds
                  << " warmup=" << options.warmupIterations
                  << " iterations=" << options.measuredIterations << '\n';

        for (const auto& scenario : scenarios)
        {
            for (const std::size_t actorCount : actorCounts)
            {
                const Result result = runScenario(scenario, actorCount, options);
                printResult(result);
                writeRow(csv, result);
            }
        }

        csv.flush();
        std::cout << "[NavMapCpuBench] csv=" << absoluteOutput.string() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[NavMapCpuBench] ERROR: " << error.what() << '\n';
        return 1;
    }
}
