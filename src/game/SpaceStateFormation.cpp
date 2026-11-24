// Joining a ship / keeping a place in its formation (author 2026-11-22; navigation/autopilot/FormationGuidance.h).
// The client turns the leader's replicated state into a reference and flies it with the ordinary pilot (the same
// control stream as the keyboard). DEV: map script "join:<ship entity id>:<wedge|echelon|column|diamond|line>:<slot>"
// and "join:off".
#include "src/game/SpaceStateInternal.h"
#include "src/game/navigation/autopilot/FormationGuidance.h"
#include "src/game/navigation/autopilot/AutopilotLawController.h"

#include <cstdio>
#include <cstdlib>

void SpaceState::devJoinFormation(const std::string& spec)
{
    namespace ap = game::navigation::autopilot;
    if (spec == "off")
    {
        if (m_formationTask.active)
            std::cerr << "[Formation] left the formation of ship " << m_formationTask.leader << std::endl;
        m_formationTask = {};
        m_clientAutopilotControlActive = false;
        m_clientAutopilotControl = ShipControlState {};
        return;
    }
    char kind[32] = "wedge";
    unsigned leader = 0;
    int slot = 0;
    std::string rest = spec;
    if (spec.rfind("nearest", 0) == 0 && m_client)
    {
        // the nearest other ship (a moving lane ship as a test leader - their ids differ from run to run)
        const auto meIt = m_client->world().ships().find(m_playerId.value);
        double best = 1.0e30;
        if (meIt != m_client->world().ships().end())
            for (const auto& [sid, sh] : m_client->world().ships())
                if (sid != m_playerId.value && sh.descriptor)
                {
                    const double d = glm::length(world::coordinates::fullMeters(sh.renderTransform.worldPosition) -
                                                 world::coordinates::fullMeters(meIt->second.renderTransform.worldPosition));
                    if (d < best)
                    {
                        best = d;
                        leader = sid;
                    }
                }
        rest = std::to_string(leader) + spec.substr(7);
        std::cerr << "[Formation] the nearest ship: " << leader << " at " << static_cast<long long>(best) << " m" << std::endl;
    }
    if (std::sscanf(rest.c_str(), "%u:%31[a-z]:%d", &leader, kind, &slot) < 1 || leader == 0)
    {
        std::cerr << "[Formation] join: bad spec '" << spec << "'" << std::endl;
        return;
    }
    m_formationTask = {};
    m_formationTask.active = true;
    // the formation owns the ship: a route / docking request in progress ends (one pilot at a time)
    m_navigationWorkspace.dockingRouteRequests().clear();
    m_formationTask.leader = leader;
    m_formationTask.kind = static_cast<int>(ap::formationKindFromName(kind));
    m_formationTask.slot = std::max(0, slot);
    m_formationPilot.reset();
    std::cerr << "[Formation] join ship " << leader << ": " << ap::formationKindName(ap::formationKindFromName(kind)) << " slot "
              << slot << std::endl;
}

void SpaceState::updateFormationTask(double dt)
{
    namespace ap = game::navigation::autopilot;
    if (!m_formationTask.active || !m_client || dt <= 0.0)
        return;
    const auto& ships = m_client->world().ships();
    const auto meIt = ships.find(m_playerId.value);
    const auto leadIt = ships.find(m_formationTask.leader);
    if (meIt == ships.end() || !meIt->second.descriptor)
        return;
    if (leadIt == ships.end())
    {
        std::cerr << "[Formation] the leader " << m_formationTask.leader << " is gone - the pilot has the ship" << std::endl;
        devJoinFormation("off");
        return;
    }
    const auto& me = meIt->second;
    const auto& lead = leadIt->second;
    const auto& fr = me.referenceFrame;
    if (!fr.valid)
        return;
    // the map frame = the follower's travel frame; both ships' relative state through the SAME transform (the world
    // velocities carry the hub's orbit - they cancel in the difference)
    const glm::dmat3 B(fr.progradeAxis, fr.radialAxis, fr.normalAxis);
    const glm::dmat3 Bt = glm::transpose(B);
    const glm::dvec3 myW = world::coordinates::fullMeters(me.renderTransform.worldPosition);
    const glm::dvec3 myV = me.transform.motion.localVelocityMps;
    const glm::dvec3 myP = me.renderTransform.motion.localPositionMeters;
    ap::FormationInput in;
    in.position = myP;
    in.velocity = myV;
    in.leaderPosition = myP + Bt * (world::coordinates::fullMeters(lead.renderTransform.worldPosition) - myW);
    in.leaderVelocity = myV + Bt * (lead.transform.motion.worldVelocityMps - me.transform.motion.worldVelocityMps);
    in.leaderForward = Bt * glm::dvec3(lead.renderTransform.forward());
    in.leaderUp = Bt * glm::dvec3(lead.renderTransform.up());
    in.leaderLengthMeters = lead.descriptor ? std::max(10.0, static_cast<double>(lead.descriptor->logicalDimensions().length)) : 30.0;
    in.lengthMeters = std::max(10.0, static_cast<double>(me.descriptor->logicalDimensions().length));
    in.kind = static_cast<ap::FormationKind>(m_formationTask.kind);
    in.slot = m_formationTask.slot;
    const auto ref = ap::formationReference(in, static_cast<ap::FormationPhase>(m_formationTask.phase));
    if (static_cast<int>(ref.phase) != m_formationTask.phase)
    {
        static const char* names[] = {"out to its side", "round behind it", "closing to the slot", "in the slot"};
        std::cerr << "[Formation] ship " << m_formationTask.leader << ": " << names[static_cast<int>(ref.phase)] << " ("
                  << static_cast<long long>(ref.distanceToSlot) << " m from the slot)" << std::endl;
        m_formationTask.phase = static_cast<int>(ref.phase);
    }

    // the pilot: the same law controller the routes use, the same control stream as the keyboard
    const auto slotState =
        game::ship::capability::buildPhysicalControlSlotState(*me.descriptor, me.physicalControlModel, me.modules);
    const auto capability = game::ship::capability::buildShipCapability(*me.descriptor, me.physicalControlModel, slotState);
    if (!capability.valid)
        return;
    ap::AutopilotControlRequest rq;
    rq.desiredVelocityMapMps = ref.desiredVelocity;
    rq.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
    rq.actualPositionMapMeters = myP;
    rq.desiredPositionMapMeters = ref.aim;
    rq.positionReferenceValid = false;
    rq.actualVelocityMapMps = myV;
    rq.assistedTargetSpeedMps = std::max(0.0, static_cast<double>(me.transform.motion.targetForwardSpeedMps));
    rq.forwardMap = glm::normalize(Bt * glm::dvec3(me.renderTransform.forward()));
    rq.rightMap = glm::normalize(Bt * glm::dvec3(me.renderTransform.right()));
    rq.upMap = glm::normalize(Bt * glm::dvec3(me.renderTransform.up()));
    rq.pitchRateRadPerSec = me.transform.pitchRate;
    rq.yawRateRadPerSec = me.transform.yawRate;
    rq.rollRateRadPerSec = me.transform.rollRate;
    rq.desiredForwardMap = ref.desiredForward;
    rq.desiredUpMap = ref.desiredUp;
    // in the slot beside a (nearly) resting leader: hold its attitude (the hull pose, not a velocity course)
    rq.terminalAttitudeHold = ref.phase == ap::FormationPhase::Hold && glm::length(in.leaderVelocity) < 2.0;
    rq.deltaSeconds = dt;
    const auto law = me.transform.motion.localControlLaw;
    const ShipControlState control = m_formationPilot.update(law, rq, capability.effectivePhysics);
    submitVirtualPilotControl(control, 0x46u);   // formation flight: the reflex over it (NAV-8)

    if (glfwGetTime() - m_formationTask.logAt > 2.0)
    {
        m_formationTask.logAt = glfwGetTime();
        std::cerr << "[Formation] ship " << m_formationTask.leader << " phase " << m_formationTask.phase << ": "
                  << static_cast<long long>(ref.distanceToSlot) << " m from the slot, "
                  << static_cast<long long>(ref.distanceToAim) << " m to the aim, relative speed "
                  << glm::length(myV - in.leaderVelocity) << " m/s, leader speed " << glm::length(in.leaderVelocity)
                  << std::endl;
    }
}

void SpaceState::arbitrateFlightTask()
{
    if (!m_client)
        return;
    const auto& pending = m_navigationWorkspace.dockingRouteRequests().pending();
    FlightTaskSlot now;
    now.dock = pending.valid();
    now.dockSerial = now.dock ? pending.serial : 0;
    now.formation = m_formationTask.active;
    now.route = m_clientRoute.state != ClientRouteRun::State::Idle;
    now.body = !now.route && (m_bodyAutopilot.active() || m_bodyAutopilotEngagePending);   // a route flies its legs on it
    if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
    {
        const auto& svc = it->second.padService;
        now.pad = svc.size() > 8 && svc.rfind("phase:", 0) == 0 && svc[6] != '3';
    }
    const auto& was = m_flightSlot;
    const char* started = now.dock && now.dockSerial != was.dockSerial ? "docking / landing route"
                        : now.formation && !was.formation               ? "formation"
                        : now.route && !was.route                       ? "map route"
                        : now.body && !was.body                         ? "body autopilot"
                        : now.pad && !was.pad                           ? "pad service"
                                                                        : nullptr;
    if (started)
    {
        std::string ended;
        const std::string s = started;
        if (s != "docking / landing route" && now.dock && s != "pad service")
        {
            m_navigationWorkspace.dockingRouteRequests().clear();
            now.dock = false;
            ended += " the docking route;";
        }
        if (s != "formation" && now.formation)
        {
            devJoinFormation("off");
            now.formation = false;
            ended += " the formation;";
        }
        if (s != "map route" && now.route)
        {
            cancelClientRoute();
            now.route = false;
            ended += " the map route;";
        }
        if (s != "body autopilot" && s != "map route" && now.body)
        {
            cancelClientBodyAutopilot();
            now.body = false;
            ended += " the body autopilot;";
        }
        if (s != "pad service" && now.pad)
        {
            // the service lets the ship go (the docking route itself also does it when it launches)
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::LandOnPad;
            command.index = 0;
            msg.payload = command;
            m_client->sendMessage(msg);
            ended += " the pad service;";
        }
        if (!ended.empty())
            std::cerr << "[FlightTask] " << started << " takes the flight slot - ended:" << ended << std::endl;
    }
    m_flightSlot = now;
}
