#include <iostream>
// SpaceState: Docking / route-autopilot orchestration: route request, planning jobs, safe course, autopilot phases and the docking corridor publication.

#include "src/game/scene/LayoutCityHitVolumes.h"
#include "src/game/damage/HitVolumeBounds.h"
#include "src/core/ThreadPriority.h"
#include "src/game/SpaceStateInternal.h"
#include "src/game/navigation/traffic/HubLaneNetwork.h"
#include "src/game/navigation/traffic/HubLaneRouting.h"
#include "src/game/navigation/HubPlanCatalog.h"
#include "src/world/navigation/NavigationObstacleGeometry.h"
#include "src/world/navigation/NavigationObstacleGrid.h"
#include <memory>



namespace
{
// The asynchronous docking planner + autopilot compile worker (the body of
// the detached planning thread launched by updateDockingStabilizing). It
// owns copies of everything it reads (the thread lambda's captures, passed
// by reference); it never touches SpaceState.
template <class T0, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9>
void runDockAdviceWorker(
    T0& job,
    T1& count,
    T2& request,
    T3& compiledTrafficRoute,
    T4& compileAgent,
    T5& compileControlLaw,
    T6& compilePhysics,
    T7& compileRequestSerial,
    T8& compileTolerance,
    T9& compileExecutionUniverseTimeSeconds)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;

    core::lowerCurrentThreadPriority();   // planning never steals the frame's CPU
    const auto workerStarted =
        std::chrono::steady_clock::now();
    // DEV ELITE_DEV_PLAN_DELAY_S=<s> (+ ELITE_DEV_PLAN_DELAY_REQUEST=<serial>, default 2): a plan slower than the start
    // straight's planning budget - the ship runs past the prefix and the late-entry rebind (the start piece re-solved
    // from the forecast point, the backbone kept) must carry it (author 2026-11-24: «а если расчет будет дольше 8 сек?»)
    if (const char* delay = std::getenv("ELITE_DEV_PLAN_DELAY_S"); delay && *delay)
    {
        const char* which = std::getenv("ELITE_DEV_PLAN_DELAY_REQUEST");
        const auto serial = which && *which ? std::strtoull(which, nullptr, 10) : 2ull;
        static std::atomic<bool> delayed {false};   // the FIRST plan of that request only (the rebind is not delayed)
        if (static_cast<unsigned long long>(compileRequestSerial) == serial && !delayed.exchange(true))
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long long>(std::atof(delay) * 1000.0)));
    }

    const auto finishWorker = [&]()
    {
        job->workerDurationMs =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() -
                workerStarted
            ).count();

        std::cerr << "[DockWorker] request=" << compileRequestSerial << " finished ms=" << job->workerDurationMs
                  << std::endl;
        // the planning-time history (the start lead's flight while planning): a FULL plan that was adopted-ready only -
        // not a rebind (0.1-0.6 s), a failure, a cancel (audit PLAN-3: [12, 0.3, 12, 0.3, 12] made the lead jump 8..18 s)
        if (!job->entryRebind && job->plan.valid() && !job->cancelled.load(std::memory_order_acquire))
            game::navigation::OrdinaryRoutePolicy::notePlanSeconds(job->workerDurationMs / 1000.0);
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

        if (std::getenv("ELITE_ROUTE_DUMP") || std::getenv("ELITE_ROUTE_SUMMARY"))   // DEV: where the planning time goes
            { char diagLine[512]; std::snprintf(diagLine, sizeof diagLine, "[RouteSummary] %s\n", job->plan.diagnosticSummary.substr(0, 4000).c_str()); std::cerr << diagLine << std::flush; }
        // the accepted route against the obstacles it was planned with (the hull radius, no extra margin): a route that
        // runs through a building is never flown (live 2026-11-22: the free leg from a lane exit to a pad's approach
        // point curved through a power block). Logged with the obstacle and the place either way.
        if (job->plan.valid())
        {
            // audit PLAN-6: a grid of the obstacles (the same per-obstacle test, only the obstacles of a segment's cells)
            // instead of a one-obstacle vector COPY per gate pair x every obstacle (~7 million tests a 20 km route); the
            // obstacle named only for the failing segment
            const auto& gates = job->plan.executionGates.empty() ? job->plan.gates : job->plan.executionGates;
            world::navigation::NavigationObstacleGrid grid;
            grid.build(request.obstacles, request.agentRadiusMeters);
            for (std::size_t g = 0; g + 1 < gates.size(); ++g)
            {
                const glm::dvec3 a = gates[g].positionMeters, b = gates[g + 1].positionMeters;
                if (grid.segmentClear(a, b))
                    continue;
                std::string hit = "an obstacle";
                for (const auto& o : request.obstacles)
                    if (world::navigation::segmentIntersectsNavigationObstacle(a, b, o, request.agentRadiusMeters))
                    {
                        hit = o.id;
                        break;
                    }
                { char diagLine[512]; std::snprintf(diagLine, sizeof diagLine, "[RouteCheck] request=%llu the route runs through %s at gate %zu/%zu (%.0f,%.0f,%.0f)\n",
                             static_cast<unsigned long long>(job->context.serial), hit.c_str(), g, gates.size(), a.x, a.y, a.z); std::cerr << diagLine << std::flush; }
                job->plan.failure = "the route runs through " + hit;
                // ... and it is NOT flown (PLAN-6: the plan stayed Ready / None - adopted anyway): a refusal the request
                // replans from
                job->plan.disposition = game::navigation::planner::RoutePlanDisposition::NeedsRefinement;
                job->plan.failureCode = game::navigation::planner::RoutePlanFailureCode::NoCollisionFreeCandidate;
                break;
            }
            if (job->plan.failure.empty())
                { char diagLine[512]; std::snprintf(diagLine, sizeof diagLine, "[RouteCheck] request=%llu the route is clear of %zu obstacles (%zu gates)\n",
                             static_cast<unsigned long long>(job->context.serial), request.obstacles.size(), gates.size()); std::cerr << diagLine << std::flush; }
        }

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
} // namespace

void SpaceState::sendDockArrivalRequest(bool engage, const std::string& dockModuleId)
{
    if (!m_client)
        return;
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::RequestDockArrival;
    command.index = engage ? 1 : 0;
    command.target = dockModuleId;   // the dock (module id) whose dispatcher handles it; empty = the nearest
    msg.payload = command;
    m_client->sendMessage(msg);
    std::cerr << "[DockDispatcher] " << (engage ? "docking request sent to the dock dispatcher" : "docking dropped: request cancelled")
              << std::endl;
}

template <class Frame>
bool SpaceState::updateDockingDispatcherGate(Frame& f)
{
    // The dock dispatcher learns of the docking when it is engaged (with or
    // without an autopilot); the real planner and autopilot fly the route as
    // before (through the blue lane to the dock). The station captures the
    // ship on the approach axis once the dispatcher clears its entry; until
    // then it waits at the end of its route. Never takes the control stream.
    using game::docking::traffic::DockTrafficPhase;
    auto& pending = f.pending;
    // every hex dock has a dispatcher (GameSimulation dock sites): the gate runs for any HubHexDock target
    bool hexDockTarget = false;
    for (const auto& [oid, object] : m_client->world().objects())
        if (object.type == ObjectType::HubHexDock && object.hubAttachment.moduleId == pending.target.stableObjectId)
            hexDockTarget = true;
    if (!hexDockTarget)
        return true;
    if (m_dockArrivalSentSerial != pending.serial)
    {
        m_dockArrivalSentSerial = pending.serial;
        m_dockArrivalSentServerSeconds = f.authoritativeServerSeconds;
        m_dockGateLastPhase = 0xff;
        sendDockArrivalRequest(true, pending.target.stableObjectId);
    }
    const auto& dock = f.player.dockDispatcher;
    const auto phase = static_cast<DockTrafficPhase>(dock.phase);
    if (dock.active && dock.phase != m_dockGateLastPhase)
    {
        m_dockGateLastPhase = dock.phase;
        std::cerr << "[DockDispatcher] " << game::docking::traffic::dockTrafficPhaseName(phase) << " pad=" << dock.padId
                  << " queue " << dock.queuePosition << "/" << dock.queueLength << " ahead=" << dock.movementsAhead
                  << " wait~" << dock.estimatedWaitSeconds << " s" << std::endl;
    }
    // Docked: the docking is complete - the request ends here (Ctrl+L asks
    // for the departure; nothing replans a docking afterwards).
    if (dock.active && phase == DockTrafficPhase::Docked)
    {
        std::cerr << "[DockDispatcher] docked on " << dock.padId << ": docking request complete" << std::endl;
        f.requests.clear();
        return false;
    }
    return true;
}

void SpaceState::updateDockingAdvisory()
{
    core::hitch::mark("dock_advisory_enter");
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

    // DEV experiment (author 2026-11-23: «если разрешить маршевый [в свободном вращении], то по сути получим почти
    // обычного ньютона»): ELITE_AUTOPILOT_NEWTONIAN_ROUTES=1 - an Assisted ship flies its automatic route in the
    // NEWTONIAN law (that law's planner capability - turns on the main drive - and autopilot), and is handed back to
    // Assisted when the request is over. The same input path as Ctrl+F10.
    {
        static const bool newtonianRoutes = [] {
            const char* v = std::getenv("ELITE_AUTOPILOT_NEWTONIAN_ROUTES");
            return v && v[0] == '1';
        }();
        if (newtonianRoutes)
        {
            const auto it = m_client->world().ships().find(m_playerId.value);
            if (it != m_client->world().ships().end())
            {
                const auto law = it->second.transform.motion.localControlLaw;
                const bool routeActive = pending.valid() && pending.mode == DockingRouteRequest::Mode::Automatic;
                const auto sendLaw = [&](game::navigation::LocalFlightControlLaw want) {
                    ShipControlState c = m_clientAutopilotControlActive ? m_clientAutopilotControl : ShipControlState {};
                    c.localControlLawCommandValid = true;
                    c.requestedLocalControlLaw = want;
                    m_client->submitInput(c);
                };
                if (routeActive && !m_routeLawSwitched && law == game::navigation::LocalFlightControlLaw::Assisted)
                {
                    sendLaw(game::navigation::LocalFlightControlLaw::Newtonian);
                    m_routeLawSwitched = true;
                    std::cerr << "[RouteLaw] request " << pending.serial << ": Assisted -> Newtonian for the route" << std::endl;
                }
                else if (!routeActive && m_routeLawSwitched)
                {
                    if (law == game::navigation::LocalFlightControlLaw::Newtonian)
                        sendLaw(game::navigation::LocalFlightControlLaw::Assisted);
                    m_routeLawSwitched = false;
                    std::cerr << "[RouteLaw] route over: back to Assisted" << std::endl;
                }
            }
        }
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
        m_dockLiftClear = {};
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
        m_dockLiftClear = {};
        m_dockSafeCourse = {};
    };

    if (!pending.valid())
    {
        if (m_dockArrivalSentSerial != 0)
        {
            sendDockArrivalRequest(false);
            m_dockArrivalSentSerial = 0;
        }
        // the body autopilot / the route own the control stream themselves
        const bool otherPilot = m_bodyAutopilotOwnsControl || m_clientRouteOwnsControl || m_capsuleRescue.ownsControl;
        if (m_clientDockingPhase != ClientDockingPhase::Idle ||
            (m_clientAutopilotControlActive && !otherPilot))
        {
            std::cerr
                << "[DockClient] phase=idle reason=no-pending-request"
                << std::endl;
        }
        if (!otherPilot)
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
        // near the hub in normal flight: the server takes the ship into the hub's frame where it is, the request waits for
        // it (live 2026-11-21: the docking refused silently from normal flight - the frame only came with a hub route)
        if (motion.systemId == pending.target.systemId && m_hubFrameRequestSerial != pending.serial)
        {
            m_hubFrameRequestSerial = pending.serial;
            m_hubFrameRequestS = glfwGetTime();
            game::network::ClientMessage msg;
            ClientShipCommand command;
            command.type = ClientShipCommand::EnterHubFrame;
            command.target = pending.target.stableObjectId;
            msg.payload = command;
            m_client->sendMessage(msg);
            std::cerr << "[DockClient] request=" << pending.serial << " asks the hub frame (normal flight)" << std::endl;
            return;
        }
        if (m_hubFrameRequestSerial == pending.serial && glfwGetTime() - m_hubFrameRequestS < 3.0)
            return;   // the server's answer is on its way
        m_pilotNotice = "СТЫКОВКА: хаб далеко (> 30 км) - сначала маршрут к хабу";
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
    // a hub city pad ("pad:<address>" over the city frame module, author 2026-11-21): the planner goal is its approach
    // point - over the pad at the transit height (over the tallest building by 80 m), approached from where the ship is
    if (pending.target.semanticAnchorId.rfind("pad:", 0) == 0)
    {
        definition = nullptr;
        runtime = nullptr;
        if (m_padAnchor.serial != pending.serial)
        {
            m_padAnchor = {};
            const std::string address = pending.target.semanticAnchorId.substr(4);
            const auto* plan = game::navigation::HubPlanCatalog::forFrameModule(pending.target.stableObjectId);
            const game::navigation::HubPlanModule* building = nullptr;
            if (plan)
                for (const auto& m : plan->modules())
                    if (m.address == address && m.pad)
                        building = &m;
            const auto* city = padRouteCityObject(pending.target.stableObjectId);
            const auto shipIt = m_client->world().ships().find(m_playerId.value);
            if (plan && building && city && shipIt != m_client->world().ships().end())
            {
                const glm::dvec3 out = building->under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
                double tallest = 0.0;
                for (const auto& b : plan->blocks())
                    tallest = std::max(tallest, building->under ? b.underDepthMeters + 30.0 : b.topHeightMeters);
                const double transit = std::max({std::min(380.0, tallest + 80.0), std::abs(building->padPosition.y) + 60.0, game::navigation::traffic::laneTubeCeilingMeters(game::navigation::traffic::HubLaneCatalog::forFrameModule(pending.target.stableObjectId), building->under) + 40.0});
                // worked out in the plan's strip metres, then bent into the city frame (a ring hub, HubRingBend.h)
                const auto& ring = plan->ring();
                const glm::dvec3 hoverStrip = building->padPosition + out * (transit - glm::dot(building->padPosition, out));
                glm::dvec3 from = ring.toStrip(padRouteCityLocal(*city, shipIt->second)) - hoverStrip;
                from -= out * glm::dot(from, out);
                const glm::dvec3 forwardStrip = glm::length(from) > 1.0 ? glm::normalize(from) : glm::dvec3(0.0, 0.0, 1.0);
                const glm::dmat3 axes = ring.basisAt(hoverStrip.x);
                const glm::dvec3 hover = ring.toFrame(hoverStrip);
                const glm::dvec3 forward = axes * forwardStrip;
                auto& d = m_padAnchor.definition;
                d.id = pending.target.semanticAnchorId;
                d.hubModuleId = pending.target.stableObjectId;
                d.displayName = "pad " + address;
                d.kind = HubSemanticAnchorKind::DockingPort;   // flown as a port approach; the pad service lands it
                d.localPositionMeters = hover;
                d.localForward = forward;                       // the route arrives along -forward (toward the pad)
                d.localUp = axes * out;
                // the "opening" sets the route-following corridor (its half margin): 400 m made a 192 m corridor in
                // which the autopilot never steered back to the line (19 m dead band, 20-72 m off the route among the
                // buildings, live 2026-11-22); 70 m -> ~20-25 m, the correction from ~2 m
                d.extentMeters = glm::dvec3(70.0);
                d.requiredClearanceMeters = 5.0;
                d.maxEntrySpeedMps = 30.0;
                d.captureDepthMeters = 1.0;
                m_padAnchor.runtime.hubModuleId = d.hubModuleId;
                m_padAnchor.runtime.anchorId = d.id;
                m_padAnchor.runtime.operational = DockingOperationalState::Online;
                m_padAnchor.runtime.occupancy = DockingOccupancyState::Free;
                m_padAnchor.runtime.access = DockingAccessState::Allowed;
                m_padAnchor.address = address;
                m_padAnchor.hoverCity = hover;
                m_padAnchor.forwardCity = forward;
                m_padAnchor.upCity = axes * out;
                m_padAnchor.valid = true;
                // a TURNING ring (author 2026-11-25: «доводит корабль до какой-то точки уже внутри кольца, выравнивает ...
                // вращение вместе с кольцом»): the route's goal is the ring's SYNC GATE on the spin axis - still in space
                // and in the turning city; arriving along the axis into the ring, the hull rolling with it (the live port
                // re-resolved with the city's spin: the terminal roll rate = the ring's). The way on is the city's frame.
                if (ring.valid && ring.periodSeconds > 0.0 && ring.hasSyncGate)
                {
                    const glm::dvec3 into = glm::dvec3(0.0, 0.0, ring.syncGate.z < 0.0 ? 1.0 : -1.0);   // toward the ring
                    d.localPositionMeters = ring.syncGate;
                    d.localForward = -into;   // the route arrives along -forward
                    d.localUp = glm::dvec3(0.0, 1.0, 0.0);
                    d.maxEntrySpeedMps = 2.0;
                    m_padAnchor.hoverCity = ring.syncGate;
                    m_padAnchor.forwardCity = -into;
                    m_padAnchor.upCity = d.localUp;
                    m_padAnchor.ringGate = true;
                    std::cerr << "[RingSync] request=" << pending.serial << " pad " << address << " on a turning ring (period "
                              << ring.periodSeconds << " s): the route goes to the sync gate on the axis (" << ring.syncGate.x
                              << "," << ring.syncGate.y << "," << ring.syncGate.z << ") city frame" << std::endl;
                }
                std::cerr << "[PadRoute] request=" << pending.serial << " pad " << address << ": approach point (" << hover.x << ","
                          << hover.y << "," << hover.z << ") city frame, transit " << transit << " m" << std::endl;
            }
            m_padAnchor.serial = pending.serial;
        }
        if (m_padAnchor.valid)
        {
            definition = &m_padAnchor.definition;
            runtime = &m_padAnchor.runtime;
        }
    }
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
        // Both laws: a capability loss or failed attempt stabilises the
        // craft and plans again from its actual state and authority.
        constexpr int MaxNewtonianRouteRecoveries = 5;
        if (!pending.valid() ||
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
        submitVirtualPilotControl(ShipControlState {}, 0x43u);   // the recovery coast: the reflex over it (NAV-8)
        m_dockAdviceJob.reset();
        m_dockLateEntry = {};
        m_dockSafeCourse = {};
        m_clientDockingPhase = ClientDockingPhase::Stabilizing;
        m_clientDockingSettledSinceServerSeconds = -1.0;
        // Assisted: a damaged craft does not try to re-enter the route at
        // its old speed (hot starts beyond the derated authority were
        // infeasible or flown with large excursions). It brakes to rest on
        // its current straight course and plans from rest, like a request
        // made after the damage. Newtonian: coasts and replans (unchanged).
        if (controlLaw == game::navigation::LocalFlightControlLaw::Assisted)
        {
            m_assistedRecoveryStopBeforePlanning = true;
            m_assistedAlignDone = false;
            m_assistedAlignActive = false;
        }
    };
    if (pending.valid() &&
        controlLaw == game::navigation::LocalFlightControlLaw::Newtonian)
    {
        struct
        {
            decltype(pending)& pending;
            decltype(releaseClientAutopilot)& releaseClientAutopilot;
            decltype(player)& player;
            decltype(motion)& motion;
            decltype(effectivePhysics)& effectivePhysics;
        } g {pending, releaseClientAutopilot, player, motion, effectivePhysics};
        if (!checkDockingNewtonianAuthority(g))
            return;
    }

    game::ship::telemetry::ShipControlAuthorityTelemetry
        liveAssistedAuthority;
    if (pending.valid() &&
        controlLaw == game::navigation::LocalFlightControlLaw::Assisted)
    {
        struct
        {
            decltype(pending)& pending;
            decltype(fail)& fail;
            decltype(player)& player;
            decltype(effectivePhysics)& effectivePhysics;
            decltype(liveAssistedAuthority)& liveAssistedAuthority;
        } g {pending, fail, player, effectivePhysics, liveAssistedAuthority};
        if (!checkDockingAssistedAuthority(g))
            return;
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
        agent.assistedTargetSpeedMps = std::max(0.0, static_cast<double>(player.transform.motion.targetForwardSpeedMps));
        // the TRAFFIC RULE: a ship ahead on the way (going our way, within our hull's corridor) sets a ceiling - its
        // speed + the closing the gap still allows (gap 50 m + 2 s of our speed); oncoming / crossing ships are the
        // evasion layer's (a lane ship was rear-ended at 65 m/s, live 2026-11-22)
        {
            // everything in the route's map frame: the WORLD velocities carry the hub's orbit (30 km/s - "we fly
            // 35027 m/s", the ceiling went to 0 and a lane ship was rear-ended at 63 m/s, suite v2)
            const glm::dvec3 myW = world::coordinates::fullMeters(player.renderTransform.worldPosition);
            const glm::dvec3 myWorldV = player.transform.motion.worldVelocityMps;
            const glm::dvec3 myV = motion.localVelocityMps;
            const double mySpeed = glm::length(myV);
            if (mySpeed > 5.0)
            {
                const glm::dvec3 fwd = myV / mySpeed;
                const double myRadius = std::max(10.0, 0.5 * hull.lengthMeters);
                double ceiling = 0.0;
                for (const auto& [sid, other] : m_client->world().ships())
                {
                    if (sid == m_playerId.value)
                        continue;
                    const glm::dvec3 rel =
                        frame.worldToLocalVector(world::coordinates::fullMeters(other.renderTransform.worldPosition) - myW);
                    const double along = glm::dot(rel, fwd);
                    if (along <= 0.0 || along > 2000.0)
                        continue;
                    const double lateral = glm::length(rel - fwd * along);
                    if (lateral > myRadius + 40.0)
                        continue;
                    const glm::dvec3 ov = myV + frame.worldToLocalVector(other.transform.motion.worldVelocityMps - myWorldV);
                    const double lead = glm::dot(ov, fwd);
                    if (glm::length(ov) < 1.0 || lead < 0.5 * glm::length(ov))
                    {
                        // GIVE WAY to a ship crossing / coming at us: its closest approach within 8 s nearer than the
                        // hulls + 15 m - slow down and let it pass (the reflex layer's first rule; braking is the
                        // smoothest, most predictable evasion in a lane)
                        const glm::dvec3 rv = ov - myV;
                        const double rv2 = glm::dot(rv, rv);
                        if (rv2 > 1.0e-6)
                        {
                            const double tCpa = -glm::dot(rel, rv) / rv2;
                            const double miss = glm::length(rel + rv * tCpa);
                            const double otherRadius = other.descriptor ? 0.5 * other.descriptor->logicalDimensions().length : 20.0;
                            if (tCpa > 0.0 && tCpa < 8.0 && miss < myRadius + otherRadius + 15.0)
                            {
                                const double cap = mySpeed * std::clamp((tCpa - 1.0) / 8.0, 0.25, 0.7);
                                ceiling = ceiling > 0.0 ? std::min(ceiling, cap) : std::max(cap, 0.01);
                                static double giveLog = 0.0;
                                if (glfwGetTime() - giveLog > 2.0)
                                {
                                    giveLog = glfwGetTime();
                                    std::cerr << "[TrafficRule] give way: ship " << sid << " crosses in " << tCpa << " s, "
                                              << miss << " m off - down to " << cap << " m/s" << std::endl;
                                }
                            }
                        }
                        continue;   // not going our way
                    }
                    const double gap = 50.0 + myRadius + 2.0 * mySpeed;
                    const double cap = std::max(0.0, lead + 0.5 * (along - gap));
                    ceiling = ceiling > 0.0 ? std::min(ceiling, cap) : std::max(cap, 0.01);
                }
                agent.trafficSpeedCeilingMps = ceiling;
                static double logAt = 0.0;
                if (ceiling > 0.0 && glfwGetTime() - logAt > 3.0)
                {
                    logAt = glfwGetTime();
                    std::cerr << "[TrafficRule] a ship ahead on our way: speed ceiling " << ceiling << " m/s (we fly "
                              << mySpeed << ")" << std::endl;
                }
            }
        }
        return agent;
    };

    // The Assisted working thrust axis (the server's; body +X right, +Y up,
    // -Z nose) expressed in the map frame of this agent's hull.
    const auto workingThrustAxisMap = [&](const FollowerAgent& agent)
    {
        const glm::dvec3 axis = effectivePhysics.assistedWorkingThrustAxisBody;
        const glm::dvec3 map = axis.x * agent.rightMap +
            axis.y * agent.upMap - axis.z * agent.forwardMap;
        const double length = glm::length(map);
        return std::isfinite(length) && length > 1.0e-9
            ? glm::dvec3(map / length) : agent.forwardMap;
    };

    const auto submitAutopilotControl =
        [&](const ShipControlState& control)
        {
            // the reflex in EVERY phase of the virtual pilot: the route's own (executing) or the world one
            m_clientAutopilotControl =
                m_routeReflexApplied
                    ? control
                    : applyManualCollisionAssist(control, m_client->lastSimulationMetadata().universeTimeSeconds, true);
            m_routeReflexApplied = false;
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
        m_assistedRecoveryStopBeforePlanning = false;
        m_assistedAlignDone = false;
        m_assistedAlignActive = false;
        m_assistedAlignCourseValid = false;
        m_lastDockingRequestTraceSerial = pending.serial;
        m_dockLateEntry = {};
        m_dockAssistedSafeRecovery = {};
        m_dockLiftClear = {};
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

    // The per-update locals and helpers the phase functions below share
    // (references; the helper lambdas keep their closure types).
    struct
    {
        decltype(workspace)& workspace;
        decltype(requests)& requests;
        decltype(pending)& pending;
        decltype(guidance)& guidance;
        decltype(eraseVisibleRoute)& eraseVisibleRoute;
        decltype(releaseClientAutopilot)& releaseClientAutopilot;
        decltype(fail)& fail;
        decltype(automatic)& automatic;
        decltype(playerIt)& playerIt;
        decltype(player)& player;
        decltype(transform)& transform;
        decltype(motion)& motion;
        decltype(definition)& definition;
        decltype(runtime)& runtime;
        decltype(dim)& dim;
        decltype(hull)& hull;
        decltype(fit)& fit;
        decltype(plannerSlotState)& plannerSlotState;
        decltype(plannerCapability)& plannerCapability;
        decltype(effectivePhysics)& effectivePhysics;
        decltype(motherPhysics)& motherPhysics;
        decltype(controlLaw)& controlLaw;
        decltype(recoverOrFail)& recoverOrFail;
        decltype(liveAssistedAuthority)& liveAssistedAuthority;
        decltype(frame)& frame;
        decltype(makeAgent)& makeAgent;
        decltype(workingThrustAxisMap)& workingThrustAxisMap;
        decltype(submitAutopilotControl)& submitAutopilotControl;
        decltype(coastForPlanningControl)& coastForPlanningControl;
        decltype(stabilizeAngularForPlanningControl)& stabilizeAngularForPlanningControl;
        decltype(manualStopControl)& manualStopControl;
        decltype(legacyNewtonianSafeCourseControl)& legacyNewtonianSafeCourseControl;
        decltype(fixedDt)& fixedDt;
        decltype(authoritativeServerSeconds)& authoritativeServerSeconds;
        decltype(universeTimeSeconds)& universeTimeSeconds;
        decltype(currentTunnelRollPhase)& currentTunnelRollPhase;
    } f {workspace, requests, pending, guidance, eraseVisibleRoute, releaseClientAutopilot, fail, automatic, playerIt, player, transform, motion, definition, runtime, dim, hull, fit, plannerSlotState, plannerCapability, effectivePhysics, motherPhysics, controlLaw, recoverOrFail, liveAssistedAuthority, frame, makeAgent, workingThrustAxisMap, submitAutopilotControl, coastForPlanningControl, stabilizeAngularForPlanningControl, manualStopControl, legacyNewtonianSafeCourseControl, fixedDt, authoritativeServerSeconds, universeTimeSeconds, currentTunnelRollPhase};

    if (!updateDockingDispatcherGate(f))
        return;

    // LIFT CLEAR (live 2026-11-21): the ship touches boxes (stands on a pad, at a wall) - before any route it rises
    // straight off them on its thrusters, the hull attitude held (no nose-up turn: the stern hit the deck), at most
    // 5 m/s, until it is the inflated clearance + a margin away; then it settles and the route is planned from there
    if (m_dockLiftClear.active && m_dockLiftClear.serial == pending.serial)
    {
        const auto agent = makeAgent();
        const double risen = glm::dot(agent.positionMapMeters - m_dockLiftClear.startMap, m_dockLiftClear.direction);
        if (risen >= m_dockLiftClear.meters)
        {
            std::cerr << "[DockLiftClear] request=" << pending.serial << " clear after " << risen << " m - planning" << std::endl;
            m_pilotNotice = "СТЫКОВКА: корабль поднят над постройками - расчёт маршрута";
            m_dockLiftClear = {};
            m_clientDockingPhase = ClientDockingPhase::Stabilizing;
            m_clientDockingSettledSinceServerSeconds = -1.0;
            submitAutopilotControl(manualStopControl());
            return;
        }
        // slow at the start and near the end (the legs leave the deck gently)
        const double left = m_dockLiftClear.meters - risen;
        const double speed = std::clamp(std::min(0.5 + risen * 0.4, left * 0.3), 0.5, 5.0);
        game::navigation::autopilot::AutopilotControlRequest lift;
        lift.desiredVelocityMapMps = m_dockLiftClear.direction * speed;
        lift.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
        lift.desiredForwardMap = m_dockLiftClear.forward;
        lift.desiredUpMap = m_dockLiftClear.up;
        lift.actualVelocityMapMps = agent.velocityMapMetersPerSecond;
        lift.forwardMap = agent.forwardMap;
        lift.rightMap = agent.rightMap;
        lift.upMap = agent.upMap;
        lift.pitchRateRadPerSec = agent.pitchRateRadPerSec;
        lift.yawRateRadPerSec = agent.yawRateRadPerSec;
        lift.rollRateRadPerSec = agent.rollRateRadPerSec;
        lift.stopRequested = false;
        lift.deltaSeconds = std::max(1.0e-4, m_client->serverFixedStepSeconds());
        submitAutopilotControl(
            controlLaw == game::navigation::LocalFlightControlLaw::Assisted
                ? m_clientDockingAssistedStabilizePilotController.update(lift, effectivePhysics)
                : m_clientDockingNewtonianStabilizePilotController.update(lift, effectivePhysics));
        static double lastTrace = -1.0e9;
        if (authoritativeServerSeconds - lastTrace >= 2.0)
        {
            lastTrace = authoritativeServerSeconds;
            std::cerr << "[DockLiftClear] request=" << pending.serial << " risen " << risen << " / " << m_dockLiftClear.meters
                      << " m at " << glm::length(agent.velocityMapMetersPerSecond) << " m/s" << std::endl;
        }
        return;
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::SecuringCourse)
    {
        updateDockingSecuringCourse(f);
        return;
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::Stabilizing)
    {
        updateDockingStabilizing(f);
        return;
    }

    core::hitch::mark("dock_advisory_setup");
    if (m_clientDockingPhase ==
        ClientDockingPhase::Planning)
    {
        if (!updateDockingPlanning(f))
            return;
        core::hitch::mark("dock_planning");
    }

    if (m_clientDockingPhase ==
        ClientDockingPhase::Executing)
    {
        if (!updateDockingExecuting(f))
            return;
    }

    publishDockingAdvisoryPresentation(f);
}

// updateDockingAdvisory: the Newtonian pre-planner gate - installed RCS authority
// published and enough RCS gas to start. false = blocked / replanning, stop.
template <class Gate>
bool SpaceState::checkDockingNewtonianAuthority(Gate& g)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = g.pending;
    auto& releaseClientAutopilot = g.releaseClientAutopilot;
    auto& player = g.player;
    auto& motion = g.motion;
    auto& effectivePhysics = g.effectivePhysics;

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
            submitVirtualPilotControl(ShipControlState {}, 0x44u);   // the authority-lost coast (NAV-8)
            m_clientDockingPhase = ClientDockingPhase::Stabilizing;
            m_clientDockingSettledSinceServerSeconds = -1.0;
            return false;
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
        return false;
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
            return false;
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

    return true;
}

// updateDockingAdvisory: the Assisted pre-planner gate - the installed motor
// matrix (server ShipCore allocator) must be published and sufficient;
// adopts the motor-constrained envelope. false = blocked / failed, stop.
template <class Gate>
bool SpaceState::checkDockingAssistedAuthority(Gate& g)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = g.pending;
    auto& fail = g.fail;
    auto& player = g.player;
    auto& effectivePhysics = g.effectivePhysics;
    auto& liveAssistedAuthority = g.liveAssistedAuthority;

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
            return false;
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
        return false;
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
        return false;
    }

    return true;
}

// updateDockingAdvisory / Stabilizing: compile the semantic traffic route (BLUE
// mandatory transit) of the Hub hex dock front gate into compiledTrafficRoute.
// false = the request failed (fail() called), stop this update.
template <class Frame, class Locals>
bool SpaceState::compileDockingTrafficRoute(Frame& f, Locals& l)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& fail = f.fail;
    auto& snapshot = l.snapshot;
    auto& localPort = l.localPort;
    auto& assisted = l.assisted;
    auto& request = l.request;
    auto& compiledTrafficRoute = l.compiledTrafficRoute;

    game::navigation::traffic::TrafficRouteGraphCatalog trafficCatalog;
    if (!trafficCatalog.load(
            "assets/data/navigation/hub_traffic_route_graph.json"))
    {
        fail("traffic graph unavailable: " + trafficCatalog.failure());
        return false;
    }

    game::navigation::NavigationVolumeCatalog volumeCatalog;
    if (!volumeCatalog.load(
            "assets/data/navigation/hub_navigation_volumes.json"))
    {
        fail("navigation volume catalog unavailable");
        return false;
    }

    const auto trafficGraph = trafficCatalog.graph();

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

    const auto trafficRoute = trafficGraph->resolve(
        "cylinder_b.entry_front",
        "hex_dock_a.dock_front"
    );
    if (!trafficRoute.valid)
    {
        fail(
            "assigned blue traffic route is invalid: " +
            trafficRoute.failure
        );
        return false;
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
        return false;
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
        return false;
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
        return false;
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

    return true;
}

// updateDockingAdvisory / Stabilizing (automatic, not an entry rebind): select and
// acquire the preplanning safety course the craft flies while the route is
// planned (Assisted committed recovery / Newtonian legacy bridge).
// false = stop this update.
template <class Frame, class Locals>
bool SpaceState::secureDockingCourseWhilePlanning(Frame& f, Locals& l)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& definition = f.definition;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;
    auto& manualStopControl = f.manualStopControl;
    auto& legacyNewtonianSafeCourseControl = f.legacyNewtonianSafeCourseControl;
    auto& authoritativeServerSeconds = f.authoritativeServerSeconds;
    auto& assisted = l.assisted;
    auto& request = l.request;
    auto& startVelocityMps = l.startVelocityMps;
    auto& hullForwardLocal = l.hullForwardLocal;
    auto& planningWhileSecuringCourse = l.planningWhileSecuringCourse;
    auto& makeAgent = f.makeAgent;

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

        if (safetyResult.message.rfind("in contact", 0) == 0)
        {
            const auto agent = makeAgent();
            m_dockLiftClear = {};
            m_dockLiftClear.active = true;
            m_dockLiftClear.serial = pending.serial;
            m_dockLiftClear.direction = safetyResult.safeCourse;
            m_dockLiftClear.startMap = agent.positionMapMeters;
            m_dockLiftClear.forward = agent.forwardMap;
            m_dockLiftClear.up = agent.upMap;
            m_dockLiftClear.meters = safety.hullRadiusMeters + safety.additionalClearanceMeters + 20.0;
            m_pilotNotice = "СТЫКОВКА: взлёт - подъём над постройками";
            std::cerr << "[DockLiftClear] request=" << pending.serial << " " << safetyResult.message << ": rise "
                      << m_dockLiftClear.meters << " m along (" << safetyResult.safeCourse.x << "," << safetyResult.safeCourse.y
                      << "," << safetyResult.safeCourse.z << ")" << std::endl;
            return false;
        }

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
                m_pilotNotice = "СТЫКОВКА: нет безопасного курса отсюда (рядом постройки / площадка) - отведите корабль на 100-200 м и повторите";   // the reason in the terminal (author 2026-11-21: «нет ни расчёта маршрута, ни причин»)
                submitAutopilotControl(manualStopControl());
                return false;
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
                m_pilotNotice = "СТЫКОВКА: нет безопасного курса отсюда (рядом постройки / площадка) - отведите корабль на 100-200 м и повторите";
                submitAutopilotControl(
                    manualStopControl()
                );
                return false;
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

    return true;
}

// updateDockingAdvisory / Stabilizing: bring the craft to the state the planning
// snapshot needs (Assisted emergency-axis ALIGN in place; recovery stop; manual
// full stop; automatic angular settle). false = not settled yet, stop this update.
template <class Frame, class Locals>
bool SpaceState::settleDockingCraftBeforePlanning(Frame& f, Locals& l)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& automatic = f.automatic;
    auto& motion = f.motion;
    auto& effectivePhysics = f.effectivePhysics;
    auto& controlLaw = f.controlLaw;
    auto& makeAgent = f.makeAgent;
    auto& workingThrustAxisMap = f.workingThrustAxisMap;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;
    auto& stabilizeAngularForPlanningControl = f.stabilizeAngularForPlanningControl;
    auto& manualStopControl = f.manualStopControl;
    auto& authoritativeServerSeconds = f.authoritativeServerSeconds;
    auto& relativeSpeedMps = l.relativeSpeedMps;
    auto& angularSpeedRadPerSec = l.angularSpeedRadPerSec;

    // ALIGN (Assisted, off-nose emergency working axis, at rest): turn
    // the hull in place until the working thrust axis lies on the
    // course the nose held when the ship came to rest; the route is then
    // planned from that pose with the working axis as its first course,
    // so it never starts by thrusting sideways while the hull turns.
    constexpr double AlignAtRestSpeedMps = 1.0;
    // Without real turning authority (e.g. aft L + fore R lost:
    // 0.0007 rad/s2) a turn in place cannot finish; skip it and let the
    // request report the missing attitude control.
    constexpr double AlignMinAngularRadPerSec2 = 0.01;
    const double alignAngularAuthorityRadPerSec2 =
        game::navigation::makeNavigationControlCapability(
            effectivePhysics, controlLaw).angularScalarRadPerSec2;
    constexpr double AlignCourseSpeedMps = 5.0;
    if (relativeSpeedMps > AlignCourseSpeedMps)
    {
        m_assistedAlignCourseValid = true;
        m_assistedAlignCourseMap = motion.localVelocityMps / relativeSpeedMps;
    }
    if (automatic && !m_assistedAlignDone &&
        controlLaw == game::navigation::LocalFlightControlLaw::Assisted &&
        effectivePhysics.assistedWorkingThrustAxisEmergency &&
        relativeSpeedMps <= AlignAtRestSpeedMps &&
        alignAngularAuthorityRadPerSec2 >= AlignMinAngularRadPerSec2)
    {
        constexpr double AlignToleranceRad = 0.025;
        constexpr double AlignStoppedAngularRadPerSec = 0.01;
        constexpr double AlignSettledSeconds = 0.50;
        constexpr double AlignTimeoutSeconds = 90.0;
        const auto agent = makeAgent();
        const glm::dvec3 workingMap = workingThrustAxisMap(agent);
        if (!m_assistedAlignActive)
        {
            m_assistedAlignActive = true;
            // The course flown before rest (the nose is off that course
            // on an emergency axis); a ship that was at rest: its nose.
            m_assistedAlignTargetMap = m_assistedAlignCourseValid
                ? m_assistedAlignCourseMap : agent.forwardMap;
            m_assistedAlignStartServerSeconds = authoritativeServerSeconds;
            m_clientDockingSettledSinceServerSeconds = -1.0;
            std::cerr << "[DockAlign] request=" << pending.serial
                      << " phase=begin offset_deg="
                      << glm::degrees(std::acos(std::clamp(glm::dot(
                             workingMap, m_assistedAlignTargetMap), -1.0, 1.0)))
                      << std::endl;
        }
        const double errorRad = std::acos(std::clamp(
            glm::dot(workingMap, m_assistedAlignTargetMap), -1.0, 1.0));
        const bool timedOut = authoritativeServerSeconds -
                m_assistedAlignStartServerSeconds > AlignTimeoutSeconds;
        if (!timedOut)
        {
            // Rotate the whole hull frame by the shortest rotation that
            // carries the working axis onto the target course.
            const glm::dquat turn =
                glm::rotation(workingMap, m_assistedAlignTargetMap);
            game::navigation::autopilot::AutopilotControlRequest align;
            align.desiredVelocityMapMps = glm::dvec3(0.0);
            align.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
            align.desiredForwardMap = glm::normalize(turn * agent.forwardMap);
            align.desiredUpMap = glm::normalize(turn * agent.upMap);
            align.terminalAttitudeHold = true;
            align.desiredCourseAngularRateMapRadPerSec = glm::dvec3(0.0);
            align.desiredRollRateRadPerSec = 0.0;
            align.actualVelocityMapMps = agent.velocityMapMetersPerSecond;
            align.forwardMap = agent.forwardMap;
            align.rightMap = agent.rightMap;
            align.upMap = agent.upMap;
            align.pitchRateRadPerSec = agent.pitchRateRadPerSec;
            align.yawRateRadPerSec = agent.yawRateRadPerSec;
            align.rollRateRadPerSec = agent.rollRateRadPerSec;
            align.stopRequested = false;
            align.deltaSeconds =
                std::max(1.0e-4, m_client->serverFixedStepSeconds());
            submitAutopilotControl(
                m_clientDockingAssistedStabilizePilotController.update(
                    align, effectivePhysics));
            if (errorRad > AlignToleranceRad ||
                angularSpeedRadPerSec > AlignStoppedAngularRadPerSec)
            {
                m_clientDockingSettledSinceServerSeconds = -1.0;
                return false;
            }
            if (m_clientDockingSettledSinceServerSeconds < 0.0)
            {
                m_clientDockingSettledSinceServerSeconds =
                    authoritativeServerSeconds;
                return false;
            }
            if (authoritativeServerSeconds -
                    m_clientDockingSettledSinceServerSeconds <
                AlignSettledSeconds)
                return false;
        }
        std::cerr << "[DockAlign] request=" << pending.serial
                  << " phase=" << (timedOut ? "timeout" : "aligned")
                  << " error_deg=" << glm::degrees(errorRad)
                  << " seconds=" << (authoritativeServerSeconds -
                                     m_assistedAlignStartServerSeconds)
                  << std::endl;
        m_assistedAlignActive = false;
        m_assistedAlignDone = true;
        m_clientDockingSettledSinceServerSeconds = -1.0;
        submitAutopilotControl(stabilizeAngularForPlanningControl());
        return false;
    }

    // MANUAL GUIDANCE CONTRACT:
    // Manual and Automatic consume the SAME route topology/geometry.
    // Manual differs only by reaching a real zero translational/angular
    // state before route calculation.
    if (automatic && m_assistedRecoveryStopBeforePlanning)
    {
        // Assisted recovery after a capability loss: brake to rest on
        // the current course (shorter than the ballistic ray the safe
        // course already cleared), damp the hull, then plan from rest.
        constexpr double RecoveryStoppedSpeedMps = 1.0;
        constexpr double RecoveryStoppedAngularRadPerSec = 0.01;
        constexpr double RecoverySettledSeconds = 0.50;
        // Translation first; once at rest only the hull rates are damped
        // (a stop command on an asymmetric plant must not keep firing
        // the surviving motor and spin the hull).
        const bool atRest = relativeSpeedMps <= RecoveryStoppedSpeedMps;
        submitAutopilotControl(atRest
            ? stabilizeAngularForPlanningControl()
            : manualStopControl());
        if (!atRest ||
            angularSpeedRadPerSec > RecoveryStoppedAngularRadPerSec)
        {
            m_clientDockingSettledSinceServerSeconds = -1.0;
            return false;
        }
        if (m_clientDockingSettledSinceServerSeconds < 0.0)
        {
            m_clientDockingSettledSinceServerSeconds =
                authoritativeServerSeconds;
            return false;
        }
        if (authoritativeServerSeconds -
                m_clientDockingSettledSinceServerSeconds <
            RecoverySettledSeconds)
            return false;
        submitAutopilotControl(coastForPlanningControl());
    }
    else if (!automatic)
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
            return false;
        }

        if (m_clientDockingSettledSinceServerSeconds < 0.0)
        {
            m_clientDockingSettledSinceServerSeconds =
                authoritativeServerSeconds;
            return false;
        }

        if (authoritativeServerSeconds -
                m_clientDockingSettledSinceServerSeconds <
            ManualSettledSeconds)
        {
            return false;
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
            return false;
        }

        if (m_clientDockingSettledSinceServerSeconds < 0.0)
        {
            m_clientDockingSettledSinceServerSeconds =
                authoritativeServerSeconds;
            return false;
        }

        if (authoritativeServerSeconds -
                m_clientDockingSettledSinceServerSeconds <
            AutomaticAngularSettledSeconds)
        {
            return false;
        }

        // Snapshot below now observes the contract state: no requested
        // linear acceleration and effectively zero hull angular velocity.
        submitAutopilotControl(coastForPlanningControl());
    }
    return true;
}

// updateDockingAdvisory / phase Stabilizing: settle, capture the authoritative
// planning snapshot, build the RoutePlanRequest (+ traffic route, safety course)
// and launch the asynchronous planner worker. Always ends this update.
template <class Frame>
void SpaceState::updateDockingStabilizing(Frame& f)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& fail = f.fail;
    auto& automatic = f.automatic;
    auto& transform = f.transform;
    auto& motion = f.motion;
    auto& definition = f.definition;
    auto& hull = f.hull;
    auto& fit = f.fit;
    auto& effectivePhysics = f.effectivePhysics;
    auto& controlLaw = f.controlLaw;
    auto& recoverOrFail = f.recoverOrFail;
    auto& liveAssistedAuthority = f.liveAssistedAuthority;
    auto& makeAgent = f.makeAgent;
    auto& workingThrustAxisMap = f.workingThrustAxisMap;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;

    const double relativeSpeedMps =
        glm::length(motion.localVelocityMps);
    const double angularSpeedRadPerSec =
        std::sqrt(
            transform.pitchRate * transform.pitchRate +
            transform.yawRate * transform.yawRate +
            transform.rollRate * transform.rollRate
        );

    struct
    {
        decltype(relativeSpeedMps)& relativeSpeedMps;
        decltype(angularSpeedRadPerSec)& angularSpeedRadPerSec;
    } l {relativeSpeedMps, angularSpeedRadPerSec};
    if (!settleDockingCraftBeforePlanning(f, l))
        return;

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
        // a city's obstacle geometry still being assembled on the client (game/scene/LayoutCityHitVolumes.h, not
        // replicated since 2026-11-23): wait for it - up to 60 s - instead of failing the request
        if (snapshot.status == game::client::ClientNavigationPlanningSnapshotStatus::ObstacleGeometryUnavailable)
        {
            if (m_geometryWaitSerial != pending.serial)
            {
                m_geometryWaitSerial = pending.serial;
                m_geometryWaitSince = glfwGetTime();
                std::cerr << "[DockClient] request=" << pending.serial
                          << " waiting for the city's obstacle geometry (assembled on the client)" << std::endl;
            }
            if (glfwGetTime() - m_geometryWaitSince < 60.0)
                return;
        }
        std::cerr << "[DockClient] request=" << pending.serial << " planning snapshot not ready: status="
                  << static_cast<int>(snapshot.status) << " source_epoch=" << snapshot.sourceEpoch.valid()
                  << " epoch=" << snapshot.epoch.valid() << " frame=" << snapshot.planningFrame.valid
                  << " target_kinematics=" << snapshot.targetModuleKinematics.valid << std::endl;
        fail("authoritative planning snapshot unavailable");
        return;
    }

    auto localPort =
        resolveDockingAdvisoryLocalPortAt(
            snapshot.targetObject.hubAttachment,
            *definition,
            snapshot.epoch.universeTimeSeconds
        );
    // a city pad: its approach point is known in the city object's own frame (the plan's coordinates - the pad service's
    // frame); the attachment path above put it elsewhere (live 2026-11-22: the ship flew away from the pad). Taken here
    // relative to the ship at one moment (the rendered city and ship), turned into the planning frame
    if (pending.target.semanticAnchorId.rfind("pad:", 0) == 0 && m_padAnchor.valid)
    {
        localPort.valid = false;
        const auto* city = padRouteCityObject(pending.target.stableObjectId);
        const auto shipIt = m_client->world().ships().find(m_playerId.value);
        if (city && shipIt != m_client->world().ships().end())
        {
            const glm::dmat3 R(glm::mat3(city->renderOrientation));
            const glm::dvec3 shipWorld = world::coordinates::fullMeters(shipIt->second.renderTransform.worldPosition);
            const glm::dvec3 hoverWorld = world::coordinates::fullMeters(city->renderWorldPosition) + R * m_padAnchor.hoverCity;
            localPort.positionMeters = snapshot.controlledShip.localPositionMeters +
                                       snapshot.planningFrame.worldToLocalVector(hoverWorld - shipWorld);
            localPort.forward = glm::normalize(snapshot.planningFrame.worldToLocalVector(R * m_padAnchor.forwardCity));
            localPort.up = glm::normalize(snapshot.planningFrame.worldToLocalVector(R * m_padAnchor.upCity));
            localPort.valid = true;
            // the same point in the module attachment's own terms (visual basis): the execution re-resolves the port live
            // from the attachment + the definition (the terminal stage) - with the plan's coordinates there it dived the
            // ship 190 m into a power block 1.1 km before the pad (live 2026-11-22)
            {
                const auto& a = snapshot.targetObject.hubAttachment;
                const glm::dvec3 angles = a.localRotationDeg + a.localAngularVelocityDegPerSecond * snapshot.epoch.universeTimeSeconds;
                const glm::dmat3 rot(glm::mat3(hubLocalEulerDegToMatrix(angles)));
                const auto tacticalToVisual = [](const glm::dvec3& t) { return glm::dvec3(t.z, t.y, -t.x); };
                auto& d = m_padAnchor.definition;
                d.localPositionMeters = glm::transpose(rot) * (tacticalToVisual(localPort.positionMeters) - a.localOffsetMeters);
                d.localForward = glm::normalize(glm::transpose(rot) * tacticalToVisual(localPort.forward));
                d.localUp = glm::normalize(glm::transpose(rot) * tacticalToVisual(localPort.up));
                const auto check = resolveDockingAdvisoryLocalPortAt(a, d, snapshot.epoch.universeTimeSeconds);
                std::cerr << "[PadRoute] request=" << pending.serial << " attachment spin (" << a.localAngularVelocityDegPerSecond.x << ","
                          << a.localAngularVelocityDegPerSecond.y << "," << a.localAngularVelocityDegPerSecond.z << ") deg/s, the live port "
                          << (check.valid ? glm::length(check.positionMeters - localPort.positionMeters) : -1.0) << " m off" << std::endl;
            }
            std::cerr << "[PadRoute] request=" << pending.serial << " approach point in the planning frame (" << localPort.positionMeters.x
                      << "," << localPort.positionMeters.y << "," << localPort.positionMeters.z << "), the ship at ("
                      << snapshot.controlledShip.localPositionMeters.x << "," << snapshot.controlledShip.localPositionMeters.y << ","
                      << snapshot.controlledShip.localPositionMeters.z << ")" << std::endl;
        }
    }
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
    // Assisted with an emergency working axis pushes along that axis,
    // not along the nose: at rest the route starts on it (after ALIGN
    // it lies on the course the nose held).
    const bool assistedEmergencyAtRest =
        controlLaw == game::navigation::LocalFlightControlLaw::Assisted &&
        effectivePhysics.assistedWorkingThrustAxisEmergency &&
        glm::length(startVelocityMps) <= 1.0;
    request.initialForward =
        game::navigation::DockingRouteEntryPolicy::initialCourse(
            startVelocityMps,
            assistedEmergencyAtRest
                ? workingThrustAxisMap(makeAgent())
                : hullForwardLocal
        );
    request.initialForwardLeadMeters =
        std::max(1000.0, hull.lengthMeters * 10.0);
    // an Assisted craft (nearly) at rest starts along its NOSE, not along its drift: a ship sinking onto a pad at
    // 6 m/s had its course straight DOWN and every route from there "ran into the city" (5 retries, 200 s, the planner
    // looked dead - author 2026-11-22: «планировщик не работает»). No course at all let the route start behind the
    // nose - "assisted-reverse-flight-not-supported", rejected round and round (the 4th landing never planned)
    if (controlLaw == game::navigation::LocalFlightControlLaw::Assisted && !assistedEmergencyAtRest &&
        glm::length(startVelocityMps) < 8.0)
        request.initialForward = hullForwardLocal;
    request.terminalReferenceDistanceMeters =
        pending.target.semanticAnchorId.rfind("pad:", 0) == 0
            ? std::max(80.0, hull.lengthMeters * 2.0)   // a pad: the route ends close to its approach point
            : std::max(
                  300.0,
                  hull.lengthMeters * 10.0 +
                      definition->requiredClearanceMeters * 4.0
              );
    request.goalMeters =
        localPort.positionMeters +
        localPort.forward *
            request.terminalReferenceDistanceMeters;
    // a city pad: the route ends on the lane nearest it; the pad service takes over there (author 2026-11-22)
    request.endAtLaneExit = pending.target.semanticAnchorId.rfind("pad:", 0) == 0;
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
        struct
        {
            decltype(assisted)& assisted;
            decltype(request)& request;
            decltype(startVelocityMps)& startVelocityMps;
            decltype(hullForwardLocal)& hullForwardLocal;
            decltype(planningWhileSecuringCourse)& planningWhileSecuringCourse;
        } l {assisted, request, startVelocityMps, hullForwardLocal, planningWhileSecuringCourse};
        if (!secureDockingCourseWhilePlanning(f, l))
            return;
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
        pending.target.stableObjectId == "hub_hex_dock_a" &&
        pending.target.semanticAnchorId == "dock_gate_front")
    {
        struct
        {
            decltype(snapshot)& snapshot;
            decltype(localPort)& localPort;
            decltype(assisted)& assisted;
            decltype(request)& request;
            decltype(compiledTrafficRoute)& compiledTrafficRoute;
        } l {snapshot, localPort, assisted, request, compiledTrafficRoute};
        if (!compileDockingTrafficRoute(f, l))
            return;
    }

    // A generated settlement's own lane network (HubLaneCatalog; game::navigation::traffic::applyHubLanes): RED zones
    // join the obstacles, the hub speed limit applies, the cheapest pre-written lane chain becomes the compiled
    // traffic route. ON by default; ELITE_HUB_LANES=0 switches it off (A/B diagnostics).
    static const bool hubLanesEnabled = [] { const char* v = std::getenv("ELITE_HUB_LANES"); return !(v && *v == '0'); }();
    if (hubLanesEnabled && !lateEntryRebind && !compiledTrafficRoute.valid)
    {
        std::string hubId;
        for (const auto& object : snapshot.objects)
            if (object.hubAttachment.valid && object.hubAttachment.moduleId == pending.target.stableObjectId)
                hubId = object.hubAttachment.hubId;
        const auto* network = game::navigation::traffic::HubLaneCatalog::forHub(hubId);
        const auto frameObject = std::find_if(
            snapshot.objects.begin(), snapshot.objects.end(), [&](const auto& object) {
                return network && object.hubAttachment.valid &&
                       object.hubAttachment.moduleId == network->frameModuleId();
            });
        if (frameObject != snapshot.objects.end())
        {
            const auto& attachment = frameObject->hubAttachment;
            const auto framePose = game::navigation::traffic::hubModulePlanningFrame(
                {attachment.localOffsetMeters, attachment.localRotationDeg, attachment.localAngularVelocityDegPerSecond},
                snapshot.epoch.universeTimeSeconds);
            std::string log;
            if (request.endAtLaneExit && m_padAnchor.valid && m_padAnchor.serial == pending.serial)
                if (const auto* city = padRouteCityObject(pending.target.stableObjectId))
                    if (const auto shipIt = m_client->world().ships().find(m_playerId.value);
                        shipIt != m_client->world().ships().end())
                    {
                        const glm::dvec3 q = padRouteCityLocal(*city, shipIt->second);
                        glm::dvec3 flat = q - m_padAnchor.hoverCity;
                        const double dh = glm::dot(flat, m_padAnchor.upCity);
                        flat -= m_padAnchor.upCity * dh;
                        // OFF (author 2026-11-22: «перемещение внутри хаба между площадками - заскриптовано, тоннель тупо над
                        // городом»): the planner flies the straight way itself now (its starts were the trouble - fixed)
                        constexpr bool ServiceDirectEnabled = false;
                        request.serviceDirectAllowed = ServiceDirectEnabled && std::abs(dh) < 400.0 && glm::length(flat) < 8000.0;
                    }
            // THE LANE-CHAIN SEARCH OFF THE FRAME THREAD (audit PLAN-6: applyHubLanes ran here synchronously - 351-559 ms,
            // a frame gap of 1.3-1.5 s on every request): it runs on a background task with a COPY of the request; the
            // planner's launch waits for it (the ship keeps its pre-planning state, the frame does not stall); its result
            // - RED zones in the obstacles, the hub speed limit, the chain's via points and straights - is carried into
            // this frame's request.
            game::navigation::traffic::CompiledTrafficRoute laneRoute;
            {
                auto& job = m_laneSearch;
                // finished searches of old requests: released without waiting
                job.retired.erase(std::remove_if(job.retired.begin(), job.retired.end(),
                                                 [](std::future<LaneSearchResult>& f) {
                                                     return !f.valid() ||
                                                            f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                                                 }),
                                  job.retired.end());
                if (!job.result.valid() || job.serial != pending.serial)
                {
                    if (job.result.valid())
                        job.retired.push_back(std::move(job.result));   // still running: never destroyed on this thread
                    job.serial = pending.serial;
                    job.result = std::async(std::launch::async,
                                            [network, framePose, copy = request]() mutable {
                                                LaneSearchResult r;
                                                r.route = game::navigation::traffic::applyHubLanes(*network, framePose, copy, &r.log);
                                                r.request = std::move(copy);
                                                return r;
                                            });
                    std::cerr << "[HubLanes] request=" << pending.serial << " the lane-chain search runs in the background"
                              << std::endl;
                    return;
                }
                if (job.result.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    return;
                LaneSearchResult r = job.result.get();
                log = std::move(r.log);
                laneRoute = std::move(r.route);
                request.obstacles = std::move(r.request.obstacles);
                request.maxSpeedMps = r.request.maxSpeedMps;
                request.geometrySpeedMps = r.request.geometrySpeedMps;
                request.requiredViaPointsMeters = std::move(r.request.requiredViaPointsMeters);
                request.mandatoryTangentStraights = std::move(r.request.mandatoryTangentStraights);
            }
            std::cerr << "[HubLanes] request=" << pending.serial << " " << log << std::endl;
            // a city pad whose straight way beats every lane chain (a free metre costs FreeInHubFactor x a lane metre,
            // author 2026-11-22: «надо иметь баланс ... это не значит, что я должен лететь вниз в зелёную зону») from a
            // ship already over the city: the pad service flies it straight across over the roofs, round the towers -
            // the planner's free leg through that sky was never feasible (5 retries, minutes)
            if (!laneRoute.valid && request.serviceDirectAllowed && m_padAnchor.valid && m_padAnchor.serial == pending.serial)
                if (const auto* city = padRouteCityObject(pending.target.stableObjectId))
                    if (const auto shipIt = m_client->world().ships().find(m_playerId.value);
                        shipIt != m_client->world().ships().end())
                    {
                        const glm::dvec3 q = padRouteCityLocal(*city, shipIt->second);
                        const glm::dvec3 up = m_padAnchor.upCity;
                        glm::dvec3 flat = q - m_padAnchor.hoverCity;
                        const double dh = glm::dot(flat, up);
                        flat -= up * dh;
                        if (std::abs(dh) < 400.0 && glm::length(flat) < 8000.0)
                        {
                            game::network::ClientMessage msg;
                            ClientShipCommand command;
                            command.type = ClientShipCommand::LandOnPad;
                            command.index = 1;
                            command.target = pending.target.stableObjectId;
                            command.detail = m_padAnchor.address;
                            msg.payload = command;
                            m_client->sendMessage(msg);
                            std::cerr << "[PadRoute] request=" << pending.serial << " the straight way to "
                                      << m_padAnchor.address << " beats the lanes (" << glm::length(flat)
                                      << " m over the city): the pad service flies it" << std::endl;
                            m_pilotNotice = "СЛУЖБА ПЛОЩАДОК: напрямую над городом к площадке " + m_padAnchor.address;
                            m_padAnchor = {};
                            f.requests.clear();
                            f.releaseClientAutopilot();
                            m_clientDockingPhase = ClientDockingPhase::Idle;
                            return;
                        }
                    }
            if (laneRoute.valid)
                compiledTrafficRoute = std::move(laneRoute);
        }
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
            // a stage with turns: its entry direction is the inbound straight's, not from -> to
            for (const auto& straight : compiledTrafficRoute.mandatoryTangentStraights)
                if (straight.inbound && straight.portalId == stage.fromPortalId &&
                    glm::length(straight.forwardWorld) > 1.0e-9)
                    preferredEntryForward = glm::normalize(straight.forwardWorld);
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
    // a new route while the pad service still flies the ship (phases 0-2): the service lets go first - two pilots at
    // once (the service holding it over a pad, the route's autopilot) made the tunnel shake between the two (author
    // 2026-11-22: «коридор планировщика дрожит, как будто не может понять координаты рамок»)
    if (m_client)
        if (const auto it = m_client->world().ships().find(m_playerId.value); it != m_client->world().ships().end())
        {
            const auto& svc = it->second.padService;
            if (svc.size() > 8 && svc.rfind("phase:", 0) == 0 && svc[6] != '3')
            {
                game::network::ClientMessage msg;
                ClientShipCommand command;
                command.type = ClientShipCommand::LandOnPad;
                command.index = 0;
                command.detail = svc.substr(8);
                msg.payload = command;
                m_client->sendMessage(msg);
                std::cerr << "[DockWorker] request=" << pending.serial << " the pad service (" << svc
                          << ") lets the ship go for the new route" << std::endl;
            }
        }
    std::cerr << "[DockWorker] request=" << pending.serial << " launched obstacles=" << request.obstacles.size()
              << std::endl;
    if (const char* dump = std::getenv("ELITE_ROUTE_DUMP"); dump && *dump)
    {
        // DEV: the obstacles the planner got near the start (route dumps: did it know the building it hit?)
        if (std::FILE* f2 = std::fopen((std::string(dump) + ".obstacles").c_str(), "w"))
        {
            std::fprintf(f2, "START %.1f %.1f %.1f GOAL %.1f %.1f %.1f\n", request.startMeters.x, request.startMeters.y, request.startMeters.z,
                         request.goalMeters.x, request.goalMeters.y, request.goalMeters.z);
            for (const auto& o : request.obstacles)
                if (glm::length(o.centerMeters - request.startMeters) < 1.0e9)
                    std::fprintf(f2, "O %s shape=%d c=%.1f %.1f %.1f h=%.1f %.1f %.1f r=%.1f clear=%.1f\n", o.id.c_str(), static_cast<int>(o.shape),
                                 o.centerMeters.x, o.centerMeters.y, o.centerMeters.z, o.halfExtentsMeters.x, o.halfExtentsMeters.y,
                                 o.halfExtentsMeters.z, o.radiusMeters, o.requiredClearanceMeters);
            // the boxes' bases (columns), for checking a route against them offline
            for (const auto& o : request.obstacles)
                std::fprintf(f2, "B %s %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f\n", o.id.c_str(), o.localToWorldBasis[0].x,
                             o.localToWorldBasis[0].y, o.localToWorldBasis[0].z, o.localToWorldBasis[1].x, o.localToWorldBasis[1].y,
                             o.localToWorldBasis[1].z, o.localToWorldBasis[2].x, o.localToWorldBasis[2].y, o.localToWorldBasis[2].z);
            std::fclose(f2);
        }
    }

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
            runDockAdviceWorker(
                job,
                count,
                request,
                compiledTrafficRoute,
                compileAgent,
                compileControlLaw,
                compilePhysics,
                compileRequestSerial,
                compileTolerance,
                compileExecutionUniverseTimeSeconds);
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

// updateDockingAdvisory / Planning (automatic): is the finished plan's ballistic
// prefix still a valid hand-off from the live state? Otherwise start the late
// entry rebind (connector replanned, backbone kept). false = stop this update.
template <class Frame, class Locals>
bool SpaceState::checkDockingEntryHandoff(Frame& f, Locals& l)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& recoverOrFail = f.recoverOrFail;
    auto& makeAgent = f.makeAgent;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;
    auto& job = l.job;

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
                return false;
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
            return false;
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
            return false;
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
        return false;
    }

    return true;
}

// updateDockingAdvisory / phase Planning: adopt the finished worker product
// (plan, compiled autopilot) and enter Executing / RouteReady.
// false = stop this update.
template <class Frame>
bool SpaceState::updateDockingPlanning(Frame& f)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& fail = f.fail;
    auto& automatic = f.automatic;
    auto& controlLaw = f.controlLaw;
    auto& recoverOrFail = f.recoverOrFail;
    auto& liveAssistedAuthority = f.liveAssistedAuthority;
    auto& makeAgent = f.makeAgent;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;
    auto& stabilizeAngularForPlanningControl = f.stabilizeAngularForPlanningControl;
    auto& manualStopControl = f.manualStopControl;
    auto& fixedDt = f.fixedDt;
    auto& authoritativeServerSeconds = f.authoritativeServerSeconds;
    auto& universeTimeSeconds = f.universeTimeSeconds;

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
        return false;
    }

    // A worker may finish before the craft has completed the physical
    // transition to its ballistic handoff state. Keep the ready result,
    // but do not adopt/validate it until motion settling is real.
    if (automatic && !automaticMotionSettled)
    {
        static double lastTrace = -1.0e9;
        if (authoritativeServerSeconds - lastTrace >= 2.0)
        {
            lastTrace = authoritativeServerSeconds;
            std::cerr << "[DockClient] request=" << pending.serial << " plan ready, waiting for the motion to settle"
                      << std::endl;
        }
        return false;
    }
    if (std::getenv("ELITE_ROUTE_DUMP"))   // DEV: the adopted route's curves (where it runs, the planning frame)
        for (const auto& c : m_dockAdviceJob->plan.routeCurves)
            std::cerr << "[DockRouteCurve] request=" << pending.serial << " kind=" << static_cast<int>(c.kind) << " s=" << c.startProgressMeters
                      << ".." << c.endProgressMeters << " (" << c.startMeters.x << "," << c.startMeters.y << "," << c.startMeters.z << ") -> ("
                      << c.endMeters.x << "," << c.endMeters.y << "," << c.endMeters.z << ") r=" << c.arcRadiusMeters << " vmax=" << c.maxSpeedMps << std::endl;
    std::cerr << "[DockClient] request=" << pending.serial << " plan ready: adopting (valid=" << m_dockAdviceJob->plan.valid()
              << ")" << std::endl;

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
        return false;
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
        return false;
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
            return false;
        }

        recoverOrFail(reason);
        return false;
    }

    if (automatic)
    {
        struct
        {
            decltype(job)& job;
        } l {job};
        if (!checkDockingEntryHandoff(f, l))
            return false;
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
        return false;
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
            return false;
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

    return true;
}

// updateDockingAdvisory / phase Executing: step the client route autopilot at the
// authoritative fixed rate and submit its control. false = stop this update.
template <class Frame>
bool SpaceState::updateDockingExecuting(Frame& f)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& releaseClientAutopilot = f.releaseClientAutopilot;
    // a pad route: close to the approach point over the pad the pad service takes the ship - across the last metres and
    // down onto the deck on its legs (the only scripted stage, author 2026-11-21)
    if (pending.target.semanticAnchorId.rfind("pad:", 0) == 0 && m_padAnchor.valid && m_padAnchor.serial == pending.serial)
    {
        const auto* city = padRouteCityObject(pending.target.stableObjectId);
        const auto shipIt = m_client->world().ships().find(m_playerId.value);
        if (city && shipIt != m_client->world().ships().end())
        {
            const glm::dvec3 qShip = padRouteCityLocal(*city, shipIt->second);
            const double d = glm::length(qShip - m_padAnchor.hoverCity);
            {
                static double lastTrace = -1.0e9;
                if (glfwGetTime() - lastTrace >= 3.0)
                {
                    lastTrace = glfwGetTime();
                    std::cerr << "[PadRoute] request=" << pending.serial << " ship at (" << qShip.x << "," << qShip.y << "," << qShip.z
                              << ") city frame, " << d << " m from the approach point" << std::endl;
                }
            }
            // ... or at the end of the route (it ends on the lane nearest the pad: endAtLaneExit)
            const double routeTotal =
                m_dockAdvice.plan.routeCurves.empty() ? 0.0 : m_dockAdvice.plan.routeCurves.back().endProgressMeters;
            const bool atRouteEnd = routeTotal > 0.0 && m_clientDockingRouteProgressMeters > 0.0 &&
                                    routeTotal - m_clientDockingRouteProgressMeters < 40.0;
            // a turning ring: the route ends at the SYNC GATE on the axis; the autopilot holds it, its hull rolling with
            // the ring (the terminal roll rate of the live port). Every point of the axis is still in both frames, so "at
            // the gate" = ON THE AXIS (within 15 m of it) near the gate (the docking-style final ends ~80 m short of its
            // point, live 2026-11-26). Synchronized = at rest there and the hull's roll RELATIVE TO THE CITY steady: drifting
            // under 3 deg over 3 s (the instant rate wobbles +-0.6 deg/s round the ring's 5.16 with the roll loop)
            bool gateSynced = false;
            if (m_padAnchor.ringGate)
            {
                const auto& st = shipIt->second.transform;
                const glm::dvec3 wShip = glm::dvec3(st.right()) * static_cast<double>(st.pitchRate) +
                                         glm::dvec3(st.up()) * static_cast<double>(st.yawRate) +
                                         glm::dvec3(st.forward()) * static_cast<double>(st.rollRate);
                const glm::dvec3 wCity = city->angularVelocityWorldRadPerSecond;
                const double rateErr = glm::length(wShip - wCity);
                const double speed = glm::length(st.relativeVelocityMps());
                const glm::dmat3 Rc(glm::mat3(city->renderOrientation));
                const glm::dvec3 axisWorld = glm::normalize(Rc * glm::dvec3(0.0, 0.0, 1.0));
                const double alongAxisDeg = glm::degrees(std::acos(std::clamp(std::abs(glm::dot(glm::dvec3(st.forward()), axisWorld)), 0.0, 1.0)));
                const double fromAxis = std::hypot(qShip.x, qShip.y);
                const double alongFromGate = std::abs(qShip.z - m_padAnchor.hoverCity.z);
                // the hull's roll in the city's own frame (its up projected across the axis)
                const glm::dvec3 upCity = glm::transpose(Rc) * glm::dvec3(shipIt->second.renderTransform.up());
                const double rollInCity = std::atan2(upCity.y, upCity.x);
                const bool holding = fromAxis < 15.0 && alongFromGate < 150.0 && speed < 1.5 && rateErr < glm::radians(2.0);
                const double now = glfwGetTime();
                if (!holding)
                    m_padAnchor.gateSyncedSinceS = -1.0;
                else if (m_padAnchor.gateSyncedSinceS < 0.0)
                {
                    m_padAnchor.gateSyncedSinceS = now;
                    m_padAnchor.gateRollStartRad = rollInCity;
                }
                double rollDrift = 0.0;
                if (m_padAnchor.gateSyncedSinceS >= 0.0)
                {
                    rollDrift = std::remainder(rollInCity - m_padAnchor.gateRollStartRad, 2.0 * 3.14159265358979323846);
                    if (std::abs(rollDrift) > glm::radians(3.0))   // the hull turns against the city: restart the window
                    {
                        m_padAnchor.gateSyncedSinceS = now;
                        m_padAnchor.gateRollStartRad = rollInCity;
                    }
                }
                if (now - m_padAnchor.gateLogS >= 3.0)
                {
                    m_padAnchor.gateLogS = now;
                    std::cerr << "[RingSync] request=" << pending.serial << " " << d << " m from the gate (" << fromAxis
                              << " m off the axis), " << speed << " m/s, the ship's turn " << glm::degrees(glm::length(wShip))
                              << " deg/s vs the ring's " << glm::degrees(glm::length(wCity)) << " (off " << glm::degrees(rateErr)
                              << " deg/s), roll in the city " << glm::degrees(rollInCity) << " deg (drift "
                              << glm::degrees(rollDrift) << "), nose " << alongAxisDeg << " deg off the axis"
                              << (holding ? " - holding" : "") << std::endl;
                }
                gateSynced = m_padAnchor.gateSyncedSinceS >= 0.0 && now - m_padAnchor.gateSyncedSinceS >= 3.0;
                if (gateSynced)
                {
                    std::cerr << "[RingSync] request=" << pending.serial << " synchronized with the ring at the gate (" << d
                              << " m, " << fromAxis << " m off the axis, " << speed << " m/s, roll drift "
                              << glm::degrees(rollDrift) << " deg in 3 s)" << std::endl;
                    m_pilotNotice = "ДИСПЕТЧЕР КОЛЬЦА: синхронизация с вращением выполнена у ворот на оси";
                }
            }
            // (until the city-frame route from the gate exists, the pad service flies on from the synchronized gate)
            if (m_padAnchor.ringGate ? gateSynced : (d < 250.0 || atRouteEnd))
            {
                std::cerr << "[PadRoute] request=" << pending.serial << " " << d << " m from the approach point of "
                          << m_padAnchor.address << ": the pad service takes the ship" << std::endl;
                game::network::ClientMessage msg;
                ClientShipCommand command;
                command.type = ClientShipCommand::LandOnPad;
                command.index = 1;
                command.target = pending.target.stableObjectId;
                command.detail = m_padAnchor.address;
                msg.payload = command;
                m_client->sendMessage(msg);
                m_pilotNotice = "СЛУЖБА ПЛОЩАДОК: над площадкой " + m_padAnchor.address + " - посадка на опорах";
                releaseClientAutopilot();
                f.requests.clear();
                m_padAnchor = {};
                return false;
            }
        }
    }
    // the station captured the ship (GameSimulationDockTraffic: entry cleared, within 60 m of the axis, < 25 m/s): from here
    // the server flies its own corridor to the pad and ignores our control - stop the client follower instead of logging a
    // "growing cross-track" against our old line (docking diagnosis 2026-11-20); the docked state ends the request as before
    if (m_client)
        if (const auto it = m_client->world().ships().find(m_playerId.value);
            it != m_client->world().ships().end() && it->second.dockDispatcher.active && it->second.dockDispatcher.stationControlled)
        {
            static std::uint64_t loggedSerial = 0;
            if (pending.valid() && loggedSerial != pending.serial)
            {
                loggedSerial = pending.serial;
                std::cerr << "[DockClient] request=" << pending.serial
                          << " phase=station-handoff: the station flies the ship, the client follower released" << std::endl;
            }
            releaseClientAutopilot();
            return false;
        }
    auto& motion = f.motion;
    auto& effectivePhysics = f.effectivePhysics;
    auto& controlLaw = f.controlLaw;
    auto& recoverOrFail = f.recoverOrFail;
    auto& liveAssistedAuthority = f.liveAssistedAuthority;
    auto& makeAgent = f.makeAgent;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;
    auto& fixedDt = f.fixedDt;
    auto& universeTimeSeconds = f.universeTimeSeconds;
    auto& currentTunnelRollPhase = f.currentTunnelRollPhase;

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
        recoverOrFail("motors degraded; committed route needs new planning");
        return false;
    }
    FollowerAgent reflexAgent {};   // the agent the autopilot flew this frame (the evasion reflex's ship state)
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
            // the REFLEX over the route: a radar contact coming inside the safe sphere -> an RCS dodge, then back
            const double reflexLateral = std::min({liveAssistedAuthority.positive[0], liveAssistedAuthority.negative[0],
                                                   liveAssistedAuthority.positive[1], liveAssistedAuthority.negative[1]});
            // the ship's state for the control programs (body axes: +X right, +Y up, -Z nose - the radar's frame)
            game::navigation::avoidance::ControlShipState reflexShip;
            reflexShip.params = effectivePhysics;
            reflexShip.law = controlLaw;
            const glm::dvec3 vMap = reflexAgent.velocityMapMetersPerSecond;
            reflexShip.velocityBody = glm::dvec3(glm::dot(vMap, reflexAgent.rightMap), glm::dot(vMap, reflexAgent.upMap),
                                                 -glm::dot(vMap, reflexAgent.forwardMap));
            reflexShip.targetForwardSpeedMps = reflexAgent.assistedTargetSpeedMps;
            reflexShip.rcsKeepsTranslationDeltaV = output.control.rcsKeepsTranslationDeltaV;
            reflexShip.deltaVSinceScanBody = deltaVSinceRadarScanBody();
            reflexShip.ratesPitchYawRoll = glm::vec3(static_cast<float>(reflexAgent.pitchRateRadPerSec),
                                                     static_cast<float>(reflexAgent.yawRateRadPerSec),
                                                     static_cast<float>(reflexAgent.rollRateRadPerSec));
            m_routeReflexApplied = true;
            submitAutopilotControl(applyEvasionReflex(output.control, reflexLateral, ownBoundingRadiusMeters(),
                                                      universeTimeSeconds, &reflexShip));

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
        core::hitch::mark("dock_assisted_pre_autopilot");
        reflexAgent = makeAgent();
        const auto output =
            AssistedClientAutopilot::update(
                m_clientAssistedRouteAutopilot,
                reflexAgent,
                effectivePhysics,
                universeTimeSeconds,
                pilotDt
            );
        core::hitch::mark("dock_assisted_autopilot_update");
        if (!handleOutput(output))
            return false;
        core::hitch::mark("dock_assisted_handle_output");
    }
    else
    {
        NewtonianClientAutopilot::setLiveTunnelRollPhase(
            m_clientNewtonianRouteAutopilot,
            tunnelRollPhase,
            pilotDt
        );
        core::hitch::mark("dock_pre_autopilot");
        reflexAgent = makeAgent();
        const auto output =
            NewtonianClientAutopilot::update(
                m_clientNewtonianRouteAutopilot,
                reflexAgent,
                effectivePhysics,
                universeTimeSeconds,
                pilotDt
            );
        core::hitch::mark("dock_newtonian_autopilot_update");
        if (!handleOutput(output))
            return false;
    }
    }

    return true;
}

// updateDockingAdvisory / phase SecuringCourse: fly the selected safety course
// while the planner worker runs. Always ends this update.
template <class Frame>
void SpaceState::updateDockingSecuringCourse(Frame& f)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& pending = f.pending;
    auto& fail = f.fail;
    auto& automatic = f.automatic;
    auto& effectivePhysics = f.effectivePhysics;
    auto& controlLaw = f.controlLaw;
    auto& makeAgent = f.makeAgent;
    auto& submitAutopilotControl = f.submitAutopilotControl;
    auto& coastForPlanningControl = f.coastForPlanningControl;
    auto& stabilizeAngularForPlanningControl = f.stabilizeAngularForPlanningControl;
    auto& manualStopControl = f.manualStopControl;
    auto& legacyNewtonianSafeCourseControl = f.legacyNewtonianSafeCourseControl;
    auto& fixedDt = f.fixedDt;
    auto& authoritativeServerSeconds = f.authoritativeServerSeconds;

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
        {
            // DEV trace (ELITE_DEV_STDOUT runs): the safe-course acquisition, every 2 s
            static double lastTrace = -1.0e9;
            if (authoritativeServerSeconds - lastTrace >= 2.0)
            {
                lastTrace = authoritativeServerSeconds;
                std::cerr << "[AssistedSafeCourse] request=" << pending.serial << " phase=steer course_err_deg="
                          << glm::degrees(safeOutput.courseErrorRad) << " hull_err_deg="
                          << glm::degrees(safeOutput.hullToVelocityErrorRad) << " speed_mps=" << speedMps
                          << " omega_radps=" << angularSpeedRadPerSec << " ctl_pitch=" << safeOutput.control.pitchInput
                          << " ctl_yaw=" << safeOutput.control.yawInput << " planner_running=" << (m_dockAdviceJob ? 1 : 0)
                          << std::endl;
            }
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

                m_pilotNotice = "СТЫКОВКА: нет безопасного курса отсюда (рядом постройки / площадка) - отведите корабль на 100-200 м и повторите";   // the reason in the terminal (author 2026-11-21: «нет ни расчёта маршрута, ни причин»)
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

// updateDockingAdvisory / every phase that reaches the end: publish the
// accepted plan as the two guidance corridors (execution + sparse frames).
template <class Frame>
void SpaceState::publishDockingAdvisoryPresentation(Frame& f)
{
    using namespace game::navigation;
    using AssistedClientAutopilot =
        game::navigation::autopilot::AssistedClientRouteAutopilot;
    using NewtonianClientAutopilot =
        game::navigation::autopilot::NewtonianClientRouteAutopilot;
    using FollowerAgent =
        game::navigation::autopilot::AutopilotAgentState;
    auto& guidance = f.guidance;
    auto& automatic = f.automatic;
    auto& player = f.player;
    auto& currentTunnelRollPhase = f.currentTunnelRollPhase;

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

// Navigator step 3: the evasion reflex (src/game/navigation/avoidance/EvasionReflex.h) over the route autopilot. The
// radar is the only source (the author: «радар должен кормить автопилот»); no radar / radar down = no reflex.
ShipControlState SpaceState::applyEvasionReflex(ShipControlState control, double lateralAccelMps2, double hullRadiusMeters,
                                                double universeTimeSeconds,
                                                const game::navigation::avoidance::ControlShipState* ship)
{
    const char* env = std::getenv("ELITE_DEV_EVASION_REFLEX");
    if (env && env[0] == '0')
        return control;
    // the episode: this request's route run (a new request / a re-request starts the reflex clean - NAV-7)
    m_evasionReflex.bindEpisode(reflexEpisodeKey(0x5254u), universeTimeSeconds);
    const auto& sensors = m_client->navigationSensors();
    if (!sensors.radarInstalled || !sensors.radarOperational || !sensors.hasRadarScan)
    {
        m_evasionReflex.reset();
        return control;
    }
    if (!m_evasionNetTried)
    {
        m_evasionNetTried = true;
        const bool ok = m_evasionNet.load("assets/data/navigation/evasion_net.json");
        std::cerr << "[Evasion] net " << (ok ? "loaded" : "absent - the teacher alone") << " (" << m_evasionNet.outputs()
                  << " outputs)" << std::endl;
        if (ok)
            m_evasionReflex.net = &m_evasionNet;
    }
    // the control programs on the real physics (ELITE_DEV_EVASION_REFLEX=pm: the old point-mass reflex)
    const bool programs = ship && !(env && env[0] == 'p');
    const auto& cmd = programs
                          ? m_evasionReflex.updatePrograms(sensors.latestRadarScan, universeTimeSeconds, *ship, hullRadiusMeters,
                                                           lateralAccelMps2)
                          : m_evasionReflex.update(sensors.latestRadarScan, universeTimeSeconds, lateralAccelMps2, hullRadiusMeters);
    if (!cmd.active)
        return control;
    control.strafeInput = cmd.strafe;
    control.liftInput = cmd.lift;
    control.forwardInput = cmd.forward;
    control.rcsTranslationPerAxisAuthority = true;
    control.velocityAlignmentCommand = game::navigation::VelocityAlignmentMode::None;
    control.cancelVelocityAlignment = true;   // NAV-6: the dodge flown is the one predicted
    if (cmd.program >= 0)
    {
        // the program owns the whole stick while it flies (an emergency: the route waits)
        control.pitchInput = cmd.pitch;
        control.yawInput = cmd.yaw;
        control.rollInput = 0.0f;
        control.targetSpeedRate = cmd.speedRate;
    }
    if (universeTimeSeconds - m_evasionLogAt > 1.0)
    {
        m_evasionLogAt = universeTimeSeconds;
        std::cerr << "[Evasion] dodge: " << cmd.threats << " contacts, course margin " << cmd.courseMarginMeters
                  << " m (window " << cmd.urgencySeconds << " s) -> " << cmd.marginMeters << " m; RCS (" << cmd.strafe << ", " << cmd.lift << ", " << cmd.forward
                  << ")" << (cmd.program >= 0 ? " program " + std::to_string(cmd.program) + " turn (" +
                                                    std::to_string(cmd.pitch) + ", " + std::to_string(cmd.yaw) + ") speed " +
                                                    std::to_string(cmd.speedRate) + ", " +
                                                    std::to_string(m_evasionReflex.lastProgramMs) + " ms"
                                              : std::string(cmd.fromNet ? " [net]" : " [teacher]"))
                  << std::endl;
    }
    return control;
}

void SpaceState::submitVirtualPilotControl(const ShipControlState& control, std::uint64_t pilotTag)
{
    if (!m_client)
        return;
    m_clientAutopilotControl =
        control.globalFlightCommandValid
            ? control
            : applyManualCollisionAssist(control, m_client->lastSimulationMetadata().universeTimeSeconds, true, pilotTag);
    m_clientAutopilotControlActive = true;
    m_client->submitInput(m_clientAutopilotControl);
}

void SpaceState::updateUnsampledCollisionAssist()
{
    const bool sampled = m_manualSampledThisFrame;
    m_manualSampledThisFrame = false;
    if (sampled || !m_client)
        return;
    const bool virtualPilot = m_clientAutopilotControlActive || m_bodyAutopilotOwnsControl || m_clientRouteOwnsControl ||
                              m_capsuleRescue.ownsControl;
    if (virtualPilot)
        return;
    static double loggedAt = -1.0e9;
    const double now = m_client->lastSimulationMetadata().universeTimeSeconds;
    if (now - loggedAt > 10.0)
    {
        loggedAt = now;
        std::cerr << "[CollisionAssist] controls not sampled this frame (a map / panel open): the assist watches the "
                     "pilot's last control" << std::endl;
    }
    const ShipControlState control = applyManualCollisionAssist(m_lastPilotSample, now);
    if (m_manualAssistActive)
    {
        m_playerControl = control;
        m_client->submitInput(m_playerControl);
        m_unsampledAssistSubmitted = true;
    }
    else if (m_unsampledAssistSubmitted)
    {
        m_playerControl = m_lastPilotSample;   // the dodge ended: the pilot's own last control again
        m_client->submitInput(m_playerControl);
        m_unsampledAssistSubmitted = false;
    }
}

std::uint64_t SpaceState::reflexEpisodeKey(std::uint64_t tag) const
{
    const auto& pending = m_navigationWorkspace.dockingRouteRequests().pending();
    const std::uint64_t serial = pending.valid() ? static_cast<std::uint64_t>(pending.serial) : 0u;
    std::uint64_t h = (serial + 1u) * 0x9E3779B97F4A7C15ULL ^ (tag * 0xC2B2AE3D27D4EB4FULL);
    h ^= h >> 31;
    return h | 2u;   // never 0 (a fresh reflex) or 1 (the pilot's own flight)
}

glm::dvec3 SpaceState::deltaVSinceRadarScanBody() const
{
    if (!m_client)
        return glm::dvec3(0.0);
    const auto& sensors = m_client->navigationSensors();
    const auto it = m_client->world().ships().find(m_playerId.value);
    if (!sensors.hasRadarScan || it == m_client->world().ships().end())
        return glm::dvec3(0.0);
    const auto& tr = it->second.transform;
    const glm::dvec3 dv = tr.motion.worldVelocityMps - sensors.latestRadarScan.observerWorldVelocityMps;
    // the body frame now: +X right, +Y up, -Z nose (the radar's)
    return glm::dvec3(glm::dot(dv, glm::dvec3(tr.right())), glm::dot(dv, glm::dvec3(tr.up())), -glm::dot(dv, glm::dvec3(tr.forward())));
}

double SpaceState::ownBoundingRadiusMeters() const
{
    if (!m_client)
        return 10.0;
    const auto it = m_client->world().ships().find(m_playerId.value);
    if (it == m_client->world().ships().end())
        return 10.0;
    double fallback = 10.0;
    if (it->second.descriptor)
    {
        const auto dim = it->second.descriptor->logicalDimensions();
        fallback = game::damage::dimensionsBoundingRadius(dim.length, dim.width, dim.height);
    }
    return std::max(1.0, game::damage::hitVolumesBoundingRadius(it->second.debugHitVolumes, fallback));
}

// THE COLLISION ASSIST while the pilot flies by hand (author 2026-11-24: «как сейчас работает система уклонения - в
// любом режиме? ... при ручном управлении внезапно возникнет помеха»; «уклонение - третий вариант» = by the difficulty):
// the same radar reflex as over the route (control programs flown on the ship's own physics), with the urgency window -
// it acts only when the touch is about as near in time as the dodge takes. "warn": a HUD warning only (hard realism);
// "assist": the program holds the stick for the manoeuvre (shown on the HUD); a SHARP pilot input (a full deflection /
// any translation key) hands it back for 3 s - the pilot is always the last word. "off": nothing.
ShipControlState SpaceState::applyManualCollisionAssist(const ShipControlState& sampled, double universeTimeSeconds,
                                                        bool autopilotPhase, std::uint64_t pilotTag)
{
    auto& reflex = autopilotPhase ? m_phaseReflex : m_manualReflex;
    // the episode (NAV-7): a virtual-pilot phase of a request, or the pilot's own flight - a new phase / request starts
    // the phase reflex clean; the clock going back resets either
    reflex.bindEpisode(autopilotPhase ? reflexEpisodeKey(0x5048u + static_cast<std::uint64_t>(m_clientDockingPhase) +
                                                         (pilotTag << 8))
                                      : 1u,
                       universeTimeSeconds);
    std::string mode = context().app ? context().app->clientModeState().collisionAssistMode : std::string("assist");
    if (const char* dev = std::getenv("ELITE_DEV_COLLISION_ASSIST"); dev && *dev)
        mode = dev;   // DEV: off / warn / assist for the checks
    if (autopilotPhase)
        mode = "assist";   // the virtual pilot always dodges (as over the route)
    const auto quiet = [&]() {
        if (!autopilotPhase && m_manualAssistActive)
        {
            m_manualAssistActive = false;
            m_pilotNotice = "ПОМОЩЬ ОТ СТОЛКНОВЕНИЙ: управление у пилота";
        }
        return sampled;
    };
    const char* env = std::getenv("ELITE_DEV_EVASION_REFLEX");
    if (mode == "off" || !m_client || (env && env[0] == '0'))
    {
        reflex.reset();
        return quiet();
    }
    const auto& sensors = m_client->navigationSensors();
    if (!sensors.radarInstalled || !sensors.radarOperational || !sensors.hasRadarScan)
    {
        reflex.reset();
        return quiet();
    }
    const auto shipIt = m_client->world().ships().find(m_playerId.value);
    if (shipIt == m_client->world().ships().end() || !shipIt->second.descriptor)
        return quiet();
    const auto& player = shipIt->second;
    const auto& transform = player.transform;
    const auto& motion = transform.motion;
    // the pilot's sharp input takes the stick back
    const bool sharp = std::abs(sampled.pitchInput) > 0.8f || std::abs(sampled.yawInput) > 0.8f ||
                       sampled.strafeInput != 0.0f || sampled.liftInput != 0.0f || sampled.forwardInput != 0.0f;
    if (!autopilotPhase && m_manualAssistActive && sharp)
    {
        m_manualAssistSuppressedUntil = universeTimeSeconds + 3.0;
        reflex.reset();
        return quiet();
    }
    if (!autopilotPhase && universeTimeSeconds < m_manualAssistSuppressedUntil)
        return quiet();
    // the ship's physical state for the control programs (body axes: +X right, +Y up, -Z nose - the radar's frame)
    // NAV-4: the physics is THIS ship's under THIS control model and law, refreshed with the server's ticks (a static
    // shared by every ship / SpaceState, kept 1 s - the reflex flew a lost engine for up to 1 s, a boarded ship's
    // params; a failed read cached zero capabilities)
    auto& cache = m_reflexPhysicsCache;
    const std::uint64_t serverTick = m_client->lastSimulationMetadata().serverTick;
    const bool keyChanged = cache.instance != static_cast<std::uint64_t>(player.instanceId) ||
                            cache.model != static_cast<int>(player.physicalControlModel) ||
                            cache.law != static_cast<int>(motion.localControlLaw);
    if (!cache.valid || keyChanged || serverTick < cache.tick || serverTick >= cache.tick + 5)
    {
        cache.instance = static_cast<std::uint64_t>(player.instanceId);
        cache.model = static_cast<int>(player.physicalControlModel);
        cache.law = static_cast<int>(motion.localControlLaw);
        cache.tick = serverTick;
        ShipParams physics {};
        const auto slots = player.physicalControlModel != player.descriptor->physicalControlModel
                               ? game::ship::plant::healthySlotState(
                                     game::ship::plant::physicalControlSlots(player.physicalControlModel))
                               : game::ship::capability::buildPhysicalControlSlotState(
                                     *player.descriptor, player.physicalControlModel, player.modules);
        const auto capability =
            game::ship::capability::buildShipCapability(*player.descriptor, player.physicalControlModel, slots);
        physics = capability.valid ? capability.effectivePhysics : ShipParams {};
        // the mother computer's envelope is the DAMAGED truth (the route reflex flies it too): the module list alone
        // predicted a healthy dodge - 120 m predicted, 37 m flown with an aft engine lost (phase_ram2)
        ShipParams mother {};
        if (m_motherComputer.physicalFlightEnvelope(static_cast<std::uint64_t>(player.instanceId),
                                                    m_client->lastSimulationMetadata().serverTick, mother))
            physics = mother;
        if (capability.valid || physics.strafeAccel > 0.0f)
        {
            cache.params = physics;
            cache.valid = true;
        }
        else if (keyChanged)
            cache.valid = false;   // no physics for this ship yet: no programs (below), never the zero ship
    }
    if (!cache.valid)
        return quiet();
    game::navigation::avoidance::ControlShipState ship;
    ship.params = cache.params;
    ship.law = motion.localControlLaw;
    const bool local = player.referenceFrame.valid && motion.mode == game::navigation::MotionMode::HubTactical;
    const auto frame = player.referenceFrame.kinematicFrame();
    const auto axis = [&](const glm::vec3& w) {
        const glm::dvec3 a = local ? frame.worldToLocalVector(glm::dvec3(w)) : glm::dvec3(w);
        return glm::length(a) > 1.0e-9 ? glm::normalize(a) : glm::dvec3(0.0, 0.0, -1.0);
    };
    const glm::dvec3 v = local ? motion.localVelocityMps : motion.worldVelocityMps - motion.referenceVelocityMps;
    const glm::dvec3 right = axis(transform.right()), up = axis(transform.up()), forward = axis(transform.forward());
    ship.velocityBody = glm::dvec3(glm::dot(v, right), glm::dot(v, up), -glm::dot(v, forward));
    ship.targetForwardSpeedMps = std::max(0.0, static_cast<double>(motion.targetForwardSpeedMps));
    ship.rcsKeepsTranslationDeltaV = sampled.rcsKeepsTranslationDeltaV;
    ship.ratesPitchYawRoll = glm::vec3(transform.pitchRate, transform.yawRate, transform.rollRate);
    ship.deltaVSinceScanBody = deltaVSinceRadarScanBody();
    const double hullRadius = ownBoundingRadiusMeters();
    // the STRUCTURES near the course (the pilot's own flight only - a planned route is proven clear of them): every hit
    // volume of an object of this system within the reach of the horizon, as its bounding sphere, when we close on it
    // faster than 15 m/s (slower is a landing / a docking, not a crash - the assist must not fight it)
    // ... and under the AUTOPILOT too (author 2026-11-24: «почему не сработала система уклонения? корабль шёл на
    // автопилоте»): the reflex there trusted the planner for every structure ("they do not move") - the ring hub's rim
    // DOES move (108 m/s), a route planned on a frozen ring ran into a sector at 100 m/s and the reflex never looked.
    // EVERY obstacle, not only the small ones (author 2026-11-24: «давай системе уклонения все препятствия, не только
    // мелкие. как раз на такой случай»): the 96 with the earliest straight-line contact
    std::vector<game::navigation::avoidance::EvasionThreatTrack> structures;
    std::vector<std::pair<double, game::navigation::avoidance::EvasionThreatTrack>> structureCandidates;
    {
        const glm::dvec3 me = world::coordinates::fullMeters(transform.worldPosition);
        const glm::dvec3 myV = motion.worldVelocityMps;
        const double reach = glm::length(motion.worldVelocityMps - motion.referenceVelocityMps) * reflex.horizonSeconds + 300.0;
        const glm::dvec3 wr(transform.right()), wu(transform.up()), wf(transform.forward());
        const auto toBody = [&](const glm::dvec3& w) { return glm::dvec3(glm::dot(w, wr), glm::dot(w, wu), -glm::dot(w, wf)); };
        for (const auto& [oid, object] : m_client->world().objects())
        {
            if (object.systemId != motion.systemId)
                continue;
            const glm::dvec3 origin = world::coordinates::fullMeters(object.worldPosition);
            if (glm::length(origin - me) > reach + 20000.0)
                continue;
            const std::vector<game::simulation::DebugHitVolumeSnapshot>* volumes = &object.debugHitVolumes;
            if (volumes->empty() && game::scene::layoutCityHitVolumesOnClient(object.type, object.hubAttachment.hubId))
                volumes = game::scene::clientLayoutCityHitVolumes(object.type, object.hubAttachment.hubId);
            if (!volumes)
                continue;
            const glm::dmat3 R(glm::mat3(object.orientation));
            for (const auto& v : *volumes)
            {
                if (v.destroyed)
                    continue;
                const glm::dvec3 cBox = origin + R * glm::dvec3(v.center);
                const glm::dvec3 half(v.halfSize);
                if (glm::length(cBox - me) - glm::length(half) > reach)
                    continue;
                // a LONG box (a truss, a ring sector) as a chain of spheres along its long axis, each as thick as the box
                // (author 2026-11-24: «компромисс, чтобы он не шарахался от каждого крупного объекта»): one sphere round
                // a 600 m truss was a 600 m ball - a pass 50 m off it was a "collision"
                int axis = 0;
                for (int k = 1; k < 3; ++k)
                    if (half[k] > half[axis])
                        axis = k;
                const double thick = std::sqrt(half[(axis + 1) % 3] * half[(axis + 1) % 3] + half[(axis + 2) % 3] * half[(axis + 2) % 3]);
                const int pieces = std::clamp(static_cast<int>(std::ceil(half[axis] / std::max(10.0, thick))), 1, 16);
                const double pieceHalf = half[axis] / pieces;
                const double pieceR = std::sqrt(thick * thick + pieceHalf * pieceHalf);
                glm::dvec3 boxAxis(0.0);
                boxAxis[axis] = 1.0;
                const glm::dvec3 worldAxis = R * (glm::dmat3(v.orientation) * boxAxis);
                for (int p = 0; p < pieces; ++p)
                {
                    const glm::dvec3 c = cBox + worldAxis * (-half[axis] + pieceHalf * (2.0 * p + 1.0));
                    const glm::dvec3 rel = c - me;
                    if (glm::length(rel) - pieceR > reach)
                        continue;
                    // the volume's own velocity: the object's + its spin (a rotating hub's rim moves ~108 m/s)
                    const glm::dvec3 relV = object.linearVelocityMps + glm::cross(object.angularVelocityWorldRadPerSecond, c - origin) - myV;
                    const double closing = glm::length(rel) > 1.0e-6 ? -glm::dot(relV, glm::normalize(rel)) : 0.0;
                    if (closing < 15.0)
                        continue;
                    game::navigation::avoidance::EvasionThreatTrack t;
                    t.position = toBody(rel);
                    t.velocity = toBody(relV);
                    t.radiusMeters = pieceR;
                    structureCandidates.emplace_back(std::max(0.0, glm::length(rel) - pieceR) / closing, t);
                }
            }
        }
        constexpr std::size_t MaxStructureThreats = 128;
        if (structureCandidates.size() > MaxStructureThreats)
            std::nth_element(structureCandidates.begin(), structureCandidates.begin() + MaxStructureThreats,
                             structureCandidates.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t i = 0; i < structureCandidates.size() && i < MaxStructureThreats; ++i)
            structures.push_back(structureCandidates[i].second);
    }
    const auto& cmd = reflex.updatePrograms(sensors.latestRadarScan, universeTimeSeconds, ship, hullRadius, 0.0,
                                            structures.empty() ? nullptr : &structures);
    if (!cmd.active)
        return quiet();
    if (mode == "warn")
    {
        if (universeTimeSeconds - m_manualAssistWarnedAt > 3.0)
        {
            m_manualAssistWarnedAt = universeTimeSeconds;
            m_pilotNotice = "ВНИМАНИЕ: КУРС СТОЛКНОВЕНИЯ - " + std::to_string(cmd.threats) + " контакт(ов), уклоняйтесь";
            std::cerr << "[CollisionAssist] warn: " << cmd.threats << " contacts, course margin " << cmd.courseMarginMeters
                      << " m (window " << cmd.urgencySeconds << " s)" << std::endl;
        }
        return sampled;
    }
    // assist: the program holds the stick for the manoeuvre - and only the stick: a velocity / acceleration target of the
    // virtual pilot (the recovery stop after an engine loss) outranks the stick in the flight law, the dodge was
    // ignored there (5 dodges logged, the miss unchanged at 39 m, failsuite phase_ram)
    ShipControlState control = sampled;
    control.navigationAccelerationDemandValid = false;
    control.navigationVelocityTargetValid = false;
    control.navigationPrecisionTranslationOnly = false;
    control.globalFlightCommandValid = false;
    control.globalJVelocityTargetValid = false;
    control.globalAttitudeTargetValid = false;
    control.strafeInput = cmd.strafe;
    control.liftInput = cmd.lift;
    control.forwardInput = cmd.forward;
    control.rcsTranslationPerAxisAuthority = true;
    // NAV-6: the real competitor of the dodge is the PERSISTENT alignment (the recovery stop's BrakeToStop: a 1/dt
    // longitudinal brake in Assisted, a hull flip with the aft block lost) - the targets cleared above are not set on
    // the client and the gateway already prefers the stick; cancel the alignment while the dodge flies
    control.velocityAlignmentCommand = game::navigation::VelocityAlignmentMode::None;
    control.cancelVelocityAlignment = true;
    if (cmd.program >= 0)
    {
        control.pitchInput = cmd.pitch;
        control.yawInput = cmd.yaw;
        control.rollInput = 0.0f;
        control.targetSpeedRate = cmd.speedRate;
    }
    if (!autopilotPhase && !m_manualAssistActive)
        m_pilotNotice = "ПОМОЩЬ ОТ СТОЛКНОВЕНИЙ: манёвр уклонения (резкое движение ручкой - управление вам)";
    if (!autopilotPhase)
        m_manualAssistActive = true;
    if (universeTimeSeconds - m_manualAssistLogAt > 1.0)
    {
        m_manualAssistLogAt = universeTimeSeconds;
        std::cerr << (autopilotPhase ? "[Evasion] phase dodge: " : "[CollisionAssist] dodge: ") << cmd.threats << " contacts, course margin " << cmd.courseMarginMeters
                  << " m (window " << cmd.urgencySeconds << " s) -> " << cmd.marginMeters << " m; program " << cmd.program
                  << std::endl;
    }
    return control;
}
