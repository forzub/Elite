#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "src/game/client/presentation/LocalPredictedPresentation.h"

namespace
{

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[FAIL] " << message << '\n';
    std::exit(1);
}

void requireNear(
    double actual,
    double expected,
    double tolerance,
    const std::string& message
)
{
    if (std::abs(actual - expected) > tolerance)
    {
        fail(
            message +
            " actual=" + std::to_string(actual) +
            " expected=" + std::to_string(expected)
        );
    }
}

ShipParams zeroShipParams()
{
    ShipParams params {};
    params.maxPitchRate = 0.0f;
    params.maxYawRate = 0.0f;
    params.maxRollRate = 0.0f;
    params.angularAccel = 0.0f;
    params.angularDamping = 0.0f;
    params.maxCombatSpeed = 500.0f;
    params.maxCruiseSpeed = 500.0f;
    params.throttleAccel = 0.0f;
    params.autoLevelStrength = 0.0f;
    params.strafeAccel = 0.0f;
    params.strafeDamping = 0.0f;
    params.maxStrafeSpeed = 0.0f;
    return params;
}

ShipTransform makeHubTacticalTransform()
{
    ShipTransform transform;
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 0;
    transform.motion.hubId = "test_hub";
    transform.motion.localPositionMeters = glm::dvec3(0.0);
    transform.motion.localVelocityMps = glm::dvec3(100.0, 0.0, 0.0);
    transform.motion.worldVelocityMps = glm::dvec3(100.0, 0.0, 0.0);
    transform.setWorldPositionMeters(glm::dvec3(0.0));
    return transform;
}

game::simulation::ShipReferenceFrameSnapshot makeHubFrame()
{
    game::simulation::ShipReferenceFrameSnapshot frame;
    frame.systemId = 0;
    frame.type = game::navigation::MotionMode::HubTactical;
    frame.hubId = "test_hub";
    frame.originMeters = glm::dvec3(0.0);
    frame.velocityMetersPerSecond = glm::dvec3(0.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0);
    frame.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    frame.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    frame.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    frame.valid = true;
    return frame;
}

void testFractionalSampleAdvancesPresentationCopy()
{
    const ShipTransform fixed = makeHubTacticalTransform();
    const auto frame = makeHubFrame();
    const ShipParams params = zeroShipParams();
    const ShipControlState control {};
    const WorldParams world {};

    const ShipTransform sample =
        game::client::presentation::sampleLocalPredictedPresentationTarget(
            fixed,
            frame,
            params,
            control,
            world,
            0.010f,
            0.020f
        );

    requireNear(
        sample.motion.localPositionMeters.x,
        1.0,
        1.0e-6,
        "10 ms fractional sample at 100 m/s"
    );
    requireNear(
        fixed.motion.localPositionMeters.x,
        0.0,
        1.0e-12,
        "fixed predicted state must remain unchanged"
    );
}

void testFractionalSampleClampsToOneFixedStep()
{
    const ShipTransform fixed = makeHubTacticalTransform();
    const auto frame = makeHubFrame();
    const ShipParams params = zeroShipParams();
    const ShipControlState control {};
    const WorldParams world {};

    const ShipTransform sample =
        game::client::presentation::sampleLocalPredictedPresentationTarget(
            fixed,
            frame,
            params,
            control,
            world,
            0.050f,
            0.020f
        );

    requireNear(
        sample.motion.localPositionMeters.x,
        2.0,
        1.0e-6,
        "fractional presentation must never integrate beyond one fixed step"
    );
}

void testZeroRemainderIsIdentity()
{
    const ShipTransform fixed = makeHubTacticalTransform();
    const auto frame = makeHubFrame();
    const ShipParams params = zeroShipParams();
    const ShipControlState control {};
    const WorldParams world {};

    const ShipTransform sample =
        game::client::presentation::sampleLocalPredictedPresentationTarget(
            fixed,
            frame,
            params,
            control,
            world,
            0.0f,
            0.020f
        );

    requireNear(
        sample.motion.localPositionMeters.x,
        fixed.motion.localPositionMeters.x,
        1.0e-12,
        "zero remainder must not move presentation sample"
    );
}

using TestFunction = void (*)();
struct TestCase
{
    const char* name;
    TestFunction function;
};

} // namespace

int main()
{
    const std::vector<TestCase> tests {
        {"fractional sample advances presentation copy", testFractionalSampleAdvancesPresentationCopy},
        {"fractional sample clamps to one fixed step", testFractionalSampleClampsToOneFixedStep},
        {"zero remainder is identity", testZeroRemainderIsIdentity},
    };

    std::size_t passed = 0;
    for (const auto& test : tests)
    {
        test.function();
        ++passed;
        std::cout << "[PASS] " << test.name << '\n';
    }

    std::cout << passed << "/" << tests.size() << " tests passed\n";
    return 0;
}
