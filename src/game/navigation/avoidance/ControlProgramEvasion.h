#pragma once

// EVASION BY THE PILOT'S CONTROLS on the REAL ship physics (author 2026-11-23: «надо задавать сети на выход - ручное
// управление кораблем ... это будет уже проблема физического слоя»). A control program = the pilot's inputs (RCS
// strafe / lift / forward, pitch / yaw, the speed command) held for holdSeconds, then hands off (the flight law flies
// on); its trajectory comes from the chain the game runs (SharedShipPhysics::integrate -> DynamicMotionSystem). The
// flights depend on the ship only: fly every program once per reflex tick, judge them against all threats.
// EliteControlEvasionLab (3000 aimed scenes): the point-mass reflex 69.6 % safe (Assisted) / 3.8 % (Newtonian), the
// best program here 94.8 % / 91.9 %.
//
// Frame: the ship's BODY at the start (+X right, +Y up, -Z nose) - the radar's sensor frame; the threats' tracks are
// relative to us, so their own motion in this frame = relative velocity + our velocity.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/navigation/avoidance/TrajectoryConflict.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/KinematicFrame.h"
#include "src/game/navigation/avoidance/MultiThreatEvasion.h"
#include "src/game/ship/control/ShipControlExecutionGateway.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipControlState.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

namespace game::navigation::avoidance
{
struct ControlProgram
{
    float strafe = 0, lift = 0, forward = 0, pitch = 0, yaw = 0, speed = 0;
    bool handsOff() const { return strafe == 0 && lift == 0 && forward == 0 && pitch == 0 && yaw == 0 && speed == 0; }
    // how much of the stick it uses (the least of the equally safe wins)
    float effort() const
    {
        return std::abs(strafe) + std::abs(lift) + std::abs(forward) + 0.5f * (std::abs(pitch) + std::abs(yaw)) +
               0.5f * std::abs(speed);
    }
};

// 27 RCS translations (index 13 = hands off) + 4 turns x 3 speed commands + the 2 speed commands alone = 41
inline const std::vector<ControlProgram>& controlPrograms()
{
    static const std::vector<ControlProgram> out = [] {
        std::vector<ControlProgram> p;
        for (int s = -1; s <= 1; ++s)
            for (int l = -1; l <= 1; ++l)
                for (int f = -1; f <= 1; ++f)
                    p.push_back({float(s), float(l), float(f), 0, 0, 0});
        const float turns[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (const auto& t : turns)
            for (int v = -1; v <= 1; ++v)
                p.push_back({0, 0, 0, t[0], t[1], float(v)});
        p.push_back({0, 0, 0, 0, 0, 1});
        p.push_back({0, 0, 0, 0, 0, -1});
        // gentle pushes along each axis (a small dodge is often enough - and stays inside the route's safe corridor)
        for (int ax = 0; ax < 3; ++ax)
            for (const float sgn : {-0.4f, 0.4f})
            {
                ControlProgram g;
                (ax == 0 ? g.strafe : ax == 1 ? g.lift : g.forward) = sgn;
                p.push_back(g);
            }
        return p;
    }();
    return out;
}
constexpr std::size_t ControlProgramHandsOff = 13;

struct ControlShipState
{
    ShipParams params;
    game::navigation::LocalFlightControlLaw law = game::navigation::LocalFlightControlLaw::Assisted;
    glm::dvec3 velocityBody {0.0};   // our velocity (the frame the threats move straight in), body axes
    double targetForwardSpeedMps = 0.0;
    glm::vec3 ratesPitchYawRoll {0.0f};
    // the consumer's RCS delta-v ownership (ShipControlState::rcsKeepsTranslationDeltaV): the Newtonian route keeps a
    // trim, the keypad releases it - the dodge is predicted with the flag it will be flown with (NAV-9)
    bool rcsKeepsTranslationDeltaV = false;
};

struct ControlFlight
{
    // our flight's states every SampleSeconds, the initial body frame. The samples only REPRESENT the non-linear path:
    // every interval between two of them is judged as continuous motion (TrajectoryConflict.h), the true path within
    // chordBound[k] of the chord of interval k (k -> k+1): a_max dt^2 / 8 with a_max the largest acceleration the
    // integration saw inside the interval
    std::vector<glm::dvec3> pos, vel;
    std::vector<double> chordBound;
    static constexpr double SampleSeconds = 0.5;
    double sampleSeconds = SampleSeconds;   // this flight's spacing (a test oracle stores every step)
};

inline ControlFlight flyControlProgram(const ControlShipState& st, const ControlProgram& pr, double holdSeconds,
                                       double horizonSeconds, double dt,
                                       double sampleSeconds = ControlFlight::SampleSeconds)
{
    ShipTransform tr {};
    tr.motion.mode = game::navigation::MotionMode::HubTactical;
    tr.motion.systemId = 1;
    tr.motion.localControlLaw = st.law;
    tr.motion.localPositionMeters = glm::dvec3(0.0);
    tr.motion.localVelocityMps = st.velocityBody;
    tr.motion.targetForwardSpeedMps = st.targetForwardSpeedMps;
    tr.pitchRate = st.ratesPitchYawRoll.x;
    tr.yawRate = st.ratesPitchYawRoll.y;
    tr.rollRate = st.ratesPitchYawRoll.z;
    tr.setWorldPositionMeters(glm::dvec3(0.0));
    tr.orientation = glm::mat4(1.0f);   // right +X, up +Y, nose -Z
    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "control-program-evasion";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;
    tr.motion.travelFrame = frame;
    WorldParams world {};
    ControlFlight out;
    const int perSample = std::max(1, static_cast<int>(std::lround(sampleSeconds / dt)));
    out.sampleSeconds = perSample * dt;
    const int steps = static_cast<int>(std::ceil(horizonSeconds / dt));
    const double sampleDt = perSample * dt;
    double intervalMaxAccel = 0.0;
    glm::dvec3 previousVelocity = tr.motion.localVelocityMps;
    for (int i = 0; i <= steps; ++i)
    {
        if (i % perSample == 0)
        {
            if (!out.pos.empty())
                out.chordBound.push_back(intervalMaxAccel * sampleDt * sampleDt / 8.0);
            intervalMaxAccel = 0.0;
            out.pos.push_back(tr.motion.localPositionMeters);
            out.vel.push_back(tr.motion.localVelocityMps);
        }
        if (i == steps)
            break;
        ShipControlState c {};
        if (i * dt < holdSeconds)
        {
            c.strafeInput = pr.strafe;
            c.liftInput = pr.lift;
            c.forwardInput = pr.forward;
            c.pitchInput = pr.pitch;
            c.yawInput = pr.yaw;
            c.targetSpeedRate = pr.speed;
        }
        // as the reflex's command is flown (NAV-9: the simulation went around the execution gateway - the per-axis RCS
        // and delta-v flags never reached the motion state, the main command was the raw rate): the same flags, the
        // same gateway (attitude via SharedShipPhysics, translation via the gateway's path)
        c.rcsTranslationPerAxisAuthority = true;
        c.rcsKeepsTranslationDeltaV = st.rcsKeepsTranslationDeltaV;
        c.cancelVelocityAlignment = true;
        game::ship::control::ShipControlExecutionGateway::integrateAttitude(tr, st.params, c, world, static_cast<float>(dt));
        (void)game::ship::control::ShipControlExecutionGateway::evaluateTranslationControl(tr, st.params, c,
                                                                                          static_cast<float>(dt));
        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(tr.motion, tr.worldPosition, frame, st.params, dt);
        intervalMaxAccel = std::max(intervalMaxAccel, glm::length(tr.motion.localVelocityMps - previousVelocity) / dt);
        previousVelocity = tr.motion.localVelocityMps;
    }
    return out;
}

// a flight against straight-moving threats (tracks relative to us at the start; their own motion = relative + ours).
// Any miss beats any touch; misses by the margin (capped at clearCap - just enough), touches by the softest impact
// along the contact normal (the glancing blow).
// reference / program: the cost of leaving the planned (collision-proven) corridor - the flight's largest departure from
// the hands-off flight beyond corridorMeters, and the stick effort (in-game 2026-11-23: a full-stick dodge with no such
// cost threw the ship ~1 km off its route into the city's buildings - the reflex does not see structures)
inline EvasionChoice judgeControlFlight(const ControlFlight& f, const EvasionScene& sc, const glm::dvec3& ourVelocity,
                                        double clearCapMeters = 30.0, const ControlFlight* reference = nullptr,
                                        const ControlProgram* program = nullptr, double corridorMeters = 40.0)
{
    // THE CONTINUOUS JUDGEMENT (TrajectoryConflict.h): every interval of our flight against every threat moving
    // straight; the safety clearance is the margin, a physical touch the contact (the EARLIEST one over all threats)
    EvasionChoice out;
    out.minMarginMeters = 1.0e30;
    SafetyEnvelopePolicy policy;
    policy.baseClearanceMeters = sc.marginMeters;
    const double dt = f.sampleSeconds;
    for (const auto& th : sc.threats)
    {
        const glm::dvec3 vAbs = th.velocity + ourVelocity;
        ConflictBody body;
        body.physicalRadiusMeters = th.radiusMeters;
        body.positionUncertaintyMeters = th.positionUncertaintyMeters;
        body.velocityUncertaintyMps = th.velocityUncertaintyMps;
        body.ageSeconds = th.ageSeconds;
        ConflictResult c;
        for (std::size_t k = 0; k + 1 < f.pos.size(); ++k)
        {
            const double t0 = static_cast<double>(k) * dt;
            const double e = k < f.chordBound.size() ? f.chordBound[k] : chordDeviationBound(f.vel[k], f.vel[k + 1], dt);
            accumulateConflictInterval(c, t0, dt, f.pos[k], f.pos[k + 1], e, th.position + vAbs * t0, vAbs,
                                       sc.caps.radiusMeters, body, policy);
        }
        if (c.safetyClearanceMeters < out.minMarginMeters)
        {
            out.minMarginMeters = c.safetyClearanceMeters;
            out.timeOfMinSeconds = c.timeOfSafetyMinimumSeconds;
        }
        if (c.physicalCollisionPredicted && (!out.contact || c.firstContactSeconds < out.contactTimeSeconds))
        {
            out.contact = true;
            out.contactTimeSeconds = c.firstContactSeconds;
            out.impactNormalSpeedMps = c.impactNormalSpeedMps;
            out.impactSpeedMps = c.impactSpeedMps;
        }
    }
    if (sc.threats.empty())
        out.minMarginMeters = 1.0e6;
    out.score = out.contact ? -1000.0 - 5.0 * out.impactNormalSpeedMps : std::min(out.minMarginMeters, clearCapMeters);
    if (reference)
    {
        double departure = 0.0;
        for (std::size_t k = 0; k < f.pos.size() && k < reference->pos.size(); ++k)
            departure = std::max(departure, glm::length(f.pos[k] - reference->pos[k]));
        out.score -= 0.5 * std::max(0.0, departure - corridorMeters);
    }
    if (program)
        out.score -= 2.0 * static_cast<double>(program->effort());
    return out;
}
} // namespace game::navigation::avoidance
