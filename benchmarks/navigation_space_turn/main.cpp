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
    int iterations = 3;
    std::filesystem::path output = "navigation_space_turn_benchmark.csv";
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
    double turnPenaltyMetersPerRadian = 50.0;
};

struct ScenarioData
{
    Space::StaticSpaceUpdate update;
    Space::CorridorQuery query;
    Space::CorridorCostPolicy zeroTurn;
    Space::CorridorCostPolicy turnAware;
};

struct Result
{
    std::string scenario;
    std::size_t regions = 0;
    std::size_t portals = 0;
    double zeroMedianMs = 0.0;
    double zeroP95Ms = 0.0;
    double turnMedianMs = 0.0;
    double turnP95Ms = 0.0;
    std::size_t zeroRegionsVisited = 0;
    std::size_t zeroPortalsExamined = 0;
    std::size_t turnRegionsVisited = 0;
    std::size_t turnPortalsExamined = 0;
    std::size_t zeroPathRegions = 0;
    std::size_t turnPathRegions = 0;
    double zeroPathTurnRadians = 0.0;
    double turnPathTurnRadians = 0.0;
    double zeroCost = 0.0;
    double turnCost = 0.0;
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
                << "navigation_space_turn_benchmark [--warmup N] [--iterations N] "
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

Space::Vec3d boundsCenter(const Space::Bounds3d& bounds)
{
    return {
        0.5 * (bounds.minMapMeters.x + bounds.maxMapMeters.x),
        0.5 * (bounds.minMapMeters.y + bounds.maxMapMeters.y),
        0.5 * (bounds.minMapMeters.z + bounds.maxMapMeters.z)
    };
}

double turnAngleRadians(
    const Space::Vec3d& incomingPortal,
    const Space::Vec3d& currentCenter,
    const Space::Vec3d& outgoingPortal
)
{
    const double ax = currentCenter.x - incomingPortal.x;
    const double ay = currentCenter.y - incomingPortal.y;
    const double az = currentCenter.z - incomingPortal.z;
    const double bx = outgoingPortal.x - currentCenter.x;
    const double by = outgoingPortal.y - currentCenter.y;
    const double bz = outgoingPortal.z - currentCenter.z;
    const double aLen = std::sqrt(ax * ax + ay * ay + az * az);
    const double bLen = std::sqrt(bx * bx + by * by + bz * bz);
    if (aLen <= 0.0 || bLen <= 0.0)
        return 0.0;
    const double cosine = std::clamp(
        (ax * bx + ay * by + az * bz) / (aLen * bLen),
        -1.0,
        1.0
    );
    return std::acos(cosine);
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
                         std::size_t bz) {
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
                if (x + 1 < shape.x)
                    addPortal(x, y, z, x + 1, y, z);
                if (y + 1 < shape.y)
                    addPortal(x, y, z, x, y + 1, z);
                if (z + 1 < shape.z)
                    addPortal(x, y, z, x, y, z + 1);
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

    data.zeroTurn.distanceWeight = 1.0;
    data.zeroTurn.preferredClearanceMultiple = 1.0;
    data.zeroTurn.clearancePenaltyMeters = 0.0;
    data.zeroTurn.turnPenaltyMetersPerRadian = 0.0;

    data.turnAware = data.zeroTurn;
    data.turnAware.turnPenaltyMetersPerRadian =
        spec.turnPenaltyMetersPerRadian;

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

double pathTurnRadians(
    const ScenarioData& data,
    const Space::CostedCorridorResult& route
)
{
    if (route.regionPath.size() < 3 ||
        route.portalPath.size() + 1 != route.regionPath.size())
    {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 1; i + 1 < route.regionPath.size(); ++i)
    {
        const auto regionId = route.regionPath[i];
        const auto incomingId = route.portalPath[i - 1];
        const auto outgoingId = route.portalPath[i];
        if (regionId == 0 || regionId > data.update.regions.size() ||
            incomingId == 0 || incomingId > data.update.portals.size() ||
            outgoingId == 0 || outgoingId > data.update.portals.size())
        {
            throw std::runtime_error("turn benchmark path id is out of range");
        }

        total += turnAngleRadians(
            data.update.portals[static_cast<std::size_t>(incomingId - 1)].centerMapMeters,
            boundsCenter(data.update.regions[static_cast<std::size_t>(regionId - 1)].boundsMapMeters),
            data.update.portals[static_cast<std::size_t>(outgoingId - 1)].centerMapMeters
        );
    }
    return total;
}

Result runScenario(const ScenarioSpec& spec, const Options& options)
{
    const ScenarioData data = buildScenario(spec);
    Space space;
    space.replaceStaticWorld(data.update);

    std::vector<double> zeroSamples;
    std::vector<double> turnSamples;
    zeroSamples.reserve(options.iterations);
    turnSamples.reserve(options.iterations);

    Space::CostedCorridorResult zeroResult;
    Space::CostedCorridorResult turnResult;

    const int total = options.warmup + options.iterations;
    for (int i = 0; i < total; ++i)
    {
        const bool measured = i >= options.warmup;

        const double zeroMs = timeCall([&] {
            zeroResult = space.queryCostedCorridor(data.query, data.zeroTurn);
        });
        const double turnMs = timeCall([&] {
            turnResult = space.queryCostedCorridor(data.query, data.turnAware);
        });

        if (measured)
        {
            zeroSamples.push_back(zeroMs);
            turnSamples.push_back(turnMs);
        }
    }

    if (!zeroResult.found || !turnResult.found)
        throw std::runtime_error(
            std::string("turn corridor failed in scenario ") + spec.name
        );

    Result result;
    result.scenario = spec.name;
    result.regions = data.update.regions.size();
    result.portals = data.update.portals.size();
    result.zeroMedianMs = percentile(zeroSamples, 0.50);
    result.zeroP95Ms = percentile(zeroSamples, 0.95);
    result.turnMedianMs = percentile(turnSamples, 0.50);
    result.turnP95Ms = percentile(turnSamples, 0.95);
    result.zeroRegionsVisited = zeroResult.diagnostics.regionsVisited;
    result.zeroPortalsExamined = zeroResult.diagnostics.portalsExamined;
    result.turnRegionsVisited = turnResult.diagnostics.regionsVisited;
    result.turnPortalsExamined = turnResult.diagnostics.portalsExamined;
    result.zeroPathRegions = zeroResult.regionPath.size();
    result.turnPathRegions = turnResult.regionPath.size();
    result.zeroPathTurnRadians = pathTurnRadians(data, zeroResult);
    result.turnPathTurnRadians = pathTurnRadians(data, turnResult);
    result.zeroCost = zeroResult.totalCostMetersEquivalent;
    result.turnCost = turnResult.totalCostMetersEquivalent;
    return result;
}

void printResult(const Result& r)
{
    std::cout << std::fixed << std::setprecision(4)
        << "[NavSpaceTurnBench] scenario=" << r.scenario
        << " regions=" << r.regions
        << " portals=" << r.portals
        << " zero_med_ms=" << r.zeroMedianMs
        << " zero_p95_ms=" << r.zeroP95Ms
        << " turn_med_ms=" << r.turnMedianMs
        << " turn_p95_ms=" << r.turnP95Ms
        << " zero_regions_visited=" << r.zeroRegionsVisited
        << " zero_portals_examined=" << r.zeroPortalsExamined
        << " turn_regions_visited=" << r.turnRegionsVisited
        << " turn_portals_examined=" << r.turnPortalsExamined
        << " zero_path_regions=" << r.zeroPathRegions
        << " turn_path_regions=" << r.turnPathRegions
        << " zero_path_turn_rad=" << r.zeroPathTurnRadians
        << " turn_path_turn_rad=" << r.turnPathTurnRadians
        << " zero_cost=" << r.zeroCost
        << " turn_cost=" << r.turnCost
        << " found=1\n";
}

void writeCsvHeader(std::ofstream& csv)
{
    csv << "scenario,regions,portals,zero_median_ms,zero_p95_ms,"
           "turn_median_ms,turn_p95_ms,zero_regions_visited,"
           "zero_portals_examined,turn_regions_visited,turn_portals_examined,"
           "zero_path_regions,turn_path_regions,zero_path_turn_rad,"
           "turn_path_turn_rad,zero_cost,turn_cost,found\n";
}

void writeCsvRow(std::ofstream& csv, const Result& r)
{
    csv << r.scenario << ',' << r.regions << ',' << r.portals << ','
        << r.zeroMedianMs << ',' << r.zeroP95Ms << ','
        << r.turnMedianMs << ',' << r.turnP95Ms << ','
        << r.zeroRegionsVisited << ',' << r.zeroPortalsExamined << ','
        << r.turnRegionsVisited << ',' << r.turnPortalsExamined << ','
        << r.zeroPathRegions << ',' << r.turnPathRegions << ','
        << r.zeroPathTurnRadians << ',' << r.turnPathTurnRadians << ','
        << r.zeroCost << ',' << r.turnCost << ",1\n";
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        const Options options = parseOptions(argc, argv);
        std::cout << "[NavSpaceTurnBench] warmup=" << options.warmup
                  << " iterations=" << options.iterations << '\n';

        const std::vector<ScenarioSpec> scenarios{
            {"open_1k",  {20, 10, 5},  1200.0, 450.0, 400.0, {30.0, 10.0}, 600.0},
            {"open_5k",  {25, 20, 10}, 1200.0, 450.0, 400.0, {30.0, 10.0}, 600.0},
            {"open_10k", {25, 20, 20}, 1200.0, 450.0, 400.0, {30.0, 10.0}, 600.0},
            {"hub_1k",   {10, 10, 10},   50.0,   8.0,   6.0, {2.0, 1.0},    25.0},
            {"hub_5k",   {20, 25, 10},   50.0,   8.0,   6.0, {2.0, 1.0},    25.0},
            {"hub_10k",  {25, 20, 20},   50.0,   8.0,   6.0, {2.0, 1.0},    25.0}
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

        std::cout << "[NavSpaceTurnBench] csv=\"" << output.string() << "\"\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[NavSpaceTurnBench][FAIL] " << e.what() << '\n';
        return 1;
    }
}
