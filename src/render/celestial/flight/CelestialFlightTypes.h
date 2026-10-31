#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace render::celestial_flight
{
/*
    One frame of the world as the runtime renderer sees it - built from the
    SAME celestial snapshot the maps use (ClientCatalogService at universe
    time), in the system frame of the ship positions (metres). The renderer
    never moves a body: position, size, orientation and spin are facts of
    the snapshot; only the representation (sprite / globe / local terrain)
    and its resolution depend on the camera.
*/
enum class FlightBodyKind
{
    Star,
    Solid,     // planets, moons, dwarf planets with a surface
    GasGiant,  // no solid surface
};

struct FlightRing
{
    double innerM = 0.0;
    double outerM = 0.0;
    glm::vec3 tint {0.8f};
    float opacity = 0.3f;
};

struct FlightBody
{
    std::string id;
    std::string name;
    FlightBodyKind kind = FlightBodyKind::Solid;
    glm::dvec3 positionM {0.0};  // system frame
    double radiusM = 1.0;
    // Body-fixed axes in the system frame (spin applied): x = prime
    // meridian, y = north, z = east (PlanetBodyOrientation).
    glm::dvec3 prime {1.0, 0.0, 0.0};
    glm::dvec3 north {0.0, 1.0, 0.0};
    glm::dvec3 east {0.0, 0.0, 1.0};
    std::string descriptorPath;  // elite.generated_body.v1
    // Curated maps (ImportedRealData): equirect albedo / height / clouds /
    // night emission; empty = procedural from the descriptor.
    std::string importedAlbedoPath;
    std::string importedHeightPath;
    std::string importedEmissionPath;
    double importedHeightRangeM = 0.0;
    bool importedHeightSigned = true;
    bool importedWaterFromHeight = false; // grey 0 = ocean (Earth DEM)
    // Stars.
    double temperatureK = 5772.0;
    double luminositySolar = 1.0;
    std::vector<FlightRing> rings;
};

struct FlightReservoir
{
    std::string id;
    std::string descriptorPath;   // elite.generated_reservoir.v1
    glm::dvec3 centreM {0.0};     // parent (star / barycentre)
    double innerM = 0.0;
    double outerM = 0.0;
    double halfThicknessM = 0.0;
};

struct FlightCelestialFrame
{
    int systemId = -1;
    glm::dvec3 cameraPositionM {0.0};  // system frame
    double worldTimeSeconds = 0.0;
    std::vector<FlightBody> bodies;
    std::vector<FlightReservoir> reservoirs;
};

struct FlightRenderQuality
{
    int maxGlobeFace = 1024;       // cube face texels of the nearest body
    int cloudFace = 256;
    int terrainCells = 128;        // per local terrain level side
    int maxTerrainLevels = 14;
    double terrainCellPixels = 4.0;
    int atmosphereSteps = 8;
    bool clouds = true;
    bool atmosphere = true;
    bool asteroids = true;
    bool rings = true;
    bool detailNormals = true;
    std::size_t textureBudgetBytes = 512u * 1024u * 1024u;
    int workerThreads = 0;         // 0 = hardware - 2
};

struct FlightRenderStats
{
    double cpuPrepareMs = 0.0;
    double cpuDrawMs = 0.0;
    // GPU (previous frame, GL_TIME_ELAPSED).
    double gpuGlobesMs = 0.0;
    double gpuTerrainMs = 0.0;
    double gpuCloudsMs = 0.0;
    double gpuAtmosphereMs = 0.0;
    double gpuRingsMs = 0.0;
    double gpuStarsSpritesMs = 0.0;
    double gpuAsteroidsMs = 0.0;
    int spriteBodies = 0;
    int globeBodies = 0;
    int terrainLevels = 0;
    long long terrainTriangles = 0;
    int asteroidMeshes = 0;
    int asteroidSprites = 0;
    std::size_t textureBytes = 0;
    std::size_t meshBytes = 0;
    int pendingJobs = 0;
    double workerBusySeconds = 0.0; // cumulative
    long long samplesTaken = 0;     // cumulative field samples
    double nearestAltitudeM = -1.0;
    std::string nearestBody;
};
} // namespace render::celestial_flight
