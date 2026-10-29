#include "src/world/celestial/StarAtlasDatabase.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <optional>
#include <unordered_map>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace world::celestial
{

namespace
{

using json = nlohmann::json;

json loadJson(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Cannot open JSON: " + path);

    // Whole file first: parsing from a buffer is several times faster than
    // from the stream in unoptimised builds (megabytes of catalog).
    f.seekg(0, std::ios::end);
    const std::streamoff size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::string text(static_cast<std::size_t>(std::max<std::streamoff>(size, 0)), ' ');
    f.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(std::max<std::streamsize>(f.gcount(), 0)));
    return json::parse(text);
}


namespace fs = std::filesystem;

constexpr std::uint64_t FnvOffsetBasis64 = 14695981039346656037ull;
constexpr std::uint64_t FnvPrime64 = 1099511628211ull;

void hashByte(std::uint64_t& hash, unsigned char value)
{
    hash ^= static_cast<std::uint64_t>(value);
    hash *= FnvPrime64;
}

void hashBytes(std::uint64_t& hash, const std::string& value)
{
    // Same FNV-1a as hashByte, one local loop (the catalog is megabytes and
    // the game also runs unoptimised builds).
    std::uint64_t h = hash;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(value.data());
    const unsigned char* end = p + value.size();
    while (p != end)
    {
        h ^= *p++;
        h *= FnvPrime64;
    }
    hash = h;
}

std::uint64_t computeCatalogFingerprint(const fs::path& root)
{
    std::vector<fs::path> files;

    for (const char* directory : {
            "systems",
            "distant_systems",
            "objects_details"})
    {
        const fs::path catalogDirectory = root / directory;
        std::error_code ec;
        if (!fs::is_directory(catalogDirectory, ec))
            continue;

        for (fs::recursive_directory_iterator it(
                 catalogDirectory,
                 fs::directory_options::skip_permission_denied,
                 ec), end;
             it != end;
             it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }

            if (!it->is_regular_file(ec) || ec)
            {
                ec.clear();
                continue;
            }

            if (it->path().extension() == ".json")
                files.push_back(it->path());
        }
    }

    // Relative paths computed once (sorting with per-comparison path
    // arithmetic costs seconds in unoptimised builds).
    std::vector<std::pair<std::string, fs::path>> ordered;
    ordered.reserve(files.size());
    for (const fs::path& file : files)
        ordered.emplace_back(file.lexically_relative(root).generic_string(), file);
    std::sort(
        ordered.begin(),
        ordered.end(),
        [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; }
    );

    std::uint64_t hash = FnvOffsetBasis64;

    for (const auto& [relative, file] : ordered)
    {

        hashBytes(hash, relative);
        hashByte(hash, 0u);

        std::ifstream input(file, std::ios::binary);
        if (!input.is_open())
        {
            hashBytes(hash, "<unreadable>");
        }
        else
        {
            input.seekg(0, std::ios::end);
            const std::streamoff size = input.tellg();
            input.seekg(0, std::ios::beg);
            std::string bytes(static_cast<std::size_t>(std::max<std::streamoff>(size, 0)), '\0');
            input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            bytes.resize(static_cast<std::size_t>(std::max<std::streamsize>(input.gcount(), 0)));
            hashBytes(hash, bytes);
        }

        hashByte(hash, 0xffu);
    }

    return hash;
}

struct SystemCatalogDocument
{
    fs::path sourcePath;
    StarSystemSummary summary;
    CelestialSystemDefinition definition;
};

std::string normalizeCatalogName(std::string value)
{
    std::string normalized;
    normalized.reserve(value.size());

    bool pendingSpace = false;

    for (unsigned char c : value)
    {
        if (std::isspace(c))
        {
            pendingSpace = !normalized.empty();
            continue;
        }

        if (pendingSpace)
        {
            normalized.push_back(' ');
            pendingSpace = false;
        }

        normalized.push_back(
            static_cast<char>(std::tolower(c))
        );
    }

    return normalized;
}

bool readPositionLy(
    const json& source,
    glm::dvec3& out
)
{
    if (!source.is_object())
        return false;

    const auto numeric =
        [&](const char* key)
        {
            return
                source.contains(key) &&
                source[key].is_number();
        };

    if (!numeric("x") ||
        !numeric("y") ||
        !numeric("z"))
    {
        return false;
    }

    out = glm::dvec3(
        source["x"].get<double>(),
        source["y"].get<double>(),
        source["z"].get<double>()
    );

    return true;
}

bool collectJsonFiles(
    const fs::path& directory,
    bool allowEmpty,
    std::vector<fs::path>& outFiles,
    std::string& error
)
{
    outFiles.clear();

    std::error_code ec;

    if (!fs::exists(directory, ec) ||
        !fs::is_directory(directory, ec))
    {
        error =
            "directory does not exist: " +
            directory.generic_string();
        return false;
    }

    fs::directory_iterator it(directory, ec);
    fs::directory_iterator end;

    if (ec)
    {
        error =
            "cannot enumerate directory: " +
            directory.generic_string() +
            ": " + ec.message();
        return false;
    }

    for (; it != end; it.increment(ec))
    {
        if (ec)
        {
            error =
                "directory iteration failed: " +
                directory.generic_string() +
                ": " + ec.message();
            return false;
        }

        const fs::directory_entry& entry = *it;

        if (!entry.is_regular_file(ec) || ec)
        {
            ec.clear();
            continue;
        }

        const fs::path path = entry.path();

        if (path.extension() == ".json")
            outFiles.push_back(path);
    }

    std::sort(
        outFiles.begin(),
        outFiles.end(),
        [](const fs::path& a, const fs::path& b)
        {
            return a.generic_string() < b.generic_string();
        }
    );

    if (!allowEmpty && outFiles.empty())
    {
        error =
            "no JSON files found in: " +
            directory.generic_string();
        return false;
    }

    return true;
}

bool parsePhysicalSystemDirectory(
    const fs::path& directory,
    StarSystemCatalogScope catalogScope,
    SystemCatalogDocument& out,
    std::string& error
);

struct CatalogLoadReport
{
    std::size_t filesDiscovered = 0;
    std::size_t filesLoaded = 0;
    std::size_t filesSkipped = 0;
    std::vector<std::string> skippedMessages;
};

void addCatalogSkip(
    CatalogLoadReport& report,
    const std::string& message
)
{
    ++report.filesSkipped;
    report.skippedMessages.push_back(message);
}

void addCatalogSkip(
    CatalogLoadReport& report,
    const fs::path& path,
    const std::string& reason
)
{
    addCatalogSkip(
        report,
        path.generic_string() + ": " + reason
    );
}

void printCatalogSkips(
    const char* prefix,
    const char* category,
    const CatalogLoadReport& report
)
{
    for (const std::string& message : report.skippedMessages)
    {
        std::cerr
            << prefix
            << " skipped invalid "
            << category
            << " file: "
            << message
            << "\n";
    }
}

bool loadSystemCatalogDocuments(
    const std::string& galaxyDetailsRoot,
    const char* directoryName,
    StarSystemCatalogScope catalogScope,
    bool allowMissingOrEmpty,
    std::vector<SystemCatalogDocument>& outDocuments,
    CatalogLoadReport& report,
    std::string& fatalError
)
{
    report = CatalogLoadReport {};

    // Physical catalog v2: one directory per system (system.json, stars/,
    // generated/). There is no other runtime schema.
    const fs::path directory =
        fs::path(galaxyDetailsRoot) /
        directoryName;

    if (allowMissingOrEmpty)
    {
        std::error_code existsError;

        if (!fs::exists(directory, existsError))
        {
            if (existsError)
            {
                fatalError =
                    "cannot inspect directory: " +
                    directory.generic_string() +
                    ": " + existsError.message();
                return false;
            }

            outDocuments.clear();
            fatalError.clear();
            return true;
        }
    }

    std::vector<fs::path> files;

    {
        std::error_code ec;
        if (fs::is_directory(directory, ec))
            for (const auto& entry : fs::directory_iterator(directory, ec))
                if (entry.is_directory() && fs::exists(entry.path() / "system.json"))
                    files.push_back(entry.path());
        std::sort(files.begin(), files.end());
        if (files.empty() && !allowMissingOrEmpty)
        {
            fatalError = "no system directories in: " + directory.generic_string();
            return false;
        }
    }

    report.filesDiscovered = files.size();

    outDocuments.clear();
    outDocuments.reserve(files.size());

    std::unordered_map<int, fs::path> idSources;
    std::unordered_map<std::string, fs::path> nameSources;

    for (const fs::path& path : files)
    {
        SystemCatalogDocument document;
        std::string fileError;

        if (!parsePhysicalSystemDirectory(path, catalogScope, document, fileError))
        {
            addCatalogSkip(report, fileError);
            continue;
        }

        const auto existingId =
            idSources.find(document.summary.id);

        if (existingId != idSources.end())
        {
            std::ostringstream reason;
            reason
                << "duplicate system id="
                << document.summary.id
                << "; first valid definition kept from "
                << existingId->second.generic_string();

            addCatalogSkip(report, path, reason.str());
            continue;
        }

        const std::string normalizedName =
            normalizeCatalogName(
                document.summary.name
            );

        const auto existingName =
            nameSources.find(normalizedName);

        if (existingName != nameSources.end())
        {
            std::ostringstream reason;
            reason
                << "duplicate normalized system name '"
                << document.summary.name
                << "'; first valid definition kept from "
                << existingName->second.generic_string();

            addCatalogSkip(report, path, reason.str());
            continue;
        }

        const SystemCatalogDocument* identicalPosition = nullptr;

        for (const SystemCatalogDocument& existingDocument :
             outDocuments)
        {
            const glm::dvec3 delta =
                existingDocument.summary.positionLy -
                document.summary.positionLy;

            const double distanceSquared =
                delta.x * delta.x +
                delta.y * delta.y +
                delta.z * delta.z;

            if (distanceSquared < 1.0e-12)
            {
                identicalPosition = &existingDocument;
                break;
            }
        }

        if (identicalPosition)
        {
            addCatalogSkip(
                report,
                path,
                "position_ly is identical to first valid definition " +
                    identicalPosition->sourcePath.generic_string()
            );
            continue;
        }

        idSources.emplace(
            document.summary.id,
            path
        );

        nameSources.emplace(
            normalizedName,
            path
        );

        outDocuments.push_back(
            std::move(document)
        );
    }

    std::sort(
        outDocuments.begin(),
        outDocuments.end(),
        [](
            const SystemCatalogDocument& a,
            const SystemCatalogDocument& b
        )
        {
            return a.summary.id < b.summary.id;
        }
    );

    report.filesLoaded = outDocuments.size();

    if (!allowMissingOrEmpty && outDocuments.empty())
    {
        std::ostringstream message;
        message
            << "no valid star-system directories found in: "
            << directory.generic_string()
            << " (discovered="
            << report.filesDiscovered
            << ", skipped="
            << report.filesSkipped
            << ")";

        fatalError = message.str();
        return false;
    }

    fatalError.clear();
    return true;
}

void removeDistantCatalogConflicts(
    const std::vector<SystemCatalogDocument>& localDocuments,
    std::vector<SystemCatalogDocument>& distantDocuments,
    CatalogLoadReport& distantReport
)
{
    std::vector<SystemCatalogDocument> accepted;
    accepted.reserve(distantDocuments.size());

    for (SystemCatalogDocument& distant : distantDocuments)
    {
        std::string conflictReason;

        for (const SystemCatalogDocument& local : localDocuments)
        {
            if (distant.summary.id == local.summary.id)
            {
                conflictReason =
                    "system id conflicts with local definition " +
                    local.sourcePath.generic_string();
                break;
            }

            if (normalizeCatalogName(distant.summary.name) ==
                normalizeCatalogName(local.summary.name))
            {
                conflictReason =
                    "system name conflicts with local definition " +
                    local.sourcePath.generic_string();
                break;
            }

            const glm::dvec3 delta =
                local.summary.positionLy -
                distant.summary.positionLy;

            const double distanceSquared =
                delta.x * delta.x +
                delta.y * delta.y +
                delta.z * delta.z;

            if (distanceSquared < 1.0e-12)
            {
                conflictReason =
                    "position_ly conflicts with local definition " +
                    local.sourcePath.generic_string();
                break;
            }
        }

        if (!conflictReason.empty())
        {
            addCatalogSkip(
                distantReport,
                distant.sourcePath,
                conflictReason + "; local definition kept"
            );
            continue;
        }

        accepted.push_back(std::move(distant));
    }

    distantDocuments = std::move(accepted);
    distantReport.filesLoaded = distantDocuments.size();
}

bool parseGalaxyObjectJson(
    const json& root,
    const fs::path& path,
    GalaxyObjectDefinition& out,
    std::string& error
)
{
    try
    {
        const auto fail =
            [&](const std::string& message)
            {
                error =
                    path.generic_string() +
                    ": " + message;
                return false;
            };

        if (!root.is_object())
            return fail("root must be an object");

        if (!root.contains("schema_version") ||
            !root["schema_version"].is_number_integer() ||
            root["schema_version"].get<int>() != 1)
        {
            return fail("unsupported or missing schema_version");
        }

        if (!root.contains("kind") ||
            !root["kind"].is_string() ||
            root["kind"].get<std::string>() != "galaxy_object")
        {
            return fail("kind must be 'galaxy_object'");
        }

        GalaxyObjectDefinition object;

        if (!root.contains("id") ||
            !root["id"].is_string())
        {
            return fail("id must be a string");
        }

        object.id = root["id"].get<std::string>();

        if (object.id.empty())
            return fail("id must not be empty");

        if (!root.contains("name") ||
            !root["name"].is_string())
        {
            return fail("name must be a string");
        }

        object.name = root["name"].get<std::string>();

        if (object.name.empty())
            return fail("name must not be empty");

        if (!root.contains("object_type") ||
            !root["object_type"].is_string())
        {
            return fail("object_type must be a string");
        }

        object.objectType =
            root["object_type"].get<std::string>();

        if (object.objectType.empty())
            return fail("object_type must not be empty");

        if (!root.contains("position_ly") ||
            !readPositionLy(root["position_ly"], object.positionLy))
        {
            return fail("position_ly must contain numeric x, y and z");
        }

        if (root.contains("description"))
        {
            if (!root["description"].is_string())
                return fail("description must be a string");

            object.description =
                root["description"].get<std::string>();
        }

        if (root.contains("tags"))
        {
            if (!root["tags"].is_array())
                return fail("tags must be an array");

            for (const json& tag : root["tags"])
            {
                if (!tag.is_string())
                    return fail("every tag must be a string");

                object.tags.push_back(
                    tag.get<std::string>()
                );
            }
        }

        if (root.contains("properties"))
        {
            if (!root["properties"].is_object())
                return fail("properties must be an object");

            object.properties = root["properties"];
        }

        out = std::move(object);
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error =
            path.generic_string() +
            ": " + e.what();
        return false;
    }
}

/*
    One objects_details file: a galaxy_object document, or an
    interstellar_cell_block (elite.interstellar_cell_block.v1): the 25-ly
    generation cells of one 4 x 4 x 4 block, each holding its background
    medium and compact bodies. Every object of a block is validated like a
    standalone document; an invalid object is dropped alone.
*/
void parseGalaxyObjectFile(
    const fs::path& path,
    std::vector<GalaxyObjectDefinition>& out,
    std::vector<std::string>& errors
)
{
    json root;
    try
    {
        root = loadJson(path.string());
    }
    catch (const std::exception& e)
    {
        errors.push_back(path.generic_string() + ": " + e.what());
        return;
    }

    if (root.is_object() &&
        root.value("kind", std::string()) == "interstellar_cell_block")
    {
        if (root.value("schema", std::string()) != "elite.interstellar_cell_block.v1" ||
            !root.contains("cells") || !root["cells"].is_array())
        {
            errors.push_back(path.generic_string() + ": unsupported interstellar_cell_block document");
            return;
        }
        for (const json& cell : root["cells"])
        {
            if (!cell.is_object() || !cell.contains("objects") || !cell["objects"].is_array())
            {
                errors.push_back(path.generic_string() + ": interstellar cell without objects");
                continue;
            }
            for (const json& item : cell["objects"])
            {
                GalaxyObjectDefinition object;
                std::string error;
                if (parseGalaxyObjectJson(item, path, object, error))
                    out.push_back(std::move(object));
                else
                    errors.push_back(error);
            }
        }
        return;
    }

    GalaxyObjectDefinition object;
    std::string error;
    if (parseGalaxyObjectJson(root, path, object, error))
        out.push_back(std::move(object));
    else
        errors.push_back(error);
}

bool loadGalaxyObjects(
    const std::string& galaxyDetailsRoot,
    std::vector<GalaxyObjectDefinition>& outObjects,
    CatalogLoadReport& report,
    std::string& fatalError
)
{
    report = CatalogLoadReport {};

    const fs::path directory =
        fs::path(galaxyDetailsRoot) /
        "objects_details";

    std::error_code directoryError;

    if (!fs::exists(directory, directoryError))
    {
        if (directoryError)
        {
            fatalError =
                "cannot inspect directory: " +
                directory.generic_string() +
                ": " + directoryError.message();
            return false;
        }

        outObjects.clear();
        fatalError.clear();
        return true;
    }

    std::vector<fs::path> files;

    if (!collectJsonFiles(
            directory,
            true,
            files,
            fatalError))
    {
        return false;
    }

    // Generated interstellar content: one file per 25-ly generation cell and
    // one per medium structure (EliteUniverseGenerator interstellar).
    for (const char* generated : {"interstellar_cells", "interstellar_medium"})
    {
        std::error_code generatedError;
        if (!fs::is_directory(directory / generated, generatedError))
            continue;
        std::vector<fs::path> generatedFiles;
        std::string generatedListError;
        if (collectJsonFiles(directory / generated, true, generatedFiles, generatedListError))
            files.insert(files.end(), generatedFiles.begin(), generatedFiles.end());
    }

    report.filesDiscovered = files.size();

    outObjects.clear();
    outObjects.reserve(files.size());

    std::unordered_map<std::string, fs::path> idSources;

    for (const fs::path& path : files)
    {
        std::vector<GalaxyObjectDefinition> parsed;
        std::vector<std::string> fileErrors;

        parseGalaxyObjectFile(
            path,
            parsed,
            fileErrors
        );

        for (const std::string& fileError : fileErrors)
            addCatalogSkip(report, fileError);

        for (GalaxyObjectDefinition& object : parsed)
        {
            const auto existing =
                idSources.find(object.id);

            if (existing != idSources.end())
            {
                addCatalogSkip(
                    report,
                    path,
                    "duplicate galaxy object id='" +
                        object.id +
                        "'; first valid definition kept from " +
                        existing->second.generic_string()
                );
                continue;
            }

            idSources.emplace(object.id, path);
            outObjects.push_back(std::move(object));
        }
    }

    std::sort(
        outObjects.begin(),
        outObjects.end(),
        [](
            const GalaxyObjectDefinition& a,
            const GalaxyObjectDefinition& b
        )
        {
            return a.id < b.id;
        }
    );

    report.filesLoaded = outObjects.size();
    fatalError.clear();
    return true;
}

void readAlternativeNames(
    const json& src,
    CelestialBodyDefinition& out
)
{
    if (!src.contains("alternative_names") || !src["alternative_names"].is_array())
        return;

    for (const auto& n : src["alternative_names"])
    {
        CelestialBodyDisplayName displayName;

        if (n.is_string())
        {
            displayName.name = n.get<std::string>();
        }
        else if (n.is_object())
        {
            displayName.name = n.value("name", "");

            if (n.contains("actors") && n["actors"].is_array())
            {
                for (const auto& actor : n["actors"])
                {
                    if (actor.is_string())
                        displayName.actors.push_back(actor.get<std::string>());
                }
            }
        }

        if (!displayName.name.empty())
            out.alternativeNames.push_back(std::move(displayName));
    }

}




void readOrientationFields(
    const json& j,
    world::celestial::CelestialBodyDefinition& body
)
{
    const auto directionSign =
        [&](const char* key) -> int
        {
            if (!j.contains(key))
                return 1;

            const json& value =
                j[key];

            if (value.is_number_integer())
            {
                return
                    value.get<int>() < 0
                        ? -1
                        : 1;
            }

            if (value.is_string())
            {
                const std::string text =
                    value.get<std::string>();

                if (text == "retrograde" ||
                    text == "reverse" ||
                    text == "-1")
                {
                    return -1;
                }
            }

            return 1;
        };

    body.orbitalDirection =
        directionSign(
            "orbit_direction"
        );

    body.rotationDirection =
        directionSign(
            "rotation_direction"
        );

    body.orbitalPhaseOffsetDeg =
        j.value(
            "orbit_phase_offset_deg",
            0.0
        );

    body.axialTiltDeg =
        j.value("axial_tilt_deg", 0.0);

    body.axisNodeDeg =
        j.value("axis_node_deg", 0.0);

    body.rotationOffsetDeg =
        j.value("rotation_offset_deg", 0.0);

    body.textureLongitudeOffsetDeg =
        j.value("texture_longitude_offset_deg", 0.0);
}






CelestialRingDisplayMode readRingDisplayMode(
    const std::string& value
)
{
    if (value == "particle_cloud")
    {
        return
            CelestialRingDisplayMode::
                ParticleCloud;
    }

    return
        CelestialRingDisplayMode::
            LayeredBands;
}

CelestialRingVisibilityClass
readRingVisibilityClass(
    const std::string& value
)
{
    if (value == "main")
    {
        return
            CelestialRingVisibilityClass::
                Main;
    }

    if (value == "secondary")
    {
        return
            CelestialRingVisibilityClass::
                Secondary;
    }

    if (value == "diffuse")
    {
        return
            CelestialRingVisibilityClass::
                Diffuse;
    }

    return
        CelestialRingVisibilityClass::
            Faint;
}

glm::vec2 readRingVec2(
    const json& object,
    const std::string& key,
    const glm::vec2& fallback
)
{
    if (!object.is_object() ||
        !object.contains(key))
    {
        return fallback;
    }

    const json& value =
        object[key];

    if (!value.is_array() ||
        value.size() < 2 ||
        !value[0].is_number() ||
        !value[1].is_number())
    {
        return fallback;
    }

    return glm::vec2(
        value[0].get<float>(),
        value[1].get<float>()
    );
}








glm::vec3 readRingTint(
    const json& renderObject,
    const glm::vec3& fallback
)
{
    if (!renderObject.is_object() ||
        !renderObject.contains("tint_rgb"))
    {
        return fallback;
    }

    const json& value =
        renderObject["tint_rgb"];

    if (!value.is_array() ||
        value.size() < 3 ||
        !value[0].is_number() ||
        !value[1].is_number() ||
        !value[2].is_number())
    {
        return fallback;
    }

    return glm::vec3(
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>()
    );
}

CelestialRingDefinition readRingBand(
    const json& source
)
{
    CelestialRingDefinition ring;

    ring.name =
        source.value(
            "name",
            "Ring"
        );

    ring.composition =
        source.value(
            "composition",
            ""
        );

    if (source.contains("distance_from_planet_km") &&
        source["distance_from_planet_km"].is_object())
    {
        const json& distances =
            source["distance_from_planet_km"];

        ring.innerRadiusKm =
            distances.value(
                "inner",
                0.0
            );

        ring.outerRadiusKm =
            distances.value(
                "outer",
                0.0
            );
    }

    const json render =
        source.value(
            "render",
            json::object()
        );

    ring.render.tint =
        readRingTint(
            render,
            ring.render.tint
        );

    ring.render.opacity =
        std::clamp(
            render.value(
                "opacity",
                ring.render.opacity
            ),
            0.0f,
            1.0f
        );

    ring.render.opticalDepth =
        std::max(
            0.0f,
            render.value(
                "optical_depth",
                ring.render.opticalDepth
            )
        );

    ring.render.radialNoiseStrength =
        std::clamp(
            render.value(
                "radial_noise_strength",
                ring.render.radialNoiseStrength
            ),
            0.0f,
            1.0f
        );

    ring.render.radialBrightnessVariation =
        std::clamp(
            render.value(
                "radial_brightness_variation",
                ring.render.radialBrightnessVariation
            ),
            0.0f,
            1.0f
        );

    ring.render.azimuthalAsymmetry =
        std::clamp(
            render.value(
                "azimuthal_asymmetry",
                ring.render.azimuthalAsymmetry
            ),
            0.0f,
            1.0f
        );

    ring.render.edgeSoftness =
        std::clamp(
            render.value(
                "edge_softness",
                ring.render.edgeSoftness
            ),
            0.001f,
            0.49f
        );





        ring.render.visibilityClass =
            readRingVisibilityClass(
                render.value(
                    "visibility_class",
                    "faint"
                )
            );

        ring.render.displayMode =
            readRingDisplayMode(
                render.value(
                    "display_mode",
                    "layered_band"
                ) == "particle_cloud"
                    ? "particle_cloud"
                    : "layered_bands"
            );

        ring.render.visualOpacityScale =
            std::max(
                0.0f,
                render.value(
                    "visual_opacity_scale",
                    1.0f
                )
            );

        ring.render.radialStructureScale =
            std::max(
                0.0f,
                render.value(
                    "radial_structure_scale",
                    1.0f
                )
            );

        ring.render.particleDensityScale =
            std::max(
                0.0f,
                render.value(
                    "particle_density_scale",
                    1.0f
                )
            );

        ring.render.particleClumpiness =
            std::clamp(
                render.value(
                    "particle_clumpiness",
                    0.4f
                ),
                0.0f,
                1.0f
            );

        ring.render.particleRadialJitter =
            std::clamp(
                render.value(
                    "particle_radial_jitter",
                    0.25f
                ),
                0.0f,
                1.0f
            );

        ring.render.particleSizePxRange =
            readRingVec2(
                render,
                "particle_size_px_range",
                glm::vec2(
                    0.5f,
                    1.3f
                )
            );








    ring.render.castsShadow =
        render.value(
            "casts_shadow",
            ring.render.castsShadow
        );

    return ring;
}

void readRings(
    const json& src,
    CelestialBodyDefinition& body
)
{
    body.rings.clear();

    /*
        Новый подробный формат имеет приоритет.
    */
    if (src.contains("ring_system") &&
        src["ring_system"].is_object())
    {
        const json& ringSystem =
            src["ring_system"];

        const json visual =
    ringSystem.value(
        "visual",
        json::object()
    );

        body.ringVisual.displayProfile =
            visual.value(
                "display_profile",
                ""
            );

        body.ringVisual.renderMode =
            readRingDisplayMode(
                visual.value(
                    "render_mode",
                    "layered_bands"
                )
            );

        body.ringVisual.recognizabilityPriority =
            visual.value(
                "recognizability_priority",
                0.5f
            );

        body.ringVisual.artisticWidthScale =
            visual.value(
                "artistic_width_scale",
                1.0f
            );

        body.ringVisual.mainBandEmphasis =
            visual.value(
                "main_band_emphasis",
                1.0f
            );

        body.ringVisual.secondaryBandEmphasis =
            visual.value(
                "secondary_band_emphasis",
                1.0f
            );

        body.ringVisual.faintBandEmphasis =
            visual.value(
                "faint_band_emphasis",
                1.0f
            );

        body.ringVisual.diffuseBandEmphasis =
            visual.value(
                "diffuse_band_emphasis",
                1.0f
            );

        body.ringVisual.gapContrast =
            visual.value(
                "gap_contrast",
                1.0f
            );

        body.ringVisual.radialStructureStrength =
            visual.value(
                "radial_structure_strength",
                0.0f
            );

        body.ringVisual.fineStructureStrength =
            visual.value(
                "fine_structure_strength",
                0.0f
            );

        body.ringVisual.edgeSoftnessScale =
            visual.value(
                "edge_softness_scale",
                1.0f
            );

        body.ringVisual.brightnessVariation =
            visual.value(
                "brightness_variation",
                0.0f
            );

        body.ringVisual.minimumVisibleWidthPx =
            visual.value(
                "minimum_visible_width_px",
                0.5f
            );

        body.ringVisual.minimumMainBandWidthPx =
            visual.value(
                "minimum_main_band_width_px",
                1.0f
            );

        body.ringVisual.continuousFill =
            visual.value(
                "continuous_fill",
                0.0f
            );

        body.ringVisual.particleDensity =
            visual.value(
                "particle_density",
                0.3f
            );

        body.ringVisual.particleOpacityScale =
            visual.value(
                "particle_opacity_scale",
                0.4f
            );

        body.ringVisual.particleSizePxRange =
            readRingVec2(
                visual,
                "particle_size_px_range",
                glm::vec2(
                    0.5f,
                    1.3f
                )
            );

        body.ringVisual.radialJitter =
            visual.value(
                "radial_jitter",
                0.25f
            );

        body.ringVisual.azimuthalClumping =
            visual.value(
                "azimuthal_clumping",
                0.4f
            );

        body.ringVisual.clusterScale =
            visual.value(
                "cluster_scale",
                0.7f
            );

        body.ringVisual.softness =
            visual.value(
                "softness",
                0.65f
            );

        const json artisticOcclusion =
            visual.value(
                "artistic_occlusion",
                json::object()
            );

        body.ringVisual.artisticOcclusionEnabled =
            artisticOcclusion.value(
                "enabled",
                false
            );

        body.ringVisual.backHalfBrightness =
            artisticOcclusion.value(
                "back_half_brightness",
                1.0f
            );

        body.ringVisual.innerEdgeDarkening =
            artisticOcclusion.value(
                "inner_edge_darkening",
                0.0f
            );

        const json plane =
            ringSystem.value(
                "plane",
                json::object()
            );

        body.ringPlaneMode =
            plane.value(
                "mode",
                "planet_equatorial"
            );

        body.ringPlaneInclinationOffsetDeg =
            plane.value(
                "inclination_offset_deg",
                0.0
            );

        if (ringSystem.contains("bands") &&
            ringSystem["bands"].is_array())
        {
            for (const json& sourceBand :
                 ringSystem["bands"])
            {
                if (!sourceBand.is_object())
                    continue;

                CelestialRingDefinition ring =
                    readRingBand(
                        sourceBand
                    );

                if (ring.outerRadiusKm >
                        ring.innerRadiusKm &&
                    ring.outerRadiusKm > 0.0)
                {
                    body.rings.push_back(
                        std::move(ring)
                    );
                }
            }
        }

        /*
            Если подробные bands успешно прочитаны,
            старый общий объект rings игнорируем.
        */
        if (!body.rings.empty())
            return;
    }

    /*
        Legacy fallback.
    */
    if (!src.contains("rings") ||
        !src["rings"].is_array())
    {
        return;
    }

    for (const json& sourceRing :
         src["rings"])
    {
        if (!sourceRing.is_object())
            continue;

        CelestialRingDefinition ring =
            readRingBand(
                sourceRing
            );

        if (ring.outerRadiusKm >
                ring.innerRadiusKm &&
            ring.outerRadiusKm > 0.0)
        {
            body.rings.push_back(
                std::move(ring)
            );
        }
    }
}







/*
    Physical catalog (src/world/generation): one directory per system,
        <dir>/system.json                 identity, position, stellar hierarchy
        <dir>/generated/system.json       present-day stars, barycenters
        <dir>/generated/bodies/*.json     planets, dwarf planets, moons
        <dir>/generated/reservoirs/*.json small-body populations
    This is the only runtime catalog schema: the directory is validated
    (readPhysicalSystemSummary) and becomes a runtime definition with
    Keplerian orbits. Migration from the legacy flat catalog lives in
    tools/universe_generator (build_stellar_inputs.py), not in the runtime.
*/

std::string joinSpectralTypes(const json& stars)
{
    std::string out;
    for (const auto& s : stars)
    {
        if (!out.empty())
            out += " + ";
        out += s.value("spectral_type", std::string("?"));
    }
    return out;
}

void readKeplerOrbit(const json& orbit, CelestialBodyDefinition& body)
{
    body.keplerOrbit = true;
    body.distanceAu = orbit.value("semi_major_axis_au", 0.0);
    body.orbitalPeriodDays = orbit.value("period_days", 0.0);
    body.eccentricity = orbit.value("eccentricity", 0.0);
    body.inclinationDeg = orbit.value("inclination_deg", 0.0);
    body.longitudeAscendingNodeDeg = orbit.value("longitude_ascending_node_deg", 0.0);
    body.argumentOfPeriapsisDeg = orbit.value("argument_of_periapsis_deg", 0.0);
    body.meanAnomalyAtEpochDeg = orbit.value("mean_anomaly_at_epoch_deg", 0.0);
    body.orbitEpochSeconds = orbit.value("epoch_universe_seconds", 0.0);
    body.orbitalDirection = orbit.value("direction", 1) < 0 ? -1 : 1;
}

// Render bands for a generated ring system, in the ring_system schema the
// renderer already reads; gaps split a band.
json ringSystemFromGenerated(const json& rings)
{
    json bands = json::array();
    for (const auto& r : rings)
    {
        const double tau = r.value("optical_depth", 0.0);
        const double ice = r.value("ice_fraction", 0.0);
        const double dust = r.value("dust_fraction", 0.0);
        const std::string vis = r.value("visibility_class", std::string("secondary"));
        const double opacity = std::max(0.02, std::min(0.9, 1.0 - std::exp(-tau)));
        json tint = ice > 0.6 ? json::array({0.90, 0.87, 0.80})
                  : dust > 0.5 ? json::array({0.62, 0.57, 0.52}) : json::array({0.56, 0.50, 0.44});
        std::vector<std::pair<double, double>> spans {{r.value("inner_radius_km", 0.0), r.value("outer_radius_km", 0.0)}};
        if (r.contains("gaps_km"))
            for (const auto& g : r["gaps_km"])
            {
                std::vector<std::pair<double, double>> next;
                for (const auto& [lo, hi] : spans)
                {
                    const double g0 = g[0].get<double>(), g1 = g[1].get<double>();
                    if (g1 <= lo || g0 >= hi) { next.push_back({lo, hi}); continue; }
                    if (g0 > lo) next.push_back({lo, g0});
                    if (g1 < hi) next.push_back({g1, hi});
                }
                spans = next;
            }
        int part = 0;
        for (const auto& [lo, hi] : spans)
        {
            json band;
            band["name"] = r.value("name", std::string("ring")) + (spans.size() > 1 ? " " + std::to_string(++part) : "");
            band["distance_from_planet_km"] = {{"inner", lo}, {"outer", hi}};
            band["composition"] = ice > 0.6 ? "water ice" : dust > 0.5 ? "dust" : "rock and dust";
            band["render"] = {{"role", "ring"}, {"tint_rgb", tint}, {"opacity", opacity}, {"optical_depth", tau},
                              {"visibility_class", vis}, {"display_mode", "layered_band"},
                              {"casts_shadow", tau > 0.1}, {"edge_softness", vis == "faint" ? 0.3 : 0.05}};
            bands.push_back(band);
        }
    }
    return json {{"plane", {{"mode", "planet_equatorial"}, {"normal_source", "planet_rotation_axis"},
                             {"inclination_offset_deg", 0.0}}},
                 {"data_status", "generated"}, {"bands", bands}};
}

CelestialBodyDefinition bodyFromGenerated(const json& b)
{
    CelestialBodyDefinition body;
    body.id = b.at("id").get<std::string>();
    body.name = b.value("designation", body.id);
    const std::string kind = b.value("kind", std::string("planet"));
    body.type = kind == "moon" ? BodyType::Moon : BodyType::Planet;
    body.parentId = b.value("parent_id", std::string());
    const json& phys = b.at("physical");
    body.massKg = phys.value("mass_kg", 0.0);
    body.gravitationalParameterM3s2 = phys.value("mu_m3s2", GravitationalConstant * body.massKg);
    body.radiusKm = phys.value("radius_km", 0.0);
    body.diameterKm = 2.0 * body.radiusKm;
    body.bodyClass = phys.value("body_class", std::string());
    body.observed = b.value("origin", std::string("generated")) != "generated";
    body.presentationLevel = kind == "moon" ? "moon" : kind == "dwarf_planet" ? "dwarf_planet" : "";
    readKeplerOrbit(b.at("orbit"), body);
    body.orbitInParentEquator = kind == "moon";
    body.environmentPresetId = b.value("environment_preset_id", std::string());
    if (b.contains("atmosphere"))
        body.atmosphereOuterRadiusKm = b["atmosphere"].value("outer_radius_km", 0.0);

    // Orientation through the shared orientation reader; authored visual keys win.
    const json spin = b.value("spin", json::object());
    json orientation = {
        {"axial_tilt_deg", spin.value("obliquity_deg", 0.0)},
        {"axis_node_deg", spin.value("axis_node_deg", 0.0)},
        {"rotation_direction", spin.value("rotation_direction", 1)},
        {"orbit_direction", body.orbitalDirection}};
    const json extra = b.value("extra", json::object());
    for (const char* key : {"axis_node_deg", "rotation_offset_deg", "texture_longitude_offset_deg",
                            "orbit_phase_offset_deg", "rotation_direction"})
        if (extra.contains(key))
            orientation[key] = extra[key];
    readOrientationFields(orientation, body);
    body.dayLengthHours = spin.value("rotation_period_hours", 0.0);
    if (extra.contains("environment_preset_id") && extra["environment_preset_id"].is_string())
        body.environmentPresetId = extra["environment_preset_id"].get<std::string>();
    readAlternativeNames(b, body);

    if (extra.contains("ring_system") && extra["ring_system"].is_object())
        readRings(json {{"ring_system", extra["ring_system"]}}, body);
    else if (b.contains("rings") && b["rings"].is_array() && !b["rings"].empty())
        readRings(json {{"ring_system", ringSystemFromGenerated(b["rings"])}}, body);
    return body;
}

bool readPhysicalSystemSummary(
    const json& sys,
    const json& gen,
    const fs::path& path,
    StarSystemCatalogScope catalogScope,
    SystemCatalogDocument& out,
    std::string& error)
{
    const auto fail = [&](const std::string& message) {
        error = path.generic_string() + ": " + message;
        return false;
    };
    if (!sys.contains("id") || !sys["id"].is_number_integer())
        return fail("id must be an integer");
    out.summary.id = sys["id"].get<int>();
    if (out.summary.id < 0)
        return fail("id must be non-negative");
    if (catalogScope == StarSystemCatalogScope::Local && out.summary.id >= 100000)
        return fail("local system id must be below 100000");
    if (catalogScope == StarSystemCatalogScope::Distant && out.summary.id < 100000)
        return fail("distant system id must be at least 100000");
    if (!sys.contains("name") || !sys["name"].is_string() || sys["name"].get<std::string>().empty())
        return fail("name must be a non-empty string");
    out.summary.name = sys["name"].get<std::string>();
    if (!sys.contains("position_ly") || !readPositionLy(sys["position_ly"], out.summary.positionLy))
        return fail("position_ly must contain numeric x, y and z");
    const std::string expectedScope = catalogScope == StarSystemCatalogScope::Distant ? "distant" : "local";
    if (sys.value("catalog_scope", std::string("local")) != expectedScope)
        return fail("catalog_scope does not match the containing directory");
    out.summary.catalogScope = catalogScope;
    out.summary.atlasVisible = sys.value("atlas_visible", catalogScope == StarSystemCatalogScope::Local);
    out.summary.routeTarget = sys.value("route_target", true);
    out.summary.questRole = sys.value("quest_role", std::string());
    if (catalogScope == StarSystemCatalogScope::Distant && out.summary.atlasVisible)
        return fail("distant systems must set atlas_visible=false in the current atlas");
    out.summary.starsCount = static_cast<int>(gen["stars"].size());
    out.summary.starType = joinSpectralTypes(gen["stars"]);
    out.sourcePath = path;
    return true;
}

bool parsePhysicalSystemDirectory(
    const fs::path& directory,
    StarSystemCatalogScope catalogScope,
    SystemCatalogDocument& out,
    std::string& error)
{
    json sys, gen;
    try
    {
        sys = loadJson((directory / "system.json").string());
        gen = loadJson((directory / "generated" / "system.json").string());
    }
    catch (const std::exception& e)
    {
        error = directory.generic_string() + ": " + e.what();
        return false;
    }
    if (sys.value("schema", std::string()) != "elite.star_system.v2" ||
        gen.value("schema", std::string()) != "elite.generated_system.v1")
    {
        error = directory.generic_string() + ": unsupported physical catalog schema";
        return false;
    }
    if (!gen.contains("stars") || !gen["stars"].is_array() || gen["stars"].empty())
    {
        error = directory.generic_string() + ": generated/system.json has no stars";
        return false;
    }
    if (gen.value("system_id", -1) != sys.value("id", -2))
    {
        error = directory.generic_string() + ": generated system_id does not match system.json id";
        return false;
    }

    if (!readPhysicalSystemSummary(sys, gen, directory / "system.json", catalogScope, out, error))
        return false;

    CelestialSystemDefinition def;
    def.systemId = out.summary.id;
    def.name = out.summary.name;
    std::unordered_map<std::string, std::string> nodeBodyId;
    for (const auto& n : gen.value("barycenters", json::array()))
        nodeBodyId[n.value("id", std::string())] = n.value("body_id", std::string());
    for (const auto& s : gen["stars"])
        nodeBodyId[s.value("key", std::string())] = s.value("id", std::string());
    auto parentBody = [&](const json& j) {
        const std::string node = j.value("parent_node", std::string());
        const auto it = nodeBodyId.find(node);
        return node.empty() || it == nodeBodyId.end() ? std::string() : it->second;
    };
    std::unordered_map<std::string, double> massKgById;

    // Barycenters (generator lists parents first), then stars.
    for (const auto& n : gen.value("barycenters", json::array()))
    {
        CelestialBodyDefinition body;
        body.id = n.value("body_id", std::string());
        body.name = n.value("id", std::string());
        body.type = BodyType::Barycenter;
        body.parentId = parentBody(n);
        body.massKg = n.value("mass_solar", 0.0) * 1.98847e30;
        body.gravitationalParameterM3s2 = GravitationalConstant * body.massKg;
        if (n.contains("orbit"))
            readKeplerOrbit(n["orbit"], body);
        massKgById[body.id] = body.massKg;
        def.bodies.push_back(std::move(body));
    }
    for (const auto& s : gen["stars"])
    {
        CelestialBodyDefinition body;
        body.id = s.value("id", std::string());
        body.name = s.value("name", std::string("Star"));
        body.type = BodyType::Star;
        body.presentationLevel = "star";
        body.parentId = parentBody(s);
        body.massKg = s.value("mass_solar", 1.0) * 1.98847e30;
        body.gravitationalParameterM3s2 = s.value("mu_m3s2", GravitationalConstant * body.massKg);
        body.radiusKm = s.value("radius_solar", 1.0) * 695700.0;
        body.diameterKm = 2.0 * body.radiusKm;
        body.dayLengthHours = s.value("rotation_period_days", 0.0) * 24.0;
        if (s.contains("orbit"))
            readKeplerOrbit(s["orbit"], body);
        readAlternativeNames(s, body);
        massKgById[body.id] = body.massKg;
        def.bodies.push_back(std::move(body));
    }

    // Bodies, one file each; an invalid body file drops only that body.
    std::vector<fs::path> files;
    std::string listError;
    collectJsonFiles(directory / "generated" / "bodies", true, files, listError);
    std::vector<CelestialBodyDefinition> planets, moons;
    for (const auto& f : files)
    {
        try
        {
            const json b = loadJson(f.string());
            if (b.value("schema", std::string()) != "elite.generated_body.v1")
                throw std::runtime_error("unsupported body schema");
            CelestialBodyDefinition body = bodyFromGenerated(b);
            (body.type == BodyType::Moon ? moons : planets).push_back(std::move(body));
        }
        catch (const std::exception& e)
        {
            std::cerr << "[StarAtlasDatabase] skipped invalid body file " << f.generic_string() << ": "
                      << e.what() << "\n";
        }
    }
    const auto byOrbit = [](const CelestialBodyDefinition& a, const CelestialBodyDefinition& b) {
        return a.parentId != b.parentId ? a.parentId < b.parentId
             : a.distanceAu != b.distanceAu ? a.distanceAu < b.distanceAu : a.id < b.id;
    };
    std::sort(planets.begin(), planets.end(), byOrbit);
    std::sort(moons.begin(), moons.end(), byOrbit);
    std::unordered_map<std::string, bool> known;
    for (const auto& b : def.bodies) known[b.id] = true;
    for (auto& b : planets)
    {
        if (b.presentationLevel.empty())
        {
            const auto central = massKgById.find(b.parentId);
            const double mStar = central != massKgById.end() ? central->second / 1.98847e30 : 1.0;
            const double mEarth = b.massKg / 5.9722e24;
            const double pi = b.distanceAu > 0.0 && mStar > 0.0
                ? 807.0 * mEarth / (std::pow(mStar, 2.5) * std::pow(b.distanceAu, 1.125))
                : 0.0;
            b.presentationLevel = pi < 1.0 ? "dwarf_planet"
                                : mEarth >= 0.3 || b.observed ? "major_planet" : "minor_planet";
        }
        if (!known.count(b.parentId))
        {
            std::cerr << "[StarAtlasDatabase] skipped body " << b.id << ": unknown parent " << b.parentId << "\n";
            continue;
        }
        known[b.id] = true;
        def.bodies.push_back(std::move(b));
    }
    for (auto& b : moons)
    {
        if (!known.count(b.parentId))
        {
            std::cerr << "[StarAtlasDatabase] skipped moon " << b.id << ": unknown parent " << b.parentId << "\n";
            continue;
        }
        def.bodies.push_back(std::move(b));
    }

    // Belts shown on maps: asteroid / outer belts and debris disks.
    files.clear();
    collectJsonFiles(directory / "generated" / "reservoirs", true, files, listError);
    for (const auto& f : files)
    {
        try
        {
            const json r = loadJson(f.string());
            const std::string kind = r.value("kind", std::string());
            if (kind != "asteroid_belt" && kind != "kuiper_belt" && kind != "debris_disk")
                continue;
            CelestialBodyDefinition body;
            body.id = r.at("id").get<std::string>();
            body.name = r.value("name", body.id);
            body.type = BodyType::AsteroidBelt;
            body.parentId = r.value("parent_id", std::string());
            body.distanceAu = r.value("peak_au", 0.0);
            // Significant belts (main asteroid belt class and above) stay on
            // the far-zoom map; trojan swarms and thin belts appear closer in.
            body.presentationLevel =
                r.value("total_mass_earth", 0.0) >= 1.0e-4 ? "major_reservoir" : "minor_reservoir";
            const auto parentMass = massKgById.find(body.parentId);
            if (parentMass != massKgById.end() && body.distanceAu > 0.0)
            {
                const double a = body.distanceAu * MetersPerAu;
                body.orbitalPeriodDays = 6.283185307179586 *
                    std::sqrt(a * a * a / (GravitationalConstant * parentMass->second)) / SecondsPerDay;
            }
            if (known.count(body.parentId))
                def.bodies.push_back(std::move(body));
        }
        catch (const std::exception& e)
        {
            std::cerr << "[StarAtlasDatabase] skipped invalid reservoir file " << f.generic_string() << ": "
                      << e.what() << "\n";
        }
    }
    out.definition = std::move(def);
    return true;
}

} // namespace

bool StarAtlasDatabase::loadSystemSummariesFromDirectory(
    const std::string& galaxyDetailsRoot,
    std::vector<StarSystemSummary>& outSystems,
    std::string* errorMessage
)
{
    std::vector<SystemCatalogDocument> documents;
    CatalogLoadReport report;
    std::string fatalError;

    if (!loadSystemCatalogDocuments(
            galaxyDetailsRoot,
            "systems",
            StarSystemCatalogScope::Local,
            false,
            documents,
            report,
            fatalError))
    {
        printCatalogSkips(
            "[GalaxyCatalog]",
            "local system",
            report
        );

        if (errorMessage)
            *errorMessage = fatalError;

        return false;
    }

    printCatalogSkips(
        "[GalaxyCatalog]",
        "local system",
        report
    );

    outSystems.clear();
    outSystems.reserve(documents.size());

    for (const SystemCatalogDocument& document : documents)
        outSystems.push_back(document.summary);

    if (report.filesSkipped > 0)
    {
        std::cerr
            << "[GalaxyCatalog] local systems loaded="
            << outSystems.size()
            << " skipped="
            << report.filesSkipped
            << " root="
            << galaxyDetailsRoot
            << "\n";
    }

    if (errorMessage)
        errorMessage->clear();

    return !outSystems.empty();
}

bool StarAtlasDatabase::load(
    const std::string& galaxyDetailsRoot
)
{
    std::vector<SystemCatalogDocument> localDocuments;
    std::vector<SystemCatalogDocument> distantDocuments;
    std::vector<GalaxyObjectDefinition> objects;

    CatalogLoadReport localReport;
    CatalogLoadReport distantReport;
    CatalogLoadReport objectReport;
    CatalogLoadReport buildReport;

    std::string fatalError;

    if (!loadSystemCatalogDocuments(
            galaxyDetailsRoot,
            "systems",
            StarSystemCatalogScope::Local,
            false,
            localDocuments,
            localReport,
            fatalError))
    {
        printCatalogSkips(
            "[StarAtlasDatabase]",
            "local system",
            localReport
        );

        std::cerr
            << "[StarAtlasDatabase] "
            << fatalError
            << "\n";
        return false;
    }

    printCatalogSkips(
        "[StarAtlasDatabase]",
        "local system",
        localReport
    );

    if (!loadSystemCatalogDocuments(
            galaxyDetailsRoot,
            "distant_systems",
            StarSystemCatalogScope::Distant,
            true,
            distantDocuments,
            distantReport,
            fatalError))
    {
        std::cerr
            << "[StarAtlasDatabase] distant catalog unavailable; "
            << "continuing without distant systems: "
            << fatalError
            << "\n";

        distantDocuments.clear();
        distantReport = CatalogLoadReport {};
    }
    else
    {
        removeDistantCatalogConflicts(
            localDocuments,
            distantDocuments,
            distantReport
        );

        printCatalogSkips(
            "[StarAtlasDatabase]",
            "distant system",
            distantReport
        );
    }

    if (!loadGalaxyObjects(
            galaxyDetailsRoot,
            objects,
            objectReport,
            fatalError))
    {
        std::cerr
            << "[StarAtlasDatabase] object catalog unavailable; "
            << "continuing without galaxy objects: "
            << fatalError
            << "\n";

        objects.clear();
        objectReport = CatalogLoadReport {};
    }
    else
    {
        printCatalogSkips(
            "[StarAtlasDatabase]",
            "galaxy object",
            objectReport
        );
    }

    std::vector<StarSystemSummary> systems;
    std::vector<StarSystemSummary> distantSystems;
    std::unordered_map<int, CelestialSystemDefinition> details;

    systems.reserve(localDocuments.size());
    distantSystems.reserve(distantDocuments.size());
    details.reserve(
        localDocuments.size() +
        distantDocuments.size()
    );

    const auto buildDocuments =
        [&](const std::vector<SystemCatalogDocument>& documents,
            std::vector<StarSystemSummary>& summaries)
        {
            for (const SystemCatalogDocument& document : documents)
            {
                try
                {
                    CelestialSystemDefinition system =
                        document.definition;

                    const auto inserted =
                        details.emplace(
                            system.systemId,
                            std::move(system)
                        );

                    if (!inserted.second)
                    {
                        addCatalogSkip(
                            buildReport,
                            document.sourcePath,
                            "system id already exists in the runtime catalog; "
                            "first valid definition kept"
                        );
                        continue;
                    }

                    summaries.push_back(document.summary);
                }
                catch (const std::exception& e)
                {
                    addCatalogSkip(
                        buildReport,
                        document.sourcePath,
                        std::string(
                            "failed to build runtime definition: "
                        ) + e.what()
                    );
                }
            }
        };

    buildDocuments(localDocuments, systems);
    buildDocuments(distantDocuments, distantSystems);

    printCatalogSkips(
        "[StarAtlasDatabase]",
        "runtime system",
        buildReport
    );

    if (systems.empty())
    {
        std::cerr
            << "[StarAtlasDatabase] no valid local systems remained "
            << "after per-file validation; catalog not published\n";
        return false;
    }

    const bool hasSol =
        std::any_of(
            systems.begin(),
            systems.end(),
            [](const StarSystemSummary& summary)
            {
                return summary.id == 0;
            }
        );

    if (!hasSol)
    {
        std::cerr
            << "[StarAtlasDatabase] WARNING: local system id=0 (Sol) "
            << "is unavailable; publishing the remaining catalog "
            << "in degraded mode\n";
    }

    // Publish the valid subset. A malformed sibling file must not discard
    // systems and objects that passed validation independently.
    m_systems = std::move(systems);
    m_distantSystems = std::move(distantSystems);
    m_details = std::move(details);
    m_objects = std::move(objects);
    m_contentFingerprint = computeCatalogFingerprint(galaxyDetailsRoot);

    const std::size_t skippedSystemFiles =
        localReport.filesSkipped +
        distantReport.filesSkipped +
        buildReport.filesSkipped;

    std::cout
        << "[StarAtlasDatabase] local_systems=" << m_systems.size()
        << " distant_systems=" << m_distantSystems.size()
        << " details=" << m_details.size()
        << " galaxy_objects=" << m_objects.size()
        << " skipped_system_files=" << skippedSystemFiles
        << " skipped_object_files=" << objectReport.filesSkipped
        << " fingerprint=0x" << std::hex << m_contentFingerprint
        << std::dec
        << " degraded="
        << (
            skippedSystemFiles > 0 ||
            objectReport.filesSkipped > 0 ||
            !hasSol
                ? "yes"
                : "no"
        )
        << " root=" << galaxyDetailsRoot
        << "\n";

    return true;
}

bool StarAtlasDatabase::loadFromRuntimeOrSource()
{
    for (const char* root : {
            "assets/data/galaxy_details",
            "../assets/data/galaxy_details",
            "../src/assets/data/galaxy_details"})
    {
        if (load(root))
            return true;
    }

    return false;
}

const CelestialSystemDefinition* StarAtlasDatabase::findSystem(int systemId) const
{
    auto it = m_details.find(systemId);
    if (it == m_details.end())
        return nullptr;

    return &it->second;
}





const StarSystemSummary*
StarAtlasDatabase::findSystemSummary(
    int systemId
) const
{
    for (const auto& system :
         m_systems)
    {
        if (system.id == systemId)
            return &system;
    }

    for (const auto& system :
         m_distantSystems)
    {
        if (system.id == systemId)
            return &system;
    }

    return nullptr;
}











} // namespace world::celestial
