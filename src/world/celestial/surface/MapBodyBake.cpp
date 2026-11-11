#include "src/world/celestial/surface/MapBodyBake.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include <nlohmann/json.hpp>

#include "src/core/ThreadPriority.h"
#include "src/world/celestial/surface/AppearancePassport.h"
#include "src/world/celestial/surface/BodySurfaceDescriptor.h"
#include "src/world/celestial/surface/BodySurfaceFieldLoader.h"
#include "src/world/celestial/surface/TextureBlockCompression.h"

namespace world::celestial::surface
{
namespace
{
namespace fs = std::filesystem;

// the folder holding assets/source/celestial (curated maps): the working directory or a parent
std::string findAssetRoot()
{
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    for (const fs::path& base : {cwd, cwd.parent_path(), cwd.parent_path().parent_path()})
        if (fs::is_directory(base / "assets/source/celestial", ec))
            return base.lexically_normal().generic_string();
    return {};
}

std::string findSystemsRoot()
{
    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    for (const char* rel : {"assets/data/galaxy_details/systems", "src/assets/data/galaxy_details/systems"})
        for (const fs::path& base : {cwd, cwd.parent_path(), cwd.parent_path().parent_path()})
            if (fs::is_directory(base / rel, ec))
                return (base / rel).lexically_normal().generic_string();
    return {};
}

struct Fnv
{
    std::uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, std::size_t n)
    {
        const auto* c = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i)
            h = (h ^ c[i]) * 1099511628211ull;
    }
    void str(const std::string& s)
    {
        bytes(s.data(), s.size());
        const char sep = '\x1f';
        bytes(&sep, 1);
    }
    template <class T>
    void pod(const T& v)
    {
        bytes(&v, sizeof v);
    }
};

void hashFile(Fnv& f, const std::string& path, bool contents)
{
    if (path.empty())
    {
        f.str("-");
        return;
    }
    std::error_code ec;
    f.str(fs::path(path).filename().generic_string());
    const auto size = fs::file_size(path, ec);
    f.pod(static_cast<std::uint64_t>(ec ? 0 : size));
    if (contents && !ec)
    {
        std::ifstream in(path, std::ios::binary);
        const std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        f.str(data);
    }
}

std::string fileNameFor(const std::string& bodyId)
{
    // body ids carry non-ASCII names ("system_0.Sol.Юпитер"): keep [A-Za-z0-9._-] and add the id's hash
    std::string out;
    for (const unsigned char c : bodyId)
        out += (std::isalnum(c) && c < 128) || c == '.' || c == '_' || c == '-' ? static_cast<char>(c) : '_';
    if (out.size() > 80)
        out.resize(80);
    Fnv f;
    f.str(bodyId);
    char hex[17];
    std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(f.h));
    return out + "-" + std::string(hex, 8);
}

BodySurfaceSource sourceFor(MapBodyCatalog& catalog, int systemId, const std::string& bodyId,
                            const std::string& descriptorPath, double radiusM)
{
    BodySurfaceSource source;
    source.id = bodyId;
    source.descriptorPath = descriptorPath;
    source.radiusM = radiusM;
    if (!catalog.assetRoot().empty())
        findImportedBodyMaps(catalog.assetRoot(), catalog.systemFolder(systemId), descriptorPath, source);
    return source;
}

constexpr std::uint32_t FileMagic = 0x424D4545;   // "EEMB"

void computeProbe(const SurfaceField& field, int width, MapBodyProbe& out)
{
    const double pi = 3.14159265358979323846;
    const double band = 2.0 * (2.0 * pi * field.referenceRadiusM() / width);
    const double golden = pi * (3.0 - std::sqrt(5.0));
    for (int i = 0; i < MapProbeCount; ++i)
    {
        const double y = 1.0 - 2.0 * (i + 0.5) / MapProbeCount;
        const double r = std::sqrt(std::max(0.0, 1.0 - y * y));
        const glm::dvec3 dir(r * std::cos(golden * i), y, r * std::sin(golden * i));
        const SurfaceSample s = field.sample(dir, band);
        const glm::vec3 c = glm::pow(glm::clamp(field.appearance(s, dir, 0.0).albedo, glm::vec3(0.0f), glm::vec3(1.0f)),
                                     glm::vec3(1.0f / 2.2f));
        for (int k = 0; k < 3; ++k)
            out[static_cast<std::size_t>(i * 3 + k)] = static_cast<std::uint8_t>(c[k] * 255.0f + 0.5f);
    }
}
} // namespace

bool mapBodyProbe(MapBodyCatalog& catalog, int systemId, const std::string& bodyId, int width, MapBodyProbe& out,
                  std::string& error)
{
    error.clear();
    const std::string path = catalog.descriptorPath(systemId, bodyId);
    BodySurfaceDescriptor body;
    if (path.empty() || !loadGeneratedBody(path, body, &error))
        return false;
    LoadedBodySurface loaded;
    if (!acquireBodySurface(sourceFor(catalog, systemId, bodyId, path, body.radiusM), loaded) || !loaded.field)
    {
        error = "surface not loaded " + loaded.error;
        return false;
    }
    computeProbe(*loaded.field, width, out);
    return true;
}

bool mapBodyProbesMatch(const MapBodyProbe& a, const MapBodyProbe& b)
{
    for (std::size_t i = 0; i < a.size(); ++i)
        if (std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i])) > 2)
            return false;
    return true;
}

// ---------------------------------------------------------------- catalog

MapBodyCatalog::MapBodyCatalog()
    : m_assetRoot(findAssetRoot())
{
}

void MapBodyCatalog::listSystems()
{
    if (m_listed)
        return;
    m_listed = true;
    const std::string root = findSystemsRoot();
    if (root.empty())
        return;
    std::error_code ec;
    // the local systems and the distant quest / route targets (galaxy_details/distant_systems)
    for (const fs::path& dirRoot : {fs::path(root), fs::path(root).parent_path() / "distant_systems"})
        for (const auto& dir : fs::directory_iterator(dirRoot, ec))
        {
            const std::string name = dir.path().filename().string();
            const std::size_t us = name.find('_');
            if (us == std::string::npos || us == 0 || name.find_first_not_of("0123456789") != us)
                continue;
            const int id = std::atoi(name.substr(0, us).c_str());
            if (!m_systemDirs.count(id))   // a local system wins over a distant one of the same id
                m_systemDirs[id] = dir.path().generic_string();
        }
}

const MapBodyCatalog::System& MapBodyCatalog::system(int systemId)
{
    if (const auto it = m_systems.find(systemId); it != m_systems.end())
        return it->second;
    listSystems();
    System sys;
    if (const auto d = m_systemDirs.find(systemId); d != m_systemDirs.end())
    {
        const fs::path dir(d->second);
        const std::string name = dir.filename().string();
        sys.folder = name.substr(name.find('_') + 1);   // "000_sol" -> "sol"
        std::error_code ec;
        for (const auto& f : fs::directory_iterator(dir / "generated" / "bodies", ec))
        {
            if (f.path().extension() != ".json")
                continue;
            try
            {
                std::ifstream in(f.path());
                const nlohmann::json j = nlohmann::json::parse(in);
                if (j.value("schema", std::string()) == "elite.generated_body.v1")
                    sys.bodies[j.value("id", std::string())] = f.path().generic_string();
            }
            catch (const std::exception&)
            {
            }
        }
    }
    return m_systems.emplace(systemId, std::move(sys)).first->second;
}

std::string MapBodyCatalog::descriptorPath(int systemId, const std::string& bodyId)
{
    std::lock_guard lock(m_mutex);
    const System& sys = system(systemId);
    const auto it = sys.bodies.find(bodyId);
    return it == sys.bodies.end() ? std::string() : it->second;
}

std::string MapBodyCatalog::systemFolder(int systemId)
{
    std::lock_guard lock(m_mutex);
    return system(systemId).folder;
}

std::vector<int> MapBodyCatalog::systemIds()
{
    std::lock_guard lock(m_mutex);
    listSystems();
    std::vector<int> out;
    for (const auto& [id, dir] : m_systemDirs)
        out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<std::string> MapBodyCatalog::bodyIds(int systemId)
{
    std::lock_guard lock(m_mutex);
    std::vector<std::string> out;
    for (const auto& [id, path] : system(systemId).bodies)
        out.push_back(id);
    std::sort(out.begin(), out.end());
    return out;
}

int mapBodySystemId(const std::string& bodyId)
{
    if (bodyId.rfind("system_", 0) != 0)
        return -1;
    std::size_t i = 7;
    if (i >= bodyId.size() || !std::isdigit(static_cast<unsigned char>(bodyId[i])))
        return -1;
    int id = 0;
    for (; i < bodyId.size() && std::isdigit(static_cast<unsigned char>(bodyId[i])); ++i)
        id = id * 10 + (bodyId[i] - '0');
    return id;
}

// ---------------------------------------------------------------- fingerprint

std::uint64_t mapBodyFingerprint(MapBodyCatalog& catalog, int systemId, const std::string& bodyId, int width)
{
    const std::string path = catalog.descriptorPath(systemId, bodyId);
    if (path.empty())
        return 0;
    Fnv f;
    f.pod(MapBodyBakeVersion);
    f.pod(width);
    f.str(bodyId);
    hashFile(f, path, true);   // the descriptor itself (a few kB)
    if (const AppearancePassport* p = findAppearancePassport(bodyId))
    {
        f.pod(p->procedural);
        f.pod(p->hasMeanAlbedo);
        f.pod(p->meanAlbedo.x);
        f.pod(p->meanAlbedo.y);
        f.pod(p->meanAlbedo.z);
    }
    else
        f.str("no-passport");
    // the curated maps (names + sizes: they are large and change as whole files)
    const BodySurfaceSource source = sourceFor(catalog, systemId, bodyId, path, 0.0);
    hashFile(f, source.importedAlbedoPath, false);
    hashFile(f, source.importedHeightPath, false);
    f.pod(source.importedHeightRangeM);
    f.pod(source.importedHeightSigned);
    f.pod(source.importedWaterFromHeight);
    return f.h == 0 ? 1 : f.h;
}

// ---------------------------------------------------------------- bake

bool bakeMapBody(MapBodyCatalog& catalog, int systemId, const std::string& bodyId, int width, int threads,
                 const std::atomic<bool>* abort, MapBodyBake& out, std::string& error)
{
    error.clear();
    const auto aborted = [&] { return abort && abort->load(std::memory_order_relaxed); };
    const std::string path = catalog.descriptorPath(systemId, bodyId);
    BodySurfaceDescriptor body;
    if (path.empty())
        return false;   // stars, belts, stations: no generated body
    if (!loadGeneratedBody(path, body, &error))
        return false;
    // the flight renderer's surface (curated maps, passports) from the shared registry: no second copy
    // while the body is also in flight; released after the bake when nobody else holds it
    const BodySurfaceSource source = sourceFor(catalog, systemId, bodyId, path, body.radiusM);
    LoadedBodySurface loaded;
    if (!acquireBodySurface(source, loaded) || !loaded.field)
    {
        error = "surface not loaded " + loaded.error;
        return false;
    }
    const std::shared_ptr<SurfaceField> field = loaded.field;
    const int w = width, h = width / 2;
    const double pi = 3.14159265358979323846;
    const double texelM = 2.0 * pi * field->referenceRadiusM() / w;
    const auto dirAt = [&](int x, int row) {   // row 0 = north
        const double lon = ((x + 0.5) / w - 0.5) * 2.0 * pi;
        const double lat = (0.5 - (row + 0.5) / h) * pi;
        return glm::dvec3(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
    };
    std::vector<SurfaceSample> samples(static_cast<std::size_t>(w) * h);
    std::vector<std::uint8_t> rgba(samples.size() * 4);
    threads = std::max(1, threads);
    const auto parallelRows = [&](auto&& fn) {
        if (threads == 1)
        {
            for (int row = 0; row < h && !aborted(); ++row)
                fn(row);
            return;
        }
        std::vector<std::thread> pool;
        for (int t = 0; t < threads; ++t)
            pool.emplace_back([&, t] {
                core::lowerCurrentThreadPriority();
                for (int row = t; row < h && !aborted(); row += threads)
                    fn(row);
            });
        for (auto& th : pool)
            th.join();
    };
    parallelRows([&](int row) {
        for (int x = 0; x < w; ++x)
            samples[static_cast<std::size_t>(row) * w + x] = field->sample(dirAt(x, row), texelM * 2.0);
    });
    parallelRows([&](int row) {
        for (int x = 0; x < w; ++x)
        {
            const auto& s = samples[static_cast<std::size_t>(row) * w + x];
            const auto& east = samples[static_cast<std::size_t>(row) * w + (x + 1) % w];
            const auto& north = samples[static_cast<std::size_t>(std::max(0, row - 1)) * w + x];
            const double slope =
                std::min(1.0, std::hypot(east.heightM - s.heightM, north.heightM - s.heightM) / texelM);
            const glm::vec3 a =
                field->appearance(s, dirAt(x, row), 1.0 - 1.0 / std::sqrt(1.0 + slope * slope)).albedo;
            const glm::vec3 c = glm::pow(glm::clamp(a, glm::vec3(0.0f), glm::vec3(1.0f)), glm::vec3(1.0f / 2.2f));
            // GL row order: south first (as TextureLoader flips the cached TGA)
            const std::size_t o = (static_cast<std::size_t>(h - 1 - row) * w + x) * 4;
            rgba[o + 0] = static_cast<std::uint8_t>(c.r * 255.0f + 0.5f);
            rgba[o + 1] = static_cast<std::uint8_t>(c.g * 255.0f + 0.5f);
            rgba[o + 2] = static_cast<std::uint8_t>(c.b * 255.0f + 0.5f);
            rgba[o + 3] = 255;
        }
    });
    if (aborted())
    {
        error = "aborted";
        return false;
    }
    // relief normal map (the baker's buildNormalFromHeightImage on the height normalised to the body's
    // range, strength 8), at half the albedo size: it only shades the Hub map's parent body
    {
        double minH = 1.0e30, maxH = -1.0e30;
        for (const auto& s : samples)
        {
            minH = std::min(minH, s.heightM);
            maxH = std::max(maxH, s.heightM);
        }
        const double range = std::max(1.0, maxH - minH);
        const int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
        const auto h01 = [&](int x, int row) {   // full-size texel, row 0 = north, wrapped x, clamped y
            x = (x % w + w) % w;
            row = std::clamp(row, 0, h - 1);
            return static_cast<float>((samples[static_cast<std::size_t>(row) * w + x].heightM - minH) / range);
        };
        constexpr float Strength = 8.0f;
        std::vector<std::uint8_t> normal(static_cast<std::size_t>(nw) * nh * 4);
        for (int row = 0; row < nh; ++row)
            for (int x = 0; x < nw; ++x)
            {
                // the full-size gradient at the 2x2 group's centre (central differences 2 texels apart)
                const int fx = x * 2, fr = row * 2;
                const float dx = (h01(fx + 2, fr) + h01(fx + 2, fr + 1) - h01(fx - 1, fr) - h01(fx - 1, fr + 1)) / 3.0f;
                const float dy = (h01(fx, fr + 2) + h01(fx + 1, fr + 2) - h01(fx, fr - 1) - h01(fx + 1, fr - 1)) / 3.0f;
                const glm::vec3 n = glm::normalize(glm::vec3(-dx * Strength, -dy * Strength, 1.0f));
                const glm::vec3 enc = n * 0.5f + glm::vec3(0.5f);
                const std::size_t o = (static_cast<std::size_t>(nh - 1 - row) * nw + x) * 4;   // GL rows: south first
                normal[o + 0] = static_cast<std::uint8_t>(enc.x * 255.0f + 0.5f);
                normal[o + 1] = static_cast<std::uint8_t>(enc.y * 255.0f + 0.5f);
                normal[o + 2] = static_cast<std::uint8_t>(enc.z * 255.0f + 0.5f);
                normal[o + 3] = 255;
            }
        out.normalWidth = nw;
        out.normalHeight = nh;
        out.normalRgba = std::move(normal);
    }
    // a small body: its field as a displaced lat-long grid (the shape flight shows). 64 x 32: the Detail
    // map projects and sorts the shape on the CPU every frame (128 x 64 cost ~15 ms a frame, 2026-11-11)
    out.shapeGrid.clear();
    out.shapeLon = out.shapeLat = 0;
    if (field->solidSurface() && field->referenceRadiusM() < MapSmallBodyShapeRadiusM)
    {
        constexpr int NLon = 64, NLat = 32;
        const double radius = field->referenceRadiusM();
        out.shapeLon = NLon;
        out.shapeLat = NLat;
        out.shapeGrid.resize(static_cast<std::size_t>(NLon + 1) * NLat);
        for (int i = 0; i < NLat; ++i)
            for (int j = 0; j <= NLon; ++j)
            {
                const double lat = pi * 0.5 - pi * i / (NLat - 1);
                const double lon = (static_cast<double>(j) / NLon - 0.5) * 2.0 * pi;
                const glm::dvec3 dir(std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon));
                const double hgt = field->sample(dir, 2.0 * pi * radius / NLon).heightM;
                out.shapeGrid[static_cast<std::size_t>(i) * (NLon + 1) + j] =
                    glm::vec3(dir * std::max(radius * 0.2, radius + hgt));
            }
    }
    computeProbe(*field, w, out.probe);
    out.width = w;
    out.height = h;
    out.rgba = std::move(rgba);
    out.description = std::string(loaded.descriptor.source == SurfaceSourceKind::ImportedRealData ? "curated maps, " : "") +
                      archetypeName(surfaceArchetype(body));
    return true;
}

MapBodyPrebake compressMapBodyBake(const MapBodyBake& bake, std::uint64_t fingerprint)
{
    MapBodyPrebake p;
    p.fingerprint = fingerprint;
    p.probe = bake.probe;
    p.width = bake.width;
    p.height = bake.height;
    if (!bake.rgba.empty())
        p.albedoMips = encodeBC1MipChain(bake.rgba.data(), bake.width, bake.height);
    p.normalWidth = bake.normalWidth;
    p.normalHeight = bake.normalHeight;
    if (!bake.normalRgba.empty())
        p.normalMips = encodeBC1MipChain(bake.normalRgba.data(), bake.normalWidth, bake.normalHeight);
    p.shapeLon = bake.shapeLon;
    p.shapeLat = bake.shapeLat;
    p.shapeGrid = bake.shapeGrid;
    return p;
}

// ---------------------------------------------------------------- files

std::string mapBodyPrebakePath(const std::string& dir, int systemId, const std::string& bodyId)
{
    return (fs::path(dir) / std::to_string(systemId) / (fileNameFor(bodyId) + ".emb")).generic_string();
}

std::string mapBodyPrebakeDir(const std::string& assetRoot)
{
    if (assetRoot.empty())
        return {};
    return (fs::path(assetRoot) / "assets/baked/celestial/map").generic_string();
}

std::string mapBodyRuntimeCacheDir(const std::string& assetRoot)
{
    const auto writable = [](const fs::path& dir) {
        std::error_code ec;
        fs::create_directories(dir, ec);
        if (!fs::is_directory(dir, ec))
            return false;
        const fs::path probe = dir / ".write_probe";
        {
            std::ofstream f(probe, std::ios::binary);
            if (!f)
                return false;
            f << '1';
        }
        fs::remove(probe, ec);
        return true;
    };
    if (!assetRoot.empty())
    {
        const fs::path dir = fs::path(assetRoot) / "cache/celestial/map";
        if (writable(dir))
            return dir.generic_string();
    }
    // an install the user cannot write to (Program Files): the per-user cache
    if (const char* local = std::getenv("LOCALAPPDATA"); local && *local)
    {
        const fs::path dir = fs::path(local) / "Elite/cache/celestial/map";
        if (writable(dir))
            return dir.generic_string();
    }
    return {};
}

namespace
{
template <class T>
void put(std::ofstream& f, const T& v)
{
    f.write(reinterpret_cast<const char*>(&v), sizeof v);
}
template <class T>
bool get(std::ifstream& f, T& v)
{
    return static_cast<bool>(f.read(reinterpret_cast<char*>(&v), sizeof v));
}

void putMips(std::ofstream& f, const std::vector<std::vector<std::uint8_t>>& mips)
{
    put(f, static_cast<std::uint32_t>(mips.size()));
    for (const auto& m : mips)
    {
        put(f, static_cast<std::uint32_t>(m.size()));
        f.write(reinterpret_cast<const char*>(m.data()), static_cast<std::streamsize>(m.size()));
    }
}

bool getMips(std::ifstream& f, int width, int height, std::vector<std::vector<std::uint8_t>>& mips)
{
    std::uint32_t count = 0;
    if (!get(f, count) || count > 20)
        return false;
    mips.resize(count);
    int w = width, h = height;
    for (auto& m : mips)
    {
        std::uint32_t size = 0;
        if (!get(f, size) || size != bc1Size(w, h))
            return false;
        m.resize(size);
        if (!f.read(reinterpret_cast<char*>(m.data()), size))
            return false;
        w = std::max(1, w / 2);
        h = std::max(1, h / 2);
    }
    return true;
}
} // namespace

bool writeMapBodyPrebake(const std::string& path, const MapBodyPrebake& p)
{
    std::error_code ec;
    fs::create_directories(fs::path(path).parent_path(), ec);
    // write a temporary file and rename it: a reader never sees half a file
    const std::string tmp = path + ".tmp" + std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()));
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f)
            return false;
        put(f, FileMagic);
        put(f, MapBodyBakeVersion);
        put(f, p.fingerprint);
        f.write(reinterpret_cast<const char*>(p.probe.data()), static_cast<std::streamsize>(p.probe.size()));
        put(f, static_cast<std::int32_t>(p.width));
        put(f, static_cast<std::int32_t>(p.height));
        putMips(f, p.albedoMips);
        put(f, static_cast<std::int32_t>(p.normalWidth));
        put(f, static_cast<std::int32_t>(p.normalHeight));
        putMips(f, p.normalMips);
        put(f, static_cast<std::int32_t>(p.shapeLon));
        put(f, static_cast<std::int32_t>(p.shapeLat));
        put(f, static_cast<std::uint32_t>(p.shapeGrid.size()));
        f.write(reinterpret_cast<const char*>(p.shapeGrid.data()),
                static_cast<std::streamsize>(p.shapeGrid.size() * sizeof(glm::vec3)));
        put(f, FileMagic);   // trailer: a complete file
        if (!f)
            return false;
    }
    fs::rename(tmp, path, ec);
    if (ec)
    {
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

bool readMapBodyPrebake(const std::string& path, std::uint64_t expectedFingerprint, MapBodyPrebake& p)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;
    std::uint32_t magic = 0, version = 0;
    std::int32_t a = 0, b = 0;
    if (!get(f, magic) || magic != FileMagic || !get(f, version) || version != MapBodyBakeVersion ||
        !get(f, p.fingerprint) || p.fingerprint != expectedFingerprint ||
        !f.read(reinterpret_cast<char*>(p.probe.data()), static_cast<std::streamsize>(p.probe.size())))
        return false;
    if (!get(f, a) || !get(f, b) || a <= 0 || b <= 0 || a > 16384 || b > 16384)
        return false;
    p.width = a;
    p.height = b;
    if (!getMips(f, p.width, p.height, p.albedoMips) || p.albedoMips.empty())
        return false;
    if (!get(f, a) || !get(f, b) || a < 0 || b < 0 || a > 16384 || b > 16384)
        return false;
    p.normalWidth = a;
    p.normalHeight = b;
    if (!getMips(f, std::max(1, a), std::max(1, b), p.normalMips))
        return false;
    std::uint32_t shapeCount = 0;
    if (!get(f, a) || !get(f, b) || !get(f, shapeCount) ||
        shapeCount != static_cast<std::uint32_t>(a > 0 && b > 0 ? (a + 1) * b : 0))
        return false;
    p.shapeLon = a;
    p.shapeLat = b;
    p.shapeGrid.resize(shapeCount);
    if (shapeCount && !f.read(reinterpret_cast<char*>(p.shapeGrid.data()),
                              static_cast<std::streamsize>(shapeCount * sizeof(glm::vec3))))
        return false;
    return get(f, magic) && magic == FileMagic;
}
} // namespace world::celestial::surface
