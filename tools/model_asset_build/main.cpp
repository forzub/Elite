// Headless Model Asset BUILD: the same metric boundary as the editor's BUILD
// button (src/model_asset/ModelAssetBuild.h), usable from scripts.
//
//   EliteModelAssetBuild <working.elmodel> <production.elmodel> [--role N=component|module|shell|proxy]...
//
// Loads the WORKING package (manifest + every .lodN.elmesh), applies the
// explicit authoring -> metre calibration to a copy, optionally sets RenderLod
// presentation roles, validates and writes the production package.

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetBuild.h"

using namespace elite::model_asset;

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: EliteModelAssetBuild <working.elmodel> <production.elmodel> [--role N=role]...\n");
        return 2;
    }
    ModelAsset working;
    std::string error;
    if (!ModelAssetBinary::load(argv[1], working, &error))
    {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    ModelAsset production;
    try
    {
        production = makeProductionCopy(working);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "BUILD failed: %s\n", e.what());
        return 1;
    }
    for (int i = 3; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--role") != 0) continue;
        const std::string spec = argv[++i];
        const auto eq = spec.find('=');
        if (eq == std::string::npos) continue;
        const std::size_t lod = static_cast<std::size_t>(std::stoul(spec.substr(0, eq)));
        const std::string role = spec.substr(eq + 1);
        if (lod >= production.renderLods.size()) continue;
        RenderLodRole r = RenderLodRole::Component;
        if (role == "module") r = RenderLodRole::ModuleProxy;
        else if (role == "shell") r = RenderLodRole::Shell;
        else if (role == "proxy") r = RenderLodRole::Proxy;
        production.renderLods[lod].role = r;
    }
    if (!ModelAssetBinary::validate(production, &error))
    {
        std::fprintf(stderr, "validation failed: %s\n", error.c_str());
        return 1;
    }
    if (!ModelAssetBinary::save(argv[2], production, &error) ||
        !ModelAssetBinary::pruneStaleLods(argv[2], production, &error))
    {
        std::fprintf(stderr, "save failed: %s\n", error.c_str());
        return 1;
    }
    std::printf("BUILD %s -> %s  scale=%.6f nodes=%zu lods=%zu\n", argv[1], argv[2],
                working.physicalSize.sourceToMeters, production.nodes.size(), production.renderLods.size());
    for (const auto& lod : production.renderLods)
    {
        std::size_t tris = 0;
        for (const auto& g : lod.geometries) tris += g.mesh.triangles.size();
        std::printf("  LOD%u role=%d rge=%.5f geometries=%zu nodes=%zu tris=%zu bounds=(%.1f %.1f %.1f)-(%.1f %.1f %.1f)\n",
                    lod.level, static_cast<int>(lod.role), lod.relativeGeometricError, lod.geometries.size(),
                    lod.nodes.size(), tris, lod.minBounds.x, lod.minBounds.y, lod.minBounds.z, lod.maxBounds.x,
                    lod.maxBounds.y, lod.maxBounds.z);
    }
    return 0;
}
