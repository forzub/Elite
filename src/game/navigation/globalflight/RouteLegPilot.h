#pragma once

// The route-leg pilot (NAVIGATION_V2 §1: the CLIENT flies the route, the server executes its commands).
// One leg of a planned route on the J drive, finished matched to the destination's motion:
//   hub    - the edge of the hub keep-out (HubArrivalM) with its velocity, then the stabilization
//            (a hub capture);
//   orbit  - the tangent point of a circular orbit at the given altitude (never below the J floor),
//            the circular velocity there, then the stabilization (the own ballistic frame);
//   point  - at rest against the dominant body at a world point; a guide point (pass-through) is
//            flown through, braking for the rest of the route;
//   ship   - a rendezvous just outside the target ship's keep-out, its velocity matched;
//   take-off (a leg from the ground) - a vertical main-engine climb to the J floor first.
// Near any object the J field is gentle (JDrive.h keep-out) and pushes out against that object;
// every massive body on the way is gone round. Moved from GameSimulation::updateFreeFlightLeg and
// the take-off of updateFreeFlights (2026-11-06) behaviour-preserving; landing legs are the body
// autopilot's (BodyAutopilot.h).
#include <cstdint>
#include <functional>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/globalflight/BodyAutopilot.h"
#include "src/game/navigation/route/RouteWire.h"

namespace game::navigation::globalflight
{

struct RouteLegResolver
{
    // a hub's world position / velocity now
    std::function<bool(const std::string& hubId, glm::dvec3& positionM, glm::dvec3& velocityMps)> hub;
    // another ship's world position / velocity now
    std::function<bool(std::uint64_t entityId, glm::dvec3& positionM, glm::dvec3& velocityMps)> ship;
    // the hub whose J keep-out holds this point (a rendezvous target parked at a hub)
    std::function<bool(const glm::dvec3& pointM, std::string& hubId, glm::dvec3& hubPositionM, glm::dvec3& hubVelocityMps)> hubAround;
};

class RouteLegPilot
{
public:
    // alreadyInJ: the route goes on past a guide point (the drive stays at full strength);
    // onGround: landed - the take-off first
    void start(const route::RoutePoint& leg, double remainingAfterM, bool alreadyInJ, bool onGround, const std::string& departureHubId,
               std::uint32_t shipId);
    void stop() { m_active = false; m_takeOff = false; }
    bool active() const { return m_active; }
    bool takingOff() const { return m_takeOff; }
    bool arrived() const { return m_arrived; }   // matched to a stop: the stabilization next
    bool passed() const { return m_passed; }     // a guide point passed: the next leg at once
    double seconds() const { return m_seconds; }
    // how long the full drive has ramped (1 g doubling every 2 s)
    double driveSeconds() const { return m_driveStartSeconds == -1.0 ? 0.0 : m_seconds - m_driveStartSeconds; }
    const route::RoutePoint& leg() const { return m_leg; }

    BodyAutopilotOutput step(const BodyAutopilotEnvironment& env, const RouteLegResolver& resolve, const BodyAutopilotShip& ship,
                             double dt);

private:
    BodyAutopilotOutput stepTakeOff(const BodyAutopilotEnvironment& env, const BodyAutopilotShip& ship, double dt);

    route::RoutePoint m_leg;
    double m_remainingAfterM = 0.0;
    bool m_active = false;
    bool m_takeOff = false;
    bool m_arrived = false;
    bool m_passed = false;
    double m_seconds = 0.0;
    double m_lastLog = -1.0e30;
    double m_driveStartSeconds = -1.0;
    std::string m_departureHubId;
    glm::dvec3 m_orbitNormal {0.0};
    bool m_haveLastBody = false;
    glm::dvec3 m_lastBodyCentre {0.0};
    std::string m_lastPrimaryId;
    glm::dvec3 m_lastPrimaryCentre {0.0};
    std::uint32_t m_shipId = 0;
    bool m_toldTargetInHub = false;
};

} // namespace game::navigation::globalflight
