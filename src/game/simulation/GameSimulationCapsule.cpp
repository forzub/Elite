// The cockpit-capsule (author 2026-11-24, memory cockpit-capsule-rescue; navigation/rules/CapsuleRules.h): the hull is
// lost, the pilot alive in the cockpit - the ship BECOMES the capsule (ShipCore::becomeCockpitCapsule: the cockpit
// fragment's pose and velocity, the cockpit its free root). It then flies by its own motor / jets / reaction mass
// (ShipCore capsuleFlying -> capsuleParams); the wreck's fragments are left where they fly (their offsets are relative
// to the ship, so the velocity the capsule's own THRUST gave is taken back out of them each tick - gravity they share)
// and dropped beyond 20 km.
#include "src/game/simulation/GameSimulation.h"

#include "src/game/navigation/rules/CapsuleRules.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/DeckFrame.h"
#include <unordered_map>

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

// The turning deck under a ship (author 2026-11-25, memory ring-rim-landing; DeckFrame.h): a hub-attached object of the
// ship's hub turning on its own (its spin well above the hub frame's orbital turn - a ring city with its wheel) and the
// ship within its reach (the ring's radius + 600 m from the axis, 2 km along it): the deck's axis point and spin in the
// ship's travel frame. The control law then holds the velocity relative to the deck; the physics pays the following.
void GameSimulation::updateDeckFrame(EntityId shipId, Ship& ship)
{
    auto& motion = ship.core().transform().motion;
    motion.deckActive = false;
    if (motion.mode != game::navigation::MotionMode::HubTactical || !motion.travelFrame.valid || motion.hubId.empty())
        return;
    // the deck is the PILOT's (hand flight, a hand-flown pad after the service's hand-over); an autopilot route is planned
    // and flown in the hub frame - the deck shift made it chase the deck instead of its route (live: stuck at 771 m)
    const auto& control = ship.core().control();
    if (!control.deckRelative || control.navigationAccelerationDemandValid || control.navigationVelocityTargetValid ||
        control.globalFlightCommandValid || m_padLandings.count(shipId))
        return;
    const auto& frame = motion.travelFrame;
    const glm::dvec3 me = world::coordinates::fullMeters(ship.core().transform().worldPosition);
    for (const auto& [oid, obj] : m_staticObjects)
    {
        if (!obj.attachedToHub || obj.hubId != motion.hubId || obj.systemId != motion.systemId)
            continue;
        const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(obj.hubModuleId);
        if (!plan || !plan->ring().valid || plan->ring().periodSeconds <= 0.0)
            continue;   // a deck is a TURNING RING city only (a belt hub's spinning bodies are not decks)
        const glm::dvec3 spin = glm::dvec3(obj.angularVelocity) - frame.angularVelocityWorldRadPerSecond;
        const double rate = glm::length(spin);
        // no fixed rate threshold: a big ring turns slowly (1 g at R 100 km is 0.0099 rad/s) - the plan says it turns
        if (rate <= 1.0e-9)
            continue;   // the ring is still (ELITE_DEV_RING_SPIN=0)
        // the deck's space is the ring's own, from its plan (hubs differ in size): inside the rim (r <= R) and across
        // the strip's width (the plan's bounds, strip z = the city frame's z = along the axis)
        const double radius = plan->ring().radius;
        const glm::dvec4 bounds = plan->bounds();   // x0, z0, x1, z1
        const glm::dvec3 centre = world::coordinates::fullMeters(obj.worldPosition);
        const glm::dvec3 axis = spin / rate;
        const glm::dvec3 rel = me - centre;
        const glm::dvec3 cityZ = glm::dvec3(glm::mat3(obj.orientation) * glm::vec3(0.0f, 0.0f, 1.0f));
        const double along = glm::dot(rel, cityZ);   // the strip's z (signed by the city's +Z, not the spin's sense)
        const double fromAxis = glm::length(rel - axis * glm::dot(rel, axis));
        const bool hasWidth = bounds.w != bounds.y;
        if (fromAxis > radius || (hasWidth && (along < std::min(bounds.y, bounds.w) || along > std::max(bounds.y, bounds.w))))
            continue;
        motion.deckActive = true;
        motion.deckAxisPointLocal = frame.worldToLocalPosition(centre);
        motion.deckOmegaLocal = frame.worldToLocalVector(spin);
        static std::unordered_map<std::uint64_t, double> loggedAt;
        auto& at = loggedAt[ship.core().registry().instanceId];
        if (serverTime() - at >= 5.0)
        {
            at = serverTime();
            const glm::dvec3 rel = motion.localVelocityMps - game::navigation::deckVelocityLocal(motion);
            std::cerr << "[Deck] ship instance " << ship.core().registry().instanceId << " over the turning deck of "
                      << obj.hubModuleId << ": " << fromAxis << " m from the axis, " << glm::length(rel)
                      << " m/s relative to the deck, following it takes "
                      << glm::length(game::navigation::deckFollowAccelerationLocal(motion)) << " m/s2" << std::endl;
        }
        return;
    }
}
