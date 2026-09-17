#pragma once

#include "OrientedPassageEvaluator.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace world::navigation
{

// Predicts one already-selected obstacle pair over a short receding horizon.
//
// This is deliberately NOT a pair-discovery stage: BoundedGapCandidateBuilder
// remains responsible for selecting at most eight plausible snapshot pairs.
// MovingGapPredictor receives one such pair, advances its compact motion state,
// and proves conservative gap-width/transverse-alignment bounds between fixed
// time samples. It does not prove that a particular ship trajectory fits the
// moving passage; that remains a downstream moving-passage trajectory task.
class MovingGapPredictor final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;
    using ObstacleGap = OrientedPassageEvaluator::ObstacleGap;

    static constexpr std::size_t kSamples = 33;
    static constexpr std::size_t kIntervals = kSamples - 1;

    struct BoundaryMotion
    {
        std::uint64_t obstacleId = 0;
        std::uint64_t snapshotRevision = 0;

        Vec3d centerMapMeters {};
        Vec3d linearVelocityMapMetersPerSec {};
        Vec3d linearAccelerationMapMetersPerSec2 {};

        // Rotation does not alter a spherical conservative gap boundary, but it
        // does alter material surface velocity used by later contact severity.
        Vec3d angularVelocityMapRadPerSec {};

        double conservativeRadiusMeters = 0.0;
    };

    struct Policy
    {
        // Applied to each obstacle only for passage clearance geometry. Contact
        // surface points remain on conservativeRadiusMeters itself.
        double boundaryClearanceMeters = 0.0;

        // Orthogonal free-space evidence remains caller-owned, just as in the
        // accepted static BoundedGapCandidateBuilder.
        double secondaryClearanceMeters = 1000.0;

        // Continuous lower bound across the horizon must stay at or above this
        // value. Zero means merely non-overlapping inflated boundaries.
        double minimumContinuousClearSeparationMeters = 0.0;

        // The gap must remain mostly transverse to requested travel. 0 means
        // exactly perpendicular; 1 disables this rejection.
        double maximumAbsSeparationTravelDot = 0.5;
    };

    struct Query
    {
        Vec3d travelDirectionMap {0.0, 0.0, 1.0};
        BoundaryMotion primary {};
        BoundaryMotion secondary {};
        double horizonSeconds = 0.0;
        Policy policy {};
    };

    struct BoundaryState
    {
        Vec3d centerMapMeters {};
        Vec3d linearVelocityMapMetersPerSec {};

        // Geometric point on the conservative physical boundary facing the gap.
        Vec3d surfacePointTowardGapMapMeters {};
        Vec3d normalTowardFreeSpaceMap {1.0, 0.0, 0.0};

        // Material velocity at surfacePointTowardGapMapMeters. This includes
        // center translation plus omega x r and can feed the already accepted
        // EmergencyContactSeverityScorer witness contract downstream.
        Vec3d surfaceVelocityAtPointMapMetersPerSec {};
    };

    struct Sample
    {
        double timeSeconds = 0.0;
        ObstacleGap gap {};
        Vec3d gapCenterVelocityMapMetersPerSec {};
        double separationRateMetersPerSec = 0.0;
        double absSeparationTravelDot = 0.0;
        BoundaryState primary {};
        BoundaryState secondary {};
    };

    enum class Status : std::uint8_t
    {
        OpenForHorizon = 0,
        GapClosesDuringHorizon,
        AlignmentLost,
        RevisionMismatch,
        InvalidInput
    };

    struct Result
    {
        Status status = Status::InvalidInput;
        bool validInput = false;
        std::size_t samplesEvaluated = 0;
        std::size_t intervalsProven = 0;

        double minimumSampleClearSeparationMeters = 0.0;
        double minimumContinuousClearSeparationMeters = 0.0;
        double maximumSampleAbsSeparationTravelDot = 0.0;
        double maximumContinuousAbsSeparationTravelDotBound = 0.0;

        // Conservative bound on how far the separation axis may deviate from
        // its nearest endpoint sample inside any one interval.
        double maximumAxisDeviationFromNearestSampleRad = 0.0;

        std::array<Sample, kSamples> samples {};
    };

    [[nodiscard]] static Result predict(const Query& query) noexcept;
};

} // namespace world::navigation
