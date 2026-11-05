#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation::landing
{
/*
    The landing virtual pilot of NORMAL flight (author 2026-11-05: the J field ends on the
    parking orbit; the de-orbit and the landing are flown in the ordinary flight mode). It
    flies the ship's NATIVE law (its physical control model: an Assisted plant cannot execute
    Newtonian main-engine commands - a forced Newtonian law delivered ~0 of 73 m/s2) and
    emits only that law's ordinary controls - never a velocity or a position. The ship's
    flight law executes them with the installed actuators; gravity is physics, not a command.

    Assisted (flySurfaceLandingAssisted): the hull level, belly down; the nose along the way
    (against it while fast, unless a reverse engine brakes nose-first); the target speed
    along the nose follows the wanted velocity through '+'/'-' (targetSpeedRate); the lift
    input holds the wanted vertical speed against gravity (the Assisted vectoring main, the
    law yields that axis to the input); the law's stabilizer removes the sideways drift.

    Newtonian (flySurfaceLanding) - hull attitude inputs (the hull's own angular authority
    and rate limits), the main-engine command ('+', 0..1, along the nozzle), per-axis RCS:
      wanted acceleration a = (v_wanted - v) / tau - g   (v over the ground, g the body's pull)
      belly down (the ventral RCS holds the weight): the hull level, the nose against the
        horizontal motion while it is fast (the main engine brakes), else along the heading;
        a weight beyond the ventral RCS pitches the nose up so the main engine takes the rest;
      tail down (no ventral authority): the nose toward a - the main engine carries all.
      The main engine takes the nose component beyond the forward RCS; the RCS trims the rest.
*/
struct SurfacePilotRequest
{
    glm::dvec3 desiredVelocityMps {0.0};   // over the ground (body-centred, non-rotating axes)
    glm::dvec3 actualVelocityMps {0.0};
    glm::dvec3 gravityMps2 {0.0};          // the body's pull on the ship
    glm::dvec3 upLocal {0.0, 1.0, 0.0};    // away from the body centre
    glm::dvec3 headingHint {0.0};          // horizontal heading when slow (zero = keep the nose)
    glm::dvec3 forward {0.0, 0.0, -1.0};
    glm::dvec3 right {1.0, 0.0, 0.0};
    glm::dvec3 up {0.0, 1.0, 0.0};
    double pitchRateRadPerSec = 0.0;
    double yawRateRadPerSec = 0.0;
    double rollRateRadPerSec = 0.0;
    bool bellyDown = false;
    double dt = 0.02;
};

struct SurfacePilotResult
{
    ShipControlState control;
    glm::dvec3 wantedAccelerationMps2 {0.0};
    glm::dvec3 noseWanted {0.0};
    double mainCommand = 0.0;
};

inline double surfaceMainEngineAccelMps2(const ShipParams& p)
{
    return p.forwardMainEngineAccelerationMps2 > 0.0f ? static_cast<double>(p.forwardMainEngineAccelerationMps2)
                                                      : game::ship::mainAccelerationLimitMps2(p);
}

// RCS authority of a body direction: axis 0 right, 1 up, 2 forward (nose); sign of `component`
inline double surfaceRcsMps2(const ShipParams& p, int axis, double component)
{
    return std::max(0.0, game::ship::newtonianRcsDirectionAccelerationMps2(p, axis, component));
}

// the hull pose (nose, up) through the ordinary attitude inputs within the hull's authority
inline void surfaceAttitudeInputs(const SurfacePilotRequest& rq, const ShipParams& p, const glm::dvec3& nose, glm::dvec3 hullUp,
                                  ShipControlState& control)
{
    using game::navigation::autopilot::HullAttitudeControl;
    using game::navigation::autopilot::HullPoseGuidance;
    hullUp = hullUp - nose * glm::dot(hullUp, nose);
    if (glm::length(hullUp) < 1.0e-3)
        hullUp = rq.up - nose * glm::dot(rq.up, nose);
    hullUp = glm::length(hullUp) > 1.0e-6 ? glm::normalize(hullUp) : rq.up;
    HullPoseGuidance::Request pose;
    pose.currentForward = rq.forward;
    pose.currentRight = rq.right;
    pose.currentUp = rq.up;
    pose.targetForward = nose;
    pose.targetUp = hullUp;
    const auto poseResult = HullPoseGuidance::evaluate(pose);
    if (!poseResult.valid)
        return;
    HullAttitudeControl::Request att;
    att.pitchYawErrorRad = {poseResult.pitchYawErrorLocalRad.x, poseResult.pitchYawErrorLocalRad.y};
    att.rollErrorRad = poseResult.rollErrorRad;
    att.pitchYawRateRadPerSec = {rq.pitchRateRadPerSec, rq.yawRateRadPerSec};
    att.rollRateRadPerSec = rq.rollRateRadPerSec;
    att.maxPitchYawRateRadPerSec = {std::max(0.0, static_cast<double>(p.maxPitchRate)),
                                    std::max(0.0, static_cast<double>(p.maxYawRate))};
    att.maxRollRateRadPerSec = std::max(0.0, static_cast<double>(p.maxRollRate));
    const double authority = std::max(1.0e-9, game::ship::angularAccelerationLimitRadPerSec2(p));
    att.pitchYawAngularAccelerationAuthorityRadPerSec2 = authority;
    att.rollAngularAccelerationAuthorityRadPerSec2 = authority;
    att.deltaSeconds = std::max(rq.dt, 1.0e-4);
    att.pitchYawResponseSeconds = 0.5;
    att.rollResponseSeconds = 0.5;
    const auto r = HullAttitudeControl::evaluate(att);
    if (!r.valid)
        return;
    control.pitchInput = static_cast<float>(r.pitchYawInput.x);
    control.yawInput = static_cast<float>(r.pitchYawInput.y);
    control.rollInput = static_cast<float>(r.rollInput);
}

// Assisted: the ship's own Assisted law flies it (see the file comment). targetForwardSpeed:
// the law's current target VREL along the nose (the ship's DynamicMotionState).
inline SurfacePilotResult flySurfaceLandingAssisted(const SurfacePilotRequest& rq, const ShipParams& p,
                                                    double targetForwardSpeedMps)
{
    SurfacePilotResult out;
    const glm::dvec3 upL = glm::normalize(rq.upLocal);
    const glm::dvec3 v = rq.actualVelocityMps;
    // normal flight: the wanted velocity stays inside the controlled envelope (90 %). At the
    // envelope the law admits no thrust that grows the speed - "faster along the nose" plus
    // the lift against gravity was cut to zero and the ship fell (run 5, 2026-11-05)
    glm::dvec3 vd = rq.desiredVelocityMps;
    {
        const double cap = 0.9 * game::ship::controlledSpeedLimitMps(p);
        if (cap > 0.0 && glm::length(vd) > cap)
            vd *= cap / glm::length(vd);
    }
    // the hull level, the nose along the way - against it while fast (the aft main brakes)
    glm::dvec3 heading = rq.headingHint - upL * glm::dot(rq.headingHint, upL);
    if (glm::length(heading) < 1.0e-6)
        heading = rq.forward - upL * glm::dot(rq.forward, upL);
    if (glm::length(heading) < 1.0e-6)
        heading = rq.up - upL * glm::dot(rq.up, upL);
    heading = glm::normalize(heading);
    // the nose along the horizontal velocity change still needed (the aft main delivers it
    // along the nose), blending into the way to the site as that change vanishes: no
    // threshold flip (a retrograde / prograde switch at 30 m/s swung the hull ~110 deg every
    // two seconds and kept the horizontal speed at 20 - 75 m/s)
    const glm::dvec3 changeH = (vd - v) - upL * glm::dot(vd - v, upL);
    const double change = glm::length(changeH);
    if (change > 1.0e-6)
    {
        const double t = std::clamp((change - 1.0) / 4.0, 0.0, 1.0);
        const double w = t * t * (3.0 - 2.0 * t);
        glm::dvec3 along = changeH / change;
        if (p.reverseMainEngineAvailable && glm::dot(along, heading) < 0.0)
            along = -along;   // a reverse engine brakes nose-first
        const glm::dvec3 blended = along * w + heading * (1.0 - w);
        if (glm::length(blended) > 1.0e-6)
            heading = glm::normalize(blended);
    }
    out.noseWanted = heading;
    surfaceAttitudeInputs(rq, p, heading, upL, out.control);

    // along the nose: the law's target VREL follows the wanted velocity ('+' / '-')
    const double maxSpeed = game::ship::controlledSpeedLimitMps(p);
    const double changeRate = std::max(static_cast<double>(p.assistedMinimumTargetSpeedChangeRateMps2),
                                       maxSpeed * static_cast<double>(p.assistedTargetSpeedChangeRateFractionPerSecond));
    const double wantedAlongNose = glm::dot(vd, rq.forward);
    out.control.targetSpeedRate =
        static_cast<float>(std::clamp((wantedAlongNose - targetForwardSpeedMps) / std::max(1.0, changeRate * 0.25), -1.0, 1.0));

    // vertical: the lift input holds the wanted vertical speed against gravity (the law
    // yields the hull-up axis to it); only as far as the hull is level
    const double lift = std::max(1.0e-6, game::ship::assistedLateralStabilizationAccelerationLimitMps2(p));
    const double aUp = glm::dot(vd - v, upL) / 1.0 - glm::dot(rq.gravityMps2, upL);
    const double level = std::max(0.0, glm::dot(rq.up, upL));
    out.control.liftInput = static_cast<float>(std::clamp(aUp * level / lift, -1.0, 1.0));
    out.wantedAccelerationMps2 = upL * aUp;
    out.mainCommand = out.control.targetSpeedRate;
    return out;
}

inline SurfacePilotResult flySurfaceLanding(const SurfacePilotRequest& rq, const ShipParams& p)
{
    SurfacePilotResult out;
    // vertical error in 1 s, horizontal in 3 s: a steady thrust direction the hull can follow
    // (a 1 s horizontal loop swung the wanted direction faster than the hull turns)
    constexpr double VerticalTauS = 1.0;
    constexpr double HorizontalTauS = 3.0;
    const double dt = std::max(rq.dt, 1.0e-4);
    const glm::dvec3 upL = glm::normalize(rq.upLocal);
    const glm::dvec3 error = rq.desiredVelocityMps - rq.actualVelocityMps;
    const glm::dvec3 errorV = upL * glm::dot(error, upL);
    const glm::dvec3 a = errorV / VerticalTauS + (error - errorV) / HorizontalTauS - rq.gravityMps2;
    out.wantedAccelerationMps2 = a;
    const double mainAccel = std::max(1.0e-3, surfaceMainEngineAccelMps2(p));

    // ---- attitude
    glm::dvec3 heading = rq.headingHint - upL * glm::dot(rq.headingHint, upL);
    if (glm::length(heading) < 1.0e-6)
        heading = rq.forward - upL * glm::dot(rq.forward, upL);
    if (glm::length(heading) < 1.0e-6)
        heading = rq.up - upL * glm::dot(rq.up, upL);
    heading = glm::normalize(heading);
    glm::dvec3 nose, hullUp;
    if (rq.bellyDown)
    {
        const glm::dvec3 vh = rq.actualVelocityMps - upL * glm::dot(rq.actualVelocityMps, upL);
        if (glm::length(vh) > 30.0)
            heading = p.reverseMainEngineAvailable ? glm::normalize(vh) : -glm::normalize(vh);
        const double ventral = surfaceRcsMps2(p, 1, 1.0);
        const double deficit = std::max(0.0, glm::dot(a, upL) - ventral);
        const double pitch = std::min(0.7, std::asin(std::clamp(deficit / mainAccel, 0.0, 1.0)));
        nose = glm::normalize(heading * std::cos(pitch) + upL * std::sin(pitch));
        hullUp = upL;
    }
    else
    {
        // tail down: roll is free (the hull's own up) - a roll target from the projected
        // heading is ill-defined with the nose near vertical and kept pitch / yaw ~17 deg off
        nose = glm::length(a) > 0.05 ? glm::normalize(a) : upL;
        hullUp = rq.up;
    }
    out.noseWanted = nose;
    surfaceAttitudeInputs(rq, p, nose, hullUp, out.control);


    // ---- translation: the main engine beyond the forward RCS, the RCS the rest
    const double aNose = glm::dot(a, rq.forward);
    const double rcsForward = surfaceRcsMps2(p, 2, 1.0);
    double main = 0.0;
    if (aNose > rcsForward)
    {
        // the main engine fires along the hull: only once the hull points the thrust it needs
        // (full within 8 deg, none beyond 25 deg) - thrust 50 deg off made the error it chased
        const double aLen = glm::length(a);
        const double cosOff = aLen > 1.0e-6 ? glm::dot(a / aLen, rq.forward) : 1.0;
        const double c25 = std::cos(25.0 * 3.14159265358979 / 180.0), c8 = std::cos(8.0 * 3.14159265358979 / 180.0);
        const double t = std::clamp((cosOff - c25) / (c8 - c25), 0.0, 1.0);
        main = std::clamp(aNose / mainAccel, 0.0, 1.0) * (t * t * (3.0 - 2.0 * t));
    }
    const glm::dvec3 rest = a - rq.forward * (main * mainAccel);
    const auto axisInput = [&](const glm::dvec3& axis, int index) {
        const double c = glm::dot(rest, axis);
        const double auth = surfaceRcsMps2(p, index, c);
        return auth > 1.0e-9 ? static_cast<float>(std::clamp(c / auth, -1.0, 1.0)) : 0.0f;
    };
    out.control.strafeInput = axisInput(rq.right, 0);
    out.control.liftInput = axisInput(rq.up, 1);
    out.control.forwardInput = main > 0.0 ? 0.0f : axisInput(rq.forward, 2);
    out.control.targetSpeedRate = static_cast<float>(main);
    out.mainCommand = main;
    out.control.rcsTranslationPerAxisAuthority = true;
    out.control.rcsKeepsTranslationDeltaV = true;
    return out;
}
} // namespace game::navigation::landing
