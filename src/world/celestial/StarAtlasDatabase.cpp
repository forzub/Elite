#include "src/world/celestial/StarAtlasDatabase.h"

#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
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
    std::ifstream f(path);
    if (!f.is_open())
        throw std::runtime_error("Cannot open JSON: " + path);

    json j;
    f >> j;
    return j;
}


namespace fs = std::filesystem;

struct SystemCatalogDocument
{
    fs::path sourcePath;
    StarSystemSummary summary;
    json details;
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

bool parseSystemCatalogFile(
    const fs::path& path,
    StarSystemCatalogScope catalogScope,
    SystemCatalogDocument& out,
    std::string& error
)
{
    json root;

    try
    {
        root = loadJson(path.string());
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }

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
        root["kind"].get<std::string>() != "star_system")
    {
        return fail("kind must be 'star_system'");
    }

    if (!root.contains("id") ||
        !root["id"].is_number_integer())
    {
        return fail("id must be an integer");
    }

    out.summary.id = root["id"].get<int>();

    if (out.summary.id < 0)
        return fail("id must be non-negative");

    if (catalogScope == StarSystemCatalogScope::Local &&
        out.summary.id >= 100000)
    {
        return fail("local system id must be below 100000");
    }

    if (catalogScope == StarSystemCatalogScope::Distant &&
        out.summary.id < 100000)
    {
        return fail("distant system id must be at least 100000");
    }

    if (!root.contains("name") ||
        !root["name"].is_string())
    {
        return fail("name must be a string");
    }

    out.summary.name = root["name"].get<std::string>();

    if (out.summary.name.empty())
        return fail("name must not be empty");

    if (!root.contains("position_ly") ||
        !readPositionLy(root["position_ly"], out.summary.positionLy))
    {
        return fail("position_ly must contain numeric x, y and z");
    }

    if (root.contains("distance_ly"))
    {
        return fail(
            "distance_ly is derived from position_ly and must not be stored"
        );
    }

    if (!root.contains("stars_count") ||
        !root["stars_count"].is_number_integer())
    {
        return fail("stars_count must be an integer");
    }

    out.summary.starsCount =
        root["stars_count"].get<int>();

    if (out.summary.starsCount < 1)
        return fail("stars_count must be positive");

    if (!root.contains("star_type") ||
        !root["star_type"].is_string())
    {
        return fail("star_type must be a string");
    }

    out.summary.starType =
        root["star_type"].get<std::string>();

    out.summary.catalogScope = catalogScope;
    out.summary.atlasVisible =
        catalogScope == StarSystemCatalogScope::Local;
    out.summary.routeTarget = true;

    if (root.contains("catalog_scope"))
    {
        if (!root["catalog_scope"].is_string())
            return fail("catalog_scope must be a string");

        const std::string expectedScope =
            catalogScope == StarSystemCatalogScope::Distant
                ? "distant"
                : "local";

        if (root["catalog_scope"].get<std::string>() != expectedScope)
        {
            return fail(
                "catalog_scope does not match the containing directory"
            );
        }
    }
    else if (catalogScope == StarSystemCatalogScope::Distant)
    {
        return fail("distant systems require catalog_scope='distant'");
    }

    if (root.contains("atlas_visible"))
    {
        if (!root["atlas_visible"].is_boolean())
            return fail("atlas_visible must be a boolean");

        out.summary.atlasVisible =
            root["atlas_visible"].get<bool>();
    }

    if (root.contains("route_target"))
    {
        if (!root["route_target"].is_boolean())
            return fail("route_target must be a boolean");

        out.summary.routeTarget =
            root["route_target"].get<bool>();
    }

    if (root.contains("quest_role"))
    {
        if (!root["quest_role"].is_string())
            return fail("quest_role must be a string");

        out.summary.questRole =
            root["quest_role"].get<std::string>();
    }

    if (catalogScope == StarSystemCatalogScope::Distant &&
        out.summary.atlasVisible)
    {
        return fail(
            "distant systems must set atlas_visible=false in the current atlas"
        );
    }

    if (!root.contains("details") ||
        !root["details"].is_object())
    {
        return fail("details must be an object");
    }

    out.sourcePath = path;
    out.details = root["details"];

    const auto requireArray =
        [&](const char* key)
        {
            return
                !out.details.contains(key) ||
                out.details[key].is_array();
        };

    if (!requireArray("stars") ||
        !requireArray("system_planets") ||
        !requireArray("asteroid_belts"))
    {
        return fail(
            "details.stars, details.system_planets and "
            "details.asteroid_belts must be arrays"
        );
    }

    if (out.details.contains("center_of_mass") &&
        !out.details["center_of_mass"].is_object())
    {
        return fail("details.center_of_mass must be an object");
    }

    if (out.details.contains("stars") &&
        static_cast<int>(out.details["stars"].size()) !=
            out.summary.starsCount)
    {
        return fail(
            "stars_count does not match details.stars size"
        );
    }

    return true;
}

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

    if (!collectJsonFiles(
            directory,
            allowMissingOrEmpty,
            files,
            fatalError))
    {
        return false;
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

        if (!parseSystemCatalogFile(
                path,
                catalogScope,
                document,
                fileError))
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
            << "no valid star-system JSON files found in: "
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

bool parseGalaxyObjectFile(
    const fs::path& path,
    GalaxyObjectDefinition& out,
    std::string& error
)
{
    json root;

    try
    {
        root = loadJson(path.string());

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

    report.filesDiscovered = files.size();

    outObjects.clear();
    outObjects.reserve(files.size());

    std::unordered_map<std::string, fs::path> idSources;

    for (const fs::path& path : files)
    {
        GalaxyObjectDefinition object;
        std::string fileError;

        if (!parseGalaxyObjectFile(
                path,
                object,
                fileError))
        {
            addCatalogSkip(report, fileError);
            continue;
        }

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

glm::dvec3 readAuPosition(const json& j)
{
    return glm::dvec3(
        j.value("x", j.value("x_au", 0.0)),
        j.value("y", j.value("y_au", 0.0)),
        j.value("z", j.value("z_au", 0.0))
    );
}

std::string safeIdPart(std::string s)
{
    for (char& c : s)
    {
        if (c == ' ' || c == '(' || c == ')' || c == '/' || c == '\\')
            c = '_';
    }
    return s;
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




void readGravityFields(
        const json& j,
        world::celestial::CelestialBodyDefinition& body
    )
    {
        body.massKg =
            j.value("mass_kg", 0.0);

        body.gravitationalParameterM3s2 =
            j.value("gravitational_parameter_m3s2", 0.0);

        if (body.gravitationalParameterM3s2 <= 0.0 &&
            body.massKg > 0.0)
        {
            body.gravitationalParameterM3s2 =
                world::celestial::GravitationalConstant *
                body.massKg;
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






void readEnvironmentFields(
    const json& source,
    CelestialBodyDefinition& body
)
{
    body.environmentPresetId =
        source.value(
            "environment_preset_id",
            ""
        );
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







void addMoonDefinitions(
    CelestialSystemDefinition& system,
    const std::string& parentPlanetId,
    const json& moons
)
{
    if (!moons.is_array())
        return;

    for (const auto& moon : moons)
    {
        if (!moon.is_object())
            continue;

        CelestialBodyDefinition body;
        body.name = moon.value("name", "Moon");
        body.id = parentPlanetId + "." + safeIdPart(body.name);
       

        body.type = BodyType::Moon;
        body.parentId = parentPlanetId;

        body.diameterKm = moon.value("diameter_km", 0.0);
        body.radiusKm = body.diameterKm * 0.5;

        readGravityFields(moon, body);

        body.distanceAu = moon.value("distance_au", 0.0);

        if (body.distanceAu <= 0.0)
        {
            const double distanceKm =
                moon.value("distance_from_planet_km", 0.0);

            if (distanceKm > 0.0)
                body.distanceAu = distanceKm / MetersPerAu * 1000.0;
        }

        body.orbitalPeriodDays = moon.value("orbital_period_days", 0.0);
        body.dayLengthHours = moon.value("day_length_hours", 0.0);

        readOrientationFields(moon, body);
        readEnvironmentFields(moon, body);
        readAlternativeNames(moon, body);

        system.bodies.push_back(std::move(body));
    }
}

void addPlanetDefinitions(
    CelestialSystemDefinition& system,
    const std::string& parentStarId,
    const json& planets
)
{
    if (!planets.is_array())
        return;

    for (const auto& planet : planets)
    {
        if (!planet.is_object())
            continue;

        CelestialBodyDefinition body;
        body.name = planet.value("name", "Planet");
        body.id = parentStarId + "." + safeIdPart(body.name);
        

        body.type = BodyType::Planet;
        body.parentId = parentStarId;

        body.diameterKm = planet.value("diameter_km", 0.0);
        body.radiusKm = body.diameterKm * 0.5;

        readGravityFields(planet, body);

        body.distanceAu = planet.value("distance_au", 0.0);


        body.orbitalPeriodDays = planet.value("orbital_period_days", 0.0);
        body.dayLengthHours = planet.value("day_length_hours", 0.0);

        readOrientationFields(planet, body);
        readEnvironmentFields(planet, body);
        readAlternativeNames(planet, body);
        readRings(planet, body);

        const std::string planetId = body.id;
        system.bodies.push_back(std::move(body));

        if (planet.contains("moons"))
            addMoonDefinitions(system, planetId, planet["moons"]);
    }
}

void addAsteroidBelts(
    CelestialSystemDefinition& system,
    const std::string& parentId,
    const json& belts
)
{
    if (!belts.is_array())
        return;

    int index = 0;

    for (const auto& belt : belts)
    {
        if (!belt.is_object())
            continue;

        CelestialBodyDefinition body;
        body.name = belt.value("name", "Asteroid Belt");
        body.id = parentId + ".belt_" + std::to_string(index++);
        

        body.type = BodyType::AsteroidBelt;
        body.parentId = parentId;

        body.distanceAu =
            belt.value(
                "distance_au",
                belt.value("distance_from_star_au", 0.0)
            );
        body.orbitalPeriodDays = belt.value("orbital_period_days", 0.0);

        system.bodies.push_back(std::move(body));
    }
}

CelestialSystemDefinition buildSystemDefinition(
    const SystemCatalogDocument& document
)
{
    const json& srcSystem = document.details;

    CelestialSystemDefinition system;
    system.systemId = document.summary.id;
    system.name = document.summary.name;

    if (srcSystem.contains("center_of_mass"))
    {
        system.barycenterAu = glm::dvec3(
            srcSystem["center_of_mass"].value("x_au", 0.0),
            srcSystem["center_of_mass"].value("y_au", 0.0),
            srcSystem["center_of_mass"].value("z_au", 0.0)
        );
    }

    if (srcSystem.contains("stars"))
    {
        if (!srcSystem["stars"].is_array())
        {
            throw std::runtime_error(
                "details.stars must be an array"
            );
        }

        for (const auto& star : srcSystem["stars"])
        {
            if (!star.is_object())
            {
                throw std::runtime_error(
                    "every details.stars entry must be an object"
                );
            }

            CelestialBodyDefinition body;
            body.name = star.value("name", "Star");
            body.id =
                "system_" +
                std::to_string(system.systemId) +
                "." +
                safeIdPart(body.name);

            body.type = BodyType::Star;
            body.parentId.clear();

            body.diameterKm = star.value("diameter_km", 0.0);
            body.radiusKm = body.diameterKm * 0.5;

            readGravityFields(star, body);
            readOrientationFields(star, body);

            if (star.contains("position_au"))
            {
                if (!star["position_au"].is_object())
                {
                    throw std::runtime_error(
                        "star.position_au must be an object"
                    );
                }

                body.staticPositionAu =
                    readAuPosition(star["position_au"]);
            }

            readAlternativeNames(star, body);

            const std::string starId = body.id;
            system.bodies.push_back(std::move(body));

            if (star.contains("planets"))
                addPlanetDefinitions(system, starId, star["planets"]);

            if (star.contains("asteroid_belts"))
                addAsteroidBelts(system, starId, star["asteroid_belts"]);
        }
    }

    if (srcSystem.contains("system_planets"))
    {
        addPlanetDefinitions(
            system,
            "system_" +
                std::to_string(system.systemId) +
                ".barycenter",
            srcSystem["system_planets"]
        );
    }

    if (srcSystem.contains("asteroid_belts"))
    {
        addAsteroidBelts(
            system,
            "system_" +
                std::to_string(system.systemId) +
                ".barycenter",
            srcSystem["asteroid_belts"]
        );
    }

    return system;
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
            "systems_details",
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
            "systems_details",
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
            "distant_systems_details",
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
                        buildSystemDefinition(document);

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
