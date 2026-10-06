#include "src/game/navigation/diagnostics/NavigationScenarioRunner.h"

#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/autopilot/ClientRouteAutopilot.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace game::navigation::diagnostics
{
namespace
{

using Autopilot =
    game::navigation::autopilot::ClientRouteAutopilot;
using Agent =
    game::navigation::autopilot::RouteFollowerAgentState;

glm::dvec3 normalizedOr(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    if (length > 1.0e-12)
        return value / length;

    const double fallbackLength = glm::length(fallback);
    if (fallbackLength > 1.0e-12)
        return fallback / fallbackLength;

    return {1.0, 0.0, 0.0};
}

void setBasis(
    ShipTransform& transform,
    const glm::dvec3& requestedForward,
    const glm::dvec3& requestedRight,
    const glm::dvec3& requestedUp
)
{
    const glm::dvec3 forward =
        normalizedOr(requestedForward, {1.0, 0.0, 0.0});

    glm::dvec3 right =
        requestedRight -
        forward * glm::dot(requestedRight, forward);

    if (glm::length(right) <= 1.0e-9)
    {
        glm::dvec3 up =
            requestedUp -
            forward * glm::dot(requestedUp, forward);
        if (glm::length(up) <= 1.0e-9)
            up = std::abs(forward.y) < 0.92
                ? glm::dvec3(0.0, 1.0, 0.0)
                : glm::dvec3(0.0, 0.0, 1.0);
        up = glm::normalize(up);
        right = glm::cross(forward, up);
    }

    right = glm::normalize(right);
    glm::dvec3 up =
        glm::normalize(glm::cross(right, forward));

    if (glm::dot(up, requestedUp) < 0.0)
    {
        right = -right;
        up = -up;
    }

    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] =
        glm::vec4(glm::vec3(right), 0.0f);
    transform.orientation[1] =
        glm::vec4(glm::vec3(up), 0.0f);
    transform.orientation[2] =
        glm::vec4(glm::vec3(-forward), 0.0f);
}

Agent makeAgent(const ShipTransform& transform)
{
    Agent out;
    out.positionMapMeters =
        transform.motion.localPositionMeters;
    out.velocityMapMetersPerSecond =
        transform.motion.localVelocityMps;
    out.forwardMap = glm::dvec3(transform.forward());
    out.rightMap = glm::dvec3(transform.right());
    out.upMap = glm::dvec3(transform.up());
    out.pitchRateRadPerSec = transform.pitchRate;
    out.yawRateRadPerSec = transform.yawRate;
    out.rollRateRadPerSec = transform.rollRate;
    return out;
}

const char* curveKindName(
    game::navigation::planner::RouteCurveKind kind
)
{
    using Kind =
        game::navigation::planner::RouteCurveKind;
    switch (kind)
    {
        case Kind::Line: return "line";
        case Kind::CircularArc: return "arc";
        case Kind::CubicBezier: return "bezier";
    }
    return "unknown";
}

} // namespace

NavigationScenarioRunner::Result
NavigationScenarioRunner::run(const Request& source)
{
    Result result;

    if (!(source.deltaSeconds > 0.0) ||
        !std::isfinite(source.deltaSeconds) ||
        !(source.maximumRunSeconds > 0.0) ||
        !std::isfinite(source.maximumRunSeconds) ||
        source.planningLeadSeconds < 0.0 ||
        !std::isfinite(source.planningLeadSeconds))
    {
        result.failure = "invalid scenario timing";
        return result;
    }

    Request request = source;

    // Initial kinematics are authoritative measured state. Scenario callers
    // cannot accidentally author a planner start that contradicts the craft
    // they later hand to the follower.
    Agent executionInitial = request.initialAgent;
    executionInitial.positionMapMeters +=
        executionInitial.velocityMapMetersPerSecond *
        request.planningLeadSeconds;

    request.route.startMeters =
        executionInitial.positionMapMeters;
    request.route.initialSpeedMps =
        glm::length(executionInitial.velocityMapMetersPerSecond);
    request.route.hasInitialForward = true;
    request.route.initialForward =
        normalizedOr(
            executionInitial.forwardMap,
            {1.0, 0.0, 0.0}
        );

    result.plan =
        game::navigation::planner::RoutePlanner::plan(
            request.route
        );
    result.plannerAccepted = result.plan.valid();
    if (!result.plannerAccepted)
    {
        result.failure =
            !result.plan.userMessage.empty()
                ? result.plan.userMessage
                : result.plan.failure;
        if (result.failure.empty())
            result.failure = "planner rejected scenario";
        return result;
    }

    Autopilot::State autopilot;
    const glm::dvec3 routeUp =
        request.hasRouteUpReference
            ? request.routeUpReference
            : glm::dvec3(0.0);

    std::string followerStartFailure;
    if (!Autopilot::start(
            autopilot,
            result.plan,
            executionInitial,
            request.controlLaw,
            request.ship,
            request.startUniverseTimeSeconds +
                request.planningLeadSeconds,
            1,
            request.trackingToleranceMeters,
            routeUp,
            request.enforceTerminalStop,
            &followerStartFailure
        ))
    {
        result.failure =
            "follower rejected planner-produced route: " +
            (followerStartFailure.empty()
                ? std::string("unknown")
                : followerStartFailure);
        return result;
    }
    result.followerAccepted = true;

    ShipTransform transform {};
    transform.motion.mode =
        game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.localControlLaw =
        request.controlLaw;
    transform.motion.localPositionMeters =
        executionInitial.positionMapMeters;
    transform.motion.localVelocityMps =
        executionInitial.velocityMapMetersPerSecond;
    transform.pitchRate =
        static_cast<float>(executionInitial.pitchRateRadPerSec);
    transform.yawRate =
        static_cast<float>(executionInitial.yawRateRadPerSec);
    transform.rollRate =
        static_cast<float>(executionInitial.rollRateRadPerSec);
    transform.setWorldPositionMeters(
        executionInitial.positionMapMeters
    );
    setBasis(
        transform,
        executionInitial.forwardMap,
        executionInitial.rightMap,
        executionInitial.upMap
    );

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "navigation-scenario-runner";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;
    transform.motion.travelFrame = frame;

    WorldParams world {};

    const std::size_t maxSteps =
        static_cast<std::size_t>(
            std::ceil(
                request.maximumRunSeconds /
                request.deltaSeconds
            )
        );

    for (std::size_t step = 0; step < maxSteps; ++step)
    {
        const double elapsed =
            static_cast<double>(step) *
            request.deltaSeconds;
        const double universeTime =
            request.startUniverseTimeSeconds +
            request.planningLeadSeconds +
            elapsed;

        const auto agent = makeAgent(transform);
        const auto output =
            Autopilot::update(
                autopilot,
                agent,
                request.controlLaw,
                request.ship,
                universeTime,
                request.deltaSeconds
            );

        if (!output.valid)
        {
            result.failure =
                "follower emitted invalid output";
            return result;
        }

        Frame trace;
        trace.timeSeconds = elapsed;
        trace.positionMapMeters = agent.positionMapMeters;
        trace.velocityMapMps =
            agent.velocityMapMetersPerSecond;
        trace.forwardMap = agent.forwardMap;
        trace.upMap = agent.upMap;
        trace.routeCurveIndex = output.routeCurveIndex;
        trace.routeCurvaturePerMeter =
            output.routeCurvaturePerMeter;
        trace.routeRadiusMeters = output.routeRadiusMeters;
        trace.crossTrackErrorMeters =
            output.crossTrackErrorMeters;
        trace.targetSpeedMps = output.targetSpeedMps;
        trace.forwardErrorRad = output.forwardErrorRad;
        trace.rollErrorRad = output.signedRollErrorRad;
        trace.forwardInput = output.control.forwardInput;
        trace.pitchInput = output.control.pitchInput;
        trace.yawInput = output.control.yawInput;
        trace.rollInput = output.control.rollInput;
        trace.strafeInput = output.control.strafeInput;
        trace.liftInput = output.control.liftInput;
        trace.terminalHold = output.terminalHold;
        trace.complete = output.complete;
        result.frames.push_back(trace);

        if (output.complete)
        {
            result.completed = true;
            return result;
        }

        SharedShipPhysics::integrate(
            transform,
            request.ship,
            output.control,
            world,
            static_cast<float>(request.deltaSeconds)
        );

        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            transform.motion,
            frame,
            request.ship,
            static_cast<float>(request.deltaSeconds),
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
            request.ship,
            request.deltaSeconds
        );
    }

    result.failure =
        "scenario reached maximum runtime before completion";
    return result;
}

std::string NavigationScenarioRunner::Result::reportText() const
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);

    out << "NAVIGATION_SCENARIO\n";
    out << "planner_accepted=" << (plannerAccepted ? 1 : 0) << "\n";
    out << "follower_accepted=" << (followerAccepted ? 1 : 0) << "\n";
    out << "completed=" << (completed ? 1 : 0) << "\n";
    if (!failure.empty())
        out << "failure=" << failure << "\n";

    out << "route_curves=" << plan.routeCurves.size() << "\n";
    for (std::size_t i = 0; i < plan.routeCurves.size(); ++i)
    {
        const auto& curve = plan.routeCurves[i];
        out << "curve[" << i << "]"
            << " kind=" << curveKindName(curve.kind)
            << " s0=" << curve.startProgressMeters
            << " s1=" << curve.endProgressMeters
            << " vmax=" << curve.maxSpeedMps
            << " radius=" << curve.arcRadiusMeters
            << " curvature0="
            << curve.curvatureAtProgress(
                   curve.startProgressMeters)
            << " tangent0=("
            << curve.tangentAtProgress(
                   curve.startProgressMeters).x << ","
            << curve.tangentAtProgress(
                   curve.startProgressMeters).y << ","
            << curve.tangentAtProgress(
                   curve.startProgressMeters).z << ")"
            << " tangent1=("
            << curve.tangentAtProgress(
                   curve.endProgressMeters).x << ","
            << curve.tangentAtProgress(
                   curve.endProgressMeters).y << ","
            << curve.tangentAtProgress(
                   curve.endProgressMeters).z << ")"
            << "\n";
    }

    for (std::size_t i = 1; i < plan.routeCurves.size(); ++i)
    {
        const auto& a = plan.routeCurves[i - 1];
        const auto& b = plan.routeCurves[i];
        const glm::dvec3 ta =
            normalizedOr(
                a.tangentAtProgress(a.endProgressMeters),
                a.endForward
            );
        const glm::dvec3 tb =
            normalizedOr(
                b.tangentAtProgress(b.startProgressMeters),
                b.startForward
            );
        const double tangentDot =
            std::clamp(glm::dot(ta, tb), -1.0, 1.0);
        out << "transition[" << (i - 1) << "->" << i << "]"
            << " tangent_angle_rad="
            << std::acos(tangentDot)
            << "\n";
    }

    out << "frames=" << frames.size() << "\n";
    for (const auto& frame : frames)
    {
        out << "frame"
            << " t=" << frame.timeSeconds
            << " pos=("
            << frame.positionMapMeters.x << ","
            << frame.positionMapMeters.y << ","
            << frame.positionMapMeters.z << ")"
            << " vel=("
            << frame.velocityMapMps.x << ","
            << frame.velocityMapMps.y << ","
            << frame.velocityMapMps.z << ")"
            << " curve=" << frame.routeCurveIndex
            << " k=" << frame.routeCurvaturePerMeter
            << " R=" << frame.routeRadiusMeters
            << " cross=" << frame.crossTrackErrorMeters
            << " target_v=" << frame.targetSpeedMps
            << " forward_err=" << frame.forwardErrorRad
            << " roll_err=" << frame.rollErrorRad
            << " input=("
            << frame.forwardInput << ","
            << frame.pitchInput << ","
            << frame.yawInput << ","
            << frame.rollInput << ","
            << frame.strafeInput << ","
            << frame.liftInput << ")"
            << " hold=" << (frame.terminalHold ? 1 : 0)
            << " complete=" << (frame.complete ? 1 : 0)
            << "\n";
    }

    return out.str();
}

bool NavigationScenarioRunner::Result::saveReport(
    const std::string& path
) const
{
    const std::filesystem::path output(path);
    if (!output.parent_path().empty())
        std::filesystem::create_directories(
            output.parent_path()
        );

    std::ofstream stream(output);
    if (!stream)
        return false;

    stream << reportText();
    return static_cast<bool>(stream);
}

} // namespace game::navigation::diagnostics
