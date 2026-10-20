#include "src/game/navigation/AssistedConcurrentManeuverBudget.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/telemetry/ReplicatedAssistedControlAuthority.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
void require(bool condition, const char* description)
{
    if (!condition)
        throw std::runtime_error(description);
}

ShipParams nominal()
{
    ShipParams p {};
    p.maxGs = 10.0f;
    p.maxLinearGs = 10.0f;
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 60.0f;
    p.reverseMainEngineAccelerationMps2 = 60.0f;
    p.strafeAccel = 60.0f;
    p.angularAccel = 3.0f;
    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 10000000.0;
    p.yawInertiaKgM2 = 10000000.0;
    p.rollInertiaKgM2 = 10000000.0;
    return p;
}

using Telemetry =
    game::ship::telemetry::ShipControlAuthorityTelemetry;

Telemetry healthyAuthority()
{
    Telemetry t;
    t.valid = true;
    t.model = game::ship::plant::PhysicalControlModelId::AssistedI;
    t.positive = {40.0, 40.0, 60.0, 3.0, 3.0, 3.0};
    t.negative = {40.0, 40.0, 60.0, 3.0, 3.0, 3.0};
    return t;
}

void testNoUndamagedDescriptorFallbackAfterMotorLoss()
{
    auto t = healthyAuthority();
    t.negative[2] = 0.0; // forward aft-bank destroyed
    const ShipParams p =
        game::ship::telemetry::applyAuthorityToControlParams(nominal(), t);
    require(!p.forwardMainEngineAvailable,
            "destroyed motor bank retained descriptor available flag");
    require(game::ship::forwardMainAccelerationLimitMps2(p) == 0.0,
            "destroyed motor bank resurrected load-envelope thrust");
    require(p.reverseMainEngineAvailable,
            "live fore main was wrongly disconnected");
    require(std::abs(
        game::ship::reverseMainAccelerationLimitMps2(p) - 60.0
    ) < 1.0e-3, "healthy reverse bank lost rated authority");
}

void testSignedDamageCannotBeHiddenByHealthyOppositeAxis()
{
    auto t = healthyAuthority();
    t.positive[0] = 40.0;
    t.negative[0] = 6.0;
    t.negative[4] = 0.7;
    const ShipParams p =
        game::ship::telemetry::applyAuthorityToControlParams(nominal(), t);
    require(std::abs(p.strafeAccel - 6.0f) < 1.0e-5f,
            "lateral preview used stronger direction despite damage");
    require(std::abs(p.angularAccel - 0.7f) < 1.0e-5f,
            "angular preview used nominal yaw despite damage");
}

void testDamageInvalidatesAcceptedRoute()
{
    const auto original = healthyAuthority();
    auto live = original;
    live.negative[4] *= 0.70;
    require(
        game::ship::telemetry::hasSignificantAssistedAuthorityLoss(
            original, live
        ),
        "impact-damaged yaw cannot invalidate committed route"
    );
    live = original;
    live.positive[0] *= 0.98;
    require(
        !game::ship::telemetry::hasSignificantAssistedAuthorityLoss(
            original, live
        ),
        "2 percent thermal variation caused spurious route abort"
    );
    live.valid = false;
    require(
        game::ship::telemetry::hasSignificantAssistedAuthorityLoss(
            original, live
        ),
        "missing live motor telemetry was treated as healthy"
    );
}

void testCombinedMotorBudgetBoundsTangentSpeed()
{
    const double v =
        game::navigation::assisted_budget::
            maximumSustainableCurveSpeedMps(60.0, 0.002);
    require(std::abs(v - 100.0) < 1.0e-9,
            "v^2*kappa failed shared lateral/braking/turn budget");
    require(game::navigation::assisted_budget::
        maximumSustainableCurveSpeedMps(0.0, 0.002) == 0.0,
        "destroyed lateral authority still allowed positive curve speed");
}

void testActualHealthyCobraMotorBasis()
{
    using namespace game::ship::plant;
    using namespace game::equipment::engine;
    const auto model = PhysicalControlModelId::AssistedI;
    const auto& topology = physicalControlSlots(model);
    require(topology.size() == 4u,
            "Assisted-I Cobra topology has unexpected engine count");

    ShipParams ship = nominal();
    ship.maxGs = 20.0f;
    ship.maxLinearGs = 20.0f;
    ship.forwardMainEngineAccelerationMps2 = 73.549875f;
    ship.reverseMainEngineAccelerationMps2 = 73.549875f;
    ship.strafeAccel = 73.549875f;
    ship.physicalActuatorHalfExtentsMeters = {13.0, 2.5, 11.1};
    ship.pitchInertiaKgM2 = 11219866.6666667;
    ship.yawInertiaKgM2 = 25324866.6666667;
    ship.rollInertiaKgM2 = 15188333.3333333;

    const auto passport = presets::CobrettyCR1VectorMainDrive();
    std::array<game::equipment::EquipmentDamageLayer,4> damage {};
    std::array<EngineOperatingState,4> operating {};
    std::vector<MountedEngine> installed;
    installed.reserve(topology.size());
    for (std::size_t i=0;i<topology.size();++i)
    {
        damage[i].failure.failureAtDamage01 = 0.95;
        operating[i].temperatureK =
            passport.nominalOperatingTemperatureK;
        installed.push_back({
            &passport, &damage[i], &operating[i], topology[i].id, true
        });
    }

    AssistedManeuverPrograms programs;
    programs.build(model, installed, ship);
    const auto telemetry =
        game::ship::telemetry::readAssistedControlAuthority(programs, 1u);
    require(telemetry.valid,
            "actual installed healthy Cobra motor basis did not build");
    for (std::size_t axis : {std::size_t{0}, std::size_t{1},
                             std::size_t{2}, std::size_t{3},
                             std::size_t{4}})
    {
        require(telemetry.positive[axis] > 1.0e-6 &&
                telemetry.negative[axis] > 1.0e-6,
                "actual healthy Cobra lost a signed maneuver axis");
    }
    // The isolated motor-basis test is not a game CTest: prove at least the
    // four-motor hardware solver produces meaningful limits, not fake axes.
    installed[0].attached = false;
    programs.build(model, installed, ship);
    const auto damaged =
        game::ship::telemetry::readAssistedControlAuthority(programs, 2u);
    require(damaged.valid &&
            damaged.negative[2] < telemetry.negative[2],
            "destroyed aft motor failed to reduce physical forward authority");
}

void testAbsentReplicatedEngineDataCannotInventAuthority()
{
    ShipDescriptor descriptor;
    descriptor.physicalControlModel =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    game::ship::plant::AssistedManeuverPrograms cache;
    const auto projected =
        game::ship::telemetry::readReplicatedAssistedControlAuthority(
            descriptor,
            game::ship::plant::PhysicalControlModelId::AssistedI,
            std::vector<game::simulation::ObjectModuleSnapshot>{},
            nominal(),
            cache
        );
    require(!projected.valid,
            "missing installed motors were reconstructed as healthy");
}

} // namespace

int main()
{
    try
    {
        testNoUndamagedDescriptorFallbackAfterMotorLoss();
        testSignedDamageCannotBeHiddenByHealthyOppositeAxis();
        testDamageInvalidatesAcceptedRoute();
        testCombinedMotorBudgetBoundsTangentSpeed();
        testActualHealthyCobraMotorBasis();
        testAbsentReplicatedEngineDataCannotInventAuthority();
        std::cout << "ASSISTED AUTHORITY CONTRACT: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "ASSISTED AUTHORITY CONTRACT: FAIL: "
                  << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
