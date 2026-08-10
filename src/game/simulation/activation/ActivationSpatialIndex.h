#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/game/simulation/activation/ActivationShadow.h"

namespace game::simulation::activation
{

struct ActivationSpatialIndexConfig
{
    // Activation only needs a conservative candidate broad-phase. Detailed
    // collision remains elsewhere. Ten kilometres keeps ordinary ship queries
    // compact while still accommodating multi-kilometre infrastructure.
    double cellSizeMeters = 10000.0;

    // A pathological speed/radius must never create an unbounded cell walk.
    // Falling back to all anchors in the same system preserves correctness.
    std::size_t maxVisitedCellsPerQuery = 4096;
};

struct ActivationSpatialQueryResult
{
    std::vector<std::size_t> candidateIndices;
    double conservativeQueryRadiusMeters = 0.0;
    double subjectResidualSpeedMetersPerSecond = 0.0;
    double maxAnchorResidualSpeedMetersPerSecond = 0.0;
    std::size_t visitedCellCount = 0;
    bool usedFallback = false;
};

namespace spatial_index_detail
{
struct CellKey
{
    int systemId = -1;
    std::int64_t x = 0;
    std::int64_t y = 0;
    std::int64_t z = 0;

    bool operator==(const CellKey& other) const noexcept
    {
        return
            systemId == other.systemId &&
            x == other.x &&
            y == other.y &&
            z == other.z;
    }
};

struct CellKeyHash
{
    std::size_t operator()(const CellKey& key) const noexcept
    {
        std::size_t seed = std::hash<int>{}(key.systemId);
        const auto mix = [&seed](std::size_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        };

        mix(std::hash<std::int64_t>{}(key.x));
        mix(std::hash<std::int64_t>{}(key.y));
        mix(std::hash<std::int64_t>{}(key.z));
        return seed;
    }
};

struct SystemEnvelopeStats
{
    double maxAnchorRadiusMeters = 0.0;

    // Broad-phase must operate on relative motion. Using absolute world speeds
    // is disastrous in orbital scenes because every nearby object may share a
    // 30 km/s bulk velocity while moving only metres/second relative to one
    // another. Store a per-system co-moving velocity origin and the maximum
    // residual anchor speed around it. Triangle inequality then gives a safe
    // upper bound for any subject/anchor relative speed.
    std::array<double, 3> velocityMinMetersPerSecond {
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    std::array<double, 3> velocityMaxMetersPerSecond {
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    std::array<double, 3> referenceVelocityMetersPerSecond {0.0, 0.0, 0.0};
    double maxAnchorResidualSpeedMetersPerSecond = 0.0;
};

inline std::int64_t cellCoordinate(double value, double cellSize) noexcept
{
    return static_cast<std::int64_t>(std::floor(value / cellSize));
}

inline double residualSpeed(
    const std::array<double, 3>& velocityMetersPerSecond,
    const std::array<double, 3>& referenceVelocityMetersPerSecond
) noexcept
{
    return detail::length(
        detail::subtract(
            velocityMetersPerSecond,
            referenceVelocityMetersPerSecond
        )
    );
}

inline double conservativeQueryRadius(
    const KinematicPoint& subject,
    const SystemEnvelopeStats& stats,
    const InteractionHorizonPolicy& policy
) noexcept
{
    const double lookAhead = std::max(0.0, policy.lookAheadSeconds);
    const double subjectResidualSpeed = residualSpeed(
        subject.velocityMetersPerSecond,
        stats.referenceVelocityMetersPerSecond
    );
    const double speedReach = lookAhead * (
        subjectResidualSpeed +
        std::max(0.0, stats.maxAnchorResidualSpeedMetersPerSecond)
    );

    return
        std::max(0.0, subject.bounds.interactionRadiusMeters) +
        std::max(0.0, stats.maxAnchorRadiusMeters) +
        std::max(0.0, policy.safetyMarginMeters) +
        std::max(0.0, policy.gameplayRangeMeters) +
        speedReach;
}
} // namespace spatial_index_detail

class ActivationSpatialIndex
{
public:
    explicit ActivationSpatialIndex(
        ActivationSpatialIndexConfig config = {}
    ) noexcept
        : m_config(config)
    {
        if (!(m_config.cellSizeMeters > 0.0) ||
            !std::isfinite(m_config.cellSizeMeters))
        {
            m_config.cellSizeMeters = 10000.0;
        }

        if (m_config.maxVisitedCellsPerQuery == 0)
            m_config.maxVisitedCellsPerQuery = 1;
    }

    void rebuild(const std::vector<ActivationAnchor>& anchors)
    {
        using namespace spatial_index_detail;

        m_cells.clear();
        m_systemAnchorIndices.clear();
        m_systemStats.clear();

        for (std::size_t index = 0; index < anchors.size(); ++index)
        {
            const auto& anchor = anchors[index];
            const auto& position = anchor.kinematics.positionMeters;

            const CellKey key{
                anchor.systemId,
                cellCoordinate(position[0], m_config.cellSizeMeters),
                cellCoordinate(position[1], m_config.cellSizeMeters),
                cellCoordinate(position[2], m_config.cellSizeMeters)
            };

            m_cells[key].push_back(index);
            m_systemAnchorIndices[anchor.systemId].push_back(index);

            auto& stats = m_systemStats[anchor.systemId];
            stats.maxAnchorRadiusMeters = std::max(
                stats.maxAnchorRadiusMeters,
                std::max(
                    0.0,
                    anchor.kinematics.bounds.interactionRadiusMeters
                )
            );
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                const double velocity =
                    anchor.kinematics.velocityMetersPerSecond[axis];
                stats.velocityMinMetersPerSecond[axis] = std::min(
                    stats.velocityMinMetersPerSecond[axis],
                    velocity
                );
                stats.velocityMaxMetersPerSecond[axis] = std::max(
                    stats.velocityMaxMetersPerSecond[axis],
                    velocity
                );
            }
        }

        // Pick the centre of each system's velocity envelope as a co-moving
        // origin. Subtracting the same velocity from every entity does not
        // change relative velocity or CPA, but removes common orbital drift
        // from the broad-phase reach calculation.
        for (auto& [systemId, stats] : m_systemStats)
        {
            (void)systemId;
            for (std::size_t axis = 0; axis < 3; ++axis)
            {
                stats.referenceVelocityMetersPerSecond[axis] = 0.5 * (
                    stats.velocityMinMetersPerSecond[axis] +
                    stats.velocityMaxMetersPerSecond[axis]
                );
            }
        }

        for (const auto& anchor : anchors)
        {
            auto statsIt = m_systemStats.find(anchor.systemId);
            if (statsIt == m_systemStats.end())
                continue;

            auto& stats = statsIt->second;
            stats.maxAnchorResidualSpeedMetersPerSecond = std::max(
                stats.maxAnchorResidualSpeedMetersPerSecond,
                residualSpeed(
                    anchor.kinematics.velocityMetersPerSecond,
                    stats.referenceVelocityMetersPerSecond
                )
            );
        }
    }

    ActivationSpatialQueryResult query(
        int subjectSystemId,
        const KinematicPoint& subject,
        const InteractionHorizonPolicy& policy
    ) const
    {
        using namespace spatial_index_detail;

        ActivationSpatialQueryResult result;

        const auto systemIt = m_systemAnchorIndices.find(subjectSystemId);
        if (systemIt == m_systemAnchorIndices.end())
            return result;

        const auto statsIt = m_systemStats.find(subjectSystemId);
        const SystemEnvelopeStats stats =
            statsIt != m_systemStats.end()
                ? statsIt->second
                : SystemEnvelopeStats{};

        result.subjectResidualSpeedMetersPerSecond = residualSpeed(
            subject.velocityMetersPerSecond,
            stats.referenceVelocityMetersPerSecond
        );
        result.maxAnchorResidualSpeedMetersPerSecond =
            stats.maxAnchorResidualSpeedMetersPerSecond;
        result.conservativeQueryRadiusMeters =
            conservativeQueryRadius(subject, stats, policy);

        const auto& p = subject.positionMeters;
        const double radius = result.conservativeQueryRadiusMeters;
        const double cellSize = m_config.cellSizeMeters;

        const std::int64_t minX = cellCoordinate(p[0] - radius, cellSize);
        const std::int64_t maxX = cellCoordinate(p[0] + radius, cellSize);
        const std::int64_t minY = cellCoordinate(p[1] - radius, cellSize);
        const std::int64_t maxY = cellCoordinate(p[1] + radius, cellSize);
        const std::int64_t minZ = cellCoordinate(p[2] - radius, cellSize);
        const std::int64_t maxZ = cellCoordinate(p[2] + radius, cellSize);

        const long double nx = static_cast<long double>(maxX - minX) + 1.0L;
        const long double ny = static_cast<long double>(maxY - minY) + 1.0L;
        const long double nz = static_cast<long double>(maxZ - minZ) + 1.0L;
        const long double cellVisits = nx * ny * nz;

        if (!std::isfinite(static_cast<double>(cellVisits)) ||
            cellVisits > static_cast<long double>(
                m_config.maxVisitedCellsPerQuery))
        {
            result.candidateIndices = systemIt->second;
            result.visitedCellCount = 0;
            result.usedFallback = true;
            return result;
        }

        result.visitedCellCount = static_cast<std::size_t>(cellVisits);

        for (std::int64_t x = minX; x <= maxX; ++x)
        {
            for (std::int64_t y = minY; y <= maxY; ++y)
            {
                for (std::int64_t z = minZ; z <= maxZ; ++z)
                {
                    const auto cellIt = m_cells.find(
                        CellKey{subjectSystemId, x, y, z}
                    );
                    if (cellIt == m_cells.end())
                        continue;

                    result.candidateIndices.insert(
                        result.candidateIndices.end(),
                        cellIt->second.begin(),
                        cellIt->second.end()
                    );
                }
            }
        }

        // Cell iteration order is spatial rather than insertion order. Restore
        // anchor-vector order so exact ties remain deterministic and match the
        // original all-pairs evaluator.
        std::sort(
            result.candidateIndices.begin(),
            result.candidateIndices.end()
        );
        result.candidateIndices.erase(
            std::unique(
                result.candidateIndices.begin(),
                result.candidateIndices.end()
            ),
            result.candidateIndices.end()
        );

        return result;
    }

private:
    ActivationSpatialIndexConfig m_config {};

    std::unordered_map<
        spatial_index_detail::CellKey,
        std::vector<std::size_t>,
        spatial_index_detail::CellKeyHash
    > m_cells;

    std::unordered_map<int, std::vector<std::size_t>> m_systemAnchorIndices;
    std::unordered_map<int, spatial_index_detail::SystemEnvelopeStats>
        m_systemStats;
};

} // namespace game::simulation::activation
