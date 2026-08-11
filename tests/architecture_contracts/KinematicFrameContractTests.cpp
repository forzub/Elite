#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/HubNavigationFrame.h"
#include "src/game/navigation/KinematicFrame.h"

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

[[noreturn]] void fail(
    const char* expression,
    const char* file,
    int line)
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

void requireVecNear(
    const glm::dvec3& actual,
    const glm::dvec3& expected,
    double epsilon,
    const char* expression,
    const char* file,
    int line)
{
    if (glm::length(actual - expected) > epsilon)
        fail(expression, file, line);
}

#define REQUIRE_VEC_NEAR(actual, expected, epsilon) \
    requireVecNear((actual), (expected), (epsilon), #actual, __FILE__, __LINE__)

game::navigation::KinematicFrame makeFrameA()
{
    game::navigation::KinematicFrame frame;
    frame.systemId = 7;
    frame.frameId = "frame_a";
    frame.originMeters = glm::dvec3(1.2e8, -3.4e7, 8.0e6);
    frame.linearVelocityMps = glm::dvec3(12000.0, -500.0, 300.0);
    frame.linearAccelerationMps2 = glm::dvec3(-0.4, 7.8, 0.2);

    // local X -> world +Y, local Y -> world -X, local Z -> world +Z
    frame.localToWorldBasis = glm::dmat3(
        glm::dvec3(0.0, 1.0, 0.0),
        glm::dvec3(-1.0, 0.0, 0.0),
        glm::dvec3(0.0, 0.0, 1.0)
    );

    frame.angularVelocityWorldRadPerSecond =
        glm::dvec3(0.0, 0.0, -0.0012);
    frame.angularAccelerationWorldRadPerSecond2 =
        glm::dvec3(0.0, 0.0, 0.00003);
    frame.valid = true;
    return frame;
}

game::navigation::KinematicFrame makeFrameB()
{
    game::navigation::KinematicFrame frame;
    frame.systemId = 7;
    frame.frameId = "frame_b";
    frame.originMeters = glm::dvec3(1.2e8 + 5000.0, -3.4e7 - 2500.0, 8.0e6 + 100.0);
    frame.linearVelocityMps = glm::dvec3(11950.0, -440.0, 320.0);
    frame.linearAccelerationMps2 = glm::dvec3(-0.6, 7.5, 0.1);
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.angularVelocityWorldRadPerSecond =
        glm::dvec3(0.0, 0.0, 0.0008);
    frame.angularAccelerationWorldRadPerSecond2 =
        glm::dvec3(0.0, 0.0, -0.00002);
    frame.valid = true;
    return frame;
}

void testPositionVelocityAccelerationRoundTrip()
{
    const auto frame = makeFrameA();

    const game::navigation::LocalKinematicState local {
        glm::dvec3(2500.0, -700.0, 90.0),
        glm::dvec3(120.0, -30.0, 4.0),
        glm::dvec3(2.0, 0.5, -0.25)
    };

    const auto world =
        game::navigation::localToWorldKinematics(frame, local);
    const auto restored =
        game::navigation::worldToLocalKinematics(frame, world);

    REQUIRE_VEC_NEAR(restored.positionMeters, local.positionMeters, 1.0e-8);
    REQUIRE_VEC_NEAR(restored.velocityMps, local.velocityMps, 1.0e-9);
    REQUIRE_VEC_NEAR(restored.accelerationMps2, local.accelerationMps2, 1.0e-9);
}

void testReferenceFrameRebasePreservesPhysicalState()
{
    const auto from = makeFrameA();
    const auto to = makeFrameB();

    const game::navigation::LocalKinematicState source {
        glm::dvec3(-400.0, 1200.0, 75.0),
        glm::dvec3(22.0, 4.0, -3.0),
        glm::dvec3(0.4, -0.2, 0.1)
    };

    game::navigation::LocalKinematicState rebased;
    REQUIRE(game::navigation::rebaseLocalKinematics(from, to, source, rebased));

    const auto worldBefore =
        game::navigation::localToWorldKinematics(from, source);
    const auto worldAfter =
        game::navigation::localToWorldKinematics(to, rebased);

    REQUIRE_VEC_NEAR(worldAfter.positionMeters, worldBefore.positionMeters, 1.0e-7);
    REQUIRE_VEC_NEAR(worldAfter.velocityMps, worldBefore.velocityMps, 1.0e-9);
    REQUIRE_VEC_NEAR(worldAfter.accelerationMps2, worldBefore.accelerationMps2, 1.0e-9);
}

void testDifferentSystemsCannotBeRebasedAsLocalMeters()
{
    const auto from = makeFrameA();
    auto to = makeFrameB();
    to.systemId = 8;

    game::navigation::LocalKinematicState result;
    REQUIRE(!game::navigation::rebaseLocalKinematics(
        from,
        to,
        game::navigation::LocalKinematicState{},
        result
    ));
}

void testHubAdapterMatchesExistingHubTransform()
{
    game::navigation::HubNavigationFrame hub;
    hub.systemId = 3;
    hub.hubId = "orbital_hub";
    hub.originMeters = glm::dvec3(1000.0, 2000.0, 3000.0);
    hub.velocityMetersPerSecond = glm::dvec3(10.0, 20.0, 30.0);
    hub.accelerationMetersPerSecond2 = glm::dvec3(0.1, 0.2, 0.3);
    hub.progradeAxis = glm::dvec3(1.0, 0.0, 0.0);
    hub.radialAxis = glm::dvec3(0.0, 1.0, 0.0);
    hub.normalAxis = glm::dvec3(0.0, 0.0, 1.0);
    hub.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, 0.01);
    hub.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.0, 0.0, 0.001);
    hub.valid = true;

    const auto generic = hub.kinematicFrame();
    const glm::dvec3 localPosition(50.0, -20.0, 4.0);
    const glm::dvec3 localVelocity(3.0, 2.0, -1.0);

    REQUIRE_VEC_NEAR(
        generic.localToWorldPosition(localPosition),
        hub.localToWorldPosition(localPosition),
        0.0
    );
    REQUIRE_VEC_NEAR(
        generic.localToWorldVelocity(localPosition, localVelocity),
        hub.localToWorldVelocity(localPosition, localVelocity),
        0.0
    );
}
}

int main()
{
    try
    {
        testPositionVelocityAccelerationRoundTrip();
        testReferenceFrameRebasePreservesPhysicalState();
        testDifferentSystemsCannotBeRebasedAsLocalMeters();
        testHubAdapterMatchesExistingHubTransform();

        std::cout << "Kinematic-frame contracts: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Kinematic-frame contracts: FAIL: "
                  << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
