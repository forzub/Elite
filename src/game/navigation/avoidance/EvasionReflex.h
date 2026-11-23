#pragma once

// THE REFLEX (navigator layer 5, step 3): emergency evasion from RADAR CONTACTS while the autopilot flies.
// Every `periodSeconds` the radar's tracks (sensor = hull frame: +X right, +Y up, -Z nose; relative position and
// velocity at the scan's epoch) are extrapolated to now and judged by the evasion score of "hold the course": when a
// ship / debris track comes inside the safe sphere within the horizon, the teacher (MultiThreatEvasion.h) picks the
// manoeuvre and the reflex holds it as RCS translation (the pilot's own keypad controls - the Assisted law yields a
// manually translated axis); when the course is clear again it lets go and the autopilot takes the ship back to its
// route. RCS only (the main drive would need the nose, which the route tracker owns): plausible - thrusters dodge,
// the engines keep the course. Structures are the planner's (they do not move), not the reflex's.
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/equipment/radar/RadarSensorTypes.h"
#include "src/game/navigation/avoidance/EvasionNet.h"

namespace game::navigation::avoidance
{
struct EvasionReflexCommand
{
    bool active = false;
    float strafe = 0.0f, lift = 0.0f, forward = 0.0f;   // per-axis RCS authority, -1..1
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
        EvasionScene sc;
        sc.forward = glm::dvec3(0.0, 0.0, -1.0);
        sc.caps.lateralAccelMps2 = std::max(0.05, lateralAccelMps2);
        sc.caps.mainAccelMps2 = sc.caps.lateralAccelMps2;   // RCS only: the main drive never beats it (no turn)
        sc.caps.turnRateRadPerS = 0.3;
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
    const EvasionReflexCommand& command() const { return m_cmd; }
    void reset()
    {
        m_cmd = {};
        m_nextAt = 0.0;
        m_clearSince = -1.0;
    }

private:
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
