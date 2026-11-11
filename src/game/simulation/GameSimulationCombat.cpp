#include <cstring>
#include <random>
#include "src/game/navigation/rules/JumpRules.h"
#include <fstream>
// GameSimulation: combat - lasers and missiles on the runtime hit volumes
// (game::combat penetration model), impacts, the shell -> full ship promotion
// of a ship under fire, the DEV combat target.
//
// Lasers fire along the hull's forward axis from every live weapon muzzle
// (attachment points of kind WeaponMuzzle on a live module), converging at the
// boresight target (or 1 km). A beam heats a spot on the first volume of the
// nearest ship it meets; damage reaches the ShipCore every 0.2 s or at once on
// a burn-through. Missiles: guided at launch by the seeker cone, gravity of
// the system, burning-through warheads on contact (shaped jet / slug).
#include "src/game/simulation/GameSimulationInternal.h"

#include "src/game/combat/Penetration.h"
#include "src/game/combat/WeaponCatalog.h"
#include "src/game/simulation/activation/ActivationClaim.h"

#include <map>

namespace
{
using game::combat::weaponCatalog;

double shipRadiusM(const ShipDescriptor& d)
{
    const auto& L = d.logicalDimensionsValue;
    return 0.5 * std::sqrt(double(L.length) * L.length + double(L.width) * L.width + double(L.height) * L.height) + 2.0;
}

// nearest t >= 0 where the ray meets the sphere (false: misses)
bool raySphere(const glm::dvec3& o, const glm::dvec3& d, const glm::dvec3& c, double r, double& t)
{
    const glm::dvec3 oc = o - c;
    const double b = glm::dot(oc, d);
    const double cc = glm::dot(oc, oc) - r * r;
    const double disc = b * b - cc;
    if (disc < 0.0)
        return false;
    const double s = std::sqrt(disc);
    t = -b - s;
    if (t < 0.0)
        t = -b + s;
    return t >= 0.0;
}

glm::dmat3 basisOf(const ShipTransform& tr)
{
    return glm::dmat3(glm::mat3(tr.orientation));
}

// a reactor module (not its structural frame "ship_frame_reactor"); service = the ship's service reactor
bool isReactorModule(const std::string& id)
{
    return id.size() > 8 && id.compare(id.size() - 8, 8, "_reactor") == 0 && id.find("frame") == std::string::npos;
}
bool isPropulsionReactorModule(const std::string& id)
{
    return isReactorModule(id) && id.find("service") == std::string::npos;
}
} // namespace

GameSimulation::CombatLoadout& GameSimulation::combatLoadout(EntityId shipId)
{
    auto it = m_combatLoadouts.find(shipId);
    if (it == m_combatLoadouts.end())
    {
        const auto& cat = weaponCatalog();
        CombatLoadout l;
        l.laserId = cat.defaultLaser;
        l.missileId = cat.defaultMissile;
        l.missiles = cat.defaultMissiles;
        if (const char* v = std::getenv("ELITE_DEV_LASER"); v && *v && cat.laser(v))
            l.laserId = v;
        if (const char* v = std::getenv("ELITE_DEV_MISSILE"); v && *v && cat.missile(v))
            l.missileId = v;
        it = m_combatLoadouts.emplace(shipId, l).first;
    }
    return it->second;
}

void GameSimulation::setLaserFiring(EntityId shipId, bool firing)
{
    if (firing)
        m_laserFiring.insert(shipId);
    else
        m_laserFiring.erase(shipId);
    std::cerr << "[Combat] ship " << shipId.value << ": laser " << (firing ? "ON " : "off ")
              << combatLoadout(shipId).laserId << std::endl;
}

void GameSimulation::cycleCombatWeapon(EntityId shipId, bool missile)
{
    const auto& cat = weaponCatalog();
    CombatLoadout& l = combatLoadout(shipId);
    if (missile)
    {
        std::size_t i = 0;
        while (i < cat.missiles.size() && cat.missiles[i].id != l.missileId)
            ++i;
        l.missileId = cat.missiles[(i + 1) % cat.missiles.size()].id;
    }
    else
    {
        std::size_t i = 0;
        while (i < cat.lasers.size() && cat.lasers[i].id != l.laserId)
            ++i;
        l.laserId = cat.lasers[(i + 1) % cat.lasers.size()].id;
    }
    std::cerr << "[Combat] ship " << shipId.value << ": laser " << l.laserId << ", missile " << l.missileId << " x"
              << l.missiles << std::endl;
}

bool GameSimulation::launchMissile(EntityId shipId)
{
    const Ship* ship = getShip(shipId);
    if (!ship)
        return false;
    CombatLoadout& l = combatLoadout(shipId);
    const auto* spec = weaponCatalog().missile(l.missileId);
    if (!spec || l.missiles <= 0)
    {
        std::cerr << "[Combat] ship " << shipId.value << ": no missiles" << std::endl;
        return false;
    }
    const ShipTransform& tr = ship->core().transform();
    const glm::dmat3 R = basisOf(tr);
    const glm::dvec3 pos = world::coordinates::fullMeters(tr.worldPosition);
    const glm::dvec3 fwd = R * glm::dvec3(0, 0, -1);

    // the seeker: the ship nearest the boresight inside the cone and range
    EntityId target {};
    double best = std::cos(glm::radians(spec->seekerDeg));
    const auto consider = [&](EntityId id, const glm::dvec3& p) {
        if (id == shipId)
            return;
        const glm::dvec3 to = p - pos;
        const double dist = glm::length(to);
        if (dist < 1.0 || dist > spec->rangeM)
            return;
        const double c = glm::dot(to / dist, fwd);
        if (c > best)
        {
            best = c;
            target = id;
        }
    };
    for (const auto& [id, s] : m_ships)
        consider(id, world::coordinates::fullMeters(s->core().transform().worldPosition));
    for (const auto& [id, s] : m_shipShells)
        consider(id, world::coordinates::fullMeters(s.transform.worldPosition));

    // from the missile rack (or under the hull), pushed clear of it
    glm::dvec3 rackLocal(0.0, -2.0, -2.0);
    for (const auto& a : ship->core().desc().attachments)
        if (a.enabled && a.kind == ShipAttachmentKind::MissileRack)
        {
            rackLocal = glm::dvec3(a.localPosition);
            for (const auto& v : ship->core().hitComponent().volumes)
                if (!v.supportLinkVolume && v.moduleId == a.parentModuleId)
                {
                    // the first volume of the carrying module only (summing every volume of a multi-volume module
                    // threw the launch point tens of metres off - the missiles then missed the 1.5 km target)
                    rackLocal += glm::dvec3(v.center) - glm::dvec3(0.0, v.halfSize.y + 1.0, 0.0);
                    break;
                }
            std::cerr << "[Combat] missile rack at " << rackLocal.x << " " << rackLocal.y << " " << rackLocal.z << " m (ship frame)" << std::endl;
            break;
        }
    CombatMissile m;
    m.id = m_nextCombatSerial++;
    m.shooter = shipId;
    m.target = target;
    m.specId = spec->id;
    m.positionW = pos + R * rackLocal;
    const auto track = m_combatTracks.find(shipId);
    const glm::dvec3 shooterVel = track != m_combatTracks.end() && track->second.valid ? track->second.velocityW : glm::dvec3(0.0);
    m.velocityW = shooterVel + fwd * 60.0 - (R * glm::dvec3(0, 1, 0)) * 8.0;
    {
        const auto& mo = tr.motion;
        glm::dvec3 targetTrack(0.0);
        if (const auto t = m_combatTracks.find(target); t != m_combatTracks.end())
            targetTrack = t->second.velocityW;
        std::cerr << "[Combat] launch diag: shooter track " << shooterVel.x << " " << shooterVel.y << " " << shooterVel.z
                  << " | motion.world " << mo.worldVelocityMps.x << " " << mo.worldVelocityMps.y << " " << mo.worldVelocityMps.z
                  << " | reference " << mo.referenceVelocityMps.x << " " << mo.referenceVelocityMps.y << " " << mo.referenceVelocityMps.z
                  << " | target track " << targetTrack.x << " " << targetTrack.y << " " << targetTrack.z << std::endl;
    }
    m.heading = fwd;
    m_combatMissiles.push_back(m);
    --l.missiles;
    std::cerr << "[Combat] ship " << shipId.value << ": MISSILE " << m.id << " " << spec->name << " -> "
              << (target.value ? "target " + std::to_string(target.value) : std::string("unguided")) << " ("
              << l.missiles << " left)" << std::endl;
    if (target.value && !getShip(target) && shipShell(target))
        (void)materializeShipShell(target);
    if (target.value)
        (void)promoteShipToDetailedSimulation(target);
    return true;
}

EntityId GameSimulation::spawnCombatTarget(EntityId nearShipId, double distanceM)
{
    const Ship* near = getShip(nearShipId);
    if (!near)
        return {};
    const auto& ptr = near->core().transform();
    const glm::dvec3 origin = world::coordinates::fullMeters(ptr.worldPosition);
    const glm::dvec3 forward(ptr.forward());
    const glm::dvec3 position = origin + forward * distanceM;
    ShipInitData init;
    init.visual.shipName = "Мишень";
    init.registry.instanceId = 970000 + m_nextCombatSerial;
    init.registry.ownerName = "combat target";
    init.registry.shipRole = ShipRoleType::Civilian;
    const EntityId id = spawnShip(ShipRole::NPC, ptr.motion.systemId, EliteCobraMk1::EliteCobraMk1Descriptor(), position, init,
                                  ptr.orientation);
    Ship* ship = id.value ? getShip(id) : nullptr;
    if (!ship)
        return {};
    m_combatTargetDummies.insert(id);
    auto& tr = ship->core().transform();
    game::navigation::ReferenceFrame frame;
    frame.type = game::navigation::ReferenceFrameType::OrbitalHub;
    frame.systemId = ptr.motion.systemId;
    frame.hubId = ptr.motion.hubId;
    frame.localOffsetMeters =
        ptr.motion.localPositionMeters + ptr.motion.travelFrame.worldToLocalVector(position - origin);
    if (frame.hubId.empty() || !placeShipInReferenceFrame(id, frame))
    {
        tr.motion.mode = game::navigation::MotionMode::HubTactical;
        tr.motion.travelFrame = ptr.motion.travelFrame;
        tr.motion.localPositionMeters = tr.motion.travelFrame.worldToLocalPosition(position);
        tr.motion.localVelocityMps = glm::dvec3(0.0);
        tr.motion.worldVelocityMps = ptr.motion.worldVelocityMps;
        tr.motion.referenceVelocityMps = ptr.motion.referenceVelocityMps;
        tr.referenceVelocityMetersPerSecond = ptr.referenceVelocityMetersPerSecond;
    }
    // broadside to the shooter
    tr.orientation = ptr.orientation * glm::rotate(glm::mat4(1.0f), glm::half_pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
    (void)promoteShipToDetailedSimulation(id);
    {
        std::string reactors;
        for (const auto& m : static_cast<const world::modules::ObjectModuleRuntime&>(ship->core().moduleRuntime()).modules())
            if (isReactorModule(m.moduleId))
                reactors += " " + m.moduleId;

    }
    std::cerr << "[Combat] target Cobra id=" << id.value << " " << distanceM << " m ahead of ship " << nearShipId.value
              << ", hub " << frame.hubId << ", " << ship->core().hitComponent().volumes.size() << " hit volumes; shooter mode "
              << static_cast<int>(ptr.motion.mode) << " frame " << ptr.motion.travelFrame.frameId << ", target mode "
              << static_cast<int>(tr.motion.mode) << " frame " << tr.motion.travelFrame.frameId << std::endl;
    return id;
}

void GameSimulation::recordCombatImpact(EntityId targetId, const world::coordinates::WorldPosition& where, double energyMJ,
                                        std::uint8_t kind, const std::string& moduleId)
{
    auto& list = m_combatImpacts[targetId];
    game::simulation::CombatImpactSnapshot imp;
    imp.serial = m_nextCombatSerial++;
    imp.where = where;
    imp.energyMJ = static_cast<float>(energyMJ);
    imp.kind = kind;
    imp.moduleId = moduleId;
    list.push_back(imp);
    if (list.size() > 12)
        list.erase(list.begin());
}

void GameSimulation::flushCombatDamage(EntityId targetId, game::damage::DamageType type,
                                       const world::coordinates::WorldPosition& where)
{
    Ship* ship = getShip(targetId);
    auto it = m_burnStates.find(targetId);
    if (!ship || it == m_burnStates.end() || it->second.pendingHealthLoss.empty())
        return;
    auto pending = std::exchange(it->second.pendingHealthLoss, {});
    bool changed = false;
    for (const auto& [key, loss] : pending)
    {
        if (loss <= 0.0)
            continue;
        const bool destroyed = ship->core().applyVolumeDamage(key, loss, type, where);
        changed = true;
        // DEV balance data (author 2026-11-10: "how survivable are the ships"): ELITE_DEV_COMBAT_LOG=<csv path>
        // - one row per applied damage portion: combat clock, target, volume key, type, loss, ship alive modules.
        static std::ofstream combatLog = [] {
            const char* path = std::getenv("ELITE_DEV_COMBAT_LOG");
            std::ofstream out;
            if (path && *path)
            {
                out.open(path, std::ios::trunc);
                out << "t_s,target,key,type,loss,destroyed,modules_attached,modules_total\n";
            }
            return out;
        }();
        if (combatLog.is_open())
        {
            int attached = 0, total = 0;
            for (const auto& m : static_cast<const world::modules::ObjectModuleRuntime&>(ship->core().moduleRuntime()).modules())
            {
                ++total;
                attached += m.state == world::modules::ObjectModuleRuntimeState::Attached && m.health > 0.0f ? 1 : 0;
            }
            combatLog << m_combatClockS << ',' << targetId.value << ',' << key << ',' << static_cast<int>(type) << ','
                      << loss << ',' << (destroyed ? 1 : 0) << ',' << attached << ',' << total << '\n';
            combatLog.flush();
        }
        if (destroyed)
        {
            std::cerr << "[Combat] ship " << targetId.value << ": DESTROYED " << key << std::endl;
            // the module behind the key: a module volume, or a support-link volume (the legacy Cobra's ER-5s have no
            // body volume - a weapon tears them off their mounts)
            std::string mod = key.rfind("module::", 0) == 0 ? key.substr(8) : std::string();
            if (mod.empty() && key.rfind("link::", 0) == 0)
                for (const auto& v : ship->core().hitComponent().volumes)
                    if (v.supportLinkVolume && v.supportLinkId == key.substr(6))
                    {
                        mod = v.moduleId;
                        break;
                    }
            const bool reactor = isReactorModule(mod);
            // the reactor bay's frame destroyed: every installed propulsion reactor in it is torn loose (a roll each)
            if (mod == "ship_frame_reactor")
                for (const auto& r : ship->core().desc().powerThermalInstallation.engineReactorModuleIds)
                {
                    const std::size_t hr = std::hash<std::string>{}(r + "#bay#" + std::to_string(targetId.value) + "#" +
                                                                    std::to_string(static_cast<long long>(m_combatClockS)));
                    const double roll = static_cast<double>(hr % 10000) / 10000.0;
                    if (roll < 0.6)   // the reactor is torn loose: an emergency (the blast itself is a later roll)
                        startContainmentEmergency(targetId, r, 0.45, "reactor bay frame destroyed");
                }
            if (reactor)
            {
                const auto* m = static_cast<const world::modules::ObjectModuleRuntime&>(ship->core().moduleRuntime()).findModule(mod);
                const bool service = mod.find("service") != std::string::npos;
                const bool running = service || (m && (m->engineReactorOperating.lastElectricalOutputMW > 0.0 ||
                                                       m->engineReactorOperating.temperatureK > 1500.0));
                // a deterministic roll (repeatable trials): hash of ship, module and the combat clock second
                const std::size_t h = std::hash<std::string>{}(mod + "#" + std::to_string(targetId.value) + "#" +
                                                               std::to_string(static_cast<long long>(m_combatClockS)));
                const double roll = static_cast<double>(h % 10000) / 10000.0;
                if (running && roll < (service ? 0.5 : 0.8))
                    startContainmentEmergency(targetId, mod, service ? 0.35 : 0.5, "destroyed while running");
            }
        }
    }
    if (changed)
    {
        markShipGraphDirty(targetId);
        disableShipIfCombatLost(targetId, *ship);
    }
}

void GameSimulation::noteCombatAggression(EntityId targetId, EntityId attackerId)
{
    static const bool passive = [] {
        const char* v = std::getenv("ELITE_DEV_TARGET_PASSIVE");
        return v && *v && *v != '0';
    }();
    if (passive || !attackerId.value || targetId == attackerId || isPlayerControlled(targetId) || m_combatHostility.count(targetId))
        return;
    const Ship* t = getShip(targetId);
    if (!t || t->core().role() == ShipRole::Player || t->core().transform().motion.destroyedCause != 0)
        return;
    m_combatHostility[targetId] = attackerId;
    std::cerr << "[Combat] ship " << targetId.value << " fights back against ship " << attackerId.value << std::endl;
}

// Author 2026-11-10 (combat balance item 4): a ship is out of the fight when its cockpit is lost or all its propulsion
// reactors are lost. It is NOT blown up: the hull keeps its modules (salvage, trophies), it drifts with no control;
// the crew leaves in the EC-2 escape cockpit when that is intact and detachable, else the crew is lost. An NPC hull
// becomes a derelict. Cause 3 (motion.destroyedCause) - the HUD says "выведен из строя".
void GameSimulation::disableShipIfCombatLost(EntityId shipId, Ship& ship)
{
    auto& motion = ship.core().transform().motion;
    if (motion.destroyedCause != 0)
        return;
    const auto& runtime = static_cast<const world::modules::ObjectModuleRuntime&>(ship.core().moduleRuntime());
    const auto alive = [](const world::modules::ObjectModuleState& m) {
        return m.state == world::modules::ObjectModuleRuntimeState::Attached && m.health > 0.0f;
    };
    bool cockpitLost = false;
    int reactors = 0, reactorsAlive = 0;
    for (const auto& m : runtime.modules())
    {
        if (m.moduleId == "ship_cockpit")
            cockpitLost = !alive(m);
        const bool propulsionReactor = isPropulsionReactorModule(m.moduleId);
        if (propulsionReactor)
        {
            ++reactors;
            reactorsAlive += alive(m) ? 1 : 0;
        }
    }
    const bool reactorsLost = reactors > 0 && reactorsAlive == 0;
    if (!cockpitLost && !reactorsLost)
        return;
    motion.destroyedCause = 3;
    const auto* cockpit = runtime.findModule("ship_cockpit");
    const bool crewEscapes = !cockpitLost && cockpit && cockpit->detachable;
    std::cerr << "[Combat] ship " << shipId.value << ": OUT OF THE FIGHT - " << (cockpitLost ? "cockpit lost" : "all propulsion reactors lost")
              << "; crew " << (crewEscapes ? "escapes in the EC-2 cockpit" : cockpitLost ? "lost with the cockpit" : "stays aboard (no escape module)")
              << " (combat clock " << m_combatClockS << " s)" << std::endl;
    cancelJump(shipId, static_cast<std::uint8_t>(game::navigation::jump::Refusal::ShipDisabled));
    motion.jEngine = false;
    motion.jStopping = false;
    motion.freeFlightThrust = 0;
    motion.freeFlightAutopilotPhase = 0;
    ship.setControlState(ShipControlState {});
    m_laserFiring.erase(shipId);
    if (!isPlayerControlled(shipId))
        (void)markShipDerelict(shipId);
}

void GameSimulation::updateCombat(double dt)
{
    if (dt <= 0.0)
        return;
    m_combatClockS += dt;
    for (auto& [id, sp] : m_ships)
        for (const auto& mod : sp->core().takeReactorCatastrophes())
            startContainmentEmergency(id, mod, 0.8, "containment loss (overheat + damage)");
    {
        // DEV (explosion trials): ELITE_DEV_REACTOR_EXPLODE=<combat s> blows up a propulsion reactor of the combat target
        static const double devBlastAt = [] {
            const char* v = std::getenv("ELITE_DEV_REACTOR_EXPLODE");
            return v && *v ? std::atof(v) : -1.0;
        }();
        static bool devBlastDone = false;
        if (devBlastAt >= 0.0 && !devBlastDone && m_combatClockS >= devBlastAt && !m_combatTargetDummies.empty())
        {
            devBlastDone = true;
            const EntityId tid = *m_combatTargetDummies.begin();
            if (const Ship* t = getShip(tid))
            {
                const auto& installed = t->core().desc().powerThermalInstallation.engineReactorModuleIds;
                if (!installed.empty())
                {
                    queueReactorExplosion(tid, installed.front(), "DEV trigger");
                    std::cerr << "[Explosion] DEV trigger: ship " << tid.value << " " << installed.front() << std::endl;
                }
            }
        }
    }
    {
        // DEV (emergency trials): ELITE_DEV_REACTOR_EMERGENCY=<combat s>[:<chance>] starts a containment emergency on
        // a propulsion reactor of the PLAYER ship (to see the siren, the heat and the outcome)
        static const char* devEm = std::getenv("ELITE_DEV_REACTOR_EMERGENCY");
        static bool devEmDone = false;
        if (devEm && *devEm && !devEmDone && m_combatClockS >= std::atof(devEm))
        {
            devEmDone = true;
            const char* colon = std::strchr(devEm, ':');
            for (const auto& [id, sp] : m_ships)
                if (sp->core().role() == ShipRole::Player)
                {
                    const auto& installed = sp->core().desc().powerThermalInstallation.engineReactorModuleIds;
                    if (!installed.empty())
                        startContainmentEmergency(id, installed.front(), colon ? std::atof(colon + 1) : 0.5, "DEV trigger");
                    break;
                }
        }
    }
    updateContainmentEmergencies(dt);
    for (const auto& e : std::exchange(m_pendingExplosions, {}))
        explodeReactor(e);
    devCombatStep(dt);
    const auto& cat = weaponCatalog();
    const auto& mat = cat.materials;
    m_combatBeams.clear();

    // world velocity of every ship (finite differences; the frame-independent
    // truth), kept all the time: a missile launched from a ship in orbit takes
    // that ship's orbital velocity from its first tick
    for (const auto& [id, s] : m_ships)
    {
        const glm::dvec3 p = world::coordinates::fullMeters(s->core().transform().worldPosition);
        CombatTrack& t = m_combatTracks[id];
        if (t.valid)
            t.velocityW = (p - t.positionW) / dt;
        t.positionW = p;
        t.valid = true;
    }
    for (auto t = m_combatTracks.begin(); t != m_combatTracks.end();)
        t = m_ships.count(t->first) ? std::next(t) : m_combatTracks.erase(t);

    for (auto& [id, burn] : m_burnStates)
        burn.cool(dt, mat);

    // ---- lasers: idle units cool into the ship coolant (the trigger released)
    for (auto& [uid, unit] : m_laserUnits)
        if (!m_laserFiring.count(uid))
            if (Ship* idle = getShip(uid))
                if (const auto* l = cat.laser(combatLoadout(uid).laserId))
                    (void)laserControllerStep(uid, *idle, *l, dt, false);
    for (const EntityId shooterId : std::vector<EntityId>(m_laserFiring.begin(), m_laserFiring.end()))
    {
        const Ship* shooter = getShip(shooterId);
        if (!shooter)
        {
            m_laserFiring.erase(shooterId);
            continue;
        }
        const auto* laser = cat.laser(combatLoadout(shooterId).laserId);
        if (!laser)
            continue;
        const ShipTransform& tr = shooter->core().transform();
        const glm::dmat3 R = basisOf(tr);
        const glm::dvec3 pos = world::coordinates::fullMeters(tr.worldPosition);
        const glm::dvec3 fwd = R * glm::dvec3(0, 0, -1);

        // the target on the boresight: the nearest ship the forward axis meets
        EntityId target {};
        double targetT = laser->rangeM;
        bool targetIsShell = false;
        for (const auto& [id, s] : m_ships)
        {
            if (id == shooterId)
                continue;
            double t = 0.0;
            if (raySphere(pos, fwd, world::coordinates::fullMeters(s->core().transform().worldPosition), shipRadiusM(s->core().desc()), t) &&
                t < targetT)
            {
                targetT = t;
                target = id;
                targetIsShell = false;
            }
        }
        for (const auto& [id, s] : m_shipShells)
        {
            double t = 0.0;
            if (s.definition && raySphere(pos, fwd, world::coordinates::fullMeters(s.transform.worldPosition), shipRadiusM(*s.definition), t) &&
                t < targetT)
            {
                targetT = t;
                target = id;
                targetIsShell = true;
            }
        }
        // a shell under fire becomes a full ship (hit volumes, modules): the
        // combat claim plus the promotion now; it is hit from the next tick on
        if (target.value)
        {
            game::simulation::activation::ActivationClaim claim;
            claim.subjectId = target;
            claim.sourceId = shooterId;
            claim.systemId = m_activeCelestialSystemId;
            claim.minimumMode = game::simulation::SimulationMode::Active;
            claim.kind = game::simulation::activation::ActivationClaimKind::Combat;
            claim.expiresAtServerTimeSeconds = serverTime() + 10.0;
            upsertActivationClaim(claim);
            if (targetIsShell)
            {
                std::cerr << "[Combat] shell " << target.value << " under fire -> full ship" << std::endl;
                (void)materializeShipShell(target);
                (void)promoteShipToDetailedSimulation(target);
            }
        }
        const double converge = target.value ? std::max(50.0, targetT + 5.0) : 1000.0;
        const glm::dvec3 aim = pos + fwd * converge;

        // pulsed lasers: the beam is on for `duty` of each pulse period
        // the laser controller: beam power from the service reactor's headroom, limited by the laser body's and
        // the ship coolant's temperature; capacitor lasers charge and fire their bank in one short burst
        double powerMW = 0.0;
        if (Ship* shooterMut = getShip(shooterId))
            powerMW = laserControllerStep(shooterId, *shooterMut, *laser, dt, true);
        const bool on = powerMW > 0.0;

        Ship* targetShip = target.value && !targetIsShell ? getShip(target) : nullptr;
        // the muzzles: the laser modules' fronts; a hull without muzzle
        // attachments fires one laser from its nose
        std::vector<glm::dvec3> muzzles;
        bool anyMuzzleAttachment = false;
        for (const auto& a : shooter->core().desc().attachments)
        {
            if (!a.enabled || a.kind != ShipAttachmentKind::WeaponMuzzle)
                continue;
            anyMuzzleAttachment = true;
            const game::damage::HitVolume* mount = nullptr;
            for (const auto& v : shooter->core().hitComponent().volumes)
                if (!v.supportLinkVolume && v.moduleId == a.parentModuleId && !v.destroyed)
                    mount = &v;
            if (mount)   // else the laser module is gone
                muzzles.push_back(glm::dvec3(mount->center) + glm::dvec3(0.0, 0.0, -mount->halfSize.z));
        }
        if (!anyMuzzleAttachment)
            muzzles.push_back(glm::dvec3(0.0, 0.0, -shipRadiusM(shooter->core().desc())));
        {
            static std::map<std::uint64_t, double> lastLog;
            double& last = lastLog[shooterId.value];
            if (m_combatClockS - last >= 1.0)
            {
                last = m_combatClockS;
                double nearest = 1.0e18;
                double nearestAngle = 0.0;
                std::string nearestInfo;
                for (const auto& [id, s] : m_ships)
                    if (id != shooterId)
                    {
                        const glm::dvec3 d = world::coordinates::fullMeters(s->core().transform().worldPosition) - pos;
                        if (glm::length(d) < nearest)
                        {
                            nearest = glm::length(d);
                            nearestAngle = glm::degrees(std::acos(std::clamp(glm::dot(d / std::max(1e-9, nearest), fwd), -1.0, 1.0)));
                            nearestInfo = " (mode " + std::to_string(static_cast<int>(s->core().transform().motion.mode)) +
                                          " vLocal " + std::to_string(glm::length(s->core().transform().motion.localVelocityMps)) + ")";
                        }
                    }
                std::cerr << "[Combat] ship " << shooterId.value << " laser " << laser->id << ": " << muzzles.size()
                          << " muzzle(s), target " << target.value << (targetIsShell ? " (shell)" : "") << " at " << targetT
                          << " m; nearest ship " << nearest << " m" << nearestInfo << ", " << nearestAngle << " deg off the boresight; mode "
                          << static_cast<int>(tr.motion.mode) << " frame " << tr.motion.travelFrame.frameId << " vLocal "
                          << glm::length(tr.motion.localVelocityMps) << std::endl;
            }
        }
        for (const glm::dvec3& muzzleLocal : muzzles)
        {
            const glm::dvec3 from = pos + R * muzzleLocal;
            const glm::dvec3 dir = glm::normalize(aim - from);
            game::simulation::CombatBeamSnapshot beam;
            beam.from = world::coordinates::makeWorldPositionFromMeters(from);
            beam.to = world::coordinates::makeWorldPositionFromMeters(from + dir * laser->rangeM);
            beam.powerMW = static_cast<float>(powerMW);
            beam.colour = laser->colour;
            if (on && targetShip)
            {
                const ShipTransform& ttr = targetShip->core().transform();
                const glm::dmat3 TR = basisOf(ttr);
                const glm::dvec3 tpos = world::coordinates::fullMeters(ttr.worldPosition);
                const glm::vec3 oL(glm::transpose(TR) * (from - tpos));
                const glm::vec3 dL(glm::transpose(TR) * dir);
                auto& burn = m_burnStates[target];
                // every mount is one laser of the loadout's type
                const auto hit = game::combat::stepBeam(targetShip->core().hitComponent(), burn, oL, dL,
                                                        static_cast<float>(laser->rangeM), powerMW, dt, laser->beamRadiusM, mat);
                if (hit.hit)
                    noteCombatAggression(target, shooterId);
                {
                    static double lastMissLog = -10.0;
                    if (!hit.hit && m_combatClockS - lastMissLog >= 1.0)
                    {
                        lastMissLog = m_combatClockS;
                        const auto& vols = targetShip->core().hitComponent().volumes;
                        std::cerr << "[Combat] MISS DIAG oL " << oL.x << " " << oL.y << " " << oL.z << " dL " << dL.x << " "
                                  << dL.y << " " << dL.z << " volumes " << vols.size() << " first c "
                                  << (vols.empty() ? 0.0f : vols.front().center.x) << " "
                                  << (vols.empty() ? 0.0f : vols.front().center.y) << " "
                                  << (vols.empty() ? 0.0f : vols.front().center.z) << " castRay "
                                  << game::combat::castRay(targetShip->core().hitComponent(), oL, dL, 5000.0f).size()
                                  << std::endl;
                    }
                }
                if (hit.hit)
                {
                    const glm::dvec3 hitW = tpos + TR * glm::dvec3(hit.pointLocal);
                    beam.to = world::coordinates::makeWorldPositionFromMeters(hitW);
                    beam.hit = 1;
                    double& flush = m_burnFlushSeconds[target];
                    flush += dt;
                    if (hit.burnedThrough > 0)
                    {
                        std::cerr << "[Combat] ship " << target.value << ": BURN-THROUGH " << hit.key << " ("
                                  << hit.burnMJ << " MJ) by " << laser->id << std::endl;
                        recordCombatImpact(target, beam.to, hit.absorbedMJ, 1, hit.moduleId);
                    }
                    if (flush >= 0.2 || hit.burnedThrough > 0)
                    {
                        flush = 0.0;
                        recordCombatImpact(target, beam.to, hit.absorbedMJ, 0, hit.moduleId);
                        flushCombatDamage(target, game::damage::DamageType::Laser, beam.to);
                    }
                }
            }
            m_combatBeams[shooterId].push_back(beam);
        }
    }

    // ---- missiles
    for (auto it = m_combatMissiles.begin(); it != m_combatMissiles.end();)
    {
        CombatMissile& m = *it;
        const auto* spec = cat.missile(m.specId);
        m.ageS += dt;
        if (!spec || m.ageS > spec->fuelS + 20.0)
        {
            std::cerr << "[Combat] missile " << m.id << ": self-destruct (no hit)" << std::endl;
            it = m_combatMissiles.erase(it);
            continue;
        }
        const Ship* target = m.target.value ? getShip(m.target) : nullptr;
        glm::dvec3 targetPos(0.0), targetVel(0.0);
        if (target)
        {
            targetPos = world::coordinates::fullMeters(target->core().transform().worldPosition);
            if (const auto t = m_combatTracks.find(m.target); t != m_combatTracks.end() && t->second.valid)
                targetVel = t->second.velocityW;
        }
        // guidance: thrust toward the target, nulling the sideways relative velocity
        glm::dvec3 want = m.heading;
        if (target)
        {
            const glm::dvec3 los = targetPos - m.positionW;
            const double dist = glm::length(los);
            const glm::dvec3 losDir = los / std::max(dist, 1.0);
            const glm::dvec3 rel = m.velocityW - targetVel;
            const glm::dvec3 lateral = rel - losDir * glm::dot(rel, losDir);
            want = glm::normalize(losDir * spec->accelMps2 - lateral * 3.0);
        }
        const double maxTurn = glm::radians(spec->turnDps) * dt;
        const double ang = std::acos(std::clamp(glm::dot(m.heading, want), -1.0, 1.0));
        if (ang > 1.0e-6)
        {
            const double f = std::min(1.0, maxTurn / ang);
            m.heading = glm::normalize(m.heading * (1.0 - f) + want * f);
        }
        const glm::dvec3 relNow = m.velocityW - targetVel;
        const bool fuel = m.ageS < spec->fuelS;
        const bool capped = glm::length(relNow) > spec->maxSpeedMps && glm::dot(relNow, m.heading) > 0.0;
        const auto g = game::navigation::GravityFieldSystem::sample(m.positionW, m_gravityBodies);
        const glm::dvec3 accel = (fuel && !capped ? m.heading * spec->accelMps2 : glm::dvec3(0.0)) + g.accelerationMps2;
        const glm::dvec3 prev = m.positionW;
        if (target)
        {
            // relative to the target: the tidal difference of gravity over a few km is negligible, so the relative
            // motion is thrust only - integrating both at ~30 km/s in the world frame accumulated a frame / gravity
            // mismatch that the guidance could not null (the missiles drifted 20-100 deg off the line of sight)
            glm::dvec3 relP = m.positionW - targetPos;
            glm::dvec3 relV = m.velocityW - targetVel;
            relV += (fuel && !capped ? m.heading * spec->accelMps2 : glm::dvec3(0.0)) * dt;
            relP += relV * dt;
            m.velocityW = targetVel + relV;
            m.positionW = targetPos + targetVel * dt + relP;
        }
        else
        {
            m.velocityW += accel * dt;
            m.positionW += m.velocityW * dt;
        }
        if (target && std::floor(m.ageS) != std::floor(m.ageS - dt))
            std::cerr << "[Combat] missile " << m.id << " t+" << std::floor(m.ageS) << " s: " << glm::length(targetPos - m.positionW)
                      << " m to target, closing " << glm::length(m.velocityW - targetVel) << " m/s, heading-LOS " << glm::degrees(std::acos(std::clamp(glm::dot(m.heading, glm::normalize(targetPos - m.positionW)), -1.0, 1.0))) << " deg" << std::endl;

        // contact: the segment of this step against every ship it passes near
        bool done = false;
        for (auto& [id, s] : m_ships)
        {
            if (id == m.shooter && m.ageS < 1.0)
                continue;
            const ShipTransform& ttr = s->core().transform();
            const glm::dvec3 tpos = world::coordinates::fullMeters(ttr.worldPosition);
            glm::dvec3 tvel(0.0);
            if (const auto t = m_combatTracks.find(id); t != m_combatTracks.end() && t->second.valid)
                tvel = t->second.velocityW;
            // relative motion over the step: the missile goes from prev (now, t) to positionW (t + dt) while the ship
            // goes from tpos (t) to tpos + tvel dt. (The old form compared both ends one tick early - an offset of
            // tvel x dt, ~500 m at orbital speeds: every missile "passed" its target at ~350-480 m.)
            const glm::dvec3 a = prev - tpos, b = m.positionW - (tpos + tvel * dt);
            const glm::dvec3 seg = b - a;
            const double len = glm::length(seg);
            const double r = shipRadiusM(s->core().desc());
            const glm::dvec3 closest = a + seg * std::clamp(len > 0.0 ? glm::dot(-a, seg) / (len * len) : 0.0, 0.0, 1.0);
            if (id == m.target)
            {
                const double c = glm::length(closest);
                if (c < m.closestM)
                    m.closestM = c;
                else if (!m.passReported && c > m.closestM + 1.0)
                {
                    m.passReported = true;
                    std::cerr << "[Combat] missile " << m.id << " passed the target: closest " << m.closestM
                              << " m (hull radius " << shipRadiusM(s->core().desc()) << " m)" << std::endl;
                }
            }
            if (glm::length(closest) > r || len < 1.0e-6)
                continue;
            const glm::dmat3 TR = basisOf(ttr);
            const glm::vec3 aL(glm::transpose(TR) * a);
            const glm::vec3 dL(glm::transpose(TR) * (seg / len));
            const auto hits = game::combat::castRay(s->core().hitComponent(), aL, dL, static_cast<float>(len));
            if (hits.empty())
                continue;
            // impact: the warhead from the entry point along the missile's line
            const glm::vec3 entryL = aL + dL * hits.front().tEnter;
            const glm::dvec3 relV = m.velocityW - tvel;
            const double relSpeed = glm::length(relV);
            const glm::vec3 lineL = spec->warhead == game::combat::WarheadKind::Slug
                                        ? glm::vec3(glm::transpose(TR) * (relV / std::max(relSpeed, 1.0)))
                                        : glm::vec3(glm::transpose(TR) * m.heading);
            const double energy = spec->warhead == game::combat::WarheadKind::Slug
                                      ? 0.5 * spec->slugKg * relSpeed * relSpeed / 1.0e6
                                      : spec->jetMJ;
            auto& burn = m_burnStates[id];
            const auto res = game::combat::penetrate(s->core().hitComponent(), burn, entryL - lineL * 0.05f, lineL, 60.0f, energy,
                                                     spec->warhead, spec->slugKg, mat);
            const auto whereW = world::coordinates::makeWorldPositionFromMeters(tpos + TR * glm::dvec3(entryL));
            noteCombatAggression(id, m.shooter);
            std::cerr << "[Combat] missile " << m.id << " HIT ship " << id.value << " at " << relSpeed << " m/s, "
                      << (spec->warhead == game::combat::WarheadKind::Slug ? "slug " : "jet ") << energy << " MJ:";
            for (const auto& l : res.layers)
                std::cerr << " " << l.key << (l.holed ? "(through " : "(spent ") << l.spentMJ << ")";
            std::cerr << (res.stuck ? " -> slug STUCK" : res.exited ? " -> out of the hull" : " -> spent") << std::endl;
            recordCombatImpact(id, whereW, energy, spec->warhead == game::combat::WarheadKind::Slug ? 3 : 2,
                               res.layers.empty() ? std::string() : res.layers.front().moduleId);
            // Doctrine (author 2026-11-10): a missile burns the INSIDE of a compartment, it does not fly out. A shaped
            // jet that holed the outer skin spends what it has left in the compartment behind the breach: the
            // equipment within 3 m of the jet's line inside the hull (hot gas and molten metal). Before, the jet left
            // the hull with most of its energy (4 Needle hits = ~50 health in the trials).
            if (spec->warhead == game::combat::WarheadKind::Shaped && !res.layers.empty() && res.layers.front().holed)
            {
                double spent = 0.0;
                for (const auto& l : res.layers)
                    spent += l.spentMJ;
                const double left = std::max(0.0, energy - spent);
                if (left > 0.1)
                {
                    std::vector<std::pair<std::string, double>> inside;   // key, weight
                    double total = 0.0;
                    for (const auto& v : s->core().hitComponent().volumes)
                    {
                        if (v.destroyed || v.supportLinkVolume || v.moduleId.find("panel") != std::string::npos)
                            continue;
                        const glm::vec3 rel = v.center - entryL;
                        const float along = glm::dot(rel, lineL);
                        if (along < 0.0f || along > 12.0f)
                            continue;
                        const float off = glm::length(rel - lineL * along) - glm::length(v.halfSize) * 0.5f;
                        if (off > 3.0f)
                            continue;
                        const double w = 1.0 / (1.0 + std::max(0.0f, off) + 0.15 * along);
                        inside.emplace_back(game::combat::volumeKey(v), w);
                        total += w;
                    }
                    if (total > 0.0)
                    {
                        for (const auto& [key, w] : inside)
                            burn.pendingHealthLoss[key] += left * (w / total) * mat.healthPerMJ;
                        std::cerr << "[Combat] missile " << m.id << ": " << left << " MJ burn the compartment behind the breach ("
                                  << inside.size() << " modules)" << std::endl;
                    }
                }
            }
            flushCombatDamage(id, game::damage::DamageType::Explosion, whereW);
            // momentum: the hull is pushed, the struck part loads its links
            const double massKg = std::max(1000.0, s->core().currentMassKg());
            const glm::dvec3 dv = relV * (spec->massKg / massKg);
            auto& motion = s->core().transform().motion;
            if (motion.travelFrame.valid)
                motion.localVelocityMps += motion.travelFrame.worldToLocalVector(dv);
            if (!res.layers.empty())
                (void)s->core().applyPartImpulse(res.layers.front().moduleId, spec->massKg * relSpeed / 1000.0);
            markShipGraphDirty(id);
            done = true;
            break;
        }
        if (done)
        {
            it = m_combatMissiles.erase(it);
            continue;
        }
        ++it;
    }
}

game::simulation::ShipCombatSnapshot GameSimulation::combatSnapshotFor(EntityId shipId) const
{
    game::simulation::ShipCombatSnapshot s;
    if (const auto l = m_combatLoadouts.find(shipId); l != m_combatLoadouts.end())
    {
        s.laserId = l->second.laserId;
        s.missileId = l->second.missileId;
        s.missiles = l->second.missiles;
    }
    s.laserFiring = m_laserFiring.count(shipId) ? 1 : 0;
    if (const auto b = m_combatBeams.find(shipId); b != m_combatBeams.end())
        s.beams = b->second;
    if (const auto i = m_combatImpacts.find(shipId); i != m_combatImpacts.end())
        s.impacts = i->second;
    for (const auto& m : m_combatMissiles)
        if (m.shooter == shipId)
        {
            game::simulation::CombatMissileSnapshot ms;
            ms.id = m.id;
            ms.position = world::coordinates::makeWorldPositionFromMeters(m.positionW);
            ms.velocityMps = glm::vec3(m.heading * 100.0);
            ms.specId = m.specId;
            s.missilesInFlight.push_back(ms);
        }
    return s;
}

// ============================================================================ DEV combat trials (author 2026-11-10)
void GameSimulation::devCombatStep(double dt)
{
    static const std::string fight = [] {
        const char* v = std::getenv("ELITE_DEV_TARGET_FIGHTS");
        return std::string(v ? v : "");
    }();
    static const bool collisions = [] {
        const char* v = std::getenv("ELITE_DEV_SHIP_COLLISIONS");
        return !(v && *v == '0');   // on by default (author 2026-11-10)
    }();
    static const bool playerTrack = [] {
        const char* v = std::getenv("ELITE_DEV_PLAYER_TRACK");
        return v && *v && *v != '0';
    }();
    if (dt <= 0.0)
        return;
    const auto& cat = weaponCatalog();

    // ---- DEV trials: the player's pilot keeps the crosshair on the nearest target (30 deg/s), as a human would
    if (playerTrack)
    {
        for (auto& [pid, ps] : m_ships)
        {
            if (ps->core().role() != ShipRole::Player)
                continue;
            auto& tp = ps->core().transform();
            const glm::dvec3 pp = world::coordinates::fullMeters(tp.worldPosition);
            double best = 1.0e30;
            glm::dvec3 aimAt(0.0);
            for (const EntityId tid : m_combatTargetDummies)
                if (const Ship* t = getShip(tid))
                {
                    const glm::dvec3 q = world::coordinates::fullMeters(t->core().transform().worldPosition);
                    if (glm::length(q - pp) < best)
                    {
                        best = glm::length(q - pp);
                        aimAt = q;
                    }
                }
            if (best > 1.0e29 || best < 1.0)
                continue;
            // a pilot sweeps the crosshair over the hull (a fixed point would only cut one line through the ship):
            // a slow spiral up to 6 m around the target centre, across the line of sight
            {
                const glm::dvec3 los = (aimAt - pp) / best;
                glm::dvec3 side = glm::cross(los, glm::dvec3(0.0, 1.0, 0.0));
                if (glm::length(side) < 1.0e-6)
                    side = glm::dvec3(1.0, 0.0, 0.0);
                side = glm::normalize(side);
                const glm::dvec3 up = glm::cross(side, los);
                const double t = m_combatClockS;
                const double r = 6.0 * (0.5 + 0.5 * std::sin(t * 0.21));
                aimAt += side * (r * std::cos(t * 0.9)) + up * (0.5 * r * std::sin(t * 0.9));
            }
            const glm::dvec3 want = glm::normalize(aimAt - pp);
            glm::dmat3 R = glm::dmat3(glm::mat3(tp.orientation));
            const glm::dvec3 fwd = R * glm::dvec3(0, 0, -1);
            const double ang = std::acos(std::clamp(glm::dot(fwd, want), -1.0, 1.0));
            if (ang > 1.0e-7)
            {
                glm::dvec3 axis = glm::cross(fwd, want);
                if (glm::length(axis) < 1.0e-12)
                    axis = R * glm::dvec3(0, 1, 0);
                R = glm::dmat3(glm::rotate(glm::dmat4(1.0), std::min(ang, glm::radians(30.0) * dt), glm::normalize(axis))) * R;
                const glm::dvec3 f = glm::normalize(R * glm::dvec3(0, 0, -1));
                const glm::dvec3 r = glm::normalize(glm::cross(f, R * glm::dvec3(0, 1, 0)));
                R = glm::dmat3(r, glm::cross(r, f), -f);
                tp.orientation = glm::mat4(glm::mat3(R));
            }
        }
    }

    // ---- combat AI (author 2026-11-10, item 5): an NPC that was shot at fights its attacker - turns to it (rate-limited),
    //      holds a duel range of 1.5 km, sweeps the aim over the hull like a pilot, fires the laser when the aim is
    //      on and launches a missile when the seeker window opens (every 25 s). DEV ram mode: rams instead.
    //      ELITE_DEV_TARGET_FIGHTS=duel|ram makes the spawned combat targets hostile to the player at once.
    if (!fight.empty())
    {
        EntityId playerId {};
        for (const auto& [id, s] : m_ships)
            if (s->core().role() == ShipRole::Player)
                playerId = id;
        if (playerId.value)
            for (const EntityId id : m_combatTargetDummies)
                if (!m_combatHostility.count(id))
                    m_combatHostility[id] = playerId;
    }
    for (auto it = m_combatHostility.begin(); it != m_combatHostility.end();)
    {
        const EntityId id = it->first;
        Ship* d = getShip(id);
        const Ship* enemy = getShip(it->second);
        if (!d || !enemy || d->core().transform().motion.destroyedCause != 0 || enemy->core().transform().motion.destroyedCause != 0)
        {
            if (d && m_devTargetLaserOn.count(id))
            {
                setLaserFiring(id, false);
                m_devTargetLaserOn.erase(id);
            }
            it = m_combatHostility.erase(it);
            continue;
        }
        ++it;
        auto& tr = d->core().transform();
        const glm::dvec3 pd = world::coordinates::fullMeters(tr.worldPosition);
        const glm::dvec3 pp = world::coordinates::fullMeters(enemy->core().transform().worldPosition);
        const auto trackP = m_combatTracks.find(m_combatHostility.count(id) ? m_combatHostility[id] : EntityId {});
        const glm::dvec3 vp = trackP != m_combatTracks.end() && trackP->second.valid ? trackP->second.velocityW : glm::dvec3(0.0);
        const glm::dvec3 to = pp - pd;
        const double dist = glm::length(to);
        if (dist < 1.0)
            continue;
        const bool ram = fight == "ram";
        static const double ramSpeed = [] {
            const char* v = std::getenv("ELITE_DEV_RAM_SPEED");
            return v && *v ? std::atof(v) : 90.0;
        }();
        // after a contact the rammer backs off for 6 s, then comes round again (no grinding against the hull)
        const bool retreat = ram && m_combatClockS < m_devRamRetreatUntilS[id];
        const double closing = ram ? (retreat ? -40.0 : ramSpeed) : std::clamp((dist - 1500.0) * 0.15, -60.0, 60.0);
        // the aim point: the enemy's centre swept over its hull (a fixed point only cuts one line through a ship)
        glm::dvec3 aimAt = pp;
        if (!ram)
        {
            const glm::dvec3 los = to / dist;
            glm::dvec3 side = glm::cross(los, glm::dvec3(0.0, 1.0, 0.0));
            side = glm::length(side) < 1.0e-6 ? glm::dvec3(1.0, 0.0, 0.0) : glm::normalize(side);
            const glm::dvec3 up = glm::cross(side, los);
            const double t = m_combatClockS + 3.0 * static_cast<double>(id.value % 7);
            const double r = 6.0 * (0.5 + 0.5 * std::sin(t * 0.23));
            aimAt += side * (r * std::cos(t * 0.8)) + up * (0.5 * r * std::sin(t * 0.8));
        }
        const glm::dvec3 want = glm::normalize(aimAt - pd);
        glm::dmat3 R = glm::dmat3(glm::mat3(tr.orientation));
        const glm::dvec3 fwd = R * glm::dvec3(0, 0, -1);
        const double ang = std::acos(std::clamp(glm::dot(fwd, want), -1.0, 1.0));
        const double maxTurn = glm::radians(ram ? 60.0 : 40.0) * dt;
        if (ang > 1.0e-6)
        {
            glm::dvec3 axis = glm::cross(fwd, want);
            if (glm::length(axis) < 1.0e-9)
                axis = R * glm::dvec3(0, 1, 0);
            R = glm::dmat3(glm::rotate(glm::dmat4(1.0), std::min(ang, maxTurn), glm::normalize(axis))) * R;
            const glm::dvec3 f = glm::normalize(R * glm::dvec3(0, 0, -1));
            const glm::dvec3 r = glm::normalize(glm::cross(f, R * glm::dvec3(0, 1, 0)));
            R = glm::dmat3(r, glm::cross(r, f), -f);
            tr.orientation = glm::mat4(glm::mat3(R));
        }
        const glm::dvec3 desiredW = vp + (to / dist) * closing;
        auto& mo = tr.motion;
        mo.localVelocityMps = mo.travelFrame.worldToLocalVelocity(pd, desiredW);
        const auto& l = combatLoadout(id);
        const auto* laser = cat.laser(l.laserId);
        const double alignedDeg = glm::degrees(std::acos(std::clamp(glm::dot(R * glm::dvec3(0, 0, -1), want), -1.0, 1.0)));
        const bool fire = !ram && laser && alignedDeg < 3.0 && dist < laser->rangeM;
        if (fire != (m_devTargetLaserOn.count(id) > 0))
        {
            setLaserFiring(id, fire);
            if (fire)
                m_devTargetLaserOn.insert(id);
            else
                m_devTargetLaserOn.erase(id);
        }
        const auto* missile = cat.missile(l.missileId);
        if (!ram && missile && alignedDeg < missile->seekerDeg * 0.5 && dist < missile->rangeM &&
            m_combatClockS - m_devTargetMissileClockS[id] > 25.0)
        {
            m_devTargetMissileClockS[id] = m_combatClockS;
            launchMissile(id);
        }
    }

    // ---- collisions (author 2026-11-10: no special ram - ordinary collisions; what hits what is not known in advance).
    //      Broad phase: bounding spheres of the ships' hit volumes; narrow phase: their oriented hit boxes (SAT). The
    //      contact is where boxes really overlap: an inelastic impulse with its lever arm (the ships separate and spin),
    //      the dissipated energy burns exactly the touching modules (MJ x health_per_mj). ELITE_DEV_SHIP_COLLISIONS=0
    //      turns it off.
    if (collisions)
        collideShips(dt);
}


// ============================================================================ laser controller (author 2026-11-10)
// Returns the beam power (MW) this tick. firing = the trigger is held. One law for every ship: what differs
// between ships is the service reactor headroom and the coolant they give the laser.
double GameSimulation::laserControllerStep(EntityId shooterId, Ship& ship, const game::combat::LaserSpec& laser, double dt,
                                           bool firing)
{
    auto& core = ship.core();
    LaserUnit& u = m_laserUnits[shooterId];
    const double coolantK = core.serviceCoolantTemperatureK();
    if (u.temperatureK <= 0.0)
        u.temperatureK = coolantK;
    const double coolCritK = core.serviceCoolantCriticalTemperatureK();
    const double bodyF = std::clamp((laser.maxTemperatureK - u.temperatureK) / 60.0, 0.0, 1.0);
    const double coolF = std::clamp((coolCritK - 50.0 - coolantK) / 100.0, 0.0, 1.0);
    const double throttle = std::min(bodyF, coolF);
    const double headroom = core.serviceReactorHeadroomMW();
    const double eff = std::clamp(laser.efficiency, 0.01, 1.0);
    double electricalMW = 0.0, beamMW = 0.0, heatMJ = 0.0;
    if (laser.capacitorMJ > 0.0)
    {
        if (u.dischargeLeftS > 0.0)
        {
            beamMW = laser.capacitorMJ * eff / std::max(0.01, laser.dischargeS);
            u.dischargeLeftS -= dt;
        }
        else if (firing)
        {
            electricalMW = std::min(laser.chargeMW * throttle, headroom);
            u.capacitorMJ += electricalMW * dt;
            if (u.capacitorMJ >= laser.capacitorMJ && throttle > 0.0)
            {
                u.capacitorMJ = 0.0;
                u.dischargeLeftS = laser.dischargeS;   // the shot: the bank goes out in one burst
                heatMJ += laser.capacitorMJ * (1.0 - eff);
                beamMW = laser.capacitorMJ * eff / std::max(0.01, laser.dischargeS);
                u.dischargeLeftS -= dt;
                ++u.shots;
            }
        }
    }
    else if (firing)
    {
        const bool pulseOn = laser.pulseHz <= 0.0 || std::fmod(m_combatClockS * laser.pulseHz, 1.0) < laser.duty;
        const double wantMW = pulseOn ? laser.powerMW / eff : 0.0;
        electricalMW = std::min(wantMW * throttle, headroom);
        beamMW = electricalMW * eff;
        heatMJ += electricalMW * (1.0 - eff) * dt;
    }
    // the body conducts into the ship's service coolant
    const double conductedMJ = std::max(0.0, laser.coolingMWperK * (u.temperatureK - coolantK) * dt);
    u.temperatureK += (heatMJ - conductedMJ) / std::max(0.01, laser.heatCapacityMJperK);
    core.addServiceHeatMJ(conductedMJ);
    u.electricalMJ += electricalMW * dt;
    u.beamMJ += beamMW * dt;

    static const bool trace = [] {
        const char* v = std::getenv("ELITE_DEV_COMBAT_LOG");
        return v && *v;
    }();
    if (trace && firing && m_combatClockS - u.lastTraceS >= 1.0)
    {
        u.lastTraceS = m_combatClockS;
        std::cerr << "[LaserCtl] ship " << shooterId.value << " " << laser.id << " body " << u.temperatureK << " K (limit "
                  << laser.maxTemperatureK << "), coolant " << coolantK << " K (critical " << coolCritK << "), headroom "
                  << headroom << " MW, electrical " << electricalMW << " MW, beam " << beamMW << " MW, throttle " << throttle
                  << (laser.capacitorMJ > 0.0 ? ", bank " + std::to_string(u.capacitorMJ) + " MJ, shots " + std::to_string(u.shots) : "")
                  << std::endl;
    }
    return beamMW;
}

// ============================================================================ reactor explosions (author 2026-11-10)
void GameSimulation::queueReactorExplosion(EntityId shipId, const std::string& moduleId, const std::string& cause)
{
    if (m_explodedReactors.count(std::to_string(shipId.value) + "#" + moduleId))
        return;   // a reactor blows up once
    for (const auto& e : m_pendingExplosions)
        if (e.shipId == shipId && e.moduleId == moduleId)
            return;
    // stored energy of the containment: a propulsion reactor (ER-5 class) ~300 MJ, a service reactor ~80 MJ
    const double energy = moduleId.find("service") != std::string::npos ? 80.0 : 300.0;
    m_pendingExplosions.push_back({shipId, moduleId, energy, cause});
}

void GameSimulation::explodeReactor(const PendingExplosion& e)
{
    Ship* ship = getShip(e.shipId);
    if (!ship || !m_explodedReactors.insert(std::to_string(e.shipId.value) + "#" + e.moduleId).second)
        return;   // marked first: a chain started by this blast cannot re-queue the same reactor
    const auto& cat = weaponCatalog();
    const auto& mat = cat.materials;
    auto& hit = ship->core().hitComponent();
    // the reactor's centre (its first hit volume; destroyed volumes keep their geometry)
    glm::vec3 c(0.0f);
    bool found = false;
    // where: the reactor's body volume, else a placed mount (link) volume, else the reactor bay's frame (the legacy
    // Cobra's ER-5s have no placed geometry of their own - they sit in ship_frame_reactor)
    for (int pass = 0; pass < 3 && !found; ++pass)
        for (const auto& v : hit.volumes)
        {
            const bool match = pass == 2 ? (v.moduleId == "ship_frame_reactor" && !v.supportLinkVolume)
                                         : (v.moduleId == e.moduleId && v.supportLinkVolume == (pass == 1));
            if (match && glm::length(v.center) > 1.0e-4f)
            {
                c = v.center;
                found = true;
                break;
            }
        }
    if (!found)
    {
        std::cerr << "[Explosion] ship " << e.shipId.value << ": no hit volume for " << e.moduleId << std::endl;
        return;
    }
    const ShipTransform& tr = ship->core().transform();
    const glm::dmat3 R = basisOf(tr);
    const glm::dvec3 shipPos = world::coordinates::fullMeters(tr.worldPosition);
    const glm::dvec3 centreW = shipPos + R * glm::dvec3(c);
    const auto where = world::coordinates::makeWorldPositionFromMeters(centreW);
    std::mt19937 rng(static_cast<std::uint32_t>(std::hash<std::string>{}(e.moduleId) ^ (e.shipId.value * 2654435761u) ^
                                                static_cast<std::uint32_t>(m_combatClockS * 1000.0)));
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    auto& burn = m_burnStates[e.shipId];

    // 1) the flash: no air, so no long-range blast wave - 25 % of the energy on the modules within 4 m
    const double flashMJ = 0.25 * e.energyMJ;
    std::vector<std::pair<std::string, double>> near;
    double wsum = 0.0;
    for (const auto& v : hit.volumes)
    {
        if (v.destroyed || v.moduleId == e.moduleId)
            continue;
        const glm::vec3 q = glm::clamp(c, v.center - v.halfSize, v.center + v.halfSize);
        const double d = glm::length(c - q);
        if (d > 4.0)
            continue;
        const double w = 1.0 / (1.0 + d * d);
        near.emplace_back(game::combat::volumeKey(v), w);
        wsum += w;
    }
    for (const auto& [key, w] : near)
        burn.pendingHealthLoss[key] += flashMJ * (w / wsum) * mat.healthPerMJ;

    // 2) fragments: the rest of the energy in N pieces flying out at random
    constexpr int N = 40;
    const double fragMJ = 0.75 * e.energyMJ / N;
    int ownHits = 0, otherHits = 0;
    for (int i = 0; i < N; ++i)
    {
        const double z = 2.0 * uni(rng) - 1.0, a = 2.0 * glm::pi<double>() * uni(rng);
        const glm::vec3 dir(static_cast<float>(std::sqrt(1.0 - z * z) * std::cos(a)), static_cast<float>(z),
                            static_cast<float>(std::sqrt(1.0 - z * z) * std::sin(a)));
        const auto res = game::combat::penetrate(hit, burn, c, dir, 60.0f, fragMJ, game::combat::WarheadKind::Slug, 2.0, mat);
        if (!res.layers.empty())
            ++ownHits;
    }
    flushCombatDamage(e.shipId, game::damage::DamageType::Explosion, where);

    // 3) other ships: fragments that left the hull reach a ship at distance d with probability (its cross-section /
    //    the sphere) - a random number of hits, each on a random module (no rays: cheap)
    for (auto& [id, other] : m_ships)
    {
        if (id == e.shipId)
            continue;
        const glm::dvec3 p = world::coordinates::fullMeters(other->core().transform().worldPosition);
        const double d = glm::length(p - centreW);
        if (d > 3000.0)
            continue;
        const double r = shipRadiusM(other->core().desc()) * 0.6;
        const double share = std::min(0.5, (r * r) / (4.0 * std::max(d * d, r * r)));
        const double expected = N * share;
        int hits = static_cast<int>(std::floor(expected + uni(rng)));
        auto& vols = other->core().hitComponent().volumes;
        std::vector<int> alive;
        for (int k = 0; k < static_cast<int>(vols.size()); ++k)
            if (!vols[k].destroyed && !vols[k].supportLinkVolume)
                alive.push_back(k);
        if (alive.empty() || hits <= 0)
            continue;
        auto& ob = m_burnStates[id];
        for (int k = 0; k < hits; ++k)
        {
            const auto& v = vols[alive[static_cast<std::size_t>(uni(rng) * alive.size()) % alive.size()]];
            ob.pendingHealthLoss[game::combat::volumeKey(v)] += fragMJ * mat.healthPerMJ;
        }
        otherHits += hits;
        flushCombatDamage(id, game::damage::DamageType::Explosion, world::coordinates::makeWorldPositionFromMeters(p));
        std::cerr << "[Explosion] fragments: ship " << id.value << " at " << d << " m took " << hits << " hits" << std::endl;
    }
    {
        // the blast pushes and spins its own ship: ~2 % of the energy goes into the hull's motion, along the reactor's
        // offset from the centre (an off-centre reactor turns the hull - a big ship tumbling into its neighbours)
        auto& tr2 = ship->core().transform();
        const double mass = std::max(1.0e3, static_cast<double>(ship->core().effectivePhysics().massKg));
        const double p = std::sqrt(2.0 * mass * 0.02 * e.energyMJ * 1.0e6);   // momentum, kg m/s
        glm::dvec3 off = glm::dvec3(c);
        const glm::dvec3 dirB = glm::length(off) > 0.1 ? -glm::normalize(off) : glm::dvec3(0.0, 1.0, 0.0);
        const glm::dvec3 Jw = R * dirB * p;
        if (tr2.motion.travelFrame.valid)
            tr2.motion.localVelocityMps += tr2.motion.travelFrame.worldToLocalVector(Jw / mass);
        double rad = 1.0;
        for (const auto& v : hit.volumes)
            if (glm::length(v.center) > 1.0e-4f)
                rad = std::max(rad, static_cast<double>(glm::length(v.center) + glm::length(v.halfSize)));
        const glm::dvec3 dw = glm::cross(off, dirB * p) / (0.25 * mass * rad * rad);   // body frame
        tr2.pitchRate += static_cast<float>(dw.x);
        tr2.yawRate += static_cast<float>(dw.y);
        tr2.rollRate += static_cast<float>(dw.z);
        std::cerr << "[Explosion] ship " << e.shipId.value << ": kicked " << glm::length(Jw) / mass << " m/s, spin "
                  << glm::length(dw) << " rad/s" << std::endl;
    }
    recordCombatImpact(e.shipId, where, e.energyMJ, 2, e.moduleId);
    // the reactor is gone: mark it, and judge the ship (all propulsion reactors lost -> out of the fight)
    ship->core().moduleRuntime().markModuleDestroyed(e.moduleId, 0.0f);
    ship->core().refreshRuntimeState();
    markShipGraphDirty(e.shipId);
    disableShipIfCombatLost(e.shipId, *ship);
    std::cerr << "[Explosion] ship " << e.shipId.value << ": REACTOR " << e.moduleId << " EXPLODED (" << e.cause << "), "
              << e.energyMJ << " MJ: flash on " << near.size() << " modules, " << ownHits << "/" << N
              << " fragments hit the own hull, " << otherHits << " hit other ships" << std::endl;
}

// ============================================================================ containment emergency (author 2026-11-10)
void GameSimulation::startContainmentEmergency(EntityId shipId, const std::string& moduleId, double chance, const std::string& cause)
{
    if (m_explodedReactors.count(std::to_string(shipId.value) + "#" + moduleId))
        return;
    for (const auto& c : m_containment)
        if (c.shipId == shipId && c.moduleId == moduleId)
            return;
    const Ship* ship = getShip(shipId);
    if (!ship)
        return;
    ContainmentEmergency c;
    c.shipId = shipId;
    c.moduleId = moduleId;
    c.cause = cause;
    c.startS = m_combatClockS;
    c.chance = std::clamp(chance, 0.0, 1.0);
    // the fuse: 20-60 s, not known to anyone aboard
    const std::size_t h = std::hash<std::string>{}(moduleId + "#fuse#" + std::to_string(shipId.value) + "#" +
                                                   std::to_string(static_cast<long long>(m_combatClockS * 10.0)));
    c.fuseS = 20.0 + 40.0 * static_cast<double>(h % 1000) / 1000.0;
    if (const auto* m = static_cast<const world::modules::ObjectModuleRuntime&>(ship->core().moduleRuntime()).findModule(moduleId))
        c.startTemperatureK = std::max(1500.0, m->engineReactorOperating.temperatureK);
    else
        c.startTemperatureK = 2500.0;
    m_containment.push_back(c);
    std::cerr << "[Containment] ship " << shipId.value << ": " << moduleId << " CONTAINMENT FAILING (" << cause
              << ") - siren; base blast chance " << c.chance << ", fuse unknown (" << c.fuseS << " s)" << std::endl;
}

void GameSimulation::updateContainmentEmergencies(double dt)
{
    std::unordered_map<EntityId, std::vector<ShipCore::ContainmentAlarm>> alarms;
    for (auto it = m_containment.begin(); it != m_containment.end();)
    {
        Ship* ship = getShip(it->shipId);
        if (!ship)
        {
            it = m_containment.erase(it);
            continue;
        }
        auto& c = *it;
        auto& core = ship->core();
        const double t = m_combatClockS - c.startS;
        // the core heats towards the critical temperature and beyond over the fuse (the HUD reads it)
        const double coreK = c.startTemperatureK + (3400.0 - c.startTemperatureK) * std::min(1.0, t / c.fuseS);
        const bool reactorsOff = !core.propulsionReactorsOn();
        const bool repairing = core.activeRepairJobCount() > 0 ||
                               (c.droneDispatchedS >= 0.0 && m_combatClockS - c.droneDispatchedS >= 8.0);
        const bool droneEnRoute = c.droneDispatchedS >= 0.0 && !repairing;
        if (repairing)
            c.stabilise01 += dt / 25.0;   // a repair drone at work: ~25 s of work stabilises the containment
        // the chance at the end of the fuse: switching the propulsion reactors off helps, but not always
        double chance = c.chance * (reactorsOff ? 0.4 : 1.0);
        ShipCore::ContainmentAlarm a;
        a.moduleId = c.moduleId;
        a.elapsedS = t;
        a.coreTemperatureK = coreK;
        a.advice = droneEnRoute ? "ремонтный дрон летит к реактору"
                 : repairing ? "ремонтный дрон работает - стабилизация " + std::to_string(static_cast<int>(100.0 * std::min(1.0, c.stabilise01))) + " %"
                 : reactorsOff ? "реакторы отключены - шанс ниже, но не ноль; катапульта L-Ctrl+Q"
                               : "отключите реакторы (Ctrl+Alt+Shift+I), катапульта (L-Ctrl+Q) или ремонтный дрон (Ctrl+R)";
        alarms[c.shipId].push_back(a);
        if (m_combatClockS - c.lastLogS >= 5.0)
        {
            c.lastLogS = m_combatClockS;
            std::cerr << "[Containment] ship " << c.shipId.value << " " << c.moduleId << " t+" << static_cast<int>(t) << " s: core "
                      << static_cast<int>(coreK) << " K, reactors " << (reactorsOff ? "OFF" : "on") << ", repair "
                      << (repairing ? "yes" : "no") << ", stabilised " << static_cast<int>(100.0 * std::min(1.0, c.stabilise01)) << " %"
                      << std::endl;
        }
        if (c.stabilise01 >= 1.0)
        {
            std::cerr << "[Containment] ship " << c.shipId.value << " " << c.moduleId << ": STABILISED by the repair drone at t+"
                      << static_cast<int>(t) << " s - no blast; the reactor is offline" << std::endl;
            m_explodedReactors.insert(std::to_string(c.shipId.value) + "#" + c.moduleId);   // never again
            it = m_containment.erase(it);
            continue;
        }
        if (t >= c.fuseS)
        {
            const std::size_t h = std::hash<std::string>{}(c.moduleId + "#outcome#" + std::to_string(c.shipId.value) + "#" +
                                                           std::to_string(static_cast<long long>(c.startS * 10.0)));
            const double roll = static_cast<double>(h % 10000) / 10000.0;
            if (roll < chance)
            {
                std::cerr << "[Containment] ship " << c.shipId.value << " " << c.moduleId << ": BLAST at t+" << static_cast<int>(t)
                          << " s (roll " << roll << " < chance " << chance << ")" << std::endl;
                queueReactorExplosion(c.shipId, c.moduleId, c.cause + ", after " + std::to_string(static_cast<int>(t)) + " s");
            }
            else
            {
                std::cerr << "[Containment] ship " << c.shipId.value << " " << c.moduleId << ": collapsed QUIETLY at t+"
                          << static_cast<int>(t) << " s (roll " << roll << " >= chance " << chance << ") - the reactor is dead" << std::endl;
                m_explodedReactors.insert(std::to_string(c.shipId.value) + "#" + c.moduleId);
                core.moduleRuntime().markModuleDestroyed(c.moduleId, 0.0f);
                core.refreshRuntimeState();
                markShipGraphDirty(c.shipId);
                disableShipIfCombatLost(c.shipId, *ship);
            }
            it = m_containment.erase(it);
            continue;
        }
        ++it;
    }
    // publish (an empty list clears a finished alarm)
    for (auto& [id, sp] : m_ships)
    {
        auto f = alarms.find(id);
        if (f != alarms.end())
            sp->core().setContainmentAlarms(std::move(f->second));
        else if (!sp->core().containmentAlarms().empty())
            sp->core().setContainmentAlarms({});
    }
}

// ============================================================================ collisions (author 2026-11-10)
namespace
{
struct WorldBox
{
    glm::dvec3 c;
    glm::dmat3 R;   // columns = box axes in the world
    glm::dvec3 h;
    std::size_t index;
};

// separating-axis test of two oriented boxes; returns the penetration depth (<= 0: apart)
double obbOverlap(const WorldBox& a, const WorldBox& b)
{
    const glm::dvec3 t = b.c - a.c;
    double minPen = 1.0e30;
    auto test = [&](const glm::dvec3& axisIn) {
        const double L = glm::length(axisIn);
        if (L < 1.0e-9)
            return true;
        const glm::dvec3 ax = axisIn / L;
        double ra = 0.0, rb = 0.0;
        for (int i = 0; i < 3; ++i)
        {
            ra += a.h[i] * std::abs(glm::dot(a.R[i], ax));
            rb += b.h[i] * std::abs(glm::dot(b.R[i], ax));
        }
        const double pen = ra + rb - std::abs(glm::dot(t, ax));
        if (pen <= 0.0)
            return false;
        minPen = std::min(minPen, pen);
        return true;
    };
    for (int i = 0; i < 3; ++i)
        if (!test(a.R[i]) || !test(b.R[i]))
            return 0.0;
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < 3; ++k)
            if (!test(glm::cross(a.R[i], b.R[k])))
                return 0.0;
    return minPen;
}
} // namespace

void GameSimulation::collideShips(double dt)
{
    (void)dt;
    const auto& cat = weaponCatalog();
    struct Body
    {
        EntityId id;
        Ship* ship;
        glm::dvec3 p, v, w;   // position, velocity, angular velocity (world)
        glm::dmat3 R;
        double m, I, r;
    };
    std::vector<Body> bodies;
    for (auto& [id, sp] : m_ships)
    {
        auto& tr = sp->core().transform();
        if (tr.motion.mode != game::navigation::MotionMode::HubTactical || !tr.motion.travelFrame.valid ||
            isDockStationControlledShip(id))
            continue;
        Body b;
        b.id = id;
        b.ship = sp.get();
        b.p = world::coordinates::fullMeters(tr.worldPosition);
        const auto t = m_combatTracks.find(id);
        b.v = t != m_combatTracks.end() && t->second.valid ? t->second.velocityW : glm::dvec3(0.0);
        b.R = basisOf(tr);
        b.w = b.R * glm::dvec3(tr.pitchRate, tr.yawRate, tr.rollRate);
        b.m = std::max(1.0e3, static_cast<double>(sp->core().effectivePhysics().massKg));
        b.r = 1.0;
        for (const auto& v : sp->core().hitComponent().volumes)
            if (!v.destroyed && glm::length(v.center) > 1.0e-4f)
                b.r = std::max(b.r, static_cast<double>(glm::length(v.center) + glm::length(v.halfSize)));
        b.I = 0.25 * b.m * b.r * b.r;   // a plate-like hull: radius of gyration ~ r / 2
        bodies.push_back(b);
    }
    const std::size_t shipCount = bodies.size();
    // stations and other static objects (author 2026-11-10: "why does the ship fly through the station"): bodies of
    // infinite mass; their spin (rotating modules) is in the contact velocity. The dock modules (hex dock, its service
    // block, guidance docks) are left out until their hit boxes are checked against the docking corridors.
    std::vector<const StaticObject*> statics;
    for (const auto& [oid, o] : m_staticObjects)
    {
        if (o.type == ObjectType::HubHexDock || o.type == ObjectType::HubDockServiceBlock ||
            o.type == ObjectType::GuidanceDockCube || o.type == ObjectType::GuidanceDockCylinder || o.hitComponent.volumes.empty())
            continue;
        Body b;
        b.id = oid;
        b.ship = nullptr;
        b.p = world::coordinates::fullMeters(o.worldPosition);
        auto& track = m_staticTracks[oid];
        b.v = track.valid && m_combatClockS > track.t ? (b.p - track.p) / (m_combatClockS - track.t) : glm::dvec3(0.0);
        track.p = b.p;
        track.t = m_combatClockS;
        track.valid = true;
        b.R = glm::dmat3(glm::mat3(o.orientation));
        b.w = b.R * glm::dvec3(o.angularVelocity);
        b.m = 1.0e15;
        b.I = 1.0e20;
        b.r = 1.0;
        for (const auto& v : o.hitComponent.volumes)
            if (!v.destroyed)
                b.r = std::max(b.r, static_cast<double>(glm::length(v.center) + glm::length(v.halfSize)));
        bodies.push_back(b);
        statics.push_back(&o);
    }
    for (std::size_t i = 0; i < bodies.size(); ++i)
        for (std::size_t k = i + 1; k < bodies.size(); ++k)
        {
            if (i >= shipCount)
                break;   // static-static pairs never collide
            Body& A = bodies[i];
            Body& B = bodies[k];
            if (B.ship && A.ship->core().transform().motion.systemId != B.ship->core().transform().motion.systemId)
                continue;
            if (glm::length(B.p - A.p) >= A.r + B.r)
                continue;
            // narrow phase: every pair of placed, intact hit boxes
            auto boxes = [&](const Body& X) {
                std::vector<WorldBox> out;
                const auto& vols = X.ship ? X.ship->core().hitComponent().volumes
                                          : statics[static_cast<std::size_t>(&X - &bodies[shipCount])]->hitComponent.volumes;
                for (std::size_t n = 0; n < vols.size(); ++n)
                {
                    const auto& v = vols[n];
                    if (v.destroyed || glm::length(v.center) < 1.0e-4f)
                        continue;
                    WorldBox wb;
                    wb.c = X.p + X.R * glm::dvec3(v.center);
                    wb.R = X.R * glm::dmat3(v.orientation);
                    wb.h = glm::dvec3(v.halfSize);
                    wb.index = n;
                    out.push_back(wb);
                }
                return out;
            };
            const auto ba = boxes(A), bb = boxes(B);
            std::vector<std::pair<std::size_t, std::size_t>> touching;
            glm::dvec3 contact(0.0), dirSum(0.0);
            double maxPen = 0.0;
            for (const auto& x : ba)
            {
                if (glm::length(x.c - B.p) > B.r + glm::length(x.h))
                    continue;
                for (const auto& y : bb)
                {
                    const double pen = obbOverlap(x, y);
                    if (pen <= 0.0)
                        continue;
                    touching.emplace_back(x.index, y.index);
                    contact += 0.5 * (x.c + y.c);
                    dirSum += y.c - x.c;
                    maxPen = std::max(maxPen, pen);
                }
            }
            if (touching.empty())
                continue;
            contact /= static_cast<double>(touching.size());
            glm::dvec3 n = glm::length(dirSum) > 1.0e-6 ? glm::normalize(dirSum) : glm::normalize(B.p - A.p);
            const glm::dvec3 rA = contact - A.p, rB = contact - B.p;
            const glm::dvec3 vRel = (B.v + glm::cross(B.w, rB)) - (A.v + glm::cross(A.w, rA));
            const double vn = glm::dot(vRel, n);
            const std::uint64_t pair = (static_cast<std::uint64_t>(std::min(A.id.value, B.id.value)) << 32) |
                                       std::max(A.id.value, B.id.value);
            // separate the overlap in any case (no sinking into each other), impulse only while closing
            auto& ta = A.ship->core().transform();
            ShipTransform* tbp = B.ship ? &B.ship->core().transform() : nullptr;
            const double push = std::min(maxPen, 0.5);
            ta.motion.localPositionMeters += ta.motion.travelFrame.worldToLocalVector(-n * push * B.m / (A.m + B.m));
            if (tbp)
                tbp->motion.localPositionMeters += tbp->motion.travelFrame.worldToLocalVector(n * push * A.m / (A.m + B.m));
            if (vn >= -0.05 || m_combatClockS - m_devContactClockS[pair] < 0.25)
                continue;
            m_devContactClockS[pair] = m_combatClockS;
            const double e = 0.2;
            const glm::dvec3 cA = glm::cross(rA, n), cB = glm::cross(rB, n);
            const double denom = 1.0 / A.m + 1.0 / B.m + glm::dot(cA, cA) / A.I + glm::dot(cB, cB) / B.I;
            const double J = -(1.0 + e) * vn / denom;   // > 0
            const glm::dvec3 Jv = n * J;
            // linear and angular velocity changes (angular -> the body rates the flight model integrates)
            ta.motion.localVelocityMps += ta.motion.travelFrame.worldToLocalVector(-Jv / A.m);
            if (tbp)
                tbp->motion.localVelocityMps += tbp->motion.travelFrame.worldToLocalVector(Jv / B.m);
            const glm::dvec3 dwA = glm::transpose(A.R) * (glm::cross(rA, -Jv) / A.I);
            const glm::dvec3 dwB = glm::transpose(B.R) * (glm::cross(rB, Jv) / B.I);
            ta.pitchRate += static_cast<float>(dwA.x); ta.yawRate += static_cast<float>(dwA.y); ta.rollRate += static_cast<float>(dwA.z);
            if (tbp)
            {
                tbp->pitchRate += static_cast<float>(dwB.x);
                tbp->yawRate += static_cast<float>(dwB.y);
                tbp->rollRate += static_cast<float>(dwB.z);
            }
            // the energy the impact dissipates: (1 - e^2) of the normal kinetic energy at the contact
            const double dissipatedMJ = 0.5 * (1.0 - e * e) * vn * vn / denom * 1.0e-6;
            // half to each ship; against a station the ship takes it all (the station's own damage is a later slice)
            const double health = (B.ship ? 0.5 : 1.0) * dissipatedMJ * cat.materials.healthPerMJ;
            const auto where = world::coordinates::makeWorldPositionFromMeters(contact);
            // What the impact does (author 2026-11-10: "where it comes from, how strong, where it lands"), per ship:
            //  - the impulse at the contact point with its lever arm: a push and, off the centre of mass, a spin (above);
            //  - energy per touched module below ~0.5 MJ: a bump, no damage;
            //  - above: the touched modules lose health (energy x health_per_mj);
            //  - the impulse loads the links that hold each touched part (ShipCore::applyPartImpulse): over a link's
            //    tolerance it breaks and the part is knocked off; below, the link fatigues;
            //  - more than ~3 GJ on one ship: the hull is crushed as a whole (destroyed, cause 4 "collision").
            for (int side = 0; side < (B.ship ? 2 : 1); ++side)
            {
                Body& X = side == 0 ? A : B;
                std::unordered_map<std::string, int> keys;
                std::unordered_map<std::string, int> modules;
                for (const auto& [ia, ib] : touching)
                {
                    const auto& v = X.ship->core().hitComponent().volumes[side == 0 ? ia : ib];
                    ++keys[game::combat::volumeKey(v)];
                    if (!v.supportLinkVolume)
                        ++modules[v.moduleId];
                }
                const double total = static_cast<double>(touching.size());
                const double shipMJ = health / cat.materials.healthPerMJ;
                for (const auto& [key, cnt] : keys)
                {
                    const double mj = shipMJ * cnt / total;
                    if (mj >= 0.5)
                        m_burnStates[X.id].pendingHealthLoss[key] += health * cnt / total;
                }
                int knocked = 0;
                for (const auto& [mod, cnt] : modules)
                    knocked += X.ship->core().applyPartImpulse(mod, (J / 1000.0) * cnt / total);
                flushCombatDamage(X.id, game::damage::DamageType::Collision, where);
                if (knocked > 0)
                {
                    markShipGraphDirty(X.id);
                    std::cerr << "[Collision] ship " << X.id.value << ": " << knocked << " link(s) torn - parts knocked loose" << std::endl;
                }
                const auto cause0 = X.ship->core().transform().motion.destroyedCause;
                if (shipMJ > 3000.0 && (cause0 == 0 || cause0 == 3))   // a disabled hull can still be crushed
                {
                    auto& mo = X.ship->core().transform().motion;
                    mo.destroyedCause = 4;
                    X.ship->setControlState(ShipControlState {});
                    X.ship->core().destroyWholeShip();
                    markShipGraphDirty(X.id);
                    if (!isPlayerControlled(X.id))
                        (void)markShipDerelict(X.id);
                    std::cerr << "[Collision] ship " << X.id.value << ": CRUSHED (" << shipMJ << " MJ at " << -vn << " m/s)" << std::endl;
                }
            }
            std::cerr << "[Collision] ship " << A.id.value << " x " << (B.ship ? "ship " : "STATION OBJECT ") << B.id.value << ": closing " << -vn << " m/s, "
                      << touching.size() << " touching box pairs, dissipated " << dissipatedMJ << " MJ, spin "
                      << glm::length(dwA) << " / " << glm::length(dwB) << " rad/s" << std::endl;
            std::cerr << "[Ram] ships " << A.id.value << " x " << B.id.value << ": closing " << -vn << " m/s, masses "
                      << A.m / 1000.0 << " / " << B.m / 1000.0 << " t, dissipated " << dissipatedMJ << " MJ, impulse "
                      << J / 1000.0 << " kN s" << std::endl;
            for (const EntityId sid : {A.id, B.id})
                if (m_combatTargetDummies.count(sid))
                    m_devRamRetreatUntilS[sid] = m_combatClockS + 6.0;
        }
}
