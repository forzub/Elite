#include "GameServer.h"
#include <type_traits>
#include "src/game/network/ClientMessage.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>

#include "src/world/coordinates/WorldPosition.h"
#include <cmath>
#include <functional>
#include <unordered_map>

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/world_state/InitialWorldState.h"



namespace {
    constexpr double AU_KM_LOCAL = 149597870.7;

    glm::vec4 fallbackBodyColor(
        world::celestial::BodyType type
    )
    {
        using world::celestial::BodyType;

        switch (type)
        {
            case BodyType::Star:
                return {1.00f, 0.93f, 0.62f, 1.00f};

            case BodyType::Planet:
                return {0.36f, 0.68f, 1.00f, 1.00f};

            case BodyType::Moon:
                return {0.70f, 0.72f, 0.78f, 1.00f};

            case BodyType::AsteroidBelt:
                return {0.55f, 0.55f, 0.55f, 0.70f};

            default:
                return {0.60f, 0.82f, 1.00f, 1.00f};
        }
    }



    void appendSystemMapMotionDebugCsv(
        const world::celestial::SystemMapSnapshot& snapshot,
        game::diagnostics::ServerDiagnostics& diagnostics
    )
    {
        if (!diagnostics.settings.systemMapMotionCsv)
            return;

        double& lastLoggedUniverseTime =
            diagnostics.server.systemMapLastLoggedUniverseTime;

        // Log no more than once per universe-time second.
        if (lastLoggedUniverseTime >= 0.0 &&
            std::abs(snapshot.universeTimeSeconds - lastLoggedUniverseTime) < 1.0)
        {
            return;
        }

        lastLoggedUniverseTime =
            snapshot.universeTimeSeconds;

        const char* path =
            "system_map_motion.csv";

        std::ifstream check(path);
        const bool needHeader =
            !check.good();
        check.close();

        std::ofstream out(
            path,
            std::ios::app
        );

        if (!out.is_open())
            return;

        if (needHeader)
        {
            out
                << "universe_time,system_id,body_id,body_name,type,"
                << "x_au,y_au,z_au,orbit_radius_au,draw_orbit\n";
        }

        for (const auto& body : snapshot.bodies)
        {
            if (body.type != world::celestial::BodyType::Planet &&
                body.type != world::celestial::BodyType::Moon)
            {
                continue;
            }

            out
                << std::fixed
                << std::setprecision(6)
                << snapshot.universeTimeSeconds
                << ","
                << snapshot.systemId
                << ",\""
                << body.id
                << "\",\""
                << body.name
                << "\","
                << world::celestial::toString(body.type)
                << ","
                << std::setprecision(12)
                << body.positionAu.x
                << ","
                << body.positionAu.y
                << ","
                << body.positionAu.z
                << ","
                << body.orbitRadiusAu
                << ","
                << (body.drawOrbit ? 1 : 0)
                << "\n";
        }
    }








    world::celestial::SystemMapRingDisplayMode
        toSystemMapRingDisplayMode(
            world::celestial::CelestialRingDisplayMode mode
        )
        {
            using Source =
                world::celestial::CelestialRingDisplayMode;

            using Target =
                world::celestial::SystemMapRingDisplayMode;

            return
                mode == Source::ParticleCloud
                    ? Target::ParticleCloud
                    : Target::LayeredBands;
        }

        world::celestial::SystemMapRingVisibilityClass
        toSystemMapRingVisibilityClass(
            world::celestial::CelestialRingVisibilityClass value
        )
        {
            using Source =
                world::celestial::CelestialRingVisibilityClass;

            using Target =
                world::celestial::SystemMapRingVisibilityClass;

            switch (value)
            {
                case Source::Main:
                    return Target::Main;

                case Source::Secondary:
                    return Target::Secondary;

                case Source::Diffuse:
                    return Target::Diffuse;

                case Source::Faint:
                default:
                    return Target::Faint;
            }
        }

        world::celestial::SystemMapRingVisualProfile
        toSystemMapRingVisualProfile(
            const world::celestial::
                CelestialRingSystemVisualProfile& source
        )
        {
            world::celestial::
                SystemMapRingVisualProfile result;

            result.displayProfile =
                source.displayProfile;

            result.renderMode =
                toSystemMapRingDisplayMode(
                    source.renderMode
                );

            result.recognizabilityPriority =
                source.recognizabilityPriority;

            result.artisticWidthScale =
                source.artisticWidthScale;

            result.mainBandEmphasis =
                source.mainBandEmphasis;

            result.secondaryBandEmphasis =
                source.secondaryBandEmphasis;

            result.faintBandEmphasis =
                source.faintBandEmphasis;

            result.diffuseBandEmphasis =
                source.diffuseBandEmphasis;

            result.gapContrast =
                source.gapContrast;

            result.radialStructureStrength =
                source.radialStructureStrength;

            result.fineStructureStrength =
                source.fineStructureStrength;

            result.edgeSoftnessScale =
                source.edgeSoftnessScale;

            result.brightnessVariation =
                source.brightnessVariation;

            result.minimumVisibleWidthPx =
                source.minimumVisibleWidthPx;

            result.minimumMainBandWidthPx =
                source.minimumMainBandWidthPx;

            result.continuousFill =
                source.continuousFill;

            result.particleDensity =
                source.particleDensity;

            result.particleOpacityScale =
                source.particleOpacityScale;

            result.particleSizePxRange =
                source.particleSizePxRange;

            result.radialJitter =
                source.radialJitter;

            result.azimuthalClumping =
                source.azimuthalClumping;

            result.clusterScale =
                source.clusterScale;

            result.softness =
                source.softness;

            result.artisticOcclusionEnabled =
                source.artisticOcclusionEnabled;

            result.backHalfBrightness =
                source.backHalfBrightness;

            result.innerEdgeDarkening =
                source.innerEdgeDarkening;

            return result;
        }

        world::celestial::SystemMapRing
        toSystemMapRing(
            const world::celestial::
                CelestialRingDefinition& source
        )
        {
            world::celestial::SystemMapRing result;

            result.name =
                source.name;

            result.innerRadiusKm =
                source.innerRadiusKm;

            result.outerRadiusKm =
                source.outerRadiusKm;

            result.composition =
                source.composition;

            result.tint =
                source.render.tint;

            result.opacity =
                source.render.opacity;

            result.opticalDepth =
                source.render.opticalDepth;

            result.radialNoiseStrength =
                source.render.radialNoiseStrength;

            result.radialBrightnessVariation =
                source.render.radialBrightnessVariation;

            result.azimuthalAsymmetry =
                source.render.azimuthalAsymmetry;

            result.edgeSoftness =
                source.render.edgeSoftness;

            result.visibilityClass =
                toSystemMapRingVisibilityClass(
                    source.render.visibilityClass
                );

            result.displayMode =
                toSystemMapRingDisplayMode(
                    source.render.displayMode
                );

            result.visualOpacityScale =
                source.render.visualOpacityScale;

            result.radialStructureScale =
                source.render.radialStructureScale;

            result.particleDensityScale =
                source.render.particleDensityScale;

            result.particleClumpiness =
                source.render.particleClumpiness;

            result.particleRadialJitter =
                source.render.particleRadialJitter;

            result.particleSizePxRange =
                source.render.particleSizePxRange;

            result.castsShadow =
                source.render.castsShadow;

            return result;
        }













    double stablePhaseRadians(const std::string& id)
    {
        uint32_t h = 2166136261u;

        for (unsigned char c : id)
        {
            h ^= c;
            h *= 16777619u;
        }

        const double t =
            static_cast<double>(h % 10000u) / 10000.0;

        return t * glm::two_pi<double>();
    }



    const world::celestial::CelestialBodyDefinition*
        findBodyById(
            const world::celestial::CelestialSystemDefinition* system,
            const std::string& bodyId
        )
        {
            if (!system)
                return nullptr;

            for (const auto& body : system->bodies)
            {
                if (body.id == bodyId)
                    return &body;
            }

            return nullptr;
        }




}


namespace
{
    world::celestial::LocalSceneAxes axesToHubLocal(
        const glm::mat4& worldOrientation,
        const game::navigation::HubNavigationFrame& frame
    )
    {
        world::celestial::LocalSceneAxes axes;

        auto convert =
            [&](const glm::dvec3& worldAxis)
            {
                return glm::dvec3(
                    glm::dot(worldAxis, frame.progradeAxis),
                    glm::dot(worldAxis, frame.radialAxis),
                    glm::dot(worldAxis, frame.normalAxis)
                );
            };

        axes.x = convert(glm::dvec3(worldOrientation[0]));
        axes.y = convert(glm::dvec3(worldOrientation[1]));
        axes.z = convert(glm::dvec3(worldOrientation[2]));

        return axes;
    }

    glm::dvec3 velocityToHubLocal(
        const glm::dvec3& worldVelocity,
        const game::navigation::HubNavigationFrame& frame
    )
    {
        const glm::dvec3 relative =
            worldVelocity - frame.velocityMetersPerSecond;

        return glm::dvec3(
            glm::dot(relative, frame.progradeAxis),
            glm::dot(relative, frame.radialAxis),
            glm::dot(relative, frame.normalAxis)
        );
    }
}














GameServer::GameServer()
    : m_diagnostics{}
    , m_simulation(m_diagnostics)
{

        m_universeClock.reset();
        m_universeClock.setTimeScale(
            m_debugFastUniverseTimeScale
        );

        m_lastUniverseTimeSeconds =
            m_universeClock.timeSeconds();

        bool atlasLoaded =
        m_starAtlas.load(
            "assets/data/galaxy_details"
        );

        if (!atlasLoaded)
        {
            atlasLoaded =
                m_starAtlas.load(
                    "../assets/data/galaxy_details"
                );
        }

        if (!atlasLoaded)
        {
            // Developer-tree fallback when EliteGame.exe is launched
            // from build/ and the copied runtime assets are stale or absent.
            atlasLoaded =
                m_starAtlas.load(
                    "../src/assets/data/galaxy_details"
                );
        }

        if (!atlasLoaded)
        {
            std::cerr
                << "[GameServer] galaxy details catalog was not loaded\n";
        }

        game::world_state::InitialWorldState initialWorldState;
        const bool initialWorldLoaded =
            game::world_state::loadInitialWorldStateWithFallbacks(
                initialWorldState
            );

        if (!initialWorldLoaded)
        {
            throw std::runtime_error(
                "authoritative initial world state is missing or invalid"
            );
        }

        for (const auto& systemState : initialWorldState.systemStates)
        {
            m_systemJurisdictions[systemState.systemId] =
                systemState.jurisdiction;
        }

        const int initialSystemId =
            initialWorldState.playerStart.systemId;

        if (!m_starAtlas.findSystem(initialSystemId))
        {
            throw std::runtime_error(
                "player_start references a physical system absent from StarAtlas"
            );
        }

        m_playerNavigation.currentSystemId =
            initialSystemId;

        m_celestialRuntimes.initialize(m_starAtlas);




const double universeTime =
    m_universeClock.timeSeconds();


std::unordered_map<std::string, glm::dvec3>
    currentCelestialPositionsAu;
std::unordered_map<std::string, glm::dvec3>
    currentCelestialVelocitiesAuPerSecond;

if (const auto* celestial =
        celestialSnapshotForSystem(
            m_playerNavigation.currentSystemId
        ))
{
    for (const auto& state : celestial->bodies)
    {
        currentCelestialPositionsAu[state.id] =
            state.positionAu;
        currentCelestialVelocitiesAuPerSecond[state.id] =
            state.velocityAuPerSecond;
    }
}

m_simulation.setOrbitalUniverseTimeSeconds(universeTime);
m_simulation.setCelestialBodyKinematicStateAu(
    m_playerNavigation.currentSystemId,
    currentCelestialPositionsAu,
    currentCelestialVelocitiesAuPerSecond
);











        m_simulation.buildInitialScene(initialWorldState);
        synchronizePlayerSystemMembership();

        applyCelestialOrbitParentParameters();

        // Готовим хабы, станции и reference frames до размещения игрока.
        // Это не полный update и не создаёт грязный стартовый snapshot.
        m_simulation.prepareReferenceFramesForSpawn();

        game::navigation::ReferenceFrame playerStartFrame;
        playerStartFrame.type =
            game::navigation::ReferenceFrameType::OrbitalHub;
        playerStartFrame.systemId =
            initialWorldState.playerStart.systemId;
        playerStartFrame.hubId =
            initialWorldState.playerStart.hubId;
        playerStartFrame.localOffsetMeters =
            initialWorldState.playerStart.localOffsetMeters;

        if (!m_simulation.placeShipInReferenceFrame(
                m_simulation.playerId(),
                playerStartFrame))
        {
            throw std::runtime_error(
                "validated player_start reference frame could not be resolved"
            );
        }

        game::server::ServerTimeContext initialTime;
        initialTime.serverTick = 0;
        initialTime.universeTimeSeconds = universeTime;
        initialTime.universeTimeScale = m_universeClock.timeScale();
        initialTime.universeTimeSimulation =
            m_universeClock.simulationMode();

        m_simulation.update(initialTime);
        m_simulation.setTick(0);






        m_lastSnapshot = m_simulation.snapshot();
        populateClientSessionSnapshot(m_lastSnapshot);


}







void GameServer::synchronizePlayerSystemMembership()
{
    const Ship* player = m_simulation.playerShip();
    if (!player)
        return;

    const int shipSystemId =
        player->core().transform().motion.systemId;

    if (shipSystemId >= 0)
        m_playerNavigation.currentSystemId = shipSystemId;
}


const world::celestial::CelestialSystemSnapshot*
GameServer::celestialSnapshotForSystem(int systemId) const
{
    return m_celestialRuntimes.resolve(
        systemId,
        m_universeClock.timeSeconds()
    );
}


void GameServer::applyCelestialOrbitParentParameters()
{
    const int systemId =
        m_playerNavigation.currentSystemId;

    const auto* system =
        m_starAtlas.findSystem(systemId);

    if (!system)
        return;

    for (const auto& body : system->bodies)
    {
        if (body.gravitationalParameterM3s2 <= 0.0)
            continue;

        if (body.radiusKm <= 0.0)
            continue;

        const double radiusMeters =
            body.radiusKm * 1000.0;

        m_simulation.setCelestialBodyGravityParameters(
            systemId,
            body.id,
            radiusMeters,
            body.gravitationalParameterM3s2
        );

        m_simulation.updateStaticObjectOrbitParentParameters(
            systemId,
            body.id,
            radiusMeters,
            body.gravitationalParameterM3s2
        );
    }

    m_appliedSimulationContextSystemId = systemId;
}
















void GameServer::update(double dt)
{
    // Capture the passive-trajectory seed before the accelerated clock is
    // advanced. At this point celestial bodies, hubs, reference frames and
    // ships all belong to the same last completed authoritative epoch.
    if (m_pendingUniverseTrajectoryDiagnosticEntry)
    {
        const bool diagnosticReady =
            m_simulation.beginUniverseTrajectoryDiagnostic(
                m_pendingUniverseTrajectoryDiagnosticEpochSeconds
            );

        m_pendingUniverseTrajectoryDiagnosticEntry = false;

        if (!diagnosticReady)
        {
            std::cerr
                << "[UniverseDiagnosticTrajectory] activation cancelled: "
                << "not every eligible ship could enter the diagnostic branch\n";

            /*
                Failure is itself a timeline transition: setSimulationMode(false)
                rewinds UniverseClock to the real epoch. Route it through the
                normal setter so revision fencing is published as well.
            */
            setDebugUniverseTimeSimulation(
                false,
                m_universeClock.configuredTimeScale()
            );
        }
    }

    m_universeClock.update(dt);
    m_serverTick++;

    const double universeTime =
        m_universeClock.timeSeconds();

    game::server::ServerTimeContext time;
    time.serverTick = m_serverTick;
    time.serverDeltaSeconds = std::max(0.0, dt);
    time.universeTimeSimulation =
        m_universeClock.simulationMode();
    time.gameplayDeltaSeconds =
        time.universeTimeSimulation
            ? 0.0
            : time.serverDeltaSeconds;
    time.universeTimeSeconds = universeTime;
    time.universeDeltaSeconds =
        universeTime - m_lastUniverseTimeSeconds;
    time.universeTimeScale =
        m_universeClock.timeScale();

    m_lastUniverseTimeSeconds = universeTime;

    // 1. Apply commands. In accelerated universe-time diagnostics ships are
    // passive bodies: controls and event commands are acknowledged/discarded
    // but never affect the trajectory. This prevents delayed commands from
    // firing when normal gameplay resumes.
    for (auto& [id, shipPtr] : m_simulation.ships())
    {
        Ship& ship = *shipPtr;

        auto controlIt = m_controlStreams.find(id.value);
        if (controlIt != m_controlStreams.end())
        {
            auto& stream = controlIt->second;

            if (time.universeTimeSimulation)
            {
                // Prediction is disabled on the client for this branch. Drain
                // every queued production input now so none can leak through
                // after the diagnostic branch is discarded.
                stream.discardPendingAndAcknowledgeNewest();
            }

            if (!time.universeTimeSimulation)
            {
                ShipControlState cmd;
                if (stream.consumeNext(cmd))
                    ship.setControlState(cmd);
            }
        }

        /*
            Accelerated diagnostics consume/acknowledge incoming controls but
            never overwrite the frozen production control state. The branch is
            observational; leaving it resumes from the same gameplay state
            that existed before the diagnostic session.
        */
        auto cmdIt = m_pendingClientShipCommands.find(id.value);
        if (cmdIt == m_pendingClientShipCommands.end())
            continue;

        auto& cmdQueue = cmdIt->second;

        if (time.universeTimeSimulation)
        {
            cmdQueue.clear();
            continue;
        }

        while (!cmdQueue.empty())
        {
            const auto& shipCmd = cmdQueue.front();

            std::cout << "GameServer::update  - ClientShipCommand received: "
                    << shipCmd.type << "\n";

            switch (shipCmd.type)
            {
                case ClientShipCommand::EjectCockpitCapsule:
                    m_simulation.ejectShipCockpitCapsule(id);
                    break;

                case ClientShipCommand::StartBestRepairJob:
                    m_simulation.startBestRepairJobForFirstMissingSlot(id);
                    break;

                default:
                    ship.applyCommand(shipCmd);
                    break;
            }

            cmdQueue.pop_front();
        }
    }



m_simulation.setOrbitalUniverseTimeSeconds(
    universeTime
);

// The player's current system is entity authority, not an independent server
// navigation variable. Resolve the active celestial context from membership
// before injecting this tick's kinematics.
synchronizePlayerSystemMembership();

std::unordered_map<std::string, glm::dvec3> celestialPositionsAu;
std::unordered_map<std::string, glm::dvec3>
    celestialVelocitiesAuPerSecond;

if (const auto* celestial =
        celestialSnapshotForSystem(
            m_playerNavigation.currentSystemId
        ))
{
    for (const auto& state : celestial->bodies)
    {
        celestialPositionsAu[state.id] = state.positionAu;
        celestialVelocitiesAuPerSecond[state.id] =
            state.velocityAuPerSecond;
    }
}

m_simulation.setCelestialBodyKinematicStateAu(
    m_playerNavigation.currentSystemId,
    celestialPositionsAu,
    celestialVelocitiesAuPerSecond
);

if (m_appliedSimulationContextSystemId !=
    m_playerNavigation.currentSystemId)
{
    applyCelestialOrbitParentParameters();
}

m_simulation.update(time);
m_simulation.setTick(m_serverTick);



    if (m_simulation.playerShip())
    {
        const ShipTransform tr =
            m_simulation.presentationShipTransform(
                m_simulation.playerId()
            );

        m_playerNavigation.worldPosition = tr.worldPosition;
        m_playerNavigation.orientation = tr.orientation;

        m_playerNavigation.systemLocalMeters =
            world::coordinates::fullMeters(tr.worldPosition);

        m_playerNavigation.systemLocalAu =
            m_playerNavigation.systemLocalMeters /
            world::celestial::MetersPerAu;

        m_playerNavigation.forward = tr.forward();
        m_playerNavigation.up = tr.up();
    }











    if (m_forceSnapshotPublication ||
        m_serverTick % m_snapshotInterval == 0)
    {
        m_lastSnapshot = m_simulation.snapshot();
        populateClientSessionSnapshot(m_lastSnapshot);
        m_forceSnapshotPublication = false;
    }

    processPendingMapRequests();
    processPendingPresentationDataRequests();
}

void GameServer::enqueueMapRequest(const game::network::MapRequest& request)
{
    if (m_pendingMapRequests.size() >= MaxPendingMapRequests)
    {
        m_pendingMapRequests.pop_front();
        ++m_queueDiagnostics.droppedMapRequests;
    }

    m_pendingMapRequests.push_back(request);
}

bool GameServer::popMapResponse(game::network::MapResponse& outResponse)
{
    if (m_completedMapResponses.empty())
        return false;
    outResponse = std::move(m_completedMapResponses.front());
    m_completedMapResponses.pop_front();
    return true;
}

void GameServer::processPendingMapRequests()
{
    const auto metadata = protocolMetadata();
    while (!m_pendingMapRequests.empty())
    {
        auto request = std::move(m_pendingMapRequests.front());
        m_pendingMapRequests.pop_front();
        std::visit([this, &metadata](const auto& typedRequest)
        {
            using RequestT = std::decay_t<decltype(typedRequest)>;
            if constexpr (std::is_same_v<RequestT, game::network::GalaxyMapRequest>)
            {
                game::network::GalaxyMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata;
                response.snapshot = buildGalaxyMapSnapshot();
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
            else if constexpr (std::is_same_v<RequestT, game::network::SystemMapRequest>)
            {
                game::network::SystemMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata; response.systemId = typedRequest.systemId;
                response.snapshot = buildSystemMapSnapshot(typedRequest.systemId);
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
            else if constexpr (std::is_same_v<RequestT, game::network::DetailMapRequest>)
            {
                game::network::DetailMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata; response.target = typedRequest.target;
                response.snapshot = buildDetailMapSnapshot(typedRequest.target);
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
            else if constexpr (std::is_same_v<RequestT, game::network::HubMapRequest>)
            {
                game::network::HubMapResponse response;
                response.requestId = typedRequest.requestId; response.metadata = metadata; response.systemId = typedRequest.systemId; response.hubId = typedRequest.hubId;
                response.snapshot = buildHubMapSnapshot(typedRequest.systemId, typedRequest.hubId);
                if (m_completedMapResponses.size() >= MaxCompletedMapResponses)
                {
                    m_completedMapResponses.pop_front();
                    ++m_queueDiagnostics.droppedMapResponses;
                }
                m_completedMapResponses.push_back(std::move(response));
            }
        }, request);
    }
}

void GameServer::enqueuePresentationDataRequest(
    const game::network::PresentationDataRequest& request)
{
    if (m_pendingPresentationDataRequests.size() >=
        MaxPendingPresentationRequests)
    {
        m_pendingPresentationDataRequests.pop_front();
        ++m_queueDiagnostics.droppedPresentationRequests;
    }

    m_pendingPresentationDataRequests.push_back(request);
}

bool GameServer::popPresentationDataResponse(
    game::network::PresentationDataResponse& outResponse)
{
    if (m_completedPresentationDataResponses.empty())
        return false;

    outResponse = std::move(m_completedPresentationDataResponses.front());
    m_completedPresentationDataResponses.pop_front();
    return true;
}

void GameServer::processPendingPresentationDataRequests()
{
    while (!m_pendingPresentationDataRequests.empty())
    {
        auto request = std::move(m_pendingPresentationDataRequests.front());
        m_pendingPresentationDataRequests.pop_front();

        std::visit(
            [this](const auto& typedRequest)
            {
                using RequestT = std::decay_t<decltype(typedRequest)>;

                if constexpr (std::is_same_v<
                                  RequestT,
                                  game::network::StarAtlasRequest>)
                {
                    game::network::StarAtlasResponse response;
                    response.requestId = typedRequest.requestId;
                    response.metadata.catalogRevision = catalogRevision();
                    response.atlas = m_starAtlas;
                    if (m_completedPresentationDataResponses.size() >=
                        MaxCompletedPresentationResponses)
                    {
                        m_completedPresentationDataResponses.pop_front();
                        ++m_queueDiagnostics.droppedPresentationResponses;
                    }
                    m_completedPresentationDataResponses.push_back(
                        std::move(response)
                    );
                }
            },
            request
        );
    }
}

void GameServer::populateClientSessionSnapshot(
    SimulationSnapshot& snapshot
) const
{
    snapshot.metadata.serverTick = m_serverTick;
    snapshot.metadata.serverTimeSeconds = m_simulation.serverTime();
    snapshot.metadata.universeTimeSeconds = m_universeClock.timeSeconds();
    snapshot.metadata.universeTimelineRevision =
        m_universeTimelineRevision;

    for (auto& ship : snapshot.ships)
    {
        const auto it = m_controlStreams.find(ship.id.value);
        ship.acknowledgedControlTick =
            it != m_controlStreams.end()
                ? it->second.lastProcessedTick()
                : 0;
    }

    snapshot.session.playerNavigation = m_playerNavigation;
    snapshot.session.predictionWorldParams = m_simulation.world();
    snapshot.session.universeTimeSeconds =
        m_universeClock.timeSeconds();
    snapshot.session.universeTimeScale =
        m_universeClock.timeScale();
    snapshot.session.universeTimelineRevision =
        m_universeTimelineRevision;
    snapshot.session.configuredUniverseTimeScale =
        m_universeClock.configuredTimeScale();
    snapshot.session.universeTimeSimulation =
        m_universeClock.simulationMode();
    snapshot.session.universeDate =
        m_universeClock.dateTimeString();
}



void GameServer::submitCommand(EntityId id, const ShipControlState& control)
{
    auto& stream = m_controlStreams[id.value];

    const auto result = stream.enqueue(control);

    using EnqueueResult =
        game::server::FixedStepControlQueue::EnqueueResult;

    if (result == EnqueueResult::Stale)
    {
        ++m_queueDiagnostics.staleControlCommands;
        return;
    }

}





void GameServer::receiveClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{
    std::visit(
        [this, playerId](const auto& payload)
        {
            using PayloadT = std::decay_t<decltype(payload)>;

            if constexpr (std::is_same_v<PayloadT, ShipControlState>)
            {
                submitCommand(playerId, payload);
            }
            else if constexpr (std::is_same_v<PayloadT, ClientShipCommand>)
            {
                auto& queue = m_pendingClientShipCommands[playerId.value];
                if (queue.size() >= MaxShipCommandsPerShip)
                {
                    ++m_queueDiagnostics.droppedShipCommands;
                    return;
                }

                queue.push_back(payload);
            }
        },
        msg.payload
    );
}







void GameServer::debugRefreshSnapshot()
{
    // Debug panels are allowed to request heavy structural data.
    // This keeps regular published snapshots lightweight, while
    // structure_debug.html still receives modules/links on demand.
    m_simulation.debugForceFullShipGraphPayload();

    // The full graph is published by the next normal authoritative tick.
    // Debug UI must never run GameSimulation::update() out of band.
    m_forceSnapshotPublication = true;
}


const SimulationSnapshot& GameServer::snapshot() const
{
    // return m_simulation.snapshot();
    return m_lastSnapshot;
}

EntityId GameServer::playerId() const
{
    return m_simulation.playerId();
}

WorldParams& GameServer::world()
{
    return m_simulation.world();
}

bool GameServer::debugDestroyShipModule(EntityId shipId, const std::string& moduleId)
{
    const bool ok = m_simulation.debugDestroyShipModule(shipId, moduleId);

    std::cout
        << "[GameServer] debugDestroyShipModule entityId="
        << shipId.value
        << " moduleId=" << moduleId
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}


bool GameServer::debugSetShipStructuralLinkHealth(
    EntityId id,
    const std::string& linkId,
    float health,
    bool destroyed
)
{
    return m_simulation.debugSetShipStructuralLinkHealth(
        id,
        linkId,
        health,
        destroyed
    );
}


bool GameServer::debugDetachShipModule(EntityId id, const std::string& moduleId)
{
    return m_simulation.debugDetachShipModule(id, moduleId);
}


bool GameServer::debugReattachShipModule(
    EntityId id,
    const std::string& moduleId
)
{
    const bool ok = m_simulation.debugReattachShipModule(id, moduleId);

    std::cout
        << "[GameServer] debugReattachShipModule entityId="
        << id.value
        << " moduleId=" << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}



bool GameServer::startShipRepairJob(
    EntityId id,
    const std::string& moduleId
)
{
    const bool ok =
        m_simulation.startShipRepairJob(id, moduleId);

    std::cout
        << "[GameServer] startShipRepairJob entityId="
        << id.value
        << " moduleId=" << moduleId
        << " ok=" << ok
        << "\n";

    return ok;
}


bool GameServer::startBestRepairJobForMissingSlot(
    EntityId targetShipId,
    const std::string& targetModuleId
)
{
    const bool ok =
        m_simulation.startBestRepairJobForMissingSlot(
            targetShipId,
            targetModuleId
        );

    std::cout
        << "[GameServer] startBestRepairJobForMissingSlot shipId="
        << targetShipId.value
        << " targetModuleId="
        << targetModuleId
        << " ok="
        << ok
        << "\n";

    return ok;
}


bool GameServer::startBestRepairJobForFirstMissingSlot(EntityId targetShipId)
{
    const bool ok =
        m_simulation.startBestRepairJobForFirstMissingSlot(targetShipId);

    std::cout
        << "[GameServer] startBestRepairJobForFirstMissingSlot shipId="
        << targetShipId.value
        << " ok=" << ok
        << "\n";

    return ok;
}





bool GameServer::ejectShipCockpitCapsule(EntityId id)
{
    const bool ok = m_simulation.ejectShipCockpitCapsule(id);

    std::cout
        << "[GameServer] ejectShipCockpitCapsule entityId="
        << id.value
        << " ok=" << ok
        << "\n";

    return ok;
}




bool GameServer::debugHangShipModule(EntityId id, const std::string& moduleId)
{
    return m_simulation.debugHangShipModule(id, moduleId);
}

bool GameServer::debugReevaluateShipStructure(EntityId id)
{
    return m_simulation.debugReevaluateShipStructure(id);
}



bool GameServer::debugRestoreShipModule(EntityId shipId, const std::string& moduleId)
{
    const bool ok = m_simulation.debugRestoreShipModule(shipId, moduleId);

    std::cout
        << "[GameServer] debugRestoreShipModule entityId="
        << shipId.value
        << " moduleId=" << moduleId
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}

bool GameServer::debugResetShipStructure(EntityId shipId)
{
    const bool ok = m_simulation.debugResetShipStructure(shipId);

    std::cout
        << "[GameServer] debugResetShipStructure entityId="
        << shipId.value
        << " result=" << (ok ? "OK" : "FAIL")
        << "\n";

    return ok;
}

void GameServer::debugResetAllShipStructures()
{
    m_simulation.debugResetAllShipStructures();

    std::cout << "[GameServer] debugResetAllShipStructures\n";
}









world::celestial::GalaxyMapSnapshot GameServer::buildGalaxyMapSnapshot() const
{
    world::celestial::GalaxyMapSnapshot out;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    out.universeDate =
        m_universeClock.dateTimeString();














    for (const auto& s : m_starAtlas.systems())
    {
        world::celestial::GalaxyMapSystem item;

        item.id = s.id;
        item.name = s.name;
        item.starType = s.starType;
        item.starsCount = s.starsCount;
        item.positionLy = s.positionLy;
        const auto jurisdictionIt =
            m_systemJurisdictions.find(s.id);
        item.jurisdiction =
            jurisdictionIt != m_systemJurisdictions.end()
                ? jurisdictionIt->second
                : "Unregistered";

        out.systems.push_back(std::move(item));
    }

    for (const auto& source : m_starAtlas.objects())
    {
        world::celestial::GalaxyMapObject item;

        item.id = source.id;
        item.name = source.name;
        item.objectType = source.objectType;
        item.positionLy = source.positionLy;
        item.description = source.description;
        item.tags = source.tags;

        out.objects.push_back(std::move(item));
    }

    return out;
}






namespace
{
    glm::dvec3 safeNormalizePlanetMap(
        const glm::dvec3& v,
        const glm::dvec3& fallback
    )
    {
        const double len2 =
            glm::dot(v, v);

        if (len2 < 1e-12)
            return fallback;

        return v / std::sqrt(len2);
    }

    world::celestial::LocalSceneAxes planetMapAxesFromOrientation(
        const glm::mat4& m
    )
    {
        world::celestial::LocalSceneAxes axes;

        axes.x = glm::dvec3(m[0]);
        axes.y = glm::dvec3(m[1]);
        axes.z = glm::dvec3(m[2]);

        return axes;
    }


    glm::dvec3 assemblySizeMetersForType(
        ObjectType typeId
    )
    {
        using game::ship::geometry::AssemblyMeshLibrary;

        if (typeId == ObjectType::None)
            return glm::dvec3(1.0);

        if (!AssemblyMeshLibrary::has(typeId))
            return glm::dvec3(1.0);

        const auto& assembly =
            AssemblyMeshLibrary::get(typeId);

        const glm::vec3 size =
            assembly.maxBounds - assembly.minBounds;

        return glm::dvec3(
            std::max(1.0f, size.x),
            std::max(1.0f, size.y),
            std::max(1.0f, size.z)
        );
    }
}











world::celestial::SystemMapSnapshot
GameServer::buildSystemMapSnapshot(
    int systemId
) const
{
    world::celestial::SystemMapSnapshot out;

    const auto* system =
        m_starAtlas.findSystem(systemId);

    if (!system)
        return out;

    out.systemId = system->systemId;
    out.systemName = system->name;

    if (const auto* summary =
        m_starAtlas.findSystemSummary(
            systemId
        ))
    {
        out.systemPositionLy =
            summary->positionLy;
    }

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    out.universeTimeScale =
        m_universeClock.timeScale();

    out.universeDate =
        m_universeClock.dateTimeString();








    const auto* runtimeSnapshot =
        celestialSnapshotForSystem(systemId);

    if (!runtimeSnapshot)
        return out;

    std::unordered_map<
        std::string,
        const world::celestial::CelestialBodyState*
    > runtimeStateById;

    for (const auto& state : runtimeSnapshot->bodies)
    {
        runtimeStateById[state.id] = &state;
    }



    for (const auto& body : system->bodies)
    {
        world::celestial::SystemMapBody item;

        item.id = body.id;
        item.name = body.name;
        item.alternativeNames = body.alternativeNames;

        item.parentId = body.parentId;
        item.environmentPresetId = body.environmentPresetId;

        item.type = body.type;
        item.radiusKm = body.radiusKm;

        item.orbitalPeriodDays =
            body.orbitalPeriodDays;

        item.orbitalDirection =
            body.orbitalDirection;

        item.orbitalPhaseOffsetRad =
            body.orbitalPhaseOffsetDeg *
            3.14159265358979323846 /
            180.0;

        item.rotationPhaseRad =
            body.rotationOffsetDeg *
            3.14159265358979323846 /
            180.0;

        item.dayLengthHours =
            body.dayLengthHours;

        item.rotationDirection =
            body.rotationDirection;

        item.axialTiltDeg =
            body.axialTiltDeg;

        item.axisNodeDeg =
            body.axisNodeDeg;

        item.textureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        auto stateIt =
            runtimeStateById.find(body.id);

        if (stateIt != runtimeStateById.end())
        {
            const auto& state =
                *stateIt->second;

            item.positionAu = state.positionAu;
            item.rotationPhaseRad = state.rotationPhaseRad;
            item.dayLengthHours = state.dayLengthHours;
            item.rotationDirection = state.rotationDirection;
            item.axialTiltDeg = state.axialTiltDeg;
            item.axisNodeDeg = state.axisNodeDeg;
            item.textureLongitudeOffsetDeg =
                state.textureLongitudeOffsetDeg;
        }
        else
        {
            item.positionAu =
                body.staticPositionAu;
        }





        if (!body.parentId.empty())
        {
            auto parentIt =
                runtimeStateById.find(body.parentId);

            if (parentIt != runtimeStateById.end())
                item.orbitCenterAu = parentIt->second->positionAu;
            else
                item.orbitCenterAu = glm::dvec3(0.0);
        }
        else
        {
            item.orbitCenterAu = glm::dvec3(0.0);
        }

        item.orbitRadiusAu = body.distanceAu;
        item.drawOrbit = body.distanceAu > 0.0;

        item.ringPlaneInclinationOffsetDeg =
            body.ringPlaneInclinationOffsetDeg;


        item.ringVisual =
            toSystemMapRingVisualProfile(
                body.ringVisual
            );



            for (const auto& ring :
                body.rings)
            {
                item.rings.push_back(
                    toSystemMapRing(
                        ring
                    )
                );
            }






        out.bodies.push_back(std::move(item));
    }





    for (const auto& [id, obj] : m_simulation.staticObjects())
    {
        if (obj.systemId != systemId ||
            !obj.systemMapVisible)
        {
            continue;
        }

        world::celestial::SystemMapObject mapObj;

        mapObj.id = id;
        mapObj.stableId =
            "entity:" +
            std::to_string(id.value);

        mapObj.name = obj.displayName;

        mapObj.owner = obj.ownerName;

        mapObj.systemId = obj.systemId;

        mapObj.parentBodyId = obj.mapParentBodyId;

        if (obj.type == ObjectType::Station)
        {
            mapObj.kind = world::celestial::SystemMapObjectKind::Station;
        }
        else
        {
            mapObj.kind = world::celestial::SystemMapObjectKind::Unknown;
        }

        const glm::dvec3 meters =
            world::coordinates::fullMeters(
                obj.worldPosition
            );

        mapObj.positionAu =
            meters /
            world::celestial::MetersPerAu;

        // If this static map object represents a hub module,
        // the actual orbit belongs to the owning OrbitalHubRuntime.
        if (!obj.hubId.empty())
        {
            auto hubIt =
                m_simulation.orbitalHubs().find(
                    obj.hubId
                );

            if (hubIt != m_simulation.orbitalHubs().end())
            {
                const auto& hub = hubIt->second;

                if (hub.motion.enabled)
                {
                    mapObj.hasOrbit = true;

                    mapObj.orbitCenterAu =
                        hub.motion.centerMeters /
                        world::celestial::MetersPerAu;

                    mapObj.orbitRadiusAu =
                        (
                            hub.motion.parentRadiusMeters +
                            hub.motion.altitudeMeters
                        ) /
                        world::celestial::MetersPerAu;

                    mapObj.orbitInclinationDeg = hub.motion.inclinationDeg;
                    mapObj.orbitLongitudeOfAscendingNodeDeg = hub.motion.longitudeOfAscendingNodeDeg;
                    mapObj.orbitArgumentOfPeriapsisDeg = hub.motion.argumentOfPeriapsisDeg;
                }
            }
        }
        else if (obj.orbitalMotion.enabled)
        {
            mapObj.hasOrbit = true;

            mapObj.orbitCenterAu =
                obj.orbitalMotion.centerMeters /
                world::celestial::MetersPerAu;

            mapObj.orbitRadiusAu =
                (
                    obj.orbitalMotion.parentRadiusMeters +
                    obj.orbitalMotion.altitudeMeters
                ) /
                world::celestial::MetersPerAu;

            mapObj.orbitInclinationDeg = obj.orbitalMotion.inclinationDeg;
            mapObj.orbitLongitudeOfAscendingNodeDeg = obj.orbitalMotion.longitudeOfAscendingNodeDeg;
            mapObj.orbitArgumentOfPeriapsisDeg = obj.orbitalMotion.argumentOfPeriapsisDeg;
        }

        out.objects.push_back(
            std::move(mapObj)
        );
    }

    /*
        A hub is a selectable map object of its own. Its modules remain in the
        snapshot as ordinary static objects, but they must never stand in for
        the hub identity used by Details/Hub navigation.
    */
    for (const auto& [hubId, hub] :
         m_simulation.orbitalHubs())
    {
        if (hub.systemId != systemId)
            continue;

        const auto* frame =
            m_simulation.hubNavigationFrame(
                hubId
            );

        if (!frame ||
            !frame->valid)
        {
            continue;
        }

        world::celestial::SystemMapObject mapHub;

        mapHub.stableId = hubId;
        mapHub.name =
            hub.name.empty()
                ? hubId
                : hub.name;
        mapHub.owner = hub.owner;
        mapHub.parentBodyId = hub.parentBodyId;
        mapHub.kind =
            world::celestial::
                SystemMapObjectKind::Hub;
        mapHub.positionAu =
            frame->originMeters /
            world::celestial::MetersPerAu;
        mapHub.systemId = hub.systemId;

        if (hub.motion.enabled)
        {
            mapHub.hasOrbit = true;
            mapHub.orbitCenterAu =
                hub.motion.centerMeters /
                world::celestial::MetersPerAu;
            mapHub.orbitRadiusAu =
                (
                    hub.motion.parentRadiusMeters +
                    hub.motion.altitudeMeters
                ) /
                world::celestial::MetersPerAu;
            mapHub.orbitInclinationDeg =
                hub.motion.inclinationDeg;
            mapHub.orbitLongitudeOfAscendingNodeDeg =
                hub.motion.longitudeOfAscendingNodeDeg;
            mapHub.orbitArgumentOfPeriapsisDeg =
                hub.motion.argumentOfPeriapsisDeg;
        }

        out.objects.push_back(
            std::move(mapHub)
        );
    }

    // Real ships are first-class System-map objects. A single instantaneous
    // position is not enough to invent orbit metadata, so hasOrbit stays false.
    for (const auto& [entityId, shipPtr] : m_simulation.ships())
    {
        if (!shipPtr)
            continue;

        const ShipTransform transform =
            m_simulation.presentationShipTransform(entityId);

        if (transform.motion.systemId != systemId)
            continue;

        const bool isPlayer =
            entityId == m_simulation.playerId();

        world::celestial::SystemMapObject mapShip;
        mapShip.id = entityId;
        mapShip.stableId =
            isPlayer
                ? "player"
                : "entity:" + std::to_string(entityId.value);
        const auto motionLabKind =
            m_simulation.hubMotionLabActorKind(entityId);

        mapShip.name =
            isPlayer
                ? "Player"
                : (motionLabKind !=
                        game::diagnostics::HubMotionLabActorKind::None
                    ? game::diagnostics::hubMotionLabLabel(motionLabKind)
                    : (shipPtr->core().descriptor().identity.shipName.empty()
                        ? "Ship " + std::to_string(entityId.value)
                        : shipPtr->core().descriptor().identity.shipName));
        mapShip.parentBodyId = transform.motion.parentBodyId;
        mapShip.kind =
            world::celestial::SystemMapObjectKind::Ship;
        mapShip.positionAu =
            transform.fullWorldMeters() /
            world::celestial::MetersPerAu;
        mapShip.systemId = transform.motion.systemId;
        mapShip.hasOrbit = false;

        out.objects.push_back(std::move(mapShip));
    }


    // Presentation-only analytic reference used by Hub Motion Lab. It is not a
    // gameplay entity and has no physics state; maps evaluate the same shared
    // time function only so the reference remains visible while diagnosing
    // spatial-domain transitions.
    if (game::diagnostics::HubMotionLabEnabled &&
        systemId == game::diagnostics::HubMotionLabSystemId)
    {
        const auto* frame =
            m_simulation.hubNavigationFrame(
                std::string(game::diagnostics::HubMotionLabHubId)
            );

        if (frame && frame->valid)
        {
            const auto pose =
                game::diagnostics::evaluateHubMotionLabCube(
                    m_simulation.serverTime()
                );

            world::celestial::SystemMapObject cube;
            cube.stableId = "diagnostic:hub_motion_lab_cube";
            cube.name = "LAB ANALYTIC CUBE";
            cube.parentBodyId = frame->parentBodyId;
            cube.kind = world::celestial::SystemMapObjectKind::Ship;
            cube.positionAu =
                frame->localToWorldPosition(pose.localPositionMeters) /
                world::celestial::MetersPerAu;
            cube.systemId = frame->systemId;
            cube.hasOrbit = false;
            out.objects.push_back(std::move(cube));
        }
    }



    appendSystemMapMotionDebugCsv(out, m_diagnostics);

    return out;
}








void GameServer::appendLocalDetailObjects(
    world::celestial::DetailMapSnapshot& out,
    double extentMeters,
    bool cubicBounds
) const
{
    using namespace world::celestial;

    const auto intersectsBounds =
        [&](const glm::dvec3& positionMeters,
            double objectRadiusMeters = 0.0)
        {
            const glm::dvec3 delta =
                positionMeters -
                out.planetCenterMeters;

            if (cubicBounds)
            {
                return
                    std::abs(delta.x) <=
                        extentMeters + objectRadiusMeters &&
                    std::abs(delta.y) <=
                        extentMeters + objectRadiusMeters &&
                    std::abs(delta.z) <=
                        extentMeters + objectRadiusMeters;
            }

            return
                glm::length(delta) <=
                    extentMeters + objectRadiusMeters;
        };

    for (const auto& [hubId, hub] :
         m_simulation.orbitalHubs())
    {
        if (hub.systemId != out.systemId)
            continue;

        const auto* frame =
            m_simulation.hubNavigationFrame(
                hubId
            );

        if (!frame ||
            !frame->valid ||
            frame->systemId != out.systemId ||
            !intersectsBounds(frame->originMeters))
        {
            continue;
        }

        LocalSceneObject object;
        object.stableId = hubId;
        object.name =
            hub.name.empty()
                ? hubId
                : hub.name;
        object.kind = "hub";
        object.parentStableId = hub.parentBodyId;
        object.objectClass = DetailObjectClass::Hub;
        object.origin = DetailObjectOrigin::Runtime;
        object.positionMeters =
            frame->originMeters;
        object.velocityMps =
            frame->velocityMetersPerSecond;
        object.axes.x = frame->normalAxis;
        object.axes.y = frame->radialAxis;
        object.axes.z = -frame->progradeAxis;
        object.valid = true;

        out.scene.objects.push_back(
            std::move(object)
        );
    }

    /*
        Nearby bodies are context objects, not duplicated scene roots.
        Authored and procedural asteroids use this same collection. A body
        crossing a cube boundary is included when its sphere intersects it.
    */
    if (const auto* celestial =
            celestialSnapshotForSystem(out.systemId))
    {
        for (const auto& body : celestial->bodies)
        {
            const glm::dvec3 positionMeters =
                body.positionAu *
                MetersPerAu;

            const double radiusMeters =
                std::max(
                    0.0,
                    body.radiusKm * 1000.0
                );

            if (!intersectsBounds(
                    positionMeters,
                    radiusMeters
                ))
            {
                continue;
            }

            LocalSceneObject contextBody;
            contextBody.stableId = body.id;
            contextBody.name = body.name;
            contextBody.kind = "celestial";
            contextBody.objectClass =
                DetailObjectClass::CelestialBody;
            contextBody.origin =
                DetailObjectOrigin::Authored;
            contextBody.positionMeters =
                positionMeters;
            contextBody.boundingRadiusMeters =
                radiusMeters;
            contextBody.valid = true;

            out.scene.objects.push_back(
                std::move(contextBody)
            );
        }
    }

    /*
        StaticObject is currently the runtime representation for station
        modules and other infrastructure. Its presentation kind may later be
        mine/base/beacon/relay, but its top-level class remains Hub.
    */
    for (const auto& [entityId, object] :
         m_simulation.staticObjects())
    {
        if (object.systemId != out.systemId)
            continue;

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(
                object.worldPosition
            );

        if (!intersectsBounds(positionMeters))
            continue;

        LocalSceneObject station;
        station.id = entityId;
        station.stableId =
            "entity:" +
            std::to_string(entityId.value);
        station.name = object.displayName;
        station.kind = "station";
        station.parentStableId = object.hubId;
        station.objectClass =
            DetailObjectClass::Hub;
        station.origin =
            DetailObjectOrigin::Runtime;
        station.positionMeters =
            positionMeters;
        station.velocityMps =
            glm::dvec3(object.linearVelocity);
        station.valid = true;

        out.scene.objects.push_back(
            std::move(station)
        );
    }

    for (const auto& [entityId, ship] :
         m_simulation.ships())
    {
        if (!ship)
            continue;

        const auto& core = ship->core();
        const ShipTransform transform =
            m_simulation.presentationShipTransform(entityId);

        if (transform.motion.systemId != out.systemId)
            continue;

        const glm::dvec3 positionMeters =
            transform.fullWorldMeters();

        if (!intersectsBounds(positionMeters))
            continue;

        LocalSceneObject object;
        object.id = entityId;
        object.stableId =
            "entity:" +
            std::to_string(entityId.value);
        const auto motionLabKind =
            m_simulation.hubMotionLabActorKind(entityId);

        object.name =
            motionLabKind !=
                    game::diagnostics::HubMotionLabActorKind::None
                ? game::diagnostics::hubMotionLabLabel(motionLabKind)
                : (core.descriptor().identity.shipName.empty()
                    ? "Ship " + std::to_string(entityId.value)
                    : core.descriptor().identity.shipName);
        object.kind = "ship";
        object.parentStableId = transform.motion.hubId;
        object.objectClass = DetailObjectClass::Ship;
        object.origin = DetailObjectOrigin::Runtime;
        object.role = LocalSceneObjectRole::Participant;
        object.positionMeters = positionMeters;
        object.velocityMps =
            transform.motion.worldVelocityMps;
        object.axes =
            planetMapAxesFromOrientation(
                transform.orientation
            );
        object.valid = true;

        out.scene.objects.push_back(
            std::move(object)
        );
    }


    if (game::diagnostics::HubMotionLabEnabled &&
        out.systemId == game::diagnostics::HubMotionLabSystemId)
    {
        const auto* frame =
            m_simulation.hubNavigationFrame(
                std::string(game::diagnostics::HubMotionLabHubId)
            );

        if (frame && frame->valid)
        {
            const auto pose =
                game::diagnostics::evaluateHubMotionLabCube(
                    m_simulation.serverTime()
                );

            const glm::dvec3 positionMeters =
                frame->localToWorldPosition(pose.localPositionMeters);

            if (intersectsBounds(positionMeters))
            {
                LocalSceneObject cube;
                cube.stableId = "diagnostic:hub_motion_lab_cube";
                cube.name = "LAB ANALYTIC CUBE";
                cube.kind = "diagnostic_probe";
                cube.parentStableId =
                    std::string(game::diagnostics::HubMotionLabHubId);
                cube.objectClass = DetailObjectClass::Ship;
                cube.origin = DetailObjectOrigin::Runtime;
                cube.role = LocalSceneObjectRole::Participant;
                cube.positionMeters = positionMeters;
                cube.valid = true;
                out.scene.objects.push_back(std::move(cube));
            }
        }
    }
}


world::celestial::DetailMapSnapshot
GameServer::buildDetailMapSnapshot(
    const world::celestial::DetailTarget& target
) const
{
    using namespace world::celestial;

    DetailMapSnapshot out;

    if (!target.valid())
        return out;

    switch (target.sceneKind)
    {
        case DetailSceneKind::CelestialBody:
            out = buildCelestialBodyDetailSnapshot(
                target.systemId,
                target.anchorId
            );
            break;

        case DetailSceneKind::LocalObject:
            if (target.focusClass ==
                    DetailObjectClass::Hub)
            {
                out = buildLocalObjectDetailSnapshot(
                    target.systemId,
                    target.anchorId
                );
            }
            break;

        case DetailSceneKind::SpatialVolume:
        {
            out.systemId = target.systemId;
            out.systemPositionLy =
                target.systemPositionLy;
            out.hasCentralBody = false;
            out.planetName = "Local Space";
            out.universeTimeSeconds =
                m_universeClock.timeSeconds();
            out.planetCenterMeters =
                target.spatialCell.centerAu *
                MetersPerAu;

            /*
                A navigation cube is an address, not a container that may
                swallow a planet. If the cube centre lies inside a body, use
                that body's own Details scene. A mere surface intersection
                remains SpatialVolume and is rendered as local context.
            */
            if (const auto* celestial =
                    celestialSnapshotForSystem(target.systemId))
            {
                for (const auto& body : celestial->bodies)
                {
                    const double radiusMeters =
                        std::max(
                            0.0,
                            body.radiusKm * 1000.0
                        );

                    if (radiusMeters <= 0.0)
                        continue;

                    const glm::dvec3 bodyCenterMeters =
                        body.positionAu *
                        MetersPerAu;

                    if (glm::length(
                            out.planetCenterMeters -
                            bodyCenterMeters
                        ) >= radiusMeters)
                    {
                        continue;
                    }

                    DetailTarget bodyTarget = target;
                    bodyTarget.sceneKind =
                        DetailSceneKind::CelestialBody;
                    bodyTarget.focusClass =
                        DetailObjectClass::CelestialBody;
                    bodyTarget.anchorId = body.id;
                    bodyTarget.focusId = body.id;

                    out = buildCelestialBodyDetailSnapshot(
                        target.systemId,
                        body.id
                    );

                    if (out.valid)
                    {
                        out.detailTarget =
                            std::move(bodyTarget);
                    }

                    return out;
                }
            }

            out.detailHalfExtentMeters =
                target.spatialCell.edgeAu *
                MetersPerAu * 0.5;
            out.valid =
                out.detailHalfExtentMeters > 0.0;

            if (out.valid)
            {
                appendLocalDetailObjects(
                    out,
                    out.detailHalfExtentMeters,
                    true
                );
            }
            break;
        }

        default:
            break;
    }

    if (out.valid)
    {
        out.detailTarget = target;
        out.scene.anchorClass =
            target.sceneKind == DetailSceneKind::CelestialBody
                ? DetailObjectClass::CelestialBody
                : target.focusClass;
        out.scene.anchorId = target.anchorId;
        out.scene.focusId = target.focusId;
        out.scene.coordinateSpace =
            LocalSceneCoordinateSpace::SystemWorldMeters;
        out.scene.originWorldMeters = out.planetCenterMeters;
        out.scene.halfExtentMeters = out.detailHalfExtentMeters;
    }

    return out;
}


world::celestial::DetailMapSnapshot
GameServer::buildLocalObjectDetailSnapshot(
    int systemId,
    const std::string& anchorHubId
) const
{
    using namespace world::celestial;

    DetailMapSnapshot out;
    out.systemId = systemId;
    out.hasCentralBody = false;
    out.detailAnchorHubId = anchorHubId;
    out.planetName = "Deep Space";
    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    if (anchorHubId.empty())
        return out;

    const auto hubIt =
        m_simulation.orbitalHubs().find(
            anchorHubId
        );

    const auto* anchorFrame =
        m_simulation.hubNavigationFrame(
            anchorHubId
        );

    if (hubIt == m_simulation.orbitalHubs().end() ||
        hubIt->second.systemId != systemId ||
        !anchorFrame ||
        !anchorFrame->valid ||
        anchorFrame->systemId != systemId)
    {
        return out;
    }

    if (const auto* summary =
            m_starAtlas.findSystemSummary(systemId))
    {
        out.systemPositionLy =
            summary->positionLy;
    }

    out.planetCenterMeters =
        anchorFrame->originMeters;
    out.planetVelocityMps =
        anchorFrame->velocityMetersPerSecond;
    out.valid = true;

    out.scene.anchorClass = DetailObjectClass::Hub;
    out.scene.anchorId = anchorHubId;
    out.scene.focusId = anchorHubId;
    out.scene.coordinateSpace =
        LocalSceneCoordinateSpace::SystemWorldMeters;
    out.scene.originWorldMeters = out.planetCenterMeters;
    out.scene.halfExtentMeters = 5.0e9;

    /*
        LocalObject Details is intentionally bounded independently from the
        navigation cube. The object is the scene anchor; nearby content is
        merely context.
    */
    constexpr double LocalDetailRadiusMeters =
        5.0e9;

    appendLocalDetailObjects(
        out,
        LocalDetailRadiusMeters,
        false
    );

    return out;
}


void GameServer::debugLogDetailMapSnapshot(
    const world::celestial::DetailMapSnapshot& snapshot
) const
{
    if (!m_diagnostics.settings.detailMapSnapshotCsv)
        return;

    auto& row =
        m_diagnostics.server.detailMapSnapshotRows;

    if (row >= 300)
        return;

    std::ofstream out(
        "planet_map_snapshot_debug.csv",
        row == 0 ? std::ios::out : std::ios::app
    );

    if (!out.is_open())
        return;

    if (row == 0)
    {
        out
            << "row,systemId,planetBodyId,valid,"
            << "hubCount,stationCount,shipCount,"
            << "hubOrbitCount,playerOrbitCount\n";
    }

    using world::celestial::DetailObjectClass;
    using world::celestial::LocalSceneObject;

    const auto countObjects =
        [&](DetailObjectClass objectClass,
            const std::string& kind = std::string())
        {
            return std::count_if(
                snapshot.scene.objects.begin(),
                snapshot.scene.objects.end(),
                [&](const LocalSceneObject& object)
                {
                    return
                        object.objectClass == objectClass &&
                        (kind.empty() || object.kind == kind);
                }
            );
        };

    out
        << row << ","
        << snapshot.systemId << ","
        << snapshot.planetBodyId << ","
        << (snapshot.valid ? 1 : 0) << ","
        << countObjects(DetailObjectClass::Hub, "hub") << ","
        << countObjects(DetailObjectClass::Hub) -
            countObjects(DetailObjectClass::Hub, "hub") << ","
        << countObjects(DetailObjectClass::Ship) << ","
        << snapshot.hubOrbits.size() << ","
        << snapshot.playerOrbits.size()
        << "\n";

    ++row;
}


world::celestial::DetailMapSnapshot
GameServer::buildCelestialBodyDetailSnapshot(
    int systemId,
    const std::string& planetBodyId
) const
{
    using namespace world::celestial;

    DetailMapSnapshot out;

    out.systemId = systemId;

    out.planetBodyId = planetBodyId;

    out.universeTimeSeconds = m_universeClock.timeSeconds();

    const auto* system = m_starAtlas.findSystem(systemId);

    if (!system)
        return out;

    if (const auto* summary =
            m_starAtlas.findSystemSummary(
                systemId
            ))
    {
        out.systemPositionLy =
            summary->positionLy;
    }

    const auto* celestial =
        celestialSnapshotForSystem(systemId);

    if (!celestial)
        return out;

    bool foundPlanet = false;

    for (const auto& body : celestial->bodies)
    {
        if (body.id != planetBodyId)
            continue;

        out.planetName = body.name;
        out.environmentPresetId = body.environmentPresetId;

        out.planetCenterMeters =
            body.positionAu *
            world::celestial::MetersPerAu;

        out.planetRadiusMeters =
            body.radiusKm * 1000.0;

        out.planetRotationPhaseRad =
            body.rotationPhaseRad;

        out.planetDayLengthHours =
            body.dayLengthHours;

        out.planetRotationDirection =
            body.rotationDirection;

        out.planetAxialTiltDeg =
            body.axialTiltDeg;

        out.planetAxisNodeDeg =
            body.axisNodeDeg;

        out.planetTextureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        foundPlanet = true;
        break;
    }

    if (!foundPlanet)
        return out;


    for (const auto& definition :
        system->bodies)
    {
        if (definition.id != planetBodyId)
            continue;

        out.gravitationalParameterM3s2 =
            definition.gravitationalParameterM3s2;

        out.ringPlaneInclinationOffsetDeg =
            definition.ringPlaneInclinationOffsetDeg;

        out.ringVisual =
            toSystemMapRingVisualProfile(
                definition.ringVisual
            );

        out.rings.clear();



        for (const auto& sourceRing :
            definition.rings)
        {
            out.rings.push_back(
                toSystemMapRing(
                    sourceRing
                )
            );
        }





        break;
    }

    // -----------------------------
    // Hubs + hub rail orbits
    // -----------------------------


    out.valid = true;

    out.scene.anchorClass = DetailObjectClass::CelestialBody;
    out.scene.anchorId = planetBodyId;
    out.scene.focusId = planetBodyId;
    out.scene.coordinateSpace =
        LocalSceneCoordinateSpace::SystemWorldMeters;
    out.scene.originWorldMeters = out.planetCenterMeters;

    /*
        Статическая часть snapshot уже построена:
            - planet definition;
            - radius;
            - rings;
            - environment;
            - gravitational parameter.

        Все динамические объекты строятся только в одном месте.
    */
    refreshDetailMapDynamicState(
        out
    );







    debugLogDetailMapSnapshot(out);












    return out;
}












void GameServer::refreshDetailMapDynamicState(
    world::celestial::DetailMapSnapshot& snapshot
) const
{
    using namespace world::celestial;

    if (!snapshot.valid ||
        snapshot.systemId < 0)
    {
        return;
    }

    if (!snapshot.hasCentralBody)
    {
        if (snapshot.detailTarget.valid())
        {
            snapshot =
                buildDetailMapSnapshot(
                    snapshot.detailTarget
                );
        }
        else if (!snapshot.detailAnchorHubId.empty())
        {
            snapshot =
                buildLocalObjectDetailSnapshot(
                    snapshot.systemId,
                    snapshot.detailAnchorHubId
                );
        }

        return;
    }

    if (snapshot.planetBodyId.empty())
        return;

    /*
        Один временной срез для всей карты.

        Положение планеты, хабов, станций и корабля
        должно относиться к одному серверному кадру.
    */
    snapshot.universeTimeSeconds =
        m_universeClock.timeSeconds();

    // ------------------------------------------------------------
    // 1. Текущее состояние выбранной планеты.
    // ------------------------------------------------------------

    const auto* celestial =
        celestialSnapshotForSystem(snapshot.systemId);

    if (!celestial)
    {
        snapshot.valid = false;
        return;
    }

    const world::celestial::CelestialBodyState* currentPlanet = nullptr;

    for (const auto& body : celestial->bodies)
    {
        if (body.id == snapshot.planetBodyId)
        {
            currentPlanet = &body;
            break;
        }
    }

    if (!currentPlanet)
    {
        snapshot.valid = false;
        return;
    }

    snapshot.planetCenterMeters = currentPlanet->worldMeters;
    snapshot.planetVelocityMps =
        currentPlanet->worldVelocityMetersPerSecond;

    if (currentPlanet->radiusKm > 0.0)
        snapshot.planetRadiusMeters = currentPlanet->radiusKm * 1000.0;

    snapshot.planetRotationPhaseRad =
        currentPlanet->rotationPhaseRad;
    snapshot.planetDayLengthHours =
        currentPlanet->dayLengthHours;
    snapshot.planetRotationDirection =
        currentPlanet->rotationDirection;
    snapshot.planetAxialTiltDeg =
        currentPlanet->axialTiltDeg;
    snapshot.planetAxisNodeDeg =
        currentPlanet->axisNodeDeg;
    snapshot.planetTextureLongitudeOffsetDeg =
        currentPlanet->textureLongitudeOffsetDeg;

    snapshot.scene.originWorldMeters = snapshot.planetCenterMeters;

    // ------------------------------------------------------------
    // 2. Dynamic arrays полностью пересобираются.
    //
    // Так исчезают:
    //   - старые позиции;
    //   - удалённые объекты;
    //   - отсутствующие новые хабы;
    //   - замороженная орбита игрока.
    // ------------------------------------------------------------

    snapshot.scene.objects.erase(
        std::remove_if(
            snapshot.scene.objects.begin(),
            snapshot.scene.objects.end(),
            [](const LocalSceneObject& object)
            {
                return
                    object.objectClass !=
                        DetailObjectClass::CelestialBody;
            }
        ),
        snapshot.scene.objects.end()
    );
    snapshot.hubOrbits.clear();
    snapshot.playerOrbits.clear();

    /*
        Строит planet-relative orbital frame из абсолютного
        серверного положения и абсолютной скорости.

        positionMeters и worldVelocityMps остаются мировыми.
        Relative velocity используется только для orbital axes.
    */
    auto resolveOrbitalAxes =
        [&](
            const glm::dvec3& positionMeters,
            const glm::dvec3& worldVelocityMps,
            const glm::dvec3& fallbackPrograde,
            const glm::dvec3& fallbackNormal,
            glm::dvec3& outRadial,
            glm::dvec3& outPrograde,
            glm::dvec3& outNormal
        )
        {
            outRadial =
                safeNormalizePlanetMap(
                    positionMeters -
                        snapshot.planetCenterMeters,

                    glm::dvec3(
                        0.0,
                        1.0,
                        0.0
                    )
                );

            const glm::dvec3 relativeVelocity =
                worldVelocityMps -
                snapshot.planetVelocityMps;

            const glm::dvec3 tangentialVelocity =
                relativeVelocity -
                outRadial *
                    glm::dot(
                        relativeVelocity,
                        outRadial
                    );

            const glm::dvec3 fallbackTangential =
                fallbackPrograde -
                outRadial *
                    glm::dot(
                        fallbackPrograde,
                        outRadial
                    );

            outPrograde =
                safeNormalizePlanetMap(
                    tangentialVelocity,

                    safeNormalizePlanetMap(
                        fallbackTangential,

                        glm::dvec3(
                            1.0,
                            0.0,
                            0.0
                        )
                    )
                );

            /*
                Сохраняем принятую в проекте конвенцию:

                    normal = cross(prograde, radial)
            */
            outNormal =
                safeNormalizePlanetMap(
                    glm::cross(
                        outPrograde,
                        outRadial
                    ),

                    safeNormalizePlanetMap(
                        fallbackNormal,

                        glm::dvec3(
                            0.0,
                            0.0,
                            1.0
                        )
                    )
                );

            /*
                Повторно ортогонализируем prograde.
            */
            outPrograde =
                safeNormalizePlanetMap(
                    glm::cross(
                        outRadial,
                        outNormal
                    ),
                    outPrograde
                );
        };

    // ------------------------------------------------------------
    // 3. Все orbital hubs выбранной планеты.
    // ------------------------------------------------------------

    for (const auto& [hubId, hub] :
         m_simulation.orbitalHubs())
    {
        if (hub.systemId !=
                snapshot.systemId ||
            hub.parentBodyId !=
                snapshot.planetBodyId)
        {
            continue;
        }

        const auto* frame =
            m_simulation.hubNavigationFrame(
                hubId
            );

        if (!frame ||
            !frame->valid ||
            frame->systemId != snapshot.systemId)
        {
            continue;
        }

        glm::dvec3 radial;
        glm::dvec3 prograde;
        glm::dvec3 normal;

        resolveOrbitalAxes(
            frame->originMeters,
            frame->velocityMetersPerSecond,
            frame->progradeAxis,
            frame->normalAxis,
            radial,
            prograde,
            normal
        );

        LocalSceneObject hubObject;

        hubObject.stableId =
            hubId;

        hubObject.name =
            hub.name.empty()
                ? hub.id
                : hub.name;

        hubObject.kind =
            "hub";
        hubObject.parentStableId =
            snapshot.planetBodyId;
        hubObject.objectClass =
            DetailObjectClass::Hub;
        hubObject.origin =
            DetailObjectOrigin::Runtime;

        /*
            Абсолютная авторитетная серверная позиция.
        */
        hubObject.positionMeters =
            frame->originMeters;

        /*
            Абсолютная авторитетная серверная скорость.
        */
        hubObject.velocityMps =
            frame->velocityMetersPerSecond;

        /*
            Ориентация hub orbital frame:
                X = normal;
                Y = radial;
                Z = -prograde.
        */
        hubObject.axes.x =
            normal;

        hubObject.axes.y =
            radial;

        hubObject.axes.z =
            -prograde;

        hubObject.valid =
            true;

        snapshot.scene.objects.push_back(
            hubObject
        );

        DetailMapOrbit orbit;

        orbit.id =
            hubId +
            "_rail_orbit";

        orbit.name =
            hubObject.name +
            " rail orbit";

        orbit.parentBodyId =
            snapshot.planetBodyId;

        orbit.centerMeters =
            snapshot.planetCenterMeters;

        orbit.positionMeters =
            frame->originMeters;

        orbit.velocityMps =
            frame->velocityMetersPerSecond;

        const glm::dvec3 fromPlanet =
            orbit.positionMeters -
            snapshot.planetCenterMeters;

        const glm::dvec3 relativeVelocity =
            orbit.velocityMps -
            snapshot.planetVelocityMps;

        orbit.radiusMeters =
            glm::length(
                fromPlanet
            );

        orbit.altitudeMeters =
            orbit.radiusMeters -
            snapshot.planetRadiusMeters;

        /*
            Orbital speed в Planet Details означает скорость
            относительно выбранной планеты.
        */
        orbit.speedMps =
            glm::length(
                relativeVelocity
            );

        orbit.radialAxis =
            radial;

        orbit.progradeAxis =
            prograde;

        orbit.normalAxis =
            normal;

        orbit.valid =
            true;

        snapshot.hubOrbits.push_back(
            orbit
        );
    }

    // ------------------------------------------------------------
    // 4. Станции и другие static objects.
    // ------------------------------------------------------------

    for (const auto& [id, object] :
         m_simulation.staticObjects())
    {
        if (object.systemId !=
                snapshot.systemId ||
            object.mapParentBodyId !=
                snapshot.planetBodyId)
        {
            continue;
        }

        if (object.type !=
            ObjectType::Station)
        {
            continue;
        }

        LocalSceneObject station;

        station.id =
            id;

        station.stableId =
            std::to_string(
                id.value
            );

        station.name =
            object.displayName.empty()
                ? "Station"
                : object.displayName;

        station.kind =
            "station";
        station.parentStableId =
            object.hubId.empty()
                ? object.mapParentBodyId
                : object.hubId;
        station.objectClass =
            DetailObjectClass::Hub;
        station.origin =
            DetailObjectOrigin::Runtime;

        station.positionMeters =
            world::coordinates::fullMeters(
                object.worldPosition
            );

        station.axes =
            planetMapAxesFromOrientation(
                object.orientation
            );

        /*
            Вычисляем абсолютную мировую скорость станции
            по тому же kinematic model, по которому сервер
            меняет её позицию.
        */
        glm::dvec3 stationWorldVelocityMps =
            glm::dvec3(
                object.linearVelocity
            );

        if (object.attachedToHub &&
            !object.hubId.empty())
        {
            const auto* frame =
                m_simulation.hubNavigationFrame(
                    object.hubId
                );

            if (frame &&
                frame->valid)
            {
                /*
                    Трансляционная скорость центра hub frame.
                */
                stationWorldVelocityMps =
                    frame->velocityMetersPerSecond;

                /*
                    Если offset вращается вместе с orbital frame,
                    добавляем omega × offset.

                    Без этого модуль на удалении от центра хаба
                    имел бы правильную позицию, но немного
                    неправильный velocity vector.
                */
                if (object.inheritHubOrientation)
                {
                    glm::dvec3 hubRadial;
                    glm::dvec3 hubPrograde;
                    glm::dvec3 hubNormal;

                    resolveOrbitalAxes(
                        frame->originMeters,
                        frame->velocityMetersPerSecond,
                        frame->progradeAxis,
                        frame->normalAxis,
                        hubRadial,
                        hubPrograde,
                        hubNormal
                    );

                    const glm::dvec3 hubRelativeVelocity =
                        frame->velocityMetersPerSecond -
                        snapshot.planetVelocityMps;

                    const glm::dvec3 hubTangentialVelocity =
                        hubRelativeVelocity -
                        hubRadial *
                            glm::dot(
                                hubRelativeVelocity,
                                hubRadial
                            );

                    const double orbitRadiusMeters =
                        glm::length(
                            frame->originMeters -
                            snapshot.planetCenterMeters
                        );

                    const double angularSpeedRadPerSecond =
                        glm::length(
                            hubTangentialVelocity
                        ) /
                        std::max(
                            1.0,
                            orbitRadiusMeters
                        );

                    /*
                        При конвенции:
                            normal = cross(prograde, radial)

                        вектор угловой скорости направлен
                        в сторону -normal.
                    */
                    const glm::dvec3 angularVelocity =
                        -hubNormal *
                        angularSpeedRadPerSecond;

                    const glm::dvec3 offsetFromHub =
                        station.positionMeters -
                        frame->originMeters;

                    stationWorldVelocityMps +=
                        glm::cross(
                            angularVelocity,
                            offsetFromHub
                        );
                }
            }
        }
        else if (object.orbitalMotion.enabled)
        {
            glm::dvec3 parentVelocityMps {
                0.0
            };

            if (!object.orbitalParentBodyId.empty())
            {
                m_simulation
                    .resolveCelestialBodyVelocityMetersPerSecond(
                        object.systemId,
                        object.orbitalParentBodyId,
                        parentVelocityMps
                    );
            }

            /*
                computeOrbitVelocity... возвращает локальную
                скорость относительно orbital center.

                Добавляем абсолютную скорость родителя.
            */
            stationWorldVelocityMps =
                parentVelocityMps +
                world::orbits::
                    computeOrbitVelocityMetersPerSecond(
                        object.orbitalMotion,
                        snapshot.universeTimeSeconds
                    );
        }

        station.velocityMps =
            stationWorldVelocityMps;

        station.valid =
            true;

        snapshot.scene.objects.push_back(
            station
        );
    }

    // ------------------------------------------------------------
    // 5. Ships.
    //
    // Detail uses the same presentation branch as SimulationSnapshot/Hub.
    // This is important both for NPC visibility and for transactional
    // accelerated diagnostics: no map is allowed to bypass the alternate
    // diagnostic transform and read a frozen production transform directly.
    // ------------------------------------------------------------

    constexpr double planetMapObjectRadiusMeters =
        100000000.0;

    for (const auto& [entityId, shipPtr] :
         m_simulation.ships())
    {
        if (!shipPtr)
            continue;

        const ShipTransform transform =
            m_simulation.presentationShipTransform(entityId);

        if (transform.motion.systemId != snapshot.systemId)
            continue;

        const glm::dvec3 shipPositionMeters =
            world::coordinates::fullMeters(
                transform.worldPosition
            );

        const double shipDistanceMeters =
            glm::length(
                shipPositionMeters -
                snapshot.planetCenterMeters
            );

        if (shipDistanceMeters >
            planetMapObjectRadiusMeters)
        {
            continue;
        }

        const bool isPlayer =
            entityId == m_simulation.playerId();

        LocalSceneObject shipObject;

        shipObject.id = entityId;
        shipObject.stableId =
            isPlayer
                ? "player"
                : "entity:" + std::to_string(entityId.value);

        const auto motionLabKind =
            m_simulation.hubMotionLabActorKind(entityId);

        shipObject.name =
            isPlayer
                ? "Player"
                : (motionLabKind !=
                        game::diagnostics::HubMotionLabActorKind::None
                    ? game::diagnostics::hubMotionLabLabel(motionLabKind)
                    : (shipPtr->core().descriptor().identity.shipName.empty()
                        ? "Ship " + std::to_string(entityId.value)
                        : shipPtr->core().descriptor().identity.shipName));

        shipObject.kind =
            isPlayer
                ? "player"
                : "ship";
        shipObject.objectClass =
            DetailObjectClass::Ship;
        shipObject.origin =
            DetailObjectOrigin::Runtime;
        shipObject.role =
            LocalSceneObjectRole::Participant;
        shipObject.parentStableId =
            transform.motion.hubId;

        shipObject.positionMeters =
            shipPositionMeters;

        shipObject.velocityMps =
            transform.motion.worldVelocityMps;

        shipObject.axes =
            planetMapAxesFromOrientation(
                transform.orientation
            );

        shipObject.valid =
            true;

        snapshot.scene.objects.push_back(
            std::move(shipObject)
        );
    }

    if (game::diagnostics::HubMotionLabEnabled &&
        snapshot.systemId == game::diagnostics::HubMotionLabSystemId)
    {
        const auto* frame =
            m_simulation.hubNavigationFrame(
                std::string(game::diagnostics::HubMotionLabHubId)
            );

        if (frame && frame->valid)
        {
            const auto pose =
                game::diagnostics::evaluateHubMotionLabCube(
                    m_simulation.serverTime()
                );

            const glm::dvec3 cubeWorldMeters =
                frame->localToWorldPosition(pose.localPositionMeters);

            if (glm::length(cubeWorldMeters - snapshot.planetCenterMeters) <=
                planetMapObjectRadiusMeters)
            {
                LocalSceneObject cube;
                cube.stableId = "diagnostic:hub_motion_lab_cube";
                cube.name = "LAB ANALYTIC CUBE";
                cube.kind = "diagnostic_probe";
                cube.objectClass = DetailObjectClass::Ship;
                cube.origin = DetailObjectOrigin::Runtime;
                cube.role = LocalSceneObjectRole::Participant;
                cube.parentStableId =
                    std::string(game::diagnostics::HubMotionLabHubId);
                cube.positionMeters = cubeWorldMeters;
                cube.sizeMeters = glm::dvec3(pose.halfExtentMeters * 2.0);
                cube.valid = true;
                snapshot.scene.objects.push_back(std::move(cube));
            }
        }
    }

    /*
        ВАЖНО.

        playerOrbits намеренно остаётся пустым.

        Текущий DetailMapOrbit умеет рисовать только окружность:
            center + radial*cos(a)*radius
                   + prograde*sin(a)*radius

        Это не является реальной траекторией свободного корабля,
        HubTactical-корабля или объекта на эллиптической орбите.

        До появления настоящего osculating-conic renderer
        рисовать жёлтую "орбиту" нельзя — это ложная навигационная
        информация.
    */
}













void GameServer::setDiagnosticsSettings(
    const game::diagnostics::ServerDiagnosticsSettings& settings
)
{
    m_diagnostics.settings = settings;
    m_diagnostics.resetCaptureState();
}


const game::diagnostics::ServerDiagnosticsSettings&
GameServer::diagnosticsSettings() const
{
    return m_diagnostics.settings;
}


void GameServer::resetDiagnosticsCapture()
{
    m_diagnostics.resetCaptureState();
}


void GameServer::setDebugFastUniverseTime(bool enabled)
{
    setDebugUniverseTimeSimulation(
        enabled,
        m_debugFastUniverseTimeScale
    );
}

bool GameServer::debugFastUniverseTime() const
{
    return
        debugUniverseTimeSimulation() &&
        std::abs(
            debugUniverseTimeScale() -
            m_debugFastUniverseTimeScale
        ) < 0.0001;
}

void GameServer::setDebugUniverseTimeSimulation(
    bool enabled,
    double timeScale
)
{
    const bool wasEnabled =
        m_universeClock.simulationMode();
    const double previousEffectiveScale =
        m_universeClock.timeScale();

    if (enabled && !wasEnabled)
    {
        m_pendingUniverseTrajectoryDiagnosticEntry = true;
        m_pendingUniverseTrajectoryDiagnosticEpochSeconds =
            m_lastUniverseTimeSeconds;
    }
    else if (!enabled)
    {
        m_pendingUniverseTrajectoryDiagnosticEntry = false;
        m_pendingUniverseTrajectoryDiagnosticEpochSeconds = 0.0;
    }

    m_universeClock.setTimeScale(timeScale);
    m_universeClock.setSimulationMode(enabled);

    const bool timelineChanged =
        wasEnabled != m_universeClock.simulationMode() ||
        std::abs(
            previousEffectiveScale - m_universeClock.timeScale()
        ) > 1.0e-12;

    if (timelineChanged)
        ++m_universeTimelineRevision;

    m_debugFastUniverseTime = debugFastUniverseTime();

    // Publish the changed time contract on the next authoritative tick so
    // every client observes the new mode and scale without waiting for the
    // normal snapshot cadence.
    m_forceSnapshotPublication = true;
}

bool GameServer::debugUniverseTimeSimulation() const
{
    return m_universeClock.simulationMode();
}

double GameServer::debugUniverseTimeScale() const
{
    return m_universeClock.timeScale();
}

double GameServer::debugUniverseTimeConfiguredScale() const
{
    return m_universeClock.configuredTimeScale();
}



world::celestial::HubMapSnapshot
GameServer::buildHubMapSnapshot(
    int systemId,
    const std::string& hubId
) const
{
    using namespace world::celestial;

    HubMapSnapshot out;

    out.systemId =
        systemId;

    out.hubId =
        hubId;

    out.universeTimeSeconds =
        m_universeClock.timeSeconds();

    auto hubIt =
        m_simulation
            .orbitalHubs()
            .find(
                hubId
            );

    if (hubIt ==
        m_simulation
            .orbitalHubs()
            .end())
    {
        return out;
    }

    const auto& hub =
        hubIt->second;

    if (hub.systemId != systemId)
        return out;

    const auto* frame =
        m_simulation.hubNavigationFrame(
            hubId
        );

    if (!frame ||
        !frame->valid ||
        frame->systemId != systemId)
    {
        return out;
    }

    out.parentBodyId =
        hub.parentBodyId;

    if (const auto* summary =
            m_starAtlas.findSystemSummary(
                systemId
            ))
    {
        out.systemPositionLy =
            summary->positionLy;
    }

    const auto* system =
        m_starAtlas.findSystem(
            systemId
        );

    if (system)
    {
        for (const auto& body :
             system->bodies)
        {
            if (body.id !=
                out.parentBodyId)
            {
                continue;
            }

            out.parentEnvironmentPresetId =
                body.environmentPresetId;

            out.parentPlanetAxialTiltDeg =
                body.axialTiltDeg;

            out.parentPlanetAxisNodeDeg =
                body.axisNodeDeg;

            out.parentPlanetTextureLongitudeOffsetDeg =
                body.textureLongitudeOffsetDeg;

            break;
        }
    }

    out.displayName =
        hub.name.empty()
            ? hub.id
            : hub.name;

    out.scene.anchorClass = DetailObjectClass::Hub;
    out.scene.anchorId = hubId;
    out.scene.focusId = hubId;
    out.scene.coordinateSpace =
        LocalSceneCoordinateSpace::AnchorLocalMeters;
    out.scene.originWorldMeters = frame->originMeters;

    /*
        Локальная Hub Map convention:

            X = prograde;
            Y = radial;
            Z = normal.
    */
    out.hubAxes.x =
        glm::dvec3(
            1.0,
            0.0,
            0.0
        );

    out.hubAxes.y =
        glm::dvec3(
            0.0,
            1.0,
            0.0
        );

    out.hubAxes.z =
        glm::dvec3(
            0.0,
            0.0,
            1.0
        );

    /*
        Эти значения являются параметрами определения орбиты.

        Текущий фактический radius и altitude дополнительно
        пересчитываются в refreshHubMapDynamicState().
    */
    out.parentPlanetRadiusMeters =
        hub.motion.parentRadiusMeters;

    out.hubAltitudeMeters =
        hub.motion.altitudeMeters;

    out.hubOrbitRadiusMeters =
        hub.motion.parentRadiusMeters +
        hub.motion.altitudeMeters;

    /*
        Теперь snapshot считается валидным по статической части.

        Все текущие координаты, оси, modules и ships
        строятся в единственной функции ниже.
    */
    out.valid =
        true;

    refreshHubMapDynamicState(
        out
    );

    return out;
}



void GameServer::refreshHubMapDynamicState(
    world::celestial::HubMapSnapshot& snapshot
) const
{
    using namespace world::celestial;

    if (snapshot.systemId < 0 ||
        snapshot.hubId.empty())
    {
        return;
    }

    auto hubIt =
        m_simulation
            .orbitalHubs()
            .find(
                snapshot.hubId
            );

    if (hubIt ==
        m_simulation
            .orbitalHubs()
            .end())
    {
        snapshot.valid =
            false;

        return;
    }

    const auto& hub =
        hubIt->second;

    const auto* frame =
        m_simulation.hubNavigationFrame(
            snapshot.hubId
        );

    if (!frame ||
        !frame->valid)
    {
        snapshot.valid =
            false;

        return;
    }

    /*
        Один временной срез для всего snapshot.
    */
    const double currentUniverseTimeSeconds =
        m_universeClock.timeSeconds();

    snapshot.universeTimeSeconds =
        currentUniverseTimeSeconds;

    // ------------------------------------------------------------
    // 1. Текущее абсолютное состояние хаба.
    // ------------------------------------------------------------

    snapshot.hubWorldPositionMeters =
        frame->originMeters;

    snapshot.scene.originWorldMeters =
        frame->originMeters;

    snapshot.hubWorldVelocityMps =
        frame->velocityMetersPerSecond;

    snapshot.hubWorldAxes.x =
        frame->progradeAxis;

    snapshot.hubWorldAxes.y =
        frame->radialAxis;

    snapshot.hubWorldAxes.z =
        frame->normalAxis;

    // ------------------------------------------------------------
    // 2. Родительская планета из того же CelestialRuntimeRegistry.
    // ------------------------------------------------------------

    const auto* celestial =
        celestialSnapshotForSystem(snapshot.systemId);

    if (!celestial)
    {
        snapshot.valid = false;
        return;
    }

    const world::celestial::CelestialBodyState* parentPlanet = nullptr;

    for (const auto& body : celestial->bodies)
    {
        if (body.id == snapshot.parentBodyId)
        {
            parentPlanet = &body;
            break;
        }
    }

    if (!parentPlanet)
    {
        snapshot.valid = false;
        return;
    }

    snapshot.parentPlanetWorldPositionMeters =
        parentPlanet->worldMeters;
    snapshot.parentPlanetWorldVelocityMps =
        parentPlanet->worldVelocityMetersPerSecond;
    snapshot.parentPlanetCenterLocalMeters =
        frame->worldToLocalPosition(parentPlanet->worldMeters);

    if (parentPlanet->radiusKm > 0.0)
    {
        snapshot.parentPlanetRadiusMeters =
            parentPlanet->radiusKm * 1000.0;
    }

    snapshot.parentPlanetRotationPhaseRad =
        parentPlanet->rotationPhaseRad;
    snapshot.parentPlanetAxialTiltDeg =
        parentPlanet->axialTiltDeg;
    snapshot.parentPlanetAxisNodeDeg =
        parentPlanet->axisNodeDeg;
    snapshot.parentPlanetTextureLongitudeOffsetDeg =
        parentPlanet->textureLongitudeOffsetDeg;

    // Authored radius is only a guide parameter. It never replaces the
    // authoritative relative pose above.
    snapshot.hubOrbitRadiusMeters =
        std::max(
            1.0,
            snapshot.parentPlanetRadiusMeters +
                hub.motion.altitudeMeters
        );
    snapshot.hubAltitudeMeters = hub.motion.altitudeMeters;

    const double measuredOrbitRadiusMeters =
        glm::length(snapshot.parentPlanetCenterLocalMeters);
    const double authoredOrbitRadiusMeters =
        snapshot.hubOrbitRadiusMeters;

    // ------------------------------------------------------------
    // 4. Modules.
    //
    // Полностью пересобираем массив из текущего server state.
    // Так из snapshot исчезают удалённые или перемещённые модули.
    // ------------------------------------------------------------

    snapshot.scene.objects.clear();

    for (const auto& [id, object] :
         m_simulation.staticObjects())
    {
        if (object.systemId != snapshot.systemId ||
            object.hubId !=
            snapshot.hubId)
        {
            continue;
        }

        HubMapModule module;

        module.id =
            id;

        module.typeId =
            object.type;

        module.stableId =
            std::to_string(
                id.value
            );

        module.name =
            object.displayName.empty()
                ? "Hub module"
                : object.displayName;

        module.kind =
            object.type ==
                ObjectType::Station
                    ? "station"
                    : "module";
        module.objectClass = DetailObjectClass::Hub;
        module.role = LocalSceneObjectRole::Component;
        module.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        module.parentStableId = snapshot.hubId;

        const glm::dvec3 moduleWorldMeters =
            world::coordinates::fullMeters(
                object.worldPosition
            );

        module.positionMeters =
            frame->worldToLocalPosition(
                moduleWorldMeters
            );

        module.axes =
            axesToHubLocal(
                object.orientation,
                *frame
            );

        module.sizeMeters =
            assemblySizeMetersForType(
                object.type
            );

        module.prime =
            !hub.modules.empty() &&
            hub.modules.front().value ==
                id.value;

        module.valid =
            true;

        snapshot.scene.objects.push_back(
            module
        );
    }

    // ------------------------------------------------------------
    // 5. Ships.
    //
    // Hub map consumes the same presentation branch as the main simulation
    // snapshot. During accelerated diagnostics this includes every real ship
    // associated with the hub, while production transforms remain untouched.
    // ------------------------------------------------------------

    for (const auto& [entityId, shipPtr] :
         m_simulation.ships())
    {
        if (!shipPtr)
            continue;

        const bool isPlayer =
            entityId == m_simulation.playerId();

        const ShipTransform transform =
            m_simulation.presentationShipTransform(entityId);

        if (transform.motion.systemId != snapshot.systemId)
            continue;

        const bool usesThisHubFrame =
            transform.motion.hubId == snapshot.hubId;

        // Preserve the historical player marker even when inspecting the
        // current hub during a transition; other ships belong only to their
        // actual hub frame.
        if (!usesThisHubFrame && !isPlayer)
            continue;

        const glm::dvec3 shipWorldMeters =
            world::coordinates::fullMeters(
                transform.worldPosition
            );

        HubMapShip ship;
        ship.id = entityId;
        ship.typeId = shipPtr->typeId();
        const auto motionLabKind =
            m_simulation.hubMotionLabActorKind(entityId);

        ship.name =
            isPlayer
                ? "Player"
                : (motionLabKind !=
                        game::diagnostics::HubMotionLabActorKind::None
                    ? game::diagnostics::hubMotionLabLabel(motionLabKind)
                    : (shipPtr->core().descriptor().identity.shipName.empty()
                        ? "Ship " + std::to_string(entityId.value)
                        : shipPtr->core().descriptor().identity.shipName));
        ship.kind = "ship";
        ship.objectClass = DetailObjectClass::Ship;
        ship.role = LocalSceneObjectRole::Participant;
        ship.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        ship.parentStableId = snapshot.hubId;

        /*
            HubTactical is canonical in local coordinates. Diagnostic passive
            trajectories and all other modes are presentation world states and
            are projected into the requested hub frame here.
        */
        if (usesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            ship.positionMeters =
                transform.motion.localPositionMeters;
        }
        else
        {
            ship.positionMeters =
                frame->worldToLocalPosition(
                    shipWorldMeters
                );
        }

        if (usesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::Docked)
        {
            ship.velocityMps = glm::dvec3(0.0);
        }
        else if (
            usesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            ship.velocityMps =
                transform.motion.localVelocityMps;
        }
        else
        {
            ship.velocityMps =
                frame->worldToLocalVelocity(
                    shipWorldMeters,
                    transform.motion.worldVelocityMps
                );
        }

        ship.axes =
            axesToHubLocal(
                transform.orientation,
                *frame
            );

        ship.sizeMeters =
            assemblySizeMetersForType(
                ship.typeId
            );

        ship.player = isPlayer;
        ship.valid = true;

        snapshot.scene.objects.push_back(ship);

        if (isPlayer)
        {
            const glm::dvec3 reconstructedPlayerWorld =
                frame->localToWorldPosition(
                    ship.positionMeters
                );

            const double playerRoundTripErrorMeters =
                glm::length(
                    reconstructedPlayerWorld -
                    shipWorldMeters
                );

            if (playerRoundTripErrorMeters > 0.01)
            {
                auto& warningCount =
                    m_diagnostics.server.hubPlayerRoundTripWarnings;

                if (warningCount < 20)
                {
                    ++warningCount;

                    std::cerr
                        << "[HubMapConsistency]"
                        << " player round-trip error="
                        << playerRoundTripErrorMeters
                        << " m"
                        << " hub="
                        << snapshot.hubId
                        << "\n";
                }
            }
        }
    }


    if (game::diagnostics::HubMotionLabEnabled &&
        snapshot.systemId == game::diagnostics::HubMotionLabSystemId &&
        snapshot.hubId == game::diagnostics::HubMotionLabHubId)
    {
        const auto pose =
            game::diagnostics::evaluateHubMotionLabCube(
                m_simulation.serverTime()
            );

        HubMapShip cube;
        cube.stableId = "diagnostic:hub_motion_lab_cube";
        cube.name = "LAB ANALYTIC CUBE";
        cube.kind = "diagnostic_probe";
        cube.objectClass = DetailObjectClass::Ship;
        cube.role = LocalSceneObjectRole::Participant;
        cube.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        cube.parentStableId = snapshot.hubId;
        cube.positionMeters = pose.localPositionMeters;
        cube.sizeMeters = glm::dvec3(pose.halfExtentMeters * 2.0);
        cube.valid = true;
        snapshot.scene.objects.push_back(std::move(cube));
    }

    snapshot.scene.halfExtentMeters = 1000.0;

    for (const auto& object : snapshot.scene.objects)
    {
        if (!object.valid)
            continue;

        const double objectExtent =
            std::max(
                object.boundingRadiusMeters,
                glm::length(object.sizeMeters) * 0.5
            );

        snapshot.scene.halfExtentMeters =
            std::max(
                snapshot.scene.halfExtentMeters,
                glm::length(object.positionMeters) +
                    objectExtent
            );
    }

    // ------------------------------------------------------------
    // 6. Контроль геометрии planet <-> hub.
    // ------------------------------------------------------------

    const double orbitRadiusErrorMeters =
        std::abs(
            measuredOrbitRadiusMeters -
            authoredOrbitRadiusMeters
        );

    const glm::dvec3 hubLocalOrigin =
        frame->worldToLocalPosition(
            snapshot.hubWorldPositionMeters
        );

    const double hubOriginErrorMeters =
        glm::length(
            hubLocalOrigin
        );

    if (orbitRadiusErrorMeters >
            1.0 ||
        hubOriginErrorMeters >
            0.01)
    {
        auto& warningCount =
            m_diagnostics.server.hubGeometryWarnings;

        if (warningCount <
            20)
        {
            ++warningCount;

            std::cerr
                << "[HubMapConsistency]"
                << " orbitRadiusError="
                << orbitRadiusErrorMeters
                << " m"
                << " hubOriginError="
                << hubOriginErrorMeters
                << " m"
                << " time="
                << snapshot.universeTimeSeconds
                << "\n";
        }
    }

    snapshot.valid =
        true;
}
