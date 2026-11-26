#pragma once

// THE REFLEX (navigator layer 5, step 3): emergency evasion from RADAR CONTACTS while the autopilot flies.
// Every `periodSeconds` the radar's tracks (sensor = hull frame: +X right, +Y up, -Z nose; relative position and
// velocity at the scan's epoch) are extrapolated to now and judged by the evasion score of "hold the course": when a
// ship / debris track comes inside the safe sphere within the horizon, the teacher (MultiThreatEvasion.h) picks the
// manoeuvre and the reflex holds it as RCS translation (the pilot's own keypad controls - the Assisted law yields a
// manually translated axis); when the course is clear again it lets go and the autopilot takes the ship back to its
// route. RCS only (the main drive would need the nose, which the route tracker owns): plausible - thrusters dodge,
// the engines keep the course. Structures come as extra threats from the caller (near contacts only on a route).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
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
    double urgencySeconds = 0.0;                        // the window the reflex acts in (the threat must be that close)
    int threats = 0;
    bool fromNet = false;
};

class EvasionReflex
{
public:
    double periodSeconds = 0.25;
    double horizonSeconds = 12.0;
    double holdSeconds = 4.0;
    // the safety envelope's base hull-to-hull gap (TrajectoryConflict.h SafetyEnvelopePolicy; the sensor's uncertainty
    // adds to it per track)
    double marginMeters = SafetyEnvelopePolicy {}.baseClearanceMeters;
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
    // the programs' integration step: 0.05 s (audit 2 HIGH: at 0.1 s the prediction drifted 11-16 m from the physics in
    // 2 s - the size of the safety gap; 0.05 halves it at ~2x the cost, the rest is the drift bound in the judge)
    double programDtSeconds = 0.05;
    double programHoldSeconds = 2.0;   // re-chosen 4 times a second anyway; a 4 s full push moved ~1 km
    double lastProgramMs = 0.0;
    // URGENCY (remote B4-11, 2026-11-23): a threat matters to the reflex only once it is about as close in time as the
    // dodge itself takes - sqrt(2 x the miss distance / the lateral accel) + a second. Judged straight over the whole
    // 12 s horizon, the city's lane traffic (which turns at the connectors) set off five dodges in a row at 120 m/s and
    // pushed the ship 162 m off its route into the built-up zone. Farther threats are the route planner's business.
    // ELITE_EVASION_URGENCY=0: the old whole-horizon trigger.
    double reactionSeconds = 1.0;
    double gentleShare = 0.4;
    double urgencyFor(double lateralAccelMps2, double hullRadiusMeters, double threatRadiusMeters = 30.0) const
    {
        static const bool off = [] {
            const char* v = std::getenv("ELITE_EVASION_URGENCY");
            return v && v[0] == '0';
        }();
        if (off)
            return horizonSeconds;
        const double miss = threatRadiusMeters + std::max(5.0, hullRadiusMeters) + marginMeters;
        // the dodge is flown by the gentle programs first (0.4 of the stick): the time is theirs
        const double a = gentleShare * lateralAccelMps2;
        const double tNeed = a > 0.1 ? std::sqrt(2.0 * miss / a) : 4.0;
        return std::clamp(tNeed + reactionSeconds, 3.0, 8.0);
    }
    // extraThreats: threats in the body frame NOW, age 0 (the structures near the course - the manual assist; audit 2:
    // the scene skipped every Structure track, the assist never protected from a station / a city)
    const EvasionReflexCommand& updatePrograms(const radar::RadarScanReport& scan, double now, const ControlShipState& ship,
                                               double hullRadiusMeters, double lateralAccelMps2 = 0.0,
                                               const std::vector<EvasionThreatTrack>* extraThreats = nullptr)
    {
        if (now < m_nextAt)
            return m_cmd;
        m_nextAt = now + periodSeconds;
        EvasionScene sc = sceneFrom(scan, now, hullRadiusMeters, &ship.ratesPitchYawRoll, &ship);
        if (extraThreats)
            sc.threats.insert(sc.threats.end(), extraThreats->begin(), extraThreats->end());
        m_cmd.threats = static_cast<int>(sc.threats.size());
        if (sc.threats.empty())
            return release(now);
        const auto& progs = controlPrograms();
        const ControlFlight handsOff = flyControlProgram(ship, progs[ControlProgramHandsOff], programHoldSeconds,
                                                         horizonSeconds, programDtSeconds);
        const EvasionChoice course = judgeControlFlight(handsOff, sc, ship.velocityBody);
        m_cmd.courseMarginMeters = course.contact ? -1.0e3 : course.minMarginMeters;
        // the window (wider while dodging: no drop in the middle of a manoeuvre)
        double largestThreat = 0.0;
        for (const auto& th : sc.threats)
            largestThreat = std::max(largestThreat, th.radiusMeters);
        // NAV-5: the lateral authority the window is computed from - the caller's live figure (Assisted telemetry), else
        // the RCS lateral acceleration of the SAME physics the programs are flown on (a Newtonian route / the manual and
        // phase assist passed none: a fixed 5 s window, 8 s needed at 2 m/s2)
        const double lateral = lateralAccelMps2 > 0.0 ? lateralAccelMps2 : static_cast<double>(ship.params.strafeAccel);
        const double window = urgencyFor(lateral, hullRadiusMeters, largestThreat) * (m_cmd.active ? 1.5 : 1.0);
        m_cmd.urgencySeconds = window;
        const bool urgent = (course.contact && course.contactTimeSeconds <= window) ||
                            (course.earliestViolationSeconds >= 0.0 && course.earliestViolationSeconds <= window);
        if (!urgent)
            return release(now);
        // DEV: which threat made the course urgent (ELITE_DEV_EVASION_TRACE=1) - each one judged alone on the hands-off
        // flight, those touching or inside the margin within the window
        static const bool trace = [] {
            const char* v = std::getenv("ELITE_DEV_EVASION_TRACE");
            return v && v[0] == '1';
        }();
        if (trace)
            for (std::size_t i = 0; i < sc.threats.size(); ++i)
            {
                EvasionScene one = sc;
                one.threats.assign(1, sc.threats[i]);
                const EvasionChoice c = judgeControlFlight(handsOff, one, ship.velocityBody);
                const bool bad = (c.contact && c.contactTimeSeconds <= window) ||
                                 (c.earliestViolationSeconds >= 0.0 && c.earliestViolationSeconds <= window);
                if (!bad)
                    continue;
                const auto& th = sc.threats[i];
                std::fprintf(stderr,
                             "[EvasionTrace] threat %zu: pos (%.1f %.1f %.1f) vel (%.1f %.1f %.1f) r %.1f turning %d | contact %d at %.2f s,"
                             " margin %.1f; own v (%.1f %.1f %.1f) deck %d corot %d\n",
                             i, th.position.x, th.position.y, th.position.z, th.velocity.x, th.velocity.y, th.velocity.z,
                             th.radiusMeters, th.turning ? 1 : 0, c.contact ? 1 : 0, c.contactTimeSeconds, c.minMarginMeters,
                             ship.velocityBody.x, ship.velocityBody.y, ship.velocityBody.z, ship.deckActive ? 1 : 0,
                             ship.deckCoRotating ? 1 : 0);
            }
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
    // the scene the reflex judges (tests: the scan's age / rotation / size / uncertainty handling)
    EvasionScene buildScene(const radar::RadarScanReport& scan, double now, double hullRadiusMeters,
                            const glm::vec3* ratesPitchYawRoll) const
    {
        return sceneFrom(scan, now, hullRadiusMeters, ratesPitchYawRoll);
    }
    EvasionScene buildSceneFor(const radar::RadarScanReport& scan, double now, double hullRadiusMeters,
                               const ControlShipState& own) const
    {
        return sceneFrom(scan, now, hullRadiusMeters, &own.ratesPitchYawRoll, &own);
    }
    void reset()
    {
        m_cmd = {};
        m_nextAt = 0.0;
        m_clearSince = -1.0;
        m_lastNow = -1.0;
    }
    // THE EPISODE the reflex serves (audit 8e3c49007 NAV-7 "phantom dodge"): one route run, one virtual-pilot phase of
    // one request, the pilot's own flight. A different episode, or the clock going back, starts it clean - never the
    // last episode's dodge (up to 1 s of a full stick by the pad after a route ended mid-dodge), its hysteresis or its
    // widened window. Call before update / updatePrograms.
    void bindEpisode(std::uint64_t key, double now)
    {
        if (key != m_episode || (m_lastNow >= 0.0 && now + 1.0e-6 < m_lastNow))
        {
            reset();
            m_episode = key;
        }
        m_lastNow = now;
    }
    std::uint64_t episode() const { return m_episode; }

private:
    // the scan -> the scene in OUR CURRENT body frame at now (the prediction's t = 0):
    //  - moved by the relative velocity over the scan's age (in the frame of the measurement, inertial over the age);
    //  - turned from the body frame AT THE MEASUREMENT into the body frame NOW by our own rotation over the age (NAV-3:
    //    0.3 rad/s x 0.58 s = 52 m sideways at 300 m) - the rotation by the same integrator the physics uses
    //    (SharedShipPhysics::propagateOrientation), no second definition of the radar's axes;
    //  - the target's real size (boundingRadiusMeters), its uncertainties and the age for the safety envelope.
    EvasionScene sceneFrom(const radar::RadarScanReport& scan, double now, double hullRadiusMeters,
                           const glm::vec3* ratesPitchYawRoll = nullptr, const ControlShipState* own = nullptr) const
    {
        EvasionScene sc;
        sc.forward = glm::dvec3(0.0, 0.0, -1.0);
        sc.caps.radiusMeters = std::max(5.0, hullRadiusMeters);
        sc.horizonSeconds = horizonSeconds;
        sc.holdSeconds = holdSeconds;
        sc.marginMeters = marginMeters;
        sc.clearCapMeters = 30.0;
        const double age = std::max(0.0, now - scan.measuredAtUniverseTimeSeconds);
        glm::dmat3 nowFromMeasured(1.0);
        if (ratesPitchYawRoll && age > 0.0)
        {
            ShipTransform turn {};
            turn.orientation = glm::mat4(1.0f);
            turn.pitchRate = ratesPitchYawRoll->x;
            turn.yawRate = ratesPitchYawRoll->y;
            turn.rollRate = ratesPitchYawRoll->z;
            constexpr int Steps = 4;
            for (int i = 0; i < Steps; ++i)
                SharedShipPhysics::propagateOrientation(turn, static_cast<float>(age / Steps));
            // the orientation maps the body NOW into the body AT THE MEASUREMENT: its transpose the other way
            nowFromMeasured = glm::transpose(glm::dmat3(glm::mat3(turn.orientation)));
        }
        for (const auto& t : scan.tracks)
        {
            if (t.kind == radar::RadarTrackKindStructure)
                continue;
            EvasionThreatTrack th;
            th.position = nowFromMeasured * (t.relativePositionMeters + t.relativeVelocityMps * age);
            th.velocity = nowFromMeasured * t.relativeVelocityMps;
            // OUR acceleration over the age (a target not accelerating): the relative velocity is down by our delta-v,
            // the relative position by its mean over the age (audit 2: 0.58 s at 8 m/s2 is 1.3 m and 4.6 m/s - comparable
            // with the safety gap at high closing speeds)
            if (own && age > 0.0)
            {
                th.velocity -= own->deltaVSinceScanBody;
                th.position -= 0.5 * own->deltaVSinceScanBody * age;
            }
            // the target's real size; a track without one (an older server): the old per-kind guess, kept explicit
            th.radiusMeters = t.boundingRadiusMeters > 0.0 ? t.boundingRadiusMeters
                                                            : (t.kind == radar::RadarTrackKindDebris ? 10.0 : 30.0);
            th.positionUncertaintyMeters = t.positionUncertaintyMeters;
            th.velocityUncertaintyMps = t.velocityUncertaintyMps;
            th.ageSeconds = age;
            // only what can reach us within the horizon (its size and ours, the safety gap and the sensor included)
            // ... and how far our own acceleration can carry us toward it over the horizon (audit 2: the prefilter took
            // our straight motion only - an accelerating ship could close on a threat it had already dropped)
            const double ownAccel = own ? std::max({static_cast<double>(own->params.forwardMainEngineAccelerationMps2),
                                                    static_cast<double>(own->params.strafeAccel), 0.0})
                                        : 0.0;
            const double reach = th.radiusMeters + sc.caps.radiusMeters + marginMeters +
                                 SafetyEnvelopePolicy::sensorGrowth(th.positionUncertaintyMeters, th.velocityUncertaintyMps,
                                                                    age, horizonSeconds) +
                                 0.5 * ownAccel * horizonSeconds * horizonSeconds;
            if (glm::length(th.position) - glm::length(th.velocity) * horizonSeconds > 400.0 + reach)
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
    double m_lastNow = -1.0;
    std::uint64_t m_episode = 0;
};
} // namespace game::navigation::avoidance
