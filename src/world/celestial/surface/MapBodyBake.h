#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial::surface
{
/*
    Map textures of a body (system / Detail / Hub maps): the equirect albedo and relief normal map
    baked from the SAME surface field flight draws (acquireBodySurface: generated descriptor, curated
    maps, appearance passport), plus a small body's shape mesh. GL-free: used by the game's map cache
    (game/system_map/ProceduralBodyTextureCache) and by the offline prebake tool (EliteMapPrebake).

    Storage (decided with the author 2026-11-11, PROJECT_STATE): one file per body,
        prebake (shipped, read-only):  <root>/assets/baked/celestial/map/<systemId>/<body>.emb
        runtime cache (self-filling):  <root>/cache/celestial/map/<systemId>/<body>.emb
                                       (%LOCALAPPDATA%/Elite/cache/celestial/map when <root> is read-only)
    BC1 mip chains (TextureBlockCompression) - uploaded to the GPU as they are. A file is valid only for
    the fingerprint it was baked from (descriptor bytes, passport, curated map files, bake version, size);
    a stale file is ignored and the body baked live (and cached again).
*/

// bump when the bake output changes for the same inputs (bake code / file format). Generator changes need
// no bump: every file carries a probe (the surface's colour at MapProbeCount fixed points), recomputed
// from the live field when the file is loaded - a changed generator fails it and the body is baked again.
constexpr std::uint32_t MapBodyBakeVersion = 2;
constexpr int MapProbeCount = 16;
using MapBodyProbe = std::array<std::uint8_t, MapProbeCount * 3>;

struct MapBodyBake
{
    int width = 0, height = 0;
    std::vector<std::uint8_t> rgba;          // albedo RGBA8, GL row order (south first), sRGB-encoded
    int normalWidth = 0, normalHeight = 0;
    std::vector<std::uint8_t> normalRgba;    // tangent-space relief normal, same layout
    // small bodies: (shapeLon + 1) x shapeLat grid of positions (m, body frame), row 0 = north;
    // uv = (j / shapeLon, 1 - i / (shapeLat - 1))
    int shapeLon = 0, shapeLat = 0;
    std::vector<glm::vec3> shapeGrid;
    std::string description;                 // log text ("curated maps, gas_giant")
    MapBodyProbe probe {};
};

// the compressed file form
struct MapBodyPrebake
{
    std::uint64_t fingerprint = 0;
    MapBodyProbe probe {};
    int width = 0, height = 0;
    std::vector<std::vector<std::uint8_t>> albedoMips;   // BC1, level 0 = width x height
    int normalWidth = 0, normalHeight = 0;
    std::vector<std::vector<std::uint8_t>> normalMips;   // BC1 (empty: none)
    int shapeLon = 0, shapeLat = 0;
    std::vector<glm::vec3> shapeGrid;
};

constexpr double MapSmallBodyShapeRadiusM = 300000.0;

/*
    The generated body descriptors of every system (galaxy_details/systems and distant_systems) by
    system id and body id. Thread-safe.
*/
class MapBodyCatalog
{
public:
    MapBodyCatalog();

    std::string descriptorPath(int systemId, const std::string& bodyId);
    std::string systemFolder(int systemId);   // "000_sol" -> "sol" (curated maps folder)
    const std::string& assetRoot() const { return m_assetRoot; }   // the folder holding assets/

    // every system id with generated bodies, and the body ids of one system (prebake)
    std::vector<int> systemIds();
    std::vector<std::string> bodyIds(int systemId);

private:
    struct System
    {
        std::string folder;
        std::unordered_map<std::string, std::string> bodies;   // id -> descriptor path
    };
    const System& system(int systemId);   // m_mutex held

    std::mutex m_mutex;
    std::unordered_map<int, System> m_systems;
    std::unordered_map<int, std::string> m_systemDirs;   // id -> system folder path
    bool m_listed = false;
    void listSystems();   // m_mutex held
    std::string m_assetRoot;
};

// "system_9.Epsilon_Eridani.eps_Eri_b" -> 9; -1 when the id has no system prefix
int mapBodySystemId(const std::string& bodyId);

// The fingerprint of a body's bake inputs at this width; 0 when the body has no generated descriptor.
std::uint64_t mapBodyFingerprint(MapBodyCatalog& catalog, int systemId, const std::string& bodyId, int width);

// Bakes the body. threads: worker threads for the texel loops (>= 1). abort may be null.
// false: no generated body (stars, belts, stations) or the surface failed (error says which, empty
// for "no descriptor").
bool bakeMapBody(MapBodyCatalog& catalog, int systemId, const std::string& bodyId, int width, int threads,
                 const std::atomic<bool>* abort, MapBodyBake& out, std::string& error);

// The probe of the body's CURRENT surface field (builds / shares it via acquireBodySurface).
bool mapBodyProbe(MapBodyCatalog& catalog, int systemId, const std::string& bodyId, int width, MapBodyProbe& out,
                  std::string& error);
// equal within rounding (a generator change moves colours far more)
bool mapBodyProbesMatch(const MapBodyProbe& a, const MapBodyProbe& b);

// albedo + normal to BC1 mip chains (CPU, ~tens of ms at 1024 x 512)
MapBodyPrebake compressMapBodyBake(const MapBodyBake& bake, std::uint64_t fingerprint);

// <dir>/<systemId>/<body file name>.emb
std::string mapBodyPrebakePath(const std::string& dir, int systemId, const std::string& bodyId);
std::string mapBodyPrebakeDir(const std::string& assetRoot);   // shipped, read-only
std::string mapBodyRuntimeCacheDir(const std::string& assetRoot);   // writable (created), "" if none

bool writeMapBodyPrebake(const std::string& path, const MapBodyPrebake& prebake);
// false when missing, damaged, or of another fingerprint / version
bool readMapBodyPrebake(const std::string& path, std::uint64_t expectedFingerprint, MapBodyPrebake& out);
} // namespace world::celestial::surface
