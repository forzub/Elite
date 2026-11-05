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
                    rackLocal += glm::dvec3(v.center) - glm::dvec3(0.0, v.halfSize.y + 1.0, 0.0);
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
        if (destroyed)
            std::cerr << "[Combat] ship " << targetId.value << ": DESTROYED " << key << std::endl;
    }
    if (changed)
        markShipGraphDirty(targetId);
}

void GameSimulation::updateCombat(double dt)
{
    if (dt <= 0.0)
        return;
    m_combatClockS += dt;
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

    // ---- lasers
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
        const bool on = laser->pulseHz <= 0.0 || std::fmod(m_combatClockS * laser->pulseHz, 1.0) < laser->duty;
        const double powerMW = on ? laser->powerMW : 0.0;

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
            beam.powerMW = static_cast<float>(on ? laser->powerMW : 0.0);
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
        m.velocityW += accel * dt;
        m.positionW += m.velocityW * dt;
        if (target && std::floor(m.ageS) != std::floor(m.ageS - dt))
            std::cerr << "[Combat] missile " << m.id << " t+" << std::floor(m.ageS) << " s: " << glm::length(targetPos - m.positionW)
                      << " m to target, closing " << glm::length(m.velocityW - targetVel) << " m/s" << std::endl;

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
            // relative motion over the step: the target's own displacement removed
            const glm::dvec3 a = prev - (tpos - tvel * dt), b = m.positionW - tpos;
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
            std::cerr << "[Combat] missile " << m.id << " HIT ship " << id.value << " at " << relSpeed << " m/s, "
                      << (spec->warhead == game::combat::WarheadKind::Slug ? "slug " : "jet ") << energy << " MJ:";
            for (const auto& l : res.layers)
                std::cerr << " " << l.key << (l.holed ? "(through " : "(spent ") << l.spentMJ << ")";
            std::cerr << (res.stuck ? " -> slug STUCK" : res.exited ? " -> out of the hull" : " -> spent") << std::endl;
            recordCombatImpact(id, whereW, energy, spec->warhead == game::combat::WarheadKind::Slug ? 3 : 2,
                               res.layers.empty() ? std::string() : res.layers.front().moduleId);
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
