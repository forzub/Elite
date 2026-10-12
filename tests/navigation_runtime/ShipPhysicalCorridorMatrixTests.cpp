#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/autopilot/ClientRouteAutopilot.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/ShipDescriptor.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Autopilot =
    game::navigation::autopilot::ClientRouteAutopilot;
using Agent =
    game::navigation::autopilot::AutopilotAgentState;
using Model =
    game::ship::plant::PhysicalControlModelId;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDt = 0.02;
constexpr double kTunnelHalfWidthMeters = 12.0;
constexpr double kRouteSpeedMps = 8.0;
constexpr double kRadiusMeters = 300.0;
constexpr double kSweepRadians = 0.5 * kPi;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

const char* modelName(Model model)
{
    switch (model)
    {
        case Model::AssistedI: return "Assisted-I";
        case Model::AssistedY: return "Assisted-Y";
        case Model::AssistedX: return "Assisted-X";
        case Model::NewtonianI: return "Newtonian-I";
        case Model::NewtonianY: return "Newtonian-Y";
        case Model::NewtonianX: return "Newtonian-X";
    }
    return "unknown";
}

struct Basis
{
    glm::dvec3 forward {1.0, 0.0, 0.0};
    glm::dvec3 right {0.0, 0.0, 1.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
};

Basis basisForForward(const glm::dvec3& requested)
{
    const glm::dvec3 forward = glm::normalize(requested);
    glm::dvec3 up(0.0, 1.0, 0.0);
    glm::dvec3 right = glm::cross(forward, up);
    if (glm::length(right) <= 1.0e-9)
    {
        up = {0.0, 0.0, 1.0};
        right = glm::cross(forward, up);
    }

    right = glm::normalize(right);
    up = glm::normalize(glm::cross(right, forward));
    return {forward, right, up};
}

void setBasis(ShipTransform& transform, const Basis& basis)
{
    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] =
        glm::vec4(glm::vec3(basis.right), 0.0f);
    transform.orientation[1] =
        glm::vec4(glm::vec3(basis.up), 0.0f);
    transform.orientation[2] =
        glm::vec4(glm::vec3(-basis.forward), 0.0f);
}

ShipParams cobraNominalParams()
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
    p.forwardMainEngineAccelerationMps2 = 73.549875f;
    p.reverseMainEngineAccelerationMps2 = 73.549875f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 73.549875f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 2.0f;

    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;
    p.stopSpeedEpsilonMps = 0.05f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

game::navigation::planner::RoutePlan tunnelPlan()
{
    using namespace game::navigation::planner;

    RoutePlan plan;
    plan.disposition = RoutePlanDisposition::Ready;
    plan.failureCode = RoutePlanFailureCode::None;

    constexpr int GateCount = 65;
    plan.gates.reserve(GateCount);
    plan.executionGates.reserve(GateCount);

    for (int i = 0; i < GateCount; ++i)
    {
        const double t =
            kSweepRadians *
            static_cast<double>(i) /
            static_cast<double>(GateCount - 1);

        RouteGate gate;
        gate.positionMeters = {
            kRadiusMeters * std::sin(t),
            0.0,
            kRadiusMeters * (1.0 - std::cos(t))
        };
        gate.forward = glm::normalize(glm::dvec3(
            std::cos(t),
            0.0,
            std::sin(t)
        ));
        gate.speedMps = kRouteSpeedMps;
        plan.gates.push_back(gate);
        plan.executionGates.push_back(gate);
    }

    RouteCurveSegment arc;
    arc.kind = RouteCurveKind::CircularArc;
    arc.startProgressMeters = 0.0;
    arc.endProgressMeters = kRadiusMeters * kSweepRadians;
    arc.maxSpeedMps = kRouteSpeedMps;
    arc.startMeters = plan.executionGates.front().positionMeters;
    arc.endMeters = plan.executionGates.back().positionMeters;
    arc.startForward = plan.executionGates.front().forward;
    arc.endForward = plan.executionGates.back().forward;
    arc.arcCenterMeters = {0.0, 0.0, kRadiusMeters};
    arc.arcNormal = {0.0, -1.0, 0.0};
    arc.arcRadiusMeters = kRadiusMeters;
    arc.arcSweepRadians = kSweepRadians;
    plan.routeCurves.push_back(arc);

    return plan;
}

Agent makeAgent(const ShipTransform& transform)
{
    Agent out;
    out.positionMapMeters = transform.motion.localPositionMeters;
    out.velocityMapMetersPerSecond = transform.motion.localVelocityMps;
    out.forwardMap = glm::dvec3(transform.forward());
    out.rightMap = glm::dvec3(transform.right());
    out.upMap = glm::dvec3(transform.up());
    out.pitchRateRadPerSec = transform.pitchRate;
    out.yawRateRadPerSec = transform.yawRate;
    out.rollRateRadPerSec = transform.rollRate;
    return out;
}

double distanceToAuthoredArc(const glm::dvec3& position)
{
    const glm::dvec3 center(0.0, 0.0, kRadiusMeters);
    const glm::dvec3 radial = position - center;
    const double radialXZ = std::hypot(radial.x, radial.z);

    return std::hypot(
        radialXZ - kRadiusMeters,
        position.y
    );
}

void runModel(Model model)
{
    ShipDescriptor descriptor;
    descriptor.physicalControlModel = model;
    descriptor.localFlightControlLaw =
        game::ship::plant::nativeFlightLaw(model);
    descriptor.physics = cobraNominalParams();

    const auto capability =
        game::ship::capability::buildHealthyShipCapability(
            descriptor
        );

    require(
        capability.valid,
        std::string(modelName(model)) +
            " produced invalid healthy capability"
    );

    const ShipParams params = capability.effectivePhysics;
    const auto law = descriptor.localFlightControlLaw;
    const auto plan = tunnelPlan();

    WorldParams world {};

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "ship-physical-corridor-matrix";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;

    ShipTransform transform {};
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.travelFrame = frame;
    transform.motion.localControlLaw = law;
    transform.motion.localPositionMeters =
        plan.executionGates.front().positionMeters;
    transform.motion.localVelocityMps =
        plan.executionGates.front().forward * kRouteSpeedMps;
    transform.motion.targetForwardSpeedMps = kRouteSpeedMps;
    transform.setWorldPositionMeters(
        transform.motion.localPositionMeters
    );
    setBasis(
        transform,
        basisForForward(plan.executionGates.front().forward)
    );

    Autopilot::State autopilot;
    std::string failureReason;

    require(
        Autopilot::start(
            autopilot,
            plan,
            makeAgent(transform),
            law,
            params,
            0.0,
            static_cast<std::uint64_t>(3000) +
                static_cast<std::uint64_t>(model),
            kTunnelHalfWidthMeters,
            glm::dvec3(0.0, 1.0, 0.0),
            false,
            &failureReason
        ),
        std::string(modelName(model)) +
            " autopilot rejected common tunnel: " +
            failureReason
    );

    double maxCrossTrack = 0.0;
    bool reachedEnd = false;

    for (int tick = 0; tick < 6000; ++tick)
    {
        const auto output =
            Autopilot::update(
                autopilot,
                makeAgent(transform),
                law,
                params,
                static_cast<double>(tick) * kDt,
                kDt
            );

        require(
            output.valid,
            std::string(modelName(model)) +
                " emitted invalid autopilot output"
        );

        SharedShipPhysics::integrate(
            transform,
            params,
            output.control,
            world,
            static_cast<float>(kDt)
        );

        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            transform.motion,
            frame,
            params,
            static_cast<float>(kDt),
            output.control.targetSpeedRate,
            output.control.cruiseActive,
            output.control.forwardInput,
            output.control.liftInput,
            output.control.strafeInput,
            transform.forward(),
            transform.right(),
            transform.up()
        );

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            transform.motion,
            transform.worldPosition,
            frame,
            params,
            kDt
        );

        const double crossTrack =
            distanceToAuthoredArc(
                transform.motion.localPositionMeters
            );
        maxCrossTrack =
            std::max(maxCrossTrack, crossTrack);

        require(
            crossTrack <= kTunnelHalfWidthMeters + 1.0e-6,
            std::string(modelName(model)) +
                " left the common corridor"
        );

        if (output.complete ||
            output.exactRemainingRouteMeters <= 1.0)
        {
            reachedEnd = true;
            break;
        }
    }

    require(
        reachedEnd,
        std::string(modelName(model)) +
            " failed to complete the common corridor"
    );

    std::cout
        << "[SHIP-PHYSICAL-CORRIDOR] model="
        << modelName(model)
        << " law="
        << game::navigation::localFlightControlLawName(law)
        << " capability_state="
        << static_cast<int>(capability.controlState)
        << " max_cross_track_m=" << maxCrossTrack
        << "\n";
}

void testAllHealthyPhysicalModelsUseSameControlCorridor()
{
    constexpr std::array<Model, 6> Models = {
        Model::AssistedI,
        Model::AssistedY,
        Model::AssistedX,
        Model::NewtonianI,
        Model::NewtonianY,
        Model::NewtonianX
    };

    for (const auto model : Models)
        runModel(model);
}

} // namespace

int main()
{
    try
    {
        testAllHealthyPhysicalModelsUseSameControlCorridor();

        std::cout
            << "SHIP PHYSICAL CORRIDOR MATRIX: PASS\n"
            << " - one authored tunnel, one input/autopilot contract\n"
            << " - all Assisted/Newtonian I/Y/X models are replaceable below control\n"
            << " - visual ship model and navigation geometry are unchanged\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP PHYSICAL CORRIDOR MATRIX: FAIL: "
            << error.what() << "\n";
        return 1;
    }
}
