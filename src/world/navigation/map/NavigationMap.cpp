#include "world/navigation/map/NavigationMap.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace world::navigation
{
namespace
{

using Vec3d = NavigationMap::Vec3d;

constexpr double kLengthEpsilon = 1.0e-12;

bool isFinite(double value) noexcept
{
    return std::isfinite(value);
}

bool isFinite(const Vec3d& value) noexcept
{
    return isFinite(value.x) && isFinite(value.y) && isFinite(value.z);
}

Vec3d operator+(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3d operator-(const Vec3d& a, const Vec3d& b) noexcept
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3d operator*(const Vec3d& value, double scalar) noexcept
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

double dot(const Vec3d& a, const Vec3d& b) noexcept
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double lengthSquared(const Vec3d& value) noexcept
{
    return dot(value, value);
}

double length(const Vec3d& value) noexcept
{
    return std::sqrt(lengthSquared(value));
}

double distanceSquared(const Vec3d& a, const Vec3d& b) noexcept
{
    return lengthSquared(a - b);
}

double distanceToSegmentSquared(
    const Vec3d& point,
    const Vec3d& segmentStart,
    const Vec3d& segmentEnd
) noexcept
{
    const Vec3d segment = segmentEnd - segmentStart;
    const double denominator = lengthSquared(segment);
    if (denominator <= kLengthEpsilon)
        return distanceSquared(point, segmentStart);

    const double t = std::clamp(
        dot(point - segmentStart, segment) / denominator,
        0.0,
        1.0
    );
    const Vec3d closest = segmentStart + segment * t;
    return distanceSquared(point, closest);
}

struct CellCoord
{
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const CellCoord& other) const noexcept
    {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct CellCoordHash
{
    std::size_t operator()(const CellCoord& value) const noexcept
    {
        std::size_t seed = static_cast<std::size_t>(value.x) * 73856093u;
        seed ^= static_cast<std::size_t>(value.y) * 19349663u;
        seed ^= static_cast<std::size_t>(value.z) * 83492791u;
        return seed;
    }
};

} // namespace

class NavigationMap::Impl
{
public:
    explicit Impl(const Config& configValue)
        : config(configValue)
    {
        validateConfig();
        gridDimension = static_cast<int>(std::ceil(
            (2.0 * config.halfExtentMeters) / config.cellSizeMeters
        ));
        if (gridDimension <= 0 || gridDimension > 4096)
        {
            throw std::invalid_argument(
                "NavigationMap grid dimension is outside the supported prototype range"
            );
        }
    }

    struct InternalActor
    {
        EntityId entityId = 0;
        Vec3d positionMapMeters {};
        Vec3d velocityMapMetersPerSecond {};
        Vec3d accelerationMapMetersPerSecond2 {};
        Vec3d angularVelocityMapRadPerSecond {};
        Vec3d predictedEndPositionMapMeters {};
        Vec3d conservativeSweptCenterMapMeters {};
        double actorRadiusMeters = 0.0;
        double conservativeSweptRadiusMeters = 0.0;
        std::vector<NavigationObstacle> exactObstacles;
        std::uint32_t flags = 0;
        Revision motionRevision = 0;
        bool indexed = false;
    };

    Config config {};
    int gridDimension = 0;
    Revision mapRevision = 0;
    Revision sourceRevision = 0;
    std::vector<InternalActor> actors;
    std::unordered_map<CellCoord, std::vector<std::size_t>, CellCoordHash> cells;
    std::size_t indexedActorCount = 0;
    std::size_t outOfBoundsActorCount = 0;
    std::size_t rejectedActorCount = 0;
    double maxConservativeSweptRadiusMeters = 0.0;
    double maxIndexedActorRadiusMeters = 0.0;
    double maxIndexedActorSpeedMetersPerSecond = 0.0;
    double maxIndexedActorAccelerationMetersPerSecond2 = 0.0;

    void replaceDynamicWorld(DynamicWorldUpdate update)
    {
        std::vector<InternalActor> newActors;
        newActors.reserve(update.actors.size());

        std::unordered_map<CellCoord, std::vector<std::size_t>, CellCoordHash> newCells;
        newCells.reserve(update.actors.size());

        std::unordered_set<EntityId> seenIds;
        seenIds.reserve(update.actors.size());

        std::size_t newIndexedActorCount = 0;
        std::size_t newOutOfBoundsActorCount = 0;
        std::size_t newRejectedActorCount = 0;
        double newMaxConservativeSweptRadiusMeters = 0.0;
        double newMaxIndexedActorRadiusMeters = 0.0;
        double newMaxIndexedActorSpeedMetersPerSecond = 0.0;
        double newMaxIndexedActorAccelerationMetersPerSecond2 = 0.0;

        const double horizon = config.predictionHorizonSeconds;
        const double halfHorizonSquared = 0.5 * horizon * horizon;

        for (const DynamicActorInput& input : update.actors)
        {
            const bool exactGeometryValid =
                std::all_of(
                    input.exactObstacles.begin(),
                    input.exactObstacles.end(),
                    [](const NavigationObstacle& obstacle)
                    {
                        return obstacle.finite();
                    }
                );

            if (!isFinite(input.positionMapMeters) ||
                !isFinite(input.velocityMapMetersPerSecond) ||
                !isFinite(input.accelerationMapMetersPerSecond2) ||
                !isFinite(input.angularVelocityMapRadPerSecond) ||
                !isFinite(input.radiusMeters) ||
                input.radiusMeters < 0.0 ||
                !exactGeometryValid ||
                !seenIds.insert(input.entityId).second)
            {
                ++newRejectedActorCount;
                continue;
            }

            InternalActor actor;
            actor.entityId = input.entityId;
            actor.positionMapMeters = input.positionMapMeters;
            actor.velocityMapMetersPerSecond =
                input.velocityMapMetersPerSecond;
            actor.accelerationMapMetersPerSecond2 =
                input.accelerationMapMetersPerSecond2;
            actor.angularVelocityMapRadPerSecond =
                input.angularVelocityMapRadPerSecond;
            actor.actorRadiusMeters = input.radiusMeters;
            actor.exactObstacles = input.exactObstacles;
            actor.flags = input.flags;
            actor.motionRevision = input.motionRevision;

            actor.predictedEndPositionMapMeters =
                actor.positionMapMeters +
                actor.velocityMapMetersPerSecond * horizon +
                actor.accelerationMapMetersPerSecond2 * halfHorizonSquared;

            const double travelBound =
                length(actor.velocityMapMetersPerSecond) * horizon +
                0.5 * length(actor.accelerationMapMetersPerSecond2) * horizon * horizon;

            actor.conservativeSweptCenterMapMeters = actor.positionMapMeters;
            actor.conservativeSweptRadiusMeters = actor.actorRadiusMeters + travelBound;

            CellCoord cell;
            if (cellForPoint(actor.positionMapMeters, cell))
            {
                actor.indexed = true;
                const std::size_t actorIndex = newActors.size();
                newCells[cell].push_back(actorIndex);
                ++newIndexedActorCount;
                newMaxConservativeSweptRadiusMeters = std::max(
                    newMaxConservativeSweptRadiusMeters,
                    actor.conservativeSweptRadiusMeters
                );
                newMaxIndexedActorRadiusMeters = std::max(
                    newMaxIndexedActorRadiusMeters,
                    actor.actorRadiusMeters
                );
                newMaxIndexedActorSpeedMetersPerSecond = std::max(
                    newMaxIndexedActorSpeedMetersPerSecond,
                    length(actor.velocityMapMetersPerSecond)
                );
                newMaxIndexedActorAccelerationMetersPerSecond2 = std::max(
                    newMaxIndexedActorAccelerationMetersPerSecond2,
                    length(actor.accelerationMapMetersPerSecond2)
                );
            }
            else
            {
                ++newOutOfBoundsActorCount;
            }

            newActors.push_back(actor);
        }

        sourceRevision = update.sourceRevision;
        actors = std::move(newActors);
        cells = std::move(newCells);
        indexedActorCount = newIndexedActorCount;
        outOfBoundsActorCount = newOutOfBoundsActorCount;
        rejectedActorCount = newRejectedActorCount;
        maxConservativeSweptRadiusMeters = newMaxConservativeSweptRadiusMeters;
        maxIndexedActorRadiusMeters = newMaxIndexedActorRadiusMeters;
        maxIndexedActorSpeedMetersPerSecond =
            newMaxIndexedActorSpeedMetersPerSecond;
        maxIndexedActorAccelerationMetersPerSecond2 =
            newMaxIndexedActorAccelerationMetersPerSecond2;
        ++mapRevision;
    }

    QueryResult queryCorridor(const CorridorQuery& query) const
    {
        if (!isFinite(query.startMapMeters) ||
            !isFinite(query.endMapMeters) ||
            !isFinite(query.radiusMeters) ||
            query.radiusMeters < 0.0 ||
            (query.lookAheadSeconds.has_value() &&
             (!isFinite(*query.lookAheadSeconds) ||
              *query.lookAheadSeconds < 0.0)))
        {
            throw std::invalid_argument("NavigationMap corridor query is invalid");
        }

        const double lookAheadSeconds =
            query.lookAheadSeconds.value_or(
                config.predictionHorizonSeconds
            );
        QueryResult result = makeQueryResult(lookAheadSeconds);
        const double broadphaseRadius =
            query.radiusMeters +
            config.interactionMarginMeters +
            maximumSweptRadiusUpperBound(lookAheadSeconds);

        const Vec3d minimum {
            std::min(query.startMapMeters.x, query.endMapMeters.x) - broadphaseRadius,
            std::min(query.startMapMeters.y, query.endMapMeters.y) - broadphaseRadius,
            std::min(query.startMapMeters.z, query.endMapMeters.z) - broadphaseRadius
        };
        const Vec3d maximum {
            std::max(query.startMapMeters.x, query.endMapMeters.x) + broadphaseRadius,
            std::max(query.startMapMeters.y, query.endMapMeters.y) + broadphaseRadius,
            std::max(query.startMapMeters.z, query.endMapMeters.z) + broadphaseRadius
        };

        CellCoord minimumCell;
        CellCoord maximumCell;
        if (!cellRangeForAabb(minimum, maximum, minimumCell, maximumCell))
            return result;

        visitCells(
            minimumCell,
            maximumCell,
            result,
            [&](const InternalActor& actor)
            {
                const double radius =
                    query.radiusMeters +
                    config.interactionMarginMeters +
                    sweptRadius(actor, lookAheadSeconds);
                return distanceToSegmentSquared(
                    actor.positionMapMeters,
                    query.startMapMeters,
                    query.endMapMeters
                ) <= radius * radius;
            },
            lookAheadSeconds
        );

        sortCandidates(result);
        return result;
    }

    QueryResult querySphere(const SphereQuery& query) const
    {
        if (!isFinite(query.centerMapMeters) ||
            !isFinite(query.radiusMeters) ||
            query.radiusMeters < 0.0 ||
            (query.lookAheadSeconds.has_value() &&
             (!isFinite(*query.lookAheadSeconds) ||
              *query.lookAheadSeconds < 0.0)))
        {
            throw std::invalid_argument("NavigationMap sphere query is invalid");
        }

        const double lookAheadSeconds =
            query.lookAheadSeconds.value_or(
                config.predictionHorizonSeconds
            );
        QueryResult result = makeQueryResult(lookAheadSeconds);
        const double broadphaseRadius =
            query.radiusMeters +
            config.interactionMarginMeters +
            maximumSweptRadiusUpperBound(lookAheadSeconds);

        const Vec3d extent {broadphaseRadius, broadphaseRadius, broadphaseRadius};
        const Vec3d minimum = query.centerMapMeters - extent;
        const Vec3d maximum = query.centerMapMeters + extent;

        CellCoord minimumCell;
        CellCoord maximumCell;
        if (!cellRangeForAabb(minimum, maximum, minimumCell, maximumCell))
            return result;

        visitCells(
            minimumCell,
            maximumCell,
            result,
            [&](const InternalActor& actor)
            {
                const double radius =
                    query.radiusMeters +
                    config.interactionMarginMeters +
                    sweptRadius(actor, lookAheadSeconds);
                return distanceSquared(
                    query.centerMapMeters,
                    actor.positionMapMeters
                ) <= radius * radius;
            },
            lookAheadSeconds
        );

        sortCandidates(result);
        return result;
    }

    Stats stats() const noexcept
    {
        Stats result;
        result.mapRevision = mapRevision;
        result.sourceRevision = sourceRevision;
        result.actorCount = actors.size();
        result.indexedActorCount = indexedActorCount;
        result.occupiedCellCount = cells.size();
        result.outOfBoundsActorCount = outOfBoundsActorCount;
        result.rejectedActorCount = rejectedActorCount;
        result.maxConservativeSweptRadiusMeters = maxConservativeSweptRadiusMeters;
        return result;
    }

private:
    void validateConfig() const
    {
        if (!isFinite(config.halfExtentMeters) || config.halfExtentMeters <= 0.0 ||
            !isFinite(config.cellSizeMeters) || config.cellSizeMeters <= 0.0 ||
            !isFinite(config.predictionHorizonSeconds) || config.predictionHorizonSeconds < 0.0 ||
            !isFinite(config.interactionMarginMeters) || config.interactionMarginMeters < 0.0)
        {
            throw std::invalid_argument("NavigationMap config is invalid");
        }
    }

    bool cellForPoint(const Vec3d& point, CellCoord& outCell) const noexcept
    {
        const double half = config.halfExtentMeters;
        if (point.x < -half || point.x >= half ||
            point.y < -half || point.y >= half ||
            point.z < -half || point.z >= half)
        {
            return false;
        }

        outCell = {
            cellIndex(point.x),
            cellIndex(point.y),
            cellIndex(point.z)
        };
        return true;
    }

    int cellIndex(double coordinate) const noexcept
    {
        const double shifted = coordinate + config.halfExtentMeters;
        const int raw = static_cast<int>(std::floor(shifted / config.cellSizeMeters));
        return std::clamp(raw, 0, gridDimension - 1);
    }

    bool cellRangeForAabb(
        const Vec3d& minimum,
        const Vec3d& maximum,
        CellCoord& outMinimum,
        CellCoord& outMaximum
    ) const noexcept
    {
        const double half = config.halfExtentMeters;
        if (maximum.x < -half || minimum.x >= half ||
            maximum.y < -half || minimum.y >= half ||
            maximum.z < -half || minimum.z >= half)
        {
            return false;
        }

        outMinimum = {
            cellIndex(std::max(minimum.x, -half)),
            cellIndex(std::max(minimum.y, -half)),
            cellIndex(std::max(minimum.z, -half))
        };
        outMaximum = {
            cellIndex(std::min(maximum.x, half)),
            cellIndex(std::min(maximum.y, half)),
            cellIndex(std::min(maximum.z, half))
        };
        return true;
    }

    double sweptRadius(
        const InternalActor& actor,
        double lookAheadSeconds
    ) const noexcept
    {
        const double travelBound =
            length(actor.velocityMapMetersPerSecond) *
                lookAheadSeconds +
            0.5 *
                length(actor.accelerationMapMetersPerSecond2) *
                lookAheadSeconds *
                lookAheadSeconds;
        return actor.actorRadiusMeters + travelBound;
    }

    double maximumSweptRadiusUpperBound(
        double lookAheadSeconds
    ) const noexcept
    {
        return
            maxIndexedActorRadiusMeters +
            maxIndexedActorSpeedMetersPerSecond *
                lookAheadSeconds +
            0.5 *
                maxIndexedActorAccelerationMetersPerSecond2 *
                lookAheadSeconds *
                lookAheadSeconds;
    }

    QueryResult makeQueryResult(double lookAheadSeconds) const
    {
        QueryResult result;
        result.mapRevision = mapRevision;
        result.sourceRevision = sourceRevision;
        result.lookAheadSeconds = lookAheadSeconds;
        return result;
    }

    Candidate toCandidate(
        const InternalActor& actor,
        double lookAheadSeconds
    ) const
    {
        Candidate result;
        result.entityId = actor.entityId;
        result.positionMapMeters = actor.positionMapMeters;
        result.velocityMapMetersPerSecond = actor.velocityMapMetersPerSecond;
        result.accelerationMapMetersPerSecond2 = actor.accelerationMapMetersPerSecond2;
        result.angularVelocityMapRadPerSecond =
            actor.angularVelocityMapRadPerSecond;
        result.predictionHorizonSeconds = lookAheadSeconds;
        result.predictedEndPositionMapMeters =
            actor.positionMapMeters +
            actor.velocityMapMetersPerSecond *
                lookAheadSeconds +
            actor.accelerationMapMetersPerSecond2 *
                (0.5 * lookAheadSeconds * lookAheadSeconds);
        result.conservativeSweptCenterMapMeters =
            actor.positionMapMeters;
        result.actorRadiusMeters = actor.actorRadiusMeters;
        result.conservativeSweptRadiusMeters =
            sweptRadius(actor, lookAheadSeconds);
        result.exactObstacles = actor.exactObstacles;
        result.flags = actor.flags;
        result.motionRevision = actor.motionRevision;
        return result;
    }

    template <typename Predicate>
    void visitCells(
        const CellCoord& minimumCell,
        const CellCoord& maximumCell,
        QueryResult& result,
        Predicate&& predicate,
        double lookAheadSeconds
    ) const
    {
        for (int z = minimumCell.z; z <= maximumCell.z; ++z)
        {
            for (int y = minimumCell.y; y <= maximumCell.y; ++y)
            {
                for (int x = minimumCell.x; x <= maximumCell.x; ++x)
                {
                    ++result.diagnostics.gridCellsVisited;
                    const auto cellIt = cells.find({x, y, z});
                    if (cellIt == cells.end())
                        continue;

                    ++result.diagnostics.occupiedCellsVisited;
                    for (const std::size_t actorIndex : cellIt->second)
                    {
                        ++result.diagnostics.actorsExamined;
                        const InternalActor& actor = actors[actorIndex];
                        if (predicate(actor))
                        {
                            result.candidates.push_back(
                                toCandidate(actor, lookAheadSeconds)
                            );
                        }
                    }
                }
            }
        }
    }

    static void sortCandidates(QueryResult& result)
    {
        std::sort(
            result.candidates.begin(),
            result.candidates.end(),
            [](const Candidate& a, const Candidate& b)
            {
                return a.entityId < b.entityId;
            }
        );
    }
};

NavigationMap::NavigationMap()
    : impl_(std::make_unique<Impl>(Config {}))
{
}

NavigationMap::NavigationMap(const Config& config)
    : impl_(std::make_unique<Impl>(config))
{
}

NavigationMap::~NavigationMap() = default;
NavigationMap::NavigationMap(NavigationMap&&) noexcept = default;
NavigationMap& NavigationMap::operator=(NavigationMap&&) noexcept = default;

void NavigationMap::replaceDynamicWorld(DynamicWorldUpdate update)
{
    impl_->replaceDynamicWorld(std::move(update));
}

NavigationMap::QueryResult NavigationMap::queryCorridor(const CorridorQuery& query) const
{
    return impl_->queryCorridor(query);
}

NavigationMap::QueryResult NavigationMap::querySphere(const SphereQuery& query) const
{
    return impl_->querySphere(query);
}

NavigationMap::Stats NavigationMap::stats() const noexcept
{
    return impl_->stats();
}

} // namespace world::navigation
