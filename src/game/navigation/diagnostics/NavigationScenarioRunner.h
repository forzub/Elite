#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/autopilot/ClientRouteAutopilot.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::diagnostics
{

// Headless production-navigation harness.
//
// The scenario owns no private planner/follower implementation. It drives the
// same RoutePlanner -> ClientRouteAutopilot -> ordinary ShipControlState ->
// SharedShipPhysics/DynamicMotionSystem chain used by the game.
//
// Tests should describe geometry/state here and inspect the resulting trace;
// they must not manufacture RoutePlan internals when testing the integrated
// navigation chain.
class NavigationScenarioRunner final
{
public:
    using AgentState =
        game::navigation::autopilot::AutopilotAgentState;

    struct Request
    {
        game::navigation::planner::RoutePlanRequest route;
        AgentState initialAgent;
        ShipParams ship;
        game::navigation::LocalFlightControlLaw controlLaw =
            game::navigation::defaultLocalFlightControlLaw();

        // Models asynchronous planning while the craft coasts with zero
        // commanded linear acceleration. The planner origin and execution
        // initial state are advanced by v * planningLeadSeconds.
        double planningLeadSeconds = 0.0;

        double startUniverseTimeSeconds = 0.0;
        double deltaSeconds = 0.02;
        double maximumRunSeconds = 30.0;
        double trackingToleranceMeters = 20.0;

        glm::dvec3 routeUpReference {0.0, 1.0, 0.0};
        bool hasRouteUpReference = false;
        bool enforceTerminalStop = true;
    };

    struct Frame
    {
        double timeSeconds = 0.0;
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMps {0.0};
        glm::dvec3 forwardMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        std::size_t routeCurveIndex = 0;
        double routeCurvaturePerMeter = 0.0;
        double routeRadiusMeters = 0.0;
        double crossTrackErrorMeters = 0.0;
        double targetSpeedMps = 0.0;
        double courseErrorRad = 0.0;
        double hullForwardErrorRad = 0.0;
        bool courseCaptureActive = false;
        double captureMeetingRouteProgressMeters = 0.0;
        double captureMeetingJoinAngleRad = 0.0;
        double rollErrorRad = 0.0;

        double forwardInput = 0.0;
        double pitchInput = 0.0;
        double yawInput = 0.0;
        double rollInput = 0.0;
        double strafeInput = 0.0;
        double liftInput = 0.0;

        bool terminalHold = false;
        bool complete = false;
    };

    struct Result
    {
        bool plannerAccepted = false;
        bool followerAccepted = false;
        bool completed = false;
        std::string failure;

        game::navigation::planner::RoutePlan plan;
        std::vector<Frame> frames;

        [[nodiscard]] std::string reportText() const;
        bool saveReport(const std::string& path) const;
    };

    [[nodiscard]] static Result run(const Request& request);
};

} // namespace game::navigation::diagnostics
