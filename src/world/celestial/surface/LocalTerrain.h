#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/celestial/surface/SurfaceField.h"

namespace world::celestial::surface
{
/*
    Camera-centred local terrain (production near-surface representation).

    The planet stays ONE continuous field. Near the camera it is evaluated
    on nested square grids (a clipmap) laid on a fixed planet-fixed lattice:
      - chart: the equal-angle coordinates (u, v) of the cube face under the
        camera (valid beyond the face edge, so no seam is ever crossed by a
        level); the face only changes with hysteresis;
      - level k has lattice step du_k = 2^k * BaseStep and (cells+1)^2
        vertices centred on the camera (centre index snapped to even), so
        every vertex of level k sits on a lattice point and its value is
        SurfaceField::sample(lattice direction, 2 * spacing_k) - a pure
        function of (field, face, k, i, j). Rebuilding a level at a new
        centre reproduces the shared vertices bit for bit;
      - each vertex carries its morph target (the same point on level k+1,
        linear along the coarse triangles) and its lattice offset; the
        vertex shader blends towards the coarse surface by the distance to
        the camera, so the outer ring of level k equals the coarser level
        exactly (watertight, no popping); the inner hole of level k+1 is
        where level k draws.
    There is no patch identity, no residency per tile and no stitching: the
    level set is a view-dependent sampling of the same world, rebuilt on
    worker threads when the camera moves (recentre threshold), and it
    vanishes when the camera leaves.
*/
struct TerrainVertex
{
    glm::vec3 position {0.0f};    // body frame, relative to the level anchor (m)
    glm::vec3 morphTarget {0.0f}; // the same point on the next coarser level
    glm::vec3 normal {0.0f, 1.0f, 0.0f};
    glm::vec2 lattice {0.0f};     // offset from the level centre (cells)
    std::uint8_t land[4] {};      // land colour (sqrt), a = ice / snow
    std::uint8_t water[4] {};     // water colour (sqrt), a = emission
    float coastM = 0.0f;          // < 0 water (interpolated per pixel)
    float heightM = 0.0f;         // displayed height (contours, haze)
};

struct TerrainLevelKey
{
    int face = 0;
    int level = 0;
    std::int64_t ci = 0; // centre lattice index (even)
    std::int64_t cj = 0;
    bool operator==(const TerrainLevelKey& o) const
    {
        return face == o.face && level == o.level && ci == o.ci && cj == o.cj;
    }
};

struct TerrainLevel
{
    TerrainLevelKey key;
    int cells = 128;
    glm::dvec3 anchorBody {0.0};  // body-frame position of the centre vertex (m)
    std::vector<TerrainVertex> vertices; // (cells + 1)^2, row-major (j, i)
    double buildSeconds = 0.0;
    std::size_t samples = 0;
};

struct LocalTerrainSettings
{
    int cells = 128;              // per level side (even, multiple of 4)
    double pixelAngleRad = 1.0e-3;
    double nadirCellPixels = 4.0; // finest cell size seen straight down
    double finestSpacingM = 0.25; // reachability policy
    int maxLevels = 14;
    double activationAltitudeRadii = 0.6;
};

struct LocalTerrainPlan
{
    bool active = false;
    int face = 0;
    int finestLevel = 0;
    int coarsestLevel = 0;
    double altitudeM = 0.0;
    double horizonM = 0.0;
    std::vector<TerrainLevelKey> levels; // finest first
};

namespace terrain_chart
{
    // Lattice step of level 0 in chart units (u spans [-1, 1] per face).
    constexpr double BaseStep = 1.0 / 1073741824.0; // 2^-30
    glm::dvec3 direction(int face, double u, double v);
    void coordinates(int face, const glm::dvec3& dir, double& u, double& v);
    int dominantFace(const glm::dvec3& dir);
    double step(int level);
    // Metric spacing of a level at the chart point (m).
    double spacingM(int level, double radiusM);
}

// face: the currently displayed face (-1 none) - kept while the camera is
// within its hysteresis band.
LocalTerrainPlan planLocalTerrain(const glm::dvec3& cameraBody, double groundHeightM, double radiusM,
                                  const LocalTerrainSettings& settings, int currentFace);

// Builds one level (any thread). Returns false when cancelled.
bool buildTerrainLevel(const SurfaceField& field, const TerrainLevelKey& key, int cells, TerrainLevel& out,
                       const std::atomic<bool>* cancel = nullptr);

// Cells of level `coarse` covered by the finer level (its hole), as
// [i0, i1) x [j0, j1) in the coarse level's cell indices; empty when the
// finer level is on another face.
bool terrainHole(const TerrainLevelKey& coarse, const TerrainLevelKey& finer, int cells, int& i0, int& i1, int& j0,
                 int& j1);
} // namespace world::celestial::surface
