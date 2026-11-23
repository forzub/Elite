#pragma once

// THE REFLEX (navigator layer 5, step 3): emergency evasion from RADAR CONTACTS while the autopilot flies.
// Every `periodSeconds` the radar's tracks (sensor = hull frame: +X right, +Y up, -Z nose; relative position and
// velocity at the scan's epoch) are extrapolated to now and judged by the evasion score of "hold the course": when a
// ship / debris track comes inside the safe sphere within the horizon, the teacher (MultiThreatEvasion.h) picks the
// manoeuvre and the reflex holds it as RCS translation (the pilot's own keypad controls - the Assisted law yields a
// manually translated axis); when the course is clear again it lets go and the autopilot takes the ship back to its
// route. RCS only (the main drive would need the nose, which the route tracker owns): plausible - thrusters dodge,
// the engines keep the course. Structures are the planner's (they do not move), not the reflex's.
#include <chrono>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/equipment/radar/RadarSensorTypes.h"
#include "src/game/navigation/avoidance/ControlProgramEvasion.h"
#include "src/game/navigation/avoidance/EvasionNet.h"

namespace game::navigation::avoidance
{
struct EvasionReflexCommand
{
    bool active = false;
    float strafe = 0.0f, lift = 0.0f, forward = 0.0f;   // per-axis RCS authority, -1..1
    float pitch = 0.0f, yaw = 0.0f, speedRate = 0.0f;    // the control-program reflex also turns and commands speed
    int program = -1;                                    // the control program held (-1: the point-mass reflex)
    double marginMeters = 0.0;                          // the margin of the chosen manoeuvre
    double courseMarginMeters = 0.0;                    // the margin when holding the course
    int threats = 0;
    bool fromNet = false;
};

class EvasionReflex
{
public:
    double periodSeconds = 0.25;
    double horizonSeconds = 12.0;
    double holdSeconds = 4.0;
    double marginMeters = 25.0;
    double lingerSeconds = 1.0;   // keep the dodge this long after the course is clear again (no flicker)
    const EvasionNet* net = nullptr;   // the classifier (289 outputs): its top 12 judged by the teacher's score, else the teacher

    // radar: the latest scan; now: universe time; caps: the hull (RCS lateral accel, radius)
    const EvasionReflexCommand& update(const radar::RadarScanReport& scan, double now, double lateralAccelMps2,
                                       double hullRadiusMeters)
    {
        if (now < m_nextAt)
            return m_cmd;
        m_nextAt = now + periodSeconds;
        EvasionScene sc = sceneFrom(scan, now, hullRadiusMeters);
        sc.caps.lateralAccelMps2 = std::max(0.05, lateralAccelMps2);
        sc.caps.mainAccelMps2 = sc.caps.lateralAccelMps2;   // RCS only: the main drive never beats it (no turn)
        sc.caps.turnRateRadPerS = 0.3;
        m_cmd.threats = static_cast<int>(sc.threats.size());
        if (sc.threats.empty())
            return release(now);
        const EvasionChoice course = scoreEvasion(sc, glm::dvec3(0.0), 0.0);
        m_cmd.courseMarginMeters = course.minMarginMeters;
        if (course.minMarginMeters >= 0.0)
            return release(now);
        bool fromNet = false;
        const EvasionChoice best = net && net->ready() && static_cast<int>(net->outputs()) == EvasionClassCount
                                       ? proposeEvasionTopK(sc, *net, 12, &fromNet)
                                       : teachEvasion(sc);
        m_cmd.fromNet = fromNet;
        m_clearSince = -1.0;
        const glm::dvec3 a = best.accel / sc.caps.lateralAccelMps2;
        m_cmd.active = glm::length(a) > 1.0e-3;
        m_cmd.strafe = static_cast<float>(std::clamp(a.x, -1.0, 1.0));
        m_cmd.lift = static_cast<float>(std::clamp(a.y, -1.0, 1.0));
        m_cmd.forward = static_cast<float>(std::clamp(-a.z, -1.0, 1.0));
        m_cmd.marginMeters = best.minMarginMeters;
        return m_cmd;
    }
    // THE CONTROL-PROGRAM REFLEX (ControlProgramEvasion.h): every program flown on the ship's real physics from its
    // state now, judged against the radar's tracks; hands off when "hands off" is clear. The program held keeps the
    // lead unless another one is clearly better (no flicker between equals).
    double programDtSeconds = 0.1;
    double programHoldSeconds = 2.0;   // re-chosen 4 times a second anyway; a 4 s full push moved ~1 km
    double lastProgramMs = 0.0;
    const EvasionReflexCommand& updatePrograms(const radar::RadarScanReport& scan, double now, const ControlShipState& ship,
                                               double hullRadiusMeters)
    {
        if (now < m_nextAt)
            return m_cmd;
        m_nextAt = now + periodSeconds;
        EvasionScene sc = sceneFrom(scan, now, hullRadiusMeters);
        m_cmd.threats = static_cast<int>(sc.threats.size());
        if (sc.threats.empty())
            return release(now);
        const auto& progs = controlPrograms();
        const ControlFlight handsOff = flyControlProgram(ship, progs[ControlProgramHandsOff], programHoldSeconds,
                                                         horizonSeconds, programDtSeconds);
        const EvasionChoice course = judgeControlFlight(handsOff, sc, ship.velocityBody);
        m_cmd.courseMarginMeters = course.contact ? -1.0e3 : course.minMarginMeters;
        if (!course.contact && course.minMarginMeters >= 0.0)
            return release(now);
        const auto t0 = std::chrono::steady_clock::now();
        int best = static_cast<int>(ControlProgramHandsOff);
        EvasionChoice bestChoice = course;
        double heldScore = -1.0e30;
        for (std::size_t i = 0; i < progs.size(); ++i)
        {
            if (i == ControlProgramHandsOff)
                continue;
            const EvasionChoice c = judgeControlFlight(flyControlProgram(ship, progs[i], programHoldSeconds, horizonSeconds,
                                                                         programDtSeconds),
                                                       sc, ship.velocityBody, 30.0, &handsOff, &progs[i]);
            if (static_cast<int>(i) == m_cmd.program)
                heldScore = c.score;
            if (c.score > bestChoice.score)
            {
                bestChoice = c;
                best = static_cast<int>(i);
            }
        }
        lastProgramMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        if (m_cmd.program >= 0 && heldScore >= bestChoice.score - 5.0)
            best = m_cmd.program;   // hysteresis: keep the one we fly unless another is 5 points better
        m_clearSince = -1.0;
        const ControlProgram& p = progs[static_cast<std::size_t>(best)];
        m_cmd.active = !p.handsOff();
        m_cmd.program = m_cmd.active ? best : -1;
        m_cmd.strafe = p.strafe;
        m_cmd.lift = p.lift;
        m_cmd.forward = p.forward;
        m_cmd.pitch = p.pitch;
        m_cmd.yaw = p.yaw;
        m_cmd.speedRate = p.speed;
        m_cmd.marginMeters = bestChoice.contact ? -1.0e3 - bestChoice.impactNormalSpeedMps : bestChoice.minMarginMeters;
        m_cmd.fromNet = false;
        return m_cmd;
    }

    const EvasionReflexCommand& command() const { return m_cmd; }
    void reset()
    {
        m_cmd = {};
        m_nextAt = 0.0;
        m_clearSince = -1.0;
    }

private:
    EvasionScene sceneFrom(const radar::RadarScanReport& scan, double now, double hullRadiusMeters) const
    {
        EvasionScene sc;
        sc.forward = glm::dvec3(0.0, 0.0, -1.0);
        sc.caps.radiusMeters = std::max(5.0, hullRadiusMeters);
        sc.horizonSeconds = horizonSeconds;
        sc.holdSeconds = holdSeconds;
        sc.marginMeters = marginMeters;
        sc.clearCapMeters = 30.0;
        const double age = std::max(0.0, now - scan.measuredAtUniverseTimeSeconds);
        for (const auto& t : scan.tracks)
        {
            if (t.kind == radar::RadarTrackKindStructure)
                continue;
            EvasionThreatTrack th;
            th.position = t.relativePositionMeters + t.relativeVelocityMps * age;
            th.velocity = t.relativeVelocityMps;
            th.radiusMeters = t.kind == radar::RadarTrackKindDebris ? 10.0 : 30.0;
            // only what can reach us within the horizon
            if (glm::length(th.position) - glm::length(th.velocity) * horizonSeconds > 400.0)
                continue;
            sc.threats.push_back(th);
        }
        return sc;
    }

    const EvasionReflexCommand& release(double now)
    {
        if (!m_cmd.active)
            return m_cmd;
        if (m_clearSince < 0.0)
            m_clearSince = now;
        if (now - m_clearSince >= lingerSeconds)
        {
            const int n = m_cmd.threats;
            m_cmd = {};
            m_cmd.threats = n;
        }
        return m_cmd;
    }
    EvasionReflexCommand m_cmd;
    double m_nextAt = 0.0;
    double m_clearSince = -1.0;
};
} // namespace game::navigation::avoidance
