#include "tools/planet_lab/PlanetLabChecks.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "src/world/celestial/surface/CubeSphere.h"
#include "src/world/celestial/surface/SurfaceIdentity.h"
#include "src/world/celestial/surface/SurfacePatch.h"

namespace planet_lab
{
namespace
{
    constexpr double Pi = 3.14159265358979323846;
    constexpr int Resolution = 32;

    int g_failures = 0;

    void check(bool ok, const std::string& what)
    {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
        if (!ok)
            ++g_failures;
    }

    glm::dvec3 fibonacci(int i, int n)
    {
        const double y = 1.0 - 2.0 * (i + 0.5) / n;
        const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        const double phi = i * 2.399963229728653;
        return glm::dvec3(r * std::cos(phi), y, r * std::sin(phi));
    }

    PatchKey keyContaining(const glm::dvec3& d, int level)
    {
        int best = 0;
        double bestDot = -2.0;
        for (int f = 0; f < 6; ++f)
        {
            glm::dvec3 n, u, v;
            cubeFaceFrame(f, n, u, v);
            if (glm::dot(d, n) > bestDot)
            {
                bestDot = glm::dot(d, n);
                best = f;
            }
        }
        glm::dvec3 n, u, v;
        cubeFaceFrame(best, n, u, v);
        const double a = std::atan(glm::dot(d, u) / glm::dot(d, n)) / (Pi / 4.0);
        const double b = std::atan(glm::dot(d, v) / glm::dot(d, n)) / (Pi / 4.0);
        const int cells = 1 << level;
        PatchKey k;
        k.face = best;
        k.level = level;
        k.x = std::clamp(static_cast<int>(std::floor((a + 1.0) * 0.5 * cells)), 0, cells - 1);
        k.y = std::clamp(static_cast<int>(std::floor((b + 1.0) * 0.5 * cells)), 0, cells - 1);
        return k;
    }

    // The generator's own (double) vertex position: the GPU copy is float
    // relative to the patch origin (sub-mm at landing LOD).
    const SurfaceField* g_field = nullptr;
    glm::dvec3 vertexWorld(const SurfacePatchData& p, int i, int j)
    {
        const double h = p.heights[static_cast<std::size_t>(j * (p.resolution + 1) + i)];
        return patchDirection(p.key, i, j, p.resolution) * (g_field->referenceRadiusM() + h);
    }

    std::string keyName(const PatchKey& k)
    {
        std::ostringstream s;
        s << "f" << k.face << "_L" << k.level << "_" << k.x << "_" << k.y;
        return s.str();
    }

    std::vector<int> checkLevels(const LabBody& body)
    {
        const int maxLevel = maximumPatchLevel(body.field->referenceRadiusM(), body.policy.finestVertexSpacingM, Resolution);
        std::vector<int> levels;
        for (int l : {0, 4, 8, 12, 16, 20})
            if (l <= maxLevel)
                levels.push_back(l);
        if (levels.back() != maxLevel)
            levels.push_back(maxLevel);
        return levels;
    }

    // ---- record ------------------------------------------------------------
    void recordBody(const LabBody& body, std::vector<std::pair<std::string, std::uint64_t>>& out)
    {
        const SurfaceField& field = *body.field;
        const std::string p = body.label + "/";
        out.push_back({p + "identity_seed", field.identitySeed()});
        for (int level : checkLevels(body))
        {
            const PatchKey k = keyContaining(body.focusDir, level);
            const SurfacePatchData patch = generateSurfacePatch(field, k, Resolution);
            out.push_back({p + "patch_" + keyName(k) + "_macro", patch.macroChecksum});
            out.push_back({p + "patch_" + keyName(k) + "_full", patch.checksum});
        }
        std::vector<SurfaceFeature> marks;
        field.landmarks(body.focusDir, 0.15, marks);
        Checksum lm;
        for (const auto& f : marks)
        {
            lm.add(f.featureClass);
            lm.add(f.id);
            lm.add(f.centerDir);
            lm.add(f.radiusM);
        }
        out.push_back({p + "landmarks_n" + std::to_string(marks.size()), lm.value});
        Checksum climate;
        for (int i = 0; i < 256; ++i)
        {
            const SurfaceSample s = field.sample(fibonacci(i, 256), 20000.0);
            climate.add(s.biome);
            climate.add(s.temperatureK);
            climate.add(s.moisture);
            climate.add(s.water);
        }
        out.push_back({p + "climate_samples", climate.value});
        if (body.clouds)
            for (double t : {0.0, 3.0 * 86400.0 + 1234.5})
            {
                Checksum c;
                for (std::size_t layer = 0; layer < body.clouds->layers().size(); ++layer)
                    for (int i = 0; i < 256; ++i)
                        c.add(body.clouds->coverage(layer, fibonacci(i, 256), t));
                out.push_back({p + "clouds_t" + std::to_string(static_cast<long long>(t)), c.value});
            }
        if (body.isSmallBody)
        {
            out.push_back({p + "asteroid_seed", body.smallBody.seed});
            out.push_back({p + "asteroid_radius_mm", static_cast<std::uint64_t>(body.smallBody.meanRadiusM * 1000.0)});
        }
    }

    // ---- order independence ---------------------------------------------------
    void checkOrder(const LabBody& body)
    {
        std::vector<PatchKey> keys;
        const PatchKey a = keyContaining(body.focusDir, 6);
        keys.push_back(a);
        keys.push_back(a.parent());
        for (int c = 0; c < 4; ++c)
            keys.push_back(a.child(c));
        keys.push_back(keyContaining(body.focusDir, 9));
        keys.push_back(keyContaining(-body.focusDir, 3));

        std::map<std::string, std::uint64_t> reference;
        for (const PatchKey& k : keys)
            reference[keyName(k)] = generateSurfacePatch(*body.field, k, Resolution).checksum;

        bool ok = true;
        for (int order = 0; order < 2; ++order)
        {
            std::vector<PatchKey> ordered = keys;
            if (order == 1)
            {
                std::reverse(ordered.begin(), ordered.end());
                std::rotate(ordered.begin(), ordered.begin() + 2, ordered.end());
            }
            SurfacePatchStreamer streamer(body.field, Resolution, 4);
            streamer.setRequests(ordered);
            streamer.waitIdle();
            for (const PatchKey& k : keys)
            {
                const auto patch = streamer.find(k);
                ok = ok && patch && patch->checksum == reference[keyName(k)];
            }
        }
        check(ok, body.label + " order: patches identical in sequential / reversed / threaded generation (" +
                      std::to_string(keys.size()) + " patches)");
    }

    // ---- parent / child -------------------------------------------------------
    void checkParentChild(const LabBody& body)
    {
        const SurfaceField& field = *body.field;
        const int maxLevel = maximumPatchLevel(field.referenceRadiusM(), body.policy.finestVertexSpacingM, Resolution);
        int flagMismatch = 0;
        int featureMismatch = 0;
        double macroDiff = 0.0;
        double heightDiff = 0.0;
        int compared = 0;
        for (int level : {3, 7, 11, 15})
        {
            if (level + 1 > maxLevel)
                continue;
            const PatchKey pk = keyContaining(body.focusDir, level);
            const SurfacePatchData parent = generateSurfacePatch(field, pk, Resolution);
            for (int c = 0; c < 4; ++c)
            {
                const PatchKey ck = pk.child(c);
                const SurfacePatchData child = generateSurfacePatch(field, ck, Resolution);
                const int ox = (ck.x & 1) * Resolution / 2;
                const int oy = (ck.y & 1) * Resolution / 2;
                for (int j = 0; j <= Resolution; j += 2)
                    for (int i = 0; i <= Resolution; i += 2)
                    {
                        const std::size_t ci = static_cast<std::size_t>(j * (Resolution + 1) + i);
                        const std::size_t pi = static_cast<std::size_t>((oy + j / 2) * (Resolution + 1) + ox + i / 2);
                        flagMismatch += child.water[ci] != parent.water[pi];
                        featureMismatch += child.featureIds[ci] != parent.featureIds[pi];
                        macroDiff = std::max(macroDiff, std::abs(child.macroHeights[ci] - parent.macroHeights[pi]));
                        // Below the macro cutoff a child only adds detail.
                        if (parent.minWavelengthM <= field.macroCutoffM())
                            heightDiff = std::max(heightDiff, std::abs(child.heights[ci] - parent.heights[pi]));
                        ++compared;
                    }
            }
        }
        if (compared == 0)
            return;
        char buffer[256];
        std::snprintf(buffer, sizeof buffer,
                      " parent/child: %d coincident vertices - water flag mismatches %d, landmark id mismatches %d, "
                      "max macro diff %.3g m, max height diff %.1f m (detail bound %.1f m)",
                      compared, flagMismatch, featureMismatch, macroDiff, heightDiff,
                      field.detailAmplitudeBoundM() + 0.0);
        const double bound = field.solidSurface() ? field.detailAmplitudeBoundM() : 0.0;
        check(flagMismatch == 0 && featureMismatch == 0 && macroDiff < 1.0e-3 && heightDiff <= bound + 1.0e-6,
              body.label + buffer);
    }

    // ---- coastline / landmark stability under detail ---------------------------
    void checkCoastline(const LabBody& body)
    {
        const SurfaceField& field = *body.field;
        const double ocean = body.isSmallBody ? 0.0 : body.descriptor.oceanCoverage;
        const int n = 20000;
        const double wavelengths[] = {50000.0, 5000.0, 500.0, 50.0, 5.0, 0.25};
        int flagChanges = 0;
        int featureChanges = 0;
        int macroChanges = 0;
        int landBelowWater = 0;
        int waterNotLevel = 0;
        int beyondBound = 0;
        int water = 0;
        for (int i = 0; i < n; ++i)
        {
            const glm::dvec3 d = fibonacci(i, n);
            const SurfaceSample ref = field.sample(d, wavelengths[0]);
            water += ref.water && !ref.lake;
            for (double w : wavelengths)
            {
                const SurfaceSample s = w == wavelengths[0] ? ref : field.sample(d, w);
                flagChanges += s.water != ref.water;
                featureChanges += s.featureId != ref.featureId;
                macroChanges += s.macroHeightM != ref.macroHeightM;
                if (s.water)
                    waterNotLevel += s.heightM != s.waterLevelM;
                else if (ocean > 0.0 && field.solidSurface())
                    landBelowWater += s.heightM <= 0.0;
                // Below the macro cutoff only detail is added (bounded).
                if (!s.water && w <= field.macroCutoffM() &&
                    std::abs(s.heightM - s.macroHeightM) > field.detailAmplitudeBoundM() + 6.0)
                    ++beyondBound;
            }
        }
        char buffer[320];
        std::snprintf(buffer, sizeof buffer,
                      " coastline: %d directions x 6 band limits (50 km .. 0.25 m): water flag changes %d, landmark "
                      "id changes %d, macro height changes %d, land at/below sea %d, water off its level %d, "
                      "beyond detail bound %d; ocean fraction %.3f (descriptor %.2f)",
                      n, flagChanges, featureChanges, macroChanges, landBelowWater, waterNotLevel, beyondBound,
                      static_cast<double>(water) / n, ocean);
        check(flagChanges == 0 && featureChanges == 0 && macroChanges == 0 && landBelowWater == 0 &&
                  waterNotLevel == 0 && beyondBound == 0,
              body.label + buffer);
        if (ocean > 0.0)
            check(std::abs(static_cast<double>(water) / n - ocean) < 0.06,
                  body.label + " ocean fraction matches the descriptor's ocean coverage");
    }

    // ---- seams -----------------------------------------------------------------
    void checkSeams(const LabBody& body)
    {
        const SurfaceField& field = *body.field;
        g_field = &field;
        double sameLevel = 0.0;
        double crossFace = 0.0;
        double tRatio = 0.0;
        for (int level : {2, 6, 10})
        {
            const PatchKey a = keyContaining(body.focusDir, level);
            const int cells = 1 << level;
            // Same face neighbour.
            PatchKey b = a;
            b.x = a.x + 1 < cells ? a.x + 1 : a.x - 1;
            const bool right = b.x > a.x;
            const SurfacePatchData pa = generateSurfacePatch(field, a, Resolution);
            const SurfacePatchData pb = generateSurfacePatch(field, b, Resolution);
            for (int j = 0; j <= Resolution; ++j)
                sameLevel = std::max(sameLevel, glm::length(vertexWorld(pa, right ? Resolution : 0, j) -
                                                            vertexWorld(pb, right ? 0 : Resolution, j)));
            // T-junction: the fine neighbour's edge against the coarse edge.
            const PatchKey fine = b.child(right ? 0 : 1);
            const SurfacePatchData pf = generateSurfacePatch(field, fine, Resolution);
            const int rowOffset = (fine.y & 1) * Resolution / 2;
            for (int j = 0; j <= Resolution; ++j)
            {
                const double t = rowOffset + j * 0.5;
                const int j0 = static_cast<int>(std::floor(t));
                const int j1 = std::min(Resolution, j0 + 1);
                const glm::dvec3 coarse = glm::mix(vertexWorld(pa, right ? Resolution : 0, j0),
                                                   vertexWorld(pa, right ? Resolution : 0, j1), t - j0);
                const double gap = glm::length(vertexWorld(pf, right ? 0 : Resolution, j) - coarse);
                tRatio = std::max(tRatio, gap / std::max(pa.skirtDepthM, pf.skirtDepthM));
            }
        }
        // Across a cube edge: face 4 (+Z) right edge meets face 0 (+X).
        for (int level : {1, 5})
        {
            const int cells = 1 << level;
            const PatchKey a {4, level, cells - 1, cells / 2};
            const SurfacePatchData pa = generateSurfacePatch(field, a, Resolution);
            for (int j = 0; j <= Resolution; j += 4)
            {
                const glm::dvec3 d = glm::normalize(vertexWorld(pa, Resolution, j));
                const PatchKey bk = keyContaining(glm::normalize(d + glm::dvec3(1.0e-9, 0, -1.0e-9)), level);
                if (bk.face == 4)
                    continue;
                const SurfacePatchData pb = generateSurfacePatch(field, bk, Resolution);
                double best = 1.0e30;
                for (int jj = 0; jj <= Resolution; ++jj)
                    for (int ii : {0, Resolution})
                        best = std::min(best, glm::length(vertexWorld(pb, ii, jj) - vertexWorld(pa, Resolution, j)));
                crossFace = std::max(crossFace, best);
            }
        }
        char buffer[256];
        std::snprintf(buffer, sizeof buffer,
                      " seams: shared edge gap same face %.3g m, across cube faces %.3g m; worst coarse/fine "
                      "T-junction gap = %.2f x skirt depth",
                      sameLevel, crossFace, tRatio);
        check(sameLevel < 1.0e-3 && crossFace < 1.0e-2 && tRatio < 1.0, body.label + buffer);
    }

    // ---- climate (Earth-like) ----------------------------------------------------
    void checkClimate(const LabBody& body)
    {
        const SurfaceField& field = *body.field;
        const int n = 40000;
        double ice[9] = {};
        double desert[9] = {};
        double land[9] = {};
        double all[9] = {};
        std::set<int> biomes45;
        std::set<int> biomes10;
        for (int i = 0; i < n; ++i)
        {
            const glm::dvec3 d = fibonacci(i, n);
            const SurfaceSample s = field.sample(d, 30000.0);
            const int band = std::min(8, static_cast<int>(std::abs(std::asin(d.y)) * 180.0 / Pi / 10.0));
            all[band] += 1.0;
            if (s.water && !s.lake)
            {
                if (s.iceMask > 0.5f)
                    ice[band] += 1.0; // sea ice counts as ice
                continue;
            }
            land[band] += 1.0;
            ice[band] += s.biome == Biome::IceSheet || s.biome == Biome::Snow;
            desert[band] += s.biome == Biome::Desert;
            if (band == 4)
                biomes45.insert(static_cast<int>(s.biome));
            if (band == 1)
                biomes10.insert(static_cast<int>(s.biome));
        }
        std::printf("     %s climate by latitude (land fraction / ice / desert):\n", body.label.c_str());
        int desertPeak = 0;
        for (int b = 0; b < 9; ++b)
        {
            const double l = std::max(1.0, land[b]);
            std::printf("       %2d-%2d deg: land %5.0f  ice %.2f  desert %.2f\n", b * 10, b * 10 + 10, land[b],
                        ice[b] / std::max(1.0, all[b]), desert[b] / l);
            if (desert[b] / l > desert[desertPeak] / std::max(1.0, land[desertPeak]))
                desertPeak = b;
        }
        const double polarIce = (ice[7] + ice[8]) / std::max(1.0, all[7] + all[8]);
        const double tropicalIce = (ice[0] + ice[1]) / std::max(1.0, all[0] + all[1]);
        check(polarIce > tropicalIce + 0.3, body.label + " climate: polar ice/snow (70-90 deg) >> tropical");
        check(desertPeak >= 1 && desertPeak <= 3, body.label + " climate: deserts peak in the subtropics (band " +
                                                      std::to_string(desertPeak * 10) + "-" +
                                                      std::to_string(desertPeak * 10 + 10) + " deg)");
        check(biomes45.size() >= 3 && biomes10.size() >= 2,
              body.label + " climate: biomes vary along a latitude (" + std::to_string(biomes10.size()) +
                  " at 10-20 deg, " + std::to_string(biomes45.size()) + " at 40-50 deg) - not colour stripes");
    }

    void checkClouds(const LabBody& body)
    {
        if (!body.clouds)
            return;
        const CloudWeatherField& c = *body.clouds;
        double same = 0.0;
        double moved = 0.0;
        double eq = 0.0;
        double sub = 0.0;
        int nEq = 0;
        int nSub = 0;
        const int n = 4000;
        for (int i = 0; i < n; ++i)
        {
            const glm::dvec3 d = fibonacci(i, n);
            const float a = c.coverage(0, d, 86400.0 * 10.0);
            same = std::max(same, static_cast<double>(std::abs(a - c.coverage(0, d, 86400.0 * 10.0))));
            moved += std::abs(a - c.coverage(0, d, 86400.0 * 10.0 + CloudWeatherField::EpochSeconds));
            const double lat = std::abs(std::asin(d.y)) * 180.0 / Pi;
            if (lat < 8.0)
            {
                eq += a;
                ++nEq;
            }
            else if (lat > 20.0 && lat < 30.0)
            {
                sub += a;
                ++nSub;
            }
        }
        char buffer[200];
        std::snprintf(buffer, sizeof buffer,
                      " clouds: same world time -> same sky (max diff %.1g); 6 h later mean change %.3f; equator "
                      "%.2f vs subtropics %.2f",
                      same, moved / n, eq / std::max(1, nEq), sub / std::max(1, nSub));
        // The latitude climatology applies to water clouds, not to haze.
        const bool waterClouds = c.layers().front().kind != "haze";
        check(same == 0.0 && (!waterClouds || (moved / n > 0.02 && eq / std::max(1, nEq) > sub / std::max(1, nSub))),
              body.label + buffer);
    }

    void checkAsteroid(const LabBody& body)
    {
        const SurfaceField& field = *body.field;
        int macroChanges = 0;
        double maxDetail = 0.0;
        for (int i = 0; i < 2000; ++i)
        {
            const glm::dvec3 d = fibonacci(i, 2000);
            const SurfaceSample a = field.sample(d, 500.0);
            const SurfaceSample b = field.sample(d, 0.25);
            macroChanges += a.macroHeightM != b.macroHeightM || a.featureId != b.featureId;
            maxDetail = std::max(maxDetail, std::abs(b.heightM - b.macroHeightM));
        }
        char buffer[200];
        std::snprintf(buffer, sizeof buffer,
                      " asteroid silhouette: macro radius / crater ids changed at %d of 2000 directions between "
                      "500 m and 0.25 m LOD; detail |r - macro| <= %.1f m (bound %.1f m, radius %.0f m, %s)",
                      macroChanges, maxDetail, field.detailAmplitudeBoundM(), field.referenceRadiusM(),
                      compositionName(body.smallBody.composition));
        check(macroChanges == 0 && maxDetail <= field.detailAmplitudeBoundM(), body.label + buffer);
    }

    void checkPolicy(const std::vector<LabBody>& bodies)
    {
        for (const LabBody& b : bodies)
        {
            const int maxLevel = maximumPatchLevel(b.field->referenceRadiusM(), b.policy.finestVertexSpacingM, Resolution);
            std::printf("     policy %-10s solid=%d ground=%d finest spacing %.2f m -> max patch level %d (%s)\n",
                        b.label.c_str(), b.policy.solidSurface, b.policy.groundReachable,
                        b.policy.finestVertexSpacingM, maxLevel, b.policy.reason.c_str());
            if (b.label == "gas_giant")
                check(!b.policy.solidSurface && !b.policy.groundReachable && !b.field->solidSurface(),
                      "policy: gas giant has no solid ground (cloud deck only)");
            if (b.label == "hostile")
                check(b.policy.solidSurface && !b.policy.groundReachable,
                      "policy: 180-bar surface - no ground patches requested");
            if (b.label == "earth_like")
                check(b.policy.groundReachable && b.policy.finestVertexSpacingM < 1.0,
                      "policy: Earth-like ground reachable down to sub-metre vertices");
        }
    }

    void performance(const LabBody& body)
    {
        const auto t0 = std::chrono::steady_clock::now();
        const int n = 4000;
        for (int i = 0; i < n; ++i)
            (void)body.field->sample(fibonacci(i, n), 1.0);
        const double us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / n;
        const SurfacePatchData p = generateSurfacePatch(*body.field, keyContaining(body.focusDir, 10), Resolution);
        std::printf("     perf %-10s %.1f us / ground sample, %.1f ms / 33x33 patch (level 10, single thread)\n",
                    body.label.c_str(), us, p.generationMs);
    }
}

int runLabChecks(const std::vector<LabBody>& bodies, const std::string& recordPath, const std::string& verifyPath)
{
    g_failures = 0;
    std::printf("Planet Lab checks - generator version %u\n", GeneratorVersion);

    std::vector<std::pair<std::string, std::uint64_t>> record;
    for (const LabBody& b : bodies)
        recordBody(b, record);
    if (!recordPath.empty())
    {
        std::ofstream out(recordPath);
        out << "generator_version " << GeneratorVersion << "\n";
        for (const auto& [name, value] : record)
            out << name << " " << value << "\n";
        std::printf("     recorded %zu values -> %s\n", record.size(), recordPath.c_str());
    }
    if (!verifyPath.empty())
    {
        std::ifstream in(verifyPath);
        std::map<std::string, std::uint64_t> previous;
        std::string name;
        std::uint64_t value = 0;
        while (in >> name >> value)
            previous[name] = value;
        int mismatches = 0;
        int missing = 0;
        for (const auto& [n, v] : record)
        {
            const auto it = previous.find(n);
            if (it == previous.end())
                ++missing;
            else if (it->second != v)
            {
                ++mismatches;
                std::printf("     mismatch %s\n", n.c_str());
            }
        }
        check(!previous.empty() && mismatches == 0 && missing == 0,
              "restart reproducibility: " + std::to_string(record.size()) + " values of a previous process (" +
                  verifyPath + "): " + std::to_string(mismatches) + " differ, " + std::to_string(missing) + " missing");
    }

    for (const LabBody& b : bodies)
    {
        checkOrder(b);
        if (b.field->solidSurface())
        {
            checkParentChild(b);
            checkCoastline(b);
            checkSeams(b);
        }
        if (b.label == "earth_like")
            checkClimate(b);
        checkClouds(b);
        if (b.isSmallBody)
            checkAsteroid(b);
        performance(b);
    }
    checkPolicy(bodies);
    std::printf("%s (%d failed)\n", g_failures ? "FAIL" : "PASS", g_failures);
    return g_failures;
}
} // namespace planet_lab
