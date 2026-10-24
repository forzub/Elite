#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/planner/RouteEntryBinder.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

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

        // Production bridge regression: the public RoutePlanner must be able
        // to publish the continuous optimizer as authoritative CubicBezier
        // geometry and sample execution gates from that same path.
        {
            using namespace game::navigation::planner;

            RoutePlanRequest continuous;
            continuous.startMeters = {0.0, 0.0, 0.0};
            continuous.goalMeters = {-6000.0, 0.0, 0.0};
            continuous.terminalOutward = {1.0, 0.0, 0.0};
            continuous.agentRadiusMeters = 17.3;
            continuous.maxSpeedMps = 80.0;
            continuous.acceleratingMps2 = 8.0;
            continuous.brakingMps2 = 8.0;
            continuous.lateralMps2 = 20.0;
            continuous.initialSpeedMps = 0.0;
            continuous.terminalSpeedMps = 25.0;
            continuous.roundTurns = true;
            continuous.useContinuousAssistedOptimizer = true;
            continuous.hasInitialForward = true;
            continuous.initialForward = {1.0, 0.0, 0.0};
            continuous.initialProtectedStraightMeters = 150.0;
            continuous.terminalApproachLengthMeters = 700.0;
            continuous.gateSpacingMeters = 500.0;
            continuous.terminalGateSpacingMeters = 250.0;
            continuous.terminalDenseDistanceMeters = 2000.0;

            continuous.initialForwardLeadMeters = 1200.0;

            const auto optimized =
                RoutePlanner::plan(continuous);

            require(
                optimized.valid(),
                "continuous Assisted production bridge did not produce a route"
            );
            // The optimizer reports the selected preset before its READY
            // marker. Assert the actual public diagnostics contract, not the
            // obsolete adjacent words "continuous-assisted ready".
            require(
                optimized.diagnosticSummary.find(
                    "continuous-assisted preset=STANDARD ready curves="
                ) != std::string::npos &&
                optimized.diagnosticSummary.find(" execution_gates=") !=
                    std::string::npos &&
                optimized.diagnosticSummary.find(" evals=") !=
                    std::string::npos,
                "continuous Assisted route did not expose current optimizer diagnostics"
            );

            bool sawBezier = false;
            double maxCurvature = 0.0;
            for (const auto& curve : optimized.routeCurves)
            {
                if (curve.kind == RouteCurveKind::CubicBezier)
                    sawBezier = true;

                const double length =
                    curve.endProgressMeters -
                    curve.startProgressMeters;
                for (int i = 0; i <= 32; ++i)
                {
                    const double progress =
                        curve.startProgressMeters +
                        length *
                            static_cast<double>(i) / 32.0;
                    maxCurvature =
                        std::max(
                            maxCurvature,
                            curve.curvatureAtProgress(progress)
                        );
                }
            }

            require(
                sawBezier,
                "continuous Assisted production bridge published no Bezier geometry"
            );
            require(
                !optimized.routeCurves.empty() &&
                optimized.routeCurves.front().kind == RouteCurveKind::Line,
                "continuous Assisted route did not author the ballistic start lead"
            );
            require(
                optimized.routeCurves.front().endProgressMeters -
                    optimized.routeCurves.front().startProgressMeters >=
                    continuous.initialForwardLeadMeters - 1.0e-6,
                "continuous Assisted route curved before the required ballistic lead ended"
            );
            require(
                optimized.executionGates.size() > optimized.gates.size(),
                "continuous Assisted execution geometry was not sampled densely"
            );

            const double directDistance =
                glm::length(
                    continuous.goalMeters -
                    continuous.startMeters
                );
            require(
                optimized.routeCurves.back().endProgressMeters <
                    directDistance * 5.0,
                "continuous Assisted production bridge produced a runaway loop"
            );

            const double requiredRadius =
                continuous.maxSpeedMps *
                continuous.maxSpeedMps /
                continuous.lateralMps2;
            require(
                maxCurvature <=
                    1.0 / requiredRadius + 1.0e-6,
                "continuous Assisted production bridge violated v^2/a radius"
            );

            const double tunnelSafeRadius =
                game::navigation::OrdinaryRoutePolicy::
                    requiredGuidanceTunnelRadiusMeters(
                        continuous.gateSpacingMeters,
                        continuous.vehicleLengthMeters
                    );
            require(
                maxCurvature <=
                    1.0 / tunnelSafeRadius + 1.0e-6,
                "continuous Assisted route violated HUD tunnel turn readability"
            );
            require(
                optimized.diagnosticSummary.find(
                    "tunnel_radius_floor_m="
                ) != std::string::npos,
                "continuous Assisted route did not expose tunnel radius floor"
            );

            requireNear(
                glm::length(
                    optimized.executionGates.front().positionMeters -
                    continuous.startMeters
                ),
                0.0,
                1.0e-6,
                "continuous Assisted execution route changed its start"
            );
            requireNear(
                glm::length(
                    optimized.executionGates.back().positionMeters -
                    continuous.goalMeters
                ),
                0.0,
                1.0e-6,
                "continuous Assisted execution route changed its goal"
            );
            requireNear(
                optimized.executionGates.back().speedMps,
                continuous.terminalSpeedMps,
                1.0e-6,
                "continuous Assisted route discarded requested terminal speed"
            );
        }

        // Late-bound start contract: elapsed wall-clock time does not
        // invalidate a route. Only spatial consumption of the authored first
        // straight matters, and the downstream tail can be retained while a
        // new live entry connector is built.
        {
            using namespace game::navigation::planner;

            RoutePlan synthetic;
            synthetic.disposition = RoutePlanDisposition::Ready;
            synthetic.failureCode = RoutePlanFailureCode::None;

            double progress = 0.0;
            const auto addLine =
                [&](const glm::dvec3& a, const glm::dvec3& b)
                {
                    RouteCurveSegment curve;
                    curve.kind = RouteCurveKind::Line;
                    curve.startProgressMeters = progress;
                    curve.endProgressMeters =
                        progress + glm::length(b - a);
                    curve.maxSpeedMps = 120.0;
                    curve.startMeters = a;
                    curve.endMeters = b;
                    curve.startForward = glm::normalize(b - a);
                    curve.endForward = curve.startForward;
                    synthetic.routeCurves.push_back(curve);
                    progress = curve.endProgressMeters;
                };

            addLine({0.0, 0.0, 0.0}, {1000.0, 0.0, 0.0});
            addLine({1000.0, 0.0, 0.0}, {2000.0, 0.0, 0.0});
            addLine({2000.0, 0.0, 0.0}, {3000.0, 0.0, 0.0});

            for (int i = 0; i <= 6; ++i)
            {
                RouteGate gate;
                gate.positionMeters = {
                    500.0 * static_cast<double>(i),
                    0.0,
                    0.0
                };
                gate.forward = {1.0, 0.0, 0.0};
                gate.speedMps = i == 6 ? 0.0 : 120.0;
                synthetic.executionGates.push_back(gate);
                synthetic.gates.push_back(gate);
            }

            const auto onPrefix =
                RouteEntryBinder::inspectBallisticPrefix(
                    synthetic,
                    {450.0, 0.0, 0.0},
                    20.0
                );
            require(
                onPrefix.disposition ==
                    BallisticPrefixDisposition::OnPrefix,
                "late-bound handoff rejected live progress on ballistic prefix"
            );
            requireNear(
                onPrefix.remainingMeters,
                550.0,
                1.0e-6,
                "late-bound handoff computed wrong remaining straight"
            );

            const auto pastPrefix =
                RouteEntryBinder::inspectBallisticPrefix(
                    synthetic,
                    {1200.0, 0.0, 0.0},
                    20.0
                );
            require(
                pastPrefix.disposition ==
                    BallisticPrefixDisposition::PastPrefix,
                "late-bound handoff did not detect consumed ballistic prefix"
            );

            const auto offAxis =
                RouteEntryBinder::inspectBallisticPrefix(
                    synthetic,
                    {450.0, 50.0, 0.0},
                    20.0
                );
            require(
                offAxis.disposition ==
                    BallisticPrefixDisposition::OffAxis,
                "late-bound handoff did not detect ballistic corridor departure"
            );

            const auto backbone =
                RouteEntryBinder::extractBackbone(
                    synthetic,
                    false,
                    {},
                    {}
                );
            require(
                backbone.valid,
                "late-bound route could not retain immutable terminal tail"
            );
            requireNear(
                glm::length(
                    backbone.anchorMeters -
                    glm::dvec3(2000.0, 0.0, 0.0)
                ),
                0.0,
                1.0e-6,
                "late-bound route selected wrong direct-route tail anchor"
            );
        }

        // Production Assisted continuous backend: a mandatory BLUE release
        // protects only the actual 150m release straight, not an unrelated
        // 8000m "room to manoeuvre" budget. The final protected docking axis
        // is 900m; braking must remain feasible on the whole free leg.
        {
            using namespace game::navigation::planner;
            RoutePlanRequest terminal;
            terminal.startMeters = {0.0, 0.0, 0.0};
            terminal.goalMeters = {4500.0, 0.0, 15000.0};
            terminal.terminalOutward = {0.0, 0.0, -1.0};
            terminal.terminalReferenceDistanceMeters = 300.0;
            terminal.agentRadiusMeters = 17.0;
            terminal.vehicleLengthMeters = 30.0;
            terminal.maxSpeedMps = 500.0;
            terminal.initialSpeedMps = 80.0;
            terminal.terminalSpeedMps = 0.0;
            terminal.acceleratingMps2 = 25.0;
            terminal.brakingMps2 = 15.0;
            terminal.lateralMps2 = 24.5;
            terminal.maxAngularVelocityRadPerSecond = 1.0;
            terminal.maxAngularAccelerationRadPerSecond2 = 1.0;
            terminal.gateSpacingMeters = 500.0;
            terminal.terminalGateSpacingMeters = 250.0;
            terminal.hasInitialForward = true;
            terminal.initialForward = {0.0, 0.0, 1.0};
            terminal.roundTurns = true;
            terminal.useContinuousAssistedOptimizer = true;
            terminal.assistedCompactStartLead = true;
            terminal.assistedUseExactProtectedStart = true;
            terminal.initialForwardLeadMeters = 8000.0;
            terminal.initialProtectedStraightMeters = 150.0;
            terminal.terminalApproachLengthMeters = 900.0;

            const auto optimized = RoutePlanner::plan(terminal);
            require(optimized.valid(),
                    "BLUE exit continuous free-leg route was not feasible");
            require(
                optimized.diagnosticSummary.find("preset=STANDARD") !=
                    std::string::npos,
                "default Assisted route was not Standard"
            );

            RoutePlanRequest extremeRequest = terminal;
            extremeRequest.assistedRoutePreset = AssistedRoutePreset::Extreme;
            const auto extremePlan = RoutePlanner::plan(extremeRequest);
            require(
                extremePlan.valid(),
                "Extreme Assisted planner failed same physical free-leg case"
            );
            require(
                extremePlan.diagnosticSummary.find("preset=EXTREME") !=
                    std::string::npos &&
                extremePlan.diagnosticSummary.find(
                    "tunnel_radius_floor_m=0.000000"
                ) != std::string::npos,
                "Extreme Assisted did not remove the visual minimum radius"
            );
            requireNear(
                glm::length(extremePlan.routeCurves.back().endMeters -
                            terminal.goalMeters),
                0.0,
                1.0e-3,
                "Extreme Assisted did not preserve the physical docking goal"
            );
            require(!optimized.routeCurves.empty() &&
                    optimized.routeCurves.front().kind ==
                        RouteCurveKind::Line,
                    "BLUE exit lost its protected initial tangent");
            requireNear(
                optimized.routeCurves.front().endProgressMeters,
                150.0,
                1.0e-3,
                "manoeuvre lead incorrectly became an 8km hard straight"
            );
            require(
                optimized.diagnosticSummary.find("hard_start_m=150.") !=
                    std::string::npos &&
                optimized.diagnosticSummary.find("hard_goal_m=900.") !=
                    std::string::npos,
                "continuous optimizer did not expose hard geometry inputs"
            );
            requireNear(
                glm::length(
                    optimized.routeCurves.back().endMeters -
                    terminal.goalMeters
                ),
                0.0, 1.0e-3,
                "optimized terminal free leg moved the dock HOLD endpoint"
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
