#include "src/game/navigation/AssistedConcurrentManeuverBudget.h"
#include "src/game/navigation/autopilot/AssistedCorridorDeviationPolicy.h"
#include "src/game/navigation/autopilot/AssistedCorridorSpeedGovernor.h"
#include "src/game/navigation/autopilot/AssistedAutopilot.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/telemetry/ReplicatedAssistedControlAuthority.h"
#include "src/game/ship/telemetry/AssistedCoupledAcceleration.h"

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

void testCoupledEngineAuthorityRejectsIndependentAxisFantasy()
{
    using namespace game::ship::telemetry;
    AssistedCoupledMotorAuthority motor;
    motor.valid=true;
    motor.motorCount=1;
    motor.available01[0]=1.0f;
    motor.achievable[0]=1.0;
    motor.achievable[8]=1.0;
    motor.fractions[0][1]=1.0f; // +X on the only motor
    motor.fractions[8][2]=1.0f; // +yaw on that SAME motor

    std::array<double,6> forceOnly {0.8,0.,0.,0.,0.,0.};
    std::array<double,6> yawOnly {0.,0.,0.,0.,0.8,0.};
    std::array<double,6> together {0.8,0.,0.,0.,0.8,0.};
    require(assistedCombinedDemandFeasible(motor,forceOnly),
            "standalone lateral demand falsely rejected");
    require(assistedCombinedDemandFeasible(motor,yawOnly),
            "standalone angular demand falsely rejected");
    require(!assistedCombinedDemandFeasible(motor,together),
            "individual axis limits falsely admitted combined motor overload");

    ShipControlAuthorityTelemetry physical;
    physical.valid=true;
    physical.positive[0]=1.0;
    physical.negative[0]=1.0;
    const glm::dvec3 lateral(1.0,0.0,0.0);
    const std::array<double,6> quiet {};
    const double withoutYaw=
        maximumAssistedCurveNormalAccelerationMps2(
            motor,physical,lateral,quiet);
    const double withYaw=
        maximumAssistedCurveNormalAccelerationMps2(
            motor,physical,lateral,yawOnly);
    require(withoutYaw>0.999 && withoutYaw<=1.001,
            "quiet motor must support physical full lateral demand");
    require(withYaw>0.59 && withYaw<0.61,
            "angular motor load did not reduce attainable curve acceleration");
    require(maximumAssistedCurveNormalAccelerationMps2(
            motor,physical,lateral,together)==0.0,
            "overcommitted hull torque was accepted as curve authority");
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

void testAssistedLostCorridorSafeStop()
{
    using Policy =
        game::navigation::autopilot::AssistedCorridorDeviationPolicy;
    // An isolated drift of 2m/s at half tolerance is recoverable with the
    // actual installed lateral basis. Do not abandon the route needlessly.
    require(!Policy::evaluate(8.3, -1.0, 64.0, 2.0, 18.0).requireSafeStop,
            "healthy aft-50 centering aborted");
    require(!Policy::evaluate(31.9, -10.0, 64.0, 1.0, 18.0).requireSafeStop,
            "sufficient room for recovery incorrectly rejected");
    require(!Policy::evaluate(33.3425, -4.63167, 64.0, 1.8, 18.0)
                 .requireSafeStop,
            "engine-out trace at 33m should be speed-governed, not STOP");
    require(!Policy::evaluate(40.9567, -8.57893, 64.0, 2.0, 18.0)
                 .requireSafeStop,
            "recoverable outward drift was interrupted without prediction");
    require(Policy::evaluate(55.0, -8.6, 64.0, 2.0, 18.0).requireSafeStop,
            "imminent physical corridor boundary missed");
    require(Policy::evaluate(70.0, +12.0, 64.0, 2.0, 18.0)
                 .requireSafeStop,
            "exceeded validated corridor continued");
    require(Policy::evaluate(std::nan(""), 0.0, 64.0, 2.0, 18.0)
                 .requireSafeStop,
            "nonfinite tracking observation permitted cruise");
}

void testAssistedSpeedGovernorForRealEngineOutStraight()
{
    using Governor =
        game::navigation::autopilot::AssistedCorridorSpeedGovernor;
    Governor::State state;
    const double tolerance = 64.0;
    const double forwardAuthority = 50.0;
    const double dt = 0.02;

    // Within the known healthy/aft-50 range there is NO change of speed.
    auto healthy = Governor::evaluate(
        state, 8.3, -1.0, tolerance, 180.0, 400.0,
        forwardAuthority, dt
    );
    require(healthy.valid && !healthy.recoveryActive,
            "baseline Assisted cruising was altered");

    // Full loss trace diverged while the route was still a STRAIGHT, long
    // before any turn. Start reducing speed at 25% corridor occupancy.
    auto recovery = Governor::evaluate(
        state, 19.0848, -2.47967, tolerance, 121.448, 141.842,
        forwardAuthority, dt
    );
    require(recovery.valid && recovery.recoveryActive &&
            recovery.speedCeilingMps < 121.448,
            "failed engine did not decelerate on the initial straight");
    const double initialCap = recovery.speedCeilingMps;

    // Continued outward drift cannot re-authorize a fast cruising target.
    recovery = Governor::evaluate(
        state, 23.5164, -3.40039, tolerance, 115.0, 163.024,
        forwardAuthority, dt
    );
    require(recovery.recoveryActive &&
            recovery.speedCeilingMps <= initialCap,
            "velocity governor accelerated during growing drift");

    // While trajectory is moving inward, restore acceleration slowly and
    // in physical units; never jump straight to the nominal 400m/s.
    const double previousCap = recovery.speedCeilingMps;
    recovery = Governor::evaluate(
        state, 20.0, +3.0, tolerance, 95.0, 300.0,
        forwardAuthority, dt
    );
    require(recovery.recoveryActive &&
            recovery.speedCeilingMps > previousCap &&
            recovery.speedCeilingMps <=
                previousCap + forwardAuthority * 0.20 * dt + 1.e-9,
            "recovery reacceleration ignored physical slew limit");

    // Once the capped trajectory is genuinely stable, the governor may
    // rejoin the already accepted speed profile.
    for (int i = 0; i < 1500 && recovery.recoveryActive; ++i)
        recovery = Governor::evaluate(
            state, 5.0, +0.5, tolerance, 150.0, 300.0,
            forwardAuthority, 0.1
        );
    require(!recovery.recoveryActive,
            "recovered Assisted corridor never released governor");

    auto invalid = Governor::evaluate(
        state, std::nan(""), 0.0, tolerance, 80.0, 400.0,
        forwardAuthority, dt
    );
    require(!invalid.valid,
            "invalid speed-governor input was accepted");
}

void testAssistedCourseResponseMeasuresVelocityNotHullNose()
{
    using game::navigation::autopilot::AssistedAutopilot;
    using game::navigation::autopilot::AutopilotControlRequest;
    const ShipParams parameters = nominal();
    AutopilotControlRequest request;
    request.deltaSeconds = 0.05;
    request.desiredVelocityMapMps = {0.0, 0.0, -100.0};
    request.actualVelocityMapMps = {0.0, 0.0, -100.0};
    request.desiredForwardMap = {0.0, 0.0, -1.0};
    request.desiredUpMap = {0.0, 1.0, 0.0};
    request.upMap = request.desiredUpMap;

    AssistedAutopilot pilot;
    // A damaged ship can move its NOSE through 80 degrees while its actual
    // velocity is stationary. Previous code interpreted the shrinking
    // nose/velocity gap as a very fast turn of the velocity vector.
    for (int i = 0; i <= 80; ++i)
    {
        const double angle = 1.4 * (1.0 - i / 80.0);
        request.forwardMap = {
            std::sin(angle), 0.0, -std::cos(angle)
        };
        request.rightMap = {
            std::cos(angle), 0.0, std::sin(angle)
        };
        (void)pilot.update(request, parameters);
    }
    require(
        std::abs(pilot.telemetry(parameters).courseResponseSeconds - 2.0) <
            1.0e-6,
        "fast hull nose rotation falsely improved measured course response"
    );

    // A growing VELOCITY-to-target error is a real failure to turn the
    // trajectory; it must lengthen rather than shorten reaction estimate.
    AssistedAutopilot diverging;
    request.forwardMap = {0.0, 0.0, -1.0};
    request.rightMap = {1.0, 0.0, 0.0};
    for (int i = 0; i <= 50; ++i)
    {
        const double angle = 0.08 + i * 0.004;
        request.actualVelocityMapMps = {
            100.0 * std::sin(angle),
            0.0,
            -100.0 * std::cos(angle)
        };
        (void)diverging.update(request, parameters);
    }
    require(diverging.telemetry(parameters).courseResponseSeconds > 2.5,
            "increasing route-course error was mistaken for fast response");
}

void testAssistedPilotDoesNotSnapControlSticks()
{
    using game::navigation::autopilot::AssistedAutopilot;
    using game::navigation::autopilot::AutopilotControlRequest;
    ShipParams parameters = nominal();
    parameters.maxPitchRate = 1.0f;
    parameters.maxYawRate = 1.0f;
    parameters.maxRollRate = 1.0f;

    AutopilotControlRequest request;
    request.deltaSeconds = 0.02;
    request.actualVelocityMapMps = {0.0, 0.0, -100.0};
    request.desiredVelocityMapMps = {100.0, 0.0, 0.0};
    request.desiredUpMap = {0.0, 1.0, 0.0};

    AssistedAutopilot pilot;
    float previousYaw = 0.0f;
    bool actuallyTurned = false;
    // High course error must not generate an instantaneous full-scale
    // yaw command, even at startup or when reversing the requested turn.
    for (int tick = 0; tick < 60; ++tick)
    {
        if (tick == 30)
            request.desiredVelocityMapMps = {-100.0, 0.0, 0.0};
        const auto control = pilot.update(request, parameters);
        const double change =
            std::abs(control.yawInput - previousYaw);
        require(change <= 0.051,
                "Assisted virtual pilot snapped the yaw stick");
        if (std::abs(control.yawInput) > 0.01f)
            actuallyTurned = true;
        require(std::isfinite(control.pitchInput) &&
                std::isfinite(control.yawInput) &&
                std::isfinite(control.rollInput),
                "Assisted slew emitted invalid stick positions");
        previousYaw = control.yawInput;
    }
    require(actuallyTurned,
            "Assisted virtual pilot suppressed the turn completely");

    // Physical emergency STOP must not wait for the normal comfort stroke.
    request.stopRequested = true;
    request.desiredVelocityMapMps = {100.0, 0.0, 0.0};
    const auto emergency = pilot.update(request, parameters);
    require(std::abs(emergency.yawInput) > 0.5,
            "Emergency STOP incorrectly kept normal stick slew");
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
    require(telemetry.valid && telemetry.coupled.valid &&
            telemetry.coupled.motorCount == 4,
            "actual installed Cobra failed to publish coupled motor basis");
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
    require(damaged.valid && damaged.coupled.valid &&
            damaged.coupled.available01[0] <= 1.0e-6f &&
            damaged.negative[2] < telemetry.negative[2],
            "destroyed aft motor leaked into joint control capability");
    const std::array<double,6> noOtherDemand {};
    const double physicalCurveAcceleration =
        game::ship::telemetry::
            maximumAssistedCurveNormalAccelerationMps2(
                damaged.coupled,damaged,
                glm::dvec3(1.0,0.0,0.0),noOtherDemand
            );
    require(std::isfinite(physicalCurveAcceleration) &&
            physicalCurveAcceleration>=0.0 &&
            physicalCurveAcceleration <= damaged.positive[0]+1.0e-5,
            "joint lateral capacity exceeded installed damaged motor basis");
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
        testCoupledEngineAuthorityRejectsIndependentAxisFantasy();
        testNoUndamagedDescriptorFallbackAfterMotorLoss();
        testSignedDamageCannotBeHiddenByHealthyOppositeAxis();
        testDamageInvalidatesAcceptedRoute();
        testCombinedMotorBudgetBoundsTangentSpeed();
        testAssistedLostCorridorSafeStop();
        testAssistedSpeedGovernorForRealEngineOutStraight();
        testAssistedCourseResponseMeasuresVelocityNotHullNose();
        testAssistedPilotDoesNotSnapControlSticks();
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
