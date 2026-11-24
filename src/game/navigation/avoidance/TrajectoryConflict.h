#pragma once

// TRAJECTORY CONFLICT - the collision / safety primitive of the avoidance layer (audit 8e3c49007 NAV-1 / NAV-2, the
// reviewer's pass «перевести avoidance с логики "есть ли столкновение в дискретной точке времени" на прогноз
// пространственно-временного конфликта траекторий»).
//
// The question is not "were the two bodies close at one of the sampled instants" but "do the swept bodies meet anywhere
// on [t0, t1], and what is the least clearance there". So every interval between two stored states of OUR predicted
// flight is treated as continuous motion:
//   - the other body moves linearly on the interval (a radar track: constant velocity; a known path can feed its own
//     states the same way);
//   - our body moves along the chord of the interval; the true path stays within a bound e of the chord: for a constant
//     acceleration a over the interval dt the largest deviation is a dt^2 / 8 = |dv| dt / 8 - taken x2 (|dv| dt / 4)
//     to cover an acceleration that changes inside the interval (a program's hold ending there). e is added to the
//     radii: the test is CONSERVATIVE whatever the sampling, the speed (m/s or a fraction of c) or the object size;
//   - the relative motion r(tau) = r0 + w tau on the interval: the closest approach analytically
//     (tau* = clamp(-r0.w / w.w, 0, dt)), the first touch from |r0 + w tau| = R (the smaller root in [0, dt]).
// Sampling still represents our NON-linear flight, but it only sets how tight e is, never whether a body is seen.
//
// Envelopes (kept apart on purpose):
//   PHYSICAL - the hulls' conservative spheres from their real geometry (ConflictBody::physicalRadiusMeters - the
//              radar track's boundingRadiusMeters, our own from our hit volumes): "do the hulls really meet";
//   SAFETY   - physical + the explicit clearance policy (SafetyEnvelopePolicy): a base hull-to-hull gap plus the
//              sensor's position uncertainty plus its velocity uncertainty grown over (scan age + t): "must the
//              pilot treat this pass as dangerous". A precise track and an uncertain one on the same nominal path
//              differ only here.
// The result (ConflictResult) is a structure, not a bool - the closest approach, its time and relative speed, both
// clearances, the first touch and its normal closing speed - so the HUD warning, traffic control, formation, debris /
// cargo / salvage and the security layer can share it. A narrow phase (hit volume pairs) can refine a broad conflict
// later through the same result; this pass keeps the sphere (conservative: no misses, only extra caution for long
// hulls).
#include <algorithm>
#include <cmath>
#include <limits>

#include <glm/glm.hpp>

namespace game::navigation::avoidance
{
// THE SAFETY ENVELOPE POLICY - the one place the "how close is too close" numbers live
struct SafetyEnvelopePolicy
{
    double baseClearanceMeters = 15.0;   // the hull-to-hull gap a pass must keep with an exact track
    // the sensor: position uncertainty as given, velocity uncertainty grows with the time since the measurement
    static double sensorGrowth(double positionUncertaintyMeters, double velocityUncertaintyMps, double ageSeconds,
                               double t)
    {
        return std::max(0.0, positionUncertaintyMeters) + std::max(0.0, velocityUncertaintyMps) * std::max(0.0, ageSeconds + t);
    }
};

// the other body as the conflict test sees it
struct ConflictBody
{
    double physicalRadiusMeters = 1.0;
    double positionUncertaintyMeters = 0.0;
    double velocityUncertaintyMps = 0.0;
    double ageSeconds = 0.0;   // of the measurement at t = 0 of the prediction
};

struct ConflictResult
{
    double closestApproachMeters = std::numeric_limits<double>::infinity();   // centre to centre
    double timeOfClosestApproachSeconds = 0.0;
    double relativeSpeedAtClosestApproachMps = 0.0;
    double physicalClearanceMeters = std::numeric_limits<double>::infinity();   // surface to surface (spheres + e)
    double safetyClearanceMeters = std::numeric_limits<double>::infinity();     // to the safety envelope
    double timeOfSafetyMinimumSeconds = 0.0;
    bool physicalCollisionPredicted = false;
    double firstContactSeconds = -1.0;
    double impactNormalSpeedMps = 0.0;   // the closing speed along the contact normal at the first touch
    double impactSpeedMps = 0.0;         // the whole relative speed there
    bool safetyConflictPredicted = false;
};

// one interval [t0, t0 + dt]: our chord from p0 to p1 (deviation bound e), the other body at q0 moving with velocity v
inline void accumulateConflictInterval(ConflictResult& out, double t0, double dt, const glm::dvec3& p0,
                                       const glm::dvec3& p1, double e, const glm::dvec3& q0, const glm::dvec3& v,
                                       double ownRadiusMeters, const ConflictBody& body,
                                       const SafetyEnvelopePolicy& policy)
{
    if (!(dt > 0.0))
        return;
    const glm::dvec3 u = (p1 - p0) / dt;   // our chord velocity
    const glm::dvec3 r0 = q0 - p0;          // them relative to us
    const glm::dvec3 w = v - u;
    const double ww = glm::dot(w, w);
    const double tau = ww > 1.0e-18 ? std::clamp(-glm::dot(r0, w) / ww, 0.0, dt) : 0.0;
    const double d = glm::length(r0 + w * tau);
    if (d < out.closestApproachMeters)
    {
        out.closestApproachMeters = d;
        out.timeOfClosestApproachSeconds = t0 + tau;
        out.relativeSpeedAtClosestApproachMps = std::sqrt(ww);
    }
    const double physicalR = ownRadiusMeters + body.physicalRadiusMeters + e;
    const double physicalClearance = d - physicalR;
    out.physicalClearanceMeters = std::min(out.physicalClearanceMeters, physicalClearance);
    // the safety envelope grows with t: its largest value on the interval (at its end) - conservative
    const double safetyR = physicalR + policy.baseClearanceMeters +
                           SafetyEnvelopePolicy::sensorGrowth(body.positionUncertaintyMeters, body.velocityUncertaintyMps,
                                                              body.ageSeconds, t0 + dt);
    const double safetyClearance = d - safetyR;
    if (safetyClearance < out.safetyClearanceMeters)
    {
        out.safetyClearanceMeters = safetyClearance;
        out.timeOfSafetyMinimumSeconds = t0 + tau;
    }
    if (safetyClearance < 0.0)
        out.safetyConflictPredicted = true;
    if (!out.physicalCollisionPredicted && physicalClearance <= 0.0)
    {
        // the first touch: |r0 + w s| = R, the smaller root in [0, dt] (already inside at the start: s = 0)
        double s = 0.0;
        const double rr = glm::dot(r0, r0) - physicalR * physicalR;
        if (rr > 0.0 && ww > 1.0e-18)
        {
            const double b = glm::dot(r0, w);
            const double disc = b * b - ww * rr;
            s = std::clamp((-b - std::sqrt(std::max(0.0, disc))) / ww, 0.0, dt);
        }
        const glm::dvec3 rel = r0 + w * s;   // them relative to us at the touch
        const double len = glm::length(rel);
        const glm::dvec3 n = len > 1.0e-9 ? -rel / len : glm::dvec3(0.0, 0.0, 1.0);   // from them to us
        out.physicalCollisionPredicted = true;
        out.firstContactSeconds = t0 + s;
        // our velocity relative to them is -w; closing along n when it points against n
        out.impactNormalSpeedMps = std::max(0.0, glm::dot(w, n));
        out.impactSpeedMps = std::sqrt(ww);
    }
}

// the deviation bound of our true path from the chord of an interval with velocities v0 -> v1 (see the header)
inline double chordDeviationBound(const glm::dvec3& v0, const glm::dvec3& v1, double dt)
{
    return glm::length(v1 - v0) * dt / 4.0;
}
} // namespace game::navigation::avoidance
