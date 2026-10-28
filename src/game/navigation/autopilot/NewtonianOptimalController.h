#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/AutopilotControlApi.h"
#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/navigation/NavigationControlCapability.h"
#include "src/game/navigation/autopilot/NewtonianFlightPolicy.h"
#include "src/game/navigation/autopilot/NewtonianManoeuvreProgram.h"
#include "src/game/navigation/autopilot/NewtonianTurnModel.h"
#include "src/game/navigation/autopilot/NewtonianExecutionSafetyMonitor.h"
#include "src/game/navigation/autopilot/NewtonianSpeedTolerancePolicy.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/world/navigation/TrajectoryGenerator.h"

namespace game::navigation::autopilot
{

// Newtonian pilot: executes a manoeuvre program.
//
// Navigation owns the route geometry and its speed profile. From them and
// the craft's real capability this pilot compiles a manoeuvre program
// (NewtonianManoeuvreProgram.h): where the main engine burns and along which
// vector (braking and curving combined), where the craft coasts on RCS and
// where the hull turns calmly so the thrust axis is ready before a burn.
//
// Execution follows the CURRENT phase. Per tick it only corrects:
//   - the required acceleration is recomputed from the actual state (speed
//     error against the profile, curve acceleration at the actual speed);
//   - the safe tube adds a lateral correction when the PREDICTED
//     cross-track leaves it (modifies the phase vector, never replaces it);
//   - the burn gate is a safety: main only through an achievable thrust
//     vector (hull + nozzle cone), limited side push near walls.
// A phase ends by progress (its physical completion along the route); a
// finished phase is never revived. Only safety (a wall exit RCS cannot stop
// and the current hull cannot burn against) may take the hull away from the
// plan.
//
// Actuator order: RCS -> nozzle while burning -> hull rotation only as
// planned, or for safety.
class NewtonianOptimalController final
{
public:
    // Executed coarse state (trace/telemetry only).
    enum class Phase : int
    {
        Coast = 0,
        Flip = 1,
        Align = 2,
        Burn = 3,
        Hold = 4,
    };

    // Route hook: the route autopilot hands over its route stations whenever
    // the accepted route or its speed profile changes (the profile is
    // re-anchored on the actual speed at every route checkpoint).
    void setManoeuvreRoute(
        const std::vector<ManoeuvreRouteSample>& samples,
        const ShipParams& params,
        const glm::dvec3& hullForwardMap,
        double currentProgressMeters = 0.0
    )
    {
        // The route is finished once the terminal sequence runs: a profile
        // refresh at the last checkpoint is no new manoeuvre.
        if (state_.terminal.active)
            return;
        program_ =
            compileNewtonianManoeuvreProgram(
                samples,
                capabilityOf(params),
                hullForwardMap,
                currentProgressMeters,
                &speedCaps_
            );
        state_.phaseIndex = 0;
        ++state_.programRevision;
        dumpProgram(samples);
    }

    [[nodiscard]] const NewtonianManoeuvreProgram& program() const noexcept
    {
        return program_;
    }

    [[nodiscard]] AutopilotControlCommand update(
        const AutopilotControlRequest& request,
        const ShipParams& params
    ) noexcept
    {
        const double dt =
            std::max(
                1.0e-4,
                std::isfinite(request.deltaSeconds)
                    ? request.deltaSeconds
                    : 0.0
            );

        const glm::dvec3 forward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        const glm::dvec3 right =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        const glm::dvec3 up =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});

        const double actualSpeed =
            finiteLength(request.actualVelocityMapMps);

        constexpr double PrecisionStopEntrySpeedMps =
            Policy::PrecisionStopEntrySpeedMps;
        const bool precisionStop =
            request.stopRequested &&
            actualSpeed <= PrecisionStopEntrySpeedMps;

        // At true rest with no translational objective, orchestration may
        // still require a launch/safety attitude before route handoff.
        const bool restAttitude =
            actualSpeed <= 0.5 &&
            finiteLength(request.desiredVelocityMapMps) <= 0.5 &&
            finiteLength(request.desiredLinearAccelerationMapMps2) <=
                1.0e-6 &&
            !request.programProgressValid &&
            !request.stopRequested;

        AutopilotControlCommand out;
        decision_ = {};
        decision_.thrustDirection = forward;
        decision_.hullTarget = forward;
        decision_.programPhase = -1;
        decision_.phaseIndex = -1;

        // The route's terminal stop: once the craft is slow near the end,
        // the terminal sequence owns the ship until a new route. A stop
        // request that flickers at the very end of the route (progress
        // projection jitter of centimetres) does not end it; only the loss
        // of the terminal target (a new route / cancellation) does.
        const bool terminalRequested =
            request.terminalTargetValid &&
            (request.stopRequested || state_.terminal.active);
        if (!terminalRequested)
            state_.terminal = {};
        if (terminalRequested &&
            (state_.terminal.active || actualSpeed <= TerminalEntrySpeedMps))
        {
            out = terminalControl(request, params, forward, right, up, dt);
            state_.phase = Phase::Hold;
        }
        else if (precisionStop)
        {
            out = precisionStopControl(
                request,
                params,
                forward,
                right,
                up,
                dt
            );
            state_.phase = Phase::Hold;
        }
        else if (request.terminalAttitudeHold || restAttitude)
        {
            decision_.hullTarget =
                normalizedOr(request.desiredForwardMap, forward);
            applyAttitude(
                out,
                request,
                params,
                decision_.hullTarget,
                request.terminalAttitudeHold
                    ? normalizedOr(request.desiredUpMap, up)
                    : up,
                dt
            );
            state_.phase = Phase::Hold;
        }
        else if (program_.valid && request.programProgressValid)
        {
            out = programControl(request, params, forward, right, up, dt);
        }
        else
        {
            out = directControl(request, params, forward, right, up, dt);
        }

        // Every RCS translation this pilot commands is deliberate trajectory
        // delta-v; the manual keypad release-cancellation must not undo it.
        out.rcsKeepsTranslationDeltaV = true;
        out.navigationAccelerationDemandValid = false;
        out.navigationVelocityTargetValid = false;
        out.navigationPrecisionTranslationOnly = false;

        traceTick(request, out, dt);
        remember(request);
        return out;
    }

    [[nodiscard]] AutopilotControlTelemetry telemetry(
        const ShipParams& params
    ) const noexcept
    {
        AutopilotControlTelemetry out;
        out.initialized = state_.initialized;
        out.previousVelocityMapMps = state_.previousVelocityMapMps;

        const double angularAuthority =
            factsOf(params).angularScalarRadPerSec2;
        const double angularRate =
            factsOf(params).maxAngularSpeedRadPerSec;
        out.courseResponseSeconds =
            angularAuthority > 1.0e-9 && angularRate > 1.0e-9
                ? angularRate / angularAuthority
                : 2.0;

        const double main =
            factsOf(params).mainForwardMps2;
        out.targetSpeedChangeRateMps2 = main;
        out.brakingResponseMps2 = main;
        out.commandResponsePerSecond =
            std::max(
                1.0e-6,
                static_cast<double>(params.throttleAccel) > 0.0
                    ? static_cast<double>(params.throttleAccel)
                    : static_cast<double>(
                        params.fallbackThrottleResponsePerSecond
                      )
            );
        out.thrustReversalLeadSeconds = plannedFlipLeadSeconds(params);
        return out;
    }

    void reset() noexcept
    {
        state_ = {};
        program_ = {};
    }

    // Coast time needed ahead of a braking burn opposite the current thrust
    // axis: a calm 180 degree flip, the align/settle time and a margin.
    [[nodiscard]] static double plannedFlipLeadSeconds(
        const ShipParams& params
    ) noexcept
    {
        return newtonianFlipLeadSeconds(factsOf(params));
    }

private:
    // Doctrine values live in NewtonianFlightPolicy (one place for the
    // compiler and this pilot); these are the pilot's names for them.
    using Policy = NewtonianFlightPolicy;
    static constexpr double DegToRadValue = Policy::DegToRad;
    static constexpr double RcsTrajectoryShare = Policy::RcsTrajectoryShare;
    static constexpr double LongitudinalResponseSeconds = Policy::LongitudinalResponseSeconds;
    static constexpr double LateralPredictionSeconds = Policy::LateralPredictionSeconds;
    static constexpr double LateralResponseSeconds = Policy::LateralResponseSeconds;
    static constexpr double SafeTubeFraction = Policy::SafeTubeFraction;
    static constexpr double TubeReleaseFraction = Policy::TubeReleaseFraction;
    static constexpr double TubeAimFraction = Policy::TubeAimFraction;
    static constexpr double WallMarginFraction = Policy::WallMarginFraction;
    static constexpr double DefaultCorridorToleranceMeters = Policy::DefaultCorridorToleranceMeters;
    static constexpr double SettleSpeedMps = Policy::PilotSettleSpeedMps;
    static constexpr double NarrowAttitudeLeadSeconds = Policy::NarrowAttitudeLeadSeconds;
    static constexpr double NarrowAttitudeToleranceRad = Policy::NarrowAttitudeToleranceRad;
    static constexpr double NarrowApproachSpeedShare = Policy::NarrowApproachSpeedShare;
    static constexpr double RcsDribbleMps2 = Policy::RcsDribbleMps2;
    static constexpr double TerminalEntrySpeedMps = Policy::TerminalEntrySpeedMps;
    static constexpr double TerminalApproachSpeedMps = Policy::TerminalApproachSpeedMps;
    static constexpr double TerminalApproachResponseSeconds = Policy::TerminalApproachResponseSeconds;
    static constexpr double TerminalVelocityResponseSeconds = Policy::TerminalVelocityResponseSeconds;
    static constexpr double TerminalDefaultPositionMeters = Policy::TerminalDefaultPositionMeters;
    static constexpr double TerminalDefaultSpeedMps = Policy::TerminalDefaultSpeedMps;
    static constexpr double TerminalDefaultAttitudeRad = Policy::TerminalDefaultAttitudeRad;
    static constexpr double TerminalDefaultRateRadPerSec = Policy::TerminalDefaultRateRadPerSec;
    static constexpr double TerminalHysteresis = Policy::TerminalHysteresis;
    static constexpr double TerminalDeadbandShare = Policy::TerminalDeadbandShare;
    static constexpr double TerminalHoldSeconds = Policy::TerminalHoldSeconds;
    static constexpr double TerminalSettleEntryShare = Policy::TerminalSettleEntryShare;
    static constexpr double TerminalDoneDwellSeconds = Policy::TerminalDoneDwellSeconds;
    static constexpr double BrakeFadeMps = Policy::BrakeFadeMps;
    static constexpr double SettleResponseSeconds = Policy::SettleResponseSeconds;
    static constexpr double MainShareOfRcs = Policy::MainShareOfRcs;
    static constexpr double OffPhaseMainShareOfRcs = Policy::OffPhaseMainShareOfRcs;
    static constexpr double HullLeadSeconds = Policy::HullLeadSeconds;
    static constexpr double PreferredRateShare = Policy::PreferredRateShare;
    static constexpr double PreferredAccelerationShare = Policy::PreferredAccelerationShare;
    static constexpr double PlannedRateMargin = Policy::PlannedRateMargin;
    static constexpr double AlignSettleSeconds = Policy::AlignSettleSeconds;
    static constexpr double FlipLeadMarginSeconds = Policy::FlipLeadMarginSeconds;
    static constexpr double DegToRad = Policy::DegToRad;
    static constexpr double FlipEntryRad = Policy::FlipEntryRad;
    static constexpr double FlipExitRad = Policy::FlipExitRad;
    static constexpr double BurnGateFullRad = Policy::BurnGateFullRad;
    static constexpr double BurnGateLimitedRad = Policy::BurnGateLimitedRad;
    static constexpr double BurnRateGateRadPerSec = Policy::BurnRateGateRadPerSec;

    // Terminal sequence: BRAKE (program) -> POSITION_HOLD -> ATTITUDE_ALIGN
    // -> RATE_SETTLE -> DONE. Every step ends inside a finite tolerance;
    // DONE is left only beyond twice that tolerance (hysteresis).
    // DONE means "the navigation terminal state is reached" (a staging,
    // waiting or rendezvous point), never "docked": a future docking or
    // formation controller takes over from DONE.
    enum class TerminalStep : int
    {
        PositionHold = 1,
        AttitudeAlign = 2,
        RateSettle = 3,
        Done = 4,
    };

    struct TerminalState
    {
        bool active = false;
        TerminalStep step = TerminalStep::PositionHold;
        glm::dvec3 holdForwardMap {0.0, 0.0, -1.0};
        glm::dvec3 holdUpMap {0.0, 1.0, 0.0};
        // How long the craft has been continuously still, slow-turning and
        // aligned (the commands already sent are still being executed).
        double settledSeconds = 0.0;
    };

    struct State
    {
        bool initialized = false;
        TerminalState terminal {};
        // Large turns use the calm rate profile; the switch back to the
        // pose servo has hysteresis (enter > FlipEntryRad, leave < FlipExitRad).
        bool calmTurn = false;
        glm::dvec3 previousVelocityMapMps {0.0};
        Phase phase = Phase::Coast;
        bool lateralCorrection = false;
        // Execution safety monitor memory (narrow-zone attitude error).
        bool monitorAttitudeValid = false;
        double monitorAttitudeErrorRad = 0.0;
        std::size_t phaseIndex = 0;
        std::uint64_t programRevision = 0;
        double traceSeconds = 0.0;
    };

    // DEV decision-trace record of the last tick; never read by control.
    struct Decision
    {
        int programPhase = -1;
        int phaseIndex = -1;
        double phaseStartSeconds = 0.0;
        double plannedSpeed = 0.0;
        double radiusMeters = 0.0;
        glm::dvec3 crossTrack {0.0};
        glm::dvec3 lateralVelocity {0.0};
        double speedError = 0.0;
        double feedForwardAlong = 0.0;
        double feedForwardLateral = 0.0;
        glm::dvec3 correction {0.0};
        glm::dvec3 demand {0.0};
        glm::dvec3 thrustDirection {0.0, 0.0, -1.0};
        glm::dvec3 hullTarget {0.0, 0.0, -1.0};
        double hullErrorRad = 0.0;
        double thrustErrorRad = 0.0;
        double planHullErrorRad = 0.0;
        double gimbalLimitRad = 0.0;
        int gate = 0;       // 0 n/a, 1 full, 2 limited, 3 closed
        int gateReason = 0; // 1 thrust error, 2 hull rate, 3 wall side
        bool mainPhase = false;
        bool mainNeeded = false;
        bool braking = false;
        bool lateralCorrection = false;
        bool emergency = false;
        bool safetyOverride = false;
        // Execution safety monitor: 0 safe, 1 warn, 2 unrecoverable,
        // +4 rotation stopped in a narrow zone.
        int monitor = 0;
        bool narrow = false;
        int terminalStep = 0;
        double terminalDistance = 0.0;
        double terminalAttitudeErrorRad = 0.0;
        double terminalRollErrorRad = 0.0;
    };

    struct GimbalDemandResult
    {
        float rightInput = 0.0f;
        float upInput = 0.0f;
        glm::dvec3 commandedDirectionMap {0.0, 0.0, -1.0};
    };

    // The pilot's one reader of the ship's capability facts.
    [[nodiscard]] static NavigationControlCapability factsOf(
        const ShipParams& params
    ) noexcept
    {
        return makeNavigationControlCapability(
            params, LocalFlightControlLaw::Newtonian);
    }

    [[nodiscard]] static ManoeuvreCapability capabilityOf(
        const ShipParams& params
    ) noexcept
    {
        ManoeuvreCapability out;
        out.mainAccelerationMps2 =
            std::max(0.0, factsOf(params).mainForwardMps2);
        out.rcsTrajectoryAccelerationMps2 =
            RcsTrajectoryShare *
            std::max(0.0, factsOf(params).rcsScalarMps2);
        out.gimbalLimitRad =
            std::max(
                factsOf(params).gimbalPitchRad,
                factsOf(params).gimbalYawRad
            );
        out.plannedTurnRateRadPerSec =
            PlannedRateMargin * preferredTurnRateRadPerSec(params);
        // Same planning share of the main authority as the route profile.
        const double fraction =
            world::navigation::TrajectoryGenerationPolicy::
                DefaultPlanningAuthorityFraction;
        out.plannedForwardAccelerationMps2 =
            fraction * out.mainAccelerationMps2;
        out.plannedBrakingAccelerationMps2 =
            fraction * out.mainAccelerationMps2;
        return out;
    }

    // Without a manoeuvre program (no accepted route): track the desired
    // velocity on RCS; a demand beyond RCS turns the hull onto it and burns
    // through the gate.
    [[nodiscard]] AutopilotControlCommand directControl(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt
    ) noexcept
    {
        AutopilotControlCommand out;
        const double mainAuthority =
            std::max(0.0, factsOf(params).mainForwardMps2);
        const double rcsAuthority =
            std::max(0.0, factsOf(params).rcsScalarMps2);
        const double rcsBudget = RcsTrajectoryShare * rcsAuthority;
        const glm::dvec3 demand =
            finiteVecOrZero(request.desiredLinearAccelerationMapMps2) +
            (finiteVecOrZero(request.desiredVelocityMapMps) -
             finiteVecOrZero(request.actualVelocityMapMps)) /
                LongitudinalResponseSeconds;
        const double magnitude = finiteLength(demand);
        glm::dvec3 hullTarget = forward;
        double throttle = 0.0;
        glm::dvec3 delivered {0.0};
        if (mainAuthority > 1.0e-9 && magnitude > rcsBudget)
        {
            const glm::dvec3 direction = demand / magnitude;
            hullTarget = direction;
            const auto gimbal =
                solveGimbalDemand(direction, forward, right, up, params);
            const double gimbalLimit =
                std::max(
                    factsOf(params).gimbalPitchRad,
                    factsOf(params).gimbalYawRad
                );
            const double thrustError =
                std::max(0.0, angleBetween(forward, direction) - gimbalLimit);
            const double hullRate =
                std::hypot(
                    finiteOrZero(request.pitchRateRadPerSec),
                    finiteOrZero(request.yawRateRadPerSec)
                );
            if (thrustError <= BurnGateFullRad &&
                hullRate <= BurnRateGateRadPerSec)
            {
                throttle = std::clamp(magnitude / mainAuthority, 0.0, 1.0);
                out.mainGimbalRightInput = gimbal.rightInput;
                out.mainGimbalUpInput = gimbal.upInput;
                delivered =
                    gimbal.commandedDirectionMap * (mainAuthority * throttle);
            }
        }
        if (calmTurn(angleBetween(forward, hullTarget)))
            applyFlipRate(out, request, params, hullTarget, up, dt);
        else
            applyAttitude(out, request, params, hullTarget, up, dt);
        out.targetSpeedRate = finiteClamp(throttle);
        (void)rcsAuthority;
        (void)rcsBudget;
        applyRcsTranslation(
            out, demand - delivered, forward, right, up, params,
            RcsTrajectoryShare);
        state_.phase = throttle > 0.0 ? Phase::Burn : Phase::Coast;
        decision_.demand = demand;
        decision_.hullTarget = hullTarget;
        decision_.thrustDirection = normalizedOr(demand, forward);
        return out;
    }

    // Translational RCS through the same physical channel as the keypad:
    // the demand (map frame) is split onto the body axes, each direction is
    // bounded by `share` of its OWN signed authority from the authoritative
    // control telemetry (a lost direction is simply 0), the vector is scaled
    // uniformly to stay inside that box, and the inputs are normalized per
    // direction (rcsTranslationPerAxisAuthority). Without per-axis data the
    // scalar bound is used, as before. Returns the acceleration commanded.
    static glm::dvec3 applyRcsTranslation(
        AutopilotControlCommand& out,
        const glm::dvec3& demandMap,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        const ShipParams& params,
        double share
    ) noexcept
    {
        const bool perAxis = factsOf(params).rcsPerAxisKnown;
        const glm::dvec3 axes[3] = {forward, right, up};
        // ShipDynamics axis ids: 0 right, 1 up, 2 forward.
        const int ids[3] = {2, 0, 1};
        double component[3];
        double limit[3];
        double scale = 1.0;
        for (int a = 0; a < 3; ++a)
        {
            component[a] = finiteOrZero(glm::dot(demandMap, axes[a]));
            limit[a] = std::max(0.0,
                perAxis
                    ? factsOf(params).rcsDirectionMps2(ids[a], component[a])
                    : factsOf(params).rcsScalarMps2);
            if (std::abs(component[a]) > 1.0e-12 && limit[a] > 1.0e-9)
                scale = std::min(
                    scale, share * limit[a] / std::abs(component[a]));
        }
        glm::dvec3 commanded {0.0};
        float* inputs[3] = {&out.forwardInput, &out.strafeInput,
                            &out.liftInput};
        for (int a = 0; a < 3; ++a)
        {
            // A dribble is dropped: below 1 % of its own direction's
            // authority AND below an absolute floor, so a fine trim on a
            // strong RCS axis still reaches the thrusters.
            double c = limit[a] > 1.0e-9 ? component[a] * scale : 0.0;
            if (std::abs(c) < std::min(0.01 * limit[a], RcsDribbleMps2))
                c = 0.0;
            *inputs[a] = finiteClamp(limit[a] > 1.0e-9 ? c / limit[a] : 0.0);
            commanded += axes[a] * c;
        }
        out.rcsTranslationPerAxisAuthority = perAxis;
        return commanded;
    }

    [[nodiscard]] bool calmTurn(double errorRad) noexcept
    {
        state_.calmTurn =
            state_.calmTurn ? errorRad > FlipExitRad : errorRad > FlipEntryRad;
        return state_.calmTurn;
    }

    // DEV diagnostic only: ELITE_NEWTONIAN_PROGRAM_DUMP=<tsv path> writes the
    // first compiled program station by station (input profile speed,
    // planned speed/acceleration, main, phase, hull vs tangent/axis).
    void dumpProgram(const std::vector<ManoeuvreRouteSample>& samples) const
    {
        // ELITE_NEWTONIAN_PROGRAM_DUMP_REVISION=<n> selects a later compile.
        static bool done = false;
        const char* path = std::getenv("ELITE_NEWTONIAN_PROGRAM_DUMP");
        const char* revision =
            std::getenv("ELITE_NEWTONIAN_PROGRAM_DUMP_REVISION");
        if (revision && *revision &&
            std::strtoull(revision, nullptr, 10) != state_.programRevision)
            return;
        if (done || !path || !*path || !program_.valid ||
            samples.size() != program_.stations.size())
            return;
        done = true;
        std::FILE* f = std::fopen(path, "w");
        if (!f)
            return;
        constexpr double Deg = 57.2957795130823208768;
        std::fputs("s_m\tin_speed\tin_accel\tspeed\taccel\tradius_m\tmain\t"
                   "phase\tkind\thull_t_deg\thull_axis_deg\n", f);
        for (std::size_t i = 0; i < program_.stations.size(); ++i)
        {
            const auto& st = program_.stations[i];
            const auto& ph = program_.phases[st.phaseIndex];
            std::fprintf(f, "%.1f\t%.2f\t%.2f\t%.2f\t%.2f\t%.0f\t%d\t%zu\t%s\t"
                "%.1f\t%.1f\n",
                st.progressMeters, samples[i].speedMps,
                samples[i].longitudinalAccelerationMps2, st.speedMps,
                st.longitudinalAccelerationMps2,
                st.curvaturePerMeter > 1.0e-9 ? 1.0 / st.curvaturePerMeter : 0.0,
                st.mainBurn ? 1 : 0, st.phaseIndex,
                manoeuvrePhaseName(ph.kind),
                angleBetween(st.hullForwardMap, st.tangentMap) * Deg,
                st.mainBurn
                    ? angleBetween(st.hullForwardMap,
                                   st.requiredAccelerationMap) * Deg
                    : -1.0);
        }
        std::fclose(f);
    }

    // ----- Terminal sequence ------------------------------------------------

    [[nodiscard]] static double toleranceOr(double value, double fallback)
    {
        return std::isfinite(value) && value > 1.0e-9 ? value : fallback;
    }

    [[nodiscard]] AutopilotControlCommand terminalControl(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt
    ) noexcept
    {
        AutopilotControlCommand out;
        TerminalState& term = state_.terminal;
        if (!term.active)
        {
            term = {};
            term.active = true;
            term.holdForwardMap = forward;
            term.holdUpMap = up;
        }

        const double tolPosition = toleranceOr(
            request.terminalPositionToleranceMeters,
            TerminalDefaultPositionMeters);
        const double tolSpeed = toleranceOr(
            request.terminalSpeedToleranceMps, TerminalDefaultSpeedMps);
        const double tolAttitude = toleranceOr(
            request.terminalAttitudeToleranceRad, TerminalDefaultAttitudeRad);
        const double tolRoll = toleranceOr(
            request.terminalRollToleranceRad, TerminalDefaultAttitudeRad);
        const double tolRate = toleranceOr(
            request.terminalAngularRateToleranceRadPerSec,
            TerminalDefaultRateRadPerSec);

        const glm::dvec3 targetForward =
            normalizedOr(request.terminalForwardMap, forward);
        const glm::dvec3 targetUp =
            normalizedOr(request.terminalUpMap, up);
        const glm::dvec3 error =
            finiteVecOrZero(request.terminalPositionMapMeters) -
            finiteVecOrZero(request.actualPositionMapMeters);
        const glm::dvec3 velocity =
            finiteVecOrZero(request.actualVelocityMapMps);
        const double distance = finiteLength(error);
        const double speed = finiteLength(velocity);
        const double rate = std::sqrt(
            finiteOrZero(request.pitchRateRadPerSec) *
                finiteOrZero(request.pitchRateRadPerSec) +
            finiteOrZero(request.yawRateRadPerSec) *
                finiteOrZero(request.yawRateRadPerSec) +
            finiteOrZero(request.rollRateRadPerSec) *
                finiteOrZero(request.rollRateRadPerSec));
        const double attitudeError = angleBetween(forward, targetForward);
        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward = forward;
        poseRequest.currentRight = right;
        poseRequest.currentUp = up;
        poseRequest.targetForward = targetForward;
        poseRequest.targetUp = targetUp;
        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        const double rollError =
            pose.valid ? std::abs(pose.rollErrorRad) : 0.0;

        // Step transitions: forward inside 1x the tolerance, back only
        // beyond TerminalHysteresis x the tolerance.
        // "Stopped" means the craft stays inside its position tolerance for
        // TerminalHoldSeconds; the route's speed tolerance only admits the
        // craft into the terminal sequence.
        const double h = TerminalHysteresis;
        const double holdSpeed =
            std::min(tolSpeed, tolPosition / TerminalHoldSeconds);
        const bool placed = distance <= tolPosition && speed <= tolSpeed;
        const bool lostPlace =
            distance > h * tolPosition || speed > h * tolSpeed;
        const bool still =
            distance <= tolPosition && speed <= holdSpeed;
        const bool lostStill =
            distance > h * tolPosition || speed > h * holdSpeed;
        const bool aligned =
            attitudeError <= tolAttitude && rollError <= tolRoll;
        const bool lostAlign =
            attitudeError > h * tolAttitude || rollError > h * tolRoll;
        term.settledSeconds =
            still && aligned && rate <= tolRate ? term.settledSeconds + dt
                                                : 0.0;
        switch (term.step)
        {
            case TerminalStep::PositionHold:
                if (placed)
                    term.step = TerminalStep::AttitudeAlign;
                break;
            case TerminalStep::AttitudeAlign:
                if (lostPlace)
                    term.step = TerminalStep::PositionHold;
                else if (aligned && rate <= TerminalSettleEntryShare * tolRate)
                    term.step = TerminalStep::RateSettle;
                break;
            case TerminalStep::RateSettle:
                // RATE_SETTLE corrects no angle: a turn that came to rest
                // just outside the attitude tolerance (inside the
                // hysteresis band) goes back to the calm alignment instead
                // of waiting forever.
                if (lostPlace)
                    term.step = TerminalStep::PositionHold;
                else if (lostAlign || (!aligned && rate <= tolRate))
                    term.step = TerminalStep::AttitudeAlign;
                else if (term.settledSeconds >= TerminalDoneDwellSeconds)
                    term.step = TerminalStep::Done;
                break;
            case TerminalStep::Done:
                if (lostPlace)
                    term.step = TerminalStep::PositionHold;
                else if (lostStill)
                    term.step = TerminalStep::RateSettle;
                else if (lostAlign)
                    term.step = TerminalStep::AttitudeAlign;
                else if (rate > h * tolRate)
                    term.step = TerminalStep::RateSettle;
                break;
        }
        // While the craft is being placed it keeps the attitude it arrived
        // with; the calm turn to the terminal pose follows.
        if (term.step == TerminalStep::PositionHold)
        {
            term.holdForwardMap = forward;
            term.holdUpMap = up;
        }

        // Attitude: calm preferred rate; nothing once DONE.
        // RATE_SETTLE is already inside the attitude tolerance: it only
        // damps the turn rate and corrects no angle.
        if (term.step != TerminalStep::Done)
        {
            const bool toTarget = term.step != TerminalStep::PositionHold;
            applyCalmPose(
                out, request, params,
                toTarget ? targetForward : term.holdForwardMap,
                toTarget ? targetUp : term.holdUpMap,
                term.step == TerminalStep::RateSettle
                    ? std::numeric_limits<double>::infinity()
                    : TerminalDeadbandShare * tolAttitude);
        }

        // Translation: main inhibited; RCS trims the position calmly inside
        // the tolerance and then stops (no chase of a mathematical zero).
        const double rcsAuthority =
            std::max(0.0, factsOf(params).rcsScalarMps2);
        const double rcsBudget = RcsTrajectoryShare * rcsAuthority;
        const bool trimDeadband =
            distance <= TerminalDeadbandShare * tolPosition &&
            speed <= TerminalDeadbandShare * holdSpeed;
        glm::dvec3 trim {0.0};
        if (term.step != TerminalStep::Done && !trimDeadband &&
            rcsAuthority > 1.0e-12)
        {
            // Glide in at up to the entry speed, never faster than half the
            // RCS budget can stop over the remaining distance.
            const double approach =
                distance > TerminalDeadbandShare * tolPosition
                    ? std::min({
                          TerminalApproachSpeedMps,
                          std::sqrt(rcsBudget * distance),
                          distance / TerminalApproachResponseSeconds})
                    : 0.0;
            const glm::dvec3 wantedVelocity =
                distance > 1.0e-9 ? error * (approach / distance)
                                  : glm::dvec3(0.0);
            trim = (wantedVelocity - velocity) /
                TerminalVelocityResponseSeconds;
            trim = applyRcsTranslation(
                out, trim, forward, right, up, params, RcsTrajectoryShare);
        }
        out.targetSpeedRate = 0.0f;

        decision_.terminalStep = static_cast<int>(term.step);
        decision_.terminalDistance = distance;
        decision_.terminalAttitudeErrorRad = attitudeError;
        decision_.terminalRollErrorRad = rollError;
        decision_.hullTarget =
            term.step == TerminalStep::PositionHold
                ? term.holdForwardMap : targetForward;
        decision_.demand = trim;
        decision_.thrustDirection = forward;
        return out;
    }

    // Calm full-pose turn: pitch/yaw and roll each follow a rate profile at
    // the preferred (calm) rate and acceleration, never the installed
    // maximum.
    static void applyCalmPose(
        AutopilotControlCommand& out,
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& targetForward,
        const glm::dvec3& targetUp,
        double angleDeadbandRad
    ) noexcept
    {
        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        poseRequest.currentRight =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        poseRequest.currentUp =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});
        poseRequest.targetForward =
            normalizedOr(targetForward, poseRequest.currentForward);
        poseRequest.targetUp = normalizedOr(targetUp, poseRequest.currentUp);
        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        const double authority =
            factsOf(params).angularScalarRadPerSec2;
        if (!pose.valid || !(authority > 1.0e-9))
            return;
        const double rateLimit = preferredTurnRateRadPerSec(params);
        const double acceleration =
            preferredTurnAccelerationRadPerSec2(params);
        constexpr double RateResponseSeconds = Policy::RateResponseSeconds;
        // Rate wanted to close `angle` and stop there at the calm
        // deceleration; linear near zero (finite gain, no limit cycle) and
        // nothing inside the deadband.
        constexpr double FinalApproachSeconds =
            Policy::PoseFinalApproachSeconds;
        const auto wantedRate = [&](double angle)
        {
            if (angle <= angleDeadbandRad)
                return 0.0;
            return std::min(
                {rateLimit, std::sqrt(2.0 * acceleration * angle),
                 angle / FinalApproachSeconds});
        };

        const glm::dvec2 error {
            pose.pitchYawErrorLocalRad.x, pose.pitchYawErrorLocalRad.y};
        const double angle = glm::length(error);
        const glm::dvec2 rate {
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec)};
        const glm::dvec2 desired =
            angle > 1.0e-9 ? error / angle * wantedRate(angle)
                           : glm::dvec2(0.0);
        glm::dvec2 planar = (desired - rate) / RateResponseSeconds;
        const double planarMagnitude = glm::length(planar);
        if (planarMagnitude > acceleration && planarMagnitude > 1.0e-12)
            planar *= acceleration / planarMagnitude;
        out.pitchInput = finiteClamp(planar.x / authority);
        out.yawInput = finiteClamp(planar.y / authority);

        const double rollAngle = pose.rollErrorRad;
        const double rollRate = finiteOrZero(request.rollRateRadPerSec);
        const double rollDesired =
            (rollAngle >= 0.0 ? 1.0 : -1.0) * wantedRate(std::abs(rollAngle));
        const double roll = std::clamp(
            (rollDesired - rollRate) / RateResponseSeconds,
            -acceleration, acceleration);
        out.rollInput = finiteClamp(roll / authority);
    }

    // ----- Program execution ------------------------------------------------

    struct PlannedPoint
    {
        double speedMps = 0.0;
        double longitudinalAccelerationMps2 = 0.0;
        double curvaturePerMeter = 0.0;
        glm::dvec3 tangentMap {0.0, 0.0, -1.0};
        glm::dvec3 normalMap {0.0};
        glm::dvec3 hullForwardMap {0.0, 0.0, -1.0};
        std::size_t phaseIndex = 0;
        bool narrow = false;
        glm::dvec3 narrowForwardMap {0.0, 0.0, -1.0};
        glm::dvec3 narrowUpMap {0.0, 1.0, 0.0};
        double narrowToleranceMeters = 0.0;
    };

    [[nodiscard]] PlannedPoint plannedAt(double progress) const noexcept
    {
        PlannedPoint out;
        const auto& st = program_.stations;
        const std::size_t k = program_.stationAt(progress);
        const auto& a = st[k];
        const auto& b = st[std::min(k + 1, st.size() - 1)];
        const double span = b.progressMeters - a.progressMeters;
        const double u = span > 1.0e-9
            ? std::clamp((progress - a.progressMeters) / span, 0.0, 1.0)
            : 0.0;
        out.speedMps = a.speedMps + (b.speedMps - a.speedMps) * u;
        out.longitudinalAccelerationMps2 =
            a.longitudinalAccelerationMps2 +
            (b.longitudinalAccelerationMps2 -
             a.longitudinalAccelerationMps2) * u;
        out.curvaturePerMeter =
            a.curvaturePerMeter +
            (b.curvaturePerMeter - a.curvaturePerMeter) * u;
        out.tangentMap = normalizedOr(
            a.tangentMap * (1.0 - u) + b.tangentMap * u, a.tangentMap);
        const glm::dvec3 normal = a.normalMap * (1.0 - u) + b.normalMap * u;
        out.normalMap = finiteLength(normal) > 1.0e-9
            ? normal / finiteLength(normal) : glm::dvec3(0.0);
        out.hullForwardMap = normalizedOr(
            a.hullForwardMap * (1.0 - u) + b.hullForwardMap * u,
            a.hullForwardMap);
        out.phaseIndex = a.phaseIndex;
        out.narrow = a.narrow;
        out.narrowForwardMap = a.narrowForwardMap;
        out.narrowUpMap = a.narrowUpMap;
        out.narrowToleranceMeters = a.narrowToleranceMeters;
        return out;
    }

    [[nodiscard]] AutopilotControlCommand programControl(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt
    ) noexcept
    {
        AutopilotControlCommand out;

        const double mainAuthority =
            std::max(0.0, factsOf(params).mainForwardMps2);
        const double rcsAuthority =
            std::max(0.0, factsOf(params).rcsScalarMps2);
        const double rcsBudget = RcsTrajectoryShare * rcsAuthority;
        const double gimbalLimit =
            std::max(
                factsOf(params).gimbalPitchRad,
                factsOf(params).gimbalYawRad
            );
        const double corridorTolerance =
            request.corridorToleranceMeters > 0.0
                ? request.corridorToleranceMeters
                : DefaultCorridorToleranceMeters;

        // A finished phase is never revived (the route projection may step
        // back a little at a seam); a later phase is entered by progress.
        // The plan is read no earlier than the current phase's start.
        state_.phaseIndex =
            std::max(
                state_.phaseIndex,
                plannedAt(request.programProgressMeters).phaseIndex);
        const ManoeuvrePhase& phase =
            program_.phases[std::min(
                state_.phaseIndex, program_.phases.size() - 1)];
        const double progress =
            std::max(request.programProgressMeters, phase.startProgressMeters);
        const PlannedPoint plan = plannedAt(progress);
        // Narrow zone (Step 3): the hull crosses it in one fixed attitude on
        // RCS. Its tube is the zone's real lateral room; no main, no turn.
        const bool narrow = plan.narrow;
        // The zone's room also governs the approach (the same lead as the
        // attitude): the craft must ENTER inside it, not correct inside.
        const double approachSpeed = std::max(
            0.0, glm::dot(finiteVecOrZero(request.actualVelocityMapMps),
                          plan.tangentMap));
        const PlannedPoint ahead = plannedAt(
            progress + approachSpeed * NarrowAttitudeLeadSeconds);
        // Only on the approach plateau (the plan already at the zone speed,
        // no manoeuvre left before the entry): preparing the zone pose
        // during an earlier brake or curve would spoil that manoeuvre.
        const bool onApproach =
            ahead.narrow &&
            plan.speedMps <= NarrowApproachSpeedShare * ahead.speedMps + 1.0;
        const PlannedPoint* zone =
            narrow ? &plan : (onApproach ? &ahead : nullptr);
        const double tolerance =
            zone && zone->narrowToleranceMeters > 0.0
                ? std::min(corridorTolerance, zone->narrowToleranceMeters)
                : corridorTolerance;
        const bool mainPhase =
            !narrow && manoeuvrePhaseUsesMain(phase.kind);

        const glm::dvec3 velocity =
            finiteVecOrZero(request.actualVelocityMapMps);
        const glm::dvec3 tangent = plan.tangentMap;
        const double alongSpeed = glm::dot(velocity, tangent);

        // ----- Required acceleration from the ACTUAL state: the profile's
        // longitudinal plan plus its speed error, the curve's normal
        // acceleration at the actual speed.
        const double speedError = plan.speedMps - alongSpeed;

        // A brake is done once the craft is no faster than its profile:
        // braking further would only reverse the motion (near-zero speed
        // ends the manoeuvre).
        const double feedForward =
            plan.longitudinalAccelerationMps2 < 0.0 && speedError > 0.0
                ? plan.longitudinalAccelerationMps2 *
                      std::clamp(
                          alongSpeed / (2.0 * BrakeFadeMps), 0.0, 1.0)
                : plan.longitudinalAccelerationMps2;
        double longitudinal =
            feedForward + speedError / LongitudinalResponseSeconds;
        const double speedDeadband =
            NewtonianSpeedTolerancePolicy::softBandMps(alongSpeed);
        if (std::abs(speedError) < speedDeadband &&
            std::abs(plan.longitudinalAccelerationMps2) <= 1.0e-3)
            longitudinal = 0.0;
        const double curveSpeed = std::max(0.0, alongSpeed);
        const glm::dvec3 curveAcceleration =
            plan.normalMap * (curveSpeed * curveSpeed * plan.curvaturePerMeter);

        // ----- Safe tube: a correction only when the PREDICTED cross-track
        // leaves it; it modifies the phase's vector, never replaces it.
        glm::dvec3 offset {0.0};
        if (request.positionReferenceValid)
            offset =
                finiteVecOrZero(request.actualPositionMapMeters) -
                finiteVecOrZero(request.desiredPositionMapMeters);
        const glm::dvec3 crossTrack =
            offset - tangent * glm::dot(offset, tangent);
        const glm::dvec3 lateralVelocity = velocity - tangent * alongSpeed;
        glm::dvec3 correction {0.0};
        bool lateralEmergency = false;
        if (request.positionReferenceValid)
        {
            const glm::dvec3 predicted =
                crossTrack + lateralVelocity * LateralPredictionSeconds;
            const double predictedDistance = finiteLength(predicted);
            if (!state_.lateralCorrection &&
                predictedDistance > SafeTubeFraction * tolerance)
                state_.lateralCorrection = true;
            else if (state_.lateralCorrection &&
                     predictedDistance <= TubeReleaseFraction * tolerance)
                state_.lateralCorrection = false;
            if (state_.lateralCorrection)
            {
                const double aimRadius = TubeAimFraction * tolerance;
                const glm::dvec3 aim =
                    predictedDistance > aimRadius
                        ? predicted * (aimRadius / predictedDistance)
                        : predicted;
                correction =
                    (aim - predicted) /
                    (LateralPredictionSeconds * LateralResponseSeconds);
            }
            const double crossDistance = finiteLength(crossTrack);
            const double outward =
                crossDistance > 1.0e-6
                    ? std::max(
                        0.0,
                        glm::dot(lateralVelocity, crossTrack / crossDistance))
                    : 0.0;
            const double margin =
                std::max(0.0, WallMarginFraction * tolerance - crossDistance);
            lateralEmergency =
                outward > 1.0e-6 &&
                (rcsBudget <= 1.0e-9 ||
                 outward * outward / (2.0 * rcsBudget) > margin);
        }

        glm::dvec3 demand =
            tangent * longitudinal + curveAcceleration + correction;
        // Terminal settle: the route stops here, so the whole velocity and
        // position error (not only its along-track part) is trimmed out
        // until the precision stop takes over.
        const bool settling =
            request.stopRequested && request.positionReferenceValid &&
            (plan.speedMps <= SettleSpeedMps ||
             finiteLength(velocity) <= SettleSpeedMps);
        if (settling)
            demand =
                -offset / (SettleResponseSeconds * SettleResponseSeconds) -
                velocity * (2.0 / SettleResponseSeconds);
        const double demandMagnitude = finiteLength(demand);

        // ----- Main engine: the phase decides. Outside a burn phase the main
        // only helps through the gate without moving the hull: a clear
        // deficit (e.g. arriving faster than planned) or a wall emergency.
        const bool mainWanted =
            mainAuthority > 1.0e-9 && !narrow &&
            ((mainPhase && !settling &&
              demandMagnitude > MainShareOfRcs * rcsBudget) ||
             lateralEmergency ||
             demandMagnitude > OffPhaseMainShareOfRcs * rcsBudget);

        const double hullRate =
            std::hypot(
                finiteOrZero(request.pitchRateRadPerSec),
                finiteOrZero(request.yawRateRadPerSec)
            );
        glm::dvec3 burnDirection = normalizedOr(demand, tangent);

        // Planned hull, a little ahead to cover the attitude servo lag. Safety
        // may take the hull: a wall exit the RCS cannot stop and the current
        // hull cannot burn against.
        const double leadProgress =
            progress + std::max(0.0, alongSpeed) * HullLeadSeconds;
        glm::dvec3 hullTarget = plannedAt(leadProgress).hullForwardMap;
        const bool safetyOverride =
            !narrow && lateralEmergency &&
            angleBetween(forward, burnDirection) - gimbalLimit >
                BurnGateLimitedRad;
        if (safetyOverride)
            hullTarget = burnDirection;
        // On a curve the phase owns the thrust axis: the main fires within
        // the nozzle cone around the planned curve axis. Corrections ride
        // inside that cone or go to RCS; a speed catch-up never swings the
        // burn off the curve (the path-normal push is what keeps the craft
        // in the corridor there).
        else if (phase.kind == ManoeuvrePhaseKind::Curve &&
                 angleBetween(plan.hullForwardMap, burnDirection) >
                     gimbalLimit)
            burnDirection =
                manoeuvre_detail::stepToward(
                    plan.hullForwardMap, burnDirection, gimbalLimit, tangent);
        const double hullError = angleBetween(forward, burnDirection);
        const double thrustError = std::max(0.0, hullError - gimbalLimit);

        double throttle = 0.0;
        glm::dvec3 delivered {0.0};
        decision_.gate = 0;
        decision_.gateReason = 0;
        if (mainWanted)
        {
            const auto gimbal =
                solveGimbalDemand(burnDirection, forward, right, up, params);
            // Only the demand's component along the burn axis is main work.
            const double wanted =
                std::clamp(
                    std::max(0.0, glm::dot(demand, burnDirection)) /
                        mainAuthority,
                    0.0,
                    1.0
                );
            // The hull rate is no gate on a curve: the plan turns it there.
            const bool rateSettled =
                phase.kind == ManoeuvrePhaseKind::Curve ||
                hullRate <= BurnRateGateRadPerSec;
            decision_.gate = 3;
            decision_.gateReason =
                thrustError > BurnGateFullRad ? 1 : !rateSettled ? 2 : 0;
            if (thrustError <= BurnGateFullRad && rateSettled)
            {
                throttle = wanted;
                decision_.gate = 1;
            }
            else if (thrustError <= BurnGateLimitedRad)
            {
                // Limited burn: its side push stays within what RCS cancels
                // and never pushes toward a near corridor wall.
                const double sidePush = mainAuthority * std::sin(thrustError);
                throttle =
                    sidePush > 1.0e-9
                        ? std::min(wanted, 0.8 * rcsBudget / sidePush)
                        : wanted;
                decision_.gate = 2;
                const glm::dvec3 side =
                    gimbal.commandedDirectionMap -
                    burnDirection *
                        glm::dot(gimbal.commandedDirectionMap, burnDirection);
                if (request.positionReferenceValid &&
                    finiteLength(crossTrack) > 0.6 * tolerance &&
                    glm::dot(side, crossTrack) > 0.0)
                {
                    throttle = 0.0;
                    decision_.gate = 3;
                    decision_.gateReason = 3;
                }
            }
            if (throttle > 1.0e-6)
            {
                out.mainGimbalRightInput = gimbal.rightInput;
                out.mainGimbalUpInput = gimbal.upInput;
                delivered =
                    gimbal.commandedDirectionMap * (mainAuthority * throttle);
            }
            else
            {
                throttle = 0.0;
            }
        }

        // A narrow zone ahead (within the attitude lead) or here fixes the
        // whole pose: nose on the zone axis as planned, up on the zone's up
        // (either sign, whichever is closer), set calmly before the entry.
        if (zone)
        {
            const glm::dvec3 zoneUp =
                glm::dot(up, zone->narrowUpMap) >= 0.0
                    ? zone->narrowUpMap : -zone->narrowUpMap;
            if (narrow)
                hullTarget = zone->narrowForwardMap;
            applyCalmPose(
                out, request, params, hullTarget, zoneUp,
                0.5 * NarrowAttitudeToleranceRad);
        }
        else if (calmTurn(angleBetween(forward, hullTarget)))
            applyFlipRate(out, request, params, hullTarget, up, dt);
        else
            applyAttitude(out, request, params, hullTarget, up, dt);
        out.targetSpeedRate = finiteClamp(throttle);

        // RCS takes everything the main does not deliver, within its
        // trajectory budget per body direction.
        glm::dvec3 rcsCommanded = applyRcsTranslation(
            out, demand - delivered, forward, right, up, params,
            RcsTrajectoryShare);

        // Independent execution safety monitor (it plans nothing; it only
        // vetoes or overrides a command that makes the corridor unsafe).
        {
            ExecutionSafetyInput monitor;
            monitor.crossTrack = crossTrack;
            monitor.lateralVelocity = lateralVelocity;
            monitor.tangent = tangent;
            monitor.commandedAcceleration = delivered + rcsCommanded;
            monitor.mainDelivered = delivered;
            monitor.referenceLateralAcceleration = curveAcceleration;
            monitor.toleranceMeters =
                request.positionReferenceValid ? tolerance : 0.0;
            monitor.lateralAuthorityMps2 =
                rcsAuthority +
                (narrow ? 0.0 : mainAuthority * std::sin(gimbalLimit));
            monitor.horizonSeconds = LateralPredictionSeconds;
            monitor.narrow = narrow;
            if (narrow && zone)
            {
                const double error =
                    angleBetween(forward, zone->narrowForwardMap);
                monitor.attitudeErrorRad = error;
                monitor.attitudeToleranceRad = NarrowAttitudeToleranceRad;
                monitor.attitudeErrorRateRadPerSec =
                    state_.monitorAttitudeValid
                        ? (error - state_.monitorAttitudeErrorRad) / dt
                        : 0.0;
                state_.monitorAttitudeErrorRad = error;
                state_.monitorAttitudeValid = true;
            }
            else
                state_.monitorAttitudeValid = false;
            // DEV experiment switch only (A/B of the monitor offline).
            static const bool monitorOff = []
            {
                const char* v = std::getenv("ELITE_NEWTONIAN_MONITOR_OFF");
                return v && *v;
            }();
            const auto verdict = monitorOff
                ? ExecutionSafetyResult {}
                : evaluateExecutionSafety(monitor);
            decision_.monitor = static_cast<int>(verdict.verdict) +
                (verdict.stopRotation ? 4 : 0);
            if (verdict.inhibitMain && throttle > 0.0)
            {
                throttle = 0.0;
                out.targetSpeedRate = 0.0f;
                out.mainGimbalRightInput = 0.0f;
                out.mainGimbalUpInput = 0.0f;
                delivered = glm::dvec3(0.0);
                rcsCommanded = applyRcsTranslation(
                    out, demand, forward, right, up, params,
                    RcsTrajectoryShare);
            }
            if (verdict.forceInwardRcs)
                (void)applyRcsTranslation(
                    out, verdict.inward * (4.0 * rcsAuthority + 1.0),
                    forward, right, up, params, 1.0);
            if (verdict.stopRotation)
            {
                constexpr double DampRateRadPerSec = 0.2;
                out.pitchInput = finiteClamp(
                    -finiteOrZero(request.pitchRateRadPerSec) / DampRateRadPerSec);
                out.yawInput = finiteClamp(
                    -finiteOrZero(request.yawRateRadPerSec) / DampRateRadPerSec);
                out.rollInput = finiteClamp(
                    -finiteOrZero(request.rollRateRadPerSec) / DampRateRadPerSec);
            }
        }

        // Executed coarse state for the trace.
        state_.phase =
            throttle > 0.0 ? Phase::Burn
            : angleBetween(forward, hullTarget) > FlipEntryRad ? Phase::Flip
            : mainWanted ? Phase::Align : Phase::Coast;

        decision_.programPhase = static_cast<int>(phase.kind);
        decision_.phaseIndex = static_cast<int>(state_.phaseIndex);
        decision_.phaseStartSeconds = phase.startTimeSeconds;
        decision_.crossTrack = crossTrack;
        decision_.lateralVelocity = lateralVelocity;
        decision_.speedError = speedError;
        decision_.plannedSpeed = plan.speedMps;
        decision_.feedForwardAlong = plan.longitudinalAccelerationMps2;
        decision_.feedForwardLateral = finiteLength(curveAcceleration);
        decision_.correction = correction;
        decision_.demand = demand;
        decision_.thrustDirection = burnDirection;
        decision_.hullTarget = hullTarget;
        decision_.hullErrorRad = hullError;
        decision_.thrustErrorRad = thrustError;
        decision_.planHullErrorRad = angleBetween(forward, hullTarget);
        decision_.gimbalLimitRad = gimbalLimit;
        decision_.mainPhase = mainPhase;
        decision_.mainNeeded = mainWanted;
        decision_.braking = mainWanted && glm::dot(burnDirection, velocity) < 0.0;
        decision_.lateralCorrection = state_.lateralCorrection;
        decision_.emergency = lateralEmergency;
        decision_.safetyOverride = safetyOverride;
        decision_.narrow = narrow;
        decision_.radiusMeters =
            plan.curvaturePerMeter > 1.0e-9 ? 1.0 / plan.curvaturePerMeter : 0.0;
        return out;
    }

    [[nodiscard]] static double preferredTurnRateRadPerSec(
        const ShipParams& params
    ) noexcept
    {
        return newtonianPreferredTurnRateRadPerSec(factsOf(params));
    }

    [[nodiscard]] static double preferredTurnAccelerationRadPerSec2(
        const ShipParams& params
    ) noexcept
    {
        return newtonianPreferredTurnAccelerationRadPerSec2(factsOf(params));
    }

    // Calm rest-to-rest turn time at the preferred rate/acceleration,
    // plus the align/settle time before a burn may start.
    [[nodiscard]] static double plannedTurnSeconds(
        double angleRad,
        const ShipParams& params
    ) noexcept
    {
        return newtonianPlannedTurnSeconds(angleRad, factsOf(params));
    }

    // Planned flip: track a calm trapezoidal rate profile toward the target
    // instead of the pose servo's full-authority bang. Roll is unchanged.
    static void applyFlipRate(
        AutopilotControlCommand& out,
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& targetForward,
        const glm::dvec3& targetUp,
        double dt
    ) noexcept
    {
        applyAttitude(out, request, params, targetForward, targetUp, dt);

        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        poseRequest.currentRight =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        poseRequest.currentUp =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});
        poseRequest.targetForward =
            normalizedOr(targetForward, poseRequest.currentForward);
        poseRequest.targetUp = poseRequest.currentUp;
        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        const double authority =
            factsOf(params).angularScalarRadPerSec2;
        if (!pose.valid || !(authority > 1.0e-9))
            return;

        const glm::dvec2 error {
            pose.pitchYawErrorLocalRad.x,
            pose.pitchYawErrorLocalRad.y
        };
        const double angle = glm::length(error);
        if (!(angle > 1.0e-9))
            return;
        const glm::dvec2 axis = error / angle;

        const double rateLimit = preferredTurnRateRadPerSec(params);
        const double acceleration =
            preferredTurnAccelerationRadPerSec2(params);
        const double remaining =
            std::max(0.0, angle - 0.5 * FlipExitRad);
        const double desiredRate =
            std::min(rateLimit, std::sqrt(2.0 * acceleration * remaining));

        const glm::dvec2 rate {
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec)
        };
        constexpr double RateResponseSeconds = Policy::RateResponseSeconds;
        glm::dvec2 wanted =
            (axis * desiredRate - rate) * (2.0 / RateResponseSeconds);
        const double magnitude = glm::length(wanted);
        if (magnitude > acceleration && magnitude > 1.0e-12)
            wanted *= acceleration / magnitude;
        out.pitchInput = finiteClamp(wanted.x / authority);
        out.yawInput = finiteClamp(wanted.y / authority);
    }

    static void applyAttitude(
        AutopilotControlCommand& out,
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& targetForward,
        const glm::dvec3& targetUp,
        double dt
    ) noexcept
    {
        HullPoseGuidance::Request poseRequest;
        poseRequest.currentForward =
            normalizedOr(request.forwardMap, {0.0, 0.0, -1.0});
        poseRequest.currentRight =
            normalizedOr(request.rightMap, {1.0, 0.0, 0.0});
        poseRequest.currentUp =
            normalizedOr(request.upMap, {0.0, 1.0, 0.0});
        poseRequest.targetForward =
            normalizedOr(targetForward, poseRequest.currentForward);
        poseRequest.targetUp =
            normalizedOr(targetUp, poseRequest.currentUp);

        const auto pose = HullPoseGuidance::evaluate(poseRequest);
        if (!pose.valid)
            return;

        HullAttitudeControl::Request attitudeRequest;
        attitudeRequest.pitchYawErrorRad = {
            pose.pitchYawErrorLocalRad.x,
            pose.pitchYawErrorLocalRad.y
        };
        attitudeRequest.rollErrorRad = pose.rollErrorRad;
        attitudeRequest.pitchYawRateRadPerSec = {
            finiteOrZero(request.pitchRateRadPerSec),
            finiteOrZero(request.yawRateRadPerSec)
        };
        attitudeRequest.desiredPitchYawRateRadPerSec = {0.0, 0.0};
        attitudeRequest.rollRateRadPerSec =
            finiteOrZero(request.rollRateRadPerSec);
        attitudeRequest.desiredRollRateRadPerSec = 0.0;
        attitudeRequest.maxPitchYawRateRadPerSec = {
            std::max(0.0, static_cast<double>(params.maxPitchRate)),
            std::max(0.0, static_cast<double>(params.maxYawRate))
        };
        attitudeRequest.maxRollRateRadPerSec =
            std::max(0.0, static_cast<double>(params.maxRollRate));

        const double angularAuthority =
            std::max(
                1.0e-9,
                factsOf(params).angularScalarRadPerSec2
            );
        attitudeRequest.pitchYawAngularAccelerationAuthorityRadPerSec2 =
            angularAuthority;
        attitudeRequest.rollAngularAccelerationAuthorityRadPerSec2 =
            angularAuthority;
        attitudeRequest.deltaSeconds = dt;
        // Same critically damped hull servo as Assisted. The old one-tick
        // predictive law demanded the target rate within 20 ms; with strong
        // installed RCS and replicated (delayed) rates it limit-cycled.
        attitudeRequest.pitchYawResponseSeconds = 0.50;
        attitudeRequest.rollResponseSeconds = 0.50;

        const auto attitude =
            HullAttitudeControl::evaluate(attitudeRequest);
        if (!attitude.valid)
            return;

        out.pitchInput =
            finiteClamp(attitude.pitchYawInput.x);
        out.yawInput =
            finiteClamp(attitude.pitchYawInput.y);
        out.rollInput =
            finiteClamp(attitude.rollInput);
    }

    [[nodiscard]] static AutopilotControlCommand precisionStopControl(
        const AutopilotControlRequest& request,
        const ShipParams& params,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        double dt
    ) noexcept
    {
        AutopilotControlCommand out;

        applyAttitude(
            out,
            request,
            params,
            request.terminalAttitudeHold
                ? request.desiredForwardMap
                : forward,
            request.terminalAttitudeHold
                ? request.desiredUpMap
                : up,
            dt
        );

        const double rcsAuthority =
            factsOf(params).rcsScalarMps2;
        if (rcsAuthority > 1.0e-12)
        {
            // Residual velocity is trimmed over a second, and a residual
            // inside the deadband is left alone (no chase of a zero).
            constexpr double PrecisionResponseSeconds =
                Policy::PrecisionResponseSeconds;
            constexpr double PrecisionDeadbandMps =
                Policy::PrecisionDeadbandMps;
            const glm::dvec3 residual =
                request.desiredVelocityMapMps - request.actualVelocityMapMps;
            glm::dvec3 wantedAcceleration =
                finiteLength(residual) <= PrecisionDeadbandMps
                    ? glm::dvec3(0.0)
                    : residual / PrecisionResponseSeconds;

            applyRcsTranslation(
                out, wantedAcceleration, forward, right, up, params, 1.0);
        }

        return out;
    }

    [[nodiscard]] static GimbalDemandResult solveGimbalDemand(
        const glm::dvec3& desiredDirection,
        const glm::dvec3& forward,
        const glm::dvec3& right,
        const glm::dvec3& up,
        const ShipParams& params
    ) noexcept
    {
        GimbalDemandResult out;

        const double yawLimit =
            factsOf(params).gimbalYawRad;
        const double pitchLimit =
            factsOf(params).gimbalPitchRad;

        const glm::dvec3 desired =
            normalizedOr(desiredDirection, forward);

        if (yawLimit <= 1.0e-12 && pitchLimit <= 1.0e-12)
        {
            out.commandedDirectionMap = forward;
            return out;
        }

        const double localForward =
            glm::dot(desired, forward);
        const double localRight =
            glm::dot(desired, right);
        const double localUp =
            glm::dot(desired, up);

        const double desiredYaw =
            std::atan2(localRight, localForward);
        const double desiredPitch =
            std::atan2(
                localUp,
                std::sqrt(
                    localForward * localForward +
                    localRight * localRight
                )
            );

        double right01 =
            yawLimit > 1.0e-12
                ? desiredYaw / yawLimit
                : 0.0;
        double up01 =
            pitchLimit > 1.0e-12
                ? desiredPitch / pitchLimit
                : 0.0;

        const double normalizedLength =
            std::sqrt(right01 * right01 + up01 * up01);
        if (normalizedLength > 1.0)
        {
            right01 /= normalizedLength;
            up01 /= normalizedLength;
        }

        right01 = std::clamp(right01, -1.0, 1.0);
        up01 = std::clamp(up01, -1.0, 1.0);

        out.rightInput = finiteClamp(right01);
        out.upInput = finiteClamp(up01);

        const double commandedYaw = right01 * yawLimit;
        const double commandedPitch = up01 * pitchLimit;
        out.commandedDirectionMap =
            normalizedOr(
                forward +
                    right * std::tan(commandedYaw) +
                    up * std::tan(commandedPitch),
                forward
            );
        return out;
    }

    [[nodiscard]] static double angleBetween(
        const glm::dvec3& a,
        const glm::dvec3& b
    ) noexcept
    {
        const double la = finiteLength(a);
        const double lb = finiteLength(b);
        if (la <= 1.0e-12 || lb <= 1.0e-12)
            return 0.0;
        return std::acos(
            std::clamp(
                glm::dot(a / la, b / lb),
                -1.0,
                1.0
            )
        );
    }

    [[nodiscard]] static double finiteOrZero(double value) noexcept
    {
        return std::isfinite(value) ? value : 0.0;
    }

    [[nodiscard]] static double finiteLength(
        const glm::dvec3& value
    ) noexcept
    {
        const double magnitude = glm::length(value);
        return std::isfinite(magnitude) ? magnitude : 0.0;
    }

    [[nodiscard]] static glm::dvec3 finiteVecOrZero(
        const glm::dvec3& value
    ) noexcept
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z)
                ? value
                : glm::dvec3(0.0);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double magnitude = glm::length(value);
        if (std::isfinite(magnitude) && magnitude > 1.0e-12)
            return value / magnitude;

        const double fallbackMagnitude = glm::length(fallback);
        return fallbackMagnitude > 1.0e-12
            ? fallback / fallbackMagnitude
            : glm::dvec3(0.0, 0.0, -1.0);
    }

    [[nodiscard]] static float finiteClamp(double value) noexcept
    {
        return static_cast<float>(
            std::clamp(
                std::isfinite(value) ? value : 0.0,
                -1.0,
                1.0
            )
        );
    }

    // DEV diagnostic only. ELITE_NEWTONIAN_POLICY_TRACE=<tsv path> writes a
    // decision row at 10 Hz (and on every event tick);
    // ELITE_NEWTONIAN_EVENT_TRACE=<tsv path> writes discrete decision events
    // and every compiled program. No effect on control when unset.
    static std::FILE* openTrace(const char* variable, const char* header)
    {
        const char* path = std::getenv(variable);
        if (!path || !*path)
            return nullptr;
        std::FILE* f = std::fopen(path, "w");
        if (f)
            std::fputs(header, f);
        return f;
    }

    void traceTick(
        const AutopilotControlRequest& request,
        const AutopilotControlCommand& out,
        double dt
    ) noexcept
    {
        static std::FILE* file = openTrace(
            "ELITE_NEWTONIAN_POLICY_TRACE",
            "t\tcurve\ts_m\tref_x\tref_y\tref_z\ttan_x\ttan_y\ttan_z\t"
            "radius_m\tprofile_mps\tref_mps\tspeed_mps\tprog_phase\t"
            "phase_idx\tphase_start_s\txtrack_m\tpred1_m\tpred2_m\t"
            "pred3_m\tpred4_m\tlat_v_mps\tlong_err_mps\tff_along\tff_lat\t"
            "corr_acc\tdemand_acc\tfwd_x\tfwd_y\tfwd_z\tomega\t"
            "thrust_x\tthrust_y\tthrust_z\thull_x\thull_y\thull_z\t"
            "hull_err_deg\tthrust_err_deg\tplan_hull_err_deg\tgimbal_r\t"
            "gimbal_u\tgimbal_lim_deg\tthrottle\trcs_f\trcs_s\trcs_l\tpitch\t"
            "yaw\troll\tphase\tmain_phase\tmain_wanted\toverride\tgate\t"
            "gate_reason\ttube\temergency\tbraking\ttarget_changed\t"
            "target_delta_deg\tthrust_delta_deg\tstop\ttol_m\tterm_step\t"
            "term_dist_m\tterm_att_deg\tterm_roll_deg\tnarrow\tup_x\tup_y\t"
            "up_z\tmonitor\n");
        static std::FILE* events = openTrace(
            "ELITE_NEWTONIAN_EVENT_TRACE", "t\ts_m\tevent\tdetail\n");
        state_.traceSeconds += dt;
        if (!file && !events)
            return;

        constexpr double Deg = 57.2957795130823208768;
        const double t = state_.traceSeconds;
        const double s = request.traceRouteProgressMeters;
        const Decision& d = decision_;
        TraceMemory& m = traceMemory_;
        const int phase = static_cast<int>(state_.phase);
        const bool curving = request.traceCurvaturePerMeter > 1.0e-6;
        const double targetDelta =
            m.valid ? angleBetween(m.hullTarget, d.hullTarget) * Deg : 0.0;
        const double thrustDelta =
            m.valid ? angleBetween(m.thrustDirection, d.thrustDirection) * Deg
                    : 0.0;
        const double crossDistance = finiteLength(d.crossTrack);
        const double tol = request.corridorToleranceMeters;
        const bool violation =
            request.positionReferenceValid && tol > 0.0 && crossDistance > tol;

        bool eventTick = false;
        char detail[200];
        auto event = [&](const char* name)
        {
            eventTick = true;
            if (events)
                std::fprintf(events, "%.3f\t%.1f\t%s\t%s\n", t, s, name, detail);
        };
        if (state_.programRevision != m.programRevision)
        {
            std::snprintf(detail, sizeof(detail),
                "revision=%llu phases=%zu stations=%zu deficit_deg=%.1f at=%.0f",
                static_cast<unsigned long long>(state_.programRevision),
                program_.phases.size(), program_.stations.size(),
                program_.worstThrustDeficitRad * Deg,
                program_.worstThrustDeficitProgressMeters);
            event("PROGRAM");
            if (events)
                for (std::size_t i = 0; i < program_.phases.size(); ++i)
                {
                    const auto& p = program_.phases[i];
                    // Planned hull against the tangent and against the
                    // required acceleration at the phase start/middle/end.
                    char angles[96];
                    int used = 0;
                    const std::size_t picks[3] = {
                        p.firstStation, (p.firstStation + p.lastStation) / 2,
                        p.lastStation};
                    for (std::size_t q : picks)
                    {
                        const auto& station = program_.stations[q];
                        used += std::snprintf(angles + used,
                            sizeof(angles) - static_cast<std::size_t>(used),
                            " %.0f/%.0f",
                            angleBetween(station.hullForwardMap,
                                         station.tangentMap) * Deg,
                            station.mainBurn
                                ? angleBetween(station.hullForwardMap,
                                      station.requiredAccelerationMap) * Deg
                                : -1.0);
                    }
                    std::fprintf(events,
                        "%.3f\t%.1f\tPROGRAM_PHASE\t#%zu %s s=%.0f-%.0f "
                        "t=%.1f-%.1f hullT/hullAxis%s\n", t, s, i,
                        manoeuvrePhaseName(p.kind),
                        p.startProgressMeters, p.endProgressMeters,
                        p.startTimeSeconds, p.endTimeSeconds, angles);
                }
        }
        if (m.valid)
        {
            if (curving != m.curving)
            {
                std::snprintf(detail, sizeof(detail),
                    "curve=%zu radius=%.0f speed=%.1f",
                    request.traceCurveIndex,
                    curving ? 1.0 / request.traceCurvaturePerMeter : 0.0,
                    finiteLength(request.actualVelocityMapMps));
                event(curving ? "ENTER_CURVE" : "LEAVE_CURVE");
            }
            if (d.phaseIndex != m.phaseIndex && d.phaseIndex >= 0)
            {
                std::snprintf(detail, sizeof(detail), "#%d %s", d.phaseIndex,
                    manoeuvrePhaseName(
                        static_cast<ManoeuvrePhaseKind>(d.programPhase)));
                event("MANOEUVRE_PHASE");
            }
            if (targetDelta > 25.0)
            {
                std::snprintf(detail, sizeof(detail),
                    "hull_target_delta=%.1f phase=%d", targetDelta, phase);
                event("TARGET_SWITCH");
            }
            if (d.monitor != m.monitor)
            {
                std::snprintf(detail, sizeof(detail), "verdict=%d xtrack=%.1f",
                    d.monitor, crossDistance);
                event("MONITOR");
            }
            if (d.safetyOverride != m.safetyOverride)
            {
                std::snprintf(detail, sizeof(detail), "xtrack=%.1f",
                    crossDistance);
                event(d.safetyOverride ? "SAFETY_ON" : "SAFETY_OFF");
            }
            const bool open = d.gate == 1 || d.gate == 2;
            const bool wasOpen = m.gate == 1 || m.gate == 2;
            if (open != wasOpen && (d.gate != 0 || m.gate != 0))
            {
                std::snprintf(detail, sizeof(detail),
                    "gate=%d reason=%d thrust_err=%.1f hull_err=%.1f",
                    d.gate, d.gateReason, d.thrustErrorRad * Deg,
                    d.hullErrorRad * Deg);
                event(open ? "BURN_GATE_OPEN" : "BURN_GATE_CLOSE");
            }
            if (phase != m.phase)
            {
                std::snprintf(detail, sizeof(detail), "%d->%d", m.phase, phase);
                event("PHASE_CHANGE");
            }
            if (thrustDelta > 90.0 && d.mainNeeded)
            {
                std::snprintf(detail, sizeof(detail), "thrust_delta=%.1f",
                    thrustDelta);
                event("COMMAND_REVERSAL");
            }
            const double pitch = out.pitchInput;
            const double yaw = out.yawInput;
            if ((pitch * m.pitchInput < -0.09 &&
                 std::abs(pitch) > 0.3 && std::abs(m.pitchInput) > 0.3) ||
                (yaw * m.yawInput < -0.09 &&
                 std::abs(yaw) > 0.3 && std::abs(m.yawInput) > 0.3))
            {
                std::snprintf(detail, sizeof(detail),
                    "rotation pitch %.2f->%.2f yaw %.2f->%.2f",
                    m.pitchInput, pitch, m.yawInput, yaw);
                event("COMMAND_REVERSAL");
            }
            if (d.terminalStep != m.terminalStep)
            {
                static const char* const Steps[] = {
                    "NONE", "POSITION_HOLD", "ATTITUDE_ALIGN", "RATE_SETTLE",
                    "DONE"};
                std::snprintf(detail, sizeof(detail),
                    "%s dist=%.2f speed=%.2f att=%.1f roll=%.1f omega=%.3f",
                    Steps[std::clamp(d.terminalStep, 0, 4)],
                    d.terminalDistance,
                    finiteLength(request.actualVelocityMapMps),
                    d.terminalAttitudeErrorRad * Deg,
                    d.terminalRollErrorRad * Deg,
                    std::sqrt(
                        request.pitchRateRadPerSec *
                            request.pitchRateRadPerSec +
                        request.yawRateRadPerSec * request.yawRateRadPerSec +
                        request.rollRateRadPerSec *
                            request.rollRateRadPerSec));
                event("TERMINAL_STEP");
            }
            if (violation != m.corridorViolation)
            {
                std::snprintf(detail, sizeof(detail), "xtrack=%.1f tol=%.1f",
                    crossDistance, tol);
                event(violation ? "CORRIDOR_VIOLATION" : "CORRIDOR_RECOVERED");
            }
        }

        if (file && (eventTick || m.tick % 5 == 0))
        {
            const glm::dvec3 v = finiteVecOrZero(request.actualVelocityMapMps);
            const glm::dvec3 vDes =
                finiteVecOrZero(request.desiredVelocityMapMps);
            const glm::dvec3 tangent =
                normalizedOr(vDes, request.desiredForwardMap);
            const glm::dvec3 ref = request.desiredPositionMapMeters;
            const glm::dvec3 fwd = request.forwardMap;
            const double omega = std::sqrt(
                request.pitchRateRadPerSec * request.pitchRateRadPerSec +
                request.yawRateRadPerSec * request.yawRateRadPerSec +
                request.rollRateRadPerSec * request.rollRateRadPerSec);
            auto pred = [&](double seconds)
            {
                return finiteLength(d.crossTrack + d.lateralVelocity * seconds);
            };
            std::fprintf(file,
                "%.3f\t%zu\t%.1f\t%.1f\t%.1f\t%.1f\t%.4f\t%.4f\t%.4f\t"
                "%.0f\t%.2f\t%.2f\t%.2f\t%d\t%d\t%.2f\t%.2f\t%.2f\t%.2f\t"
                "%.2f\t%.2f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t"
                "%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t%.4f\t"
                "%.4f\t%.2f\t%.2f\t%.2f\t%.3f\t%.3f\t%.2f\t%.3f\t%.3f\t"
                "%.3f\t%.3f\t%.3f\t%.3f\t%.3f\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t"
                "%d\t%d\t%d\t%.1f\t%.1f\t%d\t%.1f\t%d\t%.2f\t%.2f\t%.2f\t%d\t"
                "%.4f\t%.4f\t%.4f\t%d\n",
                t, request.traceCurveIndex, s, ref.x, ref.y, ref.z,
                tangent.x, tangent.y, tangent.z,
                curving ? 1.0 / request.traceCurvaturePerMeter : 0.0,
                d.plannedSpeed, request.traceProfileSpeedMps,
                finiteLength(v), d.programPhase, d.phaseIndex,
                d.phaseStartSeconds,
                request.positionReferenceValid ? crossDistance : 0.0,
                pred(1.0), pred(2.0), pred(3.0), pred(4.0),
                finiteLength(d.lateralVelocity), d.speedError,
                d.feedForwardAlong, d.feedForwardLateral,
                finiteLength(d.correction), finiteLength(d.demand),
                fwd.x, fwd.y, fwd.z, omega,
                d.thrustDirection.x, d.thrustDirection.y, d.thrustDirection.z,
                d.hullTarget.x, d.hullTarget.y, d.hullTarget.z,
                d.hullErrorRad * Deg, d.thrustErrorRad * Deg,
                d.planHullErrorRad * Deg,
                out.mainGimbalRightInput, out.mainGimbalUpInput,
                d.gimbalLimitRad * Deg, out.targetSpeedRate,
                out.forwardInput, out.strafeInput, out.liftInput,
                out.pitchInput, out.yawInput, out.rollInput,
                phase, d.mainPhase ? 1 : 0, d.mainNeeded ? 1 : 0,
                d.safetyOverride ? 1 : 0, d.gate, d.gateReason,
                d.lateralCorrection ? 1 : 0, d.emergency ? 1 : 0,
                d.braking ? 1 : 0, targetDelta > 1.0 ? 1 : 0,
                targetDelta, thrustDelta,
                request.stopRequested ? 1 : 0, tol, d.terminalStep,
                d.terminalDistance, d.terminalAttitudeErrorRad * Deg,
                d.terminalRollErrorRad * Deg, d.narrow ? 1 : 0,
                request.upMap.x, request.upMap.y, request.upMap.z, d.monitor);
        }
        if (file)
            std::fflush(file);
        if (events)
            std::fflush(events);

        m.valid = true;
        m.hullTarget = d.hullTarget;
        m.thrustDirection = d.thrustDirection;
        m.phase = phase;
        m.phaseIndex = d.phaseIndex;
        m.programRevision = state_.programRevision;
        m.safetyOverride = d.safetyOverride;
        m.monitor = d.monitor;
        m.gate = d.gate;
        m.curving = curving;
        m.corridorViolation = violation;
        m.terminalStep = d.terminalStep;
        m.pitchInput = out.pitchInput;
        m.yawInput = out.yawInput;
        ++m.tick;
    }

    void remember(const AutopilotControlRequest& request) noexcept
    {
        state_.initialized = true;
        state_.previousVelocityMapMps =
            finiteVecOrZero(request.actualVelocityMapMps);
    }

    State state_ {};
    NewtonianManoeuvreProgram program_ {};
    // Speed limits the motion profile decided on this route (one pilot per
    // route): kept for every later recompile.
    std::vector<ManoeuvreSpeedCap> speedCaps_;
    Decision decision_ {};
    struct TraceMemory
    {
        bool valid = false;
        glm::dvec3 hullTarget {0.0, 0.0, -1.0};
        glm::dvec3 thrustDirection {0.0, 0.0, -1.0};
        int phase = -1;
        int phaseIndex = -1;
        std::uint64_t programRevision = 0;
        bool safetyOverride = false;
        int monitor = 0;
        int gate = 0;
        bool curving = false;
        bool corridorViolation = false;
        int terminalStep = 0;
        double pitchInput = 0.0;
        double yawInput = 0.0;
        int tick = 0;
    } traceMemory_ {};
};

} // namespace game::navigation::autopilot
