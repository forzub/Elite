#include "src/world/celestial/surface/LocalTerrain.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace world::celestial::surface
{
namespace
{
    constexpr double QuarterPi = 0.78539816339744831;

    void faceFrame(int face, glm::dvec3& n, glm::dvec3& u, glm::dvec3& v)
    {
        switch (face)
        {
            case 0: n = {1, 0, 0}; u = {0, 0, -1}; v = {0, 1, 0}; break;
            case 1: n = {-1, 0, 0}; u = {0, 0, 1}; v = {0, 1, 0}; break;
            case 2: n = {0, 1, 0}; u = {1, 0, 0}; v = {0, 0, -1}; break;
            case 3: n = {0, -1, 0}; u = {1, 0, 0}; v = {0, 0, 1}; break;
            case 4: n = {0, 0, 1}; u = {1, 0, 0}; v = {0, 1, 0}; break;
            default: n = {0, 0, -1}; u = {-1, 0, 0}; v = {0, 1, 0}; break;
        }
    }

    std::int64_t evenRound(double x)
    {
        return static_cast<std::int64_t>(std::llround(x * 0.5)) * 2;
    }

    std::uint8_t unorm(float x)
    {
        return static_cast<std::uint8_t>(std::clamp(x, 0.0f, 1.0f) * 255.0f + 0.5f);
    }
}

namespace terrain_chart
{
    glm::dvec3 direction(int face, double u, double v)
    {
        glm::dvec3 n, a, b;
        faceFrame(face, n, a, b);
        // Equal-angle; |u| < 2 stays finite (tan < inf), the levels never
        // reach that far.
        return glm::normalize(n + a * std::tan(std::clamp(u, -1.95, 1.95) * QuarterPi) +
                              b * std::tan(std::clamp(v, -1.95, 1.95) * QuarterPi));
    }

    void coordinates(int face, const glm::dvec3& dir, double& u, double& v)
    {
        glm::dvec3 n, a, b;
        faceFrame(face, n, a, b);
        const double z = glm::dot(dir, n);
        u = std::atan2(glm::dot(dir, a), z) / QuarterPi;
        v = std::atan2(glm::dot(dir, b), z) / QuarterPi;
    }

    int dominantFace(const glm::dvec3& d)
    {
        const glm::dvec3 a = glm::abs(d);
        if (a.x >= a.y && a.x >= a.z)
            return d.x > 0.0 ? 0 : 1;
        if (a.y >= a.z)
            return d.y > 0.0 ? 2 : 3;
        return d.z > 0.0 ? 4 : 5;
    }

    double step(int level)
    {
        return std::ldexp(BaseStep, level);
    }

    double spacingM(int level, double radiusM)
    {
        return step(level) * QuarterPi * radiusM;
    }
}

LocalTerrainPlan planLocalTerrain(const glm::dvec3& cameraBody, double groundHeightM, double radiusM,
                                  const LocalTerrainSettings& settings, int currentFace)
{
    LocalTerrainPlan plan;
    const double r = glm::length(cameraBody);
    if (r <= 0.0)
        return plan;
    const glm::dvec3 nadir = cameraBody / r;
    const double ground = radiusM + groundHeightM;
    plan.altitudeM = std::max(0.5, r - ground);
    if (plan.altitudeM > settings.activationAltitudeRadii * radiusM)
        return plan;
    plan.horizonM = std::sqrt(std::max(0.0, r * r - ground * ground));

    // Face with hysteresis: keep the displayed chart while the nadir is
    // within 1.3 of its centre (equal-angle units).
    plan.face = terrain_chart::dominantFace(nadir);
    if (currentFace >= 0)
    {
        double u = 0.0, v = 0.0;
        terrain_chart::coordinates(currentFace, nadir, u, v);
        if (std::abs(u) < 1.3 && std::abs(v) < 1.3 && glm::dot(nadir, terrain_chart::direction(currentFace, 0, 0)) > 0.0)
            plan.face = currentFace;
    }
    double u = 0.0, v = 0.0;
    terrain_chart::coordinates(plan.face, nadir, u, v);

    // Finest level: a cell of nadirCellPixels straight down; coarsest: the
    // horizon (plus margin) inside its half extent.
    const double footprint = plan.altitudeM * settings.pixelAngleRad * settings.nadirCellPixels;
    const double finest = std::max(settings.finestSpacingM, footprint);
    const double s0 = terrain_chart::spacingM(0, radiusM);
    plan.finestLevel = std::max(0, static_cast<int>(std::ceil(std::log2(finest / s0))));
    const double half = settings.cells * 0.5;
    const double reach = std::min(plan.horizonM * 1.15 + plan.altitudeM, radiusM * 0.8);
    plan.coarsestLevel = plan.finestLevel;
    while (plan.coarsestLevel - plan.finestLevel + 1 < settings.maxLevels &&
           half * terrain_chart::spacingM(plan.coarsestLevel, radiusM) < reach)
        ++plan.coarsestLevel;
    for (int k = plan.finestLevel; k <= plan.coarsestLevel; ++k)
    {
        const double du = terrain_chart::step(k);
        plan.levels.push_back(TerrainLevelKey {plan.face, k, evenRound(u / du), evenRound(v / du)});
    }
    plan.active = true;
    return plan;
}

bool buildTerrainLevel(const SurfaceField& field, const TerrainLevelKey& key, int cells, TerrainLevel& out,
                       const std::atomic<bool>* cancel)
{
    const auto start = std::chrono::steady_clock::now();
    const double radius = field.referenceRadiusM();
    const double du = terrain_chart::step(key.level);
    const double band = 2.0 * terrain_chart::spacingM(key.level, radius);
    const double coarseBand = 2.0 * band;
    const int n = cells;
    const int half = n / 2;
    const int b = n + 3; // one border ring for the normals

    out.key = key;
    out.cells = n;
    const auto latticeDir = [&](std::int64_t li, std::int64_t lj, double step) {
        return terrain_chart::direction(key.face, static_cast<double>(li) * step, static_cast<double>(lj) * step);
    };
    const auto shown = [](const SurfaceSample& s) {
        return s.water ? s.waterLevelM : std::max(s.heightM, s.waterLevelM + 0.5);
    };

    // Fine grid with border.
    std::vector<SurfaceSample> fine(static_cast<std::size_t>(b) * b);
    std::vector<glm::dvec3> fineDir(fine.size());
    for (int j = 0; j < b; ++j)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;
        for (int i = 0; i < b; ++i)
        {
            const std::size_t o = static_cast<std::size_t>(j) * b + i;
            fineDir[o] = latticeDir(key.ci - half - 1 + i, key.cj - half - 1 + j, du);
            fine[o] = field.sample(fineDir[o], band);
        }
    }
    // Coarse lattice (level k + 1) covering the grid: the morph targets.
    const int cn = half + 1;
    const std::int64_t c0i = key.ci / 2 - half / 2;
    const std::int64_t c0j = key.cj / 2 - half / 2;
    std::vector<glm::dvec3> coarse(static_cast<std::size_t>(cn) * cn);
    for (int j = 0; j < cn; ++j)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;
        for (int i = 0; i < cn; ++i)
        {
            const glm::dvec3 d = latticeDir(c0i + i, c0j + j, du * 2.0);
            coarse[static_cast<std::size_t>(j) * cn + i] = d * (radius + shown(field.sample(d, coarseBand)));
        }
    }

    const std::size_t centre = static_cast<std::size_t>(half + 1) * b + (half + 1);
    out.anchorBody = fineDir[centre] * (radius + shown(fine[centre]));
    out.vertices.assign(static_cast<std::size_t>(n + 1) * (n + 1), TerrainVertex {});
    const auto coarseAt = [&](int ci, int cj) { return coarse[static_cast<std::size_t>(cj) * cn + ci]; };
    for (int j = 0; j <= n; ++j)
    {
        if (cancel && cancel->load(std::memory_order_relaxed))
            return false;
        for (int i = 0; i <= n; ++i)
        {
            const std::size_t o = static_cast<std::size_t>(j + 1) * b + (i + 1);
            const SurfaceSample& s = fine[o];
            const glm::dvec3 p = fineDir[o] * (radius + shown(s));
            const auto pos = [&](std::size_t q) { return fineDir[q] * (radius + shown(fine[q])); };
            glm::dvec3 nrm = glm::normalize(glm::cross(pos(o + 1) - pos(o - 1), pos(o + b) - pos(o - b)));
            if (glm::dot(nrm, fineDir[o]) < 0.0)
                nrm = -nrm;

            // Morph target: the coarse lattice point, or the midpoint of the
            // coarse edge / diagonal (cells split from (0,0) to (1,1)).
            const std::int64_t li = key.ci - half + i;
            const std::int64_t lj = key.cj - half + j;
            const int ci = static_cast<int>((li >> 1) - c0i);
            const int cj = static_cast<int>((lj >> 1) - c0j);
            const bool oddI = (li & 1) != 0;
            const bool oddJ = (lj & 1) != 0;
            glm::dvec3 target;
            if (!oddI && !oddJ)
                target = coarseAt(ci, cj);
            else if (oddI && !oddJ)
                target = 0.5 * (coarseAt(ci, cj) + coarseAt(ci + 1, cj));
            else if (!oddI && oddJ)
                target = 0.5 * (coarseAt(ci, cj) + coarseAt(ci, cj + 1));
            else
                target = 0.5 * (coarseAt(ci, cj) + coarseAt(ci + 1, cj + 1));

            const double slope = 1.0 - glm::dot(nrm, fineDir[o]);
            SurfaceSample landSample = s;
            if (s.water)
            {
                landSample.water = false;
                landSample.lake = false;
                landSample.coastM = 1.0;
                landSample.macroHeightM = s.waterLevelM + 1.0;
            }
            const SurfaceAppearance land = field.appearance(landSample, fineDir[o], slope);
            glm::vec3 waterColour(0.03f, 0.10f, 0.16f);
            float emission = land.emission;
            if (s.water)
            {
                const SurfaceAppearance wa = field.appearance(s, fineDir[o], 0.0);
                waterColour = wa.albedo;
                emission = wa.emission;
            }
            TerrainVertex& v = out.vertices[static_cast<std::size_t>(j) * (n + 1) + i];
            v.position = glm::vec3(p - out.anchorBody);
            v.morphTarget = glm::vec3(target - out.anchorBody);
            v.normal = glm::vec3(nrm);
            v.lattice = glm::vec2(static_cast<float>(i - half), static_cast<float>(j - half));
            const glm::vec3 l = glm::sqrt(glm::clamp(land.albedo, glm::vec3(0.0f), glm::vec3(1.0f)));
            const glm::vec3 w = glm::sqrt(glm::clamp(waterColour, glm::vec3(0.0f), glm::vec3(1.0f)));
            const float ice = land.weights[static_cast<std::size_t>(SurfaceMaterial::Ice)] +
                              land.weights[static_cast<std::size_t>(SurfaceMaterial::Snow)];
            v.land[0] = unorm(l.r); v.land[1] = unorm(l.g); v.land[2] = unorm(l.b); v.land[3] = unorm(ice);
            v.water[0] = unorm(w.r); v.water[1] = unorm(w.g); v.water[2] = unorm(w.b); v.water[3] = unorm(emission);
            v.coastM = static_cast<float>(std::clamp(s.coastM, -60000.0, 60000.0));
            v.heightM = static_cast<float>(shown(s));
        }
    }
    out.samples = fine.size() + coarse.size();
    out.buildSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    return true;
}

bool terrainHole(const TerrainLevelKey& coarse, const TerrainLevelKey& finer, int cells, int& i0, int& i1, int& j0,
                 int& j1)
{
    if (coarse.face != finer.face || finer.level + 1 != coarse.level)
        return false;
    const std::int64_t half = cells / 2;
    // Finer footprint in coarse lattice units: centre / 2 +- half / 2.
    const std::int64_t fi0 = finer.ci / 2 - half / 2;
    const std::int64_t fj0 = finer.cj / 2 - half / 2;
    const std::int64_t base_i = coarse.ci - half;
    const std::int64_t base_j = coarse.cj - half;
    i0 = static_cast<int>(std::clamp<std::int64_t>(fi0 - base_i, 0, cells));
    i1 = static_cast<int>(std::clamp<std::int64_t>(fi0 + half - base_i, 0, cells));
    j0 = static_cast<int>(std::clamp<std::int64_t>(fj0 - base_j, 0, cells));
    j1 = static_cast<int>(std::clamp<std::int64_t>(fj0 + half - base_j, 0, cells));
    return i1 > i0 && j1 > j0;
}
} // namespace world::celestial::surface
