#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/world/WorldParams.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Model = game::ship::plant::PhysicalControlModelId;
using Family = game::ship::plant::PhysicalControlFamily;
using Snapshot = game::ship::plant::PhysicalControlSnapshot;

constexpr double kDt = 0.02;
constexpr double kDuration = 1.0;
constexpr double kTiny = 1.0e-4;
constexpr double kDrift = 1.0e-3;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

const char* modelName(Model model)
{
    return game::ship::plant::physicalControlModelName(model);
}

ShipParams nominalPhysics()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;

    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    p.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;

    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 60.0f;
    p.reverseMainEngineAccelerationMps2 = 60.0f;

    p.mainGimbalPitchLimitRad = 0.12f;
    p.mainGimbalYawLimitRad = 0.12f;
    p.mainGimbalRateRadPerSec = 0.14f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 40.0f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 4.0f;
    p.manoeuvreGasUsePerSecond = 0.0f;
    p.manoeuvreGasRechargePerSecond = 0.0f;
    p.manoeuvreGasRestartFraction = 0.20f;

    p.maxGs = 20.0f;
    p.maxLinearGs = 20.0f;
    p.turnRadius = 20.0f;
    p.stopSpeedEpsilonMps = 0.05f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    p.physicalActuatorHalfExtentsMeters = {13.0, 2.5, 11.1};
    return p;
}

game::navigation::KinematicFrame frame()
{
    game::navigation::KinematicFrame out;
    out.systemId = 1;
    out.frameId = "ship-control-behavior-oracle";
    out.originMeters = {0.0, 0.0, 0.0};
    out.localToWorldBasis = glm::dmat3(1.0);
    out.valid = true;
    return out;
}

ShipTransform makeTransform(Model model, double initialForwardSpeed = 0.0)
{
    ShipTransform out {};
    out.orientation = glm::mat4(1.0f);
    out.motion.mode = game::navigation::MotionMode::HubTactical;
    out.motion.systemId = 1;
    out.motion.travelFrame = frame();
    out.motion.localControlLaw =
        game::ship::plant::nativeFlightLaw(model);
    out.motion.localPositionMeters = {0.0, 0.0, 0.0};

    const glm::dvec3 forward(out.forward());
    out.motion.localVelocityMps = forward * initialForwardSpeed;
    out.motion.targetForwardSpeedMps = initialForwardSpeed;
    out.setWorldPositionMeters({0.0, 0.0, 0.0});
    return out;
}

struct Observation
{
    glm::dvec3 deltaPosition {0.0};
    glm::dvec3 deltaVelocity {0.0};
    glm::dvec3 finalVelocity {0.0};
    glm::dvec3 peakMainAcceleration {0.0};
    glm::dvec3 peakRcsAcceleration {0.0};
    glm::dvec3 peakAssistedAcceleration {0.0};

    double finalSpeed = 0.0;
    double pitchRate = 0.0;
    double yawRate = 0.0;
    double rollRate = 0.0;
    double gimbalYaw = 0.0;
    double gimbalPitch = 0.0;
};

void keepPeak(glm::dvec3& peak, const glm::dvec3& value)
{
    if (glm::length(value) > glm::length(peak))
        peak = value;
}

Observation execute(
    Model model,
    const Snapshot& snapshot,
    const ShipControlState& control,
    double initialForwardSpeed = 0.0
)
{
    ShipTransform transform = makeTransform(model, initialForwardSpeed);
    WorldParams world {};
    const auto slots = game::ship::plant::healthySlotState(
        game::ship::plant::physicalControlSlots(model)
    );

    const glm::dvec3 startPosition =
        transform.motion.localPositionMeters;
    const glm::dvec3 startVelocity =
        transform.motion.localVelocityMps;

    Observation out;
    const int ticks = static_cast<int>(kDuration / kDt);

    for (int tick = 0; tick < ticks; ++tick)
    {
        game::ship::control::ShipControlExecutionGateway::integrateAttitude(
            transform,
            snapshot.effectivePhysics,
            control,
            world,
            static_cast<float>(kDt)
        );

        (void)game::ship::control::ShipControlExecutionGateway::
            evaluateTranslationControl(
                transform,
                snapshot.effectivePhysics,
                control,
                model,
                slots,
                static_cast<float>(kDt)
            );

        keepPeak(
            out.peakMainAcceleration,
            transform.motion.mainEngineAccelerationMps2
        );
        keepPeak(
            out.peakRcsAcceleration,
            transform.motion.manoeuvreAccelerationMps2
        );
        keepPeak(
            out.peakAssistedAcceleration,
            transform.motion.assistedStabilizationAccelerationMps2
        );

        require(
            game::ship::control::ShipControlExecutionGateway::
                propagateHubTacticalTranslation(
                    transform,
                    snapshot.effectivePhysics,
                    kDt
                ),
            "translation propagation failed"
        );
    }

    out.deltaPosition =
        transform.motion.localPositionMeters - startPosition;
    out.deltaVelocity =
        transform.motion.localVelocityMps - startVelocity;
    out.finalVelocity = transform.motion.localVelocityMps;
    out.finalSpeed = glm::length(transform.motion.localVelocityMps);
    out.pitchRate = transform.pitchRate;
    out.yawRate = transform.yawRate;
    out.rollRate = transform.rollRate;
    out.gimbalYaw = transform.motion.mainGimbalYawRad;
    out.gimbalPitch = transform.motion.mainGimbalPitchRad;
    return out;
}

std::string vec(const glm::dvec3& v)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6)
        << '(' << v.x << ',' << v.y << ',' << v.z << ')';
    return out.str();
}

void report(
    Model model,
    const char* command,
    const char* expected,
    const Observation& actual,
    bool pass
)
{
    std::cout
        << "[CONTROL-ORACLE] model=" << modelName(model)
        << " command=" << command
        << " expected=\"" << expected << "\""
        << " actual={dpos=" << vec(actual.deltaPosition)
        << ",dvel=" << vec(actual.deltaVelocity)
        << ",speed=" << std::fixed << std::setprecision(6)
        << actual.finalSpeed
        << ",rates=(" << actual.pitchRate
        << ',' << actual.yawRate
        << ',' << actual.rollRate << ')'
        << ",gimbal=(" << actual.gimbalYaw
        << ',' << actual.gimbalPitch << ')'
        << ",main_a=" << vec(actual.peakMainAcceleration)
        << ",rcs_a=" << vec(actual.peakRcsAcceleration)
        << ",assist_a=" << vec(actual.peakAssistedAcceleration)
        << "}"
        << " result=" << (pass ? "PASS" : "FAIL")
        << '\n';
}

void check(
    Model model,
    const char* command,
    const char* expected,
    const Observation& actual,
    bool pass
)
{
    report(model, command, expected, actual, pass);
    require(
        pass,
        std::string(modelName(model)) + " command=" + command +
            " violated expected control->motion behavior"
    );
}

Snapshot healthySnapshot(Model model, const ShipParams& nominal)
{
    auto out = game::ship::plant::evaluatePhysicalControlModel(
        model,
        nominal,
        game::ship::plant::healthySlotState(
            game::ship::plant::physicalControlSlots(model)
        )
    );
    require(out.valid, std::string(modelName(model)) + " invalid snapshot");
    return out;
}

void runHealthyBehaviorOracle(Model model)
{
    const ShipParams nominal = nominalPhysics();
    const Snapshot snapshot = healthySnapshot(model, nominal);
    const Family family = game::ship::plant::familyOf(model);

    {
        ShipControlState c;
        c.targetSpeedRate = 1.0f;
        const auto a = execute(model, snapshot, c);
        const bool pass =
            a.finalSpeed > kTiny &&
            glm::length(a.peakMainAcceleration) > kTiny &&
            glm::dot(a.deltaVelocity, glm::dvec3(0.0, 0.0, -1.0)) > kTiny;
        check(
            model,
            "forward_burn",
            "speed increases along hull forward and main propulsion is active",
            a,
            pass
        );
    }

    {
        ShipControlState c;
        const auto a = execute(model, snapshot, c, 50.0);
        const bool pass =
            std::abs(a.finalSpeed - 50.0) < 1.0e-6 &&
            glm::length(a.deltaVelocity) < 1.0e-6 &&
            glm::length(a.peakMainAcceleration) < kDrift &&
            glm::length(a.peakRcsAcceleration) < kDrift &&
            glm::length(a.peakAssistedAcceleration) < kDrift;
        check(
            model,
            "coast",
            "no control input preserves velocity and produces no propulsion acceleration",
            a,
            pass
        );
    }

    {
        ShipControlState c;
        c.strafeInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        const bool correctActuator =
            family == Family::Assisted
                ? glm::length(a.peakAssistedAcceleration) > kTiny
                : glm::length(a.peakRcsAcceleration) > kTiny;
        const bool pass =
            a.deltaVelocity.x > kTiny &&
            std::abs(a.deltaVelocity.y) < 0.05 &&
            correctActuator;
        check(
            model,
            "strafe_right",
            family == Family::Assisted
                ? "positive body-right velocity; Assisted vectoring channel responds"
                : "positive body-right velocity; Newtonian manoeuvre/RCS channel responds",
            a,
            pass
        );
    }

    {
        ShipControlState c;
        c.liftInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        const bool correctActuator =
            family == Family::Assisted
                ? glm::length(a.peakAssistedAcceleration) > kTiny
                : glm::length(a.peakRcsAcceleration) > kTiny;
        const bool pass =
            a.deltaVelocity.y > kTiny &&
            std::abs(a.deltaVelocity.x) < 0.05 &&
            correctActuator;
        check(
            model,
            "lift_up",
            family == Family::Assisted
                ? "positive body-up velocity; Assisted vectoring channel responds"
                : "positive body-up velocity; Newtonian manoeuvre/RCS channel responds",
            a,
            pass
        );
    }

    {
        ShipControlState c;
        c.pitchInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        check(
            model,
            "pitch_positive",
            "positive pitch command creates positive pitch rate without yaw/roll command",
            a,
            a.pitchRate > kTiny &&
            std::abs(a.yawRate) < 0.05 &&
            std::abs(a.rollRate) < 0.05
        );
    }

    {
        ShipControlState c;
        c.yawInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        check(
            model,
            "yaw_positive",
            "positive yaw command creates positive yaw rate without pitch/roll command",
            a,
            a.yawRate > kTiny &&
            std::abs(a.pitchRate) < 0.05 &&
            std::abs(a.rollRate) < 0.05
        );
    }

    {
        ShipControlState c;
        c.rollInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        check(
            model,
            "roll_positive",
            "positive roll command creates positive roll rate without pitch/yaw command",
            a,
            a.rollRate > kTiny &&
            std::abs(a.pitchRate) < 0.05 &&
            std::abs(a.yawRate) < 0.05
        );
    }

    {
        ShipControlState c;
        c.targetSpeedRate = 1.0f;
        c.mainGimbalRightInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        check(
            model,
            "main_gimbal_right",
            "main burn plus right-gimbal command moves nozzle away from center and produces lateral main-thrust component",
            a,
            std::abs(a.gimbalYaw) > kTiny &&
            std::abs(a.peakMainAcceleration.x) > kTiny &&
            glm::length(a.peakMainAcceleration) > kTiny
        );
    }

    {
        ShipControlState c;
        c.targetSpeedRate = 1.0f;
        c.mainGimbalUpInput = 1.0f;
        const auto a = execute(model, snapshot, c);
        check(
            model,
            "main_gimbal_up",
            "main burn plus up-gimbal command moves nozzle away from center and produces vertical main-thrust component",
            a,
            std::abs(a.gimbalPitch) > kTiny &&
            std::abs(a.peakMainAcceleration.y) > kTiny &&
            glm::length(a.peakMainAcceleration) > kTiny
        );
    }
}

} // namespace

int main()
{
    try
    {
        constexpr std::array<Model, 6> models = {
            Model::AssistedI,
            Model::AssistedY,
            Model::AssistedX,
            Model::NewtonianI,
            Model::NewtonianY,
            Model::NewtonianX
        };

        for (const auto model : models)
            runHealthyBehaviorOracle(model);

        std::cout
            << "SHIP CONTROL BEHAVIOR ORACLE: PASS\n"
            << " - six healthy Assisted/Newtonian I/Y/X constructions\n"
            << " - each command prints EXPECTED semantics next to ACTUAL motion\n"
            << " - test verifies control->motion causality without planner/tunnel/follower\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP CONTROL BEHAVIOR ORACLE: FAIL: "
            << error.what() << '\n';
        return 1;
    }
}
