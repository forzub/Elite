#pragma once

#include "OrientedPassageEvaluator.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace world::navigation
{

// Builds a very small set of precision-gap candidates around one already known
// primary conflict. It deliberately does not perform all-pairs discovery.
// Complexity is O(neighbors * hardCandidateLimit), where hardCandidateLimit is
// fixed and small.
class BoundedGapCandidateBuilder final
{
public:
    using Vec3d = OrientedPassageEvaluator::Vec3d;
    using ObstacleGap = OrientedPassageEvaluator::ObstacleGap;

    static constexpr std::size_t kHardCandidateLimit = 8;

    struct ObstacleWitness
    {
        std::uint64_t obstacleId = 0;
        std::uint64_t snapshotRevision = 0;
        Vec3d centerMapMeters {};
        double conservativeRadiusMeters = 0.0;
    };

    struct Policy
    {
        // Inflates both constraining obstacle witnesses before measuring the
        // free separation. Vehicle-local clearance remains owned by HullProxy.
        double boundaryClearanceMeters = 0.0;

        double minimumClearSeparationMeters = 0.0;
        double maximumClearSeparationMeters = 1000.0;

        // Two obstacles constrain one passage-plane axis. The orthogonal
        // clearance must come from already-reduced local/static evidence; the
        // builder never invents it from the pair alone.
        double secondaryClearanceMeters = 1000.0;

        // Candidate center must lie inside this bounded window relative to the
        // reference point and requested travel direction.
        double minimumForwardDistanceMeters = 0.0;
        double maximumForwardDistanceMeters = 1000.0;
        double maximumCenterlineOffsetMeters = 1000.0;

        // A side-by-side slot requires the pair separation axis to be mostly
        // transverse to travel. 0 = exactly perpendicular, 1 = unrestricted.
        double maximumAbsSeparationTravelDot = 0.5;

        // Caller may request fewer candidates, never more than the hard limit.
        std::size_t maxCandidates = kHardCandidateLimit;
    };

    struct Query
    {
        Vec3d referencePointMapMeters {};
        Vec3d travelDirectionMap {0.0, 0.0, 1.0};
        ObstacleWitness primary {};
        std::vector<ObstacleWitness> neighbors;
        Policy policy {};
    };

    struct Candidate
    {
        std::uint64_t primaryObstacleId = 0;
        std::uint64_t neighborObstacleId = 0;
        ObstacleGap gap {};
        double forwardDistanceMeters = 0.0;
        double centerlineOffsetMeters = 0.0;
    };

    struct Diagnostics
    {
        std::size_t neighborsExamined = 0;
        std::size_t rejectedInvalid = 0;
        std::size_t rejectedSameObstacle = 0;
        std::size_t rejectedRevision = 0;
        std::size_t rejectedOverlapping = 0;
        std::size_t rejectedGapRange = 0;
        std::size_t rejectedAlignment = 0;
        std::size_t rejectedLongitudinalWindow = 0;
        std::size_t rejectedCenterlineOffset = 0;
    };

    struct Result
    {
        bool validInput = false;
        std::size_t candidateLimitApplied = 0;
        Diagnostics diagnostics {};
        std::vector<Candidate> candidates;
    };

    [[nodiscard]] static Result build(const Query& query);
};

} // namespace world::navigation
