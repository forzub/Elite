// Ctrl+J free flight: the ship's own global motion (server).
//
// Hub Tactical keeps the pilot inside a moving hub frame with the 500 m/s
// controlled envelope. Free flight releases that envelope without raising
// maxCombatSpeed: the ship's travel frame is detached from the hub (the
// TravelFrameSystem seam, as Orbital Transfer does) and from then on carries
// the ship's GLOBAL motion - position and velocity in the system's world
// frame (the galaxy reference), integrated under the real gravity of every
// body (GravityFieldSystem). Each tick, before the ship's own flight step,
// the local motion of the previous tick is folded into the frame and the
// frame advances by its velocity and gravity; the ordinary flight laws then
// act on a local state that starts at rest, so thrust accumulates without a
// cap and nothing pulls the velocity back to the envelope. The laws read
// motion.freeFlight: +/- are direct main thrust, alignment (Backspace exit)
// follows the global velocity, turns under thrust are rate-limited.
//
// HOME / END / INSERT latch the main thrust (full / brake flip-and-burn / off);
// they act through the ordinary '+' command and alignment controller
// (SharedShipPhysics). The pilot's velocity reference is the source hub's
// world velocity (HUD vector, Backspace exit, brake) - not the heliocentric one.
//
// Leaving free flight is a capture: only a hub whose capture envelope contains
// the ship (FreeFlightCaptureRadiusMeters) and only below the safe capture
// speed (the tactical envelope). Otherwise the ship stays in free flight and
// the refusal is replicated for the HUD. A capture re-bases the ship into the
// hub frame with its actual relative position and velocity (no snap).
#include "src/game/simulation/GameSimulation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "src/game/equipment/EquipmentDamage.h"
#include "src/game/navigation/GravityFieldSystem.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/ship/core/ShipDynamics.h"

namespace
{
// HubCaptureEnvelope (PROPOSAL until hubs carry their own traffic-control radius): the hub's
// approach / traffic zone in which its frame may capture a ship.
constexpr double FreeFlightCaptureRadiusMeters = 50000.0;
}

void GameSimulation::setFreeFlightThrust(EntityId shipId, int command)
{
    Ship* ship = getShip(shipId);
    if (!ship || command < 0 || command > 2)
        return;
    auto& motion = ship->core().transform().motion;
    if (!motion.freeFlight)
        return;
    motion.freeFlightThrust = static_cast<std::uint8_t>(command);
    motion.freeFlightCaptureRefusal = 0;      // a new pilot action replaces the last capture message
    if (command != 2 && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity)
        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
    std::cerr << "[FreeFlight] ship " << shipId.value << " thrust "
              << (command == 1 ? "FULL (HOME)" : command == 2 ? "BRAKE (END)" : "OFF (INSERT)") << std::endl;
}

bool GameSimulation::toggleFreeFlight(EntityId shipId)
{
    Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    auto& tr = ship->core().transform();
    auto& motion = tr.motion;

    const auto active = m_freeFlights.find(shipId);
    if (active != m_freeFlights.end())
    {
        // off = a capture: the nearest hub whose capture envelope contains the ship, at a safe speed
        const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
        const glm::dvec3 shipVel = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
        std::string nearest;
        double best = std::numeric_limits<double>::infinity();
        glm::dvec3 nearestVel(0.0);
        for (const auto& [hubId, frame] : m_hubNavigationFrames)
        {
            if (!frame.valid || frame.systemId != motion.systemId)
                continue;
            const double d = glm::length(frame.originMeters - shipWorld);
            if (d < best)
            {
                best = d;
                nearest = hubId;
                nearestVel = frame.velocityMetersPerSecond;
            }
        }
        if (nearest.empty() || best > FreeFlightCaptureRadiusMeters)
        {
            motion.freeFlightCaptureRefusal = 1;
            std::cerr << "[FreeFlight] ship " << shipId.value << ": no local reference frame (nearest hub " << nearest << " at "
                      << best / 1000.0 << " km, capture envelope " << FreeFlightCaptureRadiusMeters / 1000.0
                      << " km); staying in free flight" << std::endl;
            return false;
        }
        const double relSpeed = glm::length(shipVel - nearestVel);
        const double safeSpeed = game::ship::controlledSpeedLimitMps(ship->core().effectivePhysics());
        if (relSpeed > safeSpeed)
        {
            motion.freeFlightCaptureRefusal = 2;
            std::cerr << "[FreeFlight] ship " << shipId.value << ": UNSAFE capture by " << nearest << " - " << relSpeed
                      << " m/s relative at " << best / 1000.0 << " km (safe <= " << safeSpeed << " m/s); staying in free flight" << std::endl;
            return false;
        }
        std::cerr << "[FreeFlight] ship " << shipId.value << " OFF after " << active->second.elapsedSeconds << " s, peak "
                  << active->second.peakSpeedMps / 1000.0 << " km/s; nearest hub " << nearest << " at " << best / 1000.0 << " km\n";
        motion.freeFlight = false;
        motion.freeFlightThrust = 0;
        motion.freeFlightCaptureRefusal = 0;
        motion.freeFlightReferenceId.clear();
        motion.freeFlightReferenceVelocityMps = glm::dvec3(0.0);
        motion.freeFlightOrbitBodyId.clear();
        if (motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::BackwardToVelocity)
            motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
        m_freeFlights.erase(active);
        handOverToHub(shipId, *ship, nearest, nullptr, nullptr);
        return true;
    }

    if (motion.mode != game::navigation::MotionMode::HubTactical || !motion.travelFrame.valid ||
        m_orbitalTransfers.count(shipId) || isDockStationControlledShip(shipId))
    {
        std::cerr << "[FreeFlight] ship " << shipId.value << ": not in a state for free flight (mode/frame/docked/in transfer)\n";
        return false;
    }

    // on: detach and re-base at the ship (non-rotating, same instantaneous world kinematics)
    const glm::dvec3 shipWorld = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
    const glm::dvec3 shipWorldVel = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    game::navigation::KinematicFrame frame = motion.travelFrame;
    frame.originMeters = shipWorld;
    frame.linearVelocityMps = shipWorldVel;
    frame.linearAccelerationMps2 = glm::dvec3(0.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0);
    frame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.0);
    frame.frameId = "ship_free_" + std::to_string(shipId.value);
    game::navigation::TravelFrameSystem::detach(motion);
    motion.travelFrame = frame;
    motion.travelFrame.valid = true;
    motion.localPositionMeters = glm::dvec3(0.0);
    motion.localVelocityMps = glm::dvec3(0.0);
    motion.freeFlight = true;
    motion.freeFlightThrust = 0;
    motion.freeFlightCaptureRefusal = 0;
    // the pilot's velocity reference: the hub just left (relative speed ~0 at the start)
    motion.freeFlightReferenceId = motion.hubId;
    if (const auto* hub = hubNavigationFrame(motion.hubId); hub && hub->valid)
        motion.freeFlightReferenceVelocityMps = hub->velocityMetersPerSecond;
    // the Assisted VREL servo means nothing here: no held setpoint survives into free flight
    motion.assistedTargetSpeedHold = false;
    motion.assistedThrottleTrimWasActive = false;
    motion.targetForwardSpeedMps = 0.0;

    ActiveFreeFlight f;
    f.sourceHubId = motion.hubId;
    m_freeFlights[shipId] = f;
    std::cerr << "[FreeFlight] ship " << shipId.value << " ON at hub " << motion.hubId << ": global speed "
              << glm::length(shipWorldVel) / 1000.0 << " km/s\n";
    return true;
}

const GameSimulation::ActiveFreeFlight* GameSimulation::freeFlight(EntityId shipId) const
{
    const auto it = m_freeFlights.find(shipId);
    return it == m_freeFlights.end() ? nullptr : &it->second;
}

void GameSimulation::updateFreeFlights(double dt)
{
    if (dt <= 0.0)
        return;
    for (auto it = m_freeFlights.begin(); it != m_freeFlights.end();)
    {
        Ship* ship = getShip(it->first);
        if (!ship || !ship->core().transform().motion.freeFlight)
        {
            it = m_freeFlights.erase(it);
            continue;
        }
        auto& f = it->second;
        auto& tr = ship->core().transform();
        auto& motion = tr.motion;
        auto& frame = motion.travelFrame;

        // 1. fold last tick's local motion into the frame (non-rotating frame: a pure translation)
        frame.originMeters = frame.localToWorldPosition(motion.localPositionMeters);
        frame.linearVelocityMps = frame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
        motion.localPositionMeters = glm::dvec3(0.0);
        motion.localVelocityMps = glm::dvec3(0.0);

        // 2. the frame advances by its velocity under the real gravity of all bodies (velocity Verlet)
        const auto g0 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        frame.originMeters += frame.linearVelocityMps * dt + 0.5 * g0.accelerationMps2 * dt * dt;
        const auto g1 = game::navigation::GravityFieldSystem::sample(frame.originMeters, m_gravityBodies);
        frame.linearVelocityMps += 0.5 * (g0.accelerationMps2 + g1.accelerationMps2) * dt;
        frame.linearAccelerationMps2 = g1.accelerationMps2;
        frame.valid = true;

        // the reference hub moves on its orbit: keep its current velocity
        if (const auto* hub = hubNavigationFrame(motion.freeFlightReferenceId); hub && hub->valid)
            motion.freeFlightReferenceVelocityMps = hub->velocityMetersPerSecond;

        f.elapsedSeconds += dt;
        const double speed = glm::length(frame.linearVelocityMps);
        f.peakSpeedMps = std::max(f.peakSpeedMps, speed);
        f.primaryBodyId = g1.primaryBodyId;
        f.primaryAltitudeMeters = g1.primaryAltitudeMeters;

        // orbit relative to the dominant body (HUD: falling / climbing, circular speed, Pe / Ap)
        {
            const game::navigation::GravityBody* primary = nullptr;
            for (const auto& body : m_gravityBodies)
                if (body.id == g1.primaryBodyId)
                    primary = &body;
            const bool continuous = primary && f.lastPrimaryId == primary->id && dt > 0.0;
            if (primary && continuous && primary->gravitationalParameterM3s2 > 0.0)
            {
                const double mu = primary->gravitationalParameterM3s2;
                const glm::dvec3 bodyVelocity = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                const glm::dvec3 r = frame.originMeters - primary->centerMeters;
                const glm::dvec3 v = frame.linearVelocityMps - bodyVelocity;
                const double rLen = std::max(1.0, glm::length(r));
                const double speed = glm::length(v);
                const double energy = 0.5 * speed * speed - mu / rLen;
                const double h = glm::length(glm::cross(r, v));
                motion.freeFlightOrbitBodyId = primary->id;
                motion.freeFlightOrbitAltitudeM = rLen - primary->radiusMeters;
                // J speed is flown against the dominant body (HUD, alignment,
                // brake): a planet near it, the star beyond its influence
                motion.freeFlightReferenceId = primary->id;
                motion.freeFlightReferenceVelocityMps = bodyVelocity;
                motion.freeFlightOrbitSpeedMps = speed;
                motion.freeFlightOrbitRadialMps = glm::dot(v, r) / rLen;
                motion.freeFlightOrbitCircularMps = std::sqrt(mu / rLen);
                motion.freeFlightOrbitEscapeMps = std::sqrt(2.0 * mu / rLen);
                motion.freeFlightOrbitBound = energy < 0.0;
                if (motion.freeFlightOrbitBound)
                {
                    const double a = -mu / (2.0 * energy);
                    const double e = std::sqrt(std::max(0.0, 1.0 + 2.0 * energy * h * h / (mu * mu)));
                    motion.freeFlightOrbitPeriapsisAltM = a * (1.0 - e) - primary->radiusMeters;
                    motion.freeFlightOrbitApoapsisAltM = a * (1.0 + e) - primary->radiusMeters;
                }
                else
                {
                    const double p = h * h / mu;   // hyperbola: periapsis only
                    const double e = std::sqrt(std::max(0.0, 1.0 + 2.0 * energy * h * h / (mu * mu)));
                    motion.freeFlightOrbitPeriapsisAltM = p / (1.0 + e) - primary->radiusMeters;
                    motion.freeFlightOrbitApoapsisAltM = 0.0;
                }
            }
            else if (!primary)
            {
                motion.freeFlightOrbitBodyId.clear();
            }
            // DEV: ELITE_DEV_J_DROP="<altitude km>,<fraction of circular speed>" places the
            // ship once at that altitude, horizontal, at that speed (atmosphere / impact tests)
            if (primary && continuous && !f.devDropDone)
            {
                f.devDropDone = true;
                if (const char* drop = std::getenv("ELITE_DEV_J_DROP"); drop && *drop)
                {
                    double altKm = 120.0, fraction = 0.9;
                    std::sscanf(drop, "%lf,%lf", &altKm, &fraction);
                    const glm::dvec3 bodyVelocity = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                    const glm::dvec3 rHat = glm::normalize(frame.originMeters - primary->centerMeters);
                    glm::dvec3 vRel = frame.linearVelocityMps - bodyVelocity;
                    glm::dvec3 horizontal = vRel - rHat * glm::dot(vRel, rHat);
                    horizontal = glm::length(horizontal) > 1e-6 ? glm::normalize(horizontal) : glm::dvec3(1, 0, 0);
                    const double r = primary->radiusMeters + altKm * 1000.0;
                    frame.originMeters = primary->centerMeters + rHat * r;
                    frame.linearVelocityMps = bodyVelocity + horizontal * std::sqrt(primary->gravitationalParameterM3s2 / r) * fraction;
                    std::cerr << "[FreeFlight] DEV drop: altitude " << altKm << " km, " << fraction << " x circular" << std::endl;
                }
            }

            // ---- Assisted J: an orbital "aircraft" against the dominant body
            if (primary && continuous && motion.freeFlightSurfaceState == 0)
            {
                const glm::dvec3 bodyVelocity = (primary->centerMeters - f.lastPrimaryCenterMeters) / dt;
                const glm::dvec3 rHat = glm::normalize(frame.originMeters - primary->centerMeters);
                glm::dvec3 v = frame.linearVelocityMps - bodyVelocity;
                const auto& control = ship->core().control();
                const bool steering = std::abs(control.pitchInput) > 0.05f || std::abs(control.yawInput) > 0.05f;
                if (motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted)
                {
                    // the hull holds the velocity (prograde) when the pilot does not steer:
                    // thrust stays along the velocity while gravity bends the path
                    if (!steering && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::None &&
                        motion.freeFlightThrust != 2)
                        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::ForwardToVelocity;
                    if (steering && motion.velocityAlignmentMode == game::navigation::VelocityAlignmentMode::ForwardToVelocity)
                        motion.velocityAlignmentMode = game::navigation::VelocityAlignmentMode::None;
                    // steering turns the horizontal velocity toward the nose heading at
                    // the same speed (a banked turn); the vertical part is gravity's
                    if (steering)
                    {
                        const glm::dvec3 nose = -glm::dvec3(tr.orientation[2]);
                        glm::dvec3 noseH = nose - rHat * glm::dot(nose, rHat);
                        const glm::dvec3 vR = rHat * glm::dot(v, rHat);
                        const glm::dvec3 vH = v - vR;
                        const double speedH = glm::length(vH);
                        if (glm::length(noseH) > 1e-6 && speedH > 1.0)
                        {
                            noseH = glm::normalize(noseH);
                            const glm::dvec3 wanted = noseH * speedH;
                            glm::dvec3 dv = wanted - vH;
                            const double maxDv = 20.0 * dt;   // lateral authority of the assisted turn
                            if (glm::length(dv) > maxDv)
                                dv *= maxDv / glm::length(dv);
                            frame.linearVelocityMps += dv;
                            v += dv;
                        }
                    }
                }

                // ---- atmosphere, dense layers, solid surface
                // Exponential atmosphere rho = rho0 exp(-h / H), H = top / 12 (Earth: 100 km
                // top -> 8.3 km). rho0 = 1.225 kg/m3 for every body with an atmosphere until
                // the server reads the catalog surface pressure. Drag a = rho v^2 / (2 beta),
                // beta = m / (Cd A) = 800 kg/m2 (a blunt ship). The two thresholds are physical:
                //   safe altitude  - drag at circular speed reaches 0.1 m/s2 (the orbit decays fast)
                //   dense layers   - drag at circular speed reaches 1 g (heating, structural load)
                // Earth: ~108 km / ~71 km. Airless bodies: a 0.1 % R (min 1 km) terrain margin.
                constexpr double SeaLevelDensity = 1.225;
                constexpr double BallisticCoefficient = 800.0;
                const double atmosphereTop = std::max(0.0, primary->atmosphereRadiusMeters - primary->radiusMeters);
                const double scaleHeight = atmosphereTop / 12.0;
                const double vCircSurface = std::sqrt(primary->gravitationalParameterM3s2 / std::max(primary->radiusMeters, 1.0));
                const auto dragThresholdAltitude = [&](double accel) {
                    return scaleHeight * std::log(std::max(1.0, SeaLevelDensity * vCircSurface * vCircSurface /
                                                                    (2.0 * BallisticCoefficient * accel)));
                };
                const double safeAlt = atmosphereTop > 0.0 ? dragThresholdAltitude(0.1) : std::max(1000.0, primary->radiusMeters * 0.001);
                const double denseAlt = atmosphereTop > 0.0 ? dragThresholdAltitude(9.81) : 0.0;
                const double alt = motion.freeFlightOrbitAltitudeM;
                motion.freeFlightOrbitSafeAltM = safeAlt;
                motion.freeFlightOrbitDenseAltM = denseAlt;
                motion.freeFlightDragMps2 = 0.0;
                const double speed = glm::length(v);
                if (atmosphereTop > 0.0 && alt < atmosphereTop && speed > 0.1)
                {
                    const double density = SeaLevelDensity * std::exp(-std::max(alt, 0.0) / scaleHeight);
                    const double drag = std::min(density * speed * speed / (2.0 * BallisticCoefficient), speed / dt * 0.5);
                    frame.linearVelocityMps -= v / speed * drag * dt;
                    motion.freeFlightDragMps2 = drag;
                    // dense layers at speed: an impact on entry, then heating damage
                    // dense layers: deceleration above 1 g - an impact on entry, then heating
                    const bool dense = drag > 9.81;
                    if (dense && !f.insideDenseLayers)
                    {
                        ++motion.freeFlightImpactSerial;
                        for (const auto& m : ship->core().descriptor().modules)
                            if (m.subsystemId == "hull" || m.structuralKind == ModuleStructuralKind::StructuralFrame)
                                ship->core().moduleRuntime().addEquipmentDamage(
                                    m.moduleId, game::equipment::EquipmentDamageKind::Impact, 0.15);
                        std::cerr << "[FreeFlight] ship " << it->first.value << ": DENSE LAYERS of " << primary->id << " at "
                                  << speed / 1000.0 << " km/s, altitude " << alt / 1000.0 << " km - impact" << std::endl;
                    }
                    f.insideDenseLayers = dense;
                    if (dense)
                    {
                        f.heatingDamageSeconds += dt;
                        if (f.heatingDamageSeconds >= 1.0)
                        {
                            f.heatingDamageSeconds = 0.0;
                            const double heat = std::min(0.25, drag / 400.0 * (speed / 3000.0));
                            for (const auto& m : ship->core().descriptor().modules)
                                ship->core().moduleRuntime().addEquipmentDamage(
                                    m.moduleId, game::equipment::EquipmentDamageKind::Thermal, heat);
                        }
                    }
                }
                else
                {
                    f.insideDenseLayers = false;
                }

                // the real ground under the ship (terrain / sea of the rendered field) when low
                TerrainSample terrain;
                const bool terrainKnown = alt < 15000.0 && m_terrainHeight && m_terrainHeight(primary->id, rHat, terrain);
                const double ground = terrainKnown ? terrain.heightM : 0.0;
                motion.freeFlightGroundHeightM = ground;
                motion.freeFlightGroundMaterial = static_cast<std::int16_t>(terrainKnown ? terrain.material : -1);
                motion.freeFlightGroundSlopeDeg = static_cast<float>(terrainKnown ? terrain.slopeDeg : 0.0);
                motion.freeFlightGroundFeature = static_cast<std::uint8_t>(terrainKnown ? terrain.feature : 0);

                // solid ground and sea: contact by vertical speed (crush deceleration over ~2 m)
                if (alt <= ground)
                {
                    const double vVert = -glm::dot(v, rHat);
                    const double vHor = glm::length(v - rHat * glm::dot(v, rHat));
                    // the ground decides: soft regolith / sand / snow / soil give way (~3 m),
                    // rock / basalt / ice ~1.5 m, water a splashdown (~4 m, but the hull takes
                    // a beating), lava is lethal; a steep slope tips the ship over
                    const int material = terrainKnown ? terrain.material : 0;
                    double crushM = 1.5;
                    if (material == TerrainWater) crushM = 4.0;
                    else if (material == 2 || material == 3 || material == 4 || material == 7) crushM = 3.0;   // regolith, sand, soil, snow
                    const double crushG = vVert * vVert / (2.0 * crushM) / 9.81;
                    const double slope = terrainKnown ? terrain.slopeDeg : 0.0;
                    const bool lava = material == 9;
                    ++motion.freeFlightImpactSerial;
                    if (!lava && vVert < 8.0 && vHor < 40.0 && slope < 25.0 && material != TerrainWater)
                        motion.freeFlightSurfaceState = 1;                 // landed
                    else
                    {
                        const bool survivable = !lava && crushG < 30.0 && vHor < 150.0;
                        motion.freeFlightSurfaceState = survivable ? 1 : 2;
                        double hit = survivable ? 0.35 : 1.0;
                        if (survivable && slope >= 25.0)
                            hit = std::max(hit, 0.5);                      // tipped over on the slope
                        if (survivable && material == TerrainWater)
                            hit = std::max(hit, 0.25);
                        for (const auto& m : ship->core().descriptor().modules)
                            ship->core().moduleRuntime().addEquipmentDamage(
                                m.moduleId, game::equipment::EquipmentDamageKind::Impact, hit);
                    }
                    std::cerr << "[FreeFlight] ship " << it->first.value << ": SURFACE CONTACT on " << primary->id
                              << " vertical " << vVert << " m/s horizontal " << vHor << " m/s (" << crushG << " g), ground material "
                              << material << " slope " << slope << " deg feature " << (terrainKnown ? terrain.feature : 0) << " - "
                              << (motion.freeFlightSurfaceState == 1 ? "landed / survivable" : "CRASH") << std::endl;
                    // rest on the surface, moving with the body
                    frame.originMeters = primary->centerMeters + rHat * (primary->radiusMeters + ground);
                    frame.linearVelocityMps = bodyVelocity;
                    motion.freeFlightThrust = 0;
                    ship->core().refreshRuntimeState();
                }
            }
            if (primary && motion.freeFlightSurfaceState != 0)
            {
                // on the surface: carried with the body until thrust lifts the ship off
                const glm::dvec3 rHat = glm::normalize(frame.originMeters - primary->centerMeters);
                const glm::dvec3 bodyVelocity = f.lastPrimaryId == primary->id && dt > 0.0
                    ? (primary->centerMeters - f.lastPrimaryCenterMeters) / dt : frame.linearVelocityMps;
                if (motion.freeFlightSurfaceState == 1 && motion.freeFlightThrust == 1)
                    motion.freeFlightSurfaceState = 0;                     // lift off
                else
                {
                    frame.originMeters = primary->centerMeters + rHat * (primary->radiusMeters + motion.freeFlightGroundHeightM + 1.0);
                    frame.linearVelocityMps = bodyVelocity;
                }
            }
            f.lastPrimaryId = primary ? primary->id : std::string();
            f.lastPrimaryCenterMeters = primary ? primary->centerMeters : glm::dvec3(0.0);
        }

        if (g1.insideAtmosphere || (!g1.primaryBodyId.empty() && g1.primaryAltitudeMeters < 0.0))
        {
            if (f.elapsedSeconds - f.lastLogSeconds >= 2.0)
            {
                f.lastLogSeconds = f.elapsedSeconds;
                std::cerr << "[FreeFlight] ship " << it->first.value << ": " << (g1.primaryAltitudeMeters < 0.0 ? "BELOW THE SURFACE of " : "in the atmosphere of ")
                          << g1.primaryBodyId << ", altitude " << g1.primaryAltitudeMeters / 1000.0 << " km" << std::endl;
            }
        }
        else if (f.elapsedSeconds - f.lastLogSeconds >= 5.0)
        {
            f.lastLogSeconds = f.elapsedSeconds;
            std::cerr << "[FreeFlight] ship " << it->first.value << " t+" << f.elapsedSeconds << " s: global speed " << speed / 1000.0
                      << " km/s, gravity " << glm::length(g1.accelerationMps2) << " m/s2 (primary " << g1.primaryBodyId << ", alt "
                      << g1.primaryAltitudeMeters / 1000.0 << " km), main " << glm::length(motion.mainEngineAccelerationMps2) << " m/s2 (requested "
                      << glm::length(motion.requestedMainEngineAccelerationMps2) << "), thrust cmd " << int(motion.freeFlightThrust)
                      << ", align " << int(motion.velocityAlignmentMode) << ", '+' " << tr.targetSpeedRate << ", vs reference "
                      << glm::length(frame.linearVelocityMps - motion.freeFlightReferenceVelocityMps) << " m/s" << std::endl;
        }
        ++it;
    }
}
