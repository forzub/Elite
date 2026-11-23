#pragma once

// EMERGENCY EVASION from several moving threats at once (author 2026-11-22: «нейросетку для чернового подсчёта маршрута и
// экстренного уклонения»; agreed order: the evasion first, its TEACHER = a sampling optimizer, the net learns it).
//
// The ship is a point mass that can push in any direction (RCS: lateralAccel) and, along its nose, harder (the main
// drive: mainAccel, after a turn at turnRate - modelled as a delay); the threats move straight on (radar tracks:
// position + velocity, a radius). A manoeuvre = one constant acceleration held for `holdSeconds`, then coasting.
// Its score over the horizon: the smallest SEPARATION MARGIN (centre distance minus both radii minus the safe margin)
// against every threat, minus a small cost for the thrust used (the least effort among the equally safe).
// The teacher samples a fixed set of manoeuvres (a Fibonacci sphere of directions x 3 thrust levels + "do nothing")
// and keeps the best; the same score judges what the net proposes (EvasionNet.h) - a net answer is used only when it
// is at least as safe as the teacher's quick check.
#include <algorithm>
#include <cmath>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::avoidance
{
struct EvasionThreatTrack
{
    glm::dvec3 position {0.0};   // relative to the ship
    glm::dvec3 velocity {0.0};   // relative to the ship
    double radiusMeters = 30.0;
};

struct EvasionShipCaps
{
    double lateralAccelMps2 = 1.0;
    double mainAccelMps2 = 8.0;
    double turnRateRadPerS = 0.3;
    double radiusMeters = 30.0;
};

struct EvasionScene
{
    glm::dvec3 forward {0.0, 0.0, -1.0};   // the ship's nose (unit)
    EvasionShipCaps caps;
    std::vector<EvasionThreatTrack> threats;
    double horizonSeconds = 30.0;
    double holdSeconds = 6.0;
    double marginMeters = 40.0;
    // when contact cannot be avoided: judge by the IMPACT, not the depth (author 2026-11-23: «если столкновение
    // неизбежно - минимизировать ущерб ... максимальный рикошет»): the closing speed along the contact normal at the
    // first touch; the tangential part slides off. false = the old depth-only judgement (for the A/B in EvasionLab)
    bool glancingWhenUnavoidable = true;
    double clearCapMeters = 200.0;   // beyond this margin clear is clear (the least thrust wins); the reflex uses a small
                                     // cap - a dodge just enough, not a 170 m swerve off the route (suite 2026-11-23)
};

struct EvasionChoice
{
    glm::dvec3 accel {0.0};        // the commanded acceleration (frame of the scene)
    double minMarginMeters = 0.0;  // the smallest margin over the horizon with it (< 0: hulls inside the safe sphere)
    double score = 0.0;
    double timeOfMinSeconds = 0.0;
    bool contact = false;              // the hulls touch within the horizon
    double contactTimeSeconds = -1.0;  // when they first touch (contact only)
    double impactNormalSpeedMps = 0.0; // the closing speed along the contact normal at the first touch
    double impactSpeedMps = 0.0;       // the whole relative speed there
};

// the acceleration a ship can really give in direction `dir` with throttle t01: the main drive when the nose can be
// brought round in time (its delay is the turn), else the RCS
inline glm::dvec3 evasionAccelFor(const EvasionScene& sc, const glm::dvec3& dir, double t01, double* delaySeconds = nullptr)
{
    const double c = std::clamp(glm::dot(sc.forward, dir), -1.0, 1.0);
    const double turn = std::acos(c) / std::max(1.0e-3, sc.caps.turnRateRadPerS);
    const double main = sc.caps.mainAccelMps2 * t01, rcs = sc.caps.lateralAccelMps2 * t01;
    // the main drive pays the turn: worth it when its displacement over the hold beats the RCS's
    const double h = sc.holdSeconds;
    const bool useMain = turn < h && main * (h - turn) * (h - turn) > rcs * h * h;
    if (delaySeconds)
        *delaySeconds = useMain ? turn : 0.0;
    return dir * (useMain ? main : rcs);
}

// the score of one manoeuvre: accel held from `delay` to holdSeconds (the RCS pushes during the turn), then coasting
inline EvasionChoice scoreEvasion(const EvasionScene& sc, const glm::dvec3& accel, double delay)
{
    EvasionChoice out;
    out.accel = accel;
    out.minMarginMeters = 1.0e30;
    const double dt = 0.5;
    const double aMag = glm::length(accel);
    const glm::dvec3 aDir = aMag > 1.0e-9 ? accel / aMag : glm::dvec3(0.0);
    const double rcs = std::min(aMag, sc.caps.lateralAccelMps2);
    glm::dvec3 p(0.0), v(0.0);
    for (double t = 0.0; t <= sc.horizonSeconds + 1.0e-9; t += dt)
    {
        for (const auto& th : sc.threats)
        {
            const glm::dvec3 q = th.position + th.velocity * t;
            const double dist = glm::length(q - p);
            const double margin = dist - th.radiusMeters - sc.caps.radiusMeters - sc.marginMeters;
            if (margin < out.minMarginMeters)
            {
                out.minMarginMeters = margin;
                out.timeOfMinSeconds = t;
            }
            if (!out.contact && dist < th.radiusMeters + sc.caps.radiusMeters)
            {
                // the first touch (sampled at dt: the normal is the centre line there, close enough for spheres)
                out.contact = true;
                const glm::dvec3 n = dist > 1.0e-6 ? (p - q) / dist : glm::dvec3(0.0, 0.0, 1.0);
                const glm::dvec3 rel = v - th.velocity;   // ours relative to the threat
                out.impactNormalSpeedMps = std::max(0.0, -glm::dot(rel, n));
                out.impactSpeedMps = glm::length(rel);
            }
        }
        glm::dvec3 a(0.0);
        if (t < sc.holdSeconds)
            a = t < delay ? aDir * rcs : accel;
        v += a * dt;
        p += v * dt;
    }
    if (sc.threats.empty())
        out.minMarginMeters = 1.0e6;
    // safety first (the margin, capped: beyond 200 m clear is clear), then the least thrust
    out.score = std::min(out.minMarginMeters, sc.clearCapMeters) - 2.0 * aMag;
    // an unavoidable contact: any miss beats any touch; among touches the softest along the normal (a glancing blow)
    if (sc.glancingWhenUnavoidable && out.contact)
        out.score = -1000.0 - 5.0 * out.impactNormalSpeedMps - 0.1 * aMag;
    return out;
}

// the fixed manoeuvre set: n directions on a Fibonacci sphere
inline const std::vector<glm::dvec3>& evasionDirections()
{
    static const std::vector<glm::dvec3> dirs = [] {
        std::vector<glm::dvec3> d;
        constexpr int N = 96;
        const double golden = 3.14159265358979323846 * (3.0 - std::sqrt(5.0));
        for (int i = 0; i < N; ++i)
        {
            const double y = 1.0 - 2.0 * (i + 0.5) / N;
            const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
            d.emplace_back(std::cos(golden * i) * r, y, std::sin(golden * i) * r);
        }
        return d;
    }();
    return dirs;
}

constexpr double EvasionThrustLevels[3] = {0.35, 0.7, 1.0};

// THE TEACHER: the best of the sampled manoeuvres (bestDir / bestLevel: its direction and thrust indices, -1 = nothing)
inline EvasionChoice teachEvasion(const EvasionScene& sc, int* bestDir = nullptr, int* bestLevel = nullptr)
{
    EvasionChoice best = scoreEvasion(sc, glm::dvec3(0.0), 0.0);   // doing nothing is a candidate
    int bd = -1, bl = -1;
    const auto& dirs = evasionDirections();
    for (std::size_t di = 0; di < dirs.size(); ++di)
        for (int li = 0; li < 3; ++li)
        {
            double delay = 0.0;
            const glm::dvec3 a = evasionAccelFor(sc, dirs[di], EvasionThrustLevels[li], &delay);
            const EvasionChoice c = scoreEvasion(sc, a, delay);
            if (c.score > best.score)
            {
                best = c;
                bd = static_cast<int>(di);
                bl = li;
            }
        }
    if (bestDir)
        *bestDir = bd;
    if (bestLevel)
        *bestLevel = bl;
    return best;
}
} // namespace game::navigation::avoidance
