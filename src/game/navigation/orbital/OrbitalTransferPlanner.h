#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/orbits/OrbitalMotion.h"

namespace game::navigation::orbital
{
/*
    Orbital Transfer: hub -> hub travel around one central body (Earth orbital
    network) as a high-level plan that ordinary physics executes.

    The plan is a C1 chain of cubic Hermite segments in the body-centred frame
    (the body's own free-fall frame: positions and velocities RELATIVE to the
    central body). It starts at the ship's actual state and ends at the
    destination hub's approach point AT THE ARRIVAL EPOCH with the hub's
    velocity at that epoch (+ a small closing speed): the destination is a
    moving target evaluated from its OrbitalMotion ephemeris, never a static
    coordinate.

    Geometry: intermediate states follow a guide curve that sweeps the start
    orbit plane into the target orbit plane while the radius blends between
    the two altitudes, so the tunnel arcs OVER the body and never cuts through
    it; every sample is checked against the minimum safe altitude.

    Speed envelope: not the tactical 500 m/s and not the 0.1c cruise limit.
    The arrival epoch is the smallest one for which the thrust the plan needs
    (reference acceleration minus real gravity) stays within the transfer
    thrust envelope, and within the gentle zone envelope while the ship is
    still inside the departure / arrival traffic zones. Peak speed is an
    OUTPUT of curvature, lateral acceleration, braking and target velocity.

    Execution (stepCarrier): the reference acceleration is fed forward, real
    gravity is subtracted, a PD term corrects drift, the result is clamped to
    the ship's main-engine capability and integrated together with gravity.
    With the thrust cut the carrier simply coasts on a gravitational
    trajectory (orbit or fall) - nothing here assumes the ship stays up.
*/
struct CentralBody
{
    double muM3s2 = 3.986004418e14;
    double radiusMeters = 6371000.0;
    double minAltitudeMeters = 250000.0;
};

struct TransferLimits
{
    // Sustained transfer thrust envelope (crew / fuel), well below the
    // engine's peak; zone envelope while the ship is near a hub.
    double maxThrustAccelMps2 = 18.0;
    double zoneThrustAccelMps2 = 4.9;
    double departureZoneMeters = 25000.0;
    double arrivalZoneMeters = 25000.0;
    // Arrival point: trailing the destination hub along its -prograde by this
    // distance, closing at closingSpeedMps.
    double approachOffsetMeters = 15000.0;
    double closingSpeedMps = 20.0;
    // Delta-v budget of one transfer: without it the shortest plan "hovers" against
    // gravity and lets the destination come round (physically valid, absurdly costly).
    // PROPOSAL: Cobra-class fusion drive budget for one in-orbit transfer; large LEO plane
    // changes (40-60 deg) alone cost 6-10 km/s - that is orbital mechanics, not a planner flaw.
    double maxDeltaVMps = 25000.0;
    double maxDurationSeconds = 6.0 * 3600.0;
    double minDurationSeconds = 240.0;
};

struct KinematicSample
{
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 accelerationMps2 {0.0};
};

struct HermiteSegment
{
    double t0 = 0.0;
    double t1 = 0.0;
    glm::dvec3 p0 {0.0};
    glm::dvec3 v0 {0.0};
    glm::dvec3 p1 {0.0};
    glm::dvec3 v1 {0.0};

    KinematicSample evaluate(double t) const;
};

struct OrbitalTransferPlan
{
    bool feasible = false;
    std::string reason;
    double startTimeSeconds = 0.0;
    double arrivalTimeSeconds = 0.0;
    std::vector<HermiteSegment> segments;

    double sweepRad = 0.0;
    double peakSpeedMps = 0.0;            // relative to the central body
    double peakSpeedVsTargetMps = 0.0;    // relative to the destination hub at the same epoch
    double maxThrustAccelMps2 = 0.0;      // what the plan needs
    double minAltitudeMeters = 0.0;
    double deltaVMps = 0.0;
    double pathLengthMeters = 0.0;
    int candidatesTried = 0;

    glm::dvec3 arrivalPositionMeters {0.0};   // planned approach point (relative to body)
    glm::dvec3 arrivalVelocityMps {0.0};
    glm::dvec3 targetPositionAtArrivalMeters {0.0};
    glm::dvec3 targetVelocityAtArrivalMps {0.0};

    // Tunnel centre line for the maps (relative to the body), one sample per ~1/160 of the path.
    std::vector<glm::dvec3> tunnelMeters;

    double durationSeconds() const { return arrivalTimeSeconds - startTimeSeconds; }
    KinematicSample evaluate(double t) const;
};

// Ephemeris of a hub relative to its parent body (centerMeters forced to 0).
glm::dvec3 targetPositionAt(const world::orbits::OrbitalMotion& motion, double t);
glm::dvec3 targetVelocityAt(const world::orbits::OrbitalMotion& motion, double t);
glm::dvec3 gravityAt(const CentralBody& body, const glm::dvec3& positionMeters);

OrbitalTransferPlan planOrbitalTransfer(
    const CentralBody& body,
    const glm::dvec3& startPositionMeters,
    const glm::dvec3& startVelocityMps,
    double startTimeSeconds,
    const world::orbits::OrbitalMotion& targetMotion,
    const TransferLimits& limits
);

struct TransferCarrier
{
    glm::dvec3 positionMeters {0.0};   // relative to the central body
    glm::dvec3 velocityMps {0.0};
    glm::dvec3 accelerationMps2 {0.0}; // thrust + gravity of the last step
    glm::dvec3 thrustMps2 {0.0};
    double timeSeconds = 0.0;
    double deltaVUsedMps = 0.0;
};

// One fixed step of the executor. thrustEnabled=false lets the carrier coast
// under gravity only (abort / engine failure).
void stepCarrier(
    const OrbitalTransferPlan& plan,
    const CentralBody& body,
    TransferCarrier& carrier,
    double dtSeconds,
    double engineMaxAccelMps2,
    bool thrustEnabled = true
);

// Interplanetary leg (e.g. Earth -> Ceres): the physical brachistochrone that an
// abstracted, time-compressed transit stands for.
struct InterplanetaryEstimate
{
    double distanceMeters = 0.0;
    double accelMps2 = 0.0;
    double durationSeconds = 0.0;
    double peakSpeedMps = 0.0;
    double deltaVMps = 0.0;
};
InterplanetaryEstimate brachistochrone(double distanceMeters, double accelMps2);
}
