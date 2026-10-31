#include "src/render/celestial/flight/FlightFrameBuilder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

#include "src/game/system_map/PlanetBodyOrientation.h"

namespace render::celestial_flight
{
namespace
{
    namespace fs = std::filesystem;
    using world::celestial::BodyType;

    std::string bodyFolderFromSource(const std::string& sourcePath)
    {
        std::string stem = fs::path(sourcePath).stem().string();
        for (const char* prefix : {"planet_", "moon_", "dwarf_"})
            if (stem.rfind(prefix, 0) == 0)
                return stem.substr(std::char_traits<char>::length(prefix));
        return stem;
    }

    bool isGiant(const std::string& bodyClass)
    {
        return bodyClass == "gas_giant" || bodyClass == "super_jupiter" || bodyClass == "ice_giant" ||
               bodyClass == "sub_neptune";
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
}

std::string celestialAssetFolder(const std::string& name)
{
    std::string out;
    for (char c : name)
    {
        if (std::isalnum(static_cast<unsigned char>(c)))
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        else if (!out.empty() && out.back() != '_')
            out.push_back('_');
    }
    while (!out.empty() && out.back() == '_')
        out.pop_back();
    return out;
}

FlightFrameBuilder::FlightFrameBuilder(std::string assetRoot) : m_root(std::move(assetRoot)) {}

const FlightFrameBuilder::Imported& FlightFrameBuilder::imported(const std::string& systemName,
                                                                 const std::string& sourcePath)
{
    const std::string key = systemName + "|" + sourcePath;
    Imported& im = m_imported[key];
    if (im.checked)
        return im;
    im.checked = true;
    const std::string sys = celestialAssetFolder(systemName);
    const std::string body = bodyFolderFromSource(sourcePath);
    const fs::path source = fs::path(m_root) / "assets/source/celestial" / sys / body;
    const fs::path generated = fs::path(m_root) / "assets/generated/celestial" / sys / body;
    std::error_code ec;
    for (const fs::path& p : {source / "albedo_source.jpg", source / "albedo_source.png",
                              generated / "lod/global/albedo_2048.tga"})
        if (fs::exists(p, ec))
        {
            im.albedo = p.generic_string();
            break;
        }
    if (!im.albedo.empty() && fs::exists(source / "height_source.png", ec))
    {
        im.height = (source / "height_source.png").generic_string();
        heightRange(body, im.rangeM, im.heightSigned, im.waterFromHeight);
    }
    return im;
}

void FlightFrameBuilder::build(const world::celestial::CelestialSystemDefinition& system,
                               const world::celestial::CelestialSystemSnapshot& snapshot,
                               const glm::dvec3& cameraPositionM, double worldTimeSeconds, FlightCelestialFrame& out)
{
    out = FlightCelestialFrame {};
    out.systemId = snapshot.systemId;
    out.cameraPositionM = cameraPositionM;
    out.worldTimeSeconds = worldTimeSeconds;
    std::unordered_map<std::string, const world::celestial::CelestialBodyDefinition*> definitions;
    for (const auto& d : system.bodies)
        definitions[d.id] = &d;
    std::unordered_map<std::string, glm::dvec3> positions;
    for (const auto& s : snapshot.bodies)
        positions[s.id] = s.worldMeters;

    for (const auto& s : snapshot.bodies)
    {
        const auto def = definitions.find(s.id);
        const std::string bodyClass = def != definitions.end() ? def->second->bodyClass : std::string();
        if (s.type == BodyType::AsteroidBelt)
        {
            FlightReservoir r;
            r.id = s.id;
            r.descriptorPath = s.sourcePath;
            const auto parent = positions.find(s.parentId);
            r.centreM = parent != positions.end() ? parent->second : glm::dvec3(0.0);
            out.reservoirs.push_back(r);
            continue;
        }
        if (s.type == BodyType::Barycenter || s.type == BodyType::Station || s.type == BodyType::Unknown ||
            s.radiusKm <= 0.0)
            continue;
        FlightBody b;
        b.id = s.id;
        b.name = s.name;
        b.positionM = s.worldMeters;
        b.radiusM = s.radiusKm * 1000.0;
        if (s.type == BodyType::Star)
        {
            b.kind = FlightBodyKind::Star;
            b.temperatureK = s.effectiveTemperatureK > 0.0 ? s.effectiveTemperatureK : 5772.0;
            b.luminositySolar = s.luminositySolar > 0.0 ? s.luminositySolar : 1.0;
        }
        else
        {
            b.kind = isGiant(bodyClass) ? FlightBodyKind::GasGiant : FlightBodyKind::Solid;
            b.descriptorPath = s.sourcePath;
            const auto o = game::system_map::makePlanetTextureOrientation(s.axialTiltDeg, s.axisNodeDeg,
                                                                          s.rotationPhaseRad,
                                                                          s.textureLongitudeOffsetDeg);
            b.prime = o.prime;
            b.north = o.north;
            b.east = o.east;
            if (!s.sourcePath.empty())
            {
                const Imported& im = imported(system.name, s.sourcePath);
                b.importedAlbedoPath = im.albedo;
                b.importedHeightPath = im.height;
                b.importedHeightRangeM = im.rangeM;
                b.importedHeightSigned = im.heightSigned;
                b.importedWaterFromHeight = im.waterFromHeight;
            }
            for (const auto& ring : s.rings)
            {
                FlightRing r;
                r.innerM = ring.innerRadiusKm * 1000.0;
                r.outerM = ring.outerRadiusKm * 1000.0;
                r.tint = ring.render.tint;
                r.opacity = std::clamp(ring.render.opacity * ring.render.visualOpacityScale, 0.02f, 0.95f);
                if (r.outerM > r.innerM)
                    b.rings.push_back(r);
            }
        }
        out.bodies.push_back(std::move(b));
    }
}
} // namespace render::celestial_flight
