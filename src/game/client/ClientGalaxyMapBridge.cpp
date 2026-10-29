#include "src/game/client/ClientGalaxyMapBridge.h"

#include <algorithm>
#include <atomic>
#include <cmath>

#include "src/world/interstellar/DensityLattice.h"

namespace game::client
{

/*
    Map presentation of an interstellar-medium object: a structure (relative
    density lattice, one phase) or a background cell (log10 density and a
    phase per voxel). Background opacity follows log density so the hot
    Local Bubble cavity stays clear and the neutral wall / warm medium read
    faintly; structures use their relative density.
*/
std::shared_ptr<const world::celestial::GalaxyMapMediumVolume> galaxyMapMediumVolume(
    const world::celestial::GalaxyObjectDefinition& source)
{
    const bool background = source.objectType == "interstellar_medium_background";
    if (!background && source.objectType != "interstellar_medium_region")
        return nullptr;
    const auto& props = source.properties;
    const nlohmann::json* latticeJson = nullptr;
    if (background && props.contains("lattice"))
        latticeJson = &props["lattice"];
    else if (props.contains("geometry") && props["geometry"].contains("lattice"))
        latticeJson = &props["geometry"]["lattice"];
    world::interstellar::DensityLattice lattice;
    if (!latticeJson || !world::interstellar::latticeFromJson(*latticeJson, lattice))
        return nullptr;

    auto volume = std::make_shared<world::celestial::GalaxyMapMediumVolume>();
    volume->originLy = lattice.originLy;
    volume->voxelLy = lattice.voxelLy;
    volume->dims = lattice.dims;
    volume->background = background;
    volume->opacity.resize(lattice.size());
    if (background)
    {
        volume->phases = lattice.phases;
        for (std::size_t k = 0; k < lattice.size(); ++k)
        {
            const char phase = k < lattice.phases.size() ? lattice.phases[k] : 'h';
            const double u = phase == 'h'
                ? 0.0
                : std::clamp((std::log10(std::max(lattice.densityCm3(k), 1.0e-12)) + 1.5) / 3.5, 0.0, 1.0);
            volume->opacity[k] = static_cast<std::uint8_t>(std::lround(255.0 * u));
        }
    }
    else
    {
        const char phase = world::interstellar::mediumPhaseCode(props.value("phase", std::string()));
        volume->phases.assign(lattice.size(), phase ? phase : 'p');
        volume->opacity = lattice.values;
    }
    return volume;
}

namespace
{
using Atlas = world::celestial::GalaxyMapMediumAtlas;

struct Occupancy
{
    glm::ivec3 dims {0};
    glm::dvec3 originLy {0.0};
    double voxelLy = 1.0;
    std::vector<std::uint8_t> filled;

    bool at(int i, int j, int k) const
    {
        if (i < 0 || j < 0 || k < 0 || i >= dims.x || j >= dims.y || k >= dims.z)
            return false;
        return filled[static_cast<std::size_t>(i + dims.x * (j + dims.y * k))] != 0;
    }
};

// Voxels whose mean relative density over a block of `block`^3 lattice
// voxels reaches `threshold` (block 1 = the lattice itself, 2 = coarse LOD).
Occupancy occupancy(const world::interstellar::DensityLattice& L, int block, double threshold)
{
    Occupancy o;
    o.dims = (L.dims + glm::ivec3(block - 1)) / block;
    o.originLy = L.originLy;
    o.voxelLy = L.voxelLy * block;
    o.filled.assign(static_cast<std::size_t>(o.dims.x) * o.dims.y * o.dims.z, 0);
    for (int k = 0; k < o.dims.z; ++k)
        for (int j = 0; j < o.dims.y; ++j)
            for (int i = 0; i < o.dims.x; ++i)
            {
                double sum = 0.0;
                int n = 0;
                for (int c = 0; c < block; ++c)
                    for (int b = 0; b < block; ++b)
                        for (int a = 0; a < block; ++a)
                        {
                            const int x = i * block + a, y = j * block + b, z = k * block + c;
                            if (x >= L.dims.x || y >= L.dims.y || z >= L.dims.z)
                                continue;
                            sum += L.values[static_cast<std::size_t>(x + L.dims.x * (y + L.dims.y * z))] / 255.0;
                            ++n;
                        }
                o.filled[static_cast<std::size_t>(i + o.dims.x * (j + o.dims.y * k))] =
                    n > 0 && sum / n >= threshold;
            }
    return o;
}

/*
    Outer voxel shell: only the faces between a filled and an empty voxel,
    coplanar faces of one slice merged into rectangles (greedy meshing), so a
    structure is a few dozen to a few hundred quads with its slightly cubic,
    ragged outline. Counter-clockwise seen from outside (back faces culled:
    one translucent layer per shell). Colour premultiplied, shaded per face
    direction so the relief reads without lighting.
*/
Atlas::Range appendShell(const Occupancy& o, const glm::vec4& rgbaStraight, std::vector<Atlas::Vertex>& out)
{
    Atlas::Range range;
    range.first = static_cast<std::uint32_t>(out.size());
    const int n[3] = {o.dims.x, o.dims.y, o.dims.z};
    for (int d = 0; d < 3; ++d)
    {
        const int u = (d + 1) % 3, v = (d + 2) % 3;
        const float shadePos = d == 1 ? 1.0f : d == 0 ? 0.86f : 0.78f;
        const float shadeNeg = d == 1 ? 0.68f : d == 0 ? 0.86f : 0.78f;
        std::vector<int> mask(static_cast<std::size_t>(n[u]) * n[v]);
        for (int slice = 0; slice <= n[d]; ++slice)
        {
            for (int b = 0; b < n[v]; ++b)
                for (int a = 0; a < n[u]; ++a)
                {
                    int p[3], q[3];
                    p[d] = slice - 1; p[u] = a; p[v] = b;
                    q[d] = slice;     q[u] = a; q[v] = b;
                    const bool inner = o.at(p[0], p[1], p[2]);
                    const bool outer = o.at(q[0], q[1], q[2]);
                    mask[static_cast<std::size_t>(a + n[u] * b)] = inner == outer ? 0 : inner ? 1 : -1;
                }
            for (int b = 0; b < n[v]; ++b)
                for (int a = 0; a < n[u];)
                {
                    const int sign = mask[static_cast<std::size_t>(a + n[u] * b)];
                    if (sign == 0)
                    {
                        ++a;
                        continue;
                    }
                    int w = 1;
                    while (a + w < n[u] && mask[static_cast<std::size_t>(a + w + n[u] * b)] == sign)
                        ++w;
                    int h = 1;
                    for (bool grow = true; grow && b + h < n[v];)
                    {
                        for (int x = 0; x < w; ++x)
                            if (mask[static_cast<std::size_t>(a + x + n[u] * (b + h))] != sign)
                            {
                                grow = false;
                                break;
                            }
                        if (grow)
                            ++h;
                    }
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x)
                            mask[static_cast<std::size_t>(a + x + n[u] * (b + y))] = 0;

                    const auto corner = [&](int du, int dv) {
                        glm::dvec3 c(0.0);
                        c[d] = slice;
                        c[u] = a + du;
                        c[v] = b + dv;
                        return glm::vec3(o.originLy + c * o.voxelLy);
                    };
                    const float shade = sign > 0 ? shadePos : shadeNeg;
                    const glm::vec4 rgba(glm::vec3(rgbaStraight) * shade * rgbaStraight.a, rgbaStraight.a);
                    glm::vec3 quad[4] = {corner(0, 0), corner(w, 0), corner(w, h), corner(0, h)};
                    if (sign < 0)
                        std::swap(quad[1], quad[3]);
                    for (const int index : {0, 1, 2, 0, 2, 3})
                        out.push_back({quad[index], rgba});
                    a += w;
                }
        }
    }
    range.count = static_cast<std::uint32_t>(out.size()) - range.first;
    return range;
}

/*
    Map visual weight per phase (a presentation convention, never a physical
    value): the map says "there is medium here"; the warm partly ionised
    clouds around Sol must not read as a bright nebula. Hot cavity gas has no
    proxy; warm and enriched structures are very faint; cold neutral clouds
    stand out more; molecular / dust-rich ones most.
*/
glm::vec4 phaseStyle(char phase)
{
    switch (phase)
    {
    case 'p': return {0.30f, 0.78f, 0.80f, 0.030f};
    case 'w': return {0.40f, 0.58f, 0.95f, 0.025f};
    case 'c': return {0.82f, 0.90f, 1.00f, 0.070f};
    case 'm': return {0.95f, 0.58f, 0.28f, 0.110f};
    case 's': return {0.95f, 0.40f, 0.58f, 0.030f};
    default: return {0.0f, 0.0f, 0.0f, 0.0f};
    }
}

constexpr double ShellThreshold = 0.22;       // relative density of the outer shell
constexpr double InnerShellThreshold = 0.60;  // dense molecular cores only
} // namespace

std::shared_ptr<const world::celestial::GalaxyMapMediumAtlas> buildGalaxyMapMediumAtlas(
    const std::vector<world::celestial::GalaxyObjectDefinition>& objects,
    std::vector<int>& proxyOfObject)
{
    static std::atomic<std::uint64_t> nextUid {1};
    auto atlas = std::make_shared<Atlas>();
    atlas->uid = nextUid++;
    proxyOfObject.assign(objects.size(), -1);
    for (std::size_t index = 0; index < objects.size(); ++index)
    {
        const auto& source = objects[index];
        if (source.objectType != "interstellar_medium_region")
            continue;
        const auto& props = source.properties;
        if (!props.contains("geometry") || !props["geometry"].contains("lattice"))
            continue;
        world::interstellar::DensityLattice lattice;
        if (!world::interstellar::latticeFromJson(props["geometry"]["lattice"], lattice))
            continue;
        const char code = world::interstellar::mediumPhaseCode(props.value("phase", std::string()));
        const char phase = code ? code : 'p';
        const glm::vec4 style = phaseStyle(phase);
        if (style.a <= 0.0f)
            continue;

        Atlas::Proxy proxy;
        proxy.phase = phase;
        // Faint warm cloudlets are numerous: one coarse shell serves both
        // zoom levels; cold / molecular structures get the detailed shell.
        const bool singleLod = phase == 'p' || phase == 'w' || phase == 's';
        proxy.detailed = appendShell(occupancy(lattice, singleLod ? 2 : 1, ShellThreshold), style, atlas->vertices);
        if (proxy.detailed.count == 0)
            continue;
        proxy.coarse = singleLod ? proxy.detailed
                                 : appendShell(occupancy(lattice, 2, ShellThreshold), style, atlas->vertices);
        if (proxy.coarse.count == 0)
            proxy.coarse = proxy.detailed;
        if (phase == 'm')
            proxy.inner = appendShell(occupancy(lattice, 1, InnerShellThreshold),
                                      glm::vec4(glm::vec3(style), style.a * 0.9f), atlas->vertices);
        // Bounding sphere of the shell.
        glm::dvec3 lo(1.0e30), hi(-1.0e30);
        for (std::uint32_t v = proxy.detailed.first; v < proxy.detailed.first + proxy.detailed.count; ++v)
        {
            lo = glm::min(lo, glm::dvec3(atlas->vertices[v].positionLy));
            hi = glm::max(hi, glm::dvec3(atlas->vertices[v].positionLy));
        }
        proxy.centreLy = 0.5 * (lo + hi);
        proxy.radiusLy = static_cast<float>(0.5 * glm::length(hi - lo));
        proxy.blobRgba = glm::vec4(glm::vec3(style), style.a * 1.5f);
        proxyOfObject[index] = static_cast<int>(atlas->proxies.size());
        atlas->proxies.push_back(proxy);
    }
    return atlas;
}

} // namespace game::client
