#include "src/world/celestial/StarAtlasDatabase.h"

#include <fstream>
#include <iostream>
#include <algorithm>

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

} // namespace

bool StarAtlasDatabase::load(
    const std::string& starSystemsPath,
    const std::string& systemDetailsPath
)
{
    m_systems.clear();
    m_details.clear();

    bool ok = true;
    ok = loadStarSystems(starSystemsPath) && ok;
    ok = loadSystemDetails(systemDetailsPath) && ok;

    std::cout
        << "[StarAtlasDatabase] systems=" << m_systems.size()
        << " details=" << m_details.size()
        << "\n";

    return ok;
}

bool StarAtlasDatabase::loadStarSystems(const std::string& path)
{
    json root;

    try
    {
        root = loadJson(path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[StarAtlasDatabase] " << e.what() << "\n";
        return false;
    }

    if (!root.contains("star_systems") || !root["star_systems"].is_array())
    {
        std::cerr << "[StarAtlasDatabase] no star_systems array\n";
        return false;
    }

    for (const auto& item : root["star_systems"])
    {
        StarSystemSummary s;
        s.id = item.value("id", -1);
        s.name = item.value("name", "");
        s.positionLy = glm::dvec3(
            item.value("x_ly", 0.0),
            item.value("y_ly", 0.0),
            item.value("z_ly", 0.0)
        );
        s.starsCount = item.value("stars_count", 1);
        s.starType = item.value("star_type", "");

        if (s.id >= 0)
            m_systems.push_back(std::move(s));
    }

    return true;
}

bool StarAtlasDatabase::loadSystemDetails(const std::string& path)
{
    json root;

    try
    {
        root = loadJson(path);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[StarAtlasDatabase] " << e.what() << "\n";
        return false;
    }

    if (!root.contains("systems") || !root["systems"].is_array())
    {
        std::cerr << "[StarAtlasDatabase] no systems array\n";
        return false;
    }

    for (const auto& srcSystem : root["systems"])
    {
        CelestialSystemDefinition system;

        system.systemId = srcSystem.value("system_id", -1);
        system.name = srcSystem.value("system_name", "");

        if (srcSystem.contains("center_of_mass"))
        {
            system.barycenterAu = glm::dvec3(
                srcSystem["center_of_mass"].value("x_au", 0.0),
                srcSystem["center_of_mass"].value("y_au", 0.0),
                srcSystem["center_of_mass"].value("z_au", 0.0)
            );
        }

        if (srcSystem.contains("stars") && srcSystem["stars"].is_array())
        {
            for (const auto& star : srcSystem["stars"])
            {
                CelestialBodyDefinition body;
                body.name = star.value("name", "Star");
                body.id = "system_" + std::to_string(system.systemId) + "." + safeIdPart(body.name);
              

                body.type = BodyType::Star;
                body.parentId.clear();

                body.diameterKm = star.value("diameter_km", 0.0);
                body.radiusKm = body.diameterKm * 0.5;

                readGravityFields(star, body);

                if (star.contains("position_au"))
                    body.staticPositionAu = readAuPosition(star["position_au"]);

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
                "system_" + std::to_string(system.systemId) + ".barycenter",
                srcSystem["system_planets"]
            );
        }

        if (srcSystem.contains("asteroid_belts"))
        {
            addAsteroidBelts(
                system,
                "system_" + std::to_string(system.systemId) + ".barycenter",
                srcSystem["asteroid_belts"]
            );
        }

        if (system.systemId >= 0)
            m_details.emplace(system.systemId, std::move(system));
    }

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

    return nullptr;
}











} // namespace world::celestial