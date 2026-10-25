#include <cmath>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Standard assert(expr) disappears under CMake Release (-DNDEBUG).
// Keep actual acceptance checks active in BOTH Release and Debug.
#define CHECK(expr) do { \
    if (!(expr)) throw std::runtime_error( \
        std::string("physical model test failed, line ") + \
        std::to_string(__LINE__) + ": " #expr); \
} while(false)

#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/navigation/DynamicMotionSystem.h"

namespace
{

using namespace game::ship::plant;

bool near(double a, double b, double eps = 1.0e-9)
{
    return std::abs(a - b) <= eps;
}

ShipParams nominalPhysics()
{
    ShipParams p {};
    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 60.0f;
    p.reverseMainEngineAccelerationMps2 = 60.0f;
    p.strafeAccel = 40.0f;
    p.manoeuvreThrusterAccel = 4.0f;
    p.angularAccel = 3.0f;
    p.mainGimbalPitchLimitRad = 0.12f;
    p.mainGimbalYawLimitRad = 0.12f;
    p.mainGimbalRateRadPerSec = 0.14f;
    p.maxGs = 20.0f;
    p.maxLinearGs = 20.0f;
    p.turnRadius = 20.0f;
    p.massKg = 100000.0;
    p.pitchInertiaKgM2 = 1000000.0;
    p.yawInertiaKgM2 = 1000000.0;
    p.rollInertiaKgM2 = 1000000.0;
    return p;
}

struct Counts
{
    std::size_t aft = 0;
    std::size_t fore = 0;
    std::size_t manoeuvre = 0;
};

Counts countRoles(const std::vector<EngineSlotDefinition>& slots)
{
    Counts out;
    for (const auto& slot : slots)
    {
        switch (slot.role)
        {
            case EngineSlotRole::AftMain:
                ++out.aft;
                break;
            case EngineSlotRole::ForeMain:
                ++out.fore;
                break;
            case EngineSlotRole::ManoeuvreCluster:
                ++out.manoeuvre;
                break;
        }
    }
    return out;
}

void expectCounts(
    PhysicalControlModelId model,
    std::size_t aft,
    std::size_t fore,
    std::size_t manoeuvre
)
{
    const Counts counts = countRoles(physicalControlSlots(model));
    CHECK(counts.aft == aft);
    CHECK(counts.fore == fore);
    CHECK(counts.manoeuvre == manoeuvre);
}

void testSixFixedTopologies()
{
    expectCounts(PhysicalControlModelId::AssistedI, 2, 2, 0);
    expectCounts(PhysicalControlModelId::AssistedY, 3, 3, 0);
    expectCounts(PhysicalControlModelId::AssistedX, 4, 4, 0);

    expectCounts(PhysicalControlModelId::NewtonianI, 1, 0, 8);
    expectCounts(PhysicalControlModelId::NewtonianY, 3, 0, 8);
    expectCounts(PhysicalControlModelId::NewtonianX, 4, 0, 8);

    CHECK(
        nativeFlightLaw(PhysicalControlModelId::AssistedI) ==
        game::navigation::LocalFlightControlLaw::Assisted
    );
    CHECK(
        nativeFlightLaw(PhysicalControlModelId::NewtonianX) ==
        game::navigation::LocalFlightControlLaw::Newtonian
    );
}

void testSlotSemantics()
{
    for (const auto model : {
        PhysicalControlModelId::AssistedI,
        PhysicalControlModelId::AssistedY,
        PhysicalControlModelId::AssistedX
    })
    {
        for (const auto& slot : physicalControlSlots(model))
        {
            CHECK(slot.role != EngineSlotRole::ManoeuvreCluster);
            CHECK(slot.freedom == EngineSlotFreedom::VectoringMain);
            if (slot.role == EngineSlotRole::AftMain)
                CHECK(slot.nominalForceDirectionBody.z < -0.999);
            if (slot.role == EngineSlotRole::ForeMain)
                CHECK(slot.nominalForceDirectionBody.z > 0.999);
        }
    }

    for (const auto model : {
        PhysicalControlModelId::NewtonianI,
        PhysicalControlModelId::NewtonianY,
        PhysicalControlModelId::NewtonianX
    })
    {
        for (const auto& slot : physicalControlSlots(model))
        {
            if (slot.role == EngineSlotRole::AftMain)
            {
                CHECK(slot.freedom == EngineSlotFreedom::VectoringMain);
                CHECK(slot.nominalForceDirectionBody.z < -0.999);
            }
            else
            {
                CHECK(slot.role == EngineSlotRole::ManoeuvreCluster);
                CHECK(
                    slot.freedom ==
                    EngineSlotFreedom::OmnidirectionalCluster
                );
                CHECK(near(glm::length(slot.nominalForceDirectionBody), 0.0));
            }
        }
    }
}

void testAssistedDegradation()
{
    const ShipParams nominal = nominalPhysics();
    const auto& slots =
        physicalControlSlots(PhysicalControlModelId::AssistedI);

    auto state = healthySlotState(slots);
    auto full = evaluatePhysicalControlModel(
        PhysicalControlModelId::AssistedI,
        nominal,
        state
    );

    CHECK(full.valid);
    CHECK(full.regime == RuntimeFlightRegime::FullAssisted);
    CHECK(full.mainSlots == 4);
    CHECK(full.manoeuvreSlots == 0);
    CHECK(near(full.authority.longitudinal.positive, 1.0));
    CHECK(near(full.authority.longitudinal.negative, 1.0));
    CHECK(near(full.effectivePhysics.forwardMainEngineAccelerationMps2, 60.0));
    CHECK(near(full.effectivePhysics.reverseMainEngineAccelerationMps2, 60.0));
    CHECK(near(full.effectivePhysics.manoeuvreThrusterAccel, 0.0));
    CHECK(near(full.poweredMainVectoring.trajectoryStrafe01, 1.0));
    CHECK(near(full.poweredMainVectoring.trajectoryLift01, 1.0));

    // With nominal 40 m/s^2 useful lateral authority and 60+60 m/s^2 gross
    // opposed-bank authority, only one third of generated thrust becomes net
    // lateral translation. This is propulsive utilization, not engine eta.
    CHECK(near(full.thrustUtilization.lateralTranslation01, 1.0 / 3.0));
    CHECK(near(full.thrustUtilization.cancelledDuringLateral01, 2.0 / 3.0));

    // Assisted-I slot order is aft L/R, fore L/R.
    state[2].operational = false;
    auto oneForeLost = evaluatePhysicalControlModel(
        PhysicalControlModelId::AssistedI,
        nominal,
        state
    );

    CHECK(oneForeLost.valid);
    CHECK(oneForeLost.regime == RuntimeFlightRegime::DegradedAssisted);
    CHECK(near(oneForeLost.authority.longitudinal.positive, 1.0));
    CHECK(near(oneForeLost.authority.longitudinal.negative, 0.5));
    CHECK(near(
        oneForeLost.effectivePhysics.reverseMainEngineAccelerationMps2,
        30.0
    ));
    CHECK(near(oneForeLost.authority.strafe.positive, 0.75));

    state[3].operational = false;
    auto bothForeLost = evaluatePhysicalControlModel(
        PhysicalControlModelId::AssistedI,
        nominal,
        state
    );

    CHECK(bothForeLost.valid);
    CHECK(
        bothForeLost.regime ==
        RuntimeFlightRegime::ThrustAxisFallback
    );
    CHECK(!bothForeLost.effectivePhysics.reverseMainEngineAvailable);
    CHECK(near(
        bothForeLost.effectivePhysics.reverseMainEngineAccelerationMps2,
        0.0
    ));
}

void testAssistedHasNoGhostRcs()
{
    const ShipParams nominal = nominalPhysics();
    const auto model = PhysicalControlModelId::AssistedI;
    const auto snapshot = evaluatePhysicalControlModel(
        model,
        nominal,
        healthySlotState(physicalControlSlots(model))
    );

    CHECK(snapshot.valid);
    CHECK(snapshot.effectivePhysics.manoeuvreThrusterAuthorityExplicit);
    CHECK(near(
        game::ship::manoeuvreAccelerationLimitMps2(
            snapshot.effectivePhysics
        ),
        0.0
    ));
    CHECK(
        game::ship::assistedLateralStabilizationAccelerationLimitMps2(
            snapshot.effectivePhysics
        ) > 0.0
    );

    game::navigation::DynamicMotionState motion;
    motion.mode = game::navigation::MotionMode::HubTactical;
    motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "assisted-no-ghost-rcs";
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;

    game::navigation::DynamicMotionSystem::applyLocalFrameInput(
        motion,
        frame,
        snapshot.effectivePhysics,
        0.02f,
        0.0f,
        false,
        0.0f,
        0.0f,
        1.0f,
        glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    CHECK(near(glm::length(motion.manoeuvreAccelerationMps2), 0.0));
    CHECK(glm::length(
        motion.assistedStabilizationAccelerationMps2
    ) > 1.0e-6);
}

void testNewtonianDegradation()
{
    ShipParams nominal = nominalPhysics();
    nominal.reverseMainEngineAvailable = true;
    nominal.reverseMainEngineAccelerationMps2 = 60.0f;

    const auto& slots =
        physicalControlSlots(PhysicalControlModelId::NewtonianI);

    auto state = healthySlotState(slots);
    auto full = evaluatePhysicalControlModel(
        PhysicalControlModelId::NewtonianI,
        nominal,
        state
    );

    CHECK(full.valid);
    CHECK(full.regime == RuntimeFlightRegime::FullNewtonian);
    CHECK(full.mainSlots == 1);
    CHECK(full.manoeuvreSlots == 8);
    CHECK(!full.effectivePhysics.reverseMainEngineAvailable);
    CHECK(near(
        full.effectivePhysics.reverseMainEngineAccelerationMps2,
        0.0
    ));
    CHECK(near(full.effectivePhysics.forwardMainEngineAccelerationMps2, 60.0));
    CHECK(near(full.effectivePhysics.manoeuvreThrusterAccel, 4.0));
    CHECK(near(full.effectivePhysics.mainGimbalPitchLimitRad, 0.12, 1.0e-6));
    CHECK(near(full.effectivePhysics.mainGimbalYawLimitRad, 0.12, 1.0e-6));
    CHECK(near(full.effectivePhysics.mainGimbalRateRadPerSec, 0.14, 1.0e-6));

    // Newtonian-I main is gimbaled. It can bend a powered trajectory and
    // generate pitch/yaw, but a centred single main has no direct roll moment.
    CHECK(near(full.poweredMainVectoring.trajectoryStrafe01, 1.0));
    CHECK(near(full.poweredMainVectoring.trajectoryLift01, 1.0));
    CHECK(near(full.poweredMainVectoring.pitch01, 1.0));
    CHECK(near(full.poweredMainVectoring.yaw01, 1.0));
    CHECK(near(full.poweredMainVectoring.roll01, 0.0));
    CHECK(near(full.thrustUtilization.cancelledDuringLateral01, 0.0));

    const auto newtonY = evaluatePhysicalControlModel(
        PhysicalControlModelId::NewtonianY,
        nominal,
        healthySlotState(
            physicalControlSlots(PhysicalControlModelId::NewtonianY)
        )
    );
    const auto newtonX = evaluatePhysicalControlModel(
        PhysicalControlModelId::NewtonianX,
        nominal,
        healthySlotState(
            physicalControlSlots(PhysicalControlModelId::NewtonianX)
        )
    );
    CHECK(newtonY.poweredMainVectoring.roll01 > 0.999);
    CHECK(newtonX.poweredMainVectoring.roll01 > 0.999);

    // Vectoring damage shrinks real nozzle travel/slew; it must not be
    // converted into hidden RCS authority.
    state[0].vectoring01 = 0.5;
    auto halfVectoring = evaluatePhysicalControlModel(
        PhysicalControlModelId::NewtonianI,
        nominal,
        state
    );
    CHECK(near(
        halfVectoring.effectivePhysics.mainGimbalPitchLimitRad,
        0.06,
        1.0e-6
    ));
    CHECK(near(
        halfVectoring.effectivePhysics.mainGimbalYawLimitRad,
        0.06,
        1.0e-6
    ));
    CHECK(near(
        halfVectoring.effectivePhysics.mainGimbalRateRadPerSec,
        0.07,
        1.0e-6
    ));
    CHECK(near(
        halfVectoring.effectivePhysics.manoeuvreThrusterAccel,
        4.0
    ));
    state[0].vectoring01 = 1.0;

    // Slot 0 is the axial main, remaining slots are RCS clusters.
    state[1].operational = false;
    auto rcsLost = evaluatePhysicalControlModel(
        PhysicalControlModelId::NewtonianI,
        nominal,
        state
    );

    CHECK(rcsLost.valid);
    CHECK(rcsLost.regime == RuntimeFlightRegime::DegradedNewtonian);
    CHECK(near(rcsLost.authority.strafe.positive, 0.875));
    CHECK(near(rcsLost.effectivePhysics.manoeuvreThrusterAccel, 3.5));

    state[0].operational = false;
    auto mainLost = evaluatePhysicalControlModel(
        PhysicalControlModelId::NewtonianI,
        nominal,
        state
    );

    CHECK(mainLost.valid);
    CHECK(mainLost.regime == RuntimeFlightRegime::SurvivalControl);
    CHECK(!mainLost.effectivePhysics.forwardMainEngineAvailable);
}


bool axisNotGreater(
    const AxisAuthority01& damaged,
    const AxisAuthority01& healthy,
    double eps = 1.0e-9
)
{
    return
        damaged.positive <= healthy.positive + eps &&
        damaged.negative <= healthy.negative + eps;
}

bool authorityNotGreater(
    const KinematicAuthorityEstimate& damaged,
    const KinematicAuthorityEstimate& healthy
)
{
    return
        axisNotGreater(damaged.longitudinal, healthy.longitudinal) &&
        axisNotGreater(damaged.strafe, healthy.strafe) &&
        axisNotGreater(damaged.lift, healthy.lift) &&
        axisNotGreater(damaged.pitch, healthy.pitch) &&
        axisNotGreater(damaged.yaw, healthy.yaw) &&
        axisNotGreater(damaged.roll, healthy.roll);
}

void testHealthyMatrixPreservesAcceptedEnvelope()
{
    ShipParams nominal = nominalPhysics();

    constexpr PhysicalControlModelId models[] = {
        PhysicalControlModelId::AssistedI,
        PhysicalControlModelId::AssistedY,
        PhysicalControlModelId::AssistedX,
        PhysicalControlModelId::NewtonianI,
        PhysicalControlModelId::NewtonianY,
        PhysicalControlModelId::NewtonianX
    };

    for (const auto model : models)
    {
        const auto snapshot = evaluatePhysicalControlModel(
            model,
            nominal,
            healthySlotState(physicalControlSlots(model))
        );

        CHECK(snapshot.valid);
        CHECK(near(snapshot.authority.longitudinal.positive, 1.0));
        CHECK(near(
            snapshot.effectivePhysics.forwardMainEngineAccelerationMps2,
            60.0
        ));

        if (familyOf(model) == PhysicalControlFamily::Assisted)
        {
            CHECK(snapshot.regime == RuntimeFlightRegime::FullAssisted);
            CHECK(near(snapshot.authority.longitudinal.negative, 1.0));
            CHECK(snapshot.effectivePhysics.reverseMainEngineAvailable);
            CHECK(near(
                snapshot.effectivePhysics.reverseMainEngineAccelerationMps2,
                60.0
            ));
            CHECK(near(snapshot.effectivePhysics.strafeAccel, 40.0));
            CHECK(near(snapshot.effectivePhysics.angularAccel, 3.0));
            CHECK(near(snapshot.effectivePhysics.manoeuvreThrusterAccel, 0.0));
        }
        else
        {
            CHECK(snapshot.regime == RuntimeFlightRegime::FullNewtonian);
            CHECK(!snapshot.effectivePhysics.reverseMainEngineAvailable);
            CHECK(near(
                snapshot.effectivePhysics.reverseMainEngineAccelerationMps2,
                0.0
            ));
            CHECK(near(snapshot.effectivePhysics.manoeuvreThrusterAccel, 4.0));
            CHECK(near(snapshot.effectivePhysics.strafeAccel, 40.0));
            CHECK(near(snapshot.effectivePhysics.angularAccel, 3.0));
        }
    }
}

void testDamageMatrixNeverCreatesAuthority()
{
    const ShipParams nominal = nominalPhysics();

    constexpr PhysicalControlModelId models[] = {
        PhysicalControlModelId::AssistedI,
        PhysicalControlModelId::AssistedY,
        PhysicalControlModelId::AssistedX,
        PhysicalControlModelId::NewtonianI,
        PhysicalControlModelId::NewtonianY,
        PhysicalControlModelId::NewtonianX
    };

    for (const auto model : models)
    {
        const auto& slots = physicalControlSlots(model);
        const auto healthyState = healthySlotState(slots);
        const auto healthy = evaluatePhysicalControlModel(
            model,
            nominal,
            healthyState
        );
        CHECK(healthy.valid);

        for (std::size_t failed = 0; failed < slots.size(); ++failed)
        {
            auto damagedState = healthyState;
            damagedState[failed].operational = false;

            const auto damaged = evaluatePhysicalControlModel(
                model,
                nominal,
                damagedState
            );

            CHECK(damaged.valid);
            CHECK(authorityNotGreater(damaged.authority, healthy.authority));
            CHECK(
                damaged.effectivePhysics.forwardMainEngineAccelerationMps2 <=
                healthy.effectivePhysics.forwardMainEngineAccelerationMps2 +
                    1.0e-6f
            );
            CHECK(
                damaged.effectivePhysics.reverseMainEngineAccelerationMps2 <=
                healthy.effectivePhysics.reverseMainEngineAccelerationMps2 +
                    1.0e-6f
            );
            CHECK(
                damaged.effectivePhysics.strafeAccel <=
                healthy.effectivePhysics.strafeAccel + 1.0e-6f
            );
            CHECK(
                damaged.effectivePhysics.angularAccel <=
                healthy.effectivePhysics.angularAccel + 1.0e-6f
            );
            CHECK(
                damaged.effectivePhysics.manoeuvreThrusterAccel <=
                healthy.effectivePhysics.manoeuvreThrusterAccel + 1.0e-6f
            );
        }
    }
}

void testStateShapeIsPartOfTopology()
{
    const ShipParams nominal = nominalPhysics();
    std::vector<EngineSlotState> wrong(3);

    const auto out = evaluatePhysicalControlModel(
        PhysicalControlModelId::AssistedI,
        nominal,
        wrong
    );

    CHECK(!out.valid);
}

} // namespace

int main()
{
    testSixFixedTopologies();
    testSlotSemantics();
    testAssistedDegradation();
    testAssistedHasNoGhostRcs();
    testNewtonianDegradation();
    testHealthyMatrixPreservesAcceptedEnvelope();
    testDamageMatrixNeverCreatesAuthority();
    testStateShapeIsPartOfTopology();

    std::cout << "SHIP PHYSICAL CONTROL MODELS: PASS\n";
    std::cout << " - six fixed Assisted/Newtonian I/Y/X topologies\n";
    std::cout << " - Assisted ordinary control uses vectoring mains, no RCS slots or legacy ghost-RCS fallback\n";
    std::cout << " - Newtonian uses aft vectoring main propulsion plus RCS clusters\n";
    std::cout << " - healthy authority matrices preserve the accepted pre-matrix movement envelope\n";
    std::cout << " - slot damage can only remove authority; it can never improve an axis\n";
    std::cout << " - slot loss degrades kinematic authority without changing construction family\n";
    return 0;
}
