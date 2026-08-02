#include "GameServer.h"
#include "src/game/network/ClientMessage.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>

#include "src/world/coordinates/WorldPosition.h"
#include <cmath>
#include <functional>
#include <unordered_map>

#include "src/world/celestial/SystemMapTypes.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"



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













    std::string jurisdictionForSystemId(int systemId)
    {
        if (systemId == 0)
            return "Sol Authority";

        if (systemId >= 1 && systemId <= 9)
            return "Core Jurisdiction";

        if (systemId >= 10 && systemId <= 29)
            return "Colonial Administration";

        if (systemId >= 30 && systemId <= 44)
            return "Frontier / Independent";

        return "Unregistered";
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

        m_playerNavigation.currentSystemId = 0;

        const auto* sol =
            m_starAtlas.findSystem(m_playerNavigation.currentSystemId);

        m_celestialRuntime.setSystem(sol);




const double universeTime =
    m_universeClock.timeSeconds();

constexpr double kinematicSampleDtSeconds =
    1.0;

auto collectCelestialPositionsAu =
    [this]()
    {
        std::unordered_map<std::string, glm::dvec3> result;

        for (const auto& state : m_celestialRuntime.snapshot().bodies)
        {
            result[state.id] =
                state.positionAu;
        }

        return result;
    };

// Сэмпл на секунду раньше.
m_celestialRuntime.update(
    universeTime - kinematicSampleDtSeconds
);

const auto previousCelestialPositionsAu =
    collectCelestialPositionsAu();

// Текущий сэмпл.
m_celestialRuntime.update(
    universeTime
);

const auto currentCelestialPositionsAu =
    collectCelestialPositionsAu();

m_simulation.setOrbitalUniverseTimeSeconds(
    universeTime
);

m_simulation.setCelestialBodyKinematicStateAu(
    currentCelestialPositionsAu,
    previousCelestialPositionsAu,
    kinematicSampleDtSeconds
);











        m_simulation.buildInitialScene();

        applyCelestialOrbitParentParameters();

        // Готовим хабы, станции и reference frames до размещения игрока.
        // Это не полный update и не создаёт грязный стартовый snapshot.
        m_simulation.prepareReferenceFramesForSpawn();

        game::navigation::ReferenceFrame playerStartFrame;









        playerStartFrame.type =
            game::navigation::ReferenceFrameType::OrbitalHub;

        playerStartFrame.hubId =
            "earth_orbital_hub";

        playerStartFrame.localOffsetMeters =
            glm::dvec3(-10000.0, 2500.0, 0.0);

        m_simulation.placeShipInReferenceFrame(
            m_simulation.playerId(),
            playerStartFrame
        );

        m_simulation.update(0.0);
        m_simulation.setTick(0);






        m_lastSnapshot = m_simulation.snapshot();


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

        m_simulation.updateStaticObjectOrbitParentParameters(
            body.id,
            body.radiusKm * 1000.0,
            body.gravitationalParameterM3s2,
            true
        );
    }
}
















void GameServer::update(double dt)
{



    m_universeClock.update(dt);
    m_serverTick++;

    // 1. Применяем команды
    for (auto& [id, shipPtr] : m_simulation.ships())
    {
        Ship& ship = *shipPtr;
        auto it = m_pendingCommands.find(id.value);
        if (it == m_pendingCommands.end())
            continue;

        auto& queue = it->second;

        while (!queue.empty())
        {
            const auto& cmd = queue.front();

            ship.setControlState(cmd);
            queue.pop_front();
        }


        // --- Обработка событийных ShipCommand ---
        auto cmdIt = m_pendingClientShipCommands.find(id.value);
        if (cmdIt != m_pendingClientShipCommands.end())
        {
            auto& cmdQueue = cmdIt->second;

            while (!cmdQueue.empty())
            {
                const auto& shipCmd = cmdQueue.front();

                std::cout << "GameServer::update  - ClientShipCommand received: "
                        << shipCmd.type << "\n";

                ship.applyCommand(shipCmd);

                cmdQueue.pop_front();
            }
        }
    }


const double universeTime =
    m_universeClock.timeSeconds();

m_celestialRuntime.update(universeTime);

m_simulation.setOrbitalUniverseTimeSeconds(
    universeTime
);




std::unordered_map<std::string, glm::dvec3> celestialPositionsAu;

for (const auto& state : m_celestialRuntime.snapshot().bodies)
{
    celestialPositionsAu[state.id] = state.positionAu;
}

m_simulation.setCelestialBodyWorldPositionsAu(
    celestialPositionsAu
);

m_simulation.update(dt);
m_simulation.setTick(m_serverTick);



    if (const Ship* player = m_simulation.playerShip())
    {
        const auto& tr = player->core().transform();

        m_playerNavigation.currentSystemId = 0;
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











    if (m_serverTick % m_snapshotInterval == 0)
    {
        m_lastSnapshot = m_simulation.snapshot();
    }
}





void GameServer::submitCommand(EntityId id, const ShipControlState& control)
{

    m_pendingCommands[id.value].push_back(control);
}




void GameServer::receiveClientMessage(
    EntityId playerId,
    const game::network::ClientMessage& msg)
{

    switch (msg.type)
    {
        case game::network::ClientMessageType::ControlInput:
        {
            const auto& control =
                std::get<ShipControlState>(msg.payload);

            submitCommand(playerId, control);
            break;
        }

        case game::network::ClientMessageType::ClientShipCommand:
{

            const auto& cmd =
                std::get<ClientShipCommand>(msg.payload);

            m_pendingClientShipCommands[playerId.value].push_back(cmd);
            break;
        }
    }
}







void GameServer::debugRefreshSnapshot()
{
    // Debug panels are allowed to request heavy structural data.
    // This keeps regular published snapshots lightweight, while
    // structure_debug.html still receives modules/links on demand.
    m_simulation.debugForceFullShipGraphPayload();
    m_simulation.update(0.0);
    m_simulation.setTick(m_serverTick);
    m_lastSnapshot = m_simulation.snapshot();
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
        item.jurisdiction = jurisdictionForSystemId(s.id);

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

    double planetMapMuForBodyId(
        const std::string& bodyId
    )
    {
        if (bodyId == "system_0.Sol.Земля")
            return 3.986004418e14;

        return 0.0;
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








    world::celestial::CelestialSystemRuntime runtime;

    runtime.setSystem(system);
    runtime.update(out.universeTimeSeconds);

    const auto& runtimeSnapshot =
        runtime.snapshot();

    std::unordered_map<
    std::string,
    world::celestial::CelestialBodyState
    > runtimeStateById;

    for (const auto& state : runtimeSnapshot.bodies)
    {
        runtimeStateById[state.id] =
            state;
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
                stateIt->second;

            item.positionAu =
                state.positionAu;

            item.rotationPhaseRad =
                state.rotationPhaseRad;

            item.dayLengthHours =
                state.dayLengthHours;

            item.axialTiltDeg =
                state.axialTiltDeg;

            item.axisNodeDeg =
                state.axisNodeDeg;

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
                item.orbitCenterAu = parentIt->second.positionAu;
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
        if (obj.mapSystemId != systemId)
            continue;

        world::celestial::SystemMapObject mapObj;

        mapObj.id = id;
        mapObj.stableId =
            "entity:" +
            std::to_string(id.value);

        mapObj.name = obj.displayName;

        mapObj.owner = obj.ownerName;

        mapObj.systemId = obj.mapSystemId;

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
    if (const auto* system =
            m_starAtlas.findSystem(out.systemId))
    {
        CelestialSystemRuntime localRuntime;

        localRuntime.setSystem(system);
        localRuntime.update(
            out.universeTimeSeconds
        );

        for (const auto& body :
             localRuntime.snapshot().bodies)
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
        if (object.mapSystemId != out.systemId)
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
        const auto& transform = core.transform();
        const glm::dvec3 positionMeters =
            transform.fullWorldMeters();

        if (!intersectsBounds(positionMeters))
            continue;

        LocalSceneObject object;
        object.id = entityId;
        object.stableId =
            "entity:" +
            std::to_string(entityId.value);
        object.name =
            core.descriptor().identity.shipName.empty()
                ? "Ship " + std::to_string(entityId.value)
                : core.descriptor().identity.shipName;
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
            if (const auto* system =
                    m_starAtlas.findSystem(target.systemId))
            {
                CelestialSystemRuntime localRuntime;
                localRuntime.setSystem(system);
                localRuntime.update(
                    out.universeTimeSeconds
                );

                for (const auto& body :
                     localRuntime.snapshot().bodies)
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
        !anchorFrame->valid)
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

    world::celestial::CelestialSystemRuntime planetMapRuntime;

    planetMapRuntime.setSystem(
        system
    );

    planetMapRuntime.update(
        out.universeTimeSeconds
    );

    const auto& celestial =
        planetMapRuntime.snapshot();

    bool foundPlanet = false;

    for (const auto& body : celestial.bodies)
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

        out.planetAxialTiltDeg =
            body.axialTiltDeg;

        out.planetAxisNodeDeg =
            body.axisNodeDeg;

        out.planetTextureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        out.gravitationalParameterM3s2 =
            planetMapMuForBodyId(
                body.id
            );


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

    bool currentPlanetFound =
        false;

    for (const auto& body :
         m_celestialRuntime.snapshot().bodies)
    {
        if (body.id !=
            snapshot.planetBodyId)
        {
            continue;
        }

        snapshot.planetCenterMeters =
            body.positionAu *
            world::celestial::MetersPerAu;

        if (body.radiusKm > 0.0)
        {
            snapshot.planetRadiusMeters =
                body.radiusKm *
                1000.0;
        }

        /*
            Это тоже динамическое состояние.

            Иначе текстура планеты застывает при длительно
            открытом Details.
        */
        snapshot.planetRotationPhaseRad =
            body.rotationPhaseRad;

        snapshot.planetDayLengthHours =
            body.dayLengthHours;

        snapshot.planetAxialTiltDeg =
            body.axialTiltDeg;

        snapshot.planetAxisNodeDeg =
            body.axisNodeDeg;

        snapshot.planetTextureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        currentPlanetFound =
            true;

        break;
    }

    /*
        Fallback на simulation cache.

        Обычно currentPlanetFound будет true, но карта не должна
        ломаться из-за временного отсутствия body в runtime snapshot.
    */
    if (!currentPlanetFound)
    {
        glm::dvec3 currentCenter =
            snapshot.planetCenterMeters;

        double currentRadius =
            snapshot.planetRadiusMeters;

        if (m_simulation.resolveCelestialBodyMeters(
                snapshot.planetBodyId,
                currentCenter,
                currentRadius
            ))
        {
            snapshot.planetCenterMeters =
                currentCenter;

            if (currentRadius > 1.0)
            {
                snapshot.planetRadiusMeters =
                    currentRadius;
            }
        }
    }

    snapshot.scene.originWorldMeters =
        snapshot.planetCenterMeters;

    snapshot.planetVelocityMps =
        glm::dvec3(0.0);

    m_simulation
        .resolveCelestialBodyVelocityMetersPerSecond(
            snapshot.planetBodyId,
            snapshot.planetVelocityMps
        );

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
            !frame->valid)
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
        if (object.mapSystemId !=
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

            if (!object.mapParentBodyId.empty())
            {
                m_simulation
                    .resolveCelestialBodyVelocityMetersPerSecond(
                        object.mapParentBodyId,
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
    // 5. Игрок.
    // ------------------------------------------------------------

    const Ship* player =
        m_simulation.playerShip();

    if (player)
    {
        const auto& transform =
            player->core().transform();

        const glm::dvec3 playerPositionMeters =
            world::coordinates::fullMeters(
                transform.worldPosition
            );

        const glm::dvec3 fromPlanet =
            playerPositionMeters -
            snapshot.planetCenterMeters;

        const double playerDistanceMeters =
            glm::length(
                fromPlanet
            );

        /*
            Пока сохраняем существующий Details radius:
            100 000 км от центра выбранной планеты.
        */
        constexpr double planetMapObjectRadiusMeters =
            100000000.0;

        if (playerDistanceMeters <=
            planetMapObjectRadiusMeters)
        {
            LocalSceneObject playerObject;

            playerObject.id =
                m_simulation.playerId();

            playerObject.stableId =
                "player";

            playerObject.name =
                "Player";

            playerObject.kind =
                "player";
            playerObject.objectClass =
                DetailObjectClass::Ship;
            playerObject.origin =
                DetailObjectOrigin::Runtime;
            playerObject.role =
                LocalSceneObjectRole::Participant;
            playerObject.parentStableId =
                transform.motion.hubId;

            playerObject.positionMeters =
                playerPositionMeters;

            /*
                Это настоящий world velocity сервера.

                Не target speed, не desired tactical velocity
                и не reference velocity.
            */
            playerObject.velocityMps =
                transform.motion.worldVelocityMps;

            playerObject.axes =
                planetMapAxesFromOrientation(
                    transform.orientation
                );

            playerObject.valid =
                true;

            snapshot.scene.objects.push_back(
                playerObject
            );
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
    m_debugFastUniverseTime = enabled;

    m_universeClock.setTimeScale(
        enabled
            ? m_debugFastUniverseTimeScale
            : 1.0
    );
}

bool GameServer::debugFastUniverseTime() const
{
    return m_debugFastUniverseTime;
}

double GameServer::debugUniverseTimeScale() const
{
    return m_universeClock.timeScale();
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

    const auto* frame =
        m_simulation.hubNavigationFrame(
            hubId
        );

    if (!frame ||
        !frame->valid)
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

            out.parentPlanetDayLengthHours =
                body.dayLengthHours;

            out.parentPlanetAxialTiltDeg =
                body.axialTiltDeg;

            out.parentPlanetAxisNodeDeg =
                body.axisNodeDeg;

            out.parentPlanetRotationOffsetDeg =
                body.rotationOffsetDeg;

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

    out.hubOrbitalPeriodSeconds =
        hub.motion.orbitalPeriodSeconds;

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

    snapshot.kinematicTimeSeconds =
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

    /*
        Временно обновляем и legacy epoch-поля.

        Если где-то остался старый код, он получит текущий
        frame, а не старую orbital phase.
    */
    snapshot.hubWorldAxesAtEpoch =
        snapshot.hubWorldAxes;

    snapshot.hubWorldAxesEpochSeconds =
        currentUniverseTimeSeconds;

    // ------------------------------------------------------------
    // 2. Текущее абсолютное состояние родительской планеты.
    // ------------------------------------------------------------

    glm::dvec3 parentPlanetWorldMeters =
        hub.motion.centerMeters;

    double resolvedParentRadiusMeters =
        snapshot.parentPlanetRadiusMeters;

    if (m_simulation.resolveCelestialBodyMeters(
            snapshot.parentBodyId,
            parentPlanetWorldMeters,
            resolvedParentRadiusMeters
        ))
    {
        snapshot.parentPlanetWorldPositionMeters =
            parentPlanetWorldMeters;

        if (resolvedParentRadiusMeters >
            1.0)
        {
            snapshot.parentPlanetRadiusMeters =
                resolvedParentRadiusMeters;
        }
    }
    else
    {
        /*
            hub.motion.centerMeters обновляется сервером вместе
            с текущей позицией родительского тела.
        */
        snapshot.parentPlanetWorldPositionMeters =
            hub.motion.centerMeters;
    }

    /*
        Критическая часть.

        Центр планеты в Hub Map больше не задаётся условным:

            (0, -orbitRadius, 0)

        Он вычисляется из тех же абсолютных координат,
        которые сервер использует в Details.
    */
    snapshot.parentPlanetCenterLocalMeters =
        frame->worldToLocalPosition(
            snapshot.parentPlanetWorldPositionMeters
        );

    const glm::dvec3 hubFromPlanetMeters =
        snapshot.hubWorldPositionMeters -
        snapshot.parentPlanetWorldPositionMeters;

    snapshot.hubOrbitRadiusMeters =
        glm::length(
            hubFromPlanetMeters
        );

    snapshot.hubAltitudeMeters =
        snapshot.hubOrbitRadiusMeters -
        snapshot.parentPlanetRadiusMeters;

    // ------------------------------------------------------------
    // 3. Текущая rotation phase планеты.
    // ------------------------------------------------------------

    bool currentPlanetStateFound =
        false;

    const auto& celestialSnapshot =
        m_celestialRuntime.snapshot();

    for (const auto& body :
         celestialSnapshot.bodies)
    {
        if (body.id !=
            snapshot.parentBodyId)
        {
            continue;
        }

        snapshot.parentPlanetRotationPhaseRad =
            body.rotationPhaseRad;

        snapshot.parentPlanetDayLengthHours =
            body.dayLengthHours;

        snapshot.parentPlanetAxialTiltDeg =
            body.axialTiltDeg;

        snapshot.parentPlanetAxisNodeDeg =
            body.axisNodeDeg;

        snapshot.parentPlanetTextureLongitudeOffsetDeg =
            body.textureLongitudeOffsetDeg;

        currentPlanetStateFound =
            true;

        break;
    }

    /*
        Fallback на ту же аналитическую формулу, если выбранное
        тело временно отсутствует в runtime snapshot.
    */
    if (!currentPlanetStateFound)
    {
        constexpr double pi =
            3.14159265358979323846;

        snapshot.parentPlanetRotationPhaseRad =
            snapshot.parentPlanetRotationOffsetDeg *
            pi /
            180.0;

        if (snapshot.parentPlanetDayLengthHours >
            0.0)
        {
            const double rotationPeriodSeconds =
                snapshot.parentPlanetDayLengthHours *
                3600.0;

            const double turns =
                currentUniverseTimeSeconds /
                rotationPeriodSeconds;

            const double wrappedTurns =
                turns -
                std::floor(
                    turns
                );

            snapshot.parentPlanetRotationPhaseRad +=
                wrappedTurns *
                2.0 *
                pi;
        }
    }

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
        if (object.hubId !=
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
    // Сейчас snapshot содержит игрока.
    // Позже сюда тем же способом добавим другие реальные ships.
    // ------------------------------------------------------------

    const Ship* player =
        m_simulation.playerShip();

    if (player)
    {
        const auto& transform =
            player->core().transform();

        const glm::dvec3 playerWorldMeters =
            world::coordinates::fullMeters(
                transform.worldPosition
            );

        HubMapShip ship;

        ship.id =
            m_simulation.playerId();

        ship.typeId =
            player->typeId();

        ship.name =
            "Player";
        ship.kind = "ship";
        ship.objectClass = DetailObjectClass::Ship;
        ship.role = LocalSceneObjectRole::Participant;
        ship.coordinateSpace =
            LocalSceneCoordinateSpace::AnchorLocalMeters;
        ship.parentStableId = snapshot.hubId;

        /*
            Позиция всегда вычисляется из абсолютной
            серверной world position.

            Поэтому Details и Hub Map неизбежно показывают
            одну и ту же точку пространства.
        */
        ship.positionMeters =
            frame->worldToLocalPosition(
                playerWorldMeters
            );

        const bool playerUsesThisHubFrame =
            transform.motion.hubId ==
            snapshot.hubId;

        if (playerUsesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::Docked)
        {
            ship.velocityMps =
                glm::dvec3(0.0);
        }
        else if (
            playerUsesThisHubFrame &&
            transform.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            /*
                Это серверная локальная скорость текущего
                HubNavigationFrame.

                Не target speed и не desired velocity.
            */
            ship.velocityMps =
                transform.motion.localVelocityMps;
        }
        else
        {
            /*
                Для корабля вне HubTactical переводим его
                текущую абсолютную world velocity в frame хаба.
            */
            ship.velocityMps =
                frame->worldToLocalVelocity(
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

        ship.player =
            true;

        ship.valid =
            true;

        snapshot.scene.objects.push_back(
            ship
        );

        /*
            Контроль round-trip преобразования.

            local -> world должен восстановить исходную
            серверную позицию практически без ошибки.
        */
        const glm::dvec3 reconstructedPlayerWorld =
            frame->localToWorldPosition(
                ship.positionMeters
            );

        const double playerRoundTripErrorMeters =
            glm::length(
                reconstructedPlayerWorld -
                playerWorldMeters
            );

        if (playerRoundTripErrorMeters >
            0.01)
        {
            auto& warningCount =
                m_diagnostics.server.hubPlayerRoundTripWarnings;

            if (warningCount <
                20)
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

    const glm::dvec3 reconstructedPlanetWorld =
        frame->localToWorldPosition(
            snapshot.parentPlanetCenterLocalMeters
        );

    const double planetRoundTripErrorMeters =
        glm::length(
            reconstructedPlanetWorld -
            snapshot.parentPlanetWorldPositionMeters
        );

    const glm::dvec3 hubLocalOrigin =
        frame->worldToLocalPosition(
            snapshot.hubWorldPositionMeters
        );

    const double hubOriginErrorMeters =
        glm::length(
            hubLocalOrigin
        );

    if (planetRoundTripErrorMeters >
            0.01 ||
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
                << " planetRoundTripError="
                << planetRoundTripErrorMeters
                << " m"
                << " hubOriginError="
                << hubOriginErrorMeters
                << " m"
                << " time="
                << snapshot.kinematicTimeSeconds
                << "\n";
        }
    }

    snapshot.valid =
        true;
}
