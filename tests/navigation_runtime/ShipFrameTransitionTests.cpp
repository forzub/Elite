// Core stabilization D-1: a frame change of an existing ship changes only the description of its motion.
// Contract E-8 (PROJECT_STATE.md): world position, velocity, attitude and angular rates are invariant; a failed
// transition changes nothing; frame-relative setpoints are re-based to what the ship already does.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/simulation/ShipFrameTransition.h"

using game::navigation::KinematicFrame;
using game::simulation::FrameTransitionRequest;
using game::simulation::currentWorldKinematics;
using game::simulation::transferShipIntoFrame;

namespace
{
int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

double dist(const glm::dvec3& a, const glm::dvec3& b) { return glm::length(a - b); }

// an orbiting hub: far from the origin, ~7.7 km/s, LVLH-like axes turning at the orbital rate, plus an accelerating
// origin; and a ring deck: turning fast (1 g at 1210 m)
KinematicFrame makeFrame(int systemId, const glm::dvec3& origin, const glm::dvec3& velocity, double omega,
                         const glm::dvec3& axis, double basisAngle)
{
    KinematicFrame f;
    f.systemId = systemId;
    f.frameId = "hub";
    f.originMeters = origin;
    f.linearVelocityMps = velocity;
    f.linearAccelerationMps2 = glm::dvec3(-8.6, 0.3, 0.0);
    f.localToWorldBasis = glm::dmat3(glm::rotate(glm::dmat4(1.0), basisAngle, glm::normalize(axis)));
    f.angularVelocityWorldRadPerSecond = glm::normalize(axis) * omega;
    f.valid = true;
    return f;
}

ShipTransform makeShipInFreeFrame(int systemId)
{
    ShipTransform tr;
    // a non-rotating travel frame the ship flies in (as after Ctrl+J: re-based at the ship)
    KinematicFrame own;
    own.systemId = systemId;
    own.frameId = "ship_free_7";
    own.originMeters = glm::dvec3(6.9e6, 1.2e5, -3.0e4);
    own.linearVelocityMps = glm::dvec3(120.0, 7650.0, 15.0);
    own.valid = true;
    tr.motion.systemId = systemId;
    tr.motion.travelFrame = own;
    tr.motion.localPositionMeters = glm::dvec3(40.0, -12.0, 300.0);
    tr.motion.localVelocityMps = glm::dvec3(35.0, -210.0, 4.0);
    const auto w = currentWorldKinematics(tr);
    tr.setWorldPositionMeters(w.positionMeters);
    tr.motion.worldVelocityMps = w.velocityMps;
    tr.orientation = glm::rotate(glm::mat4(1.0f), 0.7f, glm::normalize(glm::vec3(0.2f, 1.0f, -0.4f)));
    tr.pitchRate = 0.11f;
    tr.yawRate = -0.07f;
    tr.rollRate = 0.31f;
    tr.motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::ForwardToVelocity;
    tr.motion.localControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    tr.motion.assistedTargetSpeedHold = true;
    tr.motion.targetForwardSpeedMps = 250.0;   // relative to the OLD frame
    return tr;
}

void invariantCase(const char* name, const KinematicFrame& dest)
{
    ShipTransform tr = makeShipInFreeFrame(dest.systemId);
    const auto before = currentWorldKinematics(tr);
    const glm::mat4 orientation = tr.orientation;
    const float pr = tr.pitchRate, yr = tr.yawRate, rr = tr.rollRate;

    FrameTransitionRequest rq;
    rq.destination = dest;
    rq.ownedFrameId = "ship_travel_7";
    rq.referenceFrameId = "hub_a";
    rq.hubId = "hub_a";
    char what[160];
    std::snprintf(what, sizeof what, "%s: transfer succeeds", name);
    check(transferShipIntoFrame(tr, rq), what);

    const auto after = currentWorldKinematics(tr);
    std::snprintf(what, sizeof what, "%s: world position invariant (%.3g m)", name,
                  dist(before.positionMeters, after.positionMeters));
    check(dist(before.positionMeters, after.positionMeters) < 1.0e-6, what);
    std::snprintf(what, sizeof what, "%s: world velocity invariant (%.3g m/s)", name,
                  dist(before.velocityMps, after.velocityMps));
    check(dist(before.velocityMps, after.velocityMps) < 1.0e-9 * std::max(1.0, glm::length(before.velocityMps)), what);
    std::snprintf(what, sizeof what, "%s: stored world point agrees with the frame", name);
    check(dist(tr.fullWorldMeters(), after.positionMeters) < 1.0e-3, what);
    std::snprintf(what, sizeof what, "%s: stored world velocity agrees with the frame", name);
    check(dist(tr.motion.worldVelocityMps, after.velocityMps) < 1.0e-9 * glm::length(after.velocityMps), what);
    std::snprintf(what, sizeof what, "%s: attitude and angular rates kept", name);
    check(std::memcmp(&orientation, &tr.orientation, sizeof orientation) == 0 && tr.pitchRate == pr &&
              tr.yawRate == yr && tr.rollRate == rr,
          what);
    std::snprintf(what, sizeof what, "%s: alignment mode and control law kept", name);
    check(tr.motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::ForwardToVelocity &&
              tr.motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted,
          what);
    std::snprintf(what, sizeof what, "%s: frame matched, hub-tactical, travel frame owned", name);
    check(tr.motion.matchedToReferenceFrame && tr.motion.matchedReferenceFrameId == "hub_a" &&
              tr.motion.travelFrame.frameId == "ship_travel_7" &&
              tr.motion.mode == game::navigation::MotionMode::HubTactical && tr.motion.hubId == "hub_a",
          what);
    // the Assisted setpoint now means the speed the ship already has against the NEW frame (no unprovoked burn)
    const glm::dvec3 relative = dest.localToWorldVector(tr.motion.localVelocityMps);
    const double forwardNow = glm::dot(relative, glm::dvec3(tr.forward()));
    std::snprintf(what, sizeof what, "%s: Assisted setpoint re-based (%.3f vs %.3f)", name,
                  tr.motion.targetForwardSpeedMps, forwardNow);
    check(!tr.motion.assistedTargetSpeedHold && std::abs(tr.motion.targetForwardSpeedMps - forwardNow) < 1.0e-9 &&
              std::abs(tr.motion.forwardSpeedMps - forwardNow) < 1.0e-9,
          what);
    std::snprintf(what, sizeof what, "%s: reference velocity = the frame's velocity at the ship", name);
    check(dist(tr.motion.referenceVelocityMps, dest.localToWorldVelocity(tr.motion.localPositionMeters, glm::dvec3(0.0))) <
              1.0e-9,
          what);
}

bool sameBytes(const ShipTransform& a, const ShipTransform& b)
{
    // the comparable physical / control fields (strings compared by value)
    const auto& m = a.motion;
    const auto& n = b.motion;
    return std::memcmp(&a.orientation, &b.orientation, sizeof a.orientation) == 0 &&
           a.fullWorldMeters() == b.fullWorldMeters() &&
           m.localPositionMeters == n.localPositionMeters && m.localVelocityMps == n.localVelocityMps &&
           m.worldVelocityMps == n.worldVelocityMps && m.referenceVelocityMps == n.referenceVelocityMps &&
           m.travelFrame.frameId == n.travelFrame.frameId && m.travelFrame.originMeters == n.travelFrame.originMeters &&
           m.matchedToReferenceFrame == n.matchedToReferenceFrame && m.hubId == n.hubId && m.mode == n.mode &&
           m.targetForwardSpeedMps == n.targetForwardSpeedMps && m.assistedTargetSpeedHold == n.assistedTargetSpeedHold &&
           m.systemId == n.systemId;
}

void failureCase(const char* name, FrameTransitionRequest rq, const ShipTransform& start)
{
    ShipTransform tr = start;
    char what[160];
    std::snprintf(what, sizeof what, "%s: refused", name);
    check(!transferShipIntoFrame(tr, rq), what);
    std::snprintf(what, sizeof what, "%s: state untouched", name);
    check(sameBytes(tr, start), what);
}
} // namespace

int main()
{
    const int sys = 3;
    // an orbiting hub frame (LEO rate) and a ring deck (1 g at 1210 m: 0.09 rad/s), with an offset basis
    invariantCase("orbital hub",
                  makeFrame(sys, glm::dvec3(6.9e6, 1.0e5, 0.0), glm::dvec3(0.0, 7660.0, 0.0), 1.13e-3,
                            glm::dvec3(0.0, 0.0, 1.0), 0.4));
    invariantCase("ring deck",
                  makeFrame(sys, glm::dvec3(6.9e6, 1.2e5, -3.0e4), glm::dvec3(0.0, 7660.0, 0.0), 0.0900,
                            glm::dvec3(0.3, 1.0, 0.1), -1.1));
    invariantCase("non-rotating", makeFrame(sys, glm::dvec3(1.0e3, 0.0, 0.0), glm::dvec3(0.0), 0.0,
                                            glm::dvec3(0.0, 1.0, 0.0), 0.0));

    // a ship outside any frame (world fields authoritative) keeps its world state too
    {
        ShipTransform tr = makeShipInFreeFrame(sys);
        tr.motion.travelFrame.valid = false;
        const auto before = currentWorldKinematics(tr);
        FrameTransitionRequest rq;
        rq.destination = makeFrame(sys, glm::dvec3(6.9e6, 1.0e5, 0.0), glm::dvec3(0.0, 7660.0, 0.0), 1.13e-3,
                                   glm::dvec3(0.0, 0.0, 1.0), 0.4);
        rq.ownedFrameId = "ship_travel_7";
        rq.referenceFrameId = rq.hubId = "hub_a";
        check(transferShipIntoFrame(tr, rq), "no frame: transfer succeeds");
        const auto after = currentWorldKinematics(tr);
        check(dist(before.positionMeters, after.positionMeters) < 1.0e-6 &&
                  dist(before.velocityMps, after.velocityMps) < 1.0e-6,
              "no frame: world state invariant");
    }

    // failures change nothing
    {
        const ShipTransform start = makeShipInFreeFrame(sys);
        FrameTransitionRequest good;
        good.destination = makeFrame(sys, glm::dvec3(6.9e6, 1.0e5, 0.0), glm::dvec3(0.0, 7660.0, 0.0), 1.13e-3,
                                     glm::dvec3(0.0, 0.0, 1.0), 0.4);
        good.ownedFrameId = "ship_travel_7";
        good.referenceFrameId = good.hubId = "hub_a";

        FrameTransitionRequest invalid = good;
        invalid.destination.valid = false;
        failureCase("invalid frame", invalid, start);

        FrameTransitionRequest otherSystem = good;
        otherSystem.destination.systemId = sys + 1;
        failureCase("another system", otherSystem, start);

        FrameTransitionRequest noId = good;
        noId.ownedFrameId.clear();
        failureCase("no owned frame id", noId, start);

        FrameTransitionRequest nanFrame = good;
        nanFrame.destination.originMeters.x = std::numeric_limits<double>::quiet_NaN();
        failureCase("non-finite frame", nanFrame, start);

        game::navigation::WorldKinematicState nanWorld;
        nanWorld.positionMeters = glm::dvec3(1.0, std::numeric_limits<double>::infinity(), 0.0);
        FrameTransitionRequest nanState = good;
        nanState.worldOverride = &nanWorld;
        failureCase("non-finite state", nanState, start);
    }

    // a materialized ship: its world state comes from the override, carried exactly
    {
        ShipTransform tr;
        tr.motion.systemId = sys;
        game::navigation::WorldKinematicState w;
        w.positionMeters = glm::dvec3(6.9e6 + 800.0, 1.0e5 - 50.0, 20.0);
        w.velocityMps = glm::dvec3(-30.0, 7700.0, 2.0);
        FrameTransitionRequest rq;
        rq.destination = makeFrame(sys, glm::dvec3(6.9e6, 1.0e5, 0.0), glm::dvec3(0.0, 7660.0, 0.0), 0.0900,
                                   glm::dvec3(0.0, 1.0, 0.0), 0.2);
        rq.ownedFrameId = "ship_travel_9";
        rq.referenceFrameId = rq.hubId = "hub_a";
        rq.worldOverride = &w;
        check(transferShipIntoFrame(tr, rq), "override: transfer succeeds");
        const auto after = currentWorldKinematics(tr);
        check(dist(after.positionMeters, w.positionMeters) < 1.0e-6 && dist(after.velocityMps, w.velocityMps) < 1.0e-9 * 7700.0,
              "override: world state carried exactly through a turning frame");
    }

    if (g_failures == 0)
        std::printf("SHIP FRAME TRANSITION: PASS\n");
    return g_failures == 0 ? 0 : 1;
}
