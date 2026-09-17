#include "world/navigation/trajectory/OrientedPassageEvaluator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

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

Passage::HullProxy flatHull()
{
    Passage::HullProxy hull;
    hull.halfExtentsBodyMeters = {4.0, 1.0, 6.0};
    hull.additionalClearanceMeters = 0.0;
    return hull;
}

Passage::Passage flatSlot()
{
    Passage::Passage slot;
    slot.source = Passage::PassageSource::AuthoredAperture;
    slot.centerMapMeters = {0.0, 0.0, 0.0};
    slot.passageToMap = {};
    slot.halfWidthMeters = 4.5;
    slot.halfHeightMeters = 1.5;
    return slot;
}

void testFlatHullFitsFlatSlotWhileSphereWouldReject()
{
    const Passage::Result result = Passage::evaluate(
        flatHull(),
        Passage::Pose {},
        flatSlot()
    );

    require(result.status == Passage::Status::Fits && result.fits,
            "aligned flat hull must fit the flat slot");
    require(!result.conservativeSphereFits,
            "conservative sphere must demonstrate why precision fallback exists");
    require(near(result.projectedHalfWidthMeters, 4.0),
            "aligned width projection mismatch");
    require(near(result.projectedHalfHeightMeters, 1.0),
            "aligned height projection mismatch");
}

void testNinetyDegreeRollRejectsSameFlatSlot()
{
    Passage::Pose pose;
    // +90 degree roll around forward/Z: body X points +Y, body Y points -X.
    pose.bodyToMap.right = {0.0, 1.0, 0.0};
    pose.bodyToMap.up = {-1.0, 0.0, 0.0};
    pose.bodyToMap.forward = {0.0, 0.0, 1.0};

    const Passage::Result result = Passage::evaluate(flatHull(), pose, flatSlot());

    require(result.status == Passage::Status::TooTall && !result.fits,
            "rolled flat hull must be rejected by the low slot");
    require(near(result.projectedHalfHeightMeters, 4.0),
            "rolled height projection mismatch");
}

void testTwoObstacleGapBecomesPositivePassageCandidate()
{
    Passage::ObstacleGap gap;
    gap.centerMapMeters = {20.0, 0.0, 0.0};
    gap.travelDirectionMap = {0.0, 0.0, 1.0};
    gap.separationAxisMap = {1.0, 0.0, 0.0};
    gap.clearSeparationMeters = 3.0;
    gap.secondaryClearanceMeters = 10.0;

    const Passage::Passage candidate = Passage::makeObstacleGapPassage(gap);

    Passage::Pose aligned;
    aligned.centerMapMeters = gap.centerMapMeters;
    const Passage::Result alignedResult = Passage::evaluate(
        flatHull(), aligned, candidate
    );
    require(alignedResult.status == Passage::Status::TooWide,
            "wide attitude must not fit narrow obstacle gap");

    Passage::Pose rolled;
    rolled.centerMapMeters = gap.centerMapMeters;
    rolled.bodyToMap.right = {0.0, 1.0, 0.0};
    rolled.bodyToMap.up = {-1.0, 0.0, 0.0};
    rolled.bodyToMap.forward = {0.0, 0.0, 1.0};

    const Passage::Result rolledResult = Passage::evaluate(
        flatHull(), rolled, candidate
    );
    require(candidate.source == Passage::PassageSource::ObstacleGap,
            "derived candidate must retain obstacle-gap ownership");
    require(rolledResult.status == Passage::Status::Fits && rolledResult.fits,
            "two nearby obstacles must be usable as a slot when the hull rolls to fit");
    require(!rolledResult.conservativeSphereFits,
            "obstacle-gap precision case must remain invisible to sphere-only fit");
}

void testOffCenterHullFailsEvenWhenOrientationFits()
{
    Passage::Pose pose;
    pose.centerMapMeters = {0.6, 0.0, 0.0};

    const Passage::Result result = Passage::evaluate(flatHull(), pose, flatSlot());

    require(result.status == Passage::Status::OffsetOutside && !result.fits,
            "orientation alone must not excuse excessive lateral offset");
    require(result.widthClearanceMeters < 0.0,
            "off-center fixture must report negative width clearance");
}

void testDegenerateObstacleGapFailsClosed()
{
    Passage::ObstacleGap gap;
    gap.travelDirectionMap = {0.0, 0.0, 1.0};
    gap.separationAxisMap = {0.0, 0.0, 1.0};
    gap.clearSeparationMeters = 10.0;
    gap.secondaryClearanceMeters = 10.0;

    const Passage::Passage candidate = Passage::makeObstacleGapPassage(gap);
    const Passage::Result result = Passage::evaluate(
        flatHull(), Passage::Pose {}, candidate
    );

    require(result.status == Passage::Status::InvalidInput && !result.fits,
            "parallel travel/separation axes must fail closed");
}

} // namespace

int main()
{
    try
    {
        testFlatHullFitsFlatSlotWhileSphereWouldReject();
        testNinetyDegreeRollRejectsSameFlatSlot();
        testTwoObstacleGapBecomesPositivePassageCandidate();
        testOffCenterHullFailsEvenWhenOrientationFits();
        testDegenerateObstacleGapFailsClosed();

        std::cout << "NAVIGATION TRAJECTORY ORIENTED PASSAGE TESTS: PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NAVIGATION TRAJECTORY ORIENTED PASSAGE TESTS: FAIL: "
                  << error.what() << '\n';
        return 1;
    }
}
