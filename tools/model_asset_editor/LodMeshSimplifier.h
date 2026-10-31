#pragma once

#include <cstddef>
#include <string>

#include "src/model_asset/ModelAsset.h"

namespace elite::model_asset::editor
{

inline constexpr const char* LodSurfaceSimplifyAlgorithmId = "lod_surface_qem_region_constrained_v1";

struct LodSimplifyStats
{
    std::size_t inputTriangles = 0;
    std::size_t outputTriangles = 0;
    std::size_t components = 0;
    std::size_t simplifiedComponents = 0;
    std::size_t skippedSmallComponents = 0;
    std::size_t skippedNonManifoldComponents = 0;
    std::size_t skippedTexturedComponents = 0;
    std::size_t pinnedVertices = 0;
    std::size_t collapses = 0;
    double targetError = 0.0;
    double decimateSeconds = 0.0;
    double canonicalSeconds = 0.0;
    bool canonical = true;
    std::string error;
};

// Error-driven surface simplification of one canonical mesh, used by the LOD
// generator AFTER the disconnected-component cull (the two are independent
// mechanisms). Units are the mesh's own (authoring) units.
//
// libigl edge-collapse decimation (igl::decimate) with Garland-Heckbert
// quadrics of the ORIGINAL face planes, unweighted, so the collapse cost is
// the sum of squared distances to those planes; an edge whose cost exceeds
// targetError^2 is never collapsed (triangle count is a result, not a goal).
//
// Preserved: smoothing regions (hard crease > 25 deg, material seam,
// smoothing group, mesh boundary) are never cut across and their border
// lines keep their place (constraint planes; corners and authored-edge
// vertices are pinned), independent sheets stay independent (canonical edge
// topology, no positional welding), faces never flip, textured / UV
// components and non-edge-manifold components are left untouched. The result
// is re-canonicalized (render vertices, normals, edge flags).
MeshLod simplifyLodSurface(const MeshLod& source, double targetError, LodSimplifyStats* stats = nullptr);

} // namespace elite::model_asset::editor
