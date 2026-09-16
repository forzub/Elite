#include "src/world/navigation/space/NavigationSpace.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
using Space = world::navigation::NavigationSpace;
using Clock = std::chrono::steady_clock;

struct Options
{
    int warmup = 1;
    int iterations = 5;
    std::filesystem::path output = "navigation_space_cpu_benchmark.csv";
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
};

struct ScenarioData
{
    Space::StaticSpaceUpdate update;
    Space::PointQuery pointQuery;
    Space::CorridorQuery corridorQuery;
    Space::Bounds3d invalidationBounds;
    Space::LocalPatch patch;
};

struct Samples
{
    std::vector<double> replaceMs;
    std::vector<double> pointMs;
    std::vector<double> corridorMs;
    std::vector<double> invalidateMs;
    std::vector<double> patchMs;
};

struct Result
{
    std::string scenario;
    std::size_t regions = 0;
    std::size_t portals = 0;
    double replaceMedianMs = 0.0;
    double replaceP95Ms = 0.0;
    double pointMedianMs = 0.0;
    double pointP95Ms = 0.0;
    double corridorMedianMs = 0.0;
    double corridorP95Ms = 0.0;
    double invalidateMedianMs = 0.0;
    double invalidateP95Ms = 0.0;
    double patchMedianMs = 0.0;
    double patchP95Ms = 0.0;
    std::size_t pointRegionsExamined = 0;
    std::size_t corridorRegionsVisited = 0;
    std::size_t corridorPortalsExamined = 0;
    std::size_t invalidatedRegions = 0;
    std::size_t invalidatedPortals = 0;
    bool corridorFound = false;
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
                << "navigation_space_benchmark [--warmup N] [--iterations N] "
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

std::size_t regionIndex(const GridShape& shape, std::size_t x, std::size_t y, std::size_t z)
{
    return (z * shape.y + y) * shape.x + x;
}

Space::Vec3d regionCenter(const ScenarioSpec& spec, std::size_t x, std::size_t y, std::size_t z)
{
    const double s = spec.cellSizeMeters;
    return {
        (static_cast<double>(x) + 0.5) * s,
        (static_cast<double>(y) + 0.5) * s,
        (static_cast<double>(z) + 0.5) * s
    };
}

Space::Bounds3d regionBounds(const ScenarioSpec& spec, std::size_t x, std::size_t y, std::size_t z)
{
    const double s = spec.cellSizeMeters;
    return {
        {static_cast<double>(x) * s, static_cast<double>(y) * s, static_cast<double>(z) * s},
        {static_cast<double>(x + 1) * s, static_cast<double>(y + 1) * s, static_cast<double>(z + 1) * s}
    };
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
                region.regionId = static_cast<Space::RegionId>(regionIndex(shape, x, y, z) + 1);
                region.boundsMapMeters = regionBounds(spec, x, y, z);
                region.clearanceRadiusMeters = spec.regionClearanceMeters;
                region.geometryRevision = 1;
                data.update.regions.push_back(region);
            }
        }
    }

    Space::PortalId nextPortal = 1;
    auto addPortal = [&](std::size_t ax, std::size_t ay, std::size_t az,
                         std::size_t bx, std::size_t by, std::size_t bz) {
        const Space::Vec3d a = regionCenter(spec, ax, ay, az);
        const Space::Vec3d b = regionCenter(spec, bx, by, bz);
        Space::PortalInput portal;
        portal.portalId = nextPortal++;
        portal.regionA = static_cast<Space::RegionId>(regionIndex(shape, ax, ay, az) + 1);
        portal.regionB = static_cast<Space::RegionId>(regionIndex(shape, bx, by, bz) + 1);
        portal.centerMapMeters = {
            0.5 * (a.x + b.x),
            0.5 * (a.y + b.y),
            0.5 * (a.z + b.z)
        };
        portal.clearanceRadiusMeters = spec.portalClearanceMeters;
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
                if (x + 1 < shape.x) addPortal(x, y, z, x + 1, y, z);
                if (y + 1 < shape.y) addPortal(x, y, z, x, y + 1, z);
                if (z + 1 < shape.z) addPortal(x, y, z, x, y, z + 1);
            }
        }
    }

    data.pointQuery.pointMapMeters = regionCenter(spec, shape.x - 1, shape.y - 1, shape.z - 1);
    data.pointQuery.envelope = spec.envelope;

    data.corridorQuery.startMapMeters = regionCenter(spec, 0, 0, 0);
    data.corridorQuery.endMapMeters = data.pointQuery.pointMapMeters;
    data.corridorQuery.envelope = spec.envelope;

    const std::size_t cx = shape.x / 2;
    const std::size_t cy = shape.y / 2;
    const std::size_t cz = shape.z / 2;
    const Space::Vec3d c = regionCenter(spec, cx, cy, cz);
    const double r = 0.2 * spec.cellSizeMeters;
    data.invalidationBounds = {{c.x - r, c.y - r, c.z - r}, {c.x + r, c.y + r, c.z + r}};

    Space::RegionInput patched = data.update.regions[regionIndex(shape, cx, cy, cz)];
    patched.geometryRevision = 2;
    patched.flags = 1;
    data.patch.sourceRevision = 2;
    data.patch.upsertRegions.push_back(patched);

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
    const std::size_t index = static_cast<std::size_t>(std::ceil(
        std::clamp(fraction, 0.0, 1.0) * static_cast<double>(values.size())
    )) - (fraction > 0.0 ? 1u : 0u);
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
    const int totalIterations = options.warmup + options.iterations;
    Samples samples;
    samples.replaceMs.reserve(options.iterations);
    samples.pointMs.reserve(options.iterations);
    samples.corridorMs.reserve(options.iterations);
    samples.invalidateMs.reserve(options.iterations);
    samples.patchMs.reserve(options.iterations);

    Space stable;
    stable.replaceStaticWorld(data.update);

    Space::PointQueryResult pointResult;
    Space::CorridorResult corridorResult;
    Space::InvalidationResult invalidationResult;

    for (int i = 0; i < totalIterations; ++i)
    {
        const bool measured = i >= options.warmup;

        Space replaceSpace;
        auto replaceUpdate = data.update;
        const double replaceMs = timeCall([&] {
            replaceSpace.replaceStaticWorld(std::move(replaceUpdate));
        });

        const double pointMs = timeCall([&] {
            pointResult = stable.queryPoint(data.pointQuery);
        });

        const double corridorMs = timeCall([&] {
            corridorResult = stable.queryCorridor(data.corridorQuery);
        });

        Space invalidateSpace;
        invalidateSpace.replaceStaticWorld(data.update);
        const double invalidateMs = timeCall([&] {
            invalidationResult = invalidateSpace.invalidateBounds(data.invalidationBounds, 2);
        });

        Space patchSpace;
        patchSpace.replaceStaticWorld(data.update);
        auto patch = data.patch;
        const double patchMs = timeCall([&] {
            patchSpace.applyLocalPatch(std::move(patch));
        });

        if (measured)
        {
            samples.replaceMs.push_back(replaceMs);
            samples.pointMs.push_back(pointMs);
            samples.corridorMs.push_back(corridorMs);
            samples.invalidateMs.push_back(invalidateMs);
            samples.patchMs.push_back(patchMs);
        }
    }

    if (!pointResult.traversable)
        throw std::runtime_error(std::string("point query failed in scenario ") + spec.name);
    if (!corridorResult.found)
        throw std::runtime_error(std::string("corridor query failed in scenario ") + spec.name);

    Result result;
    result.scenario = spec.name;
    result.regions = data.update.regions.size();
    result.portals = data.update.portals.size();
    result.replaceMedianMs = percentile(samples.replaceMs, 0.50);
    result.replaceP95Ms = percentile(samples.replaceMs, 0.95);
    result.pointMedianMs = percentile(samples.pointMs, 0.50);
    result.pointP95Ms = percentile(samples.pointMs, 0.95);
    result.corridorMedianMs = percentile(samples.corridorMs, 0.50);
    result.corridorP95Ms = percentile(samples.corridorMs, 0.95);
    result.invalidateMedianMs = percentile(samples.invalidateMs, 0.50);
    result.invalidateP95Ms = percentile(samples.invalidateMs, 0.95);
    result.patchMedianMs = percentile(samples.patchMs, 0.50);
    result.patchP95Ms = percentile(samples.patchMs, 0.95);
    result.pointRegionsExamined = pointResult.regionsExamined;
    result.corridorRegionsVisited = corridorResult.diagnostics.regionsVisited;
    result.corridorPortalsExamined = corridorResult.diagnostics.portalsExamined;
    result.invalidatedRegions = invalidationResult.invalidatedRegionIds.size();
    result.invalidatedPortals = invalidationResult.invalidatedPortalIds.size();
    result.corridorFound = corridorResult.found;
    return result;
}

void printResult(const Result& r)
{
    std::cout << std::fixed << std::setprecision(4)
        << "[NavSpaceCpuBench] scenario=" << r.scenario
        << " regions=" << r.regions
        << " portals=" << r.portals
        << " replace_med_ms=" << r.replaceMedianMs
        << " replace_p95_ms=" << r.replaceP95Ms
        << " point_med_ms=" << r.pointMedianMs
        << " point_p95_ms=" << r.pointP95Ms
        << " corridor_med_ms=" << r.corridorMedianMs
        << " corridor_p95_ms=" << r.corridorP95Ms
        << " invalidate_med_ms=" << r.invalidateMedianMs
        << " invalidate_p95_ms=" << r.invalidateP95Ms
        << " patch_med_ms=" << r.patchMedianMs
        << " patch_p95_ms=" << r.patchP95Ms
        << " point_regions_examined=" << r.pointRegionsExamined
        << " corridor_regions_visited=" << r.corridorRegionsVisited
        << " corridor_portals_examined=" << r.corridorPortalsExamined
        << " invalidated_regions=" << r.invalidatedRegions
        << " invalidated_portals=" << r.invalidatedPortals
        << " corridor_found=" << (r.corridorFound ? 1 : 0)
        << '\n';
}

void writeCsvHeader(std::ofstream& csv)
{
    csv << "scenario,regions,portals,replace_median_ms,replace_p95_ms,"
           "point_median_ms,point_p95_ms,corridor_median_ms,corridor_p95_ms,"
           "invalidate_median_ms,invalidate_p95_ms,patch_median_ms,patch_p95_ms,"
           "point_regions_examined,corridor_regions_visited,corridor_portals_examined,"
           "invalidated_regions,invalidated_portals,corridor_found\n";
}

void writeCsvRow(std::ofstream& csv, const Result& r)
{
    csv << r.scenario << ',' << r.regions << ',' << r.portals << ','
        << r.replaceMedianMs << ',' << r.replaceP95Ms << ','
        << r.pointMedianMs << ',' << r.pointP95Ms << ','
        << r.corridorMedianMs << ',' << r.corridorP95Ms << ','
        << r.invalidateMedianMs << ',' << r.invalidateP95Ms << ','
        << r.patchMedianMs << ',' << r.patchP95Ms << ','
        << r.pointRegionsExamined << ',' << r.corridorRegionsVisited << ','
        << r.corridorPortalsExamined << ',' << r.invalidatedRegions << ','
        << r.invalidatedPortals << ',' << (r.corridorFound ? 1 : 0) << '\n';
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        std::cout << "[NavSpaceCpuBench] warmup=" << options.warmup
                  << " iterations=" << options.iterations << '\n';

        const std::vector<ScenarioSpec> scenarios{
            {"open_1k",  {20, 10, 5},  1200.0, 450.0, 400.0, {30.0, 10.0}},
            {"open_5k",  {25, 20, 10}, 1200.0, 450.0, 400.0, {30.0, 10.0}},
            {"open_10k", {25, 20, 20}, 1200.0, 450.0, 400.0, {30.0, 10.0}},
            {"hub_1k",   {10, 10, 10},   50.0,   8.0,   6.0, {2.0, 1.0}},
            {"hub_5k",   {20, 25, 10},   50.0,   8.0,   6.0, {2.0, 1.0}},
            {"hub_10k",  {25, 20, 20},   50.0,   8.0,   6.0, {2.0, 1.0}}
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

        std::cout << "[NavSpaceCpuBench] csv=\"" << output.string() << "\"\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[NavSpaceCpuBench][FAIL] " << e.what() << '\n';
        return 1;
    }
}
