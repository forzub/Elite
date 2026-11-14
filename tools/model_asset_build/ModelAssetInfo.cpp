// EliteModelAssetInfo <package.elmodel>: what a package holds (calibration, bounds, nodes, LODs, sockets) - for
// comparing a mesh editor build with the shipped package (MESH_EDITOR_SPEC 16.7).
#include <cstdio>
#include <string>

#include "src/model_asset/ModelAssetBinary.h"

using namespace elite::model_asset;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: EliteModelAssetInfo <package.elmodel>\n");
        return 2;
    }
    ModelAsset a;
    std::string error;
    if (!ModelAssetBinary::load(argv[1], a, &error))
    {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    const auto& p = a.physicalSize;
    std::printf("%s (%s)\n  calibration: enabled=%d axis=%d sourceExtent=%.4f targetMeters=%.3f sourceToMeters=%.6f space=%d\n",
                a.assetId.c_str(), a.displayName.c_str(), p.enabled ? 1 : 0, static_cast<int>(p.axis), p.sourceExtent, p.targetMeters,
                p.sourceToMeters, static_cast<int>(p.geometrySpace));
    std::printf("  bounds: (%.3f %.3f %.3f) .. (%.3f %.3f %.3f) size (%.3f %.3f %.3f)\n", a.minBounds.x, a.minBounds.y, a.minBounds.z,
                a.maxBounds.x, a.maxBounds.y, a.maxBounds.z, a.maxBounds.x - a.minBounds.x, a.maxBounds.y - a.minBounds.y,
                a.maxBounds.z - a.minBounds.z);
    std::printf("  nodes %zu, materials %zu, collision %zu, structural links %zu, sockets %zu, states %zu\n", a.nodes.size(),
                a.materials.size(), a.collisionVolumes.size(), a.structuralLinks.size(), a.sockets.size(), a.stateVariants.size());
    for (std::size_t i = 0; i < a.nodes.size() && i < 8; ++i)
        std::printf("    node %s pos (%.2f %.2f %.2f) rot (%.1f %.1f %.1f)\n", a.nodes[i].id.c_str(), a.nodes[i].localPosition.x,
                    a.nodes[i].localPosition.y, a.nodes[i].localPosition.z, a.nodes[i].localRotationDeg.x, a.nodes[i].localRotationDeg.y,
                    a.nodes[i].localRotationDeg.z);
    for (const auto& lod : a.renderLods)
    {
        std::size_t tris = 0;
        for (const auto& g : lod.geometries)
            tris += g.mesh.triangles.size();
        std::printf("  LOD%u role=%d geometries %zu nodes %zu triangles %zu bounds size (%.3f %.3f %.3f)\n", lod.level, static_cast<int>(lod.role),
                    lod.geometries.size(), lod.nodes.size(), tris, lod.maxBounds.x - lod.minBounds.x, lod.maxBounds.y - lod.minBounds.y,
                    lod.maxBounds.z - lod.minBounds.z);
    }
    for (std::size_t i = 0; i < a.sockets.size() && i < 6; ++i)
        std::printf("    socket %s kind=%s profile=%s\n", a.sockets[i].id.c_str(), a.sockets[i].kind.c_str(), a.sockets[i].interfaceProfile.c_str());
    return 0;
}
