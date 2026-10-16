#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::planner::newtonian
{

// Phase vocabulary belongs to the Newtonian planner only.
// Assisted must never consume or produce these phases.
enum class NewtonianManeuverPhaseKind : std::uint8_t
{
    Coast = 0,
    RotateForThrust,
    MainBurn,
    Reorient,
    TerminalHold
};

struct NewtonianPlannerState
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 forward {0.0, 0.0, -1.0};
    glm::dvec3 angularVelocityRadPerSec {0.0};
};

struct NewtonianPlannerLimits
{
    // Newtonian-I V1: one useful aft main axis. Positive main acceleration is
    // along hull forward. There is deliberately no lateral-main authority.
    double mainAccelerationMps2 = 0.0;

    double maxAngularVelocityRadPerSec = 0.0;
    double angularAccelerationRadPerSec2 = 0.0;
};

struct NewtonianPlannerTarget
{
    glm::dvec3 desiredVelocityMps {0.0};

    bool finalForwardRequired = false;
    glm::dvec3 desiredForward {0.0, 0.0, -1.0};

    // Position is optional in milestone 1. The empty-space solver first proves
    // velocity/attitude reachability. Position becomes authoritative when the
    // spatial route layer is connected.
    bool positionReferenceValid = false;
    glm::dvec3 desiredPositionMeters {0.0};
};

struct NewtonianObjectiveWeights
{
    double elapsedTime = 1.0;
    double terminalVelocitySquared = 80.0;
    double terminalPositionSquared = 0.002;
    double terminalForwardSquared = 20.0;
    double mainImpulse = 0.015;
    double angularTravel = 0.10;
};

struct NewtonianManeuverPhase
{
    NewtonianManeuverPhaseKind kind =
        NewtonianManeuverPhaseKind::Coast;

    double durationSeconds = 0.0;

    // Meaningful for RotateForThrust/MainBurn/Reorient.
    glm::dvec3 targetForward {0.0, 0.0, -1.0};

    // MainBurn only. V1 deliberately searches ordinary throttle as part of the
    // functional instead of hiding a bang-bang rule in the planner.
    double throttle01 = 0.0;
};

struct NewtonianManeuverPlan
{
    bool valid = false;
    double objective = std::numeric_limits<double>::infinity();

    NewtonianPlannerState predictedTerminalState {};
    std::vector<NewtonianManeuverPhase> phases;

    double elapsedSeconds = 0.0;
    double mainImpulseMps = 0.0;
    double angularTravelRadians = 0.0;
};

class NewtonianManeuverOptimizer final
{
public:
    // Pure CPU functional minimizer.
    //
    // Milestone-1 search space:
    //   optional ballistic pre-coast
    //   rotate hull toward candidate thrust direction
    //   one aft-main burn
    //   optional final reorientation
    //   terminal hold marker
    //
    // This is intentionally NOT an autopilot. It predicts an authored physical
    // programme. Execution comes later through a separate Newtonian controller.
    [[nodiscard]] static NewtonianManeuverPlan optimize(
        const NewtonianPlannerState& initial,
        const NewtonianPlannerTarget& target,
        const NewtonianPlannerLimits& limits,
        const NewtonianObjectiveWeights& weights = {}
    ) noexcept
    {
        NewtonianManeuverPlan best;

        if (!finite(initial.positionMeters) ||
            !finite(initial.velocityMps) ||
            !finite(initial.forward) ||
            !finite(initial.angularVelocityRadPerSec) ||
            !finite(target.desiredVelocityMps) ||
            !finite(target.desiredForward) ||
            !(limits.mainAccelerationMps2 > 1.0e-9) ||
            !(limits.maxAngularVelocityRadPerSec > 1.0e-9) ||
            !(limits.angularAccelerationRadPerSec2 > 1.0e-9))
        {
            return best;
        }

        const glm::dvec3 initialForward =
            normalizedOr(initial.forward, {0.0, 0.0, -1.0});
        const glm::dvec3 deltaV =
            target.desiredVelocityMps - initial.velocityMps;
        const double deltaSpeed = glm::length(deltaV);

        if (deltaSpeed <= 1.0e-6)
        {
            Candidate candidate;
            candidate.preCoastSeconds = 0.0;
            candidate.thrustDirection = initialForward;
            candidate.burnSeconds = 0.0;
            candidate.throttle01 = 0.0;
            return evaluate(
                initial,
                target,
                limits,
                weights,
                candidate
            );
        }

        const glm::dvec3 idealThrustDirection =
            deltaV / deltaSpeed;
        const double idealBurnSeconds =
            deltaSpeed / limits.mainAccelerationMps2;

        // The exact delta-v direction is included, but not privileged by the
        // scorer. Neighbouring directions/durations let later position and
        // obstacle terms move the optimum without replacing this solver.
        const auto thrustDirections =
            candidateDirections(
                idealThrustDirection,
                initialForward,
                target
            );

        constexpr std::array<double, 7> BurnFactors {
            0.70, 0.85, 0.95, 1.00, 1.05, 1.15, 1.30
        };
        constexpr std::array<double, 4> Throttles {
            0.55, 0.75, 0.90, 1.00
        };
        constexpr std::array<double, 6> CoastSeconds {
            0.0, 0.25, 0.50, 1.0, 2.0, 4.0
        };

        for (const auto& direction : thrustDirections)
        {
            for (const double throttle : Throttles)
            {
                const double throttleAccel =
                    limits.mainAccelerationMps2 * throttle;
                if (!(throttleAccel > 1.0e-9))
                    continue;

                const double exactAtThrottle =
                    deltaSpeed / throttleAccel;

                for (const double factor : BurnFactors)
                {
                    for (const double coast : CoastSeconds)
                    {
                        Candidate candidate;
                        candidate.preCoastSeconds = coast;
                        candidate.thrustDirection = direction;
                        candidate.burnSeconds =
                            exactAtThrottle * factor;
                        candidate.throttle01 = throttle;

                        const auto planned =
                            evaluate(
                                initial,
                                target,
                                limits,
                                weights,
                                candidate
                            );

                        if (planned.valid &&
                            planned.objective < best.objective)
                        {
                            best = planned;
                        }
                    }
                }
            }
        }

        // Guarantee the physically exact velocity impulse candidate is judged
        // even if the coarse functional grid changes later.
        Candidate exact;
        exact.preCoastSeconds = 0.0;
        exact.thrustDirection = idealThrustDirection;
        exact.burnSeconds = idealBurnSeconds;
        exact.throttle01 = 1.0;

        const auto exactPlan =
            evaluate(initial, target, limits, weights, exact);
        if (exactPlan.valid && exactPlan.objective < best.objective)
            best = exactPlan;

        return best;
    }

private:
    struct Candidate
    {
        double preCoastSeconds = 0.0;
        glm::dvec3 thrustDirection {0.0, 0.0, -1.0};
        double burnSeconds = 0.0;
        double throttle01 = 0.0;
    };

    [[nodiscard]] static bool finite(double value) noexcept
    {
        return std::isfinite(value);
    }

    [[nodiscard]] static bool finite(const glm::dvec3& value) noexcept
    {
        return
            finite(value.x) &&
            finite(value.y) &&
            finite(value.z);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        if (!std::isfinite(length) || length <= 1.0e-12)
            return fallback;
        return value / length;
    }

    [[nodiscard]] static double angleBetween(
        const glm::dvec3& a,
        const glm::dvec3& b
    ) noexcept
    {
        return std::acos(
            std::clamp(
                glm::dot(
                    normalizedOr(a, {0.0, 0.0, -1.0}),
                    normalizedOr(b, {0.0, 0.0, -1.0})
                ),
                -1.0,
                1.0
            )
        );
    }

    // Time-optimal symmetric slew from rest under angular acceleration and
    // angular-speed limits. The docking-entry contract supplies an effectively
    // settled initial angular state; a residual rate is paid as damping time.
    [[nodiscard]] static double rotationTimeSeconds(
        double angleRadians,
        double initialAngularSpeedRadPerSec,
        const NewtonianPlannerLimits& limits
    ) noexcept
    {
        const double angle = std::max(0.0, angleRadians);
        const double alpha = limits.angularAccelerationRadPerSec2;
        const double omegaMax = limits.maxAngularVelocityRadPerSec;

        const double damping =
            std::max(0.0, initialAngularSpeedRadPerSec) / alpha;

        if (angle <= 1.0e-12)
            return damping;

        const double accelAngle =
            omegaMax * omegaMax / alpha;

        if (angle <= accelAngle)
        {
            // accelerate half the angle, decelerate half.
            return damping +
                2.0 * std::sqrt(angle / alpha);
        }

        const double accelAndBrakeTime =
            2.0 * omegaMax / alpha;
        const double cruiseAngle = angle - accelAngle;
        return damping +
            accelAndBrakeTime +
            cruiseAngle / omegaMax;
    }

    [[nodiscard]] static std::array<glm::dvec3, 5>
    candidateDirections(
        const glm::dvec3& ideal,
        const glm::dvec3& initialForward,
        const NewtonianPlannerTarget& target
    ) noexcept
    {
        const glm::dvec3 finalForward =
            target.finalForwardRequired
                ? normalizedOr(target.desiredForward, ideal)
                : ideal;

        return {
            normalizedOr(ideal, initialForward),
            normalizedOr(ideal * 0.90 + initialForward * 0.10, ideal),
            normalizedOr(ideal * 0.75 + initialForward * 0.25, ideal),
            normalizedOr(ideal * 0.90 + finalForward * 0.10, ideal),
            normalizedOr(ideal * 0.75 + finalForward * 0.25, ideal)
        };
    }

    [[nodiscard]] static NewtonianManeuverPlan evaluate(
        const NewtonianPlannerState& initial,
        const NewtonianPlannerTarget& target,
        const NewtonianPlannerLimits& limits,
        const NewtonianObjectiveWeights& weights,
        const Candidate& candidate
    ) noexcept
    {
        NewtonianManeuverPlan out;

        if (!finite(candidate.preCoastSeconds) ||
            !finite(candidate.burnSeconds) ||
            !finite(candidate.throttle01) ||
            candidate.preCoastSeconds < 0.0 ||
            candidate.burnSeconds < 0.0 ||
            candidate.throttle01 < 0.0 ||
            candidate.throttle01 > 1.0)
        {
            return out;
        }

        NewtonianPlannerState state = initial;
        state.forward =
            normalizedOr(state.forward, {0.0, 0.0, -1.0});

        const double initialAngularSpeed =
            glm::length(state.angularVelocityRadPerSec);

        if (candidate.preCoastSeconds > 1.0e-9)
        {
            appendPhase(
                out,
                NewtonianManeuverPhaseKind::Coast,
                candidate.preCoastSeconds,
                state.forward,
                0.0
            );
            coast(state, candidate.preCoastSeconds);
        }

        const glm::dvec3 thrustDirection =
            normalizedOr(candidate.thrustDirection, state.forward);
        const double thrustTurn =
            angleBetween(state.forward, thrustDirection);
        const double thrustTurnSeconds =
            rotationTimeSeconds(
                thrustTurn,
                initialAngularSpeed,
                limits
            );

        if (thrustTurnSeconds > 1.0e-9)
        {
            appendPhase(
                out,
                NewtonianManeuverPhaseKind::RotateForThrust,
                thrustTurnSeconds,
                thrustDirection,
                0.0
            );
            coast(state, thrustTurnSeconds);
            state.forward = thrustDirection;
            state.angularVelocityRadPerSec = glm::dvec3(0.0);
            out.angularTravelRadians += thrustTurn;
        }

        if (candidate.burnSeconds > 1.0e-9 &&
            candidate.throttle01 > 1.0e-9)
        {
            appendPhase(
                out,
                NewtonianManeuverPhaseKind::MainBurn,
                candidate.burnSeconds,
                thrustDirection,
                candidate.throttle01
            );

            const glm::dvec3 acceleration =
                thrustDirection *
                (limits.mainAccelerationMps2 *
                 candidate.throttle01);

            state.positionMeters +=
                state.velocityMps * candidate.burnSeconds +
                0.5 * acceleration *
                    candidate.burnSeconds *
                    candidate.burnSeconds;
            state.velocityMps +=
                acceleration * candidate.burnSeconds;

            out.mainImpulseMps +=
                limits.mainAccelerationMps2 *
                candidate.throttle01 *
                candidate.burnSeconds;
        }

        if (target.finalForwardRequired)
        {
            const glm::dvec3 finalForward =
                normalizedOr(target.desiredForward, state.forward);
            const double finalTurn =
                angleBetween(state.forward, finalForward);
            const double finalTurnSeconds =
                rotationTimeSeconds(
                    finalTurn,
                    0.0,
                    limits
                );

            if (finalTurnSeconds > 1.0e-9)
            {
                appendPhase(
                    out,
                    NewtonianManeuverPhaseKind::Reorient,
                    finalTurnSeconds,
                    finalForward,
                    0.0
                );
                coast(state, finalTurnSeconds);
                state.forward = finalForward;
                out.angularTravelRadians += finalTurn;
            }
        }

        appendPhase(
            out,
            NewtonianManeuverPhaseKind::TerminalHold,
            0.0,
            state.forward,
            0.0
        );

        state.angularVelocityRadPerSec = glm::dvec3(0.0);

        const glm::dvec3 velocityError =
            state.velocityMps - target.desiredVelocityMps;
        double objective =
            weights.elapsedTime * out.elapsedSeconds +
            weights.terminalVelocitySquared *
                glm::dot(velocityError, velocityError) +
            weights.mainImpulse * out.mainImpulseMps +
            weights.angularTravel * out.angularTravelRadians;

        if (target.positionReferenceValid)
        {
            const glm::dvec3 positionError =
                state.positionMeters -
                target.desiredPositionMeters;
            objective +=
                weights.terminalPositionSquared *
                glm::dot(positionError, positionError);
        }

        if (target.finalForwardRequired)
        {
            const double forwardAngle =
                angleBetween(state.forward, target.desiredForward);
            objective +=
                weights.terminalForwardSquared *
                forwardAngle * forwardAngle;
        }

        if (!std::isfinite(objective))
            return out;

        out.valid = true;
        out.objective = objective;
        out.predictedTerminalState = state;
        return out;
    }

    static void appendPhase(
        NewtonianManeuverPlan& plan,
        NewtonianManeuverPhaseKind kind,
        double durationSeconds,
        const glm::dvec3& targetForward,
        double throttle01
    ) noexcept
    {
        NewtonianManeuverPhase phase;
        phase.kind = kind;
        phase.durationSeconds = std::max(0.0, durationSeconds);
        phase.targetForward = targetForward;
        phase.throttle01 = std::clamp(throttle01, 0.0, 1.0);
        plan.phases.push_back(phase);
        plan.elapsedSeconds += phase.durationSeconds;
    }

    static void coast(
        NewtonianPlannerState& state,
        double seconds
    ) noexcept
    {
        if (!(seconds > 0.0))
            return;
        state.positionMeters += state.velocityMps * seconds;
    }
};

} // namespace game::navigation::planner::newtonian
