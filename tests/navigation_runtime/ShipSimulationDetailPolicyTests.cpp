#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/game/simulation/ShipSimulationDetailPolicy.h"

namespace
{

using Fidelity = game::simulation::ShipMotionFidelity;
using Reason = game::simulation::ShipMotionFidelityReason;
using View = game::simulation::ShipObserverView;
using Policy = game::simulation::ShipMotionFidelityPolicy;
using Runtime = game::simulation::ShipMotionFidelityRuntimeState;
using Claim = game::simulation::activation::ActivationClaim;
using ClaimKind = game::simulation::activation::ActivationClaimKind;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

View observer(
    std::uint32_t id,
    const glm::dvec3& position,
    const glm::dvec3& forward
)
{
    View out;
    out.observerId = EntityId{id};
    out.systemId = 7;
    out.positionMeters = position;
    out.forward = forward;
    out.valid = true;
    return out;
}

void testBackgroundAndVisibilityBands()
{
    const std::vector<View> observers {
        observer(1, {0.0, 0.0, 0.0}, {0.0, 0.0, -1.0})
    };
    const std::vector<Claim> claims;

    auto demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {0.0, 0.0, 20000.0},
            observers,
            claims,
            1.0
        );
    require(
        demand.requested == Fidelity::RouteOnly,
        "offscreen distant ship was not RouteOnly"
    );

    demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {0.0, 0.0, -10000.0},
            observers,
            claims,
            1.0
        );
    require(
        demand.requested == Fidelity::Kinematic &&
        demand.reason == Reason::Visible,
        "visible distant ship was not Kinematic"
    );

    demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {1500.0, 0.0, -2500.0},
            observers,
            claims,
            1.0
        );
    require(
        demand.requested == Fidelity::Response,
        "observed near ship was not Response"
    );

    demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {0.0, 0.0, -2500.0},
            observers,
            claims,
            1.0
        );
    require(
        demand.requested == Fidelity::FullControl &&
        demand.reason == Reason::AimCone,
        "ship in aim cone was not FullControl"
    );
}

void testCloseShipMattersEvenBehindObserver()
{
    const std::vector<View> observers {
        observer(1, {0.0, 0.0, 0.0}, {0.0, 0.0, -1.0})
    };

    const auto demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {0.0, 0.0, 200.0},
            observers,
            {},
            1.0
        );

    require(
        demand.requested == Fidelity::FullControl &&
        demand.reason == Reason::CollisionProximity,
        "very close ship behind observer lost collision fidelity"
    );
}

void testStrongestOfSeveralPlayersWins()
{
    const std::vector<View> observers {
        observer(1, {0.0, 0.0, 0.0}, {0.0, 0.0, 1.0}),
        observer(2, {0.0, 0.0, -5000.0}, {0.0, 0.0, 1.0})
    };

    const auto demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {0.0, 0.0, -2500.0},
            observers,
            {},
            1.0
        );

    require(
        demand.requested == Fidelity::FullControl &&
        demand.observerId == EntityId{2},
        "shared target did not use strongest multiplayer observer demand"
    );
}

void testExplicitCombatClaimForcesServerFullControl()
{
    Claim claim;
    claim.subjectId = EntityId{10};
    claim.sourceId = EntityId{2};
    claim.systemId = 7;
    claim.minimumMode = game::simulation::SimulationMode::Active;
    claim.kind = ClaimKind::Combat;
    claim.expiresAtServerTimeSeconds = 20.0;

    const auto demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {50000.0, 0.0, 0.0},
            {},
            {claim},
            10.0
        );

    require(
        demand.requested == Fidelity::FullControl &&
        demand.reason == Reason::ExplicitInteraction,
        "combat claim did not force authoritative FullControl"
    );
}

void testPrewarmClaimRaisesOnlyToResponse()
{
    Claim claim;
    claim.subjectId = EntityId{10};
    claim.sourceId = EntityId{2};
    claim.systemId = 7;
    claim.minimumMode = game::simulation::SimulationMode::Prewarm;
    claim.kind = ClaimKind::Docking;
    claim.expiresAtServerTimeSeconds = 20.0;

    const auto demand =
        game::simulation::evaluateShipMotionFidelityDemand(
            EntityId{10},
            7,
            {50000.0, 0.0, 0.0},
            {},
            {claim},
            10.0
        );

    require(
        demand.requested == Fidelity::Response &&
        demand.reason == Reason::ExplicitInteraction,
        "Prewarm claim over-promoted shared ship beyond Response"
    );
}

void testClientObservationHintIsStrictlyPresentationBounded()
{
    Policy policy;

    require(
        game::simulation::sanitizeClientObservationDemand(
            Fidelity::FullControl
        ) == Fidelity::Response,
        "client observation hint was allowed to request FullControl"
    );
    require(
        game::simulation::sanitizeClientObservationDemand(
            Fidelity::Response
        ) == Fidelity::Response,
        "client Response hint changed"
    );
    require(
        game::simulation::sanitizeClientObservationDemand(
            Fidelity::Kinematic
        ) == Fidelity::Kinematic,
        "client Kinematic hint changed"
    );

    require(
        game::simulation::clientObservationDemandAllowed(
            Fidelity::Response,
            policy.responseVisibleDistanceMeters,
            policy
        ),
        "Response hint was rejected at its server distance boundary"
    );
    require(
        !game::simulation::clientObservationDemandAllowed(
            Fidelity::Response,
            policy.responseVisibleDistanceMeters + 1.0,
            policy
        ),
        "Response hint exceeded server-validated distance boundary"
    );
    require(
        game::simulation::clientObservationDemandAllowed(
            Fidelity::Kinematic,
            policy.kinematicVisibleDistanceMeters,
            policy
        ),
        "Kinematic hint was rejected at its server distance boundary"
    );
    require(
        !game::simulation::clientObservationDemandAllowed(
            Fidelity::Kinematic,
            policy.kinematicVisibleDistanceMeters + 1.0,
            policy
        ),
        "Kinematic hint exceeded server-validated distance boundary"
    );

    game::simulation::ShipClientObservationInterestState state;
    state.expiresAtServerTimeSeconds = 10.35;
    require(state.live(10.34), "fresh camera hint expired too early");
    require(state.live(10.35), "camera hint did not include expiry epoch");
    require(!state.live(10.36), "stale camera hint remained live");
}
void testShellCadenceScalesAcrossThreeCheapMotionLevels()
{
    Policy policy;

    require(
        game::simulation::shellMotionIntervalSeconds(
            Fidelity::RouteOnly, policy) ==
            policy.routeOnlyServerMotionIntervalSeconds,
        "RouteOnly shell cadence changed"
    );
    require(
        game::simulation::shellMotionIntervalSeconds(
            Fidelity::Kinematic, policy) ==
            policy.kinematicServerMotionIntervalSeconds,
        "Kinematic shell cadence changed"
    );
    require(
        game::simulation::shellMotionIntervalSeconds(
            Fidelity::Response, policy) ==
            policy.responseServerMotionIntervalSeconds,
        "Response shell cadence changed"
    );
    require(
        game::simulation::shellMotionIntervalSeconds(
            Fidelity::FullControl, policy) == 0.0,
        "FullControl transition tick was decimated"
    );
    require(
        policy.routeOnlyServerMotionIntervalSeconds >
            policy.kinematicServerMotionIntervalSeconds &&
        policy.kinematicServerMotionIntervalSeconds >
            policy.responseServerMotionIntervalSeconds,
        "cheap shell cadences are not ordered by observability"
    );
}
void testHysteresisPromotesImmediatelyAndDemotesInSteps()
{
    Policy policy;
    Runtime state;
    state.planned = Fidelity::RouteOnly;

    auto update =
        game::simulation::updateShipMotionFidelityPlan(
            state,
            Fidelity::FullControl,
            10.0,
            policy
        );
    require(
        update.changed && update.planned == Fidelity::FullControl,
        "FullControl promotion was not immediate"
    );

    update =
        game::simulation::updateShipMotionFidelityPlan(
            state,
            Fidelity::RouteOnly,
            10.5,
            policy
        );
    require(
        !update.changed && update.planned == Fidelity::FullControl,
        "FullControl demoted before release delay"
    );

    update =
        game::simulation::updateShipMotionFidelityPlan(
            state,
            Fidelity::RouteOnly,
            12.0,
            policy
        );
    require(
        update.planned == Fidelity::Response,
        "demotion skipped staged Response level"
    );

    update =
        game::simulation::updateShipMotionFidelityPlan(
            state,
            Fidelity::RouteOnly,
            15.0,
            policy
        );
    require(
        update.planned == Fidelity::Kinematic,
        "demotion skipped staged Kinematic level"
    );

    update =
        game::simulation::updateShipMotionFidelityPlan(
            state,
            Fidelity::RouteOnly,
            19.5,
            policy
        );
    require(
        update.planned == Fidelity::RouteOnly,
        "background ship did not eventually reach RouteOnly"
    );
}

} // namespace

int main()
{
    try
    {
        testBackgroundAndVisibilityBands();
        testCloseShipMattersEvenBehindObserver();
        testStrongestOfSeveralPlayersWins();
        testExplicitCombatClaimForcesServerFullControl();
        testPrewarmClaimRaisesOnlyToResponse();
        testClientObservationHintIsStrictlyPresentationBounded();
        testShellCadenceScalesAcrossThreeCheapMotionLevels();
        testHysteresisPromotesImmediatelyAndDemotesInSteps();

        std::cout
            << "SHIP SIMULATION DETAIL POLICY: PASS\n"
            << " - offscreen/background ships use RouteOnly\n"
            << " - visible ships rise through Kinematic/Response/FullControl\n"
            << " - close collision risk overrides camera direction\n"
            << " - strongest of several player observers wins\n"
            << " - Active combat claims force shared server FullControl\n"
            << " - Prewarm interaction claims require Response, not FullControl\n"
            << " - client camera hints are capped at Response, distance checked, and expire\n"
            << " - RouteOnly/Kinematic/Response shell cadences scale with observability\n"
            << " - promotion is immediate and demotion is hysteretic\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP SIMULATION DETAIL POLICY: FAIL: "
            << error.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
