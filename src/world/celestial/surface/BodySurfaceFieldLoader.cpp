#include "src/world/celestial/surface/BodySurfaceFieldLoader.h"
#include "src/world/celestial/surface/SettledSurfaceField.h"

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <unordered_map>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "src/core/DevMemoryLedger.h"
#include "src/world/celestial/surface/AppearancePassport.h"
#include "src/world/celestial/surface/ImportedSurfaceField.h"
#include "src/world/celestial/surface/PlanetSurfaceField.h"

// a private copy of stb_image (static functions: no clash with the renderer's)
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "src/render/bitmap/stb_image.h"

namespace world::celestial::surface
{
namespace
{
namespace fs = std::filesystem;

bool loadImage(const std::string& path, int channels, std::vector<std::uint8_t>& out, int& w, int& h)
{
    stbi_set_flip_vertically_on_load_thread(0);
    int n = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, channels);
    if (!data)
        return false;
    out.assign(data, data + static_cast<std::size_t>(w) * h * channels);
    stbi_image_free(data);
    return true;
}

// Plain string handling: these strings never go through a locale conversion
// (a game locale can reject non-ASCII bytes in fs::path).
std::string bodyFolderFromSource(const std::string& sourcePath)
{
    const std::size_t slash = sourcePath.find_last_of("/\\");
    std::string stem = slash == std::string::npos ? sourcePath : sourcePath.substr(slash + 1);
    const std::size_t dot = stem.rfind('.');
    if (dot != std::string::npos)
        stem = stem.substr(0, dot);
    for (const char* prefix : {"planet_", "moon_", "dwarf_"})
        if (stem.rfind(prefix, 0) == 0)
            return stem.substr(std::char_traits<char>::length(prefix));
    return stem;
}

// Vertical range of the curated 8-bit height sources (m).
void heightRange(const std::string& body, double& range, bool& isSigned, bool& water)
{
    isSigned = true;
    water = false;
    range = 4000.0;
    if (body == "earth") { range = 8848.0; isSigned = false; water = true; }
    else if (body == "moon") range = 10000.0;
    else if (body == "mars") range = 14000.0;
    else if (body == "mercury") range = 5000.0;
    else if (body == "io") range = 6000.0;
    else if (body == "venus") range = 7000.0;
    else if (body == "ganymede" || body == "callisto" || body == "europa") range = 3000.0;
}
} // namespace

std::string celestialAssetFolder(const std::string& name)
{
    std::string out;
    for (char c : name)
    {
        const bool lower = c >= 'a' && c <= 'z', upper = c >= 'A' && c <= 'Z', digit = c >= '0' && c <= '9';
        if (lower || digit)
            out.push_back(c);
        else if (upper)
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        else if (!out.empty() && out.back() != '_')
            out.push_back('_');
    }
    while (!out.empty() && out.back() == '_')
        out.pop_back();
    return out;
}

void findImportedBodyMaps(const std::string& assetRoot, const std::string& systemName, const std::string& sourcePath,
                          BodySurfaceSource& inOut)
{
    const std::string sys = celestialAssetFolder(systemName);
    const std::string body = celestialAssetFolder(bodyFolderFromSource(sourcePath));
    if (sys.empty() || body.empty())
        return;
    try
    {
        const fs::path source = fs::path(assetRoot) / "assets/source/celestial" / sys / body;
        const fs::path generated = fs::path(assetRoot) / "assets/generated/celestial" / sys / body;
        std::error_code ec;
        for (const fs::path& p : {source / "albedo_source.jpg", source / "albedo_source.png",
                                  generated / "lod/global/albedo_2048.tga"})
            if (fs::exists(p, ec))
            {
                inOut.importedAlbedoPath = p.generic_string();
                break;
            }
        if (!inOut.importedAlbedoPath.empty() && fs::exists(source / "height_source.png", ec))
        {
            inOut.importedHeightPath = (source / "height_source.png").generic_string();
            heightRange(body, inOut.importedHeightRangeM, inOut.importedHeightSigned, inOut.importedWaterFromHeight);
        }
    }
    catch (const std::exception&)
    {
        inOut.importedAlbedoPath.clear();
        inOut.importedHeightPath.clear();
    }
}

namespace
{
struct RegistryEntry
{
    std::weak_ptr<SurfaceField> field;
    BodySurfaceDescriptor descriptor;
    bool loading = false;
};

std::mutex& registryMutex()
{
    static std::mutex m;
    return m;
}

std::condition_variable& registryLoaded()
{
    static std::condition_variable cv;
    return cv;
}

std::unordered_map<std::string, RegistryEntry>& registry()
{
    static std::unordered_map<std::string, RegistryEntry> r;
    return r;
}

// DEV memory ledger: the unique surfaces alive in this process (caller holds the mutex)
void reportRegistryLocked()
{
    if (!core::dev::memoryLedgerEnabled())
        return;
    std::size_t bytes = 0, alive = 0;
    for (auto& [key, e] : registry())
        if (const auto f = e.field.lock())
        {
            bytes += f->memoryBytes();
            ++alive;
        }
    core::dev::memoryLedgerSet("cpu surfaces.registry_unique", bytes, alive);
}
} // namespace

void reportBodySurfaceRegistry()
{
    std::lock_guard<std::mutex> lock(registryMutex());
    reportRegistryLocked();
}

bool acquireBodySurface(const BodySurfaceSource& source, LoadedBodySurface& out)
{
    const std::string key = source.id + "|" + source.descriptorPath + "|" + source.importedAlbedoPath + "|" +
                            source.importedHeightPath;
    std::unique_lock<std::mutex> lock(registryMutex());
    for (;;)
    {
        auto& entry = registry()[key];
        if (const auto alive = entry.field.lock())
        {
            out.descriptor = entry.descriptor;
            out.field = alive;
            return true;
        }
        if (!entry.loading)
            break;
        registryLoaded().wait(lock);
    }
    registry()[key].loading = true;
    lock.unlock();
    const bool ok = loadBodySurface(source, out);
    lock.lock();
    auto& entry = registry()[key];
    entry.loading = false;
    if (ok && out.field)
    {
        entry.field = out.field;
        entry.descriptor = out.descriptor;
    }
    // drop expired entries (bodies nobody holds any more)
    for (auto it = registry().begin(); it != registry().end();)
        if (!it->second.loading && it->second.field.expired())
            it = registry().erase(it);
        else
            ++it;
    reportRegistryLocked();
    registryLoaded().notify_all();
    return ok;
}

bool loadBodySurface(const BodySurfaceSource& source, LoadedBodySurface& out)
{
    std::string error;
    if (!loadGeneratedBody(source.descriptorPath, out.descriptor, &error))
    {
        out.error = error;
        return false;
    }
    auto& d = out.descriptor;
    d.radiusM = source.radiusM;
    // a passport may choose the generator over the curated maps (AppearancePassport.h)
    const AppearancePassport* passport = findAppearancePassport(source.id);
    const bool passportProcedural = passport && passport->procedural;
    if (passportProcedural && !source.importedAlbedoPath.empty())
        std::fprintf(stderr, "[CelestialFlight] %s: appearance passport -> procedural (curated maps not used)\n",
                     source.id.c_str());
    if (!source.importedAlbedoPath.empty() && !passportProcedural)
    {
        d.source = SurfaceSourceKind::ImportedRealData;
        auto maps = std::make_shared<ImportedSurfaceMaps>();
        std::vector<std::uint8_t> rgb;
        int w = 0, h = 0;
        if (loadImage(source.importedAlbedoPath, 3, rgb, w, h))
        {
            maps->width = w;
            maps->height = h;
            maps->albedoRgb = std::move(rgb);   // 8-bit, decoded on read (ImportedSurfaceMaps::albedoAt)
        }
        std::vector<std::uint8_t> grey;
        int hw = 0, hh = 0;
        if (maps->width > 0 && !source.importedHeightPath.empty() &&
            loadImage(source.importedHeightPath, 1, grey, hw, hh))
        {
            // Height grey on the albedo grid (nearest).
            const int w = maps->width, h = maps->height;
            std::vector<std::uint8_t> g(maps->texelCount());
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    g[static_cast<std::size_t>(y) * w + x] =
                        grey[static_cast<std::size_t>(y * hh / h) * hw + static_cast<std::size_t>(x * hw / w)];
            if (source.importedWaterFromHeight)
            {
                // Earth: the sea is the ocean colour of the albedo
                // (the DEM has no zero sea). The curated maps do not
                // share a longitude origin: the DEM is aligned by the
                // column shift whose low half agrees best with the
                // ocean.
                const auto oceanColour = [](const glm::vec3& c) {
                    return c.b > 1.4f * c.r && c.b > 1.1f * c.g && (c.r + c.g + c.b) < 0.6f;
                };
                std::vector<std::uint8_t> sorted(g);
                std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
                const std::uint8_t median = sorted[sorted.size() / 2];
                int bestShift = 0;
                long bestAgree = -1;
                for (int shift : {0, w / 4, w / 2, 3 * w / 4})
                {
                    long agree = 0;
                    for (int y = 0; y < h; y += 4)
                        for (int x = 0; x < w; x += 4)
                        {
                            const std::size_t ai = static_cast<std::size_t>(y) * w + x;
                            const std::size_t gi = static_cast<std::size_t>(y) * w + (x + shift) % w;
                            agree += (g[gi] <= median) == oceanColour(maps->albedoAt(ai)) ? 1 : 0;
                        }
                    if (agree > bestAgree)
                    {
                        bestAgree = agree;
                        bestShift = shift;
                    }
                }
                std::vector<std::uint8_t> shifted(g.size());
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x)
                        shifted[static_cast<std::size_t>(y) * w + x] =
                            g[static_cast<std::size_t>(y) * w + (x + bestShift) % w];
                // Sea level grey: the low end of the land texels.
                std::vector<std::uint8_t> land;
                maps->water.assign(g.size(), 0);
                for (std::size_t i = 0; i < g.size(); ++i)
                {
                    maps->water[i] = oceanColour(maps->albedoAt(i)) ? 1 : 0;
                    if (!maps->water[i])
                        land.push_back(shifted[i]);
                }
                std::uint8_t sea = 0;
                if (!land.empty())
                {
                    std::nth_element(land.begin(), land.begin() + land.size() / 20, land.end());
                    sea = land[land.size() / 20];
                }
                std::fprintf(stderr, "[CelestialFlight] %s: curated sea %.2f of the surface, DEM shift %d, sea grey %d\n",
                             source.id.c_str(), 1.0 - static_cast<double>(land.size()) / g.size(), bestShift,
                             static_cast<int>(sea));
                maps->heightM.resize(g.size());
                for (std::size_t i = 0; i < g.size(); ++i)
                    maps->heightM[i] = maps->water[i]
                        ? -3500.0f
                        : static_cast<float>(2.0 + std::max(0, shifted[i] - sea) / std::max(1.0, 255.0 - sea) *
                                                        source.importedHeightRangeM);
            }
            else
            {
                ImportedSurfaceMaps heights;
                importedHeightsFromGrey(g.data(), w, h, source.importedHeightRangeM,
                                                 source.importedHeightSigned, heights);
                maps->heightM = std::move(heights.heightM);
            }
        }
        if (maps->width > 0)
            out.field = std::make_shared<ImportedSurfaceField>(d, maps);
    }
    if (!out.field)
        out.field = std::shared_ptr<SurfaceField>(makeSurfaceField(d).release());
    // the surface settlements of this body: flat pads / footprints in the ONE field the renderer
    // draws and the server collides with
    // airless bodies only: the style (an entrance under the ground, domes, an antenna) is theirs;
    // bodies with an atmosphere (only shuttles land) get their own style later
    if (out.field && out.field->solidSurface() && d.surfacePressurePa < 100.0)
    {
        const auto layouts = settlements::surfaceSettlementsOn(source.id, d.radiusM);
        if (!layouts.empty())
        {
            auto settled = std::make_shared<SettledSurfaceField>(out.field, layouts);
            std::fprintf(stderr, "[Settlements] %s: %zu surface settlement(s), %zu flattened footprints\n", source.id.c_str(),
                         layouts.size(), settled->discCount());
            out.field = std::move(settled);
        }
    }
    return out.field != nullptr;
}
} // namespace world::celestial::surface
