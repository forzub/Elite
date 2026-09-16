#include "src/world/navigation/space/NavigationSpace.h"

#include <algorithm>
#include <chrono>
#include <cmath>
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
using Space = world::navigation::NavigationSpace;
using Clock = std::chrono::steady_clock;

struct Options
{
    int warmup = 1;
    int iterations = 5;
    std::filesystem::path output = "navigation_space_costed_benchmark.csv";
};

struct GridShape
{
    std::size_t x = 1;
    std::size_t y = 1;
    std::size_t z = 1;
};

struct ScenarioSpec
{
    const char* name = "";
    GridShape shape {};
    double cellSizeMeters = 100.0;
    double regionClearanceMeters = 20.0;
    double portalClearanceMeters = 15.0;
    Space::AgentEnvelope envelope {};
    double preferredClearanceMultiple = 2.0;
};

struct ScenarioData
{
    Space::StaticSpaceUpdate update;
    Space::CorridorQuery query;
    Space::CorridorCostPolicy distanceOnly;
    Space::CorridorCostPolicy clearanceAware;
};

struct Result
{
    std::string scenario;
    std::size_t regions = 0;
    std::size_t portals = 0;
    double distanceMedianMs = 0.0;
    double distanceP95Ms = 0.0;
    double clearanceMedianMs = 0.0;
    double clearanceP95Ms = 0.0;
    std::size_t distanceRegionsVisited = 0;
    std::size_t distancePortalsExamined = 0;
    std::size_t clearanceRegionsVisited = 0;
    std::size_t clearancePortalsExamined = 0;
    std::size_t distancePathRegions = 0;
    std::size_t clearancePathRegions = 0;
    double distanceCost = 0.0;
    double clearanceCost = 0.0;
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
                << "navigation_space_costed_benchmark [--warmup N] [--iterations N] "
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

std::size_t regionIndex(
    const GridShape& shape,
    std::size_t x,
    std::size_t y,
    std::size_t z
)
{
    return (z * shape.y + y) * shape.x + x;
}

Space::Vec3d regionCenter(
    const ScenarioSpec& spec,
    std::size_t x,
    std::size_t y,
    std::size_t z
)
{
    const double s = spec.cellSizeMeters;
    return {
        (static_cast<double>(x) + 0.5) * s,
        (static_cast<double>(y) + 0.5) * s,
        (static_cast<double>(z) + 0.5) * s
    };
}

Space::Bounds3d regionBounds(
    const ScenarioSpec& spec,
    std::size_t x,
    std::size_t y,
    std::size_t z
)
{
    const double s = spec.cellSizeMeters;
    return {
        {static_cast<double>(x) * s,
         static_cast<double>(y) * s,
         static_cast<double>(z) * s},
        {static_cast<double>(x + 1) * s,
         static_cast<double>(y + 1) * s,
         static_cast<double>(z + 1) * s}
    };
}

bool narrowPortal(
    std::size_t x,
    std::size_t y,
    std::size_t z,
    std::size_t axis
)
{
    return ((x + 3 * y + 5 * z + 7 * axis) % 5) == 0;
}

ScenarioData buildScenario(const ScenarioSpec& spec)
{
    ScenarioData data;
    data.update.sourceRevision = 1;

    const auto& shape = spec.shape;
    const std::size_t regionCount = shape.x * shape.y * shape.z;
    data.update.regions.reserve(regionCount);
    data.update.portals.reserve(regionCount * 3);

    for (std::size_t z = 0; z < shape.z; ++z)
    {
        for (std::size_t y = 0; y < shape.y; ++y)
        {
            for (std::size_t x = 0; x < shape.x; ++x)
            {
                Space::RegionInput region;
                region.regionId = static_cast<Space::RegionId>(
                    regionIndex(shape, x, y, z) + 1
                );
                region.boundsMapMeters = regionBounds(spec, x, y, z);
                region.clearanceRadiusMeters = spec.regionClearanceMeters;
                region.geometryRevision = 1;
                data.update.regions.push_back(region);
            }
        }
    }

    Space::PortalId nextPortal = 1;
    auto addPortal = [&](std::size_t ax,
                         std::size_t ay,
                         std::size_t az,
                         std::size_t bx,
                         std::size_t by,
                         std::size_t bz,
                         std::size_t axis) {
        const Space::Vec3d a = regionCenter(spec, ax, ay, az);
        const Space::Vec3d b = regionCenter(spec, bx, by, bz);
        Space::PortalInput portal;
        portal.portalId = nextPortal++;
        portal.regionA = static_cast<Space::RegionId>(
            regionIndex(shape, ax, ay, az) + 1
        );
        portal.regionB = static_cast<Space::RegionId>(
            regionIndex(shape, bx, by, bz) + 1
        );
        portal.centerMapMeters = {
            0.5 * (a.x + b.x),
            0.5 * (a.y + b.y),
            0.5 * (a.z + b.z)
        };
        portal.clearanceRadiusMeters = spec.portalClearanceMeters *
            (narrowPortal(ax, ay, az, axis) ? 0.55 : 1.0);
        portal.bidirectional = true;
        portal.geometryRevision = 1;
        data.update.portals.push_back(portal);
    };

    for (std::size_t z = 0; z < shape.z; ++z)
    {
        for (std::size_t y = 0; y < shape.y; ++y)
        {
            for (std::size_t x = 0; x < shape.x; ++x)
            {
                if (x + 1 < shape.x)
                    addPortal(x, y, z, x + 1, y, z, 0);
                if (y + 1 < shape.y)
                    addPortal(x, y, z, x, y + 1, z, 1);
                if (z + 1 < shape.z)
                    addPortal(x, y, z, x, y, z + 1, 2);
            }
        }
    }

    data.query.startMapMeters = regionCenter(spec, 0, 0, 0);
    data.query.endMapMeters = regionCenter(
        spec,
        shape.x - 1,
        shape.y - 1,
        shape.z - 1
    );
    data.query.envelope = spec.envelope;

    data.distanceOnly.distanceWeight = 1.0;
    data.distanceOnly.preferredClearanceMultiple = 1.0;
    data.distanceOnly.clearancePenaltyMeters = 0.0;

    data.clearanceAware.distanceWeight = 1.0;
    data.clearanceAware.preferredClearanceMultiple =
        spec.preferredClearanceMultiple;
    data.clearanceAware.clearancePenaltyMeters = 2.0 * spec.cellSizeMeters;

    return data;
}

double milliseconds(Clock::time_point begin, Clock::time_point end)
{
    return std::chrono::duration<double, std::milli>(end - begin).count();
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

template <typename F>
double timeCall(F&& fn)
{
    const auto begin = Clock::now();
    fn();
    const auto end = Clock::now();
    return milliseconds(begin, end);
}

Result runScenario(const ScenarioSpec& spec, const Options& options)
{
    const ScenarioData data = buildScenario(spec);
    Space space;
    space.replaceStaticWorld(data.update);

    std::vector<double> distanceSamples;
    std::vector<double> clearanceSamples;
    distanceSamples.reserve(options.iterations);
    clearanceSamples.reserve(options.iterations);

    Space::CostedCorridorResult distanceResult;
    Space::CostedCorridorResult clearanceResult;

    const int total = options.warmup + options.iterations;
    for (int i = 0; i < total; ++i)
    {
        const bool measured = i >= options.warmup;

        const double distanceMs = timeCall([&] {
            distanceResult = space.queryCostedCorridor(
                data.query,
                data.distanceOnly
            );
        });

        const double clearanceMs = timeCall([&] {
            clearanceResult = space.queryCostedCorridor(
                data.query,
                data.clearanceAware
            );
        });

        if (measured)
        {
            distanceSamples.push_back(distanceMs);
            clearanceSamples.push_back(clearanceMs);
        }
    }

    if (!distanceResult.found || !clearanceResult.found)
        throw std::runtime_error(
            std::string("costed corridor failed in scenario ") + spec.name
        );

    Result result;
    result.scenario = spec.name;
    result.regions = data.update.regions.size();
    result.portals = data.update.portals.size();
    result.distanceMedianMs = percentile(distanceSamples, 0.50);
    result.distanceP95Ms = percentile(distanceSamples, 0.95);
    result.clearanceMedianMs = percentile(clearanceSamples, 0.50);
    result.clearanceP95Ms = percentile(clearanceSamples, 0.95);
    result.distanceRegionsVisited = distanceResult.diagnostics.regionsVisited;
    result.distancePortalsExamined = distanceResult.diagnostics.portalsExamined;
    result.clearanceRegionsVisited = clearanceResult.diagnostics.regionsVisited;
    result.clearancePortalsExamined = clearanceResult.diagnostics.portalsExamined;
    result.distancePathRegions = distanceResult.regionPath.size();
    result.clearancePathRegions = clearanceResult.regionPath.size();
    result.distanceCost = distanceResult.totalCostMetersEquivalent;
    result.clearanceCost = clearanceResult.totalCostMetersEquivalent;
    return result;
}

void printResult(const Result& r)
{
    std::cout << std::fixed << std::setprecision(4)
        << "[NavSpaceCostedBench] scenario=" << r.scenario
        << " regions=" << r.regions
        << " portals=" << r.portals
        << " distance_med_ms=" << r.distanceMedianMs
        << " distance_p95_ms=" << r.distanceP95Ms
        << " clearance_med_ms=" << r.clearanceMedianMs
        << " clearance_p95_ms=" << r.clearanceP95Ms
        << " distance_regions_visited=" << r.distanceRegionsVisited
        << " distance_portals_examined=" << r.distancePortalsExamined
        << " clearance_regions_visited=" << r.clearanceRegionsVisited
        << " clearance_portals_examined=" << r.clearancePortalsExamined
        << " distance_path_regions=" << r.distancePathRegions
        << " clearance_path_regions=" << r.clearancePathRegions
        << " distance_cost=" << r.distanceCost
        << " clearance_cost=" << r.clearanceCost
        << " found=1\n";
}

void writeCsvHeader(std::ofstream& csv)
{
    csv << "scenario,regions,portals,distance_median_ms,distance_p95_ms,"
           "clearance_median_ms,clearance_p95_ms,distance_regions_visited,"
           "distance_portals_examined,clearance_regions_visited,"
           "clearance_portals_examined,distance_path_regions,"
           "clearance_path_regions,distance_cost,clearance_cost,found\n";
}

void writeCsvRow(std::ofstream& csv, const Result& r)
{
    csv << r.scenario << ',' << r.regions << ',' << r.portals << ','
        << r.distanceMedianMs << ',' << r.distanceP95Ms << ','
        << r.clearanceMedianMs << ',' << r.clearanceP95Ms << ','
        << r.distanceRegionsVisited << ',' << r.distancePortalsExamined << ','
        << r.clearanceRegionsVisited << ',' << r.clearancePortalsExamined << ','
        << r.distancePathRegions << ',' << r.clearancePathRegions << ','
        << r.distanceCost << ',' << r.clearanceCost << ",1\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        std::cout << "[NavSpaceCostedBench] warmup=" << options.warmup
                  << " iterations=" << options.iterations << '\n';

        const std::vector<ScenarioSpec> scenarios{
            {"open_1k",  {20, 10, 5},  1200.0, 450.0, 400.0, {30.0, 10.0}, 8.0},
            {"open_5k",  {25, 20, 10}, 1200.0, 450.0, 400.0, {30.0, 10.0}, 8.0},
            {"open_10k", {25, 20, 20}, 1200.0, 450.0, 400.0, {30.0, 10.0}, 8.0},
            {"hub_1k",   {10, 10, 10},   50.0,   8.0,   6.0, {2.0, 1.0},   2.0},
            {"hub_5k",   {20, 25, 10},   50.0,   8.0,   6.0, {2.0, 1.0},   2.0},
            {"hub_10k",  {25, 20, 20},   50.0,   8.0,   6.0, {2.0, 1.0},   2.0}
        };

        const std::filesystem::path output = std::filesystem::absolute(options.output);
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

        std::cout << "[NavSpaceCostedBench] csv=\""
                  << output.string() << "\"\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[NavSpaceCostedBench][FAIL] " << e.what() << '\n';
        return 1;
    }
}
