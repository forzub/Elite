// The cockpit-capsule (author 2026-11-24, memory cockpit-capsule-rescue; navigation/rules/CapsuleRules.h): the hull is
// lost, the pilot alive in the cockpit - the ship BECOMES the capsule (ShipCore::becomeCockpitCapsule: the cockpit
// fragment's pose and velocity, the cockpit its free root). It then flies by its own motor / jets / reaction mass
// (ShipCore capsuleFlying -> capsuleParams); the wreck's fragments are left where they fly (their offsets are relative
// to the ship, so the velocity the capsule's own THRUST gave is taken back out of them each tick - gravity they share)
// and dropped beyond 20 km.
#include "src/game/simulation/GameSimulation.h"

#include "src/game/navigation/rules/CapsuleRules.h"

#include <iostream>

void GameSimulation::updateCapsules(double dt)
{
    for (auto it = m_capsuleTakeoverPending.begin(); it != m_capsuleTakeoverPending.end();)
    {
        Ship* ship = getShip(*it);
        if (!ship || ship->core().transform().motion.systemId != m_activeCelestialSystemId)
        {
            if (!ship)
                it = m_capsuleTakeoverPending.erase(it);
            else
                ++it;
            continue;
        }
        if (ship->core().becomeCockpitCapsule())
            markShipGraphDirty(*it);
        else
            std::cerr << "[Capsule] ship " << it->value << ": no cockpit fragment to take over" << std::endl;
        m_capsuleWake.erase(*it);
        it = m_capsuleTakeoverPending.erase(it);
    }
    if (dt <= 0.0)
        return;
    for (const EntityId id : m_playerControlledShipIds)
    {
        Ship* ship = getShip(id);
        if (!ship || !ship->core().capsuleFlying() || ship->core().transform().motion.systemId != m_activeCelestialSystemId)
        {
            m_capsuleWake.erase(id);
            continue;
        }
        const auto& tr = ship->core().transform();
        const glm::dvec3 p = world::coordinates::fullMeters(tr.worldPosition);
        const glm::dvec3 v = tr.motion.worldVelocityMps;
        auto [w, fresh] = m_capsuleWake.try_emplace(id, CapsuleWake {p, v, 0.0});
        if (!fresh)
        {
            // the fragments moved by their relative velocity (v_rel = v_frag - v_prev); the capsule moved by p - p_prev
            const int dropped = ship->core().leaveFragmentsBehind(dt);
            if (dropped > 0)
                markShipGraphDirty(id);
            w->second.position = p;
            w->second.velocity = v;
            w->second.logClockS += dt;
            if (w->second.logClockS >= 5.0)
            {
                w->second.logClockS = 0.0;
                std::cerr << "[Capsule] ship " << id.value << ": tumble "
                          << glm::length(glm::vec3(tr.pitchRate, tr.yawRate, tr.rollRate)) << " rad/s, reaction mass "
                          << tr.motion.capsuleDeltaVLeftMps << " m/s of dv, wreck fragments "
                          << ship->core().detachedFragmentRuntime().fragments().size() << std::endl;
            }
        }
    }
}
