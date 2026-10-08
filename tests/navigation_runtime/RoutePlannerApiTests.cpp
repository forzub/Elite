#include "src/game/navigation/planner/RoutePlannerApi.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(double actual, double expected, double tolerance, const char* message)
{
    if (std::abs(actual - expected) > tolerance)
        throw std::runtime_error(message);
}

} // namespace

int main()
{
    try
    {
        game::navigation::planner::RoutePlanRequest request;
        request.startMeters = {5000.0, 1200.0, 0.0};
        request.goalMeters = {0.0, 0.0, 0.0};
        request.terminalOutward = {1.0, 0.0, 0.0};
        request.terminalReferenceDistanceMeters = 300.0;
        request.agentRadiusMeters = 10.0;
        request.maxSpeedMps = 120.0;
        request.acceleratingMps2 = 8.0;
        request.brakingMps2 = 8.0;
        request.lateralMps2 = 5.0;
        request.roundTurns = false;
        request.gateSpacingMeters = 500.0;
        request.terminalGateSpacingMeters = 250.0;

        const auto plan =
            game::navigation::planner::RoutePlanner::plan(request);

        require(plan.valid(), "generic RoutePlanner API did not produce a route");
        require(
            plan.disposition ==
                game::navigation::planner::RoutePlanDisposition::Ready,
            "valid route did not report Ready disposition"
        );
        require(
            plan.failureCode ==
                game::navigation::planner::RoutePlanFailureCode::None,
            "valid route exposed a failure code"
        );
        require(
            plan.executionGates.size() >= 2,
            "generic RoutePlanner API returned no executable geometry"
        );
        requireNear(
            glm::length(
                plan.executionGates.back().positionMeters -
                request.goalMeters
            ),
            0.0,
            1.0e-6,
            "generic RoutePlanner changed the requested terminal point"
        );

        // Mandatory tangent-straight regression: the route approaches a
        // protected axis from one side, traverses two hard straights around a
        // synthetic BLUE passage, then leaves toward an offset terminal.
        {
            using namespace game::navigation::planner;

            RoutePlanRequest constrained;
            constrained.startMeters = {-3500.0, 1800.0, 0.0};
            constrained.goalMeters = {6500.0, 2600.0, 0.0};
            constrained.terminalOutward = {1.0, 0.0, 0.0};
            constrained.terminalReferenceDistanceMeters = 300.0;
            constrained.agentRadiusMeters = 10.0;
            constrained.maxSpeedMps = 120.0;
            constrained.acceleratingMps2 = 10.0;
            constrained.brakingMps2 = 10.0;
            constrained.lateralMps2 = 8.0;
            constrained.maxAngularVelocityRadPerSecond = 0.2;
            constrained.maxAngularAccelerationRadPerSecond2 = 0.3;
            constrained.roundTurns = true;
            constrained.gateSpacingMeters = 500.0;
            constrained.terminalGateSpacingMeters = 250.0;
            constrained.requiredViaPointsMeters = {
                {-1300.0, 0.0, 0.0},
                {-600.0, 0.0, 0.0},
                {600.0, 0.0, 0.0},
                {1300.0, 0.0, 0.0}
            };

            MandatoryTangentStraightConstraint inbound;
            inbound.startMeters = {-1300.0, 0.0, 0.0};
            inbound.endMeters = {-600.0, 0.0, 0.0};
            inbound.minimumStraightMeters = 700.0;
            inbound.inbound = true;

            MandatoryTangentStraightConstraint outbound;
            outbound.startMeters = {600.0, 0.0, 0.0};
            outbound.endMeters = {1300.0, 0.0, 0.0};
            outbound.minimumStraightMeters = 700.0;
            outbound.inbound = false;

            constrained.mandatoryTangentStraights = {
                inbound,
                outbound
            };

            const auto constrainedPlan =
                RoutePlanner::plan(constrained);
            require(
                constrainedPlan.valid(),
                "mandatory tangent-straight route failed"
            );

            const glm::dvec3 axis(1.0, 0.0, 0.0);
            bool sawInboundTangentArc = false;
            bool sawOutboundTangentArc = false;
            bool sawInboundHardLine = false;
            bool sawOutboundHardLine = false;

            for (const auto& curve : constrainedPlan.routeCurves)
            {
                if (curve.kind == RouteCurveKind::CircularArc)
                {
                    if (glm::dot(curve.endForward, axis) > 0.999 &&
                        curve.endMeters.x < inbound.startMeters.x + 1.0e-6)
                    {
                        sawInboundTangentArc = true;
                    }

                    if (glm::dot(curve.startForward, axis) > 0.999 &&
                        curve.startMeters.x > outbound.endMeters.x - 1.0e-6)
                    {
                        sawOutboundTangentArc = true;
                    }
                }

                if (curve.kind == RouteCurveKind::Line)
                {
                    const glm::dvec3 delta =
                        curve.endMeters - curve.startMeters;
                    const double length = glm::length(delta);
                    if (length <= 1.0e-9)
                        continue;

                    const glm::dvec3 direction = delta / length;
                    if (glm::dot(direction, axis) < 0.999999)
                        continue;

                    if (curve.startMeters.x <= inbound.startMeters.x + 1.0e-6 &&
                        curve.endMeters.x >= inbound.endMeters.x - 1.0e-6 &&
                        std::abs(curve.startMeters.y) < 1.0e-6 &&
                        std::abs(curve.endMeters.y) < 1.0e-6)
                    {
                        sawInboundHardLine = true;
                    }

                    if (curve.startMeters.x <= outbound.startMeters.x + 1.0e-6 &&
                        curve.endMeters.x >= outbound.endMeters.x - 1.0e-6 &&
                        std::abs(curve.startMeters.y) < 1.0e-6 &&
                        std::abs(curve.endMeters.y) < 1.0e-6)
                    {
                        sawOutboundHardLine = true;
                    }
                }
            }

            require(
                sawInboundHardLine && sawOutboundHardLine,
                "planner rounded away a protected tangent straight"
            );
            require(
                sawInboundTangentArc,
                "planner did not join inbound hard straight tangentially"
            );
            require(
                sawOutboundTangentArc,
                "planner did not leave outbound hard straight tangentially"
            );
        }

        game::navigation::planner::RoutePlanRequest invalid = request;
        invalid.maxSpeedMps = std::numeric_limits<double>::quiet_NaN();
        const auto invalidPlan =
            game::navigation::planner::RoutePlanner::plan(invalid);
        require(
            !invalidPlan.valid(),
            "invalid planner request unexpectedly produced a route"
        );
        require(
            invalidPlan.disposition !=
                game::navigation::planner::RoutePlanDisposition::Ready,
            "invalid planner request reported Ready"
        );
        require(
            !invalidPlan.userMessage.empty(),
            "planner failure did not expose a human-readable message"
        );

        std::cout << "ROUTE PLANNER API TESTS: PASS\n";
        std::cout << " - generic caller reaches exact goal without docking-private types\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "ROUTE PLANNER API TESTS: FAIL: "
                  << error.what() << "\n";
        return 1;
    }
}
