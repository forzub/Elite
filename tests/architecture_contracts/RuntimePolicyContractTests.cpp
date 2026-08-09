#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "src/game/entity/EntityType.h"
#include "src/game/motion/MotionModel.h"
#include "src/game/presentation/PresentationPolicy.h"
#include "src/game/presentation/PresentationPolicyResolver.h"
#include "src/game/simulation/AuthorityPolicy.h"
#include "src/game/simulation/EntityRuntimeContract.h"
#include "src/game/simulation/EntityRuntimeTransition.h"
#include "src/game/simulation/SimulationMode.h"
#include "src/game/simulation/TimelineDomain.h"

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

void testRuntimeContractsSeparateEntityMotionAndSimulation()
{
    using game::entity::EntityType;
    using game::motion::MotionModel;
    using game::simulation::AuthorityPolicy;
    using game::simulation::EntityRuntimeContract;
    using game::simulation::RuntimeContractError;
    using game::simulation::SimulationMode;
    using game::simulation::TimelineDomain;

    const EntityRuntimeContract planet {
        EntityType::CelestialBody,
        MotionModel::Orbital,
        SimulationMode::OnDemand,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::Universe
    };
    REQUIRE(game::simulation::isRuntimeContractValid(planet));

    const EntityRuntimeContract hub {
        EntityType::Hub,
        MotionModel::Orbital,
        SimulationMode::OnDemand,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::Universe
    };
    REQUIRE(game::simulation::isRuntimeContractValid(hub));

    const EntityRuntimeContract ringOnDemand {
        EntityType::StationModule,
        MotionModel::Kinematic,
        SimulationMode::OnDemand,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    const EntityRuntimeContract ringActiveCollision {
        EntityType::StationModule,
        MotionModel::Kinematic,
        SimulationMode::Active,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    REQUIRE(game::simulation::isRuntimeContractValid(ringOnDemand));
    REQUIRE(game::simulation::isRuntimeContractValid(ringActiveCollision));

    const EntityRuntimeContract scheduledCargo {
        EntityType::Ship,
        MotionModel::ScheduledTrajectory,
        SimulationMode::Scheduled,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    REQUIRE(game::simulation::isRuntimeContractValid(scheduledCargo));

    auto illegalScheduledPhysics = scheduledCargo;
    illegalScheduledPhysics.motionModel = MotionModel::DynamicPhysics;
    REQUIRE(
        game::simulation::validateRuntimeContract(illegalScheduledPhysics) ==
        RuntimeContractError::IllegalSimulationModeForMotion
    );

    auto illegalPlanetPhysics = planet;
    illegalPlanetPhysics.motionModel = MotionModel::DynamicPhysics;
    illegalPlanetPhysics.simulationMode = SimulationMode::Active;
    illegalPlanetPhysics.timeline = TimelineDomain::ServerSimulation;
    REQUIRE(
        game::simulation::validateRuntimeContract(illegalPlanetPhysics) ==
        RuntimeContractError::IllegalEntityMotionPair
    );

    auto wrongRingTimeline = ringActiveCollision;
    wrongRingTimeline.timeline = TimelineDomain::Universe;
    REQUIRE(
        game::simulation::validateRuntimeContract(wrongRingTimeline) ==
        RuntimeContractError::IllegalTimelineForMotion
    );
}

void testPresentationPolicyDependsOnObserverRole()
{
    using game::entity::EntityType;
    using game::motion::MotionModel;
    using game::presentation::PresentationPolicy;
    using game::presentation::PresentationRole;
    using game::simulation::AuthorityPolicy;
    using game::simulation::EntityRuntimeContract;
    using game::simulation::SimulationMode;
    using game::simulation::TimelineDomain;

    const EntityRuntimeContract playerShip {
        EntityType::Ship,
        MotionModel::DynamicPhysics,
        SimulationMode::Active,
        AuthorityPolicy::ServerAuthoritativeWithClientPrediction,
        TimelineDomain::ServerSimulation
    };

    const auto local = game::presentation::resolvePresentationPolicy(
        playerShip,
        PresentationRole::LocalControlled
    );
    REQUIRE(local.has_value());
    REQUIRE(*local == PresentationPolicy::LocalPredicted);

    const auto sameShipForOtherClient =
        game::presentation::resolvePresentationPolicy(
            playerShip,
            PresentationRole::RemoteObserved
        );
    REQUIRE(sameShipForOtherClient.has_value());
    REQUIRE(
        *sameShipForOtherClient ==
        PresentationPolicy::SnapshotInterpolated
    );

    const EntityRuntimeContract npc {
        EntityType::Ship,
        MotionModel::DynamicPhysics,
        SimulationMode::Active,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    REQUIRE(
        !game::presentation::resolvePresentationPolicy(
            npc,
            PresentationRole::LocalControlled
        ).has_value()
    );
    REQUIRE(
        game::presentation::presentationPolicyAllowed(
            npc,
            PresentationRole::RemoteObserved,
            PresentationPolicy::SnapshotInterpolated
        )
    );
}

void testAnalyticPresentationIsReservedForAnalyticMotion()
{
    using game::entity::EntityType;
    using game::motion::MotionModel;
    using game::presentation::PresentationPolicy;
    using game::presentation::PresentationRole;
    using game::simulation::AuthorityPolicy;
    using game::simulation::EntityRuntimeContract;
    using game::simulation::SimulationMode;
    using game::simulation::TimelineDomain;

    const EntityRuntimeContract ring {
        EntityType::StationModule,
        MotionModel::Kinematic,
        SimulationMode::Active,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    REQUIRE(
        game::presentation::presentationPolicyAllowed(
            ring,
            PresentationRole::RemoteObserved,
            PresentationPolicy::Analytic
        )
    );
    REQUIRE(
        !game::presentation::presentationPolicyAllowed(
            ring,
            PresentationRole::RemoteObserved,
            PresentationPolicy::SnapshotInterpolated
        )
    );

    const EntityRuntimeContract scheduledShip {
        EntityType::Ship,
        MotionModel::ScheduledTrajectory,
        SimulationMode::Scheduled,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    REQUIRE(
        game::presentation::presentationPolicyAllowed(
            scheduledShip,
            PresentationRole::RemoteObserved,
            PresentationPolicy::Analytic
        )
    );

    const EntityRuntimeContract diagnosticCube {
        EntityType::DiagnosticProbe,
        MotionModel::Kinematic,
        SimulationMode::OnDemand,
        AuthorityPolicy::PresentationOnly,
        TimelineDomain::ServerSimulation
    };
    REQUIRE(
        game::presentation::presentationPolicyAllowed(
            diagnosticCube,
            PresentationRole::DiagnosticReference,
            PresentationPolicy::Analytic
        )
    );
    REQUIRE(
        !game::presentation::resolvePresentationPolicy(
            diagnosticCube,
            PresentationRole::RemoteObserved
        ).has_value()
    );
}

void testSimulationLifecycleRequiresMaterializationFence()
{
    using game::entity::EntityType;
    using game::motion::MotionModel;
    using game::simulation::AuthorityPolicy;
    using game::simulation::EntityRuntimeContract;
    using game::simulation::RuntimeTransitionError;
    using game::simulation::SimulationMode;
    using game::simulation::TimelineDomain;

    const EntityRuntimeContract scheduled {
        EntityType::Ship,
        MotionModel::ScheduledTrajectory,
        SimulationMode::Scheduled,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    const EntityRuntimeContract scheduledPrewarm {
        EntityType::Ship,
        MotionModel::ScheduledTrajectory,
        SimulationMode::Prewarm,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    const EntityRuntimeContract dynamicPrewarm {
        EntityType::Ship,
        MotionModel::DynamicPhysics,
        SimulationMode::Prewarm,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    const EntityRuntimeContract active {
        EntityType::Ship,
        MotionModel::DynamicPhysics,
        SimulationMode::Active,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };

    REQUIRE(game::simulation::isRuntimeTransitionValid(scheduled, scheduledPrewarm));
    REQUIRE(game::simulation::isRuntimeTransitionValid(scheduledPrewarm, dynamicPrewarm));
    REQUIRE(game::simulation::isRuntimeTransitionValid(dynamicPrewarm, active));

    REQUIRE(
        game::simulation::validateRuntimeTransition(scheduled, active) ==
        RuntimeTransitionError::IllegalSimulationModeTransition
    );

    const EntityRuntimeContract dynamicCoarse {
        EntityType::Ship,
        MotionModel::DynamicPhysics,
        SimulationMode::Coarse,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };
    const EntityRuntimeContract trajectoryCoarse {
        EntityType::Ship,
        MotionModel::ScheduledTrajectory,
        SimulationMode::Coarse,
        AuthorityPolicy::ServerAuthoritative,
        TimelineDomain::ServerSimulation
    };

    REQUIRE(game::simulation::isRuntimeTransitionValid(active, dynamicCoarse));
    REQUIRE(game::simulation::isRuntimeTransitionValid(dynamicCoarse, trajectoryCoarse));
    REQUIRE(
        game::simulation::canTransitionSimulationMode(
            trajectoryCoarse.simulationMode,
            SimulationMode::Scheduled
        )
    );

    auto changedIdentity = dynamicPrewarm;
    changedIdentity.entityType = EntityType::Asteroid;
    REQUIRE(
        game::simulation::validateRuntimeTransition(
            scheduledPrewarm,
            changedIdentity
        ) == RuntimeTransitionError::EntityTypeChanged
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
        {
            "runtime contracts separate entity motion and simulation",
            testRuntimeContractsSeparateEntityMotionAndSimulation
        },
        {
            "presentation policy depends on observer role",
            testPresentationPolicyDependsOnObserverRole
        },
        {
            "analytic presentation is reserved for analytic motion",
            testAnalyticPresentationIsReservedForAnalyticMotion
        },
        {
            "simulation lifecycle requires materialization fence",
            testSimulationLifecycleRequiresMaterializationFence
        }
    };

    int failed = 0;
    for (const TestCase& test : tests)
    {
        try
        {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        }
        catch (const std::exception& error)
        {
            ++failed;
            std::cerr
                << "[FAIL] " << test.name
                << "\n       " << error.what()
                << '\n';
        }
    }

    std::cout
        << '\n'
        << (tests.size() - static_cast<std::size_t>(failed))
        << '/' << tests.size()
        << " tests passed\n";

    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
