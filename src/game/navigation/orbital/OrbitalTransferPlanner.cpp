#include "src/game/navigation/orbital/OrbitalTransferPlanner.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace game::navigation::orbital
{
namespace
{
constexpr double Pi = 3.14159265358979323846;

glm::dvec3 rotateAbout(const glm::dvec3& v, const glm::dvec3& axisUnit, double angle)
{
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return v * c + glm::cross(axisUnit, v) * s + axisUnit * glm::dot(axisUnit, v) * (1.0 - c);
}

double smoothstep(double s)
{
    return s * s * (3.0 - 2.0 * s);
}

// Guide curve: sweep the start plane into the target plane while the radius follows a
// phasing profile: start radius -> phasing radius -> end radius. The angle advances at the
// local circular rate sqrt(mu / r^3), so the main chain is nearly Keplerian (cheap) and the
// phasing radius is the one that makes the sweep fit the duration (lower = faster).
struct Guide
{
    glm::dvec3 u0 {1.0, 0.0, 0.0};
    glm::dvec3 n0 {0.0, 1.0, 0.0};
    glm::dvec3 tiltAxis {1.0, 0.0, 0.0};
    double tiltAngle = 0.0;
    double sweep = 0.0;
    double r0 = 0.0;
    double r1 = 0.0;
    double rPhase = 0.0;
    double T = 1.0;
    double mu = 3.986004418e14;
    std::vector<double> sigma;   // swept fraction at uniform time samples
    std::vector<double> tilt;    // plane-change fraction at uniform time samples (done where the orbit is high and slow)

    static constexpr int Samples = 600;

    double radiusAtTime(double f) const
    {
        const double edge = 0.18;
        const double up = smoothstep(std::clamp(f / edge, 0.0, 1.0));
        const double down = smoothstep(std::clamp((1.0 - f) / edge, 0.0, 1.0));
        const double base = r0 + (r1 - r0) * smoothstep(f);
        const double w = up * down;          // convex blend: the radius never leaves [min, max] of the three
        return (1.0 - w) * base + w * rPhase;
    }

    double integrateSweep() const
    {
        double th = 0.0;
        for (int i = 0; i < Samples; ++i)
        {
            const double f = (i + 0.5) / Samples;
            const double r = radiusAtTime(f);
            th += std::sqrt(mu / (r * r * r)) * T / Samples;
        }
        return th;
    }

    void buildSchedule()
    {
        sigma.assign(Samples + 1, 0.0);
        tilt.assign(Samples + 1, 0.0);
        double th = 0.0;
        double tw = 0.0;
        double rMax = std::max({r0, r1, rPhase});
        for (int i = 0; i < Samples; ++i)
        {
            const double f = (i + 0.5) / Samples;
            const double r = radiusAtTime(f);
            th += std::sqrt(mu / (r * r * r)) * T / Samples;
            sigma[static_cast<std::size_t>(i) + 1] = th;
            // a plane change costs ~ orbital speed x angle: spend it where the orbit is high (slow)
            const double q = r / rMax;
            tw += q * q * q * q;
            tilt[static_cast<std::size_t>(i) + 1] = tw;
        }
        for (auto& x : sigma)
            x /= std::max(1.0e-12, th);
        for (auto& x : tilt)
            x /= std::max(1.0e-12, tw);
    }

    glm::dvec3 at(double f) const
    {
        f = std::clamp(f, 0.0, 1.0);
        const double x = f * Samples;
        const int i = std::min(Samples - 1, static_cast<int>(x));
        const double w = x - i;
        const double sg = sigma[static_cast<std::size_t>(i)] * (1.0 - w) + sigma[static_cast<std::size_t>(i) + 1] * w;
        const double tg = tilt[static_cast<std::size_t>(i)] * (1.0 - w) + tilt[static_cast<std::size_t>(i) + 1] * w;
        const glm::dvec3 inPlane = rotateAbout(u0, n0, sg * sweep);
        const glm::dvec3 dir = tiltAngle > 1.0e-12 ? rotateAbout(inPlane, tiltAxis, tg * tiltAngle) : inPlane;
        return glm::normalize(dir) * radiusAtTime(f);
    }
};

bool solvePhasing(Guide& g, double rMin, double rMax)
{
    // sweep decreases monotonically with the phasing radius
    double lo = rMin, hi = rMax;
    g.rPhase = lo;
    if (g.integrateSweep() < g.sweep)
        return false;          // even the lowest safe phasing orbit is too slow
    g.rPhase = hi;
    if (g.integrateSweep() > g.sweep)
        return false;          // would have to hover: not a transfer
    for (int i = 0; i < 80; ++i)
    {
        g.rPhase = 0.5 * (lo + hi);
        if (g.integrateSweep() > g.sweep)
            lo = g.rPhase;
        else
            hi = g.rPhase;
    }
    g.rPhase = 0.5 * (lo + hi);
    g.buildSchedule();
    return true;
}

Guide makeGuide(const glm::dvec3& p0, const glm::dvec3& v0, const glm::dvec3& p1, const glm::dvec3& v1, int extraTurns)
{
    Guide g;
    g.u0 = glm::normalize(p0);
    g.r0 = glm::length(p0);
    g.r1 = glm::length(p1);
    glm::dvec3 h0 = glm::cross(p0, v0);
    glm::dvec3 h1 = glm::cross(p1, v1);
    g.n0 = glm::length(h0) > 1.0e-6 ? glm::normalize(h0) : glm::dvec3(0.0, 1.0, 0.0);
    const glm::dvec3 n1 = glm::length(h1) > 1.0e-6 ? glm::normalize(h1) : g.n0;
    const glm::dvec3 axis = glm::cross(g.n0, n1);
    const double sa = glm::length(axis);
    const double ca = std::clamp(glm::dot(g.n0, n1), -1.0, 1.0);
    if (sa > 1.0e-12)
    {
        g.tiltAxis = axis / sa;
        g.tiltAngle = std::atan2(sa, ca);
    }
    // start direction carried into the target plane; the remaining in-plane angle is the sweep
    const glm::dvec3 a = g.tiltAngle > 1.0e-12 ? rotateAbout(g.u0, g.tiltAxis, g.tiltAngle) : g.u0;
    const glm::dvec3 u1 = glm::normalize(p1);
    double base = std::atan2(glm::dot(n1, glm::cross(a, u1)), glm::dot(a, u1));
    if (base < 0.0)
        base += 2.0 * Pi;
    g.sweep = base + 2.0 * Pi * extraTurns;
    return g;
}

struct Candidate
{
    OrbitalTransferPlan plan;
    bool ok = false;
    double zoneThrust = 0.0;
};

void approachState(
    const world::orbits::OrbitalMotion& m,
    double t,
    const TransferLimits& L,
    glm::dvec3& p,
    glm::dvec3& v,
    glm::dvec3& tp,
    glm::dvec3& tv)
{
    tp = targetPositionAt(m, t);
    tv = targetVelocityAt(m, t);
    const glm::dvec3 prograde = glm::normalize(tv);
    p = tp - prograde * L.approachOffsetMeters;
    v = tv + prograde * L.closingSpeedMps;
}

Candidate tryDuration(
    const CentralBody& body,
    const glm::dvec3& p0,
    const glm::dvec3& v0,
    double t0,
    const world::orbits::OrbitalMotion& m,
    const TransferLimits& L,
    double T,
    int extraTurns)
{
    Candidate c;
    OrbitalTransferPlan& plan = c.plan;
    glm::dvec3 p1, v1, tp, tv;
    approachState(m, t0 + T, L, p1, v1, tp, tv);
    Guide g = makeGuide(p0, v0, p1, v1, extraTurns);
    g.T = T;
    g.mu = body.muM3s2;
    plan.sweepRad = g.sweep;
    const double rSafe = body.radiusMeters + body.minAltitudeMeters + 20000.0;
    if (!solvePhasing(g, rSafe, 4.0 * std::max(g.r0, g.r1)))
        return c;

    // departure and arrival ramps: inside the traffic zones the ship changes speed gently
    // (constant thrust along the needed velocity change, real gravity integrated); the
    // main chain between the ramps follows the guide curve.
    const double rampAccel = 0.8 * L.zoneThrustAccelMps2;
    const double tau = std::sqrt(2.0 * std::max(L.departureZoneMeters, L.arrivalZoneMeters) / std::max(0.1, rampAccel));
    if (T < 2.0 * tau + 60.0)
        return c;
    const double sd = tau / T;
    const double sa = 1.0 - tau / T;
    const double ds = 1.0e-4;
    auto guideVel = [&](double sv) {
        const double lo = std::max(0.0, sv - ds);
        const double hi = std::min(1.0, sv + ds);
        return (g.at(hi) - g.at(lo)) / ((hi - lo) * T);
    };
    auto propagate = [&](glm::dvec3 p, glm::dvec3 v, double dur, const glm::dvec3& thrust) {
        const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(dur) / 0.5)));
        const double h = dur / steps;
        for (int i = 0; i < steps; ++i)
        {
            // RK4 on (p, v) with gravity + constant thrust
            auto acc = [&](const glm::dvec3& x) { return gravityAt(body, x) + thrust; };
            const glm::dvec3 k1v = acc(p), k1p = v;
            const glm::dvec3 k2v = acc(p + 0.5 * h * k1p), k2p = v + 0.5 * h * k1v;
            const glm::dvec3 k3v = acc(p + 0.5 * h * k2p), k3p = v + 0.5 * h * k2v;
            const glm::dvec3 k4v = acc(p + h * k3p), k4p = v + h * k3v;
            p += h / 6.0 * (k1p + 2.0 * k2p + 2.0 * k3p + k4p);
            v += h / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
        }
        return std::make_pair(p, v);
    };
    auto rampThrust = [&](const glm::dvec3& from, const glm::dvec3& to, const glm::dvec3& fallback) {
        const glm::dvec3 d = to - from;
        const double len = glm::length(d);
        const glm::dvec3 dir = len > 1.0e-6 ? d / len : glm::normalize(fallback);
        return dir * std::min(rampAccel, len / tau);
    };
    const glm::dvec3 thrustD = rampThrust(v0, guideVel(sd), v0);
    const auto [pA, vA] = propagate(p0, v0, tau, thrustD);
    const glm::dvec3 thrustA = rampThrust(guideVel(sa), v1, v1);
    const auto [pB, vB] = propagate(p1, v1, -tau, thrustA);

    const int n = std::max(1, static_cast<int>(std::ceil(g.sweep * (sa - sd) / (Pi / 6.0))));
    std::vector<double> ts(static_cast<std::size_t>(n) + 3);
    std::vector<glm::dvec3> pts(static_cast<std::size_t>(n) + 3);
    std::vector<glm::dvec3> vel(static_cast<std::size_t>(n) + 3);
    ts[0] = t0;
    pts[0] = p0;
    vel[0] = v0;
    for (int i = 0; i <= n; ++i)
    {
        const double sv = sd + (sa - sd) * i / n;
        ts[static_cast<std::size_t>(i) + 1] = t0 + T * sv;
        pts[static_cast<std::size_t>(i) + 1] = g.at(sv);
        vel[static_cast<std::size_t>(i) + 1] = guideVel(sv);
    }
    pts[1] = pA;
    vel[1] = vA;
    pts[static_cast<std::size_t>(n) + 1] = pB;
    vel[static_cast<std::size_t>(n) + 1] = vB;
    ts[static_cast<std::size_t>(n) + 2] = t0 + T;
    pts[static_cast<std::size_t>(n) + 2] = p1;
    vel[static_cast<std::size_t>(n) + 2] = v1;
    for (std::size_t i = 0; i + 1 < ts.size(); ++i)
    {
        HermiteSegment seg;
        seg.t0 = ts[i];
        seg.t1 = ts[i + 1];
        seg.p0 = pts[i];
        seg.v0 = vel[i];
        seg.p1 = pts[i + 1];
        seg.v1 = vel[i + 1];
        plan.segments.push_back(seg);
    }
    plan.startTimeSeconds = t0;
    plan.arrivalTimeSeconds = t0 + T;
    plan.arrivalPositionMeters = p1;
    plan.arrivalVelocityMps = v1;
    plan.targetPositionAtArrivalMeters = tp;
    plan.targetVelocityAtArrivalMps = tv;

    // feasibility: altitude, thrust envelope, zone envelope
    const double zoneDep = tau;
    const double zoneArr = tau;
    plan.minAltitudeMeters = 1.0e30;
    const int samples = 40 * (n + 2);
    glm::dvec3 prev = p0;
    bool ok = true;
    double worstZone = 0.0;
    for (int k = 0; k <= samples; ++k)
    {
        const double t = t0 + T * k / samples;
        const KinematicSample s = plan.evaluate(t);
        const double alt = glm::length(s.positionMeters) - body.radiusMeters;
        plan.minAltitudeMeters = std::min(plan.minAltitudeMeters, alt);
        const glm::dvec3 thrust = s.accelerationMps2 - gravityAt(body, s.positionMeters);
        const double a = glm::length(thrust);
        plan.maxThrustAccelMps2 = std::max(plan.maxThrustAccelMps2, a);
        plan.peakSpeedMps = std::max(plan.peakSpeedMps, glm::length(s.velocityMps));
        plan.peakSpeedVsTargetMps = std::max(plan.peakSpeedVsTargetMps, glm::length(s.velocityMps - targetVelocityAt(m, t)));
        if (k > 0)
        {
            plan.deltaVMps += a * T / samples;
            plan.pathLengthMeters += glm::length(s.positionMeters - prev);
        }
        prev = s.positionMeters;
        const bool inZone = (t - t0) < zoneDep || (t0 + T - t) < zoneArr;
        if (inZone)
            worstZone = std::max(worstZone, a);
        if (alt < body.minAltitudeMeters || a > L.maxThrustAccelMps2 || (inZone && a > L.zoneThrustAccelMps2))
            ok = false;
        if (plan.deltaVMps > L.maxDeltaVMps)
            ok = false;
        if (k % 4 == 0)
            plan.tunnelMeters.push_back(s.positionMeters);
    }
    if (!std::isfinite(plan.deltaVMps) || !std::isfinite(plan.pathLengthMeters) || !std::isfinite(plan.maxThrustAccelMps2))
        ok = false;
    c.ok = ok;
    c.zoneThrust = worstZone;
    return c;
}
}

KinematicSample HermiteSegment::evaluate(double t) const
{
    const double T = std::max(1.0e-9, t1 - t0);
    const double s = std::clamp((t - t0) / T, 0.0, 1.0);
    const double s2 = s * s;
    const double s3 = s2 * s;
    const glm::dvec3 m0 = v0 * T;
    const glm::dvec3 m1 = v1 * T;
    KinematicSample out;
    out.positionMeters = (2 * s3 - 3 * s2 + 1) * p0 + (s3 - 2 * s2 + s) * m0 + (-2 * s3 + 3 * s2) * p1 + (s3 - s2) * m1;
    out.velocityMps = ((6 * s2 - 6 * s) * p0 + (3 * s2 - 4 * s + 1) * m0 + (-6 * s2 + 6 * s) * p1 + (3 * s2 - 2 * s) * m1) / T;
    out.accelerationMps2 = ((12 * s - 6) * p0 + (6 * s - 4) * m0 + (-12 * s + 6) * p1 + (6 * s - 2) * m1) / (T * T);
    return out;
}

KinematicSample OrbitalTransferPlan::evaluate(double t) const
{
    if (segments.empty())
        return {};
    if (t <= segments.front().t0)
        return segments.front().evaluate(segments.front().t0);
    for (const auto& s : segments)
        if (t <= s.t1)
            return s.evaluate(t);
    return segments.back().evaluate(segments.back().t1);
}

glm::dvec3 targetPositionAt(const world::orbits::OrbitalMotion& motion, double t)
{
    world::orbits::OrbitalMotion m = motion;
    m.centerMeters = glm::dvec3(0.0);
    return world::orbits::computeOrbitPositionMeters(m, t);
}

glm::dvec3 targetVelocityAt(const world::orbits::OrbitalMotion& motion, double t)
{
    world::orbits::OrbitalMotion m = motion;
    m.centerMeters = glm::dvec3(0.0);
    return world::orbits::computeOrbitVelocityMetersPerSecond(m, t);
}

glm::dvec3 gravityAt(const CentralBody& body, const glm::dvec3& p)
{
    const double r = glm::length(p);
    if (r < 1.0)
        return glm::dvec3(0.0);
    return -body.muM3s2 * p / (r * r * r);
}

OrbitalTransferPlan planOrbitalTransfer(
    const CentralBody& body,
    const glm::dvec3& p0,
    const glm::dvec3& v0,
    double t0,
    const world::orbits::OrbitalMotion& targetMotion,
    const TransferLimits& limits)
{
    OrbitalTransferPlan best;
    best.reason = "no feasible arrival epoch within the transfer envelope";
    int tried = 0;
    double bestScore = 1.0e30;
    std::string closest;
    // smallest feasible arrival epoch over the one-sweep and one-extra-turn geometries
    // the destination keeps moving: the sweep that fits a duration is the one closest to what the
    // start orbit itself would cover in that time (base angle + whole turns), +-1 turn tried as well
    const double naturalRate = glm::length(v0) / std::max(1.0, glm::length(p0));
    for (double T = limits.minDurationSeconds; T <= limits.maxDurationSeconds; T *= 1.03)
    {
        const int k0 = std::max(0, static_cast<int>(std::floor(naturalRate * T / (2.0 * Pi))));
        for (int turns = 0; turns <= k0 + 1; ++turns)
        {
            ++tried;
            Candidate c = tryDuration(body, p0, v0, t0, targetMotion, limits, T, turns);
            {
                const double score = std::max({c.plan.maxThrustAccelMps2 / limits.maxThrustAccelMps2, c.zoneThrust / limits.zoneThrustAccelMps2,
                                               (body.minAltitudeMeters - c.plan.minAltitudeMeters) / 100000.0 + 1.0, c.plan.deltaVMps / limits.maxDeltaVMps});
                if (!std::isfinite(score))
                    continue;
                if (score < bestScore)
                {
                    bestScore = score;
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "closest T=%.0f s sweep=%.0f deg: thrust %.1f/%.1f, zone %.1f/%.1f, min alt %.0f km, dv %.1f/%.1f km/s",
                                  T, c.plan.sweepRad * 57.29578, c.plan.maxThrustAccelMps2, limits.maxThrustAccelMps2, c.zoneThrust,
                                  limits.zoneThrustAccelMps2, c.plan.minAltitudeMeters / 1000.0, c.plan.deltaVMps / 1000.0, limits.maxDeltaVMps / 1000.0);
                    closest = buf;
                }
            }
            if (c.ok)
            {
                c.plan.feasible = true;
                c.plan.reason.clear();
                c.plan.candidatesTried = tried;
                return c.plan;
            }
        }
    }
    best.candidatesTried = tried;
    best.startTimeSeconds = t0;
    best.reason += "; " + closest;
    return best;
}

void stepCarrier(
    const OrbitalTransferPlan& plan,
    const CentralBody& body,
    TransferCarrier& c,
    double dt,
    double engineMaxAccelMps2,
    bool thrustEnabled)
{
    const KinematicSample ref = plan.evaluate(c.timeSeconds);
    const KinematicSample mid = plan.evaluate(c.timeSeconds + 0.5 * dt);
    const glm::dvec3 g = gravityAt(body, c.positionMeters);
    glm::dvec3 thrust(0.0);
    if (thrustEnabled)
    {
        // feed-forward of the plan, real gravity removed, critically damped correction (same epoch)
        const double w = 0.25;
        thrust = mid.accelerationMps2 - g + w * w * (ref.positionMeters - c.positionMeters) + 2.0 * w * (ref.velocityMps - c.velocityMps);
        const double a = glm::length(thrust);
        if (a > engineMaxAccelMps2 && a > 0.0)
            thrust *= engineMaxAccelMps2 / a;
    }
    c.thrustMps2 = thrust;
    c.accelerationMps2 = thrust + g;
    c.velocityMps += c.accelerationMps2 * dt;
    c.positionMeters += c.velocityMps * dt;
    c.timeSeconds += dt;
    c.deltaVUsedMps += glm::length(thrust) * dt;
}

OrbitalTransferPlan planCompressedTransit(
    const glm::dvec3& p0,
    const glm::dvec3& v0,
    double t0,
    const world::orbits::OrbitalMotion& m,
    const TransferLimits& L,
    double realSeconds)
{
    OrbitalTransferPlan plan;
    glm::dvec3 p1, v1, tp, tv;
    approachState(m, t0 + realSeconds, L, p1, v1, tp, tv);
    // a bowed arc about the star: the mid point lies on the mean radius between the two directions
    const double r0 = glm::length(p0), r1 = glm::length(p1);
    glm::dvec3 midDir = p0 / std::max(1.0, r0) + p1 / std::max(1.0, r1);
    if (glm::length(midDir) < 1.0e-6)
        midDir = glm::normalize(glm::cross(p0, glm::dvec3(0.0, 1.0, 0.0)));
    const glm::dvec3 mid = glm::normalize(midDir) * (0.5 * (r0 + r1));
    const double tm = t0 + 0.5 * realSeconds;
    const glm::dvec3 chord = (p1 - p0) / realSeconds;
    plan.segments.push_back(HermiteSegment {t0, tm, p0, glm::dvec3(0.0), mid, chord});
    plan.segments.push_back(HermiteSegment {tm, t0 + realSeconds, mid, chord, p1, glm::dvec3(0.0)});
    plan.startTimeSeconds = t0;
    plan.arrivalTimeSeconds = t0 + realSeconds;
    plan.arrivalPositionMeters = p1;
    plan.arrivalVelocityMps = v1;
    plan.targetPositionAtArrivalMeters = tp;
    plan.targetVelocityAtArrivalMps = tv;
    plan.startVelocityMps = v0;
    plan.compressed = true;
    glm::dvec3 prev = p0;
    for (int k = 0; k <= 160; ++k)
    {
        const glm::dvec3 p = plan.evaluate(t0 + realSeconds * k / 160.0).positionMeters;
        plan.pathLengthMeters += glm::length(p - prev);
        prev = p;
        plan.tunnelMeters.push_back(p);
    }
    const InterplanetaryEstimate e = brachistochrone(plan.pathLengthMeters, L.maxThrustAccelMps2);
    plan.physicalDurationSeconds = e.durationSeconds;
    plan.physicalPeakSpeedMps = e.peakSpeedMps;
    plan.peakSpeedMps = e.peakSpeedMps;
    plan.deltaVMps = e.deltaVMps;
    plan.feasible = true;
    return plan;
}

KinematicSample evaluateCompressed(const OrbitalTransferPlan& plan, double t)
{
    KinematicSample s = plan.evaluate(t);
    const double f = std::clamp((t - plan.startTimeSeconds) / std::max(1.0e-9, plan.durationSeconds()), 0.0, 1.0);
    const double w = f * f * (3.0 - 2.0 * f);
    s.velocityMps = plan.startVelocityMps * (1.0 - w) + plan.arrivalVelocityMps * w;
    s.accelerationMps2 = glm::dvec3(0.0);
    return s;
}

InterplanetaryEstimate brachistochrone(double d, double a)
{
    InterplanetaryEstimate e;
    e.distanceMeters = d;
    e.accelMps2 = a;
    e.durationSeconds = 2.0 * std::sqrt(d / std::max(1.0e-9, a));
    e.peakSpeedMps = a * e.durationSeconds / 2.0;
    e.deltaVMps = a * e.durationSeconds;
    return e;
}
}
