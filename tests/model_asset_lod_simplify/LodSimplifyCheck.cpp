// Headless check of the LOD generator's surface-simplification stage.
//
//   EliteLodSimplifyCheck <working.elmodel> [--factor F] [--write out.elmodel]
//
// The generated LOD1..N of a WORKING package are the component-cull output of
// LOD0 at their feature threshold (relativeGeometricError x LOD0 placed
// characteristic). For every level and geometry this runs the same
// simplifyLodSurface() the editor runs after the cull, with the editor's
// target (max(F x threshold, measured cull deviation)), and prints triangles,
// target and the MEASURED two-sided deviation from the LOD0 geometry.
// Instance families: each geometry is simplified once, whatever its usage.
// --write saves the package with the simplified levels (for BUILD / flight).

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "src/model_asset/ModelAssetBinary.h"
#include "src/model_asset/ModelAssetLodError.h"
#include "src/model_asset/ModelAssetPresentation.h"
#include "tools/model_asset_editor/CanonicalMeshBuilder.h"
#include "tools/model_asset_editor/LodMeshSimplifier.h"

using namespace elite::model_asset;

namespace
{

// Grid patch: corners p, p+u, p+v, p+u+v, n x n quads, own vertices (face
// borders are duplicated as in a canonical render mesh), height(s,t) along w.
template <typename Height>
void addPatch(MeshLod& mesh, glm::vec3 p, glm::vec3 u, glm::vec3 v, int n, std::int32_t material, Height height)
{
    const glm::vec3 w = glm::normalize(glm::cross(u, v));
    const std::uint32_t base = static_cast<std::uint32_t>(mesh.vertices.size());
    for (int j = 0; j <= n; ++j)
        for (int i = 0; i <= n; ++i)
        {
            const float s = static_cast<float>(i) / n, t = static_cast<float>(j) / n;
            Vertex vertex;
            vertex.position = p + u * s + v * t + w * height(s, t);
            vertex.normal = w;
            mesh.vertices.push_back(vertex);
        }
    const auto at = [&](int i, int j) { return base + static_cast<std::uint32_t>(j * (n + 1) + i); };
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
        {
            Triangle a, b;
            a.a = at(i, j); a.b = at(i + 1, j); a.c = at(i + 1, j + 1);
            b.a = at(i, j); b.b = at(i + 1, j + 1); b.c = at(i, j + 1);
            a.materialIndex = b.materialIndex = material;
            mesh.triangles.push_back(a);
            mesh.triangles.push_back(b);
        }
}

int syntheticChecks()
{
    int failures = 0;
    const auto check = [&](bool ok, const std::string& what) {
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok) ++failures;
    };
    const auto flat = [](float, float) { return 0.0f; };

    // 1. Open flat sheet: collapses to a couple of triangles, boundary kept.
    {
        MeshLod sheet;
        addPatch(sheet, glm::vec3(0), glm::vec3(10, 0, 0), glm::vec3(0, 0, -10), 20, 0, flat);
        editor::LodSimplifyStats stats;
        const MeshLod out = editor::simplifyLodSurface(sheet, 0.01, &stats);
        const float error = meshDeviation(sheet, out);
        char line[200];
        std::snprintf(line, sizeof(line), "open flat sheet 800 T -> %zu T, measured error %.5f (boundary kept)", out.triangles.size(), error);
        check(out.triangles.size() <= 4 && error < 1.0e-4f, line);
    }
    // 2. Box, subdivided faces, top face another material: creases, corners
    //    and the material seam keep their place; faces stay flat.
    {
        MeshLod box;
        const float s = 8.0f; // exact float grid: shared box edges weld exactly
        addPatch(box, {0, 0, 0}, {s, 0, 0}, {0, s, 0}, 8, 0, flat);      // z = 0
        addPatch(box, {s, 0, -s}, {-s, 0, 0}, {0, s, 0}, 8, 0, flat);    // z = -s
        addPatch(box, {0, 0, -s}, {0, 0, s}, {0, s, 0}, 8, 0, flat);     // x = 0
        addPatch(box, {s, 0, 0}, {0, 0, -s}, {0, s, 0}, 8, 0, flat);     // x = s
        addPatch(box, {0, s, 0}, {s, 0, 0}, {0, 0, -s}, 8, 1, flat);     // y = s (material 1)
        addPatch(box, {0, 0, -s}, {s, 0, 0}, {0, 0, s}, 8, 0, flat);     // y = 0
        editor::LodSimplifyStats stats;
        const MeshLod out = editor::simplifyLodSurface(box, 0.01, &stats);
        const float error = meshDeviation(box, out);
        std::printf("      box: components %zu simplified %zu small %zu non-manifold %zu collapses %zu canonical %d %s\n",
                    stats.components, stats.simplifiedComponents, stats.skippedSmallComponents,
                    stats.skippedNonManifoldComponents, stats.collapses, stats.canonical ? 1 : 0, stats.error.c_str());
        std::size_t topTriangles = 0;
        bool topOnTop = true;
        for (const auto& t : out.triangles)
            if (t.materialIndex == 1)
            {
                ++topTriangles;
                for (const std::uint32_t c : {t.a, t.b, t.c})
                    topOnTop = topOnTop && std::abs(out.vertices[c].position.y - s) < 1.0e-4f;
            }
        char line[220];
        std::snprintf(line, sizeof(line), "subdivided box 768 T -> %zu T, measured error %.5f, top material %zu T all on y=%g",
                      out.triangles.size(), error, topTriangles, s);
        check(out.triangles.size() <= 24 && error < 1.0e-3f && topTriangles >= 2 && topOnTop, line);
        const auto analysis = editor::analyzeCanonicalMesh(out);
        check(!analysis.structuralInvalid && analysis.degenerateTriangles == 0 && analysis.windingConflicts == 0,
              "simplified box passes the canonical mesh contract");
    }
    // 3. Smooth bump: error-driven - measured deviation stays near the target.
    {
        MeshLod bump;
        addPatch(bump, glm::vec3(0), glm::vec3(10, 0, 0), glm::vec3(0, 0, -10), 40, 0,
                 [](float a, float b) { return 1.5f * std::sin(3.14159265f * a) * std::sin(3.14159265f * b); });
        for (const double target : {0.02, 0.1})
        {
            editor::LodSimplifyStats stats;
            const MeshLod out = editor::simplifyLodSurface(bump, target, &stats);
            const float error = meshDeviation(bump, out);
            char line[200];
            std::snprintf(line, sizeof(line), "bump 3200 T, target %.3f -> %zu T, measured %.4f (<= 2 x target)", target,
                          out.triangles.size(), error);
            check(out.triangles.size() < bump.triangles.size() / 2 && error <= 2.0 * target, line);
        }
    }
    std::printf("RESULT failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && std::strcmp(argv[1], "--synthetic") == 0) return syntheticChecks();
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: EliteLodSimplifyCheck --synthetic | <working.elmodel> [--factor F] [--level N] [--only geometryId] [--write out.elmodel]\n");
        return 2;
    }
    double factor = 1.0;
    std::string writePath, only;
    std::size_t onlyLevel = 0;
    bool noCull = false; // what-if: simplify LOD0 directly (no component cull)
    for (int i = 2; i < argc; ++i)
        if (std::strcmp(argv[i], "--no-cull") == 0) noCull = true;
    for (int i = 2; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--factor") == 0) factor = std::atof(argv[++i]);
        else if (std::strcmp(argv[i], "--write") == 0) writePath = argv[++i];
        else if (std::strcmp(argv[i], "--only") == 0) only = argv[++i];
        else if (std::strcmp(argv[i], "--level") == 0) onlyLevel = static_cast<std::size_t>(std::atoi(argv[++i]));
    }
    ModelAsset asset;
    std::string error;
    if (!ModelAssetBinary::load(argv[1], asset, &error))
    {
        std::fprintf(stderr, "load failed: %s\n", error.c_str());
        return 1;
    }
    if (asset.renderLods.size() < 2)
    {
        std::fprintf(stderr, "no generated LODs\n");
        return 1;
    }
    const presentation::Index index = presentation::buildIndex(asset);
    const double characteristic = index.characteristicM;
    const RenderLod& lod0 = asset.renderLods[0];
    std::printf("asset %s  LOD0 placed characteristic %.4f source units  factor %.2f\n", asset.assetId.c_str(),
                characteristic, factor);

    int failures = 0;
    const auto totalStart = std::chrono::steady_clock::now();
    for (std::size_t li = 1; li < asset.renderLods.size(); ++li)
    {
        if (onlyLevel != 0 && li != onlyLevel) continue;
        RenderLod& lod = asset.renderLods[li];
        const double threshold = lod.relativeGeometricError * characteristic;
        std::printf("\nLOD%zu  feature threshold %.5f  (%s)\n", li, threshold, lod.sourceKind.c_str());
        std::printf("  %-46s %5s %9s %9s %9s %7s %9s %9s %9s %7s\n", "geometry", "uses", "LOD0", "culled", "simplif",
                    "red%", "target", "cull_err", "measured", "sec");
        std::size_t sum0 = 0, sumCull = 0, sumOut = 0, renderedCull = 0, renderedOut = 0, rendered0 = 0;
        for (std::size_t gi = 0; gi < lod.geometries.size(); ++gi)
        {
            auto& geometry = lod.geometries[gi];
            if (!only.empty() && geometry.id != only) continue;
            const MeshLod* reference = nullptr;
            for (const auto& g0 : lod0.geometries)
                if (g0.id == geometry.id) reference = &g0.mesh;
            if (!reference) continue;
            std::size_t uses = 0;
            for (const auto& node : lod.nodes)
                if (node.enabled && node.geometryIndex == static_cast<std::int32_t>(gi)) ++uses;
            if (noCull) geometry.mesh = *reference;
            const float cullError = noCull ? 0.0f : meshDeviation(*reference, geometry.mesh);
            const double target = std::max(factor * threshold, static_cast<double>(std::max(0.0f, cullError)));
            const auto start = std::chrono::steady_clock::now();
            editor::LodSimplifyStats stats;
            MeshLod simplified = editor::simplifyLodSurface(geometry.mesh, target, &stats);
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            const float measured = meshDeviation(*reference, simplified);
            const auto analysis = editor::analyzeCanonicalMesh(simplified);
            const bool canonicalOk = !analysis.structuralInvalid && analysis.degenerateTriangles == 0 &&
                                     analysis.duplicateTriangles == 0 && analysis.windingConflicts == 0 &&
                                     analysis.insideOutClosedComponents == 0;
            if (!canonicalOk || !stats.canonical) ++failures;
            const std::size_t t0 = reference->triangles.size(), tc = geometry.mesh.triangles.size(),
                              ts = simplified.triangles.size();
            std::printf("  %-46s %5zu %9zu %9zu %9zu %6.1f%% %9.3f %9.3f %9.3f %7.2f%s\n", geometry.id.c_str(), uses, t0, tc, ts,
                        tc ? 100.0 * (1.0 - static_cast<double>(ts) / static_cast<double>(tc)) : 0.0, target, cullError, measured,
                        seconds, canonicalOk && stats.canonical ? "" : "  CANONICAL FAIL");
            std::printf("      components %zu simplified %zu small %zu non-manifold %zu textured %zu pinned %zu collapses %zu%s%s\n",
                        stats.components, stats.simplifiedComponents, stats.skippedSmallComponents,
                        stats.skippedNonManifoldComponents, stats.skippedTexturedComponents, stats.pinnedVertices,
                        stats.collapses, stats.error.empty() ? "" : "  error: ", stats.error.c_str());
            std::printf("      time decimate %.2f s  canonical %.2f s\n", stats.decimateSeconds, stats.canonicalSeconds);
            sum0 += t0;
            sumCull += tc;
            sumOut += ts;
            rendered0 += t0 * uses;
            renderedCull += tc * uses;
            renderedOut += ts * uses;
            geometry.mesh = std::move(simplified);
        }
        std::printf("  stored tris  LOD0 %zu  culled %zu  simplified %zu | rendered (instances) LOD0 %zu  culled %zu  simplified %zu (-%.1f%% vs culled)\n",
                    sum0, sumCull, sumOut, rendered0, renderedCull, renderedOut,
                    renderedCull ? 100.0 * (1.0 - static_cast<double>(renderedOut) / static_cast<double>(renderedCull)) : 0.0);
    }
    std::printf("\ntotal %.1f s  canonical failures %d\n",
                std::chrono::duration<double>(std::chrono::steady_clock::now() - totalStart).count(), failures);

    if (!writePath.empty())
    {
        if (!ModelAssetBinary::validate(asset, &error) || !ModelAssetBinary::save(writePath, asset, &error) ||
            !ModelAssetBinary::pruneStaleLods(writePath, asset, &error))
        {
            std::fprintf(stderr, "write failed: %s\n", error.c_str());
            return 1;
        }
        std::printf("written %s\n", writePath.c_str());
    }
    return failures == 0 ? 0 : 1;
}
