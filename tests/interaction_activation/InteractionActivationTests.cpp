#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/game/simulation/activation/InteractionHorizon.h"
#include "src/game/simulation/activation/SpatialBounds.h"

namespace
{
class TestFailure final : public std::runtime_error
{
public:
    explicit TestFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

[[noreturn]] void fail(const char* expression, const char* file, int line)
{
    throw TestFailure(
        std::string(file) + ':' + std::to_string(line) +
        ": check failed: " + expression
    );
}

#define REQUIRE(expression) \
    do \
    { \
        if (!(expression)) \
            fail(#expression, __FILE__, __LINE__); \
    } while (false)

bool approx(double a, double b, double eps = 1e-6)
{
    return std::abs(a - b) <= eps;
}

using game::simulation::activation::InteractionHorizonPolicy;
using game::simulation::activation::KinematicPoint;
using game::simulation::activation::SpatialBounds;

void testLogicalSizeProducesDifferentBroadBounds()
{
    LogicalDimensions cobra;
    cobra.enabled = true;
    cobra.length = 22.2f;
    cobra.width = 26.0f;
    cobra.height = 5.0f;

    LogicalDimensions station;
    station.enabled = true;
    station.length = 5021.38f;
    station.width = 4000.0f;
    station.height = 4089.56f;

    const auto cobraBounds =
        game::simulation::activation::makeSpatialBounds(cobra);
    const auto stationBounds =
        game::simulation::activation::makeSpatialBounds(station);

    REQUIRE(cobraBounds.interactionRadiusMeters > 10.0);
    REQUIRE(cobraBounds.interactionRadiusMeters < 25.0);
    REQUIRE(stationBounds.interactionRadiusMeters > 3000.0);
    REQUIRE(
        stationBounds.interactionRadiusMeters >
        cobraBounds.interactionRadiusMeters * 150.0
    );
}

void testLargeStationActivatesEarlierThanSmallShip()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;
    policy.safetyMarginMeters = 25.0;

    KinematicPoint observer;
    observer.bounds = SpatialBounds{18.0};

    KinematicPoint smallShip;
    smallShip.positionMeters = {3000.0, 0.0, 0.0};
    smallShip.bounds = SpatialBounds{18.0};

    KinematicPoint station = smallShip;
    station.bounds = SpatialBounds{3800.0};

    const auto shipPrediction =
        game::simulation::activation::evaluateInteractionHorizon(
            observer,
            smallShip,
            policy
        );

    const auto stationPrediction =
        game::simulation::activation::evaluateInteractionHorizon(
            observer,
            station,
            policy
        );

    REQUIRE(!shipPrediction.currentlyWithinEnvelope);
    REQUIRE(stationPrediction.currentlyWithinEnvelope);
}

void testFastClosingPairPrewarmsBeforeDistanceThreshold()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;
    policy.safetyMarginMeters = 25.0;

    KinematicPoint a;
    a.positionMeters = {-10000.0, 0.0, 0.0};
    a.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    a.bounds = SpatialBounds{18.0};

    KinematicPoint b;
    b.positionMeters = {10000.0, 0.0, 0.0};
    b.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{18.0};

    const auto prediction =
        game::simulation::activation::evaluateInteractionHorizon(a, b, policy);

    REQUIRE(!prediction.currentlyWithinEnvelope);
    REQUIRE(prediction.closingAtSampleTime);
    REQUIRE(prediction.entersEnvelopeWithinHorizon);
    REQUIRE(prediction.timeToClosestSeconds > 3.0);
    REQUIRE(prediction.timeToClosestSeconds < 3.5);
    REQUIRE(approx(prediction.closestCenterDistanceMeters, 0.0));
}

void testDivergingPairDoesNotPrewarm()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 5.0;

    KinematicPoint a;
    a.positionMeters = {-10000.0, 0.0, 0.0};
    a.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    a.bounds = SpatialBounds{18.0};

    KinematicPoint b;
    b.positionMeters = {10000.0, 0.0, 0.0};
    b.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{18.0};

    const auto prediction =
        game::simulation::activation::evaluateInteractionHorizon(a, b, policy);

    REQUIRE(!prediction.closingAtSampleTime);
    REQUIRE(!prediction.entersEnvelopeWithinHorizon);
    REQUIRE(approx(prediction.timeToClosestSeconds, 0.0));
}

void testShortHorizonDoesNotWakeFutureCollisionTooEarly()
{
    InteractionHorizonPolicy policy;
    policy.lookAheadSeconds = 2.0;
    policy.safetyMarginMeters = 25.0;

    KinematicPoint a;
    a.positionMeters = {-10000.0, 0.0, 0.0};
    a.velocityMetersPerSecond = {3000.0, 0.0, 0.0};
    a.bounds = SpatialBounds{18.0};

    KinematicPoint b;
    b.positionMeters = {10000.0, 0.0, 0.0};
    b.velocityMetersPerSecond = {-3000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{18.0};

    const auto prediction =
        game::simulation::activation::evaluateInteractionHorizon(a, b, policy);

    REQUIRE(prediction.closingAtSampleTime);
    REQUIRE(!prediction.entersEnvelopeWithinHorizon);
    REQUIRE(approx(prediction.timeToClosestSeconds, 2.0));
}

void testGameplayRangeIsIndependentFromPhysicalRadius()
{
    InteractionHorizonPolicy physicalOnly;
    physicalOnly.lookAheadSeconds = 5.0;
    physicalOnly.safetyMarginMeters = 0.0;

    InteractionHorizonPolicy weaponCapable = physicalOnly;
    weaponCapable.gameplayRangeMeters = 5000.0;

    KinematicPoint a;
    a.bounds = SpatialBounds{20.0};

    KinematicPoint b;
    b.positionMeters = {4000.0, 0.0, 0.0};
    b.bounds = SpatialBounds{20.0};

    const auto noWeapon =
        game::simulation::activation::evaluateInteractionHorizon(
            a,
            b,
            physicalOnly
        );

    const auto weapon =
        game::simulation::activation::evaluateInteractionHorizon(
            a,
            b,
            weaponCapable
        );

    REQUIRE(!noWeapon.currentlyWithinEnvelope);
    REQUIRE(weapon.currentlyWithinEnvelope);
}

} // namespace

int main()
{
    int passed = 0;

    const auto run = [&](const char* name, auto&& test)
    {
        try
        {
            test();
            ++passed;
            std::cout << "[PASS] " << name << '\n';
        }
        catch (const std::exception& e)
        {
            std::cerr << "[FAIL] " << name << ": " << e.what() << '\n';
            throw;
        }
    };

    run("logical size produces different broad bounds", testLogicalSizeProducesDifferentBroadBounds);
    run("large station activates earlier than small ship", testLargeStationActivatesEarlierThanSmallShip);
    run("fast closing pair prewarms before range", testFastClosingPairPrewarmsBeforeDistanceThreshold);
    run("diverging pair stays asleep", testDivergingPairDoesNotPrewarm);
    run("short horizon does not wake too early", testShortHorizonDoesNotWakeFutureCollisionTooEarly);
    run("gameplay range remains separate from size", testGameplayRangeIsIndependentFromPhysicalRadius);

    std::cout << passed << "/6 tests passed\n";
    return passed == 6 ? 0 : 1;
}
