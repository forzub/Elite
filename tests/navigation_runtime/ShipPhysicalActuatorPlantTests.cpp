#include "src/game/ship/plant/ShipPhysicalActuatorPlant.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{

using namespace game::ship::plant;

bool near(double a, double b, double eps = 1.0e-9)
{
    return std::abs(a - b) <= eps;
}

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipParams physics()
{
    ShipParams p {};
    p.massKg = 260000.0;
    p.centerOfMassLocalMeters = {0.0, 0.0, 0.0};
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    p.turnRadius = 20.0f;
    p.physicalActuatorHalfExtentsMeters = {13.0, 2.5, 11.1};
    return p;
}

void testCenteredNewtonianIAxialBurnHasNoTorque()
{
    const auto model = PhysicalControlModelId::NewtonianI;
    const auto state = healthySlotState(physicalControlSlots(model));

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0}
    );

    require(out.valid, "physical reaction evaluation invalid");
    require(near(glm::length(out.torqueBodyNm), 0.0, 1.0e-6), "centered Newtonian-I axial burn generated torque");
    require(near(
        glm::length(out.angularAccelerationBodyRadPerSec2),
        0.0,
        1.0e-12
    ), "centered Newtonian-I axial burn generated angular acceleration");
}

void testNewtonianIGimbalCreatesYawTorque()
{
    const auto model = PhysicalControlModelId::NewtonianI;
    const auto state = healthySlotState(physicalControlSlots(model));

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {7.0, 0.0, -59.0}
    );

    require(out.valid, "physical reaction evaluation invalid");
    require(std::abs(out.torqueBodyNm.y) > 1.0, "Newtonian-I TVC generated no yaw torque");
    require(out.angularAccelerationBodyRadPerSec2.y > 0.0, "Newtonian-I TVC yaw reaction had wrong sign");
    require(near(out.angularAccelerationBodyRadPerSec2.x, 0.0, 1.0e-12), "Newtonian-I yaw TVC leaked pitch reaction");
    require(near(out.angularAccelerationBodyRadPerSec2.z, 0.0, 1.0e-12), "Newtonian-I yaw TVC leaked roll reaction");
}

void testHealthyNewtonianYAxialBankIsBalanced()
{
    const auto model = PhysicalControlModelId::NewtonianY;
    const auto state = healthySlotState(physicalControlSlots(model));

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0}
    );

    require(out.valid, "physical reaction evaluation invalid");
    require(near(glm::length(out.torqueBodyNm), 0.0, 1.0e-6), "centered Newtonian-I axial burn generated torque");
}

void testNewtonianYSingleMainLossCreatesPitchTorque()
{
    const auto model = PhysicalControlModelId::NewtonianY;
    auto state = healthySlotState(physicalControlSlots(model));

    // main_top
    state[0].operational = false;
    state[0].thrust01 = 0.0;
    state[0].vectoring01 = 0.0;

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -40.0}
    );

    require(out.valid, "physical reaction evaluation invalid");
    require(std::abs(out.torqueBodyNm.x) > 1.0, "single-main loss generated no pitch torque");
    require(std::abs(out.angularAccelerationBodyRadPerSec2.x) > 1.0e-3, "single-main loss generated negligible pitch acceleration");
    require(std::abs(out.angularAccelerationBodyRadPerSec2.x) < 0.25, "single-main loss pitch acceleration exceeded bound");
    require(near(out.angularAccelerationBodyRadPerSec2.y, 0.0, 1.0e-12), "Newtonian-Y single-main loss leaked yaw reaction");
}

void testNewtonianXSingleCornerMainLossCreatesTwoAxisTorque()
{
    const auto model = PhysicalControlModelId::NewtonianX;
    auto state = healthySlotState(physicalControlSlots(model));

    // main_upper_left
    state[0].operational = false;
    state[0].thrust01 = 0.0;
    state[0].vectoring01 = 0.0;

    const auto out = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -45.0}
    );

    require(out.valid, "physical reaction evaluation invalid");
    require(std::abs(out.torqueBodyNm.x) > 1.0, "single-main loss generated no pitch torque");
    require(std::abs(out.torqueBodyNm.y) > 1.0, "Newtonian-I TVC generated no yaw torque");
    require(std::abs(out.angularAccelerationBodyRadPerSec2.x) > 1.0e-3, "single-main loss generated negligible pitch acceleration");
    require(std::abs(out.angularAccelerationBodyRadPerSec2.y) > 1.0e-3, "single-main loss generated negligible yaw acceleration");
    require(std::abs(out.angularAccelerationBodyRadPerSec2.x) < 0.25, "single-main loss pitch acceleration exceeded bound");
    require(std::abs(out.angularAccelerationBodyRadPerSec2.y) < 0.25, "single-main loss yaw acceleration exceeded bound");
}


void testIrregularThrustIsDeterministicAndPhysicallyBounded()
{
    const auto model = PhysicalControlModelId::AssistedI;
    auto state = healthySlotState(physicalControlSlots(model));

    // Only aft_left is unstable. The other aft engine stays steady, so the
    // whole active bank must pulse between 50% and 100% of its static output.
    state[0].thrustIrregularity01 = 1.0;

    double minScale = 1.0;
    double maxScale = 0.0;

    for (std::uint64_t tick = 0; tick < 256; tick += 4)
    {
        const double scale =
            evaluateMainPropulsionInstantaneousScale(
                model,
                state,
                {0.0, 0.0, -60.0},
                tick
            );

        require(scale >= 0.5 - 1.0e-12, "irregular bank pulse fell below physical lower bound");
        require(scale <= 1.0 + 1.0e-12, "irregular bank pulse exceeded damage-limited maximum");

        const double repeated =
            evaluateMainPropulsionInstantaneousScale(
                model,
                state,
                {0.0, 0.0, -60.0},
                tick
            );
        require(near(scale, repeated), "irregular thrust was not deterministic for the same control tick");

        minScale = std::min(minScale, scale);
        maxScale = std::max(maxScale, scale);
    }

    require(maxScale - minScale > 0.20, "irregular thrust did not produce a meaningful pulse range");

    const auto stable = healthySlotState(physicalControlSlots(model));
    require(near(
        evaluateMainPropulsionInstantaneousScale(
            model,
            stable,
            {0.0, 0.0, -60.0},
            12345
        ),
        1.0
    ), "healthy engine bank acquired artificial thrust irregularity");
}


void testAssistedThrottleFeedbackBalancesUnequalPair()
{
    const auto model = PhysicalControlModelId::AssistedI;
    auto state = healthySlotState(physicalControlSlots(model));
    state[0].thrust01 = 0.80;

    const auto raw = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        0
    );
    require(raw.valid, "raw Assisted-I damaged reaction invalid");
    require(
        std::abs(raw.angularAccelerationBodyRadPerSec2.y) > 1.0e-6,
        "unequal Assisted-I pair produced no yaw bias"
    );

    const auto trim = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        {0.0, 0.0, 0.0},
        0.0,
        0.0,
        0
    );

    require(trim.valid, "Assisted-I feedback trim invalid");
    require(
        std::abs(trim.reaction.angularAccelerationBodyRadPerSec2.y) <
            std::abs(raw.angularAccelerationBodyRadPerSec2.y) * 0.10,
        "Assisted-I throttle feedback did not remove pair imbalance"
    );
    require(
        trim.thrustScale01 < 1.0 &&
        trim.thrustScale01 > 0.80,
        "Assisted-I balancing sacrificed implausible thrust"
    );
}

void testFeedbackDoesNotHideTorqueByUniformlyKillingThrust()
{
    const auto model = PhysicalControlModelId::AssistedY;
    auto state = healthySlotState(physicalControlSlots(model));

    // With the top aft main gone, both survivors have the same pitch lever-arm
    // sign. Uniformly reducing them cannot improve torque per unit useful force.
    state[0].operational = false;
    state[0].thrust01 = 0.0;
    state[0].vectoring01 = 0.0;

    const auto trim = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -40.0},
        {0.0, 0.0, 0.0},
        0.0,
        0.0,
        0
    );

    require(trim.valid, "Assisted-Y no-TVC feedback trim invalid");
    require(
        near(trim.thrustScale01, 1.0, 1.0e-9),
        "feedback hid uncorrectable torque by uniformly killing thrust"
    );
    require(
        std::abs(trim.reaction.angularAccelerationBodyRadPerSec2.x) > 1.0e-4,
        "uncorrectable no-TVC pitch bias was magically erased"
    );
}

void testCommonTvcFeedbackReducesResidualWithoutKillingThrust()
{
    const auto model = PhysicalControlModelId::AssistedY;
    auto state = healthySlotState(physicalControlSlots(model));
    state[0].operational = false;
    state[0].thrust01 = 0.0;
    state[0].vectoring01 = 0.0;

    const auto noTvc = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -40.0},
        {0.0, 0.0, 0.0},
        0.0,
        0.0,
        0
    );

    const auto withTvc = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -40.0},
        {0.0, 0.0, 0.0},
        0.12,
        0.12,
        0
    );

    require(withTvc.valid, "Assisted-Y TVC feedback trim invalid");
    require(
        near(withTvc.thrustScale01, 1.0, 1.0e-9),
        "common TVC trim unnecessarily sacrificed main thrust"
    );
    require(
        std::abs(withTvc.automaticPitchTrimRad) > 1.0e-6,
        "common TVC feedback did not use available pitch trim"
    );
    require(
        std::abs(withTvc.reaction.angularAccelerationBodyRadPerSec2.x) <
            std::abs(noTvc.reaction.angularAccelerationBodyRadPerSec2.x),
        "common TVC feedback did not reduce residual pitch bias"
    );
}

void testNewtonianThrottleFeedbackUsesSameSimpleRule()
{
    const auto model = PhysicalControlModelId::NewtonianY;
    auto state = healthySlotState(physicalControlSlots(model));

    state[0].thrust01 = 0.80;

    const auto raw = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        0
    );
    require(raw.valid, "raw Newtonian-Y damaged reaction invalid");

    const auto trim = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        {0.0, 0.0, 0.0},
        0.0,
        0.0,
        0
    );

    require(trim.valid, "Newtonian-Y feedback trim invalid");
    require(
        glm::length(
            trim.reaction.angularAccelerationBodyRadPerSec2
        ) <
        glm::length(
            raw.angularAccelerationBodyRadPerSec2
        ),
        "Newtonian-Y throttle feedback did not reduce damaged-main bias"
    );
    require(
        trim.thrustScale01 < 1.0,
        "Newtonian-Y feedback never redistributed main thrust"
    );
}

void testDamageAsymmetryIsNotUsedAsSteering()
{
    const auto model = PhysicalControlModelId::AssistedI;
    auto state = healthySlotState(physicalControlSlots(model));
    state[0].thrust01 = 0.80;

    const auto raw = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        0
    );
    require(raw.valid, "raw Assisted-I damaged reaction invalid");

    const double yawIntent =
        raw.angularAccelerationBodyRadPerSec2.y >= 0.0
            ? 1.0
            : -1.0;

    const auto trim = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        {0.0, yawIntent, 0.0},
        0.12,
        0.12,
        0
    );

    require(trim.valid, "Assisted-I direct balance invalid");
    require(
        trim.thrustScale01 < 1.0,
        "damaged bank was left asymmetric because its bias happened to help a turn"
    );
    require(
        std::abs(
            trim.reaction.angularAccelerationBodyRadPerSec2.y
        ) < 1.0e-9,
        "damage asymmetry leaked into steering instead of being balanced"
    );
}

void testDirectBankBalancingWorksAcrossXLayout()
{
    const auto model = PhysicalControlModelId::NewtonianX;
    auto state = healthySlotState(physicalControlSlots(model));

    state[0].thrust01 = 0.92;
    state[1].thrust01 = 0.47;
    state[2].thrust01 = 0.73;
    state[3].thrust01 = 1.00;

    const auto raw = evaluateMainPropulsionReaction(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        0
    );
    require(raw.valid, "raw Newtonian-X damaged reaction invalid");
    require(
        glm::length(raw.angularAccelerationBodyRadPerSec2) > 1.0e-6,
        "unequal Newtonian-X bank unexpectedly had no parasitic moment"
    );

    const auto trim = evaluateMainPropulsionFeedbackTrim(
        model,
        state,
        physics(),
        {0.0, 0.0, -60.0},
        {0.0, 0.0, 0.0},
        0.0,
        0.0,
        0
    );

    require(trim.valid, "Newtonian-X direct balance invalid");
    require(
        glm::length(
            trim.reaction.angularAccelerationBodyRadPerSec2
        ) < 1.0e-9,
        "Newtonian-X four-engine bank was not balanced directly to weakest engine"
    );
    require(
        trim.thrustScale01 < 1.0,
        "Newtonian-X direct balance did not reduce stronger engines"
    );
}

void testActualAllocatorTelemetryTracksOnlyFiringBank()
{
    const auto model = PhysicalControlModelId::AssistedI;
    auto state = healthySlotState(physicalControlSlots(model));
    state[0].thrust01 = 0.50;

    auto p = physics();
    p.forwardMainEngineAccelerationMps2 = 50.0f;
    p.reverseMainEngineAccelerationMps2 = 50.0f;

    const auto forward = evaluateMainPropulsionFeedbackTrim(
        model, state, p,
        {0.0, 0.0, -50.0}, {0.0, 0.0, 0.0},
        0.0, 0.0, 0
    );
    require(forward.valid && forward.reportedSlotCount == 4,
            "allocator did not report all four slot identities");
    require(near(forward.deliveredThrust01[0], 0.50) &&
            near(forward.deliveredThrust01[1], 0.50),
            "physical balance did not report delivered aft-engine burn");
    require(near(forward.deliveredThrust01[2], 0.0) &&
            near(forward.deliveredThrust01[3], 0.0),
            "inactive fore engines accrued fictitious burn");

    const auto reverse = evaluateMainPropulsionFeedbackTrim(
        model, state, p,
        {0.0, 0.0, 25.0}, {0.0, 0.0, 0.0},
        0.0, 0.0, 0
    );
    require(reverse.valid && reverse.reportedSlotCount == 4,
            "reverse allocator lost engine identity");
    require(near(reverse.deliveredThrust01[0], 0.0) &&
            near(reverse.deliveredThrust01[1], 0.0) &&
            near(reverse.deliveredThrust01[2], 0.50) &&
            near(reverse.deliveredThrust01[3], 0.50),
            "half-demand reverse burn incorrectly aged aft or overaged fore");
}

} // namespace

int main()
{
    try
    {
        testCenteredNewtonianIAxialBurnHasNoTorque();
        testNewtonianIGimbalCreatesYawTorque();
        testHealthyNewtonianYAxialBankIsBalanced();
        testNewtonianYSingleMainLossCreatesPitchTorque();
        testNewtonianXSingleCornerMainLossCreatesTwoAxisTorque();
        testIrregularThrustIsDeterministicAndPhysicallyBounded();
        testAssistedThrottleFeedbackBalancesUnequalPair();
        testFeedbackDoesNotHideTorqueByUniformlyKillingThrust();
        testCommonTvcFeedbackReducesResidualWithoutKillingThrust();
        testNewtonianThrottleFeedbackUsesSameSimpleRule();
        testDamageAsymmetryIsNotUsedAsSteering();
        testDirectBankBalancingWorksAcrossXLayout();
        testActualAllocatorTelemetryTracksOnlyFiringBank();

        std::cout
            << "SHIP PHYSICAL ACTUATOR PLANT: PASS\n"
            << " - balanced main banks produce zero parasitic torque\n"
            << " - Newtonian-I TVC produces rigid-body reaction torque\n"
            << " - Y/X single-main failures produce layout-dependent asymmetric torque\n"
            << " - severe damage produces deterministic bounded per-engine thrust hiccups\n"
            << " - cheap feedback reduces only torque-producing main imbalance\n"
            << " - common TVC trim reduces residual bias without hiding it by killing thrust\n"
            << " - direct weakest-engine balancing covers I/Y/X main banks\n"
            << " - damage asymmetry is never reused as steering authority\n"
            << " - Newtonian clustered mains keep failure angular acceleration bounded by their short radial lever arm\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP PHYSICAL ACTUATOR PLANT: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
