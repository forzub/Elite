#include "world/navigation/trajectory/MovingGapPredictor.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Predictor = world::navigation::MovingGapPredictor;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

Predictor::Query baseQuery()
{
    Predictor::Query query;
    query.travelDirectionMap = {0.0, 0.0, 1.0};
    query.primary.obstacleId = 1;
    query.primary.snapshotRevision = 7;
    query.primary.centerMapMeters = {-5.0, 0.0, 0.0};
    query.primary.conservativeRadiusMeters = 2.0;
    query.secondary.obstacleId = 2;
    query.secondary.snapshotRevision = 7;
    query.secondary.centerMapMeters = {5.0, 0.0, 0.0};
    query.secondary.conservativeRadiusMeters = 2.0;
    query.horizonSeconds = 1.0;
    query.policy.secondaryClearanceMeters = 20.0;
    query.policy.minimumContinuousClearSeparationMeters = 0.0;
    query.policy.maximumAbsSeparationTravelDot = 0.5;
    return query;
}

void testStaticGapRemainsOpenForWholeHorizon()
{
    const Predictor::Result result = Predictor::predict(baseQuery());

    require(result.validInput, "static moving-gap fixture must be valid");
    require(result.status == Predictor::Status::OpenForHorizon,
            "static transverse gap must remain open");
    require(result.samplesEvaluated == Predictor::kSamples,
            "moving-gap predictor must evaluate all 33 samples");
    require(result.intervalsProven == Predictor::kIntervals,
            "moving-gap predictor must prove all 32 intervals");
    requireNear(result.minimumSampleClearSeparationMeters, 6.0, 1.0e-9,
                "static gap sample width must remain six meters");
    requireNear(result.minimumContinuousClearSeparationMeters, 6.0, 1.0e-9,
                "static gap continuous width must remain six meters");
    requireNear(result.samples.front().gap.centerMapMeters.x, 0.0, 1.0e-9,
                "static gap center must lie midway between obstacles");
    requireNear(result.samples.front().primary.normalTowardFreeSpaceMap.x,
                1.0, 1.0e-9,
                "primary free-space normal must point into the gap");
    requireNear(result.samples.front().secondary.normalTowardFreeSpaceMap.x,
                -1.0, 1.0e-9,
                "secondary free-space normal must point into the gap");
}

void testComovingGapCarriesGapCenterVelocity()
{
    Predictor::Query query = baseQuery();
    query.primary.linearVelocityMapMetersPerSec = {3.0, 0.0, 0.0};
    query.secondary.linearVelocityMapMetersPerSec = {3.0, 0.0, 0.0};

    const Predictor::Result result = Predictor::predict(query);

    require(result.status == Predictor::Status::OpenForHorizon,
            "co-moving gap must remain open");
    requireNear(result.samples.back().gap.centerMapMeters.x, 3.0, 1.0e-9,
                "gap center must translate with co-moving boundaries");
    requireNear(result.samples.back().gapCenterVelocityMapMetersPerSec.x,
                3.0, 1.0e-9,
                "gap-center target velocity must follow co-moving boundaries");
    requireNear(result.minimumContinuousClearSeparationMeters, 6.0, 1.0e-9,
                "co-moving gap must preserve continuous width");
}

void testClosureBetweenSamplesIsRejectedContinuously()
{
    Predictor::Query query = baseQuery();
    query.primary.centerMapMeters = {0.0, 0.0, 0.0};
    query.primary.conservativeRadiusMeters = 0.9;
    query.secondary.centerMapMeters = {2.1, 0.0, 0.0};
    query.secondary.conservativeRadiusMeters = 0.9;

    // dt = 1/32 s. The secondary center moves from +2.1 m at sample 0 to
    // -2.1 m at sample 1, so both endpoint samples show 0.3 m clearance while
    // the true gap collapses midway between them.
    query.secondary.linearVelocityMapMetersPerSec = {-134.4, 0.0, 0.0};

    const Predictor::Result result = Predictor::predict(query);

    require(result.status == Predictor::Status::GapClosesDuringHorizon,
            "between-sample closure must fail the continuous gap proof");
    require(result.minimumSampleClearSeparationMeters > 0.0,
            "pinned fixture must keep sampled endpoints apparently open");
    require(result.minimumContinuousClearSeparationMeters < 0.0,
            "continuous chord bound must expose hidden closure");
}

void testGapThatRotatesIntoTravelDirectionLosesAlignment()
{
    Predictor::Query query = baseQuery();
    query.secondary.linearVelocityMapMetersPerSec = {0.0, 0.0, 10.0};

    const Predictor::Result result = Predictor::predict(query);

    require(result.status == Predictor::Status::AlignmentLost,
            "pair rotating toward the travel axis must stop being a transverse gap");
    require(result.minimumContinuousClearSeparationMeters > 0.0,
            "alignment fixture must remain geometrically separated");
    require(result.maximumContinuousAbsSeparationTravelDotBound > 0.5,
            "continuous alignment bound must exceed policy threshold");
}

void testBoundarySurfaceVelocityIncludesOmegaCrossR()
{
    Predictor::Query query = baseQuery();
    query.primary.angularVelocityMapRadPerSec = {0.0, 0.0, 1.0};

    const Predictor::Result result = Predictor::predict(query);

    require(result.status == Predictor::Status::OpenForHorizon,
            "rotating-boundary fixture must keep the static spherical gap open");
    requireNear(
        result.samples.front().primary.surfacePointTowardGapMapMeters.x,
        -3.0,
        1.0e-9,
        "primary contact surface point must use physical conservative radius"
    );
    requireNear(
        result.samples.front().primary.surfaceVelocityAtPointMapMetersPerSec.y,
        2.0,
        1.0e-9,
        "surface material velocity must include omega cross r"
    );
}

void testRevisionMismatchFailsBeforePrediction()
{
    Predictor::Query query = baseQuery();
    query.secondary.snapshotRevision = 8;

    const Predictor::Result result = Predictor::predict(query);

    require(result.validInput, "revision mismatch is structurally valid input");
    require(result.status == Predictor::Status::RevisionMismatch,
            "mixed snapshot revisions must fail closed");
    require(result.samplesEvaluated == 0,
            "revision mismatch must not run moving-gap prediction");
}

void testRelativeAccelerationChangesFutureGapState()
{
    Predictor::Query query = baseQuery();
    query.primary.linearAccelerationMapMetersPerSec2 = {-1.0, 0.0, 0.0};
    query.secondary.linearAccelerationMapMetersPerSec2 = {1.0, 0.0, 0.0};

    const Predictor::Result result = Predictor::predict(query);

    require(result.status == Predictor::Status::OpenForHorizon,
            "accelerating-apart boundaries must keep the gap open");
    requireNear(result.samples.back().gap.clearSeparationMeters, 7.0, 1.0e-9,
                "relative acceleration must change predicted gap width");
    requireNear(result.samples.back().separationRateMetersPerSec, 2.0, 1.0e-9,
                "future separation rate must include relative acceleration");
}

} // namespace

int main()
{
    try
    {
        testStaticGapRemainsOpenForWholeHorizon();
        testComovingGapCarriesGapCenterVelocity();
        testClosureBetweenSamplesIsRejectedContinuously();
        testGapThatRotatesIntoTravelDirectionLosesAlignment();
        testBoundarySurfaceVelocityIncludesOmegaCrossR();
        testRevisionMismatchFailsBeforePrediction();
        testRelativeAccelerationChangesFutureGapState();

        std::cout << "NAVIGATION TRAJECTORY MOVING GAP TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY MOVING GAP TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
