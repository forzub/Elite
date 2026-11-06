// The body autopilot flown by the CLIENT (NAVIGATION_V2 §1, author decision 2026-11-06: planners and
// autopilots on the client, the server = plant + authority on ordinary commands). The neutral module
// (globalflight/BodyAutopilot) runs here on every new server snapshot: the ship as the server last
// published it, the bodies of the client's celestial runtime at the SAME universe time, the ground of
// the shared terrain probe, the keep-out of the replicated hubs and ships. Its command goes out
// through the ordinary input stream (ShipControlState: the global-flight part, the landing pilot's
// ordinary controls, the frame / free-rotation requests) - the server only executes it.
#include "src/game/SpaceStateInternal.h"

#include "src/game/catalog/ships/ShipDesignCatalog.h"
#include "src/game/navigation/FreeFlightCodes.h"
#include "src/game/navigation/globalflight/BodyTerrainProbe.h"
#include "src/game/navigation/rules/BodyFlightRules.h"
#include "src/game/navigation/rules/JDrive.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/system_map/PlanetBodyOrientation.h"

namespace
{
namespace gf = game::navigation::globalflight;
using game::navigation::AutopilotDisplayPhase;
using game::navigation::code;
}

bool SpaceState::clientBodyAutopilotWorld(gf::BodyAutopilotEnvironment& env, gf::BodyAutopilotShip& sample)
{
    if (!m_client)
        return false;
    const auto& world = m_client->world();
    const auto shipIt = world.ships().find(m_playerId.value);
    if (shipIt == world.ships().end() || !shipIt->second.descriptor)
        return false;
    const auto& player = shipIt->second;
    const auto& tr = player.transform;
    const auto& motion = tr.motion;
    const auto& meta = m_client->lastSimulationMetadata();
    const auto* snapshot = m_client->resolveCelestialSystem(motion.systemId, meta.universeTimeSeconds);
    const auto* atlas = m_client->starAtlas();
    const auto* system = atlas ? atlas->findSystem(motion.systemId) : nullptr;
    if (!snapshot || !system)
        return false;
    m_bodyAutopilotSnapshot = *snapshot;
    // the gravity bodies exactly as the server builds them (GameSimulation::rebuildNavigationGravityContext)
    m_bodyAutopilotBodies.clear();
    for (const auto& b : m_bodyAutopilotSnapshot.bodies)
        for (const auto& c : system->bodies)
            if (c.id == b.id && c.radiusKm > 0.0 && c.gravitationalParameterM3s2 > 0.0)
            {
                game::navigation::GravityBody g;
                g.id = b.id;
                g.centerMeters = b.worldMeters;
                g.radiusMeters = c.radiusKm * 1000.0;
                g.gravitationalParameterM3s2 = c.gravitationalParameterM3s2;
                g.atmosphereRadiusMeters = std::max(g.radiusMeters, c.atmosphereOuterRadiusKm * 1000.0);
                m_bodyAutopilotBodies.push_back(std::move(g));
            }
    env.bodies = &m_bodyAutopilotBodies;
    const auto* snap = &m_bodyAutopilotSnapshot;
    env.bodyOrientation = [snap](const std::string& id, glm::dmat3& worldFromBody) {
        for (const auto& b : snap->bodies)
            if (b.id == id)
            {
                const auto o = game::system_map::makePlanetTextureOrientation(b.axialTiltDeg, b.axisNodeDeg, b.rotationPhaseRad,
                                                                              b.textureLongitudeOffsetDeg);
                worldFromBody = glm::dmat3(glm::dvec3(o.prime), glm::dvec3(o.north), glm::dvec3(o.east));
                return true;
            }
        return false;
    };
    env.terrain = [snap](const std::string& id, const glm::dvec3& worldDirection, game::navigation::landing::TerrainPoint& out) {
        gf::BodyTerrainSample g;
        if (!gf::sampleBodyTerrain(*snap, id, worldDirection, g))
            return false;
        out.heightM = g.heightM;
        out.slopeDeg = g.slopeDeg;
        out.material = g.material;
        return true;
    };
    // the J keep-out: hubs / stations (JDrive.h 20 km) and other ships (5 km), as replicated
    const auto* client = m_client;
    const std::uint64_t self = m_playerId.value;
    const int systemId = motion.systemId;
    env.keepOut = [client, self, systemId](const glm::dvec3& p, gf::KeepOutHit& hit) {
        namespace jd = game::navigation::jdrive;
        bool found = false;
        double best = std::numeric_limits<double>::infinity();
        for (const auto& [hubId, hub] : client->world().hubs())
        {
            const glm::dvec3 h = world::coordinates::fullMeters(hub.worldPosition);
            const double d = glm::length(h - p);
            if (d < jd::KeepOutObjectM && d < best)
            {
                best = d;
                hit = {hubId, h, hub.worldVelocityMps, d, false};
                found = true;
            }
        }
        for (const auto& [id, other] : client->world().ships())
        {
            if (id == self || other.transform.motion.systemId != systemId)
                continue;
            const auto& om = other.transform.motion;
            const glm::dvec3 o = om.travelFrame.valid ? om.travelFrame.localToWorldPosition(om.localPositionMeters)
                                                      : world::coordinates::fullMeters(other.transform.worldPosition);
            const double d = glm::length(o - p);
            if (d < jd::KeepOutShipM && d < best)
            {
                best = d;
                const glm::dvec3 ov = om.travelFrame.valid ? om.travelFrame.localToWorldVelocity(om.localPositionMeters, om.localVelocityMps)
                                                           : om.worldVelocityMps;
                hit = {"ship " + std::to_string(id) + " (client)", o, ov, d, true};
                found = true;
            }
        }
        return found;
    };

    // the ship as the server last published it
    sample = gf::BodyAutopilotShip {};
    sample.shipId = static_cast<std::uint32_t>(m_playerId.value);
    // the global motion is the travel frame's (normal flight / the ground / an orbit), as the server
    // reads it; the replicated world position / velocity stay at the hub-frame values in normal flight
    if (motion.travelFrame.valid)
    {
        sample.positionM = motion.travelFrame.localToWorldPosition(motion.localPositionMeters);
        sample.velocityMps = motion.travelFrame.localToWorldVelocity(motion.localPositionMeters, motion.localVelocityMps);
    }
    else
    {
        sample.positionM = world::coordinates::fullMeters(tr.worldPosition);
        sample.velocityMps = motion.worldVelocityMps;
    }
    sample.forward = glm::dvec3(tr.forward());
    sample.right = glm::dvec3(tr.right());
    sample.up = glm::dvec3(tr.up());
    sample.pitchRate = tr.pitchRate;
    sample.yawRate = tr.yawRate;
    sample.rollRate = tr.rollRate;
    const auto slots = player.physicalControlModel != player.descriptor->physicalControlModel
        ? game::ship::plant::healthySlotState(game::ship::plant::physicalControlSlots(player.physicalControlModel))
        : game::ship::capability::buildPhysicalControlSlotState(*player.descriptor, player.physicalControlModel, player.modules);
    const auto capability = game::ship::capability::buildShipCapability(*player.descriptor, player.physicalControlModel, slots);
    if (!capability.valid)
        return false;
    sample.physics = capability.effectivePhysics;
    sample.controlParams = game::ship::telemetry::applyAuthorityToControlParams(sample.physics, player.controlAuthorityTelemetry);
    sample.nativeLaw = game::ship::plant::nativeFlightLaw(player.physicalControlModel);
    sample.currentLaw = motion.localControlLaw;
    sample.assistedFreeRotation = motion.assistedFreeRotation;
    const auto* design = game::catalog::ships::ShipDesignCatalog::instance().findShip(player.catalogShipId);
    sample.shuttle = design && design->shipClass == "surface_lifter";
    // the dominant body: the server's rule (BodyFlightRules.h, the id-prefix parent)
    using B = game::navigation::GravityBody;
    bool root = false;
    const std::size_t k = game::navigation::rules::dominantBody(
        m_bodyAutopilotBodies, sample.positionM, [](const B& b) -> const std::string& { return b.id; }, [](const B&) { return std::string(); },
        [](const B& b) { return b.gravitationalParameterM3s2; }, [](const B& b) { return b.centerMeters; }, &root);
    if (k < m_bodyAutopilotBodies.size())
    {
        sample.primaryBodyId = m_bodyAutopilotBodies[k].id;
        sample.primaryAltitudeM = glm::length(sample.positionM - m_bodyAutopilotBodies[k].centerMeters) - m_bodyAutopilotBodies[k].radiusMeters;
        m_bodyAutopilotPrimaryIsRoot = root;
    }
    sample.groundBodyId = motion.freeFlightOrbitBodyId;
    sample.groundHeightM = motion.freeFlightGroundHeightM;
    sample.groundMaterial = motion.freeFlightGroundMaterial;
    sample.referenceVelocityMps = motion.freeFlightReferenceVelocityMps;
    sample.groundFrame = motion.velocityReferenceKind == 4;
    sample.surfaceState = motion.freeFlightSurfaceState;
    sample.mainEngineAccelerationMps2 = motion.mainEngineAccelerationMps2;
    sample.engineAccelerationMps2 = motion.engineAccelerationMps2;
    sample.refusal = motion.freeFlightCaptureRefusal;
    return true;
}

void SpaceState::requestFreeFlightAutopilot(const std::string& bodyId, const std::string& landingSpec)
{
    // the client flies it (the engage happens on the next sample, updateClientBodyAutopilot)
    m_bodyAutopilotEngagePending = true;
    m_bodyAutopilotLastTick = 0;     // the first step after the engage has no dt
    m_bodyAutopilotEngageBody = bodyId;
    m_bodyAutopilotEngageSpec = landingSpec;
    std::cerr << "[Autopilot] requested (client) -> " << (bodyId.empty() ? std::string("the dominant body") : bodyId)
              << (landingSpec.empty() ? std::string() : " " + landingSpec) << std::endl;
}

void SpaceState::cancelClientBodyAutopilot()
{
    m_bodyAutopilotEngagePending = false;
    if (!m_bodyAutopilot.active())
        return;
    m_bodyAutopilot.cancel();
    m_bodyAutopilotRestoreUntil = m_client ? m_client->lastSimulationMetadata().serverTimeSeconds + 2.0 : 0.0;
    m_bodyAutopilotDisplay = {};
}

bool SpaceState::clientBodyAutopilotFlying() const
{
    return m_bodyAutopilot.active() || m_bodyAutopilotEngagePending;
}

void SpaceState::applyClientBodyAutopilotDisplay(game::navigation::DynamicMotionState& motion) const
{
    // the route the client flies
    motion.routeState = static_cast<std::uint8_t>(clientRouteState());
    motion.routeLegIndex = static_cast<std::uint8_t>(m_clientRoute.state == ClientRouteRun::State::Idle ? 0 : std::min<std::size_t>(m_clientRoute.index + 1, 255));
    motion.routeLegCount = static_cast<std::uint8_t>(std::min<std::size_t>(m_clientRoute.legs.size(), 255));
    if (!m_bodyAutopilot.active() && m_bodyAutopilotDisplay.displayPhase == 0)
        return;
    motion.freeFlightAutopilotPhase = m_bodyAutopilotDisplay.displayPhase;
    motion.freeFlightAutopilotTargetId = m_bodyAutopilotDisplay.displayTargetId;
    motion.freeFlightAutopilotRangeM = m_bodyAutopilotDisplay.displayRangeM;
    motion.freeFlightAutopilotRelSpeedMps = m_bodyAutopilotDisplay.displayRelSpeedMps;
    motion.freeFlightAutopilotAccelMps2 = m_bodyAutopilotDisplay.displayAccelMps2;
    if (m_bodyAutopilot.active())
        motion.freeFlightCaptureRefusal = m_bodyAutopilotDisplay.refusal;
}

void SpaceState::updateClientBodyAutopilot()
{
    if (!m_client)
        return;
    const auto& meta = m_client->lastSimulationMetadata();
    const bool restoring = !m_bodyAutopilot.active() && meta.serverTimeSeconds < m_bodyAutopilotRestoreUntil;
    if (!m_bodyAutopilot.active() && !m_bodyAutopilotEngagePending && !restoring)
    {
        if (m_bodyAutopilotOwnsControl)
        {
            m_bodyAutopilotOwnsControl = false;
            m_clientAutopilotControlActive = false;
            m_clientAutopilotControl = {};
        }
        return;
    }
    // one control step per new server sample
    const bool fresh = meta.serverTick != m_bodyAutopilotLastTick;
    if (!fresh)
    {
        if (m_bodyAutopilotOwnsControl)
        {
            m_clientAutopilotControlActive = true;
            m_client->submitInput(m_clientAutopilotControl);
        }
        return;
    }
    const double dt = m_bodyAutopilotLastTick != 0 ? meta.serverTimeSeconds - m_bodyAutopilotLastServerTime : 0.0;
    m_bodyAutopilotLastTick = meta.serverTick;
    m_bodyAutopilotLastServerTime = meta.serverTimeSeconds;
    gf::BodyAutopilotEnvironment env;
    gf::BodyAutopilotShip sample;
    if (!clientBodyAutopilotWorld(env, sample))
        return;

    if (restoring)
    {
        // the landing pilot hands the controls back: the flight law it found, no free rotation
        ShipControlState restore {};
        if (sample.currentLaw != m_bodyAutopilot.lawBefore())
        {
            restore.localControlLawCommandValid = true;
            restore.requestedLocalControlLaw = m_bodyAutopilot.lawBefore();
        }
        if (m_bodyAutopilot.freeRotation() && sample.assistedFreeRotation)
            restore.globalFreeRotationRequest = 2;
        if (!restore.localControlLawCommandValid && restore.globalFreeRotationRequest == 0)
        {
            m_bodyAutopilotRestoreUntil = 0.0;
            m_bodyAutopilot.clearFreeRotation();
            std::cerr << "[Autopilot] landing pilot OFF (client): the controls are the pilot's again" << std::endl;
            return;
        }
        m_clientAutopilotControl = restore;
        m_clientAutopilotControlActive = true;
        m_bodyAutopilotOwnsControl = true;
        m_client->submitInput(m_clientAutopilotControl);
        return;
    }

    if (m_bodyAutopilotEngagePending)
    {
        m_bodyAutopilotEngagePending = false;
        std::uint8_t refusal = 0;
        const std::string departure = m_bodyAutopilotPrimaryIsRoot ? std::string() : sample.primaryBodyId;
        if (!m_bodyAutopilot.engage(env, sample, m_bodyAutopilotEngageBody, departure, m_bodyAutopilotEngageSpec, refusal))
        {
            m_bodyAutopilotDisplay = {};
            return;
        }
        m_bodyAutopilotDisplay = {};
        m_bodyAutopilotDisplay.displayPhase = code(AutopilotDisplayPhase::Transfer);
        m_bodyAutopilotDisplay.displayTargetId = m_bodyAutopilot.targetId();
        if (m_clientAutopilotControlActive && !m_bodyAutopilotOwnsControl)
            std::cerr << "[Autopilot] (client) the docking autopilot gives way" << std::endl;
    }

    // touchdown / crash: the plant reports it, the autopilot ends
    if (sample.surfaceState != 0 &&
        (m_bodyAutopilot.stage() == gf::AutopilotStage::GroundStabilized || m_bodyAutopilot.stage() == gf::AutopilotStage::Deorbit))
    {
        std::cerr << "[Autopilot] ship " << sample.shipId << ": " << (sample.surfaceState == 1 ? "LANDED on " : "CRASHED on ")
                  << m_bodyAutopilot.targetId() << " after " << m_bodyAutopilot.seconds() << " s (client autopilot); ground material "
                  << sample.groundMaterial;
        if (m_bodyAutopilot.landing().site().valid && env.bodyOrientation)
        {
            glm::dmat3 worldFromBody(1.0);
            for (const auto& b : m_bodyAutopilotBodies)
                if (b.id == m_bodyAutopilot.targetId() && env.bodyOrientation(b.id, worldFromBody))
                    std::cerr << ", " << std::acos(std::clamp(glm::dot(glm::normalize(sample.positionM - b.centerMeters),
                                                                         glm::normalize(worldFromBody * m_bodyAutopilot.landing().site().bodyDirection)),
                                                              -1.0, 1.0)) * b.radiusMeters
                              << " m from the chosen site (ship centre)";
        }
        std::cerr << std::endl;
        m_bodyAutopilot.finishOnSurface();
        m_bodyAutopilotDisplay = {};
        m_bodyAutopilotDisplay.displayPhase = code(AutopilotDisplayPhase::OnSurface);
        m_bodyAutopilotDisplay.displayTargetId = sample.groundBodyId;
        m_bodyAutopilotRestoreUntil = meta.serverTimeSeconds + 2.0;
        return;
    }

    const auto out = m_bodyAutopilot.step(env, sample, dt);
    if (out.finished)
    {
        m_bodyAutopilotDisplay = {};
        m_bodyAutopilotRestoreUntil = meta.serverTimeSeconds + 2.0;
        return;
    }
    ShipControlState control = out.control;
    if (out.setAssistedFreeRotation)
        control.globalFreeRotationRequest = 1;
    if (out.pilotActive || out.control.globalFlightCommandValid)
    {
        m_bodyAutopilotDisplay = out;
    }
    else
    {
        // no sample yet (the target's velocity needs two): coast, but already the normal flight
        control = ShipControlState {};
        control.globalFlightCommandValid = true;
        control.globalFrameRequest = out.control.globalFrameRequest;
    }
    m_clientAutopilotControl = control;
    m_clientAutopilotControlActive = true;
    m_bodyAutopilotOwnsControl = true;
    m_client->submitInput(m_clientAutopilotControl);
}
