#include "world/navigation/trajectory/BoundedGapCandidateBuilder.h"
#include "world/navigation/trajectory/OrientedPassageEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Builder = world::navigation::BoundedGapCandidateBuilder;
using Passage = world::navigation::OrientedPassageEvaluator;

constexpr double kTolerance = 1.0e-9;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool near(double a, double b)
{
    return std::abs(a - b) <= kTolerance;
}

Builder::ObstacleWitness witness(
    std::uint64_t id,
    std::uint64_t revision,
    double x,
    double y,
    double z,
    double radius = 1.0
)
{
    Builder::ObstacleWitness value;
    value.obstacleId = id;
    value.snapshotRevision = revision;
    value.centerMapMeters = {x, y, z};
    value.conservativeRadiusMeters = radius;
    return value;
}

Builder::Query baseQuery()
{
    Builder::Query query;
    query.referencePointMapMeters = {0.0, 0.0, 0.0};
    query.travelDirectionMap = {0.0, 0.0, 1.0};
    query.primary = witness(1, 77, -2.5, 0.0, 20.0);

    query.policy.boundaryClearanceMeters = 0.0;
    query.policy.minimumClearSeparationMeters = 0.5;
    query.policy.maximumClearSeparationMeters = 20.0;
    query.policy.secondaryClearanceMeters = 10.0;
    query.policy.minimumForwardDistanceMeters = 0.0;
    query.policy.maximumForwardDistanceMeters = 50.0;
    query.policy.maximumCenterlineOffsetMeters = 20.0;
    query.policy.maximumAbsSeparationTravelDot = 0.25;
    query.policy.maxCandidates = Builder::kHardCandidateLimit;
    return query;
}

void testPrimaryConflictAndNeighborBuildOneGap()
{
    Builder::Query query = baseQuery();
    query.neighbors.push_back(witness(2, 77, 2.5, 0.0, 20.0));

    const Builder::Result result = Builder::build(query);

    require(result.validInput, "base gap query must be valid");
    require(result.diagnostics.neighborsExamined == 1,
            "builder must examine the supplied neighbor exactly once");
    require(result.candidates.size() == 1,
            "primary conflict plus one transverse neighbor must form one gap");

    const Builder::Candidate& candidate = result.candidates.front();
    require(candidate.primaryObstacleId == 1 && candidate.neighborObstacleId == 2,
            "candidate identity mismatch");
    require(near(candidate.gap.centerMapMeters.x, 0.0) &&
            near(candidate.gap.centerMapMeters.y, 0.0) &&
            near(candidate.gap.centerMapMeters.z, 20.0),
            "gap center must lie between the two inflated surfaces");
    require(near(candidate.gap.clearSeparationMeters, 3.0),
            "clear gap width mismatch");
    require(near(candidate.forwardDistanceMeters, 20.0),
            "forward distance mismatch");
    require(near(candidate.centerlineOffsetMeters, 0.0),
            "centered gap should have zero centerline offset");
}

void testBuilderNeverEmitsNeighborNeighborPair()
{
    Builder::Query query = baseQuery();
    query.primary = witness(10, 77, -3.0, 0.0, 20.0);
    query.neighbors.push_back(witness(20, 77, 3.0, 0.0, 20.0));
    query.neighbors.push_back(witness(30, 77, 4.0, 1.0, 20.0));
    query.neighbors.push_back(witness(40, 77, 5.0, -1.0, 20.0));

    const Builder::Result result = Builder::build(query);

    require(result.validInput, "bounded pair query must be valid");
    require(result.diagnostics.neighborsExamined == query.neighbors.size(),
            "builder must perform one primary-neighbor examination per neighbor");
    require(result.candidates.size() <= query.neighbors.size(),
            "builder must not manufacture neighbor-neighbor pairs");
    for (const Builder::Candidate& candidate : result.candidates)
    {
        require(candidate.primaryObstacleId == query.primary.obstacleId,
                "every emitted pair must retain the single primary conflict");
    }
}

void testHardCandidateLimitAndDeterministicOrdering()
{
    Builder::Query query = baseQuery();
    query.primary = witness(1, 77, 0.0, 0.0, 20.0);
    query.policy.maximumClearSeparationMeters = 100.0;
    query.policy.maximumCenterlineOffsetMeters = 100.0;
    query.policy.maxCandidates = 100;

    for (std::uint64_t i = 0; i < 12; ++i)
    {
        query.neighbors.push_back(
            witness(100 + i, 77, 5.0 + static_cast<double>(i), 0.0, 20.0)
        );
    }

    const Builder::Result result = Builder::build(query);

    require(result.validInput, "candidate-limit query must be valid");
    require(result.candidateLimitApplied == Builder::kHardCandidateLimit,
            "caller must not raise the hard precision-candidate cap");
    require(result.candidates.size() == Builder::kHardCandidateLimit,
            "hard limit must bound emitted candidates");

    for (std::size_t i = 0; i < result.candidates.size(); ++i)
    {
        require(result.candidates[i].neighborObstacleId == 100 + i,
                "top-K ordering must be deterministic and favor the closest centerline gap");
    }
}

void testMixedSnapshotRevisionFailsClosed()
{
    Builder::Query query = baseQuery();
    query.neighbors.push_back(witness(2, 78, 2.5, 0.0, 20.0));

    const Builder::Result result = Builder::build(query);

    require(result.validInput, "query itself remains valid with a stale neighbor");
    require(result.candidates.empty(),
            "mixed-revision witness must not produce a gap");
    require(result.diagnostics.rejectedRevision == 1,
            "mixed-revision rejection must be observable");
}

void testLongitudinalPairIsNotAVisualSlot()
{
    Builder::Query query = baseQuery();
    query.primary = witness(1, 77, 0.0, 0.0, 10.0);
    query.neighbors.push_back(witness(2, 77, 0.0, 0.0, 15.0));

    const Builder::Result result = Builder::build(query);

    require(result.candidates.empty(),
            "front/back obstacle pair must not be treated as a transverse slot");
    require(result.diagnostics.rejectedAlignment == 1,
            "longitudinal pair must be rejected by separation/travel alignment");
}

void testOverlappingConservativeBoundsDoNotInventGap()
{
    Builder::Query query = baseQuery();
    query.primary = witness(1, 77, -0.5, 0.0, 20.0, 1.0);
    query.neighbors.push_back(witness(2, 77, 0.5, 0.0, 20.0, 1.0));

    const Builder::Result result = Builder::build(query);

    require(result.candidates.empty(),
            "overlapping conservative witnesses must not invent free space");
    require(result.diagnostics.rejectedOverlapping == 1,
            "overlap rejection must be observable");
}

void testBuiltGapFeedsOrientedPassagePrecision()
{
    Builder::Query query = baseQuery();
    query.neighbors.push_back(witness(2, 77, 2.5, 0.0, 20.0));

    const Builder::Result gaps = Builder::build(query);
    require(gaps.candidates.size() == 1,
            "precision integration fixture requires one gap");

    const Passage::Passage passage = Passage::makeObstacleGapPassage(
        gaps.candidates.front().gap
    );

    Passage::HullProxy hull;
    hull.halfExtentsBodyMeters = {4.0, 1.0, 6.0};

    Passage::Pose wide;
    wide.centerMapMeters = passage.centerMapMeters;
    const Passage::Result wideResult = Passage::evaluate(hull, wide, passage);
    require(wideResult.status == Passage::Status::TooWide && !wideResult.fits,
            "wide attitude must not fit the built 3 m obstacle gap");

    Passage::Pose rolled;
    rolled.centerMapMeters = passage.centerMapMeters;
    rolled.bodyToMap.right = {0.0, 1.0, 0.0};
    rolled.bodyToMap.up = {-1.0, 0.0, 0.0};
    rolled.bodyToMap.forward = {0.0, 0.0, 1.0};

    const Passage::Result rolledResult = Passage::evaluate(hull, rolled, passage);
    require(rolledResult.status == Passage::Status::Fits && rolledResult.fits,
            "rolled thin attitude must recover the positive passage between conflicts");
    require(!rolledResult.conservativeSphereFits,
            "precision gap must remain a case that sphere-only fit would discard");
}

void testIrrelevantGapOutsideCorridorWindowIsRejected()
{
    Builder::Query query = baseQuery();
    query.primary = witness(1, 77, 40.0, 0.0, 20.0);
    query.neighbors.push_back(witness(2, 77, 45.0, 0.0, 20.0));
    query.policy.maximumCenterlineOffsetMeters = 10.0;

    const Builder::Result result = Builder::build(query);

    require(result.candidates.empty(),
            "far-side gap must not enter the bounded precision candidate set");
    require(result.diagnostics.rejectedCenterlineOffset == 1,
            "far-side rejection must be observable");
}

} // namespace

int main()
{
    try
    {
        testPrimaryConflictAndNeighborBuildOneGap();
        testBuilderNeverEmitsNeighborNeighborPair();
        testHardCandidateLimitAndDeterministicOrdering();
        testMixedSnapshotRevisionFailsClosed();
        testLongitudinalPairIsNotAVisualSlot();
        testOverlappingConservativeBoundsDoNotInventGap();
        testBuiltGapFeedsOrientedPassagePrecision();
        testIrrelevantGapOutsideCorridorWindowIsRejected();

        std::cout << "NAVIGATION TRAJECTORY BOUNDED GAP TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY BOUNDED GAP TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
