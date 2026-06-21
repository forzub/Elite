#include "CelestialTextureBaker.h"

#include "CelestialVisualLibrary.h"
#include "CelestialVisualTypes.h"
#include "CelestialSourceCatalog.h"
#include "src/world/celestial/StarAtlasDatabase.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <utility>
#include <cctype>
#include "render/bitmap/stb_image.h"
#include <unordered_map>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

namespace world::celestial::visual
{

namespace
{

using json = nlohmann::json;

std::filesystem::path resolveProjectPath(
    const std::string& relative
);

json descriptorToJson(
    const CelestialBodyVisualDescriptor& d
);

struct ImageRgba8;

bool loadImageRgba8(
    const std::filesystem::path& path,
    ImageRgba8& out
);


bool wildcardMatchName(
    const std::string& name,
    const std::string& pattern
)
{
    const std::size_t star =
        pattern.find('*');

    if (star == std::string::npos)
        return name == pattern;

    const std::string prefix =
        pattern.substr(0, star);

    const std::string suffix =
        pattern.substr(star + 1);

    if (name.size() < prefix.size() + suffix.size())
        return false;

    return name.rfind(prefix, 0) == 0 &&
           name.substr(name.size() - suffix.size()) == suffix;
}

bool findAssetPath(
    const std::string& pattern,
    std::filesystem::path& outPath
)
{
    namespace fs = std::filesystem;

    if (pattern.find('*') == std::string::npos)
    {
        const fs::path resolved =
            resolveProjectPath(pattern);

        if (fs::exists(resolved))
        {
            outPath = resolved;
            return true;
        }

        return false;
    }

    const fs::path raw(pattern);

    const fs::path parent =
        resolveProjectPath(
            raw.parent_path().generic_string()
        );

    if (!fs::exists(parent))
        return false;

    const std::string filenamePattern =
        raw.filename().generic_string();

    for (const auto& entry : fs::directory_iterator(parent))
    {
        if (!entry.is_regular_file())
            continue;

        const std::string filename =
            entry.path().filename().generic_string();

        if (wildcardMatchName(filename, filenamePattern))
        {
            outPath = entry.path();
            return true;
        }
    }

    return false;
}

bool findRequiredAsset(
    const std::string& pattern
)
{
    std::filesystem::path found;
    return findAssetPath(pattern, found);
}





bool auditAssetGroup(
    const json& root,
    const std::string& groupName,
    bool required
)
{
    if (!root.contains(groupName))
        return true;

    if (!root[groupName].is_object())
    {
        std::cout
            << "[CelestialTextureBaker] source catalog field is not object: "
            << groupName
            << "\n";

        return !required;
    }

    bool ok = true;

    for (auto it = root[groupName].begin();
         it != root[groupName].end();
         ++it)
    {
        const std::string key =
            it.key();

        const std::string pattern =
            it.value().get<std::string>();

        const bool found =
            findRequiredAsset(pattern);

        if (found)
        {
            std::cout
                << "  OK "
                << (required ? "REQUIRED " : "OPTIONAL ")
                << key
                << " -> "
                << pattern
                << "\n";
        }
        else
        {
            std::cout
                << "  MISSING "
                << (required ? "REQUIRED " : "OPTIONAL ")
                << key
                << " -> "
                << pattern
                << "\n";
        }

        if (required)
            ok = ok && found;
    }

    return ok;
}






bool auditImportedSourceCatalog(
    const CelestialBodyVisualDescriptor& body
)
{
    if (body.sourceCatalogPath.empty())
    {
        std::cout
            << "[CelestialTextureBaker] MISSING REQUIRED source catalog: "
            << body.systemFolderName << "/"
            << body.bodyFolderName << "\n";

        return false;
    }

    const auto catalogPath =
        resolveProjectPath(body.sourceCatalogPath);

    std::ifstream f(catalogPath);
    if (!f.is_open())
    {
        std::cout
            << "[CelestialTextureBaker] MISSING REQUIRED source catalog file: "
            << body.sourceCatalogPath << "\n";

        return false;
    }

    json root;
    f >> root;

    std::cout
        << "[CelestialTextureBaker] source audit "
        << body.systemFolderName << "/"
        << body.bodyFolderName << "\n";

    const bool requiredOk =
        auditAssetGroup(
            root,
            "required_assets",
            true
        );

    const bool optionalOk =
        auditAssetGroup(
            root,
            "optional_assets",
            false
        );

    if (requiredOk)
    {
        std::cout
            << "  RESULT source audit OK: required assets present or not declared\n";
    }
    else
    {
        std::cout
            << "  RESULT source audit FAILED: missing required assets\n";
    }

    if (!optionalOk)
    {
        // Currently unreachable because optional group never fails the audit.
        // Kept for future policy changes.
    }

    return requiredOk;
}






std::string normalizeFilterToken(std::string s)
{
    for (char& c : s)
    {
        if (c == '\\')
            c = '/';

        if (c == ' ' || c == '-')
            c = '_';

        c = static_cast<char>(
            std::tolower(static_cast<unsigned char>(c))
        );
    }

    while (!s.empty() && s.front() == '/')
        s.erase(s.begin());

    while (!s.empty() && s.back() == '/')
        s.pop_back();

    return s;
}

bool equalsFilterToken(
    const std::string& a,
    const std::string& b
)
{
    return normalizeFilterToken(a) ==
           normalizeFilterToken(b);
}

bool bodyMatchesFilter(
    const CelestialBodyVisualDescriptor& body,
    const CelestialTextureBakeOptions& options
)
{
    std::string systemFilter =
        normalizeFilterToken(options.systemFilter);

    std::string bodyFilter =
        normalizeFilterToken(options.bodyFilter);

    // Support:
    //   --body sol/earth
    // even when --system is not provided.
    if (systemFilter.empty() &&
        !bodyFilter.empty())
    {
        const std::size_t slash =
            bodyFilter.find('/');

        if (slash != std::string::npos)
        {
            systemFilter = bodyFilter.substr(0, slash);
            bodyFilter = bodyFilter.substr(slash + 1);
        }
    }

    const std::string bodySystem =
        normalizeFilterToken(body.systemFolderName);

    const std::string bodyFolder =
        normalizeFilterToken(body.bodyFolderName);

    const std::string bodyDisplay =
        normalizeFilterToken(body.displayName);

    const std::string bodySource =
        normalizeFilterToken(body.sourceBodyId);

    if (!systemFilter.empty() &&
        bodySystem != systemFilter)
    {
        return false;
    }

    if (!bodyFilter.empty())
    {
        const bool bodyOk =
            bodyFolder == bodyFilter ||
            bodyDisplay == bodyFilter ||
            (bodySystem + "/" + bodyFolder) == bodyFilter;

        if (!bodyOk)
            return false;
    }

    return true;
}

void printBakeBodyLine(
    const CelestialBodyVisualDescriptor& body,
    const char* prefix
)
{
    std::cout
        << prefix
        << " "
        << body.systemFolderName
        << "/"
        << body.bodyFolderName
        << "  class="
        << toString(body.visualClass)
        << "  source="
        << toString(body.surfaceSourceKind)
        << "  name=\""
        << body.displayName
        << "\""
        << "\n";
}











struct ImageRgba8
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;
};

struct FloatMap
{
    int width = 0;
    int height = 0;
    std::vector<float> values;
};

struct GeneratedMaps
{
    FloatMap height;

    // Generic procedural masks.
    //
    // For terrestrial / ocean / desert:
    //   mask0 = water
    //   mask1 = relief / mountains
    //   mask2 = ice
    //   mask3 = coastline / surface feature accent
    //
    // For rocky / icy bodies:
    //   mask0 = large craters / ice
    //   mask1 = ridges / fractures
    //   mask2 = small craters / polar
    //   mask3 = albedo feature accent
    //
    // For gas / ice giants:
    //   mask0 = bands
    //   mask1 = turbulence
    //   mask2 = storms
    //   mask3 = fine band accent
    FloatMap mask0;
    FloatMap mask1;
    FloatMap mask2;
    FloatMap mask3;
};


struct LatLonPoint
{
    double latDeg = 0.0;
    double lonDeg = 0.0;
};

struct EarthCoastlinePolygon
{
    std::string name;
    std::vector<LatLonPoint> points;
};

std::filesystem::path resolveExistingProjectPath(
    const std::string& relative
)
{
    namespace fs = std::filesystem;

    const fs::path cwd = fs::current_path();

    const std::vector<fs::path> candidates =
    {
        cwd / relative,
        cwd.parent_path() / relative,
        cwd.parent_path().parent_path() / relative,
        fs::path("D:/__elite/work") / relative
    };

    for (const auto& p : candidates)
    {
        if (fs::exists(p))
            return p;
    }

    return fs::path(relative);
}

std::vector<EarthCoastlinePolygon> loadEarthCoastlinePolygons(
    const std::string& relativePath
)
{
    std::vector<EarthCoastlinePolygon> out;

    const auto path =
        resolveExistingProjectPath(relativePath);

    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr
            << "[CelestialTextureBaker] Cannot open coastline file: "
            << path.string()
            << "\n";

        return out;
    }

    json root;
    f >> root;

    if (!root.contains("polylines") ||
        !root["polylines"].is_array())
    {
        return out;
    }

    for (const auto& item : root["polylines"])
    {
        if (!item.is_object())
            continue;

        EarthCoastlinePolygon poly;
        poly.name = item.value("name", "");

        if (!item.contains("points") ||
            !item["points"].is_array())
        {
            continue;
        }

        for (const auto& p : item["points"])
        {
            if (!p.is_array() || p.size() < 2)
                continue;

            LatLonPoint pp;
            pp.latDeg = p[0].get<double>();
            pp.lonDeg = p[1].get<double>();

            poly.points.push_back(pp);
        }

        if (poly.points.size() >= 3)
            out.push_back(std::move(poly));
    }

    return out;
}

bool pointInPolygonLonLat(
    double lon,
    double lat,
    const EarthCoastlinePolygon& poly
)
{
    bool inside = false;

    const std::size_t n =
        poly.points.size();

    if (n < 3)
        return false;

    for (std::size_t i = 0, j = n - 1; i < n; j = i++)
    {
        const double xi = poly.points[i].lonDeg;
        const double yi = poly.points[i].latDeg;

        const double xj = poly.points[j].lonDeg;
        const double yj = poly.points[j].latDeg;

        const bool intersect =
            ((yi > lat) != (yj > lat)) &&
            (lon < (xj - xi) * (lat - yi) / ((yj - yi) + 0.0000001) + xi);

        if (intersect)
            inside = !inside;
    }

    return inside;
}

float earthLandMaskFromCoastlines(
    double lonDeg,
    double latDeg,
    const std::vector<EarthCoastlinePolygon>& polygons
)
{
    // This coastline file is simplified. We treat every closed polyline
    // as a land polygon. It will not be GIS-perfect, but it is already
    // infinitely better than random fake continents.
    for (const auto& poly : polygons)
    {
        if (pointInPolygonLonLat(lonDeg, latDeg, poly))
            return 1.0f;
    }

    return 0.0f;
}




std::filesystem::path resolveProjectPath(const std::string& relative)
{
    namespace fs = std::filesystem;

    const fs::path cwd = fs::current_path();

    const std::vector<fs::path> candidates =
    {
        cwd / relative,
        cwd.parent_path() / relative,
        cwd.parent_path().parent_path() / relative,
        fs::path("D:/__elite/work") / relative
    };

    for (const auto& p : candidates)
    {
        if (fs::exists(p))
            return p;
    }

    // For output paths the folder may not exist yet.
    return cwd.parent_path() / relative;
}




std::string toAssetPath(
    const std::filesystem::path& path
)
{
    namespace fs = std::filesystem;

    const fs::path p =
        path.lexically_normal();

    const fs::path cwd =
        fs::current_path().lexically_normal();

    const std::vector<fs::path> roots =
    {
        cwd,
        cwd.parent_path(),
        cwd.parent_path().parent_path(),
        fs::path("D:/__elite/work")
    };

    for (const fs::path& rootRaw : roots)
    {
        const fs::path root =
            rootRaw.lexically_normal();

        std::error_code ec;

        const fs::path rel =
            fs::relative(p, root, ec);

        if (ec)
            continue;

        if (rel.empty())
            continue;

        const std::string relText =
            rel.generic_string();

        if (relText.rfind("..", 0) == 0)
            continue;

        return relText;
    }

    return p.generic_string();
}








constexpr const char* kStarAtlasSystemsPath =
    "src/assets/data/star_atlas/star_systems.json";

constexpr const char* kStarAtlasSystemDetailsPath =
    "src/assets/data/star_atlas/system_details.json";

std::string normalizeIdentityToken(
    const std::string& s
)
{
    std::string out;
    out.reserve(s.size());

    for (unsigned char c : s)
    {
        if (c < 128)
        {
            if (std::isalnum(c))
            {
                out.push_back(
                    static_cast<char>(
                        std::tolower(c)
                    )
                );
            }

            continue;
        }

        // Preserve UTF-8 bytes for exact non-ASCII matching.
        out.push_back(static_cast<char>(c));
    }

    return out;
}

void addIdentityToken(
    std::vector<std::string>& out,
    const std::string& raw
)
{
    const std::string token =
        normalizeIdentityToken(raw);

    if (token.empty())
        return;

    if (std::find(out.begin(), out.end(), token) == out.end())
        out.push_back(token);
}

std::vector<std::string> splitIdentityPath(
    const std::string& s
)
{
    std::vector<std::string> out;
    std::string current;

    for (char c : s)
    {
        if (c == '.' || c == '/' || c == '\\')
        {
            if (!current.empty())
            {
                out.push_back(current);
                current.clear();
            }

            continue;
        }

        current.push_back(c);
    }

    if (!current.empty())
        out.push_back(current);

    return out;
}






std::string lastIdentityPathPart(
    const std::string& s
)
{
    const std::vector<std::string> parts =
        splitIdentityPath(s);

    if (parts.empty())
        return "";

    return parts.back();
}














const std::unordered_map<std::string, std::vector<std::string>>&
knownSolBodyAliases()
{
    static const std::unordered_map<std::string, std::vector<std::string>> aliases =
    {
        { "mercury",   { "Mercury", "Меркурий" } },
        { "venus",     { "Venus", "Венера" } },
        { "earth",     { "Earth", "Terra", "Gaia", "Земля" } },
        { "moon",      { "Moon", "Luna", "Луна" } },
        { "mars",      { "Mars", "Ares", "Марс" } },
        { "phobos",    { "Phobos", "Фобос" } },
        { "deimos",    { "Deimos", "Деймос" } },

        { "jupiter",   { "Jupiter", "Zeus", "Юпитер" } },
        { "ganymede",  { "Ganymede", "Ганимед" } },
        { "callisto",  { "Callisto", "Каллисто" } },
        { "io",        { "Io", "Ио" } },
        { "europa",    { "Europa", "Европа" } },
        { "amalthea",  { "Amalthea", "Амальтея" } },
        { "himalia",   { "Himalia", "Гималия" } },
        { "elara",     { "Elara", "Элара" } },
        { "pasiphae",  { "Pasiphae", "Пасифе" } },

        { "saturn",    { "Saturn", "Cronus", "Сатурн" } },
        { "titan",     { "Titan", "Титан" } },
        { "rhea",      { "Rhea", "Рея" } },
        { "iapetus",   { "Iapetus", "Япет" } },
        { "dione",     { "Dione", "Диона" } },
        { "tethys",    { "Tethys", "Тетис" } },
        { "enceladus", { "Enceladus", "Энцелад" } },
        { "mimas",     { "Mimas", "Мимас" } },
        { "phoebe",    { "Phoebe", "Феба" } },

        { "uranus",    { "Uranus", "Ouranos", "Уран" } },
        { "titania",   { "Titania", "Титания" } },
        { "oberon",    { "Oberon", "Оберон" } },
        { "umbriel",   { "Umbriel", "Умбриэль" } },
        { "ariel",     { "Ariel", "Ариэль" } },
        { "miranda",   { "Miranda", "Миранда" } },

        { "neptune",   { "Neptune", "Poseidon", "Нептун" } },
        { "triton",    { "Triton", "Тритон" } },
        { "proteus",   { "Proteus", "Протей" } },
        { "nereid",    { "Nereid", "Нереида" } },
        { "larissa",   { "Larissa", "Ларисса" } }
    };

    return aliases;
}

int parseSourceSystemId(
    const CelestialBodyVisualDescriptor& desc
)
{
    if (!desc.sourceSystemId.empty())
    {
        try
        {
            return std::stoi(desc.sourceSystemId);
        }
        catch (...)
        {
        }
    }

    if (desc.sourceBodyId.rfind("system_", 0) == 0)
    {
        const std::size_t begin =
            std::string("system_").size();

        std::size_t end =
            desc.sourceBodyId.find('.', begin);

        if (end == std::string::npos)
            end = desc.sourceBodyId.size();

        try
        {
            return std::stoi(
                desc.sourceBodyId.substr(
                    begin,
                    end - begin
                )
            );
        }
        catch (...)
        {
        }
    }

    // Current project default: Sol.
    if (desc.systemFolderName == "sol")
        return 0;

    return -1;
}






std::vector<std::string> descriptorPhysicalMatchTokens(
    const CelestialBodyVisualDescriptor& desc
)
{
    std::vector<std::string> out;

    // Direct visual identity.
    addIdentityToken(out, desc.bodyFolderName);
    addIdentityToken(out, desc.displayName);

    // sourceBodyId can look like:
    //   system_0.Sol.Earth
    //
    // Do NOT add all parts, because "Sol" would match the star
    // and every body would incorrectly receive the Sun radius.
    //
    // Only the last part is the actual body identity.
    addIdentityToken(
        out,
        lastIdentityPathPart(desc.sourceBodyId)
    );

    const std::string folderKey =
        normalizeIdentityToken(desc.bodyFolderName);

    const auto& aliases =
        knownSolBodyAliases();

    auto it =
        aliases.find(folderKey);

    if (it != aliases.end())
    {
        for (const std::string& alias : it->second)
            addIdentityToken(out, alias);
    }

    return out;
}





















std::vector<std::string> physicalBodyMatchTokens(
    const world::celestial::CelestialBodyDefinition& body
)
{
    std::vector<std::string> out;

    addIdentityToken(out, body.id);
    addIdentityToken(out, body.name);

    for (const auto& alt : body.alternativeNames)
        addIdentityToken(out, alt.name);

    for (const std::string& part : splitIdentityPath(body.id))
        addIdentityToken(out, part);

    return out;
}

bool anyTokenMatches(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b
)
{
    for (const std::string& x : a)
    {
        for (const std::string& y : b)
        {
            if (x == y)
                return true;
        }
    }

    return false;
}

const world::celestial::CelestialBodyDefinition* findPhysicalBodyForDescriptor(
    const CelestialBodyVisualDescriptor& desc,
    const world::celestial::StarAtlasDatabase& atlas
)
{
    const int systemId =
        parseSourceSystemId(desc);

    if (systemId < 0)
        return nullptr;

    const auto* system =
        atlas.findSystem(systemId);

    if (!system)
        return nullptr;

    const std::vector<std::string> descriptorTokens =
        descriptorPhysicalMatchTokens(desc);


        

    for (const auto& body : system->bodies)
    {
        if (body.type == world::celestial::BodyType::AsteroidBelt)
            continue;

        if (body.type == world::celestial::BodyType::Star)
            continue;

        const std::vector<std::string> bodyTokens =
            physicalBodyMatchTokens(body);

        if (anyTokenMatches(descriptorTokens, bodyTokens))
            return &body;
    }






    return nullptr;
}

bool enrichDescriptorFromPhysicalAtlas(
    CelestialBodyVisualDescriptor& desc,
    const world::celestial::StarAtlasDatabase& atlas
)
{
    const auto* body =
        findPhysicalBodyForDescriptor(desc, atlas);

    if (!body)
        return false;

    if (body->radiusKm <= 0.0)
        return false;

    // This is a derived in-memory value.
    // Source of truth remains system_details.json / diameter_km.
    desc.referenceRadiusKm = body->radiusKm;

    std::cout
        << "[CelestialTextureBaker] physical radius "
        << desc.systemFolderName
        << "/"
        << desc.bodyFolderName
        << " radius_km="
        << body->radiusKm
        << " source=star_atlas/system_details.json"
        << "\n";

    return true;
}










float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

float smoothstep01(float t)
{
    t = clamp01(t);
    return t * t * (3.0f - 2.0f * t);
}


float contrast01(float v, float contrast)
{
    return clamp01((v - 0.5f) * contrast + 0.5f);
}

float bandAroundHalf(float v, float width)
{
    const float d = std::abs(v - 0.5f);
    return 1.0f - smoothstep01(d / std::max(0.0001f, width));
}



glm::vec3 clampColor(const glm::vec3& c)
{
    return glm::vec3(
        clamp01(c.r),
        clamp01(c.g),
        clamp01(c.b)
    );
}

std::uint32_t hash32(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

float hash01(int x, int y, std::uint32_t seed)
{
    std::uint32_t v = seed;
    v ^= hash32(static_cast<std::uint32_t>(x) + 0x9e3779b9u);
    v ^= hash32(static_cast<std::uint32_t>(y) + 0x85ebca6bu);
    v = hash32(v);
    return static_cast<float>(v & 0x00ffffffu) / static_cast<float>(0x00ffffffu);
}

int positiveMod(int x, int m)
{
    int r = x % m;
    return r < 0 ? r + m : r;
}

float valueNoiseWrapped(
    float x,
    float y,
    int wrapX,
    int wrapY,
    std::uint32_t seed
)
{
    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));
    const int x1 = x0 + 1;
    const int y1 = y0 + 1;

    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);

    const int wx0 = positiveMod(x0, wrapX);
    const int wy0 = positiveMod(y0, wrapY);
    const int wx1 = positiveMod(x1, wrapX);
    const int wy1 = positiveMod(y1, wrapY);

    const float v00 = hash01(wx0, wy0, seed);
    const float v10 = hash01(wx1, wy0, seed);
    const float v01 = hash01(wx0, wy1, seed);
    const float v11 = hash01(wx1, wy1, seed);

    const float sx = smoothstep01(tx);
    const float sy = smoothstep01(ty);

    const float a = lerp(v00, v10, sx);
    const float b = lerp(v01, v11, sx);

    return lerp(a, b, sy);
}

float fbmWrapped(
    float x,
    float y,
    int wrapX,
    int wrapY,
    std::uint32_t seed,
    int octaves,
    float lacunarity,
    float gain
)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    float norm = 0.0f;

    for (int i = 0; i < octaves; ++i)
    {
        sum += amp * valueNoiseWrapped(
            x * freq,
            y * freq,
            std::max(1, static_cast<int>(wrapX * freq)),
            std::max(1, static_cast<int>(wrapY * freq)),
            seed + static_cast<std::uint32_t>(i * 101)
        );

        norm += amp;
        amp *= gain;
        freq *= lacunarity;
    }

    if (norm <= 0.0f)
        return 0.0f;

    return sum / norm;
}




float ridgedFbmWrapped(
    float x,
    float y,
    int wrapX,
    int wrapY,
    std::uint32_t seed,
    int octaves,
    float lacunarity,
    float gain
)
{
    const float n =
        fbmWrapped(x, y, wrapX, wrapY, seed, octaves, lacunarity, gain);

    return 1.0f - std::abs(2.0f * n - 1.0f);
}

glm::vec2 computeWarpedUv(
    float u,
    float v,
    const SurfaceGenerationParams& surface,
    int wrapX,
    int wrapY
)
{
    if (surface.continentWarp <= 0.0001f)
        return glm::vec2(u, v);

    const float warpX =
        fbmWrapped(
            u * 3.0f,
            v * 1.5f,
            wrapX,
            wrapY,
            surface.seed + 301u,
            4,
            2.0f,
            0.5f
        ) - 0.5f;

    const float warpY =
        fbmWrapped(
            u * 3.0f,
            v * 1.5f,
            wrapX,
            wrapY,
            surface.seed + 337u,
            4,
            2.0f,
            0.5f
        ) - 0.5f;

    return glm::vec2(
        u + warpX * surface.continentWarp,
        clamp01(v + warpY * surface.continentWarp * 0.5f)
    );
}

std::pair<int, int> resolveTextureSize(
    const CelestialBodyVisualDescriptor& desc
)
{
    int w = std::max(2, desc.textureProfile.width);
    int h = std::max(2, desc.textureProfile.height);

    if (desc.textureProfile.autoFromRadius &&
        desc.referenceRadiusKm > 0.0)
    {
        constexpr double kPi = 3.14159265358979323846;

        const double circumferenceMeters =
            2.0 * kPi * desc.referenceRadiusKm * 1000.0;

        const double targetMetersPerTexel =
            std::max(1.0, desc.textureProfile.targetMetersPerTexel);

        const int autoWidth =
            static_cast<int>(std::ceil(circumferenceMeters / targetMetersPerTexel));

        const int autoHeight =
            std::max(1, autoWidth / 2);

        w = std::max(w, autoWidth);
        h = std::max(h, autoHeight);
    }

    w = std::min(w, desc.textureProfile.maxWidth);
    h = std::min(h, desc.textureProfile.maxHeight);

    if (w < 2) w = 2048;
    if (h < 2) h = std::max(1, w / 2);

    return { w, h };
}

















float latitudeAbsFromV(float v)
{
    const float lat = (0.5f - v) * 180.0f;
    return std::abs(lat);
}

glm::vec3 applySubtleTint(
    float grayscale,
    const CelestialColorStyle& style
)
{
    grayscale = clamp01(grayscale);

    glm::vec3 gray(grayscale);
    glm::vec3 tinted = gray * style.tint;

    glm::vec3 out =
        gray * (1.0f - style.saturation) +
        tinted * style.saturation;

    out = (out - glm::vec3(0.5f)) * style.contrast + glm::vec3(0.5f);
    out *= style.brightness;

    return clampColor(out);
}

FloatMap makeFloatMap(int width, int height, float initial = 0.0f)
{
    FloatMap m;
    m.width = width;
    m.height = height;
    m.values.assign(static_cast<std::size_t>(width * height), initial);
    return m;
}

ImageRgba8 makeImage(int width, int height)
{
    ImageRgba8 img;
    img.width = width;
    img.height = height;
    img.pixels.assign(static_cast<std::size_t>(width * height * 4), 0);
    return img;
}

void setPixel(
    ImageRgba8& img,
    int x,
    int y,
    const glm::vec4& c
)
{
    const std::size_t i =
        static_cast<std::size_t>((y * img.width + x) * 4);

    img.pixels[i + 0] = static_cast<std::uint8_t>(clamp01(c.r) * 255.0f);
    img.pixels[i + 1] = static_cast<std::uint8_t>(clamp01(c.g) * 255.0f);
    img.pixels[i + 2] = static_cast<std::uint8_t>(clamp01(c.b) * 255.0f);
    img.pixels[i + 3] = static_cast<std::uint8_t>(clamp01(c.a) * 255.0f);
}

bool writeTgaRgba(
    const std::filesystem::path& path,
    const ImageRgba8& img
)
{
    std::ofstream f(path, std::ios::binary);
    if (!f.is_open())
        return false;

    std::uint8_t header[18] = {};
    header[2] = 2; // uncompressed true-color image
    header[12] = static_cast<std::uint8_t>(img.width & 0xff);
    header[13] = static_cast<std::uint8_t>((img.width >> 8) & 0xff);
    header[14] = static_cast<std::uint8_t>(img.height & 0xff);
    header[15] = static_cast<std::uint8_t>((img.height >> 8) & 0xff);
    header[16] = 32; // bpp
    header[17] = 0x20; // top-left origin

    f.write(reinterpret_cast<const char*>(header), 18);

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            const std::size_t i =
                static_cast<std::size_t>((y * img.width + x) * 4);

            const std::uint8_t r = img.pixels[i + 0];
            const std::uint8_t g = img.pixels[i + 1];
            const std::uint8_t b = img.pixels[i + 2];
            const std::uint8_t a = img.pixels[i + 3];

            const std::uint8_t bgra[4] = { b, g, r, a };
            f.write(reinterpret_cast<const char*>(bgra), 4);
        }
    }

    return true;
}

glm::vec3 sampleEquirect(
    const ImageRgba8& img,
    float u,
    float v
)
{
    while (u < 0.0f) u += 1.0f;
    while (u >= 1.0f) u -= 1.0f;

    v = clamp01(v);

    const int x = std::clamp(
        static_cast<int>(u * static_cast<float>(img.width - 1)),
        0,
        img.width - 1
    );

    const int y = std::clamp(
        static_cast<int>(v * static_cast<float>(img.height - 1)),
        0,
        img.height - 1
    );

    const std::size_t i =
        static_cast<std::size_t>((y * img.width + x) * 4);

    return glm::vec3(
        img.pixels[i + 0] / 255.0f,
        img.pixels[i + 1] / 255.0f,
        img.pixels[i + 2] / 255.0f
    );
}

void generateSurfaceMaps(
    const CelestialBodyVisualDescriptor& desc,
    GeneratedMaps& out
)
{
    const auto resolvedSize = resolveTextureSize(desc);
    const int w = resolvedSize.first;
    const int h = resolvedSize.second;

    out.height = makeFloatMap(w, h, 0.0f);
    out.mask0 = makeFloatMap(w, h, 0.0f);
    out.mask1 = makeFloatMap(w, h, 0.0f);
    out.mask2 = makeFloatMap(w, h, 0.0f);
    out.mask3 = makeFloatMap(w, h, 0.0f);

    const float macroScale = std::max(0.001f, desc.surface.macroNoiseScale);
    const float detailScale = std::max(0.001f, desc.surface.detailNoiseScale);




    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const std::size_t i =
                static_cast<std::size_t>(y * w + x);

            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);

            const float latAbs = latitudeAbsFromV(v);
            const float polarMask =
                smoothstep01((latAbs - 55.0f) / 35.0f);

            

                        const glm::vec2 warpedUv =
                computeWarpedUv(
                    u,
                    v,
                    desc.surface,
                    w,
                    h
                );

            const float uu = warpedUv.x;
            const float vv = warpedUv.y;

            const float n0 =
                fbmWrapped(
                    uu * 8.0f * macroScale,
                    vv * 4.0f * macroScale,
                    w, h,
                    desc.surface.seed + 11u,
                    5, 2.0f, 0.5f
                );

            const float n1 =
                fbmWrapped(
                    uu * 24.0f * detailScale,
                    vv * 12.0f * detailScale,
                    w, h,
                    desc.surface.seed + 37u,
                    5, 2.0f, 0.5f
                );

            const float n2 =
                fbmWrapped(
                    uu * 40.0f * detailScale,
                    vv * 20.0f * detailScale,
                    w, h,
                    desc.surface.seed + 83u,
                    4, 2.0f, 0.5f
                );

















            float height = 0.0f;
            float maskA = 0.0f;
            float maskB = 0.0f;
            float maskC = 0.0f;
            float maskD = 0.0f;

            

            switch (desc.visualClass)
            {
                

                case CelestialVisualClass::RockyBarren :
                case CelestialVisualClass::AsteroidRock:
                {
                    const float craterLargeDensity =
                        std::max(
                            desc.surface.craterLargeDensity,
                            desc.surface.craterDensity * 0.55f
                        );

                    const float craterSmallDensity =
                        std::max(
                            desc.surface.craterSmallDensity,
                            desc.surface.craterDensity
                        );

                    const float ridges =
                        ridgedFbmWrapped(
                            uu * 14.0f * macroScale,
                            vv * 7.0f * macroScale,
                            w, h,
                            desc.surface.seed + 121u,
                            5, 2.0f, 0.5f
                        );

                    const float craterLargeField =
                        fbmWrapped(
                            uu * 18.0f * detailScale,
                            vv * 9.0f * detailScale,
                            w, h,
                            desc.surface.seed + 101u,
                            4, 2.0f, 0.5f
                        );

                    const float craterSmallField =
                        fbmWrapped(
                            uu * 72.0f * detailScale,
                            vv * 36.0f * detailScale,
                            w, h,
                            desc.surface.seed + 111u,
                            4, 2.0f, 0.5f
                        );

                    const float craterLarge =
                        smoothstep01(
                            (craterLargeField - (1.0f - craterLargeDensity)) * 4.0f
                        );

                    const float craterSmall =
                        smoothstep01(
                            (craterSmallField - (1.0f - craterSmallDensity)) * 5.0f
                        );

                    height =
                        0.32f * n0 +
                        0.18f * n1 +
                        0.10f * n2 +
                        0.30f * ridges +
                        desc.surface.ridgeStrength * ridges * 0.18f;

                    height -= craterLarge * desc.surface.craterStrength * 0.26f;
                    height -= craterSmall * desc.surface.craterStrength * 0.12f;

                    maskA = craterLarge;
                    maskB = ridges;
                    maskC = craterSmall;
                    maskD = clamp01(ridges * 0.65f + craterSmall * 0.35f);
                    break;
                }



                case CelestialVisualClass::RockyIce:
                {
                    const float fractures =
                        ridgedFbmWrapped(
                            uu * 22.0f * detailScale,
                            vv * 11.0f * detailScale,
                            w, h,
                            desc.surface.seed + 131u,
                            5, 2.0f, 0.5f
                        );

                    height =
                        0.36f * n0 +
                        0.18f * n1 +
                        0.12f * n2 +
                        0.16f * fractures;

                    const float iceMask =
                        clamp01(std::max(desc.surface.iceCoverage, polarMask * 0.9f));

                    maskA = iceMask;
                    maskB = fractures;
                    maskC = polarMask;
                    maskD = clamp01(fractures * 0.85f + n2 * 0.15f);
                    break;
                }

                                case CelestialVisualClass::Terrestrial:
                case CelestialVisualClass::OceanWorld:
                case CelestialVisualClass::DesertWorld:
                {
                    const float continentFreq =
                        std::max(0.25f, desc.surface.continentFrequency);

                    // Very low frequency plate/continent field.
                    // This must define continents, not cloudy mush.
                    const float plate0 =
                        fbmWrapped(
                            uu * 1.35f * continentFreq,
                            vv * 0.68f * continentFreq,
                            w, h,
                            desc.surface.seed + 151u,
                            6, 2.0f, 0.52f
                        );

                    const float plate1 =
                        ridgedFbmWrapped(
                            uu * 2.20f * continentFreq,
                            vv * 1.10f * continentFreq,
                            w, h,
                            desc.surface.seed + 171u,
                            5, 2.0f, 0.50f
                        );

                    const float regional =
                        fbmWrapped(
                            uu * 6.0f * continentFreq,
                            vv * 3.0f * continentFreq,
                            w, h,
                            desc.surface.seed + 181u,
                            5, 2.0f, 0.50f
                        );

                    const float mountains =
                        ridgedFbmWrapped(
                            uu * 28.0f * detailScale,
                            vv * 14.0f * detailScale,
                            w, h,
                            desc.surface.seed + 191u,
                            5, 2.0f, 0.50f
                        );

                    const float canyons =
                        ridgedFbmWrapped(
                            uu * 54.0f * detailScale,
                            vv * 27.0f * detailScale,
                            w, h,
                            desc.surface.seed + 211u,
                            4, 2.0f, 0.50f
                        );

                    float waterThreshold = 0.52f;

                    if (desc.visualClass == CelestialVisualClass::OceanWorld)
                        waterThreshold = 0.60f;
                    else if (desc.visualClass == CelestialVisualClass::DesertWorld)
                        waterThreshold = 0.36f;

                    // oceanLevel now has stronger effect.
                    // Higher oceanLevel -> more water.
                    waterThreshold -= desc.surface.oceanLevel * 0.26f;

                    const float continentSignal =
                        plate0 * 0.72f +
                        plate1 * 0.20f +
                        regional * 0.08f;

                    // Sharper transition: land/water should read as shapes.
                    const float landMask =
                        smoothstep01(
                            (continentSignal - waterThreshold) * 8.0f + 0.5f
                        );

                    const float coastline =
                        bandAroundHalf(landMask, 0.18f);

                    const float relief =
                        mountains * desc.surface.mountainAmount -
                        canyons * desc.surface.canyonAmount;

                    const float oceanFloor =
                        0.20f + n0 * 0.05f + regional * 0.04f;

                    const float landHeight =
                        0.46f +
                        0.16f * plate1 +
                        0.10f * regional +
                        relief * 0.18f +
                        mountains * 0.08f;

                    height =
                        lerp(
                            oceanFloor,
                            landHeight,
                            landMask
                        );

                    // Masks:
                    // A = water
                    // B = relief
                    // C = ice
                    // D = coastline / surface feature accent
                    maskA = 1.0f - landMask;
                    maskB = clamp01(mountains * 0.75f + regional * 0.25f);
                    maskC = clamp01(std::max(desc.surface.iceCoverage, polarMask));
                    maskD = clamp01(coastline * 0.85f + regional * 0.20f);

                    break;
                }

                case CelestialVisualClass::Venusian:
                {
                    height =
                        0.60f * n0 +
                        0.25f * n1 +
                        0.15f * n2;

                    maskA = n1;
                    maskB = polarMask;
                    maskC = 0.0f;
                    break;
                }

                case CelestialVisualClass::GasGiant:
                case CelestialVisualClass::IceGiant:
                {
                    const float jet0 =
                        0.5f + 0.5f * std::sin(
                            (vv * 3.14159265f * 18.0f) +
                            (n0 - 0.5f) * 5.0f
                        );

                    const float jet1 =
                        0.5f + 0.5f * std::sin(
                            (vv * 3.14159265f * 43.0f) +
                            (n1 - 0.5f) * 9.0f
                        );

                    const float bands =
                        clamp01(0.65f * jet0 + 0.35f * jet1);

                    const float stormsField =
                        ridgedFbmWrapped(
                            uu * 32.0f,
                            vv * 16.0f,
                            w, h,
                            desc.surface.seed + 231u,
                            5, 2.0f, 0.5f
                        );

                    const float storm =
                        smoothstep01(
                            (stormsField - (0.84f - desc.surface.stormStrength * 0.30f)) * 5.0f
                        );

                    height =
                        clamp01(
                            0.58f * bands +
                            0.22f * n1 +
                            0.20f * n2
                        );

                    maskA = bands;
                    maskB = jet1;
                    maskC = storm;
                    maskD = clamp01(std::abs(jet0 - jet1) * 1.8f);
                    break;
                }

                case CelestialVisualClass::LavaWorld:
                {
                    const float cracks =
                        fbmWrapped(u * 48.0f, v * 24.0f, w, h,
                                   desc.surface.seed + 251u, 5, 2.2f, 0.55f);

                    height =
                        0.55f * n0 +
                        0.25f * n1 +
                        0.20f * n2;

                    const float lavaMask =
                        smoothstep01((cracks - (0.78f - desc.surface.lavaCoverage * 0.28f)) * 4.0f);

                    maskA = lavaMask;
                    maskB = n2;
                    maskC = 0.0f;
                    maskD = clamp01(lavaMask * 0.9f + n2 * 0.2f);
                    break;
                }

                default:
                {
                    height = 0.5f * n0 + 0.5f * n1;
                    break;
                }
            }

            out.height.values[i] = clamp01(height);
            out.mask0.values[i] = clamp01(maskA);
            out.mask1.values[i] = clamp01(maskB);
            out.mask2.values[i] = clamp01(maskC);
            out.mask3.values[i] = clamp01(maskD);
        }
    }
}

ImageRgba8 buildAlbedo(
    const CelestialBodyVisualDescriptor& desc,
    const GeneratedMaps& maps
)
{
    ImageRgba8 img = makeImage(maps.height.width, maps.height.height);

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            const std::size_t i =
                static_cast<std::size_t>(y * img.width + x);

            const float h = maps.height.values[i];
            const float m0 = maps.mask0.values[i];
            const float m1 = maps.mask1.values[i];
            const float m2 = maps.mask2.values[i];
            const float m3 = maps.mask3.values[i];

            float grayscale = h;

            switch (desc.visualClass)
            {
                case CelestialVisualClass::RockyBarren:
                case CelestialVisualClass::AsteroidRock:
                    grayscale =
                        0.24f +
                        h * 0.44f +
                        m1 * 0.14f -
                        m0 * 0.08f -
                        m2 * 0.04f;
                    break;

                case CelestialVisualClass::RockyIce:
                    grayscale =
                        0.50f +
                        h * 0.24f +
                        m0 * 0.12f +
                        m1 * 0.10f;
                    break;

                                case CelestialVisualClass::Terrestrial:
                {
                    const float water = m0;
                    const float relief = m1;
                    const float ice = m2;
                    const float coast = m3;

                    const float landGray =
                        0.54f +
                        h * 0.24f +
                        relief * 0.16f +
                        coast * 0.05f;

                    const float waterGray =
                        0.16f +
                        h * 0.05f +
                        coast * 0.04f;

                    grayscale =
                        lerp(landGray, waterGray, water);

                    grayscale =
                        lerp(grayscale, 0.88f, ice * 0.72f);

                    grayscale =
                        contrast01(grayscale, 1.28f);

                    break;
                }

                                case CelestialVisualClass::OceanWorld:
                {
                    const float water = m0;
                    const float relief = m1;
                    const float ice = m2;
                    const float coast = m3;

                    const float landGray =
                        0.58f +
                        h * 0.18f +
                        relief * 0.12f +
                        coast * 0.08f;

                    const float waterGray =
                        0.14f +
                        h * 0.045f +
                        coast * 0.035f;

                    grayscale =
                        lerp(landGray, waterGray, water);

                    grayscale =
                        lerp(grayscale, 0.86f, ice * 0.60f);

                    grayscale =
                        contrast01(grayscale, 1.34f);

                    break;
                }

                                case CelestialVisualClass::DesertWorld:
                {
                    const float water = m0;
                    const float relief = m1;
                    const float ice = m2;
                    const float feature = m3;

                    const float dryGray =
                        0.43f +
                        h * 0.26f +
                        relief * 0.18f +
                        feature * 0.10f;

                    const float lowBasin =
                        0.24f +
                        h * 0.08f +
                        feature * 0.04f;

                    grayscale =
                        lerp(dryGray, lowBasin, water * 0.50f);

                    grayscale =
                        lerp(grayscale, 0.84f, ice * 0.45f);

                    grayscale =
                        contrast01(grayscale, 1.30f);

                    break;
                }

                case CelestialVisualClass::Venusian:
                    grayscale = 0.48f + h * 0.18f;
                    break;

                case CelestialVisualClass::GasGiant:
                    grayscale =
                        0.30f +
                        h * 0.34f +
                        m0 * 0.18f +
                        m2 * 0.10f;
                    break;

                case CelestialVisualClass::IceGiant:
                    grayscale =
                        0.40f +
                        h * 0.22f +
                        m0 * 0.12f +
                        m2 * 0.08f;
                    break;

                case CelestialVisualClass::LavaWorld:
                    grayscale = 0.16f + h * 0.22f + m0 * 0.40f;
                    break;

                default:
                    grayscale = h;
                    break;
            }

            const glm::vec3 rgb =
                applySubtleTint(grayscale, desc.colorStyle);

            setPixel(img, x, y, glm::vec4(rgb, 1.0f));
        }
    }

    return img;
}

ImageRgba8 buildMaterial(
    const CelestialBodyVisualDescriptor& desc,
    const GeneratedMaps& maps
)
{
    ImageRgba8 img = makeImage(maps.height.width, maps.height.height);

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            const std::size_t i =
                static_cast<std::size_t>(y * img.width + x);

            const float h = maps.height.values[i];
            const float m0 = maps.mask0.values[i];
            const float m1 = maps.mask1.values[i];
            const float m2 = maps.mask2.values[i];

            float roughness = 0.75f;
            float channelG = h;
            float channelB = m0;
            float channelA = m1;

            switch (desc.visualClass)
            {
                case CelestialVisualClass::Terrestrial:
                case CelestialVisualClass::OceanWorld:
                {
                    const float water = m0;
                    roughness = lerp(0.72f, 0.12f, water);
                    channelB = water;
                    channelA = m2;
                    break;
                }

                case CelestialVisualClass::DesertWorld:
                    roughness = 0.82f;
                    channelB = m1;
                    channelA = m2;
                    break;

                case CelestialVisualClass::GasGiant:
                case CelestialVisualClass::IceGiant:
                    roughness = 0.95f;
                    channelB = m1;
                    channelA = m2;
                    break;

                case CelestialVisualClass::LavaWorld:
                    roughness = lerp(0.92f, 0.25f, m0);
                    channelB = m0;
                    channelA = m1;
                    break;

                default:
                    break;
            }

            setPixel(
                img,
                x, y,
                glm::vec4(
                    roughness,
                    channelG,
                    channelB,
                    channelA
                )
            );
        }
    }

    return img;
}

ImageRgba8 buildNormal(
    const GeneratedMaps& maps,
    float strength
)
{
    const int w = maps.height.width;
    const int h = maps.height.height;

    ImageRgba8 img = makeImage(w, h);

    auto sampleHeight = [&](int x, int y) -> float
    {
        if (x < 0) x += w;
        if (x >= w) x -= w;
        y = std::clamp(y, 0, h - 1);

        return maps.height.values[static_cast<std::size_t>(y * w + x)];
    };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float hl = sampleHeight(x - 1, y);
            const float hr = sampleHeight(x + 1, y);
            const float hd = sampleHeight(x, y + 1);
            const float hu = sampleHeight(x, y - 1);

            glm::vec3 n(
                (hl - hr) * strength,
                (hu - hd) * strength,
                1.0f
            );

            n = glm::normalize(n);

            const glm::vec3 enc =
                n * 0.5f + glm::vec3(0.5f);

            setPixel(img, x, y, glm::vec4(enc, 1.0f));
        }
    }

    return img;
}

ImageRgba8 buildPreviewSphere(const ImageRgba8& albedo)
{
    ImageRgba8 img = makeImage(512, 512);

    const glm::vec3 lightDir =
        glm::normalize(glm::vec3(-0.35f, 0.55f, 0.75f));

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            const float nx =
                (static_cast<float>(x) + 0.5f) / static_cast<float>(img.width) * 2.0f - 1.0f;

            const float ny =
                (static_cast<float>(y) + 0.5f) / static_cast<float>(img.height) * 2.0f - 1.0f;

            const float rr = nx * nx + ny * ny;

            if (rr > 1.0f)
            {
                setPixel(img, x, y, glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
                continue;
            }

            const float nz = std::sqrt(1.0f - rr);

            const glm::vec3 n(nx, -ny, nz);

            const float u =
                std::atan2(n.z, n.x) / (2.0f * 3.14159265f) + 0.5f;

            const float v =
                std::acos(std::clamp(n.y, -1.0f, 1.0f)) / 3.14159265f;

            glm::vec3 base = sampleEquirect(albedo, u, v);

                        const float ndotl =
                std::max(0.0f, glm::dot(n, lightDir));

            // Preview is a diagnostic image, not final game lighting.
            // Keep enough ambient light so surface features remain readable.
            const float light =
                0.46f + 0.54f * ndotl;

            const float rim =
                std::pow(1.0f - std::max(0.0f, n.z), 2.2f) * 0.10f;

            glm::vec3 lit =
                base * light + glm::vec3(rim);

            lit = clampColor(lit);

            setPixel(img, x, y, glm::vec4(lit, 1.0f));
        }
    }

    return img;
}




ImageRgba8 buildFlatPreview(const ImageRgba8& albedo)
{
    const int previewW = 1024;
    const int previewH = 512;

    ImageRgba8 img = makeImage(previewW, previewH);

    for (int y = 0; y < previewH; ++y)
    {
        for (int x = 0; x < previewW; ++x)
        {
            const float u =
                (static_cast<float>(x) + 0.5f) /
                static_cast<float>(previewW);

            const float v =
                (static_cast<float>(y) + 0.5f) /
                static_cast<float>(previewH);

            const glm::vec3 c =
                sampleEquirect(albedo, u, v);

            setPixel(img, x, y, glm::vec4(c, 1.0f));
        }
    }

    return img;
}




glm::vec4 sampleImageRgbaBilinear(
    const ImageRgba8& img,
    float u,
    float v,
    bool wrapU = true
)
{
    if (img.width <= 0 || img.height <= 0)
        return glm::vec4(0.0f);

    if (wrapU)
    {
        while (u < 0.0f) u += 1.0f;
        while (u >= 1.0f) u -= 1.0f;
    }
    else
    {
        u = clamp01(u);
    }

    v = clamp01(v);

    const float fx =
        u * static_cast<float>(img.width - 1);

    const float fy =
        v * static_cast<float>(img.height - 1);

    const int x0 =
        std::clamp(
            static_cast<int>(std::floor(fx)),
            0,
            img.width - 1
        );

    const int y0 =
        std::clamp(
            static_cast<int>(std::floor(fy)),
            0,
            img.height - 1
        );

    int x1 = x0 + 1;
    int y1 = y0 + 1;

    if (wrapU)
    {
        if (x1 >= img.width)
            x1 = 0;
    }
    else
    {
        x1 = std::clamp(x1, 0, img.width - 1);
    }

    y1 = std::clamp(y1, 0, img.height - 1);

    const float tx = fx - static_cast<float>(x0);
    const float ty = fy - static_cast<float>(y0);

    auto readPixel = [&](int x, int y) -> glm::vec4
    {
        const std::size_t i =
            static_cast<std::size_t>((y * img.width + x) * 4);

        return glm::vec4(
            static_cast<float>(img.pixels[i + 0]) / 255.0f,
            static_cast<float>(img.pixels[i + 1]) / 255.0f,
            static_cast<float>(img.pixels[i + 2]) / 255.0f,
            static_cast<float>(img.pixels[i + 3]) / 255.0f
        );
    };

    const glm::vec4 c00 = readPixel(x0, y0);
    const glm::vec4 c10 = readPixel(x1, y0);
    const glm::vec4 c01 = readPixel(x0, y1);
    const glm::vec4 c11 = readPixel(x1, y1);

    const glm::vec4 cx0 =
        glm::mix(c00, c10, tx);

    const glm::vec4 cx1 =
        glm::mix(c01, c11, tx);

    return glm::mix(cx0, cx1, ty);
}

ImageRgba8 resizeImageBilinear(
    const ImageRgba8& src,
    int dstWidth,
    int dstHeight,
    bool wrapU = true
)
{
    if (dstWidth <= 0 || dstHeight <= 0)
        return makeImage(1, 1);

    if (src.width == dstWidth &&
        src.height == dstHeight)
    {
        return src;
    }

    ImageRgba8 out =
        makeImage(dstWidth, dstHeight);

    for (int y = 0; y < dstHeight; ++y)
    {
        for (int x = 0; x < dstWidth; ++x)
        {
            const float u =
                (static_cast<float>(x) + 0.5f) /
                static_cast<float>(dstWidth);

            const float v =
                (static_cast<float>(y) + 0.5f) /
                static_cast<float>(dstHeight);

            const glm::vec4 c =
                sampleImageRgbaBilinear(
                    src,
                    u,
                    v,
                    wrapU
                );

            setPixel(out, x, y, c);
        }
    }

    return out;
}

ImageRgba8 resizeImageToMaxWidth(
    const ImageRgba8& src,
    int maxWidth,
    bool wrapU = true
)
{
    if (src.width <= maxWidth)
        return src;

    const int dstWidth =
        maxWidth;

    const int dstHeight =
        std::max(
            1,
            static_cast<int>(std::round(
                static_cast<double>(src.height) *
                static_cast<double>(dstWidth) /
                static_cast<double>(src.width)
            ))
        );

    return resizeImageBilinear(
        src,
        dstWidth,
        dstHeight,
        wrapU
    );
}

bool tryLoadGeneratedImage(
    const std::filesystem::path& path,
    ImageRgba8& out,
    bool& found
)
{
    found =
        std::filesystem::exists(path);

    if (!found)
        return true;

    return loadImageRgba8(path, out);
}







bool shouldBakeTilePyramid(
    const CelestialBodyVisualDescriptor& desc,
    const std::filesystem::path& bodyOut
)
{
    namespace fs = std::filesystem;

    // Irregular bodies will need a separate mesh-LOD path later.
    // Do not generate equirectangular sphere tiles for Phobos/Deimos meshes.
    if (fs::exists(bodyOut / "shape_model.obj"))
        return false;

    // Gas/ice giants are cloud-volume / atmosphere bodies.
    // For now they get map/global LOD only, no near-surface tile pyramid.
    switch (desc.visualClass)
    {
        case CelestialVisualClass::GasGiant:
        case CelestialVisualClass::IceGiant:
            return false;

        default:
            break;
    }

    const std::string& body =
        desc.bodyFolderName;

    // Explicit first-pass near-orbit candidates.
    // These are solid/surface bodies where close orbital view is meaningful.
    if (body == "mercury") return true;
    if (body == "venus") return true;
    if (body == "earth") return true;
    if (body == "moon") return true;
    if (body == "mars") return true;

    if (body == "io") return true;
    if (body == "europa") return true;
    if (body == "ganymede") return true;
    if (body == "callisto") return true;

    if (body == "titan") return true;
    if (body == "rhea") return true;
    if (body == "iapetus") return true;
    if (body == "dione") return true;
    if (body == "tethys") return true;
    if (body == "enceladus") return true;
    if (body == "mimas") return true;

    if (body == "titania") return true;
    if (body == "oberon") return true;
    if (body == "umbriel") return true;
    if (body == "ariel") return true;
    if (body == "miranda") return true;

    if (body == "triton") return true;

    // Fallback for future descriptors that include radius.
    if (desc.referenceRadiusKm >= 1000.0f)
        return true;

    return false;
}







bool writeTilePyramidChannel(
    const ImageRgba8& src,
    const std::filesystem::path& channelRoot,
    int tileSize,
    int maxZoom
)
{
    namespace fs = std::filesystem;

    for (int z = 0; z <= maxZoom; ++z)
    {
        const int tilesX =
            2 << z;   // 2, 4, 8, ...
        const int tilesY =
            1 << z;   // 1, 2, 4, ...

        const fs::path zoomDir =
            channelRoot / ("z" + std::to_string(z));

        fs::create_directories(zoomDir);

        for (int ty = 0; ty < tilesY; ++ty)
        {
            for (int tx = 0; tx < tilesX; ++tx)
            {
                ImageRgba8 tile =
                    makeImage(tileSize, tileSize);

                for (int py = 0; py < tileSize; ++py)
                {
                    for (int px = 0; px < tileSize; ++px)
                    {
                        const float u =
                            (static_cast<float>(tx * tileSize + px) + 0.5f) /
                            static_cast<float>(tilesX * tileSize);

                        const float v =
                            (static_cast<float>(ty * tileSize + py) + 0.5f) /
                            static_cast<float>(tilesY * tileSize);

                        const glm::vec4 c =
                            sampleImageRgbaBilinear(
                                src,
                                u,
                                v,
                                true
                            );

                        setPixel(tile, px, py, c);
                    }
                }

                const fs::path tilePath =
                    zoomDir /
                    (
                        std::string("x") +
                        std::to_string(tx) +
                        "_y" +
                        std::to_string(ty) +
                        ".tga"
                    );

                if (!writeTgaRgba(tilePath, tile))
                    return false;
            }
        }
    }

    return true;
}

bool bakeRenderReadyLodSet(
    const CelestialBodyVisualDescriptor& desc,
    const std::filesystem::path& bodyOut,
    const ImageRgba8& albedo,
    const ImageRgba8& normal,
    const ImageRgba8& material,
    const ImageRgba8& preview,
    const ImageRgba8& previewFlat,
    json& meta
)
{
    namespace fs = std::filesystem;

    const fs::path lodRoot =
        bodyOut / "lod";

    const fs::path mapDir =
        lodRoot / "map";

    const fs::path globalDir =
        lodRoot / "global";

    const fs::path tilesDir =
        lodRoot / "tiles";

    fs::create_directories(mapDir);
    fs::create_directories(globalDir);

    // ------------------------------------------------------------
    // map/
    // ------------------------------------------------------------
    const ImageRgba8 mapPreview =
        (preview.width == 512 && preview.height == 512)
            ? preview
            : resizeImageBilinear(preview, 512, 512, false);

    const ImageRgba8 mapFlat =
        (previewFlat.width == 1024 && previewFlat.height == 512)
            ? previewFlat
            : resizeImageBilinear(previewFlat, 1024, 512, true);

    const fs::path mapPreviewPath =
        mapDir / "preview_512.tga";

    const fs::path mapFlatPath =
        mapDir / "flat_1024.tga";

    if (!writeTgaRgba(mapPreviewPath, mapPreview)) return false;
    if (!writeTgaRgba(mapFlatPath, mapFlat)) return false;

    // ------------------------------------------------------------
    // global/
    // ------------------------------------------------------------
    const ImageRgba8 globalAlbedo =
        resizeImageToMaxWidth(albedo, 2048, true);

    const ImageRgba8 globalNormal =
        resizeImageToMaxWidth(normal, 2048, true);

    const ImageRgba8 globalMaterial =
        resizeImageToMaxWidth(material, 2048, true);

    const fs::path globalAlbedoPath =
        globalDir / "albedo_2048.tga";

    const fs::path globalNormalPath =
        globalDir / "normal_2048.tga";

    const fs::path globalMaterialPath =
        globalDir / "material_2048.tga";

    if (!writeTgaRgba(globalAlbedoPath, globalAlbedo)) return false;
    if (!writeTgaRgba(globalNormalPath, globalNormal)) return false;
    if (!writeTgaRgba(globalMaterialPath, globalMaterial)) return false;

    bool cloudsFound = false;
    bool emissionFound = false;

    ImageRgba8 clouds;
    ImageRgba8 emission;

    if (!tryLoadGeneratedImage(bodyOut / "clouds.tga", clouds, cloudsFound))
        return false;

    if (!tryLoadGeneratedImage(bodyOut / "emission.tga", emission, emissionFound))
        return false;

    fs::path globalCloudsPath;
    fs::path globalEmissionPath;

    if (cloudsFound)
    {
        const ImageRgba8 globalClouds =
            resizeImageToMaxWidth(clouds, 2048, true);

        globalCloudsPath =
            globalDir / "clouds_2048.tga";

        if (!writeTgaRgba(globalCloudsPath, globalClouds)) return false;
    }

    if (emissionFound)
    {
        const ImageRgba8 globalEmission =
            resizeImageToMaxWidth(emission, 2048, true);

        globalEmissionPath =
            globalDir / "emission_2048.tga";

        if (!writeTgaRgba(globalEmissionPath, globalEmission)) return false;
    }

    // ------------------------------------------------------------
    // tiles/
    // ------------------------------------------------------------
    const bool tilesEnabled =
        shouldBakeTilePyramid(desc, bodyOut);

    const int tileSize = 512;
    const int maxZoom = 2;

    if (tilesEnabled)
    {
        if (!writeTilePyramidChannel(
                albedo,
                tilesDir / "albedo",
                tileSize,
                maxZoom))
            return false;

        if (!writeTilePyramidChannel(
                normal,
                tilesDir / "normal",
                tileSize,
                maxZoom))
            return false;

        if (!writeTilePyramidChannel(
                material,
                tilesDir / "material",
                tileSize,
                maxZoom))
            return false;

        if (cloudsFound)
        {
            if (!writeTilePyramidChannel(
                    clouds,
                    tilesDir / "clouds",
                    tileSize,
                    maxZoom))
                return false;
        }

        if (emissionFound)
        {
            if (!writeTilePyramidChannel(
                    emission,
                    tilesDir / "emission",
                    tileSize,
                    maxZoom))
                return false;
        }
    }

    json channels =
        json::array();

    channels.push_back("albedo");
    channels.push_back("normal");
    channels.push_back("material");

    if (cloudsFound)
        channels.push_back("clouds");

    if (emissionFound)
        channels.push_back("emission");

    meta["render_lod"] = {
        {
            "map",
            {
                { "preview_512", toAssetPath(mapPreviewPath) },
                { "flat_1024", toAssetPath(mapFlatPath) }
            }
        },
        {
            "global",
            {
                { "albedo", toAssetPath(globalAlbedoPath) },
                { "normal", toAssetPath(globalNormalPath) },
                { "material", toAssetPath(globalMaterialPath) },
                { "clouds", cloudsFound ? toAssetPath(globalCloudsPath) : "" },
                { "emission", emissionFound ? toAssetPath(globalEmissionPath) : "" }
            }
        },
        {
            "tiles",
            {
                { "enabled", tilesEnabled },
                { "tile_size", tileSize },
                { "max_zoom", tilesEnabled ? maxZoom : 0 },
                { "root", tilesEnabled ? toAssetPath(tilesDir) : "" },
                { "channels", channels }
            }
        }
    };

    meta["runtime_texture_strategy"] =
        "Use lod/map for system-map and UI, lod/global for far-orbit rendering, and lod/tiles for close-orbit rendering where enabled.";

    return true;
}





bool loadImageRgba8(
    const std::filesystem::path& path,
    ImageRgba8& out
)
{
    int width = 0;
    int height = 0;
    int channels = 0;

    stbi_set_flip_vertically_on_load(false);

    unsigned char* data =
        stbi_load(
            path.string().c_str(),
            &width,
            &height,
            &channels,
            4
        );

    if (!data)
    {
        std::cerr
            << "[CelestialTextureBaker] failed to load image: "
            << path.string()
            << "\n";

        return false;
    }

    out.width = width;
    out.height = height;
    out.pixels.assign(
        data,
        data + static_cast<std::size_t>(width * height * 4)
    );

    stbi_image_free(data);

    return true;
}

ImageRgba8 buildFlatNormalImage(
    int width,
    int height
)
{
    ImageRgba8 img =
        makeImage(width, height);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            setPixel(
                img,
                x,
                y,
                glm::vec4(
                    0.5f,
                    0.5f,
                    1.0f,
                    1.0f
                )
            );
        }
    }

    return img;
}

ImageRgba8 buildDefaultImportedMaterialImage(
    const CelestialBodyVisualDescriptor& desc,
    int width,
    int height
)
{
    ImageRgba8 img =
        makeImage(width, height);

    float roughness = 0.78f;

    if (desc.visualClass == CelestialVisualClass::Terrestrial ||
        desc.visualClass == CelestialVisualClass::OceanWorld)
    {
        roughness = 0.55f;
    }
    else if (desc.visualClass == CelestialVisualClass::RockyBarren ||
             desc.visualClass == CelestialVisualClass::DesertWorld)
    {
        roughness = 0.82f;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            setPixel(
                img,
                x,
                y,
                glm::vec4(
                    roughness, // R = roughness
                    0.5f,      // G = height neutral
                    0.0f,      // B = mask 0
                    0.0f       // A = mask 1
                )
            );
        }
    }

    return img;
}

bool loadJsonFile(
    const std::filesystem::path& path,
    json& out
)
{
    std::ifstream f(path);

    if (!f.is_open())
        return false;

    f >> out;
    return true;
}

bool getCatalogAssetPattern(
    const json& catalog,
    const std::string& groupName,
    const std::string& key,
    std::string& outPattern
)
{
    if (!catalog.contains(groupName) ||
        !catalog[groupName].is_object())
    {
        return false;
    }

    if (!catalog[groupName].contains(key))
        return false;

    outPattern =
        catalog[groupName][key].get<std::string>();

    return !outPattern.empty();
}



std::filesystem::path outputPathFromContract(
    const std::string& contractPath,
    const std::filesystem::path& bodyOut,
    const std::string& fallbackFilename
)
{
    namespace fs = std::filesystem;

    if (contractPath.empty())
        return bodyOut / fallbackFilename;

    // Важно:
    // derived_outputs в catalog — это контракт имени результата.
    // А outputRoot всё ещё может быть переопределён через CLI.
    // Поэтому берём filename из контракта, но кладём в текущий bodyOut.
    return bodyOut / fs::path(contractPath).filename();
}

bool findCatalogAsset(
    const std::string& pattern,
    std::filesystem::path& outPath
)
{
    if (pattern.empty())
        return false;

    return findAssetPath(pattern, outPath);
}

float pixelLuma01(
    const ImageRgba8& img,
    int x,
    int y
)
{
    const std::size_t i =
        static_cast<std::size_t>((y * img.width + x) * 4);

    const float r =
        static_cast<float>(img.pixels[i + 0]) / 255.0f;

    const float g =
        static_cast<float>(img.pixels[i + 1]) / 255.0f;

    const float b =
        static_cast<float>(img.pixels[i + 2]) / 255.0f;

    return clamp01(
        r * 0.2126f +
        g * 0.7152f +
        b * 0.0722f
    );
}

float sampleHeightWrapped01(
    const ImageRgba8& img,
    int x,
    int y
)
{
    if (img.width <= 0 || img.height <= 0)
        return 0.5f;

    while (x < 0)
        x += img.width;

    while (x >= img.width)
        x -= img.width;

    y = std::clamp(y, 0, img.height - 1);

    return pixelLuma01(img, x, y);
}

ImageRgba8 buildNormalFromHeightImage(
    const ImageRgba8& height,
    float strength
)
{
    ImageRgba8 normal =
        makeImage(height.width, height.height);

    for (int y = 0; y < height.height; ++y)
    {
        for (int x = 0; x < height.width; ++x)
        {
            const float hL =
                sampleHeightWrapped01(height, x - 1, y);

            const float hR =
                sampleHeightWrapped01(height, x + 1, y);

            const float hD =
                sampleHeightWrapped01(height, x, y + 1);

            const float hU =
                sampleHeightWrapped01(height, x, y - 1);

            glm::vec3 n =
                glm::normalize(glm::vec3(
                    -(hR - hL) * strength,
                    -(hD - hU) * strength,
                    1.0f
                ));

            const glm::vec3 enc =
                n * 0.5f + glm::vec3(0.5f);

            setPixel(
                normal,
                x,
                y,
                glm::vec4(enc, 1.0f)
            );
        }
    }

    return normal;
}

ImageRgba8 buildImportedMaterialImage(
    const CelestialBodyVisualDescriptor& desc,
    const ImageRgba8& albedo,
    const ImageRgba8* height
)
{
    ImageRgba8 img =
        makeImage(albedo.width, albedo.height);

    float baseRoughness = 0.78f;

    if (desc.visualClass == CelestialVisualClass::Terrestrial ||
        desc.visualClass == CelestialVisualClass::OceanWorld)
    {
        baseRoughness = 0.55f;
    }
    else if (desc.visualClass == CelestialVisualClass::RockyBarren ||
             desc.visualClass == CelestialVisualClass::AsteroidRock ||
             desc.visualClass == CelestialVisualClass::DesertWorld)
    {
        baseRoughness = 0.84f;
    }
    else if (desc.visualClass == CelestialVisualClass::GasGiant ||
             desc.visualClass == CelestialVisualClass::IceGiant)
    {
        baseRoughness = 0.92f;
    }

    for (int y = 0; y < img.height; ++y)
    {
        for (int x = 0; x < img.width; ++x)
        {
            float heightValue = 0.5f;

            if (height &&
                height->width == img.width &&
                height->height == img.height)
            {
                heightValue =
                    pixelLuma01(*height, x, y);
            }

            float roughness =
                baseRoughness;

            // Простая универсальная эвристика:
            // очень тёмные/синие области на terrestrial/ocean world читаем как более гладкие.
            if (desc.visualClass == CelestialVisualClass::Terrestrial ||
                desc.visualClass == CelestialVisualClass::OceanWorld)
            {
                const std::size_t i =
                    static_cast<std::size_t>((y * albedo.width + x) * 4);

                const float r =
                    static_cast<float>(albedo.pixels[i + 0]) / 255.0f;

                const float g =
                    static_cast<float>(albedo.pixels[i + 1]) / 255.0f;

                const float b =
                    static_cast<float>(albedo.pixels[i + 2]) / 255.0f;

                const bool likelyWater =
                    b > r * 1.08f &&
                    b > g * 0.90f;

                if (likelyWater)
                    roughness = 0.32f;
            }

            setPixel(
                img,
                x,
                y,
                glm::vec4(
                    roughness,   // R = roughness
                    heightValue, // G = normalized height / relief helper
                    0.0f,        // B = reserved
                    1.0f         // A = valid material pixel
                )
            );
        }
    }

    return img;
}

bool writeImportedImageIfPresent(
    const std::string& sourcePattern,
    const std::filesystem::path& outputPath,
    const char* label,
    nlohmann::json& resolvedSources,
    nlohmann::json& generatedOutputs
)
{
    namespace fs = std::filesystem;

    fs::path sourcePath;

    if (!findCatalogAsset(sourcePattern, sourcePath))
        return true;

    ImageRgba8 img;

    if (!loadImageRgba8(sourcePath, img))
        return false;

    fs::create_directories(outputPath.parent_path());

    if (!writeTgaRgba(outputPath, img))
        return false;

    resolvedSources[label] =
        toAssetPath(sourcePath);

    generatedOutputs[label] =
        toAssetPath(outputPath);

    return true;
}

bool copyImportedMeshIfPresent(
    const std::string& sourcePattern,
    const std::filesystem::path& outputPath,
    nlohmann::json& resolvedSources,
    nlohmann::json& generatedOutputs
)
{
    namespace fs = std::filesystem;

    fs::path sourcePath;

    if (!findCatalogAsset(sourcePattern, sourcePath))
        return false;

    const std::string ext =
        sourcePath.extension().generic_string();

    if (ext != ".obj")
    {
        std::cerr
            << "[CelestialTextureBaker] unsupported mesh source format: "
            << sourcePath.generic_string()
            << " ; currently expected .obj\n";

        return false;
    }

    fs::create_directories(outputPath.parent_path());

    std::error_code ec;
    fs::copy_file(
        sourcePath,
        outputPath,
        fs::copy_options::overwrite_existing,
        ec
    );

    if (ec)
    {
        std::cerr
            << "[CelestialTextureBaker] failed to copy mesh "
            << sourcePath.generic_string()
            << " -> "
            << outputPath.generic_string()
            << " : "
            << ec.message()
            << "\n";

        return false;
    }

    resolvedSources["mesh"] =
        toAssetPath(sourcePath);

    generatedOutputs["mesh"] =
        toAssetPath(outputPath);

    return true;
}







bool bakeImportedRealDataBody(
    CelestialBodyVisualDescriptor desc,
    const std::filesystem::path& outputRoot
)
{
    namespace fs = std::filesystem;

    if (desc.sourceCatalogPath.empty())
    {
        std::cerr
            << "[CelestialTextureBaker] ImportedRealData body has no source catalog: "
            << desc.systemFolderName << "/"
            << desc.bodyFolderName
            << "\n";

        return false;
    }

    const fs::path catalogPath =
        resolveProjectPath(desc.sourceCatalogPath);

    CelestialSourceCatalog catalog;

    if (!loadCelestialSourceCatalog(catalogPath, catalog))
    {
        std::cerr
            << "[CelestialTextureBaker] cannot read source catalog: "
            << desc.sourceCatalogPath
            << "\n";

        return false;
    }

    const fs::path bodyOut =
        outputRoot /
        fs::path(desc.systemFolderName) /
        fs::path(desc.bodyFolderName);

    fs::create_directories(bodyOut);

    const fs::path albedoOut =
        outputPathFromContract(
            catalog.outputs.albedo,
            bodyOut,
            "albedo.tga"
        );

    const fs::path normalOut =
        outputPathFromContract(
            catalog.outputs.normal,
            bodyOut,
            "normal.tga"
        );

    const fs::path materialOut =
        outputPathFromContract(
            catalog.outputs.material,
            bodyOut,
            "material.tga"
        );

    const fs::path cloudsOut =
        outputPathFromContract(
            catalog.outputs.clouds,
            bodyOut,
            "clouds.tga"
        );

    const fs::path emissionOut =
        outputPathFromContract(
            catalog.outputs.emission,
            bodyOut,
            "emission.tga"
        );

    const fs::path meshOut =
        outputPathFromContract(
            catalog.outputs.mesh,
            bodyOut,
            "shape_model.obj"
        );

    const fs::path previewOut =
        outputPathFromContract(
            catalog.outputs.preview,
            bodyOut,
            "preview.tga"
        );

    const fs::path previewFlatOut =
        bodyOut / "preview_flat.tga";

    const fs::path metaOut =
        outputPathFromContract(
            catalog.outputs.meta,
            bodyOut,
            "meta.json"
        );

    nlohmann::json resolvedSources =
        nlohmann::json::object();

    nlohmann::json generatedOutputs =
        nlohmann::json::object();

    // ---------------------------------------------------------------------
    // Validate shape/projection contract.
    // ---------------------------------------------------------------------
    if (catalog.bodyShape == CelestialBodyShape::Sphere &&
        catalog.projection != CelestialProjection::Equirectangular)
    {
        std::cerr
            << "[CelestialTextureBaker] invalid catalog projection for sphere: "
            << desc.systemFolderName << "/"
            << desc.bodyFolderName
            << "\n";

        return false;
    }

    if (catalog.bodyShape == CelestialBodyShape::IrregularMesh &&
        catalog.projection != CelestialProjection::MeshUv)
    {
        std::cerr
            << "[CelestialTextureBaker] invalid catalog projection for irregular mesh: "
            << desc.systemFolderName << "/"
            << desc.bodyFolderName
            << "\n";

        return false;
    }

    // ---------------------------------------------------------------------
    // Required mesh for irregular bodies.
    // ---------------------------------------------------------------------
    if (catalog.bodyShape == CelestialBodyShape::IrregularMesh)
    {
        if (catalog.required.mesh.empty())
        {
            std::cerr
                << "[CelestialTextureBaker] irregular mesh body has no required mesh pattern: "
                << desc.systemFolderName << "/"
                << desc.bodyFolderName
                << "\n";

            return false;
        }

        if (!copyImportedMeshIfPresent(
                catalog.required.mesh,
                meshOut,
                resolvedSources,
                generatedOutputs))
        {
            std::cerr
                << "[CelestialTextureBaker] missing required mesh source: "
                << catalog.required.mesh
                << "\n";

            return false;
        }

        desc.textures.shapeModelPath = toAssetPath(meshOut);
    }

    // ---------------------------------------------------------------------
    // Required albedo for all imported bodies.
    // ---------------------------------------------------------------------
    if (catalog.required.albedo.empty())
    {
        std::cerr
            << "[CelestialTextureBaker] source catalog has no required albedo: "
            << desc.sourceCatalogPath
            << "\n";

        return false;
    }

    fs::path albedoSourcePath;

    if (!findCatalogAsset(
            catalog.required.albedo,
            albedoSourcePath))
    {
        std::cerr
            << "[CelestialTextureBaker] missing required albedo source: "
            << catalog.required.albedo
            << "\n";

        return false;
    }

    ImageRgba8 albedo;

    if (!loadImageRgba8(
            albedoSourcePath,
            albedo))
    {
        return false;
    }

    resolvedSources["albedo"] = toAssetPath(albedoSourcePath);

    // ---------------------------------------------------------------------
    // Optional height.
    // If present and same resolution as albedo: use it for normal/material.
    // If absent: flat normal, neutral material height.
    // ---------------------------------------------------------------------
    ImageRgba8 height;
    ImageRgba8* heightPtr = nullptr;

    if (!catalog.optional.height.empty())
    {
        fs::path heightSourcePath;

        if (findCatalogAsset(
                catalog.optional.height,
                heightSourcePath))
        {
            if (!loadImageRgba8(
                    heightSourcePath,
                    height))
            {
                return false;
            }

            if (height.width == albedo.width &&
                height.height == albedo.height)
            {
                heightPtr = &height;
            }
            else
            {
                std::cerr
                    << "[CelestialTextureBaker] optional height resolution mismatch for "
                    << desc.systemFolderName << "/"
                    << desc.bodyFolderName
                    << " ; height ignored for normal/material. height="
                    << height.width << "x" << height.height
                    << " albedo="
                    << albedo.width << "x" << albedo.height
                    << "\n";
            }

            resolvedSources["height"] = toAssetPath(heightSourcePath);
        }
    }

    ImageRgba8 normal =
        heightPtr
            ? buildNormalFromHeightImage(*heightPtr, 8.0f)
            : buildFlatNormalImage(albedo.width, albedo.height);

    ImageRgba8 material =
        buildImportedMaterialImage(
            desc,
            albedo,
            heightPtr
        );

    ImageRgba8 preview =
        buildPreviewSphere(albedo);

    ImageRgba8 previewFlat =
        buildFlatPreview(albedo);

    desc.textures.albedoPath =
        toAssetPath(albedoOut);

    desc.textures.normalPath =
        toAssetPath(normalOut);

    desc.textures.materialPath =
        toAssetPath(materialOut);

    desc.textures.previewPath =
        toAssetPath(previewOut);

    desc.textures.previewFlatPath =
        toAssetPath(previewFlatOut);

    desc.textures.metaPath =
        toAssetPath(metaOut);

    if (!writeTgaRgba(albedoOut, albedo)) return false;
    if (!writeTgaRgba(normalOut, normal)) return false;
    if (!writeTgaRgba(materialOut, material)) return false;
    if (!writeTgaRgba(previewOut, preview)) return false;
    if (!writeTgaRgba(previewFlatOut, previewFlat)) return false;




        








    generatedOutputs["albedo"] =
        toAssetPath(albedoOut);

    generatedOutputs["normal"] =
        toAssetPath(normalOut);

    generatedOutputs["material"] =
        toAssetPath(materialOut);

    generatedOutputs["preview"] =
        toAssetPath(previewOut);

    generatedOutputs["preview_flat"] =
        toAssetPath(previewFlatOut);

    // ---------------------------------------------------------------------
    // Optional clouds.
    // ---------------------------------------------------------------------
    if (!catalog.optional.clouds.empty())
    {
        if (!writeImportedImageIfPresent(
                catalog.optional.clouds,
                cloudsOut,
                "clouds",
                resolvedSources,
                generatedOutputs))
        {
            return false;
        }

        if (generatedOutputs.contains("clouds"))
            desc.textures.cloudsPath = toAssetPath(cloudsOut);
    }

    // ---------------------------------------------------------------------
    // Optional emission.
    // ---------------------------------------------------------------------
    if (!catalog.optional.emission.empty())
    {
        if (!writeImportedImageIfPresent(
                catalog.optional.emission,
                emissionOut,
                "emission",
                resolvedSources,
                generatedOutputs))
        {
            return false;
        }

        if (generatedOutputs.contains("emission"))
            desc.textures.emissionPath = toAssetPath(emissionOut);
    }


    json meta = descriptorToJson(desc);

    if (!bakeRenderReadyLodSet(
            desc,
            bodyOut,
            albedo,
            normal,
            material,
            preview,
            previewFlat,
            meta))
    {
        return false;
    }

   

    meta["baker_mode"] =
        "ImportedRealDataContract";

    meta["source_catalog_contract"] = {
        { "catalog_path", desc.sourceCatalogPath },
        { "system_id", catalog.systemId },
        { "body_id", catalog.bodyId },
        { "body_name", catalog.bodyName },
        { "source_policy", catalog.sourcePolicy },
        { "body_shape", toString(catalog.bodyShape) },
        { "projection", toString(catalog.projection) }
    };

    meta["resolved_source_assets"] =
        resolvedSources;

    meta["generated_outputs_resolved"] =
        generatedOutputs;

    meta["notes_runtime"] =
        "Imported source catalog was used as the body texture contract. Missing optional maps are allowed; missing required maps are fatal.";

    fs::create_directories(metaOut.parent_path());

    std::ofstream f(metaOut);

    if (!f.is_open())
        return false;

    f << meta.dump(4);

    std::cout
        << "[CelestialTextureBaker] imported real-data body "
        << desc.systemFolderName
        << "/"
        << desc.bodyFolderName
        << " shape="
        << toString(catalog.bodyShape)
        << " projection="
        << toString(catalog.projection)
        << "\n";

    return true;
}

















json descriptorToJson(const CelestialBodyVisualDescriptor& d)
{
    json j;

    j["system_folder_name"] = d.systemFolderName;
    j["body_folder_name"] = d.bodyFolderName;
    j["display_name"] = d.displayName;
    j["source_system_id"] = d.sourceSystemId;
    j["source_body_id"] = d.sourceBodyId;
    j["reference_radius_km"] = d.referenceRadiusKm;

    j["reference_radius_source"] = {
        { "source_path", kStarAtlasSystemDetailsPath },
        { "source_field", "diameter_km" },
        { "derivation", "reference_radius_km = diameter_km * 0.5" },
        { "authoritative_layer", "star_atlas/system_details.json" }
    };

    j["notes"] = d.notes;
    j["visual_class"] = toString(d.visualClass);
    j["surface_source_kind"] = toString(d.surfaceSourceKind);
    j["surface_reference_path"] = d.surfaceReferencePath;
    j["source_catalog_path"] = d.sourceCatalogPath;
    j["data_confidence"] = d.dataConfidence;
    j["has_atmosphere"] = d.hasAtmosphere;
    j["has_cloud_layer"] = d.hasCloudLayer;
    j["has_emission"] = d.hasEmission;

    j["color_style"] = {
        { "tint", { d.colorStyle.tint.r, d.colorStyle.tint.g, d.colorStyle.tint.b } },
        { "saturation", d.colorStyle.saturation },
        { "contrast", d.colorStyle.contrast },
        { "brightness", d.colorStyle.brightness }
    };

        j["surface"] = {
        { "seed", d.surface.seed },
        { "macro_noise_scale", d.surface.macroNoiseScale },
        { "detail_noise_scale", d.surface.detailNoiseScale },
        { "roughness", d.surface.roughness },
        { "ridge_strength", d.surface.ridgeStrength },
        { "erosion", d.surface.erosion },
        { "crater_density", d.surface.craterDensity },
        { "crater_strength", d.surface.craterStrength },

        { "crater_small_density", d.surface.craterSmallDensity },
        { "crater_large_density", d.surface.craterLargeDensity },

        { "continent_frequency", d.surface.continentFrequency },
        { "continent_warp", d.surface.continentWarp },

        { "mountain_amount", d.surface.mountainAmount },
        { "canyon_amount", d.surface.canyonAmount },

        { "albedo_variation", d.surface.albedoVariation },

        { "ocean_level", d.surface.oceanLevel },
        { "ice_coverage", d.surface.iceCoverage },
        { "desertness", d.surface.desertness },
        { "lava_coverage", d.surface.lavaCoverage },
        { "banding_strength", d.surface.bandingStrength },
        { "storm_strength", d.surface.stormStrength }
    };

    j["texture_profile"] = {
        { "width", d.textureProfile.width },
        { "height", d.textureProfile.height },
        { "auto_from_radius", d.textureProfile.autoFromRadius },
        { "max_width", d.textureProfile.maxWidth },
        { "max_height", d.textureProfile.maxHeight },
        { "target_meters_per_texel", d.textureProfile.targetMetersPerTexel },
        { "detail_target_meters_per_texel", d.textureProfile.detailTargetMetersPerTexel },
        { "min_meaningful_altitude_km", d.textureProfile.minMeaningfulAltitudeKm }
    };

    j["generated_textures"] = {
        { "albedo", d.textures.albedoPath },
        { "normal", d.textures.normalPath },
        { "material", d.textures.materialPath },
        { "clouds", d.textures.cloudsPath },
        { "emission", d.textures.emissionPath },
        { "shape_model", d.textures.shapeModelPath },
        { "preview", d.textures.previewPath },
        { "preview_flat", d.textures.previewFlatPath },
        { "meta", d.textures.metaPath }
    };

    

    return j;
}

bool bakeOneBody(
    CelestialBodyVisualDescriptor desc,
    const std::filesystem::path& outputRoot,
    const CelestialTextureBakeOptions& options
)
{
    namespace fs = std::filesystem;




    if (desc.surfaceSourceKind ==
        CelestialSurfaceSourceKind::ImportedRealData)
    {
        if (!options.allowImportedRealDataBake)
        {
            if (options.printSkipped)
            {
                std::cout
                    << "[CelestialTextureBaker] skip ImportedRealData for now: "
                    << desc.systemFolderName << "/"
                    << desc.bodyFolderName
                    << "\n";

                if (!desc.sourceCatalogPath.empty())
                {
                    std::cout
                        << "  source_catalog_path: "
                        << desc.sourceCatalogPath
                        << "\n";
                }
            }

            return true;
        }

        return bakeImportedRealDataBody(
            desc,
            outputRoot
        );
    }





    const fs::path bodyOut =
        outputRoot /
        fs::path(desc.systemFolderName) /
        fs::path(desc.bodyFolderName);

    fs::create_directories(bodyOut);

    GeneratedMaps maps;
    generateSurfaceMaps(desc, maps);

    ImageRgba8 albedo = buildAlbedo(desc, maps);
    ImageRgba8 normal = buildNormal(maps, 2.75f);
    ImageRgba8 material = buildMaterial(desc, maps);
    ImageRgba8 preview = buildPreviewSphere(albedo);
    ImageRgba8 previewFlat = buildFlatPreview(albedo);

    desc.textures.albedoPath = toAssetPath(bodyOut / "albedo.tga");
    desc.textures.normalPath = toAssetPath(bodyOut / "normal.tga");
    desc.textures.materialPath = toAssetPath(bodyOut / "material.tga");

    desc.textures.previewPath = toAssetPath(bodyOut / "preview.tga");
    desc.textures.previewFlatPath = toAssetPath(bodyOut / "preview_flat.tga");
    desc.textures.metaPath = toAssetPath(bodyOut / "meta.json");

    if (!writeTgaRgba(bodyOut / "albedo.tga", albedo)) return false;
    if (!writeTgaRgba(bodyOut / "normal.tga", normal)) return false;
    if (!writeTgaRgba(bodyOut / "material.tga", material)) return false;
    if (!writeTgaRgba(bodyOut / "preview.tga", preview)) return false;
    if (!writeTgaRgba(bodyOut / "preview_flat.tga", previewFlat)) return false;





    json metaJson =
        descriptorToJson(desc);

    if (!bakeRenderReadyLodSet(
            desc,
            bodyOut,
            albedo,
            normal,
            material,
            preview,
            previewFlat,
            metaJson))
    {
        return false;
    }




    std::ofstream meta(bodyOut / "meta.json");
    if (!meta.is_open())
        return false;

    meta << metaJson.dump(4);

    std::cout
        << "[CelestialTextureBaker] baked "
        << desc.systemFolderName << "/" << desc.bodyFolderName
        << "\n";

    return true;
}

} // namespace

bool CelestialTextureBaker::bakeLibrary(
    const CelestialTextureBakeOptions& options
)
{
    CelestialVisualLibrary lib;

    const auto resolvedPresets =
        resolveProjectPath(options.presetsPath);

    const auto resolvedBodies =
        resolveProjectPath(options.bodyVisualsRoot);

    const auto resolvedOutput =
        resolveProjectPath(options.outputRoot);


    const auto resolvedStarSystems =
        resolveProjectPath(kStarAtlasSystemsPath);

    const auto resolvedSystemDetails =
        resolveProjectPath(kStarAtlasSystemDetailsPath);

    world::celestial::StarAtlasDatabase physicalAtlas;

    const bool physicalAtlasLoaded =
        physicalAtlas.load(
            resolvedStarSystems.string(),
            resolvedSystemDetails.string()
        );

    if (!physicalAtlasLoaded)
    {
        std::cerr
            << "[CelestialTextureBaker] WARNING: physical atlas was not loaded; "
            << "reference_radius_km will stay as descriptor fallback values.\n";
    }












    if (!lib.load(
            resolvedPresets.string(),
            resolvedBodies.string()))
    {
        std::cerr
            << "[CelestialTextureBaker] visual library load failed\n";

        return false;
    }

    std::filesystem::create_directories(resolvedOutput);

int matched = 0;
int baked = 0;
int skipped = 0;
int failed = 0;

int sourceAuditOk = 0;
int sourceAuditFailed = 0;

int physicalRadiusResolved = 0;
int physicalRadiusMissing = 0;



    std::cout
        << "[CelestialTextureBaker] filters:"
        << " system=\""
        << options.systemFilter
        << "\" body=\""
        << options.bodyFilter
        << "\""
        << "\n";

    for (const auto& body : lib.bodies())
    {
        if (!bodyMatchesFilter(body, options))
            continue;

        ++matched;





        CelestialBodyVisualDescriptor bodyForBake =
            body;

        if (physicalAtlasLoaded)
        {
            if (enrichDescriptorFromPhysicalAtlas(
                    bodyForBake,
                    physicalAtlas))
            {
                ++physicalRadiusResolved;
            }
            else
            {
                ++physicalRadiusMissing;

                std::cerr
                    << "[CelestialTextureBaker] WARNING: physical radius not resolved for "
                    << bodyForBake.systemFolderName
                    << "/"
                    << bodyForBake.bodyFolderName
                    << " source_body_id="
                    << bodyForBake.sourceBodyId
                    << "\n";
            }
        }












        if (options.listOnly)
        {
            printBakeBodyLine(bodyForBake, "[CelestialTextureBaker] body");

            if (options.checkSources &&
                bodyForBake.surfaceSourceKind ==
                    CelestialSurfaceSourceKind::ImportedRealData)
            {
                const bool auditOk =
                    auditImportedSourceCatalog(bodyForBake);

                if (auditOk)
                    ++sourceAuditOk;
                else
                    ++sourceAuditFailed;
            }

            continue;
        }




        const bool importedRealData =
            bodyForBake.surfaceSourceKind ==
                CelestialSurfaceSourceKind::ImportedRealData;

        const bool willSkipImportedRealData =
            importedRealData &&
            !options.allowImportedRealDataBake;

        const bool bodyOk =
            bakeOneBody(
                bodyForBake,
                resolvedOutput,
                options
            );

        if (!bodyOk)
        {
            ++failed;

            std::cerr
                << "[CelestialTextureBaker] failed for "
                << bodyForBake.systemFolderName << "/"
                << bodyForBake.bodyFolderName << "\n";

            continue;
        }

        if (willSkipImportedRealData)
        {
            ++skipped;
        }
        else
        {
            ++baked;
        }
        
        






    }

    if (matched == 0)
    {
        std::cerr
            << "[CelestialTextureBaker] no bodies matched filters:"
            << " system=\""
            << options.systemFilter
            << "\" body=\""
            << options.bodyFilter
            << "\""
            << "\n";

        return false;
    }


    if (options.listOnly)
    {
        if (options.checkSources)
        {
            std::cout
                << "[CelestialTextureBaker] source check complete"
                << " matched=" << matched
                << " ready=" << sourceAuditOk
                << " missing_required=" << sourceAuditFailed
                << " physical_radius_resolved=" << physicalRadiusResolved
                << " physical_radius_missing=" << physicalRadiusMissing
                << "\n";

            return sourceAuditFailed == 0;
        }

        std::cout
            << "[CelestialTextureBaker] list complete, matched="
            << matched
            << "\n";

        return true;
    }



    std::cout
        << "[CelestialTextureBaker] done"
        << " matched=" << matched
        << " baked=" << baked
        << " skipped=" << skipped
        << " failed=" << failed
        << " physical_radius_resolved=" << physicalRadiusResolved
        << " physical_radius_missing=" << physicalRadiusMissing
        << " output root: "
        << resolvedOutput.string()
        << "\n";

    return failed == 0;
}

bool CelestialTextureBaker::bakeLibrary(
    const std::string& presetsPath,
    const std::string& bodyVisualsRoot,
    const std::string& outputRoot
)
{
    CelestialTextureBakeOptions options;
    options.presetsPath = presetsPath;
    options.bodyVisualsRoot = bodyVisualsRoot;
    options.outputRoot = outputRoot;

    return bakeLibrary(options);
}

} // namespace world::celestial::visual