// SpaceState: Docking / route-autopilot orchestration: route request, planning jobs, safe course, autopilot phases and the docking corridor publication.

#include "src/game/SpaceStateInternal.h"



void SpaceState::updateDockingAdvisory()
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;

    if (!m_client)
        return;

    auto& workspace = m_navigationWorkspace;
    auto& requests = workspace.dockingRouteRequests();
    const auto& pending = requests.pending();
    auto& guidance = workspace.guidance();

    if (pending.valid() &&
        pending.serial != m_lastDockingRequestTraceSerial)
    {
        m_lastDockingRequestTraceSerial = pending.serial;
        std::cerr
            << "[DockClientFlow] observed-request"
            << " serial=" << pending.serial
            << " mode="
            << (pending.mode == DockingRouteRequest::Mode::Automatic
                    ? "automatic"
                    : "guidance")
            << " last_path_serial=" << m_lastDockingPathRequestSerial
            << " phase=" << static_cast<int>(m_clientDockingPhase)
            << " target=" << pending.target.stableObjectId
            << ":" << pending.target.semanticAnchorId
            << std::endl;
    }

    const auto eraseVisibleRoute = [&]()
    {
        if (m_dockAdviceJob)
        {
            m_dockAdviceJob->cancelled.store(
                true,
                std::memory_order_release
            );
        }

        if (!m_activeDockingGuidanceCorridorId.empty())
        {
            guidance.erase(m_activeDockingGuidanceCorridorId);
            guidance.erase(m_activeDockingGuidanceCorridorId + ":frames");
        }
        m_activeDockingGuidanceCorridorId.clear();
        m_dockAdvice = {};
        m_dockAdviceJob.reset();
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockSafeCourse = {};
        m_clientDockingRouteProgressMeters = 0.0;
        m_clientDockingRouteCurveIndex = 0;
        m_clientAutopilotCrossTrackMeters = 0.0;
        m_clientAutopilotCourseErrorRad = 0.0;
        m_clientAutopilotHullErrorRad = 0.0;
        m_clientAutopilotTargetSpeedMps = 0.0;
        m_clientAutopilotTargetLongitudinalAccelerationMps2 = 0.0;
    };

    const auto releaseClientAutopilot = [&]()
    {
        m_clientAutopilotControlActive = false;
        m_lastPilotUniverseSeconds = -1.0;
        m_clientAutopilotControl = {};
        m_clientDockingSettledSinceServerSeconds = -1.0;
        m_clientDockingAssistedStabilizePilotController.reset();
        m_clientDockingNewtonianStabilizePilotController.reset();
        AssistedClientAutopilot::stop(m_clientAssistedRouteAutopilot);
        NewtonianClientAutopilot::stop(m_clientNewtonianRouteAutopilot);
    };

    const auto fail = [&](const std::string& reason)
    {
        // Fail is reached every frame while a pending request remains
        // visible. Report a transition once, not thousands of copies of
        // the same already-latched error.
        if (m_dockingGuidanceFailureReason != reason ||
            !m_noSafeDockingGuidanceSolution)
        {
            std::cerr
                << "[DockClient] request="
                << (pending.valid() ? pending.serial : 0)
                << " phase=failed reason=" << reason
                << std::endl;
        }
        m_dockingGuidanceFailureReason = reason;
        m_noSafeDockingGuidanceSolution = true;
        releaseClientAutopilot();
        m_clientDockingPhase = ClientDockingPhase::Idle;
        m_dockAdviceJob.reset();
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockSafeCourse = {};
    };

    if (!pending.valid())
    {
        if (m_clientDockingPhase != ClientDockingPhase::Idle ||
            m_clientAutopilotControlActive)
        {
            std::cerr
                << "[DockClient] phase=idle reason=no-pending-request"
                << std::endl;
        }
        releaseClientAutopilot();
        m_clientDockingPhase = ClientDockingPhase::Idle;
        m_lastDockingPathRequestSerial = 0;
        eraseVisibleRoute();
        return;
    }

    const bool automatic =
        pending.mode == DockingRouteRequest::Mode::Automatic;

    const auto playerIt =
        m_client->world().ships().find(m_playerId.value);
    if (playerIt == m_client->world().ships().end() ||
        !playerIt->second.descriptor)
    {
        fail("authoritative ship state unavailable");
        return;
    }

    const auto& player = playerIt->second;
    const auto& transform = player.transform;
    const auto& motion = transform.motion;

    if (motion.systemId != pending.target.systemId ||
        !motion.matchedToReferenceFrame ||
        motion.matchedReferenceFrameId.empty() ||
        motion.hubId.empty() ||
        motion.matchedReferenceFrameId != motion.hubId ||
        !player.referenceFrame.valid)
    {
        fail("ship is not matched to target Hub frame");
        return;
    }

    const auto* definition = m_hubSemanticAnchorCatalog.find(
        pending.target.stableObjectId,
        pending.target.semanticAnchorId
    );
    const auto* runtime = m_dockingPortRuntimeStateCatalog.find(
        pending.target.stableObjectId,
        pending.target.semanticAnchorId
    );
    if (!definition || !runtime ||
        !definition->enabled ||
        definition->kind != HubSemanticAnchorKind::DockingPort)
    {
        fail("dock state unavailable");
        return;
    }

    const auto& dim =
        player.descriptor->logicalDimensions();
    ShipDockingEnvelope hull;
    hull.valid =
        dim.enabled &&
        dim.length > 0 &&
        dim.width > 0 &&
        dim.height > 0;
    hull.lengthMeters = dim.length;
    hull.widthMeters = dim.width;
    hull.heightMeters = dim.height;

    const auto fit =
        evaluateDockingCompatibility(
            hull,
            *definition,
            *runtime
        );
    if (!fit.routeAvailable)
    {
        fail("dock unavailable or hull does not fit");
        return;
    }

    const auto plannerSlotState =
        player.physicalControlModel !=
                player.descriptor->physicalControlModel
            ? game::ship::plant::healthySlotState(
                  game::ship::plant::physicalControlSlots(
                      player.physicalControlModel
                  )
              )
            : game::ship::capability::buildPhysicalControlSlotState(
                  *player.descriptor,
                  player.physicalControlModel,
                  player.modules
              );

    const auto plannerCapability =
        game::ship::capability::buildShipCapability(
            *player.descriptor,
            player.physicalControlModel,
            plannerSlotState
        );

    ShipParams effectivePhysics =
        plannerCapability.valid
            ? plannerCapability.effectivePhysics
            : ShipParams {};

    // Both physical-law autopilots consume this one motherboard reader.
    // The separate Assisted path below requires an additional installed
    // signed motor certificate; Newtonian currently retains its legacy
    // projected limits until the multi-engine wrench provider is finished.
    ShipParams motherPhysics {};
    if (m_motherComputer.physicalFlightEnvelope(
            static_cast<std::uint64_t>(player.instanceId),
            m_client->lastSimulationMetadata().serverTick,
            motherPhysics
        ))
        effectivePhysics = motherPhysics;

    const auto controlLaw =
        motion.localControlLaw;

    // Newtonian: a failed attempt is not the end of the request. The route
    // is released, the craft coasts on a neutral command (no stale command
    // keeps burning) and the request is planned again from the actual
    // state; only after a bounded number of attempts is the failure final,
    // with its reason. Assisted: unchanged (fails as before).
    const auto recoverOrFail = [&](const std::string& reason)
    {
        constexpr int MaxNewtonianRouteRecoveries = 5;
        if (controlLaw != game::navigation::LocalFlightControlLaw::Newtonian ||
            !pending.valid() ||
            m_newtonianRouteRecoveries >= MaxNewtonianRouteRecoveries)
        {
            fail(reason);
            return;
        }
        // Attempts count at most every few seconds: a refusal that repeats
        // every frame (reactors restarting, low gas) keeps the craft safe
        // and searching instead of using the budget up at once.
        constexpr double RecoveryIntervalSeconds = 5.0;
        const double nowSeconds =
            m_client->lastSimulationMetadata().serverTimeSeconds;
        if (nowSeconds - m_newtonianLastRecoveryServerSeconds >=
                RecoveryIntervalSeconds)
        {
            m_newtonianLastRecoveryServerSeconds = nowSeconds;
            ++m_newtonianRouteRecoveries;
            std::cerr << "[DockClient] request=" << pending.serial
                << " phase=recover attempt=" << m_newtonianRouteRecoveries
                << "/" << MaxNewtonianRouteRecoveries
                << " reason=" << reason << std::endl;
        }
        releaseClientAutopilot();
        m_clientAutopilotControl = ShipControlState {};
        m_clientAutopilotControlActive = true;
        m_client->submitInput(m_clientAutopilotControl);
        m_dockAdviceJob.reset();
        m_dockLateEntry = {};
        m_dockSafeCourse = {};
        m_clientDockingPhase = ClientDockingPhase::Stabilizing;
        m_clientDockingSettledSinceServerSeconds = -1.0;
    };
    if (pending.valid() &&
        controlLaw == game::navigation::LocalFlightControlLaw::Newtonian)
    {
        // Newtonian plans and flies against the server-published installed
        // RCS authority, never the descriptor nominal. Wait for it exactly
        // like Assisted waits for its motor matrix.
        game::ship::telemetry::ShipControlAuthorityTelemetry rcsAuthority;
        if (!m_motherComputer.installedControlAuthority(
                static_cast<std::uint64_t>(player.instanceId),
                m_client->lastSimulationMetadata().serverTick,
                rcsAuthority))
        {
            if (m_clientDockingPhase == ClientDockingPhase::Executing)
            {
                // Propulsion unavailable during the route (reactors off or
                // restarting): no authority means no executable program.
                // Release the route, command a neutral ballistic coast (no
                // stale command may resume when the reactors return), wait
                // for the authority and replan from the actual state.
                std::cerr << "[DockClient] request=" << pending.serial
                    << " phase=execution-replan reason=authority-lost"
                    << " detail=\"authoritative RCS telemetry lost; coasting\""
                    << std::endl;
                releaseClientAutopilot();
                m_clientAutopilotControl = ShipControlState {};
                m_clientAutopilotControlActive = true;
                m_client->submitInput(m_clientAutopilotControl);
                m_clientDockingPhase = ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }
            const std::string reason =
                "awaiting authoritative RCS telemetry";
            if (m_dockingStatusWarning != reason)
            {
                std::cerr << "[DockAuthority] request=" << pending.serial
                    << " phase=preplanner-blocked reason=\"" << reason
                    << "\"" << std::endl;
                m_dockingStatusWarning = reason;
            }
            return;
        }
        // RCS gas: a route is not started from rest on a low tank; the
        // craft waits (no command, no draw) while the compressor refills.
        if (m_clientDockingPhase != ClientDockingPhase::Executing)
        {
            const auto systems = m_motherComputer.cockpitSnapshot(
                static_cast<std::uint64_t>(player.instanceId),
                m_client->lastSimulationMetadata().serverTick);
            const double speed =
                glm::length(motion.localVelocityMps);
            if (systems.valid && systems.systems.newtonianRcsFluidKnown &&
                speed < 1.0 &&
                systems.systems.newtonianRcsManifold01 <
                    game::navigation::autopilot::NewtonianFlightPolicy::
                        RouteStartRcsGasFill)
            {
                const std::string reason = "awaiting RCS gas pressure";
                if (m_dockingStatusWarning != reason)
                {
                    std::cerr << "[DockAuthority] request=" << pending.serial
                        << " phase=preplanner-blocked reason=\"" << reason
                        << "\" fill=" << systems.systems.newtonianRcsManifold01
                        << std::endl;
                    m_dockingStatusWarning = reason;
                }
                return;
            }
        }
        if (!m_dockingStatusWarning.empty())
        {
            m_dockingStatusWarning.clear();
            std::cerr << "[DockAuthority] request=" << pending.serial
                << " phase=capability-ready law=newtonian"
                << " angular_radps2=" << effectivePhysics.angularAccel
                << " rcs_mps2=" << effectivePhysics.manoeuvreThrusterAccel
                << std::endl;
        }
    }

    game::ship::telemetry::ShipControlAuthorityTelemetry
        liveAssistedAuthority;
    if (pending.valid() &&
        controlLaw == game::navigation::LocalFlightControlLaw::Assisted)
    {
        // This is authored by the actual server ShipCore motor allocator,
        // carried in every ship snapshot independent of sparse graph rows.
        // Never infer motor health from a missing optional module payload.
        ShipParams motherEnvelope {};
        const bool motherEnvelopeValid =
            m_motherComputer.assistedFlightEnvelope(
                static_cast<std::uint64_t>(player.instanceId),
                m_client->lastSimulationMetadata().serverTick,
                motherEnvelope,
                liveAssistedAuthority
            );
        if (!motherEnvelopeValid || !liveAssistedAuthority.valid ||
            liveAssistedAuthority.model != player.physicalControlModel)
        {
            // The ship may have only just entered HubTactical or changed
            // construction. Await the next authoritative sample instead of
            // failing a brand-new docking request forever.
            if (m_clientDockingPhase == ClientDockingPhase::Executing)
            {
                fail("authoritative motor telemetry lost during docking");
                return;
            }
            // Explain the PRE-PLANNER rejection using the already
            // observed motherboard ship/tick. No second capability
            // calculation and no fake engine state.
            const auto gate=m_motherComputer.snapshot();
            std::string reason="awaiting authoritative motor telemetry";
            double axisDot=-2.0;
            if(gate.valid &&
               gate.input.shipInstanceId==
                   static_cast<std::uint64_t>(player.instanceId) &&
               gate.input.serverTick==
                   m_client->lastSimulationMetadata().serverTick)
            {
                const auto& facts=gate.input;
                if(!facts.physicalEnvelopeValid)
                    reason="installed physics envelope unavailable";
                else if(!facts.installedMotorAuthority.valid ||
                        facts.installedMotorAuthority.model !=
                            player.physicalControlModel)
                    reason="installed motor matrix unavailable";
                else if(!gate.flightProfile.valid)
                {
                    const auto& body=facts.physicalEnvelope;
                    const double sl=glm::length(
                        facts.workingThrustAxisBody);
                    const double cl=glm::length(
                        body.assistedWorkingThrustAxisBody);
                    if(std::isfinite(sl) && std::isfinite(cl) &&
                       sl>1.0e-9 && cl>1.0e-9)
                        axisDot=glm::dot(
                            facts.workingThrustAxisBody/sl,
                            body.assistedWorkingThrustAxisBody/cl);
                    if(facts.emergencyWorkingThrustAxis !=
                           body.assistedWorkingThrustAxisEmergency ||
                       (axisDot>=-1.0 && axisDot<0.999))
                        reason="working thrust axis mismatch";
                    else
                        reason="physical flight doctrine rejected";
                }
            }
            if(m_dockingStatusWarning != reason)
            {
                std::cerr << "[DockAuthority] request=" << pending.serial
                    << " phase=preplanner-blocked"
                    << " reason=\"" << reason << "\""
                    << " server_axis="
                    << (gate.valid && gate.input.emergencyWorkingThrustAxis?1:0)
                    << " client_axis="
                    << (gate.valid && gate.input.physicalEnvelope.
                        assistedWorkingThrustAxisEmergency?1:0)
                    << " axis_dot=" << axisDot
                    << " motor_valid=" << liveAssistedAuthority.valid
                    << " model=" << static_cast<int>(
                        liveAssistedAuthority.model)
                    << " expected=" << static_cast<int>(
                        player.physicalControlModel)
                    << std::endl;
                m_dockingStatusWarning=reason;
            }
            return;
        }
        if (!m_dockingStatusWarning.empty())
        {
            m_dockingStatusWarning.clear();
            std::cerr
                << "[DockAuthority] request=" << pending.serial
                << " phase=capability-ready"
                << " actuator_rev="
                << liveAssistedAuthority.sourceActuatorRevision
                << std::endl;
        }
        // Read the motor-constrained envelope via the ship computer's
        // synchronous flight lane, never an asynchronously stale diagnosis.
        effectivePhysics = motherEnvelope;

        const double steering = std::min({
            liveAssistedAuthority.positive[3],
            liveAssistedAuthority.negative[3],
            liveAssistedAuthority.positive[4],
            liveAssistedAuthority.negative[4]
        });
        const double lateral = std::min({
            liveAssistedAuthority.positive[0],
            liveAssistedAuthority.negative[0],
            liveAssistedAuthority.positive[1],
            liveAssistedAuthority.negative[1]
        });
        // The server actuator matrix now publishes a working-forward
        // acceleration for EVERY Assisted installation: the centered -Z
        // basis when healthy, or the attainable CoM-aligned off-axis ray
        // when damaged. Pure -Z is retained only for diagnostics.
        const double forward =
            liveAssistedAuthority.workingForwardAccelerationMps2;
        const double reverse =
            liveAssistedAuthority.positive[2];
        if (steering <= 1.0e-6 || lateral <= 1.0e-6 ||
            forward <= 1.0e-6 || reverse <= 1.0e-6)
        {
            // This is a pre-planner feasibility rejection. Record the actual
            // signed basis that failed, never let a generic "no route" hide it.
            std::ostringstream reason;
            reason << "installed Assisted motor basis insufficient:"
                   << " steering=" << steering
                   << " lateral=" << lateral
                   << " working_forward=" << forward
                   << " hull_forward=" << liveAssistedAuthority.negative[2]
                   << " reverse=" << reverse
                   << " axis_emergency="
                   << (effectivePhysics.assistedWorkingThrustAxisEmergency?1:0);
            fail(reason.str());
            return;
        }
    }

    const auto frame =
        player.referenceFrame.kinematicFrame();

    const auto makeAgent = [&]() -> FollowerAgent
    {
        FollowerAgent agent;
        agent.positionMapMeters =
            motion.localPositionMeters;
        agent.velocityMapMetersPerSecond =
            motion.localVelocityMps;
        agent.forwardMap = glm::normalize(
            frame.worldToLocalVector(
                glm::dvec3(transform.forward())
            )
        );
        agent.rightMap = glm::normalize(
            frame.worldToLocalVector(
                glm::dvec3(transform.right())
            )
        );
        agent.upMap = glm::normalize(
            frame.worldToLocalVector(
                glm::dvec3(transform.up())
            )
        );
        agent.pitchRateRadPerSec = transform.pitchRate;
        agent.yawRateRadPerSec = transform.yawRate;
        agent.rollRateRadPerSec = transform.rollRate;
        return agent;
    };

    const auto submitAutopilotControl =
        [&](const ShipControlState& control)
        {
            m_clientAutopilotControl = control;
            m_clientAutopilotControlActive = true;

            // This is deliberately the same input path used by the keyboard.
            // Server physics does not know whether the sample came from a
            // human or from the client virtual pilot.
            m_client->submitInput(m_clientAutopilotControl);
        };

    const auto coastForPlanningControl = [&]()
    {
        // NEUTRAL PHYSICAL CONTROL, not a direct-navigation bypass.
        //
        // Newtonian: zero ordinary controls naturally means ballistic coast.
        // Assisted: zero ordinary controls MUST leave the normal FA-on law
        // alive.  It may remove residual lateral slip relative to the hull;
        // bypassing that with navigationAccelerationDemandValid=0 m/s^2 makes
        // Assisted behave Newtonian and visibly separates nose from velocity.
        return ShipControlState {};
    };

    const auto stabilizeAngularForPlanningControl = [&]()
    {
        const auto agent = makeAgent();

        game::navigation::autopilot::AutopilotControlRequest hold;

        // Preserve the measured translational state exactly: no linear
        // acceleration is requested while the old route is being released.
        hold.desiredVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        hold.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);

        // Freeze the CURRENT hull pose while HullAttitudeControl damps
        // pitch/yaw/roll rates to zero. terminalAttitudeHold selects explicit
        // hull-pose tracking instead of velocity-course tracking; the target
        // pose equals the current pose, so only angular-rate removal remains.
        hold.desiredForwardMap = agent.forwardMap;
        hold.desiredUpMap = agent.upMap;
        hold.terminalAttitudeHold = true;
        hold.desiredCourseAngularRateMapRadPerSec = glm::dvec3(0.0);
        hold.desiredRollRateRadPerSec = 0.0;

        hold.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        hold.forwardMap = agent.forwardMap;
        hold.rightMap = agent.rightMap;
        hold.upMap = agent.upMap;
        hold.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        hold.yawRateRadPerSec = agent.yawRateRadPerSec;
        hold.rollRateRadPerSec = agent.rollRateRadPerSec;
        hold.stopRequested = false;
        hold.deltaSeconds = std::max(
            1.0e-4,
            m_client->serverFixedStepSeconds()
        );

        if (controlLaw !=
                game::navigation::LocalFlightControlLaw::Assisted)
        {
            // Zero-rate hold with target pose == current pose: release the
            // rotational controls. The server's neutral angular damping then
            // removes rotation from its fresh authoritative rate with the
            // real installed authority; a client loop on replicated, delayed
            // rates limit-cycles at high RCS authority.
            (void)hold;
            return coastForPlanningControl();
        }
        return m_clientDockingAssistedStabilizePilotController.update(
            hold,
            effectivePhysics
        );
    };

    const auto manualStopControl = [&]()
    {
        const auto agent = makeAgent();

        game::navigation::autopilot::AutopilotControlRequest stop;
        stop.desiredVelocityMapMps = glm::dvec3(0.0);
        stop.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
        stop.desiredForwardMap = agent.forwardMap;
        stop.desiredUpMap = agent.upMap;
        stop.actualVelocityMapMps =
            agent.velocityMapMetersPerSecond;
        stop.forwardMap = agent.forwardMap;
        stop.rightMap = agent.rightMap;
        stop.upMap = agent.upMap;
        stop.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        stop.yawRateRadPerSec = agent.yawRateRadPerSec;
        stop.rollRateRadPerSec = agent.rollRateRadPerSec;
        stop.stopRequested = true;
        stop.deltaSeconds = std::max(
            1.0e-4,
            m_client->serverFixedStepSeconds()
        );

        return controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted
            ? m_clientDockingAssistedStabilizePilotController.update(
                  stop,
                  effectivePhysics
              )
            : m_clientDockingNewtonianStabilizePilotController.update(
                  stop,
                  effectivePhysics
              );
    };

    const auto legacyNewtonianSafeCourseControl =
        [&](const glm::dvec3& safeDirection)
        {
            const auto agent = makeAgent();
            const double actualSpeed =
                glm::length(agent.velocityMapMetersPerSecond);
            const glm::dvec3 desiredCourse =
                glm::length(safeDirection) > 1.0e-9
                    ? glm::normalize(safeDirection)
                    : agent.forwardMap;
            const double targetSpeed =
                m_dockSafeCourse.targetSpeedMps > 0.5
                    ? m_dockSafeCourse.targetSpeedMps
                    : actualSpeed;

            game::navigation::autopilot::AutopilotControlRequest turn;

            // Legacy Newtonian bridge only. Assisted recovery owns a separate
            // committed-trajectory API and must never enter this mutable
            // direction controller.
            const glm::dvec3 targetVelocity =
                desiredCourse * targetSpeed;
            const glm::dvec3 deltaVelocity =
                targetVelocity -
                agent.velocityMapMetersPerSecond;
            const double deltaSpeed =
                glm::length(deltaVelocity);

            turn.desiredVelocityMapMps = targetVelocity;
            turn.desiredForwardMap = desiredCourse;

            const double requestedAcceleration =
                std::max(
                    0.0,
                    m_dockSafeCourse.courseAccelerationMps2
                );
            turn.desiredLinearAccelerationMapMps2 =
                deltaSpeed > 1.0e-6 &&
                requestedAcceleration > 1.0e-9
                    ? deltaVelocity / deltaSpeed *
                        requestedAcceleration
                    : glm::dvec3(0.0);

            turn.desiredUpMap = agent.upMap;
            turn.actualVelocityMapMps =
                agent.velocityMapMetersPerSecond;
            turn.forwardMap = agent.forwardMap;
            turn.rightMap = agent.rightMap;
            turn.upMap = agent.upMap;
            turn.pitchRateRadPerSec = agent.pitchRateRadPerSec;
            turn.yawRateRadPerSec = agent.yawRateRadPerSec;
            turn.rollRateRadPerSec = agent.rollRateRadPerSec;
            turn.stopRequested = false;
            // With no velocity change to make (e.g. starting at rest), the
            // safe-course task is pure orientation: hold the selected course
            // as the hull pose. Thrust-vector attitude only applies while a
            // real translational demand exists.
            turn.terminalAttitudeHold = deltaSpeed <= 0.5;
            turn.deltaSeconds = std::max(
                1.0e-4,
                m_client->serverFixedStepSeconds()
            );

            return m_clientDockingNewtonianStabilizePilotController.update(
                turn,
                effectivePhysics
            );
        };

    // Automatic replanning has no wall-clock start deadline. Assisted either
    // follows one committed, collision-proven recovery maneuver or coasts
    // ballistically while the asynchronous route is built. Newtonian keeps
    // its legacy safety bridge until its separate recovery executor exists.
    // Route handoff remains spatially validated.

    if (pending.serial != m_lastDockingPathRequestSerial)
    {
        eraseVisibleRoute();
        releaseClientAutopilot();

        m_lastDockingPathRequestSerial = pending.serial;
        m_newtonianRouteRecoveries = 0;
        m_newtonianLastRecoveryServerSeconds = -1.0e9;
        m_lastDockingRequestTraceSerial = pending.serial;
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockSafeCourse = {};
        m_dockPlannedAssistedAuthority = {};
        m_dockAcceptedAssistedAuthority = {};
        m_clientDockingPhase =
            ClientDockingPhase::Stabilizing;
        m_noSafeDockingGuidanceSolution = false;
        m_dockingGuidanceFailureReason.clear();
        m_dockingStatusWarning.clear();

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase=stabilizing"
            << " mode=" << (automatic ? "automatic" : "guidance")
            << " system=" << pending.target.systemId
            << " module=" << pending.target.stableObjectId
            << " anchor=" << pending.target.semanticAnchorId
            << " execution=client-input"
            << std::endl;
    }

    const double fixedDt =
        std::max(1.0e-4, m_client->serverFixedStepSeconds());
    const double authoritativeServerSeconds =
        m_client->lastSimulationMetadata().serverTimeSeconds;
    const double universeTimeSeconds =
        m_client->lastSimulationMetadata().universeTimeSeconds;

    if (m_clientDockingPhase ==
        ClientDockingPhase::SecuringCourse)
    {
        if (automatic &&
            controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted &&
            m_dockAssistedSafeRecovery.active &&
            m_dockAssistedSafeRecovery.serial == pending.serial)
        {
            const auto agent = makeAgent();
            const double speedMps =
                glm::length(agent.velocityMapMetersPerSecond);
            const double angularSpeedRadPerSec =
                std::sqrt(
                    agent.pitchRateRadPerSec *
                        agent.pitchRateRadPerSec +
                    agent.yawRateRadPerSec *
                        agent.yawRateRadPerSec +
                    agent.rollRateRadPerSec *
                        agent.rollRateRadPerSec
                );

            if (m_dockAssistedSafeRecovery.stopFallback)
            {
                submitAutopilotControl(manualStopControl());

                if (speedMps <= 0.05 &&
                    angularSpeedRadPerSec <= 0.01)
                {
                    std::cerr
                        << "[AssistedSafeCourse]"
                        << " request=" << pending.serial
                        << " phase=stopped"
                        << " fallback=1"
                        << std::endl;

                    m_dockAssistedSafeRecovery = {};
                    m_clientDockingPhase =
                        ClientDockingPhase::Stabilizing;
                    m_clientDockingSettledSinceServerSeconds =
                        -1.0;
                }
                return;
            }

            game::navigation::assisted::
                AssistedSafeCourseExecutor::Input safeInput;
            safeInput.actualVelocityMapMps =
                agent.velocityMapMetersPerSecond;
            safeInput.selectedCourseMap =
                m_dockAssistedSafeRecovery.direction;
            safeInput.forwardMap = agent.forwardMap;
            safeInput.rightMap = agent.rightMap;
            safeInput.upMap = agent.upMap;
            // The safety-course controller uses the SAME physical working
            // direction as the installed Assisted flight doctrine. The
            // nose is merely the healthy equipment's default axis.
            safeInput.workingThrustAxisBody =
                effectivePhysics.assistedWorkingThrustAxisBody;
            safeInput.pitchRateRadPerSec =
                agent.pitchRateRadPerSec;
            safeInput.yawRateRadPerSec =
                agent.yawRateRadPerSec;
            safeInput.rollRateRadPerSec =
                agent.rollRateRadPerSec;
            safeInput.selectedSpeedMps =
                m_dockAssistedSafeRecovery.targetSpeedMps;
            safeInput.maxPitchRateRadPerSec =
                effectivePhysics.maxPitchRate;
            safeInput.maxYawRateRadPerSec =
                effectivePhysics.maxYawRate;
            safeInput.maxRollRateRadPerSec =
                effectivePhysics.maxRollRate;
            safeInput.angularAccelerationAuthorityRadPerSec2 =
                game::ship::angularAccelerationLimitRadPerSec2(
                    effectivePhysics
                );
            safeInput.deltaSeconds = fixedDt;

            const auto safeOutput =
                game::navigation::assisted::
                    AssistedSafeCourseExecutor::evaluate(
                        safeInput
                    );

            if (!safeOutput.valid)
            {
                fail("Assisted safe-course executor input invalid");
                return;
            }

            if (safeOutput.acquired)
            {
                // ============================================================
                // ASSISTED DOCKING ENTRY CONTRACT -- DO NOT REFACTOR AWAY.
                //
                // Once the ONE selected collision-free velocity course has
                // been acquired, safe-course work is finished.  Remove linear
                // acceleration and coast.  Do NOT:
                //   - track a time-indexed recovery arc;
                //   - compare position with an imaginary circle;
                //   - select another course because of timing/position error;
                //   - cancel/restart the expensive route planner.
                //
                // The planner is allowed to finish from its stale start.  A
                // single projected entry bind below joins the live ballistic
                // craft to the immutable downstream route.
                // ============================================================
                submitAutopilotControl(coastForPlanningControl());

                const bool plannerAlreadyLaunched =
                    m_dockAdviceJob &&
                    m_dockAdviceJob->context.serial ==
                        pending.serial;

                std::cerr
                    << "[AssistedSafeCourse]"
                    << " request=" << pending.serial
                    << " phase=acquired"
                    << " speed_mps=" << speedMps
                    << " planner_overlap="
                    << (plannerAlreadyLaunched ? 1 : 0)
                    << std::endl;

                m_dockAssistedSafeRecovery = {};
                m_clientDockingPhase =
                    plannerAlreadyLaunched
                        ? ClientDockingPhase::Planning
                        : ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            // The selected direction is immutable for this docking-entry
            // correction.  No 0.25 s reassessment/reselection loop exists.
            // Safe-course already emits ordinary Assisted controls.  Do not
            // route it through AssistedAutopilot/VelocityCourseGuidance: that
            // would create a second course controller on top of physical FA-on.
            submitAutopilotControl(safeOutput.control);
            return;
        }

        if (!automatic ||
            !m_dockSafeCourse.active ||
            m_dockSafeCourse.serial != pending.serial)
        {
            m_dockSafeCourse = {};
            m_clientDockingPhase =
                ClientDockingPhase::Stabilizing;
            m_clientDockingSettledSinceServerSeconds = -1.0;
            return;
        }

        const auto agent = makeAgent();
        const double speedMps =
            glm::length(agent.velocityMapMetersPerSecond);
        const double angularSpeedRadPerSec =
            std::sqrt(
                agent.pitchRateRadPerSec * agent.pitchRateRadPerSec +
                agent.yawRateRadPerSec * agent.yawRateRadPerSec +
                agent.rollRateRadPerSec * agent.rollRateRadPerSec
            );

        if (m_dockSafeCourse.stopFallback)
        {
            submitAutopilotControl(manualStopControl());

            if (speedMps <= 0.05 &&
                angularSpeedRadPerSec <= 0.01)
            {
                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=stopped"
                    << " fallback=1"
                    << std::endl;

                m_dockSafeCourse = {};
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
            }
            return;
        }

        const bool shouldRecheck =
            m_dockSafeCourse.lastRecheckServerSeconds < 0.0 ||
            authoritativeServerSeconds -
                m_dockSafeCourse.lastRecheckServerSeconds >= 0.25;

        if (shouldRecheck)
        {
            game::navigation::PreplanningSafetyCourseRequest safety;
            safety.positionMeters = agent.positionMapMeters;
            safety.velocityMps =
                agent.velocityMapMetersPerSecond;
            safety.hullForward = agent.forwardMap;
            safety.preferredDirection =
                m_dockSafeCourse.preferredDirection;
            safety.hullRadiusMeters =
                m_dockSafeCourse.hullRadiusMeters;
            safety.additionalClearanceMeters =
                m_dockSafeCourse.additionalClearanceMeters;
            safety.courseAccelerationMps2 =
                m_dockSafeCourse.courseAccelerationMps2;
            safety.brakingAccelerationMps2 =
                m_dockSafeCourse.brakingAccelerationMps2;
            safety.maxAngularVelocityRadPerSecond =
                m_dockSafeCourse.maxAngularVelocityRadPerSecond;
            safety.newtonian =
                m_dockSafeCourse.newtonian;
            safety.obstacles =
                m_dockSafeCourse.obstacles;

            const auto check =
                game::navigation::PreplanningSafetyCourse::
                    evaluate(safety);

            m_dockSafeCourse.lastRecheckServerSeconds =
                authoritativeServerSeconds;

            constexpr double SafeCourseAlignmentCosine =
                0.9961946980917455; // cos(5 deg)

            const glm::dvec3 actualCourse =
                speedMps > 0.5
                    ? glm::normalize(
                          agent.velocityMapMetersPerSecond
                      )
                    : glm::normalize(agent.forwardMap);

            const glm::dvec3 selectedCourse =
                glm::normalize(m_dockSafeCourse.direction);

            const bool selectedCourseAcquired =
                glm::dot(actualCourse, selectedCourse) >=
                    SafeCourseAlignmentCosine;

            if (check.currentCourseSafe &&
                selectedCourseAcquired)
            {
                // The physical course is now collision-proven. A speculative
                // planner worker may already be running (or even ready), so do
                // not throw away the overlap by returning to a fresh planning
                // start. Planning phase will finish angular damping before any
                // route handoff is accepted.
                const bool plannerAlreadyLaunched =
                    m_dockAdviceJob &&
                    m_dockAdviceJob->context.serial == pending.serial;

                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );

                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=acquired"
                    << " speed_mps=" << speedMps
                    << " ray_m=" << check.sceneRayMeters
                    << " planner_overlap="
                    << (plannerAlreadyLaunched ? 1 : 0)
                    << std::endl;

                m_dockSafeCourse = {};
                m_clientDockingPhase =
                    plannerAlreadyLaunched
                        ? ClientDockingPhase::Planning
                        : ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            if (!check.currentCourseSafe)
            {
                if (check.safeCourseFound)
                {
                    // The previous escape direction is no longer proven from
                    // the live position. Replace it with the newly proven one.
                    // Any speculative route already running remains safe to
                    // keep as a backbone candidate; spatial handoff will force
                    // a late-entry rebind if this direction change matters.
                    m_dockSafeCourse.direction =
                        check.safeCourse;
                    m_dockSafeCourse.sceneRayMeters =
                        check.sceneRayMeters;
                    m_dockSafeCourse.selectedTurnAngleRad =
                        check.turnAngleRad;
                    m_dockSafeCourse.selectedTurnRadiusMeters =
                        check.turnRadiusMeters;
                }
                else
                {
                    // No proven escape turn. Do not keep coasting into the
                    // blocker and do not hand this state back to the route
                    // planner. Fall back to the ordinary physical STOP.
                    m_dockSafeCourse.stopFallback = true;

                    if (m_dockAdviceJob)
                    {
                        m_dockAdviceJob->cancelled.store(
                            true,
                            std::memory_order_release
                        );
                    }

                    std::cerr
                        << "[DockSafetyCourse]"
                        << " request=" << pending.serial
                        << " phase=emergency-stop"
                        << " stop_corridor_clear="
                        << (check.stopFallbackClear ? 1 : 0)
                        << " reason=\"" << check.message << "\""
                        << std::endl;

                    submitAutopilotControl(manualStopControl());
                    return;
                }
            }
        }

        submitAutopilotControl(
            legacyNewtonianSafeCourseControl(
                m_dockSafeCourse.direction
            )
        );
        return;
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::Stabilizing)
    {
        const double relativeSpeedMps =
            glm::length(motion.localVelocityMps);
        const double angularSpeedRadPerSec =
            std::sqrt(
                transform.pitchRate * transform.pitchRate +
                transform.yawRate * transform.yawRate +
                transform.rollRate * transform.rollRate
            );

        // MANUAL GUIDANCE CONTRACT:
        // Manual and Automatic consume the SAME route topology/geometry.
        // Manual differs only by reaching a real zero translational/angular
        // state before route calculation.
        if (!automatic)
        {
            constexpr double ManualStoppedSpeedMps = 0.05;
            constexpr double ManualStoppedAngularRadPerSec = 0.01;
            constexpr double ManualSettledSeconds = 0.50;

            submitAutopilotControl(manualStopControl());

            const bool stopped =
                relativeSpeedMps <= ManualStoppedSpeedMps &&
                angularSpeedRadPerSec <=
                    ManualStoppedAngularRadPerSec;

            if (!stopped)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                return;
            }

            if (authoritativeServerSeconds -
                    m_clientDockingSettledSinceServerSeconds <
                ManualSettledSeconds)
            {
                return;
            }
        }
        else
        {
            // ================================================================
            // AUTOMATIC SETTLE-BEFORE-SNAPSHOT CONTRACT -- DO NOT WEAKEN.
            //
            // A new docking request first removes translational acceleration
            // and kills hull angular velocity.  Linear VELOCITY is preserved.
            //
            // The planner/compiler MUST NOT see a snapshot captured while the
            // hull is still rotating.  Waiting only at route adoption is too
            // late: the worker compiles the initial angular trajectory from
            // the snapshot and can permanently reject an otherwise valid
            // route as initial-omega-outside-capability.
            //
            // Required order:
            //   1. zero requested linear acceleration;
            //   2. damp pitch/yaw/roll rates;
            //   3. hold the settled angular state briefly;
            //   4. only then capture the authoritative planning snapshot;
            //   5. after launch, expensive planning may run asynchronously
            //      while the craft continues ballistic translation.
            // ================================================================
            constexpr double AutomaticSettledAngularRadPerSec = 0.01;
            constexpr double AutomaticAngularSettledSeconds = 0.20;

            submitAutopilotControl(
                stabilizeAngularForPlanningControl()
            );

            const bool angularStopped =
                angularSpeedRadPerSec <=
                    AutomaticSettledAngularRadPerSec;

            if (!angularStopped)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                return;
            }

            if (authoritativeServerSeconds -
                    m_clientDockingSettledSinceServerSeconds <
                AutomaticAngularSettledSeconds)
            {
                return;
            }

            // Snapshot below now observes the contract state: no requested
            // linear acceleration and effectively zero hull angular velocity.
            submitAutopilotControl(coastForPlanningControl());
        }

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase="
            << (automatic ? "settled-planning" : "stopped-planning")
            << " vrel_mps=" << relativeSpeedMps
            << " omega_radps=" << angularSpeedRadPerSec
            << std::endl;

        const auto snapshot =
            game::client::ClientNavigationPlanningSnapshotFactory::
                buildAuthoritativeHubSnapshot(
                    m_client->world(),
                    m_client->lastSimulationMetadata(),
                    m_playerId,
                    pending.target.systemId,
                    pending.target.stableObjectId
                );
        if (!snapshot.ready())
        {
            fail("authoritative planning snapshot unavailable");
            return;
        }

        const auto localPort =
            resolveDockingAdvisoryLocalPortAt(
                snapshot.targetObject.hubAttachment,
                *definition,
                snapshot.epoch.universeTimeSeconds
            );
        if (!localPort.valid)
        {
            fail("dock local pose unavailable at planning epoch");
            return;
        }

        VehicleGuidanceEnvelope envelope;
        envelope.lengthMeters = hull.lengthMeters;
        envelope.widthMeters = hull.widthMeters;
        envelope.heightMeters = hull.heightMeters;
        envelope.valid = true;

        const auto shipProfile =
            makeNavigationVehicleProfile(
                effectivePhysics,
                envelope,
                controlLaw
            );
        const bool assisted =
            controlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted;

        // Explicit experimental switch. Normal launches remain byte-for-byte
        // Standard; Newtonian is not affected. Run a second identical scene
        // with ELITE_ASSISTED_ROUTE_PRESET=extreme to compare tunnel geometry.
        const char* assistedPresetEnv =
            std::getenv("ELITE_ASSISTED_ROUTE_PRESET");
        const bool lateEntryRebind =
            automatic &&
            m_dockLateEntry.active &&
            m_dockLateEntry.serial == pending.serial;

        game::navigation::planner::RoutePlanRequest request;
        request.assistedRoutePreset =
            assisted && assistedPresetEnv &&
            (std::string(assistedPresetEnv) == "extreme" ||
             std::string(assistedPresetEnv) == "EXTREME")
                ? game::navigation::planner::AssistedRoutePreset::Extreme
                : game::navigation::planner::AssistedRoutePreset::Standard;

        const glm::dvec3 startVelocityMps =
            automatic
                ? snapshot.controlledShip.localVelocityMps
                : glm::dvec3(0.0);
        request.startMeters =
            snapshot.controlledShip.localPositionMeters;

        if (lateEntryRebind)
        {
            // One-shot projected entry.  The downstream route is immutable;
            // only its short live connector is rebuilt.  Project the current
            // ballistic craft to the handoff time selected from the previous
            // route's first-turn travel time.
            request.startMeters +=
                startVelocityMps * m_dockLateEntry.leadSeconds;
        }

        request.hasInitialForward = true;
        const glm::dvec3 hullForwardLocal =
            glm::normalize(
                snapshot.planningFrame.worldToLocalVector(
                    glm::dvec3(transform.forward())
                )
            );
        request.initialForward =
            game::navigation::DockingRouteEntryPolicy::initialCourse(
                startVelocityMps,
                hullForwardLocal
            );
        request.initialForwardLeadMeters =
            std::max(1000.0, hull.lengthMeters * 10.0);
        request.terminalReferenceDistanceMeters =
            std::max(
                300.0,
                hull.lengthMeters * 10.0 +
                    definition->requiredClearanceMeters * 4.0
            );
        request.goalMeters =
            localPort.positionMeters +
            localPort.forward *
                request.terminalReferenceDistanceMeters;
        request.terminalOutward = localPort.forward;
        request.agentRadiusMeters =
            envelope.conservativeSafetyRadiusMeters();
        request.vehicleLengthMeters = hull.lengthMeters;
        request.vehicleWidthMeters = hull.widthMeters;
        request.vehicleHeightMeters = hull.heightMeters;

        // Capability numbers of the request: one implementation shared with
        // the offline tools (execution reserve, planning shares, laws). A
        // missing authority is zero, never a small invented value: without
        // it the route is physically impossible and is not planned.
        const auto requestFacts =
            game::navigation::makeNavigationControlCapability(
                effectivePhysics, controlLaw);
        const auto requestCapability =
            game::navigation::applyRouteRequestCapability(
                request,
                shipProfile,
                controlLaw,
                &requestFacts
            );
        if (!requestCapability.ok)
        {
            recoverOrFail("PhysicallyImpossible: " + requestCapability.reason);
            return;
        }
        const double executionCurveLateralMps2 =
            requestCapability.executionCurveLateralMps2;
        request.initialSpeedMps = glm::length(startVelocityMps);

        request.assistedCompactStartLead = assisted;
        request.initialForwardLeadMeters =
            assisted
                ? game::navigation::OrdinaryRoutePolicy::
                    requiredAssistedStartLeadMeters(
                        request.initialSpeedMps,
                        request.maxSpeedMps,
                        request.brakingMps2,
                        request.lateralMps2,
                        request.maxAngularVelocityRadPerSecond,
                        hull.lengthMeters
                    )
                : game::navigation::OrdinaryRoutePolicy::
                requiredStartLeadMeters(
                    request.initialSpeedMps,
                    request.maxSpeedMps,
                    request.brakingMps2,
                    request.lateralMps2,
                    request.maxAngularVelocityRadPerSecond,
                    hull.lengthMeters
                );

        std::cout
            << "[DockEntryPolicy]"
            << " request=" << pending.serial
            << " initial_speed_mps=" << request.initialSpeedMps
            << " route_max_mps=" << request.maxSpeedMps
            << " forward_lead_m="
            << request.initialForwardLeadMeters
            << " geometry_lat_mps2="
            << request.lateralMps2
            << " execution_curve_lat_mps2="
            << executionCurveLateralMps2
            << " initial_course=("
            << request.initialForward.x << ","
            << request.initialForward.y << ","
            << request.initialForward.z << ")"
            << std::endl;

        // USER CONTRACT: visual tunnel cadence.
        request.gateSpacingMeters = 500.0;
        request.terminalGateSpacingMeters = 250.0;

        if (assisted)
        {
            // Ordinary terminal geometry is sized from the speed that may
            // arrive at the final approach and the actual terminal crossing
            // limit. Speed yields to geometry; no fixed 9 km approach.
            request.terminalApproachLengthMeters =
                game::navigation::OrdinaryRoutePolicy::
                    requiredTerminalApproachMeters(
                        request.maxSpeedMps,
                        definition->maxEntrySpeedMps,
                        request.brakingMps2,
                        request.terminalReferenceDistanceMeters,
                        hull.lengthMeters
                    );
            request.terminalTurnSegmentFraction = 0.85;
            request.deriveTerminalTurnRadiusFromVehicle = true;
        }

        request.obstacles = snapshot.navigationObstacles;

        bool planningWhileSecuringCourse = false;

        if (automatic && assisted && lateEntryRebind)
        {
            // ENTRY-REBIND IS NOT A NEW DOCKING REQUEST.
            //
            // The primary Assisted safety course has already been selected
            // and physically acquired. Re-entering PreplanningSafetyCourse
            // here used to let a rotating station trigger a second unrelated
            // course correction while the connector was being rebound,
            // invalidating the very handoff we were trying to repair.
            //
            // Keep the already accepted inertial state and rebuild only the
            // connector to the immutable backbone.
            submitAutopilotControl(coastForPlanningControl());
        }
        else if (automatic)
        {
            const glm::dvec3 preferredSafetyDirection =
                glm::length(request.goalMeters - request.startMeters) >
                        1.0e-9
                    ? glm::normalize(
                          request.goalMeters -
                          request.startMeters
                      )
                    : request.initialForward;

            if (assisted)
            {
                // ============================================================
                // RESTORED ASSISTED DOCKING-ENTRY CONTRACT.
                //
                // 1) linear acceleration is released;
                // 2) inspect the CURRENT ballistic velocity corridor;
                // 3) only if blocked, choose ONE collision-free course;
                // 4) turn the velocity vector to that course once;
                // 5) release acceleration and coast.
                //
                // The expensive route planner starts in parallel and is NEVER
                // restarted merely because the craft moved while it worked.
                // Its stale entry is repaired later by ONE projected bind.
                // ============================================================
                game::navigation::PreplanningSafetyCourseRequest safety;
                safety.positionMeters = request.startMeters;
                safety.velocityMps = startVelocityMps;
                safety.hullForward = hullForwardLocal;
                safety.preferredDirection =
                    preferredSafetyDirection;
                safety.hullRadiusMeters =
                    request.agentRadiusMeters;
                safety.additionalClearanceMeters =
                    std::max(
                        25.0,
                        definition->requiredClearanceMeters
                    );
                safety.courseSafetySpotRadiusMeters =
                    std::clamp(
                        request.agentRadiusMeters * 0.35 +
                            safety.additionalClearanceMeters * 0.25,
                        15.0,
                        50.0
                    );
                safety.courseAccelerationMps2 =
                    request.lateralMps2;
                safety.brakingAccelerationMps2 =
                    request.brakingMps2;
                safety.maxAngularVelocityRadPerSecond =
                    request.maxAngularVelocityRadPerSecond;
                safety.newtonian = false;
                safety.obstacles = request.obstacles;

                const auto safetyResult =
                    game::navigation::PreplanningSafetyCourse::
                        evaluate(safety);

                std::cerr
                    << "[AssistedSafeCourse]"
                    << " request=" << pending.serial
                    << " phase=check"
                    << " speed_mps=" << safetyResult.speedMps
                    << " current_safe="
                    << (safetyResult.currentCourseSafe ? 1 : 0)
                    << " escape_found="
                    << (safetyResult.safeCourseFound ? 1 : 0)
                    << " stop_clear="
                    << (safetyResult.stopFallbackClear ? 1 : 0)
                    << " turn_deg="
                    << safetyResult.turnAngleRad *
                        57.2957795130823208768
                    << " ray_m=" << safetyResult.sceneRayMeters
                    << " reason=\"" << safetyResult.message << "\""
                    << std::endl;

                if (!safetyResult.currentCourseSafe)
                {
                    m_dockAssistedSafeRecovery = {};
                    m_dockAssistedSafeRecovery.active = true;
                    m_dockAssistedSafeRecovery.serial =
                        pending.serial;
                    m_dockAssistedSafeRecovery.stopFallback =
                        !safetyResult.safeCourseFound;
                    m_dockAssistedSafeRecovery.direction =
                        safetyResult.safeCourseFound
                            ? safetyResult.safeCourse
                            : request.initialForward;
                    m_dockAssistedSafeRecovery.targetSpeedMps =
                        safetyResult.speedMps;
                    m_dockAssistedSafeRecovery.sceneRayMeters =
                        safetyResult.sceneRayMeters;

                    m_clientDockingPhase =
                        ClientDockingPhase::SecuringCourse;
                    m_clientDockingSettledSinceServerSeconds =
                        -1.0;

                    if (m_dockAssistedSafeRecovery.stopFallback)
                    {
                        std::cerr
                            << "[AssistedSafeCourse]"
                            << " request=" << pending.serial
                            << " phase=emergency-stop"
                            << std::endl;
                        submitAutopilotControl(manualStopControl());
                        return;
                    }

                    planningWhileSecuringCourse = true;

                    // The route must begin along the escape course that the
                    // real craft is now acquiring, NOT the unsafe velocity
                    // or hull ray captured before obstacle avoidance.
                    // Keep the measured initial speed as a magnitude; the
                    // existing late-entry connector will bind the finished
                    // backbone to the eventual live position/velocity.
                    request.initialForward =
                        m_dockAssistedSafeRecovery.direction;

                    std::cerr
                        << "[AssistedSafeCourse]"
                        << " request=" << pending.serial
                        << " phase=selected+planning"
                        << " course=("
                        << m_dockAssistedSafeRecovery.direction.x
                        << ","
                        << m_dockAssistedSafeRecovery.direction.y
                        << ","
                        << m_dockAssistedSafeRecovery.direction.z
                        << ")"
                        << " route_initial_course=("
                        << request.initialForward.x << ","
                        << request.initialForward.y << ","
                        << request.initialForward.z << ")"
                        << " initial_speed_mps=" << request.initialSpeedMps
                        << std::endl;

                    // Start the expensive planner from the snapshot we already
                    // have.  Do not pretend to know where safe-course turning
                    // will end; the single projected entry bind owns that.
                    submitAutopilotControl(
                        coastForPlanningControl()
                    );
                }
                else
                {
                    // Current ballistic course is already collision-free.
                    // Planning may begin immediately with zero acceleration.
                    submitAutopilotControl(
                        coastForPlanningControl()
                    );
                }
            }
            else if (request.initialSpeedMps > 0.5)
            {
                // Newtonian intentionally remains on the old selector/executor
                // until its separate rotate/settle/burn recovery contract is
                // implemented. Do not pretend the Assisted arc API applies.
                game::navigation::PreplanningSafetyCourseRequest safety;
                safety.positionMeters = request.startMeters;
                safety.velocityMps = startVelocityMps;
                safety.hullForward = hullForwardLocal;
                safety.preferredDirection =
                    preferredSafetyDirection;
                safety.hullRadiusMeters =
                    request.agentRadiusMeters;
                safety.additionalClearanceMeters =
                    std::max(
                        25.0,
                        definition->requiredClearanceMeters
                    );
                safety.courseSafetySpotRadiusMeters =
                    std::clamp(
                        request.agentRadiusMeters * 0.35 +
                            safety.additionalClearanceMeters * 0.25,
                        15.0,
                        50.0
                    );
                safety.courseAccelerationMps2 =
                    request.acceleratingMps2;
                safety.brakingAccelerationMps2 =
                    request.brakingMps2;
                safety.maxAngularVelocityRadPerSecond =
                    request.maxAngularVelocityRadPerSecond;
                safety.newtonian = true;
                safety.obstacles = request.obstacles;

                const auto safetyResult =
                    game::navigation::PreplanningSafetyCourse::
                        evaluate(safety);

                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=check"
                    << " speed_mps=" << safetyResult.speedMps
                    << " current_safe="
                    << (safetyResult.currentCourseSafe ? 1 : 0)
                    << " escape_found="
                    << (safetyResult.safeCourseFound ? 1 : 0)
                    << " stop_clear="
                    << (safetyResult.stopFallbackClear ? 1 : 0)
                    << " ray_m=" << safetyResult.sceneRayMeters
                    << " turn_deg="
                    << safetyResult.turnAngleRad *
                        57.2957795130823208768
                    << " turn_radius_m="
                    << safetyResult.turnRadiusMeters
                    << " reason=\""
                    << safetyResult.message
                    << "\""
                    << std::endl;

                if (!safetyResult.currentCourseSafe)
                {
                    m_dockSafeCourse = {};
                    m_dockSafeCourse.active = true;
                    m_dockSafeCourse.serial = pending.serial;
                    m_dockSafeCourse.stopFallback =
                        !safetyResult.safeCourseFound;
                    m_dockSafeCourse.direction =
                        safetyResult.safeCourse;
                    m_dockSafeCourse.targetSpeedMps =
                        safetyResult.speedMps;
                    m_dockSafeCourse.sceneRayMeters =
                        safetyResult.sceneRayMeters;
                    m_dockSafeCourse.selectedTurnAngleRad =
                        safetyResult.turnAngleRad;
                    m_dockSafeCourse.selectedTurnRadiusMeters =
                        safetyResult.turnRadiusMeters;
                    m_dockSafeCourse.lastRecheckServerSeconds =
                        authoritativeServerSeconds;
                    m_dockSafeCourse.hullRadiusMeters =
                        safety.hullRadiusMeters;
                    m_dockSafeCourse.additionalClearanceMeters =
                        safety.additionalClearanceMeters;
                    m_dockSafeCourse.courseAccelerationMps2 =
                        safety.courseAccelerationMps2;
                    m_dockSafeCourse.brakingAccelerationMps2 =
                        safety.brakingAccelerationMps2;
                    m_dockSafeCourse.
                        maxAngularVelocityRadPerSecond =
                            safety.maxAngularVelocityRadPerSecond;
                    m_dockSafeCourse.newtonian = true;
                    m_dockSafeCourse.preferredDirection =
                        safety.preferredDirection;
                    m_dockSafeCourse.obstacles =
                        std::move(safety.obstacles);

                    m_clientDockingPhase =
                        ClientDockingPhase::SecuringCourse;
                    m_clientDockingSettledSinceServerSeconds =
                        -1.0;

                    if (m_dockSafeCourse.stopFallback)
                    {
                        std::cerr
                            << "[DockSafetyCourse]"
                            << " request=" << pending.serial
                            << " phase=emergency-stop"
                            << " stop_corridor_clear="
                            << (
                                safetyResult.stopFallbackClear
                                    ? 1
                                    : 0
                            )
                            << std::endl;
                        submitAutopilotControl(
                            manualStopControl()
                        );
                        return;
                    }

                    planningWhileSecuringCourse = true;
                    request.initialForward =
                        m_dockSafeCourse.direction;

                    std::cerr
                        << "[DockSafetyCourse]"
                        << " request=" << pending.serial
                        << " phase=securing+planning"
                        << " course=("
                        << m_dockSafeCourse.direction.x << ","
                        << m_dockSafeCourse.direction.y << ","
                        << m_dockSafeCourse.direction.z << ")"
                        << std::endl;

                    submitAutopilotControl(
                        legacyNewtonianSafeCourseControl(
                            m_dockSafeCourse.direction
                        )
                    );
                }
            }
            else
            {
                // A Newtonian ship at rest has no ballistic course to secure
                // and may turn freely: the planned route chooses the first
                // direction (no launch heading, no protected lead), its own
                // collision proof covers the departure, and the pilot turns
                // the hull onto the first segment before the first burn.
                request.hasInitialForward = false;
                request.initialForwardLeadMeters = 0.0;
                std::cerr
                    << "[DockSafetyCourse]"
                    << " request=" << pending.serial
                    << " phase=skipped-at-rest"
                    << std::endl;
                submitAutopilotControl(
                    coastForPlanningControl()
                );
            }
        }

        // Planner workers are serialized. If an older worker is still
        // unwinding, the live safety controller keeps ownership of the craft
        // while we wait; no new optimizer is stacked on top of it.
        const int livePlannerWorkers =
            m_dockWorkerCount->load(std::memory_order_acquire);
        if (livePlannerWorkers > 0)
            return;

        game::navigation::traffic::CompiledTrafficRoute
            compiledTrafficRoute;

        // TEMPORARY A/B DIAGNOSTIC GATE:
        // Disable semantic traffic / BLUE mandatory traversal completely.
        // This leaves the legacy-normal RoutePlanner -> dock approach path
        // untouched so we can prove whether the regression belongs to the
        // traffic boundary layer rather than the base planner/autopilot.
        constexpr bool EnableMandatoryBlueTransit = true;

        if (!lateEntryRebind &&
            EnableMandatoryBlueTransit &&
            pending.target.stableObjectId == "guidance_dock_cube_a" &&
            pending.target.semanticAnchorId == "dock_gate_front")
        {
            game::navigation::traffic::TrafficRouteGraphCatalog trafficCatalog;
            if (!trafficCatalog.load(
                    "assets/data/navigation/hub_traffic_route_graph.json"))
            {
                fail("traffic graph unavailable: " + trafficCatalog.failure());
                return;
            }

            game::navigation::NavigationVolumeCatalog volumeCatalog;
            if (!volumeCatalog.load(
                    "assets/data/navigation/hub_navigation_volumes.json"))
            {
                fail("navigation volume catalog unavailable");
                return;
            }

            const auto trafficGraph = trafficCatalog.graph();
            const auto trafficRoute = trafficGraph->resolve(
                "cylinder_b.entry_front",
                "cube_a.dock_front"
            );
            if (!trafficRoute.valid)
            {
                fail(
                    "assigned blue traffic route is invalid: " +
                    trafficRoute.failure
                );
                return;
            }

            game::navigation::traffic::RouteStageCompiler::
                ReferenceFrameMap trafficFrames;

            for (const auto& object : snapshot.objects)
            {
                const auto& attachment = object.hubAttachment;
                if (!attachment.valid ||
                    !attachment.inheritHubOrientation ||
                    attachment.moduleId.empty())
                {
                    continue;
                }

                const glm::dvec3 angles =
                    attachment.localRotationDeg +
                    attachment.localAngularVelocityDegPerSecond *
                        snapshot.epoch.universeTimeSeconds;
                const glm::dmat3 visualRotation(
                    glm::mat3(
                        game::navigation::hubLocalEulerDegToMatrix(angles)
                    )
                );

                game::navigation::traffic::NavigationReferenceFramePose
                    framePose;
                framePose.originWorldMeters =
                    game::navigation::hubVisualToTacticalVector(
                        attachment.localOffsetMeters
                    );

                framePose.localToWorldBasis = glm::dmat3(
                    game::navigation::hubVisualToTacticalVector(
                        visualRotation * glm::dvec3(1.0, 0.0, 0.0)
                    ),
                    game::navigation::hubVisualToTacticalVector(
                        visualRotation * glm::dvec3(0.0, 1.0, 0.0)
                    ),
                    game::navigation::hubVisualToTacticalVector(
                        visualRotation * glm::dvec3(0.0, 0.0, 1.0)
                    )
                );

                trafficFrames[attachment.moduleId] = framePose;
            }

            compiledTrafficRoute =
                game::navigation::traffic::RouteStageCompiler::compile(
                    trafficRoute,
                    *trafficGraph,
                    m_hubSemanticAnchorCatalog,
                    volumeCatalog,
                    trafficFrames
                );

            if (!compiledTrafficRoute.valid)
            {
                fail(
                    "traffic route compilation failed: " +
                    compiledTrafficRoute.failure
                );
                return;
            }

            // Step 1b (Newtonian): a portal is a window, not a point. Choose
            // the crossing inside each straight mandatory volume that needs
            // the least turning of the free legs around it; every consumer
            // below sees the shifted portals/straights. Assisted keeps the
            // centred crossing.
            if (!assisted)
            {
                const auto crossings =
                    game::navigation::traffic::chooseWindowCrossings(
                        compiledTrafficRoute,
                        request.startMeters,
                        request.goalMeters,
                        request.agentRadiusMeters,
                        game::navigation::traffic::ShipHullBox {
                            0.5 * request.vehicleWidthMeters,
                            0.5 * request.vehicleHeightMeters,
                            0.5 * request.vehicleLengthMeters},
                        request.narrowZoneAttitudeToleranceRad
                    );
                // The planner's own file log (stdout is not attached in the
                // native client).
                std::ofstream crossingLog(
                    "D:/tmp/elite_docking_planner.log",
                    std::ios::out | std::ios::app);
                for (const auto& crossing : crossings)
                {
                    if (crossingLog)
                        crossingLog
                            << "[TrafficWindowCrossing]"
                            << " request=" << pending.serial
                            << " stage=" << crossing.stageIndex
                            << " offset_m="
                            << glm::length(crossing.offsetWorldMeters)
                            << " admissible_m="
                            << crossing.admissibleRadiusMeters
                            << " centred_turn_deg="
                            << crossing.centredTurnRad * 57.29577951308232
                            << " chosen_turn_deg="
                            << (crossing.turnBeforeRad +
                                crossing.turnAfterRad) * 57.29577951308232
                            << std::endl;
                    std::cout
                        << "[TrafficWindowCrossing]"
                        << " request=" << pending.serial
                        << " stage=" << crossing.stageIndex
                        << " offset_m=" << glm::length(
                               crossing.offsetWorldMeters)
                        << " admissible_m=" << crossing.admissibleRadiusMeters
                        << " centred_turn_deg="
                        << crossing.centredTurnRad * 57.29577951308232
                        << " turn_before_deg="
                        << crossing.turnBeforeRad * 57.29577951308232
                        << " turn_after_deg="
                        << crossing.turnAfterRad * 57.29577951308232
                        << std::endl;
                }
            }

            std::cout
                << "[TrafficRouteCompile]"
                << " request=" << pending.serial
                << " stages=" << compiledTrafficRoute.stages.size()
                << " via_points="
                << compiledTrafficRoute.requiredViaPointsMeters.size()
                << " hard_straights="
                << compiledTrafficRoute.mandatoryTangentStraights.size()
                << std::endl;

            for (std::size_t i = 0;
                 i < compiledTrafficRoute.stages.size();
                 ++i)
            {
                const auto& stage = compiledTrafficRoute.stages[i];
                std::cout
                    << "[TrafficStage]"
                    << " request=" << pending.serial
                    << " i=" << i
                    << " kind=" << static_cast<int>(stage.kind)
                    << " from=("
                    << stage.fromWorldMeters.x << ","
                    << stage.fromWorldMeters.y << ","
                    << stage.fromWorldMeters.z << ")"
                    << " to=("
                    << stage.toWorldMeters.x << ","
                    << stage.toWorldMeters.y << ","
                    << stage.toWorldMeters.z << ")"
                    << " volume=" << stage.volumeConstraint.volumeId
                    << " policy="
                    << static_cast<int>(stage.volumeConstraint.policy)
                    << std::endl;
            }

            for (std::size_t i = 0;
                 i < compiledTrafficRoute.requiredViaPointsMeters.size();
                 ++i)
            {
                const auto& p =
                    compiledTrafficRoute.requiredViaPointsMeters[i];
                std::cout
                    << "[TrafficVia]"
                    << " request=" << pending.serial
                    << " i=" << i
                    << " p=("
                    << p.x << "," << p.y << "," << p.z << ")"
                    << std::endl;
            }

            for (std::size_t i = 0;
                 i < compiledTrafficRoute.mandatoryTangentStraights.size();
                 ++i)
            {
                const auto& straight =
                    compiledTrafficRoute.mandatoryTangentStraights[i];
                std::cout
                    << "[TrafficHardStraight]"
                    << " request=" << pending.serial
                    << " i=" << i
                    << " portal=" << straight.portalId
                    << " inbound=" << (straight.inbound ? 1 : 0)
                    << " length_m=" << straight.lengthMeters
                    << " start=("
                    << straight.startWorldMeters.x << ","
                    << straight.startWorldMeters.y << ","
                    << straight.startWorldMeters.z << ")"
                    << " end=("
                    << straight.endWorldMeters.x << ","
                    << straight.endWorldMeters.y << ","
                    << straight.endWorldMeters.z << ")"
                    << std::endl;
            }

            request.requiredViaPointsMeters =
                compiledTrafficRoute.requiredViaPointsMeters;

            // The compiled traffic route is generic and therefore includes
            // its destination portal. For docking, however, the terminal
            // docking portal/approach is owned by DockingAdvisoryPlanner:
            // it authors preEntry -> exact terminal arc -> final axis -> dock.
            // Keeping the dock portal as a mandatory transit via would force
            // the route to visit the dock first and then fly back out to
            // preEntry before entering the terminal arc.
            if (!request.requiredViaPointsMeters.empty())
            {
                const glm::dvec3& terminalTrafficVia =
                    request.requiredViaPointsMeters.back();

                if (glm::length(
                        terminalTrafficVia - localPort.positionMeters
                    ) <= game::navigation::geometry_tolerance::ConstructedPointMeters)
                {
                    std::cout
                        << "[TrafficDockAdapter]"
                        << " request=" << pending.serial
                        << " drop_terminal_via=("
                        << terminalTrafficVia.x << ","
                        << terminalTrafficVia.y << ","
                        << terminalTrafficVia.z << ")"
                        << " reason=docking-terminal-owned-by-planner"
                        << std::endl;

                    request.requiredViaPointsMeters.pop_back();
                }
            }

            request.mandatoryTangentStraights.clear();
            request.mandatoryTangentStraights.reserve(
                compiledTrafficRoute.mandatoryTangentStraights.size()
            );

            for (const auto& straight :
                 compiledTrafficRoute.mandatoryTangentStraights)
            {
                game::navigation::planner::
                    MandatoryTangentStraightConstraint constraint;
                constraint.startMeters = straight.startWorldMeters;
                constraint.endMeters = straight.endWorldMeters;
                constraint.minimumStraightMeters = straight.lengthMeters;
                constraint.inbound = straight.inbound;
                request.mandatoryTangentStraights.push_back(constraint);
            }

            const auto* lane =
                trafficGraph->lane("cylinder_b.blue_lane");
            if (!lane ||
                lane->policy !=
                    world::navigation::NavigationVolumePolicy::KeepInside)
            {
                fail("assigned blue lane lost KeepInside policy");
                return;
            }

            const auto transitObjectIt = std::find_if(
                snapshot.objects.begin(),
                snapshot.objects.end(),
                [&](const auto& object)
                {
                    return object.hubAttachment.valid &&
                        object.hubAttachment.moduleId ==
                            "guidance_dock_cylinder_b";
                }
            );

            if (transitObjectIt == snapshot.objects.end())
            {
                fail("blue transit module unavailable in planning snapshot");
                return;
            }

            // Cylinder B render/collision solid surrounds an authored physical
            // passage. The semantic BLUE volume owns navigation inside that
            // passage, therefore the enclosing module must not simultaneously
            // appear as a solid obstacle to the route planner.
            const std::uint32_t transitEntityId =
                transitObjectIt->id.value;
            request.obstacles.erase(
                std::remove_if(
                    request.obstacles.begin(),
                    request.obstacles.end(),
                    [&](const auto& obstacle)
                    {
                        return obstacle.entityId == transitEntityId;
                    }
                ),
                request.obstacles.end()
            );

            std::cout
                << "[TrafficRoute] request=" << pending.serial
                << " stages=" << compiledTrafficRoute.stages.size()
                << " via_points="
                << compiledTrafficRoute.requiredViaPointsMeters.size()
                << " tangent_straights="
                << compiledTrafficRoute.mandatoryTangentStraights.size()
                << " lane=" << lane->id
                << " volume=" << lane->navigationVolumeId
                << " policy=KeepInside"
                << " void_bridge_entity=" << transitEntityId
                << std::endl;
        }

        bool hasPreferredEntryAnchor = false;
        glm::dvec3 preferredEntryAnchor {0.0};
        glm::dvec3 preferredEntryForward {0.0, 0.0, -1.0};

        if (compiledTrafficRoute.valid)
        {
            for (const auto& stage : compiledTrafficRoute.stages)
            {
                if (stage.kind !=
                        game::navigation::traffic::
                            TrafficRouteStageKind::VolumeTransit ||
                    (stage.volumeConstraint.policy !=
                         world::navigation::
                             NavigationVolumePolicy::KeepInside &&
                     stage.volumeConstraint.policy !=
                         world::navigation::
                             NavigationVolumePolicy::PreferInside))
                {
                    continue;
                }

                glm::dvec3 traversalForward =
                    stage.toWorldMeters - stage.fromWorldMeters;
                if (glm::length(traversalForward) <= 1.0e-9 &&
                    !stage.volumeSections.empty())
                {
                    traversalForward =
                        stage.volumeSections.front().forwardWorld;
                }

                if (glm::length(traversalForward) <= 1.0e-9)
                    break;

                traversalForward = glm::normalize(traversalForward);

                preferredEntryAnchor = stage.fromWorldMeters;
                for (const auto& straight :
                     compiledTrafficRoute.mandatoryTangentStraights)
                {
                    if (straight.inbound &&
                        straight.portalId == stage.fromPortalId)
                    {
                        preferredEntryAnchor =
                            straight.startWorldMeters;
                        break;
                    }
                }

                hasPreferredEntryAnchor = true;
                preferredEntryForward = traversalForward;
                break;
            }
        }

        if (m_dockWorkerCount->load(std::memory_order_acquire) != 0)
        {
            // Stabilizing above normally owns this wait. Keep this guard
            // non-fatal in case a worker completes between snapshot creation
            // and launch.
            return;
        }

        auto job = std::make_shared<DockAdviceJob>();
        job->planningProgressStage.store(0, std::memory_order_release);
        job->planningProgressPermille.store(100, std::memory_order_release);
        job->timelineRevision =
            snapshot.epoch.universeTimelineRevision;
        job->startedServerSeconds =
            snapshot.epoch.serverTimeSeconds;
        job->executionStartUniverseTimeSeconds =
            snapshot.epoch.universeTimeSeconds +
            (lateEntryRebind ? m_dockLateEntry.leadSeconds : 0.0);
        job->planningRequest = request;
        job->entryRebind = lateEntryRebind;
        if (lateEntryRebind)
            job->entryBackbone = m_dockLateEntry.backbone;
        job->hasPreferredEntryAnchor = hasPreferredEntryAnchor;
        job->preferredEntryAnchor = preferredEntryAnchor;
        job->preferredEntryForward = preferredEntryForward;
        job->context.serial = pending.serial;
        job->context.assistedRoutePreset = request.assistedRoutePreset;
        job->context.systemId = snapshot.systemId;
        job->context.hubId =
            snapshot.hubPredictionSource.hubId;
        job->context.targetObjectId =
            snapshot.targetObject.id.value;
        job->context.timelineRevision =
            snapshot.epoch.universeTimelineRevision;
        job->context.portDefinition = *definition;
        job->context.portAttachment =
            snapshot.targetObject.hubAttachment;
        job->context.routeUpReference = localPort.up;
        job->context.dockCenterReference =
            localPort.positionMeters;
        job->context.dockBottomReference =
            localPort.positionMeters - localPort.up;
        job->context.standoffMeters =
            request.terminalReferenceDistanceMeters;
        job->context.widthMeters =
            fit.openingWidthMeters;
        job->context.heightMeters =
            fit.openingHeightMeters;
        job->context.shipWidthMeters =
            hull.widthMeters;
        job->context.shipHeightMeters =
            hull.heightMeters;
        job->context.lateralToleranceMeters =
            fit.widthMarginMeters * 0.5;
        job->context.verticalToleranceMeters =
            fit.heightMarginMeters * 0.5;

        auto compileAgent = makeAgent();
        compileAgent.positionMapMeters = request.startMeters;
        compileAgent.velocityMapMetersPerSecond = startVelocityMps;

        if (planningWhileSecuringCourse)
        {
            // Compile against the collision-proven state we are physically
            // acquiring, not the transient pre-turn velocity/attitude. The
            // spatial handoff still validates the live craft before adoption.
            const glm::dvec3 predictedForward =
                glm::normalize(request.initialForward);

            compileAgent.velocityMapMetersPerSecond =
                predictedForward * request.initialSpeedMps;
            compileAgent.forwardMap = predictedForward;

            glm::dvec3 predictedUp =
                compileAgent.upMap -
                predictedForward *
                    glm::dot(
                        compileAgent.upMap,
                        predictedForward
                    );

            if (glm::length(predictedUp) <= 1.0e-9)
            {
                const glm::dvec3 fallbackUp =
                    std::abs(predictedForward.y) < 0.9
                        ? glm::dvec3(0.0, 1.0, 0.0)
                        : glm::dvec3(1.0, 0.0, 0.0);
                predictedUp =
                    fallbackUp -
                    predictedForward *
                        glm::dot(
                            fallbackUp,
                            predictedForward
                        );
            }

            predictedUp = glm::normalize(predictedUp);
            compileAgent.rightMap =
                glm::normalize(
                    glm::cross(
                        predictedForward,
                        predictedUp
                    )
                );
            compileAgent.upMap =
                glm::normalize(
                    glm::cross(
                        compileAgent.rightMap,
                        predictedForward
                    )
                );
        }

        const double compileTolerance =
            std::max(
                20.0,
                std::max(
                    job->context.lateralToleranceMeters,
                    job->context.verticalToleranceMeters
                )
            );
        const auto compileControlLaw = controlLaw;
        const auto compilePhysics = effectivePhysics;
        m_dockPlannedAssistedAuthority = liveAssistedAuthority;
        const std::uint64_t compileRequestSerial = pending.serial;
        const double compileExecutionUniverseTimeSeconds =
            job->executionStartUniverseTimeSeconds;
        job->autopilotCompileRequested = automatic;

        m_dockWorkerCount->fetch_add(
            1,
            std::memory_order_acq_rel
        );
        m_dockAdviceJob = job;
        m_clientDockingPhase =
            planningWhileSecuringCourse
                ? ClientDockingPhase::SecuringCourse
                : ClientDockingPhase::Planning;

        const double planningMaxSpeedMps = request.maxSpeedMps;
        const double planningInitialForwardLeadMeters =
            request.initialForwardLeadMeters;
        const glm::dvec3 planningSnapshotStartMeters =
            request.startMeters;
        const double planningInitialSpeedMps =
            request.initialSpeedMps;

        std::thread(
            [job,
             count = m_dockWorkerCount,
             request = std::move(request),
             compiledTrafficRoute = std::move(compiledTrafficRoute),
             compileAgent,
             compileControlLaw,
             compilePhysics,
             compileRequestSerial,
             compileTolerance,
             compileExecutionUniverseTimeSeconds]() mutable
            {
                const auto workerStarted =
                    std::chrono::steady_clock::now();

                const auto finishWorker = [&]()
                {
                    job->workerDurationMs =
                        std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() -
                            workerStarted
                        ).count();

                    job->ready.store(
                        true,
                        std::memory_order_release
                    );
                    count->fetch_sub(
                        1,
                        std::memory_order_acq_rel
                    );
                };

                try
                {
                    job->planningProgressStage.store(
                        1,
                        std::memory_order_release
                    );
                    job->planningProgressPermille.store(
                        200,
                        std::memory_order_release
                    );

                    const auto routeSolveStarted =
                        std::chrono::steady_clock::now();
                    if (compileControlLaw ==
                        game::navigation::LocalFlightControlLaw::Assisted)
                    {
                        using Pipeline =
                            game::navigation::planner::assisted::
                                AssistedPlanningPipeline;
                        job->plan =
                            job->entryRebind
                                ? Pipeline::bind(
                                      request,
                                      job->entryBackbone
                                  )
                                : compiledTrafficRoute.valid
                                    ? Pipeline::segmented(
                                          request,
                                          compiledTrafficRoute
                                      )
                                    : Pipeline::route(request);
                    }
                    else
                    {
                        using Pipeline =
                            game::navigation::planner::newtonian::
                                NewtonianPlanningPipeline;
                        job->plan =
                            job->entryRebind
                                ? Pipeline::bind(
                                      request,
                                      job->entryBackbone
                                  )
                                : compiledTrafficRoute.valid
                                    ? Pipeline::segmented(
                                          request,
                                          compiledTrafficRoute
                                      )
                                    : Pipeline::route(request);
                    }

                    if (job->entryRebind)
                    {
                        job->entrySolveDurationMs =
                            std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() -
                                routeSolveStarted
                            ).count();
                    }

                    job->planningProgressPermille.store(
                        700,
                        std::memory_order_release
                    );

                    if (job->cancelled.load(std::memory_order_acquire))
                    {
                        finishWorker();
                        return;
                    }

                    if (job->plan.valid() &&
                        compiledTrafficRoute.valid)
                    {
                        for (const auto& stage :
                             compiledTrafficRoute.stages)
                        {
                            if (stage.kind !=
                                    game::navigation::traffic::
                                        TrafficRouteStageKind::VolumeTransit ||
                                stage.volumeConstraint.policy !=
                                    world::navigation::
                                        NavigationVolumePolicy::KeepInside)
                            {
                                continue;
                            }

                            // Same ship fit as the composer (Step 3): the
                            // aligned hull box in a narrow volume.
                            const auto containment =
                                game::navigation::traffic::
                                    validateKeepInsideForShip(
                                        stage,
                                        job->plan.routeCurves,
                                        game::navigation::traffic::ShipHullBox {
                                            0.5 * request.vehicleWidthMeters,
                                            0.5 * request.vehicleHeightMeters,
                                            0.5 * request.vehicleLengthMeters},
                                        request.agentRadiusMeters,
                                        request.narrowZoneAttitudeToleranceRad
                                    );

                            if (!containment.valid)
                            {
                                job->plan.disposition =
                                    game::navigation::planner::
                                        RoutePlanDisposition::NeedsRefinement;
                                job->plan.failureCode =
                                    game::navigation::planner::
                                        RoutePlanFailureCode::
                                            NoCollisionFreeCandidate;
                                job->plan.failure =
                                    "assigned BLUE corridor containment failed: " +
                                    containment.failure;
                                job->plan.userMessage =
                                    job->plan.failure;
                                break;
                            }
                        }
                    }

                    if (job->plan.valid() &&
                        compiledTrafficRoute.valid)
                    {
                        // Explicit semantic -> generic contract seam.
                        // SpaceState only composes the modules; it does not
                        // derive route-stage progress or frame ownership.
                        const auto stageContract =
                            game::navigation::traffic::
                                TrafficRoutePlanAdapter::
                                    buildStageContract(
                                        compiledTrafficRoute,
                                        job->plan,
                                        job->context.routeUpReference
                                    );

                        if (!stageContract.valid)
                        {
                            job->plan.disposition =
                                game::navigation::planner::
                                    RoutePlanDisposition::NeedsRefinement;
                            job->plan.failureCode =
                                game::navigation::planner::
                                    RoutePlanFailureCode::BackendFailure;
                            job->plan.failure =
                                "traffic route-plan adapter failed: " +
                                stageContract.failure;
                            job->plan.userMessage =
                                job->plan.failure;
                        }
                        else
                        {
                            job->plan.stages = stageContract.stages;
                            job->plan.routeFrameAnchors =
                                stageContract.frameAnchors;

                            for (const auto& stage :
                                 job->plan.stages)
                            {
                                std::cerr
                                    << "[RouteStage]"
                                    << " id=" << stage.id
                                    << " kind="
                                    << static_cast<int>(stage.kind)
                                    << " s0="
                                    << stage.startProgressMeters
                                    << " s1="
                                    << stage.endProgressMeters
                                    << " frame_policy="
                                    << static_cast<int>(
                                           stage.framePolicy
                                       )
                                    << " refresh_policy="
                                    << static_cast<int>(
                                           stage.refreshPolicy
                                       )
                                    << " source=" << stage.sourceId
                                    << " volume=" << stage.volumeId
                                    << " hard="
                                    << (stage.hardContainment ? 1 : 0)
                                    << std::endl;
                            }
                        }
                    }

                    if (job->cancelled.load(std::memory_order_acquire))
                    {
                        finishWorker();
                        return;
                    }

                    // Route presentation and autopilot compilation belong to
                    // every valid planner route. They are not traffic-route
                    // concerns: a direct/free-space docking route must pass
                    // through exactly the same execution pipeline.
                    if (job->plan.valid())
                    {
                        std::vector<
                            game::navigation::RouteFrameField::Anchor>
                            visualAnchors;
                        visualAnchors.reserve(
                            job->plan.routeFrameAnchors.size()
                        );
                        for (const auto& anchor :
                             job->plan.routeFrameAnchors)
                        {
                            game::navigation::RouteFrameField::Anchor visual;
                            visual.progressMeters = anchor.progressMeters;
                            visual.upReference = anchor.upReference;
                            visual.dynamicRollPhaseWeight =
                                anchor.dynamicRollPhaseWeight;
                            visualAnchors.push_back(visual);
                        }

                        const auto visualFrameField =
                            game::navigation::RouteFrameField::build(
                                job->plan.routeCurves,
                                job->context.routeUpReference,
                                visualAnchors
                            );
                        job->context.visualGateUp =
                            game::navigation::RouteFrameField::
                                sampleUpForGates(
                                    visualFrameField,
                                    job->plan.gates
                                );
                        job->context.visualExecutionGateUp =
                            game::navigation::RouteFrameField::
                                sampleUpForGates(
                                    visualFrameField,
                                    job->plan.executionGates
                                );
                        job->context.visualGateDockPhaseWeight =
                            game::navigation::RouteFrameField::
                                sampleDynamicRollPhaseWeightForGates(
                                    visualFrameField,
                                    job->plan.gates
                                );
                        job->context.visualExecutionGateDockPhaseWeight =
                            game::navigation::RouteFrameField::
                                sampleDynamicRollPhaseWeightForGates(
                                    visualFrameField,
                                    job->plan.executionGates
                                );

                        job->planningProgressStage.store(
                            2,
                            std::memory_order_release
                        );
                        job->planningProgressPermille.store(
                            850,
                            std::memory_order_release
                        );

                        if (job->cancelled.load(std::memory_order_acquire))
                        {
                            finishWorker();
                            return;
                        }

                        if (job->autopilotCompileRequested)
                        {
                            job->planningProgressStage.store(
                                3,
                                std::memory_order_release
                            );
                            job->planningProgressPermille.store(
                                930,
                                std::memory_order_release
                            );

                            job->compiledAutopilotLaw =
                                compileControlLaw;
                            if (compileControlLaw ==
                                game::navigation::
                                    LocalFlightControlLaw::Assisted)
                            {
                                job->autopilotCompiled =
                                    game::navigation::autopilot::
                                        AssistedClientRouteAutopilot::start(
                                            job->compiledAssistedAutopilotState,
                                            job->plan,
                                            compileAgent,
                                            compilePhysics,
                                            compileExecutionUniverseTimeSeconds,
                                            compileRequestSerial,
                                            compileTolerance,
                                            job->context.routeUpReference,
                                            true,
                                            &job->autopilotCompileFailure
                                        );
                            }
                            else
                            {
                                // DEV diagnostic only: the first accepted
                                // Newtonian start, replayable offline.
                                static bool caseDumped = false;
                                const char* casePath =
                                    std::getenv("ELITE_AUTOPILOT_CASE_DUMP");
                                if (!caseDumped && casePath && *casePath)
                                {
                                    game::navigation::diagnostics::
                                        AutopilotCase dump;
                                    dump.plan = job->plan;
                                    dump.agent = compileAgent;
                                    dump.physics = compilePhysics;
                                    dump.acceptedAtUniverseTimeSeconds =
                                        compileExecutionUniverseTimeSeconds;
                                    dump.requestSerial = compileRequestSerial;
                                    dump.trackingToleranceMeters =
                                        compileTolerance;
                                    dump.routeUpReference =
                                        job->context.routeUpReference;
                                    dump.holdAtTerminal = true;
                                    caseDumped =
                                        game::navigation::diagnostics::
                                            writeAutopilotCase(casePath, dump);
                                }
                                job->autopilotCompiled =
                                    game::navigation::autopilot::
                                        NewtonianClientRouteAutopilot::start(
                                            job->compiledNewtonianAutopilotState,
                                            job->plan,
                                            compileAgent,
                                            compilePhysics,
                                            compileExecutionUniverseTimeSeconds,
                                            compileRequestSerial,
                                            compileTolerance,
                                            job->context.routeUpReference,
                                            true,
                                            &job->autopilotCompileFailure
                                        );
                            }
                        }
                    }

                    job->planningProgressStage.store(
                        4,
                        std::memory_order_release
                    );
                    job->planningProgressPermille.store(
                        1000,
                        std::memory_order_release
                    );
                }
                catch (const std::exception& error)
                {
                    job->plan.failure =
                        std::string("planner/compile exception: ") +
                        error.what();
                }
                catch (...)
                {
                    job->plan.failure =
                        "planner/compile unknown exception";
                }

                finishWorker();
            }
        ).detach();

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase="
            << (planningWhileSecuringCourse
                    ? "planning+securing-course"
                    : "planning")
            << " route_source=client"
            << " max_speed_mps=" << planningMaxSpeedMps
            << " initial_forward_lead_m="
            << planningInitialForwardLeadMeters
            << " snapshot_start_m=("
            << planningSnapshotStartMeters.x << ","
            << planningSnapshotStartMeters.y << ","
            << planningSnapshotStartMeters.z << ")"
            << " initial_speed_mps="
            << planningInitialSpeedMps
            << std::endl;
        return;
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::Planning)
    {
        bool automaticMotionSettled = true;

        if (automatic)
        {
            // The worker is allowed to run concurrently with physical
            // stabilization, but route handoff is not. Keep damping angular
            // velocity with zero linear acceleration until the craft has been
            // truly ballistic for a short settled interval.
            constexpr double AutomaticSettledAngularRadPerSec = 0.01;
            constexpr double AutomaticAngularSettledSeconds = 0.20;

            const auto liveAgent = makeAgent();
            const double angularSpeedRadPerSec =
                std::sqrt(
                    liveAgent.pitchRateRadPerSec *
                        liveAgent.pitchRateRadPerSec +
                    liveAgent.yawRateRadPerSec *
                        liveAgent.yawRateRadPerSec +
                    liveAgent.rollRateRadPerSec *
                        liveAgent.rollRateRadPerSec
                );

            if (angularSpeedRadPerSec >
                AutomaticSettledAngularRadPerSec)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                automaticMotionSettled = false;
                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );
            }
            else if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                automaticMotionSettled = false;
                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );
            }
            else if (authoritativeServerSeconds -
                         m_clientDockingSettledSinceServerSeconds <
                     AutomaticAngularSettledSeconds)
            {
                automaticMotionSettled = false;
                submitAutopilotControl(
                    stabilizeAngularForPlanningControl()
                );
            }
            else
            {
                submitAutopilotControl(coastForPlanningControl());
            }
        }
        else
        {
            // Manual route was authored from a settled zero state. Keep that
            // state while asynchronous planning finishes.
            submitAutopilotControl(manualStopControl());
        }

        if (!m_dockAdviceJob ||
            !m_dockAdviceJob->ready.load(
                std::memory_order_acquire
            ))
        {
            return;
        }

        // A worker may finish before the craft has completed the physical
        // transition to its ballistic handoff state. Keep the ready result,
        // but do not adopt/validate it until motion settling is real.
        if (automatic && !automaticMotionSettled)
            return;

        auto job = std::move(m_dockAdviceJob);

        if (job->entryRebind &&
            std::isfinite(job->workerDurationMs) &&
            job->workerDurationMs > 0.0)
        {
            // Actual measured last connector + autopilot compilation latency,
            // distinct from the original expensive backbone computation.
            m_dockObservedConnectorWorkerSeconds =
                job->workerDurationMs * 0.001;
        }

        if (automatic &&
            job->entryRebind &&
            universeTimeSeconds + fixedDt <
                job->executionStartUniverseTimeSeconds)
        {
            // The final connector was intentionally authored from a future
            // ballistic point.  If calculation finished early, wait for that
            // point; do not invent another rebind.
            submitAutopilotControl(coastForPlanningControl());
            m_dockAdviceJob = std::move(job);
            return;
        }

        // Publish one self-contained report from each completed planner
        // worker directly to D:/tmp. std::cout diagnostics may otherwise
        // remain buffered when the game closes immediately after SHOW ROUTE.
        // Never write inside the optimizer or any frame-by-frame controller.
        const bool plannerLogWritten =
            game::navigation::debug::appendDockPlannerReport(
                pending.serial,
                job->plan.valid()
                    ? (job->entryRebind
                           ? "connector-worker-ready"
                           : "route-worker-ready")
                    : "route-worker-failed",
                job->workerDurationMs,
                job->entrySolveDurationMs,
                job->planningRequest,
                job->plan
            );
        if (!plannerLogWritten)
        {
            static bool warnedOnce = false;
            if (!warnedOnce)
            {
                warnedOnce = true;
                std::cerr
                    << "[DockPlannerFile] unable to write "
                    << "D:/tmp/elite_docking_planner.log"
                    << std::endl;
            }
        }

        if (m_client->lastSimulationMetadata().
                universeTimelineRevision !=
                job->timelineRevision)
        {
            recoverOrFail("planning snapshot expired");
            return;
        }

        if (!job->plan.valid())
        {
            const std::string reason =
                !job->plan.userMessage.empty()
                    ? job->plan.userMessage
                    : job->plan.failure;

            if (job->plan.retryable())
            {
                std::cerr
                    << "[DockPlanFailure]"
                    << " request=" << pending.serial
                    << " failure_code="
                    << static_cast<int>(job->plan.failureCode)
                    << " disposition="
                    << static_cast<int>(job->plan.disposition)
                    << " raw_failure=\"" << job->plan.failure << "\""
                    << " terminal_last_rejection=\""
                    << job->plan.terminalArcLastRejection << "\""
                    << " dominant_blocker=\""
                    << job->plan.terminalArcDominantBlockerId << "\""
                    << " blocker_hits="
                    << job->plan.terminalArcDominantBlockerHits
                    << " diagnostics=\""
                    << job->plan.diagnosticSummary << "\""
                    << std::endl;

                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=replan"
                    << " reason=" << reason
                    << std::endl;
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return;
            }

            recoverOrFail(reason);
            return;
        }

        if (automatic)
        {
            const auto liveAgent = makeAgent();
            const double handoffTolerance =
                std::max(
                    20.0,
                    std::max(
                        job->context.lateralToleranceMeters,
                        job->context.verticalToleranceMeters
                    )
                );

            const auto prefix =
                game::navigation::planner::RouteEntryBinder::
                    inspectBallisticPrefix(
                        job->plan,
                        liveAgent.positionMapMeters,
                        handoffTolerance
                    );
            // The original line may be valid now but end before the final
            // connector could finish. Check spatial reachability at the
            // predicted handoff epoch without replacing any valid route.
            const double connectorLeadSeconds =
                game::navigation::planner::BallisticEntryHandoffPolicy::
                    forecastLeadSeconds(
                        m_dockObservedConnectorWorkerSeconds
                    );
            const bool forecastOnPrefix =
                game::navigation::planner::BallisticEntryHandoffPolicy::
                    canReuseStraightPrefix(
                        job->plan,
                        liveAgent.positionMapMeters,
                        liveAgent.velocityMapMetersPerSecond,
                        handoffTolerance,
                        connectorLeadSeconds
                    );
            // The projected connector worker is already FINISHED on a
            // rebound, so only the present exact prefix needs validation.
            const bool mustRebind =
                prefix.disposition !=
                    game::navigation::planner::
                        BallisticPrefixDisposition::OnPrefix ||
                (!job->entryRebind && !forecastOnPrefix);

            double firstTurnProgressMetersForTrace =
                job->plan.routeCurves.front().endProgressMeters;
            for (const auto& curve : job->plan.routeCurves)
            {
                if (curve.kind !=
                    game::navigation::planner::RouteCurveKind::Line)
                {
                    firstTurnProgressMetersForTrace =
                        curve.startProgressMeters;
                    break;
                }
            }
            const double firstTurnRemainingMeters =
                std::max(
                    0.0,
                    firstTurnProgressMetersForTrace - prefix.alongMeters
                );

            std::cerr
                << "[DockLateEntry]"
                << " request=" << pending.serial
                << " disposition="
                << static_cast<int>(prefix.disposition)
                << " along_m=" << prefix.alongMeters
                << " prefix_m=" << prefix.prefixLengthMeters
                << " remaining_m=" << prefix.remainingMeters
                << " first_turn_remaining_m=" << firstTurnRemainingMeters
                << " start_lead_policy_m="
                << job->planningRequest.initialForwardLeadMeters
                << " forecast_distance_m="
                << glm::length(liveAgent.velocityMapMetersPerSecond) *
                       connectorLeadSeconds
                << " cross_m=" << prefix.crossTrackMeters
                << " worker_ms=" << job->workerDurationMs
                << " connector_solve_ms="
                << job->entrySolveDurationMs
                << " connector_forecast_s=" << connectorLeadSeconds
                << " forecast_on_prefix="
                << (forecastOnPrefix ? 1 : 0)
                << " rebound=" << (job->entryRebind ? 1 : 0)
                << std::endl;

            if (mustRebind)
            {
                if (job->entryRebind)
                {
                    // Connector miss is recoverable. The expensive downstream
                    // backbone remains valid; do NOT fail the docking request
                    // and do NOT run the primary safe-course selector again.
                    //
                    // Re-project from the latest live ballistic state and
                    // retry only the cheap connector. Worker duration is the
                    // minimum useful lead because a connector authored for a
                    // point already passed cannot possibly be adopted.
                    ++m_dockLateEntry.rebindAttempts;
                    // An unadoptable connector must not schedule workers
                    // indefinitely. Fail safely after a bounded retry budget.
                    constexpr std::uint32_t MaxEntryConnectorMisses = 4;
                    if (m_dockLateEntry.rebindAttempts >=
                            MaxEntryConnectorMisses)
                    {
                        std::cerr
                            << "[DockClient] request=" << pending.serial
                            << " phase=entry-rebind-exhausted"
                            << " disposition="
                            << static_cast<int>(prefix.disposition)
                            << " along_m=" << prefix.alongMeters
                            << " cross_m=" << prefix.crossTrackMeters
                            << std::endl;
                        recoverOrFail("dock entry connector handoff did not converge");
                        return;
                    }
                    m_dockLateEntry.leadSeconds =
                        game::navigation::planner::
                            BallisticEntryHandoffPolicy::
                                forecastLeadSeconds(
                                    m_dockObservedConnectorWorkerSeconds
                                );

                    m_dockingStatusWarning =
                        "warning: handoff drift; connector retry " +
                        std::to_string(
                            m_dockLateEntry.rebindAttempts
                        );

                    std::cerr
                        << "[DockClient]"
                        << " request=" << pending.serial
                        << " phase=entry-rebind-retry"
                        << " attempt="
                        << m_dockLateEntry.rebindAttempts
                        << " cross_m="
                        << prefix.crossTrackMeters
                        << " lead_s="
                        << m_dockLateEntry.leadSeconds
                        << std::endl;

                    m_clientDockingPhase =
                        ClientDockingPhase::Stabilizing;
                    m_clientDockingSettledSinceServerSeconds = -1.0;
                    submitAutopilotControl(
                        coastForPlanningControl()
                    );
                    return;
                }

                auto backbone =
                    game::navigation::planner::RouteEntryBinder::
                        extractBackbone(
                            job->plan,
                            job->hasPreferredEntryAnchor,
                            job->preferredEntryAnchor,
                            job->preferredEntryForward
                        );

                if (!backbone.valid)
                {
                    recoverOrFail(
                        "late-bound route entry unavailable: " +
                        backbone.failure
                    );
                    return;
                }

                double firstTurnProgressMeters = 0.0;
                for (const auto& curve : job->plan.routeCurves)
                {
                    if (curve.kind !=
                        game::navigation::planner::RouteCurveKind::Line)
                    {
                        firstTurnProgressMeters =
                            std::max(
                                0.0,
                                curve.startProgressMeters
                            );
                        break;
                    }
                }

                if (firstTurnProgressMeters <= 1.0e-6 &&
                    !job->plan.routeCurves.empty())
                {
                    firstTurnProgressMeters =
                        std::max(
                            0.0,
                            job->plan.routeCurves.front().
                                endProgressMeters
                        );
                }

                const double liveSpeedMps =
                    glm::length(
                        liveAgent.velocityMapMetersPerSecond
                    );

                const double firstTurnTravelSeconds =
                    liveSpeedMps > 0.5
                        ? firstTurnProgressMeters / liveSpeedMps
                        : 0.0;

                // The full planner worker's elapsed time is NOT the time
                // required for a late connector; the flight time to the
                // first turn is NOT its compute time either. Use the last
                // observed connector-worker duration (or bootstrap before
                // the first sample). Revalidate after the connector finishes.
                const double measuredWorkerSeconds =
                    std::max(0.0, job->workerDurationMs * 0.001);
                const double leadSeconds = connectorLeadSeconds;

                m_dockLateEntry = {};
                m_dockLateEntry.active = true;
                m_dockLateEntry.serial = pending.serial;
                m_dockLateEntry.backbone =
                    std::move(backbone);
                m_dockLateEntry.leadSeconds =
                    leadSeconds;
                m_dockLateEntry.rebindAttempts = 0;
                m_dockingStatusWarning =
                    "handoff drift: rebinding connector";

                // ============================================================
                // PROJECTED ENTRY CONTRACT -- DO NOT TURN THIS INTO A LOOP.
                //
                // The expensive planner has finished. Keep its downstream
                // route. Project the live ballistic craft by the measured
                // last-connector worker latency, NOT time to the first turn.
                // Calculate ONLY the final entry connector from that point.
                // Connector misses are retried within the bounded budget.
                // Never re-run the expensive downstream route just because
                // its original start became stale.
                // ============================================================
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                m_clientDockingSettledSinceServerSeconds = -1.0;

                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=entry-rebind"
                    << " first_turn_m="
                    << firstTurnProgressMeters
                    << " live_speed_mps=" << liveSpeedMps
                    << " first_turn_s=" << firstTurnTravelSeconds
                    << " lead_s=" << leadSeconds
                    << " whole_worker_s=" << measuredWorkerSeconds
                    << " observed_connector_worker_s="
                    << m_dockObservedConnectorWorkerSeconds
                    << std::endl;
                return;
            }
        }

        // A worker's approved trajectory becomes stale if the installed
        // motors lost physical authority during its multi-second compile.
        if (automatic &&
            controlLaw == game::navigation::LocalFlightControlLaw::Assisted &&
            game::ship::telemetry::hasSignificantAssistedAuthorityLoss(
                m_dockPlannedAssistedAuthority, liveAssistedAuthority
            ))
        {
            fail("motors degraded during route compilation; new route required");
            return;
        }
        m_dockAcceptedAssistedAuthority = liveAssistedAuthority;
        m_dockLateEntry = {};
        m_dockingStatusWarning.clear();

        m_dockAdvice = std::move(job->context);
        m_dockAdvice.workerDurationMs = job->workerDurationMs;
        m_dockAdvice.plan = std::move(job->plan);
        m_clientDockingRouteProgressMeters = 0.0;
        m_clientDockingRouteCurveIndex = 0;
        m_clientAutopilotCrossTrackMeters = 0.0;
        m_clientAutopilotCourseErrorRad = 0.0;
        m_clientAutopilotHullErrorRad = 0.0;
        m_clientAutopilotTargetSpeedMps = 0.0;
        m_clientAutopilotTargetLongitudinalAccelerationMps2 = 0.0;

        // WORKING ASYNC CONTRACT: expensive route compilation and visual
        // frame-field construction completed in the worker. Main thread only
        // adopts ready-to-use products here.

        m_activeDockingGuidanceCorridorId =
            "dock:" +
            pending.target.stableObjectId +
            ":" +
            pending.target.semanticAnchorId;
        m_dockingGuidanceFailureReason.clear();
        m_noSafeDockingGuidanceSolution = false;

        std::cout
            << "[DockClient] request=" << pending.serial
            << " phase=route-ready"
            << " execution_points="
            << m_dockAdvice.plan.executionGates.size()
            << " hud_gates="
            << m_dockAdvice.plan.gates.size()
            << " semantic_stages="
            << m_dockAdvice.plan.stages.size()
            << " terminal_radius_m="
            << m_dockAdvice.plan.terminalTurnRadiusMeters
            << " worker_ms="
            << job->workerDurationMs
            << " planner_diagnostics=\""
            << m_dockAdvice.plan.diagnosticSummary << "\""
            << std::endl;

        for (std::size_t stageIndex = 0;
             stageIndex < m_dockAdvice.plan.stages.size();
             ++stageIndex)
        {
            const auto& stage =
                m_dockAdvice.plan.stages[stageIndex];

            std::cerr
                << "[DockRouteStage]"
                << " request=" << pending.serial
                << " index=" << stageIndex
                << " kind=" << static_cast<int>(stage.kind)
                << " start_m=" << stage.startProgressMeters
                << " end_m=" << stage.endProgressMeters
                << " source=" << stage.sourceId
                << " volume=" << stage.volumeId
                << " hard="
                << (stage.hardContainment ? 1 : 0)
                << std::endl;
        }

        if (automatic)
        {
            if (!job->autopilotCompiled)
            {
                const auto agent = makeAgent();
                std::cerr
                    << "[DockClientStartFailure]"
                    << " request=" << pending.serial
                    << " reason=\"" << job->autopilotCompileFailure << "\""
                    << " speed_mps="
                    << glm::length(agent.velocityMapMetersPerSecond)
                    << " velocity=("
                    << agent.velocityMapMetersPerSecond.x << ","
                    << agent.velocityMapMetersPerSecond.y << ","
                    << agent.velocityMapMetersPerSecond.z << ")"
                    << " forward=("
                    << agent.forwardMap.x << ","
                    << agent.forwardMap.y << ","
                    << agent.forwardMap.z << ")"
                    << " route_first_forward=("
                    << m_dockAdvice.plan.executionGates.front().forward.x << ","
                    << m_dockAdvice.plan.executionGates.front().forward.y << ","
                    << m_dockAdvice.plan.executionGates.front().forward.z << ")"
                    << std::endl;

                recoverOrFail(
                    "client autopilot could not accept planner route: " +
                    (job->autopilotCompileFailure.empty()
                        ? std::string("unknown-start-failure")
                        : job->autopilotCompileFailure)
                );
                return;
            }

            if (job->compiledAutopilotLaw ==
                game::navigation::LocalFlightControlLaw::Assisted)
            {
                m_clientAssistedRouteAutopilot =
                    std::move(job->compiledAssistedAutopilotState);
                NewtonianClientAutopilot::stop(
                    m_clientNewtonianRouteAutopilot
                );
            }
            else
            {
                m_clientNewtonianRouteAutopilot =
                    std::move(job->compiledNewtonianAutopilotState);
                AssistedClientAutopilot::stop(
                    m_clientAssistedRouteAutopilot
                );
            }

            m_clientDockingPhase =
                ClientDockingPhase::Executing;
            std::cerr
                << "[DockClient] request=" << pending.serial
                << " phase=executing"
                << " control_path=ShipControlState"
                << std::endl;
        }
        else
        {
            m_clientDockingPhase =
                ClientDockingPhase::RouteReady;
            m_clientAutopilotControlActive = false;
            m_lastPilotUniverseSeconds = -1.0;
            m_clientAutopilotControl = {};
            std::cerr
                << "[DockClient] request=" << pending.serial
                << " phase=manual"
                << " human_control=1"
                << std::endl;
        }
    }

    const auto currentTunnelRollPhase =
        [&](double sampleUniverseTimeSeconds) -> double
        {
            if (!m_dockAdvice.serial ||
                !m_dockAdvice.plan.valid() ||
                m_dockAdvice.plan.gates.empty())
            {
                return 0.0;
            }

            const auto livePort =
                resolveDockingAdvisoryLocalPortAt(
                    m_dockAdvice.portAttachment,
                    m_dockAdvice.portDefinition,
                    sampleUniverseTimeSeconds
                );
            if (!livePort.valid)
                return 0.0;

            glm::dvec3 terminalRouteAxis =
                m_dockAdvice.plan.gates.back().forward;
            if (glm::length(terminalRouteAxis) <= 1.0e-9)
                return 0.0;
            terminalRouteAxis = glm::normalize(terminalRouteAxis);

            const auto liveReference =
                game::navigation::TwoPointRollGeometry::
                    fromCenterAndUp(
                        livePort.positionMeters,
                        livePort.up
                    );

            return game::navigation::RouteFrameField::
                synchronizedTunnelRollPhase(
                    terminalRouteAxis,
                    m_dockAdvice.dockCenterReference,
                    m_dockAdvice.dockBottomReference,
                    liveReference.center,
                    liveReference.radialPoint
                );
        };

    if (m_clientDockingPhase ==
        ClientDockingPhase::Executing)
    {
        if (controlLaw == game::navigation::LocalFlightControlLaw::Assisted &&
            game::ship::telemetry::hasSignificantAssistedAuthorityLoss(
                m_dockAcceptedAssistedAuthority, liveAssistedAuthority
            ))
        {
            // Do not continue a precomputed high-speed curve after physical
            // engine damage has invalidated the simultaneous-force budget.
            // Returning the pilot interface to the human is safer than
            // pretending the old reference is still flyable. A fresh route
            // request will use the newly replicated motor state.
            std::cerr
                << "[DockAuthority] request=" << pending.serial
                << " phase=accepted-trajectory-invalidated"
                << " reason=installed-motor-capability-loss"
                << std::endl;
            fail("motors degraded; committed route needs new planning");
            return;
        }
        const auto handleOutput = [&](const auto& output) -> bool
        {
            if (output.valid)
            {
                m_clientDockingRouteCurveIndex =
                    output.routeCurveIndex;
                m_clientAutopilotCrossTrackMeters =
                    output.crossTrackErrorMeters;
                m_clientAutopilotCourseErrorRad =
                    output.courseErrorRad;
                m_clientAutopilotHullErrorRad =
                    output.hullForwardErrorRad;
                m_clientAutopilotTargetSpeedMps =
                    output.targetSpeedMps;
                m_clientAutopilotTargetLongitudinalAccelerationMps2 =
                    output.targetLongitudinalAccelerationMps2;
    
                const double routeTotalMeters =
                    m_dockAdvice.plan.routeCurves.empty()
                        ? 0.0
                        : m_dockAdvice.plan.routeCurves.back().
                            endProgressMeters;
    
                m_clientDockingRouteProgressMeters =
                    std::clamp(
                        routeTotalMeters -
                            output.exactRemainingRouteMeters,
                        0.0,
                        std::max(0.0, routeTotalMeters)
                    );
            }
    
            if (!output.valid)
            {
                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=execution-replan"
                    << " reason=follower-invalid"
                    << " detail=\""
                    << (output.invalidReason ? output.invalidReason : "")
                    << "\""
                    << std::endl;
                releaseClientAutopilot();
                m_clientDockingPhase =
                    ClientDockingPhase::Stabilizing;
                submitAutopilotControl(coastForPlanningControl());
                return false;
            }
    
            if (output.complete)
            {
                releaseClientAutopilot();
                m_clientDockingPhase =
                    ClientDockingPhase::RouteReady;
    
                std::cerr
                    << "[DockClient] request=" << pending.serial
                    << " phase=complete"
                    << " human_control=1"
                    << std::endl;
            }
            else
            {
                submitAutopilotControl(output.control);
    
                if (m_clientDockingTraceTick == 0 ||
                    m_client->lastSimulationMetadata().serverTick -
                        m_clientDockingTraceTick >= 60)
                {
                    m_clientDockingTraceTick =
                        m_client->lastSimulationMetadata().serverTick;
    
                    std::cerr
                        << "[DockClientTrack] request="
                        << pending.serial
                        << " page=" << output.pageIndex
                        << " segment=" << output.segmentIndex
                        << " cross_track_m="
                        << output.crossTrackErrorMeters
                        << " remaining_m="
                        << output.remainingDistanceMeters
                        << " target_speed_mps="
                        << output.targetSpeedMps
                        << " actual_speed_mps="
                        << glm::length(motion.localVelocityMps)
                        << " checkpoint="
                        << output.checkpointIndex
                        << " profile_rev="
                        << output.speedProfileRevision
                        << " target_accel_mps2="
                        << output.targetLongitudinalAccelerationMps2
                        << " cross_closing_mps="
                        << output.crossTrackClosingSpeedMps
                        << " course_lead_m="
                        << output.courseLeadDistanceMeters
                        << " predicted_cross_m="
                        << output.predictedCrossTrackMeters
                        << " center_deadband_m="
                        << output.centeringDeadbandMeters
                        << " curve=" << output.routeCurveIndex
                        << " curvature_1pm="
                        << output.routeCurvaturePerMeter
                        << " radius_m="
                        << output.routeRadiusMeters
                        << " required_curve_a_mps2="
                        << glm::dot(
                               motion.localVelocityMps,
                               motion.localVelocityMps
                           ) * output.routeCurvaturePerMeter
                        << " installed_turn_a_mps2="
                        << (liveAssistedAuthority.valid
                              ? std::min({
                                    liveAssistedAuthority.positive[0],
                                    liveAssistedAuthority.negative[0],
                                    liveAssistedAuthority.positive[1],
                                    liveAssistedAuthority.negative[1]
                                })
                              : 0.0)
                        << " exact_remaining_m="
                        << output.exactRemainingRouteMeters
                        << " desired_course_omega_radps="
                        << output.desiredCourseAngularRateRadPerSec
                        << " actual_course_omega_radps="
                        << output.actualCourseAngularRateRadPerSec
                        << " hull_omega_radps="
                        << output.actualAngularRateRadPerSec
                        << " phase_lead_deg="
                        << output.coursePhaseLeadAngleRad *
                            57.2957795130823208768
                        << " course_tau_s="
                        << output.courseResponseSeconds
                        << " brake_a_mps2="
                        << output.effectiveBrakingAuthorityMps2
                        << " stop_need_m="
                        << output.requiredTerminalStopDistanceMeters
                        << " terminal_brake="
                        << (output.terminalBrakeActive ? 1 : 0)
                        << " turn_vmax_mps="
                        << output.turnSpeedCeilingMps
                        << " turn_distance_m="
                        << output.distanceToTurnMeters
                        << " turn_need_m="
                        << output.requiredTurnSlowdownDistanceMeters
                        << " turn_slew_s="
                        << output.turnSpeedSetpointSlewSeconds
                        << " capture_angle_deg="
                        << output.crossTrackCorrectionAngleRad *
                            57.2957795130823208768
                        << " capture_omega_radps="
                        << output.desiredCaptureAngularRateRadPerSec
                        << " roll_err_deg="
                        << output.signedRollErrorRad *
                            57.2957795130823208768
                        << " roll_target_omega_radps="
                        << output.desiredRollRateRadPerSec
                        << " tunnel_roll_phase_deg="
                        << output.liveTunnelRollPhaseRad *
                            57.2957795130823208768
                        << " tunnel_roll_omega_radps="
                        << output.liveTunnelRollRateRadPerSec
                        << " brake_attitude_lock="
                        << (output.brakeAttitudeLockActive ? 1 : 0)
                        << " terminal_attitude_capture="
                        << (output.terminalAttitudeCaptureActive ? 1 : 0)
                        << " forward_err_deg="
                        << output.forwardErrorRad *
                            57.2957795130823208768
                        << " up_err_deg="
                        << output.upErrorRad *
                            57.2957795130823208768
                        << " terminal_hold="
                        << (output.terminalHold ? 1 : 0)
                        << " pitch=" << output.control.pitchInput
                        << " yaw=" << output.control.yawInput
                        << " roll=" << output.control.rollInput
                        << " strafe=" << output.control.strafeInput
                        << " lift=" << output.control.liftInput
                        << " speed_rate="
                        << output.control.targetSpeedRate
                        << " course_err_deg="
                        << output.courseErrorRad *
                            57.2957795130823208768
                        << " hull_err_deg="
                        << output.hullForwardErrorRad *
                            57.2957795130823208768
                        << " gimbal_cmd_r="
                        << output.control.mainGimbalRightInput
                        << " gimbal_cmd_u="
                        << output.control.mainGimbalUpInput
                        << " main_a_mps2="
                        << glm::length(
                            motion.mainEngineAccelerationMps2
                        )
                        << " motor_angular_a_radps2="
                        << glm::length(
                            motion.mainPropulsionAngularAccelerationBodyRadPerSec2
                        )
                        << " assisted_aux_a_mps2="
                        << glm::length(
                            motion.assistedStabilizationAccelerationMps2
                        )
                        << " rcs_a_mps2="
                        << glm::length(
                            motion.manoeuvreAccelerationMps2
                        )
                        << " gimbal_yaw_deg="
                        << motion.mainGimbalYawRad *
                            57.2957795130823208768
                        << " gimbal_pitch_deg="
                        << motion.mainGimbalPitchRad *
                            57.2957795130823208768
                        << std::endl;
                }
            }
    
            return true;
        };

        const double tunnelRollPhase =
            currentTunnelRollPhase(universeTimeSeconds);

        /*
            The route pilot is a fixed-step controller: it steps once per
            authoritative simulation time that actually elapsed, never once
            per render frame. (Stepping every frame with the server step made
            the pilot clock run x2-2.7 at 100+ FPS and lag in slow frames; the
            latest control sample persists in GameClient between steps.)
        */
        const double pilotElapsed =
            m_lastPilotUniverseSeconds >= 0.0
                ? universeTimeSeconds - m_lastPilotUniverseSeconds
                : fixedDt;
        const bool pilotStepDue =
            pilotElapsed < 0.0 || pilotElapsed >= 0.5 * fixedDt;
        if (pilotStepDue)
        {
        const double pilotDt =
            pilotElapsed > 0.0 && pilotElapsed < 1.0
                ? std::clamp(pilotElapsed, fixedDt, 4.0 * fixedDt)
                : fixedDt;
        m_lastPilotUniverseSeconds = universeTimeSeconds;

        if (controlLaw ==
            game::navigation::LocalFlightControlLaw::Assisted)
        {
            AssistedClientAutopilot::setLiveTunnelRollPhase(
                m_clientAssistedRouteAutopilot,
                tunnelRollPhase,
                pilotDt
            );
            const auto output =
                AssistedClientAutopilot::update(
                    m_clientAssistedRouteAutopilot,
                    makeAgent(),
                    effectivePhysics,
                    universeTimeSeconds,
                    pilotDt
                );
            if (!handleOutput(output))
                return;
        }
        else
        {
            NewtonianClientAutopilot::setLiveTunnelRollPhase(
                m_clientNewtonianRouteAutopilot,
                tunnelRollPhase,
                pilotDt
            );
            core::hitch::mark("dock_pre_autopilot");
            const auto output =
                NewtonianClientAutopilot::update(
                    m_clientNewtonianRouteAutopilot,
                    makeAgent(),
                    effectivePhysics,
                    universeTimeSeconds,
                    pilotDt
                );
            core::hitch::mark("dock_newtonian_autopilot_update");
            if (!handleOutput(output))
                return;
        }
        }
    }

    core::hitch::mark("dock_output_handling");
    auto& active = m_dockAdvice;
    if (!active.serial || !active.plan.valid())
        return;

    // Presentation uses the exact same client Planner product that Automatic
    // executes. Server navigation state is not involved.
    const auto& playerRenderFrame =
        player.renderReferenceFrame;
    if (!playerRenderFrame.valid ||
        playerRenderFrame.systemId != active.systemId ||
        playerRenderFrame.hubId != active.hubId ||
        playerRenderFrame.type != MotionMode::HubTactical)
    {
        return;
    }

    const double renderTime =
        playerRenderFrame.universeTimeSeconds;
    auto renderFrame =
        playerRenderFrame.kinematicFrame();
    renderFrame.frameId =
        playerRenderFrame.hubId;

    // WORKING CONTRACT: HUD and hull sample the same pure two-point dock
    // phase function. Only sample time differs (render time vs fixed-sim time).
    const double dockRollPhaseRad =
        currentTunnelRollPhase(renderTime);

    const auto makeRoute =
        [&](game::navigation::DockingCorridorPresentation& presentation,
            const std::vector<
                game::navigation::planner::RouteGate>& routeGates,
            const std::vector<glm::dvec3>& routeFrameUp,
            const std::vector<double>& dockPhaseWeights,
            const std::string& id,
            bool sparseFrames)
        {
            GuidanceCorridor route;
            route.id = id;
            route.systemId = active.systemId;
            route.source = GuidanceSource::DockingComputer;
            route.purpose = GuidancePurpose::Approach;
            route.advisoryOnly = !automatic;
            route.noSafePrimarySolution =
                m_noSafeDockingGuidanceSolution;
            route.priority = automatic ? 100 : 50;
            route.generatedAtUniverseTimeSeconds =
                renderTime;
            route.spatialAdvisoryGates = sparseFrames;
            route.hubLocalFrameId = active.hubId;

            game::navigation::DockingCorridorPresentation::RouteInput input;
            input.gates = &routeGates;
            input.frameUp = &routeFrameUp;
            input.dockPhaseWeights = &dockPhaseWeights;
            input.routeUpReference = active.routeUpReference;
            input.lateralToleranceMeters = active.lateralToleranceMeters;
            input.verticalToleranceMeters = active.verticalToleranceMeters;
            input.shipWidthMeters = active.shipWidthMeters;
            input.shipHeightMeters = active.shipHeightMeters;
            presentation.present(
                route,
                input,
                game::navigation::dockingCorridorPlanKey(active.serial, input),
                renderFrame,
                renderTime,
                dockRollPhaseRad
            );
            return route;
        };

    core::hitch::mark("dock_presentation_build");
    guidance.publish(
        makeRoute(
            m_dockExecutionCorridorPresentation,
            active.plan.executionGates,
            active.visualExecutionGateUp,
            active.visualExecutionGateDockPhaseWeight,
            m_activeDockingGuidanceCorridorId,
            false
        )
    );

    guidance.publish(
        makeRoute(
            m_dockFramesCorridorPresentation,
            active.plan.gates,
            active.visualGateUp,
            active.visualGateDockPhaseWeight,
            m_activeDockingGuidanceCorridorId + ":frames",
            true
        )
    );
}
