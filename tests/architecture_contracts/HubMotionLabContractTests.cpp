#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>

#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/entity/EntityType.h"
#include "src/game/motion/MotionModel.h"
#include "src/game/presentation/PresentationPolicyResolver.h"
#include "src/game/simulation/EntityRuntimeContract.h"

namespace
{

bool near(double a, double b, double eps = 1e-9)
{
    return std::abs(a - b) <= eps;
}

bool testThreeDistinctRemoteActors()
{
    using namespace game::diagnostics;

    if (HubMotionLabActors.size() != 3)
        return false;

    std::set<int> kinds;
    for (const auto& spec : HubMotionLabActors)
    {
        if (spec.kind == HubMotionLabActorKind::None)
            return false;

        if (spec.radiusMeters <= 0.0)
            return false;

        kinds.insert(static_cast<int>(spec.kind));
    }

    return kinds.size() == HubMotionLabActors.size();
}

bool testInitialProbesAreNearPlayerStart()
{
    using namespace game::diagnostics;

    const glm::dvec3 playerLocalStart {0.0, 2500.0, -9000.0};

    const auto slow = evaluateHubMotionLabActor(
        HubMotionLabActorKind::SlowOrbit,
        0.0
    );
    const auto fast = evaluateHubMotionLabActor(
        HubMotionLabActorKind::FastOrbit,
        0.0
    );

    return glm::length(slow.positionMeters - playerLocalStart) < 1000.0 &&
           glm::length(fast.positionMeters - playerLocalStart) < 2000.0;
}

bool testSlowAndFastHaveExpectedSpeeds()
{
    using namespace game::diagnostics;

    constexpr double t = 17.25;

    const auto slow = evaluateHubMotionLabActor(
        HubMotionLabActorKind::SlowOrbit,
        t
    );

    const auto fast = evaluateHubMotionLabActor(
        HubMotionLabActorKind::FastOrbit,
        t
    );

    const double slowSpeed = glm::length(slow.velocityMetersPerSecond);
    const double fastSpeed = glm::length(fast.velocityMetersPerSecond);

    return near(slowSpeed, 45.0, 1e-8) &&
           near(fastSpeed, 180.0, 1e-8) &&
           fastSpeed > slowSpeed * 3.0;
}

bool testMatchActorIsNearlyCoMoving()
{
    using namespace game::diagnostics;

    const glm::dvec3 playerPosition {125.0, -40.0, 900.0};
    const glm::dvec3 playerVelocity {120.0, 0.0, 0.0};

    const auto match = evaluateHubMotionLabActor(
        HubMotionLabActorKind::MatchPlayer,
        51.0,
        playerPosition,
        playerVelocity
    );

    const glm::dvec3 relativePosition =
        match.positionMeters - playerPosition;

    const glm::dvec3 relativeVelocity =
        match.velocityMetersPerSecond - playerVelocity;

    const auto* spec =
        hubMotionLabSpec(HubMotionLabActorKind::MatchPlayer);

    if (!spec)
        return false;

    const double horizontalRadius =
        std::sqrt(
            relativePosition.x * relativePosition.x +
            relativePosition.z * relativePosition.z
        );

    return near(horizontalRadius, spec->radiusMeters, 1e-8) &&
           near(relativePosition.y, spec->radialOffsetMeters, 1e-8) &&
           near(glm::length(relativeVelocity), 1.0, 1e-8);
}

bool testAnalyticCubeDoesNotDependOnSnapshotCadence()
{
    using namespace game::diagnostics;

    const auto a = evaluateHubMotionLabCube(10.0);
    const auto b = evaluateHubMotionLabCube(10.0 + 1.0 / 144.0);
    const auto c = evaluateHubMotionLabCube(10.0 + 2.0 / 144.0);

    if (a.halfExtentMeters <= 0.0)
        return false;

    const double d1 = glm::length(b.localPositionMeters - a.localPositionMeters);
    const double d2 = glm::length(c.localPositionMeters - b.localPositionMeters);

    return d1 > 0.0 &&
           d2 > 0.0 &&
           near(d1, d2, 1e-5) &&
           b.localRotationRadians > a.localRotationRadians &&
           c.localRotationRadians > b.localRotationRadians;
}

bool testRemoteNpcPresentationContractStaysInterpolated()
{
    game::simulation::EntityRuntimeContract contract;
    contract.entityType = game::entity::EntityType::Ship;
    contract.motionModel = game::motion::MotionModel::DynamicPhysics;
    contract.simulationMode = game::simulation::SimulationMode::Active;
    contract.authority = game::simulation::AuthorityPolicy::ServerAuthoritative;
    contract.timeline = game::simulation::TimelineDomain::ServerSimulation;

    const auto policy =
        game::presentation::resolvePresentationPolicy(
            contract,
            game::presentation::PresentationRole::RemoteObserved
        );

    return policy.has_value() &&
           *policy == game::presentation::PresentationPolicy::SnapshotInterpolated;
}

} // namespace

int main()
{
    struct TestCase
    {
        const char* name;
        bool (*fn)();
    };

    const TestCase tests[] = {
        {"three distinct remote actors", testThreeDistinctRemoteActors},
        {"initial probes are near player", testInitialProbesAreNearPlayerStart},
        {"slow/fast controlled speeds", testSlowAndFastHaveExpectedSpeeds},
        {"match actor nearly co-moving", testMatchActorIsNearlyCoMoving},
        {"analytic cube independent of snapshot cadence", testAnalyticCubeDoesNotDependOnSnapshotCadence},
        {"remote NPC presentation stays interpolated", testRemoteNpcPresentationContractStaysInterpolated},
    };

    int failures = 0;

    for (const auto& test : tests)
    {
        const bool ok = test.fn();
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        failures += ok ? 0 : 1;
    }

    std::cout
        << (static_cast<int>(sizeof(tests) / sizeof(tests[0])) - failures)
        << "/"
        << static_cast<int>(sizeof(tests) / sizeof(tests[0]))
        << " tests passed\n";

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
