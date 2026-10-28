#pragma once

namespace game::navigation::autopilot
{

// Newtonian flight POLICY in one place (the manoeuvre compiler and the
// executing pilot). These are doctrine choices, not physical capability:
// facts come from NavigationControlCapability, the speed tolerance corridor
// from NewtonianSpeedTolerancePolicy. Values are the flight-verified ones;
// where one constant is defined by another, the relation is written out.
// Fixed seconds (12 s, 15 s ...) are fallbacks that protect the verified
// baseline, not laws of physics: deriving them from capability, geometry,
// speed and control delay is later work.
struct NewtonianFlightPolicy
{
    static constexpr double DegToRad = 3.14159265358979323846 / 180.0;

    // ----- Actuator shares
    // A share of installed RCS stays reserved for attitude and nozzle
    // counter-torque; the rest may trim the trajectory.
    static constexpr double RcsTrajectoryShare = 0.60;
    // A burn is planned (compiler) and the main works in a burn phase
    // (pilot) where the requirement exceeds this share of the RCS
    // trajectory budget; the rest of that budget is the executor's margin.
    static constexpr double MainShareOfRcs = 0.80;
    // Outside burn phases only a clear deficit beyond this share uses the
    // main, through the burn gate.
    static constexpr double OffPhaseMainShareOfRcs = 2.0;
    // Smallest translational RCS acceleration worth commanding.
    static constexpr double RcsDribbleMps2 = 0.01;

    // ----- Turning
    // Planned turns use a calm share of the installed capability; the
    // program plans hull motion at a further margin below it.
    static constexpr double PreferredRateShare = 0.50;
    static constexpr double PreferredAccelerationShare = 0.50;
    static constexpr double PlannedRateMargin = 0.80;
    static constexpr double AlignSettleSeconds = 1.0;
    static constexpr double FlipLeadMarginSeconds = 1.5;
    // Calm-turn hysteresis: a flip starts beyond / ends inside.
    static constexpr double FlipEntryRad = 25.0 * DegToRad;
    static constexpr double FlipExitRad = 12.0 * DegToRad;
    // The hull target is taken this far ahead along the plan to cover the
    // attitude servo lag.
    static constexpr double HullLeadSeconds = 0.5;
    // Arrest/settle time added to a planned turn window (compiler).
    static constexpr double TurnSettleSeconds = 0.5;
    static constexpr double OrientRateRadPerSec = 0.01;

    // ----- Burn gate on the ACHIEVABLE thrust error (hull error beyond the
    // nozzle cone)
    static constexpr double BurnGateFullRad = 2.0 * DegToRad;
    static constexpr double BurnGateLimitedRad = 10.0 * DegToRad;
    static constexpr double BurnRateGateRadPerSec = 0.12;
    // A planned burn is flyable when the planned hull is within the nozzle
    // cone plus the limited-burn gate for at least half of it.
    static constexpr double UnreachableSlackRad = BurnGateLimitedRad;

    // ----- Program shaping (compiler)
    // Gaps between burns shorter than this are flown as one burn.
    static constexpr double MinCoastSeconds = 1.0;
    // Burns shorter than this are RCS work.
    static constexpr double MinBurnSeconds = 0.4;
    // Below this speed the program ends in SETTLE.
    static constexpr double ProgramSettleSpeedMps = 0.5;
    static constexpr double LowSpeedRcsOnlyMps = 2.0;
    // A longitudinal change the RCS budget clears within this time is RCS
    // work.
    static constexpr double RcsDeltaVSeconds = 2.0;
    static constexpr double CurveNormalShare = 0.30;

    // ----- Longitudinal and lateral tracking (pilot)
    static constexpr double LongitudinalResponseSeconds = 2.0;
    // Safe tube: correction starts when the PREDICTED cross-track leaves it
    // and stops once the prediction is back well inside.
    static constexpr double LateralPredictionSeconds = 4.0;
    static constexpr double LateralResponseSeconds = 2.0;
    static constexpr double SafeTubeFraction = 0.35;
    static constexpr double TubeReleaseFraction = 0.20;
    static constexpr double TubeAimFraction = 0.10;
    static constexpr double WallMarginFraction = 0.85;
    static constexpr double DefaultCorridorToleranceMeters = 50.0;
    // Pilot settle speed (route end without a terminal target).
    static constexpr double PilotSettleSpeedMps = 2.0;
    // A braking feed-forward fades out below twice this along-track speed
    // once the craft is no faster than its profile.
    static constexpr double BrakeFadeMps = 3.0;
    static constexpr double SettleResponseSeconds = 2.5;

    // Attitude servo: rate error closed over this time; a calm pose's last
    // degrees approach linearly over FinalApproach (no limit cycle).
    static constexpr double RateResponseSeconds = 0.5;
    static constexpr double PoseFinalApproachSeconds = 1.0;

    // ----- Precision stop (no route): entered below this speed; residual
    // velocity trimmed over a second, a residual inside the deadband left
    // alone.
    static constexpr double PrecisionStopEntrySpeedMps = 0.50;
    static constexpr double PrecisionResponseSeconds = 1.0;
    static constexpr double PrecisionDeadbandMps = 0.05;

    // ----- Narrow zones
    // A narrow zone's attitude (incl. roll) is taken this long before the
    // entry; the attitude error held inside matches the planner's
    // narrowZoneAttitudeToleranceRad.
    static constexpr double NarrowAttitudeLeadSeconds = 12.0;
    static constexpr double NarrowAttitudeToleranceRad = 5.0 * DegToRad;
    static constexpr double NarrowApproachSpeedShare = 1.1;
    // Plateau before a narrow zone (compiler): the pilot's attitude lead
    // plus margin.
    static constexpr double NarrowApproachSeconds = NarrowAttitudeLeadSeconds + 3.0;

    // ----- Terminal sequence: entry speed, calm approach speed toward the
    // end point, and fallbacks when the route gives no tolerance.
    static constexpr double TerminalEntrySpeedMps = 2.0;
    static constexpr double TerminalApproachSpeedMps = 2.0;
    static constexpr double TerminalApproachResponseSeconds = 4.0;
    static constexpr double TerminalVelocityResponseSeconds = 2.0;
    static constexpr double TerminalDefaultPositionMeters = 3.0;
    static constexpr double TerminalDefaultSpeedMps = 0.75;
    static constexpr double TerminalDefaultAttitudeRad = 5.0 * DegToRad;
    static constexpr double TerminalDefaultRateRadPerSec = 0.05;
    // Leaving DONE (or a finished step) needs twice the tolerance.
    static constexpr double TerminalHysteresis = 2.0;
    // Inside this share of the tolerance the RCS trim does nothing.
    static constexpr double TerminalDeadbandShare = 0.5;
    // DONE: inside the position tolerance for at least this long at the
    // residual drift; RATE_SETTLE starts once the turn is this slow
    // (in units of the angular-rate tolerance).
    static constexpr double TerminalHoldSeconds = 60.0;
    static constexpr double TerminalSettleEntryShare = 4.0;
    // DONE only after the still/aligned/slow state held this long.
    static constexpr double TerminalDoneDwellSeconds = 1.0;
};

} // namespace game::navigation::autopilot
