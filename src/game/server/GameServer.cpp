#include "GameServer.h"
#include "src/game/network/ClientMessage.h"
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
        const world::celestial::SystemMapSnapshot& snapshot
    )
    {
        static double lastLoggedUniverseTime = -1.0;

        // Логируем не чаще одного раза на 1 секунду universe time,
        // чтобы не засрать файл.
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
    world::celestial::PlanetMapAxisSet axesToHubLocal(
        const glm::mat4& worldOrientation,
        const game::navigation::HubNavigationFrame& frame
    )
    {
        world::celestial::PlanetMapAxisSet axes;

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














GameServer::GameServer(){

        m_universeClock.reset();

        bool atlasLoaded =
        m_starAtlas.load(
            "assets/data/star_atlas/star_systems.json",
            "assets/data/star_atlas/system_details.json"
        );

        if (!atlasLoaded)
        {
            atlasLoaded =
                m_starAtlas.load(
                    "../assets/data/star_atlas/star_systems.json",
                    "../assets/data/star_atlas/system_details.json"
                );
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

    world::celestial::PlanetMapAxisSet planetMapAxesFromOrientation(
        const glm::mat4& m
    )
    {
        world::celestial::PlanetMapAxisSet axes;

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

        item.rotationPhaseRad =
            body.rotationOffsetDeg *
            3.14159265358979323846 /
            180.0;

        item.dayLengthHours =
            body.dayLengthHours;

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



    appendSystemMapMotionDebugCsv(out);

    return out;
}








world::celestial::PlanetMapSnapshot
GameServer::buildPlanetMapSnapshot(
    int systemId,
    const std::string& planetBodyId
) const
{
    using namespace world::celestial;

    PlanetMapSnapshot out;

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
    for (const auto& [hubId, hub] : m_simulation.orbitalHubs())
    {
        if (hub.parentBodyId != planetBodyId)
            continue;

        const auto* frame =
            m_simulation.hubNavigationFrame(hubId);

        if (!frame || !frame->valid)
            continue;

        PlanetMapObject hubObj;

        hubObj.stableId =
            hubId;

        hubObj.name =
            hub.id;

        hubObj.kind =
            "hub";

        hubObj.positionMeters =
            frame->originMeters;

        hubObj.velocityMps =
            frame->velocityMetersPerSecond;

        hubObj.axes.x =
            frame->normalAxis;

        hubObj.axes.y =
            frame->radialAxis;

        hubObj.axes.z =
            -frame->progradeAxis;

        hubObj.valid =
            true;

        out.hubs.push_back(
            hubObj
        );

        PlanetMapOrbit orbit;

        orbit.id =
            hubId + "_rail_orbit";

        orbit.name =
            hub.id + " rail orbit";

        orbit.parentBodyId =
            planetBodyId;

        orbit.centerMeters =
            out.planetCenterMeters;

        orbit.positionMeters =
            frame->originMeters;

        orbit.velocityMps =
            frame->velocityMetersPerSecond;

        const glm::dvec3 fromPlanet =
            orbit.positionMeters -
            out.planetCenterMeters;

        orbit.radiusMeters =
            glm::length(fromPlanet);

        orbit.altitudeMeters =
            orbit.radiusMeters -
            out.planetRadiusMeters;

        orbit.speedMps =
            glm::length(orbit.velocityMps);

        orbit.radialAxis =
            frame->radialAxis;

        orbit.progradeAxis =
            frame->progradeAxis;

        orbit.normalAxis =
            frame->normalAxis;

        orbit.valid =
            true;

        out.hubOrbits.push_back(
            orbit
        );
    }

    // -----------------------------
    // Static objects near this planet
    // -----------------------------
    for (const auto& [id, obj] : m_simulation.staticObjects())
    {
        if (obj.mapSystemId != systemId)
            continue;

        if (obj.mapParentBodyId != planetBodyId)
            continue;

        PlanetMapObject item;

        item.id =
            id;

        item.stableId =
            std::to_string(id.value);

        item.name =
            obj.displayName.empty()
                ? "Object"
                : obj.displayName;

        if (obj.type == ObjectType::Station)
            item.kind = "station";
        else
            item.kind = "static";

        item.positionMeters =
            world::coordinates::fullMeters(
                obj.worldPosition
            );

        const auto* frame =
            m_simulation.hubNavigationFrame(
                obj.hubId
            );

        if (frame && frame->valid)
        {
            item.velocityMps =
                frame->velocityMetersPerSecond;
        }

        item.axes =
            planetMapAxesFromOrientation(
                obj.orientation
            );

        item.valid =
            true;

        if (item.kind == "station")
            out.stations.push_back(item);
    }

    // -----------------------------
    // Player ship
    // -----------------------------
    const Ship* player =
        m_simulation.playerShip();

    if (player)
    {
        const auto& tr =
            player->core().transform();

        const glm::dvec3 playerMeters =
            world::coordinates::fullMeters(
                tr.worldPosition
            );

        const glm::dvec3 fromPlanet =
            playerMeters -
            out.planetCenterMeters;

        const double playerDistance =
            glm::length(fromPlanet);

        // Пока фильтр простой:
        // если игрок внутри 100 000 км от планеты,
        // считаем его релевантным для planet map.
        const double planetMapRadiusMeters =
            100000000.0;

        if (playerDistance <= planetMapRadiusMeters)
        {
            PlanetMapObject shipObj;

            shipObj.id =
                m_simulation.playerId();

            shipObj.stableId =
                "player";

            shipObj.name =
                "Player";

            shipObj.kind =
                "player";

            shipObj.positionMeters =
                playerMeters;

            shipObj.velocityMps =
                tr.motion.worldVelocityMps;

            shipObj.axes =
                planetMapAxesFromOrientation(
                    tr.orientation
                );

            shipObj.valid =
                true;

            out.ships.push_back(
                shipObj
            );

            PlanetMapOrbit playerOrbit;

            playerOrbit.id =
                "player_physical_orbit";

            playerOrbit.name =
                "Player physical orbit";

            playerOrbit.parentBodyId =
                planetBodyId;

            playerOrbit.centerMeters =
                out.planetCenterMeters;

            playerOrbit.positionMeters =
                playerMeters;

            playerOrbit.velocityMps =
                tr.motion.worldVelocityMps;

            playerOrbit.radiusMeters =
                playerDistance;

            playerOrbit.altitudeMeters =
                playerDistance -
                out.planetRadiusMeters;

            playerOrbit.speedMps =
                glm::length(
                    tr.motion.worldVelocityMps
                );

            playerOrbit.radialAxis =
                safeNormalizePlanetMap(
                    fromPlanet,
                    glm::dvec3(0.0, 1.0, 0.0)
                );

            const glm::dvec3 tangentialVelocity =
                tr.motion.worldVelocityMps -
                playerOrbit.radialAxis *
                glm::dot(
                    tr.motion.worldVelocityMps,
                    playerOrbit.radialAxis
                );

            playerOrbit.progradeAxis =
                safeNormalizePlanetMap(
                    tangentialVelocity,
                    glm::dvec3(1.0, 0.0, 0.0)
                );

            playerOrbit.normalAxis =
                safeNormalizePlanetMap(
                    glm::cross(
                        playerOrbit.progradeAxis,
                        playerOrbit.radialAxis
                    ),
                    glm::dvec3(0.0, 0.0, 1.0)
                );

            playerOrbit.valid =
                true;

            out.playerOrbits.push_back(
                playerOrbit
            );
        }
    }

    out.valid = true;








{
    static int row = 0;

    if (row < 300)
    {
        std::ofstream dbg(
            "planet_map_snapshot_debug.csv",
            row == 0 ? std::ios::out : std::ios::app
        );

        if (row == 0)
        {
            dbg
                << "row,systemId,planetBodyId,valid,"
                << "hubCount,stationCount,shipCount,"
                << "hubOrbitCount,playerOrbitCount\n";
        }

        dbg
            << row << ","
            << out.systemId << ","
            << out.planetBodyId << ","
            << (out.valid ? 1 : 0) << ","
            << out.hubs.size() << ","
            << out.stations.size() << ","
            << out.ships.size() << ","
            << out.hubOrbits.size() << ","
            << out.playerOrbits.size()
            << "\n";

        ++row;
    }
}













    return out;
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

    out.systemId = systemId;
    out.hubId = hubId;
    out.universeTimeSeconds = m_universeClock.timeSeconds();

    auto hubIt =
        m_simulation.orbitalHubs().find(hubId);

    if (hubIt == m_simulation.orbitalHubs().end())
        return out;

    const auto& hub =
        hubIt->second;

    const auto* frame =
        m_simulation.hubNavigationFrame(hubId);

    if (!frame || !frame->valid)
        return out;

    out.parentBodyId = hub.parentBodyId;

    
    
    const auto* system =
        m_starAtlas.findSystem(
            systemId
        );

    if (const auto* summary =
            m_starAtlas.findSystemSummary(
                systemId
            ))
    {
        out.systemPositionLy =
            summary->positionLy;
    }

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

            break;
        }
    }





    out.displayName = hub.id;

    out.hubWorldPositionMeters = frame->originMeters;

    out.hubWorldVelocityMps = frame->velocityMetersPerSecond;

    // Hub local convention:
    // X = prograde
    // Y = radial
    // Z = normal
    out.hubAxes.x = glm::dvec3(1.0, 0.0, 0.0);
    out.hubAxes.y = glm::dvec3(0.0, 1.0, 0.0);
    out.hubAxes.z = glm::dvec3(0.0, 0.0, 1.0);


    out.parentPlanetRadiusMeters =
        hub.motion.parentRadiusMeters;

    out.hubAltitudeMeters =
        hub.motion.altitudeMeters;

    out.hubOrbitRadiusMeters =
        hub.motion.parentRadiusMeters +
        hub.motion.altitudeMeters;

    out.hubOrbitalPeriodSeconds =
        hub.motion.orbitalPeriodSeconds;

    // В локальной карте хаба хаб находится в нуле.
    // Ось Y считается радиальной наружу от планеты.
    // Значит центр планеты находится вниз по -Y на радиус орбиты.
    if (out.hubOrbitRadiusMeters > 0.0)
    {
        out.parentPlanetCenterLocalMeters =
            glm::dvec3(
                0.0,
                -out.hubOrbitRadiusMeters,
                0.0
            );
    }

    // -----------------------------
    // Modules / station objects
    // -----------------------------
    for (const auto& [id, obj] : m_simulation.staticObjects())
    {
        if (obj.hubId != hubId)
            continue;

        HubMapModule mod;

        mod.id = id;
        mod.typeId = obj.type;
        mod.stableId = std::to_string(id.value);

        mod.name =
            obj.displayName.empty()
                ? "Hub module"
                : obj.displayName;

        mod.kind =
            obj.type == ObjectType::Station
                ? "station"
                : "module";

        const glm::dvec3 worldMeters =
            world::coordinates::fullMeters(
                obj.worldPosition
            );

        mod.localPositionMeters =
            frame->worldToLocalPosition(
                worldMeters
            );

        mod.localAxes =
            axesToHubLocal(
                obj.orientation,
                *frame
            );

        // Первый вариант: prime/station рисуем длиннее.
        mod.sizeMeters =
            assemblySizeMetersForType(
                obj.type
            );

        mod.prime =
            !hub.modules.empty() &&
            hub.modules.front().value == id.value;

        mod.valid = true;

        out.modules.push_back(
            mod
        );
    }

    // -----------------------------
    // Player ship
    // -----------------------------
    const Ship* player =
        m_simulation.playerShip();

    if (player)
    {
        const auto& tr =
            player->core().transform();

        const glm::dvec3 playerWorld =
            world::coordinates::fullMeters(
                tr.worldPosition
            );

        HubMapShip ship;

        ship.id =
            m_simulation.playerId();

        ship.typeId =
            player->typeId();

        ship.name =
            "Player";

        ship.localPositionMeters =
            frame->worldToLocalPosition(
                playerWorld
            );

        ship.localVelocityMps =
            velocityToHubLocal(
                tr.motion.worldVelocityMps,
                *frame
            );

        ship.localAxes =
            axesToHubLocal(
                tr.orientation,
                *frame
            );

        ship.sizeMeters =
            assemblySizeMetersForType(ship.typeId);

        ship.player = true;
        ship.valid = true;

        out.ships.push_back(
            ship
        );
    }

    out.valid = true;

    return out;
}